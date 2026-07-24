//
//	CvBuildingInfo::mapFrom -- common sections (base dispatch fills the composed units) + the building `identity`
//	block: the StoneBase BuildingInfo flags (notConstructible / governmentCenter / forceNoPrereqScaling /
//	specialBuilding) + the shrine/corpHQ/religion FKs + the stateReligionCommerce / commerceDoubleTime maps.
//	SELF-CONTAINED (the engine getReligionType / getGlobal*Commerce reads are RETIRED). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvBuildingInfo.h"
#include "AI/CvGameAI.h"            // GC.getGame().isOption(...) -- was reaching via unity leakage; self-sufficient now
#include "CvJsonParse.h"            // jsonResolveId / jsonCommerceMap / jsonIdFk / jsonIdInt / jsonIdBool / jsonWorldArt
#include "Property/CvPropertyBridge.h" // the JSON->BoolExpr/IntExpr translator (property-audit.md increment 4)
#include "UI/CvArtFileMgr.h"        // ARTFILEMGR.getBuildingArtInfo / getMovieArtInfo -- the art shims (mirrors CvBonusInfo)
#include "Infos/CvArtInfoMovie.h"   // CvArtInfoMovie complete type -- getMovie() calls getPath() (via CvAssetInfoBase)
#include "Infos/CvArtInfoBuilding.h" // CvArtInfoBuilding complete type -- getButton() call needs the full definition
#include "Defines/CvStructs.h"      // the cy* Python tuple structs (TechYieldChange/BuildingCommerceChange/GenericTrippleInt/...)
#include "CvJsonModScan.h"          // the ONE load-time modifier-family scan (mapFrom materialization)
#include "CvJsonCondition.h"            // CvJsonCondition::kind/predKind -- the plots-target predicate extraction
#include "Conditions/CvConditionEval.h" // CvCascadeEvalCtx -- the valuation eval context
#include "Data/CvDepositRead.h"         // MMKernel::applies -- the ONE conditioned-deposit gate
#include "Engine/CityContext.h"         // CityContext -- fillEvalCtx + plotAttrs (the valuation reads these)
#include "Engine/EmpireContext.h"       // EmpireContext -- fillEvalCtx

void CvBuildingInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom (incl. reconstructFromComposed below),
	// whose writers ACCUMULATE (push_back / mod_addScalar / mod_addYield / +=) -- fully define every output.
	m_aeMapCategoryTypes.clear(); m_enabledCivTypes.clear();
	m_aFreePromoTypes.clear(); m_aiFreeTraitTypes.clear(); m_healUnitCombats.clear(); m_consumptionRelevantBonuses.clear();
	m_piPrereqAndTechs.clear(); m_prereqOrImprovement.clear(); m_prereqOrHeritage.clear();
	m_aePrereqOrBonuses.clear(); m_piPrereqOrVicinityBonuses.clear(); m_aePrereqOrRawVicinityBonuses.clear();
	m_techSpecialistChange.clear();
	m_aBuildingHappinessChanges.clear(); m_religionChange.clear();
	m_bonusDefenseChanges.clear(); m_unitCombatDefenseAgainst.clear(); m_aUnitCombatFreeExperience.clear();
	m_domainFreeExperience.clear(); m_aUnitCombatExtraStrength.clear(); m_aUnitProductionModifier.clear();
	m_unitCombatProdModifier.clear(); m_domainProductionModifier.clear(); m_aBuildingProductionModifier.clear();
	m_aGlobalBuildingProductionModifier.clear(); m_specialistCount.clear(); m_improvementFreeSpecialists.clear();
	m_freeSpecialistCount.clear();
	m_bonusProductionModifier.clear(); m_aGlobalBuildingCostModifier.clear();
	m_aGlobalBuildingCommerceChanges.clear();
	m_cond.clear(); m_counterDamage = CounterDamage();
	m_iNumUnitFullHeal = 0; m_wellbeing[WELLBEING_STATE_RELIGION_HAPPINESS] = 0;

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
				m_counterDamage.present = true;
				m_counterDamage.damage = jsonIdInt(*cd, "damage");
				m_counterDamage.chance = jsonIdInt(*cd, "chance");
				const picojson::object* un = jsonChildObj(*cd, "units");
				m_counterDamage.allAttackers = (un == NULL);   // no `units` selector => all attackers
				if (un)
				{
					picojson::object::const_iterator uc = un->find("unitCombats");
					if (uc != un->end() && uc->second.is<picojson::array>())
					{
						const picojson::array& ua = uc->second.get<picojson::array>();
						for (size_t i = 0; i < ua.size(); ++i)
							if (ua[i].is<std::string>()) { const int uid = jsonResolveId(ua[i].get<std::string>()); if (uid >= 0) m_counterDamage.unitCombats.push_back(uid); }
					}
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
	// grants.freePromotions -> getFreePromoTypes. Each entry may carry the building's unit filter (json §3.9's
	// `enabled`, e.g. IS_MOUNTED on a Riding School), curated from the legacy <FreePromotionCondition>; NULL = all.
	if (const std::vector<int>* fp = getGrants()->list("freePromotions"))
		for (size_t i = 0; i < fp->size(); ++i)
		{
			FreePromoTypes f = { (PromotionTypes)(*fp)[i], getGrants()->listCond("freePromotions", i) };
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
			if (r->unitCombatId >= 0) { HealUnitCombat h = { (UnitCombatTypes)r->unitCombatId, r->heal100, 0 }; m_healUnitCombats.push_back(h); }
		}
	}

	// GROUP 1 (requires condition tree) + GROUP 2 (keyed modifiers) -- the legacy-shaped members.
	reconstructFromComposed();

	// ===== the MATERIALIZATION pass: every legacy scalar / positional getter value is scanned ONCE here
	// (JsonModScan over the composed m_modifiers; same address table the getters carried); the getters are bare
	// member reads -- per-call string-address walks are banned from getters. =====
	{
		const CvJsonModifiers* mods = getModifiers();
		m_wellbeing[WELLBEING_HAPPINESS_PER_POPULATION] = JsonModScan::sum100(mods, "happiness.city", CASC_UNIT_PER_POPULATION);
		m_wellbeing[WELLBEING_HEALTH_PER_POPULATION]    = JsonModScan::sum100(mods, "health.city",    CASC_UNIT_PER_POPULATION);
		m_wellbeing[WELLBEING_HAPPINESS]        = JsonModScan::sum100(mods, "happiness.city", CASC_UNIT_FLAT);
		m_wellbeing[WELLBEING_AREA_HAPPINESS]   = JsonModScan::sum100(mods, "happiness.area", CASC_UNIT_FLAT);
		m_wellbeing[WELLBEING_GLOBAL_HAPPINESS] = JsonModScan::sum100(mods, "happiness.empire", CASC_UNIT_FLAT);
		m_wellbeing[WELLBEING_HEALTH]           = JsonModScan::sum100(mods, "health.city", CASC_UNIT_FLAT);
		m_wellbeing[WELLBEING_AREA_HEALTH]      = JsonModScan::sum100(mods, "health.area", CASC_UNIT_FLAT);
		m_wellbeing[WELLBEING_GLOBAL_HEALTH]    = JsonModScan::sum100(mods, "health.empire", CASC_UNIT_FLAT);
		static const char* MZ_YIELD[NUM_YIELD_TYPES]   = { "food", "production", "commerce" };
		static const char* MZ_COMM[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			m_flatYields[y]             = JsonModScan::sum100(mods, std::string(MZ_YIELD[y]) + ".city", CASC_UNIT_FLAT);
			m_yieldModifiers[y]           = JsonModScan::sum100(mods, std::string(MZ_YIELD[y]) + ".city", CASC_UNIT_PERCENT);
			m_areaYieldModifiers[y]       = JsonModScan::sum100(mods, std::string(MZ_YIELD[y]) + ".area", CASC_UNIT_PERCENT);
			m_globalYieldModifiers[y]     = JsonModScan::sum100(mods, std::string(MZ_YIELD[y]) + ".empire", CASC_UNIT_PERCENT);
			m_seaPlotYields[y] = JsonModScan::sumAll(mods, std::string(MZ_YIELD[y]) + ".empire.plots", CASC_UNIT_FLAT);
		}
		for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		{
			m_flatCommerce[c]          = JsonModScan::sum100(mods, std::string(MZ_COMM[c]) + ".city", CASC_UNIT_FLAT);
			m_commerceModifiers[c]        = JsonModScan::sum100(mods, std::string(MZ_COMM[c]) + ".city", CASC_UNIT_PERCENT);
			m_globalCommerceModifiers[c]  = JsonModScan::sum100(mods, std::string(MZ_COMM[c]) + ".empire", CASC_UNIT_PERCENT);
			m_specialistCommerce[c] = JsonModScan::sum100(mods, std::string(MZ_COMM[c]) + ".empire.specialist", CASC_UNIT_PER_SPECIALIST);
			m_commerceHappiness[c]       = JsonModScan::sum100(mods, std::string("commerceHappiness.city.") + MZ_COMM[c], CASC_UNIT_FLAT);
			std::map<std::string, int>::const_iterator dt = commerceDoubleTime.find(MZ_COMM[c]);
			m_commerceDoubleTime[c] = dt != commerceDoubleTime.end() ? dt->second : 0;
			std::map<std::string, int>::const_iterator sr = stateReligionCommerce.find(MZ_COMM[c]);
			m_stateReligionCommerce[c]    = sr != stateReligionCommerce.end() ? sr->second : 0;
		}
		// --- grouped SCALAR population (patterns.md coherent surface): one direct slot per string, x100-native;
		//     replaces the scattered m_i* assignments. Read via getDefense(DEFENSE_*)/getScalar(SCALAR_*)/... ---
		m_defense[DEFENSE_AMOUNT] = JsonModScan::sum100(mods, "defense.city.amount", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_BOMBARD] = JsonModScan::sum100(mods, "defense.city.bombardDefense", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_ALL_CITY] = JsonModScan::sum100(mods, "defense.empire.amount", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_NUKE] = JsonModScan::sum100(mods, "defense.city.nukeDefense", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_AIR] = JsonModScan::sum100(mods, "defense.city.airDefense", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_MIN] = JsonModScan::sum100(mods, "defense.city.min", CASC_UNIT_FLAT);
		m_defense[DEFENSE_NO_ENTRY_LEVEL] = JsonModScan::sum100(mods, "defense.city.noEntryLevel", CASC_UNIT_FLAT);
		m_defense[DEFENSE_LOCAL_DYNAMIC] = JsonModScan::sum100(mods, "defense.city.dynamicDefense", CASC_UNIT_FLAT);
		m_defense[DEFENSE_RIVER_PENALTY] = JsonModScan::sum100(mods, "defense.city.riverDefensePenalty", CASC_UNIT_FLAT);
		m_defense[DEFENSE_BUILDING_RECOVERY] = JsonModScan::sum100(mods, "defense.city.buildingDefenseRecovery", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_CITY_RECOVERY] = JsonModScan::sum100(mods, "defense.city.cityDefenseRecovery", CASC_UNIT_PERCENT);
		m_defense[DEFENSE_ADJACENT_DAMAGE] = JsonModScan::sum100(mods, "defense.city.adjacentDamage", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_CITY] = JsonModScan::sum100(mods, "maintenance.city", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_GLOBAL] = JsonModScan::sum100(mods, "maintenance.empire", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_AREA] = JsonModScan::sum100(mods, "maintenance.area", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_OTHER_AREA] = JsonModScan::sum100(mods, "maintenance.area.otherArea", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_DISTANCE] = JsonModScan::sum100(mods, "maintenance.empire.distance", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_NUM_CITIES] = JsonModScan::sum100(mods, "maintenance.empire.numCities", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_COASTAL_DISTANCE] = JsonModScan::sum100(mods, "maintenance.empire.coastalDistance", CASC_UNIT_PERCENT);
		m_maintenance[MAINTENANCE_CONNECTED_CITY] = JsonModScan::sum100(mods, "maintenance.empire.connectedCity", CASC_UNIT_PERCENT);
		m_tradeRoutes[TRADE_ROUTE_CITY] = JsonModScan::sum100(mods, "tradeRoutes.city", CASC_UNIT_FLAT);
		m_tradeRoutes[TRADE_ROUTE_COASTAL] = JsonModScan::sum100(mods, "tradeRoutes.empire.coastal", CASC_UNIT_FLAT);
		m_tradeRoutes[TRADE_ROUTE_GLOBAL] = JsonModScan::sum100(mods, "tradeRoutes.empire", CASC_UNIT_FLAT);
		m_tradeRoutes[TRADE_ROUTE_WORLD] = JsonModScan::sum100(mods, "tradeRoutes.world", CASC_UNIT_FLAT);
		m_tradeRoutes[TRADE_ROUTE_MODIFIER] = JsonModScan::sum100(mods, "tradeRoutes.city.modifier", CASC_UNIT_PERCENT);
		m_tradeRoutes[TRADE_ROUTE_FOREIGN_MODIFIER] = JsonModScan::sum100(mods, "tradeRoutes.city.foreignModifier", CASC_UNIT_PERCENT);
		m_greatPeople[GREAT_PEOPLE_RATE_CHANGE] = JsonModScan::sum100(mods, "greatPeopleRate.city", CASC_UNIT_FLAT);
		m_greatPeople[GREAT_PEOPLE_RATE_MODIFIER] = JsonModScan::sum100(mods, "greatPeopleRate.city", CASC_UNIT_PERCENT);
		m_greatPeople[GREAT_PEOPLE_GLOBAL_RATE_MODIFIER] = JsonModScan::sum100(mods, "greatPeopleRate.empire", CASC_UNIT_PERCENT);
		m_greatPeople[GREAT_GENERAL_RATE] = JsonModScan::sum100(mods, "greatGeneralRate.city", CASC_UNIT_PERCENT);
		m_greatPeople[GREAT_GENERAL_DOMESTIC_RATE] = JsonModScan::sum100(mods, "greatGeneralRate.city.domestic", CASC_UNIT_PERCENT);
		m_warWeariness[WAR_WEARINESS_CITY] = JsonModScan::sum100(mods, "warWeariness.city", CASC_UNIT_PERCENT);
		m_warWeariness[WAR_WEARINESS_GLOBAL] = JsonModScan::sum100(mods, "warWeariness.empire", CASC_UNIT_PERCENT);
		m_warWeariness[WAR_WEARINESS_ENEMY] = JsonModScan::sum100(mods, "warWeariness.city.enemy", CASC_UNIT_PERCENT);
		m_hurry[HURRY_COST] = JsonModScan::sum100(mods, "hurryCost.city", CASC_UNIT_PERCENT);
		m_hurry[HURRY_GLOBAL_COST] = JsonModScan::sum100(mods, "hurryCost.empire", CASC_UNIT_PERCENT);
		m_hurry[HURRY_ANGER] = JsonModScan::sum100(mods, "hurryAnger.city", CASC_UNIT_PERCENT);
		m_capture[CITY_CAPTURE_NATIONAL_PROBABILITY] = JsonModScan::sum100(mods, "cityCapture.empire.probability", CASC_UNIT_PERCENT);
		m_capture[CITY_CAPTURE_NATIONAL_RESISTANCE] = JsonModScan::sum100(mods, "cityCapture.empire.resistance", CASC_UNIT_PERCENT);
		m_capture[CITY_CAPTURE_LOCAL_PROBABILITY] = JsonModScan::sum100(mods, "cityCapture.city.probability", CASC_UNIT_PERCENT);
		m_capture[CITY_CAPTURE_LOCAL_RESISTANCE] = JsonModScan::sum100(mods, "cityCapture.city.resistance", CASC_UNIT_PERCENT);
		m_revolution[REVOLUTION_LOCAL] = JsonModScan::sum100(mods, "revolution.city", CASC_UNIT_FLAT);
		m_revolution[REVOLUTION_NATIONAL] = JsonModScan::sum100(mods, "revolution.empire", CASC_UNIT_FLAT);
		m_revolution[REVOLUTION_DISTANCE_MODIFIER] = JsonModScan::sum100(mods, "revolution.city.distanceModifier", CASC_UNIT_PERCENT);
		m_buildRate[BUILD_RATE_MILITARY] = JsonModScan::sum100(mods, "buildRate.city.military", CASC_UNIT_PERCENT);
		m_buildRate[BUILD_RATE_SPACE] = JsonModScan::sum100(mods, "buildRate.city.space", CASC_UNIT_PERCENT);
		m_buildRate[BUILD_RATE_GLOBAL_SPACE] = JsonModScan::sum100(mods, "buildRate.empire.space", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_HEAL_RATE] = JsonModScan::sum100(mods, "healing.city", CASC_UNIT_FLAT);
		m_scalars[SCALAR_FOOD_KEPT] = JsonModScan::sum100(mods, "foodKept.city", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_ANARCHY] = JsonModScan::sum100(mods, "anarchy.city", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_GOLDEN_AGE] = JsonModScan::sum100(mods, "goldenAge.empire", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_INFLATION] = JsonModScan::sum100(mods, "inflation.empire", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_OCCUPATION_TIME] = JsonModScan::sum100(mods, "occupationTime.city", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_WORKER_SPEED] = JsonModScan::sum100(mods, "workRate.empire", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_POP_GROWTH] = JsonModScan::sum100(mods, "populationGrowthRate.city", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_GLOBAL_POP_GROWTH] = JsonModScan::sum100(mods, "populationGrowthRate.empire", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_ESPIONAGE_DEFENSE] = JsonModScan::sum100(mods, "espionageDefense.city", CASC_UNIT_FLAT);
		m_scalars[SCALAR_UNIT_UPGRADE_PRICE] = JsonModScan::sum100(mods, "unitUpgradePrice.empire", CASC_UNIT_PERCENT);
		m_scalars[SCALAR_FREE_EXPERIENCE] = JsonModScan::sum100(mods, "experience.city", CASC_UNIT_FLAT);
		m_scalars[SCALAR_GLOBAL_FREE_EXPERIENCE] = JsonModScan::sum100(mods, "experience.empire", CASC_UNIT_FLAT);
		m_scalars[SCALAR_FREE_SPECIALIST] = JsonModScan::sum100(mods, "freeSpecialists.city.any", CASC_UNIT_COUNT);
		m_scalars[SCALAR_AREA_FREE_SPECIALIST] = JsonModScan::sum100(mods, "freeSpecialists.area.any", CASC_UNIT_COUNT);
		m_scalars[SCALAR_GLOBAL_FREE_SPECIALIST] = JsonModScan::sum100(mods, "freeSpecialists.empire.any", CASC_UNIT_COUNT);
		m_scalars[SCALAR_INSIDIOUSNESS] = JsonModScan::sum100(mods, "copsAndRobbers.city.insidiousness", CASC_UNIT_FLAT);
		m_scalars[SCALAR_INVESTIGATION] = JsonModScan::sum100(mods, "copsAndRobbers.city.investigation", CASC_UNIT_FLAT);
		m_bGrantsGoldenAge           = grantFlag("goldenAge");
		// The first-build provisions. These were per-call string-keyed reads straight off getGrants() on the
		// getter -- the shape [DEC-materialize-at-mapfrom] bans -- and they sit in the building VALUATION loops
		// (CvCityAI.cpp:6236-6266 / :14041, run over the whole building database per city per turn), so every
		// probe was building std::strings and walking maps. Materialized once here; the getters are bare reads.
		m_iGrantPopulationCity   = getGrants() ? getGrants()->scopedPulse100("population", "city")   : 0;
		m_iGrantPopulationEmpire = getGrants() ? getGrants()->scopedPulse100("population", "empire") : 0;
		m_iGrantFreeTechs        = getGrants() ? getGrants()->pulse100("freeTechs") : 0;
		m_iGrantFreeSpecialTech  = getGrants() ? getGrants()->firstListId("techs") : -1;
	}
}

