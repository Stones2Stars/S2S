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
#include <string>

class CvInfo;
struct CascadeDeposit;   // the compiled deposit record (perScale's cascade-side carrier) -- full def in CvDepositIndex.h
class CvModEntry;        // the info-side compiled §3.9 entry (perScale's valuation-side carrier)
class CvTraitInfo;

class MMKernel
{
public:
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
	static long perScale(const CascadeDeposit& dep, const CvCascadeEvalCtx& ec, long value);
	static long perScale(const CvModEntry& entry, const CvCascadeEvalCtx& evalCtx, long value);
	// The shared §3.7 formula core both carriers feed: value × (max(0, count − above) / each), the above leg
	// only when carried (bAboveResolved; bAboveIsCityLimit applies the world-size scale to the source-resolved
	// base -- ruling 26). Exposed so the formula is never re-derived by a third carrier.
	static long perApply(long value, long iCount, int iEach, bool bHasAbove, int iAboveBase, bool bAboveIsCityLimit);

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
