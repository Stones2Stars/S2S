//
//	CvBuildingInfo::mapFrom -- common sections (base dispatch fills the composed units) + the building `identity`
//	block: the StoneBase BuildingInfo flags (notConstructible / governmentCenter / forceNoPrereqScaling /
//	specialBuilding) + the shrine/corpHQ/religion FKs + the stateReligionCommerce / commerceDoubleTime maps.
//	SELF-CONTAINED (the engine getReligionType / getGlobal*Commerce reads are RETIRED). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvBuildingInfo.h"
#include "CvJsonParse.h"            // jsonResolveId / jsonCommerceMap / jsonIdFk / jsonIdInt / jsonIdBool / jsonWorldArt
#include "CvCascadePropertyBridge.h" // the JSON->BoolExpr/IntExpr translator (property-audit.md increment 4)
#include "UI/CvArtFileMgr.h"        // ARTFILEMGR.getBuildingArtInfo / getMovieArtInfo -- the art shims (mirrors CvBonusInfo)
#include "Infos/CvArtInfoMovie.h"   // CvArtInfoMovie complete type -- getMovie() calls getPath() (via CvAssetInfoBase)
#include "Infos/CvArtInfoBuilding.h" // CvArtInfoBuilding complete type -- getButton() call needs the full definition
#include "Defines/CvStructs.h"      // the cy* Python tuple structs (TechYieldChange/BuildingCommerceChange/GenericTrippleInt/...)
#include "CvJsonModScan.h"          // the ONE load-time modifier-family scan (mapFrom materialization)

void CvBuildingInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom (incl. reconstructFromComposed below),
	// whose writers ACCUMULATE (push_back / mod_addScalar / mod_addYield / +=) -- fully define every output.
	m_aiMayDamageUnitCombats.clear(); m_aeMapCategoryTypes.clear(); m_enabledCivTypes.clear();
	m_aFreePromoTypes.clear(); m_aiFreeTraitTypes.clear(); m_healUnitCombats.clear(); m_consumptionRelevantBonuses.clear();
	m_piPrereqAndTechs.clear(); m_prereqOrImprovement.clear(); m_prereqOrHeritage.clear();
	m_aePrereqOrBonuses.clear(); m_piPrereqOrVicinityBonuses.clear(); m_aePrereqOrRawVicinityBonuses.clear();
	m_techSpecialistChange.clear();
	m_aPlotYieldChanges.clear(); m_aTerrainYieldChanges.clear(); m_aImprovementYieldChanges.clear();
	m_aGlobalImprovementYieldChanges.clear(); m_aBuildingHappinessChanges.clear(); m_religionChange.clear();
	m_bonusDefenseChanges.clear(); m_unitCombatDefenseAgainst.clear(); m_aUnitCombatFreeExperience.clear();
	m_domainFreeExperience.clear(); m_aUnitCombatExtraStrength.clear(); m_aUnitProductionModifier.clear();
	m_unitCombatProdModifier.clear(); m_domainProductionModifier.clear(); m_aBuildingProductionModifier.clear();
	m_aGlobalBuildingProductionModifier.clear(); m_specialistCount.clear(); m_improvementFreeSpecialists.clear();
	m_freeSpecialistCount.clear(); m_techYieldChanges.clear(); m_techYieldModifiers.clear();
	m_vicinityBonusYieldChanges.clear(); m_bonusYieldChanges.clear(); m_bonusYieldModifier.clear();
	m_aTechHappinessChanges.clear(); m_piBonusHappinessChanges.clear(); m_aTechHealthChanges.clear();
	m_piBonusHealthChanges.clear(); m_bonusProductionModifier.clear(); m_aGlobalBuildingCostModifier.clear();
	m_techCommerceChanges.clear(); m_techCommerceModifiers.clear(); m_bonusCommercePercentChanges.clear();
	m_aGlobalBuildingCommerceChanges.clear();
	m_iNumUnitFullHeal = 0; m_iStateReligionHappiness = 0;
	for (int y = 0; y < NUM_YIELD_TYPES; ++y) { m_aiRiverPlotYieldChange[y] = 0; m_powerYieldModifier[y] = 0; }

	CvInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	// TOP-LEVEL bespoke FK sections (migrated OUT of identity, owner 2026-07-01): `shrine` -> religion FK,
	// `headquarters` -> corporation FK (was identity.shrine / identity.corporationHQ). Feed the shrine/corp-HQ commerce calc.
	if ((it = o.find("shrine")) != o.end() && it->second.is<std::string>())       shrineReligion = jsonResolveId(it->second.get<std::string>());
	if ((it = o.find("headquarters")) != o.end() && it->second.is<std::string>()) corpHQ = jsonResolveId(it->second.get<std::string>());
	// world.art.icon -- the ART_DEF_* tag the ARTFILEMGR lookup keys on (getArtInfo shim, mirrors CvBonusInfo).
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
	// defense.city.counterDamage -- a bespoke object (damage/chance scalars + a units.unitCombats FK list) the
	// flat/percent modifier parser skips. Absence of the `units` selector = "all attackers" (legacy bDamageAllAttackers).
	if (const picojson::object* def = jsonChildObj(o, "defense"))
		if (const picojson::object* dcity = jsonChildObj(*def, "city"))
			if (const picojson::object* cd = jsonChildObj(*dcity, "counterDamage"))
			{
				m_bHasCounterDamage = true;
				m_iDamageToAttacker = jsonIdInt(*cd, "damage");
				m_iDamageAttackerChance = jsonIdInt(*cd, "chance");
				const picojson::object* un = jsonChildObj(*cd, "units");
				m_bDamageAllAttackers = (un == NULL);   // no `units` selector => all attackers
				if (un)
				{
					picojson::object::const_iterator uc = un->find("unitCombats");
					if (uc != un->end() && uc->second.is<picojson::array>())
					{
						const picojson::array& ua = uc->second.get<picojson::array>();
						for (size_t i = 0; i < ua.size(); ++i)
							if (ua[i].is<std::string>()) { const int uid = jsonResolveId(ua[i].get<std::string>()); if (uid >= 0) m_aiMayDamageUnitCombats.push_back(uid); }
					}
				}
			}
	// <yield>.city.plots predicate-gated flats -> the legacy per-plot-TYPE arrays. HAS_RIVER entries feed the
	// RiverPlotYieldChanges array; IS_WATER/HAS_HILLS/HAS_PEAK/IS_LAND(|IS_FLATLANDS) feed getPlotYieldChanges keyed
	// by PlotType. Read raw here (the modifier parser stores conditioned plots-target entries differently). Values are
	// ×1 yields. Non-plot-type predicates (VICINITY/IS_WORKED, nested trees) are the general per-plot yield -- skipped.
	{
		static const char* YFAM[NUM_YIELD_TYPES] = { "food", "production", "commerce" };   // YIELD_* order
		for (int y = 0; y < NUM_YIELD_TYPES; ++y) m_aiRiverPlotYieldChange[y] = 0;
		YieldArray plotAccum[NUM_PLOT_TYPES];
		for (int p = 0; p < NUM_PLOT_TYPES; ++p) plotAccum[p].fill(0);
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			const picojson::object* fo = jsonChildObj(o, YFAM[y]);   if (!fo) continue;
			const picojson::object* cyo = jsonChildObj(*fo, "city"); if (!cyo) continue;
			const picojson::object* plo = jsonChildObj(*cyo, "plots"); if (!plo) continue;
			picojson::object::const_iterator fit = plo->find("flat");
			if (fit == plo->end() || !fit->second.is<picojson::array>()) continue;
			const picojson::array& fa = fit->second.get<picojson::array>();
			for (size_t i = 0; i < fa.size(); ++i)
			{
				if (!fa[i].is<picojson::object>()) continue;
				const picojson::object& e = fa[i].get<picojson::object>();
				picojson::object::const_iterator ve = e.find("value");
				if (ve == e.end() || !ve->second.is<double>()) continue;
				const int val = (int)ve->second.get<double>();
				picojson::object::const_iterator en = e.find("enabled");
				if (en == e.end() || !en->second.is<std::string>()) continue;   // only bare-string plot predicates
				const std::string& pred = en->second.get<std::string>();
				if (pred == "HAS_RIVER") { m_aiRiverPlotYieldChange[y] += val; continue; }
				int pt = -1;
				if      (pred == "IS_WATER")  pt = PLOT_OCEAN;
				else if (pred == "HAS_HILLS") pt = PLOT_HILLS;
				else if (pred == "HAS_PEAK")  pt = PLOT_PEAK;
				else if (pred == "IS_LAND" || pred == "IS_FLATLANDS") pt = PLOT_LAND;
				if (pt >= 0) plotAccum[pt][y] += val;
			}
		}
		for (int p = 0; p < NUM_PLOT_TYPES; ++p)
		{
			bool bAny = false;
			for (int y = 0; y < NUM_YIELD_TYPES && !bAny; ++y) if (plotAccum[p][y] != 0) bAny = true;
			if (bAny) m_aPlotYieldChanges.addArrayValue((PlotTypes)p, plotAccum[p]);
		}
	}
	// enables.votes may carry the non-INFOTYPE marker "FORCE_TEAM_ELIGIBLE" (legacy bForceTeamVoteEligible); the edge
	// parser drops it as unresolvable, so read the marker raw here.
	if (const picojson::object* en = jsonChildObj(o, "enables"))
	{
		picojson::object::const_iterator vt = en->find("votes");
		if (vt != en->end() && vt->second.is<picojson::array>())
		{
			const picojson::array& va = vt->second.get<picojson::array>();
			for (size_t i = 0; i < va.size(); ++i)
				if (va[i].is<std::string>() && va[i].get<std::string>() == "FORCE_TEAM_ELIGIBLE") { m_bForceTeamVoteEligible = true; break; }
		}
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
	conquestProbability = jsonIdInt(io, "conquestProbability", 50);  // legacy load default 50 (archive .add) -- 0 razes on every conquest
	maxPlayerInstancesExtra = jsonIdInt(io, "maxPlayerInstancesExtra");
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
	// identity.mapCategories -- the MAPCATEGORY_* FK list (mirrors CvBonusInfo).
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

	// --- REAL vectors backed by the composed grants/edges units (populated by CvInfo::mapFrom, called above) ---
	// grants.freePromotions -> getFreePromoTypes (a plain PROMOTION_* FK list; no per-entry condition curated -> NULL).
	if (const std::vector<int>* fp = getGrants()->list("freePromotions"))
		for (size_t i = 0; i < fp->size(); ++i)
		{
			FreePromoTypes f = { (PromotionTypes)(*fp)[i], NULL };
			m_aFreePromoTypes.push_back(f);
		}
	// enables.traits -> getFreeTraitTypes (TraitTypes FK ids; whole civ-trait conferred while active).
	if (const std::vector<int>* ft = getEdges()->find(EDGEF_ENABLES, EDGEB_TRAITS))
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

	// ===== the MATERIALIZATION pass: every legacy scalar / positional getter value is scanned ONCE here
	// (JsonModScan over the composed m_modifiers; same address table the getters carried); the getters are bare
	// member reads -- per-call string-address walks are banned from getters. =====
	{
		const CvJsonModifiers* mods = getModifiers();
		m_iHappinessPercentPerPopulation = JsonModScan::sum(mods, "happiness.city", CASC_UNIT_PER_POPULATION);
		m_iHealthPercentPerPopulation    = JsonModScan::sum(mods, "health.city",    CASC_UNIT_PER_POPULATION);
		m_iHappiness       = JsonModScan::sum(mods, "happiness.city", CASC_UNIT_FLAT);
		m_iAreaHappiness   = JsonModScan::sum(mods, "happiness.area", CASC_UNIT_FLAT);
		m_iGlobalHappiness = JsonModScan::sum(mods, "happiness.empire", CASC_UNIT_FLAT);
		m_iHealth          = JsonModScan::sum(mods, "health.city", CASC_UNIT_FLAT);
		m_iAreaHealth      = JsonModScan::sum(mods, "health.area", CASC_UNIT_FLAT);
		m_iGlobalHealth    = JsonModScan::sum(mods, "health.empire", CASC_UNIT_FLAT);
		static const char* MZ_YIELD[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
		static const char* MZ_COMM[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			m_aiYieldChange[y]             = JsonModScan::sum(mods, std::string(MZ_YIELD[y]) + ".city", CASC_UNIT_FLAT);
			m_aiYieldModifier[y]           = JsonModScan::sum(mods, std::string(MZ_YIELD[y]) + ".city", CASC_UNIT_PERCENT);
			m_aiAreaYieldModifier[y]       = JsonModScan::sum(mods, std::string(MZ_YIELD[y]) + ".area", CASC_UNIT_PERCENT);
			m_aiGlobalYieldModifier[y]     = JsonModScan::sum(mods, std::string(MZ_YIELD[y]) + ".empire", CASC_UNIT_PERCENT);
			m_aiGlobalSeaPlotYieldChange[y] = JsonModScan::sumAll(mods, std::string(MZ_YIELD[y]) + ".empire.plots", CASC_UNIT_FLAT);
		}
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			m_aiCommerceChange[c]          = JsonModScan::sum(mods, std::string(MZ_COMM[c]) + ".city", CASC_UNIT_FLAT);
			m_aiCommerceModifier[c]        = JsonModScan::sum(mods, std::string(MZ_COMM[c]) + ".city", CASC_UNIT_PERCENT);
			m_aiGlobalCommerceModifier[c]  = JsonModScan::sum(mods, std::string(MZ_COMM[c]) + ".empire", CASC_UNIT_PERCENT);
			m_aiSpecialistExtraCommerce[c] = JsonModScan::sum(mods, std::string(MZ_COMM[c]) + ".empire.specialist", CASC_UNIT_PER_SPECIALIST);
			m_aiCommerceHappiness[c]       = JsonModScan::sum(mods, std::string("commerceHappiness.city.") + MZ_COMM[c], CASC_UNIT_FLAT);
			std::map<std::string, int>::const_iterator dt = commerceDoubleTime.find(MZ_COMM[c]);
			m_aiCommerceChangeDoubleTime[c] = dt != commerceDoubleTime.end() ? dt->second : 0;
			std::map<std::string, int>::const_iterator sr = stateReligionCommerce.find(MZ_COMM[c]);
			m_aiStateReligionCommerce[c]    = sr != stateReligionCommerce.end() ? sr->second : 0;
		}
		m_iEnemyWarWearinessModifier = JsonModScan::sum(mods, "warWeariness.city.enemy", CASC_UNIT_PERCENT);
		m_iOccupationTimeModifier    = JsonModScan::sum(mods, "occupationTime.city", CASC_UNIT_PERCENT);
		m_iHealRateChange            = JsonModScan::sum(mods, "healing.city", CASC_UNIT_FLAT);
		m_iFoodKept                  = JsonModScan::sum(mods, "foodKept.city", CASC_UNIT_PERCENT);
		m_iGreatPeopleRateChange     = JsonModScan::sum(mods, "greatPeopleRate.city", CASC_UNIT_FLAT);
		m_iGreatPeopleRateModifier   = JsonModScan::sum(mods, "greatPeopleRate.city", CASC_UNIT_PERCENT);
		m_iGlobalGreatPeopleRateModifier = JsonModScan::sum(mods, "greatPeopleRate.empire", CASC_UNIT_PERCENT);
		m_iGreatGeneralRateModifier  = JsonModScan::sum(mods, "greatGeneralRate.city", CASC_UNIT_PERCENT);
		m_iDomesticGreatGeneralRateModifier = JsonModScan::sum(mods, "greatGeneralRate.city.domestic", CASC_UNIT_PERCENT);
		m_iMaintenanceModifier       = JsonModScan::sum(mods, "maintenance.city", CASC_UNIT_PERCENT);
		m_iGlobalMaintenanceModifier = JsonModScan::sum(mods, "maintenance.empire", CASC_UNIT_PERCENT);
		m_iAreaMaintenanceModifier   = JsonModScan::sum(mods, "maintenance.area", CASC_UNIT_PERCENT);
		m_iOtherAreaMaintenanceModifier = JsonModScan::sum(mods, "maintenance.area.otherArea", CASC_UNIT_PERCENT);
		m_iDistanceMaintenanceModifier  = JsonModScan::sum(mods, "maintenance.empire.distance", CASC_UNIT_PERCENT);
		m_iNumCitiesMaintenanceModifier = JsonModScan::sum(mods, "maintenance.empire.numCities", CASC_UNIT_PERCENT);
		m_iCoastalDistanceMaintenanceModifier = JsonModScan::sum(mods, "maintenance.empire.coastalDistance", CASC_UNIT_PERCENT);
		m_iConnectedCityMaintenanceModifier   = JsonModScan::sum(mods, "maintenance.empire.connectedCity", CASC_UNIT_PERCENT);
		m_iInflationModifier         = JsonModScan::sum(mods, "inflation.empire", CASC_UNIT_PERCENT);
		m_iWarWearinessModifier      = JsonModScan::sum(mods, "warWeariness.city", CASC_UNIT_PERCENT);
		m_iGlobalWarWearinessModifier = JsonModScan::sum(mods, "warWeariness.empire", CASC_UNIT_PERCENT);
		m_iHurryCostModifier         = JsonModScan::sum(mods, "hurryCost.city", CASC_UNIT_PERCENT);
		m_iGlobalHurryModifier       = JsonModScan::sum(mods, "hurryCost.empire", CASC_UNIT_PERCENT);
		m_iHurryAngerModifier        = JsonModScan::sum(mods, "hurryAnger.city", CASC_UNIT_PERCENT);
		m_iMilitaryProductionModifier = JsonModScan::sum(mods, "buildRate.city.military", CASC_UNIT_PERCENT);
		m_iSpaceProductionModifier   = JsonModScan::sum(mods, "buildRate.city.space", CASC_UNIT_PERCENT);
		m_iGlobalSpaceProductionModifier = JsonModScan::sum(mods, "buildRate.empire.space", CASC_UNIT_PERCENT);
		m_iWorkerSpeedModifier       = JsonModScan::sum(mods, "workRate.empire", CASC_UNIT_PERCENT);
		m_iTradeRoutes               = JsonModScan::sum(mods, "tradeRoutes.city", CASC_UNIT_FLAT);
		m_iCoastalTradeRoutes        = JsonModScan::sum(mods, "tradeRoutes.empire.coastal", CASC_UNIT_FLAT);
		m_iGlobalTradeRoutes         = JsonModScan::sum(mods, "tradeRoutes.empire", CASC_UNIT_FLAT);
		m_iWorldTradeRoutes          = JsonModScan::sum(mods, "tradeRoutes.world", CASC_UNIT_FLAT);
		m_iTradeRouteModifier        = JsonModScan::sum(mods, "tradeRoutes.city.modifier", CASC_UNIT_PERCENT);
		m_iForeignTradeRouteModifier = JsonModScan::sum(mods, "tradeRoutes.city.foreignModifier", CASC_UNIT_PERCENT);
		m_iFreeExperience            = JsonModScan::sum(mods, "experience.city", CASC_UNIT_FLAT);
		m_iGlobalFreeExperience      = JsonModScan::sum(mods, "experience.empire", CASC_UNIT_FLAT);
		m_iFreeSpecialist            = JsonModScan::sum(mods, "freeSpecialists.city.any", CASC_UNIT_COUNT);
		m_iAreaFreeSpecialist        = JsonModScan::sum(mods, "freeSpecialists.area.any", CASC_UNIT_COUNT);
		m_iGlobalFreeSpecialist      = JsonModScan::sum(mods, "freeSpecialists.empire.any", CASC_UNIT_COUNT);
		m_iAnarchyModifier           = JsonModScan::sum(mods, "anarchy.city", CASC_UNIT_PERCENT);
		m_iGoldenAgeModifier         = JsonModScan::sum(mods, "goldenAge.empire", CASC_UNIT_PERCENT);
		m_iPopulationgrowthratepercentage       = JsonModScan::sum(mods, "populationGrowthRate.city", CASC_UNIT_PERCENT);
		m_iGlobalPopulationgrowthratepercentage = JsonModScan::sum(mods, "populationGrowthRate.empire", CASC_UNIT_PERCENT);
		m_iRevIdxLocal               = JsonModScan::sum(mods, "revolution.city", CASC_UNIT_FLAT);
		m_iRevIdxNational            = JsonModScan::sum(mods, "revolution.empire", CASC_UNIT_FLAT);
		m_iRevIdxDistanceModifier    = JsonModScan::sum(mods, "revolution.city.distanceModifier", CASC_UNIT_PERCENT);
		m_iInsidiousness             = JsonModScan::sum(mods, "copsAndRobbers.city.insidiousness", CASC_UNIT_FLAT);
		m_iInvestigation             = JsonModScan::sum(mods, "copsAndRobbers.city.investigation", CASC_UNIT_FLAT);
		m_iEspionageDefenseModifier  = JsonModScan::sum(mods, "espionageDefense.city", CASC_UNIT_FLAT);
		m_iUnitUpgradePriceModifier  = JsonModScan::sum(mods, "unitUpgradePrice.empire", CASC_UNIT_PERCENT);
		m_iDefenseModifier           = JsonModScan::sum(mods, "defense.city.amount", CASC_UNIT_PERCENT);
		m_iBombardDefenseModifier    = JsonModScan::sum(mods, "defense.city.bombardDefense", CASC_UNIT_PERCENT);
		m_iAllCityDefenseModifier    = JsonModScan::sum(mods, "defense.empire.amount", CASC_UNIT_PERCENT);
		m_iNukeModifier              = JsonModScan::sum(mods, "defense.city.nukeDefense", CASC_UNIT_PERCENT);
		m_iAirModifier               = JsonModScan::sum(mods, "defense.city.airDefense", CASC_UNIT_PERCENT);
		m_iMinDefense                = JsonModScan::sum(mods, "defense.city.min", CASC_UNIT_FLAT);
		m_iNoEntryDefenseLevel       = JsonModScan::sum(mods, "defense.city.noEntryLevel", CASC_UNIT_FLAT);
		m_iLocalDynamicDefense       = JsonModScan::sum(mods, "defense.city.dynamicDefense", CASC_UNIT_FLAT);
		m_iRiverDefensePenalty       = JsonModScan::sum(mods, "defense.city.riverDefensePenalty", CASC_UNIT_FLAT);
		m_iBuildingDefenseRecoverySpeedModifier = JsonModScan::sum(mods, "defense.city.buildingDefenseRecovery", CASC_UNIT_PERCENT);
		m_iCityDefenseRecoverySpeedModifier     = JsonModScan::sum(mods, "defense.city.cityDefenseRecovery", CASC_UNIT_PERCENT);
		m_iAdjacentDamagePercent     = JsonModScan::sum(mods, "defense.city.adjacentDamage", CASC_UNIT_PERCENT);
		m_iNationalCaptureProbabilityModifier = JsonModScan::sum(mods, "cityCapture.empire.probability", CASC_UNIT_PERCENT);
		m_iNationalCaptureResistanceModifier  = JsonModScan::sum(mods, "cityCapture.empire.resistance", CASC_UNIT_PERCENT);
		m_iLocalCaptureProbabilityModifier    = JsonModScan::sum(mods, "cityCapture.city.probability", CASC_UNIT_PERCENT);
		m_iLocalCaptureResistanceModifier     = JsonModScan::sum(mods, "cityCapture.city.resistance", CASC_UNIT_PERCENT);
		m_bGrantsGoldenAge           = grantFlag("goldenAge");
	}
}

