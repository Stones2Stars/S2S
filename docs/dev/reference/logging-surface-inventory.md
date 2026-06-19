# Logging surface inventory

**Status: reference — generated from the multi-agent call-site sweep (2026-06-18).**
Covers all `Sources/` logging call sites across all slices. This document is the
companion to [`ai-logging-reference.md`](ai-logging-reference.md) (the wire spec / tag
taxonomy) and the consolidation targets in
[`../plans/event-spine-spec.md`](../plans/event-spine-spec.md). Together they describe
the current surface + the target surface + the gap between them.

The governing rule (owner, 2026-06-18): **log LINES are stable — tags/fields do not
change. What evolves is WHERE they are SENT.** The consolidation is a routing change,
not a content change. Exception: a line that doesn't make sense in the new structure
is dropped and redone, not preserved out of inertia.

---

## 1. Distinct route table (merged + deduped)

Routes grouped by primitive family. "count" = call-site instances found in the sweep;
"sample" = one representative source location. The gate column records what determines
whether the call fires.

### 1A. BBAI `log<Domain>AI` helpers (the standard AI logging surface)

Every helper in this family:
- calls `gDLL->logMsg(file, line)` — file write
- calls `streamLogTee(level, line)` — `/events` SSE tee
- calls `OutputDebugString` — debugger echo (always, when logging is on — see anomaly A-1)
- is gated by a scope global (checked BEFORE the call, by the helper itself)

| Primitive | Tag | Dest file | Gate global | Count | Sample |
|---|---|---|---|---|---|
| `logBuildEvaluation` | `[WAI/*]` | `BuildEvaluation.log` | `gPlayerLogLevel` | 43 | `Sources/CvWorkerAI.cpp:477` |
| `logCityAI` | `[CIT/*]` | `CityAI.log` | `gCityLogLevel` | 23 | `Sources/CvCity.cpp:15836` |
| `logUnitAI` | `[UNT/*]` | `UnitAI.log` | `gUnitLogLevel` | 18 | `Sources/CvUnitAI.cpp:479` |
| `logHunterAI` | `[HAI/*]` | `HunterAI.log` | `gUnitLogLevel` | 54 | `Sources/CvHunterAI.cpp:102` |
| `logCombatAI` | `[COM/*]` | `CombatAI.log` | `gUnitLogLevel` | 10 | `Sources/CvUnitAI.cpp:18003` |
| `logGroupAI` | `[GRP/*]` | `GroupAI.log` | `gUnitLogLevel` | 7 | `Sources/CvSelectionGroupAI.cpp:82` |
| `logFoundAI` | `[FND/*]` | `FoundAI.log` | `gPlayerLogLevel` | 1 | `Sources/CvUnitAI.cpp:19291` |
| `logDecisionAI` | `[DAI/*]` | `DecisionAI.log` | `gPlayerLogLevel` | 12 | `Sources/CvDecisionAI.cpp:37` |
| `logDiploAI` | `[DIP/*]` | `DiploAI.log` | `gPlayerLogLevel` | 9 | `Sources/CvPlayerAI.cpp:7909` |
| `logEspionageAI` | `[ESP/*]` | `EspionageAI.log` | `gPlayerLogLevel` | 2 | `Sources/CvPlayerAI.cpp:15495` |
| `logWarAI` | `[WAR/*]` | `WarAI.log` | `gTeamLogLevel` | 3 | `Sources/CvTeamAI.cpp:243` |
| `logContractBroker` | `[CTB/*]` | `ContractBroker.log` | `gPlayerLogLevel` | 66 | `Sources/CvContractBroker.cpp:62` |
| `logEngine` | `[ENG/*]` | `Engine.log` | `gTeamLogLevel` | 1 | `Sources/BetterBTSAI.cpp:340` |
| `logGameInfo` | `[GAME/*]` | `GameInfo.log` | **(ungated — see anomaly A-2)** | 5 | `Sources/BetterBTSAI.cpp:359` |
| `logCB` | (none) | `CB.log` | **(ungated — see anomaly A-3)** | 1 | `Sources/BetterBTSAI.cpp:373` |
| `logToFile` | (none) | *(caller-supplied)* | **(ungated — see anomaly A-3)** | 1 | `Sources/BetterBTSAI.cpp:379` |

**Domain → scope global cross-reference:**

| Domain | Scope global | BUG option (same for all four) | Note |
|---|---|---|---|
| Player: `[WAI]` `[DAI]` `[DIP]` `[ESP]` `[FND]` `[CTB]` | `gPlayerLogLevel` | `Autolog__LogLevelPlayerBBAI` | — |
| Team: `[WAR]` `[ENG]` | `gTeamLogLevel` | `Autolog__LogLevelPlayerBBAI` | alias — see §2 gate map |
| City: `[CIT]` | `gCityLogLevel` | `Autolog__LogLevelPlayerBBAI` | alias — see §2 gate map |
| Unit: `[UNT]` `[HAI]` `[COM]` `[GRP]` | `gUnitLogLevel` | `Autolog__LogLevelPlayerBBAI` | alias — see §2 gate map |

### 1B. PERF timers

