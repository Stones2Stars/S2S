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

class CvJsonInfo;
class CvJsonTraitInfo;
class CvPlot;
class CvPlayer;

// The C++ flat-deposit model: each CvCascadeDeposit's `address` IS the dotted "<channel>.<scope>[.<member>[.<KEY>]]" path
// (readJson built it), `unit` the leaf kind, `value100` the ×100 magnitude. So a StoneBase tree-walk
// (fam.Root.Children[scope]...Magnitudes) is an address-string match here. These mirror ModifierMath.cs.
class MMKernel
{
public:
	// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated through the
	// typed-condition evaluator against the live engine ctx. MODIFIER context = the lenient flags (default): a
	// {STATE_RELIGION:X} compound matches loosely (the strict-match form is the enabler's requires.build only).
	static bool applies(const CvCascadeCondition* enabled, const CvCascadeCondition* disabled, const CvCascadeEvalCtx& ec);

	// Sum a channel's SCOPE-WIDE percent deposits (address == "<family>.<scope>", unit "percent"), gated, as HUMAN percent.
	static int sumPercent(const CvJsonInfo* d, const std::string& wantAddress, const CvCascadeEvalCtx& ec);

	// Σ a unit at a scope-wide address as a HUMAN int (value100/100; StoneBase SumUnitAtScope = Σ (int)m.Value), gated.
	static int sumUnit(const CvJsonInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Σ a unit at a scope-wide address in ×100 FIXED-POINT (value100 direct; StoneBase SumUnit100 = Σ round(human×100)),
	// gated -- the OOS-correct sum for FRACTIONAL flats (a commerce −0.6 stays −60, not truncated to 0). modifier.md §2.
	static long sumUnit100(const CvJsonInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Σ a unit's UNCONDITIONED magnitudes at a scope-wide address (no enabled/disabled) -- the entity's INTRINSIC base
	// (a specialist's own getYield/CommerceChange). StoneBase SumUnitUnconditioned. Human int (value100/100).
	static int sumUnconditioned(const CvJsonInfo* d, const std::string& wantAddress, const char* unit);

	// The active trait set's CvJsonTraitInfo for trait t -- COMPLEX if GAMEOPTION_LEADER_COMPLEX_TRAITS, else SIMPLE
	// (StoneBase ActiveTraitSet). The two sets collide on the engine id, so they live in separate repos; this picks by the
	// live option (asserted from /state in StoneBase). NEVER the engine CvTraitInfo (its CvInfoReplacements swap is the catastrophe).
	static const CvJsonTraitInfo* traitData(int t);

	// Σ a TRAIT's deposits (addr, unit) with the PURE_TRAITS sign filter (StoneBase PureFilter: under GAMEOPTION_LEADER_PURE_TRAITS
	// a negative trait keeps only v<=0, a positive keeps only v>=0). sumTrait = human (value100/100); sumTrait100 = ×100.
	static int sumTrait(const CvJsonTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);
	static long sumTrait100(const CvJsonTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec);

	// Σ a source's flat deposits at an address with EXPLICIT plot-eval flags (bonusFromPlot: a bare {HAS_BONUS:X} reads THIS
	// plot -- the engine's per-plot improvement bonus-yield, ModifierMath.PlotEval). The caller sets ec.plot per worked plot.
	// pureSign (StoneBase PureFilter): 0 = no filter; +1 = a POSITIVE trait under PURE_TRAITS (keep only non-negative values);
	// -1 = a NEGATIVE trait (keep only non-positive). Threaded through the keyed helpers (default 0 = non-trait sources unchanged).
	static int sumFlatF(const CvJsonInfo* d, const std::string& wantAddress, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// KeyedMember: Σ a source's flat keyed by a named target -- "<channel>.<scope>.<member>.<KEY>.flat". ModifierMath.KeyedMember.
	static int keyedMember(const std::string& channel, const CvJsonInfo* d, const char* scope, const char* member,
		const std::string& key, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// KeyedPlotYield: Σ a source's flat keyed by THIS plot's improvement(s)/terrain/feature/bonus at a scope (the engine's
	// improvementYieldChange / terrainYieldChange / per-plot-bonus addends). impKeys = the plot's improvement + its
	// upgrade-ancestors (a building source) or just the direct improvement (civic/trait/substrate).
	static int keyedPlotYield(const std::string& channel, const CvJsonInfo* d, const char* scope, const CvPlot* p,
		TeamTypes eTeam, const std::vector<std::string>& impKeys, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// Just the IMPROVEMENT-keyed part of the above (the engine's impPlayer/impTeam accumulator inside the clamped improvement addend).
	static int keyedImprovementOnly(const std::string& channel, const CvJsonInfo* d, const char* scope,
		const std::vector<std::string>& impKeys, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign = 0);

	// PlotsTargetYield: Σ a source's plots-TARGET flat that applies to THIS plot (the predicate evaluated against the plot).
	static int plotsTargetYield(const std::string& channel, const CvJsonInfo* d, const char* scope, const CvCascadeEvalCtx& ec, int pureSign = 0);

	// One plot-substrate entity's own plot.flat + its plot-keyed yield (a route folds the improvement's RouteYieldChanges),
	// PlotEval (bonusFromPlot). StoneBase SubstratePlotYield. NULL info -> 0.
	static int substratePlotYield(const std::string& channel, const CvJsonInfo* d, const CvPlot* p, TeamTypes eTeam,
		const std::vector<std::string>& directImpKeys, const CvCascadeEvalCtx& ec);

	// The player's effective extra/less-yield threshold for a channel: MIN positive over active traits + civics'
	// {thresholdFamily}.empire.{channel}.flat. 0 -> no threshold. (⏳ interim trait read + no PURE_TRAITS -- §6; per-source
	// sum used as the source's threshold -- exact for the common single-deposit case.)
	static int minPosThreshold(const char* thresholdFamily, const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// getModifiedIntValue port (CvGameCoreDLL.cpp:691): mod>0 -> v×(100+mod)/100; mod<0 -> v×100/(100-mod); else v.
	static int modifiedInt(int v, int mod);
};

#endif // CV_CASCADE_MM_KERNEL_H
