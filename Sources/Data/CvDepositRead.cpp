//
//	MMKernel -- the shared per-deposit LEAF primitives of the #430 modifier machine (see the header): the §3.9
//	applicability gate, the §3.9 `ai` audience gate, the ONE §3.7 `per` count-scaler, the ctx-less intrinsic
//	read, and the active trait-set resolver. Declared as a static-methods surface so every consumer of a
//	compiled deposit reaches the ONE implementation (the single-source law, patterns.md).
//
//	⛔ Channel TOTALS are not summed here: they are the compiled (family, kind, scope, unit) slot sums
//	(CvModifiers::sum / InfoValuation::groupSum) -- a packed-slot fetch over typed ids, never a runtime
//	string-address scan (docs/architecture/patterns.md §Materialize at mapFrom).
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
#include "Infos/CvModEntry.h"          // CvModEntry + modSegmentLookup -- the interner targetSeg is written in
#include "Cascade/CvCascadeChannelRegistry.h"   // channelLookup -- the entry's (family, kind) -> channel id
#include "Engine/CvPlot.h"             // the plot substrate resolveEntry keys plot-scope targeted entries on

// Query-side cached segment ids. A hit (>=0) is cached forever; a miss RE-LOOKS-UP each call -- the interner is
// append-only, so a re-map can turn a miss into a hit but never invalidates a cached id.
static int mmk_seg(const char* s, int& cache)
{
	if (cache < 0) cache = DepositIndex::lookupSegment(std::string(s));
	return cache;
}
static int s_segmentSelf = -1;
static int s_segmentCityLimit = -1;

// ⛔ THE TARGET SEGMENTS RESOLVE THROUGH `modSegmentLookup`, NOT `DepositIndex::lookupSegment`, AND THE TWO ARE
// NOT INTERCHANGEABLE. They are independent interners with independently-assigned ids (`s_modSegments` vs
// `s_segs`), and `CvModEntry::targetSeg` is written by `modSegmentIntern` -- the FIRST of them. Comparing a
// targetSeg against the other map's id compiles clean, matches nothing, and silently stops every
// plot-substrate keyed deposit from depositing. Same caching discipline as mmk_seg above: a hit is cached
// forever, a miss re-looks-up (the interner is append-only, so a miss can become a hit but an id never moves).
static int s_segPlots = -1;
static int s_segImprovements = -1;
static int s_segTerrains = -1;
static int s_segFeatures = -1;
static int s_segBonuses = -1;
static int s_segRoutes = -1;
static int s_segEmpires = -1;
static int s_segCities = -1;

bool MMKernel::unitIsFlatSide(CvCascUnit eUnit)
{
	return eUnit == CASC_UNIT_FLAT
		|| eUnit == CASC_UNIT_COUNT
		|| eUnit == CASC_UNIT_PER_POPULATION
		|| eUnit == CASC_UNIT_PER_SPECIALIST
		|| eUnit == CASC_UNIT_PER_CORPORATION_LEVEL;
}

bool MMKernel::unitIsPercentSide(CvCascUnit eUnit)
{
	return eUnit == CASC_UNIT_PERCENT || eUnit == CASC_UNIT_RAW_PERCENT;
}

// Is this unit stored UNSCALED? A percent carries no decimals, and neither does a COUNT of things -- a
// headcount is not modified, not combined, and half of one does not exist, so the parse deliberately leaves it
// alone (CvModifiers mod_valueForUnit). ⛔ This is the question a READER must ask before reducing; "is this a
// percent" is a DIFFERENT question and answers NO for a count, which reduces a count of 1 to "0.01".
bool MMKernel::unitIsUnscaled(CvCascUnit eUnit)
{
	return unitIsPercentSide(eUnit) || eUnit == CASC_UNIT_COUNT;
}

bool MMKernel::isRateFamily(ModifierFamily eFamily)
{
	return infoFamilyYield(eFamily) >= 0 || infoFamilyCommerce(eFamily) >= 0;
}

