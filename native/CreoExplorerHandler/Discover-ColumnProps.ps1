# Probe which System.* key feeds Explorer Details columns for a Creo file.
# Also resolves column index -> PROPERTYKEY via IShellFolder2::MapColumnToSCID.
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
    "System.Devices.Category",
    "System.Devices.CategoryPlural",
    "System.Devices.CategoryGroup",
    "System.Devices.PrimaryCategory",
    "System.Devices.Classification",
    "System.Category",
    "System.SensitivityText",
    "System.Sensitivity",
    "System.Devices.Manufacturer",
    "System.Photo.CameraManufacturer",
    "System.Devices.DeviceManufacturer",
    "System.Calendar.Location",
    "System.Contact.OfficeLocation",
    "System.Supplemental.Location",
    "System.Software.ProductVersion",
    "System.FileVersion",
    "System.Author",
    "System.Title",
    "System.Comment",
    "System.Keywords"
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
Write-Host "=== GetDetailsOf (Classification / Manufacturer / Location / …) ==="
$wanted = @{}
for ($i = 0; $i -lt 400; $i++) {
    $title = $folder.GetDetailsOf($null, $i)
    if (-not $title) { continue }
    if ($title -match '^(Classification|Manufacturer|Location|Office location|Camera maker|Sensitivity|Product version|Categories|Authors|Title|Comments|Tags)$') {
        $value = $folder.GetDetailsOf($file, $i)
        Write-Host ("[{0,3}] {1,-22} = {2}" -f $i, $title, $value)
        $wanted[$title] = [uint32]$i
    }
}