| Primitive | Tag | Dest file | Gate global | Count | Sample |
|---|---|---|---|---|---|
| `logPerf` (direct call) | `[PERF/phase\|unitai]` | `Performance.log` | `gPerfLogLevel` | ~30 | `Sources/CvGame.cpp:5859` |
| `PERF_SCOPE` (`ScopedPerfTimer` dtor → `logPerf`) | `[PERF/phase]` | `Performance.log` | `gPerfLogLevel` | ~80 | `Sources/CvGame.cpp:5914` |
| `PERF_ACCUM` (accumulator only; flushed by `logPerf`) | *(no direct emit)* | `Performance.log` | `gPerfLogLevel` (at flush) | ~57 | `Sources/CvGame.cpp:2281` |

### 1C. Event spine / cascade observability

| Primitive | Tag | Dest file | Gate | Count | Sample |
|---|---|---|---|---|---|
| `eventSpine().emit(DOMAIN/DIAGNOSTIC)` → `CvCascadeLogConsumer::onEvent` → `gDLL->logMsg` | `[SPINE/DOMAIN]` or `[SPINE/<KIND>]` | `Cascade.log` | `gPlayerLogLevel >= 1` (inline in `onEvent`) | 6 | `Sources/Cascade/CvEventSpine.cpp:101` |
| `rjLogLine` (file-scope wrapper) → `gDLL->logMsg` + `streamLogTee(1, ...)` | `[READJSON]` `[PLACEMENT]` `[DORMANCY]` `[STATE/game\|fin\|dip\|city]` | `Cascade.log` | `gPlayerLogLevel >= N` at each call site | 21 | `Sources/Cascade/CvCascadeReadJson.cpp:772` |

### 1D. SSE live stream

| Primitive | Dest | Gate | Count | Sample |
|---|---|---|---|---|
| `streamLogTee(level, line)` | `/events` SSE via `CvHttpServer::publishEvent` | `level <= gStreamLogLevel` AND `CvHttpServer::isEnabled()` | ~14 definitions; called inside every BBAI helper | `Sources/BetterBTSAI.cpp:27` |
| `CvHttpServer::publishEvent` (direct, outside helpers) | `/events` SSE | `CvHttpServer::isEnabled()` (+ `isHuman()` for player-turn events) | 7 | `Sources/CvGame.cpp:6038` |

### 1E. Infrastructure / diagnostic logs (not the AI surface)

These sinks exist for infrastructure reasons (XML load, OOS, crash, profiler). They are
out of scope for the AI-logging consolidation but are catalogued here for completeness.

| Primitive | Dest file | Gate | Count | Sample |
|---|---|---|---|---|
| `gDLL->logMsg` (crash handler) | `PythonCallstack.log` | on unhandled exception / minidump | 1 | `Sources/CvGlobals.cpp:264` |
| `logging::logMsg` `Xml_MissingTypes.log` | `Xml_MissingTypes.log` | on every `getInfoTypeForString` miss | 1 | `Sources/CvGlobals.cpp:2698` |
| `logging::logMsg` `cvInternalGlobals_logInfoTypeMap.log` | `cvInternalGlobals_logInfoTypeMap.log` | explicit `logInfoTypeMap()` call | 4 | `Sources/CvGlobals.cpp:2735` |
| `logging::logMsg` `Checksum.log` | `Checksum.log` | inside `getAssetCheckSum()` | 1 | `Sources/CvGlobals.cpp:3150` |
| `gDLL->logMsg` (IFP profiler) | `IFP_log.txt` | `#ifdef USE_INTERNAL_PROFILER` only | 2 | `Sources/CvGameCoreDLL.cpp:486` |
| `gDLL->logMsg` (multiplayer net) | `Player N - Multiplayer Game Log.log` | `isNetworkMultiPlayer()` | 1 | `Sources/CvGame.cpp:9104` |
| `gDLL->logMsg` (OOS special) | `OOSSpecialLogger - Player N - Set N.log` | `isNetworkMultiPlayer()` | 1 | `Sources/CvGame.cpp:11874` |
| `logging::logMsg` (RNG trace) | `RandomLogger - Player N - Set N.log` | `isNetworkMultiPlayer() && isFinalInitialized()` (`logging::logMsg` variant also `#ifdef _DEBUG + GC.getRandLogging()`) | 2 | `Sources/CvGame.cpp:8121` |
| `logging::logMsg` (FAssert) | `Asserts.log` + `AssertsJson.log` | `#ifdef FASSERT_LOGGING` (Assert/Debug only) | 2 | `Sources/FAssert.cpp:250` |
| `logging::logMsg` `xml.log` | `xml.log` | XML load path | 25 | `Sources/CvXMLLoadUtility.cpp:31` |
| `DEBUG_LOG` (expands to `logging::logMsg` in `_DEBUG` only) | `XmlCheckDoubleTypes.log` / `MLF.log` / `CvXMLLoadUtilitySetMod_MLFEnumerateFiles.log` | `#ifdef _DEBUG` | 12 | `Sources/CvXMLLoadUtilitySet.cpp:163` |
| `gDLL->logMsg` (modular-art debug) | `CvXMLLoadUtilityModTools_isModularArt.log` | `#if (DEBUG_IS_MODULAR_ART == 1)` compile-time | 10 | `Sources/CvXMLLoadUtilityModTools.cpp:129` |
| `gDLL->messageControlLog` | engine-controlled sink (`MessageControl.log`) | `GC.getLogging()` | ~7 | `Sources/CvCity.cpp:228` |
| `logging::logMsg` `bull.log` | `bull.log` | once at BUG init | 1 | `Sources/CvBugOptions.cpp:20` |
| `std::fopen/fprintf/fclose` (PlotSnapshot) | `PlotSnapshot_<tag>_t<N>.csv` | unconditional each turn | ~3 | `Sources/Utils/PlotSnapshot.cpp:259` |
| `std::ofstream` (BuildingsBuiltTable) | `BuildingsBuiltTable.csv` | player hotkey / `toggleUnitsDisplay` | 1 | `Sources/CvMap.cpp:1843` |

