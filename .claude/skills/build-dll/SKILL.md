---
name: build-dll
description: Build the S2S CvGameCoreDLL via FastBuild. Use when the user wants to
  compile the DLL, run a compile check after editing Sources/, do a clean rebuild,
  or deploy the built DLL into Assets/. Also use to diagnose LNK2001 link errors.
---

# Build the CvGameCoreDLL

The build is driven by `Tools/_Build.ps1` (a FastBuild wrapper). Always run it
**from the `Sources/` directory**.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb> [<verb> ...]
```

- **Configs:** `Assert`, `Debug`, `Release`, `FinalRelease`, `Profile`, `ProfileExtra`.
  Output → `Build/<Config>/CvGameCoreDLL.dll` (+ `.pdb`).
- **Verbs (composable, in order):** `clean`, `build` (incremental), `rebuild` (clean+build), `deploy` (xcopy DLL/PDB into `Assets/`).

## Choosing what to run

- **Quick compile check after an edit** (the default for verifying a change builds):
  `Assert build`. Incremental link is ~30s.
- **Clean rebuild:** `Assert rebuild` (or another config). Several minutes
  (~25 unity batches × ~30s).
- **Build + install into the game:** add `deploy`, e.g. `Release rebuild deploy`.
- The `Tools/MakeDLL*.bat` shortcuts always do a full `rebuild deploy` — don't use
  them for an iterative compile-check loop.

## After the build

- C++ code must stay **C++2003-only** (no `auto`, lambdas, range-for, `nullptr`,
  `override`). See `Sources/AGENTS.md`.
- If the change touched XML/Python interfaces, validate:
  `Tools/XmlValidator.exe -a` and `Tools/XMLTools/verify-python-callbacks.py`.

## Troubleshooting LNK2001

If FastBuild fails at **link** with `LNK2001: unresolved external symbol` (but the
IDE compiles fine), a new `Sources/<Dir>/` is probably missing from the build's
source-of-truth. `Sources/fbuild.bff` — not the `.vcxproj` — decides what FastBuild
compiles. Fix:

1. Add `$SOURCE_DIR$/<Dir>` to the `.UnityInputPath` array in `Sources/fbuild.bff` (~line 201).
2. Add the files to `Sources/C2C (VS2019).vcxproj` and `…vcxproj.filters` (IDE display only).

`Sources/Repos/` and `Sources/Utils/` are reference examples of a correctly wired subdirectory.
