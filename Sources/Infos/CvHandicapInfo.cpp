//------------------------------------------------------------------------------------------------
//  FILE:    CvHandicapInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvHandicapInfo.h"
#include "CvJsonParse.h"          // jsonFamVal / jsonFamMemberVal / jsonChildObj / jsonIdInt / jsonResolveId
#include "CvCascadePropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk


CvHandicapInfo::CvHandicapInfo()
	: m_iFreeWinsVsBarbs(0)
	, m_iAnimalAttackProb(0)
	, m_iAdvancedStartPointsMod(0)
	, m_iStartingGold(0)
	, m_iUnitUpkeepPercent(0)
	, m_iDistanceMaintenancePercent(0)
	, m_iNumCitiesMaintenancePercent(0)
	, m_iColonyMaintenancePercent(0)
	, m_iMaxColonyMaintenance(0)
	, m_iCorporationMaintenancePercent(0)
	, m_iCivicUpkeepPercent(0)
	, m_iInflationPercent(0)
	, m_iRevolutionIndexPercent(0)
	, m_iHealthBonus(0)
	, m_iHappyBonus(0)
	, m_iAttitudeChange(0)
	, m_iNoTechTradeModifier(0)
	, m_iTechTradeKnownModifier(0)
	, m_iUnownedWaterTilesPerBarbarianUnit(0)
	, m_iUnownedTilesPerBarbarianCity(0)
	, m_iBarbarianCityCreationTurnsElapsed(0)
	, m_iBarbarianCityCreationProb(0)
	, m_iAnimalCombatModifier(0)
	, m_iBarbarianCombatModifier(0)
	, m_iAIAnimalCombatModifier(0)
	, m_iSubdueAnimalBonusAI(0)
	, m_iAIBarbarianCombatModifier(0)
	, m_iStartingDefenseUnits(0)
	, m_iStartingWorkerUnits(0)
	, m_iStartingExploreUnits(0)
	, m_iAIStartingDefenseUnits(0)
	, m_iAIStartingWorkerUnits(0)
	, m_iAIStartingExploreUnits(0)
	, m_iBarbarianInitialDefenders(0)
	, m_iAIDeclareWarProb(0)
	, m_iAIWorkRateModifier(0)
	, m_iAIGrowthPercent(0)
	, m_iAITrainPercent(0)
	, m_iAIWorldTrainPercent(0)
	, m_iAIConstructPercent(0)
	, m_iAIWorldConstructPercent(0)
	, m_iAICreatePercent(0)
	, m_iAIResearchPercent(0)
	, m_iAIWorldCreatePercent(0)
	, m_iAICivicUpkeepPercent(0)
	, m_iAIUnitUpkeepPercent(0)
	, m_iAIUnitSupplyPercent(0)
	, m_iAIUnitUpgradePercent(0)
	, m_iAIInflationPercent(0)
	, m_iAIWarWearinessPercent(0)
	, m_iAIPerEraModifier(0)
	, m_iAIAdvancedStartPercent(0)
{
}


