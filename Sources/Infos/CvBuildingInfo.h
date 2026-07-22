#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvBuildingInfo -- the per-type cascade info for BUILDINGS (ports StoneBase's BuildingInfo). Composes the
//	section units a building authors (requires / edges / allowed / grants / provides / modifier families /
//	whenObsolete / attributes / capabilities -- the data-grounded table); this adds the typed flags + the curator
//	`identity` block, SELF-CONTAINED (the engine getGlobalReligionCommerce / getReligionType /
//	getGlobalCorporationCommerce / getStateReligionCommerce / getCommerceChangeDoubleTime reads are RETIRED).
//	shrine/corpHQ/religion are FK ids (-1 none); the commerce blocks are {channel:value} maps.
//
//	#430 legacy-getter mirror: the ENTIRE archived CvBuildingInfo consumer surface (SourceArchive/Infos/CvBuildingInfo.h)
//	is reproduced here. REAL DATA where the poco already maps it (typed identity/cost members, the composed
//	edges/allowed/attributes/capabilities units, and the §6 modifier families read via the sumUnconditioned helper in
//	the .cpp); every remaining getter is an owner-sanctioned STUB-DEFAULT (bool->false, int->0, FK->-1/NO_*,
//	const char*->"", int*->NULL, reference->an empty backing member) tagged with why. Hotkey/action getters are
//	inherited from CvHotkeyInfo and NOT redeclared. The XML load/serialize machinery (read/getDataMembers/getCheckSum/
//	copyNonDefaults/doPostLoadCaching) and the Python cy* list wrappers are NOT reproduced (the JSON `mapFrom` path
//	replaces the former; the latter is Python-binding surface outside this consumer set).
//

#include "CvInfo.h"
#include "Engine/ConstructRequirement.h"   // #195 Phase 2 GOM-derived requirement list (STUB empty; see getConstructRequirements)
#include "Defines/CvStructs.h"             // HealUnitCombat / BonusAidModifiers / AidRateChanges / EnabledCivilizations / YieldArray / CommerceArray
#include <vector>
#include <map>

class CvArtInfoBuilding;
class CvArtInfoMovie;
class CvGameObject;
class BoolExpr;

