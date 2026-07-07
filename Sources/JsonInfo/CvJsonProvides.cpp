//
//	CvJsonProvides -- see the header. The §5a continuous in-vicinity supply: {bonuses:[BONUS_ids]}.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvJsonProvides.h"
#include "CvJsonParse.h"     // jsonResolveId

void CvJsonProvides::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it = o.find("bonuses");
	if (it == o.end() || !it->second.is<picojson::array>()) return;
	const picojson::array& a = it->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<std::string>())
		{
			const int id = jsonResolveId(a[i].get<std::string>());
			if (id >= 0) bonuses.push_back(id);
		}
}
