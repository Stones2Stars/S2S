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
	// spawnOnly lives in `identity` (StoneBase SpawnOnly = IdentityFlag(d.Identity,"spawnOnly")); unlimitedException lives
	// in `skills` (StoneBase UnlimitedException = IdentityFlag(d.Skills,"unlimitedException")) -- NOT top-level (a bare
	// o.find() found NEITHER: verified 525 spawnOnly:true all in identity, 26 unlimitedException:true all in skills).
	picojson::object::const_iterator id = o.find("identity");
	if (id != o.end() && id->second.is<picojson::object>())
	{
		const picojson::object& io = id->second.get<picojson::object>();
		picojson::object::const_iterator sit = io.find("spawnOnly");
		if (sit != io.end() && sit->second.is<bool>()) spawnOnly = sit->second.get<bool>();
	}
	picojson::object::const_iterator sk = o.find("skills");
	if (sk != o.end() && sk->second.is<picojson::object>())
	{
		const picojson::object& so = sk->second.get<picojson::object>();
		picojson::object::const_iterator uit = so.find("unlimitedException");
		if (uit != so.end() && uit->second.is<bool>()) unlimitedException = uit->second.get<bool>();
	}
}
