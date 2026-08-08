//
//	InfoValuation -- the ONE per-GROUP what-if valuation walk + the §2a fold seams (see the header). The walk
//	reads ONLY load-compiled forms: the (family, kind, scope, unit) unconditioned slot sums straight, the
//	family-sorted conditioned entry ranges through the ONE evaluator/resolver -- no anatomy, no strings, no
//	requires evaluation (the entity verdict is fed in via the enabler's operating set).
//

#include "CvGameCoreDLL.h"                // PCH umbrella
#include "Data/CvInfoValuation.h"
#include "Data/CvDepositRead.h"           // MMKernel::applies / audienceOk / perScale (the entry carrier)
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the eval state the contexts fill
#include "CvModifiers.h"                  // the compiled read surface (sum / conditioned ranges / entries)
#include "CvModEntry.h"                   // the compiled §3.9 entry + modSegmentLookup (the plots target id)
#include "Engine/CityContext.h"           // fillEvalCtx (city/plot) + plotAttrs (the plot-predicate COUNTS)
#include "Engine/EmpireContext.h"         // fillEvalCtx (player/team)
#include "Enabler/CvEnablerKernel.h"      // wireOperatingBuildings -- the FED-IN active/dormant verdict
#include "Engine/CvPlayer.h"              // isHuman -- the audience resolution
#include "CvCascadeChannelRegistry.h"     // the roll-up's channel identity + the per-scope/receiver layout
#include "Engine/CvCity.h"                // the roll-up's city package + its area/owner axes
#include "Engine/CvPlot.h"                // the roll-up's plot package
#include "Engine/CvTeam.h"                // the roll-up's team package
#include "AI/CvPlayerAI.h"                // GET_PLAYER -- the city's owning empire
#include "AI/CvTeamAI.h"                  // GET_TEAM -- the empire's team
#include "Engine/CvGame.h"                // isOption -- the resolvedCityLimit gate
#include "Engine/CvMap.h"                 // getWorldSize -- the resolvedCityLimit scale
#include "Defines/CvGlobals.h"            // GC
#include "CvWorldInfo.h"                  // getCityLimitsScalePercent

namespace
{
	//
	//	The scope FOLD SET of the experienced-here answer (modifier.md §1 scope principle: the realized value is
	//	the trivial sum of the scope packages a city sits under -- world/team/empire/city). Plot-scope and
	//	the sub-city scopes are NOT city-experienced sums (plot output is the isolated per-plot base package;
	//	unit-scope is a self-accumulator); the plots-TARGET group has its own endpoint.
	//
	const CvCascScope VAL_FOLD_SCOPES[] =
	{
		CASC_SCOPE_WORLD, CASC_SCOPE_TEAM, CASC_SCOPE_EMPIRE, CASC_SCOPE_CITY
	};
	const int NUM_VAL_FOLD_SCOPES = (int)(sizeof(VAL_FOLD_SCOPES) / sizeof(VAL_FOLD_SCOPES[0]));

	bool val_scopeFolds(CvCascScope eScope)
	{
		for (int i = 0; i < NUM_VAL_FOLD_SCOPES; ++i)
		{
			if (VAL_FOLD_SCOPES[i] == eScope)
			{
				return true;
			}
		}
		return false;
	}

	// The asking player's audience (json §3.9 `ai`): aiOnly deposits count only for an AI asker.
	bool val_aiAudience(const CvCascadeEvalCtx& evalCtx)
	{
		return evalCtx.empireContext != NULL && !evalCtx.empireContext->isHuman();
	}

	// Resolve a `plots`-target deposit's predicate to a cityContext.plotAttrs COUNT (contexts.md: "how many,
	// not which" -- the count IS the aggregate, keyed on the CASC_PRED_* id). A bare single predicate reads its
	// key directly; a one-child `all` group collapses onto its child. A compound/parameterized/presence
	// predicate has no counted key in the aggregate yet -- it answers 0 (fail-visible through the oracle, never
	// a guessed engine walk; the plotAttrs key set is OPEN and grows with the contexts, contexts.md).
	int val_plotPredicateCount(const CvCondition* pPredicate, const CityContext& cityContext)
	{
		if (pPredicate == NULL)
		{
			return 0;   // an unpredicated plots deposit names no counted key -- no "all plots" aggregate exists yet
		}
		if (pPredicate->kind == CASC_COND_PREDICATE && pPredicate->param.empty())
		{
			return cityContext.plotAttrs.count((int)pPredicate->predKind);
		}
		if (pPredicate->kind == CASC_COND_GROUP && pPredicate->all.size() == 1
			&& pPredicate->anyOf.empty() && pPredicate->noneOf.empty())
		{
			return val_plotPredicateCount(pPredicate->all[0], cityContext);
		}
		return 0;
	}

	// The shared plots-TARGET fold for one channel family: Σ over the family's plots-target flat entries of
	// value × plotAttrs-count(predicate), audience-gated. Walks entries() (a targeted entry never
	// point-folds, and an UNconditioned targeted entry is not in the conditioned list either).
	int64_t val_plotsTarget(const CvModifiers* modifiers, ModifierFamily eFamily,
		const CvCascadeEvalCtx& evalCtx, const CityContext& cityContext)
	{
		const int iPlotsSeg = modSegmentLookup("plots");
		if (iPlotsSeg < 0)
		{
			return 0;   // never authored anywhere
		}
		int64_t iTotal = 0;
		const std::vector<CvModEntry*>& entries = modifiers->entries();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			const CvModEntry* pEntry = entries[i];
			if (pEntry->family != eFamily || pEntry->targetSeg != iPlotsSeg)
			{
				continue;
			}
			if (pEntry->unit != CASC_UNIT_FLAT)
			{
				continue;   // a per-plot percent resolves inside the isolated plot package, never here (modifier.md §2)
			}
			if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
			{
				continue;
			}
			if (pEntry->disabled != NULL && !MMKernel::applies(NULL, pEntry->disabled, evalCtx))
			{
				continue;   // a plots entry's `enabled` is the per-plot FILTER (json §6.1); `disabled` stays a gate
			}
			iTotal += (int64_t)pEntry->value * val_plotPredicateCount(pEntry->enabled, cityContext);
		}
		return iTotal;
	}
}



namespace
{
	//	The keyed-combat axis -> its interned segment id, resolved ONCE per axis. An axis token the data never
	//	authored answers TARGET_SEGMENT_NONE, so the id can be forwarded straight into the keyed reads and
	//	matches nothing -- never the raw -1, which is the live DIRECT-KEYED address shape there.
	int keyedCombatSegment(InfoValuation::CombatTargetAxis eAxis)
	{
		static bool s_resolved[6] = { false, false, false, false, false, false };
		static int s_segments[6] = { 0, 0, 0, 0, 0, 0 };
		if (!s_resolved[eAxis])
		{
			static const char* const kAxisSegments[6] = { "terrain", "feature", "unitCombat", "vsUnit", "domain", "flanking" };
			const int iSegment = modSegmentLookup(kAxisSegments[eAxis]);
			s_segments[eAxis] = iSegment < 0 ? (int)InfoValuation::TARGET_SEGMENT_NONE : iSegment;
			s_resolved[eAxis] = true;
		}
		return s_segments[eAxis];
	}

	bool keyedCombatEntryMatches(const CvModEntry& kEntry, int iSegment, int iKind)
	{
		return kEntry.family == MODFAM_COMBAT
			&& kEntry.targetSeg == iSegment
			&& kEntry.kind == iKind
			&& kEntry.targetFk >= 0;
	}
}

bool InfoValuation::authorsAnySigned(const CvModifiers* modifiers, ModifierFamily eFamily, int iSign,
	int iKind, int iScope)
{
	if (modifiers == NULL)
	{
		return false;
	}
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = kEntries[iEntry];
		if (pEntry == NULL || pEntry->family != eFamily)
		{
			continue;
		}
		if (iKind >= 0 && pEntry->kind != iKind)
		{
			continue;
		}
		if (iScope >= 0 && (int)pEntry->scope != iScope)
		{
			continue;
		}
		if (iSign >= 0 ? (pEntry->value > 0) : (pEntry->value < 0))
		{
			return true;
		}
	}
	return false;
}

