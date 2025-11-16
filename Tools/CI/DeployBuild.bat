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

@REM :: WRITE VERSION TO XML ---------------------------------------
@REM powershell -ExecutionPolicy Bypass -File "%root_dir%\Tools\CI\update-c2c-version.ps1"

@REM :: INIT GIT WRITE ---------------------------------------------
@REM powershell -ExecutionPolicy Bypass -File "%root_dir%\Tools\CI\InitGit.ps1"

@REM :: COMPILE -----------------------------------------------------
@REM echo Building FinalRelease DLL...
@REM call "%root_dir%\Tools\_MakeDLL.bat" Debug build deploy
@REM if not errorlevel 0 (
@REM     echo Building FinalRelease DLL failed, aborting deployment!
@REM     exit /B 2
@REM )
@REM call "%root_dir%\Tools\_TrimFBuildCache.bat"

@REM :: SOURCE INDEXING ---------------------------------------------
@REM :source_indexing
@REM call "%root_dir%\Tools\CI\DoSourceIndexing.bat"

@REM :: CHECK OUT SVN -----------------------------------------------
@REM echo Checking out SVN working copy for deployment...
@REM call %SVN% --quiet checkout %svn_url% "%build_dir%"
@REM if %ERRORLEVEL% neq 0 goto checkoutLoopSetup
@REM goto OK

@REM :checkoutLoopSetup
@REM echo SVN checkout failed... Cleanup
@REM call %SVN% --non-interactive cleanup "%build_dir%"
@REM echo Make 25 more attempts...
@REM set /a count = 0
@REM set /a max = 25
@REM :checkoutLoop
@REM set /a count += 1
@REM call %SVN% --quiet checkout %svn_url% "%build_dir%"
@REM if %ERRORLEVEL% neq 0 (
@REM 	if %count% GTR %max% (
@REM 		echo SVN checkout failed, aborting...
@REM 		exit /B 3
@REM 	)
@REM 	echo Attempt %count% failed... cleanup
@REM 	call %SVN% --non-interactive cleanup "%build_dir%"
@REM 	goto checkoutLoop
@REM )
@REM :OK
@REM echo Successfull checkout...

@REM :: PACK FPKS ---------------------------------------------------
@REM :: We copy built FPKs and the fpklive token back from SVN 
@REM :: so we can build a patch FPK against them. This reduces how
@REM :: much we need to push back to SVN, and how much players
@REM :: need to sync
@REM echo Copying FPKs from SVN...

@REM :: Only copy existing FPKs if we didn't request clean FPK build
@REM if not "%APPVEYOR_REPO_COMMIT_MESSAGE:FPKCLEAN=%"=="%APPVEYOR_REPO_COMMIT_MESSAGE%" (
@REM     goto :fpk_live
@REM )
@REM call xcopy "%build_dir%\Assets\*.FPK" "Assets" /Y
@REM call xcopy "%build_dir%\Assets\fpklive_token.txt" "Assets" /Y

@REM :fpk_live
@REM echo Packing FPKs...
@REM call "%root_dir%\Tools\FPKLive.exe"
@REM if %ERRORLEVEL% neq 0 (
@REM     echo Packing FPKs failed, aborting deployment
@REM     exit /B 1
@REM )

@REM :: STAGE TO SVN ------------------------------------------------
@REM :: HERE IS WHERE YOU ADJUST WHAT TO PUT IN THE BUILD
@REM echo Updating SVN working copy from git...
@REM set ROBOCOPY_FLAGS=/MIR /NFL /NDL /NJH /NJS /NS /NC
@REM robocopy Assets "%build_dir%\Assets" %ROBOCOPY_FLAGS%
@REM robocopy PrivateMaps "%build_dir%\PrivateMaps" %ROBOCOPY_FLAGS%
@REM robocopy PublicMaps "%build_dir%\PublicMaps" %ROBOCOPY_FLAGS%
@REM robocopy Resource "%build_dir%\Resource" %ROBOCOPY_FLAGS%
@REM robocopy Docs "%build_dir%\Docs" %ROBOCOPY_FLAGS%
@REM xcopy "Caveman2Cosmos.ini" "%build_dir%" /R /Y
@REM xcopy "C2C1.ico" "%build_dir%" /R /Y
@REM xcopy "C2C2.ico" "%build_dir%" /R /Y
@REM xcopy "C2C3.ico" "%build_dir%" /R /Y
@REM xcopy "C2C4.ico" "%build_dir%" /R /Y
@REM xcopy "Tools\CI\C2C.bat" "%build_dir%" /R /Y

