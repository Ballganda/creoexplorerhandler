@echo off
setlocal
:: Launches Register.ps1 with a process-scoped Bypass so unsigned
:: GitHub release scripts can run under AllSigned / Restricted policies.
:: Still requires an elevated (Administrator) console.

net session >nul 2>&1
if errorlevel 1 (
  echo This must run in an elevated Command Prompt or PowerShell ^(Run as administrator^).
  echo Right-click this file → Run as administrator, or open Admin PowerShell and run:
  echo   "%~f0" %*
  exit /b 1
)

set "SCRIPT=%~dp0Register.ps1"
if not exist "%SCRIPT%" (
  echo Register.ps1 not found next to this .cmd: "%SCRIPT%"
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %ERRORLEVEL%
