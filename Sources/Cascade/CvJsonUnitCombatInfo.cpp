//
//	CvJsonUnitCombatInfo::mapFrom -- common sections (base) + the unit `skills` this combat class confers (§8). See header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonUnitCombatInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonUnitCombatInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("skills");
	if (it != o.end()) cascadeJsonBoolSet(it->second, skills);
}
