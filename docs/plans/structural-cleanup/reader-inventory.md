# Cascade read-path reader inventory

The exhaustive reader→(scope, channel, backing) inventory for the cascade read-path rework — every consumer that reads the FINAL modifier-influenced value of each channel family, classified `cascade` / `legacy` / `mixed` / `unknown`. Generated 2026-07-13 from a 15-agent fan-out sweep (13 of 14 trace families completed; `traderoutes` errored — see Known gap).

## FOOD + PRODUCTION city yields

Getters: getYieldRate100 / getBaseYieldRate / getPlotYield; food growth, production progress, AI valuations.

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getYieldRate100 @CvCity.cpp:11429 | city | FOOD/PROD (any) | mixed | canonical final getter; post-init→cascade, pre-init/load→legacy |
| CascadeAccumulator::yieldRate100→acc_yieldCombine @CvCascadeAccumulator.cpp:387 | city | FOOD/PROD | cascade | (plots+trade+flat)*(100+yPctCity)+extra; the cascade authority |
| CvCity::getYieldRate100Legacy @CvCity.cpp:11419 | city | FOOD/PROD | legacy | load-path only, dead in play |
| CvCity::getYieldRate @CvCity.cpp:11403 | city | FOOD/PROD | mixed | =rate100/100; workhorse |
| CvCity::foodDifference @CvCity.cpp:6122 | city | FOOD | mixed | getYieldRate(FOOD)-foodConsumption; drives growth |
| CvCity growth doTurn @CvCity.cpp:1371 changeFood | city | FOOD | mixed | per-turn growth/starvation mutation |
| CvCity::getProductionPerTurn @CvCity.cpp:4079 | city | PROD(+FOOD) | legacy | GAP: never calls cascade; yPctCity not applied to banked shields |
| CvCity::getProduction/CurrentProductionDifference @CvCity.cpp:4092,4100 | city | PROD+FOOD | legacy | wraps getProductionPerTurn |
| CvCity::doProduction @CvCity.cpp:17057 | city | PROD | legacy | advances build queue (actual mutation) |
| CvCity::getBaseYieldRate @CvCity.cpp:23437 | city | FOOD/PROD | legacy | pre-modifier base |
| CvCity::getPlotYield @CvCity.cpp:11441 | city | FOOD/PROD | legacy | sums worked plots; feeds both cascade & legacy |
| CvPlot::getYield/calculateYield/recomputeYieldInto @CvPlot.cpp:8179,8208,8384 | plot | FOOD/PROD | legacy | dirty-flag cache; one cascade carve-out (operatingBuildings) |
| CvCity::getBaseYieldRateModifier @CvCity.cpp:11379 | city | FOOD/PROD | legacy | all legacy modifier members, no yPctCity |
| CvCityAI::AI_yieldValueInternal @CvCityAI.cpp:9938 | city | FOOD/PROD marginal | legacy | citizen/plot-assignment AI on legacy stack |
| CvCityAI::AI_plotValue @CvCityAI.cpp:10490 | plot | FOOD/PROD | mixed | never sees yPctCity |
| CvCityAI::AI_yieldMultiplier @CvCityAI.cpp:11373 | city | FOOD/PROD/COMMERCE | legacy | process/building weighting |
| CvCityAI::AI_getYieldMultipliers @CvCityAI.cpp:8097 | city | FOOD/PROD | legacy | heuristic multipliers off raw plots |
| CvCityAI::AI_foodAvailable etc @CvCityAI.cpp:836,1003,5235,12898 | city | FOOD | mixed | inherits getYieldRate |
| CvCityAI::AI_cityValue @CvCityAI.cpp:10865 | city | PROD | cascade | directly getYieldRate100 |
| CvCityAI production comparisons @CvCityAI.cpp:1641,2664,... | city | PROD | mixed | wide AI heuristics via getYieldRate |
| CvPlayer::calculateTotalYield @CvPlayer.cpp:7822 | empire | FOOD/PROD | mixed | sums getYieldRate100 across cities |
| CvPlayerAI empire food/prod aggregations @CvPlayerAI.cpp:9824,13300,... | empire | FOOD/PROD | mixed | via per-city getYieldRate/foodDifference |
| CyCity::getYieldRate/getPlotYield/... @CyCity.cpp:297,475,... | city | FOOD/PROD | mixed | Python passthrough |
| CvGameTextMgr production tooltip @CvGameTextMgr.cpp:5576,5643 | city | PROD | legacy | GAP: duplicates legacy combine inline |
| CvGameTextMgr city-yield breakdown @CvGameTextMgr.cpp:25921,25924,25998 | city | FOOD/PROD/COMMERCE | legacy | sums legacy components directly |
| CvGameTextMgr food/prod display @CvGameTextMgr.cpp:5453,25131,26171,26764 | city | FOOD | mixed | inherits foodDifference/getYieldRate |
| CvHttpServer /computed/cities/yields @CvHttpServer.cpp:2627-2724 | city | FOOD/PROD(+COMM) | cascade | diagnostic; intentionally side-by-side legacy inputs |
| CvHttpServer /computed/cities food group @CvHttpServer.cpp:3218 | city | FOOD | mixed | diagnostic |
| CvWorkerAI plot-improvement scoring @CvWorkerAI.cpp:520,1151,1153 | plot | FOOD/PROD | legacy | build-choice scores raw plot deltas |
| CvCity::getFoodTurnsLeft @CvCity.cpp:3141 | city | FOOD | mixed | growth-turns display |

## COMMERCE yield + GOLD/RESEARCH/ESPIONAGE

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getYieldRate100/getYieldRate(COMMERCE) @CvCity.cpp:11429 | city | COMMERCE total | mixed | post-init→cascade, pre-init→legacy |
| CvCity::getCommerceRateTimes100/getCommerceRate @CvCity.cpp:12147,12161 | city | gold/research/culture/espionage | mixed | post-init→CascadeAccumulator::commerceRate100 |
| CvCity::getCommerceRateAtSliderPercent @CvCity.cpp:12245 | city | gold/research/esp | legacy | live legacy recompute; feeds updateCommerce & calculateMinMaxTax |
| CvCity::updateCommerce @CvCity.cpp:12354 | city→player | gold/research/culture/esp | legacy | sole feeder of CvPlayer legacy accumulator; bypasses cascade |
| CvPlayer::getCommerceRate @CvPlayer.cpp:13104 | empire | gold/research/culture/esp | legacy | m_aiCommerceRate; empire aggregate is legacy |
| CvPlayer::calculateBaseNetGold @CvPlayer.cpp:8117 | empire | gold | legacy | |
| CvPlayer::calculateGoldRate @CvPlayer.cpp:8260 | empire | gold | legacy | |
| CvPlayer::calculateBaseNetResearch @CvPlayer.cpp:8239 | empire | research | legacy | |
| CvPlayer::calculateResearchRate @CvPlayer.cpp:8270 | empire | research | legacy | |
| CvPlayer::calculateTotalCommerce @CvPlayer.cpp:8280 | empire | gold+research+culture+esp | legacy | feeds economy-history UI |
| CvPlayer::doGold @CvPlayer.cpp:15577 | empire | gold | legacy | actual treasury mutation |
| CvPlayer::doResearch @CvPlayer.cpp:15612 | empire/team | research | legacy | drives team beaker pool |
| CvPlayer::doEspionagePoints @CvPlayer.cpp:15685 | empire | espionage | legacy | feeds team espionage points |
| CvPlayer::doEspionageOneOffPoints @CvPlayer.cpp:15687 | empire→team | espionage | unknown | other call sites (wonders/events) unenumerated |
| CvCityAI (multiple) @CvCityAI.cpp:1366,10873,11031,... | city | gold/research/culture/esp | mixed | cascade post-init; 14621 reads legacy empire path |
| CvPlayerAI AI_* gold/research/esp heuristics @CvPlayerAI.cpp:3985,5015,... | city+empire | gold/research/culture/esp | mixed | city-scope cascade, empire-scope legacy |
| CvCityAI::AI_baseCommerceRate consumers @CvCityAI.cpp:14428, CvPlayerAI 21432,... | city | gold/research/culture/esp (base) | legacy | getBaseCommerceRate, pre-slider/pre-modifier |
| CvPlayer::getTotalCityBaseCommerceRate @CvPlayer.cpp:13115 | empire | base bucket | legacy | cached sum of city base |
| CvCity::getCommerceFromPercent @CvCity.cpp:12174 | city | gold/research/esp (slider sub-term) | legacy | reads cascade yieldRate100 then applies slider itself |
| CvGameTextMgr commerce tooltip @CvGameTextMgr.cpp:5780-5900 | city UI | COMMERCE + channels | mixed | cascade for rate lines, legacy for base-values branch |
| CvGameTextMgr setCommerceHelp/asserts @CvGameTextMgr.cpp:25662,25751,25894 | city UI/debug | gold/research/culture/esp | mixed | shadow-parity check legacy vs cascade |
| CvDLLWidgetData city widget @CvDLLWidgetData.cpp:5230 | city UI | culture | cascade | getCommerceRateTimes100 post-init |
| CvHttpServer diagnostics @CvHttpServer.cpp:447,3046,3576,... | city+empire | gold/research/esp(+culture) | mixed | city cascade truth vs legacy empire reads |
| CvGame doTurn multiplier loop @CvGame.cpp:8274 | empire (all players) | gold/research/culture/esp | legacy | getCommerceRate per type |
| CvCity::doTurn doPlotCulture caller @CvCity.cpp:1377 | city | culture | cascade | getCommerceRate(CULTURE) on hot path |
| CvCity::doTurn rank-cache/asserts @CvCity.cpp:20662,1908 | city | gold/research/culture/esp | cascade | rank comparators + FASSERT |
| CyCity::getCommerceRate/Times100/getBaseCommerceRate @CyCity.cpp:1178-1190 | city | gold/research/culture/esp | mixed | rate cascade, base legacy |
| CyPlayer::getCommerceRate/calculate* @CyPlayer.cpp:463-485,1150 | empire | gold/research/culture/esp | legacy | wraps legacy accumulator functions |
| CvGameTextMgr research-turns tooltip @CvGameTextMgr.cpp:11912 | empire | research | legacy | calculateResearchRate for turns-to-tech |

