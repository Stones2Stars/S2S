//
//	CvJsonBuildingInfo::mapFrom -- common sections (base dispatch fills the composed units) + the building `identity`
//	block: the StoneBase BuildingInfo flags (notConstructible / governmentCenter / forceNoPrereqScaling /
//	specialBuilding) + the shrine/corpHQ/religion FKs + the stateReligionCommerce / commerceDoubleTime maps.
//	SELF-CONTAINED (the engine getReligionType / getGlobal*Commerce reads are RETIRED). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonBuildingInfo.h"
#include "CvJsonParse.h"            // jsonResolveId / jsonCommerceMap / jsonIdFk / jsonIdInt / jsonIdBool / jsonWorldArt
#include "UI/CvArtFileMgr.h"        // ARTFILEMGR.getBuildingArtInfo / getMovieArtInfo -- the art shims (mirrors CvBonusInfo)
#include "Infos/CvArtInfoMovie.h"   // CvArtInfoMovie complete type -- getMovie() calls getPath() (via CvAssetInfoBase)
#include "Infos/CvArtInfoBuilding.h" // CvArtInfoBuilding complete type -- getButton() call needs the full definition

void CvJsonBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	// TOP-LEVEL bespoke FK sections (migrated OUT of identity, owner 2026-07-01): `shrine` -> religion FK,
	// `headquarters` -> corporation FK (was identity.shrine / identity.corporationHQ). Feed the shrine/corp-HQ commerce calc.
	if ((it = o.find("shrine")) != o.end() && it->second.is<std::string>())       shrineReligion = jsonResolveId(it->second.get<std::string>());
	if ((it = o.find("headquarters")) != o.end() && it->second.is<std::string>()) corpHQ = jsonResolveId(it->second.get<std::string>());
	// world.art.icon -- the ART_DEF_* tag the ARTFILEMGR lookup keys on (getArtInfo shim, mirrors CvJsonBonusInfo).
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "define", m_szArtDefineTag);
	// ui.art.movie.defineTag -- the ART_DEF_MOVIE_* tag for the wonder-movie shim (getMovieInfo).
	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* uart = jsonChildObj(*ui, "art"))
			if (const picojson::object* mov = jsonChildObj(*uart, "movie")) jsonIdStr(*mov, "defineTag", m_szMovieDefineTag);
	// cost.* -- the intrinsic production cost + the C2C real-cost scaling members (curate_building.py COST table).
	if (const picojson::object* co = jsonChildObj(o, "cost"))
	{
		m_iProductionCost           = jsonIdInt(*co, "production");
		m_iProductionCostSize       = jsonIdInt(*co, "sizeModifier");
		m_iProductionCostCount      = jsonIdInt(*co, "countModifier");
		m_iProductionCostMaterials  = jsonIdInt(*co, "materialsModifier");
		m_iProductionCostComplexity = jsonIdInt(*co, "complexityModifier");
	}
	// sound.construct -- the on-build sound tag.
	if (const picojson::object* so = jsonChildObj(o, "sound")) jsonIdStr(*so, "construct", m_szConstructSound);
	// ai.flavours -- the FLAVOR:weight array (getFlavorValue); ai.behaviour.weight -- the AI build weight (getAIWeight).
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		jsonReadFlavours(*ai, m_flavours);
		if (const picojson::object* be = jsonChildObj(*ai, "behaviour")) m_iAIWeight = jsonIdInt(*be, "weight");
	}
	// The `attributes` classification block (migrated OUT of identity): governmentCenter feeds the IS_GOVERNMENT_CENTER predicate.
	picojson::object::const_iterator at = o.find("attributes");
	if (at != o.end() && at->second.is<picojson::object>())
	{
		const picojson::object& ao = at->second.get<picojson::object>();
		if ((it = ao.find("governmentCenter")) != ao.end() && it->second.is<bool>()) governmentCenter = it->second.get<bool>();
	}
	// The `identity` block: the fields that STAYED intrinsic -- state-religion FK, the cascade commerce markers, buildability flags.
	picojson::object::const_iterator id = o.find("identity");
	if (id == o.end() || !id->second.is<picojson::object>()) return;
	const picojson::object& io = id->second.get<picojson::object>();
	if ((it = io.find("religion")) != io.end() && it->second.is<std::string>())      religion = jsonResolveId(it->second.get<std::string>());
	if ((it = io.find("stateReligionCommerce")) != io.end()) jsonCommerceMap(it->second, stateReligionCommerce);
	if ((it = io.find("commerceDoubleTime")) != io.end())    jsonCommerceMap(it->second, commerceDoubleTime);
	if ((it = io.find("notConstructible")) != io.end() && it->second.is<bool>())     notConstructible = it->second.get<bool>();
	if ((it = io.find("forceNoPrereqScaling")) != io.end() && it->second.is<bool>()) forceNoPrereqScaling = it->second.get<bool>();
	// specialBuildingType -- the per-player-capped SpecialBuilding GROUP FK (curator ID_SCALAR key `specialBuildingType`).
	if ((it = io.find("specialBuildingType")) != io.end() && it->second.is<std::string>()) specialBuildingType = it->second.get<std::string>();
	if (!specialBuildingType.empty()) m_iSpecialBuilding = jsonResolveId(specialBuildingType);
	// #430 getter-support fields (real data; see the header for the mirrored-getter reasoning).
	freeStartEra        = jsonIdFk(io, "freeStartEra");
	conquestProbability = jsonIdInt(io, "conquestProbability");
	voteSourceType       = jsonIdFk(io, "diploVoteType");
	autoBuild            = jsonIdBool(io, "autoBuild");
	m_iMaxStartEra              = jsonIdFk(io, "maxStartEra");
	m_iAdvisorType              = jsonIdFk(io, "advisor");
	m_iGreatPeopleUnitType      = jsonIdFk(io, "greatPeopleUnitType");
	m_iPromotionLineType        = jsonIdFk(io, "promotionLineType");
	m_iExtendsBuilding          = jsonIdFk(io, "extends");
	m_iProductionContinueBuilding = jsonIdFk(io, "productionContinue");
	m_iAirlift                  = jsonIdInt(io, "airlift");
	m_iAirUnitCapacity          = jsonIdInt(io, "airUnitCapacity");
	m_iDCMAirbombMission        = jsonIdInt(io, "dcmAirbombMission");
	m_iLineOfSight              = jsonIdInt(io, "sightRange");
	m_iWorkableRadius           = jsonIdInt(io, "workableRadius");
	m_iNumPopulationEmployed    = jsonIdInt(io, "populationEmployed");
	m_iLinePriority             = jsonIdInt(io, "linePriority");
	m_iAssetValue               = jsonIdInt(io, "worth");           // archive getAssetValue returns this ×100
	m_iPowerValue               = jsonIdInt(io, "militaryWorth");   // archive getPowerValue returns this ×100
	m_bCenterInCity             = jsonIdBool(io, "centerInCity");
	m_bAllowsNukes              = jsonIdBool(io, "allowsNukes");
	m_bNoLimit                  = jsonIdBool(io, "noInstanceLimit");
	// visibilityPriority is an authored fractional (float) -- read directly, not via the int helper.
	if ((it = io.find("visibilityPriority")) != io.end() && it->second.is<double>()) m_fVisibilityPriority = (float)it->second.get<double>();
	// identity.mapCategories -- the MAPCATEGORY_* FK list (mirrors CvJsonBonusInfo).
	if ((it = io.find("mapCategories")) != io.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategoryTypes.push_back((MapCategoryTypes)id); }
	}
	// identity.victoryThresholds -- a list of single-key {VICTORY_X: threshold} objects -> {victoryId: threshold}.
	if ((it = io.find("victoryThresholds")) != io.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<picojson::object>())
			{
				const picojson::object& vo = a[i].get<picojson::object>();
				for (picojson::object::const_iterator e = vo.begin(); e != vo.end(); ++e)
					if (e->second.is<double>()) { const int id = jsonResolveId(e->first); if (id >= 0) m_victoryThresholds[id] = (int)e->second.get<double>(); }
			}
	}

	// identity.enabledCivilizations -- the NPC-only build whitelist (getEnabledCivilizationType).
	if ((it = io.find("enabledCivilizations")) != io.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) { EnabledCivilizations e = { (CivilizationTypes)id }; m_enabledCivTypes.push_back(e); } }
	}

	// --- REAL vectors backed by the composed grants/edges units (populated by CvJsonInfo::mapFrom, called above) ---
	// grants.freePromotions -> getFreePromoTypes (a plain PROMOTION_* FK list; no per-entry condition curated -> NULL).
	if (const std::vector<int>* fp = getGrants()->list("freePromotions"))
		for (size_t i = 0; i < fp->size(); ++i)
		{
			FreePromoTypes f = { (PromotionTypes)(*fp)[i], NULL };
			m_aFreePromoTypes.push_back(f);
		}
	// enables.traits -> getFreeTraitTypes (TraitTypes FK ids; whole civ-trait conferred while active).
	if (const std::vector<int>* ft = getEdges()->find("enables.traits"))
		for (size_t i = 0; i < ft->size(); ++i) m_aiFreeTraitTypes.push_back((TraitTypes)(*ft)[i]);

	// grants.repeatable[] -> the property-spawn + full-heal + per-unitCombat heal reads.
	{
		const std::vector<CvJsonGrantRepeatable*>& reps = getGrants()->repeatables();
		for (size_t i = 0; i < reps.size(); ++i)
		{
			const CvJsonGrantRepeatable* r = reps[i];
			if (r->healFull) m_iNumUnitFullHeal += (r->count > 0 ? r->count : 1);
			if (r->unitId >= 0) { m_iPropertySpawnUnit = r->unitId; if (r->chancePerId >= 0) m_iPropertySpawnProperty = r->chancePerId; }
			if (r->unitCombatId >= 0) { HealUnitCombat h = { (UnitCombatTypes)r->unitCombatId, r->heal100 / 100, 0 }; m_healUnitCombats.push_back(h); }
		}
	}

	// GROUP 1 (requires condition tree) + GROUP 2 (keyed modifiers) -- the legacy-shaped members.
	reconstructFromComposed();
}

