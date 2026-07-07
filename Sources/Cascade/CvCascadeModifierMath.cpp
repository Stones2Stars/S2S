//
//	CvCascadeModifierMath -- the #430 modifier machine SHADOW HARNESS + the [MODIFIER] spine domain. See the header + the
//	build plan. The CALCULATORS were split out into per-package static-methods classes (the single-source law,
//	patterns.md): MMKernel (the leaf helpers), PercentStack, YieldBasePackages, BuildingPackage, YieldRate, CommerceCalc.
//	This file is now the thin CONSUMER of those calc surfaces -- the parity shadow (harness, NOT calc) + its logging domain.
//
//	⚠ Scale: a `percent` deposit is stored ×100 (readJson's blanket ×100), but the modifier stack is HUMAN percent (the
//	legacy modifier is 100 + Σ whole percents), so each deposit contributes value100/100.
//	⏳ Increment-1 scope: city/area/empire-scope percents from active buildings, empire buildings, civics, traits. The
//	PURE_TRAITS filter, civic building-keyed percents, and projects are follow-ons (modifier-machine.md). Divergences
//	from not-yet-modelled sources (events) and deferred predicates (HAS_POWER/HAS_BONUS, currently ignored→always-true)
//	are EXPECTED and surfaced by the shadow (validation.md: a divergence is a gap mapped to a named source).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeModifierMath.h"
#include "CvCascadePercentStack.h"     // MMBreak + PercentStack::percentStack
#include "CvCascadeYieldRate.h"        // YieldRate::yieldRate100
#include "CvCascadeAccumulator.h"      // the modifier scope accumulator -- the [SLOT] shadow's subject
#include "CvCascadeYieldBasePackages.h" // YieldBasePackages::specialist -- the accepted-diff decomposition (rate diff)
#include "CvCascadeBuildingPackage.h"  // BuildingPackage::buildingFlat -- the [SLOT] EXTRA-component fresh pair
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::commerceRate100 + the commerce channel table
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM -- the eval ctx's team
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::wireOperatingBuildings -- the standing cascade operating-buildings cache
#include "CvCascadeCapabilities.h"    // CascadeCapabilities::shadowFlush -- the in-body capability-getter shadow
#include "CvJsonInfo.h"               // the mapped info (requiresOperate/dormantTriggers) -- the dorm-attribution diagnostic
#include "Repos/InfoRepo.h"           // InfoRepo<CvBuildingInfo> -- ditto
#include "Infos/CvBuildingInfo.h"     // GC.getBuildingInfo().getType() (dorm sample) -- was a latent unity-batch ride-along
#include "Engine/CvPlot.h"            // per-plot [SLOT/plot] probe: getYield + the substrate type reads
#include "Infos/CvTerrainInfo.h"      // .getType() strings for the per-plot attribution sample
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Infos/CvRouteInfo.h"
#include "Infos/CvBonusInfo.h"
#include "CvCascadePerfCount.h"       // CascadePerf -- the [MODIFIER/perf] census (ditto)
#include "CvCascadeReadJson.h"        // cascadeReadJsonStats -- re-surface the dark load-time probe stats
#include "CvJsonParse.h"              // jsonUnresolvedIds -- re-surface the dark load-time FK misses
#include "CvEventSpine.h"             // the #430 dispatch spine -- the shadow diff rides it (SD_MODIFIER), NOT direct gDLL->logMsg
#include "CvCascadeWellbeing.h"       // the §2b wellbeing channel -- its shadow rides this harness's city loop
#include "CvCascadeScalarChannels.h"  // the increment-F scalar channels -- their [MODIFIER/scalar] slot net rides the same loop
#include <set>
#include <string>

