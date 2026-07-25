#pragma once
#ifndef CV_MODIFIERS_H
#define CV_MODIFIERS_H

//
//	CvModifiers -- an entity's MODIFIER FAMILIES per json.md §6, COMPILED at load into the runtime forms
//	(patterns.md § coherent surface; info-rebuild.md toolkit item 2). The anatomy walk is LOAD-ONLY: parseEntity
//	walks every §6 family key of the entity's JSON ONCE, interning every string key to a typed id (family /
//	kind / scope / unit / FK targets -- [DEC-materialize-at-mapfrom]) and producing:
//	  - the compiled ENTRY LIST (every §3.9 deposit as a typed CvModEntry, condition trees prebuilt);
//	  - the per-group UNCONDITIONED ×100 SUMS under the packed (family, kind, scope, unit) slot key
//	    (Σflat vs Σpercent stay SEPARATE slots -- the unit is part of the key, modifier.md §2);
//	  - the per-family CONDITIONED ranges over the entry list (what the package rebuild, the pedia, and the
//	    what-if valuation walk).
//	After load every read is a compiled-structure fetch; nothing string-shaped survives into any runtime read
//	path. Pure data, ZERO cascade runtime ([DEC-json-not-cascade]); the DepositIndex push reads entries().
//

#include "CvModEntry.h"
#include "picojson.h"   // picojson::value + object -- parseEntity takes a picojson::object (object is a TYPEDEF, not
                        // a class: forward-declaring it as `class object;` collided with the real typedef and ICE'd VC7.1)
#include <string>
#include <utility>
#include <vector>

class CvModifiers
{
public:
	CvModifiers() {}
	~CvModifiers();

	// LOAD-TIME ONLY: walk every top-level MODIFIER-FAMILY key of an entity (a non-reserved, object-valued key --
	// json §1; jsonClassifyKey is the recognizer) and COMPILE it. The one place a family string is ever read.
	void parseEntity(const picojson::object& entity);
	void clearParsed();   // frees the entries + resets (the dtor body; the clear-first half of the section re-map)

	// LOAD-TIME ONLY (the readJson general reverse pass -- [DEC-one-reverse-view], modifier.md §4): land ONE
	// synthesized own-output entry on this (target) info's compiled surface -- the reverse side of a source's
	// target-keyed deposit ("+X while the source is present": the source's presence rides as the entry's
	// `enabled` condition). Takes OWNERSHIP; refinalizes the compiled forms so the entry is indistinguishable
	// from an authored conditioned entry to every reader. Part of the write-once-at-load window.
	void landReverseEntry(CvModEntry* pEntry);

	bool empty() const { return m_entries.empty(); }

	// --- the compiled read surface (typed ids only; no runtime string read) ---
	// Every compiled entry (the DepositIndex push's iteration surface + the load-time reverse passes' source).
	const std::vector<CvModEntry*>& entries() const { return m_entries; }
	// The conditioned entries (prebuilt trees), grouped: sorted by family, range-addressable per group.
	const std::vector<const CvModEntry*>& conditioned() const { return m_conditioned; }
	void conditionedRange(ModifierFamily eFamily, size_t& iBeginOut, size_t& iEndOut) const;
	// THE POINT READ: the load-compiled unconditioned ×100 sum of the (family, kind, scope, unit) slot --
	// one lookup over the packed sorted slot table, 0 calculation ([DEC-scope-is-an-axis]: kind and scope are
	// separate arguments, exactly as the JSON's own <family>.<scope>.<member> separates them).
	int sum100(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit) const;
	// The per-property point read (the open MODFAM_PROPERTY plane is keyed by the property's FK id).
	int propertySum100(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit) const;

private:
	// The load-time recursive family walk (strings live only here): a unit key ends the address (a §3.9 leaf);
	// a bare-number/array non-unit key is the count-by-type leaf; any other key recurses one segment deeper.
	void walk(std::vector<std::string>& segments, const picojson::value& node);
	void parseLeaf(const std::vector<std::string>& segments, const picojson::value& leaf, CvCascUnit eUnit,
	               const picojson::value* pNodeQual);
	void compileEntry(CvModEntry* pEntry, const std::vector<std::string>& segments);
	void finalizeCompiled();   // sorts the slot table + builds the conditioned per-family ranges

	std::vector<CvModEntry*> m_entries;                  // owned
	std::vector<const CvModEntry*> m_conditioned;        // borrowed views into m_entries, sorted by family
	std::vector<std::pair<int, int> > m_slots;           // packed slot key -> unconditioned ×100 sum, sorted
	std::vector<std::pair<int, int> > m_propertySlots;   // packed (propertyFk, scope, unit) -> ×100 sum, sorted

	CvModifiers(const CvModifiers&);              // noncopyable -- owns the entries
	CvModifiers& operator=(const CvModifiers&);
};

#endif // CV_MODIFIERS_H