// ===================== #430 mirrored getters -- bare reads of the mapFrom-materialized members =====================
//
// The curator (curate_building.py) collapses a building's own unconditioned SCALAR_FAMILIES scalar and any
// tech/bonus-gated COND_KEYED addend (or per-scaled deposit) onto the SAME <family>.<scope> modifier address; the
// mapFrom MATERIALIZATION pass (JsonModScan) recovers each legacy field by condition shape ONCE at load. Getters
// never walk string addresses.


int CvBuildingInfo::getFlatYield(YieldTypes eYield) const             { return (eYield >= 0 && eYield < NUM_YIELD_TYPES) ? m_flatYields[eYield] : 0; }
int CvBuildingInfo::getYieldModifier(YieldTypes eYield) const           { return (eYield >= 0 && eYield < NUM_YIELD_TYPES) ? m_yieldModifiers[eYield] : 0; }
int CvBuildingInfo::getAreaYieldModifier(YieldTypes eYield) const       { return (eYield >= 0 && eYield < NUM_YIELD_TYPES) ? m_areaYieldModifiers[eYield] : 0; }
int CvBuildingInfo::getGlobalYieldModifier(YieldTypes eYield) const     { return (eYield >= 0 && eYield < NUM_YIELD_TYPES) ? m_globalYieldModifiers[eYield] : 0; }
int CvBuildingInfo::getSeaPlotYield(YieldTypes eYield) const{ return (eYield >= 0 && eYield < NUM_YIELD_TYPES) ? m_seaPlotYields[eYield] : 0; }