int InfoValuation::overThresholdPenalty(const CvModifiers* modifiers, ModifierFamily eFamily,
	const char* szAboveToken)
{
	if (modifiers == NULL || szAboveToken == NULL)
	{
		return 0;
	}
	int iWorst = 0;
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = kEntries[iEntry];
		if (pEntry == NULL || pEntry->family != eFamily || !pEntry->hasAbove)
		{
			continue;
		}
		if (pEntry->perAboveToken != szAboveToken)
		{
			continue;
		}
		// authored NEGATIVE (it is a penalty on a positive channel); hand it back positive
		if (pEntry->value < 0 && -pEntry->value > iWorst)
		{
			iWorst = -pEntry->value;
		}
	}
	return iWorst;
}

int InfoValuation::keyedTargetSegment(const char* szTargetSegment)
{
	const int iSegment = modSegmentLookup(szTargetSegment);
	//	modSegmentLookup answers -1 for "never authored", which is the LIVE direct-keyed address shape here --
	//	so it is re-coded to the distinct not-in-the-data answer the keyed reads match nothing against.
	return iSegment < 0 ? (int)TARGET_SEGMENT_NONE : iSegment;
}
void InfoValuation::collectKeyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
	int iTargetSegment, std::vector<std::pair<int, int> >& targetValues, int iScope)
{
	targetValues.clear();
	if (modifiers == NULL || iTargetSegment == TARGET_SEGMENT_NONE)
	{
		return;
	}
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry& kEntry = *kEntries[iEntry];
		if (kEntry.family != eFamily
		||  kEntry.targetSeg != iTargetSegment
		||  kEntry.targetFk < 0
		|| (iKind >= 0 && kEntry.kind != iKind)
		|| (iScope >= 0 && (int)kEntry.scope != iScope)
		||  kEntry.enabled != NULL || kEntry.disabled != NULL)
		{
			continue;
		}
		size_t iRow = 0;
		while (iRow < targetValues.size() && targetValues[iRow].first != kEntry.targetFk)
		{
			++iRow;
		}
		if (iRow == targetValues.size())
		{
			targetValues.push_back(std::make_pair(kEntry.targetFk, 0));
		}
		targetValues[iRow].second += kEntry.value;
	}
}

int InfoValuation::keyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
	int iTargetSegment, int iTargetFk)
{
	if (modifiers == NULL || iTargetFk < 0 || iTargetSegment == TARGET_SEGMENT_NONE)
	{
		return 0;
	}
	int iTotal = 0;
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry& kEntry = *kEntries[iEntry];
		if (kEntry.family == eFamily
		&&  kEntry.targetSeg == iTargetSegment
		&&  kEntry.targetFk == iTargetFk
		&& (iKind < 0 || kEntry.kind == iKind)
		&&  kEntry.enabled == NULL && kEntry.disabled == NULL)
		{
			iTotal += kEntry.value;
		}
	}
	return iTotal;
}

int InfoValuation::keyedCombat(const CvModifiers* modifiers, CombatTargetAxis eAxis, int iTargetFk, int iKind)
{
	return keyedTarget(modifiers, MODFAM_COMBAT, iKind, keyedCombatSegment(eAxis), iTargetFk);
}

void InfoValuation::collectKeyedCombat(const CvModifiers* modifiers, CombatTargetAxis eAxis, int iKind,
	std::vector<std::pair<int, int> >& targetPercents)
{
	targetPercents.clear();
	if (modifiers == NULL)
	{
		return;
	}
	const int iSegment = keyedCombatSegment(eAxis);
	if (iSegment < 0)
	{
		return;
	}
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry& kEntry = *kEntries[iEntry];
		if (!keyedCombatEntryMatches(kEntry, iSegment, iKind))
		{
			continue;
		}
		size_t iRow = 0;
		while (iRow < targetPercents.size() && targetPercents[iRow].first != kEntry.targetFk)
		{
			++iRow;
		}
		if (iRow == targetPercents.size())
		{
			targetPercents.push_back(std::make_pair(kEntry.targetFk, 0));
		}
		targetPercents[iRow].second += kEntry.value;
	}
}

void InfoValuation::collectHealByUnitCombat(const CvModifiers* modifiers, std::vector<HealByUnitCombat>& healRows)
{
	healRows.clear();
	if (modifiers == NULL)
	{
		return;
	}
	static const int iUnitCombatSeg = modSegmentLookup("unitCombat");
	if (iUnitCombatSeg < 0)
	{
		return;   // nothing anywhere authored a unitCombat-keyed deposit
	}
	const std::vector<CvModEntry*>& kEntries = modifiers->entries();
	for (size_t iEntry = 0; iEntry < kEntries.size(); ++iEntry)
	{
		const CvModEntry& kEntry = *kEntries[iEntry];
		if (kEntry.family != MODFAM_HEAL || kEntry.targetSeg != iUnitCombatSeg || kEntry.targetFk < 0)
		{
			continue;
		}
		if (kEntry.kind != HEAL_RATE && kEntry.kind != HEAL_ADJACENT)
		{
			continue;
		}
		size_t iRow = 0;
		while (iRow < healRows.size() && healRows[iRow].iUnitCombat != kEntry.targetFk)
		{
			++iRow;
		}
		if (iRow == healRows.size())
		{
			HealByUnitCombat kNew;
			kNew.iUnitCombat = kEntry.targetFk;
			healRows.push_back(kNew);
		}
		if (kEntry.kind == HEAL_RATE)
		{
			healRows[iRow].iHeal += kEntry.value;
		}
		else
		{
			healRows[iRow].iAdjacentHeal += kEntry.value;
		}
	}
}

void InfoValuation::fillEvalCtxAtPlot(const CvPlot& plot, CvCascadeEvalCtx& evalCtx)
{
	//	the ctx carries the SILOS, never the objects ([contexts.md]) -- the plot's own, its owner's empire silo
	//	(which answers every team fact too), and the working city's, which also feeds in the enabler's operating
	//	set. The CITY fill is deliberately not used: it would overwrite the PLOT silo with the city centre's,
	//	and this walk is anchored on THIS plot, so only the city half is taken.
	evalCtx.plotContext = &plot.getPlotContext();
	const PlayerTypes eOwner = plot.getOwner();
	if (eOwner != NO_PLAYER)
	{
		evalCtx.empireContext = &GET_PLAYER(eOwner).getEmpireContext();
	}
	const CvCity* pWorkingCity = plot.getWorkingCity();
	if (pWorkingCity != NULL)
	{
		evalCtx.cityContext = &pWorkingCity->getCityContext();
		EnablerKernel::wireOperatingBuildings(pWorkingCity, evalCtx);
	}
}

void InfoValuation::fillEvalCtx(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, CvCascadeEvalCtx& evalCtx)
{
	//	Each context hands the ctx ITSELF -- the isolated live-state SILO, never the object holding it
	//	([contexts.md]). The city's fill also feeds in the FED-IN entity verdict (patterns.md what-if driver):
	//	the enabler's standing operating set -- active + obsolete buildings + vicinity-provided bonuses -- so a
	//	building-presence or vicinity predicate reads the cascade-computed state and the walk itself never
	//	evaluates `requires`.
	cityContext.fillEvalCtx(evalCtx);     // the city silo + its centre plot's + the operating set
	empireContext.fillEvalCtx(evalCtx);   // the empire silo -- which answers every team fact too
	// the reserved TRADED-bonus source (contexts.md): a city-bound ctx answers connection:"trade" through the
	// city's own plot-group-backed maintained count; this explicit pass-in serves the city-less what-if.
	evalCtx.plotGroup = plotGroup;
}

