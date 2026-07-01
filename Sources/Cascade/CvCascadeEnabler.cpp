//
//	CvCascadeEnabler -- the #430 "can I?" machine (enabler.md §1-3): ONE GENERATE->GATE primitive over the InfoRepo
//	`enables` edges, applied per gate. Gates by scope:
//	   - CITY-scope:   canConstruct (buildings), canTrain (units), canCreate (projects), canMaintain (processes)
//	   - PLAYER-scope: canResearch (techs), canDoCivics (civics), canHurry (hurries)
//	   - UNIT-scope:   canAcquirePromotion (promotions)      - PLOT-scope: canBuild (builds)
//	   - PLAYER-STATE predicates (not a JSON frontier -- reproduced from game state): canFoundReligion, cap:canFoundOnPeaks
//	GENERATE  CAN GET = union(`enables`.<bucket>) over HAVE (team techs + adopted civics [+ the city's buildings for
//	   city-scope] + the universal TECH_GAME_START root) minus (obsoletes ∪ replaces ∪ disables), minus the target-side
//	   `obsoletedBy.techs`.
//	GATE      each candidate by `requires` (the typed-condition evaluator vs the city/player/unit/plot live ctx) + `allowed` (tally cap).
//	The passes are kept DISTINCT (not a per-entity output-match -- the DEC-stonebase-follows-spec trap). Shadowed vs
//	the live engine gates (names mirror them).
//
//	⏳ canFound DEFERRED (the founding RULE -- distance/area/water/peak; the capability half is done). canAddHeritage is a
//	SEPARATE move (its real gate is the MISSION_HERITAGE / `CvOutcome` system -- a mission, not the frontier). ⏳ FIRST-cut
//	divergences EXPECTED + attributed (validation.md): HAVE dominant sources still being added; `allowed` self-caps only;
//	`requires` reads the LIVE object (interim).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeEnabler.h"
#include "CvJsonInfo.h"
#include "CvJsonTechInfo.h"           // CvJsonTechInfo -- capabilities now live here (off the base), techs the only grantor
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvCascadeConditionEval.h"   // cascadeEvalCondition -- the StoneBase-ported typed-condition evaluator (was BoolExpr)
#include "CvEventSpine.h"             // the #430 dispatch spine -- the shadow diff rides it (SD_ENABLER), NOT direct gDLL->logMsg
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvSpecialBuildingInfo.h"   // getMaxPlayerInstances (special-building group cap)
#include "Infos/CvWorldInfo.h"             // getBuildingPrereqModifier (the raw world scalar for ScaledPrereq)
#include "Infos/CvUnitInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvProcessInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvUnitCombatInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Infos/CvHurryInfo.h"
#include "Engine/CvUnit.h"
#include "Engine/CvPlot.h"
#include "Engine/CvMap.h"
#include "Engine/CvGame.h"
#include <map>
#include <set>
#include <string>
#include <vector>

typedef std::map<std::string, std::set<int> > EnBucketSets;

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const char* EN_BUCKETS[] = { "buildings", "units", "projects", "processes", "techs", "civics", "promotions", "builds", "hurries", NULL };

// The per-(bucket) InfoRepo dispatch -- the entity's CvJsonInfo by bucket name + id.
static const CvJsonInfo* en_jsonFor(const std::string& b, int id)
{
	if (b == "buildings") return InfoRepo<CvBuildingInfo>::get().get(id);
	if (b == "units")     return InfoRepo<CvUnitInfo>::get().get(id);
	if (b == "projects")  return InfoRepo<CvProjectInfo>::get().get(id);
	if (b == "processes") return InfoRepo<CvProcessInfo>::get().get(id);
	if (b == "techs")     return InfoRepo<CvTechInfo>::get().get(id);
	if (b == "civics")    return InfoRepo<CvCivicInfo>::get().get(id);
	if (b == "promotions") return InfoRepo<CvPromotionInfo>::get().get(id);
	if (b == "builds")    return InfoRepo<CvBuildInfo>::get().get(id);
	// "hurries" has no InfoRepo (HURRY_ not in RJ_REPO_TYPES) -> NULL; canHurry needs only the enables.hurries edges
	// (on the civics/techs), and a NULL json passes requires/allowed -> the gate IS "the hurry type is generated".
	return NULL;
}

static void en_addEdge(const CvJsonInfo* j, const std::string& key, std::set<int>& out)
{
	if (j == NULL) return;
	std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find(key);
	if (it == j->edges.end()) return;
	for (size_t i = 0; i < it->second.size(); ++i) out.insert(it->second[i]);
}

// Accumulate one HAVE entity's source-side edges across every bucket: enables ADD to candidates; obsoletes/replaces/
// disables collected into rem (subtracted after the whole HAVE set is gathered -- enabler.md §2 set-difference).
static void en_accumHave(const CvJsonInfo* j, EnBucketSets& cand, EnBucketSets& rem)
{
	if (j == NULL) return;
	for (int i = 0; EN_BUCKETS[i] != NULL; ++i)
	{
		const std::string b = EN_BUCKETS[i];
		en_addEdge(j, "enables." + b, cand[b]);
		en_addEdge(j, "obsoletes." + b, rem[b]);
		en_addEdge(j, "replaces." + b, rem[b]);
		en_addEdge(j, "disables." + b, rem[b]);
	}
}