// ===================== #430 mirrored-getter support (family reads + edges + art shim) =====================
//
// The curator (curate_building.py) collapses a building's own unconditioned SCALAR_FAMILIES scalar (iHappiness,
// iHealth, YieldChanges, ...) and any tech/bonus-gated COND_KEYED addend (or per-scaled deposit) onto the SAME
// <family>.<scope> modifier address -- the COND_KEYED entries always carry an `enabled` atom, and per-scaled
// entries (e.g. ImprovementFreeSpecialists) carry a `per`; the plain SCALAR_FAMILIES entry carries NEITHER.
// Summing only the entries with NO condition and NO per therefore recovers EXACTLY the legacy plain-scalar field,
// verified against that split (curate_building.py SCALAR_FAMILIES vs COND_KEYED / _inject_per) -- not guessed.
static int sumUnconditioned(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	if (!mods) return 0;
	const CvJsonModFamily* f = mods->find(address);
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
	{
		const CvJsonModEntry* e = f->entries[i];
		if (e->unit == unit && e->enabled == NULL && e->disabled == NULL && !e->hasPer) total100 += e->value100;
	}
	return total100 / 100;
}

// Sum EVERY entry at `address` regardless of condition -- for families whose entries are ALWAYS plot/target
// predicate-gated by design (GlobalSeaPlotYieldChanges' IS_WATER fold, curate_building.py:_inject_plots), where
// "unconditioned only" would wrongly read 0 (there the `enabled` is a plot filter, not an owner-side gate).
static int sumAll(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
{
	if (!mods) return 0;
	const CvJsonModFamily* f = mods->find(address);
	if (!f) return 0;
	int total100 = 0;
	for (int i = 0; i < f->size(); ++i)
		if (f->entries[i]->unit == unit) total100 += f->entries[i]->value100;
	return total100 / 100;
}

static const char* BLD_YIELD_NAME[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
static const char* BLD_COMM_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

int CvJsonBuildingInfo::getHappiness() const       { return sumUnconditioned(getModifiers(), "happiness.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getAreaHappiness() const   { return sumUnconditioned(getModifiers(), "happiness.area", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getGlobalHappiness() const { return sumUnconditioned(getModifiers(), "happiness.empire", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getHealth() const          { return sumUnconditioned(getModifiers(), "health.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getAreaHealth() const      { return sumUnconditioned(getModifiers(), "health.area", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getGlobalHealth() const    { return sumUnconditioned(getModifiers(), "health.empire", CASC_UNIT_FLAT); }

int CvJsonBuildingInfo::getYieldChange(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_YIELD_NAME[i]) + ".city", CASC_UNIT_FLAT) : 0; }
int CvJsonBuildingInfo::getYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_YIELD_NAME[i]) + ".city", CASC_UNIT_PERCENT) : 0; }
int CvJsonBuildingInfo::getAreaYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_YIELD_NAME[i]) + ".area", CASC_UNIT_PERCENT) : 0; }
int CvJsonBuildingInfo::getGlobalYieldModifier(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_YIELD_NAME[i]) + ".empire", CASC_UNIT_PERCENT) : 0; }
int CvJsonBuildingInfo::getGlobalSeaPlotYieldChange(int i) const
{ return (i >= 0 && i < NUM_YIELD_TYPES) ? sumAll(getModifiers(), std::string(BLD_YIELD_NAME[i]) + ".empire.plots", CASC_UNIT_FLAT) : 0; }