### 1F. Ungated `logging::logMsg` to `C2C.log` (firehose — cleanup targets)

These are the highest-severity ungated sinks found in the sweep. They write to `C2C.log`
on every AI turn with zero level gate and are NOT part of the standard BBAI log surface.

| Call site | Content | Frequency |
|---|---|---|
| `Sources/CvGame.cpp:10152` + ~10 more | `[Flexible Difficulty]` — fires at the top of `doFlexibleDifficulty()` every turn | per-turn unconditional |
| `Sources/CvGame.cpp:6410, 6582` | doSpawns loop | per-spawnInfo per player per turn |
| `Sources/CvGameCoreUtils.cpp:3207` | `[CALENDAR]` era tick | per `calculateCurrentTick()` call |
| `Sources/CvTeam.cpp:2788-2883` | minor/full civ transitions + barb war | on `setMinorCiv()` and related paths |
| `Sources/CvPlot.cpp:867` | resource depletion event | per depletion event (probabilistic per turn) |
| `Sources/CvGameTextMgr.cpp:26667` | religion icon billboard | every city billboard render — **high-frequency UI path** |

### 1G. `OutputDebugString` (Win32 debugger sink — not a file)

> **⚠ CORRECTION 2026-06-19 (verified against `CvGameCoreDLL.h:363-367`):** `OutputDebugString` is **`#define`d to
> nothing under `#ifdef FINAL_RELEASE`** — so it **compiles out entirely in the build players run (FinalRelease)**. The
> "ungated → fires in FinalRelease, CRIT" framing throughout this section is therefore WRONG for the shipped build. The
> calls ARE live in **Release / Assert / Debug** (the owner's *test* builds — and the owner runs Release for cascade
> testing), so they're a **dev/test-build** firehose + clutter, not a shipped one. R-5 (retire the mechanism) still stands
> — the owner doesn't use `OutputDebugString` — but the urgency is dev-hygiene, not a player-facing emergency. (The
> crash-handler / `StackWalker` / `CvAllocator` uses are legitimate — they run when the normal logger may be dead — and
> should KEEP it.)

Two populations: `#ifdef _DEBUG`-gated and otherwise-ungated (live in Release/Assert/Debug; **compiled out in
FinalRelease** per the correction above). The ungated-in-dev-builds population is the R-5 cleanup target.

**`OutputDebugString` hotspots (fire in Release/Assert/Debug with any debugger/ETW attached; NO-OP in FinalRelease):**

| Location | Content | Frequency |
|---|---|---|
| `Sources/CvUnit.cpp:4975` | unit move coordinates | EVERY unit move |
| `Sources/CvUnit.cpp:13826` | unit reposition | every combat-capable unit setXY |
| `Sources/CvSelectionGroup.cpp:2023,2027,2054,2092,2290,2293,2305,2390,2399` | mission start/continue | every mission execution step |
| `Sources/CvSelectionGroup.cpp:5547,5594,5616` | mission queue mutations | every enqueue/dequeue |
| `Sources/CvContractBroker.cpp:334,827,849,1038` | work-request operations | per-unit per-request in broker inner loop |
| `Sources/CvPlayerAI.cpp:3541` | plot-danger hot path | per-unit per-turn in danger eval |
| `Sources/CvPlayerAI.cpp:6286` | AI tech choice | per AI player per research selection |
| `Sources/CvGameCoreUtils.cpp:1546` | pathfinder cost tracing (note: guarded by `bTrace` local bool hardcoded `false` — see §3) | millions per turn if `bTrace` were true; currently dead |
| `Sources/CvGame.cpp:484` + ~11 more | lifecycle markers | per game lifecycle event + some per-turn |
| `Sources/CvMap.cpp:62,67,84,128,1298,1357` | map constructor/read breadcrumbs | per load |
| `Sources/CvCity.cpp:1562` | city panel open | every `updateSelectedCity()` call |
| `Sources/CvCity.cpp:2299` | no trainable unit | per cache rebuild with no result |
| `Sources/CvCity.cpp:16019` | project completion | per project built (pre-`[CIT/produced]` duplicate) |
| `Sources/CvCityAI.cpp:4263` | "No buildable defender!!" | per CITY_DEFENSE cache-hit miss |
| `Sources/CvCityAI.cpp:4982` | building-value cache init | per-city-per-turn on NULL cache |
| `Sources/CvCityAI.cpp:8400` | build re-eval skip | every AI_updateBestBuild on fast path |

---

## 2. Gate map and file map

### 2A. Scope globals and BUG options

