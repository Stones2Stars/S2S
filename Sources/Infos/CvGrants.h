#pragma once
#ifndef CV_GRANTS_H
#define CV_GRANTS_H

//
//	CvGrants -- the §5 PAYLOAD VOCABULARY: what an action hands over. Shapes: id-list buckets (plain strings or
//	the conditioned `{<kind>: ID, enabled: <cond>}` object form), numeric pulses, scoped pulses, and bool flags.
//
//	⚖ It is a PAYLOAD, never a plane of its own. A grant is a TRIGGER WITH A NULL CONDITION (json.md §5), so the
//	`grants` authoring block compiles into the entity's ONE `CvTriggers` entry list -- as a single entry whose
//	happening is the implicit considered action (construct/research/adopt/found), with no condition and no roll.
//	This class is therefore reached in exactly TWO ways, both of them a trigger entry's `grant`: that folded
//	considered-action entry (CvTriggers::consideredGrant), and a `triggers` entry's explicit `action.grant`.
//	⛔ There is no `getGrants()` section and no per-info `m_grants` member -- the split is about AUTHORING, not
//	about two runtime mechanisms, and giving the payload its own section is what made it look like two.
//	WRITE-ONCE AT LOAD. Owns its entries/conditions.
//
//	Keys: the grants key axis (buckets / pulse channels / scopes / flags) is OPEN in the data (json.md §5's
//	`grants.<channel>: value`, plus authored buckets beyond the named lists -- greatPeople, the starting*
//	channels, the `ai` sibling), so it is interned through a LOAD-MINTED key table (CvGrants::key -- the
//	ClassificationRegistry mechanic, LOCAL to grants): the authored strings exist on the parse surface only;
//	every runtime read is int-keyed ([DEC-materialize-at-mapfrom]).
//
//	Scale: numeric pulse values are ×100 at parse (the one human->fixed-point boundary); readers take
//	pulse()/100 for the human count -- the shape the grants machine's ÷100 reads were written against.
//

#include "CvCondition.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace picojson { class value; }

class CvGrants
{
public:
	CvGrants() {}
	~CvGrants();

	// The LOCAL load-minted key intern (authored string -> stable int id, minted on first ask -- the parse or
	// a consumer's one-time handle mint). Ids are run-local: never serialized, never compared across runs.
	static int key(const char* szKey);
	static int findKey(const std::string& szKey);   // const lookup; -1 = never minted (=> absent everywhere)

	// The unit's single load-time writer: parse the whole `grants` object (every §5 shape; no key is "unknown").
	void parse(const picojson::value& v);

	// --- runtime readers (const-only, int-keyed off CvGrants::key handles; the grants machine's query surface) ---
	const std::vector<int>* list(int iBucketKey) const
	{
		std::map<int, std::vector<int> >::const_iterator bucketIt = m_lists.find(iBucketKey);
		return (bucketIt != m_lists.end()) ? &bucketIt->second : NULL;
	}
	int firstListId(int iBucketKey) const
	{
		const std::vector<int>* pList = list(iBucketKey);
		return (pList != NULL && !pList->empty()) ? (*pList)[0] : -1;
	}
	int pulse(int iChannelKey) const
	{
		std::map<int, int>::const_iterator channelIt = m_pulses.find(iChannelKey);
		return (channelIt != m_pulses.end()) ? channelIt->second : 0;
	}
	int scopedPulse(int iChannelKey, int iScopeKey) const;
	int scopedPulseSumAllScopes(int iChannelKey) const;   // one channel summed over its scopes
	bool flag(int iFlagKey) const { return m_flags.count(iFlagKey) != 0; }
	int pulseCount() const { return (int)m_pulses.size(); }            // census read
	void clearParsed();   // frees the owned entries + resets (the dtor body; the clear-first half of the section re-map)

	const std::map<int, std::vector<int> >& lists() const { return m_lists; }
	// The per-entry condition for bucket[i] (NULL = unconditional). Index-parallel to list(bucket).
	const CvCondition* listCond(int iBucketKey, size_t i) const
	{
		std::map<int, std::vector<CvCondition*> >::const_iterator bucketIt = m_listConds.find(iBucketKey);
		if (bucketIt == m_listConds.end() || i >= bucketIt->second.size())
		{
			return NULL;
		}
		return bucketIt->second[i];
	}

	// --- PARSE-SURFACE readers (LOAD-ONLY): the mapFrom materializations speak the authored vocabulary once
	// per load ("the parse surface alone touches strings"). A runtime read takes the int-keyed surface above
	// with a minted handle -- never these. ---
	const std::vector<int>* list(const std::string& szBucket) const { return list(findKey(szBucket)); }
	int firstListId(const std::string& szBucket) const { return firstListId(findKey(szBucket)); }
	int pulse(const std::string& szChannel) const { return pulse(findKey(szChannel)); }
	int scopedPulse(const std::string& szChannel, const std::string& szScope) const
	{
		return scopedPulse(findKey(szChannel), findKey(szScope));
	}

	bool isEmpty() const
	{
		return m_lists.empty() && m_pulses.empty() && m_scopedPulses.empty() && m_flags.empty();
	}

private:
	std::map<int, std::vector<int> > m_lists;              // bucket key -> FK ids (techs/units/buildings/…)
	// A list entry may be the CONDITIONED object form ({building: X, enabled: <cond>}, json §3.9 -- every grant
	// entry takes `enabled`). Its id still lands in m_lists (so every count/consumer keeps working); the condition
	// rides here, keyed by bucket, parallel to the ids. Empty = the plain always-on string form.
	std::map<int, std::vector<CvCondition*> > m_listConds;
	std::map<int, int> m_pulses;                        // channel key -> value ×100 (population/revolution/…)
	std::map<int, std::map<int, int> > m_scopedPulses;  // channel key -> {scope key -> value ×100}
	std::set<int> m_flags;                                 // bool flags ("goldenAge")

	CvGrants(const CvGrants&);              // noncopyable -- owns its entries
	CvGrants& operator=(const CvGrants&);
};

#endif // CV_GRANTS_H
