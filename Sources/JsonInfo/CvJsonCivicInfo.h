#pragma once
#ifndef CV_JSON_CIVIC_INFO_H
#define CV_JSON_CIVIC_INFO_H

//
//	CvJsonCivicInfo -- the per-type cascade info for CIVICS. Extension: the `policies` this civic ENACTS (json.md §9) --
//	pure empire STATES (noForeignTrade / noCorporations / fixedBorders / …), active while the civic is adopted. `policies`
//	is ONE meaning with two grantors (a civic enacts them, a trait grants them permanently) -- so CvJsonTraitInfo carries
//	the SAME set. A policy is a pure state, NEVER a parameterized/targeted rule (that is an enabler `requires` concern).
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonCivicInfo : public CvJsonInfo
{
public:
	std::set<std::string> policies;   // the `policies:{name:true}` block -- the pure empire states this civic enacts
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_CIVIC_INFO_H
