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
		return evalCtx.player != NULL && !evalCtx.player->isHuman();
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
	long val_plotsTarget(const CvModifiers* modifiers, ModifierFamily eFamily,
		const CvCascadeEvalCtx& evalCtx, const CityContext& cityContext)
	{
		const int iPlotsSeg = modSegmentLookup("plots");
		if (iPlotsSeg < 0)
		{
			return 0;   // never authored anywhere
		}
		long iTotal = 0;
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
			iTotal += (long)pEntry->value * val_plotPredicateCount(pEntry->enabled, cityContext);
		}
		return iTotal;
	}
}



namespace
{
	//	The keyed-combat axis -> its interned segment id, resolved ONCE per axis.
	int keyedCombatSegment(InfoValuation::CombatTargetAxis eAxis)
	{
		static int s_segments[5] = { -2, -2, -2, -2, -2 };   // -2 = not yet resolved, -1 = never authored
		if (s_segments[eAxis] == -2)
		{
			static const char* const kAxisSegments[5] = { "terrain", "feature", "unitCombat", "vsUnit", "domain" };
			s_segments[eAxis] = modSegmentLookup(kAxisSegments[eAxis]);
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

int InfoValuation::keyedTargetSegment(const char* szTargetSegment)
{
	return modSegmentLookup(szTargetSegment);
}
void InfoValuation::collectKeyedTarget(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind,
	int iTargetSegment, std::vector<std::pair<int, int> >& targetValues)
{
	targetValues.clear();
	if (modifiers == NULL || iTargetSegment < 0)
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
		|| (iKind >= 0 && kEntry.kind != iKind))
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
	if (modifiers == NULL || iTargetFk < 0 || iTargetSegment < 0)
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
		&& (iKind < 0 || kEntry.kind == iKind))
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

void InfoValuation::fillEvalCtx(const CityContext& cityContext, const EmpireContext& empireContext,
	const CvPlotGroup* plotGroup, CvCascadeEvalCtx& evalCtx)
{
	cityContext.fillEvalCtx(evalCtx);     // city + plot
	empireContext.fillEvalCtx(evalCtx);   // player + team
	// the reserved TRADED-bonus source (contexts.md): a city-bound ctx answers connection:"trade" through the
	// city's own plot-group-backed maintained count; this explicit pass-in serves the city-less what-if.
	evalCtx.plotGroup = plotGroup;
	if (evalCtx.city != NULL)
	{
		// the FED-IN entity verdict (patterns.md what-if driver): the enabler's standing operating set --
		// active buildings + vicinity-provided bonuses -- so building-presence/vicinity predicates read the
		// cascade-computed state; the walk itself never evaluates requires.
		EnablerKernel::wireOperatingBuildings(evalCtx.city, evalCtx);
	}
}

long InfoValuation::groupSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	long iTotal = 0;
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

long InfoValuation::groupSumAt(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
	CvCascScope eScope, const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	long iTotal = modifiers->sum(eFamily, iKind, eScope, eUnit, bAiAudience);
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

long InfoValuation::plotOwnYield(const CvModifiers* modifiers, ModifierFamily eFamily, const CvCascadeEvalCtx& evalCtx)
{
	if (modifiers == NULL)
	{
		return 0;
	}
	const bool bAiAudience = val_aiAudience(evalCtx);
	// (1) the compiled unconditioned PLOT slot, fetched straight
	long iTotal = modifiers->sum(eFamily, CHANNEL_AMOUNT, CASC_SCOPE_PLOT, CASC_UNIT_FLAT, bAiAudience);
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
	const CvCascadeEvalCtx& evalCtx, int (&plotYields)[NUM_YIELD_TYPES])
{
	// THE ISOLATED PLOT-AS-BASE CALC (modifier.md §2 plot note; legacy decomposition calc-map §10.1): per
	// channel -- nature = max(0, terrain + feature + bonus own-output); the improvement floored at −nature
	// (an improvement can consume the nature yield, never drive the pre-route base negative); + route; the
	// whole package floored at 0. Every term is the substrate's own untargeted plot-scope output
	// (plotOwnYield), so the reverse-landed conditioned boosts ride each term automatically.
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		const ModifierFamily eFamily = infoYieldFamily(iYield);
		long iNature = plotOwnYield(terrainModifiers, eFamily, evalCtx)
		                + plotOwnYield(featureModifiers, eFamily, evalCtx)
		                + plotOwnYield(bonusModifiers, eFamily, evalCtx);
		if (iNature < 0)
		{
			iNature = 0;
		}
		long iImprovement = plotOwnYield(improvementModifiers, eFamily, evalCtx);
		if (iImprovement < -iNature)
		{
			iImprovement = -iNature;   // floored at −nature (modifier.md §2a basePlotYield row)
		}
		long iTotal = iNature + iImprovement + plotOwnYield(routeModifiers, eFamily, evalCtx);
		if (iTotal < 0)
		{
			iTotal = 0;
		}
		plotYields[iYield] = (int)iTotal;
	}
}

long InfoValuation::cityRate(long base, long specialists, int iPercentSum, long extra)
{
	// modifier.md §2a: ONE additive percent stack applied once, floored at zero; the EXTRA tier truncates to
	// whole units before re-scaling (the engine's getExtraYield100 order -- a documented integer-truncation
	// gotcha mirrored verbatim, never "fixed").
	//
	// ⛔ SCALE CONTRACT: iPercentSum is a PLAIN percent (25 = +25%), NOT the ×100 stored sum -- the caller
	// divides before calling. It differs deliberately from realizedChannel below, which takes the ×100 sum and
	// de-scales internally, so the two neighbours on this surface do NOT share a convention: stated here because
	// passing the stored sum instead would read +25% as +2500% and the signature alone cannot say which is
	// wanted (no name carries a scale, [DEC-fixedpoint-x100]).
	long iModifier = 100 + (long)iPercentSum;
	if (iModifier < 0)
	{
		iModifier = 0;
	}
	return (base + specialists) * iModifier / 100 + 100 * (extra / 100);
}

long InfoValuation::commerceSplit(long commerceYieldRate, int iSliderPercent, long channelPercentSum,
	long channelDeposits, long productionYieldRate, int iProductionToCommerce)
{
	// TIER 1 -- the slider share of the COMMERCE yield. The slider is a plain 0..100 counter (json §3.1), so the
	// ÷100 is the percent-to-fraction conversion, not a fixed-point de-scale.
	long iShare = commerceYieldRate * (long)iSliderPercent / 100;
	// The ONE additive stack (modifier.md §2a), floored at zero exactly as the yield rate's is -- a stack below
	// -100% zeroes the share, it never flips its sign. The stored percents are ×100, hence the second ÷100.
	long iModifier = 100 + channelPercentSum / 100;
	if (iModifier < 0)
	{
		iModifier = 0;
	}
	// TIER 2 -- the process conversion, added AFTER the percentages and never multiplied by them. The production
	// yield truncates to whole hammers BEFORE the conversion scales it (the engine's order, mirrored verbatim);
	// the conversion rate is ×100, so it de-scales to the authored human percent.
	long iProcessConversion = (productionYieldRate / 100) * ((long)iProductionToCommerce / 100);
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

long InfoValuation::realizedChannel(long flatSum, long percentSum, CvCascUnit eCanonicalUnit)
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
	long iModifier = 100 + percentSum / 100;
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
	const long iFlatSum = plot.getCascadePackage().readFlat(iChannel);
	const long iPercentSum = plot.getCascadePackage().readPercent(iChannel);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_PLOT));
}

