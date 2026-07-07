#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvJsonBuildingInfo -- the per-type cascade info for BUILDINGS (ports StoneBase's BuildingInfo). The base holds the
//	modifier families / requires / allowed / edges; this adds the typed flags + the curator `identity` block, SELF-CONTAINED
//	(the engine getGlobalReligionCommerce / getReligionType / getGlobalCorporationCommerce / getStateReligionCommerce /
//	getCommerceChangeDoubleTime reads are RETIRED). shrine/corpHQ/religion are FK ids (-1 none); the commerce blocks are
//	{channel:value} maps.
//

#include "CvJsonInfo.h"

class CvJsonBuildingInfo : public CvJsonInfo
{
public:
	CvJsonBuildingInfo() : notConstructible(false), governmentCenter(false), forceNoPrereqScaling(false),
		shrineReligion(-1), corpHQ(-1), religion(-1) {}
	bool notConstructible, governmentCenter, forceNoPrereqScaling;   // notConstructible/forceNoPrereqScaling <- identity; governmentCenter <- `attributes` (IS_GOVERNMENT_CENTER)
	std::string specialBuildingType;
	int shrineReligion;                                  // top-level `shrine` -> religion FK
	int corpHQ;                                          // top-level `headquarters` -> corporation FK
	int religion;                                        // identity.religion -> religion FK (state-religion match)
	std::map<std::string, int> stateReligionCommerce;    // identity.stateReligionCommerce {channel:value}
	std::map<std::string, int> commerceDoubleTime;       // identity.commerceDoubleTime {channel:years}
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_BUILDING_INFO_H
