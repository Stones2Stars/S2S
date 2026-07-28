#pragma once
#ifndef CV_CLASSIFICATION_BLOCK_H
#define CV_CLASSIFICATION_BLOCK_H

//
//	CvClassificationBlock -- the shared `{name:true}` boolean-flag block as ONE composable unit: the §8/§9 classification
//	sections (unit `skills`/`tags`, building `attributes`, tech `capabilities`, civic/trait `policies`) all share
//	this exact shape, so ONE class serves all five -- instantiated per section on the derived infos that author it.
//	KEYED skill extras (terrainDoubleMove:{TERRAIN_X:true}, targets:[…]) are NOT this block -- they stay typed
//	members on the owning subclass. WRITE-ONCE AT LOAD.
//
//	TWO planes per entry name: TRUE (grant) and FALSE (revoke -- the skills.md §4 grant/revoke pairs; a promotion
//	authoring `stampede: false` REVOKES the ability). Non-bool values are ignored.
//
//	THE ID PLANE (the ClassificationRegistry integration): each block name resolves to its domain's generated
//	classification id (SKILL_*/TAG_*/ATTRIBUTE_*/CAPABILITY_*/POLICY_* -- runtime-minted infos, one per distinct
//	authored key), stored as a by-id bitset so the getter surface is an O(1) bit test -- NEVER a per-call string
//	lookup (the materialize-at-mapFrom ruling; the pre-resolve load window falls back to the string set so early
//	consumers stay correct).
//

#include <string>
#include <set>
#include <vector>

namespace picojson { class value; }

// The five §8/§9 classification domains -- one generated-info category each (ClassificationRegistry mints the
// SKILL_/TAG_/ATTRIBUTE_/CAPABILITY_/POLICY_ infos from the union of authored keys).
enum ClsDomain
{
	CLSD_SKILL = 0,     // unit `skills`
	CLSD_TAG,           // unit `tags`
	CLSD_ATTRIBUTE,     // building `attributes`
	CLSD_CHARACTERISTIC,// plot substrate `characteristics` (terrain/feature/improvement/route)
	CLSD_CAPABILITY,    // empire `capabilities`
	CLSD_POLICY,        // empire `policies`
	NUM_CLS_DOMAINS
};

// The getter-side O(1) classification read: memoized generated-id + bit test. Usage (a one-line getter body):
//   bool isBlitz() const CLS_HAS(m_skills, CLSD_SKILL, "blitz")
// The function-local static is per-TU under unity inlining (each copy resolves to the same id -- harmless).
#define CLS_HAS(block, domain, key) { static int s_clsId = -1; return (block).hasKey(s_clsId, domain, key); }
#define CLS_HAS_FALSE(block, domain, key) { static int s_clsId = -1; return (block).hasFalseKey(s_clsId, domain, key); }
#define CLS_COUNT(block, domain, key) { static int s_clsId = -1; return (block).countKey(s_clsId, domain, key); }

class CvClassificationBlock
{
public:
	CvClassificationBlock() {}

	// The unit's single load-time writer: parse the section's {name:bool} object (true -> grant plane, false ->
	// revoke plane; non-bool ignored).
	void parse(const picojson::value& v);

	bool has(const char* szName) const { return m_names.count(szName) != 0; }
	bool has(const std::string& szName) const { return m_names.count(szName) != 0; }
	bool hasFalse(const char* szName) const { return m_falseNames.count(szName) != 0; }
	bool hasFalse(const std::string& szName) const { return m_falseNames.count(szName) != 0; }
	const std::set<std::string>& all() const { return m_names; }
	const std::set<std::string>& allFalse() const { return m_falseNames; }
	bool isEmpty() const { return m_names.empty() && m_falseNames.empty(); }
	void clearParsed()   // the clear-first half of the full-registry section re-map
	{ m_names.clear(); m_falseNames.clear(); m_byId.clear(); m_falseById.clear(); }

	// ---- the generated-id plane (ClassificationRegistry) ----
	// Fill the by-id bitsets from the name sets via the domain's key->id registry; sized to the domain count.
	// Called by CvInfo::resolveClassificationIds after the registry mint pass (LOAD-ONLY).
	void resolveIds(int eDomain);
	// Fold another block's GRANT plane into this one, BY ID -- a load-time derivation that takes on another
	// entity's classification (a unit taking on its combat classes' tags). It must be the id plane, not the
	// names: readJson mints + resolves every block BEFORE the reverse pass runs, so by derivation time the name
	// sets are already spent and a name added there would never reach a bitset. The revoke plane is deliberately
	// NOT merged -- a tag is pure membership and nothing revokes one ([tags.md]).
	void mergeGrantedIds(const CvClassificationBlock& kOther);
	bool hasId(int iId) const { return iId >= 0 && iId < (int)m_byId.size() && m_byId[iId] != 0; }
	bool hasFalseId(int iId) const { return iId >= 0 && iId < (int)m_falseById.size() && m_falseById[iId] != 0; }
	// The getter read behind CLS_HAS: O(1) once resolved; the pre-resolve load window reads the string set.
	bool hasKey(int& iIdCache, int eDomain, const char* szKey) const;
	bool hasFalseKey(int& iIdCache, int eDomain, const char* szKey) const;
	// The tri-state count read behind CLS_COUNT (skills.md §4 grant/revoke, sign-collapsed count-abilities):
	// +1 = granted, -1 = revoked, 0 = absent. One cached id serves both planes.
	int countKey(int& iIdCache, int eDomain, const char* szKey) const;

private:
	std::set<std::string> m_names;        // TRUE-valued (grant)
	std::set<std::string> m_falseNames;   // FALSE-valued (revoke)
	std::vector<char> m_byId;             // [generated id] -> carried (grant plane)
	std::vector<char> m_falseById;        // [generated id] -> carried (revoke plane)
	CvClassificationBlock(const CvClassificationBlock&);              // noncopyable (held by-value on the noncopyable info)
	CvClassificationBlock& operator=(const CvClassificationBlock&);
};

// The unit-plane derived move-through-plots verdict (the ONE implementation, DEC-single-implementation --
// promotion + unitcombat both materialize it at mapFrom over their skill block + par.8 keyed doubleMove FK
// lists; string-plane skill reads are LOAD-TIME ONLY, the poco getters stay bare member reads): does holding
// this data change which plots the holder can move through?
bool deriveChangesMoveThroughPlots(const CvClassificationBlock& skills,
	const std::vector<int>& terrainDoubleMoves,
	const std::vector<int>& featureDoubleMoves);

#endif // CV_CLASSIFICATION_BLOCK_H