| Global | BUG option key | Wired in | Effective scope | Note |
|---|---|---|---|---|
| `gPlayerLogLevel` | `Autolog__LogLevelPlayerBBAI` | `CvGlobals::refreshOptionsBUG` | player-scope decisions: `[WAI]` `[DAI]` `[DIP]` `[ESP]` `[FND]` `[CTB]` | **Also drives `gTeamLogLevel`, `gCityLogLevel`, `gUnitLogLevel`** — one knob for all four (see note below) |
| `gTeamLogLevel` | `Autolog__LogLevelPlayerBBAI` | same `refreshOptionsBUG` | team-scope: `[WAR]` `[ENG]` | **Alias of `gPlayerLogLevel`** — the per-scope Team BUG option exists in XML but is explicitly ignored in code |
| `gCityLogLevel` | `Autolog__LogLevelPlayerBBAI` | same | city-scope: `[CIT]` | **Alias of `gPlayerLogLevel`** — same as above |
| `gUnitLogLevel` | `Autolog__LogLevelPlayerBBAI` | same | unit-scope: `[UNT]` `[HAI]` `[COM]` `[GRP]` | **Alias of `gPlayerLogLevel`** — same as above |
| `gPerfLogLevel` | `Autolog__LogLevelPerf` | `refreshOptionsBUG` | `[PERF]` turn timing | Independent knob; intentionally separate from AI verbosity |
| `gStreamLogLevel` | `Autolog__LogLevelStream` | `refreshOptionsBUG` | `/events` SSE tee level ceiling | Default 1; lines <= this level are also streamed |

**Critical implementation note:** `CvGlobals.cpp:refreshOptionsBUG` (lines 3079-3121) sets
all of `gPlayerLogLevel`, `gTeamLogLevel`, `gCityLogLevel`, AND `gUnitLogLevel` from the
single `Autolog__LogLevelPlayerBBAI` option. The per-scope BUG options (`Autolog__LogLevelTeamBBAI`
etc.) exist in the UI but their values are deliberately discarded. This is intentional
behavior documented in the code comment, but it means the four globals are aliases
in practice. The consolidation plan (owner 2026-06-18) replaces this with one coherent
"Surveillance / log level" knob matching the 0-4 tier naming.

### 2B. File map — every distinct log destination

| Log file | Written by | Gate | Notes |
|---|---|---|---|
| `BuildEvaluation.log` | `logBuildEvaluation` | `gPlayerLogLevel` | Worker AI / `[WAI/*]` |
| `CityAI.log` | `logCityAI` | `gCityLogLevel` | City production / `[CIT/*]` |
| `UnitAI.log` | `logUnitAI` | `gUnitLogLevel` | Unit dispatch / `[UNT/*]` |
| `HunterAI.log` | `logHunterAI` | `gUnitLogLevel` | Hunter AI / `[HAI/*]` |
| `CombatAI.log` | `logCombatAI` | `gUnitLogLevel` | Combat decisions / `[COM/*]` |
| `GroupAI.log` | `logGroupAI` | `gUnitLogLevel` | Group/army / `[GRP/*]` |
| `FoundAI.log` | `logFoundAI` | `gPlayerLogLevel` | Settler AI / `[FND/*]` |
| `DecisionAI.log` | `logDecisionAI` | `gPlayerLogLevel` | Flavour/strategy / `[DAI/*]` |
| `DiploAI.log` | `logDiploAI` | `gPlayerLogLevel` | Diplomacy+deals / `[DIP/*]` |
| `EspionageAI.log` | `logEspionageAI` | `gPlayerLogLevel` | Espionage / `[ESP/*]` |
| `WarAI.log` | `logWarAI` | `gTeamLogLevel` | War plans / `[WAR/*]` |
| `ContractBroker.log` | `logContractBroker` | `gPlayerLogLevel` | Contract market / `[CTB/*]` |
| `Engine.log` | `logEngine` | `gTeamLogLevel` | Sanity / `[ENG/*]` |
| `GameInfo.log` | `logGameInfo` | *(ungated)* | Session header / `[GAME/*]` |
| `CB.log` | `logCB` | *(ungated)* | Unknown; no live callers |
| `Cascade.log` | `CvCascadeLogConsumer::onEvent`; `rjLogLine` | `gPlayerLogLevel >= 1` | Cascade spine / readjson / placement |
| `Performance.log` | `logPerf`, `ScopedPerfTimer` | `gPerfLogLevel` | Turn timing / `[PERF/*]` |
| `C2C.log` | `logging::logMsg` (scattered) | *(mostly ungated)* | Legacy catch-all; firehose — see §1F |
| `PythonCallstack.log` | `gDLL->logMsg` | on crash only | Python exception traces |
| `Xml_MissingTypes.log` | `logging::logMsg` | on XML miss | Type resolution errors |
| `cvInternalGlobals_logInfoTypeMap.log` | `logging::logMsg` | explicit call only | Bulk type map dump |
| `Checksum.log` | `logging::logMsg` | on `getAssetCheckSum()` | Per-info checksum rows |
| `Asserts.log` / `AssertsJson.log` | `logging::logMsg` via FAssert | `#ifdef FASSERT_LOGGING` | Assert hits; Assert/Debug only |
| `xml.log` | `logging::logMsg` | XML load path | XML load diagnostics |
| `XmlCheckDoubleTypes.log` / `MLF.log` / `CvXMLLoadUtilitySetMod_MLFEnumerateFiles.log` | `DEBUG_LOG` | `#ifdef _DEBUG` | Debug-only XML load trace |
| `CvXMLLoadUtilityModTools_isModularArt.log` | `gDLL->logMsg` | `#if (DEBUG_IS_MODULAR_ART==1)` | Compile-time modular art debug |
| `IFP_log.txt` | `gDLL->logMsg` | `#ifdef USE_INTERNAL_PROFILER` | Internal function profiler (Profile/ProfileExtra builds only) |
| `RandomLogger - Player N - Set N.log` | `logging::logMsg` | `isNetworkMultiPlayer()` (+ `#ifdef _DEBUG` variant) | Per-player OOS RNG trace |
| `OOSSpecialLogger - Player N - Set N.log` | `gDLL->logMsg` | `isNetworkMultiPlayer()` | OOS special diagnostic |
| `Player N - Multiplayer Game Log.log` | `gDLL->logMsg` | `isNetworkMultiPlayer()` | Net message log |
| `CvGameTextMgr_buildCityBillboardString.log` | `logging::logMsg` | *(ungated)* | Religion icon UI render spam — clear cleanup target |
| `bull.log` | `logging::logMsg` | once at BUG init | BUG subsystem init marker |
| `PlotSnapshot_<tag>_t<N>.csv` | `std::fopen/fprintf` | unconditional each turn | Raw file write; intentional bypass (see anomaly A-18) |
| `BuildingsBuiltTable.csv` | `std::ofstream` | player hotkey | Debug inventory dump |
| `/events` (SSE) | `CvHttpServer::publishEvent` via `streamLogTee` or direct | `gStreamLogLevel` + `CvHttpServer::isEnabled()` | The live observability surface |

