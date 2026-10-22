#Requires -RunAsAdministrator
param(
    [ValidateSet("Register", "Unregister")]
    [string]$Action = "Register",
    [string]$DllPath = "",
    [switch]$NoExplorerRestart
)

$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($id)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-ExplorerShell {
    Write-Host "Restarting Explorer so Details columns reload property handlers..."
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    Start-Process explorer.exe
}

if (-not (Test-IsAdmin)) {
    throw @"
This script must run in an elevated PowerShell (Run as administrator).

Developer PowerShell for Visual Studio is NOT elevated - open a separate Admin shell:

  Start menu -> right-click Windows PowerShell or Terminal -> Run as administrator

Then:
  cd `"$PSScriptRoot`"
  Unblock-File .\Register.ps1, .\CreoExplorerHandler.dll, .\CreoExplorerHandler.map
  powershell -NoProfile -ExecutionPolicy Bypass -File .\Register.ps1 -Action $Action

Or run Register.cmd as Administrator (same Bypass for one process).
"@
}

if (-not $DllPath) {
    $candidates = @(
        (Join-Path $PSScriptRoot "CreoExplorerHandler.dll"),
        (Join-Path $PSScriptRoot "bin\Release\CreoExplorerHandler.dll"),
        (Join-Path $PSScriptRoot "bin\Debug\CreoExplorerHandler.dll")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $DllPath = $candidate
            break
        }
    }
}

if (-not $DllPath -or -not (Test-Path -LiteralPath $DllPath)) {
    throw @"
DLL not found next to this script or under bin\Release.

Tried:
  $($PSScriptRoot)\CreoExplorerHandler.dll
  $($PSScriptRoot)\bin\Release\CreoExplorerHandler.dll

Copy CreoExplorerHandler.dll into this folder, or pass -DllPath.
"@
}

$DllPath = (Resolve-Path -LiteralPath $DllPath).Path

# Always use 64-bit regsvr32 for this x64 DLL. SysWOW64\regsvr32 would fail to load it.
$regsvr = Join-Path $env:SystemRoot "System32\regsvr32.exe"
if (-not (Test-Path -LiteralPath $regsvr)) {
    throw "regsvr32 not found: $regsvr"
}

$mapPath = Join-Path (Split-Path -Parent $DllPath) "CreoExplorerHandler.map"
if (Test-Path -LiteralPath $mapPath) {
    Write-Host "Mapping:  $mapPath"
} else {
    Write-Host "Mapping:  (none next to DLL - built-in Authors/Tags/Comments/Title/Subject)"
    Write-Host "          Copy CreoExplorerHandler.map next to the DLL to customize columns."
}

Write-Host "$Action using:"
Write-Host "  DLL:     $DllPath"
Write-Host "  regsvr32: $regsvr"
Write-Host "  Admin:   yes"

$argList = @()
if ($Action -eq "Unregister") {
    $argList += "/u"
}
# Non-silent so Windows shows the success/failure MessageBox with the real reason.
$argList += $DllPath

$proc = Start-Process -FilePath $regsvr -ArgumentList $argList -Wait -PassThru
$code = $proc.ExitCode
Write-Host "regsvr32 exit code: $code"

if ($null -eq $code -or $code -ne 0) {
    throw @"
regsvr32 failed (exit code: $code).

Common causes:
  1. Not running elevated (must be Administrator).
  2. Wrong regsvr32 (must be System32 for x64 DLL).
  3. DLL failed to load (rebuild Release x64, install VC++ redistributable if needed).
  4. DllRegisterServer returned an error writing HKLM PropertyHandlers.

Click OK on any regsvr32 MessageBox for the Windows error text.
"@
}

if (-not $NoExplorerRestart) {
    Restart-ExplorerShell
}

if ($Action -eq "Register") {
    Write-Host "Registered successfully."
    Write-Host "Next:"
    Write-Host "  1. Open a Creo folder in a NEW Explorer window."
    Write-Host "  2. View -> Details."
    Write-Host "  3. Right-click column header -> More..."
    Write-Host "  4. Enable Authors, Tags, Comments, Title, Subject (try BOTH entries if a name appears twice)."
    Write-Host "  5. Press F5 to refresh."
} else {
    Write-Host "Unregistered successfully."
}
