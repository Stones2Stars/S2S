@echo off
PUSHD "%~dp0"
REM Extra args are forwarded to _Build.ps1, so "MakeDLLAssert.bat nostop" opts in to -nostoponerror.
call _MakeDLL.bat Assert rebuild deploy %*
POPD
