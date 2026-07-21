#pragma once
#ifndef CV_CASCADE_MM_KERNEL_H
#define CV_CASCADE_MM_KERNEL_H

//
//	MMKernel -- the shared LEAF helpers of the #430 modifier machine (StoneBase ModifierMath.cs over the flat-deposit
//	model). Every Calc package (PercentStack / YieldBasePackages / BuildingPackage / YieldRate / CommerceCalc) FEEDS
//	itself through these; they are the single-implementation gated-sum primitives. See docs/architecture/patterns.md
//	(the single-source law) + docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx / CvCascadeEvalFlags -- the eval target for deposit conditions
#include <string>
#include <vector>

class CvInfo;
struct CascadeDeposit;   // the compiled deposit record (sumUnit100From takes a vector of these) -- full def in CvCascadeDepositIndex.h
class CvTraitInfo;
class CvPlot;
class CvPlayer;
class CvCity;

// The C++ flat-deposit model: each compiled CascadeDeposit's `address` IS the dotted "<channel>.<scope>[.<member>[.<KEY>]]"
// path (the CvJsonModifiers family key, compiled by the DepositIndex push), `unit` the leaf kind, `value100` the ×100
// magnitude. So a StoneBase tree-walk (fam.Root.Children[scope]...Magnitudes) is an address match here. These mirror ModifierMath.cs.
class MMKernel
{
public:
	// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated through the
	// typed-condition evaluator against the live engine ctx. MODIFIER context = the lenient flags (default): a
	// {STATE_RELIGION:X} compound matches loosely (the strict-match form is the enabler's requires.build only).
	static bool applies(const CvJsonCondition* enabled, const CvJsonCondition* disabled, const CvCascadeEvalCtx& ec);

	// THE generic §3.7 `per` count-scaler resolver (json.md §3.7; owner semantics: resolved value =
	// value × (count / each), the count INTEGER-divided by `each` FIRST -- deterministic integer math only).
	// Count = Σ over per.anyOf, else cascadeCountOf(perType) -- the ONE count implementation (never a parallel
	// path) -- at the compiled perScope (authored, else push-resolved to the deposit's own scope). hasPer==false
	// (and an unresolved SELF token) returns value100 untouched. Applied at every MMKernel fold of a deposit's
	// value; ⛔ NOT in the property engine (self-contained by design, engine.md §Properties).
	static long perScale(const CascadeDeposit& dep, const CvCascadeEvalCtx& ec, long value100);

	// Sum a channel's SCOPE-WIDE percent deposits (address == "<family>.<scope>", unit "percent"), gated, as HUMAN percent.
	static int sumPercent(const CvInfo* d, const std::string& wantAddress, const CvCascadeEvalCtx& ec);

