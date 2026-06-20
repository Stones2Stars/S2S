# Handicaps / difficulty — the per-player vs game-handicap split

> **Status:** reference   ·   **Verified against:** `Sources/Engine/CvGame.cpp`, `CvPlayer.cpp`, `CvTeam.cpp`, `CvCity.cpp`, `CvUnit.cpp`, `CvInitCore.cpp`; `Assets/XML/GameInfo/CIV4HandicapInfo.xml` — accessor sites re-confirmed 2026-06-20.
> **Grounding:** the `getHandicapType()` accessors + `averageHandicaps()` were re-walked in the live source on 2026-06-20 (the `Cv*` engine files moved into `Sources/Engine/` since the original authoring; paths below reflect that home). The per-field application-site line numbers (§"AI-advantage read sites") are carried from the prior verified sweep and were **not** all re-walked this pass — treat each as "the function named here, around this line."
> A *handicap* is the difficulty setting, and it is two things sharing one name. Bottom line up front: **human-facing economic fields read each player's OWN per-player handicap; AI-advantage `getAI*` fields read the single GAME handicap (the average of alive humans).** So an AI's research/production/cost advantages scale with the human's difficulty, never with the AI's own per-player handicap.

> **Citations drift.** Line numbers are "the function named here, around this line" — confirm the **function**, not the integer. The original doc cited bare `Sources/Cv*.cpp`; these files now live under `Sources/Engine/`.

---

## Two things called "handicap"

- A **per-player handicap** — every player slot has its own `HandicapTypes` value in
  `CvInitCore::m_aeHandicap[playerID]`, the difficulty of that individual player
  (`CvPlayer::getHandicapType()`, `Sources/Engine/CvPlayer.cpp:12378`).
- A **single game handicap** — `CvGame::m_eHandicap`, one value for the whole game
  (`CvGame::getHandicapType()`, `Sources/Engine/CvGame.cpp:4714`). It is **not saved**; it is recomputed
  as the **integer average of the alive *human* players' per-player handicaps** by
  `CvGame::averageHandicaps()` (`Sources/Engine/CvGame.cpp:10390`).