int64_t InfoValuation::groupSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	int64_t iTotal = 0;
	// (1) the compiled unconditioned sums, fetched straight -- one slot load per folded scope, 0 calculation
	for (int iScopeIdx = 0; iScopeIdx < NUM_VAL_FOLD_SCOPES; ++iScopeIdx)
	{
		iTotal += modifiers->sum(eFamily, iKind, VAL_FOLD_SCOPES[iScopeIdx], eUnit, bAiAudience);
	}
	// (2) the family's conditioned tail, through the ONE evaluator + the ONE §3.7 resolver
	size_t iBegin = 0;
	size_t iEnd = 0;
	modifiers->conditionedRange(eFamily, iBegin, iEnd);
	const std::vector<const CvModEntry*>& conditioned = modifiers->conditioned();
	for (size_t i = iBegin; i < iEnd; ++i)
	{
		const CvModEntry* pEntry = conditioned[i];
		if (pEntry->kind != iKind || pEntry->unit != eUnit)
		{
			continue;
		}
		if (pEntry->targetSeg >= 0 || pEntry->targetFk >= 0)
		{
			continue;   // targeted entries are their own reads (plots -> expectedPlotYields; keyed -> entry-list consumers)
		}
		if (!val_scopeFolds(pEntry->scope))
		{
			continue;
		}
		if (pEntry->unitQual != NULL)
		{
			continue;   // unit-carried values ride ON TOP live ([DEC-unit-modifiers-on-top]), never inside a valuation
		}
		if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
		{
			continue;
		}
		if (!MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
		{
			continue;
		}
		iTotal += MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
	}
	return iTotal;
}

int64_t InfoValuation::groupSumAt(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	CvCascScope eScope, const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	int64_t iTotal = modifiers->sum(eFamily, iKind, eScope, eUnit, bAiAudience);
	size_t iBegin = 0;
	size_t iEnd = 0;
	modifiers->conditionedRange(eFamily, iBegin, iEnd);
	const std::vector<const CvModEntry*>& conditioned = modifiers->conditioned();
	for (size_t i = iBegin; i < iEnd; ++i)
	{
		const CvModEntry* pEntry = conditioned[i];
		if (pEntry->kind != iKind || pEntry->unit != eUnit || pEntry->scope != eScope)
		{
			continue;
		}
		if (pEntry->targetSeg >= 0 || pEntry->targetFk >= 0)
		{
			continue;   // targeted entries are their own reads (plot-keyed at the plot fold; landed boosts target-side)
		}
		if (pEntry->unitQual != NULL)
		{
			continue;   // unit-carried values ride ON TOP live ([DEC-unit-modifiers-on-top])
		}
		if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
		{
			continue;
		}
		if (!MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
		{
			continue;
		}
		iTotal += MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
	}
	return iTotal;
}

int64_t InfoValuation::plotOwnYield(const CvModifiers* modifiers, ModifierFamily eFamily, const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	// (1) the compiled unconditioned PLOT slot, fetched straight
	int64_t iTotal = modifiers->sum(eFamily, CHANNEL_AMOUNT, CASC_SCOPE_PLOT, CASC_UNIT_FLAT, bAiAudience);
	// (2) the plot-scope conditioned tail -- incl. the reverse-landed entries (CvReversePass lands a source's
	// substrate-keyed boost HERE at plot scope) -- under PLOT-EVAL semantics: a bare {HAS_BONUS:X} reads the
	// ctx's TARGET plot (bonusFromPlot), exactly the isolated plot-as-base reading of modifier.md §2.
	CvCascadeEvalFlags plotEvalFlags;
	plotEvalFlags.bonusFromPlot = true;
	size_t iBegin = 0;
	size_t iEnd = 0;
	modifiers->conditionedRange(eFamily, iBegin, iEnd);
	const std::vector<const CvModEntry*>& conditioned = modifiers->conditioned();
	for (size_t i = iBegin; i < iEnd; ++i)
	{
		const CvModEntry* pEntry = conditioned[i];
		if (pEntry->kind != (int)CHANNEL_AMOUNT || pEntry->unit != CASC_UNIT_FLAT || pEntry->scope != CASC_SCOPE_PLOT)
		{
			continue;
		}
		if (pEntry->targetSeg >= 0 || pEntry->targetFk >= 0)
		{
			continue;   // targeted entries land on their targets (the general reverse pass) -- never double-read here
		}
		if (pEntry->unitQual != NULL)
		{
			continue;   // unit-carried values ride ON TOP live ([DEC-unit-modifiers-on-top])
		}
		if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
		{
			continue;
		}
		if (pEntry->enabled != NULL && !cascadeEvalCondition(pEntry->enabled, evalCtx, plotEvalFlags))
		{
			continue;
		}
		if (pEntry->disabled != NULL && cascadeEvalCondition(pEntry->disabled, evalCtx, plotEvalFlags))
		{
			continue;
		}
		iTotal += MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
	}
	return iTotal;
}

void InfoValuation::plotBaseYields(const CvModifiers* terrainModifiers, const CvModifiers* featureModifiers,
	const CvModifiers* bonusModifiers, const CvModifiers* improvementModifiers, const CvModifiers* routeModifiers,
	const CvCascadeEvalCtx& evalCtx, int (&plotYields)[NUM_YIELD_TYPES], int (*pNatureYields)[NUM_YIELD_TYPES])
{
	// THE ISOLATED PLOT-AS-BASE CALC (modifier.md §2 plot note; legacy decomposition calc-map §10.1): per
	// channel -- nature = max(0, terrain + feature + bonus own-output); the improvement floored at −nature
	// (an improvement can consume the nature yield, never drive the pre-route base negative); + route; the
	// whole package floored at 0. Every term is the substrate's own untargeted plot-scope output
	// (plotOwnYield), so the reverse-landed conditioned boosts ride each term automatically.
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		const ModifierFamily eFamily = infoYieldFamily(iYield);
		int64_t iNature = plotOwnYield(terrainModifiers, eFamily, evalCtx)
		                + plotOwnYield(featureModifiers, eFamily, evalCtx)
		                + plotOwnYield(bonusModifiers, eFamily, evalCtx);
		if (iNature < 0)
		{
			iNature = 0;
		}
		int64_t iImprovement = plotOwnYield(improvementModifiers, eFamily, evalCtx);
		if (iImprovement < -iNature)
		{
			iImprovement = -iNature;   // floored at −nature (modifier.md §2a basePlotYield row)
		}
		int64_t iTotal = iNature + iImprovement + plotOwnYield(routeModifiers, eFamily, evalCtx);
		if (iTotal < 0)
		{
			iTotal = 0;
		}
		plotYields[iYield] = (int)iTotal;
		if (pNatureYields != NULL)
		{
			// The SUBSTRATE segment, handed back from the one place that already derives it -- a caller
			// needing the pre-improvement value never re-sums the substrate legs itself.
			(*pNatureYields)[iYield] = (int)iNature;
		}
	}
}

int64_t InfoValuation::plotScaledYield(int64_t iBaseTotal, int64_t iInterval, int64_t iAmount)
{
	// A non-positive interval means no scaling is fed in for this channel -- the overwhelmingly common case, so
	// it is the first test rather than a guard tacked on the end.
	if (iInterval <= 0 || iBaseTotal <= 0 || iAmount == 0)
	{
		return iBaseTotal;
	}
	const int64_t iSteps = iBaseTotal / iInterval;   // WHOLE intervals only -- integer division is the mechanic
	if (iSteps <= 0)
	{
		return iBaseTotal;
	}
	const int64_t iScaled = iBaseTotal + iSteps * iAmount;
	return (iScaled < 0) ? 0 : iScaled;
}

int64_t InfoValuation::cityRate(int64_t base, int64_t specialists, int iPercentSum, int64_t extra)
{
	// modifier.md §2a: ONE additive percent stack applied once, floored at zero; the EXTRA tier truncates to
	// whole units before re-scaling (the engine's getExtraYield100 order -- a documented integer-truncation
	// gotcha mirrored verbatim, never "fixed").
	//
	// SCALE: iPercentSum is a plain percent (25 = +25%), which is what a percent IS everywhere on this surface --
	// readJson does not scale one ([DEC-fixedpoint-x100]: a percentage has no decimals to carry), so the stored
	// sums are plain too and every consumer combines them the same way.
	int64_t iModifier = 100 + (int64_t)iPercentSum;
	if (iModifier < 0)
	{
		iModifier = 0;
	}
	return (base + specialists) * iModifier / 100 + 100 * (extra / 100);
}

