//
//	CvCascadeEnabler -- the #430 "can I?" machine (enabler.md §1-3): ONE GENERATE->GATE primitive over the InfoRepo
//	`enables` edges, applied per gate. This pass covers the FRONTIER gates in two scopes:
//	   - CITY-scope:   canConstruct (buildings), canTrain (units), canCreate (projects), canMaintain (processes)
//	   - PLAYER-scope: canResearch (techs), canDoCivics (civics)
//	GENERATE  CAN GET = union(`enables`.<bucket>) over HAVE (team techs + adopted civics [+ the city's buildings for
//	   city-scope]) minus (obsoletes ∪ replaces ∪ disables) over HAVE, minus the target-side `obsoletedBy.techs`.
//	GATE      each candidate by `requires` (BoolExpr vs the city/player game object) + `allowed` (tally cap).
//	The passes are kept DISTINCT (not a per-entity output-match -- the DEC-stonebase-follows-spec trap). Shadowed vs
//	the live engine gates (names mirror them: canConstruct/canTrain/canCreate/canMaintain/canEverResearch/canDoCivics).
//
//	⏳ NEXT shapes (own context, not this pass): canAcquirePromotion (per-UNIT, HAVE = the unit's promotions) and
//	canBuild(worker)/canFound (per-PLOT; canFound is `capabilities`-gated, needs that block mapped). ⏳ FIRST-cut
//	divergences EXPECTED + attributed (validation.md): HAVE is the dominant sources (bonuses/religions to add),
//	`allowed` is self-caps only (CultureLevel category caps to add), `requires` reads the LIVE object (modifier interim).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeEnabler.h"
#include "CvJsonInfo.h"
#include "Repos/InfoRepo.h"
#include "CvCascadeTally.h"
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "AI/CvPlayerAI.h"            // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvGameObject.h"
#include "Infrastructure/BoolExpr.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvTechInfo.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvProjectInfo.h"
#include "Infos/CvProcessInfo.h"
#include "Infos/CvPromotionInfo.h"
#include "Infos/CvUnitCombatInfo.h"
#include "Infos/CvBuildInfo.h"
#include "Engine/CvUnit.h"
#include "Engine/CvPlot.h"
#include "Engine/CvMap.h"
#include <map>
#include <set>
#include <string>
#include <vector>

typedef std::map<std::string, std::set<int> > EnBucketSets;

// The enables buckets this pass generates over (one HAVE traversal fills them all).
static const char* EN_BUCKETS[] = { "buildings", "units", "projects", "processes", "techs", "civics", "promotions", "builds", NULL };

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

static bool en_requiresMet(const CvJsonInfo* j, const CvGameObject* ctx)
{
	if (j == NULL) return true;
	if (j->requiresBuild != NULL && !j->requiresBuild->evaluate(ctx)) return false;
	if (j->requiresOperate != NULL && !j->requiresOperate->evaluate(ctx)) return false;
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
		const CvJsonInfo* j = InfoRepo<CvTechInfo>::get().get(t);
		if (j != NULL && j->capabilities.count(cap) != 0) return true;
	}
	return false;
}

// GATE: candidates[bucket] -> the available set (requires + allowed + obsoletedBy).
static void en_gateSet(const std::string& bucket, const EnBucketSets& cand, const CvGameObject* ctx,
	const CvPlayer& kPlayer, const CvTeam& kTeam, bool bUnit, std::set<int>& avail)
{
	EnBucketSets::const_iterator b = cand.find(bucket);
	if (b == cand.end()) return;
	for (std::set<int>::const_iterator it = b->second.begin(); it != b->second.end(); ++it)
	{
		const CvJsonInfo* j = en_jsonFor(bucket, *it);
		if (en_obsoletedByHeldTech(j, kTeam)) continue;
		if (en_requiresMet(j, ctx) && en_allowedOk(j, *it, kPlayer, bUnit)) avail.insert(*it);
	}
}

// Per-gate diff sample (capped).
// iShownGate is the GATE's own sample counter (pass gGate.shown) -- caps samples PER GATE so each gets examples.
static void en_emitDiff(const wchar_t* szWho, const char* szGate, const char* szType, bool bCasc, bool bLeg, int& iShownGate, char* szBuf)
{
	if (iShownGate >= 6) return;
	sprintf(szBuf, "[ENABLER/diff] %S %s %s casc=%d leg=%d", szWho, szGate, szType, bCasc ? 1 : 0, bLeg ? 1 : 0);
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf); ++iShownGate;
}

