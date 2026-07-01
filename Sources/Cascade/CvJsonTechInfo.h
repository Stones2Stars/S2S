#pragma once
#ifndef CV_JSON_TECH_INFO_H
#define CV_JSON_TECH_INFO_H

//
//	CvJsonTechInfo -- the per-type cascade info for TECHS. Extension over the base: the empire `capabilities` this tech
//	GRANTS when held (json.md §8; techTrading / foundOnPeaks / …). Techs are the ONLY grantor of capabilities (owner
//	2026-07-01), so the block lives HERE, not on the base. The empire's ACTIVE capability set is the union over the
//	team's held grantor techs -- derived live where consumed (the enabler's canFound/canBuild + the team-ability
//	systems), never stored on a team object.
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonTechInfo : public CvJsonInfo
{
public:
	std::set<std::string> capabilities;   // the `capabilities:{name:true}` block -- the granted capability names
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_TECH_INFO_H
