#pragma once
#ifndef CV_JSON_MOD_ENTRY_H
#define CV_JSON_MOD_ENTRY_H

//
//	CvJsonModEntry / CvJsonModFamily -- the §3.9 "one entry shape" as JsonInfo-owned DATA (json.md §3.9: every
//	conditioned modifier value is `{ value, scope?, per?, enabled?, disabled? }`). A conditioned / varied-predicate
//	modifier family on a poco (a building's HAS_POWER happiness, a HAS_BONUS yield, a civic's IS_CAPITAL bonus, …) is
//	a CvJsonModFamily -- a LIST of these entries. The predicate vocabulary is the spec-defined CvJsonCondition
//	(HAS_*/IS_*/atoms, json §3.4/§3.5) reused VERBATIM -- never re-invented (owner 2026-07-07: the predicates are all
//	clearly defined in the spec). The cascade READS these to build its runtime deposits; this is pure DATA, ZERO
//	cascade runtime ([DEC-json-not-cascade]). Values are ×100 at load (the single human->×100 boundary).
//
//	Use this ONLY for genuinely-conditioned/varied families. Families whose condition is FIXED and baked into the
//	getter name (Religion state/holy, Corp change/produced) collapse to plain int members instead -- do NOT wrap those.
//

#include "CvJsonCondition.h"   // the spec-defined predicate/condition tree (enabled/disabled) + CvCascScope
#include <vector>

namespace picojson { class value; }

// json §3.6 units -- the NATURE of the magnitude (the combine math is family metadata, not carried here). FLAT/
// PERCENT/MULTIPLIER are the authoring set; the rest are the engine-internal / count-scaling units readJson recognizes.
enum CvCascUnit
{
	CASC_UNIT_UNKNOWN = 0,
	CASC_UNIT_FLAT, CASC_UNIT_PERCENT, CASC_UNIT_MULTIPLIER,
	CASC_UNIT_POST_MULTIPLIER, CASC_UNIT_RAW_PERCENT,
	CASC_UNIT_PER_POPULATION, CASC_UNIT_PER_SPECIALIST, CASC_UNIT_PER_CORPORATION_LEVEL,
	CASC_UNIT_COUNT   // the modifier.md §6 count-by-type leaf (freeSpecialists/allowedSpecialists) -- synthesized by
	                  // the CvJsonModifiers walk for a bare-number leaf; never an authored unit key
};
// The unit string (json key above a leaf) -> the enum. CASC_UNIT_UNKNOWN = not a unit key (an address segment).
CvCascUnit cascadeUnitFromString(const std::string& s);

// One §3.9 conditioned modifier entry. Owns its enabled/disabled condition trees. Noncopyable.
class CvJsonModEntry
{
public:
	int value100;                 // ×100 at load
	CvCascUnit unit;
	CvCascScope scope;
	CvJsonCondition* enabled;     // NULL = always-on
	CvJsonCondition* disabled;    // NULL = never-suppressed
	// --- the §3.7 `per` count-scaler (value × count(type)/each) ---
	bool hasPer;                  // the entry carries a per
	std::string perType;          // the per's type/token string (POPULATION / PROPERTY_X / ...)
	int perTypeId;                // FK-resolved engine id; -1 = a catch-all token (POPULATION/TURN/...)
	int perEach;                  // the quantum ("per 5 population" -> 5); default 1
	std::vector<int> perAnyOf;    // per.anyOf summed-count FK ids (json §3.7)
	CvJsonModEntry() : value100(0), unit(CASC_UNIT_FLAT), scope(CASC_SCOPE_CITY), enabled(NULL), disabled(NULL),
		hasPer(false), perTypeId(-1), perEach(1) {}
	~CvJsonModEntry() { delete enabled; delete disabled; }
private:
	CvJsonModEntry(const CvJsonModEntry&);            // noncopyable -- owns the condition trees
	CvJsonModEntry& operator=(const CvJsonModEntry&);
};

// A modifier family's parsed entries (one JSON leaf's list). Owns its entries. Noncopyable (held by-value on the
// noncopyable poco).
class CvJsonModFamily
{
public:
	std::vector<CvJsonModEntry*> entries;
	CvJsonModFamily() {}
	~CvJsonModFamily() { for (size_t i = 0; i < entries.size(); ++i) delete entries[i]; }

	// Parse a JSON leaf (a bare number, or a LIST of `{ value, enabled?, disabled? }` entries) into `entries`,
	// ×100'ing each value and reusing cascadeParseCondition for enabled/disabled. `unit`/`scope` come from the
	// family address (the key path above the leaf).
	void parseLeaf(const picojson::value& leaf, CvCascUnit unit, CvCascScope scope);

	bool empty() const { return entries.empty(); }
	int size() const { return (int)entries.size(); }

private:
	CvJsonModFamily(const CvJsonModFamily&);          // noncopyable -- owns its entries
	CvJsonModFamily& operator=(const CvJsonModFamily&);
};

#endif // CV_JSON_MOD_ENTRY_H
