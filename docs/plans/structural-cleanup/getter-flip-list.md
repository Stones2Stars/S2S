# Getter flip-list — modifier read-path

> **⛔ CURRENT TRUTH (the flips have LANDED; the pre-init `*Legacy` FALLBACKS are CUT).** The modifier-value getters
> are flipped and now read **cascade-only** — there is **no** pre-init `!isFinalInitialized() → *Legacy` bridge left on
> any of them. The per-row "flip action" / "keep pre-init fallback" / "`*Legacy` oracle" notes below are the ORIGINAL
> 2026-07-13 PLAN and are historical: the fallback-cut ruling ([fixed-point-conformance.md](fixed-point-conformance.md),
> [DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)) removed every pre-init `*Legacy`
> fallback (rates, `getTotalGreatPeopleRateModifier`, `getEffectiveMaintenanceModifier`, `getTradeRoutes`,
> `CvPlayer::getCityDefenseModifier`, `getProductionModifier`×3) + the oracle-only `*Legacy` (happy/health/defense/
> GP-base). Cascade slots bind DIRTY so a loaded save recomputes-from-source on first read — no unwarm window.
> **The ONLY `*Legacy` still alive are NOT fallbacks:** the enabler set (`canTrain/canConstruct/canCreate/canMaintain/
> canDoCivics/canHurry/canFoundReligion` + `isPromotionValidLegacy`) — the AI's what-if data source
> (`bContinue`/`bIgnore*`/probability), retired by the **F2b consumer sweep** (rewire the ~262 whole-database AI loops
> onto the enabler frontier — [roadmap §F2b](roadmap.md)), NOT a getter-fallback delete.

The distinct modifier-value GETTERS to flip so each reads its game-object's own cached
calculations and sums them live (generated 2026-07-13, from the 14-family getter sweep).

**Model:** the getter reads the object's own cache — the modifier caches
(`m_cascadeCityPackages` / `m_cascadePlayerScope` / the `CvPlot` yield cache) or an
enabler's cached sets — and sums live. Here "cascade" = the modifier system, which
POPULATES + INVALIDATES those caches; the getter must never call out to it on read.
Where a getter's body today is `CascadeAccumulator::foo(this,…)`, note that those helpers
are ALREADY bare object-cache reads keyed on the passed object — the only violation is
that the summation lives in the `Cascade` namespace instead of in the getter body, so the
"flip" there is a shallow mechanical inline (move `ensure`+read+live-sum into the method).
Warehouse mechanics (banked stock, per-turn accrual, battle-damage counters) stay as-is.

bodyState ∈ reads-object-cache | sums-legacy | delegates-to-cascade-accumulator | mixed | warehouse-mechanic | other.
contract ∈ stable | breaks | unknown.

## FOOD+PRODUCTION city yield getters (CvCity)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getYieldRate100 (CvCity.cpp:11429) | city | delegates | inline acc_yieldCombine: read m_cascadeCityPackages (yPctCity/ySpec/yExtra100) + owner m_cascadePlayerScope (yFlatFreeCity/yFlatGoldenAge) + getPlotYield + trade; keep pre-init legacy fallback | stable |
| CvCity::getYieldRate100Legacy (CvCity.cpp:11419) | city | sums-legacy | none — intentional load-time-only shim | stable |
| CvCity::getYieldRate (CvCity.cpp:11403) | city | delegates | none — inherits getYieldRate100 via /100 | stable |
| CvCity::getBaseYieldRate (CvCity.cpp:23437) | city | mixed | keep getPlotYield/getTradeYield; replace getFreeCityYield/getGoldenAgeYield with m_cascadePlayerScope.yFlatFreeCity/yFlatGoldenAge | stable |
| CvCity::getBaseYieldRateModifier (CvCity.cpp:11379) | city | sums-legacy | replace 5/6-term legacy sum with max(0, 100+iExtra+m_cascadeCityPackages.yPctCity) | stable |
| CvCity::getPlotYield (CvCity.cpp:11441) | city | reads-object-cache | none — already flipped (sums CvPlot yield cache) | stable |
| CvCity::getProductionPerTurn (CvCity.cpp:4079) | city | mixed | none direct — inherits once callees flip; verify getExtraYield(PROD) scope | stable |
| CvCity::getProductionDifference (CvCity.cpp:4092) | city | mixed | none — inherits getProductionPerTurn | stable |
| CvCity::getExtraYield100 (CvCity.cpp:11561) | city | mixed | building-extra cache OK; align/read m_cascadeCityPackages.yExtra100 + pop term (gap) | unknown |