// ===================== [MODIFIER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The percent-stack shadow's diff + summary emit EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct
// gDLL->logMsg) -- the CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level.
// Per-emitter domain (SD_MODIFIER), one file (Cascade.log).
enum MdEvt { MDE_DIFF = 1, MDE_SHADOW, MDE_RATE, MDE_DORM, MDE_REPO, MDE_PERF, MDE_SLOT, MDE_PLOTDIFF, MDE_WELLBEING, MDE_SCALAR };
enum MdFld
{
	MDF_WHO = 1, MDF_CHANNEL, MDF_CASC, MDF_BC, MDF_BA, MDF_BE, MDF_CIV, MDF_TR,   // diff: cascade buckets
	MDF_LEG, MDF_BLD, MDF_BON, MDF_POW, MDF_EVT, MDF_PLY, MDF_CAP,                 // diff: legacy sub-terms
	MDF_CHECKED, MDF_DIVERGING,                                                     // summary
	MDF_RATEC, MDF_RATEL,                                                           // §1 rate diff: cascade vs legacy ×100
	MDF_PRESENT, MDF_ACTIVE, MDF_DORMOP, MDF_DORMTRIG, MDF_ENGDISABLED, MDF_SAMPLE, // dorm attribution (MDE_DORM)
	MDF_TOTAL, MDF_MAPPED, MDF_WDEPOSITS, MDF_WOPERATE, MDF_WTRIGGERS,              // repo census (MDE_REPO)
	MDF_FILES2, MDF_ENTITIES2,                                                      // the stashed load-probe stats
	MDF_SPECC, MDF_SPECL,                                                           // specialist sub-terms (rate diff)
	MDF_OPERATING_BUILDINGS_RECOMPUTED, MDF_OPERATING_BUILDINGS_CACHE_HITS, MDF_YRN, MDF_PSN, MDF_CRN, MDF_CEN, MDF_ACCN,          // perf: call counts (+accumulator refreshes)
	MDF_OPERATING_BUILDINGS_RECOMPUTE_MS, MDF_YRMS, MDF_PSMS, MDF_CRMS,                                      // perf: stopwatch ms (x10 int)
	MDF_UNRES, MDF_UNRESIDS,                                                        // load-time FK misses, re-surfaced live
	MDF_PLOT_S, MDF_PLOT_C, MDF_EMP_S, MDF_EMP_C, MDF_SPEC_S, MDF_SPEC_C,          // [SLOT] yield-leg component pairs (slot vs fresh calc)
	MDF_EXTRA_S, MDF_EXTRA_C, MDF_PCT_S, MDF_PCT_C,
	MDF_YC_S, MDF_YC_C, MDF_CSPEC_S, MDF_CSPEC_C, MDF_CBASE_S, MDF_CBASE_C,        // [SLOT] commerce-leg component pairs
	MDF_CPCT_S, MDF_CPCT_C,
	MDF_PX, MDF_PY,                                                                 // [SLOT/plotdiff]: the diverging plot's coords (pair = plotS/plotC re-used per plot)
	MDF_ACC_P, MDF_ACC_T, MDF_ACC_C,                                                // [SLOT/plotdiff]: the engine's SERIALIZED improvement-yield accumulators (player/team/city) for the plot's improvement
	MDF_HAP_C, MDF_HAP_L, MDF_UNH_C, MDF_UNH_L,                                     // [MODIFIER/wellbeing]: the four verdict pairs (cascade vs legacy)
	MDF_GOOD_C, MDF_GOOD_L, MDF_BAD_C, MDF_BAD_L,
	MDF_WBN, MDF_WBMS,                                                              // perf: wellbeing computes + ms×10
	MDF_GPB_S, MDF_GPB_C, MDF_GPM_S, MDF_GPM_C, MDF_DEF_S, MDF_DEF_C,              // [MODIFIER/scalar]: increment-F slot-vs-calc pairs
	MDF_MNT_S, MDF_MNT_C, MDF_TRD_S, MDF_TRD_C,
	MDF_TURNNO, MDF_TURNMS,                                                         // perf: game turn + flush-to-flush WALL time (the headline turn-time number, DEC-turn-time-is-king)
	MDF_LRMS, MDF_LWBMS,                                                            // perf: the LEGACY-side pair ms (vs yieldRateMs+commerceRateMs / wbComputeMs -- the pre-cut comparison)
	MDF_SCGPB, MDF_SCGPM, MDF_SCDEF, MDF_SCMNT,                                     // perf: flipped scalar getter READ counts
	MDF_SCREF, MDF_SCSREF, MDF_SCMS,                                                // perf: scalar refresh counts (SCALAR / SCALARSPEC) + refresh ms×10
	MDF_AUTON, MDF_AUTOMS,                                                          // perf: the AUTOMATION window (autoMission calls + accumulated ms×10)
	MDF_CE_OTHER, MDF_CE_RATES, MDF_CE_WB, MDF_CE_SC, MDF_CE_OPERATING_BUILDINGS,                 // perf: the condEval CALLER split (the 6.8M-outlier attribution)
	MDF_CE_FRB, MDF_CE_FRU, MDF_CE_FRPP, MDF_CE_FRP, MDF_CE_CANB, MDF_CE_PROMO,
	MDF_FRB_N, MDF_FRB_MS, MDF_FRU_N, MDF_FRU_MS, MDF_FRPP_N, MDF_FRPP_MS,          // perf: the frontier fill counts + ms×10 (the flip-era no-ms-bucket hole)
	MDF_FRP_N, MDF_FRP_MS, MDF_PRM_N, MDF_PRM_MS,
	MDF_HEALCHECKED, MDF_HEALDIVERGING                                             // stashed [READJSON/healdiff] shadow counts, re-surfaced live
};
static const char* mm_prefix(int evt)
{
	switch (evt)
	{
	case MDE_DIFF:   return "[MODIFIER/diff]";
	case MDE_SHADOW: return "[MODIFIER/shadow]";
	case MDE_RATE:   return "[MODIFIER/rate]";
	case MDE_DORM:   return "[MODIFIER/dorm]";
	case MDE_REPO:   return "[MODIFIER/repo]";
	case MDE_PERF:   return "[MODIFIER/perf]";
	case MDE_SLOT:   return "[MODIFIER/slot]";
	case MDE_PLOTDIFF: return "[MODIFIER/plotdiff]";
	case MDE_WELLBEING: return "[MODIFIER/wellbeing]";
	case MDE_SCALAR: return "[MODIFIER/scalar]";
	default:         return "[MODIFIER]";
	}
}
static const char* mm_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case MDF_WHO:       *peType = SFT_WSTR; return "who";
	case MDF_CHANNEL:   *peType = SFT_STR;  return "channel";
	case MDF_CASC:      return "casc";
	case MDF_BC:        return "bC";
	case MDF_BA:        return "bA";
	case MDF_BE:        return "bE";
	case MDF_CIV:       return "civ";
	case MDF_TR:        return "tr";
	case MDF_LEG:       return "leg";
	case MDF_BLD:       return "bld";
	case MDF_BON:       return "bon";
	case MDF_POW:       return "pow";
	case MDF_EVT:       return "evt";
	case MDF_PLY:       return "ply";
	case MDF_CAP:       return "cap";
	case MDF_CHECKED:   return "checked";
	case MDF_DIVERGING: return "diverging";
	case MDF_RATEC:     return "casc100";
	case MDF_RATEL:     return "leg100";
	case MDF_PRESENT:     return "present";
	case MDF_ACTIVE:      return "active";
	case MDF_DORMOP:      return "dormOperate";
	case MDF_DORMTRIG:    return "dormTrigger";
	case MDF_ENGDISABLED: return "engDisabled";
	case MDF_SAMPLE:      *peType = SFT_STR; return "sample";
	case MDF_TOTAL:       return "total";
	case MDF_MAPPED:      return "mapped";
	case MDF_WDEPOSITS:   return "withDeposits";
	case MDF_WOPERATE:    return "withOperate";
	case MDF_WTRIGGERS:   return "withTriggers";
	case MDF_FILES2:      return "probeFiles";
	case MDF_ENTITIES2:   return "probeEntities";
	case MDF_SPECC:       return "specCasc";
	case MDF_SPECL:       return "specLeg";
	case MDF_OPERATING_BUILDINGS_RECOMPUTED:       return "operatingBuildingsRecomputed";
	case MDF_OPERATING_BUILDINGS_CACHE_HITS:    return "operatingBuildingsCacheHits";
	case MDF_YRN:         return "yieldRate";
	case MDF_PSN:         return "pctStack";
	case MDF_CRN:         return "commerceRate";
	case MDF_CEN:         return "condEval";
	case MDF_ACCN:        return "accRefresh";
	case MDF_OPERATING_BUILDINGS_RECOMPUTE_MS:     return "operatingBuildingsRecomputeMsX10";
	case MDF_YRMS:        return "yieldRateMsX10";
	case MDF_PSMS:        return "pctStackMsX10";
	case MDF_CRMS:        return "commerceRateMsX10";
	case MDF_UNRES:       return "unresolvedFks";
	case MDF_UNRESIDS:    *peType = SFT_STR; return "unresolvedSample";
	case MDF_PLOT_S:      return "plotS";
	case MDF_PLOT_C:      return "plotC";
	case MDF_EMP_S:       return "empS";
	case MDF_EMP_C:       return "empC";
	case MDF_SPEC_S:      return "specS";
	case MDF_SPEC_C:      return "specC";
	case MDF_EXTRA_S:     return "extraS";
	case MDF_EXTRA_C:     return "extraC";
	case MDF_PCT_S:       return "pctS";
	case MDF_PCT_C:       return "pctC";
	case MDF_YC_S:        return "ycS";
	case MDF_YC_C:        return "ycC";
	case MDF_CSPEC_S:     return "cspecS";
	case MDF_CSPEC_C:     return "cspecC";
	case MDF_CBASE_S:     return "cbaseS";
	case MDF_CBASE_C:     return "cbaseC";
	case MDF_CPCT_S:      return "cpctS";
	case MDF_CPCT_C:      return "cpctC";
	case MDF_PX:          return "x";
	case MDF_PY:          return "y";
	case MDF_ACC_P:       return "accPlayer";
	case MDF_ACC_T:       return "accTeam";
	case MDF_ACC_C:       return "accCity";
	case MDF_HAP_C:       return "happyC";
	case MDF_HAP_L:       return "happyL";
	case MDF_UNH_C:       return "unhappyC";
	case MDF_UNH_L:       return "unhappyL";
	case MDF_GOOD_C:      return "goodC";
	case MDF_GOOD_L:      return "goodL";
	case MDF_BAD_C:       return "badC";
	case MDF_BAD_L:       return "badL";
	case MDF_WBN:         return "wbN";
	case MDF_WBMS:        return "wbMsX10";
	case MDF_TURNNO:      return "turn";
	case MDF_TURNMS:      return "turnMsX10";
	case MDF_LRMS:        return "legacyRateMsX10";
	case MDF_LWBMS:       return "legacyWbMsX10";
	case MDF_SCGPB:       return "scGpBaseReads";
	case MDF_SCGPM:       return "scGpModReads";
	case MDF_SCDEF:       return "scDefReads";
	case MDF_SCMNT:       return "scMaintReads";
	case MDF_SCREF:       return "scRefresh";
	case MDF_SCSREF:      return "scSpecRefresh";
	case MDF_SCMS:        return "scRefreshMsX10";
	case MDF_AUTON:       return "autoMissions";
	case MDF_AUTOMS:      return "autoMissionMsX10";
	case MDF_GPB_S:       return "gpBaseS";
	case MDF_GPB_C:       return "gpBaseC";
	case MDF_GPM_S:       return "gpModS";
	case MDF_GPM_C:       return "gpModC";
	case MDF_DEF_S:       return "defS";
	case MDF_DEF_C:       return "defC";
	case MDF_MNT_S:       return "maintS";
	case MDF_MNT_C:       return "maintC";
	case MDF_TRD_S:       return "tradeS";
	case MDF_TRD_C:       return "tradeC";
	case MDF_CE_OTHER:    return "ceOther";
	case MDF_CE_RATES:    return "ceRates";
	case MDF_CE_WB:       return "ceWb";
	case MDF_CE_SC:       return "ceScalars";
	case MDF_CE_OPERATING_BUILDINGS:    return "ceOperatingBuildings";
	case MDF_CE_FRB:      return "ceFrontB";
	case MDF_CE_FRU:      return "ceFrontU";
	case MDF_CE_FRPP:     return "ceFrontPP";
	case MDF_CE_FRP:      return "ceFrontP";
	case MDF_CE_CANB:     return "ceCanBuild";
	case MDF_CE_PROMO:    return "cePromo";
	case MDF_FRB_N:       return "frontBFills";
	case MDF_FRB_MS:      return "frontBMsX10";
	case MDF_FRU_N:       return "frontUFills";
	case MDF_FRU_MS:      return "frontUMsX10";
	case MDF_FRPP_N:      return "frontPPFills";
	case MDF_FRPP_MS:     return "frontPPMsX10";
	case MDF_FRP_N:       return "frontPFills";
	case MDF_FRP_MS:      return "frontPMsX10";
	case MDF_PRM_N:       return "promoFills";
	case MDF_PRM_MS:      return "promoMsX10";
	case MDF_HEALCHECKED:   return "healDiffChecked";
	case MDF_HEALDIVERGING: return "healDiffDiverging";
	default:            return NULL;
	}
}
static void mm_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_MODIFIER, mm_prefix, "Cascade.log", mm_field); s_reg = true; }
}

