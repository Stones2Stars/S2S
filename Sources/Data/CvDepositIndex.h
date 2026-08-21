#pragma once
#ifndef CV_CASCADE_DEPOSIT_INDEX_H
#define CV_CASCADE_DEPOSIT_INDEX_H

//
//	DepositIndex -- the #430 COMPILED DEPOSIT INDEX: the load-time strings->ints compile over the info-side modifier
//	families (modifier-substrate.md "the compiled deposit index"; cutover.md flip lesson: "the JSON stays
//	HUMAN-shaped ... and the LOAD step programmatically compiles it into the top-down routing"). The INPUT SOURCE is
//	the spec model (docs/architecture/patterns.md §The INFO DATA-OUT contract (info-side, never cascade-side)): readJson's push walks each mapped info's compiled CvModifiers entries
//	(`j->getModifiers()` / `j->getWhenObsolete()`) -- per CvModEntry, the spelled-back address + the entry's
//	unit are interned ONCE into a cascade-side compiled record; the hot-path matchers (MMKernel et al.) then compare
//	INTS, and a query address that was never authored anywhere answers 0 without touching a single deposit.
//
//	The compiled records (typed family/kind/scope/channel axes + the FK-resolved target id) are ALSO the generator
//	of the data-derived event->apply routing (state-repositories.md THE MAINTAINED SUM): the gatedBy* reverse
//	routes below hand plane B and C's appliers the exact deposits an atom gates or a count scales, straight off
//	the compiled index -- never a hand-wired list per event site.
//
//	Purely-organizational static-methods class: NO data members, never instantiated (patterns.md static-class law).
//	Game-thread only. The interner is APPEND-ONLY -- ids stay valid across a readJson re-map (rj_clearAllRepos +
//	re-map re-interns the same strings to the SAME ids; the compiled-record registry alone is dropped and refilled,
//	its keys being the freed infos). Query-side caches therefore may cache a hit forever but must RE-LOOKUP while
//	negative (a miss can turn into a hit after a re-map introduces new data).
//

#include "CvModEntry.h"                 // CvCascUnit -- the entry's unit enum (the unit segment the push interns)
#include "CvCascadeChannelRegistry.h"   // the minted channel vocabulary (registerDeposit at push; CASCADE_PACKAGE_SCOPES)
#include <map>
#include <set>
#include <string>
#include <vector>

class CvInfo;
class CvCondition;

