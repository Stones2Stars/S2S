//
//	CvJsonGrants -- see the header. The full json.md §5 grants shape, grounded in the curated data:
//	  lists      -- "techs":["TECH_X"] / single-id "shrine"-style strings append to their bucket
//	  pulses     -- "population": 1 / "revolution": -100 (×100 at parse; readers ÷100)
//	  scoped     -- "population": {"city": 3} (channel -> {scope: value×100})
//	  flags      -- "goldenAge": true
//	  found      -- "foundBuildings": [{"building": B, "enabled"?: <condition>}]
//	  repeatable -- [{"unit": U, "interval": "perTurn", "chance": {"per": PROPERTY_X}}]
//	                [{"unitCombat": UC, "interval": "perTurn", "heal": 5}]
//	                [{"heal": "full", "count": 1, "interval": "perTurn"}]
//	                [{"PROPERTY_X": -3, "interval": "perTurn", "on": "plot", "relation": "near", "distance": 1}]
//	No §5 key shape is "unknown" -- an unrecognized entry key surfaces via the unconsumed census, never silently drops.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonGrants.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the ONE condition boundary
#include "CvJsonModEntry.h"         // jsonParsePer -- the ONE §3.7 `per` parser (chance.per)
#include "CvJsonParse.h"            // jsonResolveId / jsonX100 / jsonNoteUnconsumed

CvJsonGrants::~CvJsonGrants()
{
	clearParsed();
}

void CvJsonGrants::clearParsed()
{
	for (size_t i = 0; i < m_foundBuildings.size(); ++i) delete m_foundBuildings[i];
	for (size_t i = 0; i < m_repeatables.size(); ++i) delete m_repeatables[i];
	for (std::map<std::string, std::vector<CvJsonCondition*> >::iterator ci = m_listConds.begin(); ci != m_listConds.end(); ++ci)
		for (size_t i = 0; i < ci->second.size(); ++i) delete ci->second[i];
	m_foundBuildings.clear();
	m_repeatables.clear();
	m_lists.clear();
	m_listConds.clear();
	m_pulses100.clear();
	m_scopedPulses100.clear();
	m_flags.clear();
}

int CvJsonGrants::scopedPulseSumAllScopes100(const std::string& szChannel) const
{
	std::map<std::string, std::map<std::string, int> >::const_iterator it = m_scopedPulses100.find(szChannel);
	if (it == m_scopedPulses100.end()) return 0;
	int iSum = 0;
	for (std::map<std::string, int>::const_iterator sc = it->second.begin(); sc != it->second.end(); ++sc) iSum += sc->second;
	return iSum;
}

int CvJsonGrants::scopedPulse100(const std::string& szChannel, const std::string& szScope) const
{
	std::map<std::string, std::map<std::string, int> >::const_iterator it = m_scopedPulses100.find(szChannel);
	if (it == m_scopedPulses100.end()) return 0;
	std::map<std::string, int>::const_iterator sc = it->second.find(szScope);
	return (sc != it->second.end()) ? sc->second : 0;
}

// "interval": bare "perTurn" = every turn (1); {"perTurn": N} = every N turns (json §3.8).
static int grants_interval(const picojson::value& v)
{
	if (v.is<std::string>()) return 1;
	if (v.is<picojson::object>())
	{
		const picojson::object& o = v.get<picojson::object>();
		picojson::object::const_iterator it = o.find("perTurn");
		if (it != o.end() && it->second.is<double>()) return (int)it->second.get<double>();
	}
	return 1;
}

void CvJsonGrants::parseRepeatable(const picojson::value& v)
{
	if (!v.is<picojson::array>()) return;
	const picojson::array& a = v.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		CvJsonGrantRepeatable* r = new CvJsonGrantRepeatable();
		for (picojson::object::const_iterator it = e.begin(); it != e.end(); ++it)
		{
			const std::string& k = it->first;
			const picojson::value& val = it->second;
			if (k == "unit" && val.is<std::string>())            r->unitId = jsonResolveId(val.get<std::string>());
			else if (k == "unitCombat" && val.is<std::string>()) r->unitCombatId = jsonResolveId(val.get<std::string>());
			else if (k == "heal")
			{
				if (val.is<std::string>() && val.get<std::string>() == "full") r->healFull = true;
				else if (val.is<double>()) r->heal100 = jsonX100(val.get<double>());
			}
			else if (k == "count" && val.is<double>())    r->count = (int)val.get<double>();
			else if (k == "interval")                     r->intervalPerTurn = grants_interval(val);
			else if (k == "chance")
			{
				if (val.is<double>()) r->chanceValue100 = jsonX100(val.get<double>());
				else if (val.is<picojson::object>())
				{
					const picojson::object& co = val.get<picojson::object>();
					picojson::object::const_iterator per = co.find("per");
					if (per != co.end())
					{
						// the ONE §3.7 `per` parser (shared with the modifier entries) -- anyOf no longer drops
						CvJsonModEntry perEntry;
						jsonParsePer(&perEntry, per->second);
						r->chancePerId    = perEntry.perTypeId;
						if (perEntry.perTypeId < 0) r->chancePerToken = perEntry.perType;   // a catch-all token survives (carry-only)
						r->chancePerEach  = perEntry.perEach;
						r->chancePerScope = perEntry.perScope;
						r->chancePerAnyOf = perEntry.perAnyOf;
					}
				}
			}
			else if (k == "on" && val.is<std::string>())        r->on = val.get<std::string>();
			else if (k == "relation" && val.is<std::string>())  r->relation = val.get<std::string>();
			else if (k == "distance" && val.is<double>())       r->distance = (int)val.get<double>();
			else if (k == "enabled")                            { delete r->enabled; r->enabled = cascadeParseCondition(val); }
			else if (k.compare(0, 9, "PROPERTY_") == 0 && val.is<double>())   // property pulse payload
			{
				r->propertyId = jsonResolveId(k);
				r->amount100 = jsonX100(val.get<double>());
			}
			else jsonNoteUnconsumed("grants.repeatable", k);   // unrecognized entry key -> the unconsumed census, never silent
		}
		m_repeatables.push_back(r);
	}
}

void CvJsonGrants::parseFoundBuildings(const picojson::value& v)
{
	if (!v.is<picojson::array>()) return;
	const picojson::array& a = v.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		CvJsonFoundBuilding* f = new CvJsonFoundBuilding();
		picojson::object::const_iterator b = e.find("building");
		if (b != e.end() && b->second.is<std::string>()) f->building = jsonResolveId(b->second.get<std::string>());
		picojson::object::const_iterator en = e.find("enabled");
		if (en != e.end()) f->enabled = cascadeParseCondition(en->second);
		if (f->building >= 0) m_foundBuildings.push_back(f); else delete f;
	}
}

void CvJsonGrants::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		const std::string& k = it->first;
		const picojson::value& val = it->second;
		if (k == "repeatable")          parseRepeatable(val);
		else if (k == "foundBuildings") parseFoundBuildings(val);
		else if (val.is<picojson::array>())
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
					CvJsonCondition* pCond = NULL;
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
