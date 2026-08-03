# Stub census — Sources/Infos constant-return getters (the hidden-debt inventory)

> **Work-state snapshot** (plans-class doc): every getter in the JSON pocos whose body returns a constant where
> legacy loaded/computed a real value, classified with named consumers. Produced by an exhaustive sweep of all
> 90 files + an adversarial second pass; excludes the 13 getters already fixed (combatLimit family, routeYields,
> maxLatitude/placementOrder/aiObjective, conquestProbability, popDestroys, trainReluctance, layerAnimationPath,
> skills hot-cache). Keep CURRENT: delete rows as they are fixed.

## 1. BUG-CONSUMED (a live consumer reads wrong values) — priority order

| # | getter | location | returns | data lost | live consumers |
|---|---|---|---|---|---|
| 1 | `getFreeBuilding` / `getFreeAreaBuilding` | CvBuildingInfo.h:287-288 | -1 | **404 + 3 authored** | CvPlayer.cpp:7391/7397 (processBuilding grant), CvCityAI.cpp:4685/4764, CvGameTextMgr.cpp:15838 — free-building chains (zFolklore ×134, GreatWonders ×67, …) silently never grant. Curator STORE_TAGS drops building-side with no emitting path. |
| 2 | `getHappinessPercentPerPopulation` / `getHealthPercentPerPopulation` | CvBuildingInfo.h:281-282 | 0 | **51 / 93 authored** | CvCity.cpp:4941-4942 + per-turn accrual (:8582/8597/8722/9285/9432). Data IS in JSON (`happiness.city`/`health.city` perPopulation); the getter refuses pending the documented SCALE ruling (h:274-278). ⛔ OWNER RULING NEEDED. |
| 3 | `getCelebrityHappy` (Promotion) | CvPromotionInfo.h:405 | 0 | 3 authored | CvUnit.cpp:18767 → CvCity.cpp:5794-5801/5943. Amount dropped for boolean `skills.celebrity`, but CvCity was never rewired to the skill — celebrity happiness dead, the skill emitted-but-unconsumed. |
| 4 | S&D / SIZE_MATTERS / WITHOUT_WARNING **option-gate family** (stealth/unnerve/enclose/lunge/dynamicDefense/perSize/perVolume changes; promotion + unitcombat) | CvPromotionInfo.h:71-92, CvUnitCombatInfo.h:143-157 | values real — the legacy getters' GAME-OPTION gate is gone | — | CvUnit.cpp:18375-18455/18809-18935 accumulate ungated (archive getters returned 0 with the option off). lunge/dynamicDefense re-gated downstream (CvUnit.cpp:24845/24955); size/volume + stealth appear NOT — units bank bonuses with the options disabled. Needs a consumer-side gate audit. |
| 5 | `getLocalSpecialistCommerceChange` | CvBuildingInfo.h:440 | 0 | 4 authored | CvCity.cpp:5005-5007 (per-turn). |
| 6 | int*-array getters ×24 | CvBuildingInfo.h:395-418 | NULL | scalars real in JSON | CvGameTextMgr pedia only (NULL-guarded) — help lines suppressed. |
| 7 | `getNotShowInCity` | CvBuildingInfo.h:121 | false | legacy derived from art | CvCity.cpp:19153 (3D display skip) — recomputable from the mapped art tag. |
| 8 | `getCityLimit(ePlayer)` | CvCivicInfo.h:92 | 0 | 6 authored | pedia only (gameplay decoupled via CvGame::getCivicCityLimit — verified). |
| 9 | `isQualifiedPromotionType` | CvUnitInfo.h:444 | false | derived post-load in legacy | Python pedia (CyInfoInterface1.cpp:273). |
| 10 | `getSpecialistYieldChange` / `getSpecialistCommerceChange` | CvBuildingInfo.h:431-432 | 0 | 36 / 5 authored | CvCityAI.cpp:15695/15779 (AI specialist valuation). Claimed relocated to curate_specialist — NOT verified. |