//
//	The COMPILED DEPOSIT RECORD -- cascade-side ONLY (docs/architecture/patterns.md §The INFO DATA-OUT contract (info-side, never cascade-side): the retired info-side generic vector
//	and its struct are gone; this equivalent record lives in the cascade's own index and is populated from the spec
//	model's (address, CvModEntry) pairs at push time). The matchers read: addressId/unitId (whole-address +
//	unit-segment ids), nSeg + seg[] (the compiled dotted segments), targetFk (the FK-resolved INFOTYPE tail),
//	value, enabled/disabled (borrowed pointers into the info-owned condition trees -- the InfoRepo-owned info
//	outlives the registry entry; clearCompiled() runs before any repo clear). The address/unit strings stay for
//	rendering/diagnostics only -- matching is ids-only.
//
struct CascadeDeposit
{
	enum { CASC_DEP_SEGS = 4 };
	// The §3.9 ENTRY this record was compiled from. Held so a dependency route can hand the APPLY path the
	// exact entries an atom gates or a count scales, and the apply resolves them through the ONE per-entry
	// resolve (MMKernel::resolveEntry) rather than growing a second copy of it for this carrier
	// (docs/architecture/patterns.md §DRY (single implementation)). ⚑ Lifetime is exact, not assumed: entries live on the WRITE-ONCE info and
	// this index is dropped by clearCompiled() before any repo clear frees them, so the pointer cannot outlive
	// its target.
	const CvModEntry* entry;
	std::string address;               // dotted address MINUS the unit (the CvModifiers family key)
	std::string unit;                  // the unit segment string (the entry's unit, spelled)
	int value;                      // x100 fixed-point magnitude (CvModEntry::value)
	bool aiOnly;                       // the §3.9 `ai` AUDIENCE flag (CvModEntry::aiOnly -- never an address
	                                   // segment, so an ai deposit's address EQUALS its base twin's): every
	                                   // gated matcher audience-filters via MMKernel::audienceOk(ec) -- an
	                                   // aiOnly record contributes only when the asking player is an AI
	const CvCondition* enabled;    // NULL = always-on (borrowed, never owned)
	const CvCondition* disabled;   // NULL = never-suppressed (borrowed, never owned)
	const CvCondition* unitQual;   // the §3.7 `unit:` predicate qualifier (borrowed; NULL = unqualified) --
	                                   // evaluated at the CONSUMER per candidate unit; plain sums must filter it
	const CvCondition* religionQual;   // the §3.7 `religion:` counted-kind filter (borrowed; NULL = unfiltered) --
	                                   // the resolver scales by the matching-religion count (cascadeCountCityReligions)
	bool hasPer;                       // the §3.7 per count-scaler rides the entry (borrowed detail below)
	std::string perType;               // the per's type/token string (kept like address/unit; a SELF token is
	                                   // collapsed to the SOURCE info's own type at push; "" = none)
	int perTypeId;                     // per type FK; -1 = a catch-all token (POPULATION/...)
	int perTokenSeg;                   // interned segment id of a CATCH-ALL token (perTypeId<0); -1 = typed/none --
	                                   // the resolver's string-free guard (an unresolved SELF skips the multiply)
	int perEach;                       // the per quantum (default 1)
	int perScope;                      // the per's scope, RESOLVED at push (authored, else the deposit's own scope -- json §3.7)
	bool hasAbove;                     // the §3.7 `per.above` over-threshold scaler rides the entry (ruling 26)
	int perAbove;                      // the threshold BASE (literal / source-resolved config); -1 = unresolved
	int perAboveSeg;                   // interned segment id of the above TOKEN spelling (CITY_LIMIT -> the
	                                   // eval-time world-size scaling leg); -1 = literal/none
	const std::vector<int>* perAnyOf;  // per.anyOf FK ids (borrowed; NULL = none)
	const std::vector<std::string>* perAnyOfTypes;   // per.anyOf type strings, parallel to perAnyOf (borrowed; NULL = none)
	int addressId;                     // interned whole-address id
	int unitId;                        // interned unit-segment id
	int nSeg;                          // dotted segment count (may exceed CASC_DEP_SEGS; extras uncompiled)
	int seg[CASC_DEP_SEGS];            // interned segment ids (family / scope / member / target), -1 = none
	int targetFk;                      // FK-resolved engine id of an INFOTYPE tail segment, -1 = not a key
	// --- the RESOLVED cascade slot, copied from the compiled CvModEntry at push (the entry typed every axis
	// at parse) + minted through the CascadeChannelRegistry. The deposit IS the info's data; resolving its
	// channel + dictionary onto the record is what lets a gather be ONE pass over an info's deposits (add each
	// into its slot) instead of N per-channel rescans -- and without materializing a second copy of static
	// info data anywhere.
	short family;                      // ModifierFamily (the closed vocabulary; MODFAM_PROPERTY = the open plane)
	short kind;                        // the family's kind id; -1 = outside the vocabulary (batch-pending member)
	int propertyFk;                    // MODFAM_PROPERTY: the property's FK id; -1 otherwise
	int channel;                       // the minted registry channel id; -1 = not a package channel
	short scopeIdx;                    // CvCascScope -- the deposit's authored scope
	bool  isPercent;                   // WHICH DICTIONARY -- the whole type axis (value vs percent)

	CascadeDeposit()
		: entry(NULL), value(0), aiOnly(false), enabled(NULL), disabled(NULL), unitQual(NULL), religionQual(NULL), hasPer(false), perTypeId(-1), perTokenSeg(-1),
		  perEach(1), perScope(-1), hasAbove(false), perAbove(-1), perAboveSeg(-1), perAnyOf(NULL), perAnyOfTypes(NULL),
		  addressId(-1), unitId(-1), nSeg(0), targetFk(-1),
		  family(-1), kind(-1), propertyFk(-1), channel(-1), scopeIdx(-1), isPercent(false)
	{ for (int i = 0; i < CASC_DEP_SEGS; ++i) seg[i] = -1; }
};

class DepositIndex
{
public:
	// Intern (assign-on-first-sight) -- the load-time compile side; append-only.
	static int internSegment(const std::string& s);
	static int internAddress(const std::string& s);
	// Lookup WITHOUT interning -- the query side. -1 = never authored anywhere => any gated sum over it is 0.
	static int lookupSegment(const std::string& s);
	static int lookupAddress(const std::string& s);

	// THE PUSH (readJson load, once per mapped info): walk j->getModifiers()->entries() (+ j->getWhenObsolete())
	// and compile every entry into this index's registry -- the spec-model input seam of the compiled index (the
	// runtime shape below it is unchanged). NULL / family-less infos no-op.
	static void pushInfo(const CvInfo* j);

	// Re-map safety (rj_clearAllRepos): drop the compiled registry -- its keys are the about-to-be-freed infos.
	// The interner is NOT cleared (append-only law; ids survive the re-map).
	static void clearCompiled();

	// The dependency/gated-deposit compile (the routes below) -- runs once at the END of each loadJson pass,
	// after every pushInfo has landed, so the accessors are bare fetches. Idempotent; clearCompiled resets it.
	static void compileDependencies();

