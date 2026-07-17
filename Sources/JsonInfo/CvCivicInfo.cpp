//
//	CvCivicInfo -- ctor (zero-init the mirrored scalar members), the section-6 read helpers (unconditioned /
//	all / unit-qualified / condition-shape-matched family sums + the target-keyed sparse collector), mapFrom (base
//	section dispatch + the civic-only typed reads: identity scalars, ai.behaviour.weight, and the load-time fill of
//	the keyed sparse vectors), and the out-of-line getter definitions. See the header for the full getter surface +
//	the real-vs-named-gap rationale per field.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, GC
#include "CvCivicInfo.h"
#include "CvJsonParse.h"            // jsonChildObj / jsonIdInt / jsonIdFk / jsonIdStr / jsonResolveId / jsonReadFlavours
#include "CvJsonCondition.h"        // CvCascPredKind / CASC_COND_PREDICATE / CASC_COND_PRESENCE -- condition-shape matching
#include "CvJsonModScan.h"          // the ONE load-time modifier-family scan (mapFrom materialization)
#include "CvCascadePropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "CvBuildingInfo.h"     // getBuildingInfo(...).getType() -- keyed happiness/health/buildRate.empire.buildings
#include "CvUnitInfo.h"         // getUnitInfo(...).getType() -- keyed buildRate.empire.units lookup (complete type)
#include "CvUnitCombatInfo.h"   // getUnitCombatInfo(...).getType() -- keyed buildRate.empire.unitCombats
#include "CvSpecialistInfo.h"   // getSpecialistInfo(...).getType() -- keyed freeSpecialists.empire
#include "CvFeatureInfo.h"     // getFeatureInfo(...).getType() -- keyed happiness.empire.features (EXE shim leaf)
#include "CvImprovementInfo.h" // getImprovementInfo(...).getType() -- keyed {yield}.empire.improvements
#include "CvTerrainInfo.h"     // getTerrainInfo(...).getType() -- keyed {yield}.empire.terrains

CvCivicInfo::CvCivicInfo()
	: m_iCivicOptionType(NO_CIVICOPTION), m_iAnarchyLength(0), m_iUpkeep(-1), m_iAIWeight(0)
{
	// Zero the dense positional arrays (mapFrom overwrites every slot, but a non-object entity early-returns before it).
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiYieldModifier[y] = 0; m_aiCapitalYieldModifier[y] = 0; m_aiTradeYieldModifier[y] = 0; m_aiLandmarkYieldChanges[y] = 0;
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		m_aiCommerceModifier[c] = 0; m_aiCapitalCommerceModifier[c] = 0; m_aiSpecialistExtraCommerce[c] = 0;
	}
	// Zero the materialized scalar members (same guard: mapFrom fully redefines them on a real entity).
	m_iMaxConscript = m_iCityLimitBase = 0;
	m_iGreatPeopleRateModifier = m_iGreatGeneralRateModifier = m_iDomesticGreatGeneralRateModifier = m_iStateReligionGreatPeopleRateModifier = 0;
	m_iDistanceMaintenanceModifier = m_iNumCitiesMaintenanceModifier = m_iHomeAreaMaintenanceModifier = m_iOtherAreaMaintenanceModifier = m_iCorporationMaintenanceModifier = 0;
	m_iExtraHealth = m_iFreeExperience = m_iWorkerSpeedModifier = m_iImprovementUpgradeRateModifier = m_iMilitaryProductionModifier = 0;
	m_iFreeUnitUpkeepCivilian = m_iFreeUnitUpkeepMilitary = m_iFreeUnitUpkeepCivilianPopPercent = m_iFreeUnitUpkeepMilitaryPopPercent = 0;
	m_iCivilianUnitUpkeepMod = m_iMilitaryUnitUpkeepMod = m_iWarWearinessModifier = m_iFreeSpecialist = m_iTradeRoutes = 0;
	m_iCivicPercentAnger = m_iStateReligionHappiness = m_iNonStateReligionHappiness = m_iStateReligionUnitProductionModifier = 0;
	m_iStateReligionBuildingProductionModifier = m_iStateReligionFreeExperience = m_iExpInBorderModifier = 0;
	m_iRevIdxLocal = m_iRevIdxNational = m_iRevIdxDistanceModifier = m_iRevIdxHolyCityGood = m_iRevIdxHolyCityBad = m_iRevIdxSwitchTo = 0;
	m_iRevReligiousFreedom = m_iRevLaborFreedom = m_iRevEnvironmentalProtection = m_iRevDemocracyLevel = 0;
	m_iAttitudeShareMod = m_iForeignerUnhappyPercent = m_iCityOverLimitUnhappy = m_iForeignTradeRouteModifier = m_iTaxRateUnhappiness = 0;
	m_iPopulationgrowthratepercentage = m_iCivicHappiness = m_iDistantUnitSupportCostModifier = m_iExtraCityDefense = 0;
	m_iNationalCaptureProbabilityModifier = m_iNationalCaptureResistanceModifier = m_iInflationModifier = 0;
	m_iHurryInflationModifier = m_iHurryCostModifier = m_iSharedCivicTradeRouteModifier = m_iFreedomFighterChange = 0;
	m_iLandmarkHappiness = m_iLargestCityHappiness = m_iHappyPerMilitaryUnit = 0;
	m_fRevIdxNationalityMod = m_fRevIdxBadReligionMod = m_fRevIdxGoodReligionMod = m_fRevViolentMod = 0.0f;
	m_bAnyImprovementYieldChange = false;
}

