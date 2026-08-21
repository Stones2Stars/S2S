@echo off
PUSHD "%~dp0"
REM Extra args are forwarded to _Build.ps1, so "MakeDLLFinalRelease.bat nostop" opts in to -nostoponerror.
call _MakeDLL.bat FinalRelease rebuild deploy %*
POPD