---

## 3. Anomalies — logging that bypasses the standard path

These are the cleanup targets for the consolidation. Severity rated:
**CRIT** = fires in FinalRelease + firehose, **HIGH** = ungated but lower frequency,
**MED** = gated by compile flag or condition but bypasses helpers,
**LOW** = dead code or debug-only.

### A-1 ✅ RESOLVED 2026-06-19: OutputDebugString echo removed from every BBAI helper

`Sources/AI/BetterBTSAI.cpp` — every `log<Domain>AI` helper appended `\n` + `OutputDebugString(buf)` *after* the file
write (`gDLL->logMsg`) and the `/events` tee (`streamLogTee`). It was a pure DUPLICATE sink (the data is already in the
file + on `/events`), so it was deleted — all 15 echoes (`logPerf`/`logContractBroker`/`logBuildEvaluation`/… + the
`logToFile` one). This retires `OutputDebugString` from the entire AI-logging surface in one move (R-5, the highest-value
slice). No data lost; Assert-clean. (Live only in Release/Assert/Debug anyway — null in FinalRelease per the §1G correction.)

### A-2 ~~HIGH~~ RESOLVED 2026-06-18: `logGameInfo` ungated → renamed `logInitInfo`, kept caller-gated

`Sources/BetterBTSAI.cpp:359` — the fn itself has no level param, BUT its sole caller
(`CvGame.cpp:589`) already gates it on "any logging active" (`gPlayerLogLevel>0 || …`), so
the original "always emits" framing was wrong (trust-but-verify). Per R-3 the session header
is intentionally caller-gated (not self-gated) and kept. **Renamed `logGameInfo`→`logInitInfo`,
tag `[GAME/*]`→`[INIT/*]`** (it logs session INIT, and `[GAME/*]` clashed with the per-turn
`[STATE/game]` feed). No further action.

### A-3 HIGH: `logCB` and `logToFile` bypass all three arms

`Sources/BetterBTSAI.cpp:373-382` — both use `logging::logMsg` instead of
`gDLL->logMsg`, have NO `streamLogTee` call and NO `OutputDebugString` echo. They bypass
all three arms of the standard helper pattern. `logToFile` accepts a caller-supplied
filename making its destination fully dynamic. No callers found in any `.cpp` file —
these are dead exports likely retained for Python access via `CyGame`.

### A-4 CRIT: `CvCascadeLogConsumer::onEvent` writes directly to `Cascade.log`

`Sources/Cascade/CvEventSpine.cpp:74-104` — writes directly via `gDLL->logMsg` (not
through any named BBAI helper), breaking the pattern. Gating is an inline `gPlayerLogLevel
>= 1` check rather than the standard helper wrapper. Post-consolidation this is the
**intended central path**; pre-consolidation it is an inconsistency.

### A-5 HIGH: `rjLogLine` hardcodes tee level

`Sources/Cascade/CvCascadeReadJson.cpp:772-776` — local file-scope function that
hardcodes `level=1` for `streamLogTee` but has no level parameter of its own. Gating is
split: caller-side `gPlayerLogLevel >= N` guards the file write; the `streamLogTee` level
is hardcoded `1`. The pattern diverges from the BBAI helpers where a single level
parameter drives both.

### A-6 CRIT: `C2C.log` firehose (ungated `logging::logMsg`)

Multiple sites in `CvGame.cpp`, `CvTeam.cpp`, `CvPlayer.cpp`, `CvPlot.cpp`,
`CvGameCoreUtils.cpp`, `CvPlayerAI.cpp` — ~45 call sites write to `C2C.log` with no
level gate, firing per-turn or per-event unconditionally. The worst offenders:
`doFlexibleDifficulty` (fires as the very first line of the function every turn),
`buildCityBillboardString` (fires on every UI frame with religion icons), and the
`doSpawns` and `setMinorCiv` paths. `C2C.log` is the old Caveman2Cosmos debug catch-all
and has no place in the new structured surface.

### A-7 CRIT: `CvGameTextMgr_buildCityBillboardString.log` UI-frame spam

`Sources/CvGameTextMgr.cpp:26667-26682` — 3 `logging::logMsg` calls to a
strangely-named dedicated log file, ungated, firing on every city billboard UI render for
cities with any religion icons. Produces a massive append-only file in normal play.

### A-8 HIGH: `CvContractBroker` four ungated `OutputDebugString` calls

