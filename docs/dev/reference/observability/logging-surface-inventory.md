# Observability — logging surface inventory — every emit site, gate, and off-standard sink

> **Status:** reference   ·   **Verified against:** 2026-06-20 (sink helpers re-confirmed in `Sources/AI/BetterBTSAI.h`; the census itself is carried from the 2026-06-18 multi-agent call-site sweep and re-grounded to the reorganized `Sources/` tree — counts/line numbers are sweep-era anchors and drift)
> **Grounding:** the 2026-06-18 `Sources/` call-site sweep (all logging primitives across all slices), with the sink/helper definitions re-confirmed against `Sources/AI/BetterBTSAI.{h,cpp}` and paths re-grounded to the post-reorg buckets. The anomaly catalog is the sweep's findings, not re-swept for this rebuild.
> This is the **census** behind the [`ai-logging-reference.md`](ai-logging-reference.md) registry: where every log line is *emitted*, what *gate* fires it, and which sinks bypass the standard helper path. It maps the current surface so the consolidation can route it onto one path; [`ai-logging-reference.md`](ai-logging-reference.md) is the wire spec (tags/fields), this is the call-site map + the gap.

The observability scale, the three hook shapes, and the Orwell bar are in [`README.md`](README.md)
([DEC-obs-scale], [DEC-obs-hook-shapes]). The `/events` tee and the live-read rules (the game holds `.log`
files open mid-session — don't tail them live) are in [`http-server.md`](http-server.md). This doc does not
restate them.

> **The governing rule** (owner, 2026-06-18): **log LINES are stable — tags/fields do not change. What
> evolves is WHERE they are SENT.** The consolidation is a routing change, not a content change. Exception:
> a line that doesn't fit the new structure is dropped and redone, not preserved out of inertia.

> Line/count anchors below are from the sweep and **drift** — confirm the named function, not the integer.
> Paths are re-grounded to the post-reorg tree: `Cv*` engine → `Sources/Engine/`; `Cv*AI` + `BetterBTSAI` +
> `CvContractBroker` → `Sources/AI/`; `CvHttpServer` + `FAssert` → `Sources/Tools/`; cascade →
> `Sources/Cascade/`; `CvGlobals` → `Sources/Defines/`; `CvGameTextMgr` → `Sources/UI/`; `PlotSnapshot` →
> `Sources/Utils/`; `CvGameCoreDLL.h` at the `Sources/` root.

---

## 1. The route table (merged + deduped)

Routes grouped by primitive family. "count" = call-site instances found in the sweep; "sample" = one
representative location. The gate column records what determines whether the call fires.

### 1A. BBAI `log<Domain>AI` helpers (the standard AI logging surface)

Every helper in this family: calls `gDLL->logMsg(file, line)` (file write); calls `streamLogTee(level, line)`
(`/events` SSE tee); and is gated by a scope global checked *before* the call by the helper itself. (The
`OutputDebugString` echo each helper used to append was removed — anomaly A-1, resolved.)

| Primitive | Tag | Dest file | Gate global | Count | Sample (post-reorg) |
|---|---|---|---|---|---|
| `logBuildEvaluation` | `[WAI/*]` | `BuildEvaluation.log` | `gPlayerLogLevel` | 43 | `Sources/AI/CvWorkerAI.cpp` |
| `logCityAI` | `[CIT/*]` | `CityAI.log` | `gCityLogLevel` | 23 | `Sources/Engine/CvCity.cpp` |
| `logUnitAI` | `[UNT/*]` | `UnitAI.log` | `gUnitLogLevel` | 18 | `Sources/AI/CvUnitAI.cpp` |
| `logHunterAI` | `[HAI/*]` | `HunterAI.log` | `gUnitLogLevel` | 54 | `Sources/AI/CvHunterAI.cpp` |
| `logCombatAI` | `[COM/*]` | `CombatAI.log` | `gUnitLogLevel` | 10 | `Sources/AI/CvUnitAI.cpp` |
| `logGroupAI` | `[GRP/*]` | `GroupAI.log` | `gUnitLogLevel` | 7 | `Sources/AI/CvSelectionGroupAI.cpp` |
| `logFoundAI` | `[FND/*]` | `FoundAI.log` | `gPlayerLogLevel` | 1 | `Sources/AI/CvUnitAI.cpp` |
| `logDecisionAI` | `[DAI/*]` | `DecisionAI.log` | `gPlayerLogLevel` | 12 | `Sources/AI/CvDecisionAI.cpp` |
| `logDiploAI` | `[DIP/*]` | `DiploAI.log` | `gPlayerLogLevel` | 9 | `Sources/AI/CvPlayerAI.cpp` |
| `logEspionageAI` | `[ESP/*]` | `EspionageAI.log` | `gPlayerLogLevel` | 2 | `Sources/AI/CvPlayerAI.cpp` |
| `logWarAI` | `[WAR/*]` | `WarAI.log` | `gTeamLogLevel` | 3 | `Sources/AI/CvTeamAI.cpp` |
| `logContractBroker` | `[CTB/*]` | `ContractBroker.log` | `gPlayerLogLevel` | 66 | `Sources/AI/CvContractBroker.cpp` |
| `logEngine` | `[ENG/*]` | `Engine.log` | `gTeamLogLevel` | 1 | `Sources/AI/BetterBTSAI.cpp` |
| `logInitInfo` (ex-`logGameInfo`) | `[INIT/*]` (ex-`[GAME/*]`) | `GameInfo.log` | caller-gated (any logging active — see A-2) | 5 | `Sources/AI/BetterBTSAI.cpp` |
| `logCB` | (none) | `CB.log` | **ungated — dead, see A-3/A-21** | 1 | `Sources/AI/BetterBTSAI.cpp` |
| `logToFile` | (none) | *(caller-supplied)* | **ungated — dead, see A-3/A-21** | 1 | `Sources/AI/BetterBTSAI.cpp` |

**Domain → scope global cross-reference** (the four AI globals are aliases of `gPlayerLogLevel` — see §2A):

| Scope | Domains | Global | BUG option |
|---|---|---|---|
| Player | `[WAI]` `[DAI]` `[DIP]` `[ESP]` `[FND]` `[CTB]` | `gPlayerLogLevel` | `Autolog__LogLevelPlayerBBAI` |
| Team | `[WAR]` `[ENG]` | `gTeamLogLevel` (alias) | (same) |
| City | `[CIT]` | `gCityLogLevel` (alias) | (same) |
| Unit | `[UNT]` `[HAI]` `[COM]` `[GRP]` | `gUnitLogLevel` (alias) | (same) |

### 1B. PERF timers

| Primitive | Tag | Dest file | Gate global | Count | Sample |
|---|---|---|---|---|---|
| `logPerf` (direct) | `[PERF/phase\|unitai]` | `Performance.log` | `gPerfLogLevel` | ~30 | `Sources/Engine/CvGame.cpp` |
| `PERF_SCOPE` (`ScopedPerfTimer` dtor → `logPerf`) | `[PERF/phase]` | `Performance.log` | `gPerfLogLevel` | ~80 | `Sources/Engine/CvGame.cpp` |
| `PERF_ACCUM` (accumulator; flushed by `logPerf`) | *(no direct emit)* | `Performance.log` | `gPerfLogLevel` (at flush) | ~57 | `Sources/Engine/CvGame.cpp` |

### 1C. Event spine / cascade observability

| Primitive | Tag | Dest file | Gate | Count | Sample |
|---|---|---|---|---|---|
| `eventSpine().emit(DOMAIN/DIAGNOSTIC)` → `CvCascadeLogConsumer::onEvent` → `gDLL->logMsg` | `[SPINE/DOMAIN]` / `[SPINE/<KIND>]` | `Cascade.log` | `gPlayerLogLevel >= 1` (inline in `onEvent`) | 6 | `Sources/Cascade/CvEventSpine.cpp` |
| `rjLogLine` (file-scope wrapper) → `gDLL->logMsg` + `streamLogTee(1, ...)` | `[READJSON]` `[PLACEMENT]` `[DORMANCY]` `[STATE/game\|fin\|dip\|city]` | `Cascade.log` | `gPlayerLogLevel >= N` at each call site | 21 | `Sources/Cascade/CvCascadeReadJson.cpp` |

### 1D. SSE live stream

| Primitive | Dest | Gate | Count | Sample |
|---|---|---|---|---|
| `streamLogTee(level, line)` | `/events` SSE via `CvHttpServer::publishEvent` | `level <= gStreamLogLevel` AND `CvHttpServer::isEnabled()` | ~14 defs; called inside every BBAI helper | `Sources/AI/BetterBTSAI.cpp` |
| `CvHttpServer::publishEvent` (direct, outside helpers) | `/events` SSE | `CvHttpServer::isEnabled()` (+ `isHuman()` for player-turn events) | 7 | `Sources/Engine/CvGame.cpp` |

### 1E. Infrastructure / diagnostic logs (NOT the AI surface)

These sinks exist for infrastructure reasons (XML load, OOS, crash, profiler). They are **out of scope** for
the AI-logging consolidation but catalogued for completeness.

| Primitive | Dest file | Gate | Sample |
|---|---|---|---|
| `gDLL->logMsg` (crash handler) | `PythonCallstack.log` | on unhandled exception / minidump | `Sources/Defines/CvGlobals.cpp` |
| `logging::logMsg` | `Xml_MissingTypes.log` | every `getInfoTypeForString` miss | `Sources/Defines/CvGlobals.cpp` |
| `logging::logMsg` | `cvInternalGlobals_logInfoTypeMap.log` | explicit `logInfoTypeMap()` call | `Sources/Defines/CvGlobals.cpp` |
| `logging::logMsg` | `Checksum.log` | inside `getAssetCheckSum()` | `Sources/Defines/CvGlobals.cpp` |
| `gDLL->logMsg` (IFP profiler) | `IFP_log.txt` | `#ifdef USE_INTERNAL_PROFILER` only | `Sources/CvGameCoreDLL.cpp` |
| `gDLL->logMsg` (multiplayer net) | `Player N - Multiplayer Game Log.log` | `isNetworkMultiPlayer()` | `Sources/Engine/CvGame.cpp` |
| `gDLL->logMsg` (OOS special) | `OOSSpecialLogger - Player N - Set N.log` | `isNetworkMultiPlayer()` | `Sources/Engine/CvGame.cpp` |
| `logging::logMsg` (RNG trace) | `RandomLogger - Player N - Set N.log` | `isNetworkMultiPlayer() && isFinalInitialized()` (+ `#ifdef _DEBUG` variant) | `Sources/Engine/CvGame.cpp` |
| `logging::logMsg` (FAssert) | `Asserts.log` + `AssertsJson.log` | `#ifdef FASSERT_LOGGING` (Assert/Debug only) | `Sources/Tools/FAssert.cpp` |
| `logging::logMsg` | `xml.log` | XML load path | `Sources/Engine/CvXMLLoadUtility.cpp` |
| `DEBUG_LOG` (→ `logging::logMsg` in `_DEBUG`) | `XmlCheckDoubleTypes.log` / `MLF.log` / `…MLFEnumerateFiles.log` | `#ifdef _DEBUG` | `Sources/Engine/CvXMLLoadUtilitySet.cpp` |
| `gDLL->logMsg` (modular-art debug) | `CvXMLLoadUtilityModTools_isModularArt.log` | `#if (DEBUG_IS_MODULAR_ART == 1)` | `Sources/Engine/CvXMLLoadUtilityModTools.cpp` |
| `gDLL->messageControlLog` | `MessageControl.log` | `GC.getLogging()` | `Sources/Engine/CvCity.cpp` |
| `logging::logMsg` | `bull.log` | once at BUG init | `Sources/Engine/CvBugOptions.cpp` |
| `std::fopen/fprintf/fclose` (PlotSnapshot) | `PlotSnapshot_<tag>_t<N>.csv` | unconditional each turn (see A-18) | `Sources/Utils/PlotSnapshot.cpp` |
| `std::ofstream` (BuildingsBuiltTable) | `BuildingsBuiltTable.csv` | player hotkey | `Sources/Engine/CvMap.cpp` |

### 1F. Ungated `logging::logMsg` to `C2C.log` (firehose — cleanup targets)

Highest-severity ungated sinks: write to `C2C.log` on every AI turn with zero level gate, NOT part of the
standard BBAI surface. **Ruled DELETE** (R-4: `C2C.log` is retired — legacy traces, no documented consumers).

| Call site (post-reorg) | Content | Frequency |
|---|---|---|
| `Sources/Engine/CvGame.cpp` (+ ~10 more) | `[Flexible Difficulty]` — top of `doFlexibleDifficulty()` | per-turn unconditional |
| `Sources/Engine/CvGame.cpp` | doSpawns loop | per-spawnInfo per player per turn |
| `Sources/Engine/CvGameCoreUtils.cpp` | `[CALENDAR]` era tick | per `calculateCurrentTick()` |
| `Sources/Engine/CvTeam.cpp` | minor/full civ transitions + barb war | on `setMinorCiv()` + related |
| `Sources/Engine/CvPlot.cpp` | resource depletion event | per depletion (probabilistic/turn) |
| `Sources/UI/CvGameTextMgr.cpp` | religion icon billboard | every city billboard render — **high-frequency UI path** |

### 1G. `OutputDebugString` (Win32 debugger sink — not a file)

> **⚠ `OutputDebugString` is `#define`d to nothing under `#ifdef FINAL_RELEASE`** (verified against
> `Sources/CvGameCoreDLL.h`, the `FINAL_RELEASE` block) — so it **compiles out entirely in the build players
> run.** The calls ARE live in **Release / Assert / Debug** (the owner's *test* builds — and the owner runs
> Release for cascade testing), so they're a **dev/test-build** firehose + clutter, NOT a shipped one. Any
> "fires in FinalRelease, CRIT" framing is therefore wrong for the shipped build. R-5 (retire the mechanism)
> still stands — the owner doesn't use `OutputDebugString` — but the urgency is dev-hygiene. (Crash-handler /
> `StackWalker` / `CvAllocator` uses are legitimate — they run when the normal logger may be dead — and KEEP it.)