## CULTURE

No readers traced. The trace agent returned an empty set for this family — this is flagged in Synthesis › Missing as the single largest completeness hole, not a confirmed "no readers exist" result.

## HAPPINESS + HEALTH (city wellbeing)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::happyLevel @CvCity.cpp:5725 | city | happyLevel | cascade | post-init→wellbeing(0)+live military term; pre-init legacy |
| CvCity::unhappyLevel(iExtra=0) @CvCity.cpp:5730 | city | unhappyLevel | mixed | iExtra==0→cascade; nonzero what-if→legacy (never cascaded) |
| CvCity::goodHealth @CvCity.cpp:5736 | city | goodHealth | cascade | wellbeing(2) bare fetch; pre-init legacy |
| CvCity::badHealth(bNoAngry,iExtra=0) @CvCity.cpp:5741 | city | badHealth | mixed | default→cascade; bNoAngry/iExtra→legacy |
| CvCity::angryPopulation(iExtra=0) @CvCity.cpp:5872 | city | angryPopulation | mixed | cascade only when iExtra==0 (AI often passes nonzero) |
| CvCity::healthRate(bNoAngry,iExtra=0) @CvCity.cpp:6028 | city | healthRate | mixed | goodHealth cascade, badHealth flips |
| CvCity::foodConsumption @CvCity.cpp:6064 | city | food consumption | mixed | composes angryPopulation+healthRate |
| CvCity::isDisorder @CvCity.cpp:5406 | city | disorder gate | legacy | pure raw-state flag, gates many reads |
| CvCity::totalGood/BadBuildingHealth/unhealthyPop/visiblePop @CvCity.cpp:5949,5959,5939,5880 | city | building-health subtotals | legacy | legacy-only; leaks into doGlobalWarming + UI/API |
| CvCity::getBuildingBadHealth/getBonusBadHealth @CvCity.cpp:8474 | city | legacy sub-accumulators | legacy | bare members; feed doGlobalWarming |
| CvCityAI (~35+ sites) @CvCityAI.cpp:770,796,...,15328 | city | happy/unhappy/health composites | mixed | mostly nonzero-iExtra what-if→legacy; few default→cascade |
| CvPlayerAI (~20 sites) @CvPlayerAI.cpp:13126,...,30764 | empire | per-city verdicts folded to empire | mixed | same per-city split, summed across cities |
| CvUnitAI isDisorder gate @CvUnitAI.cpp:24544 | city | isDisorder | legacy | war-attack decision gate |
| CvOutcome::apply/getValue @CvOutcome.cpp:1429 | city | happy-unhappy compare | mixed | happyLevel cascade, unhappyLevel(1) legacy |
| CvGameObjectCity isTag/getAttribute @CvGameObject.cpp:811,896,919,922 | city | disorder/health/happiness | mixed | event/outcome tag+attribute system |
| CvGame checksum/statistics loop @CvGame.cpp:8295 | world | verdicts per city | cascade | default-arg calls; MP determinism checksum |
| CvGame::doGlobalWarming @CvGame.cpp:6684 | world | totalBadBuildingHealth+bonus | legacy | real gameplay effect off legacy accumulators |
| CvDLLWidgetData city widget @CvDLLWidgetData.cpp:3039,5026 | city | happy/health deltas | mixed | default→cascade, isDisorder legacy |
| CvGameTextMgr help builders @CvGameTextMgr.cpp:5465,19256,19450,20019,... | city | full family | mixed | default→cascade; sub-detail lines legacy |
| CvHttpServer JSON endpoints @CvHttpServer.cpp:447,1807,2297,... | city | full family + decomposition | mixed | 'realized' cascade lines + raw legacy leaves |
| CyCity bindings isDisorder/unhappyLevel/... @CyCity.cpp:402-472 | city | same set | mixed | passthrough; mod nonzero-args silently get legacy |
| CvCity DECLARE_MAP_FUNCTOR_CONST verdict functors @CvCity.h:2146 | city | four verdicts | cascade | no live call site found (possibly dead) |

## greatPeopleRate

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getBaseGreatPeopleRate @CvCity.cpp:7298 | city | gpRate.base | mixed | post-init→scGpBase+scGpNational |
| CvCity::getBaseGreatPeopleRateLegacy @CvCity.cpp:7308 | city | gpRate.base | legacy | net oracle; called by diagnostics |
| CvCity::getGreatPeopleRate @CvCity.cpp:7314 | city | gpRate | mixed | isDisorder→0 else base*modifier |
| CvCity::getTotalGreatPeopleRateModifier @CvCity.cpp:7324 | city | gpRate.modifier | mixed | post-init→scGpModifier (re-derives religion/GA gates live) |
| CvCity::getTotalGreatPeopleRateModifierLegacy @CvCity.cpp:7331 | city | gpRate.modifier | legacy | |
| CvCity::getGreatPeopleRateModifier @CvCity.cpp:7357 | city | gpRate.modifier.city | legacy | raw member, never flipped |
| CvPlayer::getGreatPeopleRateModifier @CvPlayer.cpp:9770 | empire | gpRate.modifier.player | legacy | no flip; read by AI + UI + Python even post-init |
| CvPlayer::getStateReligionGreatPeopleRateModifier @CvPlayer.cpp:9806 | empire | gpRate.modifier.SR | legacy | raw member |
| CvPlayer::getNationalGreatPeopleRate @CvPlayer.cpp:30028 | empire | gpRate.national | legacy | raw member |
| CascadeAccumulator::scGpBase @CvCascadeAccumulator.cpp:438 | city | gpRate.base.city | cascade | reads m_cascadeCityPackages |
| CascadeAccumulator::scGpNational @CvCascadeAccumulator.cpp:611 | empire | gpRate.national | cascade | derived-trait replacement |
| CascadeAccumulator::scGpModifier @CvCascadeAccumulator.cpp:446 | city | gpRate.modifier | cascade | +live SR/golden-age |
| CvCity::getGreatPeopleUnitRate @CvCity.cpp:14230 | city | gpRate.unitRate | legacy | GAP: no flip; feeds doGreatPeople |
| CvPlayer::getNationalGreatPeopleUnitRate @CvPlayer.cpp:30021 | empire | gpRate.unitRate.national | legacy | no cascade path |
| CvCity::doGreatPeople @CvCity.cpp:17412 | city | gpRate + unitRate | mixed | flipped aggregate + unflipped per-unit rate |
| CvCityAI::AI_specialistValue (GP block) @CvCityAI.cpp:858-955 | city/empire | modifier.player + gpRate | mixed | legacy player getter × mixed city getter |
| CvCityAI::AI_buildingValue (two sites) @CvCityAI.cpp:6132,12884 | city | modifier.city + base | mixed | static XML × live getBaseGreatPeopleRate |
| CvPlayerAI::AI_civicValue @CvPlayerAI.cpp:13225 | empire | modifier.civic | unknown | reads static CivicInfo value, not realized channel |
| CvPlayerAI::AI_updateAverageGreatPeopleMultiplier @CvPlayerAI.cpp:23333 | empire | gpRate.modifier | mixed | aggregates mixed city getter into legacy cache |
| CvGameTextMgr GP help/hover @CvGameTextMgr.cpp:5829-5919,26314-26535 | city/empire | gpRate + base + modifier.* | mixed | mixed city + legacy player getters; FAssert parity |
| CyCity::getBaseGreatPeopleRate/getGreatPeopleRate/getGreatPeopleRateModifier @CyCity.cpp:639 | city | gpRate + base + modifier.city | mixed | passthrough |
| CyPlayer::getGreatPeopleRateModifier @CyPlayer.cpp:775 | empire | modifier.player | legacy | passthrough to raw member |
| CvHttpServer diagnostics @CvHttpServer.cpp:1912,3453 | city/empire | all sub-channels | mixed | prefers *Legacy bodies for breakdown math |

