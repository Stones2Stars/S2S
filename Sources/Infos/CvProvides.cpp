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
	{
		// a bare "BONUS_X" -> count INFERRED 1 (the common case, all count-1 providers)
		if (a[i].is<std::string>())
		{
			const int id = jsonResolveId(a[i].get<std::string>());
			if (id >= 0) bonuses.push_back(id);
		}
		// a single-key object { "BONUS_X": N } -> an explicit supply count (HOLLYWOOD's 6 movies, json §5a)
		else if (a[i].is<picojson::object>())
		{
			const picojson::object& e = a[i].get<picojson::object>();
			for (picojson::object::const_iterator ei = e.begin(); ei != e.end(); ++ei)
			{
				const int id = jsonResolveId(ei->first);
				if (id < 0) continue;
				bonuses.push_back(id);
				const int n = ei->second.is<double>() ? (int)ei->second.get<double>() : 1;
				if (n != 1) bonusCount[id] = n;
			}
		}
	}
}