// GENERATE (enabler.md §2). HAVE = team techs + adopted civics (+ the city's buildings if pCity != NULL).
static void en_generate(const CvPlayer& kPlayer, const CvCity* pCity, EnBucketSets& cand)
{
	const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
	EnBucketSets rem;
	// The synthetic TECH_GAME_START root: every civ grants it, so it is universally HELD -- seed its `enables` (the
	// no-tech-prereq starting set: CAVE_DWELLING/NOMADISM/LANGUAGE, UNIT_BRUTE, …) into GENERATE for every player.
	en_accumHave(&cascadeStartNode(), cand, rem);
	for (int iT = 0; iT < GC.getNumTechInfos(); ++iT)
		if (kTeam.isHasTech((TechTypes)iT)) en_accumHave(InfoRepo<CvTechInfo>::get().get(iT), cand, rem);
	for (int iCO = 0; iCO < GC.getNumCivicOptionInfos(); ++iCO)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)iCO);
		if (eCivic != NO_CIVIC) en_accumHave(InfoRepo<CvCivicInfo>::get().get((int)eCivic), cand, rem);
	}
	if (pCity != NULL)
	{
		const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
		for (size_t i = 0; i < aHas.size(); ++i) en_accumHave(InfoRepo<CvBuildingInfo>::get().get((int)aHas[i]), cand, rem);
	}
	for (EnBucketSets::iterator it = cand.begin(); it != cand.end(); ++it)
	{
		const std::set<int>& r = rem[it->first];
		for (std::set<int>::const_iterator jt = r.begin(); jt != r.end(); ++jt) it->second.erase(*jt);
	}
}

static bool en_obsoletedByHeldTech(const CvJsonInfo* j, const CvTeam& kTeam)
{
	if (j == NULL) return false;
	std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find("obsoletedBy.techs");
	if (it == j->edges.end()) return false;
	for (size_t i = 0; i < it->second.size(); ++i)
		if (kTeam.isHasTech((TechTypes)it->second[i])) return true;
	return false;
}

// requires gate: build ∧ operate, evaluated through the typed-condition evaluator (StoneBase port) against the live
// engine ctx. The ENABLER reads with STRICT flags -- a {STATE_RELIGION:X} in requires.build must MATCH the player's
// state religion (the modifier's lenient compound is the modifier-side reading; json §3.5 / enabler §3).
static bool en_requiresMet(const CvJsonInfo* j, const CvCascadeEvalCtx& ec)
{
	if (j == NULL) return true;
	CvCascadeEvalFlags flags;
	flags.strictStateReligionForBuild = true;
	if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) return false;
	if (j->requiresOperate != NULL && !cascadeEvalCondition(j->requiresOperate, ec, flags)) return false;
	return true;
}

static bool en_allowedOk(const CvJsonInfo* j, int iId, const CvPlayer& kPlayer, bool bUnit)
{
	if (j == NULL) return true;
	for (std::map<std::string, int>::const_iterator it = j->allowed.begin(); it != j->allowed.end(); ++it)
	{
		const std::string& k = it->first;
		CascadeCountScope eScope; int iEntity;
		if (k == "world")       { eScope = CASCADE_COUNT_WORLD;  iEntity = 0; }
		else if (k == "team")   { eScope = CASCADE_COUNT_TEAM;   iEntity = (int)kPlayer.getTeam(); }
		else if (k == "empire") { eScope = CASCADE_COUNT_EMPIRE; iEntity = (int)kPlayer.getID(); }
		else continue;   // category cap -> first-cut TODO
		const int iCount = bUnit ? cascadeTally().unitCount(iEntity, iId, eScope)
		                         : cascadeTally().buildingCount(iEntity, iId, eScope);
		if (iCount >= it->second) return false;
	}
	return true;
}

// EMPIRE CAPABILITY query (json.md §8): does the team have <cap>? Capabilities are TECH-granted (e.g. canFoundOnPeaks
// from TECH_ALGEBRA; civics grant `policies`, a separate axis), so it is the union over the team's held techs' mapped
// CvJsonInfo.capabilities. This is the "now queryable" completion of the capabilities map -- the canFound/canBuild gates
// and the team-ability systems read it. (NB capability != policy: capabilities are tech/empire abilities.)
static bool en_empireHasCapability(const CvTeam& kTeam, const std::string& cap)
{
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
	{
		if (!kTeam.isHasTech((TechTypes)t)) continue;
		// InfoRepo<CvTechInfo> always creates a CvJsonTechInfo (JsonPayload) -- the down-cast is safe. capabilities is a
		// CvJsonTechInfo field now (moved off the base; group-unambiguity: techs are the only capability grantor).
		const CvJsonTechInfo* j = static_cast<const CvJsonTechInfo*>(InfoRepo<CvTechInfo>::get().get(t));
		if (j != NULL && j->capabilities.count(cap) != 0) return true;
	}
	return false;
}

// canFoundReligion -- a PLAYER-WIDE state predicate (CvPlayer::canFoundReligion): NOT a JSON frontier, reproduced from
// game state so the cascade owns the gate (it is what enables/AI-reads the religion-founding action). >=1 city, not
// NPC, not the first 3 turns; under RELIGION_LIMITED a holy-city owner cannot found another (minus the rebel /
// LIMITED_RELIGIONS_EXCEPTIONS carve-out). Reads raw state only -- a faithful mirror, shadowed vs the engine.
static bool en_canFoundReligion(const CvPlayer& kPlayer)
{
	if (kPlayer.getNumCities() < 1 || kPlayer.isNPC()
	|| (GC.getGame().isGameStart() && GC.getGame().getElapsedGameTurns() < 3))
		return false;
	if (GC.getGame().isOption(GAMEOPTION_RELIGION_LIMITED))
		if (((kPlayer.getNumCities() > 1) && !kPlayer.isRebel()) || !GC.isLIMITED_RELIGIONS_EXCEPTIONS())
			if (kPlayer.hasHolyCity())
				return false;
	return true;
}

// GATE: candidates[bucket] -> the available set (requires + allowed + obsoletedBy).
static void en_gateSet(const std::string& bucket, const EnBucketSets& cand, const CvCascadeEvalCtx& ec,
	const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail)
{
	EnBucketSets::const_iterator b = cand.find(bucket);
	if (b == cand.end()) return;
	for (std::set<int>::const_iterator it = b->second.begin(); it != b->second.end(); ++it)
	{
		const CvJsonInfo* j = en_jsonFor(bucket, *it);
		if (en_obsoletedByHeldTech(j, kTeam)) continue;
		if (en_requiresMet(j, ec) && en_allowedOk(j, *it, kPlayer, bUnit)) avail.insert(*it);
	}
}

