@echo off
setlocal
echo ========================================================
echo Building DamageIndicator for GTA Vice City (x86)
echo ========================================================

where cl >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
    )
)

if not exist bin mkdir bin

cl /nologo /O2 /MD /LD /EHsc src\DamageIndicator.cpp /Febin\DamageIndicator.asi /link /INCREMENTAL:NO user32.lib kernel32.lib

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Built successfully: bin\DamageIndicator.asi
    if exist "D:\GTA Vice City\scripts" (
        copy /Y bin\DamageIndicator.asi "D:\GTA Vice City\scripts\DamageIndicator.asi" >nul
        echo [COPIED] Automatically deployed to D:\GTA Vice City\scripts\DamageIndicator.asi
    )
) else (
    echo.
    echo [ERROR] Build failed.
)

