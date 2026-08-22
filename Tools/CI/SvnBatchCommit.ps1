<#
.SYNOPSIS
    Commits a staged SVN working copy to the server in small batches.

.DESCRIPTION
    SourceForge's HTTP front end times out (504 Gateway Time-out, followed by a
    500 on the transaction) when a single commit carries the whole release
    payload -- the derived JSON alone is over 13000 files, and the FPKs are
    ~256 MB each. This script walks the staged working copy and commits it as a
    sequence of bounded transactions instead of one.

    Batches are ordered so that every transaction is legal on its own:

      1. Newly added DIRECTORIES, parents before children. SVN rejects a commit
         containing a child whose parent directory neither exists in the
         repository nor is part of the same transaction.
      2. Deletions, pruned to the top-most deleted path. Deleting a directory
         removes its subtree in one operation, so listing the descendants is
         both redundant and a source of "path not found" errors once the parent
         delete has landed.
      3. Everything else -- added, modified and replaced files, plus
         property-only changes.

    The version-stamped defines file and the built DLL are held back to the very
    last batch. A batched deploy is NOT atomic, so if it dies partway the trunk
    must not be left advertising a version whose payload never fully arrived.

.NOTES
    Written to run under both Windows PowerShell 5.1 and PowerShell 7, because
    it is invoked from DeployBuild.bat on the AppVeyor image.
#>

param(
    [string] $WorkingCopyPath = ".",
    [string] $MessageFile = "",
    [string] $Version = "",
    [int]    $MaxFilesPerBatch = 400,
    [int]    $MaxMegabytesPerBatch = 64,
    [int]    $MaxRetriesPerBatch = 5,
    # Credentials default to the environment so the password never has to survive
    # batch-file and PowerShell command-line quoting.
    [string] $Username = $env:svn_user,
    [string] $Password = $env:svn_pass
)

$ErrorActionPreference = "Stop"

# Paths whose commit is deferred to the final batch (see .DESCRIPTION).
$holdBackUntilLastBatch = @(
    "Assets\XML\A_New_Dawn_GlobalDefines.xml",
    "Assets\CvGameCoreDLL.dll"
)

function Write-Step([string] $message)
{
    Write-Host "[svn-batch] $message"
}

function Get-SvnAuthenticationArguments
{
    $arguments = @("--non-interactive", "--no-auth-cache")
    if (-not [string]::IsNullOrEmpty($Username))
    {
        $arguments += @("--username", $Username)
    }
    if (-not [string]::IsNullOrEmpty($Password))
    {
        $arguments += @("--password", $Password)
    }
    return $arguments
}

function Invoke-Svn([string[]] $arguments)
{
    $output = & svn.exe @arguments 2>&1
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output   = $output
    }
}

<#
    Reads `svn status --xml` and returns one record per committable entry.
    Anything the server does not need to hear about (unversioned, ignored,
    external, unchanged) is dropped here.
#>
function Get-CommittableEntries([bool] $reportProblemStatuses = $false)
{
    $statusResult = Invoke-Svn @("status", "--xml", "--non-interactive")
    if ($statusResult.ExitCode -ne 0)
    {
        throw "svn status failed with exit code $($statusResult.ExitCode)"
    }

    [xml] $statusXml = ($statusResult.Output -join [Environment]::NewLine)

    $entries = @()
    foreach ($target in $statusXml.status.target)
    {
        foreach ($entry in $target.entry)
        {
            if ($null -eq $entry)
            {
                continue
            }

            $workingCopyStatus = $entry.'wc-status'
            $itemStatus = $workingCopyStatus.item
            $propertyStatus = $workingCopyStatus.props

            $isCommittableItem = @("added", "deleted", "modified", "replaced") -contains $itemStatus
            $isCommittableProperty = ($propertyStatus -eq "modified") -and ($itemStatus -ne "unversioned")

            # A single root-level commit would have FAILED on these. Batching only
            # ever targets what it can commit, so without this they would be
            # skipped in silence and the release would ship a stale file.
            if ($reportProblemStatuses -and (@("missing", "conflicted", "obstructed", "incomplete") -contains $itemStatus))
            {
                Write-Step "WARNING: '$($entry.path)' is '$itemStatus' and will NOT be committed"
            }

            if (-not ($isCommittableItem -or $isCommittableProperty))
            {
                continue
            }

            $relativePath = $entry.path

            $entries += [pscustomobject]@{
                Path        = $relativePath
                ItemStatus  = $itemStatus
                IsDirectory = (Test-Path -LiteralPath $relativePath -PathType Container)
                SizeInBytes = [long] 0
            }
        }
    }

    # Size is only meaningful for content we actually upload.
    foreach ($entry in $entries)
    {
        if ($entry.ItemStatus -ne "deleted" -and -not $entry.IsDirectory -and (Test-Path -LiteralPath $entry.Path -PathType Leaf))
        {
            $entry.SizeInBytes = (Get-Item -LiteralPath $entry.Path).Length
        }
    }

    return $entries
}

<#
    Drops any path that lives underneath another path in the same set. Used to
    reduce the deletion list to its top-most entries.
