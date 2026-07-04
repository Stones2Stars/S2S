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
#include "CvCascadeAccumulator.h"      // #430 THE FLIP: the SERVING frontier accessors (the net diffs serving-vs-oracle)
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
	// POST-FLIP (2026-07-04 "flip it all"): the composite moved to CascadeAccumulator::enPromotionValid (the
	// SINGLE SOURCE the flipped isPromotionValid serves -- the PALACE lesson / event-injection mirror /
	// bespoke-half ride all live there now); this net diffs the SERVING value vs the intact Legacy oracle.
	(void)kTeam;
	int iLoop;
	for (const CvUnit* pUnit = kPlayer.firstUnit(&iLoop); pUnit != NULL && iUnits < 12; pUnit = kPlayer.nextUnit(&iLoop))
	{
		++iUnits;
		// HOLD the name: CvUnit::getName() returns a CvWString BY VALUE -- keeping only .GetCString() dangles into a
		// destroyed temporary (the blank-who= emit bug, 2026-07-02; CvCity::getName() has the same shape below).
		const CvWString sWho = pUnit->getName();
		const wchar_t* szWho = sWho.GetCString();
		for (int pr = 0; pr < nPromo; ++pr)
		{
			++g.chk;
			const bool bCasc = CascadeAccumulator::enPromotionValid(pUnit, pr);
			const bool bLeg = pUnit->isPromotionValidLegacy((PromotionTypes)pr);
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

		// ---- PLAYER-scope gates: POST-FLIP the SERVING accessors (the cached player frontier) vs the Legacy oracles ----
		if (iPlayers < 4)
		{
			++iPlayers;
			const wchar_t* szWho = kPlayer.getName();
			for (int t = 0; t < nT; ++t)
			{
				// Oracle = canResearchLegacy (researchable NOW): the all-techs+requires.build set is "prereqs held now".
				++gResearch.chk; bool c = CascadeAccumulator::enResearch(&kPlayer, t), l = kPlayer.canResearchLegacy((TechTypes)t);
				if (c != l) { ++gResearch.div; en_emitDiff(szWho, "canResearch", GC.getTechInfo((TechTypes)t).getType(), c, l, gResearch.shown); }
			}
			for (int cv = 0; cv < nC; ++cv)
			{
				++gCivics.chk; bool c = CascadeAccumulator::enCivic(&kPlayer, cv), l = kPlayer.canDoCivicsLegacy((CivicTypes)cv);
				if (c != l) { ++gCivics.div; en_emitDiff(szWho, "canDoCivics", GC.getCivicInfo((CivicTypes)cv).getType(), c, l, gCivics.shown); }
			}
			for (int hu = 0; hu < nHur; ++hu)
			{
				++gHurry.chk;
				const bool c = CascadeAccumulator::enHurry(&kPlayer, hu);
				const bool l = kPlayer.canHurryLegacy((HurryTypes)hu);
				if (c != l) { ++gHurry.div; en_emitDiff(szWho, "canHurry", GC.getHurryInfo((HurryTypes)hu).getType(), c, l, gHurry.shown); }
			}
			{
				++gFoundRel.chk;
				const bool c = CascadeAccumulator::enFoundReligion(&kPlayer);
				const bool l = kPlayer.canFoundReligionLegacy();
				if (c != l) { ++gFoundRel.div; en_emitDiff(szWho, "canFoundReligion", "", c, l, gFoundRel.shown); }
			}
		}

		// ---- UNIT-scope gate (canAcquirePromotion): per-unit GENERATE->GATE, sample-capped across players ----
		en_shadowPromotions(kPlayer, kTeam, nPromo, gPromote, iUnits);

		// ---- CITY-scope gates: POST-FLIP the SERVING accessors (the cached city frontier -- exactly what the
		// flipped canConstruct/canTrain/canCreate/canMaintain return) vs the intact Legacy oracles ----
		int iLoop;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL && iCities < 8; pCity = kPlayer.nextCity(&iLoop))
		{
			++iCities;
			// HOLD the name: CvCity::getName() returns a CvWString BY VALUE (the blank-who= emit bug -- see above).
			const CvWString sWho = pCity->getName();
			const wchar_t* szWho = sWho.GetCString();
			for (int b = 0; b < nB; ++b)
			{
				++gConstruct.chk; bool c = CascadeAccumulator::enConstruct(pCity, b), l = pCity->canConstructLegacy((BuildingTypes)b, false, false, false);   // bIgnoreCost=FALSE (2026-07-02): true disabled the productionCost==-1 gate == the spawnOnly/notConstructible semantic the cascade models
				if (c != l) { ++gConstruct.div; en_emitDiff(szWho, "canConstruct", GC.getBuildingInfo((BuildingTypes)b).getType(), c, l, gConstruct.shown); }
			}
			for (int u = 0; u < nU; ++u)
			{
				++gTrain.chk; bool c = CascadeAccumulator::enTrain(pCity, u), l = pCity->canTrainLegacy((UnitTypes)u, false, false, false);   // ditto -- the oracle must apply the real gate
				if (c != l) { ++gTrain.div; en_emitDiff(szWho, "canTrain", GC.getUnitInfo((UnitTypes)u).getType(), c, l, gTrain.shown); }
			}
			for (int pr = 0; pr < nP; ++pr)
			{
				++gCreate.chk; bool c = CascadeAccumulator::enCreate(pCity, pr), l = pCity->canCreateLegacy((ProjectTypes)pr, false, false);
				if (c != l) { ++gCreate.div; en_emitDiff(szWho, "canCreate", GC.getProjectInfo((ProjectTypes)pr).getType(), c, l, gCreate.shown); }
			}
			for (int pc = 0; pc < nProc; ++pc)
			{
				++gMaintain.chk; bool c = CascadeAccumulator::enMaintain(pCity, pc), l = pCity->canMaintainLegacy((ProcessTypes)pc);
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
		// POST-FLIP: the SERVING unlock accessor (CascadeAccumulator::enBuildUnlocked -- the rem-set +
		// target-side obsolescence + requires.build vs the plot, exactly what CvPlayer::canBuild's flipped
		// unlock half serves) vs the Legacy unlock triple. The plot-validity half stays engine (the scope
		// ruling 2026-07-02); the feature/terrain tech gates are Gate-3 wiring outside this frontier.
		const wchar_t* szWho = kOwner.getName();
		for (int b = 0; b < nBld; ++b)
		{
			const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)b);
			const bool l = !kBuild.isDisabled()
				&& !(kBuild.getObsoleteTech() != NO_TECH && kOTeam.isHasTech(kBuild.getObsoleteTech()))
				&& (kBuild.getTechPrereq() == NO_TECH || kOTeam.isHasTech((TechTypes)kBuild.getTechPrereq()));
			++gBuild.chk; const bool c = CascadeAccumulator::enBuildUnlocked(&kOwner, b, pPlot);
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