// ===================== StoneBase CascadingEnabler PORT — the per-domain cascades =====================
// The faithful port of StoneBase's CascadingEnabler (owner ruling 2026-06-30). These REPLACE the generic
// enables-frontier en_gateSet for their four domains: StoneBase iterates the WHOLE domain and gates by `requires`
// (the engine's canConstruct/canTrain/canResearch have NO enables-frontier -- an enables-frontier silently
// UNDER-offers a no-enabler entity like PALACE). Static entity flags come from the engine info getters (they
// survive cutover, JSON-backed); conditions/edges/dormant from CvJsonInfo; live state from the engine object.

// --- TechCascade.cs: a tech is available iff not disabled, not held, under allowed.world, requires.build holds.
// (Default flags -- TechCascade uses `new ConditionEvaluator()`.) The all-techs+requires set is "researchable now".
static void en_techAvailable(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	CvCascadeEvalCtx ec; ec.player = &kPlayer; ec.team = &kTeam;
	CvCascadeEvalFlags flags;   // default (NOT strict) -- mirrors StoneBase TechCascade's plain evaluator
	const int nT = GC.getNumTechInfos();
	for (int t = 0; t < nT; ++t)
	{
		if (GC.getTechInfo((TechTypes)t).isDisable()) continue;        // IsDisabled
		if (kTeam.isHasTech((TechTypes)t)) continue;                   // held
		const CvJsonInfo* j = InfoRepo<CvTechInfo>::get().get(t);
		if (j != NULL)
		{
			std::map<std::string, int>::const_iterator w = j->allowed.find("world");
			if (w != j->allowed.end())                                 // world cap (rare: a globally-unique tech)
			{
				int held = 0;
				for (int tm = 0; tm < MAX_TEAMS; ++tm)
					if (GET_TEAM((TeamTypes)tm).isAlive() && GET_TEAM((TeamTypes)tm).isHasTech((TechTypes)t)) ++held;
				if (held >= w->second) continue;
			}
			if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) continue;
		}
		avail.insert(t);
	}
}

// (BuildCascade.cs -- the unlock-level all-builds set -- is SUBSUMED by the per-plot canBuild shadow below: that gate
// already evaluates each build's requires.build against the live PLOT, which covers BuildCascade's tech-unlock plus
// the placement BuildCascade omits. Not re-ported as a separate unlock-only pass; the owner's scope is Building/Tech/Unit.)

// Instance cap (StoneBase Capped): the entity is maxed at some scope -- current tally count + in-production making >=
// allowed. Reads the cascade's own allowed (CvJsonInfo) + tally + the live making, NOT the engine's isBuildingMaxedOut
// (that would be tautological vs canConstruct -- the shadow must validate the cascade's OWN count).
static bool en_buildingCapped(const CvJsonInfo* j, int eB, const CvPlayer& kPlayer)
{
	if (j == NULL) return false;
	const int making = kPlayer.getBuildingMaking((BuildingTypes)eB);   // the player's in-production count
	for (std::map<std::string, int>::const_iterator it = j->allowed.begin(); it != j->allowed.end(); ++it)
	{
		CascadeCountScope sc; int ent;
		if (it->first == "world")       { sc = CASCADE_COUNT_WORLD;  ent = 0; }
		else if (it->first == "team")   { sc = CASCADE_COUNT_TEAM;   ent = (int)kPlayer.getTeam(); }
		else if (it->first == "empire") { sc = CASCADE_COUNT_EMPIRE; ent = (int)kPlayer.getID(); }
		else continue;   // category caps (worldWonders/...) live on CultureLevel -- a follow-on
		if (cascadeTally().buildingCount(ent, eB, sc) + making >= it->second) return true;
	}
	return false;
}

// ScaledPrereq (StoneBase BuildingCascade.ScaledPrereq, VERBATIM): the required count of a PrereqNumOfBuildings prereq --
// world-size-scaled (getModifiedIntValue: wsMod>0 -> *(100+m)/100; wsMod<0 -> *100/(100-m)), then *(1+selfCount) unless
// SELF is a limited wonder; bypassed (= base) if SELF is forceNoPrereqScaling OR the PREREQ is a limited wonder. This is
// a faithful TRANSCRIPTION of the legacy CvPlayer::getBuildingPrereqBuilding math -- ported, NOT called (the legacy
// method does not understand the cascade). CHALLENGE_ONE_CITY is omitted, as StoneBase omits it.
static int en_scaledPrereq(int baseN, int wsMod, bool selfLimited, bool prereqLimited, bool selfNoScale, int selfCount)
{
	if (baseN < 1) return 0;
	if (selfNoScale || prereqLimited) return baseN;
	int req = wsMod > 0 ? baseN * (100 + wsMod) / 100 : (wsMod < 0 ? baseN * 100 / (100 - wsMod) : baseN);
	if (!selfLimited) req *= (1 + selfCount);
	return std::max(1, req);
}

