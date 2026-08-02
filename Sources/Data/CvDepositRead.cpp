//
//	MMKernel -- the shared per-deposit LEAF primitives of the #430 modifier machine (see the header): the §3.9
//	applicability gate, the §3.9 `ai` audience gate, the ONE §3.7 `per` count-scaler, the ctx-less intrinsic
//	read, and the active trait-set resolver. Declared as a static-methods surface so every consumer of a
//	compiled deposit reaches the ONE implementation (the single-source law, patterns.md).
//
//	⛔ Channel TOTALS are not summed here: they are the compiled (family, kind, scope, unit) slot sums
//	(CvModifiers::sum / InfoValuation::groupSum) -- a packed-slot fetch over typed ids, never a runtime
//	string-address scan ([DEC-materialize-at-mapfrom]).
//

#include "CvGameCoreDLL.h"
#include "Data/CvDepositRead.h"
#include "CvInfo.h"                    // CvInfo (+ the compiled CvModEntry carrier the entry-side perScale reads)
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id) -- the JSON info home
#include "Defines/CvGlobals.h"
#include "Engine/EmpireContext.h"      // EmpireContext::isHuman -- the audience gate (the empire silo)
#include "CvTraitInfo.h"
#include "Engine/CvGame.h"             // GC.getGame().isOption (the trait-set option gate)
#include "Engine/CvMap.h"              // GC.getMap().getWorldSize() -- the per.above CITY_LIMIT scaling leg
#include "CvWorldInfo.h"               // getCityLimitsScalePercent -- the world-size scale of the civic base limit
#include "Conditions/CvConditionEval.h"    // cascadeEvalCondition / cascadeCountOf / cascadeCountCityReligions
#include "Data/CvDepositIndex.h"       // DepositIndex + the compiled CascadeDeposit record (hot paths match ints)

// Query-side cached segment ids. A hit (>=0) is cached forever; a miss RE-LOOKS-UP each call -- the interner is
// append-only, so a re-map can turn a miss into a hit but never invalidates a cached id.
static int mmk_seg(const char* s, int& cache)
{
	if (cache < 0) cache = DepositIndex::lookupSegment(std::string(s));
	return cache;
}
static int s_segmentSelf = -1;
static int s_segmentCityLimit = -1;

// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated through the
// typed-condition evaluator against the live engine ctx. MODIFIER context = the lenient flags (default): a
// {STATE_RELIGION:X} compound matches loosely (the strict-match form is the enabler's requires.build only).
bool MMKernel::applies(const CvCondition* enabled, const CvCondition* disabled, const CvCascadeEvalCtx& ec)
{
	static const CvCascadeEvalFlags kFlags;   // default: strictStateReligionForBuild=false (the modifier reading)
	if (enabled != NULL && !cascadeEvalCondition(enabled, ec, kFlags)) return false;
	if (disabled != NULL && cascadeEvalCondition(disabled, ec, kFlags)) return false;
	return true;
}

// THE AUDIENCE GATE (json §3.9 `ai`): an aiOnly deposit/entry contributes ONLY when the asking player is an AI.
// Human-audience is the default -- a NULL player (a ctx-less/ownerless read) stays human and excludes aiOnly.
bool MMKernel::audienceOk(bool bAiOnly, const CvCascadeEvalCtx& evalCtx)
{
	if (!bAiOnly)
	{
		return true;
	}
	return evalCtx.empireContext != NULL && !evalCtx.empireContext->isHuman();
}