class CvBuildingInfo : public CvInfo
{
public:
	CvBuildingInfo()
		: notConstructible(false), governmentCenter(false), forceNoPrereqScaling(false),
		  shrineReligion(-1), corpHQ(-1), religion(-1), freeStartEra(-1), conquestProbability(50), maxPlayerInstancesExtra(0),
		  m_iHappinessPercentPerPopulation(0), m_iHealthPercentPerPopulation(0),
		  m_iDamageToAttacker(0), m_iDamageAttackerChance(0), m_bHasCounterDamage(false), m_bDamageAllAttackers(false),
		  m_bForceTeamVoteEligible(false),
		  voteSourceType(-1),
		  autoBuild(false),
		  m_iMaxStartEra(-1), m_iAdvisorType(-1), m_iGreatPeopleUnitType(-1), m_iPromotionLineType(-1),
		  m_iExtendsBuilding(-1), m_iProductionContinueBuilding(-1), m_iSpecialBuilding(-1),
		  m_iAirlift(0), m_iAirUnitCapacity(0), m_iDCMAirbombMission(0), m_iLineOfSight(0), m_iWorkableRadius(0),
		  m_iNumPopulationEmployed(0), m_iLinePriority(0), m_iAssetValue(0), m_iPowerValue(0),
		  m_iProductionCost(0), m_iProductionCostSize(0), m_iProductionCostCount(0), m_iProductionCostMaterials(0),
		  m_iProductionCostComplexity(0), m_iAIWeight(0),
		  m_bCenterInCity(false), m_bAllowsNukes(false), m_bNoLimit(false),
		  m_fVisibilityPriority(0.0f), m_iMissionType(-1),
		  m_iMinAreaSize(0), m_iMinLatitude(0), m_iMaxLatitude(90), m_iNumCitiesPrereq(0), m_iNumTeamsPrereq(0), m_iPrereqPopulation(0),
		  m_iVictoryPrereq(-1), m_iHolyCity(-1), m_iPrereqStateReligion(-1), m_iPrereqReligion(-1), m_iPrereqCorporation(-1),
		  m_iPrereqCultureLevel(-1), m_iPrereqAnyoneBuilding(-1), m_iPrereqAndTech(-1), m_iPrereqAndBonus(-1),
		  m_iPrereqVicinityBonus(-1), m_iPrereqRawVicinityBonus(-1), m_iStateReligionHappiness(0),
		  m_bNeedStateReligionInCity(false), m_bWater(false), m_bRiver(false), m_bFreshWater(false), m_bNoHolyCity(false), m_bPower(false),
		  m_iNumUnitFullHeal(0), m_iPropertySpawnUnit(-1), m_iPropertySpawnProperty(-1)
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			m_powerYieldModifier[i] = 0;
			m_aiYieldChange[i] = m_aiYieldModifier[i] = m_aiAreaYieldModifier[i] = 0;
			m_aiGlobalYieldModifier[i] = m_aiGlobalSeaPlotYieldChange[i] = 0;
		}
		for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		{
			m_aiCommerceChange[i] = m_aiCommerceModifier[i] = m_aiGlobalCommerceModifier[i] = 0;
			m_aiSpecialistExtraCommerce[i] = m_aiCommerceHappiness[i] = 0;
			m_aiCommerceChangeDoubleTime[i] = m_aiStateReligionCommerce[i] = 0;
		}
		// zero the materialized scalars (mapFrom fully redefines them on a real entity)
		m_iHappiness = m_iAreaHappiness = m_iGlobalHappiness = m_iHealth = m_iAreaHealth = m_iGlobalHealth = 0;
		m_iEnemyWarWearinessModifier = m_iOccupationTimeModifier = m_iHealRateChange = m_iFoodKept = 0;
		m_iGreatPeopleRateChange = m_iGreatPeopleRateModifier = m_iGlobalGreatPeopleRateModifier = 0;
		m_iGreatGeneralRateModifier = m_iDomesticGreatGeneralRateModifier = 0;
		m_iMaintenanceModifier = m_iGlobalMaintenanceModifier = m_iAreaMaintenanceModifier = m_iOtherAreaMaintenanceModifier = 0;
		m_iDistanceMaintenanceModifier = m_iNumCitiesMaintenanceModifier = m_iCoastalDistanceMaintenanceModifier = m_iConnectedCityMaintenanceModifier = 0;
		m_iInflationModifier = m_iWarWearinessModifier = m_iGlobalWarWearinessModifier = 0;
		m_iHurryCostModifier = m_iGlobalHurryModifier = m_iHurryAngerModifier = 0;
		m_iMilitaryProductionModifier = m_iSpaceProductionModifier = m_iGlobalSpaceProductionModifier = m_iWorkerSpeedModifier = 0;
		m_iTradeRoutes = m_iCoastalTradeRoutes = m_iGlobalTradeRoutes = m_iWorldTradeRoutes = m_iTradeRouteModifier = m_iForeignTradeRouteModifier = 0;
		m_iFreeExperience = m_iGlobalFreeExperience = m_iFreeSpecialist = m_iAreaFreeSpecialist = m_iGlobalFreeSpecialist = 0;
		m_iAnarchyModifier = m_iGoldenAgeModifier = m_iPopulationgrowthratepercentage = m_iGlobalPopulationgrowthratepercentage = 0;
		m_iRevIdxLocal = m_iRevIdxNational = m_iRevIdxDistanceModifier = m_iInsidiousness = m_iInvestigation = 0;
		m_iEspionageDefenseModifier = m_iUnitUpgradePriceModifier = 0;
		m_iDefenseModifier = m_iBombardDefenseModifier = m_iAllCityDefenseModifier = m_iNukeModifier = m_iAirModifier = 0;
		m_iMinDefense = m_iNoEntryDefenseLevel = m_iLocalDynamicDefense = m_iRiverDefensePenalty = 0;
		m_iBuildingDefenseRecoverySpeedModifier = m_iCityDefenseRecoverySpeedModifier = m_iAdjacentDamagePercent = 0;
		m_iNationalCaptureProbabilityModifier = m_iNationalCaptureResistanceModifier = m_iLocalCaptureProbabilityModifier = m_iLocalCaptureResistanceModifier = 0;
		m_bGrantsGoldenAge = false;
	}

	bool notConstructible, governmentCenter, forceNoPrereqScaling;   // notConstructible/forceNoPrereqScaling <- identity; governmentCenter <- `attributes` (IS_GOVERNMENT_CENTER)
	std::string specialBuildingType;
	int shrineReligion;                                  // top-level `shrine` -> religion FK
	int corpHQ;                                          // top-level `headquarters` -> corporation FK
	int religion;                                        // identity.religion -> religion FK (state-religion match)
	std::map<std::string, int> stateReligionCommerce;    // identity.stateReligionCommerce {channel:value}
	std::map<std::string, int> commerceDoubleTime;       // identity.commerceDoubleTime {channel:years}
	int freeStartEra;         // identity.freeStartEra -> EraTypes FK
	int conquestProbability;  // identity.conquestProbability
	int m_iHappinessPercentPerPopulation, m_iHealthPercentPerPopulation;  // {happiness|health}.city perPopulation entries, materialized at mapFrom (raw legacy scale; consumer /100s)
	int maxPlayerInstancesExtra;  // identity.maxPlayerInstancesExtra -- the extra-per-player instance-cap component (only PALACE authors it)
	int m_iDamageToAttacker, m_iDamageAttackerChance;   // defense.city.counterDamage.{damage,chance} (bespoke object the modifier parser skips)
	bool m_bHasCounterDamage, m_bDamageAllAttackers;    // counterDamage present / no `units` selector => all attackers
	bool m_bForceTeamVoteEligible;                      // enables.votes marker "FORCE_TEAM_ELIGIBLE" (non-INFOTYPE, edge-dropped)
	std::vector<int> m_aiMayDamageUnitCombats;          // defense.city.counterDamage.units.unitCombats FK list
	int m_aiRiverPlotYieldChange[NUM_YIELD_TYPES];      // HAS_RIVER-gated <yield>.city.plots flats (legacy RiverPlotYieldChanges)
	int voteSourceType;       // identity.diploVoteType -> VoteSourceTypes FK
	bool autoBuild;           // identity.autoBuild
	virtual void mapFrom(const picojson::value& entity);

	// ============================ #430 mirrored legacy CvBuildingInfo getters ============================
	// (consumer surface; hotkey/action getters are inherited from CvHotkeyInfo and NOT redeclared)

	// --- bool flags: REAL where the `attributes` block / identity carry them; else STUB false ---
	// attribute flags: O(1) generated-id bit tests (CLS_HAS; ATTRIBUTE_* ids from the ClassificationRegistry)
	bool isProvidesFreshWater() const      CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "providesFreshWater")
	bool isForceAllTradeRoutes() const     CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "forceAllTradeRoutes")
	bool isForceNoPrereqScaling() const    { return forceNoPrereqScaling; }
	bool isZoneOfControl() const           CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "zoneOfControl")
	bool isProtectedCulture() const        CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "protectedCulture")
	bool isNoLimit() const                 { return m_bNoLimit; }
	bool isTeamShare() const               CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "teamShare")
	bool isAutoBuild() const               { return autoBuild; }
	bool isOrbital() const                 CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "orbital")
	bool isOrbitalInfrastructure() const   CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "orbitalInfrastructure")
	bool isAreaBorderObstacle() const      CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "borderObstacle")
	bool isForceTeamVoteEligible() const   { return m_bForceTeamVoteEligible; }   // enables.votes marker "FORCE_TEAM_ELIGIBLE" (read raw in mapFrom -- the edge parser drops the non-INFOTYPE id)
	bool isCapital() const                 CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "capital")
	bool isGovernmentCenter() const        { return governmentCenter; }
	bool isMapCentering() const            CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "mapCentering")
	bool isNoUnhappiness() const           CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "noUnhappiness")
	bool isNoUnhealthyPopulation() const   CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "noUnhealthyPopulation")
	bool isBuildingOnlyHealthy() const     CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "buildingOnlyHealthy")
	bool isNeverCapture() const            CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "destroyedOnCapture")  // curator RENAMED bNeverCapture -> attributes.destroyedOnCapture (owner ruling 2026-07-01)
	bool isNukeImmune() const              CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "nukeImmune")
	bool isCenterInCity() const            { return m_bCenterInCity; }
	bool isAllowsNukes() const             { return m_bAllowsNukes; }
	bool isQuarantine() const              CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "quarantine")
	// requires-derived flags (reconstructed in reconstructFromComposed) + documented curator-gaps.
	bool isPrereqPower() const             { return m_bPower; }   // REAL requires.operate HAS_POWER (NEEDS power; the engine dorms on power loss)
	bool isApplyFreePromotionOnMove() const{ return false; }   // CURATOR-GAP: dropped as redundant (all freePromotions are end-turn-stay)
	bool isNoEnemyPillagingIncome() const  { return false; }   // CURATOR-GAP: dead field (DROP_DEAD)
	bool isPrereqWar() const               { return false; }   // DEAD: ZERO buildings author bPrereqWar=1 (verified 2026-07-11); the engine war-dormancy path (CvCity.cpp:21374) has no data. A future war-gated building = requires.operate predicate, not this bool.
	bool isRequiresActiveCivics() const    { return false; }   // DEAD-as-getter: its meaning (build-vs-operate for civic prereqs) is FULLY captured -- all 144 civic-prereq buildings ARE RequiresActiveCivics, so curate_building emits PrereqOr/AndCivics -> requires.operate (exact); NO build-only civic building exists (verified 2026-07-11). No consumer needs the standalone bool.
	bool isWater() const                   { return m_bWater; }   // REAL requires.build HAS_COAST
	bool isRiver() const                   { return m_bRiver; }   // REAL requires.build HAS_RIVER
	bool isFreshWater() const              { return m_bFreshWater; } // REAL requires.operate HAS_FRESHWATER (NEEDS fresh water; the engine dorms on loss)
	bool isPower() const                   CLS_HAS(m_attributes, CLSD_ATTRIBUTE, "providesPower")   // REAL attributes.providesPower: the building PROVIDES power (the power-plant flag feeding changePowerCount)
	bool isNoHolyCity() const              { return m_bNoHolyCity; } // REAL requires.build.disabled IS_HOLY_CITY
	bool isGoldenAge() const               { return m_bGrantsGoldenAge; }   // REAL grants.goldenAge (materialized at mapFrom)
	bool needStateReligionInCity() const   { return m_bNeedStateReligionInCity; }   // REAL requires.build STATE_RELIGION_IN_CITY
	bool isDamageAllAttackers() const      { return m_bDamageAllAttackers; }   // defense.city.counterDamage present with NO `units` selector
	bool isDamageAttackerCapable() const   { return m_bHasCounterDamage; }     // a defense.city.counterDamage object exists
	bool getNotShowInCity() const          { return false; }   // CURATOR-GAP: derived display flag, not emitted
	bool EnablesOtherBuildings() const     { const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_BUILDINGS); return v != NULL && !v->empty(); }   // REAL enables.buildings edge
	bool EnablesUnits() const              { const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_UNITS); return v != NULL && !v->empty(); }         // REAL enables.units edge

	// --- typed identity / cost members -- REAL data ---
	int getReligionType() const              { return religion; }                // identity.religion FK (state-religion match)
	int getGlobalCorporationCommerce() const { return corpHQ; }                  // NB legacy-misnamed: a CORPORATION FK (this building IS that corp's HQ), not a commerce amount
	int getGlobalReligionCommerce() const    { return shrineReligion; }          // NB legacy-misnamed: the SHRINE's RELIGION FK (values live on the religion), mirrors getGlobalCorporationCommerce
	EraTypes getFreeStartEra() const         { return (EraTypes)freeStartEra; }
	int getMaxStartEra() const               { return m_iMaxStartEra; }
	int getConquestProbability() const       { return conquestProbability; }
	int getVoteSourceType() const            { return voteSourceType; }
	int getAdvisorType() const               { return m_iAdvisorType; }
	int getGreatPeopleUnitType() const       { return m_iGreatPeopleUnitType; }
	PromotionLineTypes getPromotionLineType() const { return (PromotionLineTypes)m_iPromotionLineType; }
	BuildingTypes getExtendsBuilding() const        { return (BuildingTypes)m_iExtendsBuilding; }
	BuildingTypes getProductionContinueBuilding() const { return (BuildingTypes)m_iProductionContinueBuilding; }
	SpecialBuildingTypes getSpecialBuilding() const { return (SpecialBuildingTypes)m_iSpecialBuilding; }
	int getAirlift() const                   { return m_iAirlift; }
	int getAirUnitCapacity() const           { return m_iAirUnitCapacity; }
	int getDCMAirbombMission() const         { return m_iDCMAirbombMission; }
	int getLineOfSight() const               { return m_iLineOfSight; }
	int getWorkableRadius() const            { return m_iWorkableRadius; }
	int getNumPopulationEmployed() const     { return m_iNumPopulationEmployed; }
	int getLinePriority() const              { return m_iLinePriority; }
	int getAssetValue() const                { return m_iAssetValue * 100; }     // archive returns the raw worth ×100
	int getPowerValue() const                { return m_iPowerValue * 100; }
	int getProductionCost() const            { return m_iProductionCost; }
	int getProductionCostSize() const        { return m_iProductionCostSize; }
	int getProductionCostCount() const       { return m_iProductionCostCount; }
	int getProductionCostMaterials() const   { return m_iProductionCostMaterials; }
	int getProductionCostComplexity() const  { return m_iProductionCostComplexity; }
	float getVisibilityPriority() const      { return m_fVisibilityPriority; }
	const char* getConstructSound() const    { return m_szConstructSound.c_str(); }

	// --- the declarative instance cap (`allowed` unit) + the store edges -- REAL data ---
	int getMaxGlobalInstances() const        { return getAllowed()->cap("world"); }
	int getMaxTeamInstances() const          { return getAllowed()->cap("team"); }
	int getMaxPlayerInstances() const        { return getAllowed()->cap("empire"); }   // NB folds +extra (curator); the split is not separately authored
	int getExtraPlayerInstances() const      { return maxPlayerInstancesExtra; }   // identity.maxPlayerInstancesExtra (separate from the allowed.empire fold; survives it -- e.g. PALACE, empty allowed block)
	int getFoundsCorporation() const;        // enables.corporations edge (REAL)
	TechTypes getObsoleteTech() const;       // obsoletedBy.techs edge (REAL)
	BuildingTypes getObsoletesToBuilding() const;  // obsoletedBy.buildings edge (REAL)

	// --- EXE-bound art surface (world.art.icon -> ARTFILEMGR shim, mirrors CvBonusInfo) -- REAL data ---
	const char* getArtDefineTag() const      { return m_szArtDefineTag.c_str(); }
	const CvArtInfoBuilding* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton -> missing icon)
	const char* getMovieDefineTag() const    { return m_szMovieDefineTag.c_str(); }   // REAL ui.art.movie.defineTag
	const char* getMovie() const;                                                     // REAL getMovieInfo()->getPath()
	const CvArtInfoMovie* getMovieInfo() const;                                       // REAL ARTFILEMGR.getMovieArtInfo(tag)

	// --- RUNTIME (set post-load, NOT JSON; mirrors CvCorporationInfo) ---
	int getMissionType() const               { return m_iMissionType; }
	void setMissionType(int iType)           { m_iMissionType = iType; }

	// --- §6 modifier-family reads over the composed m_modifiers unit -- REAL data (see the .cpp: sumUnconditioned
	// recovers the legacy plain scalar by summing only the UN-conditioned, non-per entries at the deposit address) ---
	int getHappiness() const;
	int getAreaHappiness() const;
	int getGlobalHappiness() const;
	int getHealth() const;
	int getAreaHealth() const;
	int getGlobalHealth() const;
	int getHealRateChange() const;
	int getFoodKept() const;
	int getGreatPeopleRateChange() const;
	int getGreatPeopleRateModifier() const;
	int getGlobalGreatPeopleRateModifier() const;
	int getGreatGeneralRateModifier() const;
	int getDomesticGreatGeneralRateModifier() const;
	int getMaintenanceModifier() const;
	int getGlobalMaintenanceModifier() const;
	int getAreaMaintenanceModifier() const;
	int getOtherAreaMaintenanceModifier() const;
	int getDistanceMaintenanceModifier() const;
	int getNumCitiesMaintenanceModifier() const;
	int getCoastalDistanceMaintenanceModifier() const;
	int getConnectedCityMaintenanceModifier() const;
	int getInflationModifier() const;
	int getWarWearinessModifier() const;
	int getGlobalWarWearinessModifier() const;
	int getEnemyWarWearinessModifier() const;
	int getHurryCostModifier() const;
	int getGlobalHurryModifier() const;
	int getHurryAngerModifier() const;
	int getMilitaryProductionModifier() const;
	int getSpaceProductionModifier() const;
	int getGlobalSpaceProductionModifier() const;
	int getWorkerSpeedModifier() const;
	int getTradeRoutes() const;
	int getCoastalTradeRoutes() const;
	int getGlobalTradeRoutes() const;
	int getWorldTradeRoutes() const;
	int getTradeRouteModifier() const;
	int getForeignTradeRouteModifier() const;
	int getFreeExperience() const;
	int getGlobalFreeExperience() const;
	int getFreeSpecialist() const;
	int getAreaFreeSpecialist() const;
	int getGlobalFreeSpecialist() const;
	int getAnarchyModifier() const;
	int getGoldenAgeModifier() const;
	int getOccupationTimeModifier() const;
	int getPopulationgrowthratepercentage() const;
	int getGlobalPopulationgrowthratepercentage() const;
	int getRevIdxLocal() const;
	int getRevIdxNational() const;
	int getRevIdxDistanceModifier() const;
	int getInsidiousness() const;
	int getInvestigation() const;
	int getEspionageDefenseModifier() const;
	int getUnitUpgradePriceModifier() const;
	int getDefenseModifier() const;
	int getBombardDefenseModifier() const;
	int getAllCityDefenseModifier() const;
	int getNukeModifier() const;
	int getAirModifier() const;
	int getMinDefense() const;
	int getNoEntryDefenseLevel() const;
	int getLocalDynamicDefense() const;
	int getRiverDefensePenalty() const;
	int getBuildingDefenseRecoverySpeedModifier() const;
	int getCityDefenseRecoverySpeedModifier() const;
	int getDamageAttackerChance() const;
	int getDamageToAttacker() const;
	int getAdjacentDamagePercent() const;
	int getNationalCaptureProbabilityModifier() const;
	int getNationalCaptureResistanceModifier() const;
	int getLocalCaptureProbabilityModifier() const;
	int getLocalCaptureResistanceModifier() const;

	// per-YIELD / per-COMMERCE indexed family reads -- REAL data (unconditioned scalar at the deposit address).
	int getYieldChange(int i) const;
	int getYieldModifier(int i) const;
	int getAreaYieldModifier(int i) const;
	int getGlobalYieldModifier(int i) const;
	int getGlobalSeaPlotYieldChange(int i) const;
	int getCommerceChange(int i) const;
	int getCommerceModifier(int i) const;
	int getGlobalCommerceModifier(int i) const;
	int getSpecialistExtraCommerce(int i) const;
	int getCommerceChangeDoubleTime(int i) const;   // commerceDoubleTime map (REAL)
	int getStateReligionCommerce(int i) const;      // stateReligionCommerce map (REAL)
	int getPowerYieldModifier(int i) const          { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_powerYieldModifier[i] : 0; }  // REAL <yield>.city percent enabled HAS_POWER
	int getRiverPlotYieldChange(int i) const    { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiRiverPlotYieldChange[i] : 0; }   // HAS_RIVER-gated <yield>.city.plots flats

	// ai.flavours -- REAL data.
	int getFlavorValue(int i) const { return mapGet(m_flavours, i); }

	// commerce sliders this building UNLOCKS (`capabilities` block) -- REAL data (mirror of tech isCommerceFlexible).
	bool isCommerceFlexible(int i) const;

	// per-pop getters. The yield/commerce pair is FAITHFUL-0: zero authorings in any building XML (census
	// 2026-07-17), nothing to serve. The wellbeing pair serves the curated {happiness|health}.city
	// perPopulation UNIT entries -- the RAW legacy number (the registry's specialist-iHealthPercent class:
	// the CONSUMER divides by 100, CvCity.cpp getBuildingGoodHealth "(...PerPopulation() * pop) / 100"), so
	// no de-scale here reproduces legacy exactly. No new ruling was needed: fixed-point-and-scales.md's
	// map-at-the-consumption-site method decides it.
	int getYieldPerPopChange(int /*i*/) const        { return 0; }
	int getCommercePerPopChange(int /*i*/) const     { return 0; }
	// materialized at mapFrom (the hot-path rule: no per-call string-address resolution; the sumUnconditioned
	// string-idiom getters above predate the rule and await the same materialization pass)
	int getHappinessPercentPerPopulation() const     { return m_iHappinessPercentPerPopulation; }
	int getHealthPercentPerPopulation() const        { return m_iHealthPercentPerPopulation; }

	// GROUP 1 (requires condition-tree) + clean grant/repeatable reads -- REAL, reconstructed in reconstructFromComposed().
	int getPrereqAndTech() const           { return m_iPrereqAndTech; }        // REAL requires.build first TECH atom
	int getPowerBonus() const              { return -1; }   // CURATOR-GAP: PowerBonus emitted as an operate BONUS atom with role="power", but `role` is not a CvJsonCondition field (dropped) -> indistinguishable from a plain bonus prereq
	BuildingTypes getFreeBuilding() const     { return (BuildingTypes)-1; }   // CURATOR-GAP: FreeBuilding is in STORE_TAGS (dropped building-side) and no store.py/curator path emits it -> unrecoverable
	BuildingTypes getFreeAreaBuilding() const { return (BuildingTypes)-1; }   // CURATOR-GAP: FreeAreaBuilding likewise dropped, never emitted
	int getCivicOption() const             { return -1; }   // CURATOR-GAP: iCivicOption/CivicOption not emitted by curate_building.py at all
	int getAIWeight() const                { return m_iAIWeight; }             // REAL ai.behaviour.weight
	int getMinAreaSize() const             { return m_iMinAreaSize; }          // REAL requires.build AREA_SIZE min (or HAS_COAST.minArea for water)
	int getNumCitiesPrereq() const         { return m_iNumCitiesPrereq; }      // REAL requires.build CITY min
	int getNumTeamsPrereq() const          { return m_iNumTeamsPrereq; }       // REAL requires.build TEAM min
	int getUnitLevelPrereq() const         { return 0; }    // CURATOR-GAP: iLevelPrereq intentionally dropped by owner (curate_building.py:911)
	int getMinLatitude() const             { return m_iMinLatitude; }          // REAL requires.build latitude.min
	int getMaxLatitude() const             { return m_iMaxLatitude; }          // REAL requires.build latitude.max (90 default)
	int getNukeExplosionRand() const       { return 0; }    // CURATOR-GAP: excluded-module-only data, not emitted
	int getStateReligionHappiness() const  { return m_iStateReligionHappiness; }   // REAL happiness.city flat gated STATE_RELIGION / HAS_STATE_RELIGION
	int getPrereqGameOption() const        { return -1; }   // CURATOR-GAP: PrereqGameOption feeds the entity-level enabled/disabled gate (CvJsonGate), which this poco does not compose
	int getNotGameOption() const           { return -1; }   // CURATOR-GAP: NotGameOption -> the entity gate (not composed)
	int getHolyCity() const                { return m_iHolyCity; }             // REAL requires.build predicate {IS_HOLY_CITY: RELIGION_X}
	int getPrereqStateReligion() const     { return m_iPrereqStateReligion; }  // REAL requires.build predicate {STATE_RELIGION: RELIGION_X}
	int getPrereqReligion() const          { return m_iPrereqReligion; }       // REAL requires.operate PRESENCE (RELIGION_X, city)
	int getPrereqCorporation() const       { return m_iPrereqCorporation; }    // REAL requires.operate PRESENCE (CORPORATION_X)
	int getPrereqAndBonus() const          { return m_iPrereqAndBonus; }       // REAL requires.operate BONUS (trade|vicinity, no discriminator); NB may be the PowerBonus if present (role lost, see getPowerBonus)
	int getGlobalPopulationChange() const  { return getGrants()->scopedPulse100("population", "empire") / 100; }   // REAL grants.population.empire
	int getFreeTechs() const               { return getGrants()->pulse100("freeTechs") / 100; }                    // REAL grants.freeTechs
	int getPrereqVicinityBonus() const     { return m_iPrereqVicinityBonus; }  // REAL requires.operate BONUS (vicinity, connected)
	int getPrereqRawVicinityBonus() const  { return m_iPrereqRawVicinityBonus; } // REAL requires.operate BONUS (vicinity, owned)
	int getPillageGoldModifier() const     { return 0; }    // CURATOR-GAP: dead field (DROP_DEAD)
	int getPrereqPopulation() const        { return m_iPrereqPopulation; }     // REAL requires.build POPULATION min
	int getPrereqCultureLevel() const      { return m_iPrereqCultureLevel; }   // REAL requires.build PRESENCE (CULTURELEVEL_X)
	BuildingTypes getPrereqAnyoneBuilding() const { return (BuildingTypes)m_iPrereqAnyoneBuilding; }  // REAL requires.build BUILDING atom (world scope)
	int getNumUnitFullHeal() const         { return m_iNumUnitFullHeal; }      // REAL grants.repeatable[] heal:"full" count
	int getMaxPopulationAllowed() const    { return -1; }   // CURATOR-GAP: DROP_DEAD -- -1 is the UNSET sentinel (no cap);
	                                                        // 0 rendered "Sets base max population at 0" help text on EVERY building (getMaxPopulationAllowed > -1 gate)
	int getMaxPopulationChange() const     { return 0; }    // CURATOR-GAP: DROP_DEAD
	int getPopulationChange() const        { return getGrants()->scopedPulse100("population", "city") / 100; }   // REAL grants.population.city
	int getMaxPopAllowed() const           { return 0; }    // CURATOR-GAP: DROP_DEAD
	TechTypes getFreeSpecialTech() const   { return (TechTypes)getGrants()->firstListId("techs"); }   // REAL grants.techs (FreeSpecialTech)
	UnitTypes getPropertySpawnUnit() const     { return (UnitTypes)m_iPropertySpawnUnit; }        // REAL grants.repeatable[].unit (property-spawn)
	PropertyTypes getPropertySpawnProperty() const { return (PropertyTypes)m_iPropertySpawnProperty; }  // REAL grants.repeatable[].chance.per property FK
	int getVictoryPrereq() const           { return m_iVictoryPrereq; }        // REAL requires.build PRESENCE (VICTORY_X, world)

	// --- reference / pointer returning getters -- REAL (backing members populated in reconstructFromComposed) ---
	const std::vector<ConstructRequirement>& getConstructRequirements() const { return m_constructRequirements; }  // STUB empty -- #195 GOM derivation deferred (cascade-level)
	const std::vector<BonusTypes>& getConsumptionRelevantBonuses() const { return m_consumptionRelevantBonuses; }  // deduped UNION of the 5 bonus-keyed modifier maps (reconstructFromComposed), archive buildConsumptionRelevantBonuses
	const IDValueMap<BonusTypes, int>& getFreeBonuses() const                { return m_freeBonuses; }             // REAL -- reconstructFromComposed populates m_freeBonuses from provides.bonuses (owner ruling 2026-07-11); count 1
	const IDValueMap<PlotTypes, YieldArray>& getPlotYieldChanges() const     { return m_aPlotYieldChanges; }       // STUB empty -- plots-target fold (getGlobalSeaPlotYieldChange path); not the keyed legacy shape
	const IDValueMap<TerrainTypes, YieldArray>& getTerrainYieldChanges() const { return m_aTerrainYieldChanges; }  // REAL <yield>.city.terrains.<TERRAIN>
	const IDValueMap<ReligionTypes, int>& getReligionChanges() const         { return m_religionChange; }          // REAL religion.city.<RELIGION>
	const IDValueMap<BonusTypes, int>& getBonusHealthChanges() const         { return m_piBonusHealthChanges; }    // REAL health.city flat enabled BONUS
	const IDValueMap<BonusTypes, int>& getBonusHappinessChanges() const      { return m_piBonusHappinessChanges; } // REAL happiness.city flat enabled BONUS
	const IDValueMap<UnitCombatTypes, int>& getUnitCombatFreeExperience() const { return m_aUnitCombatFreeExperience; }  // REAL experience.city.unitCombats.<UC>
	const IDValueMap<BuildingTypes, int>& getBuildingHappinessChanges() const { return m_aBuildingHappinessChanges; }   // REAL happiness.empire.buildings.<BUILDING>
	const IDValueMap<ImprovementTypes, int>& getImprovementFreeSpecialists() const { return m_improvementFreeSpecialists; } // REAL freeSpecialists.city.any per:{IMPROVEMENT}
	const IDValueMap<BuildingTypes, int>& getPrereqNumOfBuildings() const    { return m_aPrereqNumOfBuilding; }    // REAL requires.build BUILDING (empire) min
	const IDValueMap<UnitCombatTypes, int>& getUnitCombatExtraStrength() const { return m_aUnitCombatExtraStrength; }   // REAL strength.city.unitCombats.<UC>
	const IDValueMap<UnitTypes, int>& getUnitProductionModifiers() const     { return m_aUnitProductionModifier; } // REAL buildRate.city.units.<UNIT>
	const IDValueMap<BuildingTypes, CommerceArray>& getGlobalBuildingCommerceChanges() const { return m_aGlobalBuildingCommerceChanges; }  // REAL <commerce>.empire.buildings.<BUILDING>
	const IDValueMap<TechTypes, YieldArray>& getTechYieldChanges100() const  { return m_techYieldChanges; }        // REAL <yield>.city flat enabled TECH (x100)
	const IDValueMap<TechTypes, YieldArray>& getTechYieldModifiers() const   { return m_techYieldModifiers; }      // REAL <yield>.city percent enabled TECH
	const IDValueMap<TechTypes, CommerceArray>& getTechCommerceChanges100() const { return m_techCommerceChanges; }// REAL <commerce>.city flat enabled TECH (x100)
	const IDValueMap<TechTypes, CommerceArray>& getTechCommerceModifiers() const { return m_techCommerceModifiers; }// REAL <commerce>.city percent enabled TECH
	const IDValueMap<ImprovementTypes, YieldArray>& getImprovementYieldChanges() const { return m_aImprovementYieldChanges; }  // REAL <yield>.city.improvements.<IMP>
	const IDValueMap<ImprovementTypes, YieldArray>& getGlobalImprovementYieldChanges() const { return m_aGlobalImprovementYieldChanges; } // REAL <yield>.empire.improvements.<IMP>
	// the doPostLoadCaching upgrade-chain expansion's writers (a building yield keyed to an improvement also
	// lands on its upgrade DESCENDANTS -- the authored row keys the ancestor only)
	void addImprovementYieldRow(ImprovementTypes eImp, const YieldArray& yields)       { m_aImprovementYieldChanges.addArrayValue(eImp, yields); }
	void addGlobalImprovementYieldRow(ImprovementTypes eImp, const YieldArray& yields) { m_aGlobalImprovementYieldChanges.addArrayValue(eImp, yields); }
	const IDValueMap<BuildingTypes, int>& getBuildingProductionModifiers() const { return m_aBuildingProductionModifier; }       // REAL buildRate.city.buildings.<BUILDING>
	const IDValueMap<BuildingTypes, int>& getGlobalBuildingProductionModifiers() const { return m_aGlobalBuildingProductionModifier; } // REAL buildRate.empire.buildings.<BUILDING>
	const IDValueMap<BuildingTypes, int>& getGlobalBuildingCostModifiers() const { return m_aGlobalBuildingCostModifier; }       // REAL costs.empire percent enabled BUILDING
	const IDValueMap<TechTypes, int>& getTechHappinessChanges() const        { return m_aTechHappinessChanges; }   // REAL happiness.city flat enabled TECH
	const IDValueMap<TechTypes, int>& getTechHealthChanges() const           { return m_aTechHealthChanges; }      // REAL health.city flat enabled TECH
	const std::vector<ImprovementTypes>& getPrereqOrImprovements() const     { return m_prereqOrImprovement; }     // REAL requires.build OR IMPROVEMENT (plot)
	const std::vector<BonusTypes>& getPrereqOrBonuses() const                { return m_aePrereqOrBonuses; }       // REAL requires.operate OR BONUS (trade|vicinity)
	const std::vector<BonusTypes>& getPrereqOrVicinityBonuses() const        { return m_piPrereqOrVicinityBonuses; } // REAL requires.operate OR BONUS (vicinity connected)
	const std::vector<BonusTypes>& getPrereqOrRawVicinityBonuses() const     { return m_aePrereqOrRawVicinityBonuses; } // REAL requires.operate OR BONUS (vicinity owned)
	const std::vector<HeritageTypes>& getPrereqOrHeritage() const            { return m_prereqOrHeritage; }        // REAL requires.build OR HERITAGE
	const std::vector<TechTypes>& getPrereqAndTechs() const                  { return m_piPrereqAndTechs; }        // REAL requires.build AND TECH atoms
	const std::vector<MapCategoryTypes>& getMapCategories() const            { return m_aeMapCategoryTypes; }      // REAL (identity.mapCategories)
	const std::vector<FreePromoTypes>& getFreePromoTypes() const             { return m_aFreePromoTypes; }         // REAL grants.freePromotions (populated in mapFrom)
	const std::vector<TraitTypes>& getFreeTraitTypes() const                 { return m_aiFreeTraitTypes; }        // REAL enables.traits (populated in mapFrom)
	const CvProperties* getProperties() const                { return &m_Properties; }              // EMPTY by ruling -- one-shots re-classified per-turn (property-audit.md one-shot ruling); nothing is held
	const CvProperties* getPropertiesAllCities() const        { return &m_PropertiesAllCities; }      // EMPTY by ruling -- ditto (the empire entries ride getPropertyManipulatorsAllCities)
	const CvProperties* getPrereqMinProperties() const        { return &m_PrereqMinProperties; }      // STUB empty
	const CvProperties* getPrereqMaxProperties() const        { return &m_PrereqMaxProperties; }      // STUB empty
	const CvProperties* getPrereqPlayerMinProperties() const  { return &m_PrereqPlayerMinProperties; }// STUB empty
	const CvProperties* getPrereqPlayerMaxProperties() const  { return &m_PrereqPlayerMaxProperties; }// STUB empty
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; } // fed from the PROPERTY_*.{city|plot} families in mapFrom (property-audit.md increments B+4)
	// The building's EMPIRE-scope per-turn sources (the converted <PropertiesAllCities> one-shots, curated
	// PROPERTY_X.empire.flat): delivered in EVERY city of the owner by the CvGameObjectCity::foreachManipulator
	// all-cities walk, count-scaled (property-audit.md one-shot ruling / revived increment 5).
	const CvPropertyManipulators* getPropertyManipulatorsAllCities() const { return &m_PropertyManipulatorsAllCities; }
	const BoolExpr* getConstructCondition() const { return NULL; }   // CURATOR-GAP (by design): ConstructCondition dissolved into requires.build atoms (boolexpr.merge_into); no standalone BoolExpr emitted

	// --- Python-binding list wrappers (CyInfoInterface1 .def-binds these). Each returns a boost::python::list built
	// from the matching keyed getter; every such backing is STUB-empty on this poco, so each wrapper yields an empty
	// list (behaviour-equivalent to the archived body over empty data). Bodies live in the .cpp (the `python` alias +
	// boost::python come from the PCH). Exact archived signatures so the .def(&CvBuildingInfo::cyGet...) binds. ---
	const python::list cyGetGlobalBuildingCommerceChanges() const;
	const python::list cyGetTechYieldChanges100() const;
	const python::list cyGetTechYieldModifiers() const;
	const python::list cyGetTechCommerceChanges100() const;
	const python::list cyGetTechCommerceModifiers() const;
	const python::list cyGetTerrainYieldChanges() const;
	const python::list cyGetPlotYieldChanges() const;
	const python::list cyGetImprovementYieldChanges() const;
	const python::list cyGetGlobalImprovementYieldChanges() const;
	const python::list cyGetFreePromoTypes() const;

	// --- STUB int* array getters -- NULL is the idiomatic "no data" sentinel these C2C accessors already use
	// (consumers guard via the isAny*/getNum* companions), never a live pointer here. ---
	int* getYieldChangeArray() const { return NULL; }
	int* getYieldPerPopChangeArray() const { return NULL; }
	int* getYieldModifierArray() const { return NULL; }
	int* getPowerYieldModifierArray() const { return NULL; }
	int* getAreaYieldModifierArray() const { return NULL; }
	int* getGlobalYieldModifierArray() const { return NULL; }
	int* getRiverPlotYieldChangeArray() const { return NULL; }
	int* getGlobalSeaPlotYieldChangeArray() const { return NULL; }
	int* getCommerceChangeArray() const { return NULL; }
	int* getCommercePerPopChangeArray() const { return NULL; }
	int* getCommerceModifierArray() const { return NULL; }
	int* getGlobalCommerceModifierArray() const { return NULL; }
	int* getSpecialistExtraCommerceArray() const { return NULL; }
	int* getStateReligionCommerceArray() const { return NULL; }
	int* getSpecialistYieldChangeArray(int /*i*/) const { return NULL; }
	int* getSpecialistCommerceChangeArray(int /*i*/) const { return NULL; }
	int* getBonusYieldModifierArray(int /*i*/) const { return NULL; }
	int* getBonusCommerceModifierArray(int /*i*/) const { return NULL; }
	int* getBonusYieldChangesArray(int /*i*/) const { return NULL; }
	int* getVicinityBonusYieldChangesArray(int /*i*/) const { return NULL; }
	int* getBonusCommercePercentChangesArray(int /*i*/) const { return NULL; }
	int* getTechSpecialistChangeArray(int /*i*/) const { return NULL; }
	int* getLocalSpecialistYieldChangeArray(int /*i*/) const { return NULL; }
	int* getLocalSpecialistCommerceChangeArray(int /*i*/) const { return NULL; }

	// --- 2-D / paired scalar accessors -- REAL from the keyed maps where emitted; CURATOR-GAP (0/false) where dropped ---
	int getBonusProductionModifier(int i) const     { return m_bonusProductionModifier.getValue((BonusTypes)i); }   // REAL buildRate.self percent enabled BONUS
	int getDomainFreeExperience(int i) const        { return m_domainFreeExperience.getValue((DomainTypes)i); }     // REAL experience.city.domains.<DOMAIN>
	bool isAnyDomainFreeExperience() const          { return !m_domainFreeExperience.empty(); }
	int getDomainProductionModifier(int i) const    { return m_domainProductionModifier.getValue((DomainTypes)i); } // REAL buildRate.city.domains.<DOMAIN>
	int getBonusDefenseChanges(int i) const         { return m_bonusDefenseChanges.getValue((BonusTypes)i); }       // REAL defense.city.bonuses.<BONUS>
	int getSpecialistCount(int i) const             { return m_specialistCount.getValue((SpecialistTypes)i); }      // REAL allowedSpecialists.city.<SPECIALIST> (unconditioned)
	int getFreeSpecialistCount(int i) const         { return m_freeSpecialistCount.getValue((SpecialistTypes)i); }  // REAL freeSpecialists.city.<SPECIALIST>
	int getCommerceHappiness(int i) const;                              // REAL commerceHappiness.city.<commerce>.flat
	int getVictoryThreshold(int i) const { return mapGet(m_victoryThresholds, i); }   // REAL identity.victoryThresholds {VICTORY_X:n}
	int getImprovementYieldChanges(int i, int j) const { return arrSlot(m_aImprovementYieldChanges, i, j, NUM_YIELD_TYPES); }   // REAL <yield>.city.improvements.<IMP>
	int getSpecialistYieldChange(int /*i*/, int /*j*/) const   { return 0; }   // CURATOR-GAP: SpecialistYieldChanges dropped building-side (specialist-owned, curate_specialist)
	int getSpecialistCommerceChange(int /*i*/, int /*j*/) const { return 0; }   // CURATOR-GAP: SpecialistCommerceChanges dropped building-side
	int getBonusYieldModifier(int i, int j) const      { return arrSlot(m_bonusYieldModifier, i, j, NUM_YIELD_TYPES); }       // REAL <yield>.city percent enabled BONUS
	int getBonusCommerceModifier(int /*i*/, int /*j*/) const   { return 0; }   // CURATOR-GAP: BonusCommerceModifiers (the rate modifier) not emitted by curate_building.py
	int getBonusYieldChanges(int i, int j) const       { return arrSlot(m_bonusYieldChanges, i, j, NUM_YIELD_TYPES); }         // REAL <yield>.city flat enabled BONUS
	int getVicinityBonusYieldChanges(int i, int j) const { return arrSlot(m_vicinityBonusYieldChanges, i, j, NUM_YIELD_TYPES); } // REAL <yield>.city flat enabled BONUS(vicinity connected)
	int getBonusCommercePercentChanges(int i, int j) const { return arrSlot(m_bonusCommercePercentChanges, i, j, NUM_COMMERCE_TYPES); } // REAL <commerce>.city flat enabled BONUS (x100)
	int getTechSpecialistChange(int i, int j) const    { return nestedGet(m_techSpecialistChange, i, j); }   // REAL allowedSpecialists.city.<SPECIALIST> enabled TECH
	int getLocalSpecialistYieldChange(int /*i*/, int /*j*/) const  { return 0; }   // CURATOR-GAP: LocalSpecialistYieldChanges dropped building-side
	int getLocalSpecialistCommerceChange(int /*i*/, int /*j*/) const { return 0; } // CURATOR-GAP: LocalSpecialistCommerceChanges dropped building-side
	int getGlobalBuildingCommerceChange(BuildingTypes eB, CommerceTypes eC) const { return arrSlot(m_aGlobalBuildingCommerceChanges, (int)eB, (int)eC, NUM_COMMERCE_TYPES); }   // REAL <commerce>.empire.buildings.<BUILDING>
	int getTechHappiness(TechTypes eTech) const { return m_aTechHappinessChanges.getValue(eTech); }   // REAL happiness.city flat enabled TECH
	int getTechHealth(TechTypes eTech) const    { return m_aTechHealthChanges.getValue(eTech); }      // REAL health.city flat enabled TECH
	bool isAnySpecialistYieldChanges() const        { return false; }   // CURATOR-GAP (dropped building-side)
	bool isAnySpecialistCommerceChanges() const     { return false; }   // CURATOR-GAP
	bool isAnyBonusYieldModifiers() const           { return !m_bonusYieldModifier.empty(); }
	bool isAnyTechSpecialistChanges() const         { return !m_techSpecialistChange.empty(); }
	bool isAnyBonusCommerceModifiers() const        { return false; }   // CURATOR-GAP (rate modifier not emitted)
	bool isAnyBonusYieldChanges() const             { return !m_bonusYieldChanges.empty(); }
	bool isAnyVicinityBonusYieldChanges() const     { return !m_vicinityBonusYieldChanges.empty(); }
	bool isAnyBonusCommercePercentChanges() const   { return !m_bonusCommercePercentChanges.empty(); }
	bool isAnyLocalSpecialistYieldChanges() const   { return false; }   // CURATOR-GAP
	bool isAnyLocalSpecialistCommerceChanges() const { return false; }  // CURATOR-GAP

	// --- prereq/replacement/category/unitCombat list accessors -- REAL from requires/grants where emitted ---
	int getPrereqOrBuilding(int i) const            { return vecAt(m_prereqOrBuildings, i); }        // REAL requires.build OR BUILDING (city)
	short getNumPrereqOrBuilding() const            { return (short)m_prereqOrBuildings.size(); }
	bool isPrereqOrBuilding(int i) const            { return vecHas(m_prereqOrBuildings, i); }
	int getPrereqInCityBuilding(int i) const        { return vecAt(m_prereqInCityBuildings, i); }    // REAL requires.build AND BUILDING (city)
	short getNumPrereqInCityBuildings() const       { return (short)m_prereqInCityBuildings.size(); }
	bool isPrereqInCityBuilding(int i) const        { return vecHas(m_prereqInCityBuildings, i); }
	int getPrereqNotInCityBuilding(int i) const     { return vecAt(m_prereqNotInCityBuildings, i); } // REAL requires.build noneOf BUILDING
	short getNumPrereqNotInCityBuildings() const    { return (short)m_prereqNotInCityBuildings.size(); }
	int getReplacementBuilding(int i) const         { const std::vector<int>& d = getRequires()->dormantTriggers; return (i >= 0 && i < (int)d.size()) ? d[i] : -1; }  // REAL requires.operate.dormant
	short getNumReplacementBuilding() const         { return (short)getRequires()->dormantTriggers.size(); }
	void setReplacedBuilding(int i)                 { m_replacedBuildings.push_back(i); }   // RUNTIME reverse index (set post-load by the replacers)
	int getReplacedBuilding(int i) const            { return vecAt(m_replacedBuildings, i); }
	short getNumReplacedBuilding() const            { return (short)m_replacedBuildings.size(); }
	int getCategory(int /*i*/) const                { return -1; }   // CURATOR-GAP zero-corpus: ID_LIST emits identity.categories, but no building in the corpus authors Categories
	int getNumCategories() const                    { return 0; }
	bool isCategory(int /*i*/) const                { return false; }
	int getUnitCombatRetrainType(int /*i*/) const   { return -1; }   // CURATOR-GAP zero-corpus: identity.unitCombatRetrainTypes emitted by ID_LIST, no building authors it
	int getNumUnitCombatRetrainTypes() const        { return 0; }
	bool isUnitCombatRetrainType(int /*i*/) const   { return false; }
	int getMayDamageAttackingUnitCombatType(int i) const { return (i >= 0 && i < (int)m_aiMayDamageUnitCombats.size()) ? m_aiMayDamageUnitCombats[i] : -1; }   // defense.city.counterDamage.units.unitCombats[]
	int getNumMayDamageAttackingUnitCombatTypes() const  { return (int)m_aiMayDamageUnitCombats.size(); }
	bool isMayDamageAttackingUnitCombatType(int i) const { for (size_t j = 0; j < m_aiMayDamageUnitCombats.size(); ++j) if (m_aiMayDamageUnitCombats[j] == i) return true; return false; }
	int getNumUnitCombatDefenseAgainstModifiers() const { return idvCount(m_unitCombatDefenseAgainst); }
	int getUnitCombatDefenseAgainstModifier(int i) const { return m_unitCombatDefenseAgainst.getValue((UnitCombatTypes)i); }   // REAL defense.city.unitCombats.<UC>
	int getNumUnitCombatProdModifiers() const       { return idvCount(m_unitCombatProdModifier); }
	int getUnitCombatProdModifier(int i) const      { return m_unitCombatProdModifier.getValue((UnitCombatTypes)i); }   // REAL buildRate.city.unitCombats.<UC>
	int getNumHealUnitCombatTypes() const           { return (int)m_healUnitCombats.size(); }   // REAL grants.repeatable[] unitCombat heal
	const HealUnitCombat& getHealUnitCombatType(int iIndex) const;
	int getNumBonusAidModifiers() const             { return 0; }   // CURATOR-GAP: BonusAidModifiers dropped as DEAD (curate_building.py:758)
	const BonusAidModifiers& getBonusAidModifier(int iIndex) const;
	int getNumAidRateChanges() const                { return 0; }   // CURATOR-GAP: AidRateChanges dropped as DEAD
	const AidRateChanges& getAidRateChange(int iIndex) const;
	int getNumEnabledCivilizationTypes() const      { return (int)m_enabledCivTypes.size(); }   // REAL identity.enabledCivilizations
	const EnabledCivilizations& getEnabledCivilizationType(int iIndex) const;

	// --- predicate helpers (requires civic/terrain/feature) -- REAL ---
	bool isPrereqOrCivics(int i) const { return vecHas(m_prereqOrCivics, i); }     // REAL requires.operate OR CIVIC
	bool isPrereqAndCivics(int i) const { return vecHas(m_prereqAndCivics, i); }   // REAL requires.operate AND CIVIC
	bool isPrereqOrTerrain(int i) const { return vecHas(m_prereqOrTerrains, i); }  // REAL requires.build OR TERRAIN
	bool isPrereqAndTerrain(int i) const { return vecHas(m_prereqAndTerrains, i); }// REAL requires.build AND TERRAIN
	bool isPrereqOrFeature(int i) const { return vecHas(m_prereqOrFeatures, i); }  // REAL requires.build OR FEATURE
	bool isHurry(int i) const   // REAL enables.hurries edge (HURRY_* FK ids)
	{ const std::vector<int>* v = getEdges()->find(EDGEF_ENABLES, EDGEB_HURRIES); if (!v) return false;
	  for (std::size_t k = 0; k < v->size(); ++k) if ((*v)[k] == i) return true; return false; }
	bool isNewCityFree(const CvGameObject* /*pObject*/) const { return false; }  // CURATOR-GAP (by design): NewCityFree relocated onto settler grants.foundBuildings (curate_unit); nothing emitted building-side

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()     const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()        const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()       const { return &m_grants; }
	virtual const CvJsonProvides*  getProvides()     const { return &m_provides; }
	virtual const CvJsonModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvJsonModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	virtual const CvJsonBoolBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvJsonBoolBlock* getCapabilities() const { return &m_capabilities; }

