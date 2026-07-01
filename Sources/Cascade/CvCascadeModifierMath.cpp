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
#include "CvCascadeCommerceCalc.h"     // CommerceCalc::commerceRate100 + the commerce channel table
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "AI/CvTeamAI.h"              // GET_TEAM -- the eval ctx's team
#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx
#include "CvCascadeEnablerKernel.h"   // EnablerKernel::computeCityBuildingFacts -- the cascade-computed active-building set + vicinity provides
#include "CvEventSpine.h"             // the #430 dispatch spine -- the shadow diff rides it (SD_MODIFIER), NOT direct gDLL->logMsg
#include <set>

// ===================== [MODIFIER] spine domain (logging.md §4: logging is a spine CONSUMER) =====================
// The percent-stack shadow's diff + summary emit EVENTKIND_DIAGNOSTIC events through the event spine (NOT direct
// gDLL->logMsg) -- the CvCascadeLogConsumer renders the raw typed fields + tees to /events, gated by level.
// Per-emitter domain (SD_MODIFIER), one file (Cascade.log).
enum MdEvt { MDE_DIFF = 1, MDE_SHADOW, MDE_RATE };
enum MdFld
{
	MDF_WHO = 1, MDF_CHANNEL, MDF_CASC, MDF_BC, MDF_BA, MDF_BE, MDF_CIV, MDF_TR,   // diff: cascade buckets
	MDF_LEG, MDF_BLD, MDF_BON, MDF_POW, MDF_EVT, MDF_PLY, MDF_CAP,                 // diff: legacy sub-terms
	MDF_CHECKED, MDF_DIVERGING,                                                     // summary
	MDF_RATEC, MDF_RATEL                                                            // §1 rate diff: cascade vs legacy ×100
};
static const char* mm_prefix(int evt)
{
	switch (evt)
	{
	case MDE_DIFF:   return "[MODIFIER/diff]";
	case MDE_SHADOW: return "[MODIFIER/shadow]";
	case MDE_RATE:   return "[MODIFIER/rate]";
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
	mm_registerDomain();   // self-register SD_MODIFIER on the spine (idempotent) before the first emit

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
				rec.city = pCity; rec.player = &player; rec.team = &GET_TEAM(player.getTeam());
				std::set<int> recActiveB, recProvB;   // cascade-COMPUTED active set + in-vicinity provides (dormancy derived from operate, not the engine)
				EnablerKernel::computeCityBuildingFacts(pCity, rec, recActiveB, recProvB);
				rec.activeBuildings = &recActiveB; rec.vicinityProvidedBonuses = &recProvB;
				const long cascRate = YieldRate::yieldRate100(aszChannel[y], eY, pCity, rec);
				const int legRate = pCity->getYieldRate100(eY);
				if (cascRate != (long)legRate)
				{
					++iRateDiverging;
					if (iRateShown < 40)
					{
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, SD_MODIFIER, MDE_RATE, 1)
							.addWStr(MDF_WHO, pCity->getName().GetCString()).addStr(MDF_CHANNEL, aszChannel[y])
							.addI(MDF_RATEC, (int)cascRate).addI(MDF_RATEL, legRate));
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
			cec.city = pCity; cec.player = &player; cec.team = &GET_TEAM(player.getTeam());
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
}
