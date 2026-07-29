//
//	CvClassificationBlock -- see the header. One shape, five sections (skills/tags/attributes/capabilities/policies),
//	two planes (grant/revoke), plus the generated-id bitsets the getter surface reads O(1).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvClassificationBlock.h"
#include "CvClassificationRegistry.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition -- the §3.9 entry-form gate

void CvClassificationBlock::parse(const picojson::value& v)
{
	// ARRAY form (unit skills/tags after the curator restructure): a plain list of enabler names -- name present
	// == held (grant plane only; a pure-boolean list carries no revoke). Additive: the OBJECT form below is kept
	// verbatim for the sections that still author {name:bool} grant/revoke pairs (promotion skills, building
	// attributes, tech capabilities, empire policies).
	if (v.is<picojson::array>())
	{
		const picojson::array& a = v.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) m_names.insert(a[i].get<std::string>());
		return;
	}
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		// The §3.9 ENTRY form: an OBJECT value carries the grant's own gate ({"enabled": <condition>}), so a
		// grantor can confer a key only where the condition holds -- the capital-only `abolishedAnger` is the
		// live case. The key still joins the GRANT plane (it is held, conditionally); the condition is stored
		// beside it and evaluated by the HOLDER when it folds the grant in or out.
		if (it->second.is<picojson::object>())
		{
			m_names.insert(it->first);
			const picojson::object& e = it->second.get<picojson::object>();
			picojson::object::const_iterator en = e.find("enabled");
			if (en != e.end())
			{
				m_condByName[it->first] = cascadeParseCondition(en->second);
			}
			continue;
		}
		if (!it->second.is<bool>()) continue;
		if (it->second.get<bool>()) m_names.insert(it->first);
		else m_falseNames.insert(it->first);
	}
}

void CvClassificationBlock::resolveIds(int eDomain)
{
	const int n = ClassificationRegistry::count(eDomain);
	m_byId.assign(n, 0);
	m_falseById.assign(n, 0);
	m_condById.assign(n, (const CvCondition*)NULL);
	for (std::set<std::string>::const_iterator it = m_names.begin(); it != m_names.end(); ++it)
	{
		const int id = ClassificationRegistry::keyId(eDomain, *it);
		if (id >= 0 && id < n)
		{
			m_byId[id] = 1;
			// carry the entry-form gate onto the id plane the holder reads
			std::map<std::string, const CvCondition*>::const_iterator c = m_condByName.find(*it);
			if (c != m_condByName.end()) m_condById[id] = c->second;
		}
	}
	for (std::set<std::string>::const_iterator it = m_falseNames.begin(); it != m_falseNames.end(); ++it)
	{
		const int id = ClassificationRegistry::keyId(eDomain, *it);
		if (id >= 0 && id < n) m_falseById[id] = 1;
	}
}

void CvClassificationBlock::mergeGrantedIds(const CvClassificationBlock& kOther)
{
	// resolveIds sizes every block to the registry count, so these normally match; the grow is defensive for a
	// merge attempted before this block was resolved (it then simply takes the other's width).
	if (kOther.m_byId.size() > m_byId.size())
	{
		m_byId.resize(kOther.m_byId.size(), 0);
	}
	for (size_t i = 0; i < kOther.m_byId.size(); ++i)
	{
		if (kOther.m_byId[i] != 0)
		{
			m_byId[i] = 1;
		}
	}
}

bool CvClassificationBlock::hasKey(int& iIdCache, int eDomain, const char* szKey) const
{
	if (!m_byId.empty()) return hasId(ClassificationRegistry::cachedKeyId(iIdCache, eDomain, szKey));
	return !m_names.empty() && m_names.count(szKey) != 0;   // pre-resolve load window -- string fallback
}

bool CvClassificationBlock::hasFalseKey(int& iIdCache, int eDomain, const char* szKey) const
{
	if (!m_falseById.empty()) return hasFalseId(ClassificationRegistry::cachedKeyId(iIdCache, eDomain, szKey));
	return !m_falseNames.empty() && m_falseNames.count(szKey) != 0;
}

int CvClassificationBlock::countKey(int& iIdCache, int eDomain, const char* szKey) const
{
	if (!m_byId.empty() || !m_falseById.empty())
	{
		const int id = ClassificationRegistry::cachedKeyId(iIdCache, eDomain, szKey);
		return hasId(id) ? 1 : (hasFalseId(id) ? -1 : 0);
	}
	if (m_names.count(szKey) != 0) return 1;        // pre-resolve load window -- string fallback
	if (m_falseNames.count(szKey) != 0) return -1;
	return 0;
}

bool deriveChangesMoveThroughPlots(const CvClassificationBlock& skills,
	const std::vector<int>& terrainDoubleMoves,
	const std::vector<int>& featureDoubleMoves)
{
	return skills.has("amphib")
		|| skills.has("canPassPeaks")
		|| skills.has("canLeadThroughPeaks")
		|| skills.has("hillsDoubleMove")
		|| !terrainDoubleMoves.empty()
		|| !featureDoubleMoves.empty();
}