int64_t InfoValuation::commerceSplit(int64_t commerceYieldRate, int iSliderPercent, int64_t channelPercentSum,
	int64_t channelDeposits, int64_t productionYieldRate, int iProductionToCommerce)
{
	// TIER 1 -- the slider share of the COMMERCE yield. The slider is a plain 0..100 counter (json §3.1), so the
	// ÷100 is the percent-to-fraction conversion, not a fixed-point de-scale.
	int64_t iShare = commerceYieldRate * (int64_t)iSliderPercent / 100;
	// The ONE additive stack (modifier.md §2a), floored at zero exactly as the yield rate's is -- a stack below
	// -100% zeroes the share, it never flips its sign. A stored percent is PLAIN, so it combines directly.
	int64_t iModifier = 100 + channelPercentSum;
	if (iModifier < 0)
	{
		iModifier = 0;
	}
	// TIER 2 -- the process conversion, added AFTER the percentages and never multiplied by them. The production
	// yield truncates to whole hammers BEFORE the conversion scales it (the engine's order, mirrored verbatim);
	// the conversion rate is ×100, so it de-scales to the authored human percent.
	int64_t iProcessConversion = (productionYieldRate / 100) * ((int64_t)iProductionToCommerce / 100);
	return iShare * iModifier / 100 + channelDeposits + iProcessConversion;
}

namespace
{
	// The channel's CANONICAL AUTHORED UNIT at the reading scope, resolved from the channel's own identity axes
	// -- the census's flat-vs-percent verdict lives beside the vocabulary (infoKindUnit) and is consumed here,
	// never re-decided per getter. A PROPERTY-plane channel carries kind 0 and answers the table's flat default.
	CvCascUnit val_channelCanonicalUnit(int iChannel, CvCascScope eScope)
	{
		const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
		const int iKind = CascadeChannelRegistry::channelKind(iChannel);
		return infoKindUnit(eFamily, iKind, eScope);
	}

	// WHICH commerce channel this is, or -1 for any other channel. Resolved by CHANNEL IDENTITY through the same
	// family lookup every other read uses -- never by a slot order and never by a hand-kept id list, so a
	// re-minted registry cannot silently re-point it.
	int val_commerceIndexOfChannel(int iChannel)
	{
		if (iChannel < 0)
		{
			return -1;
		}
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			if (CascadeChannelRegistry::channelLookup(infoCommerceFamily(iCommerce), (int)CHANNEL_AMOUNT, -1) == iChannel)
			{
				return iCommerce;
			}
		}
		return -1;
	}
}

int64_t InfoValuation::realizedChannel(int64_t flatSum, int64_t percentSum, CvCascUnit eCanonicalUnit)
{
	// A PERCENT-unit channel has no flat plane of its own: the ONE additive stack IS its realized value
	// (modifier.md §2a -- purely additive, applied once wherever it later scales something).
	if (eCanonicalUnit == CASC_UNIT_PERCENT || eCanonicalUnit == CASC_UNIT_RAW_PERCENT)
	{
		return percentSum;
	}
	// A FLAT-unit channel is the flat sum the stack scales: the §2 combine with no external base and multiplier
	// deposits identity. The stack floors at zero exactly as the §2a rate's does -- a stack below -100% zeroes
	// the value, it never flips its sign.
	int64_t iModifier = 100 + percentSum;
	if (iModifier < 0)
	{
		iModifier = 0;
	}
	return flatSum * iModifier / 100;
}

int InfoValuation::realizedAtPlot(const CvPlot& plot, int iChannel)
{
	if (iChannel < 0)
	{
		return 0;
	}
	// PLOT is the bottom of the spine and the ONE scope with no upper chain: a per-plot value resolves in
	// isolation as one base package BEFORE any city-level stack runs (modifier.md §2 plot-as-base), so an upper
	// scope's percent must never reach it -- that percent applies once, later, to the city's already-summed
	// base, and applying it here as well would scale the same magnitude twice. The plot carries no receiver
	// slot (nothing is consumed at plot scope), so there is no maintained sum to prefer over the combine.
	const int64_t iFlatSum = plot.getCascadePackage().readFlat(iChannel);
	const int64_t iPercentSum = plot.getCascadePackage().readPercent(iChannel);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_PLOT));
}

// The TIERED form (modifier.md §2a): the UPPER legs are TIER 1 BASE -- an empire/team flat is a free-city
// yield or its kin, which the percent stack MULTIPLIES -- while the CITY's own flat is the TIER 2 EXTRA that
// is added after it. Only building flats belong in that second tier, and they are exactly what a city-scope
// flat is. ⛔ Summing the three into one number and passing it as `extra` silently strips the multiplier from
// every upper-scope flat.
void InfoValuation::rolledLegsAtCity(const CvCity& city, int iChannel, int64_t& upperFlatSum,
	int64_t& cityFlatSum, int64_t& percentSum)
{
	upperFlatSum = 0;
	cityFlatSum = 0;
	percentSum = 0;
	if (iChannel < 0)
	{
		return;
	}
	const PlayerTypes eOwner = city.getOwner();
	if (eOwner != NO_PLAYER)
	{
		const CvPlayer& owner = GET_PLAYER(eOwner);
		const CvTeam& team = GET_TEAM(owner.getTeam());
		upperFlatSum += team.getCascadePackage().readFlat(iChannel);
		percentSum += team.getCascadePackage().readPercent(iChannel);
		upperFlatSum += owner.getCascadePackage().readFlat(iChannel);
		percentSum += owner.getCascadePackage().readPercent(iChannel);
	}
	cityFlatSum += city.getCascadePackage().readFlat(iChannel);
	percentSum += city.getCascadePackage().readPercent(iChannel);
}

void InfoValuation::rolledLegsAtCity(const CvCity& city, int iChannel, int64_t& flatSum, int64_t& percentSum)
{
	flatSum = 0;
	percentSum = 0;
	if (iChannel < 0)
	{
		return;
	}
	// The UPPER legs exist only while the city has an owner: an ownerless city has no empire or team to sit
	// under, so its chain is its own package alone rather than an invalid index.
	const PlayerTypes eOwner = city.getOwner();
	if (eOwner != NO_PLAYER)
	{
		const CvPlayer& owner = GET_PLAYER(eOwner);
		const CvTeam& team = GET_TEAM(owner.getTeam());
		flatSum += team.getCascadePackage().readFlat(iChannel);
		percentSum += team.getCascadePackage().readPercent(iChannel);
		flatSum += owner.getCascadePackage().readFlat(iChannel);
		percentSum += owner.getCascadePackage().readPercent(iChannel);
	}
	flatSum += city.getCascadePackage().readFlat(iChannel);
	percentSum += city.getCascadePackage().readPercent(iChannel);
}


// THE SPECIALIST TERM (see the header): the ONE fold both the oracle and the stored read use.
int64_t InfoValuation::specialistTerm(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx,
	std::vector<SpecialistTermRow>* pRowsOut)
{
	if (pRowsOut != NULL)
	{
		// Cleared before any early return, so a caller never reads rows left over from a previous channel.
		pRowsOut->clear();
	}
	if (iChannel < 0)
	{
		return 0;
	}
	int64_t iSpecialists = 0;
	const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
	const int iKind = CascadeChannelRegistry::channelKind(iChannel);
	for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
	{
		const int iCount = city.getSpecialistCount((SpecialistTypes)iSpecialist);
		if (iCount > 0)
		{
			const int64_t iPerUnit = groupSumAt(
				GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(),
				eFamily, iKind, CASC_UNIT_FLAT, CASC_SCOPE_CITY, evalCtx);
			iSpecialists += iCount * iPerUnit;
			if (pRowsOut != NULL)
			{
				SpecialistTermRow kRow;
				kRow.specialist = iSpecialist;
				kRow.assigned = iCount;
				kRow.freeTyped = city.getFreeSpecialistCount((SpecialistTypes)iSpecialist);
				kRow.perUnit = iPerUnit;
				kRow.contribution = iCount * iPerUnit;
				pRowsOut->push_back(kRow);
			}
		}
		else if (pRowsOut != NULL)
		{
			// CENSUS ONLY -- a type the city holds ONLY as free-typed contributes nothing today, and an absent row
			// would report that as "the city has no such specialist" rather than as the gap it is. The extra fold
			// is paid only when the census asks; the value path above never reaches here.
			const int iFreeTyped = city.getFreeSpecialistCount((SpecialistTypes)iSpecialist);
			if (iFreeTyped > 0)
			{
				SpecialistTermRow kRow;
				kRow.specialist = iSpecialist;
				kRow.assigned = 0;
				kRow.freeTyped = iFreeTyped;
				kRow.perUnit = groupSumAt(
					GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(),
					eFamily, iKind, CASC_UNIT_FLAT, CASC_SCOPE_CITY, evalCtx);
				kRow.contribution = 0;
				pRowsOut->push_back(kRow);
			}
		}
	}
	return iSpecialists;
}

