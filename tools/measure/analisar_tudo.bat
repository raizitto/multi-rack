@echo off
setlocal
rem Analisa todos os .wav de uma pasta (arraste a pasta sobre este arquivo,
rem ou coloque os .wav na mesma pasta deste .bat e de um duplo clique).
cd /d "%~dp0"
where python >nul 2>nul
if errorlevel 1 (
  echo Python nao encontrado.
  echo Instale em https://www.python.org/downloads/ e marque "Add python.exe to PATH".
  pause
  exit /b 1
)
echo Instalando/verificando numpy, scipy e matplotlib...
python -m pip install --quiet --disable-pip-version-check numpy scipy matplotlib
if "%~1"=="" (set "DIR=%~dp0") else (set "DIR=%~1")
for %%F in ("%DIR%\*.wav") do (
  if /I not "%%~nxF"=="test_signal_48k.wav" (
    echo.
    echo === %%~nxF
    python "%~dp0mr_measure.py" analyze "%%~fF"
  )
)
echo.
echo Pronto. Envie os arquivos *_report.json e *_report.png para a pasta measurements do GitHub.
pause
