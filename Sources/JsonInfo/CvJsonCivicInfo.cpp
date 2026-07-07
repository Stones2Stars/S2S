//
//	CvJsonCivicInfo::mapFrom -- the base dispatch fills every composed unit (edges / grants / modifiers / the §9
//	`policies` bool block); no civic-only typed members today. See header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonCivicInfo.h"

void CvJsonCivicInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units
}
