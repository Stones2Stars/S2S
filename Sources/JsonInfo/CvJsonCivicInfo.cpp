//
//	CvJsonCivicInfo::mapFrom -- common sections (base) + the `policies` (pure empire states) this civic enacts (§9). See header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonCivicInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonCivicInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("policies");
	if (it != o.end()) cascadeJsonBoolSet(it->second, policies);
}
