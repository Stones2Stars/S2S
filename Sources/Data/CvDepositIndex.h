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
//	The compiled SEGMENTS (family / scope / member / target + the FK-resolved target id) are ALSO the generator of
//	the data-derived event->cache routing (state-repositories.md end-state): a DOMAIN event's source names the
//	channels x scopes x targets it touches straight off its compiled deposits -- today's hand-wired hook masks are
//	the interim shape of that derivation.
//
//	Purely-organizational static-methods class: NO data members, never instantiated (patterns.md static-class law).
//	Game-thread only. The interner is APPEND-ONLY -- ids stay valid across a readJson re-map (rj_clearAllRepos +
//	re-map re-interns the same strings to the SAME ids; the compiled-record registry alone is dropped and refilled,
//	its keys being the freed infos). Query-side caches therefore may cache a hit forever but must RE-LOOKUP while
//	negative (a miss can turn into a hit after a re-map introduces new data).
//

#include "CvModEntry.h"   // CvCascUnit -- the entry's unit enum (the unit segment the push interns)
#include <string>
#include <vector>

class CvInfo;
class CvCondition;

//
//	The COMPILED DEPOSIT RECORD -- cascade-side ONLY ([DEC-json-not-cascade]: the retired info-side generic vector
//	and its struct are gone; this equivalent record lives in the cascade's own index and is populated from the spec
//	model's (address, CvModEntry) pairs at push time). The matchers read: addressId/unitId (whole-address +
//	unit-segment ids), nSeg + seg[] (the compiled dotted segments), targetFk (the FK-resolved INFOTYPE tail),
//	value100, enabled/disabled (borrowed pointers into the info-owned condition trees -- the InfoRepo-owned info
//	outlives the registry entry; clearCompiled() runs before any repo clear). The address/unit strings stay for
//	rendering/diagnostics only -- matching is ids-only.
//
struct CascadeDeposit
{
	enum { CASC_DEP_SEGS = 4 };
	std::string address;               // dotted address MINUS the unit (the CvModifiers family key)
	std::string unit;                  // the unit segment string (the entry's unit, spelled)
	int value100;                      // x100 fixed-point magnitude (CvModEntry::value100)
	const CvCondition* enabled;    // NULL = always-on (borrowed, never owned)
	const CvCondition* disabled;   // NULL = never-suppressed (borrowed, never owned)
	const CvCondition* unitQual;   // the §3.7 `unit:` predicate qualifier (borrowed; NULL = unqualified) --
	                                   // evaluated at the CONSUMER per candidate unit; plain sums must filter it
	bool hasPer;                       // the §3.7 per count-scaler rides the entry (borrowed detail below)
	std::string perType;               // the per's type/token string (kept like address/unit; a SELF token is
	                                   // collapsed to the SOURCE info's own type at push; "" = none)
	int perTypeId;                     // per type FK; -1 = a catch-all token (POPULATION/...)
	int perTokenSeg;                   // interned segment id of a CATCH-ALL token (perTypeId<0); -1 = typed/none --
	                                   // the resolver's string-free guard (an unresolved SELF skips the multiply)
	int perEach;                       // the per quantum (default 1)
	int perScope;                      // the per's scope, RESOLVED at push (authored, else the deposit's own scope -- json §3.7)
	const std::vector<int>* perAnyOf;  // per.anyOf FK ids (borrowed; NULL = none)
	const std::vector<std::string>* perAnyOfTypes;   // per.anyOf type strings, parallel to perAnyOf (borrowed; NULL = none)
	int addressId;                     // interned whole-address id
	int unitId;                        // interned unit-segment id
	int nSeg;                          // dotted segment count (may exceed CASC_DEP_SEGS; extras uncompiled)
	int seg[CASC_DEP_SEGS];            // interned segment ids (family / scope / member / target), -1 = none
	int targetFk;                      // FK-resolved engine id of an INFOTYPE tail segment, -1 = not a key
	// --- the RESOLVED cascade slot, filled once by compile() (CvCascadeChannels) -------------------------
	// The deposit IS the info's data; resolving its channel + dictionary onto the record is what lets a gather
	// be ONE pass over an info's deposits (add each into its slot) instead of N per-channel rescans -- and
	// without materializing a second copy of static info data anywhere.
	short chan;                        // CascadeChannel; -1 = not a cascade channel (unit-plane / retired family)
	short scopeIdx;                    // CascadeScope;   -1 = unrecognized scope segment
	bool  isPercent;                   // WHICH DICTIONARY -- the whole type axis (value vs percent)

	CascadeDeposit()
		: value100(0), enabled(NULL), disabled(NULL), unitQual(NULL), hasPer(false), perTypeId(-1), perTokenSeg(-1),
		  perEach(1), perScope(-1), perAnyOf(NULL), perAnyOfTypes(NULL),
		  addressId(-1), unitId(-1), nSeg(0), targetFk(-1),
		  chan(-1), scopeIdx(-1), isPercent(false)
	{ for (int i = 0; i < CASC_DEP_SEGS; ++i) seg[i] = -1; }
};

//
//	The COMPILED REVERSE ROUTE of a source info (#430 F0 R2, state-repositories.md "the event->cache routing is
//	DERIVED from the deposit index"). A DOMAIN event's source names the channels x scopes it touches straight off its
//	compiled deposits; this is that inversion, computed ONCE per source info and cached. It carries ONLY the
//	CROSS-SCOPE reach a source's EMPIRE/AREA/WORLD deposits route to -- the player-package bits, the city-package bits
//	every owner city realizes (empire/area deposits roll DOWN to all cities), and the world flag. The source's OWN
//	city floor is NOT here: for a BUILDING it is the operate/provides fixpoint's conservative mark (a building flip can
//	flip sibling buildings' active state, so the own-city effect is not bounded by the building's own deposits), added
//	by the consumer. So a city-only source routes to nothing here; only a genuine grantor fans out.
//	Granularity: the PERCENT branch is per-CHANNEL (R2b -- yield-percent -> CPK_YPCT only, commerce-percent ->
//	CPK_CPCT, gp/maint -> CPK_SCPCT+PSC_SC, buildRate -> CPK_BR+PSC_BR; grouped/unknown families fall to a
//	coarse-safe mask, never under-marking). The FLAT branch stays scope x unit x member coarse (its pulled-live vs
//	city-realized routing is not yet nailed per family -- kept safe rather than guessed).
//
struct SourceRoute
{
	int  playerBits;   // CascadePlayerPkg bits (PSC_*) marked on the owner
	int  cityBits;     // CascadeCityPkg bits (CPK_*) marked on EVERY owner city (the empire/area realization)
	bool world;        // a world-scope deposit is present -> mark CascadeWorldScope
	SourceRoute() : playerBits(0), cityBits(0), world(false) {}
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

	// THE REVERSE ROUTE (F0 R2): the source info's cross-scope package reach, unioned over its compiled deposits and
	// cached (lazy, first query; dropped by clearCompiled with the compiled registry -- its keys are the freed infos).
	// The invalidation consumer queries this O(1) to mark exactly the player/sibling/world packages a source feeds,
	// replacing buildingProcessed's inline per-deposit derivation. NULL / family-less info -> the empty route.
	static const SourceRoute& routeFor(const CvInfo* j);

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
