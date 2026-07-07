//
//	CvJsonUnitInfo::mapFrom -- common sections (base dispatch fills the composed units, incl. the §8 `skills`/`tags`
//	bool blocks) + the top-level `builds` repertoire + the SpawnOnly / UnlimitedException flags. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonUnitInfo.h"
#include "CvJsonParse.h"            // jsonResolveId

void CvJsonUnitInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;
	// top-level `builds` (the unit type's build REPERTOIRE): a flat array of BUILD_* strings -> resolved ids. The base
	// classifier tags this CJK_INTRINSIC (skips it), so the unit subclass owns the parse. (NOT `enables.builds`.)
	if ((it = o.find("builds")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<std::string>()) continue;
			const int bid = jsonResolveId(a[i].get<std::string>());
			if (bid >= 0) builds.push_back(bid);
		}
	}
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