// AugmentState's prereq-WAIVER set (StoneBase BuildingCascade.AugmentState: ObsoleteBuildings ∪ PrereqWaivedBuildings):
// a BUILDING is a waived prereq iff its OBSOLETE tech is held by the team, OR its SpecialBuilding group is made
// not-required by an adopted civic (enables.specialBuildingsWaived). Shared by the building + unit cascades (both gate
// requires.build through the SAME evaluator). The vicinity-supply + gov-center AugmentState facts are read LIVE by the
// evaluator (hasVicinityBonus / isGovernmentCenter), so only the waived set is materialized here.
static void en_augmentWaived(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& waived)
{
	const int nB = GC.getNumBuildingInfos();
	for (int b = 0; b < nB; ++b)   // obsolete-by-held-tech
	{
		const TechTypes obs = GC.getBuildingInfo((BuildingTypes)b).getObsoleteTech();
		if (obs != NO_TECH && kTeam.isHasTech(obs)) waived.insert(b);
	}
	std::set<int> waivedSpecials;   // the SpecialBuilding groups the player's adopted civics make not-required
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = kPlayer.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* j = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (j == NULL) continue;
		std::map<std::string, std::vector<int> >::const_iterator it = j->edges.find("enables.specialBuildingsWaived");
		if (it != j->edges.end())
			for (size_t i = 0; i < it->second.size(); ++i) waivedSpecials.insert(it->second[i]);
	}
	if (!waivedSpecials.empty())
		for (int b = 0; b < nB; ++b)
		{
			const SpecialBuildingTypes sb = GC.getBuildingInfo((BuildingTypes)b).getSpecialBuilding();
			if (sb != NO_SPECIALBUILDING && waivedSpecials.count((int)sb) != 0) waived.insert(b);
		}
}

// --- BuildingCascade.cs: the city's BUILDABLE set (the engine canConstruct TRUE-set), computed IN ISOLATION.
// FRONTIER = ALL buildings (the engine has NO enables-frontier; an enables-frontier under-offers no-enabler buildings
// like PALACE). Prune in StoneBase's order: tech-obsolete, already-built, in-queue, never-buildable (notConstructible),
// instance-capped, special-building GROUP-capped, dormant-on-build, prereq-AMOUNT unmet. Then GATE requires.build
// (STRICT state religion) + requires.operate (IgnoreDisabled -- its dormancy `disabled` must not remove the building
// from buildable; POSITIVE prereqs still gate, with obsolete/civic-waived prereqs skipped via the AugmentState set).
static void en_buildingBuildable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	std::set<int> waived;
	en_augmentWaived(kPlayer, kTeam, waived);
	CvCascadeEvalCtx ec; ec.city = pCity; ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	CvCascadeEvalFlags buildFlags; buildFlags.strictStateReligionForBuild = true;   // requires.build = strict
	CvCascadeEvalFlags operFlags;  operFlags.ignoreDisabled = true;                  // requires.operate = positive prereqs only
	const int nB = GC.getNumBuildingInfos();

	// QueuedBuildings (StoneBase exclude): buildings already in THIS city's production order queue.
	std::set<int> queued;
	for (int iq = 0; iq < pCity->getOrderQueueLength(); ++iq)
	{
		const OrderData od = pCity->getOrderData(iq);
		if (od.eOrderType == ORDER_CONSTRUCT) queued.insert(od.iData1);
	}

	// SpecialBuilding GROUP COUNT (StoneBase GroupCount): the player's summed count of every member of each group.
	std::map<int, int> specialCount;
	for (int b = 0; b < nB; ++b)
	{
		const SpecialBuildingTypes sb = GC.getBuildingInfo((BuildingTypes)b).getSpecialBuilding();
		if (sb != NO_SPECIALBUILDING)
			specialCount[(int)sb] += cascadeTally().buildingCount((int)kPlayer.getID(), b, CASCADE_COUNT_EMPIRE);
	}

	const int wsMod = GC.getWorldInfo(GC.getMap().getWorldSize()).getBuildingPrereqModifier();

	for (int b = 0; b < nB; ++b)
	{
		const BuildingTypes eB = (BuildingTypes)b;
		const CvBuildingInfo& bi = GC.getBuildingInfo(eB);
		const CvJsonInfo* j = InfoRepo<CvBuildingInfo>::get().get(b);
		if (en_obsoletedByHeldTech(j, kTeam)) continue;                  // PRUNE: tech-obsolescence (obsoletedBy.techs)
		if (pCity->hasBuilding(eB)) continue;                           // EXCLUDE: already built in this city
		if (queued.count(b) != 0) continue;                             // EXCLUDE: already in this city's production queue
		// EXCLUDE never-buildable: productionCost < 0 = the engine's "can never be built" marker (notConstructible,
		// OUTSIDE canConstruct -- owner ruling 2026-06-30), as is an auto-placed building.
		if (bi.getProductionCost() < 0 || bi.isAutoBuild()) continue;
		if (en_buildingCapped(j, b, kPlayer)) continue;                 // INSTANCE CAP (created + making >= allowed)
		// SPECIALBUILDING GROUP CAP: a member leaves buildable once its group count >= getMaxPlayerInstances (engine
		// special buildings have only a player cap; -1 = uncapped).
		const SpecialBuildingTypes sb = bi.getSpecialBuilding();
		if (sb != NO_SPECIALBUILDING)
		{
			const int cap = GC.getSpecialBuildingInfo(sb).getMaxPlayerInstances();
			if (cap != -1 && specialCount[(int)sb] >= cap) continue;
		}
		// DORMANT-ON-BUILD: a building whose successor (operate.dormant trigger) is already present is born dormant.
		bool bornDormant = false;
		if (j != NULL)
			for (size_t i = 0; i < j->dormantTriggers.size() && !bornDormant; ++i)
				if (pCity->hasBuilding((BuildingTypes)j->dormantTriggers[i])) bornDormant = true;
		if (bornDormant) continue;
		// PREREQ-AMOUNT scaling (StoneBase): each PrereqNumOfBuildings prereq needs a SCALED count of that building.
		const bool selfLimited = (j != NULL && !j->allowed.empty());
		const bool selfNoScale = bi.isForceNoPrereqScaling();
		const int selfCount = cascadeTally().buildingCount((int)kPlayer.getID(), b, CASCADE_COUNT_EMPIRE);
		bool amountFailed = false;
		const IDValueMap<BuildingTypes, int>& prereqs = bi.getPrereqNumOfBuildings();
		for (IDValueMap<BuildingTypes, int>::const_iterator it = prereqs.begin(); it != prereqs.end() && !amountFailed; ++it)
		{
			const CvJsonInfo* pj = InfoRepo<CvBuildingInfo>::get().get((int)it->first);
			const bool prereqLimited = (pj != NULL && !pj->allowed.empty());
			const int required = en_scaledPrereq(it->second, wsMod, selfLimited, prereqLimited, selfNoScale, selfCount);
			if (cascadeTally().buildingCount((int)kPlayer.getID(), (int)it->first, CASCADE_COUNT_EMPIRE) < required) amountFailed = true;
		}
		if (amountFailed) continue;
		// GATE: requires.build (strict) + requires.operate (ignoreDisabled -- positive prereqs only).
		if (j != NULL)
		{
			if (j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, buildFlags)) continue;
			if (j->requiresOperate != NULL && !cascadeEvalCondition(j->requiresOperate, ec, operFlags)) continue;
		}
		avail.insert(b);
	}
}