protected:
	virtual CvJsonRequires*  mutRequires()     { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()        { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()       { return &m_grants; }
	virtual CvJsonProvides*  mutProvides()     { return &m_provides; }
	virtual CvJsonModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvJsonModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvJsonBoolBlock* mutAttributes()   { return &m_attributes; }
	virtual CvJsonBoolBlock* mutCapabilities() { return &m_capabilities; }

private:
	static int mapGet(const std::map<int, int>& m, int k)
	{ std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }
	static bool vecHas(const std::vector<int>& v, int x)
	{ for (std::size_t i = 0; i < v.size(); ++i) if (v[i] == x) return true; return false; }
	static int vecAt(const std::vector<int>& v, int i)
	{ return (i >= 0 && i < (int)v.size()) ? v[i] : -1; }
	// Read one slot of an array-valued IDValueMap without instantiating getValue (which returns the scalar
	// defaultValue and would not convert to Yield/CommerceArray). Iterates the public pair range.
	template <class MapT> static int arrSlot(const MapT& m, int id, int slot, int n)
	{
		if (slot < 0 || slot >= n) return 0;
		for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it)
			if ((int)it->first == id) return it->second[slot];
		return 0;
	}
	static int nestedGet(const std::map<int, std::map<int, int> >& m, int i, int j)
	{ std::map<int, std::map<int, int> >::const_iterator t = m.find(i); if (t == m.end()) return 0;
	  std::map<int, int>::const_iterator s = t->second.find(j); return s != t->second.end() ? s->second : 0; }
	template <class MapT> static int idvCount(const MapT& m)
	{ int n = 0; for (typename MapT::const_iterator it = m.begin(); it != m.end(); ++it) ++n; return n; }

	// GROUP 1 (requires condition tree) + GROUP 2 (keyed m_modifiers) reconstruction -- the ONE load-time walk that
	// populates the legacy-shaped members below from the composed units. Called at the end of mapFrom.
	void reconstructFromComposed();

	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonProvides  m_provides;
	CvJsonModifiers m_modifiers;
	CvJsonModifiers m_whenObsolete;
	CvJsonBoolBlock m_attributes;
	CvJsonBoolBlock m_capabilities;

	// --- typed members mapped from the JSON (REAL data) ---
	std::string m_szArtDefineTag;   // world.art.icon (ART_DEF_* tag; the ARTFILEMGR lookup key)
	std::string m_szMovieDefineTag; // ui.art.movie.defineTag (ART_DEF_MOVIE_* tag)
	std::string m_szConstructSound; // sound.construct
	int m_iMaxStartEra, m_iAdvisorType, m_iGreatPeopleUnitType, m_iPromotionLineType;
	int m_iExtendsBuilding, m_iProductionContinueBuilding, m_iSpecialBuilding;
	int m_iAirlift, m_iAirUnitCapacity, m_iDCMAirbombMission, m_iLineOfSight, m_iWorkableRadius;
	int m_iNumPopulationEmployed, m_iLinePriority, m_iAssetValue, m_iPowerValue;
	int m_iProductionCost, m_iProductionCostSize, m_iProductionCostCount, m_iProductionCostMaterials, m_iProductionCostComplexity;
	int m_iAIWeight;                // ai.behaviour.weight
	bool m_bCenterInCity, m_bAllowsNukes, m_bNoLimit;
	float m_fVisibilityPriority;
	std::map<int, int> m_flavours;                        // FlavorTypes -> weight (ai.flavours)
	std::map<int, int> m_victoryThresholds;               // VictoryTypes -> threshold (identity.victoryThresholds)
	std::vector<MapCategoryTypes> m_aeMapCategoryTypes;   // identity.mapCategories
	int m_iMissionType;             // RUNTIME (set post-load, NOT JSON; mirrors CvCorporationInfo)

	// --- MATERIALIZED §6 scalar/positional members (mapFrom-filled via JsonModScan, same address table as the old
	// per-call reads; the getters are bare member reads -- per-call string-address walks are banned from getters) ---
	int m_iHappiness, m_iAreaHappiness, m_iGlobalHappiness, m_iHealth, m_iAreaHealth, m_iGlobalHealth;
	int m_aiYieldChange[NUM_YIELD_TYPES], m_aiYieldModifier[NUM_YIELD_TYPES], m_aiAreaYieldModifier[NUM_YIELD_TYPES];
	int m_aiGlobalYieldModifier[NUM_YIELD_TYPES], m_aiGlobalSeaPlotYieldChange[NUM_YIELD_TYPES];
	int m_aiCommerceChange[NUM_COMMERCE_TYPES], m_aiCommerceModifier[NUM_COMMERCE_TYPES], m_aiGlobalCommerceModifier[NUM_COMMERCE_TYPES];
	int m_aiSpecialistExtraCommerce[NUM_COMMERCE_TYPES], m_aiCommerceHappiness[NUM_COMMERCE_TYPES];
	int m_aiCommerceChangeDoubleTime[NUM_COMMERCE_TYPES], m_aiStateReligionCommerce[NUM_COMMERCE_TYPES];
	int m_iEnemyWarWearinessModifier, m_iOccupationTimeModifier, m_iHealRateChange, m_iFoodKept;
	int m_iGreatPeopleRateChange, m_iGreatPeopleRateModifier, m_iGlobalGreatPeopleRateModifier;
	int m_iGreatGeneralRateModifier, m_iDomesticGreatGeneralRateModifier;
	int m_iMaintenanceModifier, m_iGlobalMaintenanceModifier, m_iAreaMaintenanceModifier, m_iOtherAreaMaintenanceModifier;
	int m_iDistanceMaintenanceModifier, m_iNumCitiesMaintenanceModifier, m_iCoastalDistanceMaintenanceModifier, m_iConnectedCityMaintenanceModifier;
	int m_iInflationModifier, m_iWarWearinessModifier, m_iGlobalWarWearinessModifier;
	int m_iHurryCostModifier, m_iGlobalHurryModifier, m_iHurryAngerModifier;
	int m_iMilitaryProductionModifier, m_iSpaceProductionModifier, m_iGlobalSpaceProductionModifier, m_iWorkerSpeedModifier;
	int m_iTradeRoutes, m_iCoastalTradeRoutes, m_iGlobalTradeRoutes, m_iWorldTradeRoutes, m_iTradeRouteModifier, m_iForeignTradeRouteModifier;
	int m_iFreeExperience, m_iGlobalFreeExperience, m_iFreeSpecialist, m_iAreaFreeSpecialist, m_iGlobalFreeSpecialist;
	int m_iAnarchyModifier, m_iGoldenAgeModifier, m_iPopulationgrowthratepercentage, m_iGlobalPopulationgrowthratepercentage;
	int m_iRevIdxLocal, m_iRevIdxNational, m_iRevIdxDistanceModifier, m_iInsidiousness, m_iInvestigation;
	int m_iEspionageDefenseModifier, m_iUnitUpgradePriceModifier;
	int m_iDefenseModifier, m_iBombardDefenseModifier, m_iAllCityDefenseModifier, m_iNukeModifier, m_iAirModifier;
	int m_iMinDefense, m_iNoEntryDefenseLevel, m_iLocalDynamicDefense, m_iRiverDefensePenalty;
	int m_iBuildingDefenseRecoverySpeedModifier, m_iCityDefenseRecoverySpeedModifier, m_iAdjacentDamagePercent;
	int m_iNationalCaptureProbabilityModifier, m_iNationalCaptureResistanceModifier, m_iLocalCaptureProbabilityModifier, m_iLocalCaptureResistanceModifier;
	bool m_bGrantsGoldenAge;   // grants.goldenAge flag (materialized at mapFrom)

	// --- backing members for the reference-returning getters (populated in reconstructFromComposed where REAL;
	//     a few remain empty for documented curator-gaps -- HARD CONSTRAINT: always return a real member) ---
	std::vector<ConstructRequirement> m_constructRequirements;
	std::vector<BonusTypes> m_consumptionRelevantBonuses;
	IDValueMap<BonusTypes, int> m_freeBonuses;
	IDValueMap<PlotTypes, YieldArray> m_aPlotYieldChanges;
	IDValueMap<TerrainTypes, YieldArray> m_aTerrainYieldChanges;
	IDValueMap<ReligionTypes, int> m_religionChange;
	IDValueMap<BonusTypes, int> m_piBonusHealthChanges;
	IDValueMap<BonusTypes, int> m_piBonusHappinessChanges;
	IDValueMap<UnitCombatTypes, int> m_aUnitCombatFreeExperience;
	IDValueMap<BuildingTypes, int> m_aBuildingHappinessChanges;
	IDValueMap<ImprovementTypes, int> m_improvementFreeSpecialists;
	IDValueMap<BuildingTypes, int> m_aPrereqNumOfBuilding;
	IDValueMap<UnitCombatTypes, int> m_aUnitCombatExtraStrength;
	IDValueMap<UnitTypes, int> m_aUnitProductionModifier;
	IDValueMap<BuildingTypes, CommerceArray> m_aGlobalBuildingCommerceChanges;
	IDValueMap<TechTypes, YieldArray> m_techYieldChanges;
	IDValueMap<TechTypes, YieldArray> m_techYieldModifiers;
	IDValueMap<TechTypes, CommerceArray> m_techCommerceChanges;
	IDValueMap<TechTypes, CommerceArray> m_techCommerceModifiers;
	IDValueMap<ImprovementTypes, YieldArray> m_aImprovementYieldChanges;
	IDValueMap<ImprovementTypes, YieldArray> m_aGlobalImprovementYieldChanges;
	IDValueMap<BuildingTypes, int> m_aBuildingProductionModifier;
	IDValueMap<BuildingTypes, int> m_aGlobalBuildingProductionModifier;
	IDValueMap<BuildingTypes, int> m_aGlobalBuildingCostModifier;
	IDValueMap<TechTypes, int> m_aTechHappinessChanges;
	IDValueMap<TechTypes, int> m_aTechHealthChanges;
	std::vector<ImprovementTypes> m_prereqOrImprovement;
	std::vector<BonusTypes> m_aePrereqOrBonuses;
	std::vector<BonusTypes> m_piPrereqOrVicinityBonuses;
	std::vector<BonusTypes> m_aePrereqOrRawVicinityBonuses;
	std::vector<HeritageTypes> m_prereqOrHeritage;
	std::vector<TechTypes> m_piPrereqAndTechs;
	std::vector<FreePromoTypes> m_aFreePromoTypes;
	std::vector<TraitTypes> m_aiFreeTraitTypes;
	CvProperties m_Properties;
	CvProperties m_PropertiesAllCities;
	CvProperties m_PrereqMinProperties;
	CvProperties m_PrereqMaxProperties;
	CvProperties m_PrereqPlayerMinProperties;
	CvProperties m_PrereqPlayerMaxProperties;
	CvPropertyManipulators m_PropertyManipulators;
	CvPropertyManipulators m_PropertyManipulatorsAllCities;   // empire-scope per-turn sources (converted <PropertiesAllCities>)

	// ===== GROUP 1: requires-condition-tree reconstruction (populated in reconstructFromComposed) =====
	int m_iMinAreaSize, m_iMinLatitude, m_iMaxLatitude, m_iNumCitiesPrereq, m_iNumTeamsPrereq, m_iPrereqPopulation;
	int m_iVictoryPrereq, m_iHolyCity, m_iPrereqStateReligion, m_iPrereqReligion, m_iPrereqCorporation;
	int m_iPrereqCultureLevel, m_iPrereqAnyoneBuilding, m_iPrereqAndTech, m_iPrereqAndBonus, m_iPrereqVicinityBonus, m_iPrereqRawVicinityBonus;
	int m_iStateReligionHappiness;
	bool m_bNeedStateReligionInCity, m_bWater, m_bRiver, m_bFreshWater, m_bNoHolyCity, m_bPower;
	std::vector<int> m_prereqInCityBuildings, m_prereqNotInCityBuildings, m_prereqOrBuildings, m_replacedBuildings;
	std::vector<int> m_prereqAndCivics, m_prereqOrCivics, m_prereqAndTerrains, m_prereqOrTerrains, m_prereqOrFeatures;
	std::vector<EnabledCivilizations> m_enabledCivTypes;   // identity.enabledCivilizations
	// clean grant/repeatable reads:
	int m_iNumUnitFullHeal, m_iPropertySpawnUnit, m_iPropertySpawnProperty;
	std::vector<HealUnitCombat> m_healUnitCombats;         // grants.repeatable[] unitCombat heal

	// ===== GROUP 2: keyed-modifier reconstruction (new backing maps; the reused maps are declared above) =====
	IDValueMap<BonusTypes, YieldArray> m_bonusYieldChanges;        // <yield>.city flat enabled BONUS
	IDValueMap<BonusTypes, YieldArray> m_bonusYieldModifier;       // <yield>.city percent enabled BONUS
	IDValueMap<BonusTypes, YieldArray> m_vicinityBonusYieldChanges;// <yield>.city flat enabled BONUS(vicinity connected)
	IDValueMap<BonusTypes, CommerceArray> m_bonusCommercePercentChanges; // <commerce>.city flat enabled BONUS (x100)
	IDValueMap<BonusTypes, int> m_bonusDefenseChanges;            // defense.city.bonuses.<BONUS>
	IDValueMap<BonusTypes, int> m_bonusProductionModifier;        // buildRate.self percent enabled BONUS
	IDValueMap<DomainTypes, int> m_domainFreeExperience;          // experience.city.domains.<DOMAIN>
	IDValueMap<DomainTypes, int> m_domainProductionModifier;      // buildRate.city.domains.<DOMAIN>
	IDValueMap<UnitCombatTypes, int> m_unitCombatProdModifier;    // buildRate.city.unitCombats.<UC>
	IDValueMap<UnitCombatTypes, int> m_unitCombatDefenseAgainst;  // defense.city.unitCombats.<UC>
	IDValueMap<SpecialistTypes, int> m_specialistCount;          // allowedSpecialists.city.<SPECIALIST>
	IDValueMap<SpecialistTypes, int> m_freeSpecialistCount;     // freeSpecialists.city.<SPECIALIST>
	int m_powerYieldModifier[NUM_YIELD_TYPES];                    // <yield>.city percent enabled HAS_POWER
	std::map<int, std::map<int, int> > m_techSpecialistChange;    // tech -> specialist -> count (allowedSpecialists enabled TECH)
};

#endif // CV_JSON_BUILDING_INFO_H
