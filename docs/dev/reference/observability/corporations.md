# Corporations — Observability Map

> DRAFT observability map (2026-06-18 by parent agent) — every claim cited from code; verify before relying.
> Scope: the #428/#430 total-observability ("Orwell") bar — can the running corporation state be
> reconstructed PURELY from the HTTP endpoints + /events + gated logs, without looking at the game screen?

---

## 1. How it actually works

### 1-A. Two modes of operation: Classic vs Advanced Realistic

There are **two completely distinct operating modes** gated by `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS`
(`CvEnums.h:868`).

- **Classic mode** (flag absent): corporations are **spread manually** by "executive" units that execute
  `MISSION_SPREAD_CORPORATION` (`CvEnums.h:1668`). `doCorporation()` returns immediately
  (`CvCity.cpp:21461`). No autonomous per-turn spread.
- **Advanced mode** (flag present): `doCorporation()` runs per city per turn (`CvCity.cpp:1316`) and
  autonomously propagates or decays each corporation. Manual unit-spread is **blocked** unless
  `MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING` is also set (`CvUnit.cpp:8592`).

In both modes, `calcCorporateMaintenance()` (`CvCity.cpp:21662`) and
`calculateCorporationMaintenanceTimes100()` (`CvCity.cpp:7791`) run as usual — the gold drain is
always present once a corporation is active.

### 1-B. Founding / headquarters placement

**Advanced mode**: `CvGame::doFoundCorporation()` (`CvGame.cpp:11166`) is the auto-founder. It runs
periodically (every `CORPORATION_FOUND_CHECK_TURNS` × speedPercent turns) and selects a best city by
`getCorporationInfluence()` × `getSpread()` + RNG. If `MODDERGAMEOPTION_NO_AUTO_CORPORATION_FOUNDING`
is set it defers to the unit-spread path instead (`CvGame.cpp:11184`).

**Classic mode**: `CvPlayer::foundCorporation()` (`CvPlayer.cpp:8893`) is called from event script /
Python. It picks the best city by population + bonus resources + RNG, then calls
`CvCity::setHeadquarters()` → `CvGame::setHeadquarters()` (`CvGame.cpp:5619`), which installs the HQ
building and calls `setHasCorporation(corp, true)` on the HQ city.

`isCorporationFounded` = `m_paiCorporationGameTurnFounded[eIndex] != -1`; turn recorded in
`CvGame::makeCorporationFounded()` (`CvGame.cpp:5405`).

### 1-C. Per-turn autonomous spread and decay (Advanced mode only)

`CvCity::doCorporation()` (`CvCity.cpp:21457`), called inside `CvCity::doTurn()` (`CvCity.cpp:1316`),
for each corporation not yet present in this city:

1. Checks HQ exists + `canEverSpread()` (game option gate) + not blocked by civic
   (`isNoCorporations()` / `isNoForeignCorporations()`).
2. Sums `iRandThreshold` = max over all CONNECTED cities of:
   `pLoopCity->getCorporationInfluence(corp)` × `getSpread()` / 100 ÷ distance_factor
   (`CvCity.cpp:21492–21506`), then multiplied by player spread modifier and owner influence
   (`CvCity.cpp:21512–21514`), divided by `1 + getCorporationCount()/2` (`CvCity.cpp:21516`).
3. Rolls `getSorenRandNum(CORPORATION_SPREAD_RAND × speedPercent / 100)`. If roll < threshold →
   removes competing corps first, then `setHasCorporation(corp, true)` (`CvCity.cpp:21548`).

**Decay** (also in `doCorporation()`): for cities that already have the corp but are not the HQ:
compares `getAverageCorporationInfluence()` against the city's own adjusted influence. If average
exceeds local influence (iDiff > 0), rolls same RNG; if roll < iDiff →
`setHasCorporation(corp, false)` (`CvCity.cpp:21577`).

### 1-D. Manual unit spread (Classic mode, or Advanced+NO_AUTO_FOUNDING)