// ===================== section-6 read plumbing =====================
//
// The family-sum / keyed-collect walkers live on the ONE shared surface (JsonModScan -- [DEC-single-implementation];
// the per-file civ* duplicates are gone). ALL of them run at mapFrom time ONLY: the curator collapses a civic's
// unconditioned SCALAR and any conditioned / qualified addend onto the SAME address, so the materialization pass
// recovers each legacy field by condition SHAPE once at load; every getter is then a bare member read.

static const char* CIV_YIELD_NAME[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
static const char* CIV_COMM_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

// The archived dense int* accessors returned NULL when nothing was authored (the array was never allocated); the
// UI-help consumers branch on `if (aList)`. Mirror that: NULL when every slot is 0, else the (const-cast) array.
static int* civArrOrNull(const int* arr, int n)
{
	for (int i = 0; i < n; ++i) if (arr[i] != 0) return const_cast<int*>(arr);
	return NULL;
}

// mapFrom-time keyed fill: collect a target-keyed family prefix into a plain id->value map.
static void civFillKeyedMap(const CvJsonModifiers* mods, const std::string& prefix, CvCascUnit unit, std::map<int, int>& out)
{
	out.clear();
	std::vector<std::pair<int, int> > v;
	JsonModScan::collectKeyedSparse<int>(mods, prefix, unit, v);
	for (size_t k = 0; k < v.size(); ++k) out[v[k].first] = v[k].second;
}

// ========================================== mapFrom ==========================================

void CvCivicInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the sparse keyed caches append and must clear; the IDValueMap
	// (m_aBuildingProductionModifier) needs none -- setValue updates-or-appends per key, idempotent on a re-parse.
	m_vBuildingHappinessChangesSparse.clear(); m_vBuildingHealthChangesSparse.clear();
	m_vFeatureHappinessChangesSparse.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units (fills m_modifiers)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// identity.civicOption (CIVICOPTION_* FK) / anarchyLength / upkeepLevel (UPKEEP_* FK) / weLoveTheKing
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iCivicOptionType = jsonIdFk(*io, "civicOption");
		m_iAnarchyLength   = jsonIdInt(*io, "anarchyLength");
		m_iUpkeep          = jsonIdFk(*io, "upkeepLevel");
		std::string s;
		if (jsonIdStr(*io, "weLoveTheKing", s)) m_szWeLoveTheKingKey = CvWString(s.c_str());
	}

	// ai.behaviour.weight (curate_civic.py: iAIWeight -> ai.behaviour.weight, only emitted when non-zero) + ai.flavours
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		if (const picojson::object* be = jsonChildObj(*ai, "behaviour"))
			m_iAIWeight = jsonIdInt(*be, "weight");
		jsonReadFlavours(*ai, m_flavours);
	}

	// PROPERTY_* per-turn SOURCES: a civic's <PROPERTY_X>.city.flat deposits in EVERY city while the civic is
	// adopted -- the player gather walks civics; RELATION_ASSOCIATED fans each source to every owner city
	// (mirrors the legacy CITY+ASSOCIATED shape). The ONE shared walk (clear-and-refill inside).
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);

	// Fill the sparse (id, value) caches from the now-composed m_modifiers (WRITE-ONCE AT LOAD; the per-index getters
	// read these same materialized structures).
	JsonModScan::collectKeyedSparse<BuildingTypes>(getModifiers(), "happiness.empire.buildings.", CASC_UNIT_FLAT, m_vBuildingHappinessChangesSparse);
	JsonModScan::collectKeyedSparse<BuildingTypes>(getModifiers(), "health.empire.buildings.",    CASC_UNIT_FLAT, m_vBuildingHealthChangesSparse);
	JsonModScan::collectKeyedSparse<FeatureTypes>(getModifiers(),  "happiness.empire.features.",  CASC_UNIT_FLAT, m_vFeatureHappinessChangesSparse);
	JsonModScan::collectKeyedMap<BuildingTypes>(getModifiers(), "buildRate.empire.buildings.", CASC_UNIT_PERCENT, m_aBuildingProductionModifier);

	// ===== the MATERIALIZATION pass: every legacy scalar / positional / keyed getter value is scanned ONCE here
	// (JsonModScan over the composed m_modifiers); the getters are bare member reads. =====
	const CvJsonModifiers* mods = getModifiers();
	m_iMaxConscript                    = JsonModScan::sum(mods, "conscript.empire", CASC_UNIT_FLAT);
	m_iCityLimitBase                   = JsonModScan::sum(mods, "happiness.empire.cityLimit", CASC_UNIT_FLAT);
	m_iGreatPeopleRateModifier         = JsonModScan::sum(mods, "greatPeopleRate.empire", CASC_UNIT_PERCENT);
	m_iGreatGeneralRateModifier        = JsonModScan::sum(mods, "greatGeneralRate.empire", CASC_UNIT_PERCENT);
	m_iDomesticGreatGeneralRateModifier = JsonModScan::sum(mods, "greatGeneralRate.empire.domestic", CASC_UNIT_PERCENT);
	m_iStateReligionGreatPeopleRateModifier = JsonModScan::sum(mods, "stateReligion.empire.greatPeopleRate", CASC_UNIT_PERCENT);
	m_iDistanceMaintenanceModifier     = JsonModScan::sum(mods, "maintenance.empire.distance", CASC_UNIT_PERCENT);
	m_iNumCitiesMaintenanceModifier    = JsonModScan::sum(mods, "maintenance.empire.numCities", CASC_UNIT_PERCENT);
	m_iHomeAreaMaintenanceModifier     = JsonModScan::sum(mods, "maintenance.empire.homeArea", CASC_UNIT_PERCENT);
	m_iOtherAreaMaintenanceModifier    = JsonModScan::sum(mods, "maintenance.empire.otherArea", CASC_UNIT_PERCENT);
	m_iCorporationMaintenanceModifier  = JsonModScan::sum(mods, "maintenance.empire.corporation", CASC_UNIT_PERCENT);
	m_iExtraHealth                     = JsonModScan::sum(mods, "health.empire", CASC_UNIT_FLAT);
	m_iFreeExperience                  = JsonModScan::sum(mods, "experience.empire", CASC_UNIT_FLAT);
	m_iWorkerSpeedModifier             = JsonModScan::sum(mods, "workRate.empire", CASC_UNIT_PERCENT);
	m_iImprovementUpgradeRateModifier  = JsonModScan::sum(mods, "improvementUpgradeRate.empire", CASC_UNIT_PERCENT);
	m_iMilitaryProductionModifier      = JsonModScan::sum(mods, "buildRate.empire.military", CASC_UNIT_PERCENT);
	m_iFreeUnitUpkeepCivilian          = JsonModScan::sum(mods, "upkeep.empire.freeCivilian", CASC_UNIT_FLAT);
	m_iFreeUnitUpkeepMilitary          = JsonModScan::sum(mods, "upkeep.empire.freeMilitary", CASC_UNIT_FLAT);
	m_iFreeUnitUpkeepCivilianPopPercent = JsonModScan::sum(mods, "upkeep.empire.freeCivilian", CASC_UNIT_PER_POPULATION);
	m_iFreeUnitUpkeepMilitaryPopPercent = JsonModScan::sum(mods, "upkeep.empire.freeMilitary", CASC_UNIT_PER_POPULATION);
	m_iCivilianUnitUpkeepMod           = JsonModScan::sum(mods, "upkeep.empire.unitCivilian", CASC_UNIT_PERCENT);
	m_iMilitaryUnitUpkeepMod           = JsonModScan::sum(mods, "upkeep.empire.unitMilitary", CASC_UNIT_PERCENT);
	m_iWarWearinessModifier            = JsonModScan::sum(mods, "diplomacy.empire.warWeariness", CASC_UNIT_PERCENT);
	m_iFreeSpecialist                  = JsonModScan::sum(mods, "freeSpecialists.empire.any", CASC_UNIT_COUNT);
	m_iTradeRoutes                     = JsonModScan::sum(mods, "tradeRoutes.empire", CASC_UNIT_FLAT);
	m_iCivicPercentAnger               = JsonModScan::sum(mods, "happiness.empire.civicAnger", CASC_UNIT_PERCENT);
	m_iStateReligionHappiness          = JsonModScan::sum(mods, "stateReligion.empire.happiness", CASC_UNIT_FLAT);
	m_iNonStateReligionHappiness       = JsonModScan::sum(mods, "happiness.empire.nonStateReligion", CASC_UNIT_FLAT);
	m_iStateReligionUnitProductionModifier = JsonModScan::sum(mods, "stateReligion.empire.unitProduction", CASC_UNIT_PERCENT);
	m_iStateReligionBuildingProductionModifier = JsonModScan::sum(mods, "stateReligion.empire.buildingProduction", CASC_UNIT_PERCENT);
	m_iStateReligionFreeExperience     = JsonModScan::sum(mods, "stateReligion.empire.freeExperience", CASC_UNIT_FLAT);
	m_iExpInBorderModifier             = JsonModScan::sum(mods, "experience.empire.inBorder", CASC_UNIT_PERCENT);
	m_iRevIdxLocal                     = JsonModScan::sum(mods, "revolution.empire.local", CASC_UNIT_FLAT);
	m_iRevIdxNational                  = JsonModScan::sum(mods, "revolution.empire.national", CASC_UNIT_FLAT);
	m_iRevIdxDistanceModifier          = JsonModScan::sum(mods, "revolution.empire.distanceModifier", CASC_UNIT_PERCENT);
	m_iRevIdxHolyCityGood              = JsonModScan::sum(mods, "revolution.empire.holyCityGood", CASC_UNIT_FLAT);
	m_iRevIdxHolyCityBad               = JsonModScan::sum(mods, "revolution.empire.holyCityBad", CASC_UNIT_FLAT);
	m_iRevIdxSwitchTo                  = grantPulse100("revolution") / 100;
	m_iRevReligiousFreedom             = JsonModScan::sum(mods, "revolution.empire.religiousFreedom", CASC_UNIT_FLAT);
	m_iRevLaborFreedom                 = JsonModScan::sum(mods, "revolution.empire.laborFreedom", CASC_UNIT_FLAT);
	m_iRevEnvironmentalProtection      = JsonModScan::sum(mods, "revolution.empire.environmentalProtection", CASC_UNIT_FLAT);
	m_iRevDemocracyLevel               = JsonModScan::sum(mods, "revolution.empire.democracyLevel", CASC_UNIT_FLAT);
	m_iAttitudeShareMod                = JsonModScan::sum(mods, "diplomacy.empire.attitudeShare", CASC_UNIT_FLAT);
	m_iForeignerUnhappyPercent         = JsonModScan::sum(mods, "happiness.empire.foreignerUnhappy", CASC_UNIT_PERCENT);
	m_iCityOverLimitUnhappy            = JsonModScan::sum(mods, "happiness.empire.cityOverLimit", CASC_UNIT_FLAT);
	m_iForeignTradeRouteModifier       = JsonModScan::sum(mods, "tradeRoutes.empire.foreign", CASC_UNIT_PERCENT);
	m_iTaxRateUnhappiness              = JsonModScan::sum(mods, "happiness.empire.taxRate", CASC_UNIT_PERCENT);
	m_iPopulationgrowthratepercentage  = JsonModScan::sum(mods, "growth.empire", CASC_UNIT_PERCENT);
	m_iCivicHappiness                  = JsonModScan::sum(mods, "happiness.empire", CASC_UNIT_FLAT);
	m_iDistantUnitSupportCostModifier  = JsonModScan::sum(mods, "upkeep.empire.supply", CASC_UNIT_PERCENT);
	m_iExtraCityDefense                = JsonModScan::sum(mods, "defense.empire.amount", CASC_UNIT_PERCENT);
	m_iNationalCaptureProbabilityModifier = JsonModScan::sum(mods, "combat.empire.captureProbability", CASC_UNIT_PERCENT);
	m_iNationalCaptureResistanceModifier = JsonModScan::sum(mods, "combat.empire.captureResistance", CASC_UNIT_PERCENT);
	m_iInflationModifier               = JsonModScan::sum(mods, "inflation.empire", CASC_UNIT_PERCENT);
	m_iHurryInflationModifier          = JsonModScan::sum(mods, "hurry.empire.inflation", CASC_UNIT_PERCENT);
	m_iHurryCostModifier               = JsonModScan::sum(mods, "hurry.empire.cost", CASC_UNIT_PERCENT);
	m_iSharedCivicTradeRouteModifier   = JsonModScan::sum(mods, "tradeRoutes.empire.sharedCivic", CASC_UNIT_PERCENT);
	m_iFreedomFighterChange            = JsonModScan::sum(mods, "revolution.empire.freedomFighter", CASC_UNIT_FLAT);
	m_fRevIdxNationalityMod            = JsonModScan::sumF(mods, "revolution.empire.nationalityMod", CASC_UNIT_PERCENT);
	m_fRevIdxBadReligionMod            = JsonModScan::sumF(mods, "revolution.empire.badReligionMod", CASC_UNIT_PERCENT);
	m_fRevIdxGoodReligionMod           = JsonModScan::sumF(mods, "revolution.empire.goodReligionMod", CASC_UNIT_PERCENT);
	m_fRevViolentMod                   = JsonModScan::sumF(mods, "revolution.empire.violentMod", CASC_UNIT_PERCENT);
	m_iLandmarkHappiness               = JsonModScan::sumEnabledPresence(mods, "happiness.empire", CASC_UNIT_FLAT, "GAMEOPTION_MAP_PERSONALIZED");
	m_iLargestCityHappiness            = JsonModScan::sum(mods, "happiness.empire.cities", CASC_UNIT_FLAT);
	m_iHappyPerMilitaryUnit            = JsonModScan::sumUnitQualified(mods, "happiness.empire.cities", CASC_UNIT_FLAT);

	// dense positional arrays (the int* bulk accessors return these NULL-when-all-zero)
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiYieldModifier[y]        = JsonModScan::sum(mods, std::string(CIV_YIELD_NAME[y]) + ".empire", CASC_UNIT_PERCENT);
		m_aiCapitalYieldModifier[y] = JsonModScan::sumEnabledPred(mods, std::string(CIV_YIELD_NAME[y]) + ".empire", CASC_UNIT_PERCENT, CASC_PRED_IS_CAPITAL);
		m_aiTradeYieldModifier[y]   = JsonModScan::sum(mods, std::string(CIV_YIELD_NAME[y]) + ".empire.tradeRoute", CASC_UNIT_PERCENT);
		m_aiLandmarkYieldChanges[y] = JsonModScan::sumAll(mods, std::string(CIV_YIELD_NAME[y]) + ".empire.plots", CASC_UNIT_FLAT);
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		m_aiCommerceModifier[c]        = JsonModScan::sum(mods, std::string(CIV_COMM_NAME[c]) + ".empire", CASC_UNIT_PERCENT);
		m_aiCapitalCommerceModifier[c] = JsonModScan::sumEnabledPred(mods, std::string(CIV_COMM_NAME[c]) + ".empire", CASC_UNIT_PERCENT, CASC_PRED_IS_CAPITAL);
		m_aiSpecialistExtraCommerce[c] = JsonModScan::sum(mods, std::string(CIV_COMM_NAME[c]) + ".empire.specialist", CASC_UNIT_PER_SPECIALIST);
	}

	// keyed per-index maps (the per-index getters are bare map reads)
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		civFillKeyedMap(mods, std::string(CIV_COMM_NAME[c]) + ".empire.buildings.", CASC_UNIT_PERCENT, m_buildingCommerceModifier[c]);
	m_bAnyImprovementYieldChange = false;
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		civFillKeyedMap(mods, std::string(CIV_YIELD_NAME[y]) + ".empire.improvements.", CASC_UNIT_FLAT, m_improvementYieldChanges[y]);
		civFillKeyedMap(mods, std::string(CIV_YIELD_NAME[y]) + ".empire.terrains.",     CASC_UNIT_FLAT, m_terrainYieldChanges[y]);
		if (!m_improvementYieldChanges[y].empty()) m_bAnyImprovementYieldChange = true;
	}
	civFillKeyedMap(mods, "buildRate.empire.units.",       CASC_UNIT_PERCENT, m_unitProductionModifier);
	civFillKeyedMap(mods, "buildRate.empire.unitCombats.", CASC_UNIT_PERCENT, m_unitCombatProductionModifier);
	civFillKeyedMap(mods, "diplomacy.empire.civics.",      CASC_UNIT_FLAT,    m_civicAttitudeChanges);
	civFillKeyedMap(mods, "freeSpecialists.empire.",       CASC_UNIT_COUNT,   m_freeSpecialistCount);
}

