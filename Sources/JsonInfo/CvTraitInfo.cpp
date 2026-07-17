//
//	CvTraitInfo -- ctor, mapFrom, and the mirrored-getter support. Grounded field-by-field against
//	Tools/Migration/curate_trait.py + the shipped Assets/Data/traits/**/*.json (addresses verified, never guessed).
//
//	REAL DATA:
//	  - §6 scalar modifier families + the grouped stateReligion / positional yield-commerce arrays: read in mapFrom
//	    via traitScalar -- the UNCONDITIONED sum at the leaf (a bare number, or the no-`enabled`/`disabled`/`per`/`unit`
//	    entries of a merged list). Summing only the unconditioned entries recovers the legacy plain scalar EXACTLY even
//	    when a conditioned addend (a per-bonus happiness, an IS_CAPITAL yield%) is merged into the same leaf list
//	    (CvCivicInfo's proven sumUnconditioned approach; jsonFamVal alone returns 0 on such a list -- the bug fixed).
//	  - RevIdx* (revolution.empire.<member>.<unit>; the fRev* are floats carried verbatim).
//	  - target-KEYED families the consumer iterates by list-index: enumerated into typed std::vectors in mapFrom
//	    (buildRate.empire.{buildings|units|specialUnits|unitCombats}.percent, experience.empire.unitCombats.flat,
//	    upkeep.empire.civicOptions.enabler, top-level `excludes`).
//	  - 2D specialist/improvement yield/commerce (per actual type id): read live off the parsed m_modifiers via a
//	    target's own GC type string (CvCivicInfo's sumKeyed template).
//	  - FK references: succession.promotionLine, grants.eraAdvanceFreeSpecialist, grants.goldenAgeOnBirthOfGreatPerson,
//	    greatPeopleRate.empire.units.{UNIT} (the GP unit + its rate change).
//
//	  - the reference-returning IDValueMap<X,int>[::filtered] getters: populated in mapFrom via IDValueMap::setValue per
//	    keyed entry (the addresses noted at each traitFillMap call + the per-bonus happiness walk).
//	  - conditioned IS_CAPITAL / ranked-cities / unit-qualified leaves (getCapital*/getLargestCityHappiness/
//	    getHappyPerMilitaryUnit/getHolyCityofNonStateReligionXPModifier/getSeaPlotYieldChanges): REAL -- the legacy
//	    getter returns the raw magnitude and the legacy consumer applies the predicate/ranked gate itself.
//
//	STUB (genuine, documented curator-DROP / store-inversion -- NOT reachable from this trait's JSON):
//	  - game-option gates (isValidTrait / On|NotOnGameOption / Category): curator-DROPPED (the simple/complex folder
//	    split IS the option gate; Categories dead) -> false/0/-1.
//	  - prereq FKs (getPrereqTrait/getPrereqOrTrait1/2/getPrereqTech): curator store-INVERTS them onto the prereq
//	    entity's enables.traits -> not authored on this trait -> NO_TRAIT/NO_TECH.
//	  - getPropertyManipulators (XML-era property engine, deferred) + the three 2D bulk int* array views
//	    (getSpecialist{Yield,Commerce}ChangeArray/getImprovementYieldChangeArray): no live cascade consumer.
//
//	Inherited by CvSimpleTraitInfo / CvComplexTraitInfo. See the header for the exact per-getter mapping.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson, GC, boost range/bind
#include "CvTraitInfo.h"
#include "CvJsonParse.h"            // jsonChildObj/jsonResolveId/jsonIdBool/jsonIdInt/jsonIdStr/jsonReadFlavours
#include "CvJsonModScan.h"          // the ONE load-time modifier-family scan (mapFrom materialization)
#include "CvJsonModEntry.h"         // CvJsonModFamily/CvJsonModEntry/CvCascUnit -- the parsed-modifier live reads (2D)
#include "CvCascadePropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "CvSpecialistInfo.h"   // GC.getSpecialistInfo(i).getType() -- the 2D specialist address
#include "CvImprovementInfo.h" // GC.getImprovementInfo(i).getType() -- the 2D improvement address