// Unit instance cap (StoneBase UnitCascade.Capped): WORLD = lifetime-created (getUnitCreatedCount) + making >=
// allowed.world; EMPIRE = live count (tally) + making >= ERA-SCALED (base-5 => +5/era) allowed.empire, waived by
// NO_NATIONAL_UNIT_LIMIT unless the unit is unlimitedException. (Units have no team cap.)
static bool en_unitCapped(const CvJsonInfo* j, int eU, const CvPlayer& kPlayer, bool noNationalLimit)
{
	if (j == NULL) return false;
	const int making = kPlayer.getUnitMaking((UnitTypes)eU);
	std::map<std::string, int>::const_iterator w = j->allowed.find("world");
	if (w != j->allowed.end() && GC.getGame().getUnitCreatedCount((UnitTypes)eU) + making >= w->second) return true;
	std::map<std::string, int>::const_iterator e = j->allowed.find("empire");
	if (e != j->allowed.end() && !(noNationalLimit && !GC.getUnitInfo((UnitTypes)eU).isUnlimitedException()))
	{
		const int era = (int)kPlayer.getCurrentEra();
		const int cap = (e->second == 5 && era > 0) ? e->second + era * 5 : e->second;   // era-scaled base-5 national cap
		if (cascadeTally().unitCount((int)kPlayer.getID(), eU, CASCADE_COUNT_EMPIRE) + making >= cap) return true;
	}
	return false;
}

// reachable(v) (StoneBase UnitCascade.Reachable): v is itself available OR some DIRECT upgrade of v (its dormant
// triggers = requires.build.dormant.all) is reachable. Cycle-guarded (a cycle resolves to the self-available terminal).
static bool en_unitReachable(int v, const std::set<int>& available, std::map<int, bool>& cache, std::set<int>& inProgress)
{
	std::map<int, bool>::const_iterator c = cache.find(v);
	if (c != cache.end()) return c->second;
	if (!inProgress.insert(v).second) return available.count(v) != 0;   // cycle -> self-available terminal
	bool r = available.count(v) != 0;
	if (!r)
	{
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(v);
		if (j != NULL)
			for (size_t i = 0; i < j->dormantTriggers.size() && !r; ++i)
				r = en_unitReachable(j->dormantTriggers[i], available, cache, inProgress);
	}
	inProgress.erase(v);
	cache[v] = r;
	return r;
}

// --- UnitCascade.cs: the city's TRAINABLE set (the engine canTrain TRUE-set), GENERATE-then-GATE. Units REUSE the
// building machinery -- only the inputs differ. (1) GATE availability: all units minus spawnOnly (cost<0, never built --
// owner 2026-06-30) / tech-obsoleted / instance-capped, then requires.build (STRICT). (2) GENERATE frontier: all units
// minus spawnOnly/obsoleted/replaced-when-the-replacer-is-available (the `replaces` edge -- source-side, inverted;
// inert today, enabler.md §2). (3) GATE the frontier: LISTED = available AND not dormant (requires.build.dormant.all =
// the direct-upgrade closure: a unit hides only when EVERY direct upgrade is reachable-trainable; one dead branch keeps
// it buildable). AugmentState vicinity/gov-center facts are read LIVE by the evaluator.
static void en_unitTrainable(const CvCity* pCity, const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& result)
{
	std::set<int> waived;
	en_augmentWaived(kPlayer, kTeam, waived);   // SAME AugmentState waiver the building cascade uses (shared evaluator)
	CvCascadeEvalCtx ec; ec.city = pCity; ec.player = &kPlayer; ec.team = &kTeam; ec.waivedPrereqBuildings = &waived;
	CvCascadeEvalFlags flags; flags.strictStateReligionForBuild = true;
	const bool noNationalLimit = GC.getGame().isOption(GAMEOPTION_NO_NATIONAL_UNIT_LIMIT);
	const int nU = GC.getNumUnitInfos();

	// (1) GATE availability.
	std::set<int> available;
	for (int u = 0; u < nU; ++u)
	{
		if (GC.getUnitInfo((UnitTypes)u).getProductionCost() < 0) continue;   // spawnOnly: never built (outside canTrain)
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (en_obsoletedByHeldTech(j, kTeam)) continue;
		if (en_unitCapped(j, u, kPlayer, noNationalLimit)) continue;
		if (j != NULL && j->requiresBuild != NULL && !cascadeEvalCondition(j->requiresBuild, ec, flags)) continue;
		available.insert(u);
	}

	// The replaced set: a unit is HIDDEN if any AVAILABLE unit's `replaces.units` names it (source-side edge inverted;
	// no target-side replacedBy is curated -- inert today). Computed BEFORE its own requires is weighed (a GENERATE removal).
	std::set<int> replacedUnits;
	for (std::set<int>::const_iterator a = available.begin(); a != available.end(); ++a)
	{
		const CvJsonInfo* ja = InfoRepo<CvUnitInfo>::get().get(*a);
		if (ja == NULL) continue;
		std::map<std::string, std::vector<int> >::const_iterator re = ja->edges.find("replaces.units");
		if (re != ja->edges.end())
			for (size_t i = 0; i < re->second.size(); ++i) replacedUnits.insert(re->second[i]);
	}

	// (2) GENERATE frontier: all units minus spawnOnly / obsoleted / replaced.
	std::set<int> frontier;
	for (int u = 0; u < nU; ++u)
	{
		if (GC.getUnitInfo((UnitTypes)u).getProductionCost() < 0) continue;
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		if (en_obsoletedByHeldTech(j, kTeam)) continue;
		if (replacedUnits.count(u) != 0) continue;
		frontier.insert(u);
	}

	// (3) GATE the frontier: LISTED = in CAN GET ∧ available ∧ not dormant.
	std::map<int, bool> cache; std::set<int> inProgress;
	for (std::set<int>::const_iterator it = frontier.begin(); it != frontier.end(); ++it)
	{
		const int u = *it;
		if (available.count(u) == 0) continue;   // in CAN GET but requires.build unmet => GREYED, not LISTED
		const CvJsonInfo* j = InfoRepo<CvUnitInfo>::get().get(u);
		bool dormant = false;
		if (j != NULL && !j->dormantTriggers.empty())
		{
			dormant = true;
			for (size_t i = 0; i < j->dormantTriggers.size() && dormant; ++i)
				if (!en_unitReachable(j->dormantTriggers[i], available, cache, inProgress)) dormant = false;
		}
		if (!dormant) result.insert(u);
	}
}

