//
//	CvJsonCivicOptionInfo::mapFrom -- a civic option is a pure structural axis (type + description on the base; no
//	families / enables / prereqs). This override exists for uniformity + as the home for any future option-level typed
//	member; today it is exactly the base read. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonCivicOptionInfo.h"

void CvJsonCivicOptionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading (type + description) + availability -- nothing else on a civic option
}