namespace
{
	// The curator's family names for the split yield/commerce arrays (json.md §6; YIELD_*/COMMERCE_* order).
	const char* const YIELD_FAM[NUM_YIELD_TYPES]       = { "food", "production", "commerce" };
	const char* const COMMERCE_FAM[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

	// --- empty-view `filtered` builder (concrete bst::function predicate; a raw template-fn-pointer won't convert -- C2664) ---
	template <class VT>
	struct TraitAllPass { bool operator()(const VT&) const { return true; } };
	template <class Map>
	typename Map::filtered jsonEmptyFiltered(const Map& m)
	{
		typedef typename Map::value_type VT;
		bst::function<bool(const VT&)> pred = TraitAllPass<VT>();
		return typename Map::filtered(pred, m);
	}

	// Navigate entity[fam][scope]([member])?[unit] -> the leaf value*, or NULL.
	const picojson::value* traitLeaf(const picojson::object& o, const char* fam, const char* scope, const char* member, const char* unit)
	{
		const picojson::object* fo = jsonChildObj(o, fam);     if (!fo) return NULL;
		const picojson::object* so = jsonChildObj(*fo, scope); if (!so) return NULL;
		const picojson::object* mo = member ? jsonChildObj(*so, member) : so; if (!mo) return NULL;
		picojson::object::const_iterator u = mo->find(unit);   if (u == mo->end()) return NULL;
		return &u->second;
	}

	// The UNCONDITIONED magnitude at a leaf: a bare number, or (a merged §3.9 list) the sum of entries carrying NONE of
	// enabled/disabled/per/unit -- i.e. the legacy plain scalar, with conditioned addends excluded.
	double traitLeafSum(const picojson::value* leaf)
	{
		if (!leaf) return 0.0;
		if (leaf->is<double>()) return leaf->get<double>();
		if (!leaf->is<picojson::array>()) return 0.0;
		const picojson::array& a = leaf->get<picojson::array>();
		double total = 0.0;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].is<double>()) { total += a[i].get<double>(); continue; }
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			if (e.find("enabled") != e.end() || e.find("disabled") != e.end()
			 || e.find("per") != e.end() || e.find("unit") != e.end()) continue;   // conditioned/scaled -> skip
			picojson::object::const_iterator v = e.find("value");
			if (v != e.end() && v->second.is<double>()) total += v->second.get<double>();
		}
		return total;
	}
	int    traitScalar (const picojson::object& o, const char* fam, const char* scope, const char* member, const char* unit)
	{ return (int)traitLeafSum(traitLeaf(o, fam, scope, member, unit)); }
	double traitScalarF(const picojson::object& o, const char* fam, const char* scope, const char* member, const char* unit)
	{ return traitLeafSum(traitLeaf(o, fam, scope, member, unit)); }

	// Enumerate entity[fam][scope][targetType] = { TARGET: { unit: value } } -> (resolvedId, humanInt) pairs
	// (a number leaf, or a bool `enabler` -> 1). Unresolved targets are skipped.
	void traitCollectKeyed(const picojson::object& o, const char* fam, const char* scope, const char* tt, const char* unit,
	                       std::vector<std::pair<int, int> >& out)
	{
		const picojson::object* fo = jsonChildObj(o, fam);     if (!fo) return;
		const picojson::object* so = jsonChildObj(*fo, scope); if (!so) return;
		const picojson::object* to = jsonChildObj(*so, tt);    if (!to) return;
		for (picojson::object::const_iterator it = to->begin(); it != to->end(); ++it)
		{
			int id = jsonResolveId(it->first);
			if (id < 0 || !it->second.is<picojson::object>()) continue;
			const picojson::object& e = it->second.get<picojson::object>();
			picojson::object::const_iterator u = e.find(unit);
			if (u == e.end()) continue;
			int v = 0;
			if (u->second.is<double>())    v = (int)u->second.get<double>();
			else if (u->second.is<bool>()) v = u->second.get<bool>() ? 1 : 0;
			out.push_back(std::make_pair(id, v));
		}
	}

	// setValue every keyed (id, UNCONDITIONED value) at o[fam][scope][tt] into `map`, skipping 0 (load-path contract).
	template <class Map>
	void traitFillMap(const picojson::object& o, const char* fam, const char* scope, const char* tt, const char* unit, Map& map)
	{
		std::vector<std::pair<int, int> > v;
		traitCollectKeyed(o, fam, scope, tt, unit, v);
		for (size_t k = 0; k < v.size(); ++k)
			if (v[k].second != 0)
				map.setValue((typename Map::value_type::first_type)v[k].first, v[k].second);
	}

	// Sum the values of list entries whose `enabled` is exactly the bare-string predicate `pred` (e.g. "IS_CAPITAL"/"IS_WATER").
	int traitCondStr(const picojson::value* leaf, const char* pred)
	{
		if (!leaf || !leaf->is<picojson::array>()) return 0;
		const picojson::array& a = leaf->get<picojson::array>();
		int total = 0;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			picojson::object::const_iterator en = e.find("enabled");
			if (en == e.end() || !en->second.is<std::string>() || en->second.get<std::string>() != pred) continue;
			picojson::object::const_iterator vv = e.find("value");
			if (vv != e.end() && vv->second.is<double>()) total += (int)vv->second.get<double>();
		}
		return total;
	}
	// Sum the values of list entries whose `enabled` is an OBJECT (a composite predicate) -- the holy-city-nonState case
	// (enabled:{all:[IS_HOLY_CITY, !IS_STATE_RELIGION_HOLY_CITY]}, the only object-enabled entry on experience.empire.percent).
	int traitCondObj(const picojson::value* leaf)
	{
		if (!leaf || !leaf->is<picojson::array>()) return 0;
		const picojson::array& a = leaf->get<picojson::array>();
		int total = 0;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			picojson::object::const_iterator en = e.find("enabled");
			if (en == e.end() || !en->second.is<picojson::object>()) continue;
			picojson::object::const_iterator vv = e.find("value");
			if (vv != e.end() && vv->second.is<double>()) total += (int)vv->second.get<double>();
		}
		return total;
	}
	// Sum the values of list entries carrying `unit`==qual (the §3.7 unit qualifier, e.g. "IS_MILITARY").
	int traitUnitQual(const picojson::value* leaf, const char* qual)
	{
		if (!leaf || !leaf->is<picojson::array>()) return 0;
		const picojson::array& a = leaf->get<picojson::array>();
		int total = 0;
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			picojson::object::const_iterator uq = e.find("unit");
			if (uq == e.end() || !uq->second.is<std::string>() || uq->second.get<std::string>() != qual) continue;
			picojson::object::const_iterator vv = e.find("value");
			if (vv != e.end() && vv->second.is<double>()) total += (int)vv->second.get<double>();
		}
		return total;
	}
}

