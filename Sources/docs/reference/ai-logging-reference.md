# AI tagged-logging reference

Developer reference for the structured AI decision logs. Every AI subsystem writes a
`<Domain>AI.log` of grep-friendly, tagged lines so you can see *what the AI decided and
why* without a debugger. This complements the design plan in
[`ai-logging-rollout.md`](../plans/ai-logging-rollout.md) — that one is the rollout plan, this one
documents the system as it actually exists.

The logging is **observation only**: it never changes AI behaviour. It has, however, been
the primary tool for finding AI bugs (e.g. a never-ending-turn hang and a long-dead
property-control subsystem were both first spotted as anomalous log volume).

---

## 1. Enabling it

Logs are gated by a verbosity level. Set it via the in-game **BUG options → Autolog**
("Player BBAI log level"). All four internal scope globals
(`gPlayerLogLevel` / `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel`) are currently
driven from that **single** option (`Autolog__LogLevelPlayerBBAI`), so one knob turns
everything on. `0` = off.

On a `_DEBUG` build all four are forced to `4`.

**Turn timing is separate.** The `[PERF]` domain (wall-clock turn-phase timing) has its
**own** knob — **BUG options → Autolog → "Performance / turn-timing log level"**
(`Autolog__LogLevelPerf` → `gPerfLogLevel`) — so you can measure real performance with any
DLL (Assert/Release) without turning on the verbose AI logs. It is deliberately *not* forced
to 4 in `_DEBUG` (timing a debug build is meaningless). `0` = off; `1` enables the timers.

Logs are written to:

```
Documents/My Games/Beyond The Sword/Logs/<Domain>AI.log
```

### Level mapping

Levels are cumulative (a level-N setting emits levels 1..N). They mean the same thing in
every subsystem:

| Level | Meaning | Typical tags |
|---|---|---|
| **1** | Headline: begin/end, baselines, the final decision/mission committed | `begin`, `best`, `decision`, `mission`, `produced` |
| **2** | Per-decision: scores, per-option choices, dedup/skip summaries | `score`, `order`, `act`, `push`, `danger` |
| **3** | Per-candidate trace: each candidate/flavour/factor considered | `cand`, `tender/cand`, detailed `skip` |
| **4** | Inner-loop trace: per-unit / per-iteration spam (off by default) | `assess` rejects, `postprocess` |

**Volume warning:** level 3 is verbose and level 4 is *very* verbose (ContractBroker can
emit 10k+ lines/turn at level 4). For routine play use 1–2; use 3 when chasing a specific
decision; use 4 only for a targeted dig.

---

## 2. The registry

Each subsystem has a `log<Domain>AI(int level, fmt, ...)` helper in `BetterBTSAI.{h,cpp}`
that writes one file, gated by one scope global. Tag prefixes are short readable mnemonics;
path segments after the `/` are lowercase and may nest (`[XXX/group/detail]`).

| Domain | Tag | Log file | Scope global | Emitted in |
|---|---|---|---|---|
| Worker | `[WAI]` | `BuildEvaluation.log` | player | `CvWorkerAI` |
| Hunter | `[HAI]` | `HunterAI.log` | unit | `CvHunterAI` |
| Decision (flavours) | `[DAI]` | `DecisionAI.log` | player | `CvDecisionAI` |
| Diplomacy / deals | `[DIP]` | `DiploAI.log` | player | `CvPlayerAI`, `CvDeal` |
| Team & war | `[WAR]` | `WarAI.log` | team | `CvTeamAI::AI_doWar` |
| Unit dispatch | `[UNT]` | `UnitAI.log` | unit | `CvUnitAI`, `CvSelectionGroupAI` |
| City production | `[CIT]` | `CityAI.log` | city | `CvCityAI`, `CvCity` |
| Group & army | `[GRP]` | `GroupAI.log` | unit | `CvSelectionGroupAI`, `CvArmy` |
| Espionage | `[ESP]` | `EspionageAI.log` | player | `CvPlayerAI` |
| Founding / settle | `[FND]` | `FoundAI.log` | player | `CvUnitAI::AI_found` |
| Combat | `[COM]` | `CombatAI.log` | unit | `CvUnitAI`, `CvSelectionGroupAI` |
| ContractBroker | `[CTB]` | `ContractBroker.log` | player | `CvContractBroker` |
| Game session header | `[GAME]` | `GameInfo.log` | (ungated) | `CvGame::onFinalInitialized` |
| Engine integrity | `[ENG]` | `Engine.log` | team | `CvPlot` (more as asserts are demoted) |
| Turn timing | `[PERF]` | `Performance.log` | **`gPerfLogLevel`** (own knob) | `CvGame`, `CvPlayer`, `CvPlayerAI`, `CvCity`, `CvCityAI`, `CvPropertySolver` |

