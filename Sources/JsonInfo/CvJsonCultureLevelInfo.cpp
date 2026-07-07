//
//	CvJsonCultureLevelInfo::mapFrom -- base (availability: the entity-level `enabled`/`disabled` gate + the
//	replacedBy edge + the `allowed` wonder caps, all composed units the base dispatch fills), then the tier's city
//	defense + radius + culture threshold. HUMAN-native. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonCultureLevelInfo.h"

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int fam_member_val(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);   if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope);  if (!so) return 0;
	const picojson::object* mo = child_obj(*so, member); if (!mo) return 0;
	picojson::object::const_iterator u = mo->find(unit);
	return (u != mo->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
static int id_int(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }

void CvJsonCultureLevelInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (the entity-level gate, replacedBy edge, the allowed caps)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_iCityDefenseModifier = fam_member_val(o, "defense", "city", "amount", "percent");

	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iCityRadius = id_int(*io, "cityRadius");
		// cultureThreshold: a bare scalar, OR (if a game speed breaks the geometric ratio) a {base, overrides} object --
		// read the base; the per-speed overrides are ⏳ deferred (the consumer derives per-speed by ×gamespeed%).
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
