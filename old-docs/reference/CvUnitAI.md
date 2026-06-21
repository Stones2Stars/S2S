# CvUnitAI

**File:** `CvUnitAI.h` / `CvUnitAI.cpp`  
**Inherits:** [`CvUnit`](CvUnit.md)

## Overview

Adds automated behaviour to every unit in the game. The class manages the full unit
lifecycle from AI perspective: mission selection, movement, combat decisions, contract
dispatch, healing, promotion, upgrade, and serialisation.

## Contract State Machine

Units participate in the `CvContractBroker` workflow through a five-state machine:

| State | Description |
|---|---|
| `CONTRACTUAL_STATE_NONE` | Not currently seeking work. |
| `CONTRACTUAL_STATE_AWAITING_ANSWER` | Work request sent; waiting for broker response. |
| `CONTRACTUAL_STATE_AWAITING_WORK` | Matched to a request; en route to work site. |
| `CONTRACTUAL_STATE_FOUND_WORK` | Arrived at site and executing mission. |
| `CONTRACTUAL_STATE_NO_WORK_FOUND` | Broker search failed; falls back to default behaviour. |

## Lifecycle

| Method | Description |
|---|---|
| `CvUnitAI(bool bIsDummy)` | Constructor; initialises all AI state. Pass `true` for placeholder/dummy units. |
| `AI_init(UnitAITypes, int iBirthmark)` | Sets AI type and unique birthmark; resets state. |
| `AI_uninit()` | Releases resources; resets variables. |
| `AI_reset(UnitAITypes, bool bConstructorCall)` | Full state reset; used both at construction and on reassignment. |
| `operator=(const CvUnitAI&)` | Deep copy of all AI members. |
| `read(FDataStreamBase*)` | Loads all AI state from a save stream. |
| `write(FDataStreamBase*)` | Saves all AI state to a save stream. |

## Core Behaviour

| Method | Description |
|---|---|
| `AI_update()` | Main per-turn entry. Checks if the unit can move, handles automation/contracts, executes the current mission, and finalises state. Returns `true` if the unit should wait until next turn. |
| `AI_follow()` | Pursues a target: attacks, pillages, or advances toward an objective. Returns `true` if an action was taken. |
| `AI_load(UnitAITypes, ...)` | Finds and boards the best available transport matching the given criteria. |
| `AI_upgrade()` | Walks the upgrade chain and performs an upgrade if conditions are met. |
| `AI_promote()` | Evaluates the available promotion tree and selects the best option. |

## Combat

| Method | Description |
|---|---|
| `AI_attackOdds(pPlot, bPotentialEnemy, ...)` | Calculates attack odds against the best defender on a plot (0–100). |
| `AI_attackOddsAtPlot(pPlot, pDefender, ...)` | Calculates odds against a specific defender on a plot. |
| `AI_sacrificeValue(pPlot)` | Estimates how expendable the unit is in a combat on the given plot. |
| `scoreCityHealerNeed(eUnitCombat, eDomain, pCity)` | Scores a city's need for a healer of a given combat type and domain. |

## Build & City Support

| Method | Description |
|---|---|
| `AI_bestCityBuild(pCity, ...)` | Finds the best tile improvement to build in or around a city. Returns the target plot and build type. |
| `AI_isCityAIType()` | Returns `true` if the unit's AI type is a city-related role (garrison, defender, etc.). |
| `AI_isCityGarrison(pCity)` | Returns `true` if this unit is tasked as a garrison for the given city. |
| `AI_setAsGarrison(pCity)` | Assigns this unit as garrison for a city. |

## City garrison: primary vs auxiliary tiers (#384)

City defense runs on **two independent ledgers**, and they must not be conflated:

- **Garrison membership** (`m_iGarrisonCity`, serialized; set via `AI_setAsGarrison`,
  self-expiring — each `AI_update` drops membership unless the unit's move re-affirms it).
  Membership is the **auxiliary tier**: every member's defensive strength counts toward the
  city's *actual* defense (`PUF_isCityGarrison` → `CvCityAI::getGarrisonStrength` →
  `AI_isDefended`, and the broker request priority in `AI_doContractFloatingDefenders`).
  **Any** combat unit can be a member — it keeps its own UNITAI while parked.