`CvUnit::canSpreadCorporation()` (`CvUnit.cpp:8564`): checks `getCorporationSpreads(corp) > 0`, city
present, not already has corp, obsolete-tech check, building prereqs, can-enter-area, owner
`isActiveCorporation`, no competing HQ in city, at least one prereq bonus present, gold >= cost.

`CvUnit::spreadCorporation()` (`CvUnit.cpp:8694`): deducts gold (spread cost modified by foreign
territory and competing corps), rolls `getCorporationSpreads(corp)` (halved for foreign territory)
against `getSorenRandNum(100)`, calls `setHasCorporation` on success, kills the unit in either case.
`spreadCorporationCost()` (`CvUnit.cpp:8668`): base `getSpreadCost()` × foreign-percent modifier ×
per-active-competing-corp spread factor.

### 1-E. isActiveCorporation — the dormancy gate

**City level** (`CvCity::isActiveCorporation()`, `CvCity.cpp:13419`): the city `isHasCorporation` AND
`CvPlayer::isActiveCorporation(corp)` is true AND corp not obsoleted by team tech AND the city has
at least one prereq bonus (if corp requires any).

**Player level** (`CvPlayer::isActiveCorporation()`, `CvPlayer.cpp:14031`): not
`isNoCorporations()`, not (`isNoForeignCorporations()` and doesn't own HQ), not obsoleted by team
tech.

**Effect of going inactive**: all yields/commerce from the corp drop to 0 (`getCorporationYieldByCorporation`
and `getCorporationCommerceByCorporation` both return 0 when `!isActiveCorporation`), the
corporation maintenance still accrues but at 0 rate. Buildings whose `getPrereqCorporation()` matches
are disabled via `applyCorporationModifiers()` → `setDisabledBuilding()` (`CvCity.cpp:15182`).

### 1-F. Resource consumption and bonus production

`getCorporationInfluence()` (`CvCity.cpp:21586`): city-level influence = 100 base, +1 per available
instance of each prereq bonus, + `CORPORATION_RESOURCE_BASE_INFLUENCE / iBonusesConsumed` for each
*present* (not just available) prereq bonus, divided by 10 for each active competing corp, ×
population / averagePop.

`updateCorporationBonus()` (`CvCity.cpp:12681`): iterates corps active in the city; for each corp
with `getBonusProduced() != -1`, adds `aiLastCorpProducedBonus[bonusConsumed]` units of the produced
bonus to a `m_corpBonusProduction` vector (up to N iterations to handle chained production). These
produced bonuses are added via `processBonus()` when the set changes. This is the "resource
transformation" mechanic — e.g. corp A consumes Oil to produce Steel.

Maintenance formula (`calculateCorporationMaintenanceTimes100(corp)`, `CvCity.cpp:7807`):
- Base = sum of `HeadquarterCommerce` over all commerce types × 100.
- + `getMaintenance() × numBonuses × worldSizeCorporationMaintenancePercent / 100`.
- × `(17 + pop) / 18` (population scaling).
- In Advanced mode: × handicap² / 8000; Classic: × handicap / 100.
- Modifier by player `getCorporationMaintenanceModifier()` + team modifier.
- Rebels pay 50%.

### 1-G. Yields and commerce from active corporations

`getCorporationYieldByCorporation(yield, corp)` (`CvCity.cpp:12574`): if active + not disorder,
returns `getYieldChange(yield) × 100 + getYieldProduced(yield) × sum(numBonuses) × worldSizePercent
/ 100`, rounded up. Summed per yield type into `m_aiCorporationYield` by `updateCorporationYield()`.

`getCorporationCommerceByCorporation(commerce, corp)` (`CvCity.cpp:12596`): same structure but for
commerce, also multiplied by `getCorporationRevenueModifier()` from the team.

Aggregated in `m_aiCorporationCommerce`, fed into `getBaseCommerceRate()` (`CvCity.cpp:11921`).

### 1-H. AI decision-making (spread)

`CvUnitAI::AI_spreadCorporation()` (`CvUnitAI.cpp:13540`): if the unit can spread a corp and a good
target city exists (`bestCorporationCity()`), pushes `MISSION_SPREAD_CORPORATION`. The mission logs
via `[UNT/act]` (level 2) at the push point — but the spread *result* (success/fail at the target)
is not logged.

`CvPlayerAI::AI_corporationValue()` (`CvPlayerAI.cpp:12291`): evaluates approximate gpt value of
spreading the corp to a given city (used for training executive units and choosing spread targets).
This is internal to the AI decision; not observable from outside.

`CvCityAI` at `CvCityAI.cpp:9034`: evaluates `AI_executiveValue` (a random-gated check, not every
turn) and trains executive units. The train decision surfaces as `[CIT/produced]` when a unit
completes.

---

## 2. Current observability

**Current tier: Tier 0 — Oblivious** for corporations specifically.

The snapshots at Tier 1 (`/cities`, `/players`, `/units`) capture:
- `/cities`: population, yields, commerce (but NOT broken down by source — corporation yield contribution
  is folded into the aggregate `food`/`production`/`commerce` numbers, indistinguishable from building
  yields). No corporation-specific fields.
- `/players`: gold rate (includes corporation maintenance drag, but not isolated). No corporation-spread
  modifier, no HQ flags, no per-corp city counts.
- `/units`: unit type/AI — an executive unit with `UNITAI_MISSIONARY` and `getCorporationSpreads > 0`
  is visible by its `type` field (e.g. `UNIT_EXECUTIVE_*`), but there is no `missionCorp` field.

The `/events` stream carries:
- `[UNT/act]` (level 2): the `AI_spreadCorporation` commit when a unit starts the mission. This fires
  at `pushMission`, not at the spread *result*.
- `[CIT/produced]` (level 1): when an executive unit finishes production in a city, with `type=` and
  `aiRoleHas=`.
- `[PERF/phase]` (level 1): `city.doCorporation` phase time per city per turn.

What is **not exposed today**:

| State | Where it lives | Observable today? |
|---|---|---|
| Which corporations are present in each city (`isHasCorporation`) | `CvCity.m_pabHasCorporation[]` | **NO** — no `/cities` field |
| Which corporations are *active* (`isActiveCorporation`) per city | derived from HasCorp + prereq bonuses + player gates | **NO** |
| HQ location per corporation | `CvGame.m_paHeadquarters[]` | **NO** |
| Corporation founded turn | `CvGame.m_paiCorporationGameTurnFounded[]` | **NO** |
| Per-city corporation influence score | computed in `getCorporationInfluence()` | **NO** |
| Spread/decay RNG rolls and their threshold | inside `doCorporation()` | **NO** |
| Spread success / decay events | `setHasCorporation` call inside `doCorporation` | **NO** — only a `DLL message` for human players, not a machine-readable event |
| Per-city corporation yield contribution | `m_aiCorporationYield[]` | **NO** (folded into aggregate) |
| Per-city corporation commerce contribution | `m_aiCorporationCommerce[]` | **NO** (folded into aggregate) |
| Per-city corporation maintenance cost | `calculateCorporationMaintenance()` | **NO** — no `/cities` field |
| Per-city corp bonus production (resource transformation) | `m_corpBonusProduction` | **NO** |
| Player-level corp spread modifier | `getCorporationSpreadModifier()` | **NO** (not in `/players`) |
| Player's active corp count per corp type | `getHasCorporationCount()` | **NO** |
| AI corp value score (`AI_corporationValue`) | internal to `CvPlayerAI` | **NO** |
| Manual spread attempt result (success/fail) | inside `spreadCorporation()` | **NO** — only a `DLL message` to the unit owner |

---

## 3. The gap

The entire corporation state is opaque from outside. Given the total-observability bar, we cannot today:

1. **Reconstruct the corporation presence map** — which corps are in which cities. `/cities` has no
   `hasCorporation` array. Without this, the maintenance formula (`§1-F`), the active-corp dormancy
   gate (`§1-E`), the building-disable cascade (`applyCorporationModifiers`), and the yield/commerce
   contributions cannot be replicated from the wire.

2. **Observe spread/decay events** — `doCorporation()` calls `setHasCorporation` silently (only a
   `DLL message` to human-visible UI, not a machine-readable event). An autoplay session where corp
   boundaries shift cannot be reconstructed turn-by-turn.

3. **Know the HQ location** — needed to evaluate the influence formula and the "connected to HQ"
   condition. Not in any snapshot.

4. **Observe the AI corporation decision** — `AI_spreadCorporation` logs via `[UNT/act]` at mission
   push (visible in `/events`), but the *value calculation* (`AI_corporationValue`) that drove the
   decision is silent. Whether the AI decided to train an executive this turn is only visible via
   `[CIT/produced]` when the unit actually finishes, not when the decision was made.

5. **See corporation maintenance as a separate line** — the `/cities` snapshot folds maintenance
   into the overall gold-rate picture; no per-corp or per-city breakdown.

6. **Observe the resource-transformation chain** — `updateCorporationBonus()` mutates the effective
   bonus set silently. A city that has Corp A (consuming Iron → producing Horses) shows the Horses in
   its bonus count but the chain is invisible.

---

## 4. Proposed hooks

All hooks follow the existing `gPlayerLogLevel`-gated pattern (`logPlayerAI` / `logCityAI` at the
appropriate scope) and should be added as **gated log lines** teed to `/events` via `streamLogTee`,
matching the `[TAG/subtag] key=value` taxonomy. Endpoint fields are additions to the existing
publish-and-serve snapshot. All hooks are **read-only** — the OOS firewall is not touched.

### Hook 1: `/cities` snapshot — add corporation presence array

**What**: add a `corporations` array to each city's JSON in `renderCities()` /
`CvHttpServer.cpp`'s snapshot build loop (`CvHttpServer.cpp:~1542`). Each element: corp type string
+ `active` bool.

```json
"corporations": [
  {"type": "CORPORATION_MINING_INC", "active": true},
  {"type": "CORPORATION_CEREAL_MILLS", "active": false}
]
```

Populated from `isHasCorporation(corp)` and `isActiveCorporation(corp)` in the `publishIfDue()` city
walk. The HQ flag can be `"hq": true` for the HQ city (check `GC.getGame().getHeadquarters(corp) == pCity`).

**Priority**: CRITICAL — without this, no other reconstruction is possible.

### Hook 2: `/players` snapshot — add per-player corporation fields

Add to `PlayerSnap` / `renderPlayers()`:
- `hqCorps`: array of corp type strings where `hasHeadquarters(corp)` is true.
- `hasCorporationCount`: object `{CORP_TYPE: count}` for corps with count > 0 (from
  `getHasCorporationCount()`).
- `noCorporations`: bool (civic gate, from `isNoCorporations()`).
- `noForeignCorporations`: bool (civic gate, from `isNoForeignCorporations()`).

**Priority**: HIGH — needed to reconstruct the player-level active gate.

### Hook 3: `[CORP/spread]` gated log tag — per-turn spread/decay events

**Tag**: `[CORP/spread]` in a new `logCorpAI(int level, ...)` helper or reuse `logCityAI` at level 1.
Log file: reuse `CityAI.log` or create `CorpAI.log`.
Scope global: `gCityLogLevel`.

Emit at the `setHasCorporation(corp, true/false)` calls inside `doCorporation()` (`CvCity.cpp:21548`
and `21577`):

```
[CORP/spread] turn=N city=<name> owner=N corp=CORP_X action=spread threshold=N roll=N influence=N
[CORP/spread] turn=N city=<name> owner=N corp=CORP_X action=decay avgInfluence=N cityInfluence=N roll=N
```

Also emit when `doCorporation()` SKIPS a city for a corp (level 2):
```
[CORP/spread] turn=N city=<name> owner=N corp=CORP_X action=skip reason=noHQ|noCivic|noBonusReach|competitorPresent
```

**Priority**: HIGH — this is the event spine for the system; without it, autoplay reconstruction is
impossible.

### Hook 4: `[CORP/found]` — HQ placement event

Emit at `CvGame::setHeadquarters()` (`CvGame.cpp:5619`) when `pNewValue != NULL`:

```
[CORP/found] turn=N corp=CORP_X city=<name> owner=N x=X y=Y foundTurn=N
```

And at `CvGame::doFoundCorporation()` when a best city is found (level 1):
```
[CORP/found] turn=N corp=CORP_X mode=auto city=<name> owner=N spread=N
```

**Priority**: HIGH — the HQ location is load-bearing for influence/spread calculation; its placement
must be visible.

### Hook 5: `[CORP/maintenance]` — per-city per-turn maintenance snapshot

Emit once per active corporation per city at level 2 inside `calcCorporateMaintenance()` (Advanced)
or `calculateCorporationMaintenanceTimes100(corp)` (Classic):

```
[CORP/maint] turn=N city=<name> owner=N corp=CORP_X maintenance=N bonuses=N pop=N handicapMod=N
```

**Priority**: MEDIUM — useful for AI budget analysis; the per-turn gold drain from corporations is
currently invisible.

### Hook 6: `[CORP/bonus]` — resource-transformation events

Emit inside `updateCorporationBonus()` (`CvCity.cpp:12681`) when `bChanged` is true (the corps
bonus production set mutated), at level 2:

```
[CORP/bonus] turn=N city=<name> owner=N corp=CORP_X produced=BONUS_X count=N consumes=BONUS_Y
```

**Priority**: MEDIUM — needed for the cascade to replicate the `getBonusProduced` chain.

### Hook 7: `/diagnostic/corpSweep` endpoint

A read-only diagnostic endpoint (same mailbox pattern as `placementSweep`) that for `player=N`:
- Lists every corporation, its HQ city+owner, founding turn.
- For each of the player's cities: which corps are present, which are active, the city-level
  influence for each.
- The player-level `isActiveCorporation` gate result per corp.

This is the snapshot analogue of Hook 1/2 — a full point-in-time state dump for verification.

Example:
```
GET /diagnostic/corpSweep?player=1
{
  "player": 1,
  "noCorporations": false,
  "noForeignCorporations": false,
  "corps": [
    {"type": "CORPORATION_MINING_INC", "founded": true, "hqCity": "Rome", "hqOwner": 2, "foundTurn": 145,
     "playerActive": true, "cityCount": 7,
     "cities": [
       {"cityId": 3, "name": "London", "has": true, "active": true, "influence": 132, "maintenance": 44}
     ]
    }
  ]
}
```

**Priority**: HIGH — this is the "render-from-API" bar for corporations.

### Hook 8: `[CORP/ai]` — AI executive valuation trace (level 3)

Emit inside `AI_corporationValue()` (`CvPlayerAI.cpp:12291`) and `AI_executiveValue()` at level 3:

```
[CORP/ai] turn=N player=N corp=CORP_X city=<name> value=N reason=<label>
```

And at `AI_spreadCorporation()` commit (`CvUnitAI.cpp:13776`):

```
[CORP/ai] turn=N player=N unit=N corp=CORP_X action=spreadMission targetCity=<name>
```

**Priority**: LOW-MEDIUM — the `[UNT/act]` already surfaces the mission commit; this fills in the
value reasoning.

---

## 5. Summary

| Gap | Hook | Priority |
|---|---|---|
| Corp presence + active per city | Hook 1 (`/cities` array) | CRITICAL |
| HQ location + player-level gates | Hook 2 (`/players` fields) | HIGH |
| Per-turn spread/decay events | Hook 3 (`[CORP/spread]` log) | HIGH |
| HQ founding events | Hook 4 (`[CORP/found]` log) | HIGH |
| Full on-demand corp state | Hook 7 (`/diagnostic/corpSweep`) | HIGH |
| Per-city maintenance breakdown | Hook 5 (`[CORP/maint]` log) | MEDIUM |
| Resource-transformation events | Hook 6 (`[CORP/bonus]` log) | MEDIUM |
| AI valuation trace | Hook 8 (`[CORP/ai]` log) | LOW-MEDIUM |

With Hooks 1, 2, 3, 4, and 7 implemented, corporations reach **Tier 4 (Thought Police)** for their
domain: the full per-turn state is reconstructible from endpoints + /events, including for AI players
the agent cannot watch on screen.

---

## 6. Key code references

| Function | File:line | What |
|---|---|---|
| `CvCity::doCorporation()` | `CvCity.cpp:21457` | Per-turn spread/decay (Advanced mode only) |
| `CvCity::getCorporationInfluence()` | `CvCity.cpp:21586` | City-level influence score |
| `CvGame::getAverageCorporationInfluence()` | `CvGame.cpp:11265` | Average influence (decay threshold) |
| `CvGame::doFoundCorporation()` | `CvGame.cpp:11166` | Auto-founder for Advanced mode |
| `CvPlayer::foundCorporation()` | `CvPlayer.cpp:8893` | Manual HQ placement |
| `CvGame::setHeadquarters()` | `CvGame.cpp:5619` | HQ assignment + HQ building install |
| `CvGame::replaceCorporation()` | `CvGame.cpp:3209` | Hostile-takeover cascade |
| `CvGame::canEverSpread()` | `CvGame.cpp:11533` | Game-option gate |
| `CvCity::isActiveCorporation()` | `CvCity.cpp:13419` | Dormancy gate (city level) |
| `CvPlayer::isActiveCorporation()` | `CvPlayer.cpp:14031` | Dormancy gate (player level) |
| `CvCity::setHasCorporation()` | `CvCity.cpp:15215` | Corp install/remove + modifiers |
| `CvCity::applyCorporationModifiers()` | `CvCity.cpp:15182` | Building disable/enable + stat changes |
| `CvCity::updateCorporationBonus()` | `CvCity.cpp:12681` | Resource-transformation chain |
| `CvCity::calculateCorporationMaintenanceTimes100(corp)` | `CvCity.cpp:7807` | Per-corp maintenance |
| `CvCity::calcCorporateMaintenance()` | `CvCity.cpp:21662` | Advanced-mode maintenance (pop-weighted) |
| `CvUnit::canSpreadCorporation()` | `CvUnit.cpp:8564` | Manual spread precondition check |
| `CvUnit::spreadCorporation()` | `CvUnit.cpp:8694` | Manual spread execution + RNG |
| `CvUnit::spreadCorporationCost()` | `CvUnit.cpp:8668` | Spread gold cost |
| `CvUnitAI::AI_spreadCorporation()` | `CvUnitAI.cpp:13540` | AI mission dispatch |
| `CvPlayerAI::AI_corporationValue()` | `CvPlayerAI.cpp:12291` | AI value score |
| `CvCorporationInfo::getPrereqBonuses()` | `CvCorporationInfo.h:46` | Required bonus resources |
| `CvCorporationInfo::getBonusProduced()` | `CvCorporationInfo.h:38` | Produced bonus (resource transform) |
| `CvCorporationInfo::getSpread()` | `CvCorporationInfo.h:59` | Base spread rate |
| `CvCorporationInfo::getMaintenance()` | `CvCorporationInfo.h:35` | Base per-bonus maintenance |
| `CvPlayer::getCorporationInfluence()` | `CvPlayer.cpp:27701` | Player-level influence modifier |
| `CvPlayer::getCorporationSpreadModifier()` | `CvPlayer.cpp:27673` | Civic spread modifier |
| Snapshot publish | `CvHttpServer.cpp:1461` | Where to add new fields |
| Corp in `/cities` JSON | `CvHttpServer.cpp:340` | renderCities — currently no corp fields |
