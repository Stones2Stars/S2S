#pragma once
#ifndef CV_CASCADE_DEPOSIT_INDEX_H
#define CV_CASCADE_DEPOSIT_INDEX_H

//
//	DepositIndex -- the #430 COMPILED DEPOSIT INDEX: the load-time strings->ints compile over the info-side modifier
//	families (modifier-substrate.md "the compiled deposit index"; cutover.md flip lesson: "the JSON stays
//	HUMAN-shaped ... and the LOAD step programmatically compiles it into the top-down routing"). The INPUT SOURCE is
//	the spec model ([DEC-json-not-cascade]): readJson's push walks each mapped info's compiled CvModifiers entries
//	(`j->getModifiers()` / `j->getWhenObsolete()`) -- per CvModEntry, the spelled-back address + the entry's
//	unit are interned ONCE into a cascade-side compiled record; the hot-path matchers (MMKernel et al.) then compare
//	INTS, and a query address that was never authored anywhere answers 0 without touching a single deposit.
//
//	The compiled records (typed family/kind/scope/channel axes + the FK-resolved target id) are ALSO the generator
//	of the data-derived event->cache routing (state-repositories.md): a DOMAIN event's source names the
//	channels x scopes x targets it touches straight off its compiled deposits -- routeFor + the condition-dependency
//	routes below ARE that derivation; no event site carries a hand-wired mask.
//
//	Purely-organizational static-methods class: NO data members, never instantiated (patterns.md static-class law).
//	Game-thread only. The interner is APPEND-ONLY -- ids stay valid across a readJson re-map (rj_clearAllRepos +
//	re-map re-interns the same strings to the SAME ids; the compiled-record registry alone is dropped and refilled,
//	its keys being the freed infos). Query-side caches therefore may cache a hit forever but must RE-LOOKUP while
//	negative (a miss can turn into a hit after a re-map introduces new data).
//

#include "CvModEntry.h"                 // CvCascUnit -- the entry's unit enum (the unit segment the push interns)
#include "CvCascadeChannelRegistry.h"   // the minted channel vocabulary + per-scope bit spaces the routes speak
#include <string>
#include <vector>

class CvInfo;
class CvCondition;

//
//	The COMPILED DEPOSIT RECORD -- cascade-side ONLY ([DEC-json-not-cascade]: the retired info-side generic vector
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
		: value(0), aiOnly(false), enabled(NULL), disabled(NULL), unitQual(NULL), religionQual(NULL), hasPer(false), perTypeId(-1), perTokenSeg(-1),
		  perEach(1), perScope(-1), hasAbove(false), perAbove(-1), perAboveSeg(-1), perAnyOf(NULL), perAnyOfTypes(NULL),
		  addressId(-1), unitId(-1), nSeg(0), targetFk(-1),
		  family(-1), kind(-1), propertyFk(-1), channel(-1), scopeIdx(-1), isPercent(false)
	{ for (int i = 0; i < CASC_DEP_SEGS; ++i) seg[i] = -1; }
};

//
//	The COMPILED REVERSE ROUTE of a source info (state-repositories.md: "the event->cache routing is DERIVED
//	FROM THE DATA, never hand-wired -- the dirty flags fall out of the deposit addresses"). A DOMAIN event's
//	source names the channels x scopes it touches straight off its compiled deposits; this is that inversion,
//	computed ONCE per source info (lazily, post-load -- the registry layouts are complete by then) and cached.
//
//	THE UNIFORM MODEL: per package scope, the 64-bit channel mask (in THAT scope's local bit space,
//	CascadeChannelRegistry) the source's deposits land in -- the consumer marks the owner object each event
//	names (the plot's / city's / player's / area's / team's package). The registry's bit contract is
//	ORDER-INDEPENDENT (channel slots append-only, receiver bits in a fixed top region), so a cached route's
//	bits stay valid across later channel minting -- caching needs no ordering guarantee against the load's
//	push. ONE derivation marks BOTH levels
//	([DEC-uniform-cache-shape]): the receiver-fan masks name the SUM slots those packages feed -- the city's
//	realized rates (cityFanAll when an above-city deposit rolls DOWN to every owner city) and the player's
//	empire sums. There is NO dependency-ordered rebuild: package and sum are both marked dirty, and a sum's
//	rebuild reads its packages through their own lazy dirty-check.
//
//	Unit-qualified deposits are EXCLUDED: a unit-carried value rides ON TOP live and never dirties any cache
//	([DEC-unit-modifiers-on-top]).
//
struct SourceRoute
{
	int64_t packageMask[CASCADE_PACKAGE_SCOPES];   // per-scope package channel bits (that scope's bit space)
	int64_t citySumMask;                           // receiver bits (CITY bit space) the deposits feed
	int64_t empireSumMask;                         // receiver bits (EMPIRE bit space) the deposits feed
	bool cityFanAll;    // an above-city deposit feeds EVERY owner city's sums (else only the event's own city)
	bool world;         // a world-scope deposit is authored (world is CONFIG -- census visibility, no package)