// ===================== #430 mirrored getters -- bare reads of the mapFrom-materialized members =====================
//
// The curator (curate_building.py) collapses a building's own unconditioned SCALAR_FAMILIES scalar and any
// tech/bonus-gated COND_KEYED addend (or per-scaled deposit) onto the SAME <family>.<scope> modifier address; the
// mapFrom MATERIALIZATION pass (JsonModScan) recovers each legacy field by condition shape ONCE at load. Getters
// never walk string addresses.

int CvBuildingInfo::getHappiness() const       { return m_iHappiness; }
int CvBuildingInfo::getAreaHappiness() const   { return m_iAreaHappiness; }
int CvBuildingInfo::getGlobalHappiness() const { return m_iGlobalHappiness; }
int CvBuildingInfo::getHealth() const          { return m_iHealth; }
int CvBuildingInfo::getAreaHealth() const      { return m_iAreaHealth; }
int CvBuildingInfo::getGlobalHealth() const    { return m_iGlobalHealth; }

int CvBuildingInfo::getYieldChange(int i) const             { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
int CvBuildingInfo::getYieldModifier(int i) const           { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldModifier[i] : 0; }
int CvBuildingInfo::getAreaYieldModifier(int i) const       { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiAreaYieldModifier[i] : 0; }
int CvBuildingInfo::getGlobalYieldModifier(int i) const     { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiGlobalYieldModifier[i] : 0; }
int CvBuildingInfo::getGlobalSeaPlotYieldChange(int i) const{ return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiGlobalSeaPlotYieldChange[i] : 0; }

int CvBuildingInfo::getCommerceChange(int i) const          { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceChange[i] : 0; }
int CvBuildingInfo::getCommerceModifier(int i) const        { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceModifier[i] : 0; }
int CvBuildingInfo::getGlobalCommerceModifier(int i) const  { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiGlobalCommerceModifier[i] : 0; }
int CvBuildingInfo::getSpecialistExtraCommerce(int i) const { return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiSpecialistExtraCommerce[i] : 0; }

int CvBuildingInfo::getEnemyWarWearinessModifier() const { return m_iEnemyWarWearinessModifier; }
int CvBuildingInfo::getOccupationTimeModifier() const    { return m_iOccupationTimeModifier; }

int CvBuildingInfo::getHealRateChange() const               { return m_iHealRateChange; }
int CvBuildingInfo::getFoodKept() const                     { return m_iFoodKept; }
int CvBuildingInfo::getGreatPeopleRateChange() const        { return m_iGreatPeopleRateChange; }
int CvBuildingInfo::getGreatPeopleRateModifier() const      { return m_iGreatPeopleRateModifier; }
int CvBuildingInfo::getGlobalGreatPeopleRateModifier() const{ return m_iGlobalGreatPeopleRateModifier; }
int CvBuildingInfo::getGreatGeneralRateModifier() const     { return m_iGreatGeneralRateModifier; }
int CvBuildingInfo::getDomesticGreatGeneralRateModifier() const { return m_iDomesticGreatGeneralRateModifier; }
int CvBuildingInfo::getMaintenanceModifier() const          { return m_iMaintenanceModifier; }
int CvBuildingInfo::getGlobalMaintenanceModifier() const    { return m_iGlobalMaintenanceModifier; }
int CvBuildingInfo::getAreaMaintenanceModifier() const      { return m_iAreaMaintenanceModifier; }
int CvBuildingInfo::getOtherAreaMaintenanceModifier() const { return m_iOtherAreaMaintenanceModifier; }
int CvBuildingInfo::getDistanceMaintenanceModifier() const  { return m_iDistanceMaintenanceModifier; }
int CvBuildingInfo::getNumCitiesMaintenanceModifier() const { return m_iNumCitiesMaintenanceModifier; }
int CvBuildingInfo::getCoastalDistanceMaintenanceModifier() const { return m_iCoastalDistanceMaintenanceModifier; }
int CvBuildingInfo::getConnectedCityMaintenanceModifier() const { return m_iConnectedCityMaintenanceModifier; }
int CvBuildingInfo::getInflationModifier() const            { return m_iInflationModifier; }
int CvBuildingInfo::getWarWearinessModifier() const         { return m_iWarWearinessModifier; }
int CvBuildingInfo::getGlobalWarWearinessModifier() const   { return m_iGlobalWarWearinessModifier; }
int CvBuildingInfo::getHurryCostModifier() const            { return m_iHurryCostModifier; }
int CvBuildingInfo::getGlobalHurryModifier() const          { return m_iGlobalHurryModifier; }
int CvBuildingInfo::getHurryAngerModifier() const           { return m_iHurryAngerModifier; }
int CvBuildingInfo::getMilitaryProductionModifier() const   { return m_iMilitaryProductionModifier; }
int CvBuildingInfo::getSpaceProductionModifier() const      { return m_iSpaceProductionModifier; }
int CvBuildingInfo::getGlobalSpaceProductionModifier() const{ return m_iGlobalSpaceProductionModifier; }
int CvBuildingInfo::getWorkerSpeedModifier() const          { return m_iWorkerSpeedModifier; }
int CvBuildingInfo::getTradeRoutes() const                  { return m_iTradeRoutes; }
int CvBuildingInfo::getCoastalTradeRoutes() const           { return m_iCoastalTradeRoutes; }
int CvBuildingInfo::getGlobalTradeRoutes() const            { return m_iGlobalTradeRoutes; }
int CvBuildingInfo::getWorldTradeRoutes() const             { return m_iWorldTradeRoutes; }
int CvBuildingInfo::getTradeRouteModifier() const           { return m_iTradeRouteModifier; }
int CvBuildingInfo::getForeignTradeRouteModifier() const    { return m_iForeignTradeRouteModifier; }
int CvBuildingInfo::getFreeExperience() const               { return m_iFreeExperience; }
int CvBuildingInfo::getGlobalFreeExperience() const         { return m_iGlobalFreeExperience; }
int CvBuildingInfo::getFreeSpecialist() const               { return m_iFreeSpecialist; }
int CvBuildingInfo::getAreaFreeSpecialist() const           { return m_iAreaFreeSpecialist; }
int CvBuildingInfo::getGlobalFreeSpecialist() const         { return m_iGlobalFreeSpecialist; }
int CvBuildingInfo::getAnarchyModifier() const              { return m_iAnarchyModifier; }
int CvBuildingInfo::getGoldenAgeModifier() const            { return m_iGoldenAgeModifier; }
int CvBuildingInfo::getPopulationgrowthratepercentage() const       { return m_iPopulationgrowthratepercentage; }
int CvBuildingInfo::getGlobalPopulationgrowthratepercentage() const { return m_iGlobalPopulationgrowthratepercentage; }
int CvBuildingInfo::getRevIdxLocal() const                  { return m_iRevIdxLocal; }
int CvBuildingInfo::getRevIdxNational() const               { return m_iRevIdxNational; }
int CvBuildingInfo::getRevIdxDistanceModifier() const       { return m_iRevIdxDistanceModifier; }
int CvBuildingInfo::getInsidiousness() const                { return m_iInsidiousness; }
int CvBuildingInfo::getInvestigation() const                { return m_iInvestigation; }
int CvBuildingInfo::getEspionageDefenseModifier() const     { return m_iEspionageDefenseModifier; }
int CvBuildingInfo::getUnitUpgradePriceModifier() const     { return m_iUnitUpgradePriceModifier; }
int CvBuildingInfo::getDefenseModifier() const              { return m_iDefenseModifier; }
int CvBuildingInfo::getBombardDefenseModifier() const       { return m_iBombardDefenseModifier; }
int CvBuildingInfo::getAllCityDefenseModifier() const       { return m_iAllCityDefenseModifier; }
int CvBuildingInfo::getNukeModifier() const                 { return m_iNukeModifier; }
int CvBuildingInfo::getAirModifier() const                  { return m_iAirModifier; }
// GAMEOPTION_COMBAT_REALISTIC_SIEGE-gated (archive mirror -- SourceArchive/Infos/CvBuildingInfo.cpp:695/:880)
int CvBuildingInfo::getMinDefense() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE) ? m_iMinDefense : 0; }
int CvBuildingInfo::getNoEntryDefenseLevel() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE) ? m_iNoEntryDefenseLevel : 0; }
int CvBuildingInfo::getLocalDynamicDefense() const          { return m_iLocalDynamicDefense; }
int CvBuildingInfo::getRiverDefensePenalty() const          { return m_iRiverDefensePenalty; }
int CvBuildingInfo::getBuildingDefenseRecoverySpeedModifier() const { return m_iBuildingDefenseRecoverySpeedModifier; }
int CvBuildingInfo::getCityDefenseRecoverySpeedModifier() const     { return m_iCityDefenseRecoverySpeedModifier; }
int CvBuildingInfo::getDamageAttackerChance() const         { return m_iDamageAttackerChance; }   // defense.city.counterDamage.chance (bespoke object, parsed in mapFrom)
int CvBuildingInfo::getDamageToAttacker() const             { return m_iDamageToAttacker; }        // defense.city.counterDamage.damage
int CvBuildingInfo::getAdjacentDamagePercent() const        { return m_iAdjacentDamagePercent; }
int CvBuildingInfo::getNationalCaptureProbabilityModifier() const { return m_iNationalCaptureProbabilityModifier; }
int CvBuildingInfo::getNationalCaptureResistanceModifier() const  { return m_iNationalCaptureResistanceModifier; }
int CvBuildingInfo::getLocalCaptureProbabilityModifier() const    { return m_iLocalCaptureProbabilityModifier; }
int CvBuildingInfo::getLocalCaptureResistanceModifier() const     { return m_iLocalCaptureResistanceModifier; }
// commerceHappiness.city.<commerce>.flat -- happiness gained per unit of each commerce produced (grouped family).
int CvBuildingInfo::getCommerceHappiness(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiCommerceHappiness[i] : 0; }

