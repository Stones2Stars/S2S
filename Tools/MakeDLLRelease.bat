@echo off
PUSHD "%~dp0"
REM Extra args are forwarded to _Build.ps1, so "MakeDLLRelease.bat nostop" opts in to -nostoponerror.
call _MakeDLL.bat Release rebuild deploy %*
POPD
REM Force clean exit code
exit /B 0
