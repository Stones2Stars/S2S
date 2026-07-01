//
//	CvJsonUnitInfo::mapFrom -- common sections (base) + the unit §8 blocks (`skills`, `tags`) + the top-level SpawnOnly /
//	UnlimitedException flags. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonUnitInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet

void CvJsonUnitInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	if ((it = o.find("skills")) != o.end()) cascadeJsonBoolSet(it->second, skills);
	if ((it = o.find("tags")) != o.end())   cascadeJsonBoolSet(it->second, tags);
	if ((it = o.find("spawnOnly")) != o.end() && it->second.is<bool>())          spawnOnly = it->second.get<bool>();
	if ((it = o.find("unlimitedException")) != o.end() && it->second.is<bool>()) unlimitedException = it->second.get<bool>();
}