	// Σ a unit at a scope-wide address as a HUMAN int (value100/100; StoneBase SumUnitAtScope = Σ (int)m.Value), gated.
	static int sumUnit(const CvInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Vector-taking core of sumUnit (the int twin of sumUnit100From): sum a compiled-record vector directly, so a channel
	// summer folds an obsolete building's `whenObsolete` tree (DepositIndex::whenObsoleteFor(d)) into the SAME per-position
	// sum with the SAME gated match as its normal records (json §4.2). sumUnit delegates here.
	static int sumUnitFrom(const std::vector<CascadeDeposit>& deps, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Σ a unit at a scope-wide address in ×100 FIXED-POINT (value100 direct; StoneBase SumUnit100 = Σ round(human×100)),
	// gated -- the OOS-correct sum for FRACTIONAL flats (a commerce −0.6 stays −60, not truncated to 0). modifier.md §2.
	static long sumUnit100(const CvInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Vector-taking core of sumUnit100: sum a compiled-record vector directly -- so a channel folds a building's
	// `whenObsolete` tree (DepositIndex::whenObsoleteFor(d), json §4.2) with the SAME gated match as its normal records.
	// sumUnit100 delegates here.
	static long sumUnit100From(const std::vector<CascadeDeposit>& deps, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Σ a unit's UNCONDITIONED magnitudes at a scope-wide address (no enabled/disabled) -- the entity's INTRINSIC base
	// (a specialist's own getYield/CommerceChange). StoneBase SumUnitUnconditioned. Human int (value100/100).
	static int sumUnconditioned(const CvInfo* d, const std::string& wantAddress, const char* unit);

	// The active trait set's CvTraitInfo for trait t -- COMPLEX if GAMEOPTION_LEADER_COMPLEX_TRAITS, else SIMPLE
	// (StoneBase ActiveTraitSet). The two sets collide on the engine id, so they live in separate repos; this picks by the
	// live option (asserted from /state in StoneBase). NEVER a runtime swap of a single shared engine CvTraitInfo.
	static const CvTraitInfo* traitData(int t);

	// Σ a TRAIT's deposits (addr, unit) with the PURE_TRAITS sign filter (StoneBase PureFilter: under GAMEOPTION_LEADER_PURE_TRAITS
	// a negative trait keeps only v<=0, a positive keeps only v>=0). sumTrait = human (value100/100); sumTrait100 = ×100.
	static int sumTrait(const CvTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);
	static long sumTrait100(const CvTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// ---- the KEYED plot helpers run on the COMPILED deposit index (DepositIndex): callers pass INTERNED segment
	// ---- ids (chanId = DepositIndex::lookupSegment(channel); scopeId likewise; impKeyIds via segIdForImprovement).
	// ---- A negative id means "never authored anywhere" and sums 0 without touching a deposit.

	// KeyedMember by compiled segments: Σ a source's <unit> at "<chan>.<scope>.<member>.<KEY>"
	// (ModifierMath.KeyedMember). sumKeyed4U takes the unit SEGMENT id explicitly (percent for the keyed
	// buildRate members -- they are percent-unit deposits); sumKeyed4F is the flat parameterization.
	static int sumKeyed4U(const CvInfo* d, int chanId, int scopeId, int memberId, int keyId, int unitId,
		const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);
	static int sumKeyed4F(const CvInfo* d, int chanId, int scopeId, int memberId, int keyId,
		const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// KeyedPlotYield: Σ a source's flat keyed by THIS plot's improvement(s)/terrain/feature/bonus at a scope (the engine's
	// improvementYieldChange / terrainYieldChange / per-plot-bonus addends). impKeyIds = the plot's improvement + its
	// upgrade-ancestors (a building source) or just the direct improvement (civic/trait/substrate), as interned segment ids.
	static int keyedPlotYield(int chanId, const CvInfo* d, int scopeId, const CvPlot* p,
		TeamTypes eTeam, const std::vector<int>& impKeyIds, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// Just the IMPROVEMENT-keyed part of the above (the engine's impPlayer/impTeam accumulator inside the clamped improvement addend).
	static int keyedImprovementOnly(int chanId, const CvInfo* d, int scopeId,
		const std::vector<int>& impKeyIds, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// PlotsTargetYield: Σ a source's plots-TARGET flat that applies to THIS plot (the predicate evaluated against the plot).
	static int plotsTargetYield(int chanId, const CvInfo* d, int scopeId, const CvCascadeEvalCtx& ec, int pureSign = 0);

	// One plot-substrate entity's own plot.flat + its plot-keyed yield (a route folds the improvement's RouteYieldChanges),
	// PlotEval (bonusFromPlot). StoneBase SubstratePlotYield. NULL info -> 0.
	static int substratePlotYield(int chanId, const CvInfo* d, const CvPlot* p, TeamTypes eTeam,
		const std::vector<int>& directImpKeyIds, const CvCascadeEvalCtx& ec);

	// The player's effective extra/less-yield threshold for a channel: MIN over the POSITIVE per-DEPOSIT magnitudes of
	// {thresholdFamily}.empire.{channel}.flat across the player's active traits + civics (StoneBase MinPositiveThreshold --
	// MINs individual magnitudes, NOT a per-source sum). 0 -> no threshold. Applies the StoneBase PURE_TRAITS threshold-family
	// gate (a lessYieldThreshold dropped from a non-negative trait, an extraYieldThreshold from a negative one; civics unfiltered).
	static int minPosThreshold(const char* thresholdFamily, const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// Σ CIVIC BUILDING-KEYED percent for the city's ACTIVE (non-dormant constructed) buildings -- the engine's
	// owner.getBuildingCommerceModifier (civic-fed) folded into modBuilding per built building (StoneBase
	// BuildingKeyedSourcePercent): for each active building B in the city, Σ each adopted civic's
	// <channel>.empire.buildings.<B_TYPE>.percent deposit (gated). Civic-only; channel-parameterized (serves yield + commerce).
	static int buildingKeyedSourcePercent(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// getModifiedIntValue port (CvGameCoreDLL.cpp:691): mod>0 -> v×(100+mod)/100; mod<0 -> v×100/(100-mod); else v.
	static int modifiedInt(int v, int mod);
};

#endif // CV_CASCADE_MM_KERNEL_H
