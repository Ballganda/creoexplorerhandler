# Windows verification checklist

This Linux environment builds and tests the shared extractor only. Complete these steps on a Windows PC with Visual Studio 2026 (C++ desktop workload).

## Build

`msbuild` is usually not on a normal Command Prompt PATH. Prefer one of:

**PowerShell (recommended):**
```powershell
cd native\CreoExplorerHandler
.\Build.ps1
```

**Or Visual Studio UI:** open `CreoExplorerHandler.sln` → Release | x64 → Build Solution (accept v145 retarget if asked).

**Or** start **Developer PowerShell for VS 2026**, then:
```powershell
msbuild CreoExplorerHandler.sln /p:Configuration=Release /p:Platform=x64
```

If MSBuild is missing, open Visual Studio Installer and confirm workload **Desktop development with C++**.

Confirm `bin\Release\CreoExplorerHandler.dll` exists.

## Register

Elevated PowerShell:

```powershell
cd native\CreoExplorerHandler
.\Register.ps1 -Action Register
```

## Explorer check

1. Copy `testdata\detailstest.prt.2` to a local folder.
2. Open that folder in File Explorer → Details view.
3. Column header → More… → enable Authors, Tags, Comments, Title, Subject.
4. Expected for the sample file:
   - Authors = Authors
   - Tags = Tags
   - Comments = Comments
   - Title = Title
   - Subject = (blank) until SUBJECT is a String parameter in Creo

Or:

```powershell
.\Verify-ExplorerProps.ps1 -Path C:\path\to\detailstest.prt.2
```

## Unregister

```powershell
.\Register.ps1 -Action Unregister
```

## Offline proof already run in CI / Linux

```bash
./scripts/verify_extractor.sh
```

This proves the same parser the DLL links will emit the five field mappings from the sample file.