// ===================== identity / edge getters =====================

int CvCivicInfo::getMaxConscript() const { return m_iMaxConscript; }
int CvCivicInfo::getCityLimitBase() const { return m_iCityLimitBase; }

bool CvCivicInfo::isHurry(int i) const
{
	const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_HURRIES);
	if (v == NULL) return false;
	for (size_t k = 0; k < v->size(); ++k) if ((*v)[k] == i) return true;
	return false;
}

// --- enables-edge derived bools (curate_civic.py ENABLE_LISTS -> enables.specialists / enables.specialBuildingsWaived) ---
bool CvCivicInfo::isSpecialistValid(int i) const
{
	const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_SPECIALISTS);
	if (v == NULL) return false;
	for (size_t k = 0; k < v->size(); ++k) if ((*v)[k] == i) return true;
	return false;
}
bool CvCivicInfo::isAnySpecialistValid() const
{
	const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_SPECIALISTS);
	return v != NULL && !v->empty();
}
bool CvCivicInfo::isSpecialBuildingNotRequired(int i) const
{
	const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED);
	if (v == NULL) return false;
	for (size_t k = 0; k < v->size(); ++k) if ((*v)[k] == i) return true;
	return false;
}

// ===================== section-6 getters -- bare reads of the mapFrom-materialized members =====================
int CvCivicInfo::getGreatPeopleRateModifier() const           { return m_iGreatPeopleRateModifier; }
int CvCivicInfo::getGreatGeneralRateModifier() const          { return m_iGreatGeneralRateModifier; }
int CvCivicInfo::getDomesticGreatGeneralRateModifier() const  { return m_iDomesticGreatGeneralRateModifier; }
int CvCivicInfo::getStateReligionGreatPeopleRateModifier() const { return m_iStateReligionGreatPeopleRateModifier; }
int CvCivicInfo::getDistanceMaintenanceModifier() const       { return m_iDistanceMaintenanceModifier; }
int CvCivicInfo::getNumCitiesMaintenanceModifier() const      { return m_iNumCitiesMaintenanceModifier; }
int CvCivicInfo::getHomeAreaMaintenanceModifier() const       { return m_iHomeAreaMaintenanceModifier; }
int CvCivicInfo::getOtherAreaMaintenanceModifier() const      { return m_iOtherAreaMaintenanceModifier; }
int CvCivicInfo::getCorporationMaintenanceModifier() const    { return m_iCorporationMaintenanceModifier; }
int CvCivicInfo::getExtraHealth() const                       { return m_iExtraHealth; }
int CvCivicInfo::getFreeExperience() const                    { return m_iFreeExperience; }
int CvCivicInfo::getWorkerSpeedModifier() const               { return m_iWorkerSpeedModifier; }
int CvCivicInfo::getImprovementUpgradeRateModifier() const    { return m_iImprovementUpgradeRateModifier; }
int CvCivicInfo::getMilitaryProductionModifier() const        { return m_iMilitaryProductionModifier; }
int CvCivicInfo::getFreeUnitUpkeepCivilian() const            { return m_iFreeUnitUpkeepCivilian; }
int CvCivicInfo::getFreeUnitUpkeepMilitary() const            { return m_iFreeUnitUpkeepMilitary; }
int CvCivicInfo::getFreeUnitUpkeepCivilianPopPercent() const  { return m_iFreeUnitUpkeepCivilianPopPercent; }
int CvCivicInfo::getFreeUnitUpkeepMilitaryPopPercent() const  { return m_iFreeUnitUpkeepMilitaryPopPercent; }
int CvCivicInfo::getCivilianUnitUpkeepMod() const             { return m_iCivilianUnitUpkeepMod; }
int CvCivicInfo::getMilitaryUnitUpkeepMod() const             { return m_iMilitaryUnitUpkeepMod; }
int CvCivicInfo::getWarWearinessModifier() const              { return m_iWarWearinessModifier; }
int CvCivicInfo::getFreeSpecialist() const                    { return m_iFreeSpecialist; }
int CvCivicInfo::getTradeRoutes() const                       { return m_iTradeRoutes; }
int CvCivicInfo::getCivicPercentAnger() const                 { return m_iCivicPercentAnger; }
int CvCivicInfo::getStateReligionHappiness() const            { return m_iStateReligionHappiness; }
int CvCivicInfo::getNonStateReligionHappiness() const         { return m_iNonStateReligionHappiness; }
int CvCivicInfo::getStateReligionUnitProductionModifier() const     { return m_iStateReligionUnitProductionModifier; }
int CvCivicInfo::getStateReligionBuildingProductionModifier() const { return m_iStateReligionBuildingProductionModifier; }
int CvCivicInfo::getStateReligionFreeExperience() const       { return m_iStateReligionFreeExperience; }
int CvCivicInfo::getExpInBorderModifier() const               { return m_iExpInBorderModifier; }
int CvCivicInfo::getRevIdxLocal() const                       { return m_iRevIdxLocal; }
int CvCivicInfo::getRevIdxNational() const                    { return m_iRevIdxNational; }
int CvCivicInfo::getRevIdxDistanceModifier() const            { return m_iRevIdxDistanceModifier; }
int CvCivicInfo::getRevIdxHolyCityGood() const                { return m_iRevIdxHolyCityGood; }
int CvCivicInfo::getRevIdxHolyCityBad() const                 { return m_iRevIdxHolyCityBad; }
int CvCivicInfo::getRevReligiousFreedom() const               { return m_iRevReligiousFreedom; }
int CvCivicInfo::getRevLaborFreedom() const                   { return m_iRevLaborFreedom; }
int CvCivicInfo::getRevEnvironmentalProtection() const        { return m_iRevEnvironmentalProtection; }
int CvCivicInfo::getRevDemocracyLevel() const                 { return m_iRevDemocracyLevel; }
int CvCivicInfo::getAttitudeShareMod() const                  { return m_iAttitudeShareMod; }
int CvCivicInfo::getForeignerUnhappyPercent() const           { return m_iForeignerUnhappyPercent; }
int CvCivicInfo::getCityOverLimitUnhappy() const              { return m_iCityOverLimitUnhappy; }
int CvCivicInfo::getForeignTradeRouteModifier() const         { return m_iForeignTradeRouteModifier; }
int CvCivicInfo::getTaxRateUnhappiness() const                { return m_iTaxRateUnhappiness; }
int CvCivicInfo::getPopulationgrowthratepercentage() const    { return m_iPopulationgrowthratepercentage; }
int CvCivicInfo::getCivicHappiness() const                    { return m_iCivicHappiness; }
int CvCivicInfo::getDistantUnitSupportCostModifier() const    { return m_iDistantUnitSupportCostModifier; }
int CvCivicInfo::getExtraCityDefense() const                  { return m_iExtraCityDefense; }
int CvCivicInfo::getNationalCaptureProbabilityModifier() const{ return m_iNationalCaptureProbabilityModifier; }
int CvCivicInfo::getNationalCaptureResistanceModifier() const { return m_iNationalCaptureResistanceModifier; }
int CvCivicInfo::getInflationModifier() const                 { return m_iInflationModifier; }
int CvCivicInfo::getHurryInflationModifier() const            { return m_iHurryInflationModifier; }
int CvCivicInfo::getHurryCostModifier() const                 { return m_iHurryCostModifier; }
int CvCivicInfo::getSharedCivicTradeRouteModifier() const     { return m_iSharedCivicTradeRouteModifier; }
int CvCivicInfo::getFreedomFighterChange() const              { return m_iFreedomFighterChange; }

