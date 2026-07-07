#pragma once
#ifndef CV_JSON_SPECIALBUILDING_INFO_H
#define CV_JSON_SPECIALBUILDING_INFO_H

//
//	CvJsonSpecialBuildingInfo -- the JSON poco for SPECIAL BUILDINGS (the building group/slot axis; uniformity
//	ruling: every info type has its own CvJson<X>Info home). Composes the section unit the special-building data
//	authors: `allowed` (the per-player group instance caps). Everything else is served by the CvJsonInfo base; no
//	typed members yet (the base dispatch covers the composed section -- no mapFrom override).
//

#include "CvJsonInfo.h"

class CvJsonSpecialBuildingInfo : public CvJsonInfo
{
public:
	CvJsonSpecialBuildingInfo();

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonAllowed* getAllowed() const { return &m_allowed; }

protected:
	virtual CvJsonAllowed* mutAllowed() { return &m_allowed; }

private:
	CvJsonAllowed m_allowed;
};

#endif // CV_JSON_SPECIALBUILDING_INFO_H