	SourceRoute() : citySumMask(0), empireSumMask(0), cityFanAll(false), world(false)
	{
		for (int i = 0; i < CASCADE_PACKAGE_SCOPES; ++i)
		{
			packageMask[i] = 0;
		}
	}
	bool empty() const
	{
		if (citySumMask != 0 || empireSumMask != 0)
		{
			return false;
		}
		for (int i = 0; i < CASCADE_PACKAGE_SCOPES; ++i)
		{
			if (packageMask[i] != 0)
			{
				return false;
			}
		}
		return true;
	}
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

	// The compiled records of one source info -- the matchers' iteration surface (a shared empty vector when the
	// info authored none / is NULL). whenObsoleteFor = the building's obsolete-state tree (json #4.2).
	static const std::vector<CascadeDeposit>& depositsFor(const CvInfo* j);
	static const std::vector<CascadeDeposit>& whenObsoleteFor(const CvInfo* j);

	// THE REVERSE ROUTE: the source info's per-scope package masks + receiver fan, unioned over its compiled
	// deposits and cached (lazy, first query -- post-load, so the registry layouts are complete; dropped by
	// clearCompiled with the compiled registry, its keys being the freed infos). The modifier consumer queries
	// this O(1) to mark exactly the packages a source feeds AND the sum slots they roll into (the ONE mark
	// derivation, [DEC-uniform-cache-shape]). NULL / family-less info -> the empty route.
	static const SourceRoute& routeFor(const CvInfo* j);

	// THE CONDITION-DEPENDENCY ROUTES -- the same derivation applied to the deposits' OWN gates (modifier.md
	// §3: conditions re-evaluate on every recompute, so the state a condition READS must mark the packages
	// that carry the conditioned deposit). Compiled lazily in ONE global pass over every compiled record's
	// enabled/disabled trees, per scalers, and religion filters -- the routing stays a pure function of the
	// index, never a hand-coded mask per event site. NULL = nothing anywhere depends on that state (mark
	// nothing). Keys:
	//  - an INFOTYPE the state names (a presence atom's `type`, a parameterized predicate's `param`, a typed
	//    `per`): dependencyForType, by the TYPE string ("BUILDING_X", "RELIGION_Y", ...);
	//  - a counter/token a `per` reads (POPULATION, CITY, ERA, GOLD_RATE, ...): dependencyForToken;
	//  - a bare predicate's state (IS_GOLDEN_AGE, IS_CAPITAL, HAS_POWER, ...): dependencyForPredicate;
	//  - the counted-religion filter class (`religion:` qualifiers + religion-scoped counts): dependencyForReligionCounts.
	static const SourceRoute* dependencyForType(const std::string& szType);
	static const SourceRoute* dependencyForToken(const char* szToken);
	static const SourceRoute* dependencyForPredicate(CvCascPredKind ePredicate);
	static const SourceRoute* dependencyForReligionCounts();

	// Fill a record's compiled fields from its address/unit strings (push-time; the strings stay for
	// rendering/diagnostics). Splits the dotted address, interns each segment (the first CASC_DEP_SEGS kept),
	// interns the whole address, and FK-resolves the LAST segment to an engine info id when it is an INFOTYPE key.
	static void compile(CascadeDeposit& d);

	// The unit enum's segment spelling (the exact reverse of cascadeUnitFromString; "" for UNKNOWN) -- the push's
	// unit-segment source, shared with the [READJSON] census sample lines.
	static const char* unitSegment(CvCascUnit u);

	// Lazily-cached SEGMENT ids for per-info-type key strings (TYPE string -> segment id) -- kills all string
	// handling in the per-plot keyed walks. -1 = that TYPE was never authored as a deposit key (nothing matches).
	// Hits cache forever; misses re-lookup (append-only interner, see the header note).
	static int segIdForTerrain(int i);
	static int segIdForFeature(int i);
	static int segIdForBonus(int i);
	static int segIdForImprovement(int i);
	static int segIdForBuilding(int i);
};

#endif // CV_CASCADE_DEPOSIT_INDEX_H
