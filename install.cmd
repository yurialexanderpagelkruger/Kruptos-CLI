@echo off
setlocal

rem Instalador portable de Kruptos. Evita cambiar la politica de ejecucion
rem de forma permanente: solo se aplica a este proceso.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set "RC=%ERRORLEVEL%"

echo.
if not "%RC%"=="0" (
    echo La instalacion ha fallado. Revisa el mensaje anterior.
) else (
    echo Instalacion completada.
)
pause
exit /b %RC%
