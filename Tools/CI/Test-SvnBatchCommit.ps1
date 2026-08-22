# End-to-end exercise of Tools/CI/SvnBatchCommit.ps1 against a throwaway local
# repository, so the batching rules can be verified without running a release.
#
#   pwsh -NoProfile -ExecutionPolicy Bypass -File Tools/CI/Test-SvnBatchCommit.ps1
#
# Requires svn.exe and svnadmin.exe on PATH (TortoiseSVN ships both). It creates
# its repo under $env:TEMP and touches nothing in this checkout. Exits non-zero
# on any failed check. See docs/reference/release-deploy.md.
$ErrorActionPreference = "Stop"

$scriptUnderTest = Join-Path $PSScriptRoot "SvnBatchCommit.ps1"

$maxFilesPerBatch = 5
$maxMegabytesPerBatch = 1

$base = Join-Path $env:TEMP ('svnbatch_' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $base | Out-Null
$repo = Join-Path $base 'repo'
$wc   = Join-Path $base 'wc'

svnadmin create $repo
$repoUrl = ([System.Uri]$repo).AbsoluteUri
svn checkout --quiet $repoUrl $wc

# ---- baseline -------------------------------------------------------------
New-Item -ItemType Directory -Force -Path "$wc\Assets\XML","$wc\Assets\Data\old","$wc\Docs" | Out-Null
1..30 | ForEach-Object { Set-Content -Path "$wc\Assets\Data\base_$_.json" -Value "{v:$_}" }
1..5  | ForEach-Object { Set-Content -Path "$wc\Assets\Data\old\stale_$_.json" -Value "stale" }
Set-Content -Path "$wc\Assets\XML\A_New_Dawn_GlobalDefines.xml" -Value "<Civ4Defines><Define><DefineName>C2C_VERSION</DefineName><DefineTextVal>v1.0</DefineTextVal></Define></Civ4Defines>"
Set-Content -Path "$wc\Assets\CvGameCoreDLL.dll" -Value "OLD-DLL"
Set-Content -Path "$wc\Docs\readme.txt" -Value "hello"
Push-Location $wc
svn add --quiet --force .
svn commit --quiet -m "baseline"
Pop-Location
$baselineRevision = [int](svn info --show-item revision $repoUrl)

# ---- stage a release-shaped change set ------------------------------------
New-Item -ItemType Directory -Force -Path "$wc\Assets\Data\new\sub\deep","$wc\Assets\Data\new\other" | Out-Null
1..12 | ForEach-Object { Set-Content -Path "$wc\Assets\Data\new\sub\deep\d_$_.json" -Value "deep$_" }
1..8  | ForEach-Object { Set-Content -Path "$wc\Assets\Data\new\other\o_$_.json" -Value "other$_" }
Push-Location $wc
svn delete --quiet "Assets\Data\old"
svn delete --quiet "Assets\Data\base_30.json"
Pop-Location
1..20 | ForEach-Object { Set-Content -Path "$wc\Assets\Data\base_$_.json" -Value "{v:$_,changed:true}" }
[System.IO.File]::WriteAllBytes("$wc\Assets\big.FPK", (New-Object byte[] (2MB)))
# Deliberately MISSING entry: versioned, but gone from disk and never svn-deleted.
# DeployBuild.bat normally converts these to deletes first; the script must warn
# rather than skip it in silence.
Remove-Item -LiteralPath "$wc\Docs\readme.txt" -Force
Set-Content -Path "$wc\Assets\XML\A_New_Dawn_GlobalDefines.xml" -Value "<Civ4Defines><Define><DefineName>C2C_VERSION</DefineName><DefineTextVal>v2.0</DefineTextVal></Define></Civ4Defines>"
Set-Content -Path "$wc\Assets\CvGameCoreDLL.dll" -Value "NEW-DLL"
Push-Location $wc
svn add --quiet --force .
Pop-Location

$messageFile = Join-Path $base 'commit_desc.md'
Set-Content -LiteralPath $messageFile -Value "## v2.0 release notes"

# ---- run the thing under test ---------------------------------------------
$runOutput = & pwsh -NoProfile -ExecutionPolicy Bypass -File $scriptUnderTest `
    -WorkingCopyPath $wc -MessageFile $messageFile -Version 'v2.0' `
    -MaxFilesPerBatch $maxFilesPerBatch -MaxMegabytesPerBatch $maxMegabytesPerBatch 2>&1
$scriptExitCode = $LASTEXITCODE
$runOutput | ForEach-Object { Write-Host $_ }

"=============================== VERIFY ==============================="
$failures = New-Object System.Collections.ArrayList
function Assert-That([bool] $condition, [string] $label)
{
    if ($condition) { Write-Host "  PASS  $label" }
    else { Write-Host "  FAIL  $label"; [void] $failures.Add($label) }
}

Assert-That ($scriptExitCode -eq 0) "script exit code is 0 (got $scriptExitCode)"

Push-Location $wc

# The deliberately-missing readme is expected to remain outstanding.
$leftoverStatus = @(svn status | Where-Object { $_ -match '\S' -and $_ -notmatch 'readme\.txt' })
Pop-Location
Assert-That ($leftoverStatus.Count -eq 0) "working copy fully committed (leftover status lines: $($leftoverStatus.Count))"

$warnedAboutMissing = @($runOutput | Where-Object { "$_" -match 'WARNING.*readme\.txt.*missing' }).Count -gt 0
Assert-That $warnedAboutMissing "a MISSING entry is reported loudly, not skipped silently"

$headRevision = [int](svn info --show-item revision $repoUrl)
Assert-That ($headRevision -gt $baselineRevision) "head advanced past baseline ($baselineRevision -> $headRevision)"

# No single revision may exceed the file-count cap.
$oversizedRevisions = @()
$holdBackRevisions = @{}
for ($revision = $baselineRevision + 1; $revision -le $headRevision; $revision++)
{
    [xml] $revisionLogXml = ((svn log $repoUrl -r $revision -v --xml) -join "`n")
    $changedPaths = @($revisionLogXml.log.logentry.paths.path)
    if ($changedPaths.Count -gt $maxFilesPerBatch)
    {
        # A directory delete legitimately reports one path while removing a subtree.
        $oversizedRevisions += "r$revision has $($changedPaths.Count) paths"
    }
    $revisionText = (svn log $repoUrl -r $revision -v) -join "`n"
    if ($revisionText -match 'A_New_Dawn_GlobalDefines\.xml') { $holdBackRevisions['xml'] = $revision }
    if ($revisionText -match 'CvGameCoreDLL\.dll')            { $holdBackRevisions['dll'] = $revision }
}
Assert-That ($oversizedRevisions.Count -eq 0) "no revision exceeds $maxFilesPerBatch paths ($($oversizedRevisions -join '; '))"

Assert-That ($holdBackRevisions['xml'] -eq $headRevision) "version defines landed in the FINAL revision (r$($holdBackRevisions['xml']) vs head r$headRevision)"
Assert-That ($holdBackRevisions['dll'] -eq $headRevision) "DLL landed in the FINAL revision (r$($holdBackRevisions['dll']) vs head r$headRevision)"

$headMessage = (svn log $repoUrl -r $headRevision) -join "`n"
Assert-That ($headMessage -match 'v2\.0 release notes') "final revision carries the generated changelog"

# Server-side content checks.
# svn ls -R lists directories too, with a trailing slash; keep files only.
$serverListing = @(svn ls -R $repoUrl | Where-Object { -not $_.EndsWith('/') })
Assert-That (-not ($serverListing -match '^Assets/Data/old/')) "deleted directory is gone from the server"
Assert-That (-not ($serverListing -contains 'Assets/Data/base_30.json')) "deleted file is gone from the server"
Assert-That (($serverListing | Where-Object { $_ -like 'Assets/Data/new/sub/deep/*' }).Count -eq 12) "deep new directory tree fully present"
Assert-That (($serverListing | Where-Object { $_ -like 'Assets/Data/new/other/*' }).Count -eq 8) "second new directory tree fully present"

$serverXml = (svn cat "$repoUrl/Assets/XML/A_New_Dawn_GlobalDefines.xml") -join ""
Assert-That ($serverXml -match 'v2\.0') "version stamp updated on the server"
$serverDll = (svn cat "$repoUrl/Assets/CvGameCoreDLL.dll") -join ""
Assert-That ($serverDll -match 'NEW-DLL') "DLL content updated on the server"
$serverBig = svn info "$repoUrl/Assets/big.FPK" --show-item kind
Assert-That ($serverBig -eq 'file') "oversized file committed in its own batch"

"======================================================================"
if ($failures.Count -eq 0) { "ALL CHECKS PASSED (revisions $($baselineRevision + 1)..$headRevision)" }
else { "FAILURES: $($failures.Count)"; $failures | ForEach-Object { "  - $_" }; exit 1 }
