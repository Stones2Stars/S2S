//
//	CvJsonTraitInfo::mapFrom -- common sections (base) + the `identity.negativeTrait` flag + the `policies` (pure empire
//	states) this trait grants (§9). Inherited by CvJsonSimpleTraitInfo / CvJsonComplexTraitInfo. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonTraitInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonTraitInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	if ((it = o.find("identity")) != o.end() && it->second.is<picojson::object>())
	{
		const picojson::object& io = it->second.get<picojson::object>();
		picojson::object::const_iterator ng = io.find("negativeTrait");
		if (ng != io.end() && ng->second.is<bool>()) negativeTrait = ng->second.get<bool>();
	}
	if ((it = o.find("policies")) != o.end()) cascadeJsonBoolSet(it->second, policies);
}