// ===================== [ENABLER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The shadow's diff + summary emit EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct gDLL->logMsg) -- the
// CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level. Per-emitter domain (SD_ENABLER),
// one file (Cascade.log). The per-turn summary is split into one COUNTS event + one per-gate SHADOW event (the field cap
// is 16; a single ~24-field line doesn't fit -- event-spine.md drop/redo).
enum EnEvt { ENE_DIFF = 1, ENE_SHADOW, ENE_COUNTS };
enum EnFld
{
	ENF_WHO = 1, ENF_GATE, ENF_TYPE, ENF_CASC, ENF_LEG,   // diff
	ENF_CITIES, ENF_PLAYERS, ENF_UNITS, ENF_PLOTS,        // counts
	ENF_DIV, ENF_CHK                                       // per-gate summary
};
static const char* en_prefix(int evt)
{
	switch (evt)
	{
	case ENE_DIFF:   return "[ENABLER/diff]";
	case ENE_SHADOW: return "[ENABLER/shadow]";
	case ENE_COUNTS: return "[ENABLER/shadow]";
	default:         return "[ENABLER]";
	}
}
static const char* en_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case ENF_WHO:     *peType = SFT_WSTR; return "who";
	case ENF_GATE:    *peType = SFT_STR;  return "gate";
	case ENF_TYPE:    *peType = SFT_STR;  return "type";
	case ENF_CASC:    return "casc";
	case ENF_LEG:     return "leg";
	case ENF_CITIES:  return "cities";
	case ENF_PLAYERS: return "players";
	case ENF_UNITS:   return "units";
	case ENF_PLOTS:   return "plots";
	case ENF_DIV:     return "diverging";
	case ENF_CHK:     return "checked";
	default:          return NULL;
	}
}
static void en_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_ENABLER, en_prefix, "Cascade.log", en_field); s_reg = true; }
}

// Per-gate diff sample (capped). iShownGate is the GATE's own sample counter (caps samples PER GATE so each gets examples).
static void en_emitDiff(const wchar_t* szWho, const char* szGate, const char* szType, bool bCasc, bool bLeg, int& iShownGate)
{
	if (iShownGate >= 6) return;
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_DIFF, 1)
		.addWStr(ENF_WHO, szWho).addStr(ENF_GATE, szGate).addStr(ENF_TYPE, szType)
		.addI(ENF_CASC, bCasc ? 1 : 0).addI(ENF_LEG, bLeg ? 1 : 0));
	++iShownGate;
}

// Per-gate counters (chk/div) + a per-gate sample cap (shown) so every gate gets diff examples -- a shared cap got
// eaten entirely by the first-iterated gate.
struct EnGate { int chk, div, shown; EnGate() : chk(0), div(0), shown(0) {} };

// canAcquirePromotion -- the per-UNIT shape. HAVE = the unit's held promotions + team techs + its unitcombat;
// GENERATE enables.promotions over that (minus the obsoletes/replaces/disables); GATE by `requires` (vs the UNIT's
// game object). Shadowed vs isPromotionValid (the prereq-availability the cascade GENERATE->GATE reproduces).
// Sample-capped. Same primitive as the frontier gates -- only the HAVE source + context object differ.
static void en_shadowPromotions(const CvPlayer& kPlayer, const CvTeam& kTeam, int nPromo, EnGate& g, int& iUnits)
{
	int iLoop;
	for (const CvUnit* pUnit = kPlayer.firstUnit(&iLoop); pUnit != NULL && iUnits < 12; pUnit = kPlayer.nextUnit(&iLoop))
	{
		++iUnits;
		EnBucketSets cand, rem;
		for (int pr = 0; pr < nPromo; ++pr)
			if (pUnit->isHasPromotion((PromotionTypes)pr)) en_accumHave(InfoRepo<CvPromotionInfo>::get().get(pr), cand, rem);
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kTeam.isHasTech((TechTypes)t)) en_accumHave(InfoRepo<CvTechInfo>::get().get(t), cand, rem);
		const UnitCombatTypes eUC = pUnit->getUnitCombatType();
		if (eUC != NO_UNITCOMBAT) en_accumHave(InfoRepo<CvUnitCombatInfo>::get().get((int)eUC), cand, rem);

		std::set<int>& promoCand = cand["promotions"];
		const std::set<int>& promoRem = rem["promotions"];
		for (std::set<int>::const_iterator it = promoRem.begin(); it != promoRem.end(); ++it) promoCand.erase(*it);

		CvCascadeEvalCtx ec;
		ec.unit = pUnit; ec.player = &kPlayer; ec.team = &kTeam; ec.plot = pUnit->plot();
		const wchar_t* szWho = pUnit->getName().GetCString();
		for (int pr = 0; pr < nPromo; ++pr)
		{
			++g.chk;
			const bool bCasc = promoCand.count(pr) != 0 && en_requiresMet(InfoRepo<CvPromotionInfo>::get().get(pr), ec);
			const bool bLeg = pUnit->isPromotionValid((PromotionTypes)pr);
			if (bCasc != bLeg) { ++g.div; en_emitDiff(szWho, "canAcquirePromotion", GC.getPromotionInfo((PromotionTypes)pr).getType(), bCasc, bLeg, g.shown); }
		}
	}
}

