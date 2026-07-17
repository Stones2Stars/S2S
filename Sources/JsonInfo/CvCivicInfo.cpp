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
}

// ===================== section-6 read helpers (family sums + the target-keyed sparse collector) =====================
//
// NB naming: every file-local helper is prefixed `civ*`/`CIV_` -- this project's unity build textually concatenates
// multiple .cpp files into one translation unit, so a `static` helper name is NOT safely reusable across JsonInfo
// .cpp files even though each is internal-linkage (CvBuildingInfo.cpp's own sumUnconditioned / BLD_YIELD_NAME
// follow the exact same per-file-prefix convention for the same reason).
//
// The curator (curate_civic.py) collapses a civic's own unconditioned SCALAR value and any conditioned / qualified
// addend onto the SAME address -- the conditioned entries carry an `enabled` atom, the qualified ones a `unit:`
// predicate, per-scaled ones a `per`; the plain scalar carries NONE of these. So each getter recovers EXACTLY its
// legacy field by filtering the composed m_modifiers entries at the address on that shape (pure DATA inspection --
// never a runtime GC.getGame() read; [DEC-json-not-cascade] holds).

static const char* CIV_YIELD_NAME[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
static const char* CIV_COMM_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

// The plain legacy scalar: unconditioned, non-`per`, un-qualified entries of `unit` only.
static int civFamilyUnconditioned100(const CvJsonModFamily* f, CvCascUnit unit)
{
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		if (e->unit == unit && e->enabled == NULL && e->disabled == NULL && !e->hasPer && e->unitQual == NULL)
			total100 += e->value100;
	}
	return total100;
}
static int civSumUnconditioned100(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{ return civFamilyUnconditioned100(mods ? mods->find(address) : NULL, unit); }
static int civSumUnconditioned(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{ return civSumUnconditioned100(mods, address, unit) / 100; }
static float civSumUnconditionedF(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{ return civSumUnconditioned100(mods, address, unit) / 100.0f; }

// A target-KEYED family read: the target's own GC type string (e.g. "BUILDING_FORGE") is the address's trailing
// segment (curate_civic.py KEYED). Real data; a per-index scalar getter builds the address live off the index's type.
static int civSumKeyed(const CvJsonModifiers* mods, const std::string& baseAddr, const char* typeStr, CvCascUnit unit)
{
	if (!mods || typeStr == NULL || *typeStr == '\0') return 0;
	return civSumUnconditioned(mods, baseAddr + "." + typeStr, unit);
}

// Does ANY family address under `prefix` exist? A plain map-key prefix scan over the (small) parsed family set.
static bool civHasPrefixedFamily(const CvJsonModifiers* mods, const std::string& prefix)
{
	if (!mods) return false;
	const std::map<std::string, CvJsonModFamily*>& all = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
		if (it->first.compare(0, prefix.size(), prefix) == 0) return true;
	return false;
}

// Sum EVERY entry of `unit` regardless of condition -- for a sub-scope authored ONLY as conditioned entries (the
// landmark `plots` deposit: value gated on GAMEOPTION_MAP_PERSONALIZED + HAS_LANDMARK), where "unconditioned only"
// would wrongly read 0.
static int civSumAll(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
		if (f->entries[i]->unit == unit) total100 += f->entries[i]->value100;
	return total100 / 100;
}

// Sum entries carrying a `unit:` predicate qualifier (the IS_MILITARY-tagged happyPerMilitaryUnit entry on the
// shared happiness.empire.cities.flat leaf). civFamilyUnconditioned100 excludes these (unitQual == NULL), so the two
// `cities` getters stay disjoint.
static int civSumUnitQualified(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		if (e->unit == unit && e->unitQual != NULL && e->enabled == NULL && e->disabled == NULL && !e->hasPer)
			total100 += e->value100;
	}
	return total100 / 100;
}

// Sum entries whose `enabled` clause is exactly a bare predicate of kind `k` (IS_CAPITAL) -- recovers the legacy
// capital-only modifier the curator merged into the same leaf as the empire-wide one (SPLIT_ARRAY_COND).
static int civSumEnabledPred(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, CvCascPredKind k)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		const CvJsonCondition* c = e->enabled;
		if (e->unit == unit && e->disabled == NULL && !e->hasPer
		 && c != NULL && c->kind == CASC_COND_PREDICATE && c->predKind == k)
			total100 += e->value100;
	}
	return total100 / 100;
}

