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
		return city.getCascadePackage().readSum(iChannel);
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
		return player.getCascadePackage().readSum(iChannel);
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
	const int64_t iCombinedPercent100 = (int64_t)iChannelBasePercent * 100 + (int64_t)iChannelModifierPercentSum;
	return routeYield * iCombinedPercent100 / 10000;
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
