#pragma once
#ifndef CV_CASCADE_MM_KERNEL_H
#define CV_CASCADE_MM_KERNEL_H

//
//	MMKernel -- the shared PER-DEPOSIT LEAF primitives of the #430 modifier machine: the §3.9 applicability gate,
//	the §3.9 `ai` audience gate, the ONE §3.7 `per` count-scaler, the ctx-less INTRINSIC read, and the active
//	trait-set resolver. Every consumer of a compiled deposit (the cascade gather, the info valuation, the
//	property channel) folds through these, so each rule exists exactly once (docs/architecture/patterns.md, the
//	single-source law; [DEC-single-implementation]).
//
//	⛔ SUMMING IS NOT HERE. A channel total is the COMPILED (family, kind, scope, unit) slot sum --
//	CvModifiers::sum / CvInfo::modifier / InfoValuation::groupSum -- a packed-slot fetch over typed ids. No read
//	path addresses a deposit by a runtime-built std::string ([DEC-materialize-at-mapfrom]).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//
//	VALUE SCALE: every magnitude these touch is ×100 fixed-point (the single human→×100 conversion happened in
//	readJson -- docs/specs/curators/fixed-point-and-scales.md §1). The `per` scaler multiplies a ×100 value by a
//	plain COUNT, so the product stays ×100; ⛔ two ×100 values are never multiplied without a ÷100 at the multiply.
//

#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx / CvCascadeEvalFlags -- the eval target for deposit conditions
#include "Infos/CvInfoKinds.h"           // CvCascUnit / ModifierFamily -- the axes resolveEntry filters on
#include <string>

class CvInfo;
class CvPlot;
struct CascadeDeposit;   // the compiled deposit record (perScale's cascade-side carrier) -- full def in CvDepositIndex.h
class CvModEntry;        // the info-side compiled §3.9 entry (perScale's valuation-side carrier)
class CvTraitInfo;

class MMKernel
{
public:
	// ⚖ THE ONE PER-ENTRY RESOLVE — what a compiled entry DEPOSITS at a scope, and nothing about where it goes.
	// Answers the whole question "does this entry land here, and as what?": the scope test (incl. json §3.3's
	// `empires` and `cities` plural fans), the unit-carried skip ([DEC-unit-modifiers-on-top]), the flat/percent
	// side, the §2a specialist rate carve-out, the channel lookup, the plot-substrate target keys and the
	// `plots` filter, the PURE_TRAITS sign filter, the `ai` audience gate, the §3.9 enabled/disabled gate, and
	// the ONE §3.7 `per` scaler × the source's live multiplicity.
	//
	// ⛔ THIS EXISTS BECAUSE TWO SINKS CONSUME IT AND MUST NEVER DIVERGE ([DEC-single-implementation]): the
	// maintained sum APPLIES the resolved value into the package slot as its fact arrives, and the endpoint
	// ORACLE folds the same value into its own scratch document. Those two are diffed against each other
	// (state-repositories.md § THE RECOMPUTE IS AN ENDPOINT ORACLE), so a second copy of this resolve would make
	// the tripwire compare two derivations instead of one derivation against two builds — it could disagree for
	// reasons that are not a missed emit, which is exactly the flaw that killed the old comparison twin.
	// ⚑ What legitimately differs between the two is ONLY the sink, which is why nothing about a slot, a mask or
	// a package appears in this signature.
	//
	// iMultiplier = the source's live multiplicity (owned building count, project count, specialist count; 1 for
	// presence). pKeyPlot = the plot whose substrate keys plot-scope targeted entries, NULL off the plot plane.
	// bSkipRateChannels = the §2a specialist carve-out (its yield/commerce joins the rate BASE at the receiver
	// combine, never a city flat).
	// ⛔ There is deliberately NO pure-traits parameter: the alignment rule is applied ONCE as a parse transform
	// that gates the entry (CvModifiers::applyPureTraitGate), so the `applies` call below enforces it like any
	// other condition ([DEC-single-implementation]).
	// Returns false when the entry deposits nothing here; the out-params are then untouched.
	// ⚖ WHICH VALUE THE CALLER WANTS, and it is decided by WHICH FACT is applying.
	// A SOURCE fact deposits what the source is worth right now, so the `per` scaler is APPLIED: `value × count`.
	// A COUNT fact moves an already-deposited amount, so it wants the per-UNIT value and multiplies by the delta
	// the fact itself carries -- `Δ(value × count) = value × Δcount`, which is EXACT because `value` is a
	// load-compiled constant ([DEC-maintained-sum] plane B). ⛔ Handing a count fact the SCALED value would
	// re-deposit the whole scaled amount on every tick of that count instead of moving it by the difference.
	enum PerScaling
	{
		PER_SCALE_APPLIED,      // value × count(now) -- a SOURCE arriving or leaving
		PER_SCALE_SUPPRESSED,   // the per-UNIT value -- a COUNT fact scales it by its own Δ
	};
	static bool resolveEntry(const CvModEntry& kEntry, int iMultiplier, CvCascScope eScope,
		const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot, bool bSkipRateChannels,
		int& iChannelOut, bool& bPercentSideOut, int64_t& iValueOut,
		PerScaling ePerScaling = PER_SCALE_APPLIED);