// Sum entries whose `enabled` clause is a bare presence atom of the given type string (GAMEOPTION_MAP_PERSONALIZED) --
// recovers the legacy landmark-happiness the curator merged into happiness.empire.flat as a conditioned entry
// (SCALAR_COND). A bare "GAMEOPTION_*" string parses to a PRESENCE atom (cp_isTypeRef), so match on kind + type.
static int civSumEnabledPresence(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, const char* typeStr)
{
	const CvJsonModFamily* f = mods ? mods->find(address) : NULL;
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		const CvJsonCondition* c = e->enabled;
		if (e->unit == unit && e->disabled == NULL && !e->hasPer
		 && c != NULL && c->kind == CASC_COND_PRESENCE && c->type == typeStr)
			total100 += e->value100;
	}
	return total100 / 100;
}

// Collect a target-keyed family (happiness.empire.buildings.<B>, health.empire.buildings.<B>, happiness.empire.
// features.<F>) into a sparse (id, value) vector -- the hot-path processCivics iteration form. The family key is the
// full address MINUS the unit, so the segment after `prefix` IS the target's type string (targets carry no dots).
template <class IdT>
static void civCollectKeyedSparse(const CvJsonModifiers* mods, const std::string& prefix, CvCascUnit unit,
                                  std::vector<std::pair<IdT, int> >& out)
{
	if (!mods) return;
	const std::map<std::string, CvJsonModFamily*>& all = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
	{
		const std::string& key = it->first;
		if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
		const std::string typeStr = key.substr(prefix.size());
		if (typeStr.find('.') != std::string::npos) continue;   // only a direct target child, not a deeper member
		const int id = jsonResolveId(typeStr);
		if (id < 0) continue;
		const int v = civFamilyUnconditioned100(it->second, unit) / 100;
		if (v != 0) out.push_back(std::make_pair((IdT)id, v));
	}
}

// Same collection into an IDValueMap (the foreach_-iterated form) via the unblocked IDValueMap::setValue mutator.
static void civCollectKeyedMap(const CvJsonModifiers* mods, const std::string& prefix, CvCascUnit unit,
                               IDValueMap<BuildingTypes, int>& out)
{
	if (!mods) return;
	const std::map<std::string, CvJsonModFamily*>& all = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
	{
		const std::string& key = it->first;
		if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
		const std::string typeStr = key.substr(prefix.size());
		if (typeStr.find('.') != std::string::npos) continue;   // only a direct target child, not a deeper member
		const int id = jsonResolveId(typeStr);
		if (id < 0) continue;
		const int v = civFamilyUnconditioned100(it->second, unit) / 100;
		if (v != 0) out.setValue((BuildingTypes)id, v);
	}
}

