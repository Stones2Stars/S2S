//
//	CvJsonEdges -- see the header. A faithful relocation of the former base walk_edge (json §4.1/§4.2).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvJsonEdges.h"
#include "CvJsonParse.h"     // jsonResolveId

void CvJsonEdges::parse(const std::string& szEdge, const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (!it->second.is<picojson::array>()) continue;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>())
			{
				const int id = jsonResolveId(a[i].get<std::string>());
				if (id >= 0) m_edges[szEdge + "." + it->first].push_back(id);
			}
	}
}
