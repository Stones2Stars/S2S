#pragma once

#ifndef CV_SPECIAL_BUILDING_INFO_H
#define CV_SPECIAL_BUILDING_INFO_H

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  class : CvSpecialBuildingInfo
//
//  DESC:   A special-building class (cathedral / monastery / corporation / ...).
//          #430: JSON-fed (Assets/Data/specialbuildings/*.json via mapFrom); no XML read.
//          getTechPrereq is RECONSTRUCTED at load from the tech-side inversion
//          (tech.enables.specialBuildings; cascadeLoadJson) via setTechPrereq -- the curator
//          stores it there (store.py), not on the special building. getObsoleteTech /
//          getTechPrereqAnyone stay NO_TECH by design (verified unused across all groups).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvSpecialBuildingInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvSpecialBuildingInfo();

	TechTypes getObsoleteTech() const { return m_iObsoleteTech; }
	TechTypes getTechPrereq() const { return m_iTechPrereq; }
	void setTechPrereq(TechTypes eTech) { m_iTechPrereq = eTech; }   // #430: un-inversion from tech.enables.specialBuildings (cascadeLoadJson, LOAD-ONLY)
	int getTechPrereqAnyone() const { return m_iTechPrereqAnyone; }
	int getMaxPlayerInstances() const { return m_iMaxPlayerInstances; }

	bool isValid() const { return m_bValid; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	TechTypes m_iObsoleteTech;
	TechTypes m_iTechPrereq;
	int m_iTechPrereqAnyone;
	int m_iMaxPlayerInstances;

	bool m_bValid;
};

#endif // CV_SPECIAL_BUILDING_INFO_H