// The archived dense int* accessors returned NULL when nothing was authored (the array was never allocated); the
// UI-help consumers branch on `if (aList)`. Mirror that: NULL when every slot is 0, else the (const-cast) array.
static int* civArrOrNull(const int* arr, int n)
{
	for (int i = 0; i < n; ++i) if (arr[i] != 0) return const_cast<int*>(arr);
	return NULL;
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
	// read the same addresses live, so the two forms agree).
	civCollectKeyedSparse<BuildingTypes>(getModifiers(), "happiness.empire.buildings.", CASC_UNIT_FLAT, m_vBuildingHappinessChangesSparse);
	civCollectKeyedSparse<BuildingTypes>(getModifiers(), "health.empire.buildings.",    CASC_UNIT_FLAT, m_vBuildingHealthChangesSparse);
	civCollectKeyedSparse<FeatureTypes>(getModifiers(),  "happiness.empire.features.",  CASC_UNIT_FLAT, m_vFeatureHappinessChangesSparse);
	civCollectKeyedMap(getModifiers(), "buildRate.empire.buildings.", CASC_UNIT_PERCENT, m_aBuildingProductionModifier);

	// Materialize the dense positional arrays the UI-help bulk accessors return (from the per-index getters, which
	// read the same composed m_modifiers). getYieldModifierArray etc. return these NULL-when-all-zero.
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiYieldModifier[y]        = getYieldModifier(y);
		m_aiCapitalYieldModifier[y] = getCapitalYieldModifier(y);
		m_aiTradeYieldModifier[y]   = getTradeYieldModifier(y);
		m_aiLandmarkYieldChanges[y] = getLandmarkYieldChanges(y);
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		m_aiCommerceModifier[c]        = getCommerceModifier(c);
		m_aiCapitalCommerceModifier[c] = getCapitalCommerceModifier(c);
		m_aiSpecialistExtraCommerce[c] = getSpecialistExtraCommerce(c);
	}
}

// ===================== identity / edge getters =====================

int CvCivicInfo::getMaxConscript() const { return civSumUnconditioned(getModifiers(), "conscript.empire", CASC_UNIT_FLAT); }
int CvCivicInfo::getCityLimitBase() const { return civSumUnconditioned(getModifiers(), "happiness.empire.cityLimit", CASC_UNIT_FLAT); }

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