Because all AIs default to `HANDICAP_NOBLE` and the game handicap tracks the *human's* chosen difficulty,
the AIs' production/research/cost advantages scale with the human's difficulty, not with any per-AI
difficulty. See [Setting an AI to non-Noble](#setting-an-ai-to-non-noble).

`CvHandicapInfo` (info class `Sources/Infos/CvHandicapInfo.{h,cpp}`, data in
`Assets/XML/GameInfo/CIV4HandicapInfo.xml`) holds ~50 integer fields plus a goody list and
`PropertyManipulators`. The levels, easiest → hardest, are `HANDICAP_SETTLER, CHIEFTAIN, WARLORD, NOBLE,
PRINCE, MONARCH, EMPEROR, IMMORTAL, DEITY, NIGHTMARE, NIGHTMARE_PLUS, AI_BOOSTED`. `NOBLE` is the 100 %
baseline.

> The fields are difficulty knobs, not modifier-cascade tenants — for whether each could become a tenant
> of the #423 `CvCascadingModifierBundle`, see [the cascade fit verdict](#cascade-423-tenancy-verdict)
> below. Percent fields here are plain XML integers, not the cascade's fixed-point ×100 — that scale lives
> in [the scale registry](../cascade/fixed-point-and-scales.md) and does not apply to `CvHandicapInfo`.

---

## The four `getHandicapType()` accessors

| Accessor | Returns | Source | When it is the right one |
| --- | --- | --- | --- |
| `CvGame::getHandicapType()` | `m_eHandicap`, the single game difficulty (average of alive humans) | `CvGame.cpp:4714` | Anything *global* (barbarians, animals, goodies) and **every AI-advantage field** (`getAI*`). |
| `CvPlayer::getHandicapType()` | that player's own per-player handicap | `CvPlayer.cpp:12378` | Human-facing per-player economics: happiness/health, maintenance %, upkeep %, starting gold, inflation base. Applies to whoever owns the handicap, human or AI alike. |
| `CvCity::getHandicapType()` | owner player's handicap (`GET_PLAYER(getOwner()).getHandicapType()`) | `CvCity.cpp:5211` | City-level reads of the owner's per-player handicap (maintenance/health). Pure delegation. |
| `CvUnit::getHandicapType()` | owner player's handicap | `CvUnit.cpp:10678` | Unit-level reads of the owner's per-player handicap. Pure delegation. |
| `CvTeam::getHandicapType()` | **average** of the per-player handicaps of alive team members | `CvTeam.cpp:2947` | Team-scoped diplomacy/trade reads (tech-trade thresholds). **Note:** despite the name, no AI-advantage cost field uses it, and it is a *different* average than `CvGame`'s (team members vs. alive humans). |

`FAssert(m_eHandicap != NO_HANDICAP)` in `CvGame::getHandicapType()` (`CvGame.cpp:4716`) means reading the
game handicap before `averageHandicaps()` has run trips an assert in Assert/Debug builds.

---

## How handicaps are set

- **Default at reset:** every slot is initialized to `STANDARD_HANDICAP` (`CvInitCore.cpp:747`), which
  `GlobalDefines.xml` defines as `HANDICAP_NOBLE`. Barbarian/NPC slots are overwritten with
  `BARBARIAN_HANDICAP` during `CvGame` setup.
- **Do all AIs share NOBLE by default? Yes.** On a fresh start with no scenario override, every AI keeps
  its reset value `HANDICAP_NOBLE`; the human's chosen difficulty is written to the human slot, and the
  *game* handicap becomes the average of alive humans (`averageHandicaps()`, `CvGame.cpp:10390`). With one
  human at `EMPEROR`, the game handicap is `EMPEROR` while every AI's per-player handicap stays `NOBLE`.
  (Matches the live `/players` HTTP snapshot: all AIs `HANDICAP_NOBLE`, human `HANDICAP_EMPEROR`.)
- **`averageHandicaps()` runs** at game init and whenever a human's handicap changes; it sets the game
  handicap to `iAggregate / iHumanCount` over alive humans, or `STANDARD_HANDICAP` if no humans are alive
  (verified in the body at `CvGame.cpp:10390`). Two humans at `EMPEROR` (6) and `NOBLE` (3)
  integer-average to `PRINCE` (4) — the UI can say "EMPEROR" while AIs get the lower-tier bonuses.
- **Saving:** per-player handicaps in `CvInitCore` are saved; the game handicap is reconstructed on load
  (`reset(NO_HANDICAP)` then `averageHandicaps()`).
- **`CvPlayer::setHandicap(iNewVal, bAdjustGameHandicap=true)`** writes the per-player value to InitCore,
  optionally re-averages the game handicap, and marks maintenance + unit-upkeep dirty so the per-player
  economic fields recompute. `CvInitCore::copyNonDefaults` restores per-player handicaps from
  scenario/save files, so a manually-set AI handicap persists.
- **Increasing difficulty** (`GAMEOPTION_CHALLENGE_INCREASING_DIFFICULTY`, `CvGame::doIncreasingDifficulty`)
  raises **only human** players' handicaps (`isHumanPlayer(true)` guard), then re-averages. AIs are never
  touched here.
- **Flexible difficulty** (`CvGame::doFlexibleDifficulty`) can raise/lower **both** human and AI handicaps
  based on score deviation; AI changes are gated by the modder option
  `MODDERGAMEOPTION_AI_USE_FLEXIBLE_DIFFICULTY` and bounded by per-game AI min/max. It calls
  `setHandicap()` for each adjusted player. This is the only in-game path that gives an AI a non-NOBLE
  *per-player* handicap — and even then, see [Gotchas](#gotchas).

---

## Field categories

`CvHandicapInfo` fields split into two families. The **human-facing** family is read through the *owner's*
per-player handicap (so it applies to whoever owns it, human or AI). The **AI-only `getAI*`** family is
gated behind an `isNormalAI()` / `!isHuman()` check and (with one exception) read through the **game**
handicap.

| Family | Representative getters | Read through | Applies to |
| --- | --- | --- | --- |
| Economy / upkeep (human-facing) | `getStartingGold`, `getUnitUpkeepPercent`, `getDistanceMaintenancePercent`, `getNumCitiesMaintenancePercent`, `getColonyMaintenancePercent`, `getMaxColonyMaintenance`, `getCorporationMaintenancePercent`, `getCivicUpkeepPercent`, `getInflationPercent`, `getRevolutionIndexPercent` | owner's per-player handicap | every player |
| Wellbeing (human-facing) | `getHealthBonus`, `getHappyBonus` | owner's per-player handicap | every player |
| Advanced Start | `getAdvancedStartPointsMod` | owner's per-player handicap | every player |
| Diplomacy / trade | `getAttitudeChange`, `getNoTechTradeModifier`, `getTechTradeKnownModifier` | target player's / team's per-player handicap | every player |
| Starting units (human side) | `getStartingDefenseUnits`, `getStartingWorkerUnits`, `getStartingExploreUnits` | own per-player handicap | humans only |
| Barbarian / animal control (global) | `getAnimalAttackProb`, `getAnimalCombatModifier`, `getBarbarianCombatModifier`, `getUnownedWaterTilesPerBarbarianUnit`, `getUnownedTilesPerBarbarianCity`, `getBarbarianCityCreationTurnsElapsed`, `getBarbarianCityCreationProb`, `getBarbarianInitialDefenders`, `getFreeWinsVsBarbs` | **game** handicap | global |
| AI-only production / growth | `getAITrainPercent`, `getAIWorldTrainPercent`, `getAIConstructPercent`, `getAIWorldConstructPercent`, `getAICreatePercent`, `getAIWorldCreatePercent`, `getAIResearchPercent`, `getAIGrowthPercent`, `getAIWorkRateModifier` | **game** handicap | AI only |
| AI-only costs / upkeep | `getAIUnitUpkeepPercent`, `getAICivicUpkeepPercent`, `getAIUnitSupplyPercent`, `getAIUnitUpgradePercent`, `getAIInflationPercent`, `getAIWarWearinessPercent` | **game** handicap | AI only |
| AI-only military / behaviour | `getAIAnimalCombatModifier`, `getAIBarbarianCombatModifier`, `getSubdueAnimalBonusAI`, `getAIDeclareWarProb` | **game** handicap | AI only |
| AI-only starting units | `getAIStartingDefenseUnits`, `getAIStartingWorkerUnits`, `getAIStartingExploreUnits` | **game** handicap | AI only |
| Era scaling (multiplier on the AI family) | `getAIPerEraModifier` | **game** handicap | AI only |
| AI Advanced Start | `getAIAdvancedStartPercent` | **own per-player handicap** (the single exception) | AI only |
| Goodies / properties | `getGoodies(i)`, `getNumGoodies`, `getPropertyManipulators` | game handicap (goody filtering) | global |

---

## AI-advantage read sites (read site → handicap source)

Almost every `getAI*` field is read through `GC.getGame().getHandicapType()` (the game handicap), behind an
`isNormalAI()` / `!isHumanPlayer()` / `!isHuman()` guard. The `getAIPerEraModifier()` term
(`× getCurrentEra()`) is applied alongside most of them.

> The line numbers in this table are carried from the prior verified sweep and were not individually
> re-walked on 2026-06-20 (paths are now under `Sources/Engine/`). Confirm the function, not the integer.

| AI field | Primary read site | Guard | Handicap source |
| --- | --- | --- | --- |
| `getAITrainPercent` / `getAIWorldTrainPercent` | `CvPlayer.cpp:~7004` | `isNormalAI()` | **game** |
| `getAIConstructPercent` / `getAIWorldConstructPercent` | `CvPlayer.cpp:~7131` | `isNormalAI()` | **game** |
| `getAICreatePercent` / `getAIWorldCreatePercent` | `CvPlayer.cpp:~7194` | `isNormalAI()` | **game** |
| `getAIUnitSupplyPercent` | `CvPlayer.cpp:~7952` | `isNormalAI()` | **game** |
| `getAIInflationPercent` | `CvPlayer.cpp:~8008` | `isNormalAI()` | **game** |
| `getAIWorkRateModifier` | `CvPlayer.cpp:~9878`; also `CvUnit.cpp:~27934` | `isNormalAI()` | **game** |
| `getAIUnitUpkeepPercent` | `CvPlayer.cpp:~10371` | `!isHumanPlayer()` | **game** |
| `getAIWarWearinessPercent` | `CvPlayer.cpp:~10973` | `isNormalAI()` | **game** |
| `getAICivicUpkeepPercent` | `CvPlayer.cpp:~14262` | `isNormalAI()` | **game** |
| `getAIGrowthPercent` | `CvPlayer.cpp:~24467` | `isNormalAI()` | **game** |
| `getAIResearchPercent` | `CvTeam.cpp:~2654` | `!isNPC() && !isHuman(true)` | **game** (read on `CvTeam`, but from `GC.getGame()`) |
| `getAIUnitUpgradePercent` | `CvUnit.cpp:~10360` | `!isHuman()` | **game** |
| `getAIAnimalCombatModifier` | `CvUnit.cpp:~11609, ~11631` | defender/attacker not human | **game** |
| `getAIBarbarianCombatModifier` | `CvUnit.cpp:~11654, ~11678` | not human | **game** |
| `getAIDeclareWarProb` | `CvTeamAI.cpp:~4205, ~4380`; `CvDLLWidgetData.cpp:~4093` | — | **game** |
| `getAIStartingDefenseUnits` | `CvPlayer.cpp:~1928` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIStartingWorkerUnits` | `CvPlayer.cpp:~1937` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIStartingExploreUnits` | `CvPlayer.cpp:~1945` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIPerEraModifier` | applied alongside all of the above | inherited | **game** |
| **`getAIAdvancedStartPercent`** | `CvPlayer.cpp:~1889` | `!isHumanPlayer()` | **own per-player handicap** ⟵ exception |

**The dual-application pattern.** Several cost fields apply *both* a per-player human-facing field (own
handicap) *and* an AI-only modifier (game handicap), in sequence:

- Unit upkeep — `getUnitUpkeepPercent()` (own, `CvPlayer.cpp:~10366`), then for AIs `getAIUnitUpkeepPercent()`
  and `getAIPerEraModifier()` (game, `~10371`).
- Civic upkeep — `getCivicUpkeepPercent()` (own, `~14256`), then for AIs `getAICivicUpkeepPercent()` (game, `~14262`).
- Inflation — `getInflationPercent()` (own, `~7987`), then for AIs `getAIInflationPercent()` (game, `~8008`).

So an AI's unit-upkeep number is the *NOBLE* per-player `getUnitUpkeepPercent` (100 %) multiplied by the
*game* handicap's `getAIUnitUpkeepPercent`.

The military-analysis read at `CvUnitAI.cpp:~3413` is a special case: it uses
`GET_PLAYER(pTargetCity->getOwner()).getHandicapType()` — the *defender's* own per-player handicap, not the
game handicap — to scale `getAITrainPercent` when sizing up a target city (an attacker estimating a
defender's production rate; intentional but unusual).

---

## Game-global difficulty effects (keyed on the game handicap)

These read `GC.getGame().getHandicapType()` and affect the whole game regardless of any individual player's
handicap.

| Effect | Field(s) | Read site |
| --- | --- | --- |
| Barb-city spawn delay | `getBarbarianCityCreationTurnsElapsed` | `CvGame.cpp:~6973` |
| Barb-city spawn probability / density | `getBarbarianCityCreationProb` | `CvGame.cpp:~7012, ~7019` |
| Barb-city placement tile threshold | `getUnownedTilesPerBarbarianCity` | `CvGame.cpp:~7058` |
| Water-barb spawn density (halved by `GAMEOPTION_BARBARIAN_RAGING`) | `getUnownedWaterTilesPerBarbarianUnit` | `CvGame.cpp:~7168` |
| Barb-city initial garrison | `getBarbarianInitialDefenders` | `CvPlayer.cpp:~6332`; `CvCityAI.cpp:~7054` |
| Animal attack probability | `getAnimalAttackProb` | `CvUnitAI.cpp:~1658, ~1669` |
| Animal combat (vs human / vs AI) | `getAnimalCombatModifier` / `getAIAnimalCombatModifier` | `CvUnit.cpp:~11600/11609, ~11622/11631` |
| Barbarian combat (vs human / vs AI) | `getBarbarianCombatModifier` / `getAIBarbarianCombatModifier` | `CvUnit.cpp:~11645/11654, ~11669/11678` |
| Free wins vs barbarians | `getFreeWinsVsBarbs` | `CvCombatModel.cpp:~46, ~52` |
| Goody-hut reward set | `getGoodies(i)` | `CIV4HandicapInfo.xml` Goodies list, filtered by game handicap |
| AI subdue-animal bonus | `getSubdueAnimalBonusAI` | `CvOutcome.cpp:~351` |
| Final-score multiplier | handicap index | `CvPlayer.cpp:~27840` |
| Event minimum-difficulty gate | handicap index | `CvPlayer.cpp:~22910` |

Note `getFreeWinsVsBarbs` is read through the *player's own* handicap in `CvCombatModel.cpp`
(`GET_PLAYER(...).getHandicapType()`), so it is per-player despite sitting in the barbarian family.

---

## Setting an AI to non-Noble

**Confirmed.** Setting an AI player's per-player handicap to something other than `NOBLE` (via
scenario/WorldBuilder/save, or flexible difficulty) changes **only the fields that read that player's own
per-player handicap**. It does **not** change the AI's research/production/cost advantages or any
game-global effect, because those read the *game* handicap (the average of human handicaps), not the AI's
per-player handicap.

**Fields that DO change** (read the player's own per-player handicap):

| Field | Read site |
| --- | --- |
| Happy bonus | `CvPlayer.cpp:~3697` (deposited into cascade bundle; consumed at `CvCity.cpp:~5637, ~5731`) |
| Health bonus | `CvCity.cpp:~5858, ~5885` |
| Unit upkeep % (base) | `CvPlayer.cpp:~10366` |
| Distance maintenance % | `CvCity.cpp:~7692` |
| NumCities maintenance % | `CvCity.cpp:~7752` |
| Colony maintenance % / max | `CvCity.cpp:~7795, ~7802` |
| Corporation maintenance % | `CvCity.cpp:~7864` |
| Civic upkeep % (base) | `CvPlayer.cpp:~14256` |
| Inflation % (base) | `CvPlayer.cpp:~7987` |
| Starting gold | `CvPlayer.cpp:~1865` |
| Advanced-start points mod | `CvPlayer.cpp:~1884` |
| AI advanced-start % (the exception) | `CvPlayer.cpp:~1889` |
| Revolution index % | defined (`CvHandicapInfo.cpp:~429`) but **not applied** in DLL logic — see [Gotchas](#gotchas) |

**Fields that do NOT change** — every `getAI*` advantage (train/construct/create/research/growth/upkeep/
supply/upgrade/inflation/war-weariness/work-rate, per-era modifier, declare-war prob, animal/barbarian
combat modifiers, AI starting units) reads the **game** handicap behind an `isNormalAI()` / `!isHuman()`
guard, so they track the *human's* difficulty, not the AI's. Full read sites in the
[AI-advantage table](#ai-advantage-read-sites-read-site--handicap-source) above.

**Why.** Human-facing fields and base starting values read each player's own `getHandicapType()` from
`CvInitCore.m_aeHandicap[playerID]`. Every `getAI*` advantage reads `GC.getGame().getHandicapType()`, which
`averageHandicaps()` derives from alive *humans* only. All players initialize to `NOBLE`
(`CvInitCore.cpp:747`), so unless you also raise the human's difficulty, the AI cost/research/production
multipliers stay at the NOBLE (100 %) baseline no matter what the AI's per-player handicap is. The single
per-AI-handicap exception is `getAIAdvancedStartPercent` (`CvPlayer.cpp:~1889`). AI starting-unit counts
use the game handicap, so all AIs spawn identical starting stacks regardless of their individual handicap.

**Practical upshot:** to make the AI economically/research-stronger, change the **game** handicap (raise the
human's difficulty, or set the game handicap directly), not an individual AI's per-player handicap. The
per-AI handicap only moves that AI's own upkeep/maintenance/health/happiness/starting-gold numbers.

---

## Cascade (#423) tenancy verdict

What each field *sets*, and whether it could become a tenant of the `CvCascadingModifierBundle` effect
cascade (#423 — the scoped Game→Team→Player→City modifier system; **not** the `BONUS` map resource). Fit
verdicts:

- **FLAT** — additive per-entity stat; drops into the cascade's FLAT changeset (like the happy bonus already did).
- **MODIFIER** — percentage scale on an ongoing cost/rate; drops into the cascade's MODIFIER changeset.
- **MODIFIER\*** — percentage-shaped, but AI-only **and** read from the *game* handicap, so it needs the
  cascade to model "deposit at Game scope, apply to AI players only" — applicability the bundle does not have yet.
- **No** — does not fit: one-time setup value, a game-global rule/probability, behavioural/diplomatic
  tuning, a cap, or incomplete code. These stay their own mechanism.

| Field | What setting it does | Read through | #423 fit |
| --- | --- | --- | --- |
| `getHappyBonus` | flat happiness in every city of the owner | own | **FLAT** ✅ *(already migrated)* |
| `getHealthBonus` | flat health in every city of the owner | own | **FLAT** |
| `getUnitUpkeepPercent` | % scale on unit gold upkeep | own | **MODIFIER** |
| `getDistanceMaintenancePercent` | % scale on distance maintenance | own | **MODIFIER** |
| `getNumCitiesMaintenancePercent` | % scale on number-of-cities maintenance | own | **MODIFIER** |
| `getColonyMaintenancePercent` | % scale on colony maintenance | own | **MODIFIER** |
| `getCorporationMaintenancePercent` | % scale on corporation maintenance | own | **MODIFIER** |
| `getCivicUpkeepPercent` | % scale on civic upkeep | own | **MODIFIER** |
| `getInflationPercent` | % scale on the inflation rate (inflation is its own kludge — see gotchas) | own | **MODIFIER** |
| `getMaxColonyMaintenance` | hard cap on colony maintenance | own | **No** — a cap, not a flat/percent channel |
| `getStartingGold` | gold the player starts with | own | **No** — one-time setup |
| `getAdvancedStartPointsMod` | % on the advanced-start point budget | own | **No** — one-time setup |
| `getStartingDefenseUnits` / `WorkerUnits` / `ExploreUnits` | free units at game start (humans) | own | **No** — one-time setup |
| `getRevolutionIndexPercent` | not applied **yet** — Revolution mechanic is WIP (tracked) | own | **No (for now)** — incomplete, not dead |
| `getAttitudeChange` | flat shift to AI diplomatic attitude | target's | **No** — behavioural/diplo |
| `getNoTechTradeModifier` / `getTechTradeKnownModifier` | tech-trade availability thresholds | team | **No** — diplo rule |
| `getAITrainPercent` / `WorldTrainPercent` | AI unit-build cost % | **game** | **MODIFIER\*** |
| `getAIConstructPercent` / `WorldConstructPercent` | AI building-build cost % | **game** | **MODIFIER\*** |
| `getAICreatePercent` / `WorldCreatePercent` | AI wonder/project-build cost % | **game** | **MODIFIER\*** |
| `getAIResearchPercent` | AI tech cost % | **game** | **MODIFIER\*** |
| `getAIGrowthPercent` | AI city food-to-grow % | **game** | **MODIFIER\*** |
| `getAIUnitUpkeepPercent` | AI unit-upkeep % (on top of the base) | **game** | **MODIFIER\*** |
| `getAICivicUpkeepPercent` | AI civic-upkeep % (on top of the base) | **game** | **MODIFIER\*** |
| `getAIUnitSupplyPercent` | AI unit-supply cost % | **game** | **MODIFIER\*** |
| `getAIUnitUpgradePercent` | AI unit-upgrade gold cost % | **game** | **MODIFIER\*** |
| `getAIInflationPercent` | AI inflation % (on top of the base) | **game** | **MODIFIER\*** |
| `getAIWarWearinessPercent` | AI war-weariness accumulation % | **game** | **MODIFIER\*** |
| `getAIWorkRateModifier` | AI worker build-rate % | **game** | **MODIFIER\*** |
| `getAIPerEraModifier` | per-era multiplier applied to the whole `getAI*` family | **game** | **No** — meta-modifier-on-modifiers (era ramp) |
| `getAIAdvancedStartPercent` | AI advanced-start budget % *(the lone per-AI-handicap exception)* | own | **No** — one-time setup |
| `getAIStartingDefenseUnits` / `WorkerUnits` / `ExploreUnits` | free AI starting units | **game** | **No** — one-time setup |
| `getAIDeclareWarProb` | AI declare-war probability | **game** | **No** — behavioural |
| barbarian/animal combat odds & free wins (`getAnimalAttackProb`, `getAnimalCombatModifier`, `getBarbarianCombatModifier`, the `getAI*` siblings, `getFreeWinsVsBarbs`, `getSubdueAnimalBonusAI`) | wildlife/barbarian combat odds | game / own (mixed) | **No** — combat-rule probabilities |
| barbarian spawn density/timing/garrison (`getUnownedWaterTilesPerBarbarianUnit`, `getUnownedTilesPerBarbarianCity`, `getBarbarianCityCreationTurnsElapsed`, `getBarbarianCityCreationProb`, `getBarbarianInitialDefenders`) | barbarian spawn rules | **game** | **No** — game-global spawn rules |
| `getGoodies(i)` / `getPropertyManipulators` | goody-hut reward set / per-handicap property sources | game | **No** — list / property mechanism |
| (handicap **index**) | final-score multiplier (`CvPlayer.cpp:~27840`); event min-difficulty gate (`~22910`) | game | **No** — meta / uses the raw enum index |

**Can "all of it" move into #423?** No — and that is correct per the cascade's boundary. **Migrates
cleanly:** the 2 FLAT wellbeing bonuses + the ~7 per-player MODIFIER cost percents (the ongoing per-entity
scalars). **The `getAI*` percents are MODIFIER-shaped** and *could* migrate, but only once the cascade
learns Game-scope deposits with **AI-only applicability** (and they read the *game* handicap, so it is a
game-scope AI-only channel, subtler than a per-player deposit). **Everything else stays out:** one-time
setup, game-global rules, and behavioural/diplo tuning are a different shape. The handicap is a *partial*
tenant — its economic core fits, its world-rules and setup halves do not. The "build the proper structure
once" rationale behind not bending the cascade to absorb the misfits is [DEC-proper-once](../../architecture/decisions.md#dec-proper-once).

---

## Gotchas

- **Game-vs-player asymmetry is the whole point.** The same `CvHandicapInfo` row is read two ways depending
  on the field family; reading the wrong `getHandicapType()` silently uses the wrong difficulty.
  Human-facing → owner's per-player; AI-advantage → game.
- **Game handicap is derived, not chosen, and not saved.** Integer average of alive *human* handicaps,
  recomputed on init/load/change. Two humans at EMPEROR+NOBLE yield PRINCE — the UI can display one
  difficulty while AIs receive another tier's bonuses.
- **Inflation example (the canonical trap).** `CvPlayer::getInflationMod10000` multiplies by the player's
  *own* `getInflationPercent` (`~7987`), then for `isNormalAI()` applies `getAIInflationPercent` from the
  *game* handicap (`~8008`). An AI's per-player inflation setting is effectively ignored; only the game
  inflation setting reaches it.
- **`CvTeam::getHandicapType()` is decorative for difficulty purposes.** No AI-advantage cost field reads
  it; `getAIResearchPercent` is read on `CvTeam` but pulls from `GC.getGame()` (`CvTeam.cpp:~2654`). The
  team average is used only by team-scoped diplomacy/trade reads and display.
- **`getRevolutionIndexPercent` is INCOMPLETE, not dead (owner, 2026-06-14).** It exists as XML data, a
  getter (`CvHandicapInfo.cpp:~429`), and a Python binding (`CyInfoInterface2.cpp:~82`), but no C++ path
  applies it **yet** — the Revolution mechanic is unfinished (tracked). A WIP gap to complete, not a field
  to purge. (Earlier this note read "dead"; corrected.)
- **Increasing vs flexible difficulty differ on AIs.** `doIncreasingDifficulty` raises *humans only*.
  `doFlexibleDifficulty` can raise/lower AIs but only if `MODDERGAMEOPTION_AI_USE_FLEXIBLE_DIFFICULTY` is set.
- **Flexible difficulty's AI change is half-effective.** Even when it bumps an AI to a new per-player
  handicap, only that AI's *own-handicap* fields move (maintenance/upkeep/health/happiness/gold —
  `setHandicap` flags maintenance and unit-upkeep dirty). Its research/production/growth still come from the
  *game* handicap, producing a fractured AI where some advantages scale with its new difficulty and most do not.
- **Score uses the handicap as a raw integer index** (`CvPlayer.cpp:~27840`):
  `score * (100 + OFFSET + handicapType * PER) / 100`. Reordering handicaps in XML silently shifts score
  multipliers — the index, not a named field, is the multiplier.
- **Barbarian-spawn probability is inverted.** `CvGame.cpp:~7012` is a `>=` check (`rand >= 10 * prob` →
  *no* spawn), so a *lower* `getBarbarianCityCreationProb` means a *higher* spawn chance.
- **`getSubdueAnimalBonusAI` is excluded from the savegame checksum** (comment near `CvHandicapInfo.cpp:~41`),
  unlike the rest of the fields.
- **Starting units split human/AI fields entirely.** Humans read `getStartingDefenseUnits/WorkerUnits/
  ExploreUnits` from their own handicap; AIs read the `getAIStarting…` siblings from the game handicap. The
  AI-only starting-unit fields exist only in the EMPEROR+ XML rows, so at lower game handicaps AIs get no
  extra starting units.

---

## See also

- [Save / load format](save-load-format.md) — why the game handicap need not be saved: it is derived
  (`reset(NO_HANDICAP)` + `averageHandicaps()` on load), an instance of the name-keyed soft-removal world
  this doc lives in.
- [Fixed-point and scales](../cascade/fixed-point-and-scales.md) — the cascade's ×100 fixed-point scale
  registry; relevant because `CvHandicapInfo` percent fields are **plain XML integers, NOT** cascade
  fixed-point, and the #423 tenancy verdict above turns on that distinction.
- [Decisions ledger](../../architecture/decisions.md) — [DEC-proper-once](../../architecture/decisions.md#dec-proper-once)
  is the ruling behind not bending the cascade to absorb the handicap's setup/world-rule fields.
- [Docs map](../../README.md) — the overview-of-overviews; engine-core state classes the rework touches.
- [Root `AGENTS.md`](../../../../AGENTS.md) — the trust-but-verify and read-gate conventions these citations
  are written under.
