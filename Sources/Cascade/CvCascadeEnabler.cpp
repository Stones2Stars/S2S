//
//	CvCascadeEnabler -- the #430 "can I?" machine SHADOW HARNESS (enabler.md §1-3). The GENERATE->GATE calc surface now
//	lives in the per-domain static-methods classes it CONSUMES: EnablerKernel (the shared primitive + gate helpers) +
//	TechCascade / BuildingCascade / UnitCascade (the StoneBase CascadingEnabler port). This file is the thin consumer:
//	the [ENABLER] spine domain (logging), the per-turn parity shadow that diffs each cascade's verdict against the live
//	engine gate (canConstruct / canTrain / canResearch / …), and the promotions per-unit shape. Gates by scope:
//	   - CITY-scope:   canConstruct (buildings), canTrain (units), canCreate (projects), canMaintain (processes)
//	   - PLAYER-scope: canResearch (techs), canDoCivics (civics), canHurry (hurries)
//	   - UNIT-scope:   canAcquirePromotion (promotions)      - PLOT-scope: canBuild (builds)
//	   - PLAYER-STATE predicates (not a JSON frontier -- reproduced from game state): canFoundReligion, cap:canFoundOnPeaks
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
#include "CvCascadeEnablerKernel.h"    // EnablerKernel -- the shared GENERATE->GATE primitive + gate helpers
#include "CvCascadeTechCascade.h"      // TechCascade::available
#include "CvCascadeBuildingCascade.h"  // BuildingCascade::buildable
#include "CvCascadeUnitCascade.h"      // UnitCascade::trainable
#include "CvJsonInfo.h"
#include "Repos/InfoRepo.h"
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