// ===================== section-6 scalar modifier-family reads (curate_civic.py SCALAR / STATE_RELIGION) =====================
int CvCivicInfo::getGreatPeopleRateModifier() const           { return civSumUnconditioned(getModifiers(), "greatPeopleRate.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getGreatGeneralRateModifier() const          { return civSumUnconditioned(getModifiers(), "greatGeneralRate.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getDomesticGreatGeneralRateModifier() const  { return civSumUnconditioned(getModifiers(), "greatGeneralRate.empire.domestic", CASC_UNIT_PERCENT); }
int CvCivicInfo::getStateReligionGreatPeopleRateModifier() const { return civSumUnconditioned(getModifiers(), "stateReligion.empire.greatPeopleRate", CASC_UNIT_PERCENT); }
int CvCivicInfo::getDistanceMaintenanceModifier() const       { return civSumUnconditioned(getModifiers(), "maintenance.empire.distance", CASC_UNIT_PERCENT); }
int CvCivicInfo::getNumCitiesMaintenanceModifier() const      { return civSumUnconditioned(getModifiers(), "maintenance.empire.numCities", CASC_UNIT_PERCENT); }
int CvCivicInfo::getHomeAreaMaintenanceModifier() const       { return civSumUnconditioned(getModifiers(), "maintenance.empire.homeArea", CASC_UNIT_PERCENT); }
int CvCivicInfo::getOtherAreaMaintenanceModifier() const      { return civSumUnconditioned(getModifiers(), "maintenance.empire.otherArea", CASC_UNIT_PERCENT); }
int CvCivicInfo::getCorporationMaintenanceModifier() const    { return civSumUnconditioned(getModifiers(), "maintenance.empire.corporation", CASC_UNIT_PERCENT); }
int CvCivicInfo::getExtraHealth() const                       { return civSumUnconditioned(getModifiers(), "health.empire", CASC_UNIT_FLAT); }
int CvCivicInfo::getFreeExperience() const                    { return civSumUnconditioned(getModifiers(), "experience.empire", CASC_UNIT_FLAT); }
int CvCivicInfo::getWorkerSpeedModifier() const               { return civSumUnconditioned(getModifiers(), "workRate.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getImprovementUpgradeRateModifier() const    { return civSumUnconditioned(getModifiers(), "improvementUpgradeRate.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getMilitaryProductionModifier() const        { return civSumUnconditioned(getModifiers(), "buildRate.empire.military", CASC_UNIT_PERCENT); }
int CvCivicInfo::getFreeUnitUpkeepCivilian() const            { return civSumUnconditioned(getModifiers(), "upkeep.empire.freeCivilian", CASC_UNIT_FLAT); }
int CvCivicInfo::getFreeUnitUpkeepMilitary() const            { return civSumUnconditioned(getModifiers(), "upkeep.empire.freeMilitary", CASC_UNIT_FLAT); }
int CvCivicInfo::getFreeUnitUpkeepCivilianPopPercent() const  { return civSumUnconditioned(getModifiers(), "upkeep.empire.freeCivilian", CASC_UNIT_PER_POPULATION); }
int CvCivicInfo::getFreeUnitUpkeepMilitaryPopPercent() const  { return civSumUnconditioned(getModifiers(), "upkeep.empire.freeMilitary", CASC_UNIT_PER_POPULATION); }
int CvCivicInfo::getCivilianUnitUpkeepMod() const             { return civSumUnconditioned(getModifiers(), "upkeep.empire.unitCivilian", CASC_UNIT_PERCENT); }
int CvCivicInfo::getMilitaryUnitUpkeepMod() const             { return civSumUnconditioned(getModifiers(), "upkeep.empire.unitMilitary", CASC_UNIT_PERCENT); }
int CvCivicInfo::getWarWearinessModifier() const              { return civSumUnconditioned(getModifiers(), "diplomacy.empire.warWeariness", CASC_UNIT_PERCENT); }
int CvCivicInfo::getFreeSpecialist() const                    { return civSumUnconditioned(getModifiers(), "freeSpecialists.empire.any", CASC_UNIT_COUNT); }
int CvCivicInfo::getTradeRoutes() const                       { return civSumUnconditioned(getModifiers(), "tradeRoutes.empire", CASC_UNIT_FLAT); }
int CvCivicInfo::getCivicPercentAnger() const                 { return civSumUnconditioned(getModifiers(), "happiness.empire.civicAnger", CASC_UNIT_PERCENT); }
int CvCivicInfo::getStateReligionHappiness() const            { return civSumUnconditioned(getModifiers(), "stateReligion.empire.happiness", CASC_UNIT_FLAT); }
int CvCivicInfo::getNonStateReligionHappiness() const         { return civSumUnconditioned(getModifiers(), "happiness.empire.nonStateReligion", CASC_UNIT_FLAT); }
int CvCivicInfo::getStateReligionUnitProductionModifier() const     { return civSumUnconditioned(getModifiers(), "stateReligion.empire.unitProduction", CASC_UNIT_PERCENT); }
int CvCivicInfo::getStateReligionBuildingProductionModifier() const { return civSumUnconditioned(getModifiers(), "stateReligion.empire.buildingProduction", CASC_UNIT_PERCENT); }
int CvCivicInfo::getStateReligionFreeExperience() const       { return civSumUnconditioned(getModifiers(), "stateReligion.empire.freeExperience", CASC_UNIT_FLAT); }
int CvCivicInfo::getExpInBorderModifier() const               { return civSumUnconditioned(getModifiers(), "experience.empire.inBorder", CASC_UNIT_PERCENT); }
int CvCivicInfo::getRevIdxLocal() const                       { return civSumUnconditioned(getModifiers(), "revolution.empire.local", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevIdxNational() const                    { return civSumUnconditioned(getModifiers(), "revolution.empire.national", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevIdxDistanceModifier() const            { return civSumUnconditioned(getModifiers(), "revolution.empire.distanceModifier", CASC_UNIT_PERCENT); }
int CvCivicInfo::getRevIdxHolyCityGood() const                { return civSumUnconditioned(getModifiers(), "revolution.empire.holyCityGood", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevIdxHolyCityBad() const                 { return civSumUnconditioned(getModifiers(), "revolution.empire.holyCityBad", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevReligiousFreedom() const               { return civSumUnconditioned(getModifiers(), "revolution.empire.religiousFreedom", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevLaborFreedom() const                   { return civSumUnconditioned(getModifiers(), "revolution.empire.laborFreedom", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevEnvironmentalProtection() const        { return civSumUnconditioned(getModifiers(), "revolution.empire.environmentalProtection", CASC_UNIT_FLAT); }
int CvCivicInfo::getRevDemocracyLevel() const                 { return civSumUnconditioned(getModifiers(), "revolution.empire.democracyLevel", CASC_UNIT_FLAT); }
int CvCivicInfo::getAttitudeShareMod() const                  { return civSumUnconditioned(getModifiers(), "diplomacy.empire.attitudeShare", CASC_UNIT_FLAT); }
int CvCivicInfo::getForeignerUnhappyPercent() const           { return civSumUnconditioned(getModifiers(), "happiness.empire.foreignerUnhappy", CASC_UNIT_PERCENT); }
int CvCivicInfo::getCityOverLimitUnhappy() const              { return civSumUnconditioned(getModifiers(), "happiness.empire.cityOverLimit", CASC_UNIT_FLAT); }
int CvCivicInfo::getForeignTradeRouteModifier() const         { return civSumUnconditioned(getModifiers(), "tradeRoutes.empire.foreign", CASC_UNIT_PERCENT); }
int CvCivicInfo::getTaxRateUnhappiness() const                { return civSumUnconditioned(getModifiers(), "happiness.empire.taxRate", CASC_UNIT_PERCENT); }
int CvCivicInfo::getPopulationgrowthratepercentage() const    { return civSumUnconditioned(getModifiers(), "growth.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getCivicHappiness() const                    { return civSumUnconditioned(getModifiers(), "happiness.empire", CASC_UNIT_FLAT); }
int CvCivicInfo::getDistantUnitSupportCostModifier() const    { return civSumUnconditioned(getModifiers(), "upkeep.empire.supply", CASC_UNIT_PERCENT); }
int CvCivicInfo::getExtraCityDefense() const                  { return civSumUnconditioned(getModifiers(), "defense.empire.amount", CASC_UNIT_PERCENT); }
int CvCivicInfo::getNationalCaptureProbabilityModifier() const{ return civSumUnconditioned(getModifiers(), "combat.empire.captureProbability", CASC_UNIT_PERCENT); }
int CvCivicInfo::getNationalCaptureResistanceModifier() const { return civSumUnconditioned(getModifiers(), "combat.empire.captureResistance", CASC_UNIT_PERCENT); }
int CvCivicInfo::getInflationModifier() const                 { return civSumUnconditioned(getModifiers(), "inflation.empire", CASC_UNIT_PERCENT); }
int CvCivicInfo::getHurryInflationModifier() const            { return civSumUnconditioned(getModifiers(), "hurry.empire.inflation", CASC_UNIT_PERCENT); }
int CvCivicInfo::getHurryCostModifier() const                 { return civSumUnconditioned(getModifiers(), "hurry.empire.cost", CASC_UNIT_PERCENT); }
int CvCivicInfo::getSharedCivicTradeRouteModifier() const     { return civSumUnconditioned(getModifiers(), "tradeRoutes.empire.sharedCivic", CASC_UNIT_PERCENT); }
int CvCivicInfo::getFreedomFighterChange() const              { return civSumUnconditioned(getModifiers(), "revolution.empire.freedomFighter", CASC_UNIT_FLAT); }

float CvCivicInfo::getRevIdxNationalityMod() const { return civSumUnconditionedF(getModifiers(), "revolution.empire.nationalityMod", CASC_UNIT_PERCENT); }
float CvCivicInfo::getRevIdxBadReligionMod() const { return civSumUnconditionedF(getModifiers(), "revolution.empire.badReligionMod", CASC_UNIT_PERCENT); }
float CvCivicInfo::getRevIdxGoodReligionMod() const{ return civSumUnconditionedF(getModifiers(), "revolution.empire.goodReligionMod", CASC_UNIT_PERCENT); }
float CvCivicInfo::getRevViolentMod() const        { return civSumUnconditionedF(getModifiers(), "revolution.empire.violentMod", CASC_UNIT_PERCENT); }

// one-time revolution-index BURST on switching TO this civic (curate_civic.py: iRevIdxSwitchTo -> grants["revolution"],
// NOT a continuous modifier) -- REAL via the base's grantPulse100 read-through.
int CvCivicInfo::getRevIdxSwitchTo() const { return grantPulse100("revolution") / 100; }

// --- split yield/commerce arrays (curate_civic.py SPLIT_ARRAY) ---
int CvCivicInfo::getYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? civSumUnconditioned(getModifiers(), std::string(CIV_YIELD_NAME[i]) + ".empire", CASC_UNIT_PERCENT) : 0; }
int CvCivicInfo::getCommerceModifier(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? civSumUnconditioned(getModifiers(), std::string(CIV_COMM_NAME[i]) + ".empire", CASC_UNIT_PERCENT) : 0; }
int CvCivicInfo::getTradeYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? civSumUnconditioned(getModifiers(), std::string(CIV_YIELD_NAME[i]) + ".empire.tradeRoute", CASC_UNIT_PERCENT) : 0; }
int CvCivicInfo::getSpecialistExtraCommerce(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? civSumUnconditioned(getModifiers(), std::string(CIV_COMM_NAME[i]) + ".empire.specialist", CASC_UNIT_PER_SPECIALIST) : 0; }

// --- conditioned / ranked / qualified leaves (recovered by condition SHAPE; see the helpers) ---
int CvCivicInfo::getCapitalYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? civSumEnabledPred(getModifiers(), std::string(CIV_YIELD_NAME[i]) + ".empire", CASC_UNIT_PERCENT, CASC_PRED_IS_CAPITAL) : 0; }
int CvCivicInfo::getCapitalCommerceModifier(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? civSumEnabledPred(getModifiers(), std::string(CIV_COMM_NAME[i]) + ".empire", CASC_UNIT_PERCENT, CASC_PRED_IS_CAPITAL) : 0; }
int CvCivicInfo::getLandmarkHappiness() const
{ return civSumEnabledPresence(getModifiers(), "happiness.empire", CASC_UNIT_FLAT, "GAMEOPTION_MAP_PERSONALIZED"); }
int CvCivicInfo::getLargestCityHappiness() const
{ return civSumUnconditioned(getModifiers(), "happiness.empire.cities", CASC_UNIT_FLAT); }
int CvCivicInfo::getHappyPerMilitaryUnit() const
{ return civSumUnitQualified(getModifiers(), "happiness.empire.cities", CASC_UNIT_FLAT); }
int CvCivicInfo::getLandmarkYieldChanges(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? civSumAll(getModifiers(), std::string(CIV_YIELD_NAME[i]) + ".empire.plots", CASC_UNIT_FLAT) : 0; }

// --- target-KEYED section-6 families -- REAL (live address built off the target's own GC type string) ---
int CvCivicInfo::getBuildingHappinessChanges(int i) const
{
	if (i < 0 || i >= GC.getNumBuildingInfos()) return 0;
	return civSumKeyed(getModifiers(), "happiness.empire.buildings", GC.getBuildingInfo((BuildingTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getBuildingHealthChanges(int i) const
{
	if (i < 0 || i >= GC.getNumBuildingInfos()) return 0;
	return civSumKeyed(getModifiers(), "health.empire.buildings", GC.getBuildingInfo((BuildingTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getBuildingProductionModifier(BuildingTypes e) const
{
	if (e < 0 || e >= GC.getNumBuildingInfos()) return 0;
	return civSumKeyed(getModifiers(), "buildRate.empire.buildings", GC.getBuildingInfo(e).getType(), CASC_UNIT_PERCENT);
}
int CvCivicInfo::getBuildingCommerceModifier(int i, int j) const
{
	if (i < 0 || i >= GC.getNumBuildingInfos() || j < 0 || j >= NUM_COMMERCE_TYPES) return 0;
	return civSumKeyed(getModifiers(), std::string(CIV_COMM_NAME[j]) + ".empire.buildings", GC.getBuildingInfo((BuildingTypes)i).getType(), CASC_UNIT_PERCENT);
}
int CvCivicInfo::getFeatureHappinessChanges(int i) const
{
	if (i < 0 || i >= GC.getNumFeatureInfos()) return 0;
	return civSumKeyed(getModifiers(), "happiness.empire.features", GC.getFeatureInfo((FeatureTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getImprovementYieldChanges(int i, int j) const
{
	if (i < 0 || i >= GC.getNumImprovementInfos() || j < 0 || j >= NUM_YIELD_TYPES) return 0;
	return civSumKeyed(getModifiers(), std::string(CIV_YIELD_NAME[j]) + ".empire.improvements", GC.getImprovementInfo((ImprovementTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getTerrainYieldChanges(int i, int j) const
{
	if (i < 0 || i >= GC.getNumTerrainInfos() || j < 0 || j >= NUM_YIELD_TYPES) return 0;
	return civSumKeyed(getModifiers(), std::string(CIV_YIELD_NAME[j]) + ".empire.terrains", GC.getTerrainInfo((TerrainTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getUnitProductionModifier(int i) const
{
	if (i < 0 || i >= GC.getNumUnitInfos()) return 0;
	return civSumKeyed(getModifiers(), "buildRate.empire.units", GC.getUnitInfo((UnitTypes)i).getType(), CASC_UNIT_PERCENT);
}
int CvCivicInfo::getUnitCombatProductionModifier(int i) const
{
	if (i < 0 || i >= GC.getNumUnitCombatInfos()) return 0;
	return civSumKeyed(getModifiers(), "buildRate.empire.unitCombats", GC.getUnitCombatInfo((UnitCombatTypes)i).getType(), CASC_UNIT_PERCENT);
}
int CvCivicInfo::getCivicAttitudeChange(int i) const
{
	if (i < 0 || i >= GC.getNumCivicInfos()) return 0;
	return civSumKeyed(getModifiers(), "diplomacy.empire.civics", GC.getCivicInfo((CivicTypes)i).getType(), CASC_UNIT_FLAT);
}
int CvCivicInfo::getFreeSpecialistCount(int i) const
{
	if (i < 0 || i >= GC.getNumSpecialistInfos()) return 0;
	return civSumKeyed(getModifiers(), "freeSpecialists.empire", GC.getSpecialistInfo((SpecialistTypes)i).getType(), CASC_UNIT_COUNT);
}

bool CvCivicInfo::isAnyBuildingHappinessChange() const { return civHasPrefixedFamily(getModifiers(), "happiness.empire.buildings."); }
bool CvCivicInfo::isAnyBuildingHealthChange() const    { return civHasPrefixedFamily(getModifiers(), "health.empire.buildings."); }
bool CvCivicInfo::isAnyFeatureHappinessChange() const  { return civHasPrefixedFamily(getModifiers(), "happiness.empire.features."); }
bool CvCivicInfo::isAnyImprovementYieldChange() const
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		if (civHasPrefixedFamily(getModifiers(), std::string(CIV_YIELD_NAME[i]) + ".empire.improvements.")) return true;
	return false;
}

// --- dense positional bulk-array views (materialized in mapFrom; NULL-when-all-zero, archived dense-storage semantics) ---
int* CvCivicInfo::getYieldModifierArray() const           { return civArrOrNull(m_aiYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getCapitalYieldModifierArray() const    { return civArrOrNull(m_aiCapitalYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getTradeYieldModifierArray() const      { return civArrOrNull(m_aiTradeYieldModifier, NUM_YIELD_TYPES); }
int* CvCivicInfo::getLandmarkYieldChangesArray() const    { return civArrOrNull(m_aiLandmarkYieldChanges, NUM_YIELD_TYPES); }
int* CvCivicInfo::getCommerceModifierArray() const        { return civArrOrNull(m_aiCommerceModifier, NUM_COMMERCE_TYPES); }
int* CvCivicInfo::getCapitalCommerceModifierArray() const { return civArrOrNull(m_aiCapitalCommerceModifier, NUM_COMMERCE_TYPES); }
int* CvCivicInfo::getSpecialistExtraCommerceArray() const { return civArrOrNull(m_aiSpecialistExtraCommerce, NUM_COMMERCE_TYPES); }