- **The `UNITAI_CITY_DEFENSE` role** is the **primary tier**: only deliberately-assigned
  defenders (unit training/role selection — never a parking side effect) satisfy the
  count-based demand gates (`AI_chooseProduction` defender gates, `AI_minDefenders`
  searches in `AI_guardCityMinDefender`, `AI_getTotalFloatingDefenders`).

Rules that follow (owner rulings, issue #384):

- **Garrisoning never retypes a unit.** `AI_guardCity` and the leave-a-defender-behind
  paths (`AI_goToTargetCity`, `AI_cityAttack`) mark membership + park persistently
  (FORTIFY → SLEEP → SKIP), keeping the unit's UNITAI. A strong-but-unsuited unit (e.g. a
  `noDefensiveBonus()` elephant gunner) stays as auxiliary defense without occupying a
  primary slot, so the city keeps training/requesting real defenders.
- **Overdefended beats underdefended — join eagerly, release reluctantly.** The single
  release gate (`AI_guardCity`'s `AI_isDefended` hold test) demands a
  `GARRISON_RELEASE_MARGIN_PERCENT` (125%) strength surplus before an existing garrison
  member may leave; joining is judged at the plain 100% bar.
- **"Guard the vicinity" means radius 2.** `AI_guardCity`'s guard-spot chooser historically
  scanned `rect(NUM_CITY_PLOTS_2, …)` — but that constant is a plot COUNT (21), not a
  radius, so "vicinity" guards marched to defensive tiles up to 21 tiles away while staying
  members of the origin city's garrison (caught live in the #384 playtest: an elephant
  gunner parked 7 tiles out, contributing nothing — `getGarrisonStrength` only reads range
  2). Now `rect(2,2)`, matching the range-2 strength accounting; and the in-city recall
  preference includes garrison members, not just city-AI types, since auxiliaries keep
  their own UNITAI.
- **Garrison wildlife sorties are harvests, not defense — near-sure odds only (#400).**
  Animals cannot attack cities, but killing one banks food/production/commerce yields
  (owner ruling: a capable defender taking that kill is valid strategy). So
  `AI_leaveAttack` from an own city demands `GARRISON_ANIMAL_SORTIE_MIN_ODDS` (85% lead
  win) for animal-only target plots, while real military targets keep the caller's 55–70
  bar — coin-flip bear duels bled the prehistoric defense corps 84 → 25 by turn 71. Field
  use is unchanged; attack-incapable units (`bOnlyDefensive`) were never able to sortie
  (an attacker-less group gets 0% win odds). `AI_chokeDefend` sorties via
  `AI_leaveAttack(1, 65, 150)`, not `AI_anyAttack` — the anyAttack call was the #382 cold
  case's second site (range-inflated to 4–6 tiles through `AI_searchRange`, no territory
  bound).
- **Mis-typed primary defenders self-correct.** At its re-plan, a CITY_DEFENSE unit with
  `noDefensiveBonus()` — the categorical unsuitability test, the same one strict
  `AI_findBestDefender` uses to reject defense candidates — demotes back to its XML
  default UNITAI (top of `AI_cityDefenseMove`). This bleeds the historic retype
  population out of existing saves; with the retype sources gone it only shrinks. The
  *general* "is this unit any good at its role" re-evaluation remains #380 (reset tool)
  / `unit-ai-valuation.md` territory.
- Membership changes log as `[UNT/garrison]` (level 2) — `type=` distinguishes primary
  (CITY_DEFENSE) from auxiliary members.

## Group & Mission

