#pragma once
#ifndef CV_EDGES_H
#define CV_EDGES_H

//
//	CvEdges -- the `enables`-family edges as ONE composable unit (json.md par.4.1/par.4.2): the source-side
//	enables/obsoletes/replaces/disables buckets + the target-side obsoletedBy, each a per-kind FK id list.
//	Composed BY VALUE on the derived infos that author any edge (the data-grounded table). WRITE-ONCE AT LOAD.
//
//	INTERNED STORAGE (the DepositIndex discipline): the edge/bucket vocabulary is the SPEC'S FIXED SET
//	(json.md par.4.1 + the enabler.md par.2 reverse-map buckets), so both axes are ENUMS -- the JSON strings are
//	interned ONCE in the load-time parse(); every runtime read is an int-keyed lookup. No string survives into
//	the query surface (scope-packages GENERIC-CODE-STATIC-STORAGE; the earlier stringly dotted-key map was the
//	JSON shape leaking into runtime).
//

#include <string>
#include <vector>
#include <algorithm>
#include <map>

namespace picojson { class value; }

// The edge FAMILIES: the AUTHORED set (json.md par.4.1/par.4.2 -- the source-side four + the target-side
// obsoletedBy) plus the LOAD-DERIVED reverse families (never authored in JSON; populated onto the referenced
// info by the readJson reverse-view pass, so every info ALREADY CARRIES its reverse lookups after load --
// modifier.md par.1: the reverse view is derived once at load, never on the hot path; no consumer may build
// its own scan or side index):
//   EDGEF_RELATED     -- display axis (the pedia/tooltip): entities of a kind that REFERENCE this info in any
//                        relation (prereq, obsolete, tech-keyed value tables, secondary gates). A candidate
//                        SUPERSET -- consumers keep their exact predicates over it. ⛔ Display-only: the
//                        enabler's GENERATE/GATE never reads it.
//   EDGEF_REQUIRED_BY -- the gate axis (the enabler's requires-reverse-index, enabler.md par.7.1 step 2):
//                        entities whose `requires` (build + operate + dormant triggers) reference this info as
//                        an atom, POPULATED by the readJson reverse pass. The gate stage re-gates exactly
//                        these dependents on this info's HAVE-event -- never a database sweep, and never a
//                        bespoke index inside an enabler.
//   EDGEF_ENABLED_BY  -- the UNLOCK axis: entities that name this info in their own `enables`. It exists
//                        because enabling is the one relation with NO target-side authoring: a building
//                        declares its own `obsoletedBy` and its own `replacedBy`, so those reverses come off
//                        the target's own data, but nothing authors "what unlocks me" -- only the SOURCE says
//                        `enables.buildings`. Without this family the target's sole recourse is the MERGED
//                        EDGEF_RELATED bucket, which cannot tell an unlocking tech from an obsoleting one or
//                        from one that merely deposits onto it. Populated by the same reverse pass.
//   EDGEF_MEMBERS     -- the MEMBERSHIP axis: entities that BELONG TO this info, where belonging is a plain FK
//                        the member carries and the group never lists (a unit names its combat classes; a
//                        combat class names no units). ⛔ It cannot ride EDGEF_RELATED, and that is the whole
//                        reason it exists: a unit also names a combat class to deposit a vs-modifier onto it,
//                        so the RELATED superset mixes members with every unit that merely has a bonus
//                        AGAINST them -- an answer that is wrong rather than merely wide. Populated by the
//                        same reverse pass; a group's members are never re-derived by sweeping the members.
enum EnEdgeFamily
{
	EDGEF_ENABLES = 0,
	EDGEF_OBSOLETES,
	EDGEF_REPLACES,
	EDGEF_DISABLES,
	EDGEF_OBSOLETED_BY,
	EDGEF_RELATED,
	EDGEF_REQUIRED_BY,
	EDGEF_ENABLED_BY,
	EDGEF_MEMBERS,
	NUM_EDGEF
};

