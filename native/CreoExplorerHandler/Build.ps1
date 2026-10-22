# Finds MSBuild via vswhere (no PATH required) and builds x64 Release.
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$solution = Join-Path $PSScriptRoot "CreoExplorerHandler.sln"
if (-not (Test-Path $solution)) {
    throw "Solution not found: $solution"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw @"
Visual Studio / vswhere not found at:
  $vswhere

Install Visual Studio 2022/2026 with workload:
  Desktop development with C++

Or open CreoExplorerHandler.sln in Visual Studio and Build > Build Solution (x64 / Release).
"@
}

# Prefer the newest VS install that has MSBuild (works for VS 2022 and 2026).
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
    Select-Object -First 1

if (-not $msbuild) {
    throw @"
MSBuild was not found. In Visual Studio Installer, confirm workload:
  Desktop development with C++
Then reopen PowerShell and run this script again.

Or use Start Menu: "Developer PowerShell for VS 2026"
"@
}

Write-Host "Using MSBuild: $msbuild"
& $msbuild $solution /restore /p:Configuration=$Configuration /p:Platform=$Platform /m
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

$dll = Join-Path $PSScriptRoot "bin\$Configuration\CreoExplorerHandler.dll"
if (-not (Test-Path $dll)) {
    throw "Build finished but DLL missing: $dll"
}

Write-Host "OK: $dll"
Write-Host "Next (elevated PowerShell): .\Register.ps1 -Action Register"