// The shared §3.7 formula core (see the header): value × (max(0, count − above) / each). The `above` leg
// (ruling 26) composes with `each`; CITY_LIMIT applies the live world-size scale to the source-resolved base --
// exactly the archived engine limit formula. An unresolved base skips the whole multiply (the SELF-guard
// pattern: never zero and never over-apply silently). Both carrier adapters below feed this ONE formula.
// SCALE: `value` is ×100 and `iCount`/`iThreshold` are plain COUNTS, so the product stays ×100; the threshold's
// own × scalePercent / 100 is a count × HUMAN percent -- no two ×100 operands ever meet here.
int64_t MMKernel::perApply(int64_t value, int64_t iCount, int iEach, bool bHasAbove, int iAboveBase, bool bAboveIsCityLimit)
{
	if (bHasAbove)
	{
		int iThreshold = iAboveBase;
		if (iThreshold < 0) return value;   // unresolved base (or a malformed negative literal)
		if (bAboveIsCityLimit)
		{
			iThreshold = iThreshold * GC.getWorldInfo(GC.getMap().getWorldSize()).getCityLimitsScalePercent() / 100;
		}
		iCount = iCount > iThreshold ? iCount - iThreshold : 0;
	}
	const int iQuantum = iEach > 0 ? iEach : 1;
	return value * (iCount / iQuantum);
}

// THE generic §3.7 `per` count-scaler resolver (json.md §3.7; owner semantics: resolved value = value × (count /
// each) -- count whatever per.type names at the per's scope, INTEGER-divide by `each` FIRST, multiply the deposit's
// value; deterministic integer math only). The count resolves through cascadeCountOf -- the SAME core as the
// condition count-atoms (DEC-single-implementation, never a parallel count path). A deposit without a per is
// returned byte-identical; an UNRESOLVED SELF token (json §3.1 -- the push could not resolve the owning entity)
// SKIPS the multiply entirely (counting it 0 would wrongly zero the contribution).
int64_t MMKernel::perScale(const CascadeDeposit& dep, const CvCascadeEvalCtx& ec, int64_t value)
{
	// the §3.7 `religion:` counted-kind filter (ruling 23) composes multiplicatively with the per: the value
	// scales by the count of the city's religions matching the filter (0 matches -> 0 contribution, exactly
	// the per-religion semantics of "N per matching religion").
	if (dep.religionQual != NULL)
	{
		value *= cascadeCountCityReligions(dep.religionQual, ec);
	}
	if (!dep.hasPer) return value;
	if (dep.perTokenSeg >= 0 && dep.perTokenSeg == mmk_seg("SELF", s_segmentSelf)) return value;   // unresolved SELF
	const CvCascScope eScope = (CvCascScope)dep.perScope;   // push-resolved: authored, else the deposit's own scope
	int64_t iCount = 0;
	if (dep.perAnyOf != NULL && dep.perAnyOfTypes != NULL)   // per.anyOf: the SUMMED count of every listed type (json §3.7)
	{
		for (size_t i = 0; i < dep.perAnyOf->size() && i < dep.perAnyOfTypes->size(); ++i)
			iCount += cascadeCountOf((*dep.perAnyOf)[i], (*dep.perAnyOfTypes)[i], eScope, ec);
	}
	else if (!dep.perType.empty())
		iCount = cascadeCountOf(dep.perTypeId, dep.perType, eScope, ec);
	else
		return value;   // a per with nothing to count (malformed authoring) -- never zero silently
	bool bAboveIsCityLimit = false;
	if (dep.hasAbove && dep.perAboveSeg >= 0)
	{
		if (dep.perAboveSeg != mmk_seg("CITY_LIMIT", s_segmentCityLimit)) return value;   // unknown token -- skip
		bAboveIsCityLimit = true;
	}
	return perApply(value, iCount, dep.perEach, dep.hasAbove, dep.perAbove, bAboveIsCityLimit);
}