CvTraitInfo::CvTraitInfo()
	: negativeTrait(false), civilizationTrait(false), m_bAnySpecYield(false), m_bAnySpecCommerce(false)
	, m_iHealth(0), m_iHappiness(0), m_iMaxAnarchy(-1), m_iMinAnarchy(0), m_iUpkeepModifier(0), m_iLevelExperienceModifier(0)
	, m_iGreatPeopleRateModifier(0), m_iGreatGeneralRateModifier(0), m_iDomesticGreatGeneralRateModifier(0)
	, m_iMaxGlobalBuildingProductionModifier(0), m_iMaxTeamBuildingProductionModifier(0), m_iMaxPlayerBuildingProductionModifier(0)
	, m_iWarWearinessAccumulationModifier(0), m_iCivicAnarchyTimeModifier(0), m_iReligiousAnarchyTimeModifier(0)
	, m_iImprovementUpgradeRateModifier(0), m_iWorkerSpeedModifier(0), m_iMaxConscript(0)
	, m_iDistanceMaintenanceModifier(0), m_iNumCitiesMaintenanceModifier(0), m_iCorporationMaintenanceModifier(0)
	, m_iStateReligionGreatPeopleRateModifier(0), m_iFreeExperience(0)
	, m_iFreeUnitUpkeepCivilian(0), m_iFreeUnitUpkeepMilitary(0), m_iFreeUnitUpkeepCivilianPopPercent(0), m_iFreeUnitUpkeepMilitaryPopPercent(0)
	, m_iCivilianUnitUpkeepMod(0), m_iMilitaryUnitUpkeepMod(0), m_iFreeSpecialist(0), m_iTradeRoutes(0)
	, m_iStateReligionHappiness(0), m_iNonStateReligionHappiness(0), m_iStateReligionUnitProductionModifier(0)
	, m_iStateReligionBuildingProductionModifier(0), m_iStateReligionFreeExperience(0), m_iExpInBorderModifier(0)
	, m_iCityDefenseBonus(0), m_iMilitaryProductionModifier(0), m_iAttitudeModifier(0), m_iEspionageDefense(0)
	, m_iMaxTradeRoutesChange(0), m_iGoldenAgeDurationModifier(0), m_iGreatPeopleRateChange(0), m_iHurryAngerModifier(0)
	, m_iHurryCostModifier(0), m_iEnemyWarWearinessModifier(0), m_iForeignTradeRouteModifier(0), m_iBombardDefense(0)
	, m_iUnitUpgradePriceModifier(0), m_iCoastalTradeRoutes(0), m_iGlobalPopulationgrowthratepercentage(0)
	, m_iCityStartCulture(0), m_iGlobalAirUnitCapacity(0), m_iHolyCityofStateReligionXPModifier(0)
	, m_iBonusPopulationinNewCities(0), m_iMissileRange(0), m_iFlightOperationRange(0), m_iNavalCargoSpace(0), m_iMissileCargoSpace(0)
	, m_iNationalCaptureProbabilityModifier(0), m_iNationalCaptureResistanceModifier(0)
	, m_iStateReligionSpreadProbabilityModifier(0), m_iNonStateReligionSpreadProbabilityModifier(0)
	, m_iFreedomFighterChange(0), m_iLinePriority(0)
	, m_iRevIdxLocal(0), m_iRevIdxNational(0), m_iRevIdxDistanceModifier(0), m_iRevIdxHolyCityGood(0), m_iRevIdxHolyCityBad(0)
	, m_fRevIdxNationalityMod(0.0f), m_fRevIdxBadReligionMod(0.0f), m_fRevIdxGoodReligionMod(0.0f)
	, m_iGreatPeopleUnitType(-1), m_iGoldenAgeonBirthofGreatPeopleType(-1)
	, m_ePromotionLine(NO_PROMOTIONLINE), m_eEraAdvanceFreeSpecialistType(NO_SPECIALIST)
	, m_ePrereqTrait(NO_TRAIT), m_ePrereqOrTrait1(NO_TRAIT), m_ePrereqOrTrait2(NO_TRAIT), m_ePrereqTech(NO_TECH)
	, m_iCapitalXPModifier(0), m_iHolyCityofNonStateReligionXPModifier(0), m_iLargestCityHappiness(0), m_iHappyPerMilitaryUnit(0)
	, m_bCoastalAIInfluence(false), m_bFreeSpecialistperWorldWonder(false), m_bFreeSpecialistperNationalWonder(false), m_bFreeSpecialistperTeamProject(false)
	, m_bImpurePropertyManipulators(false), m_bImpurePromotions(false), m_bBarbarianSelectionOnly(false)
{
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		m_aiCommerceChange[c] = 0; m_aiCommerceModifier[c] = 0;
		m_aiSpecialistExtraCommerce[c] = 0; m_aiGoldenAgeCommerceChange[c] = 0;
		m_aiCapitalCommerceModifier[c] = 0;
	}
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiYieldChange[y] = 0; m_aiYieldModifier[y] = 0; m_aiTradeYieldModifier[y] = 0;
		m_aiExtraYieldThreshold[y] = -1;   // archived default: no threshold authored = -1
		m_aiLessYieldThreshold[y] = 0;
		m_aiSpecialistExtraYield[y] = 0; m_aiGoldenAgeYieldChange[y] = 0;
		m_aiCapitalYieldModifier[y] = 0; m_aiSeaPlotYieldChanges[y] = 0;
	}
}

void CvTraitInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending vector
	// (the IDValueMap setValue + the freePromotions map-of-sets re-apply identically: no clear needed).
	m_aBuildingProductionModifiers.clear(); m_aUnitProductionModifiers.clear();
	m_aSpecialUnitProductionModifiers.clear(); m_aUnitCombatProductionModifiers.clear();
	m_aUnitCombatFreeExperiences.clear(); m_aCivicOptionNoUpkeepTypes.clear(); m_aDisallowedTraitTypes.clear();

	CvInfo::mapFrom(entity);   // core reading + section dispatch (edges/grants/modifiers/policies)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: a trait's <PROPERTY_X>.city.flat deposits in EVERY city while the trait is
	// held -- the player gather walks traits; RELATION_ASSOCIATED fans each source to every owner city (the
	// legacy CITY+ASSOCIATED shape; the crime-band gates + per-pop scalers translate inside the shared walk).
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);

	// --- scalar modifier families (curate_trait.py SCALAR) -- UNCONDITIONED sum at the leaf ---
	m_iHealth                             = traitScalar(o, "health", "empire", NULL, "flat");
	m_iHappiness                          = traitScalar(o, "happiness", "empire", NULL, "flat");
	m_iNonStateReligionHappiness          = traitScalar(o, "happiness", "empire", "nonStateReligion", "flat");
	m_iGlobalPopulationgrowthratepercentage = traitScalar(o, "growth", "empire", NULL, "percent");
	m_iGreatPeopleRateModifier            = traitScalar(o, "greatPeopleRate", "empire", NULL, "percent");
	m_iGreatGeneralRateModifier           = traitScalar(o, "greatGeneralRate", "empire", NULL, "percent");
	m_iDomesticGreatGeneralRateModifier   = traitScalar(o, "greatGeneralRate", "empire", "domestic", "percent");
	m_iFreeSpecialist                     = traitScalar(o, "freeSpecialists", "empire", NULL, "any");
	m_iMilitaryProductionModifier         = traitScalar(o, "buildRate", "empire", "military", "percent");
	m_iMaxGlobalBuildingProductionModifier  = traitScalar(o, "buildRate", "empire", "worldWonder", "percent");
	m_iMaxTeamBuildingProductionModifier    = traitScalar(o, "buildRate", "empire", "teamWonder", "percent");
	m_iMaxPlayerBuildingProductionModifier  = traitScalar(o, "buildRate", "empire", "nationalWonder", "percent");
	m_iDistanceMaintenanceModifier        = traitScalar(o, "maintenance", "empire", "distance", "percent");
	m_iNumCitiesMaintenanceModifier       = traitScalar(o, "maintenance", "empire", "numCities", "percent");
	m_iCorporationMaintenanceModifier     = traitScalar(o, "maintenance", "empire", "corporation", "percent");
	m_iUpkeepModifier                     = traitScalar(o, "upkeep", "empire", "civic", "percent");
	m_iFreeUnitUpkeepCivilian             = traitScalar(o, "upkeep", "empire", "freeCivilian", "flat");
	m_iFreeUnitUpkeepMilitary             = traitScalar(o, "upkeep", "empire", "freeMilitary", "flat");
	m_iFreeUnitUpkeepCivilianPopPercent   = traitScalar(o, "upkeep", "empire", "freeCivilian", "perPopulation");
	m_iFreeUnitUpkeepMilitaryPopPercent   = traitScalar(o, "upkeep", "empire", "freeMilitary", "perPopulation");
	m_iCivilianUnitUpkeepMod              = traitScalar(o, "upkeep", "empire", "unitCivilian", "percent");
	m_iMilitaryUnitUpkeepMod              = traitScalar(o, "upkeep", "empire", "unitMilitary", "percent");
	m_iUnitUpgradePriceModifier           = traitScalar(o, "upkeep", "empire", "upgradePrice", "percent");
	m_iWorkerSpeedModifier                = traitScalar(o, "workRate", "empire", NULL, "percent");
	m_iImprovementUpgradeRateModifier     = traitScalar(o, "improvementUpgradeRate", "empire", NULL, "percent");
	m_iMaxConscript                       = traitScalar(o, "conscript", "empire", NULL, "flat");
	m_iHurryAngerModifier                 = traitScalar(o, "hurry", "empire", "anger", "percent");
	m_iHurryCostModifier                  = traitScalar(o, "hurry", "empire", "cost", "percent");
	m_iTradeRoutes                        = traitScalar(o, "tradeRoutes", "empire", NULL, "flat");
	m_iCoastalTradeRoutes                 = traitScalar(o, "tradeRoutes", "empire", "coastal", "flat");
	m_iMaxTradeRoutesChange               = traitScalar(o, "tradeRoutes", "empire", "max", "flat");
	m_iForeignTradeRouteModifier          = traitScalar(o, "tradeRoutes", "empire", "foreign", "percent");
	m_iGoldenAgeDurationModifier          = traitScalar(o, "goldenAge", "empire", NULL, "percent");
	m_iGlobalAirUnitCapacity              = traitScalar(o, "unitCapability", "empire", "airUnitCapacity", "flat");
	m_iFreeExperience                     = traitScalar(o, "experience", "empire", NULL, "flat");
	m_iLevelExperienceModifier            = traitScalar(o, "experience", "empire", "levelModifier", "percent");
	m_iExpInBorderModifier                = traitScalar(o, "experience", "empire", "inBorder", "percent");
	m_iCityDefenseBonus                   = traitScalar(o, "defense", "empire", "amount", "percent");
	m_iBombardDefense                     = traitScalar(o, "defense", "empire", "bombardDefense", "percent");
	m_iEspionageDefense                   = traitScalar(o, "combat", "empire", "espionageDefense", "percent");
	m_iNationalCaptureProbabilityModifier = traitScalar(o, "combat", "empire", "captureProbability", "percent");
	m_iNationalCaptureResistanceModifier  = traitScalar(o, "combat", "empire", "captureResistance", "percent");
	m_iMissileRange                       = traitScalar(o, "combat", "empire", "missileRange", "flat");
	m_iFlightOperationRange               = traitScalar(o, "combat", "empire", "flightRange", "flat");
	m_iNavalCargoSpace                    = traitScalar(o, "combat", "empire", "navalCargo", "flat");
	m_iMissileCargoSpace                  = traitScalar(o, "combat", "empire", "missileCargo", "flat");
	m_iAttitudeModifier                   = traitScalar(o, "diplomacy", "empire", "attitude", "flat");
	m_iWarWearinessAccumulationModifier   = traitScalar(o, "diplomacy", "empire", "warWeariness", "percent");
	m_iEnemyWarWearinessModifier          = traitScalar(o, "diplomacy", "empire", "enemyWarWeariness", "percent");
	m_iCivicAnarchyTimeModifier           = traitScalar(o, "durations", "empire", "civicAnarchy", "percent");
	m_iReligiousAnarchyTimeModifier       = traitScalar(o, "durations", "empire", "religiousAnarchy", "percent");
	m_iFreedomFighterChange               = traitScalar(o, "revolution", "empire", "freedomFighter", "flat");
	m_iCityStartCulture                   = traitScalar(o, "cityFounding", "empire", "startCulture", "flat");
	m_iBonusPopulationinNewCities         = traitScalar(o, "cityFounding", "empire", "startPopulation", "flat");

	// state-religion grouped family (curate_trait.py STATE_RELIGION -> stateReligion.empire.<member>.<unit>)
	m_iStateReligionHappiness                 = traitScalar(o, "stateReligion", "empire", "happiness", "flat");
	m_iStateReligionGreatPeopleRateModifier   = traitScalar(o, "stateReligion", "empire", "greatPeopleRate", "percent");
	m_iStateReligionUnitProductionModifier    = traitScalar(o, "stateReligion", "empire", "unitProduction", "percent");
	m_iStateReligionBuildingProductionModifier= traitScalar(o, "stateReligion", "empire", "buildingProduction", "percent");
	m_iStateReligionFreeExperience            = traitScalar(o, "stateReligion", "empire", "freeExperience", "flat");
	m_iHolyCityofStateReligionXPModifier      = traitScalar(o, "stateReligion", "empire", "holyCityXP", "percent");
	m_iStateReligionSpreadProbabilityModifier = traitScalar(o, "stateReligion", "empire", "spreadProbability", "percent");
	m_iNonStateReligionSpreadProbabilityModifier = traitScalar(o, "stateReligion", "empire", "nonStateSpreadProbability", "percent");

	// RevIdx* (RevolutionDCM; curate_trait.py SCALAR -> revolution.empire.<member>.<unit>; fRev* are floats verbatim)
	m_iRevIdxLocal            = traitScalar(o, "revolution", "empire", "local", "flat");
	m_iRevIdxNational         = traitScalar(o, "revolution", "empire", "national", "flat");
	m_iRevIdxDistanceModifier = traitScalar(o, "revolution", "empire", "distanceModifier", "percent");
	m_iRevIdxHolyCityGood     = traitScalar(o, "revolution", "empire", "holyCityGood", "flat");
	m_iRevIdxHolyCityBad      = traitScalar(o, "revolution", "empire", "holyCityBad", "flat");
	m_fRevIdxNationalityMod   = (float)traitScalarF(o, "revolution", "empire", "nationalityMod", "percent");
	m_fRevIdxBadReligionMod   = (float)traitScalarF(o, "revolution", "empire", "badReligionMod", "percent");
	m_fRevIdxGoodReligionMod  = (float)traitScalarF(o, "revolution", "empire", "goodReligionMod", "percent");

	// positional yield/commerce arrays (curate_trait.py SPLIT_ARRAY / GROUPED_YIELD_ARRAY)
	const bool bHasExtraYieldThreshold = (jsonChildObj(o, "extraYieldThreshold") != NULL);
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiYieldChange[y]          = traitScalar(o, YIELD_FAM[y], "empire", NULL, "flat");
		m_aiYieldModifier[y]        = traitScalar(o, YIELD_FAM[y], "empire", NULL, "percent");
		m_aiTradeYieldModifier[y]   = traitScalar(o, YIELD_FAM[y], "empire", "tradeRoute", "percent");
		m_aiSpecialistExtraYield[y] = traitScalar(o, YIELD_FAM[y], "empire", "specialist", "perSpecialist");
		m_aiGoldenAgeYieldChange[y] = traitScalar(o, YIELD_FAM[y], "empire", "goldenAge", "flat");
		m_aiExtraYieldThreshold[y]  = bHasExtraYieldThreshold ? traitScalar(o, "extraYieldThreshold", "empire", YIELD_FAM[y], "flat") : -1;
		m_aiLessYieldThreshold[y]   = traitScalar(o, "lessYieldThreshold", "empire", YIELD_FAM[y], "flat");
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		m_aiCommerceChange[c]          = traitScalar(o, COMMERCE_FAM[c], "empire", NULL, "flat");
		m_aiCommerceModifier[c]        = traitScalar(o, COMMERCE_FAM[c], "empire", NULL, "percent");
		m_aiSpecialistExtraCommerce[c] = traitScalar(o, COMMERCE_FAM[c], "empire", "specialist", "perSpecialist");
		m_aiGoldenAgeCommerceChange[c] = traitScalar(o, COMMERCE_FAM[c], "empire", "goldenAge", "flat");
	}

	// great-people rate change + GP unit (curate_trait.py: GreatPeopleUnitType + GreatPeopleRateChange fold into
	// greatPeopleRate.empire.units.{UNIT}.flat when typed, else the untyped greatPeopleRate.empire.flat national pool)
	{
		const picojson::object* fo = jsonChildObj(o, "greatPeopleRate");
		const picojson::object* so = fo ? jsonChildObj(*fo, "empire") : NULL;
		const picojson::object* uo = so ? jsonChildObj(*so, "units") : NULL;
		if (uo != NULL && !uo->empty())
		{
			picojson::object::const_iterator it = uo->begin();
			m_iGreatPeopleUnitType = jsonResolveId(it->first);   // -1 if unresolved
			if (it->second.is<picojson::object>())
			{
				const picojson::object& e = it->second.get<picojson::object>();
				picojson::object::const_iterator fl = e.find("flat");
				if (fl != e.end() && fl->second.is<double>()) m_iGreatPeopleRateChange = (int)fl->second.get<double>();
			}
		}
		else
		{
			m_iGreatPeopleRateChange = traitScalar(o, "greatPeopleRate", "empire", NULL, "flat");
		}
	}

	// target-KEYED families the consumer iterates by list-index -> typed vectors (curate_trait.py KEYED)
	std::vector<std::pair<int, int> > tmp;
	traitCollectKeyed(o, "buildRate", "empire", "buildings", "percent", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { BuildingModifier m; m.eBuilding = (BuildingTypes)tmp[k].first; m.iModifier = tmp[k].second; m_aBuildingProductionModifiers.push_back(m); }
	tmp.clear(); traitCollectKeyed(o, "buildRate", "empire", "units", "percent", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { UnitModifier m; m.eUnit = (UnitTypes)tmp[k].first; m.iModifier = tmp[k].second; m_aUnitProductionModifiers.push_back(m); }
	tmp.clear(); traitCollectKeyed(o, "buildRate", "empire", "specialUnits", "percent", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { SpecialUnitModifier m; m.eSpecialUnit = (SpecialUnitTypes)tmp[k].first; m.iModifier = tmp[k].second; m_aSpecialUnitProductionModifiers.push_back(m); }
	tmp.clear(); traitCollectKeyed(o, "buildRate", "empire", "unitCombats", "percent", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { UnitCombatModifier m; m.eUnitCombat = (UnitCombatTypes)tmp[k].first; m.iModifier = tmp[k].second; m_aUnitCombatProductionModifiers.push_back(m); }
	tmp.clear(); traitCollectKeyed(o, "experience", "empire", "unitCombats", "flat", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { UnitCombatModifier m; m.eUnitCombat = (UnitCombatTypes)tmp[k].first; m.iModifier = tmp[k].second; m_aUnitCombatFreeExperiences.push_back(m); }
	tmp.clear(); traitCollectKeyed(o, "upkeep", "empire", "civicOptions", "enabler", tmp);
	for (size_t k = 0; k < tmp.size(); ++k) { CivicOptionTypeBool c; c.eCivicOption = (CivicOptionTypes)tmp[k].first; c.bBool = (tmp[k].second != 0); m_aCivicOptionNoUpkeepTypes.push_back(c); }

	// keyed IDValueMap families (setValue per entry; skip 0 -- curate_trait.py KEYED)
	traitFillMap(o, "happiness",              "empire", "buildings",       "flat",    m_aBuildingHappinessModifiers);
	traitFillMap(o, "improvementUpgradeRate", "empire", "improvements",    "percent", m_aImprovementUpgradeModifierTypes);
	traitFillMap(o, "workRate",               "empire", "builds",          "percent", m_aBuildWorkerSpeedModifierTypes);
	traitFillMap(o, "experience",             "empire", "domains",         "flat",    m_aDomainFreeExperiences);
	traitFillMap(o, "buildRate",              "empire", "domains",         "percent", m_aDomainProductionModifiers);
	traitFillMap(o, "researchRate",           "empire", "techs",           "percent", m_aTechResearchModifiers);
	traitFillMap(o, "buildRate",              "empire", "specialBuildings","percent", m_aSpecialBuildingProductionModifiers);

	// per-bonus happiness -> IDValueMap<BonusTypes,int> (curate_trait.py BonusHappinessChanges: happiness.empire.flat
	// LIST entries {value, enabled:{type:BONUS_X, scope:empire, min:1}}; the base iHappiness is a bare number, skipped)
	{
		const picojson::value* leaf = traitLeaf(o, "happiness", "empire", NULL, "flat");
		if (leaf && leaf->is<picojson::array>())
		{
			const picojson::array& a = leaf->get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& e = a[i].get<picojson::object>();
				picojson::object::const_iterator en = e.find("enabled");
				if (en == e.end() || !en->second.is<picojson::object>()) continue;
				const picojson::object& eo = en->second.get<picojson::object>();
				picojson::object::const_iterator ty = eo.find("type");
				if (ty == eo.end() || !ty->second.is<std::string>()) continue;
				const std::string& bt = ty->second.get<std::string>();
				if (bt.compare(0, 6, "BONUS_") != 0) continue;
				int bid = jsonResolveId(bt);
				if (bid < 0) continue;
				picojson::object::const_iterator vv = e.find("value");
				int val = (vv != e.end() && vv->second.is<double>()) ? (int)vv->second.get<double>() : 0;
				if (val != 0) m_aBonusHappinessChanges.setValue((BonusTypes)bid, val);
			}
		}
	}

	// conditioned / ranked / unit-qualified leaves (REAL -- the legacy "Capital*"/"SeaPlot*"/"LargestCity"/"HappyPerMil"
	// getters return the raw magnitude; the legacy consumer applies the predicate/ranked gate itself). curate_trait.py
	// SCALAR_COND / SPLIT_ARRAY_COND / SeaPlotYieldChanges fold / iLargestCityHappiness / iHappyPerMilitaryUnit.
	m_iCapitalXPModifier                    = traitCondStr(traitLeaf(o, "experience", "empire", NULL, "percent"), "IS_CAPITAL");
	m_iHolyCityofNonStateReligionXPModifier = traitCondObj(traitLeaf(o, "experience", "empire", NULL, "percent"));
	m_iLargestCityHappiness                 = traitScalar(o, "happiness", "empire", "cities", "flat");   // the bare-number cities.flat (ranked siblings max/orderedBy)
	m_iHappyPerMilitaryUnit                 = traitUnitQual(traitLeaf(o, "happiness", "empire", "cities", "flat"), "IS_MILITARY");
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		m_aiCapitalYieldModifier[y] = traitCondStr(traitLeaf(o, YIELD_FAM[y], "empire", NULL, "percent"), "IS_CAPITAL");
		m_aiSeaPlotYieldChanges[y]  = traitCondStr(traitLeaf(o, YIELD_FAM[y], "empire", "plots", "flat"), "IS_WATER");
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		m_aiCapitalCommerceModifier[c] = traitCondStr(traitLeaf(o, COMMERCE_FAM[c], "empire", NULL, "percent"), "IS_CAPITAL");

	// ai.behaviour.coastalAIInfluence (curate_trait.py bCoastalAIInfluence)
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		if (const picojson::object* be = jsonChildObj(*ai, "behaviour"))
			m_bCoastalAIInfluence = jsonIdBool(*be, "coastalAIInfluence");

	// freeSpecialists.empire.any per-wonder entries -> the legacy per-wonder bool flags (curate_trait.py
	// FREE_SPEC_PER_WONDER: list entries {value:1, per:{type:<WONDER_TOKEN>, scope:city}})
	{
		const picojson::object* fo = jsonChildObj(o, "freeSpecialists");
		const picojson::object* so = fo ? jsonChildObj(*fo, "empire") : NULL;
		if (so != NULL)
		{
			picojson::object::const_iterator an = so->find("any");
			if (an != so->end() && an->second.is<picojson::array>())
			{
				const picojson::array& a = an->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object* per = jsonChildObj(a[i].get<picojson::object>(), "per");
					if (per == NULL) continue;
					picojson::object::const_iterator ty = per->find("type");
					if (ty == per->end() || !ty->second.is<std::string>()) continue;
					const std::string& t = ty->second.get<std::string>();
					if (t == "WORLD_WONDER")         m_bFreeSpecialistperWorldWonder = true;
					else if (t == "NATIONAL_WONDER") m_bFreeSpecialistperNationalWonder = true;
					else if (t == "TEAM_WONDER")     m_bFreeSpecialistperTeamProject = true;
				}
			}
		}
	}

	// excludes (curate_trait.py DisallowedTraitTypes -> top-level `excludes` array of trait type strings)
	{
		picojson::object::const_iterator ex = o.find("excludes");
		if (ex != o.end() && ex->second.is<picojson::array>())
		{
			const picojson::array& a = ex->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0) { DisallowedTraitType d; d.eTrait = (TraitTypes)id; m_aDisallowedTraitTypes.push_back(d); }
				}
		}
	}

	// identity flags + shortDescription + anarchy bounds (curate_trait.py IDENTITY_FLAGS / identity)
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		if (jsonIdBool(*io, "negativeTrait")) negativeTrait = true;
		civilizationTrait             = jsonIdBool(*io, "civilizationTrait");
		m_bImpurePropertyManipulators = jsonIdBool(*io, "impurePropertyManipulators");
		m_bImpurePromotions           = jsonIdBool(*io, "impurePromotions");
		m_bBarbarianSelectionOnly     = jsonIdBool(*io, "barbarianSelectionOnly");
		std::string s;
		if (jsonIdStr(*io, "shortDescription", s)) m_szShortDescription = s;
		m_iMinAnarchy = jsonIdInt(*io, "minAnarchy");                            // default 0
		picojson::object::const_iterator ma = io->find("maxAnarchy");           // default -1 when absent
		if (ma != io->end() && ma->second.is<double>()) m_iMaxAnarchy = (int)ma->second.get<double>();
	}

	// succession (curate_trait.py: PromotionLine -> succession.promotionLine, LinePriority -> succession.priority)
	if (const picojson::object* su = jsonChildObj(o, "succession"))
	{
		m_iLinePriority = jsonIdInt(*su, "priority");
		std::string s;
		if (jsonIdStr(*su, "promotionLine", s)) { int id = jsonResolveId(s); if (id >= 0) m_ePromotionLine = (PromotionLineTypes)id; }
	}

	// grants FKs + freePromotions (curate_trait.py: EraAdvanceFreeSpecialistType / GoldenAgeonBirthofGreatPersonType /
	// FreePromotionUnitCombatTypes -> grants.freePromotions {PROMOTION: [UNITCOMBAT, ...]})
	if (const picojson::object* g = jsonChildObj(o, "grants"))
	{
		std::string s;
		if (jsonIdStr(*g, "eraAdvanceFreeSpecialist", s))      { int id = jsonResolveId(s); if (id >= 0) m_eEraAdvanceFreeSpecialistType = (SpecialistTypes)id; }
		if (jsonIdStr(*g, "goldenAgeOnBirthOfGreatPerson", s)) { m_iGoldenAgeonBirthofGreatPeopleType = jsonResolveId(s); }
		if (const picojson::object* fp = jsonChildObj(*g, "freePromotions"))
			for (picojson::object::const_iterator it = fp->begin(); it != fp->end(); ++it)
			{
				int pid = jsonResolveId(it->first);
				if (pid < 0 || !it->second.is<picojson::array>()) continue;
				const picojson::array& ucs = it->second.get<picojson::array>();
				std::set<int>& ucSet = m_freePromotionUnitCombats[pid];
				for (size_t k = 0; k < ucs.size(); ++k)
					if (ucs[k].is<std::string>()) { int uc = jsonResolveId(ucs[k].get<std::string>()); if (uc >= 0) ucSet.insert(uc); }
			}
	}

	// ai.flavours {FLAVOR:int}
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		jsonReadFlavours(*ai, m_flavours);

	// --- 2D specialist/improvement yield/commerce MATERIALIZATION (the per-index getters are bare map reads;
	// per-call string-address walks are banned from getters). Clear-first: this appends per channel. ---
	m_bAnySpecYield = false; m_bAnySpecCommerce = false;
	for (int j = 0; j < NUM_YIELD_TYPES; ++j)
	{
		m_specYield[j].clear(); m_impYield[j].clear();
		std::vector<std::pair<int, int> > v;
		JsonModScan::collectKeyedSparse<int>(getModifiers(), std::string(YIELD_FAM[j]) + ".empire.specialists.", CASC_UNIT_FLAT, v);
		for (size_t k = 0; k < v.size(); ++k) m_specYield[j][v[k].first] = v[k].second;
		if (!m_specYield[j].empty()) m_bAnySpecYield = true;
		v.clear();
		JsonModScan::collectKeyedSparse<int>(getModifiers(), std::string(YIELD_FAM[j]) + ".empire.improvements.", CASC_UNIT_FLAT, v);
		for (size_t k = 0; k < v.size(); ++k) m_impYield[j][v[k].first] = v[k].second;
	}
	for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
	{
		m_specCommerce[j].clear();
		std::vector<std::pair<int, int> > v;
		JsonModScan::collectKeyedSparse<int>(getModifiers(), std::string(COMMERCE_FAM[j]) + ".empire.specialists.", CASC_UNIT_FLAT, v);
		for (size_t k = 0; k < v.size(); ++k) m_specCommerce[j][v[k].first] = v[k].second;
		if (!m_specCommerce[j].empty()) m_bAnySpecCommerce = true;
	}
}

