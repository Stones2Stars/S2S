# Stub census — Sources/Infos constant-return getters (the hidden-debt inventory)

> **Work-state snapshot** (plans-class doc): every getter in the JSON pocos whose body returns a constant where
> legacy loaded/computed a real value, classified with named consumers. Keep CURRENT: delete rows as they are
> fixed.
>
> ⚠ **Sections 3-6 have NOT been swept against the current tree.** The Info headers were rebuilt around the
> cascade-modifier surface after this census was taken, so a row here may name a getter that no longer exists.
> Verify before relying on one.

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

BUG-CONSUMED 0 (cleared) · UNCLEAR 0 (cleared) · DELIBERATE-DROP 24 · MOOT 22 · FAITHFUL ~46 rows (~190 getters) ·
stale annotations 9 · zero-stub classes 30/46. *(The last four counts predate the Info-header rebuild and are
unverified — see the note above §1.)*
