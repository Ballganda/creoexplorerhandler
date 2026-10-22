CreoExplorerHandler.dll (x64) — cross-built for Windows

DISCLAIMER — USE AT YOUR OWN RISK
This is an AI-generated code project. It has not been reviewed in any way.
You assume all risk from downloading, installing, or using this software.

Install (must be Administrator — Developer PowerShell is NOT enough):
  1. Put these three files in one permanent folder, e.g.
       C:\Users\user\Projects\CreoExplorerHandler\release\
         CreoExplorerHandler.dll
         CreoExplorerHandler.map
         Register.ps1
  2. Edit CreoExplorerHandler.map if you want custom column ↔ Creo parameter mappings.
  3. Start menu → right-click Windows PowerShell or Terminal → Run as administrator
  4. Then either:

       cd C:\Users\user\Projects\CreoExplorerHandler\release
       Unblock-File .\Register.ps1, .\CreoExplorerHandler.dll, .\CreoExplorerHandler.map
       powershell -NoProfile -ExecutionPolicy Bypass -File .\Register.ps1 -Action Register

     or double-click / run Register.cmd as Administrator:

       Register.cmd -Action Register

     (restarts Explorer)

Unregister:

       powershell -NoProfile -ExecutionPolicy Bypass -File .\Register.ps1 -Action Unregister
       :: or: Register.cmd -Action Unregister

Then open a Creo folder in Details view and enable the mapped columns
(default: Authors, Tags, Comments, Title, Subject).

Map file format (see comments at the top of CreoExplorerHandler.map):
  // comment
  "Authors" "AUTHORS"
  "Company" "COMPANY"
  "System.Document.RevisionNumber" "REV"

Important column keys:
  Classification  -> System.Devices.PrimaryCategory  (NOT Devices.Classification)
  Manufacturer    -> System.Devices.Manufacturer
  Location        -> System.Calendar.Location
  Product version -> System.Software.ProductVersion

If a column is blank, run Discover-ColumnProps.ps1 on the file; it prints the
real PROPERTYKEY behind each Explorer column (MapColumnToSCID).

After adding a new Explorer column name to the map, run Register.ps1 again.
Changing only the Creo parameter on an existing left-side name: F5 is enough
(the DLL reloads the .map when its mtime changes; restart Explorer if needed).

Performance: the handler caches the map in-process, reads at most the first
32 MiB of each Creo file, and extracts all mapped parameters in one scan.
