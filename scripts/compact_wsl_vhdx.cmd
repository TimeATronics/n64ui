@echo off
REM ============================================
REM  WSL VHDX Compaction Script (run as Admin)
REM  Reclaims Windows disk space from the WSL
REM  virtual disk after deleting files inside WSL.
REM  Usage: right-click -> Run as administrator
REM ============================================

echo Shutting down WSL...
wsl --shutdown
timeout /t 5 /nobreak >nul

echo Finding VHDX path from registry...
for /f "tokens=3" %%a in ('reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss" /s /v BasePath ^| findstr /i "BasePath"') do set BASEPATH=%%a
echo VHDX dir: %BASEPATH%
if not exist "%BASEPATH%\ext4.vhdx" (
    echo ERROR: ext4.vhdx not found at %BASEPATH%
    pause
    exit /b 1
)

REM Create diskpart script
set SCRIPT=%TEMP%\compact_vhdx.txt
(
    echo select vdisk file="%BASEPATH%\ext4.vhdx"
    echo attach vdisk readonly
    echo compact vdisk
    echo detach vdisk
    echo exit
) > "%SCRIPT%"

echo Compacting VHDX (this can take several minutes)...
diskpart /s "%SCRIPT%"
del "%SCRIPT%" 2>nul

echo.
echo Done. VHDX compacted. Restart WSL when ready.
pause
