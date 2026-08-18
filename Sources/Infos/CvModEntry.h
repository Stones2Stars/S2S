#pragma once
#ifndef CV_MOD_ENTRY_H
#define CV_MOD_ENTRY_H

//
//	CvModEntry -- ONE COMPILED §3.9 modifier deposit (json.md §3.9 "the one entry shape"), the runtime form the
//	load compile pass produces (patterns.md § coherent surface; docs/architecture/patterns.md §Materialize at mapFrom). Every axis of the
//	full deposit address `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` is interned to a
//	typed id AT PARSE: family -> the closed ModifierFamily vocabulary, scope -> CvCascScope, member -> the
//	family's kind enum (CvInfoKinds.h), named-entity targets -> FK-resolved engine ids, conditions -> prebuilt
//	CvCondition trees. The raw authored segments stay as interned ids (spell-back capable) so the DepositIndex
//	push and the [READJSON] diagnostics can render the exact authored address -- no runtime read ever compares a
//	string. Values are ×100 at load (the single human->×100 boundary).
//

#include "CvCondition.h"   // the spec-defined predicate/condition tree (enabled/disabled) + CvCascScope
#include "CvInfoKinds.h"   // ModifierFamily + the shared kind-enum vocabulary
#include <string>
#include <vector>

namespace picojson { class value; }

// The unit string (json key above a leaf) -> the CvCascUnit enum (CvInfoKinds.h -- the vocabulary home).
// CASC_UNIT_UNKNOWN = not a unit key (an address segment).
CvCascUnit cascadeUnitFromString(const std::string& s);

// --- the address-segment interner (append-only, spell-back capable). The compiled entries carry interned ids
// for every authored address segment; the spellings serve the DepositIndex push + diagnostics only. ---
int modSegmentIntern(const std::string& szSegment);
int modSegmentLookup(const std::string& szSegment);   // -1 = never authored anywhere
const char* modSegmentSpell(int iSegmentId);          // "" for an invalid id

//	The CACHED lookup every READ PATH uses. `modSegmentLookup` takes a `std::string`, so passing a literal costs
//	a heap string construction plus a map walk on EVERY call -- which is docs/architecture/patterns.md §Materialize at mapFrom violated
//	wherever it sits under a per-turn or per-candidate loop. The caller owns the cache slot (init it to -1).
//
//	⚠ A HIT is cached forever; a MISS re-looks-up. That asymmetry is the whole discipline: the interner is
//	append-only, so a segment nothing has authored YET can become live later, while an id once assigned never
//	moves. ⛔ A plain `static const int` initialised on first call caches the -1 permanently and then answers
//	"nothing is keyed on this" for the rest of the session -- silently, since -1 is also the honest answer.
//
//	⚑ It lives HERE, beside the interner it reads, because a file-static copy per consumer is the DRY hazard
//	docs/architecture/patterns.md §DRY (single implementation) names: the next consumer cannot see it and writes a fifth one.
int modSegmentCached(const char* szSegment, int& iCache);

class CvModEntry;
// The ONE §3.7 `per` count-scaler parser ({type|anyOf, each?} or a bare type string) -> the entry's per fields.
// Shared: the modifier entries AND a trigger entry's `chance.per` both parse through it.
void jsonParsePer(CvModEntry* entry, const picojson::value& v);

// One compiled §3.9 modifier entry. Owns its enabled/disabled/unit-qualifier condition trees. Noncopyable.
class CvModEntry
{
public:
	enum { MOD_ENTRY_SEGS = 6 };

	// --- the interned five-axis address (typed at parse) ---
	ModifierFamily family;        // the closed vocabulary; MODFAM_PROPERTY = the open per-property plane
	int propertyFk;               // MODFAM_PROPERTY: the property's FK id; -1 otherwise
	CvCascScope scope;            // json §3.2 (default city)
	int kind;                     // the family's kind id (CvInfoKinds vocabulary / engine-enum axis);
	                              // 0 = the scope-wide amount; -1 = outside the vocabulary (memberSeg keeps it)
	int memberSeg;                // interned member-path id ('.'-joined for a nested member); -1 = memberless
	int targetSeg;                // interned §3.3 target / keyed-container token id; -1 = untargeted
	int targetFk;                 // FK-resolved engine id of a named-entity target key; -1 = none/unresolved
	int nSeg;                     // authored segment count (address minus unit; extras beyond the array uncompiled)
	int seg[MOD_ENTRY_SEGS];      // the authored segments, interned, in order -- the spell-back address source

