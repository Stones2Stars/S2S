#pragma once
#ifndef CV_JSON_UNITCOMBAT_INFO_H
#define CV_JSON_UNITCOMBAT_INFO_H

//
//	CvJsonUnitCombatInfo -- the per-type cascade info for UNITCOMBATS. Extension: the unit `skills` this combat class
//	confers (json.md §8; e.g. healsAs / defenders / rBombardDirect — the unit-combat-scoped abilities). Like a
//	promotion, a unitcombat is a grantor of unit skills; the unit's ACTIVE set folds them in on the instance later.
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonUnitCombatInfo : public CvJsonInfo
{
public:
	std::set<std::string> skills;   // the `skills:{name:true}` block this unit-combat class confers
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_UNITCOMBAT_INFO_H