	// The dictionary a unit folds into (the whole type axis -- value vs percent; modifier.md §2). The legacy
	// per-* unit spellings are flat-side (their scaling is the §3.7 resolver's); a multiplier composes by
	// product and never slots. Exposed beside resolveEntry because a sink that routes by side reads them too.
	static bool unitIsFlatSide(CvCascUnit eUnit);
	static bool unitIsPercentSide(CvCascUnit eUnit);
	// Is the stored value UNSCALED (percent side, or a COUNT)? The test a READER asks before its ÷100.
	static bool unitIsUnscaled(CvCascUnit eUnit);
	// Is this family a rate (yield/commerce) channel family? The §2a specialist carve-out keys on it.
	static bool isRateFamily(ModifierFamily eFamily);

	// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated through the
	// typed-condition evaluator against the live engine ctx. MODIFIER context = the lenient flags (default): a
	// {STATE_RELIGION:X} compound matches loosely (the strict-match form is the enabler's requires.build only).
	static bool applies(const CvCondition* enabled, const CvCondition* disabled, const CvCascadeEvalCtx& ec);

	// THE AUDIENCE GATE (json §3.9 `ai`): an aiOnly deposit/entry contributes ONLY when the asking player is an
	// AI. Human-audience is the default everywhere -- a NULL/absent player reads human. Every gated record loop
	// applies this beside `applies` (the aiOnly record's ADDRESS equals its base twin's; only the flag splits them).
	static bool audienceOk(bool bAiOnly, const CvCascadeEvalCtx& evalCtx);

	// THE generic §3.7 `per` count-scaler resolver (json.md §3.7; owner semantics: resolved value =
	// value × (count / each), the count INTEGER-divided by `each` FIRST -- deterministic integer math only).
	// Count = Σ over per.anyOf, else cascadeCountOf(perType) -- the ONE count implementation (never a parallel
	// path) -- at the compiled perScope (authored, else push-resolved to the deposit's own scope). hasPer==false
	// (and an unresolved SELF token) returns value untouched. Applied at every MMKernel fold of a deposit's
	// value; ⛔ NOT in the property engine (self-contained by design, engine.md §Properties).
	// TWO CARRIER SHAPES, ONE FORMULA: the cascade-side CascadeDeposit record and the info-side CvModEntry (the
	// expected* valuation walks entries directly) adapt into the SAME perApply core below -- the §3.7
	// arithmetic exists once (DEC-single-implementation), only the field plumbing differs per carrier.
	static int64_t perScale(const CascadeDeposit& dep, const CvCascadeEvalCtx& ec, int64_t value);
	static int64_t perScale(const CvModEntry& entry, const CvCascadeEvalCtx& evalCtx, int64_t value);
	// The shared §3.7 formula core both carriers feed: value × (max(0, count − above) / each), the above leg
	// only when carried (bAboveResolved; bAboveIsCityLimit applies the world-size scale to the source-resolved
	// base -- ruling 26). Exposed so the formula is never re-derived by a third carrier.
	static int64_t perApply(int64_t value, int64_t iCount, int iEach, bool bHasAbove, int iAboveBase, bool bAboveIsCityLimit);

	// Σ a unit's UNCONDITIONED magnitudes at a scope-wide address (no enabled/disabled) -- the entity's INTRINSIC base.
	// Human int (value/100). ⚠ The LAST address-string read on this surface: the property plane's channel bridge
	// builds its address from the property's type at call time (CvPropertyChannel). Re-homing it onto the compiled
	// MODFAM_PROPERTY + property-FK slot read (CvModifiers::propertySum) retires this signature.
	static int sumUnconditioned(const CvInfo* d, const std::string& wantAddress, const char* unit);

	// The active trait set's CvTraitInfo for trait t -- COMPLEX if GAMEOPTION_LEADER_COMPLEX_TRAITS, else SIMPLE.
	// The two sets collide on the engine id, so they live in separate repos; this picks by the live option.
	// NEVER a runtime swap of a single shared engine CvTraitInfo.
	static const CvTraitInfo* traitData(int t);
};

#endif // CV_CASCADE_MM_KERNEL_H