// commerce double-time / state-religion commerce -- materialized positional arrays (REAL data).
// CULTURE branch mirrors the archive's GAMEOPTION_CULTURE_EQUILIBRIUM default (SourceArchive :238): an
// UNAUTHORED double-time block reads 1000 for culture under the option (NULL-array legacy semantics), so every
// building's culture halves at the equilibrium pace; an authored block keeps its values.
int CvBuildingInfo::getCommerceChangeDoubleTime(int i) const
{
	if (i < 0 || i >= NUM_COMMERCE_TYPES) return 0;
	if (i == COMMERCE_CULTURE && commerceDoubleTime.empty() && GC.getGame().isOption(GAMEOPTION_CULTURE_EQUILIBRIUM))
		return 1000;
	return m_aiCommerceChangeDoubleTime[i];
}
int CvBuildingInfo::getStateReligionCommerce(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiStateReligionCommerce[i] : 0; }

// commerce sliders this building unlocks (`capabilities` block -- canSet{Science|Culture|Espionage}Rate; gold has no
// slider). Mirror of CvTechInfo::isCommerceFlexible (capabilities.md key mapping) -- REAL data, O(1) id bit tests.
bool CvBuildingInfo::isCommerceFlexible(int i) const
{
	static int s_r = -1, s_c = -1, s_e = -1;
	return (i == COMMERCE_RESEARCH  && m_capabilities.hasKey(s_r, CLSD_CAPABILITY, "canSetScienceRate"))
	    || (i == COMMERCE_CULTURE   && m_capabilities.hasKey(s_c, CLSD_CAPABILITY, "canSetCultureRate"))
	    || (i == COMMERCE_ESPIONAGE && m_capabilities.hasKey(s_e, CLSD_CAPABILITY, "canSetEspionageRate"));
}

