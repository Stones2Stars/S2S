# SURROUND_DESTROY removal map (deferred — turnkey for circle-back)

`GAMEOPTION_COMBAT_SURROUND_DESTROY` is the "S&D Extended" family: **surround + enclose +
lunge + unnerve + dynamicDefense** (unit) and **LocalDynamicDefense** (city/building).
Unlike BATTLEWORN it is a **LIVE runtime option** (no `#define` gate) — it actually changes
combat when enabled. User deferred it (2026-06: "least bad of the bad options"). Concept
captured in combat-simplification-scope.md good-ideas list. Map below is complete; removal
has zero cross-system dependencies (verified) — it's just large.

## The one combat application site (simplify, don't just hunt)
`CvUnit::getDefenderCombatValues` (~CvUnit.cpp:12176):
```
const int iSurround = pAttacker->surroundedDefenseModifier(pAttackedPlot, this);
iTempModifier -= std::max(0, iSurround - iSurround * dynamicDefenseTotal() / 100);
```
→ delete both lines. That's the only place S&D touches combat math.

## Whole functions to delete (no other callers after the above is gone)
- `CvUnit::surroundedDefenseModifier` (CvUnit.cpp ~24961-25053, 93 lines; sums adjacent
  allies' `encloseTotal()`/`unnerveTotal()`, caps at `GC.getSAD_MAX_MODIFIER()+iEnclose`,
  scales by `lungeTotal()`).
- `CvUnit::dynamicDefenseTotal` / `unnerveTotal` / `encloseTotal` / `lungeTotal`
  (CvUnit.cpp ~13244-13267). dynamicDefenseTotal also folds in city
  `getExtraLocalDynamicDefense()`.

## Unit side (CvUnit.h/.cpp)
- members `m_iExtraUnnerve/Enclose/Lunge/DynamicDefense` (decl ~1623-1626; init ~585-588;
  copy ~929-932; WRAPPER_READ ~19978-81; WRAPPER_WRITE ~21049-52).
- `getExtra*` (~16090-16196) + `changeExtra*` mutators + their `*Total` accessors (above).
- processPromotion (~19127) + processUnitCombat (~18676) call `changeExtra*(...Change()*iChange)`.

## Info classes
- CvUnitInfo: m_iUnnerve/Enclose/Lunge/DynamicDefense (.h ~765-768; ctor ~222-225; getters
  ~1860-1895 (option-gated); read ~3723-26; checksum ~3109-12; copyNonDefaults ~4591-94).
- CvPromotionInfo: *Change variants (.h ~577-580; ctor ~139-142; getters ~950-987; read
  ~2696-99; checksum ~3885-88; copy ~3367-70).
- CvUnitCombatInfo: *Change getters (.h ~94-97; .cpp ~459-491 option-gated) + read/members.
- CvBuildingInfo: m_iLocalDynamicDefense (.h ~213,679; getter ~1120; ctor ~272; read ~2956;
  checksum ~1828; copy ~4016).

## City side (CvCity.h/.cpp) + Python
- m_iExtraLocalDynamicDefense (.h ~1723; accessors ~1939-41 / .cpp ~22871-82); ctor ~628;
  building apply ~4793 (`changeExtraLocalDynamicDefense(kBuilding.getLocalDynamicDefense()*iChange)`);
  serialization ~17222/17859.
- CyInfoInterface1.cpp ~423: `.def("getLocalDynamicDefense", ...)`.

## AI + UI
- CvPlayerAI.cpp ~29167-29212 + ~32380-32439: promotion-value multipliers (simple deletes).
- CvCityAI.cpp ~5330, 12485, 12657/12976: building-value `getLocalDynamicDefense()/2` + bSAD.
- CvBuildingFilters.cpp ~277-281: filter gate.
- CvGameTextMgr.cpp gated blocks: unit help ~1334-1388, promo summary ~8846-52, unit-type
  ~13316-40, building help ~17901-08, city hover ~29956-61.

## Enum / option / XML
- CvEnums.h ~851; CvPythonEnumLoader/CyEnumsInterface `.value(...)`.
- CIV4GameOptionInfos.xml option def; Gameoptions_CIV4GameText option name+help.
- Schema: C2C_CIV4UnitSchema (iUnnerve/Enclose/Lunge/DynamicDefense + *Change ElementTypes +
  Unit/Promotion/UnitCombat element refs); C2C_CIV4BuildingsSchema (iLocalDynamicDefense).
- Data: CIV4PromotionInfos (20+ *Change), CIV4UnitCombatInfos, U_Land_CIV4UnitInfos
  (iUnnerve/…), Regular_CIV4BuildingInfos (12+ iLocalDynamicDefense), P2K module.
- GameText keys: UNITHELP_{UNNERVE,ENCLOSE,LUNGE,DYNAMIC_DEFENSE}[_TOTAL][_SHORT],
  PROMOTIONHELP_{UNNERVE,ENCLOSE,LUNGE,DYNAMIC_DEFENSE}, LOCAL_DYNAMIC_DEFENSE,
  MISC_BUILDING_DYNAMIC_DEFENSE_HOVER, plus ENCLOSE promotion/promotionline keys if the
  enclose promotions are also dropped.

## Procedure (when circling back)
1. Delete the combat application (getDefenderCombatValues 2 lines) + the whole functions.
2. Remove unit members/accessors/mutators/serialization + processPromotion/UnitCombat wiring.
3. Remove Info fields (+read/checksum/copy) + schema + XML data (strip tags BEFORE schema).
4. Remove city LocalDynamicDefense + building apply + Python + AI + UI + option + GameText.
5. Build (Assert) + XML-validate after each phase. KEEP `getSAD_MAX_MODIFIER` global only if
   referenced elsewhere (check) — likely removable too.