	// --- the payload ---
	int value;                 // ×100 at load
	CvCascUnit unit;
	// --- the §3.9 `ai` AUDIENCE axis: the entry applies to AI PLAYERS ONLY (json §3.9 -- the optional `ai`
	// sibling block, same inner shape). Compiled as THIS FLAG, never an address segment: the authored `ai` hop
	// (upkeep.empire.unit.ai.percent -- the handicap human/AI dual-leaf pattern) is consumed by the walk, so
	// the member path kind-resolves cleanly and the audience rides the entry. Point sums stay HUMAN-audience by
	// default (an aiOnly entry folds into the SEPARATE ai slot table); an aiOnly-inclusive read is an explicit
	// parameter (CvModifiers::sum), and every gated record read audience-filters via the asking player. ---
	bool aiOnly;
	CvCondition* enabled;         // NULL = always-on (owned)
	CvCondition* disabled;        // NULL = never-suppressed (owned)
	// --- the §3.7 `unit:` predicate qualifier (cargo.space.{unit: IS_AIR}; happiness.empire.cities.{unit:
	// IS_MILITARY}) -- evaluated at the CONSUMER against each candidate unit (live-on-top per
	// docs/cascade.md §2b (unit-carried modifiers apply on top, live)); NULL = unqualified. Owned. ---
	CvCondition* unitQual;
	// --- the §3.7 counted-kind RELIGION filter (`religion: "!IS_STATE_RELIGION"`, ruling 23): the value scales
	// by the count of the city's religions matching this predicate (each religion tested via ctx.religion --
	// cascadeCountCityReligions); NULL = unfiltered. Owned. ---
	CvCondition* religionQual;
	// --- the §3.3/§3.9 ranked-selection qualifiers (max:/orderedBy/orderedByDescending -- ruling 25: they ride
	// individual entries; node-level spelling is shorthand). PARSE-CARRIED ONLY: the ranked SELECTION evaluation
	// is the parked plan (plans/parked/ranked-target-selection.md) -- until it lands a ranked entry applies
	// unranked, exactly as the pre-parse data did. ---
	bool hasRankQual;             // any ranked qualifier present on this entry
	int rankMax;                  // literal `max:` N; -1 = token-carried (rankMaxToken) or absent
	std::string rankMaxToken;     // `max:` token spelling (TARGET_NUM_CITIES); empty = literal/none
	int orderedBySeg;             // interned metric segment id (CITY_SIZE); -1 = none
	bool orderedDescending;       // orderedByDescending vs orderedBy
	// --- the §3.7 `per` count-scaler (value × count(type)/each) ---
	bool hasPer;                  // the entry carries a per
	std::string perType;          // the per's type/token string (POPULATION / PROPERTY_X / ...) -- a catch-all
	                              // token survives HERE (perTypeId stays -1); the DepositIndex push carries it on
	std::vector<std::string> perAnyOfTypes;   // per.anyOf type strings, PARALLEL to perAnyOf -- the resolver's
	                              // prefix routing (cascadeCountOf) needs the kind, an id alone is ambiguous
	int perTypeId;                // FK-resolved engine id; -1 = a catch-all token (POPULATION/TURN/...)
	int perEach;                  // the quantum ("per 5 population" -> 5); default 1
	int perScope;                 // the AUTHORED per scope (a CvCascScope value); -1 = absent -> the deposit's
	                              // own scope (json §3.7: cross-city scopes resolve via the tally, city/plot local)
	std::vector<int> perAnyOf;    // per.anyOf summed-count FK ids (json §3.7)
	// --- the §3.7 `per.above` over-threshold scaler (ruling 26): value × max(0, count − above), composing with
	// `each` ((count − above) / each). Threshold literal or TOKEN: `above: "CITY_LIMIT"` reads the depositing
	// civic's base-limit config (resolved into perAbove by CvCivicInfo::mapFrom via resolveAboveToken -- the
	// SELF-collapse precedent), × the world-size scale percent at EVAL (MMKernel::perScale). ---
	bool hasAbove;                // the per carries an `above:`
	int perAbove;                 // resolved threshold BASE (literal, or the source-resolved config); -1 = unresolved
	std::string perAboveToken;    // the token spelling (CITY_LIMIT = base × world scale at eval); empty = literal

	bool isConditioned() const
	{ return enabled != NULL || disabled != NULL || unitQual != NULL || religionQual != NULL || hasPer || hasRankQual; }
	// Folds into its group's compiled unconditioned sum: unconditioned, untargeted, and inside the kind
	// vocabulary. Everything else stays an entry-list read (keyed walks, conditioned lists, unkinded members).
	bool isPointFoldable() const { return !isConditioned() && targetSeg < 0 && targetFk < 0 && kind >= 0; }
	// The authored dotted address (minus unit), spelled back from the interned segments -- the DepositIndex
	// push's render source + the [READJSON] sample lines. Diagnostics/push only, never a runtime read.
	std::string address() const;

	CvModEntry()
		: family(MODFAM_NONE), propertyFk(-1), scope(CASC_SCOPE_CITY), kind(-1), memberSeg(-1), targetSeg(-1),
		  targetFk(-1), nSeg(0),
		  value(0), unit(CASC_UNIT_FLAT), aiOnly(false), enabled(NULL), disabled(NULL), unitQual(NULL), religionQual(NULL),
		  hasRankQual(false), rankMax(-1), orderedBySeg(-1), orderedDescending(false),
		  hasPer(false), perTypeId(-1), perEach(1), perScope(-1),
		  hasAbove(false), perAbove(-1)
	{
		for (int i = 0; i < MOD_ENTRY_SEGS; ++i)
		{
			seg[i] = -1;
		}
	}
	~CvModEntry() { delete enabled; delete disabled; delete unitQual; delete religionQual; }

private:
	CvModEntry(const CvModEntry&);            // noncopyable -- owns the condition trees
	CvModEntry& operator=(const CvModEntry&);
};

#endif // CV_MOD_ENTRY_H