// THE CITY RECEIVER (see the header): the §2a rate, re-summed from the members it is made of.
int64_t InfoValuation::cityReceiverRate(const CvCity& city, int iChannel, CityRateTerms* pTermsOut)
{
	if (pTermsOut != NULL)
	{
		// Fully defined before any early return, so a caller never reads a half-filled census.
		pTermsOut->plotBase = 0;
		pTermsOut->plotNature = 0;
		pTermsOut->plotImprovement = 0;
		pTermsOut->plotRest = 0;
		pTermsOut->tradeYield = 0;
		pTermsOut->goldenAge = 0;
		pTermsOut->upperFlat = 0;
		pTermsOut->specialists = 0;
		pTermsOut->cityFlat = 0;
		pTermsOut->percentSum = 0;
		pTermsOut->workedPlots = 0;
		pTermsOut->rate = 0;
		pTermsOut->specialistRows.clear();
	}
	if (iChannel < 0)
	{
		return 0;
	}
	// BASE -- the Σ over this city's WORKED PLOTS of their own package flats. This is the member re-sum a
	// receiver IS; nothing maintains it as a total, because a total of combines cannot be delta'd.
	int64_t iPlotBase = 0;
	int64_t iPlotNature = 0;
	int64_t iPlotImprovement = 0;
	int64_t iPlotRest = 0;
	int iWorkedPlots = 0;
	const int iNumPlots = city.getNumCityPlots();
	for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
	{
		if (!city.isWorkingPlot(iPlotIndex))
		{
			continue;
		}
		const CvPlot* pWorkedPlot = city.getCityIndexPlot(iPlotIndex);
		if (pWorkedPlot != NULL)
		{
			iPlotBase += pWorkedPlot->getCascadePackage().readFlat(iChannel);
			// the SAME walk, decomposed -- the census reads the segments the total is made of rather than
			// re-deriving them beside it ([DEC-single-implementation])
			if (pTermsOut != NULL)
			{
				iPlotNature += pWorkedPlot->getCascadePackage().readSubstrateFlat(iChannel);
				iPlotImprovement += pWorkedPlot->getCascadePackage().readImprovementFlat(iChannel);
				iPlotRest += pWorkedPlot->getCascadePackage().readRestFlat(iChannel);
			}
			++iWorkedPlots;
		}
	}
	int64_t iBase = iPlotBase;
	int64_t iTradeYield = 0;
	int64_t iGoldenAgeYield = 0;
	// ⚖ THE TRADE-ROUTE YIELD IS TIER 1 BASE, NOT AN EXTRA (modifier.md §2a): it is the ONE sanctioned live-yield
	// INPUT -- the cascade cannot re-derive the trade NETWORK, so the engine owns that calculation
	// (north-star.md KEEP) and its value is FOLDED IN here, where the percent stack still multiplies it.
	// ⛔ It is NOT commerce-only: the yield a route delivers per channel is the route PROFIT scaled by the
	// PLAYER's own per-yield trade modifier (CvCity::calculateTradeYield -> CvPlayer::getTradeYieldModifier), so
	// a player carrying a food trade modifier genuinely eats off its routes. There is no per-yield modifier on
	// CvYieldInfo to consult, and assuming one reads food's contribution as zero.
	// ⚑ NO CONVERSION HAPPENS HERE, and the absence is deliberate: the store is ×100 like every other amount, so
	// the fold is a plain add. ⛔ A `× 100` on this line would mean the store had been reduced somewhere upstream
	// — a round trip that keeps the scale and throws the FRACTION away, right under the percent stack
	// ([fixed-point-and-scales.md §4c-bis]).
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		if (CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1) == iChannel)
		{
			iTradeYield = (int64_t)city.getTradeYield((YieldTypes)iYield);
			iBase += iTradeYield;
			break;
		}
	}
	// ⚖ THE GOLDEN-AGE YIELD IS TIER 1 BASE TOO (modifier.md §2a / §3, golden-age.md): the player-wide
	// golden-age yield is the PERMANENT engine member-mirror `{ch}.empire.goldenAge.flat`, so it carries its own
	// MEMBER and therefore its own channel -- a plain package read at the empire, never a re-walk of the traits
	// that fed it. It applies only while the golden age holds, which is the whole of what the mirror means.
	const PlayerTypes eBaseOwner = city.getOwner();
	if (eBaseOwner != NO_PLAYER && GET_PLAYER(eBaseOwner).isGoldenAge())
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			if (CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1) != iChannel)
			{
				continue;
			}
			const int iGoldenChannel = CascadeChannelRegistry::channelLookup(
				infoYieldFamily(iYield), (int)CHANNEL_GOLDEN_AGE, -1);
			if (iGoldenChannel >= 0)
			{
				iGoldenAgeYield = GET_PLAYER(eBaseOwner).getCascadePackage().readFlat(iGoldenChannel);
				iBase += iGoldenAgeYield;
			}
			break;
		}
	}
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(city.getCityContext(), GET_PLAYER(city.getOwner()).getEmpireContext(), NULL, evalCtx);
	int64_t iUpperFlatSum = 0;
	int64_t iCityFlatSum = 0;
	int64_t iPercentSum = 0;
	rolledLegsAtCity(city, iChannel, iUpperFlatSum, iCityFlatSum, iPercentSum);
	// the upper legs join the BASE the stack multiplies; only the city's own flats are the post-stack EXTRA
	// the per-type rows come out of THIS fold, never a second one beside it ([DEC-single-implementation])
	const int64_t iSpecialists = specialistTerm(city, iChannel, evalCtx,
		pTermsOut != NULL ? &pTermsOut->specialistRows : NULL);
	const int64_t iRate = cityRate(iBase + iUpperFlatSum, iSpecialists, (int)iPercentSum, iCityFlatSum);
	if (pTermsOut != NULL)
	{
		pTermsOut->plotBase = iPlotBase;
		pTermsOut->plotNature = iPlotNature;
		pTermsOut->plotImprovement = iPlotImprovement;
		pTermsOut->plotRest = iPlotRest;
		pTermsOut->tradeYield = iTradeYield;
		pTermsOut->goldenAge = iGoldenAgeYield;
		pTermsOut->upperFlat = iUpperFlatSum;
		pTermsOut->specialists = iSpecialists;
		pTermsOut->cityFlat = iCityFlatSum;
		pTermsOut->percentSum = (int)iPercentSum;
		pTermsOut->workedPlots = iWorkedPlots;
		pTermsOut->rate = iRate;
	}
	return iRate;
}

namespace
{
	// |value| descending -- the biggest refusal is the one worth reading first.
	bool val_refusedBigger(const InfoValuation::RefusedDeposit& kLeft, const InfoValuation::RefusedDeposit& kRight)
	{
		const int64_t iLeft = kLeft.iValue < 0 ? -kLeft.iValue : kLeft.iValue;
		const int64_t iRight = kRight.iValue < 0 ? -kRight.iValue : kRight.iValue;
		return iLeft > iRight;
	}
}

