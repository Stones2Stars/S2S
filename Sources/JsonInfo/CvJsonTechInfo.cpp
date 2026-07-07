//
//	CvJsonTechInfo::mapFrom -- common sections (base) + the empire-ability blocks (§8): `capabilities` plus the
//	`canTrade` / `canTradeOn` / `canWorkOn` siblings (capabilities.md). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonTechInfo.h"
#include "CvCascadeJsonParse.h"     // cascadeJsonBoolSet + cascadeJsonResolveId

void CvJsonTechInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);    // the common cascade sections first
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it = o.find("capabilities");
	if (it != o.end()) cascadeJsonBoolSet(it->second, capabilities);
	if ((it = o.find("canTrade")) != o.end()) cascadeJsonBoolSet(it->second, canTrade);
	if ((it = o.find("canWorkOn")) != o.end()) cascadeJsonBoolSet(it->second, canWorkOn);
	// canTradeOn: { terrains: [TERRAIN_..] } -- real FK refs, resolved to ids (capabilities.md: the tradable-terrain
	// set is pure data; the consumer asks generic set-membership, new tradable terrains need zero code).
	if ((it = o.find("canTradeOn")) != o.end() && it->second.is<picojson::object>())
	{
		const picojson::object& cto = it->second.get<picojson::object>();
		picojson::object::const_iterator tt = cto.find("terrains");
		if (tt != cto.end() && tt->second.is<picojson::array>())
		{
			const picojson::array& a = tt->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					const int id = cascadeJsonResolveId(a[i].get<std::string>());
					if (id >= 0) canTradeOnTerrains.insert(id);
				}
		}
	}
}