`Sources/CvContractBroker.cpp:334,827,849,1038` — fire on every work-request operation
(add, satisfy, partially satisfy, assess) in every build. Line :1038 is in the broker's
inner unit-assessment loop — a per-unit per-request hot path, potential firehose in any
debugger session.

### A-9 CRIT: `CvSelectionGroup` mission-queue `OutputDebugString` firehose

`Sources/CvSelectionGroup.cpp:2023,2027,2054,2092,2290,2293,2305,2390,2399,5547,5594,5616`
— ungated `OutputDebugString` calls throughout `startMission`, `continueMission`,
`clearMissionQueue`, `insertAtEndMissionQueue`, `deleteMissionQueueNode`. Fire on every
unit mission operation in every build including FinalRelease.

### A-10 CRIT: `CvUnit` move/setXY ungated `OutputDebugString`

`Sources/CvUnit.cpp:4975` and `13826` — fire on EVERY unit move and unit reposition
respectively, with no gate at all. Highest-volume single spam source in the codebase.

### A-11 HIGH: `CvPlayerAI::AI_getPlotDangerInternal` ungated `OutputDebugString`

`Sources/CvPlayerAI.cpp:3541` — fires on every plot-danger hot-path call (per-unit
per-turn). No `#ifdef _DEBUG`, no level gate. Severe spam in any debugger session.

### A-12 HIGH: `CvPlayerAI` four ungated `OutputDebugString` clusters

`Sources/CvPlayerAI.cpp:1406-1421,6286,8406,8412,1683,1823` — ungated calls for AI tech
selection (per AI player per research pick), GPT trade loop (inner while-loop), and city
conquest. All fire in Release builds.

### A-13 HIGH: `CvCityAI` three ungated `OutputDebugString` calls (non-DEBUG)

`Sources/CvCityAI.cpp:4263,4982,8400` — "No buildable defender!!", building-value cache
init, and "City skips re-evaluation" — all ungated, fire in every build during normal
AI operation.

### A-14 HIGH: `CvMap` constructor/read `OutputDebugString` firehose

`Sources/CvMap.cpp:62,67,84,128,1298,1357` — six unconditional `OutputDebugString` calls
in map constructor and `CvMap::read()`. No `#ifdef` guard; fire in FinalRelease.

### A-15 HIGH: `CvGameCoreUtils` pathfinder `OutputDebugString` (currently dead)

`Sources/CvGameCoreUtils.cpp:1546` — ~10 active `OutputDebugString` calls inside
pathfinder cost functions, gated by `bTrace` local bool that is hardcoded `false` at
lines 1288/1893. Currently inert but one line change from millions of writes per turn.
Should be removed or properly guarded.

### A-16 MED: `CvContractBroker` mixed-gate anomaly

`Sources/CvContractBroker.cpp:284` — outer guard is `gUnitLogLevel > 2` but the called
`log()` delegates to `logContractBroker()` which gates on `gPlayerLogLevel`. If
`gPlayerLogLevel < 1` the call no-ops even when `gUnitLogLevel > 2`; if `gUnitLogLevel <=
2` the `[CTB/work/intransit]` line is suppressed even when `gPlayerLogLevel >= 1`. Neither
gate matches the ContractBroker domain gate (`gPlayerLogLevel`).

### A-17 MED: `CvGame::logRandomResult` dynamic filename rotation

`Sources/CvGame.cpp:8121` — writes to a dynamically-named per-player-per-50-turn rotating
file `RandomLogger - Player N - Set N.log` via `bst::format`. Gated on
`isNetworkMultiPlayer()`, but the filename pattern is an unusual deviation not seen
elsewhere.

### A-18 MED: `PlotSnapshot` raw `fopen/fprintf` intentional bypass

`Sources/Utils/PlotSnapshot.cpp:250-259` — direct `std::fopen/fprintf/fclose` to CSV,
bypassing `gDLL->logMsg` entirely. The comment at line 7 explains: gDLL holds handles
open so `remove()` fails on previously written files. Intentional workaround. Fires
unconditionally every turn (~9600 `fprintf` calls/turn). Not part of the AI logging
surface but worth noting as a raw-file write outside all infra.

### A-19 LOW: Dead `PropertyBuildingOOS.log` references

`Sources/CvProperties.cpp:152,174,207` — three commented-out `gDLL->logMsg` calls
referencing `PropertyBuildingOOS.log`, a sink that exists nowhere else. Dead OOS-debug
traces that were disabled without removal.

### A-20 LOW: `CvPlot::dump()` ungated `OutputDebugString`

`Sources/Infos/CvArtInfoFeature.cpp:208-246` — nine `OutputDebugString` calls in `dump()`
with no `#ifdef _DEBUG` guard. `dump()` appears debug-only but any live code path calling
it produces debugger spam.

### A-21 LOW: `logCB` / `logToFile` dead exports

As noted in A-3: no `.cpp` call sites found. Retained for possible Python access via
`CyGame::log` / `CyGame::logw`. The `CyGame` Python-callable log sinks are an open
arbitrary-file-write surface from Python (any script can call `game.log('arbitrary',
'msg')` with no gate).

---

## 4. Consolidation design

### 4A. Target architecture (from `event-spine-spec.md` §8/§9a)

The goal: **one surface, one gate path, one tee**. Today there are ~14 per-domain
destination files, a cascade-specific sink, and multiple independent `logMsg`+`tee` call
paths. The consolidation routes everything through the event spine's logging consumer:

