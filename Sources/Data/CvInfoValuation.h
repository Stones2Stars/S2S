#pragma once
#ifndef CV_INFO_VALUATION_H
#define CV_INFO_VALUATION_H

//
//	InfoValuation -- the ONE per-GROUP what-if valuation walk + the §2a fold seams (patterns.md § THE GETTER
//	SETUP category 3; contexts.md § The read). The rebuilt infos' `expected*` endpoints DELEGATE here
//	([DEC-single-implementation] -- no per-type reimplementation): each endpoint answers *"what do I gain from
//	this entity HERE?"* as the compiled unconditioned ×100 sums fetched straight PLUS the group's conditioned
//	tail through the ONE evaluator (MMKernel::applies over a CvCascadeEvalCtx the CONTEXTS fill), `plots`-target
//	deposits scaled by `cityContext.plotAttrs` counts, per-scalers resolved through the ONE §3.7 resolver
//	(MMKernel::perScale, entry carrier), the SCOPE AXIS FOLDED into the experienced-here answer (the §1 scope
//	principle: a city experiences the sum of the world/team/empire/area/city packages), and the AUDIENCE
//	resolved from the asking player (json §3.9 `ai`).
//
//	⛔ A what-if NEVER evaluates `requires`: the entity-level active/dormant verdict is the ENABLER's and is FED
//	IN via the precomputed operating set (EnablerKernel::wireOperatingBuildings fills the eval ctx's
//	activeBuildings/vicinityProvidedBonuses) -- the walk evaluates only the deposits' own §3.9 conditions.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

#include "CvInfoKinds.h"                  // ModifierFamily / kind enums / WellbeingChannel / CvCascUnit / CvCascScope
#include "Defines/CvEnums.h"              // NUM_YIELD_TYPES / NUM_COMMERCE_TYPES -- the group out-array extents

class CvModifiers;
class CityContext;
class EmpireContext;
class CvPlotGroup;
struct CvCascadeEvalCtx;

class InfoValuation
{
public:
	// THE EVAL-CTX FILL SEAM (contexts.md: "the contexts ARE the eval state"): CityContext::fillEvalCtx
	// (city/plot) + EmpireContext::fillEvalCtx (player/team) + the plotGroup pass-in (the reserved TRADED-bonus
	// source -- evalCtx.plotGroup; NULL = the city's own plot-group-backed reads answer trade) + the enabler's
	// standing operating set wired in (EnablerKernel::wireOperatingBuildings -- the FED-IN active/dormant
	// verdict). Every expected* endpoint builds its ctx through this one seam; no caller assembles a
	// raw-pointer ctx beside the contexts.
	static void fillEvalCtx(const CityContext& cityContext, const EmpireContext& empireContext,
		const CvPlotGroup* plotGroup, CvCascadeEvalCtx& evalCtx);

	// ---- the per-GROUP what-if endpoints (contexts.md: one endpoint per GROUP of channels, never per single
	// ---- channel; each fills its group's ×100 out-array -- contexts in, expected values out). The per-info
	// ---- endpoints (CvBuildingInfo::expectedFlatYields et al.) are one-line delegations onto these.