// ===================== [ENABLER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The shadow's diff + summary emit EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct gDLL->logMsg) -- the
// CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level. Per-emitter domain (SD_ENABLER),
// one file (Cascade.log). The per-turn summary is split into one COUNTS event + one per-gate SHADOW event (the field cap
// is 16; a single ~24-field line doesn't fit -- event-spine.md drop/redo).
enum EnEvt { ENE_DIFF = 1, ENE_SHADOW, ENE_COUNTS, ENE_PROCDIAG };
enum EnFld
{
	ENF_WHO = 1, ENF_GATE, ENF_TYPE, ENF_CASC, ENF_LEG,   // diff
	ENF_CITIES, ENF_PLAYERS, ENF_UNITS, ENF_PLOTS,        // counts
	ENF_DIV, ENF_CHK,                                      // per-gate summary
	ENF_CAND, ENF_AVAIL, ENF_EDGES, ENF_RESOLVE, ENF_HASTECH   // procdiag (the canMaintain chain decomposition)
};
static const char* en_prefix(int evt)
{
	switch (evt)
	{
	case ENE_DIFF:     return "[ENABLER/diff]";
	case ENE_SHADOW:   return "[ENABLER/shadow]";
	case ENE_COUNTS:   return "[ENABLER/shadow]";
	case ENE_PROCDIAG: return "[ENABLER/procdiag]";
	default:           return "[ENABLER]";
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
	case ENF_CAND:    return "cand";
	case ENF_AVAIL:   return "avail";
	case ENF_EDGES:   return "curEdges";
	case ENF_RESOLVE: return "wealthId";
	case ENF_HASTECH: return "hasCurrency";
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
			if (pUnit->isHasPromotion((PromotionTypes)pr)) EnablerKernel::accumHave(InfoRepo<CvPromotionInfo>::get().get(pr), cand, rem);
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kTeam.isHasTech((TechTypes)t)) EnablerKernel::accumHave(InfoRepo<CvTechInfo>::get().get(t), cand, rem);
		const UnitCombatTypes eUC = pUnit->getUnitCombatType();
		if (eUC != NO_UNITCOMBAT) EnablerKernel::accumHave(InfoRepo<CvUnitCombatInfo>::get().get((int)eUC), cand, rem);

		std::set<int>& promoCand = cand["promotions"];
		const std::set<int>& promoRem = rem["promotions"];
		for (std::set<int>::const_iterator it = promoRem.begin(); it != promoRem.end(); ++it) promoCand.erase(*it);
		// THE PALACE LESSON for promotions (2026-07-02): most combat promos (COMBAT1-5, LEADER, …) have NO tech
		// prereq, so no tech's enables.promotions ever generates them — the enables-frontier structurally
		// under-offers them (the canAcquirePromotion refusal bulk). A promo rooted in NO tech edge anywhere is
		// ALWAYS-unlocked; only enabler-rooted promos are tech-locked. Derived once from the static readJson data.
		static std::set<int> s_enablerRooted;
		static bool s_rootedBuilt = false;
		if (!s_rootedBuilt)
		{
			for (int t = 0; t < GC.getNumTechInfos(); ++t)
				EnablerKernel::addEdge(InfoRepo<CvTechInfo>::get().get(t), "enables.promotions", s_enablerRooted);
			s_rootedBuilt = true;
		}

		CvCascadeEvalCtx ec;
		ec.unit = pUnit; ec.player = &kPlayer; ec.team = &kTeam; ec.plot = pUnit->plot();
		// HOLD the name: CvUnit::getName() returns a CvWString BY VALUE -- keeping only .GetCString() dangles into a
		// destroyed temporary (the blank-who= emit bug, 2026-07-02; CvCity::getName() has the same shape below).
		const CvWString sWho = pUnit->getName();
		const wchar_t* szWho = sWho.GetCString();
		for (int pr = 0; pr < nPromo; ++pr)
		{
			++g.chk;
			// ⛔ SCOPE (2026-07-02): the cascade owns the TECH/enables frontier half only. isPromotionValid's bespoke
			// unit-state rules (qualified/disqualified unitcombat lists, game options, spy/pillage/commander/blend/
			// intercept caps) are engine LOGIC that survives cutover (their DATA moves to JSON at Gate 3 — parked in
			// identity.unitCombats today, unmodeled by the frontier). isPromotionValid(pr, bFree=true) applies exactly
			// that bespoke half (bFree skips ONLY the tech + promotion-line tech gates, CvUnit.cpp:17947) — riding it
			// on BOTH sides isolates the diff to the frontier half (the 4,729-diff noise was the unmodeled half).
			const bool bUnlocked = promoCand.count(pr) != 0
				|| (s_enablerRooted.count(pr) == 0 && promoRem.count(pr) == 0);   // no-enabler promo = always-unlocked (unless obsoleted)
			// EVENT-INJECTION-ONLY mirror (2026-07-02): a promo with NO qualified-unitcombat list (and not
			// forOffset/zeroesXP) is refused by legacy unless FREE (`bValid = bFree` -- CvUnit.cpp:17977: "no CC
			// prereq = only assigned by event or special injection"). The bFree=true bespoke ride passes exactly
			// that clause, so mirror it here (with legacy's free-promotion carve-out) -- the WINTERBORN/SAND_DEVIL
			// affinity-promo over-offer tail.
			const CvPromotionInfo& kPromo = GC.getPromotionInfo((PromotionTypes)pr);
			const bool bEventOnly = kPromo.getNumQualifiedUnitCombatTypes() == 0
				&& !kPromo.isForOffset() && !kPromo.isZeroesXP()
				&& !pUnit->getUnitInfo().getFreePromotions(pr)
				&& !kPlayer.isFreePromotion(pUnit->getUnitType(), (PromotionTypes)pr);
			const bool bCasc = bUnlocked && !bEventOnly
				&& EnablerKernel::requiresMet(InfoRepo<CvPromotionInfo>::get().get(pr), ec)
				&& pUnit->isPromotionValid((PromotionTypes)pr, true);
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
	EnGate gConstruct, gTrain, gCreate, gMaintain, gResearch, gCivics, gPromote, gBuild;
	EnGate gHurry, gFoundRel;
	int iCities = 0, iPlayers = 0, iUnits = 0, iPlots = 0;

	for (int p = 0; p < MAX_PLAYERS && iCities < 8; ++p)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
		// NPC players are SKIPPED (2026-07-02): the NPC build-lockdown domain (stronglyRestricted) is BLOCKED/deferred
		// by ruling, so sampling NPC cities polluted the gate diffs with a domain we deliberately don't model yet
		// (the blank-who canTrain/canMaintain refusal storms). Real civs only.
		if (!kPlayer.isAlive() || kPlayer.isNPC()) continue;
		const CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());

		// (The former cap:canFoundOnPeaks shadow is GONE (2026-07-02): its oracle -- the legacy CvTeam counter --
		// was deleted in the Gate-3 capability cut, and the flipped getter now IS CascadeCapabilities, so the diff
		// had become cascade-vs-cascade. The capability plane's live net is [CAPSHADOW] in CvCascadeCapabilities.)

		// ---- PLAYER-scope gates (canResearch / canDoCivics): one GENERATE/GATE per player ----
		if (iPlayers < 4)
		{
			++iPlayers;
			EnBucketSets candP;
			EnablerKernel::generate(kPlayer, NULL, candP);
			CvCascadeEvalCtx pec;                          // PLAYER-scope eval ctx (no city/plot)
			pec.player = &kPlayer; pec.team = &kTeam;
			std::set<int> availT, availCv;
			TechCascade::available(kPlayer, kTeam, availT);    // TechCascade port (all-techs frontier; replaces the enables-frontier)
			EnablerKernel::gateSet("civics", candP, pec, kPlayer, kTeam, false, availCv);
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
			EnablerKernel::gateSet("hurries", candP, pec, kPlayer, kTeam, false, availHur);
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
				const bool c = EnablerKernel::canFoundReligion(kPlayer);
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
			EnablerKernel::generate(kPlayer, pCity, candC);
			CvCascadeEvalCtx cec;                          // CITY-scope eval ctx
			cec.city = pCity; cec.plot = pCity->plot(); cec.player = &kPlayer; cec.team = &kTeam;   // plot = the CITY plot (2026-07-02 fix: bare HAS_COAST / HAS_FRESHWATER take the plot branch; a NULL plot failed them all)
			// The two per-city building facts (active set + in-vicinity `provides` supply, json §5a) for the projects/
			// processes gateSet requires-eval -- computed from the cascade, not the engine (DEC-calc-zero-ride-in).
			EnablerKernel::wireFacts(pCity, cec);
			std::set<int> avB, avU, avPr, avProc;
			BuildingCascade::buildable(pCity, kPlayer, kTeam, avB);   // BuildingCascade port (all-buildings frontier)
			UnitCascade::trainable(pCity, kPlayer, kTeam, avU);       // UnitCascade port (generate-then-gate)
			EnablerKernel::gateSet("projects",  candC, cec, kPlayer, kTeam, false, avPr);
			EnablerKernel::gateSet("processes", candC, cec, kPlayer, kTeam, false, avProc);
			// [ENABLER/procdiag] -- the canMaintain chain decomposition (map, don't guess): the frontier came out
			// EMPTY while the tech JSONs verifiably carry enables.processes. One emit (first city) pins WHICH link
			// breaks: wealthId<0 = FK resolve; curEdges=0 = the readJson map lost the edge; cand=0 = GENERATE;
			// avail=0 with cand>0 = the gate. Remove once canMaintain reaches parity.
			if (iCities == 1)
			{
				const int iWealthId = GC.getInfoTypeForString("PROCESS_WEALTH", true);
				const int iCurrency = GC.getInfoTypeForString("TECH_CURRENCY", true);
				int iCurEdges = -1;
				if (iCurrency >= 0)
				{
					const CvJsonInfo* jt = InfoRepo<CvTechInfo>::get().get(iCurrency);
					if (jt != NULL)
					{
						std::map<std::string, std::vector<int> >::const_iterator eit = jt->edges.find("enables.processes");
						iCurEdges = (eit == jt->edges.end()) ? 0 : (int)eit->second.size();
					}
				}
				EnBucketSets::const_iterator pcit = candC.find("processes");
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_ENABLER, ENE_PROCDIAG, 1)
					.addI(ENF_CAND, pcit == candC.end() ? -1 : (int)pcit->second.size())
					.addI(ENF_AVAIL, (int)avProc.size())
					.addI(ENF_EDGES, iCurEdges)
					.addI(ENF_RESOLVE, iWealthId)
					.addI(ENF_HASTECH, iCurrency >= 0 && kTeam.isHasTech((TechTypes)iCurrency) ? 1 : 0));
			}
			// HOLD the name: CvCity::getName() returns a CvWString BY VALUE (the blank-who= emit bug -- see above).
			const CvWString sWho = pCity->getName();
			const wchar_t* szWho = sWho.GetCString();
			for (int b = 0; b < nB; ++b)
			{
				++gConstruct.chk; bool c = avB.count(b) != 0, l = pCity->canConstruct((BuildingTypes)b, false, false, false);   // bIgnoreCost=FALSE (2026-07-02): true disabled the productionCost==-1 gate == the spawnOnly/notConstructible semantic the cascade models
				if (c != l) { ++gConstruct.div; en_emitDiff(szWho, "canConstruct", GC.getBuildingInfo((BuildingTypes)b).getType(), c, l, gConstruct.shown); }
			}
			for (int u = 0; u < nU; ++u)
			{
				++gTrain.chk; bool c = avU.count(u) != 0, l = pCity->canTrain((UnitTypes)u, false, false, false);   // ditto -- the oracle must apply the real gate
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
		if (eOwner == NO_PLAYER || !GET_PLAYER(eOwner).isAlive() || GET_PLAYER(eOwner).isNPC()) continue;
		++iPlots;
		const CvPlayer& kOwner = GET_PLAYER(eOwner);
		const CvTeam& kOTeam = GET_TEAM(kOwner.getTeam());
		CvCascadeEvalCtx pec;                          // PLOT-scope eval ctx (the build's target plot)
		pec.plot = pPlot; pec.player = &kOwner; pec.team = &kOTeam;
		CvCascadeEvalFlags bflags; bflags.strictStateReligionForBuild = true;
		// BuildCascade (StoneBase): the FRONTIER is ALL builds (not the enables-frontier), gated by requires.build.
		// ⛔ SCOPE (2026-07-02): the cascade owns ONLY the UNLOCK half — the plot-validity half (CvPlot::canBuild:
		// canHaveImprovement, feature removal, route placement, water) STAYS ENGINE at cutover (enabler.md: "builds
		// stay the per-plot canBuild gate; the cascade subsumes the unlock set"). So the shadow diffs UNLOCK vs
		// UNLOCK: the oracle mirrors CvPlayer::canBuild's unlock block (disabled + obsoleteTech + techPrereq;
		// CvPlayer.cpp:7529-7551) — diffing against the FULL CvPlot::canBuild compared two different questions
		// (the 2,083-diff noise: every not-yet-plot-valid build read as an over-offer). The plot-conditional
		// feature/terrain TECH gates (getFeatureTech / TerrainStructs) live in produces.* data, not requires —
		// they are Gate-3 wiring for the surviving engine gate, outside this frontier.
		// Source-side obsolescence: a build's XML ObsoleteTech is store-inverted to the TECH's `obsoletes.builds`
		// edge (curate_build) — collect the removal set over the owner's held techs (the whole-domain frontier
		// bypasses generate(), so the rem subtraction must happen here; missing it over-offered the whole
		// TRAIL/PATH/PAVED_ROAD obsolete-route class).
		std::set<int> remBld;
		for (int t = 0; t < GC.getNumTechInfos(); ++t)
			if (kOTeam.isHasTech((TechTypes)t))
				EnablerKernel::addEdge(InfoRepo<CvTechInfo>::get().get(t), "obsoletes.builds", remBld);
		std::set<int> availBld;
		for (int b = 0; b < nBld; ++b)
		{
			if (remBld.count(b) != 0) continue;
			const CvJsonInfo* j = InfoRepo<CvBuildInfo>::get().get(b);
			if (EnablerKernel::obsoletedByHeldTech(j, kOTeam)) continue;
			if (j == NULL || j->requiresBuild == NULL || cascadeEvalCondition(j->requiresBuild, pec, bflags)) availBld.insert(b);
		}
		const wchar_t* szWho = kOwner.getName();
		for (int b = 0; b < nBld; ++b)
		{
			const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)b);
			const bool l = !kBuild.isDisabled()
				&& !(kBuild.getObsoleteTech() != NO_TECH && kOTeam.isHasTech(kBuild.getObsoleteTech()))
				&& (kBuild.getTechPrereq() == NO_TECH || kOTeam.isHasTech((TechTypes)kBuild.getTechPrereq()));
			++gBuild.chk; const bool c = availBld.count(b) != 0;
			if (c != l) { ++gBuild.div; en_emitDiff(szWho, "canBuild", kBuild.getType(), c, l, gBuild.shown); }
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
#undef EN_GATE_EMIT
}