void InfoValuation::cityRefusedDeposits(const CvCity& city, int iChannel,
	std::vector<RefusedDeposit>& refusedOut)
{
	refusedOut.clear();
	if (iChannel < 0)
	{
		return;
	}
	const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
	const int iKind = CascadeChannelRegistry::channelKind(iChannel);
	// the SAME ctx the apply path builds, so a refusal reported here is the refusal that actually happened --
	// a tooltip evaluating against a differently-filled ctx would invent refusals nobody experienced
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(city.getCityContext(), GET_PLAYER(city.getOwner()).getEmpireContext(),
		city.plotGroup(city.getOwner()), evalCtx);
	EnablerKernel::wireOperatingBuildings(&city, evalCtx);

	// ⛔ BUILDINGS ARE ONLY HALF OF IT. The cascade ROLLS DOWN: an empire-level source -- a civic, a trait, a
	// tech, a heritage, a project -- lands its CITY-scope entries in every city of the owner
	// (mc_applySourceDeposits' city branch walks the owner's cities for exactly this). An audit that walked only
	// the city's own buildings therefore explained 82 of a 155-point food percent stack and left 73 unattributed,
	// which reads identically to an over-apply. What rolls down is part of the answer, not context for it.
	std::vector<const CvInfo*> kSources;
	const OperatingBuildings& kOperating = EnablerKernel::operatingBuildings(&city);
	for (std::set<int>::const_iterator itActive = kOperating.active.begin(); itActive != kOperating.active.end(); ++itActive)
	{
		kSources.push_back(&GC.getBuildingInfo((BuildingTypes)(*itActive)));
	}
	const CvPlayer& kOwner = GET_PLAYER(city.getOwner());
	const EmpireContext& kEmpire = kOwner.getEmpireContext();
	int iId = 0;
	for (iId = 0; iId < GC.getNumCivicInfos(); ++iId)
	{
		if (kEmpire.hasCivic(iId)) { kSources.push_back(&GC.getCivicInfo((CivicTypes)iId)); }
	}
	for (iId = 0; iId < GC.getNumTraitInfos(); ++iId)
	{
		if (kEmpire.hasTrait(iId))
		{
			const CvTraitInfo* pTrait = MMKernel::traitData(iId);
			if (pTrait != NULL) { kSources.push_back((const CvInfo*)pTrait); }
		}
	}
	for (iId = 0; iId < GC.getNumTechInfos(); ++iId)
	{
		if (kEmpire.teamHasTech(iId)) { kSources.push_back(&GC.getTechInfo((TechTypes)iId)); }
	}
	for (iId = 0; iId < GC.getNumHeritageInfos(); ++iId)
	{
		if (kEmpire.hasHeritage(iId)) { kSources.push_back(&GC.getHeritageInfo((HeritageTypes)iId)); }
	}
	for (iId = 0; iId < GC.getNumProjectInfos(); ++iId)
	{
		if (kEmpire.teamProjectCount(iId) > 0) { kSources.push_back(&GC.getProjectInfo((ProjectTypes)iId)); }
	}
	// ⛔ THE CITY'S OWN NON-BUILDING SOURCES, and they are not a footnote: a city holding 258 resources has 258
	// sources depositing into its packages, and every one was missing from this walk. A census that omits a whole
	// source CLASS reports a shortfall that looks exactly like an over-apply -- the same total, the wrong story.
	const CityContext& kCityContext = city.getCityContext();
	std::vector<int> kTradedHeld;
	std::vector<int> kOnSiteHeld;
	kCityContext.collectBonusStores(kTradedHeld, kOnSiteHeld);
	std::set<int> kHeldBonuses(kTradedHeld.begin(), kTradedHeld.end());
	kHeldBonuses.insert(kOnSiteHeld.begin(), kOnSiteHeld.end());
	for (std::set<int>::const_iterator itBonus = kHeldBonuses.begin(); itBonus != kHeldBonuses.end(); ++itBonus)
	{
		if (*itBonus >= 0 && *itBonus < GC.getNumBonusInfos())
		{
			kSources.push_back(&GC.getBonusInfo((BonusTypes)(*itBonus)));
		}
	}
	for (iId = 0; iId < GC.getNumReligionInfos(); ++iId)
	{
		if (kCityContext.hasReligion(iId)) { kSources.push_back(&GC.getReligionInfo((ReligionTypes)iId)); }
	}
	for (iId = 0; iId < GC.getNumCorporationInfos(); ++iId)
	{
		if (kCityContext.hasCorporation(iId)) { kSources.push_back(&GC.getCorporationInfo((CorporationTypes)iId)); }
	}

	for (size_t iSource = 0; iSource < kSources.size(); ++iSource)
	{
		const CvInfo* pSourceInfo = kSources[iSource];
		const CvModifiers* pModifiers = (pSourceInfo != NULL) ? pSourceInfo->getModifiers() : NULL;
		if (pModifiers == NULL || pModifiers->empty())
		{
			continue;
		}
		const std::vector<CvModEntry*>& entries = pModifiers->entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			// ⛔ THE SCOPE TEST IS resolveEntry's, NEVER A HAND-ROLLED `entry->scope == CITY`. A civic's per-city
			// buff is authored at EMPIRE scope with a `cities` TARGET (json §3.3) and only RESOLVES at city scope,
			// so a raw scope comparison drops every rolled-down source -- which is precisely what the cascade is
			// for. Measured: the hand-rolled test admitted 190 entries and explained 82 of a 155-point stack; the
			// rolled-down half was invisible to it ([DEC-single-implementation]).
			if (pEntry == NULL || pEntry->family != eFamily || pEntry->kind != iKind)
			{
				continue;
			}
			// THE ONE RESOLVE decides whether this entry lands here and as what -- scope, target fan, audience,
			// unit side and the §3.9 gate, all of it. A census that re-implemented any of that could disagree
			// with the apply it claims to explain ([DEC-single-implementation]).
			int iEntryChannel = -1;
			bool bEntryPercent = false;
			int64_t iEntryValue = 0;
			const bool bResolved = MMKernel::resolveEntry(*pEntry, 1, CASC_SCOPE_CITY, evalCtx, NULL, 0, false,
				iEntryChannel, bEntryPercent, iEntryValue);
			const bool bConditioned = (pEntry->enabled != NULL || pEntry->disabled != NULL);
			if (!bResolved)
			{
				// It declined. Only a §3.9 gate makes that a REFUSAL worth reporting -- every other decline
				// (wrong scope, wrong audience, no target here) means the entry was never this city's to take.
				if (!bConditioned || MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
				{
					continue;
				}
			}
			RefusedDeposit kAudit;
			kAudit.szSource = pSourceInfo->getType();
			kAudit.iValue = bResolved ? iEntryValue : pEntry->value;
			kAudit.bPercentSide = bResolved ? bEntryPercent : MMKernel::unitIsPercentSide(pEntry->unit);
			kAudit.pCondition = (pEntry->enabled != NULL) ? pEntry->enabled : pEntry->disabled;
			kAudit.bApplied = bResolved;
			refusedOut.push_back(kAudit);
		}
	}
	std::sort(refusedOut.begin(), refusedOut.end(), val_refusedBigger);
}

int InfoValuation::realizedAtCity(const CvCity& city, int iChannel)
{
	if (iChannel < 0)
	{
		return 0;
	}
	// The channels the CITY consumes (food/production/commerce/culture) answer the maintained receiver sum the
	// gather's §2a combine already wrote AT THE MARK -- the realized RATE, which the generic combine below
	// cannot reproduce (it carries no specialist term and no post-percent EXTRA tier).
	if (CascadeChannelRegistry::scopeReceiverIndex(CASC_SCOPE_CITY, iChannel) >= 0)
	{
		return (int)cityReceiverRate(city, iChannel);
	}
	int64_t iFlatSum = 0;
	int64_t iPercentSum = 0;
	rolledLegsAtCity(city, iChannel, iFlatSum, iPercentSum);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_CITY));
}

