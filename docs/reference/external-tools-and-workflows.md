# External tools & workflows — out-of-tree tooling and side-channel dev workflows

> **Status:** reference   ·   **Verified against:** 2026-06-20 (working tree, `json-data-migration`)
> **Grounding:** `Sources/Defines/CvGlobals.cpp`, `Tools/` listing, `Tools/CI/DeployBuild.bat`, the
> external repos under `c:\code\s2s\` (FpkBuilder). Line numbers drift — confirm the named
> function, not the integer.
> Out-of-tree tooling and side-channel workflows used when developing S2S that are **not** part of the
> in-tree build/validate loop and have no other home: offline crash-dump symbolization, one known-harmless
> crash, and the sibling binary (`FpkBuilder`) that lives outside this tree. For the
> in-tree tooling (build wrapper, validators, the migration curators, the cascade testers), this doc only
> points — those are documented at their canonical homes; see [§ See also](#see-also).

This doc is deliberately narrow. It catalogues the things you reach for *outside* the normal
edit→`Assert build`→validate→playtest loop. Anything that loop already covers lives where it is owned, not
here (single-source — [`AGENTS.md`](../../AGENTS.md)).

## Crash-dump analysis (offline)

An S2S crash writes a minidump to the **BTS root** via the EXE's own handler — `CreateMiniDump`, called
from the top-level exception filter in `Sources/Defines/CvGlobals.cpp:216` (filter installed at
`:270`). This is separate from Windows WER's `%LOCALAPPDATA%\CrashDumps`:

```
C:\Games\Civilization IV Complete\Civ4\Beyond the Sword\MiniDump-*.dmp
```

Other failure artifacts (all under `Documents\My Games\Beyond the Sword\Logs\`):

- Asserts on an `Assert` build → `Asserts.log` + `AssertsJson.log`.
- Python tracebacks → `PythonErr.log`. A `RuntimeError: unidentifiable C++ exception` there is a **C++
  access violation propagated out of a boost.python-wrapped DLL call** (the boost wrapper can't name it).

### Symbolize the dump

Use the Store WinDbg's bundled console debugger (`cdb`), **offline**. Civ4 is **32-bit → use the x86
`cdb`**:

```
C:\Program Files\WindowsApps\Microsoft.WinDbg_*_x64__8wekyb3d8bbwe\x86\cdb.exe
```

Point `-y` at the **local PDB dirs only** (no `srv*` — keeps it offline and fast). The DLL PDB lives at
`Assets\CvGameCoreDLL.pdb` and `Build\<Config>\CvGameCoreDLL.pdb`; `cdb` matches by signature, so list all
configs. EXE frames stay unresolved (no Firaxis symbols) — that's expected; our DLL frames resolve.

- **Faulting stack:**
  `cdb -z <dump> -y "C:\code\s2s\s2s\Assets;C:\code\s2s\s2s\Build\Assert;C:\code\s2s\s2s\Build\Release" -c ".ecxr; kp; q"`
- **Fault detail (address + registers + disasm):**
  `... -c ".exr -1; .ecxr; r; u . L4; q"` — `Parameter[1]` of the exception record is the bad read/write
  address; the disasm line at the IP shows which deref. A read at a tiny address (e.g. `+0x60`, `0x1`) =
  null/garbage pointer deref.

Output is verbose (NatVis unload spam) — filter with `Select-String`.

## Known harmless crash — load-after-load `onFinalInitialized`

**Pre-existing, harmless, deferred.** Loading a save **after a previous game has already been loaded in the
same process** throws `RuntimeError: unidentifiable C++ exception` from `CvGame::onFinalInitialized` (via
`CvEventManager.gameStart` → `onLoadGame`). A fresh EXE start + a single load does **not** hit it.

- **Root cause:** `reset()` isn't reliably called between in-session loads, so `onFinalInitialized` runs over
  stale team/player/plot state.
- Commit `f567bbc5` only removed a debug-only `FAssert(!m_bFinalInitialized)`; the underlying release-build
  exception remains. Confirmed harmless — the game still loads and plays.
- **To revisit:** build `Assert`/`Debug` (translates the C++ exception into a real message + `FAssert`
  line), load a 2nd game in-session to repro, and focus on what state survives between loads without
  `reset()`.

## External sibling repos / binaries

These live **OUTSIDE this repo** (separate solutions under `c:\code\s2s\`) and are **not** built by the S2S
build chain. They are vendored as prebuilt binaries into `Tools/` (FpkBuilder).

### FpkBuilder — the art-packing tool

`Tools/FpkBuilder.exe` is a **vendored prebuilt binary**; its source is a separate solution at
`c:\code\s2s\FpkBuilder` (.NET 10, single-file self-contained; `src/FpkBuilder/Program.cs`).

- **To change packing:** edit there → `dotnet publish -c Release` → copy
  `bin/Release/net10.0/win-x64/publish/FpkBuilder.exe` over `Tools/FpkBuilder.exe`.
- **Packing model** — driven by `Assets/fpklive_token.txt` (line 1 = HEAD revision at the last full build;
  remaining lines = dirty art files):
  - **No token** → from-scratch build (packs all of `UnpackedArt/` into `C2C0.fpk…C2CN.fpk`).
  - **Token present** → incremental (packs only art changed since the token revision into `C2CPatch*.fpk`;
    base FPKs stay byte-identical until the next from-scratch rebuild).
- `PakBuild /S=<MB>` is the per-FPK size cap, set from `GetFpkMaxSizeMb` (default **95 MB**, in the external
  C# source) to stay under GitHub's 100 MB push limit.
- **CI:** `Tools/CI/DeployBuild.bat` calls `Tools/FpkBuilder.exe` with no args; a commit message containing
  `FPKCLEAN` forces a from-scratch rebuild (`DeployBuild.bat:97`/`:105`).

## See also

The in-tree tooling this doc deliberately does **not** restate — go to its owner:

- [`../../AGENTS.md`](../../AGENTS.md) — **the build wrapper + validators.** `Tools/_Build.ps1` (FastBuild
  wrapper; configs/verbs), `Tools/XmlValidator.exe -a`, `Tools/XMLTools/verify-python-callbacks.py`, and the
  frozen VC7.1/C++03 toolchain facts all live in the root guide's "Build And Test" section.
- [`engine.md`](engine.md) — **why the toolchain is frozen** (the dual Boost
  1.32/1.55 + Python 2.4 stack locked by the closed Firaxis `.exe`), context for the boost.python crash
  signature above.
- [`../../Tools/Migration/README.md`](../../Tools/Migration/README.md) — **the XML→JSON curators**
  (`curate_<entity>.py --write`, `store.py`, `engine.py` and its `--write` superseded-emitter footgun). The
  operational runner reference; the model lives in the cascade specs.
- [`../specs/curators/fixed-point-and-scales.md`](../specs/curators/fixed-point-and-scales.md) — the ×100 fixed-point
  conversion and how the effective value is diffed against the live engine →
  [DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100).
  The `readjson` harness (`Tools/ReadJson/`) is the in-DLL JSON loader's offline driver, exercised by the
  same value-verification flow.
- [`observability.md`](observability.md) — **the live surveillance surface** the crash/known-
  issue debugging reads against (HTTP `127.0.0.1:7227`, `/events`, `/state/*` + `/computed/*`,
  gated logs) → [DEC-obs-scale](../architecture/decisions.md#dec-obs-scale). Delegate bulk data reads to the
  `data-reader` sub-agent rather than pulling raw dumps into context.
- [`../README.md`](../README.md) — the comprehension map (where every subsystem doc lives).
