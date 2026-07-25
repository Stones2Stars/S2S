//
//	CvGrants -- see the header. The json.md §5 grants shape, grounded in the curated data:
//	  lists      -- "techs":["TECH_X"] / single-id "shrine"-style strings append to their bucket; a list entry
//	                may be the conditioned object form ({"building": "BUILDING_PALACE", "enabled": <cond>})
//	  pulses     -- "population": 1 / "revolution": -100 (×100 at parse; readers ÷100)
//	  scoped     -- "population": {"city": 3} (channel -> {scope: value×100})
//	  flags      -- "goldenAge": true
//	No §5 key shape is "unknown" -- an unrecognized entry key surfaces via the unconsumed census, never silently drops.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvGrants.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE condition boundary
#include "CvJsonParse.h"            // jsonResolveId / jsonX100

CvGrants::~CvGrants()
{
	clearParsed();
}

void CvGrants::clearParsed()
{
	for (std::map<std::string, std::vector<CvCondition*> >::iterator ci = m_listConds.begin(); ci != m_listConds.end(); ++ci)
		for (size_t i = 0; i < ci->second.size(); ++i) delete ci->second[i];
	m_lists.clear();
	m_listConds.clear();
	m_pulses100.clear();
	m_scopedPulses100.clear();
	m_flags.clear();
}

int CvGrants::scopedPulseSumAllScopes100(const std::string& szChannel) const
{
	std::map<std::string, std::map<std::string, int> >::const_iterator it = m_scopedPulses100.find(szChannel);
	if (it == m_scopedPulses100.end()) return 0;
	int iSum = 0;
	for (std::map<std::string, int>::const_iterator sc = it->second.begin(); sc != it->second.end(); ++sc) iSum += sc->second;
	return iSum;
}

int CvGrants::scopedPulse100(const std::string& szChannel, const std::string& szScope) const
{
	std::map<std::string, std::map<std::string, int> >::const_iterator it = m_scopedPulses100.find(szChannel);
	if (it == m_scopedPulses100.end()) return 0;
	std::map<std::string, int>::const_iterator sc = it->second.find(szScope);
	return (sc != it->second.end()) ? sc->second : 0;
}

void CvGrants::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		const picojson::value& val = it->second;
		if (val.is<picojson::array>())
		{
			const picojson::array& a = val.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (a[i].is<std::string>())
				{
					const int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0) { m_lists[k].push_back(id); m_listConds[k].push_back(NULL); }
				}
				else if (a[i].is<picojson::object>())
				{
					// The CONDITIONED entry form ({<kind>: ID, enabled: <cond>}, json §3.9). Skipping it -- which a
					// string-only reader does SILENTLY -- drops the grant entirely, so a targeted provision reaches
					// nobody. The id goes into m_lists as usual; the condition rides index-parallel.
					const picojson::object& e = a[i].get<picojson::object>();
					int id = -1;
					CvCondition* pCond = NULL;
					for (picojson::object::const_iterator eit = e.begin(); eit != e.end(); ++eit)
					{
						if (eit->first == "enabled") { delete pCond; pCond = cascadeParseCondition(eit->second); }
						else if (eit->second.is<std::string>() && id < 0) id = jsonResolveId(eit->second.get<std::string>());
					}
					if (id >= 0) { m_lists[k].push_back(id); m_listConds[k].push_back(pCond); }
					else delete pCond;
				}
			}
		}
		else if (val.is<double>())      m_pulses100[k] = jsonX100(val.get<double>());
		else if (val.is<std::string>()) { const int id = jsonResolveId(val.get<std::string>()); if (id >= 0) m_lists[k].push_back(id); }
		else if (val.is<bool>())        { if (val.get<bool>()) m_flags.insert(k); }
		else if (val.is<picojson::object>())   // scoped pulse: "population": {"city": 3}
		{
			const picojson::object& so = val.get<picojson::object>();
			for (picojson::object::const_iterator sc = so.begin(); sc != so.end(); ++sc)
				if (sc->second.is<double>()) m_scopedPulses100[k][sc->first] = jsonX100(sc->second.get<double>());
		}
	}
}