```
call site
  └─ emit(KIND, type, raw payload)            ← one clean line; no string at call site
        └─ CvEventSpine::dispatch
              ├─ CvCascadeLogConsumer::onEvent ← BROAD (formats ALL kinds, gated)
              │     ├─ gDLL->logMsg(file, line)   → file sink (per-domain file or unified)
              │     └─ streamLogTee(level, line)  → /events SSE (one shared tee)
              ├─ CvCascadeTally::onEvent       ← SELECTIVE (DOMAIN only → counts)
              └─ (future: grants consumer)
```

The BBAI `log<Domain>AI` helpers are the current format-and-route layer. Under the spine
model, each helper becomes a thin call to `emit(DIAGNOSTIC, type, raw fields)` and all
formatting moves into the logging consumer. The **stable log lines** (tags, fields) are
reproduced identically by the consumer — content is unchanged, routing consolidates.

### 4B. The open question: string-carrying DIAGNOSTIC vs raw-field payload

**⚑ Owner ruling required before migrating the BBAI helpers.**

The spine spec (§3) mandates raw payloads — never pre-formatted strings. The BBAI
helpers today assemble `key=value` strings at the call site. Two options for the
transition:

**Option 1 — raw fields from day one.** Each `log<Domain>AI` call site is converted to
`emit(DIAGNOSTIC, domainType, field1, field2, ...)` with the consumer rendering the
`[TAG/subtag] key=value` string. Maximally clean; requires cataloguing every call site's
fields before migrating. This is the §5 "field catalog" pre-work flagged in the spec.

**Option 2 — string-carrying DIAGNOSTIC event (transitional).** A `DIAGNOSTIC` event
carries a pre-formatted `const char*` string (the already-assembled log line) through the
spine, with the consumer just writing it. This lets the BBAI helpers adopt the spine
routing path immediately without the full field-catalog pre-work. It is a transitional
shape — the formatted string violates the "raw payload" rule but keeps the lines stable
during migration; raw fields come in a second pass per domain. The spec flags this as an
open question (§5 ⚑).

**Lean (to be confirmed by owner):** Option 2 as an intermediate stage — get all domains
routing through the spine consumer first (preserving stable lines), then convert to raw
fields domain by domain as part of the field-catalog work. Avoids a big-bang conversion
while still collapsing the routing to one path.

### 4C. Per-domain file survival vs unified file

**⚑ Owner ruling required.**

Today: 14 per-domain `.log` files. Target options:

**Option A — keep per-domain files** (routing detail, not a structural constraint). The
logging consumer writes `[CIT/*]` to `CityAI.log`, `[UNT/*]` to `UnitAI.log`, etc. — the
domain routing is just a switch inside the consumer, not per-helper. Grep recipes and
existing forensic workflows are unchanged. GameTracker parses `/events` regardless.

**Option B — collapse to one file** (`AILog.log` or similar) + tag-based routing. Simpler
consumer; one `tail -f` covers all domains. Existing grep recipes still work (tag prefix
is the filter). Loses the ability to `tail -f CityAI.log` for city-only monitoring.

**Lean:** Option A. The per-domain files are useful forensic tools (level 3+ output is
large; separating city from unit from combat avoids multi-MB combined files). The
consolidation win is the single routing path and single tee, not the file count. Keep
files as a consumer routing detail; the consumer is the one place that knows about files.

### 4D. `[PERF]` domain treatment

`logPerf` / `PERF_SCOPE` / `PERF_ACCUM` write to `Performance.log` via `logPerf` which
uses the standard `gDLL->logMsg` + `streamLogTee` pattern but with `gPerfLogLevel` as the
gate (independent from the BBAI AI-log gate — intentional, documented). Post-consolidation
`[PERF]` routes through the spine consumer too but retains its own gate level (the
`gPerfLogLevel` knob maps to its own surveillance tier within the same 0–5 scale). The
`Performance.log` file is retained as a separate destination (timing forensics are a
different use pattern from AI-decision forensics).

**No structural change needed for PERF.** It already follows the gated-helper pattern. The
only work is wiring the `logPerf` emit through `emit(DIAGNOSTIC, PERF_PHASE, ...)` on the
spine, which can be a later pass.

### 4E. Infrastructure logs (out of scope for AI consolidation)

The following sinks are **not part of the AI-logging consolidation** and should not be
touched by the migration:

- OOS / multiplayer logs (`RandomLogger`, `OOSSpecialLogger`, `Multiplayer Game Log`) —
  gated on `isNetworkMultiPlayer()`, serve a distinct purpose.
- `PythonCallstack.log`, `Xml_MissingTypes.log`, `Checksum.log`, `IFP_log.txt`,
  `Asserts.log` / `AssertsJson.log` — infrastructure / crash / debug paths.
- `xml.log`, `MLF.log`, `XmlCheckDoubleTypes.log` — XML load diagnostics.
- `PlotSnapshot_*.csv` — intentional raw-file bypass with documented rationale.

### 4F. Staged migration order (shadow discipline)

Per `event-spine-spec.md` §7: old logging + old `m_pai*Count` stay live as ground truth;
spine emits a superset alongside; diff confirms parity; cut over removes old machinery.
No big-bang.

