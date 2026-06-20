# Observability — AI tagged-logging reference — the `[TAG]` registry & gating knobs

> **Status:** reference   ·   **Verified against:** 2026-06-20 (helper declarations confirmed in `Sources/AI/BetterBTSAI.h`; registry/field detail carried from the old `ai-logging-reference.md` and re-grounded to the reorganized `Sources/` tree — line numbers drift)
> **Grounding:** `Sources/AI/BetterBTSAI.h` (the `log<Domain>AI` / `logPerf` / `streamLogTee` declarations + `ScopedPerfTimer`/`PERF_SCOPE`/`PERF_ACCUM`), the per-domain emit sites under `Sources/AI/` + `Sources/Engine/`, and the cascade emit sites under `Sources/Cascade/`.
> This is the **wire spec** for the structured AI decision logs: the `[TAG/subtag]` taxonomy, which file each domain writes, and which knob gates it. Every AI subsystem writes a `<Domain>AI.log` of grep-friendly, `key=value`, single-line records so you can see *what the AI decided and why* without a debugger. Logging is **observation only** — it never changes AI behaviour — but it has been the primary tool for finding AI bugs (a never-ending-turn hang and a long-dead property-control subsystem were both first spotted as anomalous log *volume*).

The observability scale (0–5), the three canonical hook shapes, and the "Orwell" reconstruct-from-API bar
are defined once in [`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]). The `/events` SSE tee
mechanics and the live-read rules (the game holds `.log` files open mid-session; use `/events` +
`/diagnostic`) live in [`http-server.md`](http-server.md). This doc does not restate them — it owns the
**registry detail** (the tag tables + field lists) and the **gating-knob facts**, which are its unique value.

A gated `[TAG]` log line is hook shape #2 ([DEC-obs-hook-shapes]); this doc is the catalog of every such
line the AI surface emits.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function/helper, not
> the integer.

---

## 1. The gating knobs (how to turn it on)

Logs are gated by a verbosity level on the 0–5 surveillance scale ([DEC-obs-scale]: `0` Oblivious · `1`
Telescreen · `2` Informant · `3` Big Brother · `4` Thought Police · `5` Meta). Levels are **cumulative** — a
level-N setting emits 1..N. Placement rule: a line visible in normal play is level ≤ 3; only genuine
inner-loop tracing is 4. The legacy subsystem vocabulary maps onto the scale: level 1 = headline
(`begin`/`best`/`decision`/`mission`/`produced`), 2 = per-decision (`score`/`order`/`act`/`push`/`danger`),
3 = per-candidate (`cand`/`tender/cand`/`skip`), 4 = inner-loop trace (`assess` rejects, `postprocess`).

The actual gate is a set of **scope globals**, set from a BUG option in
`CvGlobals::refreshOptionsBUG` (`Sources/Engine/CvGlobals.cpp`):

| Global | BUG option | Gates these domains | Note |
|---|---|---|---|
| `gPlayerLogLevel` | `Autolog__LogLevelPlayerBBAI` | `[WAI]` `[DAI]` `[DIP]` `[ESP]` `[FND]` `[CTB]` + cascade (`Cascade.log`) | the master knob |
| `gTeamLogLevel` | (same) | `[WAR]` `[ENG]` | **alias of `gPlayerLogLevel`** — the per-scope Team BUG option exists in XML but is deliberately ignored in code |
| `gCityLogLevel` | (same) | `[CIT]` | **alias of `gPlayerLogLevel`** |
| `gUnitLogLevel` | (same) | `[UNT]` `[HAI]` `[COM]` `[GRP]` | **alias of `gPlayerLogLevel`** |
| `gPerfLogLevel` | `Autolog__LogLevelPerf` | `[PERF]` turn timing | **independent knob** — measure performance on any DLL without the verbose AI logs; `0`=off, `1`=timers on |
| `gStreamLogLevel` | `Autolog__LogLevelStream` | the `/events` tee level ceiling | default 1; a line streams only if it ALSO passed its file gate → the stream is a subset of the files |

**One knob in practice.** `refreshOptionsBUG` drives all four AI globals
(`gPlayerLogLevel`/`gTeamLogLevel`/`gCityLogLevel`/`gUnitLogLevel`) from the *single*
`Autolog__LogLevelPlayerBBAI` option, so one setting turns the whole AI surface on. The per-scope BUG
options (`Autolog__LogLevelTeamBBAI` etc.) exist in the UI but their values are discarded — the four globals
are aliases. (The consolidation will replace this scatter with one coherent "Surveillance / log level" knob;
see [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md).)

- **Turn timing is separate.** `[PERF]` has its own `gPerfLogLevel` knob so you can time a Release/Assert
  build without the AI verbosity. It is deliberately *not* forced high in `_DEBUG` (timing a debug build is
  meaningless).
- **The live stream is a further gate.** `gStreamLogLevel` (default 1) controls the `/events` SSE tee; it is
  inert unless `Autolog__HttpServer` is on. See [`http-server.md`](http-server.md) for the tee mechanics and
  the live-read rules — **do not live-read the `.log` files** while the game runs.
- On a `_DEBUG` build the four AI globals are forced to `4`.
- **Volume warning:** level 3 (Big Brother) is verbose; level 4 (Thought Police) is *very* verbose
  (ContractBroker can emit 10k+ lines/turn at 4). The owner plays at 3; use 4 only for a targeted dig.

Files are written to `Documents/My Games/Beyond The Sword/Logs/<Domain>AI.log`.

---

## 2. The domain registry

Each subsystem has a `log<Domain>AI(int level, const char* fmt, ...)` helper declared in
`Sources/AI/BetterBTSAI.h` (defined in `BetterBTSAI.cpp`) that formats one `key=value` line, gated by one
scope global, written to one file, and teed to `/events` via the shared `streamLogTee`. Tag prefixes are
short readable mnemonics; path segments after the `/` are lowercase and may nest (`[XXX/group/detail]`).

| Domain | Tag | Log file | Scope global | Emitted in (post-reorg) |
|---|---|---|---|---|
| Worker | `[WAI]` | `BuildEvaluation.log` | `gPlayerLogLevel` | `Sources/AI/CvWorkerAI.cpp` |
| Hunter | `[HAI]` | `HunterAI.log` | `gUnitLogLevel` | `Sources/AI/CvHunterAI.cpp` |
| Decision (flavours) | `[DAI]` | `DecisionAI.log` | `gPlayerLogLevel` | `Sources/AI/CvDecisionAI.cpp` |
| Diplomacy / deals | `[DIP]` | `DiploAI.log` | `gPlayerLogLevel` | `Sources/AI/CvPlayerAI.cpp`, `Sources/Engine/CvDeal.cpp` |
| Team & war | `[WAR]` | `WarAI.log` | `gTeamLogLevel` | `Sources/AI/CvTeamAI.cpp` (`AI_doWar`) |
| Unit dispatch | `[UNT]` | `UnitAI.log` | `gUnitLogLevel` | `Sources/AI/CvUnitAI.cpp`, `Sources/AI/CvSelectionGroupAI.cpp` |
| City production | `[CIT]` | `CityAI.log` | `gCityLogLevel` | `Sources/AI/CvCityAI.cpp`, `Sources/Engine/CvCity.cpp` |
| Group & army | `[GRP]` | `GroupAI.log` | `gUnitLogLevel` | `Sources/AI/CvSelectionGroupAI.cpp`, `Sources/Engine/CvArmy.cpp` |
| Espionage | `[ESP]` | `EspionageAI.log` | `gPlayerLogLevel` | `Sources/AI/CvPlayerAI.cpp` |
| Founding / settle | `[FND]` | `FoundAI.log` | `gPlayerLogLevel` | `Sources/AI/CvUnitAI.cpp` (`AI_found`) |
| Combat | `[COM]` | `CombatAI.log` | `gUnitLogLevel` | `Sources/AI/CvUnitAI.cpp`, `Sources/AI/CvSelectionGroupAI.cpp` |
| ContractBroker | `[CTB]` | `ContractBroker.log` | `gPlayerLogLevel` | `Sources/AI/CvContractBroker.cpp` |
| Session init header | `[INIT]` | `GameInfo.log` | caller-gated (any logging active) | `Sources/Engine/CvGame.cpp` (`onFinalInitialized` → `logInitInfo`, ex-`logGameInfo`) |
| Engine integrity | `[ENG]` | `Engine.log` | `gTeamLogLevel` | `Sources/Engine/CvPlot.cpp` (more as asserts are demoted) |
| Turn timing | `[PERF]` | `Performance.log` | **`gPerfLogLevel`** (own knob) | `Sources/Engine/CvGame.cpp`, `CvPlayer.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/Engine/CvCity.cpp`, `Sources/AI/CvCityAI.cpp`, `Sources/Cascade/CvPropertySolver.cpp` |

`GameInfo.log` is written once on game start/load (speed, handicap, map, options, players) so every other log
can be read against the active ruleset. The `[GAME/*]` tag was renamed `[INIT/*]` (it logs session INIT, and
`[GAME/*]` clashed with the per-turn `[STATE/game]` cascade feed; `logGameInfo` → `logInitInfo`).

The cascade observability lines (`[SPINE/*]`, `[STATE/game|fin|dip|city]`, `[PLACEMENT]`, `[DORMANCY]`,
`[READJSON]`) are emitted from `Sources/Cascade/` (the spine logging consumer `CvEventSpine.cpp`; the
`rjLogLine` wrapper in `CvCascadeReadJson.cpp`), gated `gPlayerLogLevel >= 1`, written to `Cascade.log`. They
are part of the cascade map, not the BBAI surface — see [`../cascade/data-model.md`](../cascade/data-model.md)
and the per-turn cascade shadow lines documented in [`http-server.md`](http-server.md).

---

## 3. Subsystem tag detail

Only the higher-traffic / more-useful subsystems are expanded here; the rest follow the same
`begin`/`cand`/`score`/`best`/`decision` vocabulary. Authoritative field lists live in the doc-comment beside
each `log<Domain>AI` definition in `Sources/AI/BetterBTSAI.cpp` and the per-domain headers
(`Sources/AI/CvWorkerAI.h`, `Sources/AI/CvDecisionAI.h`). (`[WAI]`, `[HAI]`, `[DAI]`, `[DIP]`, `[ESP]`,
`[FND]`, `[GRP]`, `[ENG]` remain stubs here — completing them is the Stage-0 field-catalog prework for the
spine migration; see [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md).)

### `[CIT]` — city production (`CityAI.log`)

Decision side (`CvCityAI::AI_chooseProduction`):

- `[CIT/begin]` (1) — production-choice context (pop, danger, finances).
- `[CIT/order]` (1) — the order the city commits to (TRAIN/CONSTRUCT/CREATE/MAINTAIN) + reason.
- `[CIT/danger]` (2) — inputs to the "minimal attack (danger)" military gate
  (`minAtk`/`need`/`ownedAtk` SM-effective/`ownedAtkRaw` bodies/`fire`).
- `[CIT/prop]` (2) — inputs to the property-control production gate
  (`val`/`change`/`proj`/`eval`/`check` + `getting`/`good`/`maxed` veto flags + `fire`).
- `[CIT/garrcons]` (1) — Size Matters garrison consolidation (in `CvCityAI::AI_doGarrisonConsolidation`):
  `merges=` defender triples merged this turn, `strLeft=`/`need=` the strength position kept. Each merge also
  logs centrally as `[UNT/merge]`.

Pipeline side (`CvCity::pushOrder`/`popOrder`/`doProduction`):

- `[CIT/push]` (2) — an order enters the queue (catches contract-driven units).
- `[CIT/push/reject]` (2) — the queue anti-spam guard blocked another copy of a unit/building.
- `[CIT/produced]` (1) — a unit/building/project actually completes. UNIT lines carry `ownerHas` (count of
  that unit type) + `aiRoleHas` (count of that UNITAI) → surfaces unit-spam outliers.
- `[CIT/cancel]` (1) — an order popped without finishing (switch / abandon / obsolete) + `progressLost`;
  repeated cancels = production thrashing.
- `[CIT/waste]` (1) — production overflowed the cap and was burned to gold.
- `[CIT/proplevel]` (1) — per-city snapshot of every active property (`val` + per-turn `change`) at turn
  start, for tracking crime/disease/etc. **trends** over time.

### `[UNT]` — unit AI (`UnitAI.log`)

- `[UNT/move]` (2) — which `AI_*Move` routine (UNITAI role) the unit ran this turn.
- `[UNT/act]` (2) — **the "why":** the decision helper that won the unit's prioritised cascade
  (`decision=retreatToCity reason=danger`), emitted at each helper's commit point so only the winning action
  logs. Read with `[UNT/move]` for the full picture. The naval attack-sea cascade is fully covered: alongside
  the generic helpers (`anyAttack`, `patrol`, `safety`, `pillageRange`, `protect`, `group`, `heal`), the
  sea-specific helpers log too — `seaAreaAttack` (`pursueEnemyInArea`/`ambush`), `blockade`
  (`blockadeHere`/`moveToBlockade`), `seaBombard` (`bombardOrPlunder`/`moveToBombard`), `shadow`
  (`shadowHere`/`moveToShadow`).
- `[UNT/role]` (1) — a UNITAI role reassignment (`UNITAI 4 -> 10`). Since garrisoning no longer retypes units
  (#384), these should be rare; a flood of `-> 10` (CITY_DEFENSE) conversions indicates a regression.
- `[UNT/garrison]` (2) — city-garrison membership change (`action=join/leave city=N`, in
  `CvUnitAI::AI_setAsGarrison`). `type=` shows whether the member is a primary defender (CITY_DEFENSE) or an
  auxiliary keeping its own UNITAI.
- `[UNT/mission]` (2) — the committed MissionAI + target (central, in `CvSelectionGroupAI::AI_setMissionAI`).
- `[UNT/merge]` (1) — Size Matters merge: three units became one (`ids=(a,b,c)->d rank= quality=`, in
  `CvUnit::mergeUnits`). Logged because merges deflate raw unit counts under count-based demand targets —
  without this line a census count drop can't be told apart from attrition.
- `[UNT/split]` (1) — Size Matters split: one unit became three (`id=a->(b,c,d)`, in `CvUnit::doSplit`). The
  inverse count distortion of `[UNT/merge]`.
- `[UNT/merge2breach]` (1) — need-driven siege merge decision (in `CvUnitAI::AI_smMergeToBreachCity`): the
  stack cannot out-muscle the target city's best defender as singles (`singleStr`/`defStr`) and merges a
  triple to flip the duel. The merge itself logs as `[UNT/merge]`.
- `[UNT/horde]` (2) — barb horde courage triggered (in `AI_barbAttackMove`): `horde=` NPC units within
  `BARB_HORDE_COURAGE_RANGE` of the adjacent enemy `city=` — the unit wave-assaults at the odds floor (the
  attack commit logs via `[UNT/act]` cityAttack as usual).

### `[COM]` — combat (`CombatAI.log`)

- `[COM/odds]` (3) — attack odds the AI computed for a target plot.
- `[COM/threshold]` (3) — the go/no-go odds bar required to attack.
- `[COM/decision]` (2) — the attack target a unit commits to (`AI_cityAttack` / `AI_anyAttack` /
  `AI_leaveAttack`), closing the odds → outcome loop.
- `[COM/calib]` (3) — calibration: the strength-ratio heuristic (`heurBase`) vs the binomial engine (`binom`,
  what the AI now decides on) win%, pre-bias, per non-air attack eval, with the matchup. See
  [`../../plans/parked/combat-phase3b-plan.md`](../../plans/parked/combat-phase3b-plan.md).

### `[WAR]` — team war (`WarAI.log`, **team** scope)

- `[WAR/begin]` (1) — once/turn per-team baseline (enemy-power %, funding) so the log populates even with no
  transition.
- `[WAR/area]` (1) — per-area military posture change.
- `[WAR/warplan]` (1) — warplan transition vs another team.

### `[CTB]` — ContractBroker (`ContractBroker.log`)

The unit work/contract market. High volume by nature. Headline events (`[CTB/contract]` unit→work,
`[CTB/tender/win]`) are level 1; per-candidate tender evaluation is level 3; the per-unit
assess/reject/suitability spam is level 4.

### `[ENG]` — engine integrity (`Engine.log`)

"Should never happen" sanity checks demoted from `FAssert`/`FErrorMsg` so they still surface in FinalRelease
(where `FASSERT` compiles out) without popping dialogs or writing per-hit stack traces to `Asserts.log`. One
`key=value` line per occurrence — the repeated-line count is itself the signal.

- `[ENG/viscap]` (2) — `CvPlot::changeVisibilityCount`: a team's plot visibility count went negative and was
  capped to zero (`team= plot= count= change=`). Known to fire en masse during `recalculateModifiers` (its
  remove/re-add sight passes subtract sight the reset state never added); a flood *outside* recalc means a
  real sight-accounting bug. The cap itself is suspect — plots capped to 0 on the remove pass get re-added on
  top, which can leave them permanently visible.

### `[PERF]` — turn timing (`Performance.log`, own `gPerfLogLevel` knob)

Wall-clock timing of the headline turn phases, for diagnosing late-game turn-time growth. Implemented with a
RAII scope timer (`PERF_SCOPE(phase, owner)` → `ScopedPerfTimer`, declared in `Sources/AI/BetterBTSAI.h`);
each emits one line on scope exit. Cost when off is a single integer compare, so it ships in normal DLLs.

- `[PERF/phase]` (1) — `turn=<n> owner=<playerId|-1> phase=<label> ms=<elapsed>`. `owner=-1` marks
  game/map-scope phases. ~40 scopes across the per-turn path: `CvGame::doTurn` and its tail (`game.*`), the
  3 Python hooks, `CvPlayer::doTurn` (per player) split into `doTurn.cities`/`AI_doTurnPre`/`AI_doTurnPost`,
  and the per-city tree (`city.doTurn` → `cacheFlush`/`AI_doTurn`/`doProduction`/`AI_chooseProduction`/
  `CalculateAllBuildingValues`/…, tagged by city owner). Aggregate by label. (Measured: `CABV.PreLoop` ≈ 30%
  of the whole turn; the periodic `CvGame::doTurn` spike = `game.autoSave`.)
- `[PERF/cabv]` (1) — one line per `CalculateAllBuildingValues` call decomposing it by dimension
  (`preloop`/`building`/`defense`/…/`food`), via `PERF_ACCUM(double&)` accumulating timers that sum each
  section across the building loop and log once.
- `[PERF/cabvset]` (1) — `turn= owner= city= numBuildings= constructible= enablers= setSize=`, one line per
  constructible-set (re)build. The leak-vs-growth discriminator: a flat `setSize` turn-over-turn means the
  PreLoop rebuild is redundant. Recipe: `grep cabvset Performance.log | awk '{print $3,$8,$9}'`.
- `[PERF/reqmodel]` (1) — one-shot per session (from the CABV PreLoop) verifying the unified prerequisite
  model reproduces the typed prereq fields the enabler index relies on. Summary line `checked buildings=
  units= mismatches=` plus one `[PERF/reqmodel] MISMATCH …` per divergence. **`mismatches=0` is the pass
  condition** — logged (not asserted) so it surfaces in FinalRelease where `FASSERT` compiles out.

Helpers in `Sources/AI/BetterBTSAI.h`: `PERF_SCOPE(label, owner)` times the enclosing scope; `PERF_ACCUM(acc)`
adds the enclosing scope's ms to an accumulator (for interleaved sub-sections). Full scope inventory + awk
recipes lived in the old `turn-time-optimization` plan.

---

## 4. Reading the logs (grep recipes)

The fields are `key=value`, space-separated, by design — `grep`/`awk`/`sort | uniq -c` is the intended
analysis path. (Read these on disk **after the game closes** — the live game holds the files open; for a
live read use `/events`, per [`http-server.md`](http-server.md).)

```bash
cd "Documents/My Games/Beyond The Sword/Logs"

# What did one unit decide this turn (routine + winning action)?
grep 'unit=876682' UnitAI.log

# Production-output stream for a player; spot unit spam (watch aiRoleHas climb)
grep '\[CIT/produced\].*owner=1 UNIT' CityAI.log

# Why is the property gate (not) firing? show the veto flags
grep '\[CIT/prop\].*PROPERTY_CRIME' CityAI.log

# Role-reassignment thrashing (oscillation detector)
grep -oE 'UNITAI [0-9]+ -> [0-9]+' UnitAI.log | sort | uniq -c | sort -rn

# Per-unit re-evaluation spin (a unit appearing hundreds of times = a bug)
grep -oE 'UNT/move\] owner=[0-9]+ unit=[0-9]+' UnitAI.log | sort | uniq -c | sort -rn | head

# Turn-time breakdown: total ms per phase across the game (which phase dominates?)
awk -F'phase=| ms=' '/PERF\/phase/{sum[$2]+=$3} END{for(p in sum) printf "%10.1f  %s\n", sum[p], p}' Performance.log | sort -rn

# Per-player doTurn cost on one turn (which empire is slowest?)
grep 'phase=CvPlayer::doTurn .*turn=312 ' Performance.log | sort -t= -k5 -rn
```

---

## 5. Adding logging to a new subsystem

1. Add `log<Domain>AI(int level, const char* fmt, ...)` to `Sources/AI/BetterBTSAI.{h,cpp}` — copy an
   existing one and swap the **log file name** and the **scope global** (`gPlayerLogLevel` / `gTeamLogLevel` /
   `gCityLogLevel` / `gUnitLogLevel` — pick by the decision's natural owner; note they are aliases today).
2. Reserve a readable ~3-char tag prefix in the registry above (avoid collisions).
3. Instrument the decision functions: `begin`/`end` lines carry the actor id + `turn=` context; pick the
   level by the §1 mapping (headline=1 … inner-loop=4); keep fields `key=value` and space-separated.
4. Document the tag table + level mapping in a doc-comment beside the `log<Domain>AI` definition (and update
   §2/§3 here).
5. For an expensive log line, guard it: `if (g<Scope>LogLevel >= N) { ... }` so the lookups aren't paid when
   logging is off.

C++03 only (no `auto`/lambdas/range-for/`nullptr`; use `foreach_`). Log the *winning* decision at its commit
point (one line per decision), not every candidate, unless it's a level-3+ trace — the repeated-line count is
itself a signal; don't dedup it away (a unit logged 196× was how the turn-hang bug was found).

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]) this doc's levels
  index into, and the three hook shapes ([DEC-obs-hook-shapes]) — a gated `[TAG]` log line is shape #2, and
  this is its catalog.
- [`http-server.md`](http-server.md) — the `/events` SSE tee these lines stream onto, and the live-read rules
  (logs held open mid-session — don't tail them live).
- [`logging-surface-inventory.md`](logging-surface-inventory.md) — the full call-site census (every emit
  site, the gate map, the off-standard anomalies) behind this registry.
- [`../../plans/cascade-migration.md`](../../json-migration/cascade-migration.md) — the in-flight consolidation that
  routes every line through the event-spine logging consumer (one sink, one tee, one gate path).
- [`../../plans/parked/ai-logging-rollout.md`](../../plans/parked/ai-logging-rollout.md) — the original
  rollout plan this reference documents the realized state of.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
