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
	// The GROUP cap (json §4.4: the member authors identity.specialBuildingType, the GROUP holds allowed:{empire:N}).
	// Reads the COMPOSED allowed unit -- the CvBuildingInfo shape -- so the cap has ONE representation: the enabler's
	// group gate gets it off getAllowed(), this getter off the same map. A hand-parsed private int instead left
	// getAllowed() NULL, which silently disabled the group gate (every member offered at once).
	int getMaxPlayerInstances() const { return m_allowed.cap("empire"); }

	bool isValid() const { return m_bValid; }

	virtual const CvJsonAllowed* getAllowed() const { return &m_allowed; }
	virtual CvJsonAllowed*       mutAllowed()       { return &m_allowed; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	TechTypes m_iObsoleteTech;
	TechTypes m_iTechPrereq;
	int m_iTechPrereqAnyone;

	CvJsonAllowed m_allowed;   // §4.4 -- the composed section unit (parsed by the base dispatch)

	bool m_bValid;
};

#endif // CV_SPECIAL_BUILDING_INFO_H
