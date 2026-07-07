//
//	CvJsonTraitInfo::mapFrom -- the base dispatch fills the composed units (edges / grants / modifiers / the §9
//	`policies` bool block), then the `identity.negativeTrait` flag. Inherited by CvJsonSimpleTraitInfo /
//	CvJsonComplexTraitInfo. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonTraitInfo.h"

void CvJsonTraitInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("identity");
	if (it != o.end() && it->second.is<picojson::object>())
	{
		const picojson::object& io = it->second.get<picojson::object>();
		picojson::object::const_iterator ng = io.find("negativeTrait");
		if (ng != io.end() && ng->second.is<bool>()) negativeTrait = ng->second.get<bool>();
	}
}