## DEFENSE (city building/bombard/min defense)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getBuildingDefense @CvCity.cpp:10114 | city | scDefense | mixed | post-init→cascade, load→legacy |
| CvCity::getBuildingDefenseLegacy @CvCity.cpp:10122 | city | building defense | legacy | raw member; net oracle + HTTP |
| CvCity::getBuildingBombardDefense @CvCity.cpp:10141 | city | scBuildingBombardDefense | mixed | cascade city+player capped; legacy fallback |
| CvCity::getExtraMinDefense @CvCity.cpp:23511 | city | scDefenseMin | mixed | post-init→cascade |
| CvPlayer::getCityDefenseModifier @CvPlayer.cpp:9976 | empire | scDefensePlayer | mixed | post-init→defPlayerAll |
| CvCity::getTotalDefense @CvCity.cpp:10402 | city | composed | mixed | buildingDefense+naturalDefense+player+bonusDefense (legacy legs) |
| CvCity::getDefenseModifier @CvCity.cpp:10408 | city | final realized % | mixed | THE canonical final getter; decayed by legacy defenseDamage |
| CvCity::getNaturalDefense @CvCity.cpp:10392 | city | culture-level bonus | legacy | static CultureLevelInfo value |
| CvCity::calculateBonusDefense @CvCity.cpp:20938 | city | per-bonus defense | legacy | m_bonusDefenseChanges map; no cascade |
| CvCity::getDefenseDamage/changeDefenseDamage @CvCity.cpp:10332,10338 | city | war-damage decay | legacy | raw member |
| CvCity::getMinimumDefenseLevel @CvCity.cpp:22520 | city | siege min floor | mixed | legacy member vs cascade getExtraMinDefense |
| CvCity::isBombardable @CvCity.cpp:10381 | city | defenseModifier vs minDefense | mixed | bombard gate |
| CvCity::cityDefenseRecoveryRate @CvCity.cpp:23571 | city | defenseModifier gate | mixed | recovery-speed cap |
| CvCity::getAdditionalDefenseByBuilding @CvCity.cpp:21212 | city | projected delta | mixed | live baseline + static deltas |
| CvPlot::defenseModifier @CvPlot.cpp:4432 | plot(+city) | terrain+city composite | mixed | bridge into unit combat odds |
| CvUnit::updateCombat modifier calc @CvUnit.cpp:~11700 | unit | iPlotDefenseModifier | mixed | actual combat-odds input |
| CvUnit isEnemy/bombard helpers @CvUnit.cpp:7478,16933,... | city (from unit) | defenseModifier/bombard/total | mixed | bombard math + attack gating |
| CvSelectionGroup bombard/siege estimate @CvSelectionGroup.cpp:3221 | city | bombard/total/damage | mixed | siege duration estimate |
| CvUnitAI targeting/valuation @CvUnitAI.cpp:3636,10015,... | city (from unit-AI) | defenseModifier/damage/total | mixed | siege-target & attack pre-filters |
| CvCityAI::AI_buildingValue-family @CvCityAI.cpp:5441,13139 | city | building.defenseModifier + city | mixed | static field × live readers |
| CvPlayerAI city-threat valuation @CvPlayerAI.cpp:3249 | city (player-loop) | defenseModifier/damage | mixed | defense-need scoring |
| CvGameTextMgr::getDefenseHelp @CvGameTextMgr.cpp:29589 | city | full breakdown | mixed | live parity-check (FAssertMsg) |
| CvGameTextMgr::buildCityBillboardIconString @CvGameTextMgr.cpp:26597 | city | defenseModifier/minDefense | mixed | city-bar icon |
| CvGameTextMgr::getPlotHelp @CvGameTextMgr.cpp:28648 | city (mouse-over) | minDefense/defenseModifier | mixed | tooltip |
| CvGameTextMgr::setUnitHelp bombard @CvGameTextMgr.cpp:452,3297 | city | bombard defense | mixed | unit-help tooltip |
| CvHttpServer 'CH.4 DEFENSE' @CvHttpServer.cpp:3085 | city(+player) | full breakdown | mixed | cascade + raw legacy side by side |
| CvHttpServer worldbuilder block @CvHttpServer.cpp:1930 | city/player | scDefense* vs legacy | mixed | cascade-vs-legacy compare |
| CyCity bindings getBuildingDefense/... @CyCity.cpp:946 | city | above getters | mixed | passthrough (minDefense/bombard not exposed) |
| CvBuildingFilters isDefensiveBuilding @CvBuildingFilters.cpp:282 | n/a | buildingInfo.getDefenseModifier | legacy | static XML field |
| CvDLLWidgetData improvement defense help @CvDLLWidgetData.cpp:3160 | plot | improvement defenseModifier | legacy | static field + legacy decay |

