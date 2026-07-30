#pragma once
#ifndef CV_MODIFIERS_H
#define CV_MODIFIERS_H

//
//	CvModifiers -- an entity's MODIFIER FAMILIES per json.md §6, COMPILED at load into the runtime forms
//	(patterns.md § coherent surface; info-rebuild.md toolkit item 2). The anatomy walk is LOAD-ONLY: parseEntity
//	walks every §6 family key of the entity's JSON ONCE, interning every string key to a typed id (family /
//	kind / scope / unit / FK targets -- [DEC-materialize-at-mapfrom]) and producing:
//	  - the compiled ENTRY LIST -- COMPLETE (ruling 29): every §3.9 deposit as a typed CvModEntry, condition
//	    trees prebuilt, UNCONDITIONED entries retained as entries (per-entry text + per-entry attribution both
//	    require the full list); this list is the ONE compiled source;
//	  - the per-group UNCONDITIONED ×100 SUMS under the packed (family, kind, scope, unit) slot key, DERIVED
//	    from the retained list at compile end (finalizeCompiled -- one derivation, list -> sums, never two
//	    parallel fills that can drift; Σflat vs Σpercent stay SEPARATE slots -- the unit is part of the key,
//	    modifier.md §2);
//	  - the per-family CONDITIONED ranges -- a family-sorted view over the CONDITIONED SUBSET of the list
//	    (what the package rebuild, the pedia, and the what-if valuation walk).
//	After load every read is a compiled-structure fetch; nothing string-shaped survives into any runtime read
//	path. Pure data, ZERO cascade runtime ([DEC-json-not-cascade]); the DepositIndex push reads entries().
//

#include "CvModEntry.h"
#include "picojson.h"   // picojson::value + object -- parseEntity takes a picojson::object (object is a TYPEDEF, not
                        // a class: forward-declaring it as `class object;` collided with the real typedef and ICE'd VC7.1)
#include <string>
#include <utility>
#include <vector>

struct ModNodeQuals;   // the node-level §3.9 qualifier shorthand carrier (defined in the .cpp; load-time only)

// The §3.9 `ai` AUDIENCE axis of a point read, spelled out (the three askable planes): HUMAN = the base slot
// table alone (what a human player experiences); AI_ONLY = the aiOnly twin table alone (the `ai` sibling leaf
// by itself -- never computed as inclusive − human); INCLUSIVE = base + twin (what an AI player experiences).
enum CvModAudience
{
	MOD_AUDIENCE_HUMAN = 0,
	MOD_AUDIENCE_AI_ONLY,
	MOD_AUDIENCE_INCLUSIVE
};

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

	// LOAD-TIME ONLY (the source subclass's mapFrom -- the SELF-collapse precedent, json §3.1): resolve every
	// entry whose `per.above` carries szToken to the SOURCE's own config value (CITY_LIMIT = the depositing
	// civic's identity.cityLimit base). The token spelling STAYS on the entry -- it marks the eval-time
	// world-size scaling leg (MMKernel::perScale multiplies the base by getCityLimitsScalePercent()/100).
	void resolveAboveToken(const char* szToken, int iBase);

	// LOAD-TIME ONLY (the same SELF-collapse precedent, applied to the `per` COUNT axis): resolve every entry
	// whose per.type carries szToken to the SOURCE's own engine id (CORPORATION_LEVEL = the depositing corp's
	// id -- CvCorporationInfo::mapFrom; the count core reads countCorporationLevels(id)). The token spelling
	// STAYS as perType -- it names WHAT is counted; the id names WHOSE.
	void resolvePerToken(const char* szToken, int iId);

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
	// THE AUDIENCE AXIS (json §3.9 `ai`): the default read is HUMAN-audience -- aiOnly entries fold into a
	// SEPARATE slot table and never leak into it. The CvModAudience overload is THE spelled-out read (HUMAN /
	// AI_ONLY / INCLUSIVE -- the ai leaf is directly askable, never derived as inclusive − human); the bool
	// form (bIncludeAiOnly: false = HUMAN, true = INCLUSIVE) is the transitional delegate the pre-enum call
	// sites still use -- they move to the enum with their next touch (reported follow-up).
	int sum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, CvModAudience eAudience) const;
	int sum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly = false) const;
	// The per-property point read (the open MODFAM_PROPERTY plane is keyed by the property's FK id).
	int propertySum(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit, CvModAudience eAudience) const;
	// THE KEYED READ ([modifier.md §5]): what this source deposits onto each NAMED target of a keyed axis
	// (happiness.<scope>.buildings.{B}, experience.city.unitCombats.{UC}, <yield>.<scope>.improvements.{I}).
	// ⛔ A keyed entry deliberately does NOT fold into the scope's point slot -- folding it scope-wide is
	// silently, plausibly WRONG (a melee-only experience bonus handed to every unit trained here), so this is
	// an ENTRY-LIST read by design. iTargetSeg is the AXIS token (`buildings` / `improvements` / ...): without
	// it the read mixes axes, since every named target resolves a targetFk. Cheap: it walks the handful this entity AUTHORED, never a keyed container
	// the info no longer holds. Fills a CALLER-OWNED array of (targetFk, ×100 value), unconditioned entries
	// only -- the same audience + condition semantics as the point sum above.
	void targetedSums(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
		int iTargetSeg, std::vector<std::pair<int, int> >& kOut, CvModAudience eAudience) const;
	void targetedSums(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
		int iTargetSeg, std::vector<std::pair<int, int> >& kOut, bool bIncludeAiOnly = false) const;
	// The point form of the same read: the sum this source deposits onto ONE named target.
	int targetedSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
		int iTargetSeg, int iTargetFk, CvModAudience eAudience) const;
	int targetedSum(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit,
		int iTargetSeg, int iTargetFk, bool bIncludeAiOnly = false) const;
	int propertySum(int iPropertyFk, CvCascScope eScope, CvCascUnit eUnit, bool bIncludeAiOnly = false) const;

