#pragma once
#ifndef CV_MOD_ENTRY_H
#define CV_MOD_ENTRY_H

//
//	CvModEntry -- ONE COMPILED §3.9 modifier deposit (json.md §3.9 "the one entry shape"), the runtime form the
//	load compile pass produces (patterns.md § coherent surface; [DEC-materialize-at-mapfrom]). Every axis of the
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
	int value100;                 // ×100 at load
	CvCascUnit unit;
	CvCondition* enabled;         // NULL = always-on (owned)
	CvCondition* disabled;        // NULL = never-suppressed (owned)
	// --- the §3.7 `unit:` predicate qualifier (cargo.space.{unit: IS_AIR}; happiness.empire.cities.{unit:
	// IS_MILITARY}) -- evaluated at the CONSUMER against each candidate unit (live-on-top per
	// [DEC-unit-modifiers-on-top]); NULL = unqualified. Owned. ---
	CvCondition* unitQual;
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

	bool isConditioned() const { return enabled != NULL || disabled != NULL || unitQual != NULL || hasPer; }
	// Folds into its group's compiled unconditioned sum: unconditioned, untargeted, and inside the kind
	// vocabulary. Everything else stays an entry-list read (keyed walks, conditioned lists, unkinded members).
	bool isPointFoldable() const { return !isConditioned() && targetSeg < 0 && targetFk < 0 && kind >= 0; }
	// The authored dotted address (minus unit), spelled back from the interned segments -- the DepositIndex
	// push's render source + the [READJSON] sample lines. Diagnostics/push only, never a runtime read.
	std::string address() const;

	CvModEntry()
		: family(MODFAM_NONE), propertyFk(-1), scope(CASC_SCOPE_CITY), kind(-1), memberSeg(-1), targetSeg(-1),
		  targetFk(-1), nSeg(0),
		  value100(0), unit(CASC_UNIT_FLAT), enabled(NULL), disabled(NULL), unitQual(NULL),
		  hasPer(false), perTypeId(-1), perEach(1), perScope(-1)
	{
		for (int i = 0; i < MOD_ENTRY_SEGS; ++i)
		{
			seg[i] = -1;
		}
	}
	~CvModEntry() { delete enabled; delete disabled; delete unitQual; }

private:
	CvModEntry(const CvModEntry&);            // noncopyable -- owns the condition trees
	CvModEntry& operator=(const CvModEntry&);
};

#endif // CV_MOD_ENTRY_H