// --- 2D per-index specialist/improvement yield/commerce -- bare reads of the mapFrom-materialized sparse maps ---
int CvTraitInfo::getSpecialistYieldChange(int i, int j) const
{
	if (j < 0 || j >= NUM_YIELD_TYPES) return 0;
	std::map<int, int>::const_iterator it = m_specYield[j].find(i);
	return it != m_specYield[j].end() ? it->second : 0;
}
int CvTraitInfo::getSpecialistCommerceChange(int i, int j) const
{
	if (j < 0 || j >= NUM_COMMERCE_TYPES) return 0;
	std::map<int, int>::const_iterator it = m_specCommerce[j].find(i);
	return it != m_specCommerce[j].end() ? it->second : 0;
}
int CvTraitInfo::getImprovementYieldChange(int i, int j) const
{
	if (j < 0 || j >= NUM_YIELD_TYPES) return 0;
	std::map<int, int>::const_iterator it = m_impYield[j].find(i);
	return it != m_impYield[j].end() ? it->second : 0;
}
bool CvTraitInfo::isAnySpecialistYieldChanges() const { return m_bAnySpecYield; }
bool CvTraitInfo::isAnySpecialistCommerceChanges() const { return m_bAnySpecCommerce; }

// --- `filtered` getters -- an always-pass view over the member IDValueMap (populated in mapFrom via setValue). No
//     PURE_TRAITS runtime gate (a consumer concern, modifier.md §4), so the predicate simply passes every entry. ---
const IDValueMap<BuildingTypes, int>::filtered CvTraitInfo::getBuildingHappinessModifiersFiltered() const
{ return jsonEmptyFiltered(m_aBuildingHappinessModifiers); }
const IDValueMap<ImprovementTypes, int>::filtered CvTraitInfo::getImprovementUpgradeModifiers() const
{ return jsonEmptyFiltered(m_aImprovementUpgradeModifierTypes); }
const IDValueMap<BuildTypes, int>::filtered CvTraitInfo::getBuildWorkerSpeedModifiers() const
{ return jsonEmptyFiltered(m_aBuildWorkerSpeedModifierTypes); }
const IDValueMap<DomainTypes, int>::filtered CvTraitInfo::getDomainFreeExperience() const
{ return jsonEmptyFiltered(m_aDomainFreeExperiences); }
const IDValueMap<DomainTypes, int>::filtered CvTraitInfo::getDomainProductionModifiers() const
{ return jsonEmptyFiltered(m_aDomainProductionModifiers); }
const IDValueMap<TechTypes, int>::filtered CvTraitInfo::getTechResearchModifiers() const
{ return jsonEmptyFiltered(m_aTechResearchModifiers); }
const IDValueMap<SpecialBuildingTypes, int>::filtered CvTraitInfo::getSpecialBuildingProductionModifiers() const
{ return jsonEmptyFiltered(m_aSpecialBuildingProductionModifiers); }
const IDValueMap<BonusTypes, int>::filtered CvTraitInfo::getBonusHappinessChanges() const
{ return jsonEmptyFiltered(m_aBonusHappinessChanges); }