## MAINTENANCE (city maintenance + empire modifiers)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getMaintenance/getMaintenanceTimes100 @CvCity.cpp:7738 | city | city.total | mixed | legacy base × mixed effective modifier |
| CvCity::updateMaintenance @CvCity.cpp:7784 | city | city.total | mixed | realizes final per-city value |
| CvCity::getEffectiveMaintenanceModifier @CvCity.cpp:7756 | city | effectiveModifier | mixed | #430 flip: post-init→scMaintenanceModifier |
| CvCity::getEffectiveMaintenanceModifierLegacy @CvCity.cpp:7763 | city | effectiveModifier | legacy | city+player+area+connected |
| CascadeAccumulator::scMaintenanceModifier @CvCascadeAccumulator.cpp:518 | city | effectiveModifier | cascade | city+player+area-pct+connected gate |
| CvCity::calculateDistanceMaintenance* @CvCity.cpp:7802 | city (reads empire) | city.distance | legacy | player distance/coastal modifiers, no cascade |
| CvCity::calculateNumCitiesMaintenance* @CvCity.cpp:7865 | city | city.numCities | legacy | numCities + vassal loop |
| CvCity::calculateColonyMaintenance* @CvCity.cpp:7933 | city | city.colony | legacy | handicap cap; no cascade |
| CvCity::calculateCorporationMaintenance* @CvCity.cpp:7973 | city (+player/team) | city.corporation | legacy | player+team legacy corp modifiers |
| CvCity::calcCorporateMaintenance→CvPlayer::getCorporateMaintenance @CvCity.cpp:22306,CvPlayer.cpp:27837 | city→empire | city.corporation | legacy | corp-tax path for gold expense |
| CvCity::calculateBuildingMaintenanceTimes100 @CvCity.cpp:8045 | city | city.building | legacy | per-building sum |
| CvCity::calculateBaseMaintenanceTimes100 @CvCity.cpp:8067 | city | city.base | legacy | sum of 5 legacy components |
| CvCity::getMaintenanceModifier @CvCity.cpp:8083 | city | city.flatModifier | legacy | raw member |
| CvCity::getSavedMaintenanceTimes100ByBuilding @CvCity.cpp:7607 | city | city.total | legacy | building-delta (uses mixed modifier) |
| CvCity::get*MaintenanceSavedTimes100ByCivic @CvCity.cpp:7645-7736 | city | city.* civic delta | mixed | before/after using mixed modifier |
| CvPlayer::updateMaintenance @CvPlayer.cpp:10704 | empire | empire.total | mixed | sums city getMaintenanceTimes100 |
| CvPlayer::getTotalMaintenance @CvPlayer.cpp:10809 | empire | empire.total | mixed | lazy wrapper |
| CvPlayer::getMaintenance/Coastal/Connected/CorporationModifier @CvPlayer.cpp:10718 | empire | empire.* sub-modifiers | legacy | plain accessors |
| CvArea::getTotalAreaMaintenanceModifier/getMaintenanceModifier @CvArea.cpp:788 | area | area.modifier | legacy | m_aiMaintenanceModifier; only via legacy path |
| CvTeam::getCorporationMaintenanceModifier @CvTeam.cpp:6877 | team | team.corporation | legacy | plain accessor |
| CvPlayer::calculatePreInflatedCosts @CvPlayer.cpp:7985 | empire | empire.total | mixed | folds getTotalMaintenance+getCorporateMaintenance |
| CvPlayer::getFinalExpense/calculateGoldRate/getProfitMargin @CvPlayer.cpp:8050-8267 | empire | empire.total | mixed | gold-per-turn chain |
| CvPlayerAI AI_buildingValue/civic heuristics @CvPlayerAI.cpp:14655,25400 | empire | empire.corporation | legacy | reads corp modifier directly |
| CvPlayerAI inflation-cost/CvCityAI unit-cost @CvPlayerAI.cpp:13290,CvCityAI.cpp:1212 | empire | empire.total | mixed | via preInflatedCosts |
| CvTeamAI @CvTeamAI.cpp:2485 | team/empire | empire.total | mixed | per-member preInflatedCosts |
| CvCityAI building-evaluation @CvCityAI.cpp:5940,13640 | city | city.total | mixed | prices maintenance-reducing buildings |
| CvPlayerAI::AI_cityValue financial-trouble @CvPlayerAI.cpp:9851,9934 | city | city.total | mixed | gold − maintenance net-loss check |
| CvPlayerAI building-value across cities @CvPlayerAI.cpp:5607 | city→empire | city.total | mixed | sums over 5 cities |
| CvGameTextMgr financial-advisor @CvGameTextMgr.cpp:24980 | empire UI | city.* + empire.total | mixed | loops cities via mixed modifier |
| CvGameTextMgr city maintenance line @CvGameTextMgr.cpp:5853 | city UI | city.total | mixed | base (legacy) vs total (mixed) |
| CvGameTextMgr civic-help previews @CvGameTextMgr.cpp:10439,15337,25291 | city→empire UI | city.* civic/building delta | mixed | saved-maintenance deltas |
| CvGameTextMgr corp/tech/trait hover @CvGameTextMgr.cpp:7416,10472,12077 | world | static.corporationModifier | legacy | static info values |
| CvDLLWidgetData hover @CvDLLWidgetData.cpp:5015 | city UI | city.* + total + effModifier | mixed | also recomposes legacy directly at 5081 |
| CvHttpServer maintenance diagnostics @CvHttpServer.cpp:1969,3120,3578 | city/empire | multiple | mixed | legacy + mixed side by side |
| CvBuildingSort::getBuildingValue @CvBuildingSort.cpp:75 | city UI | city.total | mixed | adds savedMaintenanceByBuilding to sort |
| CyCity::getMaintenance*/calculate* @CyCity.cpp:699 | city | city.total/distance/numCities | mixed | passthrough |
| CyPlayer::getTotalMaintenance/getCorporate/preInflated/finalExpense @CyPlayer.cpp:453 | empire | empire.total/corporation | mixed | passthrough |
| CvCorporationInfo::getMaintenance @CvCorporationInfo.h:39 | world | static.corporationDefine | legacy | static define |

## buildRate / production-COST (item discount)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getProductionModifier(Unit/Building/Project) @CvCity.cpp:3963,4015,4051 | city | buildRate discount | mixed | THE entry point; post-init→cascade branch |
| CascadeAccumulator::buildRateUnit/Building/Project @CvCascadeAccumulator.cpp:821,847,866 | city | buildRate discount | cascade | ledger + realized package fields |
| CvCity::getProductionModifierLegacy(...) @CvCity.cpp:3971,4022,4058 | city | buildRate discount | legacy | pre-cascade oracle |
| CvPlayer::getProductionModifier(...) @CvPlayer.cpp:7226,7265,7316 | empire | discount (empire component) | legacy | NO cascade counterpart; always legacy walk |
| CvCity::getHurryCostModifier/getHurryCost @CvCity.cpp:6191,6221 | city | discount→hurry cost | mixed | via getProductionModifier; feeds canHurry + AI whip |
| CvCity::getProductionNeeded(Unit/Building/Project) @CvCity.cpp:3715,3720,3725 | city | final cost after discount | mixed | wide fan-out (turns-left, canTrain, UI, Python) |
| CvCity::getMilitary/SpaceProductionModifier @CvCity.cpp:10034,10046 | city | military/space sub-channel | legacy | never cascaded; AI reads directly |
| CvPlayer::getMilitary/SpaceProductionModifier @CvPlayer.cpp:9952,9964 | empire | military/space sub-channel | legacy | parallel-to-cascade; AI + Python read raw |
| CvCity::getDomainProductionModifier @CvCity.cpp:13299 | city(+player) | domain sub-channel | legacy | cascade uses own ledger; AI reads raw |
| CvCity::getUnitCombatProductionModifier @CvCity.cpp:23394 | city | unit-combat sub-channel | legacy | cascade uses ledger; AI reads raw |
| CvCity::getUnitProductionModifier/getBuildingProductionModifier @CvCity.cpp:20867,20898 | city | per-item map sub-channel | legacy | feeds legacy oracle only |
| CvPlayer::getUnitProductionModifier/getBuildingProductionModifier @CvPlayer.cpp:26548,26486 | empire | per-item map sub-channel | legacy | feeds legacy oracle only |
| CvHttpServer diagnostic @CvHttpServer.cpp:3424-3554,2010-2078 | city+empire | discount + legacy sub-fields | mixed | live mixed getter + 'Leg' components |
| CvUnit::upgradePrice @CvUnit.cpp:10337 | empire | OUT OF FAMILY (base cost only) | legacy | uses player getProductionNeeded, no discount |
| CvGameTextMgr trait/civic/building help sites | n/a (info-object) | source XML values | unknown | reads raw info modifiers for tooltips |
| CvPlayerAI civic-choice heuristics @CvPlayerAI.cpp:13263,13506,13530 | n/a (info-object) | source XML values | unknown | scores candidate civic's raw modifier |

## Unit Combat (strength/withdrawal/firstStrike/bombard/collateral/air/capture/heal)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvUnit::currCombatStr/maxCombatStr/air* @CvUnit.cpp:11464,12192,12415 | unit | combat strength/% | legacy | no cascade anywhere in path |
| CvCombatModel resolveCombat/etc @CvCombatModel.cpp:35,70,271 | unit | strength/firstStrikes/withdrawal | legacy | actual combat resolution consumer |
| CvUnitAI AI_attackOdds/pillage/etc @CvUnitAI.cpp:1305,3348,... | unit | strength/firstStrikes/withdrawal/collateral | legacy | reads legacy getters directly |
| CvUnitAI heal-decision @CvUnitAI.cpp:13377 | unit | heal rate | legacy | healRate(plot())>10 |
| CvSelectionGroupAI stack-combat @CvSelectionGroupAI.cpp:496,525,... | unit | strength/collateral | legacy | AI_compareStacks |
| CvPlayerAI combat-value heuristics @CvPlayerAI.cpp:1453,10992,... | empire | bombard/collateral/withdrawal/firstStrike/capture | legacy | info-level rates + AI weight tables |
| CvUnit::defenderValue @CvUnit.cpp:3822 | unit | collateral | legacy | weights defender selection |
| CvUnit collateral-strike resolution @CvUnit.cpp:20949 | unit | collateral damage/limit/maxUnits | legacy | actual in-combat application |
| CvUnit::airCombatDamage/rangeCombatDamage consumers @CvUnit.cpp:21396,22861 | unit | air/bombard damage | legacy | strike resolution |
| CvUnit::doHeal @CvUnit.cpp:6488 | unit | heal rate | legacy | actual heal mutation |
| CvUnit bombard + CvSelectionGroup rate @CvUnit.cpp:7407,23164 | unit | bombard rate + city bombard-def | mixed | legacy rate × mixed getBuildingBombardDefense |
| CvCity::getBuildingBombardDefense/Legacy @CvCity.cpp:10141 | city | bombard defense | mixed | post-init cascade; load legacy |
| CvUnit::withdrawalHP @CvUnit.cpp:11279 | unit | withdrawal (HP-scaled) | legacy | separate legacy computation |
| CvUnit::currInterception/evasion/maxInterception @CvUnit.cpp:12941 | unit | interception | legacy | info + legacy extras |
| CvUnit::captureProbabilityTotal @CvUnit.cpp:26325 | unit | capture probability | legacy | 3 scopes folded; no confirmed gameplay consumer |
| CvGameTextMgr unit/promotion help @CvGameTextMgr.cpp:1237,1837,... | unit | ~every channel | mixed | UI display; re-sums raw getters (drift risk) |
| CvUnitSort::bombardRate @CvUnitSort.cpp:65 | unit | bombard rate | legacy | static info base only |
| CyUnit bombardRate/captureProbabilityTotal/etc @CyUnit.cpp:291,646 | unit | bombard/capture/etc | legacy | passthrough |
| CvHttpServer diagnostic @CvHttpServer.cpp:1933,3504 | city | bombard (cascade vs legacy), capture raw | mixed | parity/telemetry |

