//
//	CvJsonUnitCombatInfo::mapFrom -- the base dispatch fills every composed unit (modifiers / the §8 `skills` bool
//	block / the entity-level gate); no unitcombat-only typed members today. See header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonUnitCombatInfo.h"

void CvJsonUnitCombatInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + the section dispatch into the composed units
}