void InfoValuation::rolledLegsAtCity(const CvCity& city, int iChannel, long& flatSum, long& percentSum)
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
	long iFlatSum = 0;
	long iPercentSum = 0;
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
	long iFlatSum = 0;
	long iPercentSum = 0;
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
	const long iFlatSum = team.getCascadePackage().readFlat(iChannel);
	const long iPercentSum = team.getCascadePackage().readPercent(iChannel);
	return (int)realizedChannel(iFlatSum, iPercentSum, val_channelCanonicalUnit(iChannel, CASC_SCOPE_TEAM));
}

void InfoValuation::expectedFlatYields(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&flatYields)[NUM_YIELD_TYPES])
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		flatYields[iYield] = (int)groupSum(modifiers, infoYieldFamily(iYield), CHANNEL_AMOUNT, CASC_UNIT_FLAT, evalCtx);
	}
}

void InfoValuation::expectedYieldModifiers(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&yieldModifiers)[NUM_YIELD_TYPES])
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		yieldModifiers[iYield] = (int)groupSum(modifiers, infoYieldFamily(iYield), CHANNEL_AMOUNT, CASC_UNIT_PERCENT, evalCtx);
	}
}

void InfoValuation::expectedPlotYields(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&plotYields)[NUM_YIELD_TYPES])
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
	int (&flatCommerce)[NUM_COMMERCE_TYPES])
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		flatCommerce[iCommerce] = (int)groupSum(modifiers, infoCommerceFamily(iCommerce), CHANNEL_AMOUNT, CASC_UNIT_FLAT, evalCtx);
	}
}

void InfoValuation::expectedWellbeing(const CvModifiers* modifiers,
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
	int (&wellbeing)[NUM_WELLBEING_CHANNELS])
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
			const long iValue = MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
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
	const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup)
{
	CvCascadeEvalCtx evalCtx;
	fillEvalCtx(cityContext, empireContext, plotGroup, evalCtx);
	return (int)groupSum(modifiers, eFamily, iKind, eUnit, evalCtx);
}

// ---- the §2a fold seams (see the header docs; pure statics, inputs in / ×100 out) ----

long InfoValuation::tradeRouteChannelYield(long routeYield, int iChannelBasePercent, int iChannelModifierPercentSum)
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
	const long iCombinedPercent100 = (long)iChannelBasePercent * 100 + (long)iChannelModifierPercentSum;
	return routeYield * iCombinedPercent100 / 10000;
}

long InfoValuation::combinedGroupSum(ModifierFamily eFamily, int iKind, long sum)
{
	// modifier.md §2: non-additive combine modes are FAMILY metadata -- the ruled floor rows live beside the
	// vocabulary (infoCombineFloorAtZero); this is the ONE site that applies them to a summed group total.
	if (sum < 0 && infoCombineFloorAtZero(eFamily, iKind))
	{
		return 0;
	}
	return sum;
}

long InfoValuation::netUpkeepAfterFree(long upkeep, long freeAmount)
{
	// ruling 28's ENGINE-side second floor: net class upkeep = max(0, upkeep − free). `freeAmount` arrives
	// already group-floored (combinedGroupSum) -- the two floors are distinct by design.
	const long iNet = upkeep - freeAmount;
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
