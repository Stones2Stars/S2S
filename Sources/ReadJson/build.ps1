# build.ps1 — compile the isolated readjson harness with the VENDORED VC7.1 toolchain (#428 step 1).
# Strict C++03 / MSVC 7.1 (despair #13 — fbuild.bff is truth, the .vcxproj's v142 is a lie).
# Produces Sources/ReadJson/readjson.exe. Run from anywhere; paths resolve off this script.
$ErrorActionPreference = 'Stop'
$here    = $PSScriptRoot
$root    = (Resolve-Path "$here\..\..").Path                       # repo root
$toolkit = Join-Path $root 'Build\deps\Microsoft Visual C++ Toolkit 2003'
$psdk    = Join-Path $root 'Build\deps\Microsoft SDKs\Windows\v6.0'
$cl      = Join-Path $toolkit 'bin\cl.exe'

if (-not (Test-Path $cl))   { throw "VC7.1 cl.exe not found at: $cl" }
if (-not (Test-Path $psdk)) { throw "Platform SDK not found at: $psdk" }

# the toolkit bin holds the compiler/linker support DLLs (mspdb71 etc.) — put it on PATH
$env:PATH = "$toolkit\bin;$env:PATH"

$args = @(
  # /MD (the toolkit's static-CRT /MT link is flaky here). The two VC7.1 runtime DLLs ship in the toolkit
  # bin and are copied next to the exe below, so it still runs standalone on a machine without VC2003.
  # NB: plain "/Ipath" double-quoted strings — let PowerShell quote the spaces; do NOT add inner quotes
  # (those get double-wrapped on the native call and mangle the path).
  '/nologo','/MD','/EHsc','/W3','/DWIN32','/O2',
  "/I$toolkit\include",
  "/I$psdk\Include",
  "/I$root\Sources\include",                                       # vendored picojson.h
  "$here\readjson.cpp",
  "/Fe$here\readjson.exe",
  "/Fo$here\readjson.obj",
  '/link',
  "/LIBPATH:$toolkit\lib",
  "/LIBPATH:$psdk\Lib",
  'kernel32.lib'
)

Write-Host "Compiling readjson.cpp with VC7.1 ($cl)..." -ForegroundColor Cyan
& $cl @args | Out-Host
if ($LASTEXITCODE -ne 0) { throw "cl.exe failed with exit code $LASTEXITCODE" }
# ship the VC7.1 runtime DLLs next to the exe so it runs on a machine without VC2003 installed
Copy-Item (Join-Path $toolkit 'bin\msvcr71.dll') $here -Force
Copy-Item (Join-Path $toolkit 'bin\msvcp71.dll') $here -Force
Write-Host "OK -> $here\readjson.exe (+ msvcr71/msvcp71 runtime)" -ForegroundColor Green