void cvCascadeModifierShadow()
{
	// Emit EVERY end-turn (gated by gPlayerLogLevel) -- NOT a one-shot, so the modifier diff is re-capturable each
	// turn during iterative validation (the one-shot needed a save reload to re-arm). Free when gPlayerLogLevel<1.
	if (gPlayerLogLevel < 1) return;
	mm_registerDomain();
	// ANTI-MEMO-SKEW (2026-07-02): the turn memos may hold values frozen from EARLY-turn calls (the getter
	// instrument); comparing those against END-of-turn legacy showed as false divergences. Recompute fresh for
	// the shadow sweep -- still memoized WITHIN the sweep.
	YieldRate::memoClear();   // the calculator-oracle's turn memo only -- the operating buildings are a standing event-correct cache now (no anti-skew reset needed)
	CascadeCapabilities::shadowFlush();   // #430 wiring step 1: flush the in-body capability-getter shadow (per turn)

	// [MODIFIER/repo] -- BUILDING REPO CENSUS (2026-07-02, the Orwell bar): decisive on the one-cause hypothesis
	// (repo unmapped => d==NULL everywhere => bC=0 AND zero dorms). The load-time [READJSON] burst is currently
	// dark (gPlayerLogLevel is 0 during doPostLoadCaching), so the census re-emits per turn where logging is live.
	{
		int nMapped = 0, nDep = 0, nOp = 0, nTrig = 0;
		const int nTot = GC.getNumBuildingInfos();
		for (int b = 0; b < nTot; ++b)
		{
			const CvJsonInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
			if (d == NULL) continue;
			++nMapped;
			if (d->getModifiers() != NULL && !d->getModifiers()->empty()) ++nDep;
			if (d->requiresOperate() != NULL) ++nOp;
			if (!d->dormantTriggers().empty()) ++nTrig;
		}
		// + the stashed probe stats: what the dark load-time [READJSON] burst saw (files found / entities parsed /
		// the dataDir). files>0 with mapped=0 convicts a post-map clear or a duplicate-singleton read; files<=0
		// convicts the dataDir scan (path shown verbatim in `sample`). -1 = the probe never ran.
		int iFiles = 0, iEnt = 0;
		const std::string& sDir = cascadeReadJsonStats(false, iFiles, iEnt, std::string());
		// The load-time FK-unresolved set, re-surfaced LIVE (the [READJSON/unresolved-fk] burst is dark at load --
		// exactly how the pre-menu-map bug hid: every PROCESS_ edge silently dropped with the misses visible only
		// in the window nobody can read). unresolvedFks>0 after a clean load is ALWAYS a bug: a data typo or a
		// map-before-registration ordering hole.
		const std::set<std::string>& unres = jsonUnresolvedIds();
		std::string sUnres;
		for (std::set<std::string>::const_iterator uit = unres.begin(); uit != unres.end() && sUnres.size() < 96; ++uit)
		{
			if (!sUnres.empty()) sUnres += ",";
			sUnres += *uit;
		}
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_REPO, 1)
			.addI(MDF_TOTAL, nTot).addI(MDF_MAPPED, nMapped).addI(MDF_WDEPOSITS, nDep)
			.addI(MDF_WOPERATE, nOp).addI(MDF_WTRIGGERS, nTrig)
			.addI(MDF_FILES2, iFiles).addI(MDF_ENTITIES2, iEnt).addStr(MDF_SAMPLE, sDir.c_str())
			.addI(MDF_UNRES, (int)unres.size()).addStr(MDF_UNRESIDS, sUnres.c_str()));
	}

	const char* aszChannel[NUM_YIELD_TYPES] = { "food", "production", "commerce" };   // indexed by the YieldTypes enum
	int iChecked = 0, iDiverging = 0, iShown = 0;
	int iRateDiverging = 0, iRateShown = 0;   // the §1 holistic rate diff (YieldRate::yieldRate100 vs getYieldRate100)
	int iSlotChecked = 0, iSlotDiverging = 0, iSlotShown = 0;   // the ACCUMULATOR vs its calculator oracle ([SLOT])
	int iPlotDiffShown = 0;   // the per-plot attribution probe ([MODIFIER/plotdiff]) -- its own cap
	// Sample ACROSS EMPIRES (a per-player city cap, NOT a global one): traits/civics/religion are player-level, so
	// 1-empire sampling is "a recipe for disaster" (owner ruling 2026-06-30) -- it misses every other civ's divergence.
	const int MM_CITIES_PER_PLAYER = 2;

	for (int p = 0; p < MAX_PLAYERS; ++p)
	{
		const CvPlayer& player = GET_PLAYER((PlayerTypes)p);
		if (!player.isAlive()) continue;
		int iLoop, iCityN = 0;
		for (const CvCity* pCity = player.firstCity(&iLoop); pCity != NULL && iCityN < MM_CITIES_PER_PLAYER; pCity = player.nextCity(&iLoop))
		{
			++iCityN;
			for (int y = 0; y < NUM_YIELD_TYPES; ++y)
			{
				++iChecked;
				const YieldTypes eY = (YieldTypes)y;
				MMBreak bk;
				const int iCascade = PercentStack::percentStack(aszChannel[y], pCity, bk);
				const int iLegacy = pCity->getBaseYieldRateModifier(eY);
				if (iCascade != iLegacy)
				{
					++iDiverging;
					if (iShown < 40)
					{
						// 1b attribution: cascade buckets vs the legacy sub-terms (getBaseYieldRateModifier's parts,
						// CvCity.cpp:11174). The area term is the derivable residual (leg total − the parts shown).
						const int legBld = pCity->getBuildingYieldModifier(eY);
						const int legBon = pCity->getBonusYieldRateModifier(eY);
						const int legPow = pCity->isPower() ? pCity->getPowerYieldRateModifier(eY) : 0;
						const int legEvt = pCity->getYieldRateModifier(eY);
						const int legPly = player.getYieldRateModifier(eY);
						const int legCap = pCity->isCapital() ? player.getCapitalYieldRateModifier(eY) : 0;
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_DIFF, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, aszChannel[y])
							.addI(MDF_CASC, iCascade).addI(MDF_BC, bk.bCity).addI(MDF_BA, bk.bArea).addI(MDF_BE, bk.bEmpire)
							.addI(MDF_CIV, bk.civic).addI(MDF_TR, bk.trait)
							.addI(MDF_LEG, iLegacy).addI(MDF_BLD, legBld).addI(MDF_BON, legBon).addI(MDF_POW, legPow)
							.addI(MDF_EVT, legEvt).addI(MDF_PLY, legPly).addI(MDF_CAP, legCap));
						++iShown;
					}
				}

				// §1 RATE shadow: the full assembled rate vs legacy getYieldRate100 -- the HOLISTIC modifier diff (the
				// real verification, modifier-machine §0: judged after the WHOLE calc is in, port-fidelity vs StoneBase).
				CvCascadeEvalCtx rec;
				rec.city = pCity; rec.plot = pCity->plot(); rec.player = &player; rec.team = &GET_TEAM(player.getTeam());
				EnablerKernel::wireOperatingBuildings(pCity, rec);   // the STANDING cascade operating buildings (active set + vicinity provides)

				// [MODIFIER/dorm] -- DORMANCY ATTRIBUTION (2026-07-02, the Orwell bar: emit before hypothesising).
				// Re-derives each present building's cascade dorm verdict WITH its cause (operate-failed vs
				// trigger-dormed) and diffs against the engine's disabled verdict at the comparison boundary.
				// One line per sampled city (y==0 only, so once not thrice); samples list DISAGREEING buildings.
				if (y == 0)
				{
					CvCascadeEvalCtx recOp = rec; recOp.activeBuildings = NULL;   // mirror recomputeOperatingBuildingsInto's operate ctx
					CvCascadeEvalFlags dormFlags;
					int nPresent = 0, nDormOp = 0, nDormTrig = 0, nEngDisabled = 0;
					std::string sSample;
					for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
					{
						if (!pCity->hasBuilding((BuildingTypes)b)) continue;
						++nPresent;
						const CvJsonInfo* jb = InfoRepo<CvBuildingInfo>::get().get(b);
						bool bDormOp = (jb != NULL && jb->requiresOperate() != NULL && !cascadeEvalCondition(jb->requiresOperate(), recOp, dormFlags));
						bool bDormTrig = false;
						if (!bDormOp && jb != NULL)
						{
							const std::vector<int>& dorm = jb->dormantTriggers();
							for (size_t i = 0; i < dorm.size(); ++i)
								if (pCity->hasBuilding((BuildingTypes)dorm[i])) { bDormTrig = true; break; }
						}
						if (bDormOp) ++nDormOp;
						if (bDormTrig) ++nDormTrig;
						const bool bEngActive = pCity->isActiveBuilding((BuildingTypes)b);
						if (!bEngActive) ++nEngDisabled;
						const bool bCascActive = !bDormOp && !bDormTrig;
						if (bCascActive != bEngActive && sSample.size() < 360)   // DISAGREEMENTS only, capped
						{
							if (!sSample.empty()) sSample += "|";
							sSample += GC.getBuildingInfo((BuildingTypes)b).getType();
							sSample += bCascActive ? ":engOnlyDorm" : (bDormOp ? ":cascDormOperate" : ":cascDormTrigger");
						}
					}
					eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_DORM, 1)
						.addWStr(MDF_WHO, pCity->getName().GetCString())
						.addI(MDF_PRESENT, nPresent).addI(MDF_ACTIVE, rec.activeBuildings != NULL ? (int)rec.activeBuildings->size() : 0)
						.addI(MDF_DORMOP, nDormOp).addI(MDF_DORMTRIG, nDormTrig).addI(MDF_ENGDISABLED, nEngDisabled)
						.addStr(MDF_SAMPLE, sSample.c_str()));
				}
				const long cascRate = YieldRate::yieldRate100(aszChannel[y], eY, pCity, rec);
				int legRate;
				{
					PerfAccumTimer perfLeg(CascadePerf::legacyRateMs);   // the cascade-vs-legacy pair timing (pre-cut window)
					legRate = pCity->getYieldRate100Legacy(eY);   // post-flip: the getter returns the SLOT; the Legacy sibling is the oracle
				}
				// [SLOT] -- the ACCUMULATOR (modifier-substrate.md) vs the fresh CALCULATOR (its oracle): a
				// divergence names a dirty-mapping hole (an event the coarse hooks missed this turn).
				{
					++iSlotChecked;
					const long slotRate = CascadeAccumulator::yieldRate100(pCity, eY);
					if (slotRate != cascRate)
					{
						++iSlotDiverging;
						if (iSlotShown < 40)
						{
							// COMPONENT-DECOMPOSED pairs (modifier-substrate.md next-attribution): each standing slot
							// vs its fresh calculator package on the same ctx, so the diverging component NAMES itself
							// (a stale slot = a dirty-mapping hole in that component's hooks). Slot side = the standing
							// state acc_ensure just served; calc side = the fresh package. plots is the live CvPlot-cache
							// pull vs the basePlot package -- the one term the two sides source differently by design.
							const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
							const CascadePlayerScope& psn = player.m_cascadePlayerScope;
							MMBreak bkS;
							const int plotS = pCity->getPlotYield(eY);
							const int plotC = (int)YieldBasePackages::basePlot(aszChannel[y], eY, pCity, rec);
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SLOT, 1)
								.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, aszChannel[y])
								.addI(MDF_RATEC, (int)slotRate).addI(MDF_RATEL, (int)cascRate)
								.addI(MDF_PLOT_S, plotS).addI(MDF_PLOT_C, plotC)
								.addI(MDF_EMP_S, (int)(psn.yFlatFreeCity[y] + (player.isGoldenAge() ? std::max(0L, psn.yFlatGoldenAge[y]) : 0)))
								.addI(MDF_EMP_C, YieldBasePackages::freeCity(aszChannel[y], player, rec) + YieldBasePackages::goldenAge(aszChannel[y], player, rec))
								.addI(MDF_SPEC_S, (int)st.ySpec[y])
								.addI(MDF_SPEC_C, YieldBasePackages::specialist(aszChannel[y], pCity, rec))
								.addI(MDF_EXTRA_S, (int)st.yExtra100[y])
								.addI(MDF_EXTRA_C, (int)BuildingPackage::buildingFlat(aszChannel[y], pCity, rec))
								.addI(MDF_PCT_S, (int)std::max(0L, 100 + st.yPctCity[y]))
								.addI(MDF_PCT_C, PercentStack::percentStack(aszChannel[y], pCity, bkS)));
							++iSlotShown;

							// [MODIFIER/plotdiff] -- the NEXT attribution level when the plot pair diverges: per worked
							// plot, the engine's CvPlot cache value vs the SAME per-plot package basePlot sums
							// (basePlotOne, single-source). The substrate sample string names the source class
							// (terrain/feature/improvement/route/bonus/centre) without a follow-up sweep.
							if (plotS != plotC)
							{
								for (int iPI = 0; iPI < NUM_CITY_PLOTS && iPlotDiffShown < 24; ++iPI)
								{
									const CvPlot* pp = pCity->getCityIndexPlot(iPI);
									if (pp == NULL || !pCity->isWorkingPlot(pp)) continue;
									const int ppS = pp->getYield(eY);
									const int ppC = YieldBasePackages::basePlotOne(aszChannel[y], eY, pCity, pp, rec);
									if (ppS == ppC) continue;
									std::string sPlot;
									if (pp->getTerrainType() != NO_TERRAIN)         sPlot += GC.getTerrainInfo(pp->getTerrainType()).getType();
									if (pp->getFeatureType() != NO_FEATURE)       { sPlot += "|"; sPlot += GC.getFeatureInfo(pp->getFeatureType()).getType(); }
									if (pp->getImprovementType() != NO_IMPROVEMENT) { sPlot += "|"; sPlot += GC.getImprovementInfo(pp->getImprovementType()).getType(); }
									if (pp->getRouteType() != NO_ROUTE)           { sPlot += "|"; sPlot += GC.getRouteInfo(pp->getRouteType()).getType(); }
									if (pp->getBonusType(player.getTeam()) != NO_BONUS) { sPlot += "|"; sPlot += GC.getBonusInfo(pp->getBonusType(player.getTeam())).getType(); }
									if (pp == pCity->plot())                        sPlot += "|CENTRE";
									// The engine's SERIALIZED improvement-yield accumulators for this plot's improvement --
									// player (civic/trait/building-global writers), team (tech/Python-event writers), city
									// (building city-scope writer). A value here with NO live data source backing it is the
									// history-polluted-accumulator class named by the numbers, not asserted.
									int accP = 0, accT = 0, accC = 0;
									if (pp->getImprovementType() != NO_IMPROVEMENT)
									{
										accP = player.getImprovementYieldChange(pp->getImprovementType(), eY);
										accT = GET_TEAM(player.getTeam()).getImprovementYieldChange(pp->getImprovementType(), eY);
										accC = pCity->getImprovementYieldChange(pp->getImprovementType(), eY);
									}
									eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PLOTDIFF, 1)
										.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, aszChannel[y])
										.addI(MDF_PX, pp->getX()).addI(MDF_PY, pp->getY())
										.addI(MDF_PLOT_S, ppS).addI(MDF_PLOT_C, ppC)
										.addI(MDF_ACC_P, accP).addI(MDF_ACC_T, accT).addI(MDF_ACC_C, accC)
										.addStr(MDF_SAMPLE, sPlot.c_str()));
									++iPlotDiffShown;
								}
							}
						}
					}
				}
				if (cascRate != (long)legRate)
				{
					++iRateDiverging;
					if (iRateShown < 40)
					{
						// Specialist sub-terms on BOTH sides (owner 2026-07-02): the specialist-bucket move (building
						// free-specialists ride the specialist BASE, validation.md's accepted diff) must be NAILED per
						// divergence — the intentional component quantified exactly, never assumed to cover the whole
						// delta. specCasc = the cascade SpecialistPackage; specLeg = legacy's specialist yield term.
						const int specCasc = YieldBasePackages::specialist(aszChannel[y], pCity, rec);
						const int specLeg = pCity->getSpecialistYieldTotal(eY);
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, aszChannel[y])
							.addI(MDF_RATEC, (int)cascRate).addI(MDF_RATEL, legRate)
							.addI(MDF_SPECC, specCasc).addI(MDF_SPECL, specLeg));
						++iRateShown;
					}
				}
			}
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SHADOW, 1)
		.addI(MDF_CHECKED, iChecked).addI(MDF_DIVERGING, iDiverging));
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
		.addI(MDF_CHECKED, iChecked).addI(MDF_DIVERGING, iRateDiverging));

	// §2 COMMERCE rate shadow: the assembled per-type commerce rate vs legacy getCommerceRateTimes100 (gold/research/
	// culture/espionage; channel = the commerce-type string). Separate loop -- the §2 packages ride §1. ⚠ PERF: each call
	// recomputes the §1 commerce+production rate, so the cap is modest; memoize per city if the turn drags.
	int iCChecked = 0, iCDiverging = 0, iCShown = 0;
	int iWbChecked = 0, iWbDiverging = 0, iWbShown = 0, iWbSlotDiverging = 0;
	int iScChecked = 0, iScSlotDiverging = 0, iScShown = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p)
	{
		const CvPlayer& player = GET_PLAYER((PlayerTypes)p);
		if (!player.isAlive()) continue;
		int iLoop, iCityN = 0;
		for (const CvCity* pCity = player.firstCity(&iLoop); pCity != NULL && iCityN < MM_CITIES_PER_PLAYER; pCity = player.nextCity(&iLoop))
		{
			++iCityN;
			CvCascadeEvalCtx cec;
			cec.city = pCity; cec.plot = pCity->plot(); cec.player = &player; cec.team = &GET_TEAM(player.getTeam());
			EnablerKernel::wireOperatingBuildings(pCity, cec);   // the STANDING cascade operating buildings -- alive for the whole city's calc
			// Precompute the §1 commerce-yield + production-rate ONCE per city (the 4 commerce types share them) -- this is
			// the big perf fix (was 8 redundant full §1 rate computes per city; now 2).
			const long yc100 = YieldRate::yieldRate100("commerce", YIELD_COMMERCE, pCity, cec);
			const long prate = YieldRate::yieldRate100("production", YIELD_PRODUCTION, pCity, cec) / 100;
			for (int cc = 0; cc < NUM_COMMERCE_TYPES; ++cc)
			{
				++iCChecked;
				const CommerceTypes eC = (CommerceTypes)cc;
				const long cascC = CommerceCalc::commerceRate100(CommerceCalc::channel(cc), eC, pCity, cec, yc100, prate);
				int legC;
				{
					PerfAccumTimer perfLeg(CascadePerf::legacyRateMs);   // the pair timing (see the yield leg)
					legC = pCity->getCommerceRateTimes100Legacy(eC);   // post-flip: the Legacy sibling is the oracle (see the yield leg)
				}
				// [SLOT] commerce leg -- the accumulator's C_RATE vs the fresh calculator (its oracle)
				{
					++iSlotChecked;
					const long slotC = CascadeAccumulator::commerceRate100(pCity, eC);
					if (slotC != cascC)
					{
						++iSlotDiverging;
						if (iSlotShown < 40)
						{
							// COMPONENT-DECOMPOSED pairs (commerce leg): the standing plugin numbers vs their fresh
							// packages on the same ctx + the shared commerce-YIELD input (ycS = the slot combine the
							// accumulator's splitter consumed; ycC = the fresh calculator's -- a ycS/ycC diff means
							// the divergence lives in the YIELD slots, not the commerce plugins). Slider/disorder
							// are read live on both sides, so they can never be the diverging term.
							const CascadeCityPackages& st = pCity->m_cascadeCityPackages;
							const CascadePlayerScope& psn = player.m_cascadePlayerScope;
							MMBreak bkS;
							const long gaC = player.isGoldenAge() ? std::max(0L, psn.cGoldenAge[cc]) : 0;
							const long baseS = st.cBaseOwn100[cc] + st.cKeyed100[cc] + psn.cPlayerExtra100[cc]
								+ 100L * gaC + 100L * psn.cSrPool[cc] * st.iCSrMatch;
							eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SLOT, 1)
								.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, CommerceCalc::channel(cc))
								.addI(MDF_RATEC, (int)slotC).addI(MDF_RATEL, (int)cascC)
								.addI(MDF_YC_S, (int)CascadeAccumulator::yieldRate100(pCity, YIELD_COMMERCE))
								.addI(MDF_YC_C, (int)yc100)
								.addI(MDF_CSPEC_S, (int)st.cSpec100[cc])
								.addI(MDF_CSPEC_C, (int)(100L * YieldBasePackages::specialist(CommerceCalc::channel(cc), pCity, cec)))
								.addI(MDF_CBASE_S, (int)baseS)
								.addI(MDF_CBASE_C, (int)CommerceCalc::baseExtra100(CommerceCalc::channel(cc), pCity, cec))
								.addI(MDF_CPCT_S, (int)std::max(0L, 100 + st.cPct[cc]))
								.addI(MDF_CPCT_C, PercentStack::percentStack(CommerceCalc::channel(cc), pCity, bkS)));
							++iSlotShown;
						}
					}
				}
				if (cascC != (long)legC)
				{
					++iCDiverging;
					if (iCShown < 40)
					{
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, CommerceCalc::channel(cc))
							.addI(MDF_RATEC, (int)cascC).addI(MDF_RATEL, legC));
						++iCShown;
					}
				}
			}
			// [MODIFIER/wellbeing] -- the §2b channel's four verdicts vs the legacy engine (the SAME wired ctx).
			// The two documented accepted classes (improvement BALANCE-CUT + stored-accumulator DRIFT, modifier.md
			// §2b) show as standing attributed residue; anything beyond them is a port bug.
			{
				++iWbChecked;
				const CascadeWellbeingVerdicts wv = CascadeWellbeing::compute(pCity, cec);
				// [SLOT]-style net: the PACKAGE-composed verdicts (what the flipped getters return) vs this fresh
				// compute realized the same way -- a diff is a dirty-mapping hole in the packaged terms.
				{
					const int iMilLive = wv.iMilPerUnit * pCity->getMilitaryHappinessUnits();
					const int iFreshHappy = std::max(0, wv.iHappy + std::max(0, iMilLive));
					const int iFreshUnhappy = std::max(0, wv.iUnhappy - std::min(0, iMilLive));
					if (CascadeAccumulator::wellbeing(pCity, 0) != iFreshHappy
						|| CascadeAccumulator::wellbeing(pCity, 1) != iFreshUnhappy
						|| CascadeAccumulator::wellbeing(pCity, 2) != wv.iGood
						|| CascadeAccumulator::wellbeing(pCity, 3) != wv.iBad)
					{
						++iWbSlotDiverging;
					}
				}
				// the REALIZED cascade verdicts (slot + the live military fold -- what the FLIPPED getters return)
				// vs the LEGACY siblings (post-flip the plain getters ARE the cascade; Legacy is the net oracle)
				const int iCascHappy = CascadeAccumulator::wellbeing(pCity, 0);
				const int iCascUnhappy = CascadeAccumulator::wellbeing(pCity, 1);
				int iLegHappy, iLegUnhappy, iLegGood, iLegBad;
				{
					PerfAccumTimer perfLeg(CascadePerf::legacyWbMs);   // the wb pair timing (vs wbComputeMs -- pre-cut window)
					iLegHappy = pCity->happyLevelLegacy();
					iLegUnhappy = pCity->unhappyLevelLegacy();
					iLegGood = pCity->goodHealthLegacy();
					iLegBad = pCity->badHealthLegacy();
				}
				if (iCascHappy != iLegHappy || iCascUnhappy != iLegUnhappy || wv.iGood != iLegGood || wv.iBad != iLegBad)
				{
					++iWbDiverging;
					if (iWbShown < 40)
					{
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_WELLBEING, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString())
							.addI(MDF_HAP_C, iCascHappy).addI(MDF_HAP_L, iLegHappy)
							.addI(MDF_UNH_C, iCascUnhappy).addI(MDF_UNH_L, iLegUnhappy)
							.addI(MDF_GOOD_C, wv.iGood).addI(MDF_GOOD_L, iLegGood)
							.addI(MDF_BAD_C, wv.iBad).addI(MDF_BAD_L, iLegBad));
						++iWbShown;
					}
				}
			}
			// [MODIFIER/scalar] -- the increment-F scalar slots vs their fresh calculators (the same wired ctx).
			// RAW slot reads (the ensuring read next to the turn-stamp would be the tautological-0 trap
			// increment A caught): the number measures within-turn staleness. The HOOK MAP itself was PROVEN
			// 2026-07-04 (a transient self-heal-off gate + ensured reads over owner-played turns: diverging=0).
			{
				++iScChecked;
				const int cGpBase = CascadeScalarChannels::gpRateBase(pCity, cec);
				const int cGpMod = CascadeScalarChannels::gpRateModifier(pCity, cec);
				const int cDef = CascadeScalarChannels::defenseAmount(pCity, cec);
				const int cMaint = CascadeScalarChannels::maintenanceModifier(pCity, cec);
				const int cTrade = CascadeScalarChannels::tradeRouteCount(pCity, cec);
				const int sGpBase = CascadeAccumulator::scGpBase(pCity);
				const int sGpMod = CascadeAccumulator::scGpModifier(pCity);
				const int sDef = CascadeAccumulator::scDefense(pCity);
				const int sMaint = CascadeAccumulator::scMaintenanceModifier(pCity);
				const int sTrade = CascadeAccumulator::scTradeRoutes(pCity);
				if (sGpBase != cGpBase || sGpMod != cGpMod || sDef != cDef || sMaint != cMaint || sTrade != cTrade)
				{
					++iScSlotDiverging;
					if (iScShown < 40)
					{
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SCALAR, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString())
							.addI(MDF_GPB_S, sGpBase).addI(MDF_GPB_C, cGpBase)
							.addI(MDF_GPM_S, sGpMod).addI(MDF_GPM_C, cGpMod)
							.addI(MDF_DEF_S, sDef).addI(MDF_DEF_C, cDef)
							.addI(MDF_MNT_S, sMaint).addI(MDF_MNT_C, cMaint)
							.addI(MDF_TRD_S, sTrade).addI(MDF_TRD_C, cTrade));
						++iScShown;
					}
				}
			}
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
		.addI(MDF_CHECKED, iCChecked).addI(MDF_DIVERGING, iCDiverging));
	// the wellbeing summary: diverging = calc vs LEGACY (the accepted-class residue); slotDiverging (the RATEC
	// field) = the ACCD_WB slots vs the fresh calc (dirty-mapping holes -- must be 0 before the getter flip).
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_WELLBEING, 1)
		.addI(MDF_CHECKED, iWbChecked).addI(MDF_DIVERGING, iWbDiverging).addI(MDF_RATEC, iWbSlotDiverging));
	// the scalar summary: diverging = the increment-F slots vs their fresh calculators (dirty-mapping holes,
	// modulo the ruled cadence lag -- must read 0 before any scalar getter flip)
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SCALAR, 1)
		.addI(MDF_CHECKED, iScChecked).addI(MDF_DIVERGING, iScSlotDiverging));
	// the [SLOT] summary covers BOTH legs (yield + commerce) -- emitted once, after both loops
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_SLOT, 1)
		.addI(MDF_CHECKED, iSlotChecked).addI(MDF_DIVERGING, iSlotDiverging));

	// [MODIFIER/perf] census -- EXTRACTED (owner 2026-07-05) so it survives the shadow DISCONNECT: the perf
	// surface is ruled load-bearing, so it emits EVERY turn independent of the legacy diff above (disconnected).
	cvCascadeModifierPerfCensus();
}

