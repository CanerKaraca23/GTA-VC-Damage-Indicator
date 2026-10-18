@echo off
setlocal
echo Setting up MSVC environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 (
    echo Error: Failed to setup MSVC build environment.
    exit /b 1
)

if not exist bin mkdir bin

echo Compiling version resource...
rc.exe /nologo /fo bin\resource.res res\resource.rc
if errorlevel 1 (
    echo Resource compilation failed!
    exit /b 1
)

echo Compiling DamageIndicator for GTA San Andreas...
cl /nologo /O2 /MD /LD /EHsc /Febin\DamageIndicator.asi src\DamageIndicatorSA.cpp bin\resource.res /link /INCREMENTAL:NO user32.lib kernel32.lib
if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

copy /Y DamageIndicator.ini bin\DamageIndicator.ini >nul
echo Built bin\DamageIndicator.asi successfully!

if exist "D:\GTA San Andreas" (
    copy /Y bin\DamageIndicator.asi "D:\GTA San Andreas\DamageIndicator.asi" >nul
    copy /Y DamageIndicator.ini "D:\GTA San Andreas\DamageIndicator.ini" >nul
    echo Automatically deployed to D:\GTA San Andreas\
)