#>
function Remove-DescendantPaths([string[]] $paths)
{
    $sortedPaths = @($paths | Sort-Object { $_.Length })
    $keptPaths = New-Object System.Collections.ArrayList

    foreach ($candidatePath in $sortedPaths)
    {
        $hasAncestorInSet = $false
        foreach ($keptPath in $keptPaths)
        {
            $ancestorPrefix = $keptPath + [System.IO.Path]::DirectorySeparatorChar
            if ($candidatePath.StartsWith($ancestorPrefix, [System.StringComparison]::OrdinalIgnoreCase))
            {
                $hasAncestorInSet = $true
                break
            }
        }
        if (-not $hasAncestorInSet)
        {
            [void] $keptPaths.Add($candidatePath)
        }
    }

    return $keptPaths.ToArray()
}

<#
    Splits an ordered entry list into batches bounded by BOTH a file count and a
    byte total, appending each batch to $destination. A single file larger than
    the byte cap becomes its own batch rather than being merged into a neighbour.

    Batches are APPENDED to a caller-supplied list rather than returned, because
    a PowerShell function that returns an array of arrays has both levels
    unrolled by the output stream -- one batch of four paths comes back out as
    four batches of one, and the resulting single-directory commits then sweep
    up whole subtrees recursively.

    $commitDepth travels with the batch: "empty" restricts a commit to the named
    node so a directory target cannot drag its subtree into the transaction,
    while "infinity" is what makes a single directory deletion remove its
    subtree in one cheap operation.
#>
function Add-EntryBatches($destination, $entries, [string] $commitDepth, [int] $maxFiles, [long] $maxBytes)
{
    $currentBatch = New-Object System.Collections.ArrayList
    $currentBytes = [long] 0

    foreach ($entry in $entries)
    {
        $wouldExceedFileCount = ($currentBatch.Count + 1) -gt $maxFiles
        $wouldExceedByteCount = ($currentBatch.Count -gt 0) -and (($currentBytes + $entry.SizeInBytes) -gt $maxBytes)

        if ($wouldExceedFileCount -or $wouldExceedByteCount)
        {
            [void] $destination.Add([pscustomobject]@{ Entries = $currentBatch.ToArray(); CommitDepth = $commitDepth })
            $currentBatch = New-Object System.Collections.ArrayList
            $currentBytes = [long] 0
        }

        [void] $currentBatch.Add($entry)
        $currentBytes += $entry.SizeInBytes
    }

    if ($currentBatch.Count -gt 0)
    {
        [void] $destination.Add([pscustomobject]@{ Entries = $currentBatch.ToArray(); CommitDepth = $commitDepth })
    }
}

function Write-TargetsFile($entries)
{
    $fileName = "svn-batch-targets-" + [System.Guid]::NewGuid().ToString("N") + ".txt"
    $targetsFilePath = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), $fileName)
    $lines = @($entries | ForEach-Object { $_.Path })
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllLines($targetsFilePath, $lines, $utf8WithoutBom)
    return $targetsFilePath
}

<#
    Returns $true when none of the supplied paths still has an outstanding
    change. After a gateway timeout the transaction may well have landed on the
    server even though the client reported failure, so this is what tells a
    successful-but-unacknowledged commit apart from one that genuinely needs
    resending.
#>
function Test-BatchAlreadyCommitted($entries)
{
    $outstandingPaths = @{}
    foreach ($entry in (Get-CommittableEntries))
    {
        $outstandingPaths[$entry.Path.ToLowerInvariant()] = $true
    }

    foreach ($entry in $entries)
    {
        if ($outstandingPaths.ContainsKey($entry.Path.ToLowerInvariant()))
        {
            return $false
        }
    }

    return $true
}