float CvCivicInfo::getRevIdxNationalityMod() const { return m_fRevIdxNationalityMod; }
float CvCivicInfo::getRevIdxBadReligionMod() const { return m_fRevIdxBadReligionMod; }
float CvCivicInfo::getRevIdxGoodReligionMod() const{ return m_fRevIdxGoodReligionMod; }
float CvCivicInfo::getRevViolentMod() const        { return m_fRevViolentMod; }

// one-time revolution-index BURST on switching TO this civic (curate_civic.py: iRevIdxSwitchTo -> grants["revolution"],
// NOT a continuous modifier) -- materialized at mapFrom.
int CvCivicInfo::getRevIdxSwitchTo() const { return m_iRevIdxSwitchTo; }

// --- split yield/commerce arrays -- bare reads of the mapFrom-materialized dense arrays ---
int CvCivicInfo::getYieldModifier(int i) const           { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldModifier[i] : 0; }
int CvCivicInfo::getCommerceModifier(int i) const        { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceModifier[i] : 0; }
int CvCivicInfo::getTradeYieldModifier(int i) const      { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiTradeYieldModifier[i] : 0; }
int CvCivicInfo::getSpecialistExtraCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiSpecialistExtraCommerce[i] : 0; }

// --- conditioned / ranked / qualified leaves -- materialized by condition SHAPE at mapFrom ---
int CvCivicInfo::getCapitalYieldModifier(int i) const    { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiCapitalYieldModifier[i] : 0; }
int CvCivicInfo::getCapitalCommerceModifier(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCapitalCommerceModifier[i] : 0; }
int CvCivicInfo::getLandmarkHappiness() const            { return m_iLandmarkHappiness; }
int CvCivicInfo::getLargestCityHappiness() const         { return m_iLargestCityHappiness; }
int CvCivicInfo::getHappyPerMilitaryUnit() const         { return m_iHappyPerMilitaryUnit; }
int CvCivicInfo::getLandmarkYieldChanges(int i) const    { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiLandmarkYieldChanges[i] : 0; }

