#pragma once
#ifndef CV_JSON_PROMOTION_INFO_H
#define CV_JSON_PROMOTION_INFO_H

//
//	CvJsonPromotionInfo -- the per-type cascade info for PROMOTIONS. Extension: the unit `skills` this promotion grants
//	(json.md §8; the mutable, promotion-grantable abilities -- blitz / amphib / …). A promotion is a grantor of unit
//	skills (a unit's ACTIVE skill set is its type's base skills + the skills of its held promotions, resolved on the
//	unit INSTANCE later -- this static info is just the definition of what THIS promotion contributes).
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonPromotionInfo : public CvJsonInfo
{
public:
	std::set<std::string> skills;   // the `skills:{name:true}` block this promotion grants
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_PROMOTION_INFO_H
