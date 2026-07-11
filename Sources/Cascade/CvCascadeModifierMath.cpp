//
//	CvCascadeModifierMath -- the [MODIFIER] spine domain + the per-turn census. The modifier CALCULATORS live in the
//	per-package static-methods classes (the single-source law, patterns.md): MMKernel (the leaf helpers), PercentStack,
//	YieldBasePackages, BuildingPackage, CommerceCalc -- consumed by CvCascadeAccumulator (the live slots). The
//	cascade-vs-legacy shadow harness that used to fill this file died with the shadow phase (validation.md); what
//	remains is the load-bearing observability: the [MODIFIER/perf] census (perf-profile-wiring.md) and the
//	[MODIFIER/repo] census (the building-repo map + the load-time FK-unresolved set, re-surfaced live because the
//	load burst is dark -- gPlayerLogLevel is 0 while readJson maps).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeModifierMath.h"
#include "Defines/CvGlobals.h"
#include "CvInfo.h"               // the mapped info (getModifiers/requiresOperate/dormantTriggers) -- the repo census
#include "Repos/InfoRepo.h"           // InfoRepo<CvBuildingInfo> -- ditto
#include "CvCascadePerfCount.h"       // CascadePerf -- the [MODIFIER/perf] census
#include "CvCascadeReadJson.h"        // cascadeReadJsonStats -- re-surface the dark load-time probe stats
#include "CvJsonParse.h"              // jsonUnresolvedIds -- re-surface the dark load-time FK misses
#include "CvEventSpine.h"             // the #430 dispatch spine -- the census rides it (SD_MODIFIER), NOT direct gDLL->logMsg
#include <set>
#include <string>

// ===================== [MODIFIER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The census emits EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct gDLL->logMsg) -- the
// CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level.
// Per-emitter domain (SD_MODIFIER), one file (Cascade.log).
enum MdEvt { MDE_REPO = 1, MDE_PERF };
enum MdFld
{
	MDF_SAMPLE = 1,                                                                 // the long string slot (dataDir / unresolved ids)
	MDF_TOTAL, MDF_MAPPED, MDF_WDEPOSITS, MDF_WOPERATE, MDF_WTRIGGERS,              // repo census (MDE_REPO)
	MDF_FILES2, MDF_ENTITIES2,                                                      // the stashed load-probe stats
	MDF_OPERATING_BUILDINGS_RECOMPUTED, MDF_OPERATING_BUILDINGS_CACHE_HITS, MDF_PSN, MDF_CEN, MDF_ACCN, // perf: call counts (+accumulator refreshes)
	MDF_OPERATING_BUILDINGS_RECOMPUTE_MS, MDF_PSMS,                                 // perf: stopwatch ms (x10 int)
	MDF_UNRES, MDF_UNRESIDS,                                                        // load-time FK misses, re-surfaced live
	MDF_WBN, MDF_WBMS,                                                              // perf: wellbeing computes + ms×10 (the /computed decomposition path)
	MDF_TURNNO, MDF_TURNMS,                                                         // perf: game turn + flush-to-flush WALL time (the headline turn-time number, DEC-turn-time-is-king)
	MDF_SCGPB, MDF_SCGPM, MDF_SCDEF, MDF_SCMNT,                                     // perf: flipped scalar getter READ counts
	MDF_SCREF, MDF_SCSREF, MDF_SCMS,                                                // perf: scalar refresh counts (SCALAR / SCALARSPEC) + refresh ms×10
	MDF_AUTON, MDF_AUTOMS,                                                          // perf: the AUTOMATION window (autoMission calls + accumulated ms×10)
	MDF_CE_OTHER, MDF_CE_RATES, MDF_CE_WB, MDF_CE_SC, MDF_CE_OPERATING_BUILDINGS,   // perf: the condEval CALLER split (the 6.8M-outlier attribution)
	MDF_CE_FRB, MDF_CE_FRU, MDF_CE_FRPP, MDF_CE_FRP, MDF_CE_CANB, MDF_CE_PROMO,
	MDF_FRB_N, MDF_FRB_MS, MDF_FRU_N, MDF_FRU_MS, MDF_FRPP_N, MDF_FRPP_MS,          // perf: the frontier fill counts + ms×10
	MDF_FRP_N, MDF_FRP_MS, MDF_PRM_N, MDF_PRM_MS
};
static const char* mm_prefix(int evt)
{
	switch (evt)
	{
	case MDE_REPO:   return "[MODIFIER/repo]";
	case MDE_PERF:   return "[MODIFIER/perf]";
	default:         return "[MODIFIER]";
	}
}
static const char* mm_field(int tag, SpineFieldType* peType)
{
	*peType = SFT_INT;
	switch (tag)
	{
	case MDF_SAMPLE:      *peType = SFT_STR; return "sample";
	case MDF_TOTAL:       return "total";
	case MDF_MAPPED:      return "mapped";
	case MDF_WDEPOSITS:   return "withDeposits";
	case MDF_WOPERATE:    return "withOperate";
	case MDF_WTRIGGERS:   return "withTriggers";
	case MDF_FILES2:      return "probeFiles";
	case MDF_ENTITIES2:   return "probeEntities";
	case MDF_OPERATING_BUILDINGS_RECOMPUTED:       return "operatingBuildingsRecomputed";
	case MDF_OPERATING_BUILDINGS_CACHE_HITS:    return "operatingBuildingsCacheHits";
	case MDF_PSN:         return "pctStack";
	case MDF_CEN:         return "condEval";
	case MDF_ACCN:        return "accRefresh";
	case MDF_OPERATING_BUILDINGS_RECOMPUTE_MS:     return "operatingBuildingsRecomputeMsX10";
	case MDF_PSMS:        return "pctStackMsX10";
	case MDF_UNRES:       return "unresolvedFks";
	case MDF_UNRESIDS:    *peType = SFT_STR; return "unresolvedSample";
	case MDF_WBN:         return "wbN";
	case MDF_WBMS:        return "wbMsX10";
	case MDF_TURNNO:      return "turn";
	case MDF_TURNMS:      return "turnMsX10";
	case MDF_SCGPB:       return "scGpBaseReads";
	case MDF_SCGPM:       return "scGpModReads";
	case MDF_SCDEF:       return "scDefReads";
	case MDF_SCMNT:       return "scMaintReads";
	case MDF_SCREF:       return "scRefresh";
	case MDF_SCSREF:      return "scSpecRefresh";
	case MDF_SCMS:        return "scRefreshMsX10";
	case MDF_AUTON:       return "autoMissions";
	case MDF_AUTOMS:      return "autoMissionMsX10";
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
	default:            return NULL;
	}
}
static void mm_registerDomain()
{
	static bool s_reg = false;
	if (!s_reg) { spineRegisterDomain(SD_MODIFIER, mm_prefix, "Cascade.log", mm_field); s_reg = true; }
}