## Unit non-combat (movement/vision/workRate/cargo/XP)

Entire family confirmed ZERO cascade representation (grep of Sources/Cascade for these keywords empty).

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvUnit::maxMoves @CvUnit.cpp:10800 | unit | MOVEMENT | legacy | baseMoves cache; legacy accumulators |
| CvUnit::movesLeft @CvUnit.cpp:10812 | unit | MOVEMENT | legacy | max(0,maxMoves-getMoves) |
| CvUnit::canMove/hasMoved @CvUnit.cpp:10817,10822 | unit | MOVEMENT | legacy | gate logic |
| CvPlot::movementCost @CvPlot.cpp:4488 | plot | MOVEMENT | legacy | terrain/feature/route + team; cached |
| CvSelectionGroup::movesLeft @CvSelectionGroup.cpp:5027 | group | MOVEMENT | legacy | min over units |
| CvGameCoreUtils pathfinding @CvGameCoreUtils.cpp:1202,... | unit/plot | MOVEMENT | legacy | A* path build |
| CvUnit::changeMoves call sites @CvUnit.cpp:~3200-4972 | unit | MOVEMENT | legacy | decrements per step |
| CvSelectionGroupAI move-order @CvSelectionGroupAI.cpp:242 | group | MOVEMENT | legacy | group move order |
| CvUnitAI/CvHunterAI/CvPlayerAI/CvWorkerAI move-planning | unit/plot | MOVEMENT | legacy | move-budget heuristics (not enumerated) |
| CvGameTextMgr move tooltip @CvGameTextMgr.cpp:559 | unit | MOVEMENT | legacy | movesLeft text |
| CyUnit::movesLeft @CyUnit.cpp:161 | unit | MOVEMENT | legacy | passthrough |
| CvUnit::visibilityRange @CvUnit.cpp:10775 | unit | VISION | legacy | terrain elevation + extras |
| CvPlot::seeFromLevel @CvPlot.cpp:2549 | plot | VISION | legacy | improvement + team flag |
| Fog-of-war/visibility propagation | plot/team/world | VISION | unknown | consumer not traced (gap) |
| CvUnit::workRate(bMax) @CvUnit.cpp:27867 | unit | workRate | legacy | base + legacy percent modifiers |
| CvUnitAI worker-task planning | unit | workRate | legacy | build ETA (not enumerated) |
| CvCityAI/CvPlayerAI worker-need | city/empire | workRate | legacy | worker-build valuation |
| CvUnit::cargoSpace @CvUnit.cpp:13275 | unit | cargo | legacy | info + legacy members |
| CvUnit::isFull @CvUnit.cpp:13318 | unit | cargo | legacy | |
| CvUnit::cargoSpaceAvailable @CvUnit.cpp:13328 | unit | cargo | legacy | loading/boarding |
| CvUnitAI cargo-loading logic | unit | cargo | legacy | transport assignment (not enumerated) |
| CvUnit::getExperience100/getExperience @CvUnit.cpp:14507,14598 | unit | XP | legacy | raw member; mod on write |
| CvUnit::changeExperience100 @CvUnit.cpp:14527 | unit(+player GG) | XP | legacy | applies legacy modifier percents |
| CvUnit::getExperiencePercent @CvUnit.h:1234 | unit | XP modifier | legacy | body not opened; inferred |
| CvPlayerAI GG/promotion heuristics | unit/empire | XP | legacy | not enumerated |
| CvGameTextMgr/CvMessageData XP display | unit | XP | legacy | tooltips + combat popups |
| CyUnit XP wrappers | unit | XP | legacy | passthrough (names not confirmed) |

## Free XP + Free/Max Specialist counts + specialist output

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::getFreeExperience @CvCity.cpp:10180 | city | city.freeXP | legacy | bare member |
| CvPlayer::getFreeExperience @CvPlayer.cpp:9854 | empire | empire.freeXP | legacy | bare member |
| CvCity::getDomainFreeExperience @CvCity.cpp:13281 | city+empire | city.domainFreeXP | legacy | city + national |
| CvCity::getUnitCombatFreeExperience @CvCity.cpp:14601 | city | city.unitCombatFreeXP | legacy | array read |
| CvCity::getSpecialistFreeExperience @CvCity.cpp:14641 | city | city.specialistFreeXP | legacy | bare member |
| CvCity::getProductionExperience @CvCity.cpp:3278 | city→unit | unit training XP | legacy | sums all free-XP channels; grants XP to trained units |
| CvCity::doGrowth unit-training @CvCity.cpp:3351 | unit | unit XP grant | legacy | stamps free XP onto trained/conscript unit |
| CyCity::getProductionExperience @CyCity.cpp:192 | city | unit training XP | legacy | passthrough |
| CvCityAI::AI_bestUnitAI valuation @CvCityAI.cpp:4438 | city | unit training XP | legacy | unit-choice value |
| CvCityAI naval/ship XP heuristic @CvCityAI.cpp:10577 | city | training XP + domain XP | legacy | naval production valuation |
| CvGameTextMgr unit help @CvGameTextMgr.cpp:14741 | city UI | unit training XP | legacy | build tooltip |
| CvGameTextMgr city/civic help @CvGameTextMgr.cpp:14288,14301 | city+empire UI | city/empire freeXP | legacy | direct reads |
| CvHttpServer /computed audit @CvHttpServer.cpp:3553-3557 | city+empire | freeXP + production XP | legacy | audit endpoint reads legacy directly |
| CvCity::getMaxSpecialistCount() @CvCity.cpp:14419 | city | maxSpecialistTotal | legacy | totalFreeSpecialists + pop − angry − employed |
| CvCity::getMaxSpecialistCount(Specialist) @CvCity.cpp:14424 | city | maxSpecialistPerType | legacy | array read |
| CvCity::isSpecialistValid @CvCity.cpp:14430 | city | maxSpecialistPerType/total | legacy | citizen-assignment gate |
| CvCity::getFreeSpecialistCount @CvCity.cpp:14497 | city | freeSpecialistPerType | legacy | city+player+team rollup |
| CvCity::getAddedFreeSpecialistCount @CvCity.cpp:14503 | city | freeSpecialistUnattributed | legacy | bare member |
| CvPlayer::getFreeSpecialistCount @CvPlayer.cpp:26886 | empire | freeSpecialistPerType | legacy | pushed down to cities |
| CvTeam::getFreeSpecialistCount @CvTeam.cpp:7124 | team | freeSpecialistPerType | legacy | propagates to member cities |
| CvCity::getYieldBySpecialist @CvCity.cpp:11366 | city | specialist yield per-unit | mixed | player accumulators' backing unresolved |
| CvCity::getAdditional*GreatPeopleRateBySpecialist @CvCity.cpp:7495 | city | specialist GP-rate delta | legacy | BUG preview math |
| CvCity::getAdditional*CommerceRateBySpecialist @CvCity.cpp:12858 | city | specialist commerce delta | legacy | building-preview commerce |
| CvCity::getExtraYield (specialist loop) @CvCity.cpp:11218 | city | yield from free specialists | legacy | building-preview extra-yield |
| CvCity::getExtraCommerce (specialist loop) @CvCity.cpp:12630 | city | commerce from free specialists | legacy | mirrors yield case |
| YieldBasePackages::specialist @CvCascadeYieldBasePackages.cpp:63 | city(+empire) | specialist yield/commerce base | mixed | cascade term × LEGACY free-specialist count input |
| CascadeWellbeing specialist block @CvCascadeWellbeing.cpp:379 | city | wellbeing specialist flats | mixed | legacy count × cascade package |
| CascadeScalarChannels::gpBaseSpecialists @CvCascadeScalarChannels.cpp:146 | city | gpRate specialist base | mixed | legacy count × cascade flat |
| CvCityAI::AI_buildingValue specialist valuations @CvCityAI.cpp:648,5978,... | city | free/max/force specialist | legacy | numerous AI heuristics |
| CvPlayerAI building/tech/civic valuation @CvPlayerAI.cpp:4776,13659,15587 | empire | free-specialist value | legacy | static info + legacy runtime |
| CvGameTextMgr specialist-count help @CvGameTextMgr.cpp:4566,5977,... | city UI | free/max specialist | legacy | tooltips |
| CvDLLWidgetData city widget @CvDLLWidgetData.cpp:3584 | city UI | maxSpecialistPerType | legacy | tooltip |
| CvGameObject specialist count read @CvGameObject.cpp:695 | city | specialist total | legacy | state-export path |
| CyCity::getFreeExperience/getMax/getFreeSpecialistCount @CyCity.cpp:949,1448,1478 | city | freeXP + counts | legacy | passthrough |
| CyPlayer::getFreeExperience @CyPlayer.cpp:785 | empire | freeXP | legacy | passthrough |
| CvCity::totalFreeSpecialists @CvCity.cpp | city | freeSpecialistTotal | legacy | helper (body not read; see gaps) |