void cvCascadeEnablerShadow()
{
	// Emit EVERY end-turn (gated by gPlayerLogLevel) -- NOT a one-shot. The one-shot probe fought the iterative
	// capture->attribute->fix->re-capture validation loop (you had to reload the save to re-arm it). Free when
	// gPlayerLogLevel<1; the per-gate work is bounded by the 8-city / sample caps below.
	if (gPlayerLogLevel < 1) return;
	en_registerDomain();   // self-register SD_ENABLER on the spine (idempotent) before the first emit

	const int nB = GC.getNumBuildingInfos(), nU = GC.getNumUnitInfos();
	const int nP = GC.getNumProjectInfos(), nProc = GC.getNumProcessInfos();
	const int nT = GC.getNumTechInfos(), nC = GC.getNumCivicInfos(), nPromo = GC.getNumPromotionInfos();
	const int nBld = GC.getNumBuildInfos();
	const int nHur = GC.getNumHurryInfos();
	EnGate gConstruct, gTrain, gCreate, gMaintain, gResearch, gCivics, gPromote, gBuild, gCapPeaks;
	EnGate gHurry, gFoundRel;
	int iCities = 0, iPlayers = 0, iUnits = 0, iPlots = 0;

	for (int p = 0; p < MAX_PLAYERS && iCities < 8; ++p)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
		if (!kPlayer.isAlive()) continue;
		const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());

		// ---- CAPABILITY shadow: the mapped+queried capability vs the engine team flag (clean parity, no founding
		// rule). canFoundOnPeaks (TECH_ALGEBRA) vs CvTeam::isCanFoundOnPeaks -- verifies the §8 capabilities map+query.
		{
			++gCapPeaks.chk;
			const bool c = en_empireHasCapability(kTeam, "canFoundOnPeaks");
			const bool l = kTeam.isCanFoundOnPeaks();
			if (c != l) { ++gCapPeaks.div; en_emitDiff(kPlayer.getName(), "cap:canFoundOnPeaks", "", c, l, gCapPeaks.shown); }
		}

		// ---- PLAYER-scope gates (canResearch / canDoCivics): one GENERATE/GATE per player ----
		if (iPlayers < 4)
		{
			++iPlayers;
			EnBucketSets candP;
			en_generate(kPlayer, NULL, candP);
			CvCascadeEvalCtx pec;                          // PLAYER-scope eval ctx (no city/plot)
			pec.player = &kPlayer; pec.team = &kTeam;
			std::set<int> availT, availCv;
			en_techAvailable(kPlayer, kTeam, availT);    // TechCascade port (all-techs frontier; replaces the enables-frontier)
			en_gateSet("civics", candP, pec, kPlayer, kTeam, false, availCv);
			const wchar_t* szWho = kPlayer.getName();
			for (int t = 0; t < nT; ++t)
			{
				// Oracle = canResearch (researchable NOW): the all-techs+requires.build set is "prereqs held now", which is
				// canResearch, NOT the broader canEverResearch (could-ever). The two MUST be paired (changed with the port).
				++gResearch.chk; bool c = availT.count(t) != 0, l = kPlayer.canResearch((TechTypes)t);
				if (c != l) { ++gResearch.div; en_emitDiff(szWho, "canResearch", GC.getTechInfo((TechTypes)t).getType(), c, l, gResearch.shown); }
			}
			for (int cv = 0; cv < nC; ++cv)
			{
				++gCivics.chk; bool c = availCv.count(cv) != 0, l = kPlayer.canDoCivics((CivicTypes)cv);
				if (c != l) { ++gCivics.div; en_emitDiff(szWho, "canDoCivics", GC.getCivicInfo((CivicTypes)cv).getType(), c, l, gCivics.shown); }
			}
			// canHurry: the player-level enablement = the hurry type is generated (enables.hurries, mostly civics) --
			// the gate that lights the two Python hurry buttons / tells the AI it can hurry. (City-level gold/slavery
			// AMOUNT checks are runtime, outside this frontier.) Shadowed vs CvPlayer::canHurry = getHurryCount>0.
			std::set<int> availHur;
			en_gateSet("hurries", candP, pec, kPlayer, kTeam, false, availHur);
			for (int hu = 0; hu < nHur; ++hu)
			{
				++gHurry.chk;
				const bool c = availHur.count(hu) != 0;
				const bool l = kPlayer.canHurry((HurryTypes)hu);
				if (c != l) { ++gHurry.div; en_emitDiff(szWho, "canHurry", GC.getHurryInfo((HurryTypes)hu).getType(), c, l, gHurry.shown); }
			}
			// canFoundReligion: a player-wide state predicate (one verdict/player), reproduced vs the engine.
			{
				++gFoundRel.chk;
				const bool c = en_canFoundReligion(kPlayer);
				const bool l = kPlayer.canFoundReligion();
				if (c != l) { ++gFoundRel.div; en_emitDiff(szWho, "canFoundReligion", "", c, l, gFoundRel.shown); }
			}
		}

		// ---- UNIT-scope gate (canAcquirePromotion): per-unit GENERATE->GATE, sample-capped across players ----
		en_shadowPromotions(kPlayer, kTeam, nPromo, gPromote, iUnits);

		// ---- CITY-scope gates (canConstruct / canTrain / canCreate / canMaintain) ----
		int iLoop;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL && iCities < 8; pCity = kPlayer.nextCity(&iLoop))
		{
			++iCities;
			EnBucketSets candC;
			en_generate(kPlayer, pCity, candC);
			CvCascadeEvalCtx cec;                          // CITY-scope eval ctx
			cec.city = pCity; cec.player = &kPlayer; cec.team = &kTeam;
			std::set<int> avB, avU, avPr, avProc;
			en_buildingBuildable(pCity, kPlayer, kTeam, avB);   // BuildingCascade port (all-buildings frontier)
			en_unitTrainable(pCity, kPlayer, kTeam, avU);       // UnitCascade port (generate-then-gate)
			en_gateSet("projects",  candC, cec, kPlayer, kTeam, false, avPr);
			en_gateSet("processes", candC, cec, kPlayer, kTeam, false, avProc);
			const wchar_t* szWho = pCity->getName().GetCString();
			for (int b = 0; b < nB; ++b)
			{
				++gConstruct.chk; bool c = avB.count(b) != 0, l = pCity->canConstruct((BuildingTypes)b, false, false, true);
				if (c != l) { ++gConstruct.div; en_emitDiff(szWho, "canConstruct", GC.getBuildingInfo((BuildingTypes)b).getType(), c, l, gConstruct.shown); }
			}
			for (int u = 0; u < nU; ++u)
			{
				++gTrain.chk; bool c = avU.count(u) != 0, l = pCity->canTrain((UnitTypes)u, false, false, true);
				if (c != l) { ++gTrain.div; en_emitDiff(szWho, "canTrain", GC.getUnitInfo((UnitTypes)u).getType(), c, l, gTrain.shown); }
			}
			for (int pr = 0; pr < nP; ++pr)
			{
				++gCreate.chk; bool c = avPr.count(pr) != 0, l = pCity->canCreate((ProjectTypes)pr, false, false);
				if (c != l) { ++gCreate.div; en_emitDiff(szWho, "canCreate", GC.getProjectInfo((ProjectTypes)pr).getType(), c, l, gCreate.shown); }
			}
			for (int pc = 0; pc < nProc; ++pc)
			{
				++gMaintain.chk; bool c = avProc.count(pc) != 0, l = pCity->canMaintain((ProcessTypes)pc);
				if (c != l) { ++gMaintain.div; en_emitDiff(szWho, "canMaintain", GC.getProcessInfo((ProcessTypes)pc).getType(), c, l, gMaintain.shown); }
			}
		}
	}

	// ---- PLOT-scope gate (canBuild, worker): sampled OWNED non-city plots. GENERATE = the owner's enables.builds
	// (techs), GATE = the build's `requires` vs the PLOT game object (terrain predicates). Shadow vs CvPlot::canBuild.
	// (capability-gated builds may diverge until those caps are wired -- first-cut, attributed.)
	const int nMapPlots = GC.getMap().numPlots();
	for (int ip = 0; ip < nMapPlots && iPlots < 12; ++ip)
	{
		const CvPlot* pPlot = GC.getMap().plotByIndex(ip);
		if (pPlot == NULL || pPlot->isCity()) continue;
		const PlayerTypes eOwner = pPlot->getOwner();
		if (eOwner == NO_PLAYER || !GET_PLAYER(eOwner).isAlive()) continue;
		++iPlots;
		const CvPlayer& kOwner = GET_PLAYER(eOwner);
		const CvTeam& kOTeam = GET_TEAM(kOwner.getTeam());
		CvCascadeEvalCtx pec;                          // PLOT-scope eval ctx (the build's target plot)
		pec.plot = pPlot; pec.player = &kOwner; pec.team = &kOTeam;
		CvCascadeEvalFlags bflags; bflags.strictStateReligionForBuild = true;
		// BuildCascade (StoneBase): the FRONTIER is ALL builds (not the enables-frontier), gated by requires.build.
		std::set<int> availBld;
		for (int b = 0; b < nBld; ++b)
		{
			const CvJsonInfo* j = InfoRepo<CvBuildInfo>::get().get(b);
			if (j == NULL || j->requiresBuild == NULL || cascadeEvalCondition(j->requiresBuild, pec, bflags)) availBld.insert(b);
		}
		const wchar_t* szWho = kOwner.getName();
		for (int b = 0; b < nBld; ++b)
		{
			++gBuild.chk; bool c = availBld.count(b) != 0, l = pPlot->canBuild((BuildTypes)b, eOwner);
			if (c != l) { ++gBuild.div; en_emitDiff(szWho, "canBuild", GC.getBuildInfo((BuildTypes)b).getType(), c, l, gBuild.shown); }
		}
	}

	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_COUNTS, 1)
		.addI(ENF_CITIES, iCities).addI(ENF_PLAYERS, iPlayers).addI(ENF_UNITS, iUnits).addI(ENF_PLOTS, iPlots));
	// One per-gate SHADOW event (gate + diverging/checked) -- the old 24-field single line doesn't fit the 16-field cap.
#define EN_GATE_EMIT(NAME, G) eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_SHADOW, 1).addStr(ENF_GATE, NAME).addI(ENF_DIV, (G).div).addI(ENF_CHK, (G).chk))
	EN_GATE_EMIT("canConstruct", gConstruct);
	EN_GATE_EMIT("canTrain", gTrain);
	EN_GATE_EMIT("canCreate", gCreate);
	EN_GATE_EMIT("canMaintain", gMaintain);
	EN_GATE_EMIT("canResearch", gResearch);
	EN_GATE_EMIT("canDoCivics", gCivics);
	EN_GATE_EMIT("canAcquirePromotion", gPromote);
	EN_GATE_EMIT("canBuild", gBuild);
	EN_GATE_EMIT("canHurry", gHurry);
	EN_GATE_EMIT("canFoundReligion", gFoundRel);
	EN_GATE_EMIT("cap:canFoundOnPeaks", gCapPeaks);
#undef EN_GATE_EMIT
}
