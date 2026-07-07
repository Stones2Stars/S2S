#pragma once
#ifndef CV_JSON_BONUSCLASS_INFO_H
#define CV_JSON_BONUSCLASS_INFO_H

//
//	CvJsonBonusClassInfo -- the JSON poco for BONUS CLASSES (the resource grouping axis; uniformity ruling: every
//	info type has its own CvJson<X>Info home, even when empty). A bonus class is a pure structural axis (the
//	bonus->class categorization lives on the BONUS, identity.bonusClassType); it composes no section units today.
//	Type + description are served by the CvJsonInfo base; this is where any future class-level typed member would land.
//

#include "CvJsonInfo.h"

class CvJsonBonusClassInfo : public CvJsonInfo
{
public:
	CvJsonBonusClassInfo();
};

#endif // CV_JSON_BONUSCLASS_INFO_H
