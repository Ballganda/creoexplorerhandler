# Verifies that Windows Shell sees the five System.* properties for a Creo file
# after CreoExplorerHandler.dll is registered. Run on Windows only.
param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $Path)) {
    throw "File not found: $Path"
}

$shell = New-Object -ComObject Shell.Application
$folder = $shell.NameSpace((Split-Path -Parent (Resolve-Path $Path)))
$item = $folder.ParseName((Split-Path -Leaf (Resolve-Path $Path)))
if (-not $item) {
    throw "Shell could not parse file: $Path"
}

$wanted = @("Authors", "Tags", "Comments", "Title", "Subject")
$matches = @()

for ($i = 0; $i -lt 400; $i++) {
    $name = $folder.GetDetailsOf($null, $i)
    if ($wanted -contains $name) {
        $value = $folder.GetDetailsOf($item, $i)
        $matches += [pscustomobject]@{ Index = $i; Name = $name; Value = $value }
        Write-Host ("[{0}] {1}={2}" -f $i, $name, $value)
    }
}

Write-Host ""
if ($matches.Count -eq 0) {
    Write-Host "No matching columns found. Handler may not be registered."
} else {
    Write-Host "If Explorer Details is blank but values appear above:"
    Write-Host "  - Rebuild + re-register the DLL (newer builds add FullDetails property lists)."
    Write-Host "  - Let Register.ps1 restart Explorer."
    Write-Host "  - In More…, if a name appears twice, enable both and see which fills."
    Write-Host "  - Also enable Comments; reopen the folder and press F5."
}
