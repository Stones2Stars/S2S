#pragma once
#ifndef CV_SPECIAL_BUILDING_INFO_H
#define CV_SPECIAL_BUILDING_INFO_H

//
//	CvSpecialBuildingInfo -- the SPECIAL-BUILDING poco on the exemplar surface (patterns.md § THE GETTER
//	SETUP). A special-building GROUP (cathedral / monastery / ...): the member building authors
//	identity.specialBuildingType, the GROUP holds the cap (json.md §4.4 `allowed:{empire:N}`, riding the
//	composed `allowed` so the enabler's group gate and this read share ONE representation). getTechPrereq is
//	RECONSTRUCTED at load from the tech-side inversion (tech.enables.specialBuildings -- the readJson reverse
//	pass calls setTechPrereq); getObsoleteTech reads the `obsoletedBy.techs` edge off the base dispatch (the
//	group's obsoleting tech is ALSO curator-inherited onto its member buildings, which is what retires them --
//	this group-level read serves the consumers that ask the GROUP: the pedia line + the reverse edge).
//	No legacy getter name returns ([DEC-new-getter-surface]).
//

#include "CvInfo.h"

namespace picojson { class value; }

class CvSpecialBuildingInfo : public CvInfo
{
public:
	CvSpecialBuildingInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvAllowed* getAllowed() const { return &m_allowed; }   // §4.4 the group cap
	virtual const CvEdges*   getEdges()   const { return &m_edges; }     // §4.1/§4.2 (obsoletedBy.techs)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	bool isValid() const { return m_bValid; }   // identity.valid (default TRUE; a project unlock flips the false ones)
	// The GROUP cap (-1 = uncapped) -- materialized at mapFrom from the composed `allowed` unit
	// ([DEC-materialize-at-mapfrom]: this read sits under per-candidate hot loops), so the getter is a bare
	// member read; the enabler's group gate still reads the SAME composed unit via getAllowed() (ONE representation).
	int getMaxPlayerInstances() const { return m_iMaxPlayerInstances; }
	TechTypes getObsoleteTech() const   // obsoletedBy.techs -- the same edge read as CvBuildingInfo
	{
		const std::vector<int>* pTechs = edge(EDGEF_OBSOLETED_BY, EDGEB_TECHS);
		return (TechTypes)((pTechs != NULL && !pTechs->empty()) ? (*pTechs)[0] : NO_TECH);
	}
	// --- store-inverted tech FK (tech.enables.specialBuildings), reconstructed at LOAD by the readJson
	// reverse pass (CvReversePass), which calls the setter below. LOAD-ONLY writer. ---
	TechTypes getTechPrereq() const { return m_iTechPrereq; }
	void setTechPrereq(TechTypes eTech) { m_iTechPrereq = eTech; }

protected:
	virtual CvAllowed* mutAllowed() { return &m_allowed; }
	virtual CvEdges*   mutEdges()   { return &m_edges; }

private:
	// --- the composed section units ---
	CvAllowed m_allowed;
	CvEdges   m_edges;

	// --- the reverse-pass-fed FK + the intrinsic identity members (materialized once at mapFrom) ---
	TechTypes m_iTechPrereq;
	bool m_bValid;
	int m_iMaxPlayerInstances;
};

#endif // CV_SPECIAL_BUILDING_INFO_H
