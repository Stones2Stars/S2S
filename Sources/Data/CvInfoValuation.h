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
// the scope objects the cross-scope roll-up reads packages off (declarations only -- the engine headers stay
// out of this header; the roll-up bodies include them)
class CvPlot;
class CvCity;
class CvPlayer;
class CvTeam;
class CvArea;

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

	// THE PER-COMMERCE SPLIT (modifier.md §2a's commerce paragraph -- the §2a two-tier shape carried by the
	// channel's own pieces; StoneBase's CommerceSplit calc package). A city's realized gold / research / culture /
	// espionage rate is NOT a fifth receiver slot: the city receives the COMMERCE YIELD, and the empire's slider
	// percentages split that yield across the four channels, each channel adding its own deposits on top.
	//   rate100 = commerceYieldRate x sliderPercent/100 x max(0, 100 + channelPercentSum/100)/100
	//             + channelDeposits
	//             + (productionYieldRate/100) x (productionToCommerce/100)
	// TIER 1 (BASE, multiplied by the ONE additive stack): the slider share of the commerce yield AND the §2a
	// baseExtra sub-terms (religion / corporation / golden-age / player-extra / the building-commerce block) --
	// which reach this function ALREADY SCALED, inside `channelDeposits`, because the cross-scope roll-up applies
	// the channel's own stack to the channel's own flats (realizedAtCity). `channelPercentSum` is therefore
	// supplied for the SLIDER SHARE alone, so the share meets the same stack the deposits already met.
	// TIER 2 (EXTRA, added AFTER the percentages, never multiplied): the process conversion -- the production
	// yield truncated to whole hammers first, then scaled by the process's own conversion rate (the engine's
	// truncation order, mirrored verbatim).
	// SCALES: every magnitude is ×100 EXCEPT `iSliderPercent`, which is the player's plain slider counter 0..100
	// (json §3.1's GOLD_RATE / RESEARCH_RATE / CULTURE_RATE / ESPIONAGE_RATE tokens, read through
	// EmpireContext::commerceRates); `channelPercentSum` and `productionToCommerce` are ×100 as stored, so both
	// carry their own ÷100 here.
	// ⚠ CULTURE is the lone dual consumer: its `channelDeposits` is the city's MAINTAINED RECEIVER SUM (the
	// combine the gather already wrote), not a roll-up -- so the receiver sum passes through untouched and only
	// the commerce yield is slider-scaled. Scaling a receiver sum by a slider would re-scale a realized total.
	static long commerceSplit(long commerceYieldRate, int iSliderPercent, long channelPercentSum,
		long channelDeposits, long productionYieldRate, int iProductionToCommerce);

	// ---- THE CROSS-SCOPE ROLL-UP -- the GAME-OBJECT read role's realized answer (patterns.md § THE TWO READ
	// ---- ROLES: *"what do I HAVE, right now?"*, the live-state twin of the expected* what-if walk above; the
	// ---- two answers sit on ONE calc surface so no second combine can appear beside them).
	// ---- modifier.md §1: deposits accumulate in a package AT THEIR OWN SCOPE and the downward roll is realized
	// ---- AT READ -- so a scope object's realized channel value is the combine over the packages it sits under,
	// ---- computed HERE once for every consumer ([DEC-single-implementation]).
	// ---- ⛔ A lower scope never STORES an upper scope's sums: this composes at read and caches nothing upward.
	// ---- ⛔ Every package read below is a BARE FETCH (readFlat/readPercent/readSum -- the CONSUMER read path),
	// ---- never the gather's mark-firing sourceFlat/sourcePercent inputs, which belong to a rebuild only.

	// The §2 combine over ONE channel's rolled sums. A group read has no external base (`base` = 0) and
	// multiplier deposits are identity on every package channel, so the arithmetic is
	// `Σflat × max(0, 100 + Σpercent)/100`. The channel's CANONICAL AUTHORED UNIT decides which side IS the
	// answer -- the census verdict declared ONCE beside the vocabulary (infoKindUnit), never re-decided per
	// getter: a PERCENT-unit channel (the maintenance modifier, buildRate, combat, ...) has no flat plane of its
	// own, so its realized value IS the ONE additive percent stack (modifier.md §2a); a FLAT-unit channel is the
	// flat sum that stack scales. A kind the census records as DUAL takes its dominant plane as the base and the
	// opposing plane as the scaler, which is exactly what the §2 combine says. Both answers are ×100 native
	// ([DEC-fixedpoint-x100]); the ÷100 on the percent operand is the fixed-point scale of a ×100 percent used
	// as a multiplier, the same conversion cityRate's callers apply.
	// ⚠ NOT a duplicate of cityRate: that is the §2a RATE specialization (two tiers + the specialist layer) the
	// receiver sums are built from; this is the generic §2 slot combine every other channel answers by.
	static long realizedChannel(long flatSum, long percentSum, CvCascUnit eCanonicalUnit);

	// The realized ×100 value of one channel AT one scope object -- the entry every game-object group read
	// folds per channel. The object IS the scope (patterns.md rule 5: scope is never an argument on this role),
	// so each entry knows its own chain: city = team + empire + (area × owner) + city · empire = team + empire ·
	// team = team · area slot = itself · plot = itself. WORLD is deliberately absent (CONFIG, no package --
	// state-repositories.md), and PLOT never enters an upper scope's chain: a per-plot value resolves INSIDE the
	// isolated plot package before any city stack runs (modifier.md §2 plot-as-base), and the worked-plot Σ that
	// DOES feed a city is the receiver rate's own BASE term, not a channel roll-up.
	// ⚑ THE ORIGIN RULE IS ENFORCED BY THE DATA, not by a hand-written scope filter: a channel is minted into a
	// scope's set only where the compiled deposits author it (CascadeChannelRegistry -- KEYS ONLY WHERE NEEDED),
	// so a scope that carries no flat/percent side of a channel answers 0 with no storage anywhere. That is why
	// the yield/commerce families come out flat-at-plot-and-city / percent-above-plot exactly as the rule states,
	// while the families the rule does not speak for (wellbeing's empire+area flats, the plot-scope health and
	// defense percents) keep the sides their data actually authors.
	// A channel the scope CONSUMES answers its maintained RECEIVER sum instead of a roll-up -- the receiving
	// scope stores its own realized total ([DEC-uniform-cache-shape]: a receiver is the same cache holding a
	// different slot), so re-rolling it here would be a second derivation of a number that already exists.
	// iChannel < 0 (never authored anywhere) answers 0.
	static int realizedAtPlot(const CvPlot& plot, int iChannel);
	static int realizedAtCity(const CvCity& city, int iChannel);
	// The CITY chain's two LEGS, before the combine -- the ONE description of what a city sits under (team +
	// empire + (area × owner) + city), shared by realizedAtCity and by every consumer that needs the ONE additive
	// stack as a number of its own (the commerce split's slider share). It is the SAME roll-up, handed back
	// un-combined; a consumer that re-walked the chain itself would be a second description of the chain.
	// ⛔ The flat leg is NOT a realized answer: a channel the city CONSUMES answers its maintained receiver sum,
	// which is realizedAtCity's preference and is deliberately absent here. iChannel < 0 answers both legs 0.
	static void rolledLegsAtCity(const CvCity& city, int iChannel, long& flatSum, long& percentSum);
	static int realizedAtEmpire(const CvPlayer& player, int iChannel);
	static int realizedAtTeam(const CvTeam& team, int iChannel);
	// The AREA slot's read. ePlayer is the (area × player) slot's IDENTITY axis -- an area "knows no borders", so
	// an area-scope value realizes per player (state-repositories.md) -- never a scope argument.
	static int realizedAtArea(const CvArea& area, PlayerTypes ePlayer, int iChannel);

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
