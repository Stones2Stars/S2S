#pragma once
#ifndef CV_JSON_CIVIC_OPTION_INFO_H
#define CV_JSON_CIVIC_OPTION_INFO_H

//
//	CvCivicOptionInfo -- the JSON poco for CIVIC OPTIONS (the civic category/slot axis: Government, Economy, …).
//	A civic option is a PURE STRUCTURAL AXIS: it carries no modifier families, no enables, no prereqs -- only its type
//	+ description (served by the CvInfo base). The civic->option categorization lives on the CIVIC
//	(getCivicOptionType), not here. The subclass exists for UNIFORMITY -- every cascade info type has its own
//	CvJson<X>Info home, and this is where any future option-level typed member would land. No cascade here.
//
//	Live callers (verified 2026-07-07): base text only (getTextKeyWide/getDescription) -- e.g. CvGameTextMgr,
//	CvPlayer, CvDLLWidgetData. No typed value getters (the legacy isPolicy / traitNoUpkeep are dead).
//

#include "CvInfo.h"

class CvCivicOptionInfo : public CvInfo
{
public:
	CvCivicOptionInfo() {}
	virtual void mapFrom(const picojson::value& entity);   // base core reading + availability only (no own typed members yet)
};

#endif // CV_JSON_CIVIC_OPTION_INFO_H
