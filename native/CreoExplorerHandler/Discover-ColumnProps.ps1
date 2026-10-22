# Lists Explorer Details column titles and tries ExtendedProperty names
# so you can see which System.* key feeds a blank column.
param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath
)

$ErrorActionPreference = "Stop"
$item = Get-Item -LiteralPath $FilePath
$shell = New-Object -ComObject Shell.Application
$folder = $shell.Namespace($item.DirectoryName)
$file = $folder.ParseName($item.Name)
if (-not $file) { throw "ParseName failed for $FilePath" }

$candidates = @(
    "System.Devices.Classification",
    "System.SensitivityText",
    "System.Sensitivity",
    "System.Devices.Manufacturer",
    "System.Photo.CameraManufacturer",
    "System.Devices.DeviceManufacturer",
    "System.Calendar.Location",
    "System.Contact.OfficeLocation",
    "System.Supplemental.Location",
    "System.Software.ProductVersion",
    "System.FileVersion"
)

Write-Host "File: $($item.FullName)"
Write-Host ""
Write-Host "=== ExtendedProperty probes ==="
foreach ($name in $candidates) {
    try {
        $val = $file.ExtendedProperty($name)
        if ($null -eq $val -or $val -eq "") {
            Write-Host ("{0,-42} <empty>" -f $name)
        } else {
            Write-Host ("{0,-42} {1}" -f $name, $val)
        }
    } catch {
        Write-Host ("{0,-42} <error: {1}>" -f $name, $_.Exception.Message)
    }
}

Write-Host ""
Write-Host "=== GetDetailsOf columns matching Classification/Manufacturer/Location ==="
for ($i = 0; $i -lt 400; $i++) {
    $title = $folder.GetDetailsOf($null, $i)
    if (-not $title) { continue }
    if ($title -match '^(Classification|Manufacturer|Location|Office location|Camera maker|Sensitivity|Product version)$') {
        $value = $folder.GetDetailsOf($file, $i)
        Write-Host ("[{0,3}] {1,-22} = {2}" -f $i, $title, $value)
    }
}