Two populations: `#ifdef _DEBUG`-gated, and otherwise-ungated (live in Release/Assert/Debug; compiled out in
FinalRelease per above). The ungated-in-dev-builds population is the R-5 cleanup target.

**Hotspots (fire in Release/Assert/Debug with a debugger/ETW attached; NO-OP in FinalRelease):**

| Location (post-reorg) | Content | Frequency |
|---|---|---|
| `Sources/Engine/CvUnit.cpp` (move + reposition) | unit move coordinates / setXY | EVERY unit move / every combat-capable setXY |
| `Sources/Engine/CvSelectionGroup.cpp` (~12 sites) | mission start/continue + queue mutations | every mission step / enqueue / dequeue |
| `Sources/AI/CvContractBroker.cpp` (4 sites) | work-request operations | per-unit per-request in broker inner loop |
| `Sources/AI/CvPlayerAI.cpp` | plot-danger hot path; AI tech choice; GPT trade loop; conquest | per-unit per-turn / per research pick / inner while-loop |
| `Sources/Engine/CvGameCoreUtils.cpp` | pathfinder cost tracing | guarded by hardcoded-`false` `bTrace` local — currently DEAD (one line from millions/turn) |
| `Sources/Engine/CvGame.cpp` (+ ~11 more) | lifecycle markers | per lifecycle event + some per-turn |
| `Sources/Engine/CvMap.cpp` (6 sites) | map constructor / read breadcrumbs | per load |
| `Sources/Engine/CvCity.cpp` | city panel open; no trainable unit; project completion | per panel open / per empty cache rebuild / per project |
| `Sources/AI/CvCityAI.cpp` | "No buildable defender!!"; building-value cache init; build re-eval skip | per cache-hit miss / per NULL cache / per fast-path skip |