## GOLD/RESEARCH/ESPIONAGE commerce getters (CvCity + CvPlayer)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getCommerceRateTimes100 | city | delegates | unwind: read city commerce package + sum live (mirror getYieldRate100); keep pre-init legacy | unknown |
| CvCity::getCommerceRate | city | delegates | none — inherits getCommerceRateTimes100 | stable |
| CvCity::getCommerceRateTimes100Legacy | city | warehouse-mechanic | none — pre-init fallback oracle | stable |
| CvCity::getCommerceFromPercent | city | reads-object-cache | none — already reads flipped getYieldRate100 | stable |
| CvCity::getBaseCommerceRate | city | mixed | fold getBaseCommerceRateExtra sub-terms into cascade commerce package (no forced flip) | stable |
| CvCity::getBaseCommerceRateTimes100 | city | mixed | same as getBaseCommerceRate | stable |
| CvCity::getCommerceRateModifier | city | sums-legacy | event-only accumulator: route through cascade or keep legacy (owner ruling) | stable |
| CvCity::getTotalCommerceRateModifier | city | mixed | leave caching; only getCommerceRateModifier source in question | stable |
| CvPlayer::getCommerceRate | player | sums-legacy | flip to live sum of city getCommerceRateTimes100 (or player cascade package) — forces auditing every changeCommerceRate writer | breaks |
| CvPlayer::getCommerceRateModifier | player | sums-legacy | read player-scope cascade modifier package (owner ruling) | unknown |
| CvPlayer::getTotalCityBaseCommerceRate | player | reads-object-cache | none — already target pattern (algo::accumulate over cities) | stable |
| CvPlayer::calculateBaseNetGold | player | mixed | none — inherits getCommerceRate | stable |
| CvPlayer::calculateBaseNetResearch | player | mixed | none — inherits getCommerceRate; Cy passthrough stable | stable |

## Culture (COMMERCE_CULTURE rate + banked culture value getters)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getCommerceRate(CommerceTypes) | CvCity | mixed | none — inherits getCommerceRateTimes100 | stable |
| CvCity::getCommerceRateTimes100(CommerceTypes) | CvCity | delegates | inline commerceRate100: read m_cascadeCityPackages + m_cascadePlayerScope + CommerceCalc::combineSplit; keep pre-init legacy | stable |
| CvCity::getCulture(PlayerTypes) | CvCity | warehouse-mechanic | none — banked m_aiCulture | stable |
| CvCity::getCultureTimes100(PlayerTypes) | CvCity | warehouse-mechanic | none — banked m_aiCulture | stable |
| CvCity::getCultureLevel() | CvCity | warehouse-mechanic | none — cached m_eCultureLevel off warehouse total | stable |
| CvPlot::getCulture(PlayerTypes) | CvPlot | warehouse-mechanic | none — banked plot m_aiCulture | stable |
| CvPlayer::getCulture() | CvPlayer | warehouse-mechanic | none — banked m_iCulture | stable |