// --- target-KEYED section-6 families -- bare reads of the mapFrom-materialized structures ---
int CvCivicInfo::getBuildingHappinessChanges(int i) const
{
	for (size_t k = 0; k < m_vBuildingHappinessChangesSparse.size(); ++k)
		if ((int)m_vBuildingHappinessChangesSparse[k].first == i) return m_vBuildingHappinessChangesSparse[k].second;
	return 0;
}
int CvCivicInfo::getBuildingHealthChanges(int i) const
{
	for (size_t k = 0; k < m_vBuildingHealthChangesSparse.size(); ++k)
		if ((int)m_vBuildingHealthChangesSparse[k].first == i) return m_vBuildingHealthChangesSparse[k].second;
	return 0;
}
int CvCivicInfo::getBuildingProductionModifier(BuildingTypes e) const { return m_aBuildingProductionModifier.getValue(e); }
int CvCivicInfo::getBuildingCommerceModifier(int i, int j) const
{ return (j >= 0 && j < NUM_COMMERCE_TYPES) ? mapGet(m_buildingCommerceModifier[j], i) : 0; }
int CvCivicInfo::getFeatureHappinessChanges(int i) const
{
	for (size_t k = 0; k < m_vFeatureHappinessChangesSparse.size(); ++k)
		if ((int)m_vFeatureHappinessChangesSparse[k].first == i) return m_vFeatureHappinessChangesSparse[k].second;
	return 0;
}
int CvCivicInfo::getImprovementYieldChanges(int i, int j) const
{ return (j >= 0 && j < NUM_YIELD_TYPES) ? mapGet(m_improvementYieldChanges[j], i) : 0; }
int CvCivicInfo::getTerrainYieldChanges(int i, int j) const
{ return (j >= 0 && j < NUM_YIELD_TYPES) ? mapGet(m_terrainYieldChanges[j], i) : 0; }
int CvCivicInfo::getUnitProductionModifier(int i) const       { return mapGet(m_unitProductionModifier, i); }
int CvCivicInfo::getUnitCombatProductionModifier(int i) const { return mapGet(m_unitCombatProductionModifier, i); }
int CvCivicInfo::getCivicAttitudeChange(int i) const          { return mapGet(m_civicAttitudeChanges, i); }
int CvCivicInfo::getFreeSpecialistCount(int i) const          { return mapGet(m_freeSpecialistCount, i); }