---

## 2. Gate map and file map

### 2A. Scope globals and BUG options

Set in `CvGlobals::refreshOptionsBUG` (`Sources/Defines/CvGlobals.cpp`).

| Global | BUG option key | Effective scope | Note |
|---|---|---|---|
| `gPlayerLogLevel` | `Autolog__LogLevelPlayerBBAI` | `[WAI]` `[DAI]` `[DIP]` `[ESP]` `[FND]` `[CTB]` + cascade | **the master** — also drives the other three AI globals |
| `gTeamLogLevel` | (same) | `[WAR]` `[ENG]` | **alias** — per-scope Team BUG option exists in XML, explicitly ignored in code |
| `gCityLogLevel` | (same) | `[CIT]` | **alias** |
| `gUnitLogLevel` | (same) | `[UNT]` `[HAI]` `[COM]` `[GRP]` | **alias** |
| `gPerfLogLevel` | `Autolog__LogLevelPerf` | `[PERF]` turn timing | independent — intentionally separate from AI verbosity |
| `gStreamLogLevel` | `Autolog__LogLevelStream` | `/events` SSE tee ceiling | default 1; lines `<=` this level are also streamed (and only if the file gate also passed) |

**Critical implementation note:** `refreshOptionsBUG` sets `gPlayerLogLevel`, `gTeamLogLevel`,
`gCityLogLevel`, AND `gUnitLogLevel` from the *single* `Autolog__LogLevelPlayerBBAI` option; the per-scope
options (`Autolog__LogLevelTeamBBAI` etc.) exist in the UI but their values are deliberately discarded. The
four globals are aliases in practice. The consolidation replaces this with one coherent "Surveillance / log
level" knob matching the 0–5 tier naming ([DEC-obs-scale]) — see
[`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md).

### 2B. File map — every distinct log destination

| Log file | Written by | Gate | Notes |
|---|---|---|---|
| `BuildEvaluation.log` | `logBuildEvaluation` | `gPlayerLogLevel` | `[WAI/*]` |
| `CityAI.log` | `logCityAI` | `gCityLogLevel` | `[CIT/*]` |
| `UnitAI.log` | `logUnitAI` | `gUnitLogLevel` | `[UNT/*]` |
| `HunterAI.log` | `logHunterAI` | `gUnitLogLevel` | `[HAI/*]` |
| `CombatAI.log` | `logCombatAI` | `gUnitLogLevel` | `[COM/*]` |
| `GroupAI.log` | `logGroupAI` | `gUnitLogLevel` | `[GRP/*]` |
| `FoundAI.log` | `logFoundAI` | `gPlayerLogLevel` | `[FND/*]` |
| `DecisionAI.log` | `logDecisionAI` | `gPlayerLogLevel` | `[DAI/*]` |
| `DiploAI.log` | `logDiploAI` | `gPlayerLogLevel` | `[DIP/*]` |
| `EspionageAI.log` | `logEspionageAI` | `gPlayerLogLevel` | `[ESP/*]` |
| `WarAI.log` | `logWarAI` | `gTeamLogLevel` | `[WAR/*]` |
| `ContractBroker.log` | `logContractBroker` | `gPlayerLogLevel` | `[CTB/*]` |
| `Engine.log` | `logEngine` | `gTeamLogLevel` | `[ENG/*]` |
| `GameInfo.log` | `logInitInfo` | caller-gated (any logging active) | `[INIT/*]` session header |
| `CB.log` | `logCB` | ungated | dead; no live callers (A-3/A-21) |
| `Cascade.log` | `CvCascadeLogConsumer::onEvent`; `rjLogLine` | `gPlayerLogLevel >= 1` | cascade spine / readjson / placement |
| `Performance.log` | `logPerf`, `ScopedPerfTimer` | `gPerfLogLevel` | `[PERF/*]` |
| `C2C.log` | `logging::logMsg` (scattered) | *(mostly ungated)* | legacy catch-all firehose — **ruled DELETE** (§1F, R-4) |
| `/events` (SSE) | `CvHttpServer::publishEvent` via `streamLogTee` or direct | `gStreamLogLevel` + `CvHttpServer::isEnabled()` | the live observability surface |

(Infra-only sinks — `PythonCallstack.log`, `Xml_MissingTypes.log`, `Checksum.log`, `Asserts.log`, `xml.log`,
the OOS/multiplayer logs, `PlotSnapshot_*.csv`, etc. — are listed in §1E and are out of scope for the AI
consolidation.)

---

## 3. Anomalies — logging that bypasses the standard path

The cleanup targets for the consolidation. Severity: **CRIT** = high-frequency firehose, **HIGH** = ungated
but lower frequency, **MED** = compile-flag/condition-gated but bypasses helpers, **LOW** = dead/debug-only.
(All "fires in FinalRelease" CRIT framing on `OutputDebugString` is corrected by the §1G note — those compile
out of the shipped build; they fire in Release/Assert/Debug.)

- **A-1 ✅ RESOLVED:** `OutputDebugString` echo removed from every BBAI helper (`Sources/AI/BetterBTSAI.cpp`).
  It was a pure DUPLICATE sink (data already in the file + on `/events`); all 15 echoes deleted. Retires
  `OutputDebugString` from the entire AI-logging surface (R-5, highest-value slice).
- **A-2 ✅ RESOLVED:** `logGameInfo` was thought ungated, but its sole caller (`CvGame::onFinalInitialized`)
  already gates it on "any logging active". Per R-3 the session header is intentionally caller-gated and
  kept; renamed `logGameInfo`→`logInitInfo`, tag `[GAME/*]`→`[INIT/*]` (the old tag clashed with the per-turn
  `[STATE/game]` feed).
- **A-3 HIGH:** `logCB` / `logToFile` (`Sources/AI/BetterBTSAI.cpp`) use `logging::logMsg` (not
  `gDLL->logMsg`), have no `streamLogTee` and no echo — bypass all arms of the helper pattern. `logToFile`
  takes a caller-supplied filename (fully dynamic destination). No `.cpp` callers — dead exports likely
  retained for Python access via `CyGame`. **Ruled REMOVE but DEFERRED** with the Python pass (R-6).
- **A-4 CRIT (now the intended path):** `CvCascadeLogConsumer::onEvent` (`Sources/Cascade/CvEventSpine.cpp`)
  writes directly via `gDLL->logMsg`, gated by an inline `gPlayerLogLevel >= 1`. Post-consolidation this is
  the **intended central path**; pre-consolidation it is an inconsistency.
- **A-5 HIGH:** `rjLogLine` (`Sources/Cascade/CvCascadeReadJson.cpp`) hardcodes `level=1` for `streamLogTee`
  with no level parameter; gating is split (caller-side `gPlayerLogLevel >= N` guards the file write, the tee
  level is hardcoded). Diverges from the BBAI helpers where one level parameter drives both.
- **A-6 CRIT:** `C2C.log` firehose — ~45 ungated `logging::logMsg` sites across `CvGame`/`CvTeam`/`CvPlayer`/
  `CvPlot`/`CvGameCoreUtils`/`CvPlayerAI`, firing per-turn/per-event. Worst: `doFlexibleDifficulty` (first
  line every turn), `buildCityBillboardString` (every UI frame with religion icons), `doSpawns`/`setMinorCiv`.
  **Ruled DELETE — `C2C.log` is retired** (R-4).
- **A-7 CRIT:** `CvGameTextMgr_buildCityBillboardString.log` (`Sources/UI/CvGameTextMgr.cpp`) — 3 ungated
  `logging::logMsg` to a dedicated file, firing on every city billboard render with religion icons. Massive
  append-only file in normal play. Cleanup target.
- **A-8 … A-14 (HIGH/CRIT, see §1G correction):** ungated `OutputDebugString` in `CvContractBroker`,
  `CvSelectionGroup` (mission queue), `CvUnit` (move/setXY — highest-volume single source), `CvPlayerAI`
  (plot-danger, tech, trade), `CvCityAI`, `CvMap` (constructor/read). Fire in Release/Assert/Debug, NO-OP in
  FinalRelease. R-5 cleanup: convert real events to a `log<Domain>AI` call, delete the rest.
- **A-15 HIGH:** `CvGameCoreUtils.cpp` pathfinder `OutputDebugString` — ~10 calls gated by a `bTrace` local
  hardcoded `false`. Currently inert but one line from millions of writes/turn. Remove or properly guard.
- **A-16 MED:** `CvContractBroker` mixed-gate — an outer `gUnitLogLevel > 2` guard wrapping a `log()` that
  delegates to `logContractBroker()` (gated on `gPlayerLogLevel`). Neither gate matches the ContractBroker
  domain gate; the line can be suppressed/emitted inconsistently.
- **A-17 MED:** `CvGame::logRandomResult` — dynamically-named per-player-per-50-turn rotating file via
  `bst::format`, gated on `isNetworkMultiPlayer()`. Unusual filename-rotation deviation.
- **A-18 MED:** `PlotSnapshot` (`Sources/Utils/PlotSnapshot.cpp`) — direct `std::fopen/fprintf/fclose` to
  CSV, bypassing `gDLL->logMsg` **intentionally** (gDLL holds handles open so `remove()` fails on prior
  files). Fires unconditionally every turn (~9600 `fprintf`/turn). Not the AI surface; noted as a raw write
  outside all infra.
- **A-19 LOW:** dead `PropertyBuildingOOS.log` references (`Sources/Engine/CvProperties.cpp`) — three
  commented-out `gDLL->logMsg` to a nonexistent sink. Disabled-without-removal OOS traces.
- **A-20 LOW:** `dump()` ungated `OutputDebugString` (`Sources/Infos/CvArtInfoFeature.cpp`) — debug-only, but
  any live path calling `dump()` produces spam.
- **A-21 LOW:** `logCB` / `logToFile` dead exports (as A-3) — retained for possible Python access via
  `CyGame::log`/`logw`. The `CyGame` Python-callable sinks are an open arbitrary-file-write surface from
  Python (any script can call `game.log('arbitrary','msg')` with no gate).

---

## 4. The consolidation (target surface)

The full design lives in [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md) — this section
is the gap summary. The goal: **one surface, one gate path, one tee.** Today there are ~14 per-domain files, a
cascade sink, and multiple independent `logMsg`+`tee` paths. The target routes everything through the event
spine's logging consumer:

```
call site
  └─ emit(KIND, type, raw payload)             ← one clean line; no string at call site
        └─ CvEventSpine::dispatch
              ├─ CvCascadeLogConsumer::onEvent  ← BROAD: formats all kinds, gated
              │     ├─ gDLL->logMsg(file, line)    → file sink (per-domain file, kept)
              │     └─ streamLogTee(level, line)   → /events SSE (one shared tee)
              ├─ CvCascadeTally::onEvent        ← SELECTIVE: DOMAIN only → counts
              └─ (future: grants consumer)
```

The **owner rulings (R-1..R-6, 2026-06-18)** that fix the target shape:

| # | Question | Ruling |
|---|---|---|
| R-1 | String-carrying DIAGNOSTIC event as a transitional spine payload? | **NO** — the spine stays raw-payload-pure; each helper's lines get a raw-field catalog (Stage-0 prework) **before** it migrates. No string shortcut. |
| R-2 | Per-domain files, or one unified file? | **Keep per-domain `<Domain>AI.log`** as a consumer routing detail. The unified SURFACE is the spine + `/events`, not the file. |
| R-3 | `logInitInfo` (ex-`logGameInfo`) gate? | **Keep caller-gated** — the session header is always useful. |
| R-4 | `C2C.log` traces — gate or delete? | **DELETE — `C2C.log` is retired.** Legacy, no consumers, fired every turn. |
| R-5 | Ungated `OutputDebugString` in production paths? | **REPLACE entirely with the unified logger** — convert each site (don't `#ifdef`); the owner never uses `OutputDebugString`, so it is retired as a mechanism. |
| R-6 | Dead `logCB` / `logToFile` Python exports? | **REMOVE — but DEFERRED** with the separate Python-structure pass; left in place this round. |

Stage-0 (the field catalog) is the prework gating the spine migration: complete the §3 subsystem field tables
in [`ai-logging-reference.md`](ai-logging-reference.md) for every domain (currently `[CIT]`/`[UNT]`/`[COM]`/
`[WAR]`/`[CTB]`/`[PERF]` are expanded; the rest are stubs). The migration uses shadow discipline — old + new
coexist, diff for parity, cut over per domain — never a big bang ([DEC-map-before-delete]). The BUG-options
screen rework (unifying the alias knobs into one "Surveillance / log level" 0–5 knob, removing the phantom
per-scope knobs) is **deferred to near-live** — the option set isn't known until the cascade shadows land.

---

## See also
- [`ai-logging-reference.md`](ai-logging-reference.md) — the wire spec (tag taxonomy + field lists) this
  census enumerates the emit sites for; together they are the current surface + the gap to the target.
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]) the gates index
  into, and the three hook shapes ([DEC-obs-hook-shapes]) — a gated `[TAG]` log is shape #2.
- [`http-server.md`](http-server.md) — the `/events` tee these lines stream onto, and the live-read rules
  (logs held open mid-session — don't tail them live).
- [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md) — the in-flight consolidation: the
  event-spine routing this inventory maps the migration target for.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