`GameInfo.log` is written once on game start/load (speed, handicap, map, options, players)
so every other log can be read against the active ruleset.

---

## 3. Subsystem tag detail

Only the higher-traffic / more-useful subsystems are expanded here; the rest follow the
same `begin`/`cand`/`score`/`best`/`decision` vocabulary. Authoritative field lists live in
the doc-comment beside each `log<Domain>AI` in `BetterBTSAI.cpp` and the headers
(`CvWorkerAI.h`, `CvDecisionAI.h`).

### `[CIT]` — city production (`CityAI.log`)

Decision side (`CvCityAI::AI_chooseProduction`):
- `[CIT/begin]` (1) — production-choice context (pop, danger, finances).
- `[CIT/order]` (1) — the order the city commits to (TRAIN/CONSTRUCT/CREATE/MAINTAIN) + reason.
- `[CIT/danger]` (2) — inputs to the "minimal attack (danger)" military gate
  (`minAtk`/`need`/`ownedAtk` SM-effective/`ownedAtkRaw` bodies/`fire`).
- `[CIT/prop]` (2) — inputs to the property-control production gate
  (`val`/`change`/`proj`/`eval`/`check` + `getting`/`good`/`maxed` veto flags + `fire`).
- `[CIT/garrcons]` (1) — Size Matters garrison consolidation (#395, in
  `CvCityAI::AI_doGarrisonConsolidation`): `merges=` defender triples merged this turn,
  `strLeft=`/`need=` the strength position kept. Each merge also logs centrally as
  `[UNT/merge]`.

Pipeline side (`CvCity::pushOrder`/`popOrder`/`doProduction`):
- `[CIT/push]` (2) — an order enters the queue (catches contract-driven units).
- `[CIT/push/reject]` (2) — the queue anti-spam guard blocked another copy of a unit/building.
- `[CIT/produced]` (1) — a unit/building/project actually completes. UNIT lines carry
  `ownerHas` (count of that unit type) + `aiRoleHas` (count of that UNITAI) → surfaces
  unit-spam outliers.
- `[CIT/cancel]` (1) — an order popped without finishing (switch / abandon / obsolete) +
  `progressLost`; repeated cancels = production thrashing.
- `[CIT/waste]` (1) — production overflowed the cap and was burned to gold.
- `[CIT/proplevel]` (1) — per-city snapshot of every active property (`val` + per-turn
  `change`) at turn start, for tracking crime/disease/etc. **trends** over time.

### `[UNT]` — unit AI (`UnitAI.log`)

- `[UNT/move]` (2) — which `AI_*Move` routine (UNITAI role) the unit ran this turn.
- `[UNT/act]` (2) — **the "why":** the decision helper that won the unit's prioritised
  cascade (`decision=retreatToCity reason=danger`), emitted at each helper's commit point so
  only the winning action logs. Read with `[UNT/move]` for the full picture. The naval
  attack-sea cascade is now fully covered: alongside the generic helpers (`anyAttack`,
  `patrol`, `safety`, `pillageRange`, `protect`, `group`, `heal`), the sea-specific helpers
  log too — `seaAreaAttack` (`pursueEnemyInArea`/`ambush`), `blockade`
  (`blockadeHere`/`moveToBlockade`), `seaBombard` (`bombardOrPlunder`/`moveToBombard`),
  `shadow` (`shadowHere`/`moveToShadow`). See [`../plans/sea-ai-rework.md`](../plans/sea-ai-rework.md).
- `[UNT/role]` (1) — a UNITAI role reassignment (`UNITAI 4 -> 10`). Since #384 garrisoning
  no longer retypes units, so these should be rare; a flood of `-> 10` (CITY_DEFENSE)
  conversions indicates a regression.
- `[UNT/garrison]` (2) — city-garrison membership change (`action=join/leave city=N`,
  in `CvUnitAI::AI_setAsGarrison`). `type=` shows whether the member is a primary defender
  (CITY_DEFENSE) or an auxiliary keeping its own UNITAI — see the garrison-tiers section
  in [`CvUnitAI.md`](CvUnitAI.md).
- `[UNT/mission]` (2) — the committed MissionAI + target (central, in
  `CvSelectionGroupAI::AI_setMissionAI`).
- `[UNT/merge]` (1) — Size Matters merge: three units became one
  (`ids=(a,b,c)->d rank= quality=`, in `CvUnit::mergeUnits`). Logged because merges
  deflate raw unit counts under count-based demand targets
  ([`../plans/unit-ai-valuation.md`](../plans/unit-ai-valuation.md) §A5) — without this
  line, a census count drop can't be told apart from attrition.
- `[UNT/split]` (1) — Size Matters split: one unit became three
  (`id=a->(b,c,d)`, in `CvUnit::doSplit`). The inverse count distortion of `[UNT/merge]`.