// entity[family][scope][member][ai][unit] -- the AI-AUDIENCE sibling of a member leaf (json: an extra "ai" object
// hop before the unit; the human/AI dual-leaf pattern, curate_handicap.py). 5 levels -> no shared walker exists;
// any missing hop -> 0 (the curator drops zero values, so absent is faithfully 0).
static int jsonFamMemberAiVal(const picojson::object& o, const char* family, const char* scope,
	const char* member, const char* unit)
{
	const picojson::object* fo = jsonChildObj(o, family);   if (!fo) return 0;
	const picojson::object* so = jsonChildObj(*fo, scope);  if (!so) return 0;
	const picojson::object* mo = jsonChildObj(*so, member); if (!mo) return 0;
	const picojson::object* ao = jsonChildObj(*mo, "ai");   if (!ao) return 0;
	picojson::object::const_iterator u = ao->find(unit);
	return (u != ao->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}


// #430: the FLAT family model (no `modifiers` wrapper) -> the mirrored scalar members. Values are engine-native
// ints (percent/flat authored as-is; NO ×100 -- the reader walkers do not scale). The human/AI DUALITY: a leaf's
// bare unit is the BASE (all players); the sibling `ai` object is the AI-only override (stacked or AI-only, per
// field). See curate_handicap.py FAMILIES/GRANTS + handicaps.md for the sourcing (which record a player reads is
// the engine's job, not encoded here).
void CvHandicapInfo::mapFrom(const picojson::value& entity)
{
	m_piGoodies.clear();       // idempotency (CvInfo.h): the full-registry re-run must not double-append the roster
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + the section dispatch into m_modifiers
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// --- maintenance.empire.* : gold city-maintenance components (colony carries BOTH percent AND the hard cap) ---
	m_iDistanceMaintenancePercent    = jsonFamMemberVal(o, "maintenance", "empire", "distance",    "percent");
	m_iNumCitiesMaintenancePercent   = jsonFamMemberVal(o, "maintenance", "empire", "numCities",   "percent");
	m_iColonyMaintenancePercent      = jsonFamMemberVal(o, "maintenance", "empire", "colony",      "percent");
	m_iMaxColonyMaintenance          = jsonFamMemberVal(o, "maintenance", "empire", "colony",      "cap");
	m_iCorporationMaintenancePercent = jsonFamMemberVal(o, "maintenance", "empire", "corporation", "percent");

	// --- upkeep.empire.* : recurring gold upkeep (unit/civic/inflation DUAL base+ai; supply/upgrade AI-only) ---
	m_iUnitUpkeepPercent    = jsonFamMemberVal(o,   "upkeep", "empire", "unit",      "percent");
	m_iAIUnitUpkeepPercent  = jsonFamMemberAiVal(o, "upkeep", "empire", "unit",      "percent");
	m_iCivicUpkeepPercent   = jsonFamMemberVal(o,   "upkeep", "empire", "civic",     "percent");
	m_iAICivicUpkeepPercent = jsonFamMemberAiVal(o, "upkeep", "empire", "civic",     "percent");
	m_iInflationPercent     = jsonFamMemberVal(o,   "upkeep", "empire", "inflation", "percent");
	m_iAIInflationPercent   = jsonFamMemberAiVal(o, "upkeep", "empire", "inflation", "percent");
	m_iAIUnitSupplyPercent  = jsonFamMemberAiVal(o, "upkeep", "empire", "supply",    "percent");
	m_iAIUnitUpgradePercent = jsonFamMemberAiVal(o, "upkeep", "empire", "upgrade",   "percent");

	// --- wellbeing single-concept families (empire.flat; base -- applies to every city of the owner) ---
	m_iHealthBonus = jsonFamVal(o, "health",    "empire", "flat");
	m_iHappyBonus  = jsonFamVal(o, "happiness", "empire", "flat");

	// --- AI economy rates (PROVISIONAL family names; single-concept, AI-only -> empire.ai.percent) ---
	m_iAIGrowthPercent    = jsonFamMemberVal(o, "growth",   "empire", "ai", "percent");
	m_iAIResearchPercent  = jsonFamMemberVal(o, "techCost", "empire", "ai", "percent");  // AI tech-COST % (renamed off "research")
	m_iAIWorkRateModifier = jsonFamMemberVal(o, "workRate", "empire", "ai", "percent");
	m_iAIPerEraModifier   = jsonFamMemberVal(o, "perEra",   "empire", "ai", "percent");  // meta: ramps the AI family per era

	// --- buildCost.empire.<kind>.ai.percent : AI build-cost per produced kind (all AI-only) ---
	m_iAITrainPercent          = jsonFamMemberAiVal(o, "buildCost", "empire", "train",          "percent");
	m_iAIWorldTrainPercent     = jsonFamMemberAiVal(o, "buildCost", "empire", "worldTrain",     "percent");
	m_iAIConstructPercent      = jsonFamMemberAiVal(o, "buildCost", "empire", "construct",      "percent");
	m_iAIWorldConstructPercent = jsonFamMemberAiVal(o, "buildCost", "empire", "worldConstruct", "percent");
	m_iAICreatePercent         = jsonFamMemberAiVal(o, "buildCost", "empire", "create",         "percent");
	m_iAIWorldCreatePercent    = jsonFamMemberAiVal(o, "buildCost", "empire", "worldCreate",    "percent");

	// --- revolution.empire.percent : the Revolution-index % (WIP mechanic; single-concept, base) ---
	m_iRevolutionIndexPercent = jsonFamVal(o, "revolution", "empire", "percent");

	// --- diplomacy: empire (attitude flat, declareWar/warWeariness AI-only) + team (tech-trade thresholds, base) ---
	m_iAttitudeChange        = jsonFamMemberVal(o,   "diplomacy", "empire", "attitude",     "flat");
	m_iAIDeclareWarProb      = jsonFamMemberAiVal(o, "diplomacy", "empire", "declareWar",   "percent");
	m_iAIWarWearinessPercent = jsonFamMemberAiVal(o, "diplomacy", "empire", "warWeariness", "percent");
	m_iNoTechTradeModifier    = jsonFamMemberVal(o, "diplomacy", "team", "noTechTrade",    "percent");
	m_iTechTradeKnownModifier = jsonFamMemberVal(o, "diplomacy", "team", "techTradeKnown", "percent");

	// --- combat: world wildlife/barbarian odds (animal/barbarian DUAL, subdueAnimal AI-only) + empire freeWins ---
	m_iAnimalCombatModifier      = jsonFamMemberVal(o,   "combat", "world", "animal",    "percent");
	m_iAIAnimalCombatModifier    = jsonFamMemberAiVal(o, "combat", "world", "animal",    "percent");
	m_iBarbarianCombatModifier   = jsonFamMemberVal(o,   "combat", "world", "barbarian", "percent");
	m_iAIBarbarianCombatModifier = jsonFamMemberAiVal(o, "combat", "world", "barbarian", "percent");
	m_iSubdueAnimalBonusAI       = jsonFamMemberAiVal(o, "combat", "world", "subdueAnimal", "percent");
	m_iFreeWinsVsBarbs           = jsonFamMemberVal(o,   "combat", "empire", "freeWinsVsBarbs", "flat");

	// --- barbarians.world.* : game-global barbarian/animal spawn rules ---
	m_iAnimalAttackProb                  = jsonFamMemberVal(o, "barbarians", "world", "animalAttackProb",  "percent");
	m_iUnownedWaterTilesPerBarbarianUnit = jsonFamMemberVal(o, "barbarians", "world", "waterTilesPerUnit", "flat");
	m_iUnownedTilesPerBarbarianCity      = jsonFamMemberVal(o, "barbarians", "world", "tilesPerCity",      "flat");
	m_iBarbarianCityCreationTurnsElapsed = jsonFamMemberVal(o, "barbarians", "world", "cityCreationTurns", "flat");
	m_iBarbarianCityCreationProb         = jsonFamMemberVal(o, "barbarians", "world", "cityCreationProb",  "percent");
	m_iBarbarianInitialDefenders         = jsonFamMemberVal(o, "barbarians", "world", "defenders",         "flat");

	// --- grants: one-shot GAME-START provisioning, read off the COMPOSED unit. The base keys are §5 numeric PULSES
	//     (stored ×100 by the section parse); the AI override rides `grants.ai.<key>`, which the same parse captures
	//     as a SCOPED pulse under scope "ai" (humans and AIs split entirely, own vs game handicap). ONE
	//     representation, so the grants machine reads the same parsed data these scalars view. ---
	m_iStartingGold         = m_grants.pulse100("startingGold") / 100;
	m_iStartingDefenseUnits = m_grants.pulse100("startingDefenseUnits") / 100;
	m_iStartingWorkerUnits  = m_grants.pulse100("startingWorkerUnits") / 100;
	m_iStartingExploreUnits = m_grants.pulse100("startingExploreUnits") / 100;
	//     (the nested object parses as channel "ai" -> {key -> value}, so "ai" is the CHANNEL argument here)
	m_iAIStartingDefenseUnits = m_grants.scopedPulse100("ai", "startingDefenseUnits") / 100;
	m_iAIStartingWorkerUnits  = m_grants.scopedPulse100("ai", "startingWorkerUnits")  / 100;
	m_iAIStartingExploreUnits = m_grants.scopedPulse100("ai", "startingExploreUnits") / 100;

	// --- identity: the parked advanced-start POINTS BUDGET mods + the goody-hut roster (GOODY_* FK strings -> ids) ---
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		if (const picojson::object* as = jsonChildObj(*io, "advancedStart"))
		{
			m_iAdvancedStartPointsMod = jsonIdInt(*as, "pointsMod");
			m_iAIAdvancedStartPercent = jsonIdInt(*as, "aiPercent");
		}
		picojson::object::const_iterator gd = io->find("goodies");
		if (gd != io->end() && gd->second.is<picojson::array>())
		{
			const picojson::array& a = gd->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					const int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0) m_piGoodies.push_back(id);   // unresolved GOODY_* surface via jsonResolveId (Orwell)
				}
		}
	}

	// PROPERTY_* per-turn SOURCES (per-handicap crime/education): the player gather walks the handicap alongside
	// civics/traits/heritage (CvGameObjectPlayer::foreachManipulator) -> RELATION_ASSOCIATED fans each source to
	// every owner city. #429 KNOWN GAP: the curator emits PROPERTY_*.city.flat as a SCALAR {value, per} shape, not
	// the gated-list shape bridgeFamilies expects, so this legitimately yields EMPTY today -- an ACCEPTED fail-loud
	// gap (the manipulators stay empty; NOT faked). Wired the ONE shared way so it lights up once #429 reconciles.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);
}
