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
//          stores it there (store.py), not on the special building.
//          getObsoleteTech reads the `obsoletedBy.techs` EDGE (the CvBuildingInfo shape, off the
//          base dispatch). The group's obsoleting tech is ALSO inherited onto its member buildings
//          by the curator (store._inherit_group_obsoletes), and THAT is what retires them in
//          CvTeam::setHasTech; this group-level read serves the consumers that ask the GROUP --
//          the pedia's "Obsolete with <tech>" line (CvGameTextMgr) and the EDGEB_SPECIAL_BUILDINGS
//          reverse edge. getTechPrereqAnyone stays NO_TECH (no authoring exists in the XML).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvSpecialBuildingInfo : public CvInfo
{
	//---------------------------PUBLIC INTERFACE---------------------------------
public:

	CvSpecialBuildingInfo();

	TechTypes getObsoleteTech() const   // obsoletedBy.techs -- the same edge read as CvBuildingInfo
	{ const std::vector<int>* v = edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS); return (TechTypes)((v != NULL && !v->empty()) ? (*v)[0] : NO_TECH); }
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

	// §4.1/§4.2 edge storage -- without it the base dispatch has nowhere to put an authored
	// `obsoletedBy`/`enables` and routes the key to jsonNoteUnconsumed instead.
	virtual const CvJsonEdges*   getEdges() const   { return &m_edges; }
	virtual CvJsonEdges*         mutEdges()         { return &m_edges; }

	virtual void mapFrom(const picojson::value& entity);

	//----------------------PROTECTED MEMBER VARIABLES----------------------------
protected:

	TechTypes m_iTechPrereq;
	int m_iTechPrereqAnyone;

	CvJsonAllowed m_allowed;   // §4.4 -- the composed section unit (parsed by the base dispatch)
	CvJsonEdges   m_edges;     // §4.1/§4.2 -- ditto

	bool m_bValid;
};

#endif // CV_SPECIAL_BUILDING_INFO_H
