# Cascade modifier inventory — every flat & percent modifier

> **♻ RECOVERED + FACT-CHECKED 2026-06-17** (lost in the `Sources/docs → docs/dev` shuffle; no delete commit).
> **Keep this for its COVERAGE** — the comprehensive, sweep-verified list of every flat/percent modifier with its
> scopes + source accessors. That checklist (what the modifier system must cover) is still accurate. But its
> **framing is superseded**:
> - It's written for the #421/#423 **`CvCascadingModifierBundle`** model — a flat scalar-channel array
>   (`CASCADEFLAT_*` / `CASCADEMOD_*`) plus an "OPEN consideration" about whether the indexed families join the
>   bundle. The #428/#430 model replaced that with **modifier FAMILIES** (`modifier-cascade-spec.md` §1: split
>   families `yield→food/production/commerce`, scope-keyed `<family>.<scope>.<targetType>.{TARGET}`). The
>   indexed-families "open question" is **resolved** — families handle keyed/indexed deposits uniformly.
> - Some channels **re-classify** under the current model — e.g. `CASCADEMOD_MILITARY_PRODUCTION` /
>   `CASCADEMOD_SPACE_PRODUCTION` fold into `buildRate` (modifier-spec §6.2), not standalone production mods.
> - The `cascade-modifier-migration.md` cross-reference (bottom) is **dead** — that companion was dropped as
>   superseded; the live migration map is `migration-renames.md` + modifier-spec §9 (demolition list).
>
> **So:** mine the tables below as the authoritative *coverage checklist*; map each row to its modifier **family**
> (modifier-spec §1), not to a `CASCADEFLAT_/CASCADEMOD_` bundle slot.

