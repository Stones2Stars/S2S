//
//	CvJsonReligionInfo::mapFrom -- common sections (base) + the top-level `shrine` block ({channel:value}), which the
//	modifier multiplies by the world religion-levels count. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonReligionInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonCommerceMap

void CvJsonReligionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("shrine");
	if (it != o.end()) cascadeJsonCommerceMap(it->second, shrineCommerce);
}