private:
	// The load-time recursive family walk (strings live only here): a unit key ends the address (a §3.9 leaf);
	// a bare-number/array non-unit key is the count-by-type leaf; a BOOL value is the §8 keyed-container
	// MEMBERSHIP flag (combat targets/unitTargets/defenders {UNITCOMBAT_X: true}) -- compiled as a targeted
	// COUNT entry only under a recognized keyed-container token; a node-level §3.9 qualifier key (`unit:` /
	// `religion:` / a non-object `max:` / `orderedBy(Descending):`) is gathered into ModNodeQuals -- shorthand
	// applying to every entry of the node's leaves that carries none of its own; an object-valued `ai` key is
	// the §3.9 AUDIENCE HOP (recursed with bAiOnly=true, NEVER pushed as an address segment -- the entries
	// below it compile with the aiOnly flag and the member path kind-resolves cleanly); any other key recurses
	// one segment deeper.
	void walk(std::vector<std::string>& segments, const picojson::value& node, bool bAiOnly);
	void parseLeaf(const std::vector<std::string>& segments, const picojson::value& leaf, CvCascUnit eUnit,
	               const ModNodeQuals& nodeQuals, bool bAiOnly);
	void compileEntry(CvModEntry* pEntry, const std::vector<std::string>& segments);
	void foldPointEntry(const CvModEntry* pEntry);   // route a point-foldable entry into its audience's slot table
	// THE ONE DERIVATION at compile end: clears + re-derives the slot tables FROM the retained entry list
	// (fold every point-foldable entry, sort), then rebuilds the conditioned per-family view. Idempotent.
	void finalizeCompiled();

	std::vector<CvModEntry*> m_entries;                  // owned
	std::vector<const CvModEntry*> m_conditioned;        // borrowed views into m_entries, sorted by family
	std::vector<std::pair<int, int> > m_slots;           // packed slot key -> unconditioned ×100 sum, sorted (HUMAN audience)
	std::vector<std::pair<int, int> > m_propertySlots;   // packed (propertyFk, scope, unit) -> ×100 sum, sorted (HUMAN audience)
	std::vector<std::pair<int, int> > m_slotsAiOnly;           // the aiOnly twin of m_slots (the §3.9 `ai` audience)
	std::vector<std::pair<int, int> > m_propertySlotsAiOnly;   // the aiOnly twin of m_propertySlots

	CvModifiers(const CvModifiers&);              // noncopyable -- owns the entries
	CvModifiers& operator=(const CvModifiers&);
};

#endif // CV_MODIFIERS_H