- `[UNT/merge2breach]` (1) — need-driven siege merge decision (#395, in
  `CvUnitAI::AI_smMergeToBreachCity`): the stack cannot out-muscle the target city's best
  defender as singles (`singleStr`/`defStr`) and merges a triple to flip the duel. The
  merge itself logs as `[UNT/merge]`.

### `[COM]` — combat (`CombatAI.log`)

- `[COM/odds]` (3) — attack odds the AI computed for a target plot.
- `[COM/threshold]` (3) — the go/no-go odds bar required to attack.
- `[COM/decision]` (2) — the attack target a unit commits to (`AI_cityAttack` / `AI_anyAttack`
  / `AI_leaveAttack`), closing the odds → outcome loop.
- `[COM/calib]` (3) — Phase 3b calibration: the strength-ratio heuristic (`heurBase`) vs the
  binomial engine (`binom`, what the AI now decides on) win%, pre-bias, per non-air attack
  eval, with the matchup. The harness for re-tuning the `AI_finalOddsThreshold *23/20`
  recenter and the future per-player aggression bar. See [`../plans/combat-phase3b-plan.md`](../plans/combat-phase3b-plan.md).

### `[WAR]` — team war (`WarAI.log`, **team** scope)

- `[WAR/begin]` (1) — once/turn per-team baseline (enemy-power %, funding) so the log
  populates even with no transition.
- `[WAR/area]` (1) — per-area military posture change.
- `[WAR/warplan]` (1) — warplan transition vs another team.

### `[CTB]` — ContractBroker (`ContractBroker.log`)

The unit work/contract market. High volume by nature. Headline events
(`[CTB/contract]` unit→work, `[CTB/tender/win]`) are level 1; per-candidate tender
evaluation is level 3; the per-unit assess/reject/suitability spam is level 4.

### `[ENG]` — engine integrity (`Engine.log`)

"Should never happen" sanity checks demoted from `FAssert`/`FErrorMsg` so they still
surface in FinalRelease (where `FASSERT` compiles out) without popping dialogs or writing
per-hit stack traces to `Asserts.log`. One `key=value` line per occurrence — the
repeated-line count is itself the signal.

- `[ENG/viscap]` (2) — `CvPlot::changeVisibilityCount`: a team's plot visibility count
  went negative and was capped to zero (`team= plot= count= change=`). Known to fire en
  masse during `recalculateModifiers` (its remove/re-add sight passes subtract sight that
  the reset state never added); a flood *outside* recalc means a real sight-accounting bug.
  Note the cap itself is suspect — plots capped to 0 on the remove pass get re-added on
  top, which can leave them permanently visible.

### `[PERF]` — turn timing (`Performance.log`, own `gPerfLogLevel` knob)

Wall-clock timing of the headline turn phases, for diagnosing late-game turn-time growth.
Implemented with a RAII scope timer (`PERF_SCOPE(phase, owner)` →
`ScopedPerfTimer`/`win32::Stopwatch`, declared in `BetterBTSAI.h`); each emits one line on
scope exit. Cost when off is a single integer compare, so it ships in normal DLLs.

- `[PERF/phase]` (1) — `turn=<n> owner=<playerId|-1> phase=<label> ms=<elapsed>`. `owner=-1`
  marks game/map-scope phases. ~40 scopes across the per-turn path: `CvGame::doTurn` and its full
  tail (`game.*` — `doDeals`/`teamDoTurn`/`mapDoTurn`/`doSpawns`/`writePlotSnapshot`/`autoSave`/… +
  the 3 Python hooks `game.py.beginGameTurn`/`preEndGameTurn`/`endGameTurn`), `CvPlayer::doTurn` (per
  player) split into `doTurn.cities`/`AI_doTurnPre`/`AI_doTurnPost`, the `AI_doTurnPre` internals
  (`pre.*`), and the per-city tree (`city.doTurn` → `cacheFlush`/`AI_doTurn`/`doProduction`/
  `AI_chooseProduction`/`CalculateAllBuildingValues`/…, tagged by city owner). Aggregate by label.
  (Measured target: `CalculateAllBuildingValues.PreLoop` ≈ 30% of the whole turn; the periodic
  `CvGame::doTurn` spike = `game.autoSave`.)
- `[PERF/cabv]` (1) — one line per `CalculateAllBuildingValues` call decomposing it by dimension
  (`preloop`/`building`/`defense`/…/`food`), via `PERF_ACCUM(double&)` accumulating timers that sum
  each section across the building loop and log once. (Found the CABV hot spot = the `PreLoop`.)