int CvBuildingInfo::getFlatCommerce(CommerceTypes eCommerce) const          { return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_flatCommerce[eCommerce] : 0; }
int CvBuildingInfo::getCommerceModifier(CommerceTypes eCommerce) const        { return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_commerceModifiers[eCommerce] : 0; }
int CvBuildingInfo::getGlobalCommerceModifier(CommerceTypes eCommerce) const  { return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_globalCommerceModifiers[eCommerce] : 0; }
int CvBuildingInfo::getSpecialistCommerce(CommerceTypes eCommerce) const { return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_specialistCommerce[eCommerce] : 0; }


// commerceHappiness.city.<commerce>.flat -- happiness gained per unit of each commerce produced (grouped family).
int CvBuildingInfo::getCommerceHappiness(CommerceTypes eCommerce) const
{ return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_commerceHappiness[eCommerce] : 0; }

// commerce double-time / state-religion commerce -- materialized positional arrays (REAL data).
// CULTURE branch mirrors the archive's GAMEOPTION_CULTURE_EQUILIBRIUM default (SourceArchive :238): an
// UNAUTHORED double-time block reads 1000 for culture under the option (NULL-array legacy semantics), so every
// building's culture halves at the equilibrium pace; an authored block keeps its values.
int CvBuildingInfo::getCommerceDoubleTime(CommerceTypes eCommerce) const
{
	if (eCommerce < 0 || eCommerce >= NUM_COMMERCE_TYPES) return 0;
	if (eCommerce == COMMERCE_CULTURE && commerceDoubleTime.empty() && GC.getGame().isOption(GAMEOPTION_CULTURE_EQUILIBRIUM))
		return 1000;
	return m_commerceDoubleTime[eCommerce];
}
int CvBuildingInfo::getStateReligionCommerce(CommerceTypes eCommerce) const
{ return (eCommerce >= 0 && eCommerce < NUM_COMMERCE_TYPES) ? m_stateReligionCommerce[eCommerce] : 0; }