// The edge BUCKETS (json.md par.4.1's per-kind list + the enabler.md par.2 reverse-map buckets
// traitsAnd/traitsOr/routesAnd). Grounded exhaustively against the authored data.
enum EnEdgeBucket
{
	EDGEB_BUILDINGS = 0,
	EDGEB_UNITS,
	EDGEB_BUILDS,
	EDGEB_TECHS,
	EDGEB_CIVICS,
	EDGEB_RELIGIONS,
	EDGEB_CORPORATIONS,
	EDGEB_PROJECTS,
	EDGEB_PROCESSES,
	EDGEB_PROMOTIONS,
	EDGEB_PROMOTION_LINES,
	EDGEB_HERITAGES,
	EDGEB_SPECIAL_BUILDINGS,
	EDGEB_SPECIAL_BUILDINGS_WAIVED,
	EDGEB_IMPROVEMENTS,
	EDGEB_BONUSES,
	EDGEB_ROUTES,
	EDGEB_ROUTES_AND,
	EDGEB_VOTES,
	EDGEB_HURRIES,
	EDGEB_TRAITS,
	EDGEB_TRAITS_AND,
	EDGEB_TRAITS_OR,
	EDGEB_SPECIALISTS,
	// ⚑ DERIVED-ONLY, and the one bucket that is not an `enables` kind ([json.md] par.4.1). A leaderhead holds
	// its traits as a plain FK list and a trait never names a leader, so "which leaders hold this" exists only
	// as the load-built inverse -- which is what this bucket carries. No authored block fills it.
	EDGEB_LEADERS,
	NUM_EDGEB,
	NO_EDGEB = -1
};

class CvEdges
{
public:
	CvEdges() {}

	// The unit's single load-time writer: parse ONE edge section ("enables"/"obsoletes"/"replaces"/"disables"/
	// "obsoletedBy") -- a per-kind bucket object {bucket:[INFOTYPE_ids]} -- FK-resolving each id and INTERNING
	// the family + bucket strings to the enums above. An unknown bucket key fails LOUD (assert) and is skipped.
	void parse(const std::string& szEdge, const picojson::value& v);

	// The runtime read: the id list at (family, bucket), or NULL if not authored. Int-keyed -- no strings.
	const std::vector<int>* find(EnEdgeFamily eFamily, EnEdgeBucket eBucket) const
	{
		std::map<short, std::vector<int> >::const_iterator it = m_edges.find(key(eFamily, eBucket));
		return (it != m_edges.end()) ? &it->second : NULL;
	}
	// Membership in one edge list -- "does this source ENABLE that id" ([json.md] §4.1). The list is the
	// authored handful, so this is the read that replaces a per-id bool table over a whole registry.
	bool has(EnEdgeFamily eFamily, EnEdgeBucket eBucket, int iId) const
	{
		const std::vector<int>* pList = find(eFamily, eBucket);
		if (pList == NULL)
		{
			return false;
		}
		return std::find(pList->begin(), pList->end(), iId) != pList->end();
	}
	int count() const { return (int)m_edges.size(); }   // the readJson census
	bool isEmpty() const { return m_edges.empty(); }

	// The reverse-view writers -- LOAD-ONLY (the readJson reverse pass; part of the write-once-at-load window).
	void add(EnEdgeFamily eFamily, EnEdgeBucket eBucket, int iId) { m_edges[key(eFamily, eBucket)].push_back(iId); }
	void sortUnique();                                  // dedup every list after the reverse build
	void clearParsed() { m_edges.clear(); }             // the clear-first half of the full-registry section re-map

	// The load-time intern tables (also serve any render/pedia reverse need).
	static EnEdgeFamily familyFromString(const std::string& szFamily);   // NUM_EDGEF = unknown
	static EnEdgeBucket bucketFromString(const std::string& szBucket);   // NO_EDGEB = unknown
	static const char* bucketName(EnEdgeBucket eBucket);
	static const char* familyName(EnEdgeFamily eFamily);

private:
	static short key(EnEdgeFamily eFamily, EnEdgeBucket eBucket) { return (short)(((int)eFamily << 5) | (int)eBucket); }

	std::map<short, std::vector<int> > m_edges;         // (family,bucket) -> FK ids
	CvEdges(const CvEdges&);                    // noncopyable (held by-value on the noncopyable info)
	CvEdges& operator=(const CvEdges&);
};

#endif // CV_EDGES_H