// FoundsCorporation -> the building's `enables.corporations` edge (curate_building.py, owner 2026-07-01) -- REAL data.
int CvBuildingInfo::getFoundsCorporation() const
{ const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_CORPORATIONS); return (v != NULL && !v->empty()) ? (*v)[0] : -1; }

// REAL: grants.repeatable[] unitCombat heal (getHealUnitCombatType) + identity.enabledCivilizations
// (getEnabledCivilizationType). getBonusAidModifier / getAidRateChange stay zero-filled statics (CURATOR-GAP: both
// AidRateChanges + BonusAidModifiers are DROPPED as dead by curate_building.py, so getNum*() returns 0 -> unreached).
const HealUnitCombat& CvBuildingInfo::getHealUnitCombatType(int iIndex) const
{
	if (iIndex >= 0 && iIndex < (int)m_healUnitCombats.size()) return m_healUnitCombats[iIndex];
	static const HealUnitCombat s = { (UnitCombatTypes)-1, 0, 0 }; return s;
}
const BonusAidModifiers& CvBuildingInfo::getBonusAidModifier(int /*iIndex*/) const
{ static const BonusAidModifiers s = { (BonusTypes)-1, (PropertyTypes)-1, 0 }; return s; }
const AidRateChanges& CvBuildingInfo::getAidRateChange(int /*iIndex*/) const
{ static const AidRateChanges s = { (PropertyTypes)-1, 0 }; return s; }
const EnabledCivilizations& CvBuildingInfo::getEnabledCivilizationType(int iIndex) const
{
	if (iIndex >= 0 && iIndex < (int)m_enabledCivTypes.size()) return m_enabledCivTypes[iIndex];
	static const EnabledCivilizations s = { (CivilizationTypes)-1 }; return s;
}

