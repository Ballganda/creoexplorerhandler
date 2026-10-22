CreoExplorerHandler.dll (x64) — cross-built for Windows

Install (Admin PowerShell):
  1. Copy these three files into one permanent folder, e.g.
       C:\Tools\CreoExplorerHandler\
         CreoExplorerHandler.dll
         CreoExplorerHandler.map
         Register.ps1
  2. Edit CreoExplorerHandler.map if you want custom column ↔ Creo parameter mappings.
  3. cd to that folder
  4. .\Register.ps1 -Action Register
     (restarts Explorer)

Then open a Creo folder in Details view and enable the mapped columns
(default: Authors, Tags, Comments, Title, Subject).

Map file format (see comments at the top of CreoExplorerHandler.map):
  // comment
  "Authors" "AUTHORS"
  "Company" "COMPANY"
  "System.Document.RevisionNumber" "REV"

After adding a new Explorer column name to the map, run Register.ps1 again.