## 2. UNCLEAR (verify / owner ruling)

- **`CvCivicInfo::getTechPrereq`** (NO_TECH) — store-inverted to tech.enables.civics; but CvPlayer.cpp:8493
  (canDoCivic) + CvGlobals.cpp:3791 still call the stub. If the enabler does not own the tech→civic gate
  end-to-end, civics are selectable without tech.
- **`CvCorporationInfo::getPrereqBuilding`** (-1) — 10 corps authored prereq-building COUNTS; the
  CvUnit.cpp:8609/8611 corp-spread "own N buildings" gate is a silent no-op (the boolean enables-edge does not
  reproduce the count semantic).
- **CvJsonLeaderHeadInfo `ai` group** — curated (~90 AI params) but no poco getter surface; legacy
  CvLeaderHeadInfo still serves the engine. Confirm where (if anywhere) the JSON group is read.

## 3. DELIBERATE-DROP (ruling recorded; flags where the receiving side has not landed)

- **CvOutcome system** (unit/unitcombat outcome getters NULL) — Tier-G stays-XML carve-out. ⚑ FLAG: 9
  KillOutcomes/Actions records (Subdue Animals) with LIVE consumers (CvUnit.cpp:3102/3627/9283,
  CvUnitAI.cpp:15186-15199) — subdue/capture outcomes never fire until the carve-out is served.
- **`CvImprovementInfo::getHealthPercent`** — owner BALANCE-CUT (13 authored; attributed divergence class).
- **UnitMeshGroups geometry** (groupSize=1 etc.) — cosmetic: single-figure rendering, wrong battle-anim crowds.
- Promotion PrereqPromotion chains → line+priority succession (gate NOW BUILT in CvUnit.cpp); BATTLEWORN trio;
  trap subsystem; UC bSpy → unit tag; building isNewCityFree → settler foundBuildings; getConstructCondition →
  requires.build; properties one-shots → manipulators; unit animalIgnoresBorders; RBombard family (system
  removal); era art grids; civic specialist folds (verified relocated); trait On/NotOnGameOption; culturelevel
  OCC cap; build placeBonus/mapCategories; feature warmingDefense (GLOBAL_WARMING compiled out); leaderhead
  traits drop; handicap future-rework flag.

## 4. MOOT (zero consumers) — abbreviated

Building maxPopAllowed/constructRequirements; promotion category/mapCategories; UC celebrity/revoke-quintet/
categories; promotionline contract trio; civic categories/attitudeChanges; trait isValidTrait/categories/
improvementYieldArray; unit unitGroupRequired/categories/smCargoVolume; bonus improvementChange (legacy-NULL
too)/categories/manipulators; tech freeSpecialistCount.

## 5. FAITHFUL — ~190 getters verified constant == legacy default with zero authorings (details in the class
headers); 30 of 46 classes stub-free (all game-setup CvJson* + infrastructure pairs + civicoption/specialist/
process/project/heritage).

## 6. Stale annotations (comment claims stub, code is REAL — fix-docs-now list)

CvBuildingInfo.h:329 (plotYieldChanges real) · CvImprovementInfo.h:234-235 (buildTypes + manipulators real) ·
CvTechInfo.h:111/179 (leadsTo built at CvGlobals.cpp:3367-3379) · CvSpecialistInfo.h:82-84 (bridge wired) ·
CvTraitInfo.cpp:29-32 (prereq FKs reverse-mapped; only one array view NULL) · CvFeatureInfo.h:132 + terrain/
route zobrist comments (ctor-drawn, faithful) · CvBuildingInfo.h:294 curator cite off · CvPropertyInfo.h:65
(manipulators populated).

## Counts

BUG-CONSUMED 11 rows (6 per-turn engine, 4 UI-pedia, 1 AI-rare) · UNCLEAR 3 · DELIBERATE-DROP 24 · MOOT 22 ·
FAITHFUL ~46 rows (~190 getters) · stale annotations 9 · zero-stub classes 30/46.