// --- VALUATION endpoints (per-GROUP): the building's ACTUAL ×100 output in a city = the UNCONDITIONED base PLUS every
// conditioned m_cond deposit whose condition holds, via the ONE evaluator (MMKernel::applies over the ctx the contexts
// fill -- CityContext = city/plot, EmpireContext = player/team). CvPlotGroup is the reserved explicit traded-bonus
// source; traded bonuses resolve through the bound city's own plot-group today. ---
void CvBuildingInfo::expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& /*plotGroup*/, int aiOut[NUM_YIELD_TYPES]) const
{
	for (int y = 0; y < NUM_YIELD_TYPES; ++y) aiOut[y] = m_flatYields[y];
	CvCascadeEvalCtx ec; cityContext.fillEvalCtx(ec); empireContext.fillEvalCtx(ec);
	for (std::size_t i = 0; i < m_cond.size(); ++i)
	{
		const CondDeposit& d = m_cond[i];
		if (d.family != COND_YIELD || d.unit != CASC_UNIT_FLAT || d.target != COND_TGT_CITY) continue;
		if (MMKernel::applies(d.e->enabled, d.e->disabled, ec)) aiOut[d.index] += d.e->value100;
	}
}
void CvBuildingInfo::expectedYieldModifiers(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& /*plotGroup*/, int aiOut[NUM_YIELD_TYPES]) const
{
	for (int y = 0; y < NUM_YIELD_TYPES; ++y) aiOut[y] = m_yieldModifiers[y];
	CvCascadeEvalCtx ec; cityContext.fillEvalCtx(ec); empireContext.fillEvalCtx(ec);
	for (std::size_t i = 0; i < m_cond.size(); ++i)
	{
		const CondDeposit& d = m_cond[i];
		if (d.family != COND_YIELD || d.unit != CASC_UNIT_PERCENT || d.target != COND_TGT_CITY) continue;
		if (MMKernel::applies(d.e->enabled, d.e->disabled, ec)) aiOut[d.index] += d.e->value100;
	}
}
void CvBuildingInfo::expectedFlatCommerce(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& /*plotGroup*/, int aiOut[NUM_COMMERCE_TYPES]) const
{
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) aiOut[c] = m_flatCommerce[c];
	CvCascadeEvalCtx ec; cityContext.fillEvalCtx(ec); empireContext.fillEvalCtx(ec);
	for (std::size_t i = 0; i < m_cond.size(); ++i)
	{
		const CondDeposit& d = m_cond[i];
		if (d.family != COND_COMMERCE || d.unit != CASC_UNIT_FLAT || d.target != COND_TGT_CITY) continue;
		if (MMKernel::applies(d.e->enabled, d.e->disabled, ec)) aiOut[d.index] += d.e->value100;
	}
}
void CvBuildingInfo::expectedWellbeing(const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup& /*plotGroup*/, int aiOut[NUM_WELLBEING_KINDS]) const
{
	for (int k = 0; k < NUM_WELLBEING_KINDS; ++k) aiOut[k] = m_wellbeing[k];
	CvCascadeEvalCtx ec; cityContext.fillEvalCtx(ec); empireContext.fillEvalCtx(ec);
	for (std::size_t i = 0; i < m_cond.size(); ++i)
	{
		const CondDeposit& d = m_cond[i];
		if (d.target != COND_TGT_CITY || (d.family != COND_HAPPINESS && d.family != COND_HEALTH)) continue;
		if (MMKernel::applies(d.e->enabled, d.e->disabled, ec))
			aiOut[d.family == COND_HAPPINESS ? WELLBEING_HAPPINESS : WELLBEING_HEALTH] += d.e->value100;
	}
}
// plots-target: <yield>.city.plots output = flat × (worked plots matching the deposit's plot predicate), read from
// cityContext.plotAttrs (the "how many river/water/... plots" counts -- a building reads the CITY context, never a
// PlotContext). A deposit whose condition is not a single tracked plot predicate carries pred=0xFF and is skipped:
// the plotAttrs-coverage limit (river/hills/peak/freshwater/water/land/flatlands folded today; compound/mutable pending).
void CvBuildingInfo::expectedPlotYields(const CityContext& cityContext, const EmpireContext& /*empireContext*/, const CvPlotGroup& /*plotGroup*/, int aiOut[NUM_YIELD_TYPES]) const
{
	for (int y = 0; y < NUM_YIELD_TYPES; ++y) aiOut[y] = 0;
	for (std::size_t i = 0; i < m_cond.size(); ++i)
	{
		const CondDeposit& d = m_cond[i];
		if (d.family != COND_YIELD || d.target != COND_TGT_PLOTS || d.pred == 0xFF) continue;
		aiOut[d.index] += d.e->value100 * cityContext.plotAttrs.count((int)d.pred);
	}
}