	// The compiled records of one source info -- the matchers' iteration surface (a shared empty vector when the
	// info authored none / is NULL). whenObsoleteFor = the building's obsolete-state tree (json #4.2).
	static const std::vector<CascadeDeposit>& depositsFor(const CvInfo* j);
	static const std::vector<CascadeDeposit>& whenObsoleteFor(const CvInfo* j);

	// ⚖ THE CONDITION-DEPENDENCY ROUTES, ANSWERED AS DEPOSITS -- what planes B and C apply. Compiled by
	// compileDependencies() in ONE global pass over every compiled record's enabled/disabled trees, per
	// scalers, and religion filters -- the routing stays a pure function of the index, never a hand-coded
	// list per event site. The APPLY gets the DEPOSITS themselves: the exact entries that atom gates or that
	// count scales, so it can move each slot by that entry's own resolved value (docs/cascade.md §THE MAINTAINED SUM: B is
	// ±value × Δcount on the COUNT fact, C is ±value on the ATOM's verdict crossing). Keys:
	//  - an INFOTYPE the state names (a presence atom's `type`, a parameterized predicate's `param`, a typed
	//    `per`): gatedByType, by the TYPE string ("BUILDING_X", "RELIGION_Y", ...);
	//  - a counter/token a `per` reads (POPULATION, CITY, ERA, GOLD_RATE, ...): gatedByToken;
	//  - a bare predicate's state (IS_GOLDEN_AGE, IS_CAPITAL, HAS_POWER, ...): gatedByPredicate;
	//  - the counted-religion filter class (`religion:` qualifiers + religion-scoped counts): gatedByReligionCounts.
	// ⛔ EACH DEPOSIT IS PAIRED WITH ITS OWNING SOURCE, and that is not bookkeeping: a gated deposit applies
	// only where its source is LIVE. The count route tests that with an O(1) has() at the owner and applies for
	// nobody else, which is exactly what makes source-then-count and count-then-source converge
	// (state-repositories.md § THE INVARIANT, ORDER-INDEPENDENCE).
	// NULL = nothing anywhere depends on that state.
	struct GatedDeposit
	{
		const CvInfo* source;             // whose deposit it is -- the subject of the liveness test
		const CascadeDeposit* deposit;    // the compiled record (its `entry` is the ONE resolve's input)
		int sourceIndex;                  // that source's dense index -- the apply's liveness key (see below)
		GatedDeposit() : source(NULL), deposit(NULL), sourceIndex(-1) {}
		GatedDeposit(const CvInfo* s, const CascadeDeposit* d, int i) : source(s), deposit(d), sourceIndex(i) {}
	};
	// ⚖ THE SOURCE INDEX -- a dense id per pushed source info, minted at push and stable for the load.
	// The apply path records WHAT IT HAS DEPOSITED at an owner keyed on this, which is what plane B and C test
	// before moving an already-deposited amount (docs/cascade.md §THE MAINTAINED SUM: the count applies for every deposit whose
	// source is already live).
	// ⛔ IT IS DELIBERATELY NOT THE ENGINE ID, and not a (kind, id) pair. Keying on the engine id would force the
	// apply to route by INFOTYPE prefix to know which registry -- a per-call string walk on the event path, and a
	// second copy of a routing table that already exists elsewhere. A dense index needs neither.
	// ⚑ AND IT IS THE RIGHT QUESTION, not merely the cheap one: the HAVE axis would answer "does this city hold
	// that building", which is NOT the same as "did that building's deposits land here" -- a PRESENT but DORMANT
	// building deposits nothing ([enabler.md] §3.2). The apply's own record cannot disagree with what it applied.
	static int sourceIndexOf(const CvInfo* j);

	static const std::vector<GatedDeposit>* gatedByType(const std::string& szType);
	static const std::vector<GatedDeposit>* gatedByToken(const char* szToken);
	static const std::vector<GatedDeposit>* gatedByPredicate(CvCascPredKind ePredicate);
	static const std::vector<GatedDeposit>* gatedByReligionCounts();

	// The PROPERTY boundaries the DEPOSIT gates declare (PROPERTY_ id -> boundary values), collected in the
	// dependency scan. One half of the ONE authored-boundary registry the band emit tests -- the enabler's
	// operate bands are the other half, and EnablerKernel::propertyBandThresholds unions the two.
	static const std::map<int, std::set<int> >& propertyGateThresholds();

	// Fill a record's compiled fields from its address/unit strings (push-time; the strings stay for
	// rendering/diagnostics). Splits the dotted address, interns each segment (the first CASC_DEP_SEGS kept),
	// interns the whole address, and FK-resolves the LAST segment to an engine info id when it is an INFOTYPE key.
	static void compile(CascadeDeposit& d);

	// The unit enum's segment spelling (the exact reverse of cascadeUnitFromString; "" for UNKNOWN) -- the push's
	// unit-segment source, shared with the [READJSON] census sample lines.
	static const char* unitSegment(CvCascUnit u);

};

#endif // CV_CASCADE_DEPOSIT_INDEX_H