// --- Python-binding list wrappers (CyInfoInterface1 .def-binds these). The backing IDValueMaps ARE populated by
// reconstructFromComposed, and IDValueMap is foreach_-iterable with value_type == the archived pair typedef
// (TechArray = pair<TechTypes,YieldArray>, etc.), so these mirror the archived CvBuildingInfo bodies verbatim. ---
const python::list CvBuildingInfo::cyGetGlobalBuildingCommerceChanges() const
{
	python::list pyList = python::list();
	foreach_(const BuildingCommerce& pair, m_aGlobalBuildingCommerceChanges)
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(BuildingCommerceChange(pair.first, (CommerceTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetTechYieldChanges100() const
{
	python::list pyList = python::list();
	foreach_(const TechArray& pair, m_techYieldChanges)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(TechYieldChange(pair.first, (YieldTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetTechYieldModifiers() const
{
	python::list pyList = python::list();
	foreach_(const TechArray& pair, m_techYieldModifiers)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(TechYieldChange(pair.first, (YieldTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetTechCommerceChanges100() const
{
	python::list pyList = python::list();
	foreach_(const TechCommerceArray& pair, m_techCommerceChanges)
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(TechCommerceChange(pair.first, (CommerceTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetTechCommerceModifiers() const
{
	python::list pyList = python::list();
	foreach_(const TechCommerceArray& pair, m_techCommerceModifiers)
		for (int i = 0; i < NUM_COMMERCE_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(TechCommerceChange(pair.first, (CommerceTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetTerrainYieldChanges() const
{
	python::list pyList = python::list();
	foreach_(const TerrainArray& pair, m_aTerrainYieldChanges)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(TerrainYieldChange(pair.first, (YieldTypes)i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetPlotYieldChanges() const   // empty until getPlotYieldChanges() populates m_aPlotYieldChanges (the plot-yields fix)
{
	python::list pyList = python::list();
	foreach_(const PlotArray& pair, m_aPlotYieldChanges)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(GenericTrippleInt((int)pair.first, i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetImprovementYieldChanges() const
{
	python::list pyList = python::list();
	foreach_(const ImprovementArray& pair, m_aImprovementYieldChanges)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(GenericTrippleInt((int)pair.first, i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetGlobalImprovementYieldChanges() const
{
	python::list pyList = python::list();
	foreach_(const ImprovementArray& pair, m_aGlobalImprovementYieldChanges)
		for (int i = 0; i < NUM_YIELD_TYPES; i++)
			if (pair.second[i] != 0) pyList.append(GenericTrippleInt((int)pair.first, i, pair.second[i]));
	return pyList;
}
const python::list CvBuildingInfo::cyGetFreePromoTypes() const
{
	python::list pyList = python::list();
	foreach_(const FreePromoTypes& pChange, m_aFreePromoTypes)
		pyList.append(pChange);
	return pyList;
}

// obsoletedBy = the obsoleting tech (ObsoleteTech) + the superseding building (ObsoletesToBuilding), authored
// directly off THIS building's own fields (curate_building.py, "no store inversion"), composed into m_edges by
// the base dispatch. NB a RELIC-SHELL supersession (a non-constructible ObsoletesToBuilding target -- the 6 wonder
// relics) is authored to `whenObsolete` INSTEAD of `obsoletedBy.buildings`, so getObsoletesToBuilding() faithfully
// returns NO_BUILDING for those; the reduced-output tree lives on getWhenObsolete().
TechTypes CvBuildingInfo::getObsoleteTech() const
{ const std::vector<int>* v = edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS); return (TechTypes)((v != NULL && !v->empty()) ? (*v)[0] : NO_TECH); }
BuildingTypes CvBuildingInfo::getObsoletesToBuilding() const
{ const std::vector<int>* v = edge(EDGEF_OBSOLETED_BY, EDGEB_BUILDINGS); return (BuildingTypes)((v != NULL && !v->empty()) ? (*v)[0] : NO_BUILDING); }

// EXE-bound art surface: ARTFILEMGR keyed by the art-define tag (mirrors SourceArchive/Infos/CvBonusInfo.cpp's
// shim pattern, re-based onto the JSON-mapped m_szArtDefineTag).
const CvArtInfoBuilding* CvBuildingInfo::getArtInfo() const
{
	return ARTFILEMGR.getBuildingArtInfo(getArtDefineTag());
}

const char* CvBuildingInfo::getButton() const   // art-define button (mirrors archived CvBuildingInfo::getButton)
{
	const CvArtInfoBuilding* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}

// Wonder-movie art surface (mirrors the archived CvBuildingInfo::getMovieInfo/getMovie), keyed by ui.art.movie.defineTag.
const CvArtInfoMovie* CvBuildingInfo::getMovieInfo() const
{
	const char* pcTag = getMovieDefineTag();
	if (pcTag != NULL && *pcTag != '\0' && strcmp(pcTag, "NONE") != 0) return ARTFILEMGR.getMovieArtInfo(pcTag);
	return NULL;
}
const char* CvBuildingInfo::getMovie() const
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
// the ONE unconditioned-family sum (JsonModScan; [DEC-single-implementation]) -- kept as a local alias for the walk below
static int fam_uncond100(const CvJsonModFamily* f, CvCascUnit unit)
{ return JsonModScan::familyUnconditioned100(f, unit); }
static int bldNameIndex(const char* const* names, int n, const std::string& s)
{ for (int i = 0; i < n; ++i) if (s == names[i]) return i; return -1; }

void CvBuildingInfo::reconstructFromComposed()
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
	m_iPrereqPopulation  = cnd_andMin(op, "POPULATION");   // engine DORMANCY leg -> authored on OPERATE (curator 2026-07-15)
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
	// NEEDS-power / NEEDS-freshwater are engine DORMANCY legs -> authored on OPERATE (curator 2026-07-15).
	// PROVIDES-power is attributes.providesPower, read directly by isPower() -- never a requires atom.
	m_bFreshWater = cnd_hasPredicate(op, CASC_PRED_HAS_FRESHWATER);
	m_bNoHolyCity = build && cnd_hasPredicate(build->disabled, CASC_PRED_IS_HOLY_CITY);
	m_bPower      = cnd_hasPredicate(op, CASC_PRED_HAS_POWER);
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

	// getFreeBonuses -- the manufactured-resource (and culture-building) TRADE-NETWORK + vicinity supply (owner ruling
	// 2026-07-11: manufactured resources are trade + vicinity, not vicinity-only). The engine's processBuilding
	// injects the provides delta straight into the city's plot group (getFreeBonus is COMPUTED from the processed
	// buildings + the persisted event grants -- no ledger) -> getNumBonuses/hasBonus (TRADE)
	// AND clears the vicinity caches (hasVicinityBonus). Sourced from provides.bonuses (BOTH are the legacy
	// ExtraFreeBonuses); with the poco's getFreeBonuses stubbed empty the free bonus was vicinity-only (cascade provides)
	// and never entered the trade network -- so anything needing it via `trade` (tanks/helicopters) couldn't be built,
	// and culture buildings' free bonus vanished. The supply COUNT rides provides.bonuses ({BONUS_X:N}; absent = 1)
	// -- e.g. HOLLYWOOD supplies 6 (the legacy iNumFreeBonuses), restoring tradeable-luxury supply that a flat count-1 cut.
	{
		const CvJsonProvides* pv = getProvides();
		if (pv != NULL)
			for (size_t i = 0; i < pv->bonuses.size(); ++i)
				if (pv->bonuses[i] >= 0) m_freeBonuses.setValue((BonusTypes)pv->bonuses[i], pv->countOf(pv->bonuses[i]));
	}

	// -------------------- GROUP 2: the keyed modifier families --------------------
	const CvJsonModifiers* mods = getModifiers();
	if (!mods) return;
	static const char* YN[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
	static const char* CN[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

	// PROPERTY_* per-turn SOURCES (property-audit.md increments B+4 + the one-shot ruling): city/plot flats ->
	// m_PropertyManipulators (KEEP-legacy solver); empire flats (the converted <PropertiesAllCities>) -> the
	// all-cities container, delivered per owner city by the CvGameObjectCity gather. The ONE shared walk
	// (clear-and-refill inside -- the CvInfo.h idempotency contract).
	CascadePropertyBridge::bridgeFamilies(mods, m_PropertyManipulators, NO_RELATION, 0, &m_PropertyManipulatorsAllCities);

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
		// (PROPERTY_* families are bridged by the ONE shared CascadePropertyBridge::bridgeFamilies walk above.)
		if (f0.compare(0, 9, "PROPERTY_") == 0) continue;
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

	// getConsumptionRelevantBonuses -- archive buildConsumptionRelevantBonuses: the deduped UNION of every BONUS keyed
	// by this building's bonus-conditioned modifier maps (health/happiness/defense/yield-change/yield-modifier), all
	// filled above. Consumers test membership only, so a plain key-union reproduces it (the load-derived view of wired data).
	m_consumptionRelevantBonuses.clear();
	{
		std::vector<bool> seen(GC.getNumBonusInfos(), false);
		const IDValueMap<BonusTypes, int>* intMaps[3] = { &m_piBonusHealthChanges, &m_piBonusHappinessChanges, &m_bonusDefenseChanges };
		for (int m = 0; m < 3; ++m)
			for (IDValueMap<BonusTypes, int>::const_iterator it = intMaps[m]->begin(); it != intMaps[m]->end(); ++it)
			{ const int b = it->first; if (b >= 0 && b < (int)seen.size() && !seen[b]) { seen[b] = true; m_consumptionRelevantBonuses.push_back((BonusTypes)b); } }
		const IDValueMap<BonusTypes, YieldArray>* yaMaps[2] = { &m_bonusYieldChanges, &m_bonusYieldModifier };
		for (int m = 0; m < 2; ++m)
			for (IDValueMap<BonusTypes, YieldArray>::const_iterator it = yaMaps[m]->begin(); it != yaMaps[m]->end(); ++it)
			{ const int b = it->first; if (b >= 0 && b < (int)seen.size() && !seen[b]) { seen[b] = true; m_consumptionRelevantBonuses.push_back((BonusTypes)b); } }
	}
}
