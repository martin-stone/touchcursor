@echo off

echo Building exes...

set DEVENV="C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com"
%DEVENV% /build Release touchcursor.sln

if errorlevel 1 exit %errorlevel%

echo.
echo Building distribution...
echo.
cd setup
call makedist.bat
cd ..