function Invoke-BatchCommit($batch, [int] $batchNumber, [int] $batchCount, [bool] $useMessageFile)
{
    $entries = @($batch.Entries)
    $commitDepth = $batch.CommitDepth

    $batchBytes = [long] 0
    foreach ($entry in $entries)
    {
        $batchBytes += $entry.SizeInBytes
    }
    $batchMegabytes = [math]::Round($batchBytes / 1MB, 1)

    Write-Step "Batch $batchNumber/$batchCount - $($entries.Count) path(s), $batchMegabytes MB, depth=$commitDepth"

    $targetsFilePath = Write-TargetsFile $entries

    try
    {
        for ($attempt = 1; $attempt -le $MaxRetriesPerBatch; $attempt++)
        {
            $commitArguments = @("commit", "--targets", $targetsFilePath, "--depth", $commitDepth)
            $commitArguments += Get-SvnAuthenticationArguments

            # Only the final batch carries the generated changelog; the rest get a
            # short marker so the SVN log is not N copies of the same release notes.
            if ($useMessageFile -and $MessageFile -ne "")
            {
                $commitArguments += @("-F", $MessageFile)
            }
            else
            {
                $commitArguments += @("-m", "$Version (part $batchNumber/$batchCount)")
            }

            $commitResult = Invoke-Svn $commitArguments
            $commitResult.Output | ForEach-Object { Write-Host $_ }

            if ($commitResult.ExitCode -eq 0)
            {
                return
            }

            Write-Step "Batch $batchNumber/$batchCount failed on attempt $attempt (exit $($commitResult.ExitCode))"

            if ($attempt -eq $MaxRetriesPerBatch)
            {
                break
            }

            # A 504 can mean "the server took the transaction but the gateway gave
            # up relaying the answer". Clean up, resynchronise, and re-check before
            # deciding this batch actually needs sending again.
            [void] (Invoke-Svn @("cleanup", "--non-interactive"))

            $updateArguments = @("update", "--quiet")
            $updateArguments += Get-SvnAuthenticationArguments
            [void] (Invoke-Svn $updateArguments)

            if (Test-BatchAlreadyCommitted $entries)
            {
                Write-Step "Batch $batchNumber/$batchCount already present on the server; continuing"
                return
            }

            $backoffSeconds = 10 * $attempt
            Write-Step "Retrying batch $batchNumber/$batchCount in $backoffSeconds second(s)"
            Start-Sleep -Seconds $backoffSeconds
        }

        throw "SVN commit batch $batchNumber/$batchCount failed after $MaxRetriesPerBatch attempt(s)"
    }
    finally
    {
        Remove-Item -LiteralPath $targetsFilePath -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------

Push-Location -LiteralPath $WorkingCopyPath
try
{
    Write-Step "Scanning working copy '$((Get-Location).Path)' for staged changes..."
    $allEntries = Get-CommittableEntries $true

    if ($allEntries.Count -eq 0)
    {
        Write-Step "Nothing to commit."
        exit 0
    }

    Write-Step "$($allEntries.Count) committable path(s) found."

    # Pass 1 -- added/replaced directories, parents first. A lexicographic sort
    # is sufficient: a parent path is a prefix of its children, so it sorts ahead.
    $addedDirectories = @($allEntries |
        Where-Object { $_.IsDirectory -and (@("added", "replaced") -contains $_.ItemStatus) } |
        Sort-Object { $_.Path })

    # Pass 2 -- deletions, reduced to top-most paths.
    $deletedEntries = @($allEntries | Where-Object { $_.ItemStatus -eq "deleted" })
    $topMostDeletedPaths = Remove-DescendantPaths @($deletedEntries | ForEach-Object { $_.Path })
    $topMostDeletedPathSet = @{}
    foreach ($path in $topMostDeletedPaths)
    {
        $topMostDeletedPathSet[$path] = $true
    }
    $deletions = @($deletedEntries |
        Where-Object { $topMostDeletedPathSet.ContainsKey($_.Path) } |
        Sort-Object { $_.Path })

    # Pass 3 -- everything left, with the version marker and DLL pushed to the end.
    $addedDirectoryPathSet = @{}
    foreach ($entry in $addedDirectories)
    {
        $addedDirectoryPathSet[$entry.Path] = $true
    }

    $remainingEntries = @($allEntries | Where-Object {
        ($_.ItemStatus -ne "deleted") -and (-not $addedDirectoryPathSet.ContainsKey($_.Path))
    })

    $heldBackEntries = @($remainingEntries | Where-Object { $holdBackUntilLastBatch -contains $_.Path })
    $ordinaryEntries = @($remainingEntries |
        Where-Object { -not ($holdBackUntilLastBatch -contains $_.Path) } |
        Sort-Object { $_.Path })

    $fileCount = $ordinaryEntries.Count + $heldBackEntries.Count
    Write-Step "Added directories: $($addedDirectories.Count) | Deletions: $($deletions.Count) | Files: $fileCount"

    $maxBytesPerBatch = [long] $MaxMegabytesPerBatch * 1MB

    # Directory adds and deletions carry no payload, so only the file count bounds them.
    # Deletions commit at infinite depth so one target removes its whole subtree;
    # everything else is pinned to "empty" so no target can widen its own transaction.
    $allBatches = New-Object System.Collections.ArrayList
    Add-EntryBatches $allBatches $addedDirectories "empty"    $MaxFilesPerBatch ([long]::MaxValue)
    Add-EntryBatches $allBatches $deletions        "infinity" $MaxFilesPerBatch ([long]::MaxValue)
    Add-EntryBatches $allBatches $ordinaryEntries  "empty"    $MaxFilesPerBatch $maxBytesPerBatch
    Add-EntryBatches $allBatches $heldBackEntries  "empty"    $MaxFilesPerBatch $maxBytesPerBatch

    $batchCount = $allBatches.Count
    Write-Step "Committing in $batchCount batch(es) of at most $MaxFilesPerBatch path(s) / $MaxMegabytesPerBatch MB."

    for ($batchIndex = 0; $batchIndex -lt $batchCount; $batchIndex++)
    {
        $isFinalBatch = ($batchIndex -eq ($batchCount - 1))
        Invoke-BatchCommit $allBatches[$batchIndex] ($batchIndex + 1) $batchCount $isFinalBatch
    }

    Write-Step "All $batchCount batch(es) committed."
    exit 0
}
catch
{
    Write-Host "[svn-batch] FAILED: $($_.Exception.Message)"
    exit 3
}
finally
{
    Pop-Location
}