## PROPERTY_* (crime/pollution/disease/education, CvPropertySolver)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::checkPropertyBuildings @CvCity.cpp:1532 | city | all PROPERTY_* | legacy | pseudobuilding-band mechanism; slated for cascade replacement |
| CvPropertySolver::doTurn @CvPropertySolver.cpp (CvGame.cpp:6011) | world | all PROPERTY_* | legacy | actual per-turn solver; no cascade |
| CvCity::getProperties/getPropertiesConst @CvCity.cpp:7381 | city | all PROPERTY_* | legacy | canonical per-city legacy store (+player/team/game/plot/unit) |
| CvCity::getGlobalSourcedProperty @CvCity.cpp:23376 | city | all PROPERTY_* | legacy | walks legacy source list |
| CvCityAI::getPropertySourceValue @CvCityAI.cpp:14784 | city | crime/pollution/disease/etc | legacy | scores hypothetical property change |
| CvCityAI::getPropertyDecay/getPropertyNonBuildingSource @CvCityAI.cpp:14843,14880 | city | decay/attr-const sources | legacy | reads legacy source instances |
| CvCity::getPropertyNeed @CvCity.cpp:24006 | city | PROPERTY_* AIWeight≠0 | legacy | AI need heuristic |
| CvCityAI::AI_choosePropertyControlBuildingAndUnit @CvCityAI.cpp:14934 | city | source-drain PROPERTY_* | legacy | AI property-control build/train choices |
| CvPlayerAI::AI_getTotalProperty @CvPlayerAI.cpp:23479 | empire | all PROPERTY_* | legacy | sums per-city legacy |
| CvUnitAI (commented-out) @CvUnitAI.cpp:29120 | unit/city | n/a | legacy | dead code, for completeness |
| CvCity prereq checks @CvCity.cpp:2832,19285 | city | prereq min/max bands | legacy | gates construction/event triggers |
| CvPlayer prereq checks @CvPlayer.cpp:6815,23123 | empire | prereq min/max bands | legacy | building/trigger prereqs |
| CvCascadeConditionEval::ev_countCore PROPERTY_ branch @CvCascadeConditionEval.cpp:183 | city | PROPERTY_* as count atom | mixed | KEY COUPLING: cascade gating reads LEGACY property value |
| CascadeProperty::citySourceFlat/cityUnitFlat/cityDecayPercent @CvCascadeProperty.cpp:27 | city | all PROPERTY_* | cascade | full parallel recompute, NOT wired to gameplay |
| CvHttpServer wellbeing/property audit @CvHttpServer.cpp:1834 | city | all PROPERTY_* | mixed | ONLY consumer of CascadeProperty; audit-only |
| CvHttpServer city/building property dumps @CvHttpServer.cpp:357,653,3535 | city/empire | all PROPERTY_* + bands | legacy | debug/telemetry |
| CyCity::getProperties @CyCity.cpp:1724 + CyPropertiesInterface | city | all PROPERTY_* | legacy | Python UI screens |
| CvGameTextMgr display builders @CvGameTextMgr.cpp:2060,17541,30006 | unit/plot/city/player/team/game | all PROPERTY_* + bands | legacy | tooltip text |
| CvBuildingSort::getValue/CvBuildingFilters @CvBuildingSort.cpp:187,CvBuildingFilters.cpp:300 | city UI | authored deltas + predicted impact | legacy | build-list ranking |
| IntExpr evaluator PROPERTY branch @IntExpr.cpp:404 | expr object | all PROPERTY_* | legacy | XML formulas reference legacy value |
| CvOutcome/CvOutcomeMission grants @CvOutcome.cpp:1138,CvOutcomeMission.cpp:204 | city/player | authored deltas | legacy | one-shot mutation into legacy store |
| CvCity::doPropertyUnitSpawn @CvCity.cpp:24127 | city | all PROPERTY_* | legacy | spawn-on-threshold (crime→criminal) |

## Enabler availability (canConstruct/canTrain/canResearch/canDoCivics/canMaintain)

| reader (file:function) | scope | channel | backing | note |
|---|---|---|---|---|
| CvCity::canConstruct @CvCity.cpp:2528 | city | enConstruct/Visible | mixed | default-shape+post-init→cascade; any what-if arg→legacy |
| CvCity::canConstructLegacy/Internal @CvCity.cpp:2554,2613 | city | legacy hand gates | legacy | oracle + m_bCanConstruct cache |
| CvPlayer::canConstruct/Internal @CvPlayer.cpp:6534,6593 | empire | legacy hand gates | legacy | NO cascade; player-leg gate |
| CvCity::canTrain(Unit) @CvCity.cpp:2373 | city | enTrain/Visible | mixed | default shape→cascade else legacy |
| CvCity::canTrainLegacy/Internal @CvCity.cpp:2389 | city | legacy + cache | legacy | CAN_TRAIN_CACHING |
| CvPlayer::canTrain @CvPlayer.cpp:6395 | empire/team | legacy hand gates | legacy | no cascade |
| CvCity::canTrain(UnitCombat) @CvCity.cpp:2485 | city | enTrain via per-unit loop | mixed | inherits per-call backing |
| CvCity::canMaintain @CvCity.cpp:3119 | city | enMaintain/Visible | mixed | post-init→cascade else legacy+Python veto |
| CvCity::canMaintainLegacy @CvCity.cpp:3130 | city/player | legacy + Python hook | legacy | player canMaintain + cannotMaintain veto |
| CvPlayer::canResearch @CvPlayer.cpp:8327 | team | enResearch | mixed | only bRightNow&&special&&post-init→cascade; else legacy |
| CvPlayer::canResearchLegacy @CvPlayer.cpp:8339 | team | legacy hand gates | legacy | |
| CvPlayer::canDoCivics @CvPlayer.cpp:8497 | empire | enCivic | mixed | eCivic≠NO&&post-init→cascade else legacy |
| CvPlayer::canDoCivicsLegacy @CvPlayer.cpp:8508 | empire | legacy hand gates | legacy | |
| CascadeAccumulator::enConstruct/enTrain/enMaintain/enResearch/enCivic @CvCascadeAccumulator.h:83 | city/player/team | cascade frontier | cascade | set-membership against GATE result |
| CvCityAI production choice AI_best* @CvCityAI.cpp:4553-4700 | city | canConstruct | mixed | default shape |
| CvCityAI building-value scoring @CvCityAI.cpp:5110,6394,... | city/empire | canConstruct | mixed | some kOwner (legacy), some this-city (mixed) |
| CvCityAI unit value/build lists @CvCityAI.cpp:4396,12268,... | city | canTrain | mixed | 12268 bTestVisible→cascade Visible |
| CvCityAI civic gating @CvCityAI.cpp:6125,13851 | empire | canDoCivics | mixed | kOwner |
| CvCityAI::AI_bestProcess/processValue @CvCityAI.cpp:7002,7011 | city | canMaintain | mixed | |
| CvPlayerAI::AI_bestTech @CvPlayerAI.cpp:4140,4199,4226 | team/player | canResearch (bRightNow=false) | legacy | primary tech loop legacy despite flip |
| CvPlayerAI::AI_averageCurrentTechValue @CvPlayerAI.cpp:4307 | team | canResearch | mixed | 4319 legacy, 4320 cascade |
| CvPlayerAI::findStartTech @CvPlayerAI.cpp:4519 | team | canResearch | cascade | default shape post-init |
| CvPlayerAI::AI_chooseResearch @CvPlayerAI.cpp:6407,6419 | team | canResearch | cascade | default shape |
| CvPlayerAI::AI_bestReligiousTech @CvPlayerAI.cpp:33082,33095 | team | canResearch (bRightNow=false) | legacy | |
| CvPlayerAI::AI_bestCivic @CvPlayerAI.cpp:13092,13102 | empire | canDoCivics | cascade | default |
| CvPlayerAI::AI_doCivics @CvPlayerAI.cpp:17033,17134,17342 | empire | canDoCivics | cascade | default |
| CvPlayerAI::AI_doDiplo favorite-civic @CvPlayerAI.cpp:17609,18142 | empire | canDoCivics | cascade | |
| CvPlayerAI::AI_techBuildingValue @CvPlayerAI.cpp:5545,5762,5810 | city/empire | canConstruct | mixed | what-if legacy + default cascade |
| CvPlayerAI::AI_baseBonusVal @CvPlayerAI.cpp:9051,9226,9249 | empire | canConstruct | legacy | bIgnoreCost=true breaks cascade guard |
| CvPlayerAI::bestBuildableUnitForAIType @CvPlayerAI.cpp:26330,26411 | empire | canConstruct (bIgnoreCost) | legacy | |
| CvUnitFilters @CvUnitFilters.cpp:54 | city/player | canTrain(bTestVisible) | mixed | feeds Python build-list |
| CvBuildingFilters @CvBuildingFilters.cpp:58 | city/player | canConstruct(bTestVisible) | mixed | feeds Python build-list |
| CvGameTextMgr civilopedia/tooltip @CvGameTextMgr.cpp:12383,15802,... | city/player | canConstruct/Train/Research/Civics | mixed | mostly Visible cascade; a few bRightNow=false legacy |
| CyCity::canConstruct/canTrain/canMaintain @CyCity.cpp:142,147,157 | city | passthrough | mixed | backing per Python args |
| CyPlayer::canResearch/canDoCivics @CyPlayer.cpp:489,514 | team/empire | passthrough | mixed | backing per args |
| CvMessageData @CvMessageData.cpp:479 | team | canResearch(false) | legacy | diplomacy tech-trade |