Add-Type -Language CSharp -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class CreoColumnKey {
  [StructLayout(LayoutKind.Sequential)]
  public struct PROPERTYKEY {
    public Guid fmtid;
    public uint pid;
    public override string ToString() {
      return "{" + fmtid.ToString().ToUpperInvariant() + "}, " + pid;
    }
  }

  [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
   Guid("000214E6-0000-0000-C000-000000000046")]
  interface IShellFolder {
    void ParseDisplayName(IntPtr hwnd, IntPtr pbc,
      [MarshalAs(UnmanagedType.LPWStr)] string pszDisplayName,
      out uint pchEaten, out IntPtr ppidl, ref uint pdwAttributes);
    void EnumObjects(IntPtr hwnd, int grfFlags, out IntPtr ppenumIDList);
    void BindToObject(IntPtr pidl, IntPtr pbc, [In] ref Guid riid, out IntPtr ppv);
    void BindToStorage(IntPtr pidl, IntPtr pbc, [In] ref Guid riid, out IntPtr ppv);
    [PreserveSig] int CompareIDs(IntPtr lParam, IntPtr pidl1, IntPtr pidl2);
    void CreateViewObject(IntPtr hwndOwner, [In] ref Guid riid, out IntPtr ppv);
    void GetAttributesOf(uint cidl, IntPtr apidl, ref uint rgfInOut);
    void GetUIObjectOf(IntPtr hwndOwner, uint cidl, IntPtr apidl,
      [In] ref Guid riid, IntPtr rgfReserved, out IntPtr ppv);
    void GetDisplayNameOf(IntPtr pidl, uint uFlags, IntPtr pName);
    void SetNameOf(IntPtr hwnd, IntPtr pidl,
      [MarshalAs(UnmanagedType.LPWStr)] string pszName, uint uFlags, out IntPtr ppidlOut);
  }

  // IID_IShellFolder2 = {93F2F68C-1D1B-11D3-A30E-00C04F79ABD1}
  [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
   Guid("93F2F68C-1D1B-11D3-A30E-00C04F79ABD1")]
  interface IShellFolder2 {
    void ParseDisplayName(IntPtr hwnd, IntPtr pbc,
      [MarshalAs(UnmanagedType.LPWStr)] string pszDisplayName,
      out uint pchEaten, out IntPtr ppidl, ref uint pdwAttributes);
    void EnumObjects(IntPtr hwnd, int grfFlags, out IntPtr ppenumIDList);
    void BindToObject(IntPtr pidl, IntPtr pbc, [In] ref Guid riid, out IntPtr ppv);
    void BindToStorage(IntPtr pidl, IntPtr pbc, [In] ref Guid riid, out IntPtr ppv);
    [PreserveSig] int CompareIDs(IntPtr lParam, IntPtr pidl1, IntPtr pidl2);
    void CreateViewObject(IntPtr hwndOwner, [In] ref Guid riid, out IntPtr ppv);
    void GetAttributesOf(uint cidl, IntPtr apidl, ref uint rgfInOut);
    void GetUIObjectOf(IntPtr hwndOwner, uint cidl, IntPtr apidl,
      [In] ref Guid riid, IntPtr rgfReserved, out IntPtr ppv);
    void GetDisplayNameOf(IntPtr pidl, uint uFlags, IntPtr pName);
    void SetNameOf(IntPtr hwnd, IntPtr pidl,
      [MarshalAs(UnmanagedType.LPWStr)] string pszName, uint uFlags, out IntPtr ppidlOut);
    void GetDefaultSearchGUID(out Guid pguid);
    void EnumSearches(out IntPtr ppenum);
    void GetDefaultColumn(uint dwRes, out uint pSort, out uint pDisplay);
    void GetDefaultColumnState(uint iColumn, out uint pcsFlags);
    void GetDetailsEx(IntPtr pidl, ref PROPERTYKEY pkey, out object pv);
    void GetDetailsOf(IntPtr pidl, uint iColumn, IntPtr psd);
    [PreserveSig] int MapColumnToSCID(uint iColumn, out PROPERTYKEY pscid);
  }

  [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
  static extern int SHParseDisplayName(string name, IntPtr bc, out IntPtr pidl,
      uint sfgaoIn, IntPtr sfgaoOut);

  [DllImport("shell32.dll")]
  static extern int SHBindToParent(IntPtr pidl,
      [MarshalAs(UnmanagedType.LPStruct)] Guid riid,
      out IShellFolder2 folder, out IntPtr child);

  [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
  static extern int PSGetNameFromPropertyKey(ref PROPERTYKEY key, out IntPtr name);

  [DllImport("ole32.dll")]
  static extern void CoTaskMemFree(IntPtr p);

  public static string Resolve(string folderPath, uint column) {
    IntPtr pidl;
    int hr = SHParseDisplayName(folderPath, IntPtr.Zero, out pidl, 0, IntPtr.Zero);
    if (hr != 0) return "SHParseDisplayName hr=0x" + hr.ToString("X8");
    try {
      IShellFolder2 sf;
      IntPtr child;
      Guid iid = new Guid("93F2F68C-1D1B-11D3-A30E-00C04F79ABD1");
      hr = SHBindToParent(pidl, iid, out sf, out child);
      if (hr != 0) return "SHBindToParent hr=0x" + hr.ToString("X8");
      PROPERTYKEY key;
      hr = sf.MapColumnToSCID(column, out key);
      if (hr != 0) return "MapColumnToSCID hr=0x" + hr.ToString("X8");
      IntPtr namePtr;
      int phr = PSGetNameFromPropertyKey(ref key, out namePtr);
      string name = (phr == 0 && namePtr != IntPtr.Zero)
          ? Marshal.PtrToStringUni(namePtr) : "<no canonical name>";
      if (namePtr != IntPtr.Zero) CoTaskMemFree(namePtr);
      return key.ToString() + "  =>  " + name;
    } finally {
      CoTaskMemFree(pidl);
    }
  }
}
"@

Write-Host ""
Write-Host "=== MapColumnToSCID (column index -> PROPERTYKEY / System.* name) ==="
$dir = $item.DirectoryName
foreach ($pair in @(
        @{ Name = "Classification"; Index = 147 },
        @{ Name = "Manufacturer";   Index = 144 },
        @{ Name = "Location";       Index = 46 },
        @{ Name = "Product version"; Index = 298 },
        @{ Name = "Sensitivity";    Index = 293 }
    )) {
    $idx = $pair.Index
    if ($wanted.ContainsKey($pair.Name)) { $idx = $wanted[$pair.Name] }
    try {
        $resolved = [CreoColumnKey]::Resolve($dir, [uint32]$idx)
        Write-Host ("[{0,3}] {1,-22} {2}" -f $idx, $pair.Name, $resolved)
    } catch {
        Write-Host ("[{0,3}] {1,-22} <error: {2}>" -f $idx, $pair.Name, $_.Exception.Message)
    }
}

Write-Host ""
Write-Host "Note: ExtendedProperty/GetDetailsOf often stay blank even when Explorer"
Write-Host "      Details columns show values (Shell.Application cache). Trust"
Write-Host "      MapColumnToSCID for the key, and Explorer Details for the value."
Write-Host "Tip: Classification (col 147) => System.Devices.PrimaryCategory."
Write-Host "     System.Devices.Classification is invalid and will never fill."