// The [MODIFIER/perf] census + reset -- the RULED perf surface (perf-profile-wiring.md). NO legacy calls; it
// reports the whole-turn CascadePerf counts/ms + the flush-to-flush turnMs (DEC-turn-time-is-king) + the condEval
// caller split + the frontier fills, then resets. Runs EVERY turn from doTurn -- the modifier shadow diff (which
// re-walked legacy) is disconnected, so these counts now reflect PURE gameplay cost, not the shadow's extra reads.
void cvCascadeModifierPerfCensus()
{
	mm_registerDomain();   // #430: the SD_MODIFIER domain registration moved here from the disconnected shadow
	int iTurnMsX10 = 0;
	{
		static LARGE_INTEGER s_lastPerfFlush = { 0 };
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		if (s_lastPerfFlush.QuadPart != 0 && freq.QuadPart != 0)
			iTurnMsX10 = (int)((now.QuadPart - s_lastPerfFlush.QuadPart) * 10000 / freq.QuadPart);
		s_lastPerfFlush = now;
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_OPERATING_BUILDINGS_RECOMPUTED, CascadePerf::operatingBuildingsRecomputed).addI(MDF_OPERATING_BUILDINGS_CACHE_HITS, CascadePerf::operatingBuildingsCacheHits)
		.addI(MDF_YRN, CascadePerf::yieldRate).addI(MDF_PSN, CascadePerf::pctStack)
		.addI(MDF_CRN, CascadePerf::commerceRate).addI(MDF_CEN, CascadePerf::condEval)
		.addI(MDF_ACCN, CascadePerf::accRefresh)
		.addI(MDF_OPERATING_BUILDINGS_RECOMPUTE_MS, (int)(CascadePerf::operatingBuildingsRecomputeMs * 10.0)).addI(MDF_YRMS, (int)(CascadePerf::yieldRateMs * 10.0))
		.addI(MDF_PSMS, (int)(CascadePerf::pctStackMs * 10.0)).addI(MDF_CRMS, (int)(CascadePerf::commerceRateMs * 10.0))
		.addI(MDF_WBN, CascadePerf::wbCompute).addI(MDF_WBMS, (int)(CascadePerf::wbComputeMs * 10.0)));
	// the second perf line (the spine caps an event at 16 fields): the game turn + the flush-to-flush WALL
	// time (the DEC-turn-time-is-king headline) + the legacy-side pair ms (the pre-cut comparison numbers)
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_TURNNO, GC.getGame().getGameTurn()).addI(MDF_TURNMS, iTurnMsX10)
		.addI(MDF_LRMS, (int)(CascadePerf::legacyRateMs * 10.0)).addI(MDF_LWBMS, (int)(CascadePerf::legacyWbMs * 10.0))
		.addI(MDF_SCGPB, CascadePerf::scGpBaseReads).addI(MDF_SCGPM, CascadePerf::scGpModReads)
		.addI(MDF_SCDEF, CascadePerf::scDefReads).addI(MDF_SCMNT, CascadePerf::scMaintReads)
		.addI(MDF_SCREF, CascadePerf::scRefresh).addI(MDF_SCSREF, CascadePerf::scSpecRefresh)
		.addI(MDF_SCMS, (int)(CascadePerf::scRefreshMs * 10.0))
		.addI(MDF_AUTON, CascadePerf::autoMissions).addI(MDF_AUTOMS, (int)(CascadePerf::autoMissionMs * 10.0)));
	// the third perf line: the condEval CALLER split (the 6.8M-outlier attribution -- who initiated the
	// eval chains; ceOther is the honest residual: a big OTHER names the next tag to place)
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_CE_OTHER, CascadePerf::condEvalBy[CC_OTHER]).addI(MDF_CE_RATES, CascadePerf::condEvalBy[CC_RATES])
		.addI(MDF_CE_WB, CascadePerf::condEvalBy[CC_WB]).addI(MDF_CE_SC, CascadePerf::condEvalBy[CC_SCALARS])
		.addI(MDF_CE_OPERATING_BUILDINGS, CascadePerf::condEvalBy[CC_OPERATING_BUILDINGS]).addI(MDF_CE_FRB, CascadePerf::condEvalBy[CC_FRONT_B])
		.addI(MDF_CE_FRU, CascadePerf::condEvalBy[CC_FRONT_U]).addI(MDF_CE_FRPP, CascadePerf::condEvalBy[CC_FRONT_PP])
		.addI(MDF_CE_FRP, CascadePerf::condEvalBy[CC_FRONT_P]).addI(MDF_CE_CANB, CascadePerf::condEvalBy[CC_CANBUILD])
		.addI(MDF_CE_PROMO, CascadePerf::condEvalBy[CC_PROMO]));
	// the fourth perf line: the frontier fill counts + wall clock (the flip-era surfaces the census had
	// NO ms bucket for -- how often each frontier rebuilds and what a rebuild costs)
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_FRB_N, CascadePerf::frontBFills).addI(MDF_FRB_MS, (int)(CascadePerf::frontBMs * 10.0))
		.addI(MDF_FRU_N, CascadePerf::frontUFills).addI(MDF_FRU_MS, (int)(CascadePerf::frontUMs * 10.0))
		.addI(MDF_FRPP_N, CascadePerf::frontPPFills).addI(MDF_FRPP_MS, (int)(CascadePerf::frontPPMs * 10.0))
		.addI(MDF_FRP_N, CascadePerf::frontPFills).addI(MDF_FRP_MS, (int)(CascadePerf::frontPMs * 10.0))
		.addI(MDF_PRM_N, CascadePerf::promoFills).addI(MDF_PRM_MS, (int)(CascadePerf::promoMs * 10.0)));
	// the fifth perf line: the dark LOAD-TIME [READJSON] stats, re-surfaced LIVE here (the load burst is dark --
	// gPlayerLogLevel 0 during doPostLoadCaching -- and the old [MODIFIER/repo] home is on the DISCONNECTED shadow
	// sweep, so it never fires this session). healDiffDiverging is the headline the owner drives to 0 before the
	// XML-read cut; probeFiles/probeEntities + unresolvedFks are the load-probe/FK counts that rode the same dead line.
	// Numeric-only (the 16-field cap; the long dataDir/unresIds strings stay on the [MODIFIER/repo] line). -1 = never ran.
	int iHealChecked = 0, iHealDiverging = 0, iProbeFiles = 0, iProbeEnt = 0;
	cascadeReadJsonHealDiffStats(false, iHealChecked, iHealDiverging);
	cascadeReadJsonStats(false, iProbeFiles, iProbeEnt, std::string());
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_HEALCHECKED, iHealChecked).addI(MDF_HEALDIVERGING, iHealDiverging)
		.addI(MDF_FILES2, iProbeFiles).addI(MDF_ENTITIES2, iProbeEnt)
		.addI(MDF_UNRES, (int)jsonUnresolvedIds().size()));
	// (the internal-profiler [PERF/turn] sink was removed 2026-07-05 -- owner ruling: never use the internal
	// profiler; the census/[MODIFIER/perf] gated logging IS the perf surface. See perf-profile-wiring.md + the
	// PROFILE_FUNC removal ticket.)
	CascadePerf::reset();
}
