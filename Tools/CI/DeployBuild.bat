@echo off

:: What is going on in this script?
:: 1. Immediately abort this script if we aren't in the designated release branch (it should only run there).
:: 2. Update the version number in the mod XML
:: 3. Compile FinalRelease DLL (and add source indexing)
:: 4. Fetch the latest SVN to a temp dir
:: 4. Build FPK patch against the last SVN FPKs
:: 5. Update the files in the SVN dir with our new ones
:: 6. Generate change logs
:: 7. Commit changed files to SVN
:: 8. Set tag on git containing version number and SVN commit

if "%APPVEYOR_REPO_BRANCH%" neq "%release_branch%" (
    echo Skipping deployment due to not being on release branch
    exit /b 0
)

if "%APPVEYOR_PULL_REQUEST_TITLE%" neq "" (
    echo Skipping deployment due to being a PR build
    exit /b 0
)

PUSHD "%~dp0..\.."

SET C2C_VERSION=v%APPVEYOR_BUILD_VERSION%
SET "root_dir=%cd%"
echo %root_dir%
set SVN=svn.exe
if not exist "%build_dir%" goto :skip_delete
rmdir /Q /S "%build_dir%"

:skip_delete

echo C2C %C2C_VERSION% DEPLOYMENT
echo.

echo %svn_user%
echo %svn_pass%

:: WRITE VERSION TO XML ---------------------------------------
powershell -ExecutionPolicy Bypass -File "%root_dir%\Tools\CI\update-c2c-version.ps1"

:: INIT GIT WRITE ---------------------------------------------
powershell -ExecutionPolicy Bypass -File "%root_dir%\Tools\CI\InitGit.ps1"
set GIT_TERMINAL_PROMPT=0
set "git_push_url=https://%git_access_token%:x-oauth-basic@github.com/Stones2Stars/S2S.git"

:: COMPILE -----------------------------------------------------
echo Building FinalRelease DLL...
call "%root_dir%\Tools\_MakeDLL.bat" FinalRelease build deploy
if not errorlevel 0 (
    echo Building FinalRelease DLL failed, aborting deployment!
    exit /B 2
)
call "%root_dir%\Tools\_TrimFBuildCache.bat"

:: SOURCE INDEXING ---------------------------------------------
:source_indexing
call "%root_dir%\Tools\CI\DoSourceIndexing.bat"

:: CHECK OUT SVN -----------------------------------------------
echo Checking out SVN working copy for deployment...
call %SVN% --quiet checkout %svn_url% "%build_dir%"
if %ERRORLEVEL% neq 0 goto checkoutLoopSetup
goto OK

:checkoutLoopSetup
echo SVN checkout failed... Cleanup
call %SVN% --non-interactive cleanup "%build_dir%"
echo Make 25 more attempts...
set /a count = 0
set /a max = 25
:checkoutLoop
set /a count += 1
call %SVN% --quiet checkout %svn_url% "%build_dir%"
if %ERRORLEVEL% neq 0 (
	if %count% GTR %max% (
		echo SVN checkout failed, aborting...
		exit /B 3
	)
	echo Attempt %count% failed... cleanup
	call %SVN% --non-interactive cleanup "%build_dir%"
	goto checkoutLoop
)
:OK
echo Successfull checkout...

:: PACK FPKS ---------------------------------------------------
:: We copy built FPKs and the fpklive token back from SVN 
:: so we can build a patch FPK against them. This reduces how
:: much we need to push back to SVN, and how much players
:: need to sync
echo Copying FPKs from SVN...

:: Only copy existing FPKs if we didn't request clean FPK build
if not "%APPVEYOR_REPO_COMMIT_MESSAGE:FPKCLEAN=%"=="%APPVEYOR_REPO_COMMIT_MESSAGE%" (
    goto :fpk_live
)
call xcopy "%build_dir%\Assets\*.FPK" "Assets" /Y
call xcopy "%build_dir%\Assets\fpklive_token.txt" "Assets" /Y

:fpk_live
echo Packing FPKs...
call "%root_dir%\Tools\FpkBuilder.exe"
if %ERRORLEVEL% neq 0 (
    echo Packing FPKs failed, aborting deployment
    exit /B 1
)

