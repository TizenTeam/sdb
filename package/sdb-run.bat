@echo off
set SCRIPT=%0

:: delims is a TAB followed by a space
set KEY=TIZEN_SDK_INSTALLED_PATH

REM find sdk path
set rkey="HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders"
set rval="Local AppData"
FOR /f "tokens=3*" %%a IN ('reg query %rkey% /v %rval%') DO (
    set sdk_conf_path=%%b
)

REM find cli path
FOR /f "tokens=1,2 delims==" %%i IN ('type "%sdk_conf_path%\tizen-sdk-data\tizensdkpath"') DO IF %%i==%KEY% (set SDK_PATH=%%j)
set TOOLS_HOME=%SDK_PATH%\tools

if "%1" == "-s" (
title sdb - %2
) else (
title sdb
)

set EXEC=%TOOLS_HOME%\ansicon.exe %TOOLS_HOME%\sdb.exe %*
%EXEC%