int InfoValuation::realizedAtEmpire(const CvPlayer& player, int iChannel)
{
	if (iChannel < 0)
	{
		return 0;
	}
	// The channels the EMPIRE consumes (gold/research/culture/espionage) answer their maintained receiver sum --
	// the per-city realized rates already summed by the gather; culture is the lone dual consumer and answers a
	// receiver sum at BOTH scopes, each over its own combine.
	if (CascadeChannelRegistry::scopeReceiverIndex(CASC_SCOPE_EMPIRE, iChannel) >= 0)
	{
		// ⚖ THE Σ OF ITS MEMBERS' REALIZED VALUES, re-summed here (owner: "the receiver re-sums its
		// participating members, and nothing is built to avoid that"). ⛔ The empire's OWN package is NEVER
		// added on top: its deposits roll DOWN and are therefore already inside every city's realized value,
		// so adding them again would count each empire-scope deposit once per city PLUS once more.
		// ⚠ PARTICIPATION is a gate on the Σ, not a term in the combine: a city under DISORDER or celebrating
		// WLTKD keeps its real value and simply is not contributed (economy.md).
		//
		// ⛔ A COMMERCE RECEIVER'S PER-CITY QUANTITY IS THE WHOLE §2a SPLIT, NEVER THE CHANNEL'S DEPOSITS ALONE
		// (state-repositories.md § A CROSS-SCOPE receiver total). A city does not RECEIVE gold/research/culture/
		// espionage as four independent channels -- it receives the COMMERCE yield, and the EMPIRE'S SLIDERS
		// divide that yield across the four, each adding its own deposits and the process conversion. So the
		// member value is the city's realized COMMERCE group read, which is the ONE implementation of that split
		// ([DEC-single-implementation]); cityReceiverRate answers the deposits leg and is the wrong member here.
		// ⚑ Without this the sliders moved nothing at the empire: the Σ summed a yield-shaped combine over the
		// gold channel, which the slider is not an input to.
		const int iCommerceIndex = val_commerceIndexOfChannel(iChannel);
		int64_t iTotal = 0;
		for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
		{
			const CvCity* pCity = *cityIterator;
			if (pCity != NULL && !pCity->isDisorder())
			{
				if (iCommerceIndex >= 0)
				{
					int aiCityCommerces[NUM_COMMERCE_TYPES];
					pCity->getCommerces(aiCityCommerces);
					iTotal += aiCityCommerces[iCommerceIndex];
				}
				else
				{
					iTotal += cityReceiverRate(*pCity, iChannel);
				}
			}
		}
		return (int)iTotal;
	}
	// AREA is deliberately absent from the empire chain: an area-scope value belongs to ONE area while an empire
	// spans several, so it is read by each CITY for its own area id -- folding it here would credit every city
	// with every area's deposits.
	int64_t iFlatSum = 0;
	int64_t iPercentSum = 0;
	// The TEAM leg exists only while the player is on one (an unassigned player slot is on none), so its chain
	// is its own package alone rather than an invalid index.
	if (player.getTeam() != NO_TEAM)
	{
		const CvTeam& team = GET_TEAM(player.getTeam());
		iFlatSum += team.getCascadePackage().readFlat(iChannel);
		iPercentSum += team.getCascadePackage().readPercent(iChannel);
	}
	iFlatSum += player.getCascadePackage().readFlat(iChannel);
	iPercentSum += player.getCascadePackage().readPercent(iChannel);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_EMPIRE));
}

int InfoValuation::realizedAtTeam(const CvTeam& team, int iChannel)
{
	if (iChannel < 0)
	{
		return 0;
	}
	// TEAM is the top scope carrying a package (WORLD is CONFIG and has none -- state-repositories.md), so its
	// chain is itself alone.
	const int64_t iFlatSum = team.getCascadePackage().readFlat(iChannel);
	const int64_t iPercentSum = team.getCascadePackage().readPercent(iChannel);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_TEAM));
}

void InfoValuation::expectedFlatYields(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&flatYields)[NUM_YIELD_TYPES],
	const CvCascadeHypothetical* pHypothetical)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		flatYields[iYield] = (int)groupSum(modifiers, infoYieldFamily(iYield), CHANNEL_AMOUNT, CASC_UNIT_FLAT, evalCtx);
	}
}

void InfoValuation::expectedYieldModifiers(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&yieldModifiers)[NUM_YIELD_TYPES],
	const CvCascadeHypothetical* pHypothetical)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		yieldModifiers[iYield] = (int)groupSum(modifiers, infoYieldFamily(iYield), CHANNEL_AMOUNT, CASC_UNIT_PERCENT, evalCtx);
	}
}

void InfoValuation::expectedPlotYields(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&plotYields)[NUM_YIELD_TYPES],
	const CvCascadeHypothetical* pHypothetical)
{
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		plotYields[iYield] = 0;
	}
	if (modifiers == NULL)
	{
		return;
	}
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		// leg 1: the plots-TARGET deposits, scaled by the plotAttrs counts (a building's radius buff);
		// leg 2: the info's OWN untargeted plot-scope output (the substrate leg -- what this
		// terrain/feature/improvement/route itself yields on the ctx's target plot).
		plotYields[iYield] = (int)val_plotsTarget(modifiers, infoYieldFamily(iYield), evalCtx, cityContext)
		                      + (int)plotOwnYield(modifiers, infoYieldFamily(iYield), evalCtx);
	}
}

void InfoValuation::expectedFlatCommerce(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&flatCommerce)[NUM_COMMERCE_TYPES],
	const CvCascadeHypothetical* pHypothetical)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		flatCommerce[iCommerce] = (int)groupSum(modifiers, infoCommerceFamily(iCommerce), CHANNEL_AMOUNT, CASC_UNIT_FLAT, evalCtx);
	}
}

void InfoValuation::expectedWellbeing(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&wellbeing)[NUM_WELLBEING_CHANNELS],
	const CvCascadeHypothetical* pHypothetical)
{
	for (int iChannel = 0; iChannel < NUM_WELLBEING_CHANNELS; ++iChannel)
	{
		wellbeing[iChannel] = 0;
	}
	if (modifiers == NULL)
	{
		return;
	}
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	const bool bAiAudience = val_aiAudience(evalCtx);
	// The two AUTHORED families, each sign-routed onto its channel pair (modifier.md §2b: a negative deposit is
	// routed to the opposing channel -- at THIS fill; the info's storage stays the two signed families).
	// Contribution granularity: the collapsed unconditioned slot sum routes by its NET sign per scope (the
	// per-entry split is preserved across the conditioned tail, which routes entry by entry).
	const ModifierFamily FAMILY_OF[2] = { MODFAM_HAPPINESS, MODFAM_HEALTH };
	const WellbeingChannel GOOD_OF[2] = { WELLBEING_HAPPINESS, WELLBEING_HEALTH };
	const WellbeingChannel BAD_OF[2]  = { WELLBEING_ANGER, WELLBEING_UNHEALTH };
	for (int iPair = 0; iPair < 2; ++iPair)
	{
		const ModifierFamily eFamily = FAMILY_OF[iPair];
		// (1) the compiled unconditioned sums per folded scope, net-sign-routed
		for (int iScopeIdx = 0; iScopeIdx < NUM_VAL_FOLD_SCOPES; ++iScopeIdx)
		{
			const int iSum = modifiers->sum(eFamily, CHANNEL_AMOUNT, VAL_FOLD_SCOPES[iScopeIdx], CASC_UNIT_FLAT, bAiAudience);
			if (iSum >= 0)
			{
				wellbeing[GOOD_OF[iPair]] += iSum;
			}
			else
			{
				wellbeing[BAD_OF[iPair]] -= iSum;
			}
		}
		// (2) the conditioned tail, routed entry by entry
		size_t iBegin = 0;
		size_t iEnd = 0;
		modifiers->conditionedRange(eFamily, iBegin, iEnd);
		const std::vector<const CvModEntry*>& conditioned = modifiers->conditioned();
		for (size_t i = iBegin; i < iEnd; ++i)
		{
			const CvModEntry* pEntry = conditioned[i];
			if (pEntry->kind != (int)CHANNEL_AMOUNT || pEntry->unit != CASC_UNIT_FLAT)
			{
				continue;
			}
			if (pEntry->targetSeg >= 0 || pEntry->targetFk >= 0)
			{
				continue;
			}
			if (!val_scopeFolds(pEntry->scope))
			{
				continue;
			}
			if (pEntry->unitQual != NULL)
			{
				continue;   // unit-carried wellbeing rides ON TOP live ([DEC-unit-modifiers-on-top])
			}
			if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
			{
				continue;
			}
			if (!MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
			{
				continue;
			}
			const int64_t iValue = MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
			if (iValue >= 0)
			{
				wellbeing[GOOD_OF[iPair]] += (int)iValue;
			}
			else
			{
				wellbeing[BAD_OF[iPair]] -= (int)iValue;
			}
		}
	}
}

int InfoValuation::expectedSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	const CvCascadeHypothetical* pHypothetical)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	// The what-if rides the ctx the contexts filled: it overrides individual have-atom answers and nothing else,
	// so every other read the conditioned tail makes stays the real city's.
	evalCtx.hypothetical = pHypothetical;
	return (int)groupSum(modifiers, eFamily, iKind, eUnit, evalCtx);
}

