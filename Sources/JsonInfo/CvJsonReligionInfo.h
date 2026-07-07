#pragma once
#ifndef CV_JSON_RELIGION_INFO_H
#define CV_JSON_RELIGION_INFO_H

//
//	CvJsonReligionInfo -- the per-type cascade info for RELIGIONS (ports StoneBase's ReligionInfo: modifier families on the
//	base) + the `shrine` block ({channel:value}), which the modifier multiplies by the world religion-levels count.
//

#include "CvJsonInfo.h"

class CvJsonReligionInfo : public CvJsonInfo
{
public:
	std::map<std::string, int> shrineCommerce;           // shrine {channel:value}
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_RELIGION_INFO_H
