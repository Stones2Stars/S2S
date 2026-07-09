//
//	CvJsonCultureLevelInfo::mapFrom -- base (availability: the entity-level `enabled`/`disabled` gate + the
//	replacedBy edge + the `allowed` wonder caps, all composed units the base dispatch fills), then the tier's city
//	defense + radius + culture threshold. HUMAN-native. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonCultureLevelInfo.h"
#include "CvJsonParse.h"          // the shared walkers (jsonChildObj/jsonFamMemberVal/jsonIdInt)

void CvJsonCultureLevelInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (the entity-level gate, replacedBy edge, the allowed caps)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_iCityDefenseModifier = jsonFamMemberVal(o, "defense", "city", "amount", "percent");

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iCityRadius = jsonIdInt(*io, "cityRadius");
		// cultureThreshold: a bare scalar, OR (if a game speed breaks the geometric ratio) a {base, overrides} object --
		// read the base; the per-speed overrides are STUB deferred (the consumer derives per-speed by ×gamespeed%).
		picojson::object::const_iterator ct = io->find("cultureThreshold");
		if (ct != io->end())
		{
			if (ct->second.is<double>()) m_iCultureThreshold = (int)ct->second.get<double>();
			else if (ct->second.is<picojson::object>())
			{
				const picojson::object& cto = ct->second.get<picojson::object>();
				picojson::object::const_iterator b = cto.find("base");
				if (b != cto.end() && b->second.is<double>()) m_iCultureThreshold = (int)b->second.get<double>();
			}
		}
	}
}
