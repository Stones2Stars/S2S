//
//	CvGrants -- see the header. The json.md §5 grants shape, grounded in the curated data:
//	  lists      -- "techs":["TECH_X"] / single-id "shrine"-style strings append to their bucket; a list entry
//	                may be the conditioned object form ({"building": "BUILDING_PALACE", "enabled": <cond>})
//	  pulses     -- "population": 1 / "revolution": -100 (×100 at parse; readers ÷100)
//	  scoped     -- "population": {"city": 3} (channel -> {scope: value×100})
//	  flags      -- "goldenAge": true
//	No §5 KEY is ever "unknown" -- the key axis is open, so every authored key interns through the LOCAL
//	load-minted table (CvGrants::key) and storage + the runtime readers are int-keyed
//	([DEC-materialize-at-mapfrom]). What CAN be dropped is a VALUE: an id resolving to nothing, a malformed
//	array element, a conditioned entry naming nothing, a scope carrying a non-number. Every one of those goes
//	through the ONE census (jsonNoteUnconsumed) -- a grant is a trigger (json.md §5), so it answers to the same
//	contract the trigger side does: being fail-closed is right, being fail-closed AND SILENT is not.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvGrants.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE condition boundary
#include "CvJsonParse.h"            // jsonResolveId / jsonX100

// The ONE key table (function-local static: safe from any static-init order; single game thread).
static std::map<std::string, int>& grantKeyTable()
{
	static std::map<std::string, int> s_table;
	return s_table;
}

int CvGrants::key(const char* szKey)
{
	std::map<std::string, int>& table = grantKeyTable();
	const std::string keyString(szKey);
	std::map<std::string, int>::const_iterator keyIt = table.find(keyString);
	if (keyIt != table.end())
	{
		return keyIt->second;
	}
	const int iMinted = (int)table.size();
	table[keyString] = iMinted;
	return iMinted;
}

int CvGrants::findKey(const std::string& szKey)
{
	const std::map<std::string, int>& table = grantKeyTable();
	std::map<std::string, int>::const_iterator keyIt = table.find(szKey);
	return (keyIt != table.end()) ? keyIt->second : -1;
}

CvGrants::~CvGrants()
{
	clearParsed();
}

void CvGrants::clearParsed()
{
	for (std::map<int, std::vector<CvCondition*> >::iterator bucketIt = m_listConds.begin(); bucketIt != m_listConds.end(); ++bucketIt)
	{
		for (size_t i = 0; i < bucketIt->second.size(); ++i)
		{
			delete bucketIt->second[i];
		}
	}
	m_lists.clear();
	m_listConds.clear();
	m_pulses.clear();
	m_scopedPulses.clear();
	m_flags.clear();
}

int CvGrants::scopedPulseSumAllScopes(int iChannelKey) const
{
	std::map<int, std::map<int, int> >::const_iterator channelIt = m_scopedPulses.find(iChannelKey);
	if (channelIt == m_scopedPulses.end())
	{
		return 0;
	}
	int iSum = 0;
	for (std::map<int, int>::const_iterator scopeIt = channelIt->second.begin(); scopeIt != channelIt->second.end(); ++scopeIt)
	{
		iSum += scopeIt->second;
	}
	return iSum;
}

int CvGrants::scopedPulse(int iChannelKey, int iScopeKey) const
{
	std::map<int, std::map<int, int> >::const_iterator channelIt = m_scopedPulses.find(iChannelKey);
	if (channelIt == m_scopedPulses.end())
	{
		return 0;
	}
	std::map<int, int>::const_iterator scopeIt = channelIt->second.find(iScopeKey);
	return (scopeIt != channelIt->second.end()) ? scopeIt->second : 0;
}

void CvGrants::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>())
	{
		jsonNoteUnconsumed("grants", "sectionNotAnObject");   // a malformed section is DROPPED WHOLE -- say so
		return;
	}
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const int iKey = key(it->first.c_str());   // the one string->id boundary (parse-time intern)
		const std::string szBucket = "grants." + it->first;   // the census path for anything this key drops
		const picojson::value& val = it->second;
		if (val.is<picojson::array>())
		{
			const picojson::array& a = val.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (a[i].is<std::string>())
				{
					const int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0)
					{
						m_lists[iKey].push_back(id);
						m_listConds[iKey].push_back(NULL);
					}
					else
					{
						// An authored id that resolves to nothing: the grant is GONE. It has to announce rather
						// than vanish into a shorter list -- the same reason the trigger side announces it.
						jsonNoteUnconsumed(szBucket, a[i].get<std::string>());
					}
				}
				else if (a[i].is<picojson::object>())
				{
					// The CONDITIONED entry form ({<kind>: ID, enabled: <cond>}, json §3.9). Skipping it -- which a
					// string-only reader does SILENTLY -- drops the grant entirely, so a targeted provision reaches
					// nobody. The id goes into m_lists as usual; the condition rides index-parallel.
					const picojson::object& entryObj = a[i].get<picojson::object>();
					int id = -1;
					CvCondition* pCond = NULL;
					for (picojson::object::const_iterator entryIt = entryObj.begin(); entryIt != entryObj.end(); ++entryIt)
					{
						if (entryIt->first == "enabled")
						{
							delete pCond;
							pCond = cascadeParseCondition(entryIt->second);
						}
						else if (entryIt->second.is<std::string>() && id < 0)
						{
							id = jsonResolveId(entryIt->second.get<std::string>());
						}
					}
					if (id >= 0)
					{
						m_lists[iKey].push_back(id);
						m_listConds[iKey].push_back(pCond);
					}
					else
					{
						// A conditioned entry naming nothing resolvable -- dropped WITH its condition, so a
						// targeted provision reaches nobody. Never silent.
						jsonNoteUnconsumed(szBucket, "entryHasNoResolvableId");
						delete pCond;
					}
				}
				else
				{
					jsonNoteUnconsumed(szBucket, "entryNotAStringOrObject");
				}
			}
		}
		else if (val.is<double>())
		{
			m_pulses[iKey] = jsonX100(val.get<double>());
		}
		else if (val.is<std::string>())
		{
			const int id = jsonResolveId(val.get<std::string>());
			if (id >= 0)
			{
				m_lists[iKey].push_back(id);
			}
			else
			{
				jsonNoteUnconsumed(szBucket, val.get<std::string>());
			}
		}
		else if (val.is<bool>())
		{
			if (val.get<bool>())
			{
				m_flags.insert(iKey);
			}
		}
		else if (val.is<picojson::object>())   // scoped pulse: "population": {"city": 3}
		{
			const picojson::object& scopedObj = val.get<picojson::object>();
			for (picojson::object::const_iterator scopeIt = scopedObj.begin(); scopeIt != scopedObj.end(); ++scopeIt)
			{
				if (scopeIt->second.is<double>())
				{
					m_scopedPulses[iKey][key(scopeIt->first.c_str())] = jsonX100(scopeIt->second.get<double>());
				}
				else
				{
					jsonNoteUnconsumed(szBucket, scopeIt->first);   // a scope carrying a non-number is dropped
				}
			}
		}
	}
}