## Synthesis

328 modifier-reader sites across 13 families (12 populated; CULTURE empty). Backing skews heavily legacy/mixed: only ~19 pure-cascade readers vs ~150 pure-legacy and ~130 mixed.

| family | total | cascade | legacy | mixed |
|---|---|---|---|---|
| FOOD + PRODUCTION city yields | 30 | 3 | 14 | 13 |
| COMMERCE (gold/research/culture/espionage) | 29 | 3 | 17 | 8 |
| CULTURE | 0 | 0 | 0 | 0 |
| HAPPINESS + HEALTH (city wellbeing) | 22 | 4 | 5 | 13 |
| greatPeopleRate | 23 | 3 | 9 | 10 |
| DEFENSE (city building/bombard/min defense) | 30 | 0 | 6 | 24 |
| MAINTENANCE (city + empire modifiers) | 38 | 1 | 16 | 21 |
| buildRate / production-COST (item discount) | 16 | 1 | 9 | 4 |
| Unit Combat (strength/withdrawal/firstStrike/bombard/collateral/air/capture/heal) | 19 | 0 | 15 | 4 |
| Unit non-combat (movement/vision/workRate/cargo/XP) | 27 | 0 | 26 | 0 |
| Free XP + Free/Max Specialist counts + specialist output | 36 | 0 | 32 | 4 |
| PROPERTY_* (crime/pollution/disease/etc, CvPropertySolver) | 22 | 1 | 19 | 2 |
| Enabler availability (canConstruct/canTrain/canResearch/canDoCivics/canMaintain) | 36 | 6 | 12 | 18 |

### Missing (adversarial completeness)

- **CULTURE family COMPLETELY UNMAPPED** (0 readers). Absent: getCulture/getCultureTimes100, getCommerceRate(CULTURE) as first-class rate, CvPlot culture accumulation, doPlotCulture, culture-level thresholds (CultureLevelInfo), culture pressure / revolt-risk (RevIdx), cultural-defense, free culture, culture-per-turn on buildings, cultural-victory progress, and every AI/UI/Python/HTTP consumer. Largest single completeness hole.
- **SAVE / SERIALIZATION readers omitted entirely** — CvCity/CvPlayer/CvTeam/CvArea/CvGame read/write restore the legacy accumulator members that back every family; a load-time divergence surface with zero coverage.
- **TRADE ROUTE channels** — getTradeYield / m_aiTradeYield (only named as input to getBaseYieldRate), getTradeRoutes/getTradeYieldModifier, trade-route commerce; no family, no readers.
- **INFLATION** — calculateInflationRate/getInflationModifier/getInflationRate feeds gold-per-turn; flagged in maintenance gaps, no reader family.
- **UNIT UPKEEP / SUPPLY / CIVIC UPKEEP** — getFinalUnitUpkeep, calculateUnitSupply, getCivicUpkeep, getTreasuryUpkeep summed into preInflatedCosts/getFinalExpense; unmapped.
- **GREAT GENERAL / GREAT COMMANDER rate** — getGreatGeneralRateModifier, getDomesticGreatGeneralRateModifier, getExpInBorderModifier, GG points/threshold, changeFractionalCombatExperience; referenced only as XP-write inputs.
- **ESPIONAGE beyond commerce channel** — points-against-team, spy detection/interception, espionage defense/resistance, getEspionageDefenseModifier; only per-turn commerce feed mapped; doEspionageOneOffPoints (wonders/events) unenumerated.
- **WAR WEARINESS / anger channels** — getWarWearinessModifier, war-weariness percent, conscription/hurry/draft anger (getHurryAngerModifier); feed unhappiness, unmapped.
- **BONUS (resource) yield/commerce/health/happiness modifiers** — getBonusYieldRateModifier, bonus commerce, getBonusHappiness/getBonusHealth (getBonusBadHealth appears only in doGlobalWarming); no consolidated family.
- **Player-scope SPECIALIST OUTPUT accumulators UNRESOLVED** — CvPlayer::getExtraSpecialistYield and getSpecialistYieldPercentChanges (make getYieldBySpecialist 'mixed'), plus getExtraSpecialistCommerce/getSpecialistExtraCommerce/getLocalSpecialistExtraCommerce; legacy-vs-cascade backing never confirmed.
- **GOLDEN AGE effects** — golden-age yield/commerce flats and GOLDEN_AGE_GREAT_PEOPLE_MODIFIER read live in several paths, but the owner-ruled member-mirror carve-out has no reader entries.
- **CvTeam-scope readers thin/absent** — team research progress, team espionage points, getMaxTeamBuildingProductionModifier, possible CvTeam::canResearch (research is team-stored).
- **CORPORATION output (not maintenance)** — corporation yield/commerce from bonuses, corporation spread; only maintenance mapped.
- **Assets/Python .py UI/BUG screens** are the TRUE Cy* consumers (CityScreen, TechChooserScreen, CivicsScreen, CvMainInterface, CvDomesticAdvisor, financial advisor, BUG mods) — out of the searched Sources tree; all downstream Python readers asserted by inference.
- **CvUnitAI.cpp / CvSelectionGroupAI.cpp / CvTeamAI.cpp only sampled** (30k+ line files); many combat/maintenance/commerce/specialist/enabler AI call sites unenumerated.
- **AI 'mixed' classifications are pattern-grouped, not line-verified** — happiness/health (angryPopulation(iExtra) with runtime-variable iExtra), enabler (arg-shape), greatPeople sites bucketed heuristically; real runtime backing per site unresolved.
- **CascadeAccumulator orchestration bodies** (commerceRate100, enConstruct/enTrain/enMaintain impls) not read line-by-line — possible hidden legacy fallbacks unverified.
- **INTERCEPTION / air-combat** (currInterceptionProbability, evasionProbability) listed under Unit Combat; fog-of-war/vision propagation consumer flagged unknown & unmapped.
- **captureProbabilityTotal has NO confirmed live gameplay consumer** (only UI + Python); city-capture resolution never searched to confirm the channel is applied.
- **Info-layer source getters** (Building/Unit/Civic/Trait/Corporation/Tech/Event *Modifier* getters) are the write/source side; ~30+ CvGameTextMgr and CvPlayerAI sites read them raw and were only partially catalogued (marked 'unknown').

### Biggest legacy gaps

