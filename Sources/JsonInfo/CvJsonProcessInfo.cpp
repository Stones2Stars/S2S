//
//	CvJsonProcessInfo::mapFrom -- base core reading + availability (the tech prereq rides the base as tech.enables.
//	processes; the only-latest supersession as this process's obsoletedBy.techs), then the per-commerce production->
//	commerce conversion percents. Natural human %, NO ×100. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonProcessInfo.h"

CvJsonProcessInfo::CvJsonProcessInfo()
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiProductionToCommerce[i] = 0;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}

void CvJsonProcessInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (tech enables.processes / obsoletedBy.techs)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// split-commerce production->commerce conversion (the process's whole point) -- natural %, NOT ×100
	m_aiProductionToCommerce[COMMERCE_GOLD]      = fam_val(o, "gold", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_RESEARCH]  = fam_val(o, "research", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_CULTURE]   = fam_val(o, "culture", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_ESPIONAGE] = fam_val(o, "espionage", "city", "percent");
}