## HAPPINESS + HEALTH (CvCity/CvPlayer wellbeing getters)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::happyLevel | CvCity | delegates | inline read aWbVerdict[0] + live military fold; keep pre-init guard | stable |
| CvCity::unhappyLevel | CvCity | delegates | inline read aWbVerdict[1] + military fold (iExtra==0); what-if stays legacy | stable |
| CvCity::goodHealth | CvCity | delegates | inline read aWbVerdict[2] | stable |
| CvCity::badHealth | CvCity | delegates | inline read aWbVerdict[3] (bNoAngry==false && iExtra==0); what-if legacy | stable |
| CvCity::unhappyLevelLegacy | CvCity | sums-legacy | none — shadow/what-if oracle; flip its sub-getters | stable |
| CvCity::happyLevelLegacy | CvCity | sums-legacy | none — shadow oracle | stable |
| CvCity::goodHealthLegacy | CvCity | sums-legacy | none — shadow oracle | stable |
| CvCity::badHealthLegacy | CvCity | sums-legacy | none — shadow oracle | stable |
| CvCity::angryPopulation | CvCity | mixed | none — combinator over happy/unhappyLevel | stable |
| CvCity::healthRate | CvCity | mixed | none — combinator over good/badHealth | stable |
| CvCity::getMilitaryHappiness | CvCity | warehouse-mechanic | none — live counter×rate, excluded per #430 | stable |
| CvCity::getBuildingGoodHappiness | CvCity | sums-legacy | read cascade building-happiness cache; keep calculatePopulationHappiness fold | unknown |
| CvCity::getBuildingBadHappiness | CvCity | sums-legacy | same as getBuildingGoodHappiness | unknown |
| CvCity::getExtraBuildingGoodHappiness | CvCity | sums-legacy | read cascade building-keyed ledger fold | unknown |
| CvCity::getExtraBuildingBadHappiness | CvCity | sums-legacy | same | unknown |
| CvCity::getFeatureGoodHappiness | CvCity | sums-legacy | read cascade feature-happiness cache | unknown |
| CvCity::getFeatureBadHappiness | CvCity | sums-legacy | same | unknown |
| CvCity::getBonusGoodHappiness | CvCity | sums-legacy | read cascade bonus-happiness cache | unknown |
| CvCity::getBonusBadHappiness | CvCity | sums-legacy | same | unknown |
| CvCity::getReligionGoodHappiness | CvCity | sums-legacy | read cascade religion-happiness cache | unknown |
| CvCity::getReligionBadHappiness | CvCity | sums-legacy | same | unknown |
| CvCity::getExtraHappiness | CvCity | sums-legacy | read cascade extra-happiness cache | unknown |
| CvCity::getCommerceHappinessPer | CvCity | sums-legacy | read cascade commerce-happiness-per-type array (CommerceTypes-indexed, Python-visible) | breaks |
| CvCity::getCommerceHappinessByType | CvCity | mixed | none — combinator | stable |
| CvCity::getCommerceHappiness | CvCity | mixed | none — combinator loop | stable |
| CvCity::getCurrentStateReligionHappiness | CvCity | mixed | flip sub-getter getStateReligionHappiness | stable |
| CvCity::getStateReligionHappiness | CvCity | sums-legacy | read cascade per-religion happiness array | unknown |
| CvCity::getCivicHappiness | CvCity→CvPlayer | mixed | flip CvPlayer::getCivicHappiness (m_iCivicHappiness) | stable |
| CvCity::getSpecialistHappiness | CvCity | sums-legacy | read cascade specialist-happiness cache | unknown |
| CvCity::getSpecialistUnhappiness | CvCity | sums-legacy | same | unknown |
| CvCity::getVassalHappiness | CvCity | other | live team loop, no cache; fold on-top or leave (gap) | unknown |
| CvCity::getVassalUnhappiness | CvCity | other | same (gap) | unknown |
| CvCity::getLargestCityHappiness | CvCity→CvPlayer | mixed | flip CvPlayer::getLargestCityHappiness (m_iLargestCityHappiness) | stable |
| CvCity::getFreshWaterGoodHealth | CvCity | sums-legacy | read cascade freshwater-health cache | unknown |
| CvCity::getFeatureGoodHealth | CvCity | sums-legacy | read cascade feature-health cache | unknown |
| CvCity::getFeatureBadHealth | CvCity | sums-legacy | same | unknown |
| CvCity::getBonusGoodHealth | CvCity | sums-legacy | read cascade bonus-health cache | unknown |
| CvCity::getBonusBadHealth | CvCity | sums-legacy | same | unknown |
| CvCity::getExtraHealth | CvCity | sums-legacy | read cascade extra-health cache | unknown |
| CvCity::getImprovementGoodHealth | CvCity | sums-legacy | read cascade improvement-health cache | unknown |
| CvCity::getImprovementBadHealth | CvCity | sums-legacy | same | unknown |
| CvCity::getSpecialistGoodHealth | CvCity | sums-legacy | read cascade specialist-health cache | unknown |
| CvCity::getSpecialistBadHealth | CvCity | sums-legacy | same | unknown |
| CvCity::calculateCorporationHealth | CvCity | other | live XML loop, no cache slot (gap — fold into cascade gather or leave) | unknown |
| CvCity::calculateCorporationHappiness | CvCity | other | same (gap) | unknown |
| CvCity::calculatePopulationHappiness | CvCity | warehouse-mechanic | none — rate×live pop, excluded per #430 | stable |
| CvCity::calculatePopulationHealth | CvCity | warehouse-mechanic | none — rate×live pop | stable |
| CvCity::getExtraTechHappinessTotal | CvCity | other | body not read — classify before flip (gap) | unknown |
| CvCity::getExtraTechHealthTotal | CvCity | other | body not read (gap) | unknown |
| CvCity::totalGoodBuildingHealth | CvCity/Area/Player | mixed | none — combinator; flips transitively | stable |
| CvCity::totalBadBuildingHealth | CvCity/Area/Player | mixed | none — combinator | stable |
| CascadeAccumulator::wellbeing (CvCascadeAccumulator.cpp:426) | CvCity (Cascade fn) | reads-object-cache | delete/inline its bare aWbVerdict fetch into the four CvCity getters | stable |

## greatPeopleRate

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getBaseGreatPeopleRate (CvCity.cpp:7298) | city | reads-object-cache | none — already flipped; keep isFinalInitialized guard | stable |
| CvCity::getBaseGreatPeopleRateLegacy (7308) | city | sums-legacy | none — intentional legacy oracle | stable |
| CvCity::getGreatPeopleRate (7314) | city | reads-object-cache | none — conforms | stable |
| CvCity::getTotalGreatPeopleRateModifier (7324) | city | reads-object-cache | none — already flipped | stable |
| CvCity::getTotalGreatPeopleRateModifierLegacy (7331) | city | sums-legacy | none — legacy oracle | stable |
| CvCity::getGreatPeopleRateModifier (7357) | city | sums-legacy | add standalone scGpModCity accessor + isFinalInitialized guard, read it (not yet flipped) | stable |
| CvCity::getAdditionalGreatPeopleRateByBuilding (7398) | city | mixed | none — what-if projection | stable |
| CvCity::getAdditionalBaseGreatPeopleRateByBuilding (7434) | city | other | none — what-if | stable |
| CvCity::getAdditionalGreatPeopleRateModifierByBuilding (7481) | city | other | none — what-if | stable |
| CvCity::getAdditionalGreatPeopleRateBySpecialist (7495) | city | mixed | none — what-if | stable |
| CvCity::getAdditionalBaseGreatPeopleRateBySpecialist (7509) | city | other | none — what-if | stable |
| CvCity::getGreatPeopleUnitRate (14230) | city | sums-legacy | BLOCKED — needs new per-unit-type cascade channel | unknown |
| CvPlayer::getGreatPeopleRateModifier (9770) | player | sums-legacy | unclear — civic/trait % re-routed to city package; literal flip changes value (scope mismatch) | unknown |
| CvPlayer::getStateReligionGreatPeopleRateModifier (9806) | player | sums-legacy | unclear — cascade form is intrinsically per-city; no player-wide equivalent | unknown |
| CvPlayer::getNationalGreatPeopleRate (30028) | player | sums-legacy | add isFinalInitialized guard → scGpNational(this), fallback renamed *Legacy | stable |
| CvPlayer::getNationalGreatPeopleUnitRate (30021) | player | sums-legacy | BLOCKED — needs per-unit-type channel | unknown |

