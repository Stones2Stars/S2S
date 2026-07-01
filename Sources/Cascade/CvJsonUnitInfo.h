#pragma once
#ifndef CV_JSON_UNIT_INFO_H
#define CV_JSON_UNIT_INFO_H

//
//	CvJsonUnitInfo -- the per-type cascade info for UNITS. Base: modifier families / requires / allowed / obsoletedBy /
//	replacedBy. Extensions: the top-level SpawnOnly / UnlimitedException flags the enabler reads, + the two §8
//	classification blocks a UNIT owns -- `skills` (mutable, promotion-grantable abilities) and `tags` (immutable,
//	type-derived membership, accounting-only). These are the unit TYPE's definition; a unit INSTANCE's ACTIVE skill set
//	(type-base skills + its promotions' + unit-combat's) is resolved on the instance later (out of this static pass).
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonUnitInfo : public CvJsonInfo
{
public:
	CvJsonUnitInfo() : spawnOnly(false), unlimitedException(false) {}
	bool spawnOnly, unlimitedException;
	std::set<std::string> skills;   // §8 mutable abilities (blitz/amphib/…) -- the unit type's base skill set
	std::set<std::string> tags;     // §8 immutable type membership (military/gunpowder/…) -- accounting, no behaviour
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_UNIT_INFO_H
