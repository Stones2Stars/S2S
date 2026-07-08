//
//	CvJsonPromotionLineInfo::mapFrom -- base (availability: tech enables.promotionLines; the game-option gates ride
//	the composed entity-level `enabled`/`disabled` gate), then the build-up flag + the parked not-on-domain list.
//	The member promotions are a runtime reverse index (not JSON). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonPromotionLineInfo.h"
#include "CvJsonParse.h"          // jsonResolveId (DOMAIN_ FKs) + the shared walkers (jsonChildObj/jsonIdBool)

bool CvJsonPromotionLineInfo::isNotOnDomainType(int iDomain) const
{
	for (size_t i = 0; i < m_aeNotOnDomains.size(); ++i)
		if (m_aeNotOnDomains[i] == iDomain) return true;
	return false;
}

void CvJsonPromotionLineInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (tech enables.promotionLines; the entity-level gate)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* bu = jsonChildObj(o, "buildUp"))
		m_bBuildUp = jsonIdBool(*bu, "active");

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		picojson::object::const_iterator nd = io->find("notOnDomains");
		if (nd != io->end() && nd->second.is<picojson::array>())
		{
			const picojson::array& a = nd->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeNotOnDomains.push_back(id); }
		}
	}
}