- `[PERF/cabvset]` (1) — `turn= owner= city= numBuildings= constructible= enablers= setSize=`,
  one line per constructible-set (re)build. The leak-vs-growth discriminator: a flat `setSize`
  turn-over-turn means the PreLoop rebuild is redundant (→ cross-turn retention is safe). Recipe:
  `grep cabvset Performance.log | awk '{print $3,$8,$9}'`. (Since #195 the PreLoop builds the set
  from the static enabler reverse-index `GC.getBuildingsEnabledBy`, so `enablers=` counts
  directly-constructible buildings that have a non-empty dependent list.)
- `[PERF/reqmodel]` (1) — one-shot per session (fired from the CABV PreLoop) verifying the
  #195 unified prerequisite model (`CvBuildingInfo::getConstructRequirements` /
  `CvUnitInfo::getTrainRequirements`) reproduces the typed prereq fields the enabler index
  relies on. A summary line `checked buildings= units= mismatches=` plus one
  `[PERF/reqmodel] MISMATCH building=…|unit=…` per divergence. **`mismatches=0` is the pass
  condition** — it is logged (not asserted) precisely so it surfaces in FinalRelease, where
  `FASSERT` compiles out. See [`../plans/unified-prerequisites-and-constructibility.md`](../plans/unified-prerequisites-and-constructibility.md).

Helpers in `BetterBTSAI.h`: `PERF_SCOPE(label, owner)` times the enclosing scope; `PERF_ACCUM(acc)`
adds the enclosing scope's ms to an accumulator (for interleaved sub-sections). Creep check:
`Tools/turn-perf-trend.awk` least-squares-fits a phase's ms/turn and prints a CREEP/flat verdict
(`-v phase=total|CvPlayer::doTurn|doTurn.cities`). Full scope inventory + awk recipes:
[`../plans/turn-time-optimization.md`](../plans/turn-time-optimization.md).

---

## 4. Reading the logs (grep recipes)

```bash
cd "Documents/My Games/Beyond The Sword/Logs"

# What did one unit decide this turn (routine + winning action)?
grep 'unit=876682' UnitAI.log

# Production-output stream for a player; spot unit spam (watch aiRoleHas climb)
grep '\[CIT/produced\].*owner=1 UNIT' CityAI.log

# Why is the property gate (not) firing? show the veto flags
grep '\[CIT/prop\].*PROPERTY_CRIME' CityAI.log

# Crime trend per city over turns
grep '\[CIT/proplevel\].*PROPERTY_CRIME' CityAI.log

# Role-reassignment thrashing (oscillation detector)
grep -oE 'UNITAI [0-9]+ -> [0-9]+' UnitAI.log | sort | uniq -c | sort -rn

# Per-unit re-evaluation spin (a unit appearing hundreds of times = a bug)
grep -oE 'UNT/move\] owner=[0-9]+ unit=[0-9]+' UnitAI.log | sort | uniq -c | sort -rn | head

# Turn-time breakdown: total ms per phase across the game (which phase dominates?)
awk -F'phase=| ms=' '/PERF\/phase/{sum[$2]+=$3} END{for(p in sum) printf "%10.1f  %s\n", sum[p], p}' Performance.log | sort -rn

# How much is the visibility "stickytape" costing each turn?
grep 'phase=doTurn.visibilityRebuild' Performance.log

# Per-player doTurn cost on one turn (which empire is slowest?)
grep 'phase=CvPlayer::doTurn .*turn=312 ' Performance.log | sort -t= -k5 -rn
```

The fields are `key=value`, space-separated, by design — `grep`/`awk`/`sort | uniq -c` is
the intended analysis path.

---

## 5. Adding logging to a new subsystem

1. Add `log<Domain>AI(int level, const char* fmt, ...)` to `BetterBTSAI.{h,cpp}` — copy an
   existing one and swap the **log file name** and the **scope global** (`gPlayerLogLevel` /
   `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel` — pick by the decision's natural owner).
2. Reserve a readable ~3-char tag prefix in the registry above (avoid collisions).
3. Instrument the decision functions:
   - `begin`/`end` lines carry the actor id + `turn=` context.
   - Pick the level by the mapping in §1 (headline=1 … inner-loop=4).
   - Keep fields `key=value` and space-separated.
4. Document the tag table + level mapping in a doc-comment beside the `log<Domain>AI`
   definition (and update §2/§3 here).
5. For an expensive log line, guard it: `if (g<Scope>LogLevel >= N) { ... }` so the lookups
   aren't paid when logging is off.

### Conventions

- C++03 only (no `auto`, lambdas, range-for, `nullptr` in new code paths that must match the
  surrounding style; use `foreach_`).
- Adding a `.cpp` to a new `Sources/<Dir>/` requires wiring `Sources/fbuild.bff`, not just the
  `.vcxproj` (else LNK2001 in FastBuild only).
- Log the *winning* decision at its commit point (one line per decision), not every candidate,
  unless it's a level-3+ trace. The repeated-line count is itself a signal — don't dedup it
  away (a unit logged 196× was how the turn-hang bug was found).