// [MODIFIER/repo] -- BUILDING REPO CENSUS (the Orwell bar): decisive on the one-cause hypothesis (repo unmapped =>
// d==NULL everywhere). The load-time [READJSON] burst is dark (gPlayerLogLevel is 0 while readJson maps), so the
// census re-emits per turn where logging is live -- including the load-time FK-unresolved set: unresolvedFks>0
// after a clean load is ALWAYS a bug (a data typo or a map-before-registration ordering hole; exactly how the
// pre-menu-map bug hid, with the misses visible only in the window nobody can read).
static void mm_repoCensus()
{
	int nMapped = 0, nDep = 0, nOp = 0, nTrig = 0;
	const int nTot = GC.getNumBuildingInfos();
	for (int b = 0; b < nTot; ++b)
	{
		const CvInfo* d = InfoRepo<CvBuildingInfo>::get().get(b);
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

// The [MODIFIER/perf] census + reset -- the RULED perf surface (perf-profile-wiring.md). NO legacy calls; it
// reports the whole-turn CascadePerf counts/ms + the flush-to-flush turnMs (DEC-turn-time-is-king) + the condEval
// caller split + the frontier fills, then resets. Runs EVERY turn from doTurn.
void cvCascadeModifierPerfCensus()
{
	mm_registerDomain();
	mm_repoCensus();
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
		.addI(MDF_PSN, CascadePerf::pctStack).addI(MDF_CEN, CascadePerf::condEval)
		.addI(MDF_ACCN, CascadePerf::accRefresh)
		.addI(MDF_OPERATING_BUILDINGS_RECOMPUTE_MS, (int)(CascadePerf::operatingBuildingsRecomputeMs * 10.0))
		.addI(MDF_PSMS, (int)(CascadePerf::pctStackMs * 10.0))
		.addI(MDF_WBN, CascadePerf::wbCompute).addI(MDF_WBMS, (int)(CascadePerf::wbComputeMs * 10.0)));
	// the second perf line (the spine caps an event at 16 fields): the game turn + the flush-to-flush WALL
	// time (the DEC-turn-time-is-king headline) + the flipped scalar getter reads + the automation window
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_TURNNO, GC.getGame().getGameTurn()).addI(MDF_TURNMS, iTurnMsX10)
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
	// (the internal-profiler [PERF/turn] sink was removed -- owner ruling: never use the internal profiler; the
	// census/[MODIFIER/perf] gated logging IS the perf surface. See perf-profile-wiring.md.)
	CascadePerf::reset();
}