| Method | Description |
|---|---|
| `AI_groupFirstVal()` | Leader-priority value for group formation (range: `LEADER_PRIORITY_MIN`–`LEADER_PRIORITY_MAX`). |
| `AI_groupSecondVal()` | Secondary sort value (combat strength) for group formation. |
| `AI_searchRange(int iRange)` | Calculates the effective search radius for mission targeting. |
| `AI_isAwaitingContract()` | Returns `true` if the unit is in `CONTRACTUAL_STATE_AWAITING_ANSWER`. |

## Birthmark & direction finding (automation spreading)

Every unit gets a **birthmark** at `AI_init` — a stable per-unit AI seed. Wherever several
automated units would otherwise make identical choices, behaviour variation is keyed off it
(`AI_getBirthmark() % N` gates exist across the unit AI). For DIRECTIONS there is exactly **one
place** that turns the birthmark into a heading: `AI_getPreferredDirection()` (birthmark mod
`NUM_DIRECTION_TYPES`) with `AI_directionAffinity(eDir)` scoring how closely a candidate
direction matches it (4 = exactly preferred … 0 = opposite). Consumers: `AI_moveToBorders`
(fans units out to different border crossings) and `AI_patrolBorders` (fans patrollers into
different ring directions). New direction-spreading behaviours must use these helpers, not
re-derive birthmark math.

## Border Patrol (AUTOMATE_BORDER_PATROL)

`AI_borderPatrol()` is the human "Border Patrol" automation. Cascade (2026-06-11, #24):
return-to-borders (when >2 tiles out) → heal → **intercept** visible enemies nearest-first out
to range 12 (odds bar = the `AUTO_PATROL_MIN_COMBAT_ODDS` modder option, +5/+10/+15 at range
5/7/12; targets confined to own territory except range 1-2 with `AUTO_PATROL_CAN_LEAVE_BORDERS`)
→ **walk the border** (`AI_patrolBorders`: randomized circuit biased onto border tiles,
continues the unit's heading, pulled toward the unit's preferred compass sector so patrollers
fan out; when automated mid-territory with no border in reach it hands off to
`AI_moveToBorders` instead of random-walking the interior) → roam outside (option) →
retreat/safety. City capturing during intercepts is gated by `AUTO_PATROL_NO_CITY_CAPTURING`.
Historical bug: the range-7/12 intercepts used to sit AFTER the walk and were unreachable —
patrollers circled past interior intruders.

**Known deferred limitation (owner decision 2026-06-11):** coverage concentrates on the border
ring, so the deep interior of a very large empire can sit in fog where intruders are invisible
to the intercept scans ("a unit can only hunt what someone can see"). Judged good enough for
now; if enemy units are observed jogging around freely in the center of a large empire, file a
fresh specialized issue for coverage-driven territory patrol (sketch existed in #376:
visibility-staleness-weighted wander + the shared birthmark direction bias, with a UI rename to
"Territory Patrol").

## State / Identity

| Method | Description |
|---|---|
| `AI_getUnitAIType()` | Returns the current `UnitAITypes` role. |
| `AI_setUnitAIType(UnitAITypes)` | Overrides the unit's AI role. |
| `AI_getBirthmark()` | Returns the unit's unique AI identifier (set at creation). |
| `AI_setBirthmark(int)` | Sets the unit's birthmark (normally only called during `AI_init`). |
| `AI_flushValueCache()` | Clears the cached generic value score so it is recalculated next time. |
| `getIntendedConstructBuilding()` | Returns the building this unit intends to construct (if any). |
| `getIntendedHeritage()` | Returns the heritage this unit intends to found (if any). |

## Related

- [`CvUnit`](CvUnit.md) — base class (non-AI state)  
- [`CvSelectionGroupAI`](CvSelectionGroupAI.md) — groups of units driven together  
- [`CvContractBroker`](CvContractBroker.md) — work-request dispatch system  
- [`CvPlayerAI`](CvPlayerAI.md) — player AI that drives unit updates  
- [`BetterBTSAI.h`](../BetterBTSAI.h) — logging macros (`LOG_BBAI_UNIT`, `LOG_EVALAI_UNIT`)  