:: STAGE TO SVN ------------------------------------------------
:: HERE IS WHERE YOU ADJUST WHAT TO PUT IN THE BUILD
echo Updating SVN working copy from git...
set ROBOCOPY_FLAGS=/MIR /NFL /NDL /NJH /NJS /NS /NC
robocopy Assets "%build_dir%\Assets" %ROBOCOPY_FLAGS%
robocopy PrivateMaps "%build_dir%\PrivateMaps" %ROBOCOPY_FLAGS%
robocopy PublicMaps "%build_dir%\PublicMaps" %ROBOCOPY_FLAGS%
robocopy Resource "%build_dir%\Resource" %ROBOCOPY_FLAGS%
robocopy Docs "%build_dir%\Docs" %ROBOCOPY_FLAGS%
xcopy "Stones2Stars.ini" "%build_dir%" /R /Y
xcopy "C2C1.ico" "%build_dir%" /R /Y
xcopy "C2C2.ico" "%build_dir%" /R /Y
xcopy "C2C3.ico" "%build_dir%" /R /Y
xcopy "C2C4.ico" "%build_dir%" /R /Y
xcopy "Tools\CI\S2S.bat" "%build_dir%" /R /Y

echo %APPVEYOR_REPO_COMMIT%
echo %C2C_VERSION%

:: SET TEMP GIT RELEASE TAG -----------------------------------
:: This is temporary so that the change log gets created
:: correctly (it uses origin tags I guess).
:: TODO: update chlog to not require this...
echo Setting release version build tag on git ...
call git tag -a %C2C_VERSION% %APPVEYOR_REPO_COMMIT% -m "%C2C_VERSION%" -f
call git push "%git_push_url%" %C2C_VERSION% --force

:: GENERATE NEW CHANGES LOG ------------------------------------
echo Generate SVN commit description...
call Tools\CI\git-chglog_windows_amd64.exe --output "%root_dir%\commit_desc.md" --config Tools\CI\.chglog\config.yml %C2C_VERSION%

echo Generate forum commit description...
call Tools\CI\git-chglog_windows_amd64.exe --output "%root_dir%\commit_desc.txt" --config Tools\CI\.chglog\config-bbcode.yml %C2C_VERSION%

:: GENERATE FULL CHANGELOG -------------------------------------
:: Regenerated from git tags every release, so it always reflects the full
:: history. Written to the repo root, then staged into both deploy roots.
echo Update full changelog ...
call Tools\CI\git-chglog_windows_amd64.exe --output "%root_dir%\CHANGELOG.md" --config Tools\CI\.chglog\config.yml
:: Stage CHANGELOG.md + the release README (from Docs\MOD-README.md) into the SVN root.
copy /Y "%root_dir%\CHANGELOG.md" "%build_dir%\CHANGELOG.md"
copy /Y "%root_dir%\Docs\MOD-README.md" "%build_dir%\README.md"

:: DELETE TEMP RELEASE TAG -------------------------------------
:: We delete it ASAP so it isn't left up if the build fails
:: below.
call git push "%git_push_url%" --delete %C2C_VERSION%

:: DETECT SVN CHANGES ------------------------------------------
echo Detecting working copy changes...
PUSHD "%build_dir%"
:: One-time cleanup: C2C.bat was renamed to S2S.bat. svn delete is a no-op
:: once it's gone from the working copy, so this self-clears after one release.
if exist "C2C.bat" call %SVN% delete --force "C2C.bat"
call %SVN% status | findstr /R "^!" > ..\missing.list
for /F "tokens=* delims=! " %%A in (..\missing.list) do (svn delete "%%A")
del ..\missing.list 2>NUL
call %SVN% add * --force

:: COMMIT TO SVN -----------------------------------------------
:: SourceForge's HTTP front end times out on a single whole-release transaction
:: (504 Gateway Time-out, then a 500 on the txn) -- the derived JSON alone is
:: over 13000 files and the FPKs are ~256 MB each. SvnBatchCommit.ps1 sends the
:: same payload as a sequence of bounded transactions, retrying each one.
:: Credentials are read from the environment by the script, so the password is
:: never placed on a command line.
if not defined svn_batch_files set svn_batch_files=400
if not defined svn_batch_megabytes set svn_batch_megabytes=64
if not defined svn_batch_retries set svn_batch_retries=5

set "PS_EXE=pwsh"
where /q pwsh || set "PS_EXE=powershell"

echo Commiting new build to SVN in batches of %svn_batch_files% path(s) / %svn_batch_megabytes% MB...
%PS_EXE% -NoProfile -ExecutionPolicy Bypass -File "%root_dir%\Tools\CI\SvnBatchCommit.ps1" -WorkingCopyPath "." -MessageFile "%root_dir%\commit_desc.md" -Version "%C2C_VERSION%" -MaxFilesPerBatch %svn_batch_files% -MaxMegabytesPerBatch %svn_batch_megabytes% -MaxRetriesPerBatch %svn_batch_retries%
if %ERRORLEVEL% neq 0 (
    echo SVN commit failed, aborting...
    exit /B 3
)

