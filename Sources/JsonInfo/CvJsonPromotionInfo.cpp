//
//	CvJsonPromotionInfo::mapFrom -- the base dispatch fills every composed unit (modifiers / the §8 `skills` bool
//	block / the entity-level gate); no promotion-only typed members today. See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonPromotionInfo.h"

void CvJsonPromotionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units
}