bool CvCivicInfo::isAnyBuildingHappinessChange() const { return !m_vBuildingHappinessChangesSparse.empty(); }
bool CvCivicInfo::isAnyBuildingHealthChange() const    { return !m_vBuildingHealthChangesSparse.empty(); }
bool CvCivicInfo::isAnyFeatureHappinessChange() const  { return !m_vFeatureHappinessChangesSparse.empty(); }
bool CvCivicInfo::isAnyImprovementYieldChange() const  { return m_bAnyImprovementYieldChange; }

// --- dense positional bulk-array views (materialized in mapFrom; NULL-when-all-zero, archived dense-storage semantics) ---
int* CvCivicInfo::getYieldModifierArray() const           { return civArrOrNull(m_aiYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getCapitalYieldModifierArray() const    { return civArrOrNull(m_aiCapitalYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getTradeYieldModifierArray() const      { return civArrOrNull(m_aiTradeYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getLandmarkYieldChangesArray() const    { return civArrOrNull(m_aiLandmarkYieldChanges, NUM_YIELD_TYPES); }
int* CvCivicInfo::getCommerceModifierArray() const        { return civArrOrNull(m_aiCommerceModifier, NUM_COMMERCE_TYPES); }
int* CvCivicInfo::getCapitalCommerceModifierArray() const { return civArrOrNull(m_aiCapitalCommerceModifier, NUM_COMMERCE_TYPES); }
int* CvCivicInfo::getSpecialistExtraCommerceArray() const { return civArrOrNull(m_aiSpecialistExtraCommerce, NUM_COMMERCE_TYPES); }