**Stage 0 (pre-work — field catalog):** For each `log<Domain>AI` domain, document the
complete field set (tag, level, key-value fields) in a structured table. This is the §5
pre-work item flagged in `event-spine-spec.md`. Without it the consumer cannot reconstruct
the lines. Recommend: extend the `ai-logging-reference.md` §3 subsystem tables to be
complete for every domain (currently only `[CIT]`, `[UNT]`, `[COM]`, `[WAR]`, `[CTB]`,
`[PERF]` are expanded; `[WAI]`, `[HAI]`, `[DAI]`, `[DIP]`, `[ESP]`, `[FND]`, `[GRP]`,
`[ENG]` are stubs).

**⚑ Owner ruling required: confirm Option 2 (string-carrying DIAGNOSTIC) is acceptable
as the Stage 1 transitional shape, so Stage 0 field-catalog is not a hard gate for Stage 1.**

**Stage 1 — cascade / spine domains (already partially done):**
`Cascade.log` already routes through the spine consumer (`CvEventSpine.cpp`). Tighten:
- Move `rjLogLine`'s hardcoded `level=1` tee to a proper level parameter (anomaly A-5).
- Have the `CvCascadeLogConsumer` also call `streamLogTee` via the shared helper (not
  direct `publishEvent`) to use one canonical tee path.

**Stage 2 — highest-value BBAI domains (shadow):**
Pick 2-3 domains as the spine migration pilot. Recommended: `[CIT]` (city production,
well-documented, moderate call-site count) and `[UNT]` (unit dispatch, high value for
GameTracker). Shadow: emit via `emit(DIAGNOSTIC, ...)` alongside the existing `logCityAI`
call; logging consumer writes `CityAI.log`; diff the output line by line for one game. On
parity: cut `logCityAI` emit side, keep consumer.

**Stage 3 — remaining BBAI domains:** Roll through `[WAI]`, `[HAI]`, `[DAI]`, `[DIP]`,
`[WAR]`, `[COM]`, `[GRP]`, `[ESP]`, `[FND]`, `[CTB]`, `[ENG]` one at a time per the
serial-conversion rule. Each: shadow → diff → cut. `[CTB]` last (highest call-site
count, most complex gate logic — see anomaly A-16).

**Stage 4 — `[PERF]`:** Wire `logPerf` through the spine consumer. Lower priority;
`[PERF]` already follows the gated-helper pattern cleanly.

**Stage 5 — cleanup (ungated sinks):** Gate or remove the anomalies catalogued in §3:
- Remove ungated `OutputDebugString` calls from `CvUnit`, `CvSelectionGroup`,
  `CvContractBroker`, `CvPlayerAI`, `CvCityAI`, `CvMap`, `CvCity` (anomalies A-8 through
  A-14). Either wrap in `#ifdef _DEBUG` or, for the ones that surface real events (e.g.
  "No buildable defender"), convert to proper `log<Domain>AI` calls.
- Gate the `C2C.log` firehose calls (anomaly A-6) — each should either be deleted
  (if it never helped anyone), gated behind `gPlayerLogLevel`, or converted to a proper
  BBAI helper call.
- Remove the `CvGameTextMgr` city-billboard spam (anomaly A-7).
- Clean up the dead `PropertyBuildingOOS.log` commented code (anomaly A-19).

**Stage 6 — BUG options screen alignment (owner 2026-06-18):** Once the consolidated
structure is set, rework the in-game BUG options → Autolog screen to reflect the unified
0-4 "Surveillance / log level" knob (aligned with the tier names: Oblivious / Telescreen /
Informant / Big Brother / Thought Police + Meta at 5). Remove the phantom per-scope knobs
(`LogLevelTeamBBAI` etc.) that exist in XML but are silently ignored.

The consolidation is not done until the BUG options screen, the gate code
(`refreshOptionsBUG`), the log-consumer routing, and this document are all aligned.

---

## 5. Decisions requiring owner ruling

These are flagged `⚑` in §4 and collected here for a single ruling pass:

**ALL SIX RULED 2026-06-18 (owner) — see the DECIDED column:**

| # | Question | DECIDED |
|---|---|---|
| R-1 | String-carrying DIAGNOSTIC event as a transitional spine payload? | **(b) NO — full raw-field catalog FIRST.** No BBAI helper migrates onto the spine until its lines' fields are catalogued as RAW payloads. The spine stays raw-payload-pure (no string shortcut); the field catalog is Stage-0 prework that gates the migration. |
| R-2 | Per-domain log files, or one unified file? | **(a) Keep per-domain `<Domain>AI.log`** as a consumer routing detail. The unified SURFACE is the spine + `/events`, not the file. |
| R-3 | `logGameInfo` gate? | **(a) Keep ungated** — the session header is always useful. |
| R-4 | `C2C.log` traces — gate or delete? | **(b) DELETE — `C2C.log` is retired.** Legacy traces, no documented consumers, fired every turn. |
| R-5 | Ungated `OutputDebugString` in production paths? | **REPLACE `OutputDebugString` ENTIRELY with the unified logging system (owner).** Do NOT `#ifdef` it — convert every site to the new logger, which fetches the data the same way all other logging does. The owner NEVER uses `OutputDebugString` in day-to-day dev → it is retired as a mechanism; the new surface is its replacement. |
| R-6 | Dead `logCB` / `logToFile` Python exports? | **(b) REMOVE — but DEFERRED with the Python dragon (owner 2026-06-18).** They're dead C++ (no callers, no live Cy binding) but Python-*purposed*; the whole Python structure is a separate future pass ("only ~6 dragons at a time; the Python ones stay for now"). So leave them in place THIS round; remove + route Python through the gated path when the Python pass happens. |
