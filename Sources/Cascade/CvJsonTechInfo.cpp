//
//	CvJsonTechInfo::mapFrom -- common sections (base) + the empire `capabilities` block (§8). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonTechInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonTechInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);    // the common cascade sections first
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("capabilities");
	if (it != o.end()) cascadeJsonBoolSet(it->second, capabilities);
}
