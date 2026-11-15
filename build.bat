@echo off
rem Build ef2.exe with MSVC (x64, static CRT). Run from any prompt; finds Visual Studio via vswhere.
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if not errorlevel 1 goto build

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl >nul 2>nul
if errorlevel 1 goto novs

:build
rc /nologo /fo src\resource.res src\resource.rc
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /EHsc /O2 /GF /MT /W3 /DNDEBUG /DUNICODE /D_UNICODE /Fe:ef2.exe main.cpp src\resource.res /link /MANIFEST:NO
if errorlevel 1 exit /b 1
echo.
echo Build OK: %~dp0ef2.exe
exit /b 0

:novs
echo MSVC build tools not found. Install "Desktop development with C++" or run from a Developer Command Prompt.
exit /b 1