// THE ONE PER-ENTRY RESOLVE (see the header): what this entry deposits at this scope, and nothing about where
// the answer goes. Every consumer folds through it, so there is exactly one derivation
// (docs/architecture/patterns.md §DRY (single implementation)).
bool MMKernel::resolveEntry(const CvModEntry& kEntry, int iMultiplier, CvCascScope eScope,
	const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot, bool bSkipRateChannels,
	int& iChannelOut, bool& bPercentSideOut, int64_t& iValueOut, PerScaling ePerScaling)
{
	if (iMultiplier == 0)
	{
		return false;
	}
	const int iEmpiresSeg = modSegmentCached("empires", s_segEmpires);
	const int iCitiesSeg = modSegmentCached("cities", s_segCities);
	// json §3.3's `empires` plural target: a WORLD-scope deposit onto EVERY empire lands in each PLAYER's
	// package, because WORLD is CONFIG and carries no package of its own (state-repositories.md). So the empire
	// scope accepts it even though the authored scope is world.
	const bool bWorldEmpires = (eScope == CASC_SCOPE_EMPIRE
		&& kEntry.scope == CASC_SCOPE_WORLD
		&& iEmpiresSeg >= 0
		&& kEntry.targetSeg == iEmpiresSeg);
	// json §3.3's `cities` plural target, the exact sibling one scope down: an EMPIRE-scope deposit onto EVERY
	// city lands in each CITY's package, because this city IS one of the targets.
	// ⚑ Resolving PER CITY is the whole point: it is what lets the entry's `per` scaler and its conditions
	// resolve against THIS city. Summed into the empire package instead they would resolve once, against no
	// city, and hand every city that one number.
	const bool bEmpireCities = (eScope == CASC_SCOPE_CITY
		&& kEntry.scope == CASC_SCOPE_EMPIRE
		&& iCitiesSeg >= 0
		&& kEntry.targetSeg == iCitiesSeg);
	// json §3.3's `plots` plural target, the same fan one scope further down: a CITY- or EMPIRE-scope deposit
	// onto EVERY plot lands in each PLOT's package, because this plot IS one of the targets (modifier.md §5).
	// ⚑ Resolving PER PLOT is the whole point: it is what lets the entry's own filter (`enabled: IS_WATER`)
	// decide, which no city-scope fold could do.
	// ⛔ Without this a `plots` entry resolved NOWHERE -- declined at city scope for being targeted, and at plot
	// scope for being authored one scope up -- so a Lighthouse's "+1 food on every water plot" was valued by the
	// AI's what-if (val_plotsTarget) and delivered to no package at all.
	// ⚠ THE AUTHORED SCOPE STILL DECIDES **WHOSE** PLOTS: the CALLER fans a CITY-scope entry over the plots of
	// the city holding the source and an EMPIRE-scope one over every city's, so this only admits the entry to
	// plot resolution -- it never widens its reach.
	const int iPlotsSeg = modSegmentCached("plots", s_segPlots);
	const bool bScopePlots = (eScope == CASC_SCOPE_PLOT
		&& (kEntry.scope == CASC_SCOPE_CITY || kEntry.scope == CASC_SCOPE_EMPIRE)
		&& iPlotsSeg >= 0
		&& kEntry.targetSeg == iPlotsSeg);
	if (kEntry.scope != eScope && !bWorldEmpires && !bEmpireCities && !bScopePlots)
	{
		return false;
	}
	if (kEntry.unitQual != NULL)
	{
		return false;
	}
	const bool bPercentSide = unitIsPercentSide(kEntry.unit);
	if (!bPercentSide && !unitIsFlatSide(kEntry.unit))
	{
		return false;
	}
	if (bSkipRateChannels && isRateFamily(kEntry.family))
	{
		return false;
	}
	const int iChannel = CascadeChannelRegistry::channelLookup(kEntry.family,
		(kEntry.family == MODFAM_PROPERTY) ? 0 : kEntry.kind, kEntry.propertyFk);
	if (iChannel < 0)
	{
		return false;   // outside the vocabulary / never a package channel -- drops out with no special-casing
	}
	// TARGETED entries: at PLOT scope a keyed entry resolves iff its key IS this plot's substrate (the engine's
	// improvement/terrain-keyed addends resolve INSIDE the isolated plot package, modifier.md §2); a
	// `plots`-target entry resolves iff its per-plot filter holds HERE. At every other scope a targeted entry
	// stays an entry-list read (the reverse pass already landed the building-keyed boosts on their targets).
	if (kEntry.targetSeg >= 0 || kEntry.targetFk >= 0)
	{
		// The `empires` and `cities` fans are the targeted entries that resolve outside plot scope: their target
		// IS the owner being resolved for, so landing it here is the deposit, not a keyed lookup.
		if (!bWorldEmpires && !bEmpireCities && (eScope != CASC_SCOPE_PLOT || pKeyPlot == NULL))
		{
			return false;
		}
		if (bWorldEmpires || bEmpireCities)
		{
			// falls through: the fan IS the deposit, no per-target test applies
		}
		else if (kEntry.targetSeg == modSegmentCached("plots", s_segPlots))
		{
			// falls through: applies() below evaluates the per-plot filter against THIS plot's ctx
		}
		else if (kEntry.targetSeg == modSegmentCached("improvements", s_segImprovements))
		{
			if (kEntry.targetFk < 0 || kEntry.targetFk != (int)pKeyPlot->getImprovementType())
			{
				return false;
			}
		}
		else if (kEntry.targetSeg == modSegmentCached("terrains", s_segTerrains))
		{
			if (kEntry.targetFk < 0 || kEntry.targetFk != (int)pKeyPlot->getTerrainType())
			{
				return false;
			}
		}
		else if (kEntry.targetSeg == modSegmentCached("features", s_segFeatures))
		{
			if (kEntry.targetFk < 0 || kEntry.targetFk != (int)pKeyPlot->getFeatureType())
			{
				return false;
			}
		}
		else if (kEntry.targetSeg == modSegmentCached("bonuses", s_segBonuses))
		{
			const TeamTypes eSeeingTeam = (evalCtx.empireContext != NULL) ? (TeamTypes)evalCtx.empireContext->teamId() : NO_TEAM;
			if (kEntry.targetFk < 0 || kEntry.targetFk != (int)pKeyPlot->getBonusType(eSeeingTeam))
			{
				return false;
			}
		}
		else if (kEntry.targetSeg == modSegmentCached("routes", s_segRoutes))
		{
			if (kEntry.targetFk < 0 || kEntry.targetFk != (int)pKeyPlot->getRouteType())
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	if (!audienceOk(kEntry.aiOnly, evalCtx))
	{
		return false;
	}
	if (!applies(kEntry.enabled, kEntry.disabled, evalCtx))
	{
		return false;   // the conditioned evaluation -- the dormancy model (modifier.md §3)
	}
	// the ONE §3.7 resolver applies the per scaler AND the religion: counted-kind filter. A COUNT fact wants
	// the per-UNIT value instead, because it supplies its own Δ (see PerScaling on the declaration).
	int64_t iValue = (ePerScaling == PER_SCALE_APPLIED)
		? perScale(kEntry, evalCtx, kEntry.value)
		: (int64_t)kEntry.value;
	// modifier.md §2b: a negative WELLBEING amount routes to the opposing channel HERE, at the one resolve, so
	// every plane applies, books and withdraws through the same routed slot. The sign is tested BEFORE the
	// multiplicity multiply: multiplicity carries the apply/withdraw direction, so testing after it would route
	// a withdrawal of a positive deposit onto the twin.
	int iChannelRouted = iChannel;
	if (!bPercentSide && iValue < 0)
	{
		const int iTwin = CascadeChannelRegistry::wellbeingTwin(iChannel);
		if (iTwin >= 0)
		{
			iChannelRouted = iTwin;
			iValue = -iValue;
		}
	}
	iValue *= iMultiplier;

	iChannelOut = iChannelRouted;
	bPercentSideOut = bPercentSide;
	iValueOut = iValue;
	return true;
}

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
// condition count-atoms (docs/architecture/patterns.md §DRY (single implementation), never a parallel count path). A deposit without a per is
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
// The OPTION read no longer disambiguates anything -- the sets share NO id, so a lookup could ask both repos in
// either order and get the same answer. What it still does is keep a COMPLEX id from resolving in a SIMPLE game.
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
