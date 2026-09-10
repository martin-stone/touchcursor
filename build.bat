@echo off

echo Building exes...

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %VSWHERE% (
    for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set MSBUILD="%%i"
    )
)

if not defined MSBUILD (
    where msbuild >nul 2>&1
    if %errorlevel% equ 0 (
        set MSBUILD=msbuild
    ) else (
        echo Error: Could not locate MSBuild.exe via vswhere or PATH.
        exit /b 1
    )
)

%MSBUILD% touchcursor.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild /m /nologo
if errorlevel 1 exit /b %errorlevel%

echo.
echo Building distribution...
echo.
cd setup
call makedist.bat
cd ..