# Handicaps / Difficulty

A *handicap* is the difficulty setting. In S2S (as in BTS/C2C) it is two distinct
things that share a name and are easy to conflate:

- A **per-player handicap** — every player slot has its own `HandicapTypes` value
  stored in `CvInitCore::m_aeHandicap[playerID]`. This is the "difficulty" assigned to
  that individual player (`CvPlayer::getHandicapType()`, `CvPlayer.cpp:12398-12401`).
- A **single game handicap** — `CvGame::m_eHandicap`, one value for the whole game
  (`CvGame::getHandicapType()`, `CvGame.cpp:4702-4706`). It is **not saved**; it is
  recomputed as the **average of the alive *human* players' per-player handicaps** by
  `CvGame::averageHandicaps()` (`CvGame.cpp:10420-10439`).

The crux of the whole system: **human-facing fields read each player's *own*
per-player handicap, while the AI-advantage fields read the *game* handicap.** Because
all AIs default to `HANDICAP_NOBLE` and the game handicap tracks the *human's* chosen
difficulty, the AIs' production/research/cost advantages scale with the human's
difficulty, not with any per-AI difficulty. See
[What happens if you set an AI to non-Noble](#what-happens-if-you-set-an-ai-to-non-noble).

`CvHandicapInfo` (info class `Sources/Infos/CvHandicapInfo.{h,cpp}`, data in
`Assets/XML/GameInfo/CIV4HandicapInfo.xml`) holds ~50 integer fields plus a goody list
and `PropertyManipulators`. The handicap levels, easiest → hardest, are
`HANDICAP_SETTLER, CHIEFTAIN, WARLORD, NOBLE, PRINCE, MONARCH, EMPEROR, IMMORTAL,
DEITY, NIGHTMARE, NIGHTMARE_PLUS, AI_BOOSTED`. `NOBLE` is the 100 % baseline.

---

## The four `getHandicapType()` accessors

| Accessor | Returns | Source | When it is the right one |
| --- | --- | --- | --- |
| `CvGame::getHandicapType()` | `m_eHandicap`, the single game difficulty (average of alive humans) | `CvGame.cpp:4702-4706` | Anything *global* (barbarians, animals, goodies) and **every AI-advantage field** (`getAI*`). |
| `CvPlayer::getHandicapType()` | that player's own per-player handicap | `CvPlayer.cpp:12398-12401` | Human-facing per-player economics: happiness/health, maintenance %, upkeep %, starting gold, inflation base. Applies to whoever owns the handicap, human or AI alike. |
| `CvCity::getHandicapType()` | owner player's handicap (`GET_PLAYER(getOwner()).getHandicapType()`) | `CvCity.cpp:5201-5204` | City-level reads of the owner's per-player handicap (maintenance/health). Pure delegation. |
| `CvUnit::getHandicapType()` | owner player's handicap | `CvUnit.cpp:10677-10680` | Unit-level reads of the owner's per-player handicap. Pure delegation. |
| `CvTeam::getHandicapType()` | **average** of the per-player handicaps of alive team members | `CvTeam.cpp:2974-2995` | Diplomacy/trade reads that are team-scoped (tech-trade thresholds). **Note:** despite the name, this is *not* used by any AI-advantage cost field, and it is a different average than `CvGame`'s (team members vs. alive humans). |

`FAssert(m_eHandicap != NO_HANDICAP)` in `CvGame::getHandicapType()` (`CvGame.cpp:4704`)
means reading the game handicap before `averageHandicaps()` has run will trip an assert
in Assert/Debug builds.

---

## How handicaps are set

- **Default at reset:** every slot is initialized to `STANDARD_HANDICAP`
  (`CvInitCore.cpp:747`), which `GlobalDefines.xml:163-164` defines as
  `HANDICAP_NOBLE`. Barbarian/NPC slots are overwritten with `BARBARIAN_HANDICAP`
  during `CvGame` setup (`CvGame.cpp:352,363,374,…465`).
- **Do all AIs share NOBLE by default? Yes.** On a fresh start with no scenario
  override, every AI keeps its reset value `HANDICAP_NOBLE`. The human's chosen
  difficulty is written to the human slot, and the *game* handicap becomes the average
  of the alive humans (`CvGame::averageHandicaps()`, `CvGame.cpp:10420-10439`). With one
  human at `EMPEROR`, the game handicap is `EMPEROR` while every AI's per-player
  handicap stays `NOBLE`. (This matches the live `/players` HTTP snapshot showing all
  AIs = `HANDICAP_NOBLE`, human = `HANDICAP_EMPEROR`.)
- **`averageHandicaps()` runs** at game init and whenever a human's handicap changes;
  it sets the game handicap to `aggregate / humanCount` over alive humans, or
  `STANDARD_HANDICAP` if no humans are alive (`CvGame.cpp:10434-10438`). With two humans
  at `EMPEROR` (6) and `NOBLE` (3) the game handicap is integer-averaged to `PRINCE`
  (4) — the UI can say "EMPEROR" while AIs get the lower-tier bonuses.
- **Saving:** per-player handicaps in `CvInitCore` are saved; the game handicap is
  reconstructed on load (`reset(NO_HANDICAP)` then `averageHandicaps()`).
- **`CvPlayer::setHandicap(iNewVal, bAdjustGameHandicap=true)`** writes the per-player
  value to InitCore, optionally re-averages the game handicap, and marks maintenance and
  unit-upkeep dirty so the per-player economic fields recompute
  (`CvPlayer.cpp:27639-27654`). `CvInitCore::copyNonDefaults` restores per-player
  handicaps from scenario/save files, so a manually-set AI handicap persists.
- **Increasing difficulty** (`GAMEOPTION_CHALLENGE_INCREASING_DIFFICULTY`,
  `CvGame::doIncreasingDifficulty`, `CvGame.cpp:10057-10098`) raises **only human**
  players' handicaps (`isHumanPlayer(true)` guard at `:10081`), then re-averages. AIs are
  never touched here.
- **Flexible difficulty** (`CvGame::doFlexibleDifficulty`, ~`CvGame.cpp:10139-10416`) can
  raise/lower **both** human and AI handicaps based on score deviation; AI changes are
  gated by the modder option `MODDERGAMEOPTION_AI_USE_FLEXIBLE_DIFFICULTY`
  (`CvGame.cpp:10144`) and bounded by per-game AI min/max. It calls
  `playerX.setHandicap(iNewHandicap)` for each adjusted player (`CvGame.cpp:10410`). This
  is the only in-game path that gives an AI a non-NOBLE *per-player* handicap — and even
  then, see the caveat in [Gotchas](#gotchas--non-obvious-behavior).

---

## Field categories

`CvHandicapInfo` fields fall into two families. The **human-facing** family is read
through the *owner's* per-player handicap (so it applies to whoever owns it, human or
AI). The **AI-only `getAI*`** family is gated behind an `isNormalAI()` / `!isHuman()`
check and (with one exception) read through the **game** handicap.

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

## Where AI-advantage fields are applied (read site → handicap source)

Almost every `getAI*` field is read through `GC.getGame().getHandicapType()` (the game
handicap), behind an `isNormalAI()` / `!isHumanPlayer()` / `!isHuman()` guard. The
`getAIPerEraModifier()` term (`× getCurrentEra()`) is applied alongside most of them.

| AI field | Primary read site | Guard | Handicap source |
| --- | --- | --- | --- |
| `getAITrainPercent` / `getAIWorldTrainPercent` | `CvPlayer.cpp:7004-7008` | `isNormalAI()` | **game** |
| `getAIConstructPercent` / `getAIWorldConstructPercent` | `CvPlayer.cpp:7131-7137` | `isNormalAI()` | **game** |
| `getAICreatePercent` / `getAIWorldCreatePercent` | `CvPlayer.cpp:7194-7202` | `isNormalAI()` | **game** |
| `getAIUnitSupplyPercent` | `CvPlayer.cpp:7952` | `isNormalAI()` | **game** |
| `getAIInflationPercent` | `CvPlayer.cpp:8008-8013` | `isNormalAI()` | **game** |
| `getAIWorkRateModifier` | `CvPlayer.cpp:9878-9880`; also `CvUnit.cpp:27934-27936` | `isNormalAI()` | **game** |
| `getAIUnitUpkeepPercent` | `CvPlayer.cpp:10371-10374` | `!isHumanPlayer()` | **game** |
| `getAIWarWearinessPercent` | `CvPlayer.cpp:10973-10975` | `isNormalAI()` | **game** |
| `getAICivicUpkeepPercent` | `CvPlayer.cpp:14262-14264` | `isNormalAI()` | **game** |
| `getAIGrowthPercent` | `CvPlayer.cpp:24467-24469` | `isNormalAI()` | **game** |
| `getAIResearchPercent` | `CvTeam.cpp:2654-2656` | `!isNPC() && !isHuman(true)` | **game** (read on `CvTeam`, but from `GC.getGame()`) |
| `getAIUnitUpgradePercent` | `CvUnit.cpp:10360-10362` | `!isHuman()` | **game** |
| `getAIAnimalCombatModifier` | `CvUnit.cpp:11609,11631` | defender/attacker not human | **game** |
| `getAIBarbarianCombatModifier` | `CvUnit.cpp:11654,11678` | not human | **game** |
| `getAIDeclareWarProb` | `CvTeamAI.cpp:4205,4380`; `CvDLLWidgetData.cpp:4093` | — | **game** |
| `getAIStartingDefenseUnits` | `CvPlayer.cpp:1928` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIStartingWorkerUnits` | `CvPlayer.cpp:1937` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIStartingExploreUnits` | `CvPlayer.cpp:1945` (AI branch) | `!isHumanPlayer()` | **game** |
| `getAIPerEraModifier` | applied with all above (e.g. `7008, 7131, 7194, 7954, 8013, 9880, 10374, 10975, 14264, 24469`; `CvUnit.cpp:10362, 27936`; `CvTeam.cpp:2656`) | inherited | **game** |
| **`getAIAdvancedStartPercent`** | `CvPlayer.cpp:1889` | `!isHumanPlayer()` | **own per-player handicap** ⟵ exception |

**The dual-application pattern.** Several cost fields apply *both* a per-player
human-facing field (own handicap) *and* an AI-only modifier (game handicap), in
sequence:

- Unit upkeep — `getUnitUpkeepPercent()` (own, `CvPlayer.cpp:10366`), then for AIs
  `getAIUnitUpkeepPercent()` and `getAIPerEraModifier()` (game,
  `CvPlayer.cpp:10371-10374`).
- Civic upkeep — `getCivicUpkeepPercent()` (own, `CvPlayer.cpp:14256`), then for AIs
  `getAICivicUpkeepPercent()` (game, `CvPlayer.cpp:14262`).
- Inflation — `getInflationPercent()` (own, `CvPlayer.cpp:7987`), then for AIs
  `getAIInflationPercent()` (game, `CvPlayer.cpp:8008-8013`).

So an AI's unit-upkeep number is the *NOBLE* per-player `getUnitUpkeepPercent` (100 %)
multiplied by the *game* handicap's `getAIUnitUpkeepPercent`.

The military-analysis read at `CvUnitAI.cpp:3413` is a special case: it uses
`GET_PLAYER(pTargetCity->getOwner()).getHandicapType()` — the *defender's* own
per-player handicap, not the game handicap — to scale `getAITrainPercent` when sizing up
a target city. (This is an attacker estimating a defender's production rate; treat as
intentional but unusual.)

---

## Game-global difficulty effects (keyed on the game handicap)

These read `GC.getGame().getHandicapType()` (or `getHandicapType()` on the `CvGame`
object) and affect the whole game regardless of any individual player's handicap:

| Effect | Field(s) | Read site |
| --- | --- | --- |
| Barb-city spawn delay | `getBarbarianCityCreationTurnsElapsed` | `CvGame.cpp:6973` |
| Barb-city spawn probability / density | `getBarbarianCityCreationProb` | `CvGame.cpp:7012, 7019` |
| Barb-city placement tile threshold | `getUnownedTilesPerBarbarianCity` | `CvGame.cpp:7058` |
| Water-barb spawn density (halved by `GAMEOPTION_BARBARIAN_RAGING`) | `getUnownedWaterTilesPerBarbarianUnit` | `CvGame.cpp:7168` |
| Barb-city initial garrison | `getBarbarianInitialDefenders` | `CvPlayer.cpp:6332`; `CvCityAI.cpp:7054` |
| Animal attack probability | `getAnimalAttackProb` | `CvUnitAI.cpp:1658, 1669` |
| Animal combat (vs human / vs AI) | `getAnimalCombatModifier` / `getAIAnimalCombatModifier` | `CvUnit.cpp:11600/11609, 11622/11631` |
| Barbarian combat (vs human / vs AI) | `getBarbarianCombatModifier` / `getAIBarbarianCombatModifier` | `CvUnit.cpp:11645/11654, 11669/11678` |
| Free wins vs barbarians | `getFreeWinsVsBarbs` | `CvCombatModel.cpp:46, 52` |
| Goody-hut reward set | `getGoodies(i)` | `CIV4HandicapInfo.xml` Goodies list, filtered by game handicap |
| AI subdue-animal bonus | `getSubdueAnimalBonusAI` | `CvOutcome.cpp:351` |
| Final-score multiplier | handicap index | `CvPlayer.cpp:27840` |
| Event minimum-difficulty gate | handicap index | `CvPlayer.cpp:22910` |

Note `getFreeWinsVsBarbs` is read through the *player's own* handicap in
`CvCombatModel.cpp` (`GET_PLAYER(...).getHandicapType()`), so it is per-player despite
sitting in the barbarian family.

---

## What happens if you set an AI to non-Noble

Verdict: **confirmed.** Setting an AI player's per-player handicap to something other
than `NOBLE` (via scenario/WorldBuilder/save, or flexible difficulty) changes **only the
fields that read that player's own per-player handicap**. It does **not** change the AI's
research/production/cost advantages or any game-global effect, because those read the
*game* handicap (the average of human handicaps), not the AI's per-player handicap.

**Fields that DO change** (read the player's own per-player handicap):

| Field | Read site |
| --- | --- |
| Happy bonus | `CvPlayer.cpp:3697` (deposited into cascade bundle; consumed at `CvCity.cpp:5637, 5731`) |
| Health bonus | `CvCity.cpp:5858, 5885` |
| Unit upkeep % (base) | `CvPlayer.cpp:10366` |
| Distance maintenance % | `CvCity.cpp:7692` |
| NumCities maintenance % | `CvCity.cpp:7752` |
| Colony maintenance % / max | `CvCity.cpp:7795, 7802` |
| Corporation maintenance % | `CvCity.cpp:7864-7871` |
| Civic upkeep % (base) | `CvPlayer.cpp:14256` |
| Inflation % (base) | `CvPlayer.cpp:7987` |
| Starting gold | `CvPlayer.cpp:1865` |
| Advanced-start points mod | `CvPlayer.cpp:1884` |
| AI advanced-start % (the exception) | `CvPlayer.cpp:1889` |
| Revolution index % | defined (`CvHandicapInfo.cpp:429`) but **not applied** in DLL logic — see gotchas |

**Fields that do NOT change** (read the game handicap, behind an `isNormalAI()` /
`!isHuman()` guard — so they track the *human's* difficulty, not the AI's):

`getAITrainPercent` / `getAIWorldTrainPercent` (`CvPlayer.cpp:7004-7006`),
`getAIConstructPercent` / `getAIWorldConstructPercent` (`:7135-7137`),
`getAICreatePercent` / `getAIWorldCreatePercent` (`:7198-7202`),
`getAIResearchPercent` (`CvTeam.cpp:2654`),
`getAIGrowthPercent` (`CvPlayer.cpp:24467`),
`getAIUnitUpkeepPercent` (`:10371`),
`getAICivicUpkeepPercent` (`:14262`),
`getAIInflationPercent` (`:8008-8011`),
`getAIUnitSupplyPercent` (`:7952`),
`getAIUnitUpgradePercent` (`CvUnit.cpp:10360`),
`getAIWarWearinessPercent` (`CvPlayer.cpp:10973`),
`getAIWorkRateModifier` (`:9878-9880`),
`getAIPerEraModifier` (applied with all the above),
`getAIDeclareWarProb` (`CvTeamAI.cpp:4205, 4380`),
`getAIAnimalCombatModifier` / `getAIBarbarianCombatModifier` (`CvUnit.cpp:11609, 11654`),
and `getAIStartingDefenseUnits` / `WorkerUnits` / `ExploreUnits`
(`CvPlayer.cpp:1928, 1937, 1945`).

**Why.** Human-facing fields and base starting values read each player's own
`getHandicapType()` from `CvInitCore.m_aeHandicap[playerID]`. Every `getAI*` advantage
field reads `GC.getGame().getHandicapType()`, which `averageHandicaps()` derives from
alive *humans* only (`CvGame.cpp:10428`). All players initialize to `NOBLE`
(`CvInitCore.cpp:747` + `GlobalDefines.xml:164`), so unless you also raise the human's
difficulty, the AI cost/research/production multipliers stay at the NOBLE (100 %)
baseline no matter what the AI's per-player handicap is. The single per-AI-handicap
exception is `getAIAdvancedStartPercent` (`CvPlayer.cpp:1889`), which reads the AI's own
handicap. Starting-unit counts for AIs use the game handicap (`:1928, 1937, 1945`), so
all AIs spawn identical starting stacks regardless of their individual handicap.

**Practical upshot:** to actually make the AI economically/research-stronger, change the
**game** handicap (raise the human's difficulty, or set the game handicap directly), not
an individual AI's per-player handicap. The per-AI handicap only moves that AI's own
upkeep/maintenance/health/happiness/starting-gold numbers.

---

## Field reference — what each value does, and can it move into the #423 cascade?

What *setting* each field does, and whether it could become a tenant of the
`CvCascadingModifierBundle` effect cascade (#423 — the scoped Game→Team→Player→City modifier
system; **not** the `BONUS` map resource). Fit verdicts:

- **FLAT** — additive per-entity stat; drops into the cascade's FLAT changeset (like the happy bonus already did).
- **MODIFIER** — percentage scale on an ongoing cost/rate; drops into the cascade's MODIFIER changeset.
- **MODIFIER\*** — percentage-shaped, but AI-only **and** read from the *game* handicap, so it would need the
  cascade to model "deposit at Game scope, apply to AI players only" — applicability the bundle does not have yet.
- **No** — does not fit: one-time setup value, a game-global rule/probability, behavioural/diplomatic tuning, a
  cap, or dead code. These stay their own mechanism (consistent with the cascade's "uniform ongoing per-entity
  effects only" boundary).

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
| `getAIPerEraModifier` | per-era multiplier applied to the whole `getAI*` family | **game** | **No** — a meta-modifier-on-modifiers (era ramp) |
| `getAIAdvancedStartPercent` | AI advanced-start budget % *(the lone per-AI-handicap exception)* | own | **No** — one-time setup |
| `getAIStartingDefenseUnits` / `WorkerUnits` / `ExploreUnits` | free AI starting units | **game** | **No** — one-time setup |
| `getAIDeclareWarProb` | AI declare-war probability | **game** | **No** — behavioural |
| `getAnimalAttackProb`, `getAnimalCombatModifier`, `getBarbarianCombatModifier`, `getAIAnimalCombatModifier`, `getAIBarbarianCombatModifier`, `getFreeWinsVsBarbs`, `getSubdueAnimalBonusAI` | wildlife/barbarian combat odds & free wins | game / own (mixed) | **No** — combat-rule probabilities |
| `getUnownedWaterTilesPerBarbarianUnit`, `getUnownedTilesPerBarbarianCity`, `getBarbarianCityCreationTurnsElapsed`, `getBarbarianCityCreationProb`, `getBarbarianInitialDefenders` | barbarian spawn density / timing / garrison | **game** | **No** — game-global spawn rules |
| `getGoodies(i)` / `getPropertyManipulators` | goody-hut reward set / per-handicap property sources | game | **No** — list / property mechanism |
| (handicap **index**) | final-score multiplier (`CvPlayer.cpp:27840`); event min-difficulty gate (`:22910`) | game | **No** — meta / uses the raw enum index |

**Migration verdict — can "all of it" move into #423?** No, and that's the right answer per the cascade's
boundary. **What migrates cleanly:** the 2 FLAT wellbeing bonuses + the ~7 per-player MODIFIER cost percents
(upkeep/maintenance/inflation) — the ongoing per-entity scalars. **The `getAI*` percents are MODIFIER-shaped**
and *could* migrate, but only once the cascade learns Game-scope deposits with **AI-only applicability** (and
note they read the *game* handicap, so the "one uniform AI/human lever" the #423 doc imagines is more subtle
here than a simple per-player deposit — it's a game-scope, AI-only channel). **Everything else stays out:**
one-time setup (starting gold/units, advanced-start), game-global rules (barbarians/animals/goodies), and
behavioural/diplo tuning (attitude, declare-war, tech-trade) are a different shape and don't belong in a
per-entity flat/percent cascade. So the handicap is a *partial* tenant — its economic core fits, its world-rules
and setup halves do not.

## Gotchas / non-obvious behavior

- **Game-vs-player asymmetry is the whole point.** The same `CvHandicapInfo` row is read
  two different ways depending on the field family; reading the wrong `getHandicapType()`
  silently uses the wrong difficulty. Human-facing → owner's per-player; AI-advantage →
  game.
- **Game handicap is derived, not chosen, and not saved.** It is the integer average of
  alive *human* handicaps, recomputed on init/load/change. Two humans at EMPEROR+NOBLE
  yield PRINCE — the UI can display one difficulty while AIs receive another tier's
  bonuses.
- **Inflation example (the canonical trap).** `CvPlayer::getInflationMod10000`
  (`CvPlayer.cpp:7983-8024`) multiplies by the player's *own* `getInflationPercent`
  (`:7987`), then for `isNormalAI()` applies `getAIInflationPercent` from the *game*
  handicap (`:8008-8011`). An AI's per-player inflation setting is effectively ignored;
  only the game inflation setting reaches it.
- **`CvTeam::getHandicapType()` is decorative for difficulty purposes.** No AI-advantage
  cost field reads it; `getAIResearchPercent` is read on `CvTeam` but pulls from
  `GC.getGame()` (`CvTeam.cpp:2654`). The team average is used only by team-scoped
  diplomacy/trade reads and display.
- **`getRevolutionIndexPercent` is INCOMPLETE, not dead (owner, 2026-06-14).** It exists as XML data, a getter
  (`CvHandicapInfo.cpp:429`), and a Python binding (`CyInfoInterface2.cpp:82`), but no C++ path applies it
  **yet** — the Revolution mechanic is unfinished (there is a tracking issue to wire it up), so this is a
  WIP gap to complete, not a field to purge. (Earlier this note read "dead"; corrected.)
- **Increasing vs flexible difficulty differ on AIs.** `doIncreasingDifficulty` raises
  *humans only* (`CvGame.cpp:10081`). `doFlexibleDifficulty` can raise/lower AIs but only
  if `MODDERGAMEOPTION_AI_USE_FLEXIBLE_DIFFICULTY` is set (`CvGame.cpp:10144`).
- **Flexible difficulty's AI change is half-effective.** Even when flexible difficulty
  bumps an AI to a new per-player handicap, only that AI's *own-handicap* fields move
  (maintenance/upkeep/health/happiness/gold — note `setHandicap` flags maintenance and
  unit-upkeep dirty, `CvPlayer.cpp:27651-27652`). Its research/production/growth still
  come from the *game* handicap, producing a fractured AI where some advantages scale
  with its new difficulty and most do not.
- **Score uses the handicap as a raw integer index** (`CvPlayer.cpp:27840`):
  `score * (100 + OFFSET + handicapType * PER) / 100`. Reordering handicaps in XML
  silently shifts score multipliers — the index, not a named field, is the multiplier.
- **Barbarian-spawn probability is inverted.** `CvGame.cpp:7012` is a `>=` check
  (`rand >= 10 * prob` → *no* spawn), so a *lower* `getBarbarianCityCreationProb` means a
  *higher* spawn chance.
- **`getSubdueAnimalBonusAI` is excluded from the savegame checksum** (comment near
  `CvHandicapInfo.cpp:41`), unlike the rest of the fields.
- **Starting units split human/AI fields entirely.** Humans read
  `getStartingDefenseUnits/WorkerUnits/ExploreUnits` from their own handicap; AIs read
  the `getAIStarting…` siblings from the game handicap (`CvPlayer.cpp:1928, 1937, 1945`).
  The AI-only starting-unit fields only exist in the EMPEROR+ XML rows, so at lower game
  handicaps AIs get no extra starting units.