int CvJsonBuildingInfo::getCommerceChange(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_COMM_NAME[i]) + ".city", CASC_UNIT_FLAT) : 0; }
int CvJsonBuildingInfo::getCommerceModifier(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_COMM_NAME[i]) + ".city", CASC_UNIT_PERCENT) : 0; }
int CvJsonBuildingInfo::getGlobalCommerceModifier(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_COMM_NAME[i]) + ".empire", CASC_UNIT_PERCENT) : 0; }
int CvJsonBuildingInfo::getSpecialistExtraCommerce(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? sumUnconditioned(getModifiers(), std::string(BLD_COMM_NAME[i]) + ".empire.specialist", CASC_UNIT_PER_SPECIALIST) : 0; }

int CvJsonBuildingInfo::getEnemyWarWearinessModifier() const { return sumUnconditioned(getModifiers(), "warWeariness.city.enemy", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getOccupationTimeModifier() const    { return sumUnconditioned(getModifiers(), "occupationTime.city", CASC_UNIT_PERCENT); }

// --- the rest of the §6 scalar-family getters (address per curate_building.py SCALAR_FAMILIES; unit per the table) ---
int CvJsonBuildingInfo::getHealRateChange() const               { return sumUnconditioned(getModifiers(), "healing.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getFoodKept() const                     { return sumUnconditioned(getModifiers(), "foodKept.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGreatPeopleRateChange() const        { return sumUnconditioned(getModifiers(), "greatPeopleRate.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getGreatPeopleRateModifier() const      { return sumUnconditioned(getModifiers(), "greatPeopleRate.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalGreatPeopleRateModifier() const{ return sumUnconditioned(getModifiers(), "greatPeopleRate.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGreatGeneralRateModifier() const     { return sumUnconditioned(getModifiers(), "greatGeneralRate.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getDomesticGreatGeneralRateModifier() const { return sumUnconditioned(getModifiers(), "greatGeneralRate.city.domestic", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getMaintenanceModifier() const          { return sumUnconditioned(getModifiers(), "maintenance.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalMaintenanceModifier() const    { return sumUnconditioned(getModifiers(), "maintenance.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getAreaMaintenanceModifier() const      { return sumUnconditioned(getModifiers(), "maintenance.area", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getOtherAreaMaintenanceModifier() const { return sumUnconditioned(getModifiers(), "maintenance.area.otherArea", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getDistanceMaintenanceModifier() const  { return sumUnconditioned(getModifiers(), "maintenance.empire.distance", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getNumCitiesMaintenanceModifier() const { return sumUnconditioned(getModifiers(), "maintenance.empire.numCities", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getCoastalDistanceMaintenanceModifier() const { return sumUnconditioned(getModifiers(), "maintenance.empire.coastalDistance", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getConnectedCityMaintenanceModifier() const { return sumUnconditioned(getModifiers(), "maintenance.empire.connectedCity", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getInflationModifier() const            { return sumUnconditioned(getModifiers(), "inflation.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getWarWearinessModifier() const         { return sumUnconditioned(getModifiers(), "warWeariness.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalWarWearinessModifier() const   { return sumUnconditioned(getModifiers(), "warWeariness.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getHurryCostModifier() const            { return sumUnconditioned(getModifiers(), "hurryCost.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalHurryModifier() const          { return sumUnconditioned(getModifiers(), "hurryCost.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getHurryAngerModifier() const           { return sumUnconditioned(getModifiers(), "hurryAnger.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getMilitaryProductionModifier() const   { return sumUnconditioned(getModifiers(), "buildRate.city.military", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getSpaceProductionModifier() const      { return sumUnconditioned(getModifiers(), "buildRate.city.space", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalSpaceProductionModifier() const{ return sumUnconditioned(getModifiers(), "buildRate.empire.space", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getWorkerSpeedModifier() const          { return sumUnconditioned(getModifiers(), "workRate.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getTradeRoutes() const                  { return sumUnconditioned(getModifiers(), "tradeRoutes.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getCoastalTradeRoutes() const           { return sumUnconditioned(getModifiers(), "tradeRoutes.empire.coastal", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getGlobalTradeRoutes() const            { return sumUnconditioned(getModifiers(), "tradeRoutes.empire", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getWorldTradeRoutes() const             { return sumUnconditioned(getModifiers(), "tradeRoutes.world", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getTradeRouteModifier() const           { return sumUnconditioned(getModifiers(), "tradeRoutes.city.modifier", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getForeignTradeRouteModifier() const    { return sumUnconditioned(getModifiers(), "tradeRoutes.city.foreignModifier", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getFreeExperience() const               { return sumUnconditioned(getModifiers(), "experience.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getGlobalFreeExperience() const         { return sumUnconditioned(getModifiers(), "experience.empire", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getFreeSpecialist() const               { return sumUnconditioned(getModifiers(), "freeSpecialists.city.any", CASC_UNIT_COUNT); }
int CvJsonBuildingInfo::getAreaFreeSpecialist() const           { return sumUnconditioned(getModifiers(), "freeSpecialists.area.any", CASC_UNIT_COUNT); }
int CvJsonBuildingInfo::getGlobalFreeSpecialist() const         { return sumUnconditioned(getModifiers(), "freeSpecialists.empire.any", CASC_UNIT_COUNT); }
int CvJsonBuildingInfo::getAnarchyModifier() const              { return sumUnconditioned(getModifiers(), "anarchy.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGoldenAgeModifier() const            { return sumUnconditioned(getModifiers(), "goldenAge.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getPopulationgrowthratepercentage() const       { return sumUnconditioned(getModifiers(), "populationGrowthRate.city", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getGlobalPopulationgrowthratepercentage() const { return sumUnconditioned(getModifiers(), "populationGrowthRate.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getRevIdxLocal() const                  { return sumUnconditioned(getModifiers(), "revolution.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getRevIdxNational() const               { return sumUnconditioned(getModifiers(), "revolution.empire", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getRevIdxDistanceModifier() const       { return sumUnconditioned(getModifiers(), "revolution.city.distanceModifier", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getInsidiousness() const                { return sumUnconditioned(getModifiers(), "copsAndRobbers.city.insidiousness", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getInvestigation() const                { return sumUnconditioned(getModifiers(), "copsAndRobbers.city.investigation", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getEspionageDefenseModifier() const     { return sumUnconditioned(getModifiers(), "espionageDefense.city", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getUnitUpgradePriceModifier() const     { return sumUnconditioned(getModifiers(), "unitUpgradePrice.empire", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getDefenseModifier() const              { return sumUnconditioned(getModifiers(), "defense.city.amount", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getBombardDefenseModifier() const       { return sumUnconditioned(getModifiers(), "defense.city.bombardDefense", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getAllCityDefenseModifier() const       { return sumUnconditioned(getModifiers(), "defense.empire.amount", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getNukeModifier() const                 { return sumUnconditioned(getModifiers(), "defense.city.nukeDefense", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getAirModifier() const                  { return sumUnconditioned(getModifiers(), "defense.city.airDefense", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getMinDefense() const                   { return sumUnconditioned(getModifiers(), "defense.city.min", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getNoEntryDefenseLevel() const          { return sumUnconditioned(getModifiers(), "defense.city.noEntryLevel", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getLocalDynamicDefense() const          { return sumUnconditioned(getModifiers(), "defense.city.dynamicDefense", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getRiverDefensePenalty() const          { return sumUnconditioned(getModifiers(), "defense.city.riverDefensePenalty", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getBuildingDefenseRecoverySpeedModifier() const { return sumUnconditioned(getModifiers(), "defense.city.buildingDefenseRecovery", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getCityDefenseRecoverySpeedModifier() const     { return sumUnconditioned(getModifiers(), "defense.city.cityDefenseRecovery", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getDamageAttackerChance() const         { return sumUnconditioned(getModifiers(), "defense.city.damageAttackerChance", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getDamageToAttacker() const             { return sumUnconditioned(getModifiers(), "defense.city.damageToAttacker", CASC_UNIT_FLAT); }
int CvJsonBuildingInfo::getAdjacentDamagePercent() const        { return sumUnconditioned(getModifiers(), "defense.city.adjacentDamage", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getNationalCaptureProbabilityModifier() const { return sumUnconditioned(getModifiers(), "cityCapture.empire.probability", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getNationalCaptureResistanceModifier() const  { return sumUnconditioned(getModifiers(), "cityCapture.empire.resistance", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getLocalCaptureProbabilityModifier() const    { return sumUnconditioned(getModifiers(), "cityCapture.city.probability", CASC_UNIT_PERCENT); }
int CvJsonBuildingInfo::getLocalCaptureResistanceModifier() const     { return sumUnconditioned(getModifiers(), "cityCapture.city.resistance", CASC_UNIT_PERCENT); }
// commerceHappiness.city.<commerce>.flat -- happiness gained per unit of each commerce produced (grouped family).
int CvJsonBuildingInfo::getCommerceHappiness(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? sumUnconditioned(getModifiers(), std::string("commerceHappiness.city.") + BLD_COMM_NAME[i], CASC_UNIT_FLAT) : 0; }

// commerce double-time / state-religion commerce -- the {channel:value} maps (REAL data).
int CvJsonBuildingInfo::getCommerceChangeDoubleTime(int i) const
{
	if (i < 0 || i >= NUM_COMMERCE_TYPES) return 0;
	std::map<std::string, int>::const_iterator it = commerceDoubleTime.find(BLD_COMM_NAME[i]);
	return it != commerceDoubleTime.end() ? it->second : 0;
}
int CvJsonBuildingInfo::getStateReligionCommerce(int i) const
{
	if (i < 0 || i >= NUM_COMMERCE_TYPES) return 0;
	std::map<std::string, int>::const_iterator it = stateReligionCommerce.find(BLD_COMM_NAME[i]);
	return it != stateReligionCommerce.end() ? it->second : 0;
}

// commerce sliders this building unlocks (`capabilities` block -- canSet{Science|Culture|Espionage}Rate; gold has no
// slider). Mirror of CvJsonTechInfo::isCommerceFlexible (capabilities.md key mapping) -- REAL data.
bool CvJsonBuildingInfo::isCommerceFlexible(int i) const
{
	return (i == COMMERCE_RESEARCH  && getCapabilities()->has("canSetScienceRate"))
	    || (i == COMMERCE_CULTURE   && getCapabilities()->has("canSetCultureRate"))
	    || (i == COMMERCE_ESPIONAGE && getCapabilities()->has("canSetEspionageRate"));
}

// FoundsCorporation -> the building's `enables.corporations` edge (curate_building.py, owner 2026-07-01) -- REAL data.
int CvJsonBuildingInfo::getFoundsCorporation() const
{ const std::vector<int>* v = edge("enables.corporations"); return (v != NULL && !v->empty()) ? (*v)[0] : -1; }

// REAL: grants.repeatable[] unitCombat heal (getHealUnitCombatType) + identity.enabledCivilizations
// (getEnabledCivilizationType). getBonusAidModifier / getAidRateChange stay zero-filled statics (CURATOR-GAP: both
// AidRateChanges + BonusAidModifiers are DROPPED as dead by curate_building.py, so getNum*() returns 0 -> unreached).
const HealUnitCombat& CvJsonBuildingInfo::getHealUnitCombatType(int iIndex) const
{
	if (iIndex >= 0 && iIndex < (int)m_healUnitCombats.size()) return m_healUnitCombats[iIndex];
	static const HealUnitCombat s = { (UnitCombatTypes)-1, 0, 0 }; return s;
}
const BonusAidModifiers& CvJsonBuildingInfo::getBonusAidModifier(int /*iIndex*/) const
{ static const BonusAidModifiers s = { (BonusTypes)-1, (PropertyTypes)-1, 0 }; return s; }
const AidRateChanges& CvJsonBuildingInfo::getAidRateChange(int /*iIndex*/) const
{ static const AidRateChanges s = { (PropertyTypes)-1, 0 }; return s; }
const EnabledCivilizations& CvJsonBuildingInfo::getEnabledCivilizationType(int iIndex) const
{
	if (iIndex >= 0 && iIndex < (int)m_enabledCivTypes.size()) return m_enabledCivTypes[iIndex];
	static const EnabledCivilizations s = { (CivilizationTypes)-1 }; return s;
}

// --- Python-binding list wrappers (CyInfoInterface1 .def-binds these). Every underlying keyed getter is STUB-empty on
// this poco, so each wrapper iterates nothing and returns an empty list -- behaviour-equivalent to the archived
// bodies run over empty data (SourceArchive/Infos/CvBuildingInfo.cpp). ---
const python::list CvJsonBuildingInfo::cyGetGlobalBuildingCommerceChanges() const { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetTechYieldChanges100() const          { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetTechYieldModifiers() const           { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetTechCommerceChanges100() const       { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetTechCommerceModifiers() const        { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetTerrainYieldChanges() const          { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetPlotYieldChanges() const             { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetImprovementYieldChanges() const      { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetGlobalImprovementYieldChanges() const { return python::list(); }
const python::list CvJsonBuildingInfo::cyGetFreePromoTypes() const               { return python::list(); }

// obsoletedBy = the obsoleting tech (ObsoleteTech) + the superseding building (ObsoletesToBuilding), authored
// directly off THIS building's own fields (curate_building.py, "no store inversion"), composed into m_edges by
// the base dispatch. NB a RELIC-SHELL supersession (a non-constructible ObsoletesToBuilding target -- the 6 wonder
// relics) is authored to `whenObsolete` INSTEAD of `obsoletedBy.buildings`, so getObsoletesToBuilding() faithfully
// returns NO_BUILDING for those; the reduced-output tree lives on getWhenObsolete().
TechTypes CvJsonBuildingInfo::getObsoleteTech() const
{ const std::vector<int>* v = edge("obsoletedBy.techs"); return (TechTypes)((v != NULL && !v->empty()) ? (*v)[0] : NO_TECH); }
BuildingTypes CvJsonBuildingInfo::getObsoletesToBuilding() const
{ const std::vector<int>* v = edge("obsoletedBy.buildings"); return (BuildingTypes)((v != NULL && !v->empty()) ? (*v)[0] : NO_BUILDING); }

// EXE-bound art surface: ARTFILEMGR keyed by the art-define tag (mirrors SourceArchive/Infos/CvBonusInfo.cpp's
// shim pattern, re-based onto the JSON-mapped m_szArtDefineTag).
const CvArtInfoBuilding* CvJsonBuildingInfo::getArtInfo() const
{
	return ARTFILEMGR.getBuildingArtInfo(getArtDefineTag());
}

const char* CvJsonBuildingInfo::getButton() const   // art-define button (mirrors archived CvBuildingInfo::getButton)
{
	const CvArtInfoBuilding* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}

// Wonder-movie art surface (mirrors the archived CvBuildingInfo::getMovieInfo/getMovie), keyed by ui.art.movie.defineTag.
const CvArtInfoMovie* CvJsonBuildingInfo::getMovieInfo() const
{
	const char* pcTag = getMovieDefineTag();
	if (pcTag != NULL && *pcTag != '\0' && strcmp(pcTag, "NONE") != 0) return ARTFILEMGR.getMovieArtInfo(pcTag);
	return NULL;
}
const char* CvJsonBuildingInfo::getMovie() const
{
	const CvArtInfoMovie* pArt = getMovieInfo();
	return pArt ? pArt->getPath() : NULL;
}

// =========================================================================================================
//  #430 GROUP 1 (requires condition-tree) + GROUP 2 (keyed m_modifiers) reconstruction into legacy members.
//  Grounded in curate_building.py (requires_building + the SCALAR/TARGET/COND_KEYED tables) and the parsed
//  CvJsonCondition / CvJsonModEntry shapes. Every address here is a curator emission site, not a guess.
// =========================================================================================================

// ---- condition-tree helpers (walk getRequires()->build / operate) ----
static bool cnd_prefix(const CvJsonCondition* c, const char* p) { return c->type.compare(0, strlen(p), p) == 0; }

static bool cnd_hasPredicate(const CvJsonCondition* c, CvCascPredKind k)
{
	if (!c) return false;
	if (c->kind == CASC_COND_PREDICATE) return c->predKind == k;
	size_t i;
	for (i = 0; i < c->all.size(); ++i)    if (cnd_hasPredicate(c->all[i], k)) return true;
	for (i = 0; i < c->anyOf.size(); ++i)  if (cnd_hasPredicate(c->anyOf[i], k)) return true;
	for (i = 0; i < c->noneOf.size(); ++i) if (cnd_hasPredicate(c->noneOf[i], k)) return true;
	return false;   // NB does NOT descend into enabled/disabled -- those are queried separately
}
static const CvJsonCondition* cnd_findPredicate(const CvJsonCondition* c, CvCascPredKind k)
{
	if (!c) return NULL;
	if (c->kind == CASC_COND_PREDICATE && c->predKind == k) return c;
	size_t i; const CvJsonCondition* r;
	for (i = 0; i < c->all.size(); ++i)    { r = cnd_findPredicate(c->all[i], k); if (r) return r; }
	for (i = 0; i < c->anyOf.size(); ++i)  { r = cnd_findPredicate(c->anyOf[i], k); if (r) return r; }
	for (i = 0; i < c->noneOf.size(); ++i) { r = cnd_findPredicate(c->noneOf[i], k); if (r) return r; }
	return NULL;
}
static void cnd_andPresence(const CvJsonCondition* c, const char* prefix, int scope, std::vector<int>& out)
{
	if (!c) return;
	for (size_t i = 0; i < c->all.size(); ++i)
	{
		const CvJsonCondition* e = c->all[i];
		if (e->kind == CASC_COND_PRESENCE && cnd_prefix(e, prefix) && (scope < 0 || (int)e->scope == scope) && e->id >= 0)
			out.push_back(e->id);
	}
}
static void cnd_orPresence(const CvJsonCondition* c, const char* prefix, int scope, std::vector<int>& out)
{
	if (!c) return;
	for (size_t i = 0; i < c->all.size(); ++i)
	{
		const CvJsonCondition* g = c->all[i];
		if (g->kind != CASC_COND_GROUP) continue;
		for (size_t j = 0; j < g->anyOf.size(); ++j)
		{
			const CvJsonCondition* e = g->anyOf[j];
			if (e->kind == CASC_COND_PRESENCE && cnd_prefix(e, prefix) && (scope < 0 || (int)e->scope == scope) && e->id >= 0)
				out.push_back(e->id);
		}
	}
}
static void cnd_nonePresence(const CvJsonCondition* c, const char* prefix, int scope, std::vector<int>& out)
{
	if (!c) return;
	for (size_t i = 0; i < c->noneOf.size(); ++i)
	{
		const CvJsonCondition* e = c->noneOf[i];
		if (e->kind == CASC_COND_PRESENCE && cnd_prefix(e, prefix) && (scope < 0 || (int)e->scope == scope) && e->id >= 0)
			out.push_back(e->id);
	}
}
static int cnd_firstAnd(const CvJsonCondition* c, const char* prefix, int scope)
{ std::vector<int> v; cnd_andPresence(c, prefix, scope, v); return v.empty() ? -1 : v[0]; }
static int cnd_andMin(const CvJsonCondition* c, const char* type)
{
	if (!c) return 0;
	for (size_t i = 0; i < c->all.size(); ++i)
	{ const CvJsonCondition* e = c->all[i]; if (e->kind == CASC_COND_PRESENCE && e->type == type) return e->min > 0 ? e->min : 0; }
	return 0;
}
static bool cnd_matchCV(const CvJsonCondition* e, const char* prefix, CvCascConnection conn, CvCascVicinity vic)
{ return e->kind == CASC_COND_PRESENCE && cnd_prefix(e, prefix) && e->connection == conn && e->vicinity == vic && e->id >= 0; }
static int cnd_firstAndCV(const CvJsonCondition* c, const char* prefix, CvCascConnection conn, CvCascVicinity vic)
{ if (!c) return -1; for (size_t i = 0; i < c->all.size(); ++i) if (cnd_matchCV(c->all[i], prefix, conn, vic)) return c->all[i]->id; return -1; }
static void cnd_orCV(const CvJsonCondition* c, const char* prefix, CvCascConnection conn, CvCascVicinity vic, std::vector<int>& out)
{ if (!c) return; for (size_t i = 0; i < c->all.size(); ++i) { const CvJsonCondition* g = c->all[i]; if (g->kind == CASC_COND_GROUP) for (size_t j = 0; j < g->anyOf.size(); ++j) if (cnd_matchCV(g->anyOf[j], prefix, conn, vic)) out.push_back(g->anyOf[j]->id); } }

// ---- modifier `enabled` inspection + array accumulation ----
static int mod_enabledId(const CvJsonModEntry* e, const char* prefix, int conn, int vic)
{
	const CvJsonCondition* c = e->enabled;
	if (!c || c->kind != CASC_COND_PRESENCE) return -1;
	if (c->type.compare(0, strlen(prefix), prefix) != 0) return -1;
	if (conn >= 0 && (int)c->connection != conn) return -1;
	if (vic >= 0 && (int)c->vicinity != vic) return -1;
	return c->id;
}
static bool mod_enabledPred(const CvJsonModEntry* e, CvCascPredKind k)
{ return e->enabled && e->enabled->kind == CASC_COND_PREDICATE && e->enabled->predKind == k; }

// Array-valued maps: accumulate via addArrayValue (element-wise, insert-or-add) -- NOT getValue/setValue, since
// IDValueMap::getValue returns the scalar defaultValue and would not convert to a Yield/CommerceArray.
template <class MapT, class KeyT> static void mod_addYield(MapT& m, KeyT id, int slot, int val)
{ YieldArray a; a.fill(0); a[slot] = val; m.addArrayValue(id, a); }
template <class MapT, class KeyT> static void mod_addComm(MapT& m, KeyT id, int slot, int val)
{ CommerceArray a; a.fill(0); a[slot] = val; m.addArrayValue(id, a); }
// Scalar (int) maps: getValue/setValue are safe (defaultValue is int 0).
template <class MapT, class KeyT> static void mod_addScalar(MapT& m, KeyT id, int val)
{ m.setValue(id, m.getValue(id) + val); }

static void mod_split(const std::string& s, std::vector<std::string>& out)
{
	size_t start = 0, dot;
	while ((dot = s.find('.', start)) != std::string::npos) { out.push_back(s.substr(start, dot - start)); start = dot + 1; }
	out.push_back(s.substr(start));
}
static int fam_uncond100(const CvJsonModFamily* f, CvCascUnit unit)
{
	if (!f) return 0; int t = 0;
	for (int i = 0; i < f->size(); ++i)
	{ const CvJsonModEntry* e = f->entries[i]; if (e->unit == unit && e->enabled == NULL && e->disabled == NULL && !e->hasPer) t += e->value100; }
	return t;
}
static int bldNameIndex(const char* const* names, int n, const std::string& s)
{ for (int i = 0; i < n; ++i) if (s == names[i]) return i; return -1; }

void CvJsonBuildingInfo::reconstructFromComposed()
{
	// -------------------- GROUP 1: the requires condition tree --------------------
	const CvJsonRequires* req = getRequires();
	const CvJsonCondition* build = req ? req->build : NULL;
	const CvJsonCondition* op    = req ? req->operate : NULL;

	m_iMinAreaSize = cnd_andMin(build, "AREA_SIZE");
	if (m_iMinAreaSize == 0) { const CvJsonCondition* hc = cnd_findPredicate(build, CASC_PRED_HAS_COAST); if (hc && hc->min > 0) m_iMinAreaSize = hc->min; }
	{ const CvJsonCondition* lat = cnd_findPredicate(build, CASC_PRED_LATITUDE);
	  if (lat) { if (lat->min >= 0) m_iMinLatitude = lat->min; if (lat->max >= 0) m_iMaxLatitude = lat->max; } }
	m_iNumCitiesPrereq   = cnd_andMin(build, "CITY");
	m_iNumTeamsPrereq    = cnd_andMin(build, "TEAM");
	m_iPrereqPopulation  = cnd_andMin(build, "POPULATION");
	m_iVictoryPrereq     = cnd_firstAnd(build, "VICTORY_", -1);
	{ const CvJsonCondition* p = cnd_findPredicate(build, CASC_PRED_IS_HOLY_CITY);   if (p) m_iHolyCity = p->id; }
	{ const CvJsonCondition* p = cnd_findPredicate(build, CASC_PRED_STATE_RELIGION); if (p) m_iPrereqStateReligion = p->id; }
	m_iPrereqReligion     = cnd_firstAnd(op, "RELIGION_", CASC_SCOPE_CITY);
	m_iPrereqCorporation  = cnd_firstAnd(op, "CORPORATION_", -1);
	m_iPrereqCultureLevel = cnd_firstAnd(build, "CULTURELEVEL_", -1);
	m_iPrereqAnyoneBuilding = cnd_firstAnd(build, "BUILDING_", CASC_SCOPE_WORLD);
	{ std::vector<int> techs; cnd_andPresence(build, "TECH_", CASC_SCOPE_TEAM, techs);
	  for (size_t i = 0; i < techs.size(); ++i) m_piPrereqAndTechs.push_back((TechTypes)techs[i]);
	  m_iPrereqAndTech = techs.empty() ? -1 : techs[0]; }
	m_bNeedStateReligionInCity = cnd_hasPredicate(build, CASC_PRED_STATE_RELIGION_IN_CITY);
	m_bWater      = cnd_hasPredicate(build, CASC_PRED_HAS_COAST);
	m_bRiver      = cnd_hasPredicate(build, CASC_PRED_HAS_RIVER);
	m_bFreshWater = cnd_hasPredicate(build, CASC_PRED_HAS_FRESHWATER);
	m_bNoHolyCity = build && cnd_hasPredicate(build->disabled, CASC_PRED_IS_HOLY_CITY);
	m_bPower      = cnd_hasPredicate(build, CASC_PRED_HAS_POWER);
	m_iPrereqAndBonus         = cnd_firstAndCV(op, "BONUS_", CASC_CONN_TRADE_OR_VICINITY, CASC_VIC_NONE);
	m_iPrereqVicinityBonus    = cnd_firstAndCV(op, "BONUS_", CASC_CONN_VICINITY, CASC_VIC_CONNECTED);
	m_iPrereqRawVicinityBonus = cnd_firstAndCV(op, "BONUS_", CASC_CONN_VICINITY, CASC_VIC_OWNED);

	cnd_andPresence(build, "BUILDING_", CASC_SCOPE_CITY, m_prereqInCityBuildings);
	cnd_nonePresence(build, "BUILDING_", -1, m_prereqNotInCityBuildings);
	cnd_orPresence(build, "BUILDING_", CASC_SCOPE_CITY, m_prereqOrBuildings);
	cnd_andPresence(op, "CIVIC_", CASC_SCOPE_EMPIRE, m_prereqAndCivics);
	cnd_orPresence(op, "CIVIC_", CASC_SCOPE_EMPIRE, m_prereqOrCivics);
	cnd_andPresence(build, "TERRAIN_", CASC_SCOPE_PLOT, m_prereqAndTerrains);
	cnd_orPresence(build, "TERRAIN_", CASC_SCOPE_PLOT, m_prereqOrTerrains);
	cnd_orPresence(build, "FEATURE_", CASC_SCOPE_PLOT, m_prereqOrFeatures);
	{ std::vector<int> t; cnd_orPresence(build, "IMPROVEMENT_", CASC_SCOPE_PLOT, t); for (size_t i = 0; i < t.size(); ++i) m_prereqOrImprovement.push_back((ImprovementTypes)t[i]); }
	{ std::vector<int> t; cnd_orPresence(build, "HERITAGE_", CASC_SCOPE_EMPIRE, t); for (size_t i = 0; i < t.size(); ++i) m_prereqOrHeritage.push_back((HeritageTypes)t[i]); }
	{ std::vector<int> t; cnd_orCV(op, "BONUS_", CASC_CONN_TRADE_OR_VICINITY, CASC_VIC_NONE, t); for (size_t i = 0; i < t.size(); ++i) m_aePrereqOrBonuses.push_back((BonusTypes)t[i]); }
	{ std::vector<int> t; cnd_orCV(op, "BONUS_", CASC_CONN_VICINITY, CASC_VIC_CONNECTED, t); for (size_t i = 0; i < t.size(); ++i) m_piPrereqOrVicinityBonuses.push_back((BonusTypes)t[i]); }
	{ std::vector<int> t; cnd_orCV(op, "BONUS_", CASC_CONN_VICINITY, CASC_VIC_OWNED, t); for (size_t i = 0; i < t.size(); ++i) m_aePrereqOrRawVicinityBonuses.push_back((BonusTypes)t[i]); }
	// PrereqAmountBuildings: BUILDING (empire scope) with a min -> building->count map.
	if (build) for (size_t i = 0; i < build->all.size(); ++i)
	{ const CvJsonCondition* e = build->all[i]; if (e->kind == CASC_COND_PRESENCE && cnd_prefix(e, "BUILDING_") && e->scope == CASC_SCOPE_EMPIRE && e->id >= 0)
	      m_aPrereqNumOfBuilding.setValue((BuildingTypes)e->id, e->min > 0 ? e->min : 0); }

	// -------------------- GROUP 2: the keyed modifier families --------------------
	const CvJsonModifiers* mods = getModifiers();
	if (!mods) return;
	static const char* YN[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
	static const char* CN[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

	// Part A: TARGET-KEYED (unconditioned) -- iterate every family, dispatch by dotted address.
	const std::map<std::string, CvJsonModFamily*>& all = mods->all();
	for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
	{
		std::vector<std::string> s; mod_split(it->first, s);
		const CvJsonModFamily* f = it->second;
		if (s.size() < 2) continue;
		// the trailing FK segment: resolve ONLY when it looks like an INFOTYPE id (contains '_'), so scope/member
		// words (city/empire/terrains/buildings/...) never hit jsonResolveId and pollute the unresolved-id census.
		const std::string& last = s[s.size() - 1];
		const int fk = (last.find('_') != std::string::npos) ? jsonResolveId(last) : -1;
		const std::string& f0 = s[0];

		// yields: <yield>.city.terrains.<T> / <yield>.city.improvements.<I> / <yield>.empire.improvements.<I>
		const int yi = bldNameIndex(YN, NUM_YIELD_TYPES, f0);
		if (yi >= 0 && s.size() == 4 && fk >= 0)
		{
			const int v = fam_uncond100(f, CASC_UNIT_FLAT) / 100;
			if (s[1] == "city" && s[2] == "terrains")          mod_addYield(m_aTerrainYieldChanges, (TerrainTypes)fk, yi, v);
			else if (s[1] == "city" && s[2] == "improvements") mod_addYield(m_aImprovementYieldChanges, (ImprovementTypes)fk, yi, v);
			else if (s[1] == "empire" && s[2] == "improvements") mod_addYield(m_aGlobalImprovementYieldChanges, (ImprovementTypes)fk, yi, v);
			continue;
		}
		// commerces: <commerce>.empire.buildings.<B>  (GlobalBuildingExtraCommerces)
		const int ci = bldNameIndex(CN, NUM_COMMERCE_TYPES, f0);
		if (ci >= 0 && s.size() == 4 && fk >= 0 && s[1] == "empire" && s[2] == "buildings")
		{ mod_addComm(m_aGlobalBuildingCommerceChanges, (BuildingTypes)fk, ci, fam_uncond100(f, CASC_UNIT_FLAT) / 100); continue; }

		if (f0 == "happiness" && s.size() == 4 && fk >= 0 && s[1] == "empire" && s[2] == "buildings")
		{ mod_addScalar(m_aBuildingHappinessChanges, (BuildingTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100); continue; }
		if (f0 == "religion" && s.size() == 3 && fk >= 0 && s[1] == "city")
		{ mod_addScalar(m_religionChange, (ReligionTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100); continue; }
		if (f0 == "defense" && s.size() == 4 && fk >= 0 && s[1] == "city")
		{ if (s[2] == "bonuses")     mod_addScalar(m_bonusDefenseChanges, (BonusTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100);
		  else if (s[2] == "unitCombats") mod_addScalar(m_unitCombatDefenseAgainst, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100);
		  continue; }
		if (f0 == "experience" && s.size() == 4 && fk >= 0 && s[1] == "city")
		{ if (s[2] == "unitCombats") mod_addScalar(m_aUnitCombatFreeExperience, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100);
		  else if (s[2] == "domains") mod_addScalar(m_domainFreeExperience, (DomainTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100);
		  continue; }
		if (f0 == "strength" && s.size() == 4 && fk >= 0 && s[1] == "city" && s[2] == "unitCombats")
		{ mod_addScalar(m_aUnitCombatExtraStrength, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT) / 100); continue; }
		if (f0 == "buildRate" && s.size() == 4 && fk >= 0)
		{ const int v = fam_uncond100(f, CASC_UNIT_PERCENT) / 100;
		  if (s[1] == "city" && s[2] == "units")            mod_addScalar(m_aUnitProductionModifier, (UnitTypes)fk, v);
		  else if (s[1] == "city" && s[2] == "unitCombats") mod_addScalar(m_unitCombatProdModifier, (UnitCombatTypes)fk, v);
		  else if (s[1] == "city" && s[2] == "domains")     mod_addScalar(m_domainProductionModifier, (DomainTypes)fk, v);
		  else if (s[1] == "city" && s[2] == "buildings")   mod_addScalar(m_aBuildingProductionModifier, (BuildingTypes)fk, v);
		  else if (s[1] == "empire" && s[2] == "buildings") mod_addScalar(m_aGlobalBuildingProductionModifier, (BuildingTypes)fk, v);
		  continue; }
		// specialist capacity families -- allowedSpecialists.city.<S> (SpecialistCounts + TechSpecialistChanges) /
		// freeSpecialists.city.<S> (FreeSpecialistCounts); freeSpecialists.city.any per:{IMPROVEMENT}.
		if (f0 == "allowedSpecialists" && s.size() == 3 && fk >= 0 && s[1] == "city")
		{
			for (int i = 0; i < f->size(); ++i)
			{
				const CvJsonModEntry* e = f->entries[i];
				if (e->unit != CASC_UNIT_COUNT || e->disabled) continue;
				if (e->enabled == NULL) mod_addScalar(m_specialistCount, (SpecialistTypes)fk, e->value100 / 100);
				else { const int tech = mod_enabledId(e, "TECH_", -1, -1); if (tech >= 0) m_techSpecialistChange[tech][fk] += e->value100 / 100; }
			}
			continue;
		}
		if (f0 == "freeSpecialists" && s.size() == 3 && s[1] == "city")
		{
			if (s[2] == "any")
			{
				for (int i = 0; i < f->size(); ++i)
				{ const CvJsonModEntry* e = f->entries[i]; if (e->hasPer && e->perTypeId >= 0) mod_addScalar(m_improvementFreeSpecialists, (ImprovementTypes)e->perTypeId, e->value100 / 100); }
			}
			else if (fk >= 0) mod_addScalar(m_freeSpecialistCount, (SpecialistTypes)fk, fam_uncond100(f, CASC_UNIT_COUNT) / 100);
			continue;
		}
	}

	// Part B: COND-KEYED (base address; the entry `enabled` carries the key).
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		const CvJsonModFamily* f = mods->find(std::string(YN[y]) + ".city");
		if (!f) continue;
		for (int i = 0; i < f->size(); ++i)
		{
			const CvJsonModEntry* e = f->entries[i];
			if (e->enabled == NULL || e->disabled) continue;
			const int tech = mod_enabledId(e, "TECH_", -1, -1);
			if (tech >= 0)
			{
				if (e->unit == CASC_UNIT_FLAT)         mod_addYield(m_techYieldChanges, (TechTypes)tech, y, e->value100);       // PER100: raw x100
				else if (e->unit == CASC_UNIT_PERCENT) mod_addYield(m_techYieldModifiers, (TechTypes)tech, y, e->value100 / 100);
				continue;
			}
			const int vbon = mod_enabledId(e, "BONUS_", CASC_CONN_VICINITY, CASC_VIC_CONNECTED);
			if (vbon >= 0 && e->unit == CASC_UNIT_FLAT) { mod_addYield(m_vicinityBonusYieldChanges, (BonusTypes)vbon, y, e->value100 / 100); continue; }
			const int bon = mod_enabledId(e, "BONUS_", CASC_CONN_NONE, CASC_VIC_NONE);
			if (bon >= 0)
			{
				if (e->unit == CASC_UNIT_FLAT)         mod_addYield(m_bonusYieldChanges, (BonusTypes)bon, y, e->value100 / 100);
				else if (e->unit == CASC_UNIT_PERCENT) mod_addYield(m_bonusYieldModifier, (BonusTypes)bon, y, e->value100 / 100);
				continue;
			}
			if (e->unit == CASC_UNIT_PERCENT && mod_enabledPred(e, CASC_PRED_HAS_POWER)) m_powerYieldModifier[y] += e->value100 / 100;
		}
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const CvJsonModFamily* f = mods->find(std::string(CN[c]) + ".city");
		if (!f) continue;
		for (int i = 0; i < f->size(); ++i)
		{
			const CvJsonModEntry* e = f->entries[i];
			if (e->enabled == NULL || e->disabled) continue;
			const int tech = mod_enabledId(e, "TECH_", -1, -1);
			if (tech >= 0)
			{
				if (e->unit == CASC_UNIT_FLAT)         mod_addComm(m_techCommerceChanges, (TechTypes)tech, c, e->value100);   // PER100: raw x100
				else if (e->unit == CASC_UNIT_PERCENT) mod_addComm(m_techCommerceModifiers, (TechTypes)tech, c, e->value100 / 100);
				continue;
			}
			const int bon = mod_enabledId(e, "BONUS_", CASC_CONN_NONE, CASC_VIC_NONE);
			if (bon >= 0 && e->unit == CASC_UNIT_FLAT) mod_addComm(m_bonusCommercePercentChanges, (BonusTypes)bon, c, e->value100);   // PER100: raw x100
		}
	}
	// happiness.city / health.city flat, keyed by TECH or BONUS
	for (int h = 0; h < 2; ++h)
	{
		const char* fam = (h == 0) ? "happiness.city" : "health.city";
		const CvJsonModFamily* f = mods->find(fam);
		if (!f) continue;
		for (int i = 0; i < f->size(); ++i)
		{
			const CvJsonModEntry* e = f->entries[i];
			if (e->enabled == NULL || e->disabled || e->unit != CASC_UNIT_FLAT) continue;
			const int tech = mod_enabledId(e, "TECH_", -1, -1);
			const int bon  = (tech < 0) ? mod_enabledId(e, "BONUS_", -1, -1) : -1;
			if (h == 0) { if (tech >= 0) mod_addScalar(m_aTechHappinessChanges, (TechTypes)tech, e->value100 / 100); else if (bon >= 0) mod_addScalar(m_piBonusHappinessChanges, (BonusTypes)bon, e->value100 / 100);
			              else if (mod_enabledPred(e, CASC_PRED_STATE_RELIGION) || mod_enabledPred(e, CASC_PRED_HAS_STATE_RELIGION) || mod_enabledPred(e, CASC_PRED_STATE_RELIGION_IN_CITY)) m_iStateReligionHappiness += e->value100 / 100; }
			else        { if (tech >= 0) mod_addScalar(m_aTechHealthChanges, (TechTypes)tech, e->value100 / 100); else if (bon >= 0) mod_addScalar(m_piBonusHealthChanges, (BonusTypes)bon, e->value100 / 100); }
		}
	}
	// buildRate.self percent enabled BONUS -> bonus production modifier
	{ const CvJsonModFamily* f = mods->find("buildRate.self");
	  if (f) for (int i = 0; i < f->size(); ++i) { const CvJsonModEntry* e = f->entries[i]; if (e->enabled && !e->disabled && e->unit == CASC_UNIT_PERCENT) { const int b = mod_enabledId(e, "BONUS_", -1, -1); if (b >= 0) mod_addScalar(m_bonusProductionModifier, (BonusTypes)b, e->value100 / 100); } } }
	// costs.empire percent enabled BUILDING -> global building cost modifier
	{ const CvJsonModFamily* f = mods->find("costs.empire");
	  if (f) for (int i = 0; i < f->size(); ++i) { const CvJsonModEntry* e = f->entries[i]; if (e->enabled && !e->disabled && e->unit == CASC_UNIT_PERCENT) { const int b = mod_enabledId(e, "BUILDING_", -1, -1); if (b >= 0) mod_addScalar(m_aGlobalBuildingCostModifier, (BuildingTypes)b, e->value100 / 100); } } }
}
