@echo off
setlocal
echo ========================================================
echo Building GTA V Damage Indicator for GTA Vice City (x86)
echo ========================================================

where cl >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
    )
)

if not exist bin mkdir bin

cl /nologo /O2 /MD /LD /EHsc src\GTAVDamageIndicator.cpp /Fe:bin\GTAVDamageIndicator.asi /link /INCREMENTAL:NO user32.lib kernel32.lib

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Built successfully: bin\GTAVDamageIndicator.asi
) else (
    echo.
    echo [ERROR] Build failed.
)
pause