// The INFO-SIDE carrier adapter: the same §3.7 resolution over a compiled CvModEntry (the expected* valuation
// walks an info's conditioned entries directly -- patterns.md § THE GETTER SETUP). Field plumbing only; the
// count goes through the ONE cascadeCountOf core and the formula through the ONE perApply. Token spellings
// (SELF / CITY_LIMIT) ride the entry as strings; comparing them here is the bounded per-decision tail walk,
// never a hot-path address lookup (the cascade-side record path above stays int-only).
int64_t MMKernel::perScale(const CvModEntry& entry, const CvCascadeEvalCtx& evalCtx, int64_t value)
{
	if (entry.religionQual != NULL)
	{
		value *= cascadeCountCityReligions(entry.religionQual, evalCtx);
	}
	if (!entry.hasPer) return value;
	if (entry.perTypeId < 0 && entry.perType == "SELF") return value;   // unresolved SELF (json §3.1)
	const CvCascScope eScope = (entry.perScope >= 0) ? (CvCascScope)entry.perScope : entry.scope;   // json §3.7 default
	int64_t iCount = 0;
	if (!entry.perAnyOf.empty() && !entry.perAnyOfTypes.empty())
	{
		for (size_t i = 0; i < entry.perAnyOf.size() && i < entry.perAnyOfTypes.size(); ++i)
			iCount += cascadeCountOf(entry.perAnyOf[i], entry.perAnyOfTypes[i], eScope, evalCtx);
	}
	else if (!entry.perType.empty())
		iCount = cascadeCountOf(entry.perTypeId, entry.perType, eScope, evalCtx);
	else
		return value;   // a per with nothing to count (malformed authoring) -- never zero silently
	bool bAboveIsCityLimit = false;
	if (entry.hasAbove && !entry.perAboveToken.empty())
	{
		if (entry.perAboveToken != "CITY_LIMIT") return value;   // unknown token -- skip, never zero
		bAboveIsCityLimit = true;
	}
	return perApply(value, iCount, entry.perEach, entry.hasAbove, entry.perAbove, bAboveIsCityLimit);
}

// Σ a unit's UNCONDITIONED magnitudes at a scope-wide address (no enabled/disabled) -- the entity's INTRINSIC base.
// Human int (value/100). ⚠ The one surviving address-string read: its live caller (the property plane's channel
// bridge) composes the address from the property's type at call time; the compiled equivalent is the
// MODFAM_PROPERTY + property-FK slot read (CvModifiers::propertySum).
int MMKernel::sumUnconditioned(const CvInfo* d, const std::string& wantAddress, const char* unit)
{
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	int sum = 0;
	const std::vector<CascadeDeposit>& deps = DepositIndex::depositsFor(d);
	for (size_t i = 0; i < deps.size(); ++i)
	{
		const CascadeDeposit& dep = deps[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (dep.aiOnly) continue;   // ctx-less intrinsic read: human audience by default (json §3.9 `ai`)
		if (dep.enabled != NULL || dep.disabled != NULL) continue;
		// NO perScale here: this ctx-less intrinsic read cannot resolve a count -- a per'd deposit stays unscaled
		// (the pre-resolver behavior; no intrinsic-base family authors a per today).
		sum += dep.value / 100;
	}
	return sum;
}

// The active trait set's CvTraitInfo for trait t -- COMPLEX if GAMEOPTION_LEADER_COMPLEX_TRAITS, else SIMPLE.
// The two sets live in separate repos and are separated BY ID too (a complex trait keeps its own TRAIT_COMPLEX_
// identity, modifier.md par.4), so a given id resolves in exactly one of them and the fall-through is total.
// The OPTION read is still load-bearing for the one id BOTH sets carry -- TRAIT_BARBARIAN, base-filled into
// complex/ so that set stays self-complete -- where it picks the entry belonging to the active set.
// NEVER a runtime swap of a single shared engine CvTraitInfo.
const CvTraitInfo* MMKernel::traitData(int t)
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_COMPLEX_TRAITS))
	{
		const CvInfo* d = InfoRepo<CvComplexTraitTag>::get().get(t);
		if (d != NULL) return static_cast<const CvTraitInfo*>(d);
	}
	const CvInfo* d = InfoRepo<CvTraitInfo>::get().get(t);   // the SIMPLE set (engine CvTraitInfo tag = the simple repo)
	return d != NULL ? static_cast<const CvTraitInfo*>(d) : NULL;
}
