//
//	CvJsonPromotionLineInfo::mapFrom -- base (availability: tech enables.promotionLines; the game-option gates ride
//	the composed entity-level `enabled`/`disabled` gate), then the build-up flag + the parked not-on-domain list.
//	The member promotions are a runtime reverse index (not JSON). See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonPromotionLineInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- DOMAIN_ FKs

bool CvJsonPromotionLineInfo::isNotOnDomainType(int iDomain) const
{
	for (size_t i = 0; i < m_aeNotOnDomains.size(); ++i)
		if (m_aeNotOnDomains[i] == iDomain) return true;
	return false;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }

void CvJsonPromotionLineInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (tech enables.promotionLines; the entity-level gate)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* bu = child_obj(o, "buildUp"))
		m_bBuildUp = id_bool(*bu, "active");

	if (const picojson::object* io = child_obj(o, "identity"))
	{
		picojson::object::const_iterator nd = io->find("notOnDomains");
		if (nd != io->end() && nd->second.is<picojson::array>())
		{
			const picojson::array& a = nd->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeNotOnDomains.push_back(id); }
		}
	}
}