@REM :: SET TEMP GIT RELEASE TAG -----------------------------------
@REM :: This is temporary so that the change log gets created
@REM :: correctly (it uses origin tags I guess).
@REM :: TODO: update chlog to not require this...
@REM @REM echo Setting release version build tag on git ...
@REM @REM call git tag -a %C2C_VERSION% %APPVEYOR_REPO_COMMIT% -m "%C2C_VERSION%" -f
@REM @REM call git push --tags

@REM @REM :: GENERATE NEW CHANGES LOG ------------------------------------
@REM @REM echo Generate SVN commit description...
@REM @REM call Tools\CI\git-chglog_windows_amd64.exe --output "%root_dir%\commit_desc.md" --config Tools\CI\.chglog\config.yml %C2C_VERSION%

@REM @REM echo Generate forum commit description...
@REM @REM call Tools\CI\git-chglog_windows_amd64.exe --output "%root_dir%\commit_desc.txt" --config Tools\CI\.chglog\config-bbcode.yml %C2C_VERSION%

@REM @REM :: GENERATE FULL CHANGELOG -------------------------------------
@REM @REM echo Update full SVN changelog ...
@REM @REM call Tools\CI\git-chglog_windows_amd64.exe --output "%build_dir%\CHANGELOG.md" --config Tools\CI\.chglog\config.yml

@REM @REM :: DELETE TEMP RELEASE TAG -------------------------------------
@REM @REM :: We delete it ASAP so it isn't left up if the build fails
@REM @REM :: below.
@REM @REM call git push origin --delete %C2C_VERSION%

@REM :: DETECT SVN CHANGES ------------------------------------------
@REM echo Detecting working copy changes...
@REM PUSHD "%build_dir%"
@REM call %SVN% status | findstr /R "^!" > ..\missing.list
@REM for /F "tokens=* delims=! " %%A in (..\missing.list) do (svn delete "%%A")
@REM del ..\missing.list 2>NUL
@REM call %SVN% add * --force

@REM :: COMMIT TO SVN -----------------------------------------------
@REM echo Commiting new build to SVN...
@REM call %SVN% commit -F "%root_dir%\commit_desc.md" --non-interactive --no-auth-cache --username %svn_user% --password %svn_pass%
@REM if %ERRORLEVEL% neq 0 (
@REM     call %SVN% cleanup --non-interactive
@REM     call %SVN% commit -F "%root_dir%\commit_desc.md" --non-interactive --no-auth-cache --username %svn_user% --password %svn_pass%
@REM     if %ERRORLEVEL% neq 0 (
@REM         echo SVN commit failed, aborting...
@REM         exit /B 3
@REM     )
@REM )

@REM :: REFRESH SVN -------------------------------------------------
@REM :: Ensuring that the svnversion call below will give a clean 
@REM :: revision number
@REM echo Refreshing SVN working copy...
@REM call %SVN% --quiet update
@REM if %ERRORLEVEL% neq 0 (
@REM     call %SVN% cleanup --non-interactive
@REM     call %SVN% update
@REM     if %ERRORLEVEL% neq 0 (
@REM         echo SVN update failed, aborting...
@REM         exit /B 3
@REM     )
@REM )

@REM :: SET RELEASE TAG -----------------------------------------
@REM @REM echo Setting SVN commit tag on git ...
@REM @REM for /f "delims=" %%a in ('svnversion') do @set svn_rev=%%a

@REM @REM POPD

@REM @REM :: Add the tag, this time annotated with our SVN ID
@REM @REM call git tag -a %C2C_VERSION% %APPVEYOR_REPO_COMMIT% -m "SVN-%svn_rev%" -f
@REM @REM call git push --tags


@REM @REM POPD

@REM @REM echo FORUM COMMIT MESSAGE ----------------------------------------------------------
@REM @REM echo -------------------------------------------------------------------------------
@REM @REM echo.
@REM @REM echo [size=6][b]SVN-%svn_rev%[/b][/size]
@REM @REM type "%root_dir%\commit_desc.txt"
@REM @REM echo.
@REM @REM echo -------------------------------------------------------------------------------
@REM @REM echo -------------------------------------------------------------------------------

echo Done!

exit /B 0