:: REFRESH SVN -------------------------------------------------
:: Ensuring that the svnversion call below will give a clean 
:: revision number
echo Refreshing SVN working copy...
call %SVN% --quiet update
if %ERRORLEVEL% neq 0 (
    call %SVN% cleanup --non-interactive
    call %SVN% update
    if %ERRORLEVEL% neq 0 (
        echo SVN update failed, aborting...
        exit /B 3
    )
)

:: SET RELEASE TAG -----------------------------------------
echo Setting SVN commit tag on git ...
for /f "delims=" %%a in ('svnversion') do @set svn_rev=%%a

POPD

:: Add the tag, this time annotated with our SVN ID
call git tag -a %C2C_VERSION% %APPVEYOR_REPO_COMMIT% -m "SVN-%svn_rev%" -f
call git push "%git_push_url%" %C2C_VERSION% --force

:: PUBLISH TO GITHUB -------------------------------------------
:: Mirror the same payload that was committed to SVN into the
:: GitHub distribution repo. Runs after SVN so a GitHub failure
:: cannot block the proven SVN channel. Reuses commit_desc.md and
:: the git credentials configured by InitGit.ps1. The repo must be
:: seeded once locally first (see docs/reference/release-deploy.md).
echo Publishing build to GitHub...
set "git_dist_dir=%root_dir%\_gitbuild"
set "git_dist_url=https://%git_access_token%:x-oauth-basic@github.com/Stones2Stars/Stones2Stars.git"

if not exist "%git_dist_dir%\.git" (
    call git clone --depth 1 "%git_dist_url%" "%git_dist_dir%"
) else (
    call git -C "%git_dist_dir%" fetch --depth 1 origin
    call git -C "%git_dist_dir%" reset --hard origin/main
)

:: Same targets as the SVN staging block above. /XD .git guards the
:: repo metadata against robocopy /MIR purging.
robocopy "%root_dir%\Assets"      "%git_dist_dir%\Assets"      %ROBOCOPY_FLAGS% /XD .git /XF CvGameCoreDLL.pdb
robocopy "%root_dir%\PrivateMaps" "%git_dist_dir%\PrivateMaps" %ROBOCOPY_FLAGS%
robocopy "%root_dir%\PublicMaps"  "%git_dist_dir%\PublicMaps"  %ROBOCOPY_FLAGS%
robocopy "%root_dir%\Resource"    "%git_dist_dir%\Resource"    %ROBOCOPY_FLAGS%
robocopy "%root_dir%\Docs"         "%git_dist_dir%\Docs"        %ROBOCOPY_FLAGS%
xcopy "%root_dir%\Stones2Stars.ini" "%git_dist_dir%" /R /Y
xcopy "%root_dir%\C2C1.ico" "%git_dist_dir%" /R /Y
xcopy "%root_dir%\C2C2.ico" "%git_dist_dir%" /R /Y
xcopy "%root_dir%\C2C3.ico" "%git_dist_dir%" /R /Y
xcopy "%root_dir%\C2C4.ico" "%git_dist_dir%" /R /Y
xcopy "%root_dir%\Tools\CI\S2S.bat" "%git_dist_dir%" /R /Y
:: Binary-safety: ensure the mirror treats every file as binary (no EOL conversion).
copy /Y "%root_dir%\Tools\CI\dist.gitattributes" "%git_dist_dir%\.gitattributes"
:: Keep the ~80 MB debug symbols out of git history.
copy /Y "%root_dir%\Tools\CI\dist.gitignore" "%git_dist_dir%\.gitignore"
:: Root README (from Docs\MOD-README.md) + full changelog.
copy /Y "%root_dir%\Docs\MOD-README.md" "%git_dist_dir%\README.md"
copy /Y "%root_dir%\CHANGELOG.md" "%git_dist_dir%\CHANGELOG.md"

PUSHD "%git_dist_dir%"
call git add -A
call git commit -F "%root_dir%\commit_desc.md"
call git push origin HEAD:main
if %ERRORLEVEL% neq 0 (
    echo GitHub publish failed; SVN deploy already succeeded, continuing...
)
POPD


POPD

@REM echo FORUM COMMIT MESSAGE ----------------------------------------------------------
@REM echo -------------------------------------------------------------------------------
@REM echo.
@REM echo [size=6][b]SVN-%svn_rev%[/b][/size]
@REM type "%root_dir%\commit_desc.txt"
@REM echo.
@REM echo -------------------------------------------------------------------------------
@REM echo -------------------------------------------------------------------------------

echo Done!

exit /B 0