// commerce sliders this building unlocks (`capabilities` block -- canSet{Science|Culture|Espionage}Rate; gold has no

// FoundsCorporation -> the building's `enables.corporations` edge (curate_building.py, owner 2026-07-01) -- REAL data.
int CvBuildingInfo::getFoundsCorporation() const
{ const std::vector<int>* v = edge(EDGEF_ENABLES, EDGEB_CORPORATIONS); return (v != NULL && !v->empty()) ? (*v)[0] : -1; }

// REAL: grants.repeatable[] unitCombat heal (getHealUnitCombatType) + identity.enabledCivilizations
// (getEnabledCivilizationType).
const HealUnitCombat& CvBuildingInfo::getHealUnitCombatType(int iIndex) const
{
	if (iIndex >= 0 && iIndex < (int)m_healUnitCombats.size()) return m_healUnitCombats[iIndex];
	static const HealUnitCombat s = { (UnitCombatTypes)-1, 0, 0 }; return s;
}
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

		// commerces: <commerce>.empire.buildings.<B>  (GlobalBuildingExtraCommerces)
		const int ci = bldNameIndex(CN, NUM_COMMERCE_TYPES, f0);
		if (ci >= 0 && s.size() == 4 && fk >= 0 && s[1] == "empire" && s[2] == "buildings")
		{ mod_addComm(m_aGlobalBuildingCommerceChanges, (BuildingTypes)fk, ci, fam_uncond100(f, CASC_UNIT_FLAT)); continue; }

		if (f0 == "happiness" && s.size() == 4 && fk >= 0 && s[1] == "empire" && s[2] == "buildings")
		{ mod_addScalar(m_aBuildingHappinessChanges, (BuildingTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT)); continue; }
		if (f0 == "religion" && s.size() == 3 && fk >= 0 && s[1] == "city")
		{ mod_addScalar(m_religionChange, (ReligionTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT)); continue; }
		if (f0 == "defense" && s.size() == 4 && fk >= 0 && s[1] == "city")
		{ if (s[2] == "bonuses")     mod_addScalar(m_bonusDefenseChanges, (BonusTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT));
		  else if (s[2] == "unitCombats") mod_addScalar(m_unitCombatDefenseAgainst, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT));
		  continue; }
		if (f0 == "experience" && s.size() == 4 && fk >= 0 && s[1] == "city")
		{ if (s[2] == "unitCombats") mod_addScalar(m_aUnitCombatFreeExperience, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT));
		  else if (s[2] == "domains") mod_addScalar(m_domainFreeExperience, (DomainTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT));
		  continue; }
		if (f0 == "strength" && s.size() == 4 && fk >= 0 && s[1] == "city" && s[2] == "unitCombats")
		{ mod_addScalar(m_aUnitCombatExtraStrength, (UnitCombatTypes)fk, fam_uncond100(f, CASC_UNIT_FLAT)); continue; }
		if (f0 == "buildRate" && s.size() == 4 && fk >= 0)
		{ const int v = fam_uncond100(f, CASC_UNIT_PERCENT);
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
				if (e->enabled == NULL) mod_addScalar(m_specialistCount, (SpecialistTypes)fk, e->value100);
				else { const int tech = mod_enabledId(e, "TECH_", -1, -1); if (tech >= 0) m_techSpecialistChange[tech][fk] += e->value100; }
			}
			continue;
		}
		if (f0 == "freeSpecialists" && s.size() == 3 && s[1] == "city")
		{
			if (s[2] == "any")
			{
				for (int i = 0; i < f->size(); ++i)
				{ const CvJsonModEntry* e = f->entries[i]; if (e->hasPer && e->perTypeId >= 0) mod_addScalar(m_improvementFreeSpecialists, (ImprovementTypes)e->perTypeId, e->value100); }
			}
			else if (fk >= 0) mod_addScalar(m_freeSpecialistCount, (SpecialistTypes)fk, fam_uncond100(f, CASC_UNIT_COUNT));
			continue;
		}
		// (PROPERTY_* families are bridged by the ONE shared CascadePropertyBridge::bridgeFamilies walk above.)
		if (f0.compare(0, 9, "PROPERTY_") == 0) continue;
	}

	// Part B: CONDITIONED own-output -> m_cond (typed pointers into m_modifiers; the (cityContext, plotGroup) getters sum value x
	// count(predicate) over the entries whose condition holds). City-scope AND plots-target both fold here; the
	// STATE_RELIGION-gated happiness stays the materialized wellbeing scalar (a fixed engine gate, not data-varying).
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
	{
		const CvJsonModFamily* f = mods->find(std::string(YN[y]) + ".city");
		if (f) for (int i = 0; i < f->size(); ++i)
		{ const CvJsonModEntry* e = f->entries[i]; if (e->enabled || e->disabled) { CondDeposit d; d.family = COND_YIELD; d.index = (unsigned char)y; d.unit = (unsigned char)e->unit; d.target = COND_TGT_CITY; d.pred = 0xFF; d.e = e; m_cond.push_back(d); } }
		const CvJsonModFamily* fp = mods->find(std::string(YN[y]) + ".city.plots");
		if (fp) for (int i = 0; i < fp->size(); ++i)
		{ const CvJsonModEntry* e = fp->entries[i]; CondDeposit d; d.family = COND_YIELD; d.index = (unsigned char)y; d.unit = (unsigned char)e->unit; d.target = COND_TGT_PLOTS; d.pred = (e->enabled != NULL && e->enabled->kind == CASC_COND_PREDICATE) ? (unsigned char)e->enabled->predKind : 0xFF; d.e = e; m_cond.push_back(d); }
	}
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const CvJsonModFamily* f = mods->find(std::string(CN[c]) + ".city");
		if (f) for (int i = 0; i < f->size(); ++i)
		{ const CvJsonModEntry* e = f->entries[i]; if (e->enabled || e->disabled) { CondDeposit d; d.family = COND_COMMERCE; d.index = (unsigned char)c; d.unit = (unsigned char)e->unit; d.target = COND_TGT_CITY; d.pred = 0xFF; d.e = e; m_cond.push_back(d); } }
	}
	for (int h = 0; h < 2; ++h)
	{
		const CvJsonModFamily* f = mods->find((h == 0) ? "happiness.city" : "health.city");
		if (!f) continue;
		for (int i = 0; i < f->size(); ++i)
		{
			const CvJsonModEntry* e = f->entries[i];
			if (e->enabled == NULL && e->disabled == NULL) continue;
			// STATE_RELIGION-gated happiness -> the materialized wellbeing scalar (a fixed engine gate, kept)
			if (h == 0 && e->unit == CASC_UNIT_FLAT &&
			    (mod_enabledPred(e, CASC_PRED_STATE_RELIGION) || mod_enabledPred(e, CASC_PRED_HAS_STATE_RELIGION) || mod_enabledPred(e, CASC_PRED_STATE_RELIGION_IN_CITY)))
			{ m_wellbeing[WELLBEING_STATE_RELIGION_HAPPINESS] += e->value100; continue; }
			CondDeposit d; d.family = (unsigned char)(h == 0 ? COND_HAPPINESS : COND_HEALTH); d.index = 0; d.unit = (unsigned char)e->unit; d.target = COND_TGT_CITY; d.pred = 0xFF; d.e = e; m_cond.push_back(d);
		}
	}
	// buildRate.self percent enabled BONUS -> bonus production modifier
	{ const CvJsonModFamily* f = mods->find("buildRate.self");
	  if (f) for (int i = 0; i < f->size(); ++i) { const CvJsonModEntry* e = f->entries[i]; if (e->enabled && !e->disabled && e->unit == CASC_UNIT_PERCENT) { const int b = mod_enabledId(e, "BONUS_", -1, -1); if (b >= 0) mod_addScalar(m_bonusProductionModifier, (BonusTypes)b, e->value100); } } }
	// costs.empire percent enabled BUILDING -> global building cost modifier
	{ const CvJsonModFamily* f = mods->find("costs.empire");
	  if (f) for (int i = 0; i < f->size(); ++i) { const CvJsonModEntry* e = f->entries[i]; if (e->enabled && !e->disabled && e->unit == CASC_UNIT_PERCENT) { const int b = mod_enabledId(e, "BUILDING_", -1, -1); if (b >= 0) mod_addScalar(m_aGlobalBuildingCostModifier, (BuildingTypes)b, e->value100); } } }

	// getConsumptionRelevantBonuses -- archive buildConsumptionRelevantBonuses: the deduped UNION of every BONUS keyed
	// by this building's bonus-conditioned modifier maps (health/happiness/defense/yield-change/yield-modifier), all
	// filled above. Consumers test membership only, so a plain key-union reproduces it (the load-derived view of wired data).
	m_consumptionRelevantBonuses.clear();
	{
		std::vector<bool> seen(GC.getNumBonusInfos(), false);
		for (IDValueMap<BonusTypes, int>::const_iterator it = m_bonusDefenseChanges.begin(); it != m_bonusDefenseChanges.end(); ++it)
			{ const int b = it->first; if (b >= 0 && b < (int)seen.size() && !seen[b]) { seen[b] = true; m_consumptionRelevantBonuses.push_back((BonusTypes)b); } }
		for (size_t i = 0; i < m_cond.size(); ++i)
			{ const int b = mod_enabledId(m_cond[i].e, "BONUS_", -1, -1); if (b >= 0 && b < (int)seen.size() && !seen[b]) { seen[b] = true; m_consumptionRelevantBonuses.push_back((BonusTypes)b); } }
	}
}