// Per-gate counters (chk/div) + a per-gate sample cap (shown) so every gate gets diff examples -- a shared cap got
// eaten entirely by the first-iterated gate.
struct EnGate { int chk, div, shown; EnGate() : chk(0), div(0), shown(0) {} };

// canAcquirePromotion -- the per-UNIT shape. HAVE = the unit's held promotions + team techs + its unitcombat;
// GENERATE enables.promotions over that (minus the obsoletes/replaces/disables); GATE by `requires` (vs the UNIT's
// game object). Shadowed vs isPromotionValid (the prereq-availability the cascade GENERATE->GATE reproduces).
// Sample-capped. Same primitive as the frontier gates -- only the HAVE source + context object differ.
static void en_shadowPromotions(const CvPlayer& kPlayer, const CvTeam& kTeam, int nPromo, EnGate& g, int& iUnits, char* szBuf)
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

		const CvGameObject* ctx = pUnit->getGameObject();
		const wchar_t* szWho = pUnit->getName().GetCString();
		for (int pr = 0; pr < nPromo; ++pr)
		{
			++g.chk;
			const bool bCasc = promoCand.count(pr) != 0 && en_requiresMet(InfoRepo<CvPromotionInfo>::get().get(pr), ctx);
			const bool bLeg = pUnit->isPromotionValid((PromotionTypes)pr);
			if (bCasc != bLeg) { ++g.div; en_emitDiff(szWho, "canAcquirePromotion", GC.getPromotionInfo((PromotionTypes)pr).getType(), bCasc, bLeg, g.shown, szBuf); }
		}
	}
}