## DEFENSE getters (building/total/modifier, bombard, min-defense floor) — CvCity/CvPlayer/CvPlot

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getBuildingDefense | city | reads-object-cache | none — already flipped (scDefense bare read) | stable |
| CvCity::getBuildingDefenseLegacy | city | sums-legacy | none — pre-init oracle | stable |
| CvCity::getBuildingBombardDefense | city | reads-object-cache | none — already flipped (scDefBombard + player bombard) | stable |
| CvCity::getBuildingBombardDefenseLegacy | city | sums-legacy | none — oracle | stable |
| CvCity::getAdditionalBombardDefenseByBuilding | city | mixed | none — live what-if | stable |
| CvCity::getAdditionalDefenseByBuilding | city | mixed | none — what-if | stable |
| CvCity::getDefenseDamage | city | warehouse-mechanic | none — battle-damage counter | stable |
| CvCity::getLastDefenseDamage | city | warehouse-mechanic | none — damage bookkeeping | stable |
| CvPlot::getDefenseDamage | plot | warehouse-mechanic | none — plot battle-damage counter | stable |
| CvCity::getNaturalDefense | city | other | none — XML culture-level lookup | stable |
| CvCity::getTotalDefense | city | mixed | none on composition; calculateBonusDefense still legacy | stable |
| CvCity::getDefenseModifier | city | mixed | none — inherits inputs | stable |
| CvCity::getBonusDefenseChanges | city | sums-legacy | fold into cascade city package, read cache (no counterpart today) | stable |
| CvCity::calculateBonusDefense | city | sums-legacy | read pre-summed cascade scalar instead of iterating m_bonusDefenseChanges | stable |
| CvCity::getEspionageDefenseModifier | city | sums-legacy | read cascade city package (city+national fold); no sc* channel yet (gap) | stable |
| CvCity::getMinimumDefenseLevel | city | mixed | none — getExtraMinDefense flipped; floor is set directly | stable |
| CvCity::getExtraMinDefense | city | reads-object-cache | none — already flipped (scDefMin bare read) | stable |
| CvCity::getExtraMinDefenseLegacy | city | sums-legacy | none — oracle | stable |
| CvPlayer::getCityDefenseModifier | player | reads-object-cache | none — already flipped (defPlayerAll) | stable |
| CvPlayer::getCityDefenseModifierLegacy | player | sums-legacy | none — oracle | stable |
| CvPlayer::getExtraCityDefense | player | sums-legacy | none itself — legacy input folded into scDefensePlayer | stable |
| CvPlayer::getTraitExtraCityDefense | player | sums-legacy | none itself — legacy input | stable |
| CvPlayer::getNationalEspionageDefense | player | sums-legacy | flip with getEspionageDefenseModifier once fold exists (gap) | stable |
| CvPlayer::getNationalBombardDefenseModifier | player | sums-legacy | none itself — already folded into scBuildingBombardDefense | stable |
| CvCity::getExtraLocalDynamicDefense | city | sums-legacy | candidate cascade fold; no counterpart (gap) | stable |
| CvCity::getExtraRiverDefensePenalty | city | sums-legacy | same (gap) | stable |
| CvCity::getExtraBuildingDefenseRecoverySpeedModifier | city | sums-legacy | candidate recovery-speed cascade fold (gap) | stable |
| CvCity::getModifiedBuildingDefenseRecoverySpeedCap | city | sums-legacy | same (gap) | stable |
| CvCity::getExtraCityDefenseRecoverySpeedModifier | city | sums-legacy | same (gap) | stable |
| CvCity::cityDefenseRecoveryRate | city | warehouse-mechanic | none — per-turn rate math; its modifier inputs are the flip candidates | stable |
| CvPlot::defenseModifier | plot | other | none — live combat calc; city dep already flipped | stable |

