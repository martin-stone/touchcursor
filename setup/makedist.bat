@echo off

set VERSION=1.7.0
set INSTALLER_NAME=TouchCursorSetup-%VERSION%
set ZIP_NAME=TouchCursor-%VERSION%

del ..\TouchCursorSetup*.exe /Q
del ..\TouchCursor*.zip /Q

rem rmdir TouchCursor /S /Q
md TouchCursor
md TouchCursor\docs

copy ..\bin\release\tcconfig.exe TouchCursor
if errorlevel 1 exit %errorlevel%
copy ..\bin\release\touchcursor.exe TouchCursor
if errorlevel 1 exit %errorlevel%
copy ..\bin\release\touchcursor.dll TouchCursor
if errorlevel 1 exit %errorlevel%
copy ..\bin\release\touchcursor_update.exe TouchCursor
if errorlevel 1 exit %errorlevel%

make_docs.py TouchCursor\docs
if errorlevel 1 exit %errorlevel%

copy ..\COPYING.txt TouchCursor
if errorlevel 1 exit %errorlevel%

copy .\redist_deps\*.* TouchCursor
if errorlevel 1 exit %errorlevel%

7z a -tzip ..\%ZIP_NAME%.zip TouchCursor
if errorlevel 1 exit %errorlevel%

"C:\Program Files\Inno Setup 5\iscc.exe" /Q /F%INSTALLER_NAME% setup.iss
if errorlevel 1 exit %errorlevel%

rmdir TouchCursor /S /Q