void cvCascadeEnablerShadow()
{
	static bool s_done = false;
	if (s_done || gPlayerLogLevel < 1) return;
	s_done = true;

	const int nB = GC.getNumBuildingInfos(), nU = GC.getNumUnitInfos();
	const int nP = GC.getNumProjectInfos(), nProc = GC.getNumProcessInfos();
	const int nT = GC.getNumTechInfos(), nC = GC.getNumCivicInfos(), nPromo = GC.getNumPromotionInfos();
	const int nBld = GC.getNumBuildInfos();
	EnGate gConstruct, gTrain, gCreate, gMaintain, gResearch, gCivics, gPromote, gBuild, gCapPeaks;
	int iCities = 0, iPlayers = 0, iUnits = 0, iPlots = 0;
	char szBuf[512];

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
			if (c != l) { ++gCapPeaks.div; en_emitDiff(kPlayer.getName(), "cap:canFoundOnPeaks", "", c, l, gCapPeaks.shown, szBuf); }
		}

		// ---- PLAYER-scope gates (canResearch / canDoCivics): one GENERATE/GATE per player ----
		if (iPlayers < 4)
		{
			++iPlayers;
			EnBucketSets candP;
			en_generate(kPlayer, NULL, candP);
			const CvGameObject* pctx = kPlayer.getGameObject();
			std::set<int> availT, availCv;
			en_gateSet("techs", candP, pctx, kPlayer, kTeam, false, availT);
			en_gateSet("civics", candP, pctx, kPlayer, kTeam, false, availCv);
			const wchar_t* szWho = kPlayer.getName();
			for (int t = 0; t < nT; ++t)
			{
				++gResearch.chk; bool c = availT.count(t) != 0, l = kPlayer.canEverResearch((TechTypes)t);
				if (c != l) { ++gResearch.div; en_emitDiff(szWho, "canResearch", GC.getTechInfo((TechTypes)t).getType(), c, l, gResearch.shown, szBuf); }
			}
			for (int cv = 0; cv < nC; ++cv)
			{
				++gCivics.chk; bool c = availCv.count(cv) != 0, l = kPlayer.canDoCivics((CivicTypes)cv);
				if (c != l) { ++gCivics.div; en_emitDiff(szWho, "canDoCivics", GC.getCivicInfo((CivicTypes)cv).getType(), c, l, gCivics.shown, szBuf); }
			}
		}

		// ---- UNIT-scope gate (canAcquirePromotion): per-unit GENERATE->GATE, sample-capped across players ----
		en_shadowPromotions(kPlayer, kTeam, nPromo, gPromote, iUnits, szBuf);

		// ---- CITY-scope gates (canConstruct / canTrain / canCreate / canMaintain) ----
		int iLoop;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL && iCities < 8; pCity = kPlayer.nextCity(&iLoop))
		{
			++iCities;
			EnBucketSets candC;
			en_generate(kPlayer, pCity, candC);
			const CvGameObject* ctx = pCity->getGameObject();
			std::set<int> avB, avU, avPr, avProc;
			en_gateSet("buildings", candC, ctx, kPlayer, kTeam, false, avB);
			en_gateSet("units",     candC, ctx, kPlayer, kTeam, true,  avU);
			en_gateSet("projects",  candC, ctx, kPlayer, kTeam, false, avPr);
			en_gateSet("processes", candC, ctx, kPlayer, kTeam, false, avProc);
			const wchar_t* szWho = pCity->getName().GetCString();
			for (int b = 0; b < nB; ++b)
			{
				++gConstruct.chk; bool c = avB.count(b) != 0, l = pCity->canConstruct((BuildingTypes)b, false, false, true);
				if (c != l) { ++gConstruct.div; en_emitDiff(szWho, "canConstruct", GC.getBuildingInfo((BuildingTypes)b).getType(), c, l, gConstruct.shown, szBuf); }
			}
			for (int u = 0; u < nU; ++u)
			{
				++gTrain.chk; bool c = avU.count(u) != 0, l = pCity->canTrain((UnitTypes)u, false, false, true);
				if (c != l) { ++gTrain.div; en_emitDiff(szWho, "canTrain", GC.getUnitInfo((UnitTypes)u).getType(), c, l, gTrain.shown, szBuf); }
			}
			for (int pr = 0; pr < nP; ++pr)
			{
				++gCreate.chk; bool c = avPr.count(pr) != 0, l = pCity->canCreate((ProjectTypes)pr, false, false);
				if (c != l) { ++gCreate.div; en_emitDiff(szWho, "canCreate", GC.getProjectInfo((ProjectTypes)pr).getType(), c, l, gCreate.shown, szBuf); }
			}
			for (int pc = 0; pc < nProc; ++pc)
			{
				++gMaintain.chk; bool c = avProc.count(pc) != 0, l = pCity->canMaintain((ProcessTypes)pc);
				if (c != l) { ++gMaintain.div; en_emitDiff(szWho, "canMaintain", GC.getProcessInfo((ProcessTypes)pc).getType(), c, l, gMaintain.shown, szBuf); }
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
		EnBucketSets candPlot;
		en_generate(kOwner, NULL, candPlot);
		const CvGameObject* pctx = pPlot->getGameObject();
		std::set<int> availBld;
		en_gateSet("builds", candPlot, pctx, kOwner, kOTeam, false, availBld);
		const wchar_t* szWho = kOwner.getName();
		for (int b = 0; b < nBld; ++b)
		{
			++gBuild.chk; bool c = availBld.count(b) != 0, l = pPlot->canBuild((BuildTypes)b, eOwner);
			if (c != l) { ++gBuild.div; en_emitDiff(szWho, "canBuild", GC.getBuildInfo((BuildTypes)b).getType(), c, l, gBuild.shown, szBuf); }
		}
	}

	sprintf(szBuf, "[ENABLER/shadow] cities=%d players=%d units=%d plots=%d  canConstruct(%d/%d) canTrain(%d/%d) canCreate(%d/%d) canMaintain(%d/%d) canResearch(%d/%d) canDoCivics(%d/%d) canAcquirePromotion(%d/%d) canBuild(%d/%d) cap:canFoundOnPeaks(%d/%d)  [diverging/checked]",
		iCities, iPlayers, iUnits, iPlots, gConstruct.div, gConstruct.chk, gTrain.div, gTrain.chk, gCreate.div, gCreate.chk,
		gMaintain.div, gMaintain.chk, gResearch.div, gResearch.chk, gCivics.div, gCivics.chk, gPromote.div, gPromote.chk, gBuild.div, gBuild.chk, gCapPeaks.div, gCapPeaks.chk);
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
}