## Maintenance getters (getMaintenance/modifier/base + distance/numCities/connected/area components)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getMaintenance (CvCity.cpp:7738) | city | reads-object-cache | none — correct dirty-flag cache shape | stable |
| CvCity::getMaintenanceTimes100 (7747) | city | reads-object-cache | none — correct | stable |
| CvCity::getEffectiveMaintenanceModifier (7756) | city | delegates | inline scMaintenanceModifier body (scMaintModCity + player maintPlayerAll/maintAreaPct/maintConnPct); retire helper | stable |
| CvCity::getEffectiveMaintenanceModifierLegacy (7763) | city | sums-legacy | none — oracle/fallback | stable |
| CvCity::getMaintenanceModifier (8083) | city | sums-legacy | not flippable (single scalar); retire once consumers read scMaintModCity | stable |
| CvCity::calculateBaseMaintenanceTimes100 (8067) | city | mixed | none — inherits children | stable |
| CvCity::calculateDistanceMaintenance (7802) | city | sums-legacy | tracks Times100 sibling | stable |
| CvCity::calculateDistanceMaintenanceTimes100 (7807) | city | sums-legacy | BLOCKED — needs distance-maint cascade channel | unknown |
| CvCity::calculateNumCitiesMaintenance (7865) | city | sums-legacy | tracks Times100 sibling | stable |
| CvCity::calculateNumCitiesMaintenanceTimes100 (7870) | city | sums-legacy | BLOCKED — needs numCities channel | unknown |
| CvCity::calculateColonyMaintenance (7928) | city | other | tracks Times100 sibling | stable |
| CvCity::calculateColonyMaintenanceTimes100 (7933) | city | other | none — no modifier input | stable |
| CvCity::calculateCorporationMaintenance (7971) | city | other | tracks Times100 sibling | stable |
| CvCity::calculateCorporationMaintenanceTimes100 (no-arg) (7976) | city | mixed | none — propagates from per-corp overload | stable |
| CvCity::calculateCorporationMaintenanceTimes100(CorporationTypes) (7992) | city | sums-legacy | BLOCKED — needs corp-maint channel | unknown |
| CvCity::calculateBuildingMaintenanceTimes100 (8045) | city | other | none — no modifier input | stable |
| CvCity::calcCorporateMaintenance (22306) | city | sums-legacy | flips with corp-maint channel (revenue, borderline member; gap) | unknown |
| CvPlayer::getMaintenanceModifier (10718) | player | sums-legacy | none itself — dead once Legacy retired | stable |
| CvPlayer::getCoastalDistanceMaintenanceModifier (10732) | player | sums-legacy | BLOCKED — distance channel gap | unknown |
| CvPlayer::getConnectedCityMaintenanceModifier (10746) | player | sums-legacy | none itself — dead once Legacy retired | stable |
| CvPlayer::getDistanceMaintenanceModifier (CvPlayer.h:696) | player | sums-legacy | BLOCKED — distance channel gap | unknown |
| CvPlayer::getNumCitiesMaintenanceModifier (CvPlayer.h:699) | player | sums-legacy | BLOCKED — numCities channel gap | unknown |
| CvPlayer::getCorporationMaintenanceModifier (CvPlayer.h:702) | player | sums-legacy | BLOCKED — corp channel gap | unknown |
| CvPlayer::getHomeAreaMaintenanceModifier (CvPlayer.h:705) | player | sums-legacy | none itself — cascade eq exists; dead once Legacy retired | stable |
| CvPlayer::getOtherAreaMaintenanceModifier (CvPlayer.h:708) | player | sums-legacy | none itself — dead once Legacy retired | stable |
| CvPlayer::getTotalMaintenance (10809) | player | reads-object-cache | none — correct dirty-flag cache | stable |
| CvArea::getMaintenanceModifier(PlayerTypes) (CvArea.cpp:788) | area | sums-legacy | none itself — dead once Legacy retired | stable |
| CvArea::getTotalAreaMaintenanceModifier(PlayerTypes) (CvArea.cpp:828) | area | sums-legacy | none — dead weight, will be deleted (superseded by cascade) | stable |

