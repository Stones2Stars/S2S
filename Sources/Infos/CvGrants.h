#pragma once
#ifndef CV_GRANTS_H
#define CV_GRANTS_H

//
//	CvGrants -- the `grants` section as ONE composable unit (json.md §5): the PURE PAYLOAD handed out on the
//	source's CONSIDERED ACTION (construct/research/adopt/found/mission -- implicit, no trigger field, no odds).
//	Shapes: id-list buckets (plain strings or the conditioned `{<kind>: ID, enabled: <cond>}` object form),
//	numeric pulses, scoped pulses, and bool flags. Anything recurring / chance-rolled / happening-fired is a
//	`triggers` entry (CvTriggers), never a grant. Composed BY VALUE on the derived infos that author grants
//	(the data-grounded table), and nested whole as a trigger action's `grant` payload.
//	WRITE-ONCE AT LOAD. Owns its entries/conditions.
//
//	Scale: numeric pulse values are ×100 at parse (the one human->fixed-point boundary); readers take
//	pulse100()/100 for the human count -- the shape the grants machine's ÷100 reads were written against.
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

	// The unit's single load-time writer: parse the whole `grants` object (every §5 shape; no key is "unknown").
	void parse(const picojson::value& v);

	// --- readers (const-only; the grants machine's query surface) ---
	const std::vector<int>* list(const std::string& szBucket) const
	{
		std::map<std::string, std::vector<int> >::const_iterator it = m_lists.find(szBucket);
		return (it != m_lists.end()) ? &it->second : NULL;
	}
	int firstListId(const std::string& szBucket) const
	{
		const std::vector<int>* l = list(szBucket);
		return (l != NULL && !l->empty()) ? (*l)[0] : -1;
	}
	int pulse100(const std::string& szChannel) const
	{
		std::map<std::string, int>::const_iterator it = m_pulses100.find(szChannel);
		return (it != m_pulses100.end()) ? it->second : 0;
	}
	int scopedPulse100(const std::string& szChannel, const std::string& szScope) const;
	int scopedPulseSumAllScopes100(const std::string& szChannel) const;   // one channel summed over its scopes
	bool flag(const std::string& szName) const { return m_flags.count(szName) != 0; }
	int pulseCount() const { return (int)m_pulses100.size(); }            // census read
	void clearParsed();   // frees the owned entries + resets (the dtor body; the clear-first half of the section re-map)

	const std::map<std::string, std::vector<int> >& lists() const { return m_lists; }
	// The per-entry condition for bucket[i] (NULL = unconditional). Index-parallel to list(bucket).
	const CvCondition* listCond(const std::string& szBucket, size_t i) const
	{
		std::map<std::string, std::vector<CvCondition*> >::const_iterator it = m_listConds.find(szBucket);
		if (it == m_listConds.end() || i >= it->second.size()) return NULL;
		return it->second[i];
	}

	bool isEmpty() const
	{
		return m_lists.empty() && m_pulses100.empty() && m_scopedPulses100.empty() && m_flags.empty();
	}

private:
	std::map<std::string, std::vector<int> > m_lists;              // bucket -> FK ids (techs/units/buildings/…)
	// A list entry may be the CONDITIONED object form ({building: X, enabled: <cond>}, json §3.9 -- every grant
	// entry takes `enabled`). Its id still lands in m_lists (so every count/consumer keeps working); the condition
	// rides here, keyed by bucket, parallel to the ids. Empty = the plain always-on string form.
	std::map<std::string, std::vector<CvCondition*> > m_listConds;
	std::map<std::string, int> m_pulses100;                        // channel -> value ×100 (population/revolution/…)
	std::map<std::string, std::map<std::string, int> > m_scopedPulses100;   // channel -> {scope -> value ×100}
	std::set<std::string> m_flags;                                 // bool flags ("goldenAge")

	CvGrants(const CvGrants&);              // noncopyable -- owns its entries
	CvGrants& operator=(const CvGrants&);
};

#endif // CV_GRANTS_H
