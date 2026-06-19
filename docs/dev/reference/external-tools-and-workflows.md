# External tools & workflows

Out-of-tree tooling and side-channel workflows used when developing S2S: offline
crash-dump analysis, a known harmless crash, and the sibling repos/binaries that
live outside this tree.

## Crash-dump analysis (offline)

S2S crashes write a minidump to the BTS root via the EXE's own handler
(`CreateMiniDump` in `CvGlobals.cpp`, separate from Windows WER's
`%LOCALAPPDATA%\CrashDumps`):
`C:\Games\Civilization IV Complete\Civ4\Beyond the Sword\MiniDump-*.dmp`.
(Asserts on an Assert build go to `Documents\My Games\Beyond the Sword\Logs\Asserts.log`
+ `AssertsJson.log`; Python tracebacks to `PythonErr.log` — a
`RuntimeError: unidentifiable C++ exception` there is a C++ access violation
propagated out of a boost.python-wrapped DLL call.)

Symbolize the dump **offline** with the Store WinDbg's bundled console debugger
(`cdb`). Civ4 is **32-bit → use the x86 `cdb`**:
`C:\Program Files\WindowsApps\Microsoft.WinDbg_*_x64__8wekyb3d8bbwe\x86\cdb.exe`

Point `-y` at the **local PDB dirs only** (no `srv*` — stays offline/fast). The DLL
PDB lives at `Assets\CvGameCoreDLL.pdb` and `Build\<Config>\CvGameCoreDLL.pdb`; `cdb`
matches by signature, so list all configs. EXE frames stay unresolved (no Firaxis
symbols) — that's fine; our DLL frames resolve.

- **Faulting stack:**
  `cdb -z <dump> -y "C:\code\s2s\s2s\Assets;C:\code\s2s\s2s\Build\Assert;C:\code\s2s\s2s\Build\Release" -c ".ecxr; kp; q"`
- **Fault detail (address + registers + disasm):**
  `... -c ".exr -1; .ecxr; r; u . L4; q"` — `Parameter[1]` of the exception record is
  the bad read/write address; the disasm line at the IP shows which deref. A read at a
  tiny address (e.g. `+0x60`, `0x1`) = null/garbage pointer deref.

Output is verbose (NatVis unload spam) — filter with `Select-String`.

## Known issues

- **Load-after-load `onFinalInitialized` crash (pre-existing, harmless, deferred).**
  Loading a save **after a previous game has already been loaded in the same process**
  throws `RuntimeError: unidentifiable C++ exception` from `CvGame::onFinalInitialized`
  (via `CvEventManager.gameStart` → `onLoadGame`). A fresh EXE start + single load does
  not hit it. Root cause: `reset()` isn't reliably called between in-session loads, so
  `onFinalInitialized` runs over stale team/player/plot state. Commit `f567bbc5` only
  removed a debug-only `FAssert(!m_bFinalInitialized)`; the underlying release-build
  exception remains. Confirmed harmless (the game still loads and plays). To revisit:
  build Assert/Debug (translates the C++ exception to a real message + FAssert line) and
  load a 2nd game in-session to repro; focus on what state survives between loads without
  `reset()`.

## External sibling repos / tools

These live OUTSIDE this repo (separate solutions under `c:\code\s2s\`) — they are not
built by the S2S build chain.

- **FpkBuilder** — `Tools/FpkBuilder.exe` is a **vendored prebuilt binary**; its source
  is a separate solution at `c:\code\s2s\FpkBuilder` (.NET 10 single-file self-contained,
  `src/FpkBuilder/Program.cs`). To change packing: edit there, `dotnet publish -c Release`,
  copy `bin/Release/net10.0/win-x64/publish/FpkBuilder.exe` over `Tools/FpkBuilder.exe`.
  Packing model: driven by `Assets/fpklive_token.txt` (line 1 = HEAD revision at last full
  build, rest = dirty art files). No token → from-scratch build (packs all of `UnpackedArt/`
  into `C2C0.fpk…C2CN.fpk`); token present → incremental (packs only art changed since the
  token revision into `C2CPatch*.fpk`; base FPKs stay byte-identical until the next
  from-scratch rebuild). PakBuild `/S=<MB>` is the per-FPK size cap, set from
  `GetFpkMaxSizeMb` (default 95 MB) to stay under GitHub's 100 MB push limit. CI
  (`DeployBuild.bat`) calls it with no args; an `FPKCLEAN` commit-message path forces a
  from-scratch rebuild.

- **GameTracker** — separate ASP.NET Core Razor Pages dashboard at
  `c:\code\s2s\GameTracker` (repo `https://github.com/Stones2Stars/GameTracker`,
  private until tuned). `dotnet run` → `localhost:5000` (auto-refresh 15s), polling the
  S2S dev HTTP endpoint (`127.0.0.1:7227`). Shows connection state, turn+gameId, census
  cards, score chart, standings, per-city crime/education/disease. Appends per-turn CSVs
  under `data/<gameId>/` (`players_timeseries.csv`, `cities_timeseries.csv` — same schema
  as `Tools/BenchmarkCensusCollector.ps1` — plus `census_timeseries.csv`). Purpose: the
  no-agent path for playtesters to contribute benchmark data (zip `data/<gameId>/`;
  conventions in `Benchmarks/README.md`). Benchmark games are dual-recorded when both the
  collector and GameTracker run, keyed by gameId.