int64_t InfoValuation::keyedTargetSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
	int iTargetSegment, int iTargetFk, const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL || iTargetFk < 0 || iTargetSegment == TARGET_SEGMENT_NONE)
	{
		return 0;
	}
	// (1) the UNCONDITIONED half, through the ONE keyed read -- no second matcher beside it.
	int64_t iTotal = keyedTarget(modifiers, eFamily, iKind, iTargetSegment, iTargetFk);
	// (2) the CONDITIONED tail, the groupSum walk with the target axis matched instead of skipped. A keyed
	// entry is in the conditioned list exactly as an untargeted one is, so the only difference from the point
	// form is WHICH entries are taken.
	size_t iBegin = 0;
	size_t iEnd = 0;
	modifiers->conditionedRange(eFamily, iBegin, iEnd);
	const std::vector<const CvModEntry*>& conditioned = modifiers->conditioned();
	for (size_t i = iBegin; i < iEnd; ++i)
	{
		const CvModEntry* pEntry = conditioned[i];
		if (pEntry->targetSeg != iTargetSegment || pEntry->targetFk != iTargetFk)
		{
			continue;
		}
		if (iKind >= 0 && pEntry->kind != iKind)
		{
			continue;
		}
		if (!val_scopeFolds(pEntry->scope))
		{
			continue;   // the experienced-here fold set, exactly as the point form (modifier.md §1)
		}
		if (pEntry->unitQual != NULL)
		{
			continue;   // unit-carried values ride ON TOP live ([DEC-unit-modifiers-on-top])
		}
		if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
		{
			continue;
		}
		if (!MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
		{
			continue;
		}
		iTotal += MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
	}
	return iTotal;
}

int InfoValuation::expectedKeyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
	int iTargetSegment, int iTargetFk,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	const CvCascadeHypothetical* pHypothetical)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	evalCtx.hypothetical = pHypothetical;
	return (int)keyedTargetSum(modifiers, eFamily, iKind, iTargetSegment, iTargetFk, evalCtx);
}

// ---- the §2a fold seams (see the header docs; pure statics, inputs in / ×100 out) ----

int64_t InfoValuation::tradeRouteChannelYield(int64_t routeYield, int iChannelBasePercent, int iChannelModifierPercentSum)
{
	// modifier.md §2a (the tradeYield BASE input fold), ruling 27: the per-channel modifier rides ON TOP of the
	// engine's incoming route yield -- profit × (base% + Σmod) / 100, the channel identity on the base percent
	// (commerce 100, food/production 0 -- CvYieldInfo::getTradeModifier; engine CvCity::calculateTradeYield).
	//
	// ⛔ THIS FUNCTION IS AN EDGE, which is why the two scales differ here and why the conversion belongs HERE.
	// tradeYield is the ONE sanctioned live-yield INPUT (modifier.md §2a): the cascade cannot re-derive the
	// trade NETWORK, so that calculation stays engine-owned (north-star.md KEEP -- it is none of the four
	// systems' job) and its value is FOLDED IN rather than computed. So one operand arrives from OUTSIDE the
	// cascade on the engine's PLAIN scale (iChannelBasePercent = CvYieldInfo::getTradeModifier, 100 / 0) and the
	// other from INSIDE it on the stored ×100 scale (iChannelModifierPercentSum = getTradeRouteYieldModifier).
	// An EDGE CONVERTS -- the standing rule, not a local one: readJson converts at the IN boundary, a reader ÷100s
	// at the OUT boundary, and ×100 is native everywhere between ([DEC-fixedpoint-x100]). So the base is lifted to
	// ×100 and one ÷10000 takes both back down. Converting at the edge means a caller passes what it HAS and
	// cannot get the scale wrong; pushing it outward would put a scale contract on every consumer of the fold,
	// which is how a +50% modifier becomes +5000%.
	// Both are PERCENTS and a percent is NOT scaled ([DEC-fixedpoint-x100]), so they combine directly against
	// the ordinary 100 identity. ⛔ The old shape lifted the base to ×100 and reduced by 10000, applying every
	// authored modifier at a hundredth of its value -- the second identity constant fixed-point-and-scales.md §1
	// says must never appear. cityRate, in this same file, is the correct reference.
	const int64_t iCombinedPercent = (int64_t)iChannelBasePercent + (int64_t)iChannelModifierPercentSum;
	return routeYield * iCombinedPercent / 100;
}

int64_t InfoValuation::combinedGroupSum(ModifierFamily eFamily, int iKind, int64_t sum)
{
	// modifier.md §2: non-additive combine modes are FAMILY metadata -- the ruled floor rows live beside the
	// vocabulary (infoCombineFloorAtZero); this is the ONE site that applies them to a summed group total.
	if (sum < 0 && infoCombineFloorAtZero(eFamily, iKind))
	{
		return 0;
	}
	return sum;
}

int64_t InfoValuation::netUpkeepAfterFree(int64_t upkeep, int64_t freeAmount)
{
	// ruling 28's ENGINE-side second floor: net class upkeep = max(0, upkeep − free). `freeAmount` arrives
	// already group-floored (combinedGroupSum) -- the two floors are distinct by design.
	const int64_t iNet = upkeep - freeAmount;
	return iNet > 0 ? iNet : 0;
}

int InfoValuation::resolvedCityLimit(int iBaseCityLimit)
{
	// ruling 26: the engine-side resolved limit -- the civic's identity.cityLimit base × the world-size scale
	// percent, ONLY under the overexpansion-penalties option (the archived CvCivicInfo::getCityLimit contract:
	// option off -> 0 -> no limit anywhere). The cascade's own CITY_LIMIT eval leg applies the same scale with
	// the option riding the authored deposit's `enabled` -- one formula, two entry doors, both declared here.
	if (iBaseCityLimit <= 0)
	{
		return 0;
	}
	if (!GC.getGame().isOption(GAMEOPTION_EXP_OVEREXPANSION_PENALTIES))
	{
		return 0;
	}
	return iBaseCityLimit * GC.getWorldInfo(GC.getMap().getWorldSize()).getCityLimitsScalePercent() / 100;
}


int InfoValuation::netHappiness(const int (&wellbeing)[NUM_WELLBEING_CHANNELS])
{
	// modifier.md §2b: happiness sums AGAINST anger. The result is SIGNED -- a surplus of happy faces is as
	// meaningful as a deficit, and clamping belongs to whichever final-state value wants it (angryPopulation
	// clamps the deficit; the AI reads the surplus).
	return (wellbeing[WELLBEING_HAPPINESS] - wellbeing[WELLBEING_ANGER]) / 100;
}


int InfoValuation::netHealth(const int (&wellbeing)[NUM_WELLBEING_CHANNELS])
{
	// modifier.md §2b: health sums AGAINST unhealth. Signed, for the same reason.
	return (wellbeing[WELLBEING_HEALTH] - wellbeing[WELLBEING_UNHEALTH]) / 100;
}

void InfoValuation::fillDoubleMoveTargets(const CvModifiers* modifiers,
	std::vector<int>& terrainTargets, std::vector<int>& featureTargets)
{
	terrainTargets.clear();
	featureTargets.clear();

	if (modifiers == NULL)
	{
		return;
	}
	// A memberless keyed address compiles to KIND 0 (CvModifiers.cpp mod_decodeLeaf), never a negative --
	// the COLLECT form matches the kind EXACTLY, so a wildcard would read nothing here.
	std::vector<std::pair<int, int> > rows;

	collectKeyedTarget(modifiers, MODFAM_MOVEMENT, 0, keyedTargetSegment("terrain"), rows);
	for (size_t iRow = 0; iRow < rows.size(); ++iRow)
	{
		terrainTargets.push_back(rows[iRow].first);
	}
	rows.clear();

	collectKeyedTarget(modifiers, MODFAM_MOVEMENT, 0, keyedTargetSegment("feature"), rows);
	for (size_t iRow = 0; iRow < rows.size(); ++iRow)
	{
		featureTargets.push_back(rows[iRow].first);
	}
}
