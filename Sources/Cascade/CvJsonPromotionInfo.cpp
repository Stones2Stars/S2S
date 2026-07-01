//
//	CvJsonPromotionInfo::mapFrom -- common sections (base) + the unit `skills` this promotion grants (§8). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonPromotionInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonPromotionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("skills");
	if (it != o.end()) cascadeJsonBoolSet(it->second, skills);
}
