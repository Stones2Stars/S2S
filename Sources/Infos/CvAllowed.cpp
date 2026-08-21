//
//	CvAllowed -- see the header. The §4.4 key axis interns to EnAllowedCap at parse (the one place the authored
//	strings are touched); an authored key outside the closed vocabulary is dropped LOUDLY (assert), never
//	stored -- the Assets/Data census carries zero such keys.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvAllowed.h"
#include <string>

CvAllowed::CvAllowed()
{
	clearParsed();
}

void CvAllowed::clearParsed()
{
	for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
	{
		m_aiCaps[iKind] = -1;
	}
}

int CvAllowed::authoredCount() const
{
	int iCount = 0;
	for (int iKind = 0; iKind < NUM_ALLOWEDCAP; ++iKind)
	{
		if (m_aiCaps[iKind] >= 0)
		{
			++iCount;
		}
	}
	return iCount;
}

// The parse-surface intern: authored key -> EnAllowedCap (the closed §4.4 vocabulary; -1 = outside it).
static int allowedCapKind(const std::string& szKey)
{
	if (szKey == "world")
	{
		return ALLOWEDCAP_WORLD;
	}
	if (szKey == "team")
	{
		return ALLOWEDCAP_TEAM;
	}
	if (szKey == "empire")
	{
		return ALLOWEDCAP_EMPIRE;
	}
	if (szKey == "worldWonders")
	{
		return ALLOWEDCAP_WORLD_WONDERS;
	}
	if (szKey == "teamWonders")
	{
		return ALLOWEDCAP_TEAM_WONDERS;
	}
	if (szKey == "nationalWonders")
	{
		return ALLOWEDCAP_NATIONAL_WONDERS;
	}
	if (szKey == "totalWonders")
	{
		return ALLOWEDCAP_TOTAL_WONDERS;
	}
	return -1;
}

void CvAllowed::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>())
	{
		return;
	}
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator entryIt = o.begin(); entryIt != o.end(); ++entryIt)
	{
		if (!entryIt->second.is<double>())
		{
			continue;
		}
		const int iKind = allowedCapKind(entryIt->first);
		if (iKind < 0)
		{
			FAssertMsg(false, "allowed cap key outside the closed json.md par.4.4 vocabulary");
			continue;
		}
		m_aiCaps[iKind] = (int)entryIt->second.get<double>();
	}
}