	// The three yield channels' FLAT group ({food,production,commerce}.<scope>.flat).
	static void expectedFlatYields(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&flatYields)[NUM_YIELD_TYPES]);
	// The three yield channels' PERCENT group ({food,production,commerce}.<scope>.percent -- the §2a additive stack's slice).
	static void expectedYieldModifiers(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&yieldModifiers)[NUM_YIELD_TYPES]);
	// The PLOT yield group, two legs summed per channel: (1) the `plots`-target deposits ({channel}.<scope>.plots
	// -- json §6.1), each value × the count of the city's radius plots matching its predicate, read from
	// cityContext.plotAttrs (COUNTS, never objects); (2) the info's OWN untargeted plot-scope output
	// (plotOwnYield -- the substrate leg: what this terrain/feature/improvement/route itself yields on the
	// ctx's target plot, the city centre under the city fill; a specific-plot ask calls plotOwnYield /
	// plotBaseYields with a plot-bound ctx directly).
	static void expectedPlotYields(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&plotYields)[NUM_YIELD_TYPES]);
	// The four commerce channels' FLAT group ({gold,research,culture,espionage}.<scope>.flat).
	static void expectedFlatCommerce(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&flatCommerce)[NUM_COMMERCE_TYPES]);
	// The four wellbeing channels (modifier.md §2b): the authored happiness/health deposits, each contribution
	// SIGN-ROUTED to its channel (a negative happiness value lands as positive anger -- a routing rule, never a
	// storage shape). Every channel of the out-array carries a POSITIVE magnitude.
	static void expectedWellbeing(const CvModifiers* modifiers,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup,
		int (&wellbeing)[NUM_WELLBEING_CHANNELS]);

	// The GROUPED/SCALAR-family analogue (the same walk for one (family, kind, unit) slot): the experienced-here
	// ×100 answer for a defense kind, a maintenance modifier, an InfoScalar slot, ... -- the per-info
	// expectedScalar/expectedModifier endpoints delegate here with their vocabulary axes spelled out.
	static int expectedSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		const CityContext& cityContext, const EmpireContext& empireContext, const CvPlotGroup* plotGroup);

	// The ctx-taking core the endpoints share (fill the ctx ONCE per endpoint, fold each channel through this):
	// Σ over the folded scopes of the compiled unconditioned sums (audience-resolved) + the family's conditioned
	// tail (kind/unit-matched, untargeted, gated via the ONE evaluator, per-resolved via the ONE §3.7 resolver).
	static long groupSum(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		const CvCascadeEvalCtx& evalCtx);

	// The SCOPE-RESTRICTED sibling of groupSum -- ONE info's (family, kind, unit) contribution AT one scope
	// (unconditioned slot sum + the conditioned tail at that scope). The package plane's per-source fold and
	// the receiver combine's specialist term read through this ([DEC-single-implementation]: the gather never
	// grows a second per-info fold beside the valuation's).
	static long groupSumAt(const CvModifiers* modifiers, ModifierFamily eFamily, int iKind, CvCascUnit eUnit,
		CvCascScope eScope, const CvCascadeEvalCtx& evalCtx);

	// ONE substrate info's OWN untargeted PLOT-scope output of a channel family, for the ctx's TARGET plot
	// (evalCtx.plot): the compiled plot-slot unconditioned sum + the plot-scope conditioned tail -- incl. the
	// reverse-landed conditioned entries (a building/civic/tech boost keyed to this substrate lands HERE,
	// CvReversePass) -- evaluated under PLOT-EVAL semantics (a bare {HAS_BONUS:X} reads THIS plot,
	// bonusFromPlot; the groupSumAt sibling evaluates city-relative and cannot serve the substrate leg).
	static long plotOwnYield(const CvModifiers* modifiers, ModifierFamily eFamily, const CvCascadeEvalCtx& evalCtx);

	// THE ISOLATED PLOT-AS-BASE CALC (modifier.md §2 plot note -- "a plot's yield is ONE base package, resolved
	// in isolation BEFORE the city modifiers"; the wave-B substrate valuation leg): the target plot's base-yield
	// package from the substrate infos' compiled entries. Per channel:
	//   nature = max(0, terrain + feature + bonus own-output)  ·  improvement floored at −nature  ·  + route  ·
	//   max(0, total)
	// A NULL substrate slot contributes 0 (no feature / no improvement / no route / no bonus). The plot package
	// rebuild AND every what-if plot read call THIS one function ([DEC-single-implementation]); the caller
	// passes each present substrate's getModifiers() and a ctx whose `plot` is the target plot.
	static void plotBaseYields(const CvModifiers* terrainModifiers, const CvModifiers* featureModifiers,
		const CvModifiers* bonusModifiers, const CvModifiers* improvementModifiers, const CvModifiers* routeModifiers,
		const CvCascadeEvalCtx& evalCtx, int (&plotYields)[NUM_YIELD_TYPES]);

	// THE CITY RATE COMBINE (modifier.md §2a -- the sharp two-tier shape, the order load-bearing):
	//   rate100 = (BASE + specialists) x max(0, 100 + percentSum)/100 + 100 x (EXTRA100 / 100)
	// BASE = the worked-plot Σ + the upper-scope flats rolled down at the combine; the specialist term carries
	// its OWN percent layer before joining BASE; EXTRA is the building-flat tier, truncated to whole units
	// before re-scaling (the engine's documented integer-truncation order, mirrored verbatim). The receiver
	// sum rebuild (CascadeGather) and every realized-rate consumer call THIS one function -- the combine
	// exists once, on the calc surface.
	static long cityRate(long base, long specialists, int iPercentSum, long extra);

	// ---- the §2a fold seams (their canonical calc functions live HERE so the future package rebuild and the
	// ---- expected* endpoints call the SAME math -- pure statics: inputs in, ×100 out).

	// THE PER-CHANNEL tradeRoutes.modifier FOLD (modifier.md §2a "trade-route yield" BASE input; ruling 27):
	// realized channel trade yield = routeYield × (channelBase% + Σ tradeRoutes.modifier.<channel>) / 100.
	// channelBase% is the channel's engine identity (CvYieldInfo::getTradeModifier -- commerce 100,
	// food/production 0); the modifier sum is the per-channel TRADE_ROUTE_MODIFIER_FOOD+eYield kind, summed over
	// the player's live sources. Engine transcription: CvCity::calculateTradeYield (profit × mod[ch] / 100).
	static long tradeRouteChannelYield(long routeYield, int iChannelBasePercent, int iChannelModifierPercentSum);

	// THE FAMILY-COMBINE FLOOR (modifier.md §2 "a `min` member that floors the combined total" -- declared as
	// FAMILY metadata, never per-deposit): apply the (family, kind) slot's ruled combine floor to a summed group
	// total. Consumes infoCombineFloorAtZero (ruling 28: upkeep.freeMilitary/freeCivilian -- Σfree floored at 0
	// as a GROUP; signed free-amount entries sum first, the floor applies once here).
	static long combinedGroupSum(ModifierFamily eFamily, int iKind, long sum);

	// THE FREE-UPKEEP NET (modifier.md §2a sibling; ruling 28's second, ENGINE-side floor): net class upkeep =
	// max(0, upkeep − free). `freeAmount` is the ALREADY-FLOORED group total (combinedGroupSum above) -- the two
	// floors are distinct by design (free >= 0, then net >= 0; engine CvPlayer.cpp:10295/:10315).
	static long netUpkeepAfterFree(long upkeep, long freeAmount);

	// THE RESOLVED CITY LIMIT (ruling 26): the engine-side read of a civic's base city-limit config -- base ×
	// the world-size scale percent, gated on GAMEOPTION_EXP_OVEREXPANSION_PENALTIES exactly as the archived
	// engine getter (0 when the option is off or the civic carries no limit). The per.above CITY_LIMIT eval leg
	// (MMKernel::perApply) applies the SAME scale; its option gate rides the authored deposit's `enabled`.
	static int resolvedCityLimit(int iBaseCityLimit);
};

#endif // CV_INFO_VALUATION_H