- **PRODUCTION BANKING is legacy while display is cascade** — getProductionPerTurn/getCurrentProductionDifference/doProduction combine getBaseYieldRate×getBaseYieldRateModifier and NEVER call CascadeAccumulator::yieldRate100, so yPctCity is silently NOT applied to shields actually banked, though it IS applied to getYieldRate100(PRODUCTION) shown elsewhere. Highest-impact see-vs-build divergence.
- **EMPIRE COMMERCE is fully legacy** — CvCity::updateCommerce recomputes via legacy getCommerceRateAtSliderPercent into m_aiCommerceRate; getCommerceRate → calculateGold/Research/TotalCommerce → doGold/doResearch/doEspionagePoints all legacy. Entire per-turn empire economy bypasses commerceRate100 while city getters are cascade.
- **UNIT COMBAT 100% legacy, ZERO cascade** — currCombatStr/maxCombatStr, withdrawalProbability/withdrawalHP, firstStrikes, collateralDamage, air/rangeCombatDamage, captureProbabilityTotal, healRate/doHeal; consumed by CvCombatModel + all AI odds.
- **UNIT NON-COMBAT 100% legacy, ZERO cascade** — maxMoves/movesLeft/movementCost, visibilityRange/seeFromLevel, workRate, cargoSpace, getExperience/changeExperience100/getExperiencePercent; confirmed zero Sources/Cascade hits.
- **PROPERTY_* gameplay fully legacy** — CvPropertySolver::doTurn + all CvProperties reads run legacy every turn; CascadeProperty::* exists but wired ONLY to HTTP audit. Worse, CvCascadeConditionEval reads legacy property values to gate OTHER cascade channels' requires.operate dormancy.
- **FREE-XP GRANT + SPECIALIST COUNTS legacy and feed cascade as inputs** — getProductionExperience (XP stamped on trained/conscript units in doGrowth) fully legacy; getFreeSpecialistCount/getMaxSpecialistCount legacy yet read AS COUNTS inside live cascade yield/wellbeing/greatPeople packages — cutting them over silently moves cascade totals.
- **CITIZEN/PLOT-ASSIGNMENT AI on legacy modifier stack** — AI_yieldValueInternal/AI_plotValue/AI_yieldMultiplier/AI_getYieldMultipliers compute marginal yields via legacy getBaseYieldRate×getBaseYieldRateModifier, bypassing yPctCity; worked-plot & specialist assignment never see the cascade the display uses.
- **GREAT-PERSON UNIT RATE legacy** — getGreatPeopleUnitRate/getNationalGreatPeopleUnitRate have no flip and feed doGreatPeople directly; per-unit-type GPP that spawns Great People blends cascade aggregate with unflipped legacy per-type rate.
- **CvPlayer::getProductionModifier(Unit/Building/Project) has NO cascade counterpart** — always raw legacy trait/wonder walk; called by advanced-start costing, getNewCityProductionValue AI, diagnostics.
- **MILITARY/SPACE/DOMAIN/UNIT-COMBAT production sub-modifiers read raw-legacy by AI even post-cutover** — CvCity/CvPlayer getMilitary/SpaceProductionModifier, getDomainProductionModifier, getUnitCombatProductionModifier bypassed by cascade (parallel fields) yet read directly by AI_get{Military,NavalMilitary}ProductionRateRank, AI_yieldMultiplier, AI_specialistValue; silent drift risk.
- **MAINTENANCE base components legacy** — distance/numCities/colony/corporation/building base calcs + player/area/team sub-modifiers all legacy; only getEffectiveMaintenanceModifier flips, so base bill (and whole empire gold-expense chain) is legacy.
- **CvGame::doGlobalWarming** — real gameplay effect computed entirely off legacy building/bonus health accumulators (totalBadBuildingHealth + getBonusBadHealth); never routes through wellbeing cascade/flip.
- **DEFENSE has zero pure-cascade readers, legacy legs everywhere** — getNaturalDefense (CultureLevelInfo static), calculateBonusDefense (map), getDefenseDamage (war decay), CvPlayer::getNationalBombardDefenseModifier remain legacy inside getTotalDefense/getDefenseModifier → CvPlot::defenseModifier → combat odds.
- **UI TOOLTIPS duplicate legacy formulas inline** — CvGameTextMgr production/yield-breakdown (5576/5643/25921/25998), maintenance recompose (DLLWidgetData 5081), bombard/capture help re-sum raw legacy getters instead of the composed getter; display can diverge from same-screen values.
- **AI TECH CHOICE legacy despite enabler flip** — CvPlayerAI::AI_bestTech and AI_bestReligiousTech pass bRightNow=false to canResearch, forcing canResearchLegacy; primary tech-selection loop never touches the cascade frontier.

## Trade routes — owner ruling (2026-07-13), supersedes the failed trace

The `traderoutes` trace agent errored (StructuredOutput cap) and produced no map — but the owner's ruling makes a full re-trace unnecessary, because trade routes split into two things with different homes:

- **Trade-route YIELD is NOT a cascade calc — it is READ.** It is already an **engine-generated package** (the trade network's yield, which the cascade can never re-derive — the network is out of scope). It is the ONE live-yield INPUT the read path folds ([modifier.md](../../specs/modifier.md) §2a `tradeYield`, [http-endpoints.md](../../specs/http-endpoints.md) — the sole live-yield input). So on the unified surface the trade-route yield is a **read-only provider** (a package the game object pulls, never a cascade sum).
- **The trade-route COUNT (`getMaxTradeRoutes` — allowed routes) IS calculated by the cascade** — it is a modifier-influenced value (game + player + coastal + `city.extra` slot deposits). This is the `traderoutes` channel proper on the surface; `getMaxTradeRoutes` is a drycalc TARGET ([http-endpoints.md](../../specs/http-endpoints.md)).

So: trade yield = read the engine package; trade route count = cascade-calculated. No reader re-trace needed.

## CULTURE (re-traced 2026-07-13 — hole closed)

Culture is the dual-scope channel, and it turns out to be in decent structural shape — the RATE is cascade, the VALUE is a game-object warehouse (the same pattern as production banking).

**Backing-defining getters:**

| reader (file:function) | scope | thing | backing |
|---|---|---|---|
| `CvCity::getCommerceRateTimes100(COMMERCE_CULTURE)` :12161 → `CascadeAccumulator::commerceRate100` | city | culture RATE/turn | **CASCADE** (post-init; legacy pre-init) |
| `CvCity::getCulture`/`getCultureTimes100` :13316 (`m_aiCulture`) | city | culture VALUE (banked) | **legacy storage, CASCADE-sourced** — `doCulture`→`changeCultureTimes100` banks the cascade rate |
| `CvCity::getCultureLevel` :10725 (`m_eCultureLevel`) | city | culture LEVEL | legacy (from the banked value) |
| `CvPlot::getCulture`/`calculateCulturalOwner` :8520/4873 | plot | plot culture + border owner | legacy |
| `CvPlayer::getCulture` :28167 (`m_iCulture`) | empire | civ culture total | legacy (Σ city banked) |
| `CvTeam::countTotalCulture` :2406 | team | total | legacy (Σ player) |

**Consumers (all read the getters above, unchanged by a body-flip):** border expansion / revolt (`calculateCulturalOwner`, `baseRevoltRisk100`), wonder caps + city defense + building prereqs (`getCultureLevel`→XML `CvCultureLevelInfo`), culture victory (`CvGame::testVictory`, `CvTeam::calculateVictoryScore`), great-work culture (`CvUnit::getGreatWorkCulture`), the sync checksum, ~40 AI sites (`CvCityAI`/`CvPlayerAI`/`CvUnitAI`), ~20 UI/text/widget sites, and the **Cy\* Python bindings (thin passthroughs** — `CyCity`/`CyPlot`/`CyPlayer`/`CyTeam` forward straight to the C++ getters, so a body-flip never touches Python).

**Verdict — culture is mostly RIGHT, and it validates the model:** the culture *rate* (`getCommerceRate(COMMERCE_CULTURE)`) is cascade-fed; the culture *value/level/borders* are the city's **culture warehouse** — accumulate the cascade rate, spend it on borders + level-ups — a game-object banking mechanic, correctly legacy, NOT a cascade job (exactly the production-warehouse carve-out). So the only cascade concern here is that the *rate* getter stays cascade; the accumulation stays the game object's.

**Unresolved (low-stakes):** no reader gates on "held culture ≥ X" for traits/heritage (heritage *feeds* culture as a `PSC_CFLAT` cascade commerce source via `updateEra`, it doesn't read a culture threshold); `findCommerceRateRank`'s cascade backing is high-confidence-inferred (body not fully read).