The canonical, deduplicated channel inventory for the S2S cascade datastructure (#421/#423), produced by a
parallel sweep of CvPlayer / CvCity / CvTeam+CvGame accumulators and every modifier-bearing Info class
(Civic, Trait, Handicap, Building, Project, Tech, Bonus, Corporation, Religion, Promotion, Specialist). A
"channel" is the additive/multiplicative bucket a modifier writes into; many sources feed the same channel.

## Two tiers: committed scalar surface + an OPEN consideration for the indexed families
- **Committed:** the **scalar uniform channels** (56 flat + 62 percent below) are the cascade surface — a flat
  per-channel array in the bundle. These are clearly scope-wide scalars; no question they belong.
- **OPEN CONSIDERATION (owner, 2026-06-13 — explicitly NOT a decision):** whether the **indexed/conditional
  families** (sections A–N: per-yield, per-commerce, per-specialist, per-building, per-plot/terrain/improvement,
  per-bonus, per-tech…) *also* go on the same surface as **indexed rate-table channels**, instead of staying
  the consumer's local/indexed effects. Owner: "I am not saying adding cascading modifiers for those 3
  [buildings, specialists, plot yields] is the right call, but it is a consideration."
  - **FOR:** modder uniformity ("infinitely confusing if buildings and specialists magically used a different
    modifier than everything else"); more levers (any scope could grant a per-yield / per-specialist effect);
    and it would give a **clean lever to the recorded design thoughts about specialists influencing building /
    plot output** (see `specialist-rebalance.md` / migration cross-ref). Likely a read-side **perf win**
    (one composed object vs walking many accumulators — see migration §1d).
  - **COST:** the datastructure grows from scalar channels to scalars **plus indexed rate tables**
    (`yieldMod[Yield]`, `specialistYield[Specialist][Yield]`, `improvementYield[Improvement][Yield]`, …) —
    more upfront design. *Mitigant:* this reorganizes the EXISTING `m_ppaa*` storage, not new memory.
  - **Decide per-family** when migrating; the inventory (§2 scalars + §3 families) is the data for that call.
  - **If adopted:** the "percentages don't affect buildings" edge case gets **deleted, not encoded** — buildings
    use the same flat→% rule as everything, and the data is re-balanced for it (structure over parity, §1f).
- **Genuinely outside the city cascade regardless: per-UNIT promotions** (section M) — *unit* scope, not the
  Game→Team→Player→City chain. A different SCOPE, not "buildings are special"; units would need their own
  analogous structure as a separate effort.

The `uniform` column below means "scalar (single array slot)". Section 3 catalogues the indexed/conditional
families — the candidate rate tables the consideration above would add.

---

## FLAT channels (additive)

| Channel | Scopes seen | Uniform? | Sources | Notes |
|---|---|---|---|---|
| **CASCADEFLAT_HAPPINESS** | player, city, area, game, team | yes | getExtraHappiness/changeExtraHappiness (m_iExtraHappiness, m_iExtraHappinessUnattributed); getBuildingHappiness; getCivicHappiness; getWorldHappiness; getProjectHappiness; getLandmarkHappiness; getEspionageHappinessCounter; CvCivicInfo::getCivicHappiness; CvTraitInfo::getHappiness; CvHandicapInfo::getHappyBonus; Building getHappiness/getGlobalHappiness/getAreaHappiness; Project getGlobalHappiness/getWorldHappiness; CvTechInfo::getHappiness; CvBonusInfo::getHappiness; CvCorporationInfo::getHappiness | Empire/city catch-all happy bucket. Area & game scope cascade within their tiers. Conditional good+bad splits → section 3. |
| **CASCADEFLAT_LARGEST_CITY_HAPPINESS** | player, city | yes | getLargestCityHappiness; CvCivicInfo/CvTraitInfo::getLargestCityHappiness | Applied to the N largest cities; rate is scope-uniform. |
| **CASCADEFLAT_HAPPY_PER_MILITARY_UNIT** | player, city | yes | getHappyPerMilitaryUnit; CvCivicInfo/CvTraitInfo::getHappyPerMilitaryUnit | Rate scope-uniform; output scales with garrison. |
| **CASCADEFLAT_TAX_RATE_UNHAPPINESS** | player, city | yes | getTaxRateUnhappiness/calculateTaxRateUnhappiness | Output scales with the tax slider. |
| **CASCADEFLAT_CITY_OVER_LIMIT_UNHAPPY** | player, city | yes | getCityOverLimitUnhappy/changeCityOverLimitUnhappy | Unhappiness per city above the soft limit. |
| **CASCADEFLAT_HEALTH** | player, city, area, game | yes | getExtraHealth; getCivicHealth; getBuildingGoodHealth/getBuildingBadHealth; getWorldHealth; getProjectHealth; getCivilizationHealth; getFreshWaterGoodHealth; getEspionageHealthCounter; CvCivicInfo::getExtraHealth; CvTraitInfo::getHealth; CvHandicapInfo::getHealthBonus; Building getHealth/getGlobalHealth/getAreaHealth; Project getGlobalHealth/getWorldHealth; CvTechInfo::getHealth; CvBonusInfo::getHealth; CvCorporationInfo::getHealth | Empire/city catch-all health bucket. Conditional splits → section 3. |
| **CASCADEFLAT_HEAL_RATE** | city | yes | getHealRate; Building getHealRateChange | Extra unit heal rate in city. |
| **CASCADEFLAT_NUM_UNIT_FULL_HEAL** | city | yes | getNumUnitFullHeal; Building getNumUnitFullHeal | Units fully healed per turn. |
| **CASCADEFLAT_FREE_SPECIALIST** | player, city, area | yes | getFreeSpecialist; CvCivicInfo/CvTraitInfo::getFreeSpecialist; Building getFreeSpecialist/getAreaFreeSpecialist/getGlobalFreeSpecialist | Free any-type specialist slots in every city. |
| **CASCADEFLAT_FREE_EXPERIENCE** | player, city | yes | getFreeExperience; CvCivicInfo/CvTraitInfo::getFreeExperience; Building getFreeExperience/getGlobalFreeExperience; CvCorporationInfo::getFreeXP | Free XP to units built in any city. |
| **CASCADEFLAT_SPECIALIST_FREE_EXPERIENCE** | city | yes | getSpecialistFreeExperience | Flat free XP per specialist. |
| **CASCADEFLAT_TRADE_ROUTES** | player, city, game | yes | getTradeRoutes/changeTradeRoutes (player/game/team merge); getMaxTradeRoutesAdjustment; getExtraTradeRoutes (city); CvCivicInfo/CvTraitInfo::getTradeRoutes; Building getTradeRoutes/getGlobalTradeRoutes/getWorldTradeRoutes; Project getWorldTradeRoutes; CvTechInfo::getTradeRoutes | Trade-route slots per city. WIRED (this is the live channel). |
| **CASCADEFLAT_MAX_TRADE_ROUTES_CHANGE** | player | yes | CvTraitInfo::getMaxTradeRoutesChange | Trait cap adjust. |
| **CASCADEFLAT_BASE_FREE_UNIT_UPKEEP_CIVILIAN** | player | yes | getBaseFreeUnitUpkeepCivilian; CvCivicInfo/CvTraitInfo::getFreeUnitUpkeepCivilian | Free civilian upkeep count. |
| **CASCADEFLAT_BASE_FREE_UNIT_UPKEEP_MILITARY** | player | yes | getBaseFreeUnitUpkeepMilitary; CvCivicInfo/CvTraitInfo::getFreeUnitUpkeepMilitary | Free military upkeep count. |
| **CASCADEFLAT_BUILDING_INFLATION** | player | yes | getBuildingInflation | Inflation from buildings. |
| **CASCADEFLAT_PROJECT_INFLATION** | player | yes | getProjectInflation | |
| **CASCADEFLAT_TECH_INFLATION** | player | yes | getTechInflation | |
| **CASCADEFLAT_CIVIC_INFLATION** | player | yes | getCivicInflation | |
| **CASCADEFLAT_FOOD_KEPT** | city | yes | getFoodKept | Flat food carried across growth (distinct from FoodKeptPercent). |
| **CASCADEFLAT_BASE_GREAT_PEOPLE_RATE** | city | yes | getBaseGreatPeopleRate; CvTraitInfo::getGreatPeopleRateChange; Building getGreatPeopleRateChange | Flat GPP/turn/city. |
| **CASCADEFLAT_FEATURE_PRODUCTION** | city | yes | getFeatureProduction | One-shot chopped-feature production. |
| **CASCADEFLAT_AIR_UNIT_CAPACITY** | player, city | yes | getNationalAirUnitCapacity; getAirUnitCapacity/getSMAirUnitCapacity; CvTraitInfo::getGlobalAirUnitCapacity; Building getAirUnitCapacity | Air-unit basing slots. |
| **CASCADEFLAT_MAX_AIRLIFT** | city | yes | getMaxAirlift; Building getAirlift | Airlift capacity/turn. |
| **CASCADEFLAT_BUILDING_DEFENSE** | city | yes | getBuildingDefense | Flat building defense (then culture-scaled). |
| **CASCADEFLAT_BUILDING_BOMBARD_DEFENSE** | city | yes | getBuildingBombardDefense | Bombard resistance of building defense. |
| **CASCADEFLAT_EXTRA_CITY_DEFENSE** | player, city | yes | getExtraCityDefense; getTraitExtraCityDefense | Flat free city-defense strength. |
| **CASCADEFLAT_EXTRA_MIN_DEFENSE** | city | yes | getExtraMinDefense; Building getMinDefense | Min guaranteed defense floor. |
| **CASCADEFLAT_EXTRA_LOCAL_DYNAMIC_DEFENSE** | city | yes | getExtraLocalDynamicDefense | Dynamic regenerating defense (flat). |
| **CASCADEFLAT_EXTRA_RIVER_DEFENSE_PENALTY** | city | yes | getExtraRiverDefensePenalty | Across-river defense penalty (flat). |
| **CASCADEFLAT_ADJACENT_DAMAGE_PERCENT** | city | yes | getAdjacentDamagePercent; Building getAdjacentDamagePercent | Damage to adjacent enemies/turn (flat per-turn). |
| **CASCADEFLAT_EXTRA_INSIDIOUSNESS** | city | yes | getExtraInsidiousness/getSpecialistInsidiousness; Building getInsidiousness | Espionage-offense accumulator. |
| **CASCADEFLAT_EXTRA_INVESTIGATION** | city | yes | getExtraInvestigation/getInvestigationTotal/getSpecialistInvestigation; Building getInvestigation | Espionage-defense accumulator. |
| **CASCADEFLAT_LINE_OF_SIGHT** | city | yes | getLineOfSight; Building getLineOfSight | Extra city vision range. |
| **CASCADEFLAT_WORKABLE_RADIUS** | city | yes | Building getWorkableRadius | City workable-plot radius. |
| **CASCADEFLAT_REV_IDX_LOCAL** | player, city | yes | getRevIdxLocal; Building getRevIdxLocal | Revolution-index local. |
| **CASCADEFLAT_REV_IDX_NATIONAL** | player | yes | getRevIdxNational; Building getRevIdxNational | Revolution-index national. |
| **CASCADEFLAT_FREEDOM_FIGHTERS** | player | yes | changeFreedomFighterCount; getExtraFreedomFighters | Free/extra freedom-fighter strength. |
| **CASCADEFLAT_CITY_LIMIT** | player | yes | getCityLimit | Soft city-count limit. |
| **CASCADEFLAT_MAX_CONSCRIPT** | player | yes | getMaxConscript; CvCivicInfo/CvTraitInfo::getMaxConscript | Conscription capacity/turn. |
| **CASCADEFLAT_OVERFLOW_RESEARCH** | player | yes | getOverflowResearch | Carried-over research beakers. |
| **CASCADEFLAT_NATIONAL_MISSILE_RANGE** | player | yes | getNationalMissileRangeChange; CvTraitInfo::getMissileRange | Missile operational range. |
| **CASCADEFLAT_NATIONAL_FLIGHT_OPERATION_RANGE** | player | yes | getNationalFlightOperationRangeChange; CvTraitInfo::getFlightOperationRange | Air flight op range. |
| **CASCADEFLAT_NATIONAL_NAVAL_CARGO_SPACE** | player | yes | getNationalNavalCargoSpaceChange; CvTraitInfo::getNavalCargoSpace | Naval cargo space. |
| **CASCADEFLAT_NATIONAL_MISSILE_CARGO_SPACE** | player | yes | getNationalMissileCargoSpaceChange; CvTraitInfo::getMissileCargoSpace | Missile cargo space. |
| **CASCADEFLAT_CITY_START_CULTURE** | player, city | yes | getNationalCityStartCulture; CvTraitInfo::getCityStartCulture | Starting culture per new city. |
| **CASCADEFLAT_CITY_START_BONUS_POPULATION** | player, city | yes | getNationalCityStartBonusPopulation; CvTraitInfo::getBonusPopulationinNewCities | Extra starting pop per new city. |
| **CASCADEFLAT_GLOBAL_POPULATION_CHANGE** | player | yes | Building getGlobalPopulationChange | Free pop added to every owned city. |
| **CASCADEFLAT_POPULATION_CHANGE** | city | yes | Building getPopulationChange | Pop added to this city. |
| **CASCADEFLAT_MAX_POPULATION_CHANGE** | city | yes | Building getMaxPopulationChange | City population cap change. |
| **CASCADEFLAT_NUM_POPULATION_EMPLOYED** | city | yes | Building getNumPopulationEmployed | Pop consumed/employed by the building. |
| **CASCADEFLAT_FREE_TECHS** | player | yes | Building getFreeTechs | Free techs granted. |
| **CASCADEFLAT_NO_UNHAPPINESS** (enabler) | city | yes | Building isNoUnhappiness | Removes all city unhappiness (count/bool enabler). |
| **CASCADEFLAT_NO_UNHEALTHY_POPULATION** (enabler) | player, city | yes | changeNoUnhealthyPopulationCount; Building isNoUnhealthyPopulation | Removes pop-based unhealth. |
| **CASCADEFLAT_BUILDING_ONLY_HEALTHY** (enabler) | player, city | yes | changeBuildingOnlyHealthyCount; Building isBuildingOnlyHealthy | Suppress building bad health. |
| **CASCADEFLAT_NO_CAPITAL_UNHAPPINESS** (enabler) | player, city | yes | isNoCapitalUnhappiness | Removes capital unhappiness. |
| **CASCADEFLAT_MILITARY_FOOD_PRODUCTION** (enabler) | player, city | yes | changeMilitaryFoodProductionCount | Food→military-production. |
| **CASCADEFLAT_PROVIDES_FRESH_WATER** (enabler) | city | yes | Building isProvidesFreshWater | Grants fresh water. |
| **CASCADEFLAT_PROVIDES_POWER** (enabler) | city | yes | Building isPower/isDirtyPower | Provides city power. |
| **CASCADEFLAT_NO_ENEMY_PILLAGING_INCOME** (enabler) | city | yes | Building isNoEnemyPillagingIncome | Denies enemy pillage income. |
| **CASCADEFLAT_FREE_TRADE** (enabler) | game | yes | changeFreeTradeCount/isFreeTrade | Game-wide free-trade. |

> **Enabler channels** (NoUnhappiness, NoUnhealthyPopulation, BuildingOnlyHealthy, NoCapitalUnhappiness,
> MilitaryFoodProduction, ProvidesFreshWater, Power, NoEnemyPillagingIncome, FreeTrade): count/bool, not
> numeric. Cascade as uniform booleans (presence-count > 0); flag `kind=enabler` if the enum distinguishes.

---

## PERCENT channels (multiplicative %)

| Channel | Scopes seen | Sources | Notes |
|---|---|---|---|
| **CASCADEMOD_FOREIGN_UNHAPPY_PERCENT** | player, city | getForeignUnhappyPercent | Scales unhappiness from foreign tiles/culture. |
| **CASCADEMOD_WAR_WEARINESS_PERCENT_ANGER** | player, city | getWarWearinessPercentAnger/getModifiedWarWearinessPercentAnger | |
| **CASCADEMOD_WAR_WEARINESS** | player, city, team | getWarWearinessModifier; CvCivicInfo::getWarWearinessModifier; Building getWarWearinessModifier/getGlobalWarWearinessModifier | War-weariness anger multiplier. |
| **CASCADEMOD_WAR_WEARINESS_ACCUMULATION** | player | CvTraitInfo::getWarWearinessAccumulationModifier | Accrual rate. |
| **CASCADEMOD_ENEMY_WAR_WEARINESS** | player, team | getNationalEnemyWarWearinessModifier; CvTeam getEnemyWarWearinessModifier; CvTraitInfo; Building getEnemyWarWearinessModifier | Inflicted on enemies. |
| **CASCADEMOD_HURRY_ANGER** | player, city | getNationalHurryAngerModifier; getHurryAngerModifier; CvTraitInfo; Building getHurryAngerModifier | Hurry-anger duration. |
| **CASCADEMOD_GREAT_PEOPLE_RATE** | player, city | getGreatPeopleRateModifier/getTotalGreatPeopleRateModifier; CvCivicInfo/CvTraitInfo; Building getGreatPeopleRateModifier/getGlobalGreatPeopleRateModifier | GP generation %. |
| **CASCADEMOD_GREAT_PEOPLE_THRESHOLD** | player | getGreatPeopleThresholdModifier | GP cost-threshold. |
| **CASCADEMOD_GREAT_GENERAL_RATE** | player | getGreatGeneralRateModifier; CvCivicInfo/CvTraitInfo; Building getGreatGeneralRateModifier | |
| **CASCADEMOD_DOMESTIC_GREAT_GENERAL_RATE** | player | getDomesticGreatGeneralRateModifier; CvCivicInfo/CvTraitInfo; Building | |
| **CASCADEMOD_GREAT_GENERALS_THRESHOLD** | player | getGreatGeneralsThresholdModifier | |
| **CASCADEMOD_LEVEL_EXPERIENCE** | player | getLevelExperienceModifier; CvTraitInfo::getLevelExperienceModifier | XP/level. |
| **CASCADEMOD_FEATURE_PRODUCTION** | player, city | getFeatureProductionModifier; CvTechInfo | % of cleared-feature production. |
| **CASCADEMOD_MILITARY_PRODUCTION** | player, city | getMilitaryProductionModifier; CvCivicInfo/CvTraitInfo; CvCorporationInfo; Building | |
| **CASCADEMOD_SPACE_PRODUCTION** | player, city | getSpaceProductionModifier; Building getSpaceProductionModifier/getGlobalSpaceProductionModifier | |
| **CASCADEMOD_MAINTENANCE** | player, city | getMaintenanceModifier/getEffectiveMaintenanceModifier; CvTechInfo; Building getMaintenanceModifier/getGlobalMaintenanceModifier/getAreaMaintenanceModifier; Project | Overall maintenance. |
| **CASCADEMOD_DISTANCE_MAINTENANCE** | player, city | getDistanceMaintenanceModifier; CvCivicInfo/CvTraitInfo; CvHandicapInfo::getDistanceMaintenancePercent; CvTechInfo; Building/Project | |
| **CASCADEMOD_NUM_CITIES_MAINTENANCE** | player, city | getNumCitiesMaintenanceModifier; CvCivicInfo/CvTraitInfo; CvHandicapInfo::getNumCitiesMaintenancePercent; CvTechInfo; Building/Project | |
| **CASCADEMOD_CORPORATION_MAINTENANCE** | player, city, team | getCorporationMaintenanceModifier; CvTeam; CvCivicInfo/CvTraitInfo; CvHandicapInfo::getCorporationMaintenancePercent; CvTechInfo | |
| **CASCADEMOD_COASTAL_DISTANCE_MAINTENANCE** | player, city | getCoastalDistanceMaintenanceModifier; CvTechInfo; Building | |
| **CASCADEMOD_CONNECTED_CITY_MAINTENANCE** | player, city | getConnectedCityMaintenanceModifier; Building/Project | |
| **CASCADEMOD_UPKEEP** | player | getUpkeepModifier; CvTraitInfo; CvHandicapInfo::getCivicUpkeepPercent | Civic upkeep %. |
| **CASCADEMOD_CIVILIAN_UNIT_UPKEEP** | player | getCivilianUnitUpkeepMod; CvCivicInfo/CvTraitInfo; CvHandicapInfo::getUnitUpkeepPercent | |
| **CASCADEMOD_MILITARY_UNIT_UPKEEP** | player | getMilitaryUnitUpkeepMod; CvCivicInfo/CvTraitInfo | |
| **CASCADEMOD_FREE_UNIT_UPKEEP_CIVILIAN_POP_PERCENT** | player | getFreeUnitUpkeepCivilianPopPercent; CvCivicInfo/CvTraitInfo | |
| **CASCADEMOD_FREE_UNIT_UPKEEP_MILITARY_POP_PERCENT** | player | getFreeUnitUpkeepMilitaryPopPercent; CvCivicInfo/CvTraitInfo | |
| **CASCADEMOD_DISTANT_UNIT_SUPPORT_COST** | player | getDistantUnitSupportCostModifier; CvCivicInfo | |
| **CASCADEMOD_INFLATION** | player | getInflationMod10000; CvCivicInfo::getInflationModifier; CvHandicapInfo::getInflationPercent; CvTechInfo; Building/Project | Overall inflation rate. |
| **CASCADEMOD_HURRY** | player, city | getHurryModifier (m_iGlobalHurryModifier); Building getGlobalHurryModifier | Hurry output %. |
| **CASCADEMOD_HURRY_COST** | player, city | getHurryCostModifier; CvCivicInfo/CvTraitInfo; Building | Hurry gold/pop cost. |
| **CASCADEMOD_HURRY_INFLATION** | player | getHurryInflationModifier; CvCivicInfo | |
| **CASCADEMOD_WORKER_SPEED** | player | getWorkerSpeedModifier; CvCivicInfo/CvTraitInfo; CvTechInfo; Building | Worker build-rate %. |
| **CASCADEMOD_IMPROVEMENT_UPGRADE_RATE** | player | getImprovementUpgradeRateModifier; CvCivicInfo/CvTraitInfo | |
| **CASCADEMOD_CITY_DEFENSE** | player, city | getCityDefenseModifier; CvCivicInfo::getExtraCityDefense; CvTraitInfo::getCityDefenseBonus; Building getDefenseModifier/getAllCityDefenseModifier | City defense % (distinct from flat ExtraCityDefense). |
| **CASCADEMOD_BOMBARD_DEFENSE** | player, city | getNationalBombardDefenseModifier; CvTraitInfo::getBombardDefense; Building getBombardDefenseModifier | |
| **CASCADEMOD_AIR** | city | getAirModifier; Building getAirModifier | Air-combat-in-city. |
| **CASCADEMOD_NUKE** | city | getNukeModifier; Building getNukeModifier | Nuke-damage-vs-city. |
| **CASCADEMOD_ESPIONAGE_DEFENSE** | player, city | getNationalEspionageDefense; getEspionageDefenseModifier; CvTraitInfo; Building | |
| **CASCADEMOD_CITY_DEFENSE_RECOVERY_SPEED** | city | getExtraCityDefenseRecoverySpeedModifier/getExtraBuildingDefenseRecoverySpeedModifier; Building | + cap. |
| **CASCADEMOD_LOCAL_DYNAMIC_DEFENSE** | city | Building getLocalDynamicDefense | (flat counterpart EXTRA_LOCAL_DYNAMIC_DEFENSE) |
| **CASCADEMOD_RIVER_DEFENSE_PENALTY** | city | Building getRiverDefensePenalty | (flat counterpart EXTRA_RIVER_DEFENSE_PENALTY) |
| **CASCADEMOD_OCCUPATION_TIME** | city | Building getOccupationTimeModifier | Post-capture occupation duration. |
| **CASCADEMOD_LOCAL_CAPTURE_PROBABILITY** | city | getExtraLocalCaptureProbabilityModifier; Building | |
| **CASCADEMOD_LOCAL_CAPTURE_RESISTANCE** | city | getExtraLocalCaptureResistanceModifier; Building | |
| **CASCADEMOD_NATIONAL_CAPTURE_PROBABILITY** | player | getExtraNationalCaptureProbabilityModifier; CvCivicInfo/CvTraitInfo; Building | |
| **CASCADEMOD_NATIONAL_CAPTURE_RESISTANCE** | player | getExtraNationalCaptureResistanceModifier; CvCivicInfo/CvTraitInfo; Building | |
| **CASCADEMOD_ENSLAVEMENT_CHANCE** | player | getEnslavementChance | |
| **CASCADEMOD_TRADE_ROUTE_PROFIT** | city, team | getTradeRouteModifier; CvTeam getTradeModifier; CvTechInfo::getGlobalTradeModifier; Building | Domestic trade-route profit %. |
| **CASCADEMOD_FOREIGN_TRADE_ROUTE_PROFIT** | city, team | getForeignTradeRouteModifier; CvTeam; CvTechInfo; Building | Foreign trade-route profit %. |
| **CASCADEMOD_TRADE_MISSION** | team | getTradeMissionModifier; CvTechInfo | Great-merchant trade-mission gold %. |
| **CASCADEMOD_CORPORATION_REVENUE** | team | getCorporationRevenueModifier; CvTechInfo | |
| **CASCADEMOD_CORPORATION_SPREAD** | player, city | getCorporationSpreadModifier; CvCivicInfo::getCorporationSpreadRate | |
| **CASCADEMOD_RELIGION_SPREAD** | player, city | getReligionSpreadRate; CvCivicInfo::getReligionSpreadRate | |
| **CASCADEMOD_UNIT_UPGRADE_PRICE** | player | getUnitUpgradePriceModifier; CvTraitInfo; Building | |
| **CASCADEMOD_ANARCHY** | player | getAnarchyModifier; Building getAnarchyModifier | Anarchy length. |
| **CASCADEMOD_CIVIC_ANARCHY** | player | getCivicAnarchyModifier; CvTraitInfo::getCivicAnarchyTimeModifier | |
| **CASCADEMOD_RELIGIOUS_ANARCHY** | player | getReligiousAnarchyModifier; CvTraitInfo::getReligiousAnarchyTimeModifier | |
| **CASCADEMOD_GOLDEN_AGE** | player | getGoldenAgeModifier; CvTraitInfo::getGoldenAgeDurationModifier; Building | Golden-age length. |
| **CASCADEMOD_POPULATION_GROWTH_RATE** | player, city | getPopulationgrowthratepercentage; CvCivicInfo; CvTraitInfo::getGlobalPopulationgrowthratepercentage; Building | Food/growth threshold. |
| **CASCADEMOD_FOOD_KEPT_PERCENT** | city | getFoodKeptPercent; Building getFoodKept | % stored food kept on growth. |
| **CASCADEMOD_HAPPINESS_PERCENT_PER_POPULATION** | city | calculatePopulationHappiness/changeHappinessPercentPerPopulation | |
| **CASCADEMOD_HEALTH_PERCENT_PER_POPULATION** | city | calculatePopulationHealth/changeHealthPercentPerPopulation | |
| **CASCADEMOD_RESEARCH_COST** | player | CvTechInfo::getResearchCost; CvHandicapInfo research-cost scalars | Base tech acquisition cost. (Per-tech research-mod → §3 G.) |
| **CASCADEMOD_REV_IDX_DISTANCE** | player, city | getRevIdxDistanceModifier; getRevIndexDistanceMod; Building | |
| **CASCADEMOD_REV_IDX_NATIONALITY** | player, city | getRevIdxNationalityMod | |
| **CASCADEMOD_REVOLUTION_INDEX** | player | CvHandicapInfo::getRevolutionIndexPercent | Difficulty revolution-index. |
| **CASCADEMOD_PILLAGE_GOLD** | player | Building getPillageGoldModifier | |
| **CASCADEMOD_CONQUEST_PROBABILITY** | city | Building getConquestProbability | Chance a building survives conquest. |
| **CASCADEMOD_NUKE_INTERCEPTION** | player | Project getNukeInterception | |
| **CASCADEMOD_VICTORY_DELAY** | game | Project getVictoryDelayPercent | |
| **CASCADEMOD_ATTITUDE_SHARE** | player | CvCivicInfo::getAttitudeShareMod | |
| **CASCADEMOD_CIVIC_PERCENT_ANGER** | player | CvCivicInfo::getCivicPercentAnger | Anger to players running a different civic. |
| **CASCADEMOD_CAPITAL_XP** | player, city | getCapitalXPModifier; CvTraitInfo::getCapitalXPModifier | Capital-only XP %. |
| **CASCADEMOD_EXP_IN_BORDER** | player | getExpInBorderModifier; CvCivicInfo/CvTraitInfo | XP for combat in own borders. |

> Foreign-trade-route / capital-XP / exp-in-border are location-conditional but a single scope-wide rate
> (condition evaluated at apply). Animal/Barbarian combat handicap modifiers are foe-type-conditional → §3 M.

---

## 3. Indexed / conditional families — candidate rate tables (the OPEN consideration)

These are the families the consideration above would add as indexed rate-table channels (vs leaving them the
consumer's local/indexed effects). Listing them here gives the per-family decision its data. Grouped by the
index/condition dimension:

- **A. Per-YIELD** — YieldRateModifier (player & city), Capital/Power/Bonus/Building/Base/Area/Global YieldModifier, Civic/Trait YieldModifier/CapitalYieldModifier/TradeYieldModifier; flat per-yield: Sea/River/FreeCity/GoldenAge/Landmark/Extra plot yields, SpecialistYield, BaseYieldPerPop, Building/Trait/Corp/Bonus/Specialist YieldChange.
- **B. Per-COMMERCE** — CommerceRateModifier (+FromEvents/FromBuildings), Capital/Bonus/Building CommerceModifier, ProductionToCommerce, Civic/Trait/Building/Project CommerceModifier; flat: ExtraCommerce100, State-religion/GoldenAge/Building/Specialist/Religion/Corp Commerce, CommerceHappiness, CommerceFlexible.
- **C. Per-SPECIALIST / per-(specialist,yield|commerce) 2D** — SpecialistExtraYield/Commerce, Specialist*PercentChanges, LocalSpecialist*, FreeSpecialistCount (per-specialist, all scopes), EraAdvanceFreeSpecialistCount, ImprovementFreeSpecialists, MaxSpecialistCount, Team BuildingSpecialistChange, Building TechSpecialistChange.
- **D. Per-BUILDING (and 2D building×yield/commerce)** — ExtraBuildingHappiness/Health/Yield, BuildingProduction/Cost Modifier, BuildingCommerceModifier/Change, BuildingYieldChange/TechChange/Modifier, Max{Global,Team,Player}BuildingProductionModifier, Civic/Trait/Building global variants.
- **E. Per-UNIT / per-UNITCOMBAT / per-DOMAIN** — Unit/UnitCombat/Domain ProductionModifier & FreeExperience, UnitCombatDefenseAgainst/ExtraStrength, DamageAttackingUnitCombat, HealUnitCombatType, GreatPeopleUnitRate, GreatGeneralPointsForType, UnitExtraCost, Team/Tech DomainExtraMoves, Specialist UnitCombatExperienceType.
- **F. Per-BONUS / per-RESOURCE (with-bonus)** — BonusCommerceModifier (2D), BonusMintedPercent, CorpBonusProduction, FreeBonus(es), BonusDefenseChanges, Building Bonus{Happiness,Health,Yield,Commerce*,Production,Aid}/Vicinity/PowerBonus, Trait BonusHappiness, Bonus YieldChange.
- **G. Per-TECH (with-tech)** — NationalTechResearchModifier/Trait TechResearchModifiers (per-tech research cost), Tech{Happiness,Health,Yield,Commerce,Specialist}* changes/modifiers, Team TechExtraBuildingHappiness, Building/Specialist Tech* variants.
- **H. Per-TERRAIN/FEATURE/PLOT/IMPROVEMENT/ROUTE (worked-plot)** — TerrainYieldChange, PlotYieldChange, ImprovementYieldChange (player/city/team, +Global), FeatureHappiness, Improvement Happiness/Health/UpgradeRate, BuildWorkerSpeed (per-build), Team RouteChange, Building Terrain/Plot/Improvement YieldChanges.
- **I. Per-RELIGION / state-religion gated** — State/NonState ReligionHappiness, StateReligion GreatPeopleRate/FreeExperience/Unit&Building ProductionModifier/HolyCityXP, religion spread mods, Religion Good/Bad Happiness, Religion Commerce/Influence, RevIdx HolyCity/Religion mods, Religion/Building/Project state-religion commerce, Promotion ReligiousCombatModifier.
- **J. Capital / area / golden-age / power / coastal conditional scalars** — Capital{Yield,Commerce}RateModifier, Home/Other AreaMaintenanceModifier, Colony maintenance, PowerYieldRateModifier, CoastalTradeRoutes, GoldenAge Yield/Commerce. (Several also appear in A/B/J.)
- **K. Per-PROPERTY / aid (TB property system)** — AidRate/ExtraBonusAidModifier (per-property, per (bonus,property)), Building AidRateChange/BonusAidModifier, Properties/PropertiesAllCities.
- **L. Per-CIVIC / diplo** — CivicAttitudeChange (per-civic), SharedCivicTradeRouteModifier (partner-civic).
- **M. Foe-type combat + per-UNIT PROMOTIONS** — Animal/Barbarian combat (handicap, vs-foe). **The entire
  Promotion family is per-UNIT, a different SCOPE — NOT the Game→Team→Player→City cascade.** Promotions stay
  on `CvPromotionInfo`/`CvUnit`; if unit-scope ever gets its own analogous bundle that's a separate effort.
  This is the only family genuinely outside the city cascade — and it's a scope difference, not an exception.
- **N. Per-population-scaled rate** — BaseYieldPerPopRate, CommercePerPopFromBuildings, Building Yield/CommercePerPopChange (rate scope-uniform, indexed per yield/commerce → folded into A/B).

---

## Counts
**FLAT scalar channels: 56** (incl. 9 enablers) · **PERCENT scalar channels: 62** · **Indexed/conditional
families: 14** (A–N), of which only **M's promotion sub-family is genuinely off the city cascade** (unit scope).

This is the set `CascadeFlatTypes` / `CascadeModifierTypes` (scalars) + the indexed rate-table channels must
cover. It is the checklist for the per-source migration (cascade-modifier-migration.md §1f).