## Trade Route getters (count: getMaxTradeRoutes/getTradeRoutes; yield: getTradeYield)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getTradeRoutes (CvCity.cpp:15779) | city | delegates | inline scTradeRoutes sum (scTradeCity + player tradeEmpireAll/tradeCoastalAll + world tradeWorldFlat + live inputs), clamp to getMaxTradeRoutes | stable |
| CvCity::getTradeRoutesLegacy (15785) | city | sums-legacy | none — the net oracle (#430) | stable |
| CvCity::getMaxTradeRoutes (10076) | city | sums-legacy | GC static stays; owner-adjustment term has no cascade eq (gap) | unknown |
| CvPlayer::getMaxTradeRoutesAdjustment (30034) | player | sums-legacy | none — no cascade package (gap) | unknown |
| CvPlayer::getTradeRoutes (11186) | player | sums-legacy | none — now only feeds legacy oracle | stable |
| CvCity::getTradeYield (11784) | city | warehouse-mechanic | none — engine-written per-city package (m_aiTradeYield) | stable |

## buildRate / production-cost modifier getters (unit/building/project buildRate, military/space/wonder)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getProductionModifier(UnitTypes) | city | delegates | inline buildRateUnit cache reads (brCityKeyed/brCityMilitary/brSrUnitProd + brEmpKeyed + self-mod scan) | stable |
| CvCity::getProductionModifier(BuildingTypes) | city | delegates | inline buildRateBuilding cache reads (brCityKeyed/wonder/brSrBuildingProd) | stable |
| CvCity::getProductionModifier(ProjectTypes) | city | delegates | inline buildRateProject (brCitySpace + brEmpSpace) | stable |
| CvCity::getProductionModifierLegacy(Unit/Building/Project) | city | sums-legacy | none — pre-init fallback | stable |
| CvCity::getProductionModifier(OrderData)/() | city | other | none — pure dispatcher | stable |
| CvCity::getUnitProductionModifier(UnitTypes) | city | sums-legacy | read brCityKeyed.units slot instead of m_unitProductionMod map | unknown |
| CvCity::getBuildingProductionModifier(BuildingTypes) | city | sums-legacy | read brCityKeyed.buildings | unknown |
| CvCity::getDomainProductionModifier(DomainTypes) | city(+player) | sums-legacy | read brCityKeyed.domains + brEmpKeyed.domains | unknown |
| CvCity::getUnitCombatProductionModifier(UnitCombatTypes) | city | sums-legacy | read brCityKeyed.unitCombats | unknown |
| CvCity::getMilitaryProductionModifier() | city | sums-legacy | read brCityMilitary cache field | stable |
| CvCity::getSpaceProductionModifier() | city | sums-legacy | read brCitySpace cache field | stable |
| CvPlayer::getProductionModifier(UnitTypes) | player | sums-legacy | replace trait walk with brEmpKeyed.units read | unknown |
| CvPlayer::getProductionModifier(BuildingTypes) | player | sums-legacy | read brEmpKeyed.specialBuildings + wonder maxima | unknown |
| CvPlayer::getProductionModifier(ProjectTypes) | player | sums-legacy | read brEmpSpace directly | stable |
| CvPlayer::getUnitProductionModifier(UnitTypes) | player | sums-legacy | read brEmpKeyed.units | unknown |
| CvPlayer::getBuildingProductionModifier(BuildingTypes) | player | sums-legacy | read brEmpKeyed.buildings | unknown |
| CvPlayer::getUnitCombatProductionModifier(UnitCombatTypes) | player | sums-legacy | read brEmpKeyed.unitCombats | unknown |
| CvPlayer::getMilitaryProductionModifier() | player | sums-legacy | read brEmpMilitary | stable |
| CvPlayer::getSpaceProductionModifier() | player | sums-legacy | read brEmpSpace | stable |
| CvPlayer::getMaxGlobal/Team/PlayerBuildingProductionModifier() | player | sums-legacy | fold into cascade wonder-package cache reads (promoted to player scope) | stable |

## Unit Combat (combat strength/withdrawal/first strikes/bombard/collateral/air/capture/heal)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvUnit::maxCombatStr / currCombatStr / currCombatStrFloat | unit | sums-legacy | unwind ~20 legacy modifier terms onto unit cascade combat-modifier package; keep LRU perf cache | stable |
| CvUnit::withdrawalProbability | unit | sums-legacy | replace getExtraWithdrawal with cascade withdrawal package; keep cap | stable |
| CvUnit::firstStrikes | unit | sums-legacy | replace getExtraFirstStrikes with cascade first-strike package; flip chance/maxFirstStrikes together | stable |
| CvUnit::getBombardRate | unit | sums-legacy | replace getExtraBombardRate with cascade bombard package (both non-SM and SM branches) | stable |
| CvUnit::collateralDamage (+Limit/MaxUnits) | unit | sums-legacy | replace each getExtra* with cascade collateral package | stable |
| CvUnit::airMaxCombatStr (+air dependents) | unit | sums-legacy | replace legacy modifier lookups with cascade air-combat package in getModifiedIntValue | stable |
| CvUnit::captureProbabilityTotal | unit(+player/city) | sums-legacy | replace four legacy reads (unit/commander/commodore/player/city) with cascade packages | stable |
| CvUnit::healRate | unit | mixed | replace getSelfHealModifierTotal + getExtra*Heal with cascade heal package; keep derivation logic | stable |

## Unit non-combat (moveCost/maxMoves, visibilityRange/seeFromLevel, workRate, cargoSpace, getExperience)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvUnit::maxMoves() | unit | sums-legacy | flip baseMoves() legacy sum → cascade move package; keep turn-memo shell | stable |
| CvPlot::movementCost(CvUnit*, CvPlot*) | plot | sums-legacy | swap getExtraMoveDiscount → cascade move-discount package; keep terrain math + hash memo | stable |
| CvUnit::getExtraMoveDiscount() | unit | sums-legacy | read cascade move-discount package (keep commander/commodore branch) | stable |
| CvUnit::visibilityRange(CvPlot*) | unit | sums-legacy | read cascade visibility package instead of getExtraVisibilityRange accumulator | stable |
| CvUnit::getExtraVisibilityRange() | unit | sums-legacy | read cascade visibility package (keep commander branch) | stable |
| CvPlot::seeFromLevel(TeamTypes) | plot | sums-legacy | replace isExtraWaterSeeFrom with team water-sight package | stable |
| CvUnit::baseWorkRate/workRate/hills/peaks/getWorkModifier | unit | sums-legacy | unwind m_worker accumulators → cascade work-rate package; leave SM branch | stable |
| CvUnit::cargoSpace() | unit | sums-legacy | replace m_iCargoCapacity + national cargo accumulators → cascade cargo packages; leave SM branch | stable |
| CvUnit::getExperience() | unit | sums-legacy | likely warehouse (XP banking) — owner ruling; else unwind getExperience100 | stable |

## FREE XP / FREE SPECIALIST COUNT / SPECIALIST YIELD-COMMERCE OUTPUT (CvCity/CvPlayer/CvTeam)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::getFreeExperience() (CvCity.cpp:10180) | city | sums-legacy | introduce cascade city free-XP channel, sum cache | stable |
| CvPlayer::getFreeExperience() (CvPlayer.cpp:9854) | player | sums-legacy | fold into player cascade free-XP package | stable |
| CvCity::getMaxSpecialistCount() / (SpecialistTypes) (14419/14424) | city | mixed | typed: fold into cascade per-spec cap package; no-arg: composite of flipped feeders | stable |
| CvCity::getFreeSpecialistCount(SpecialistTypes) (14497) | city | sums-legacy | DO NOT flip — this is a cascade INPUT (circular); leave legacy write-time accumulation | stable |
| CvPlayer::getFreeSpecialistCount(SpecialistTypes) (26886) | player | sums-legacy | leave — upstream feeder into city count | stable |
| CvTeam::getFreeSpecialistCount(SpecialistTypes) (CvTeam.cpp:7124) | team | sums-legacy | leave — upstream feeder | stable |
| CvCity::getYieldBySpecialist(YieldTypes,SpecialistTypes) (11366) | city(+player) | sums-legacy | replace player getExtraSpecialistYield/getSpecialistYieldPercentChanges with cascade package | stable |
| CvCity::getSpecialistYieldTotal(YieldTypes) (11591) | city | sums-legacy | replace percent-change reads with cascade package; keep recompute-on-read shape | stable |
| CvCity::getExtraSpecialistYield (total + per-spec) (12020/12027) | city | mixed | keep city cache shape; re-source inner sum from cascade specialist-extra-yield package | stable |
| CvCity::getExtraSpecialistCommerceTotal (+per-spec) (12091/12097) | city | mixed | same as yield sibling | stable |
| CvCity::getSpecialistCommerce(CommerceTypes) (12760) | city | sums-legacy | replace getSpecialistCommercePercentChanges with cascade package | stable |
| CvCity::getAdditionalBaseGreatPeopleRateBySpecialist (7509) | city | other | none — pure XML what-if | stable |
| CvCity::getAdditionalBaseCommerceRateBySpecialist / getAdditionalCommerceTimes100BySpecialist (12858/12845) | city(+player) | mixed | base: none (XML); Times100: swap percent-changes for cascade read | stable |

## PROPERTY_*

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvProperties::getValue / getValueByProperty | per-game-object | warehouse-mechanic | none — banked stock ledger | stable |
| CvProperties::getProperty(index) / getChangeProperty(index) | per-game-object | warehouse-mechanic | none — identity/index accessor | stable |
| CvProperties::getChange(index) / getChangeByProperty(eProp) | per-game-object | warehouse-mechanic | none — solver scratch accumulator | stable |
| CvProperties::getPositionByProperty / getNumProperties | per-game-object | other | none — container bookkeeping | stable |
| CvGameObject*/CvCity/CvPlayer/CvUnit/CvPlot/CvTeam/CvGame::getProperties | multi | reads-object-cache | none — hands back the object's own ledger | stable |
| CascadeProperty::citySourceFlat(eProp,pCity,ec) | city | delegates | wire CvPropertySource to read this cached package; currently orphaned (debug-only) | unknown |
| CascadeProperty::cityUnitFlat(eProp,pCity,ec) | city | delegates | live addend on top of flat sum; wire into engine getter (kept uncached by design) | unknown |
| CascadeProperty::cityDecayPercent(eProp) | city | delegates | cascade-fed rate; wire into CvPropertySourceDecay (XML-authored today) | unknown |

## ENABLER availability (canConstruct/canTrain/canResearch/canDoCivics/canMaintain)

| getter (file:function) | scope | bodyState | flip action | contract |
|---|---|---|---|---|
| CvCity::canConstruct(BuildingTypes,…) | CvCity (CPK_FRONT_B) | delegates | inline ensure(CPK_FRONT_B)+enBuildable[Visible].count(); drop enConstruct/enConstructVisible indirection | stable |
| CvCity::canTrain(UnitTypes,…) | CvCity (CPK_FRONT_U) | delegates | inline ensure(CPK_FRONT_U)+enTrainable[Visible] read | stable |
| CvCity::canMaintain(ProcessTypes) | CvCity (CPK_FRONT_PP) | delegates | inline ensure(CPK_FRONT_PP)+enMaintainable.count; FLAG cannotMaintain Python veto gap (Legacy-only) | stable |
| CvPlayer::canResearch(TechTypes,…) | CvPlayer (PSC_FRONT_P) | delegates | inline ensure(PSC_FRONT_P)+enResearchable read | stable |
| CvPlayer::canDoCivics(CivicTypes) | CvPlayer (PSC_FRONT_P) | delegates | inline enCivicsOk read | stable |
| CvPlayer::canTrain(UnitTypes,…) | CvPlayer | other | none — legacy eligibility gate, no cascade frontier (gap) | unknown |
| CvPlayer::canConstruct(BuildingTypes,…) | CvPlayer | other | none — own m_bCanConstruct cache, pre-#430 (gap) | unknown |
| CvPlayer::canMaintain(ProcessTypes) | CvPlayer | other | none — trivial tech-prereq check | unknown |
| CvUnit::canConstruct(CvPlot*,BuildingTypes,…) | CvUnit | other | none — unit-ability gate | unknown |
| CvPlot::canTrain(UnitTypes,…) | CvPlot | other | none — terrain/domain eligibility gate | unknown |

## Synthesis

| family | getters | needFlip | stable | breaks | warehouse |
|---|---|---|---|---|---|
| FOOD+PRODUCTION city yield (CvCity) | 9 | 8 | 8 | 0 | 0 |
| GOLD/RESEARCH/ESPIONAGE commerce (CvCity+CvPlayer) | 13 | 10 | 10 | 1 | 1 |
| Culture (COMMERCE_CULTURE + banked culture) | 7 | 2 | 7 | 0 | 5 |
| HAPPINESS+HEALTH (CvCity/CvPlayer wellbeing) | 52 | 42 | 21 | 1 | 3 |
| greatPeopleRate | 16 | 10 | 12 | 0 | 0 |
| DEFENSE (building/total/modifier, bombard, min-floor) | 31 | 21 | 31 | 0 | 4 |
| Maintenance (getMaintenance/modifier/base + components) | 28 | 21 | 20 | 0 | 0 |
| Trade Route (count + trade yield) | 6 | 5 | 4 | 0 | 1 |
| buildRate / production-cost modifiers | 20 | 19 | 11 | 0 | 0 |
| Unit Combat (strength/withdrawal/firststrike/bombard/collateral/air/capture/heal) | 8 | 8 | 8 | 0 | 0 |
| Unit non-combat (moves/visibility/workRate/cargo/experience) | 9 | 9 | 9 | 0 | 0 |
| FREE XP / FREE SPECIALIST / SPECIALIST YIELD-COMMERCE | 13 | 12 | 13 | 0 | 0 |
| PROPERTY_* (property stock + cascade source feeds) | 8 | 3 | 5 | 0 | 3 |
| ENABLER availability (canConstruct/canTrain/canResearch/canDoCivics/canMaintain) | 10 | 5 | 5 | 0 | 0 |
| **TOTAL** | **230** | **175** | **164** | **2** | **17** |

### Thin ice (contract breaks)

The full list of getters whose flip forces a consumer/signature/Python change:

1. **CvPlayer::getCommerceRate** (CvPlayer.cpp; family GOLD/RESEARCH/ESPIONAGE) —
   contract=BREAKS, HIGHEST IMPACT. `m_aiCommerceRate` is a legacy delta-accumulator pushed
   city-by-city AND written directly from many non-city sites (capital-change handlers
   ~11503/11512/12911, espionage flows, ~28538 reset). Flipping the read to a live cross-city
   sum (à la getTotalCityBaseCommerceRate) forces enumerating and re-pointing/removing every
   `changeCommerceRate` call site — the largest contract risk in the sweep. Feeds
   calculateBaseNetGold/calculateBaseNetResearch (the latter Cy-exposed via
   CyPlayer::calculateBaseNetResearch).
2. **CvCity::getCommerceHappinessPer** (CvCity.cpp:13184; family HAPPINESS+HEALTH) —
   contract=BREAKS. CommerceTypes-array-indexed (`m_aiCommerceHappinessPer`), only reached from
   Python via CyCity `getCommerceHappinessByType` (CyCity.cpp:1243). The break only materializes
   if the backing array is restructured to a differently-indexed cascade cache; if the same
   CommerceTypes index shape is preserved it degrades to stable. Lower blast radius than
   getCommerceRate, but flagged because its per-type storage shape is a Python-visible assumption.

### Clean flips

**120** contract-stable safe body-flips (getters that are `needFlip` AND `contract=stable`).

### Summary

This is a BOUNDED CLEAN BODY-FLIP, not a crusade. Of 230 getters across 14 families, 175 need
flipping and 120 of those are contract-stable safe body-flips; only 2 getters break a
consumer/signature contract. The bulk of the "delegates-to-cascade-accumulator" work is shallow
mechanical inlining — the CascadeAccumulator:: helpers (yieldRate100, commerceRate100, wellbeing,
scDefense/scGp*, buildRate*, enConstruct/enTrain/enResearch, scMaintenanceModifier, scTradeRoutes)
are ALREADY bare object-cache reads keyed on the passed object; the only "violation" is that the
summation lives in the Cascade namespace instead of in the getter body, so the fix is to move
ensure+read+live-sum into CvCity/CvPlayer/CvUnit. The ice cracks in exactly ONE structurally hard
place: CvPlayer::getCommerceRate — its m_aiCommerceRate is a push-accumulator written from many
non-city sites, so unwinding its read to a live city-sum forces auditing/removing every
changeCommerceRate writer, not just editing one body. The SECOND, milder break is
CvCity::getCommerceHappinessPer (CommerceTypes-indexed, reachable via the CyCity
getCommerceHappinessByType binding). The real friction is NOT contract breaks but the ~53 needFlip
getters marked contract=unknown — chiefly the happiness/health sub-term accumulators, maintenance
distance/numCities/corporation modifiers, and buildRate per-type maps — which are BLOCKED on
cascade channels that do not exist yet (no distance/numCities/corporation-maintenance channel, no
per-unit-type GP channel, no espionage/bonus/recovery defense fold, no free-XP/specialist-modifier
channel), so they cannot be flipped today regardless of their eventual stability.
