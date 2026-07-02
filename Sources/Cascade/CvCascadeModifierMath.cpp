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
#include "CvCascadeYieldBasePackages.h" // YieldBasePackages::specialist -- the accepted-diff decomposition (rate diff)
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::commerceRate100 + the commerce channel table
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM -- the eval ctx's team
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::computeCityBuildingFacts -- the cascade-computed active-building set + vicinity provides
#include "CvJsonInfo.h"               // the mapped info (requiresOperate/dormantTriggers) -- the dorm-attribution diagnostic
#include "Repos/InfoRepo.h"           // InfoRepo<CvBuildingInfo> -- ditto
#include "CvCascadeReadJson.h"        // cascadeReadJsonStats -- re-surface the dark load-time probe stats
#include "CvEventSpine.h"             // the #430 dispatch spine -- the shadow diff rides it (SD_MODIFIER), NOT direct gDLL->logMsg
#include <set>
#include <string>

// ===================== [MODIFIER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The percent-stack shadow's diff + summary emit EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct
// gDLL->logMsg) -- the CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level.
// Per-emitter domain (SD_MODIFIER), one file (Cascade.log).
enum MdEvt { MDE_DIFF = 1, MDE_SHADOW, MDE_RATE, MDE_DORM, MDE_REPO, MDE_PERF };
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
	MDF_FACTS, MDF_FACTSHIT, MDF_YRN, MDF_PSN, MDF_CRN, MDF_CEN,                    // perf: call counts
	MDF_FACTSMS, MDF_YRMS, MDF_PSMS, MDF_CRMS                                       // perf: stopwatch ms (x10 int)
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
	case MDF_FACTS:       return "facts";
	case MDF_FACTSHIT:    return "factsMemoHit";
	case MDF_YRN:         return "yieldRate";
	case MDF_PSN:         return "pctStack";
	case MDF_CRN:         return "commerceRate";
	case MDF_CEN:         return "condEval";
	case MDF_FACTSMS:     return "factsMsX10";
	case MDF_YRMS:        return "yieldRateMsX10";
	case MDF_PSMS:        return "pctStackMsX10";
	case MDF_CRMS:        return "commerceRateMsX10";
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
	YieldRate::memoClear();
	EnablerKernel::factsMemoClear();   // self-register SD_MODIFIER on the spine (idempotent) before the first emit

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
			if (!d->deposits.empty()) ++nDep;
			if (d->requiresOperate != NULL) ++nOp;
			if (!d->dormantTriggers.empty()) ++nTrig;
		}
		// + the stashed probe stats: what the dark load-time [READJSON] burst saw (files found / entities parsed /
		// the dataDir). files>0 with mapped=0 convicts a post-map clear or a duplicate-singleton read; files<=0
		// convicts the dataDir scan (path shown verbatim in `sample`). -1 = the probe never ran.
		int iFiles = 0, iEnt = 0;
		const std::string& sDir = cascadeReadJsonStats(false, iFiles, iEnt, std::string());
		eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_REPO, 1)
			.addI(MDF_TOTAL, nTot).addI(MDF_MAPPED, nMapped).addI(MDF_WDEPOSITS, nDep)
			.addI(MDF_WOPERATE, nOp).addI(MDF_WTRIGGERS, nTrig)
			.addI(MDF_FILES2, iFiles).addI(MDF_ENTITIES2, iEnt).addStr(MDF_SAMPLE, sDir.c_str()));
	}

	const char* aszChannel[NUM_YIELD_TYPES] = { "food", "production", "commerce" };   // indexed by the YieldTypes enum
	int iChecked = 0, iDiverging = 0, iShown = 0;
	int iRateDiverging = 0, iRateShown = 0;   // the §1 holistic rate diff (YieldRate::yieldRate100 vs getYieldRate100)
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
				std::set<int> recActiveB, recProvB;   // cascade-COMPUTED active set + in-vicinity provides (dormancy derived from operate, not the engine)
				EnablerKernel::computeCityBuildingFacts(pCity, rec, recActiveB, recProvB);
				rec.activeBuildings = &recActiveB; rec.vicinityProvidedBonuses = &recProvB;

				// [MODIFIER/dorm] -- DORMANCY ATTRIBUTION (2026-07-02, the Orwell bar: emit before hypothesising).
				// Re-derives each present building's cascade dorm verdict WITH its cause (operate-failed vs
				// trigger-dormed) and diffs against the engine's disabled verdict at the comparison boundary.
				// One line per sampled city (y==0 only, so once not thrice); samples list DISAGREEING buildings.
				if (y == 0)
				{
					CvCascadeEvalCtx recOp = rec; recOp.activeBuildings = NULL;   // mirror computeCityBuildingFacts' operate ctx
					CvCascadeEvalFlags dormFlags;
					int nPresent = 0, nDormOp = 0, nDormTrig = 0, nEngDisabled = 0;
					std::string sSample;
					for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
					{
						if (!pCity->hasBuilding((BuildingTypes)b)) continue;
						++nPresent;
						const CvJsonInfo* jb = InfoRepo<CvBuildingInfo>::get().get(b);
						bool bDormOp = (jb != NULL && jb->requiresOperate != NULL && !cascadeEvalCondition(jb->requiresOperate, recOp, dormFlags));
						bool bDormTrig = false;
						if (!bDormOp && jb != NULL)
							for (size_t i = 0; i < jb->dormantTriggers.size(); ++i)
								if (pCity->hasBuilding((BuildingTypes)jb->dormantTriggers[i])) { bDormTrig = true; break; }
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
						.addI(MDF_PRESENT, nPresent).addI(MDF_ACTIVE, (int)recActiveB.size())
						.addI(MDF_DORMOP, nDormOp).addI(MDF_DORMTRIG, nDormTrig).addI(MDF_ENGDISABLED, nEngDisabled)
						.addStr(MDF_SAMPLE, sSample.c_str()));
				}
				const long cascRate = YieldRate::yieldRate100(aszChannel[y], eY, pCity, rec);
				const int legRate = pCity->getYieldRate100(eY);
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
			std::set<int> cecActiveB, cecProvB;   // cascade-COMPUTED active set + in-vicinity provides (dormancy derived from operate) -- alive for the whole city's calc
			EnablerKernel::computeCityBuildingFacts(pCity, cec, cecActiveB, cecProvB);
			cec.activeBuildings = &cecActiveB; cec.vicinityProvidedBonuses = &cecProvB;
			// Precompute the §1 commerce-yield + production-rate ONCE per city (the 4 commerce types share them) -- this is
			// the big perf fix (was 8 redundant full §1 rate computes per city; now 2).
			const long yc100 = YieldRate::yieldRate100("commerce", YIELD_COMMERCE, pCity, cec);
			const long prate = YieldRate::yieldRate100("production", YIELD_PRODUCTION, pCity, cec) / 100;
			for (int cc = 0; cc < NUM_COMMERCE_TYPES; ++cc)
			{
				++iCChecked;
				const CommerceTypes eC = (CommerceTypes)cc;
				const long cascC = CommerceCalc::commerceRate100(CommerceCalc::channel(cc), eC, pCity, cec, yc100, prate);
				const int legC = pCity->getCommerceRateTimes100(eC);
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
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
		.addI(MDF_CHECKED, iCChecked).addI(MDF_DIVERGING, iCDiverging));

	// [MODIFIER/perf] -- the repeat-calc hunt (owner 2026-07-02: chase the needless repeat calcs BEFORE parity).
	// Whole-turn call counts + our stopwatch accumulators (PerfAccumTimer; ms x10 as ints -- the spine carries ints).
	// Counts cover EVERYTHING since the last flush (the full turn incl. the enabler sweep + getter instrument).
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_PERF, 1)
		.addI(MDF_FACTS, CascadePerf::facts).addI(MDF_FACTSHIT, CascadePerf::factsMemoHit)
		.addI(MDF_YRN, CascadePerf::yieldRate).addI(MDF_PSN, CascadePerf::pctStack)
		.addI(MDF_CRN, CascadePerf::commerceRate).addI(MDF_CEN, CascadePerf::condEval)
		.addI(MDF_FACTSMS, (int)(CascadePerf::factsMs * 10.0)).addI(MDF_YRMS, (int)(CascadePerf::yieldRateMs * 10.0))
		.addI(MDF_PSMS, (int)(CascadePerf::pctStackMs * 10.0)).addI(MDF_CRMS, (int)(CascadePerf::commerceRateMs * 10.0)));
	CascadePerf::reset();
}
