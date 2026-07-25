//
//	CvJsonAllowed -- see the header. A faithful relocation of the former base walk_allowed (json §4.4).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvJsonAllowed.h"

void CvJsonAllowed::parse(const picojson::value& v)
{
	if (!v.is<picojson::object>()) return;
	const picojson::object& o = v.get<picojson::object>();
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		if (it->second.is<double>()) m_caps[it->first] = (int)it->second.get<double>();
}
