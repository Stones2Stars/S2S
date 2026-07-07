#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvJsonBuildingInfo -- the per-type cascade info for BUILDINGS (ports StoneBase's BuildingInfo). Composes the
//	section units a building authors (requires / edges / allowed / grants / provides / modifier families /
//	whenObsolete / attributes / capabilities -- the data-grounded table); this adds the typed flags + the curator
//	`identity` block, SELF-CONTAINED (the engine getGlobalReligionCommerce / getReligionType /
//	getGlobalCorporationCommerce / getStateReligionCommerce / getCommerceChangeDoubleTime reads are RETIRED).
//	shrine/corpHQ/religion are FK ids (-1 none); the commerce blocks are {channel:value} maps.
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

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()     const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()        const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()       const { return &m_grants; }
	virtual const CvJsonProvides*  getProvides()     const { return &m_provides; }
	virtual const CvJsonModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvJsonModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	virtual const CvJsonBoolBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvJsonBoolBlock* getCapabilities() const { return &m_capabilities; }

protected:
	virtual CvJsonRequires*  mutRequires()     { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()        { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()       { return &m_grants; }
	virtual CvJsonProvides*  mutProvides()     { return &m_provides; }
	virtual CvJsonModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvJsonModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvJsonBoolBlock* mutAttributes()   { return &m_attributes; }
	virtual CvJsonBoolBlock* mutCapabilities() { return &m_capabilities; }

private:
	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonProvides  m_provides;
	CvJsonModifiers m_modifiers;
	CvJsonModifiers m_whenObsolete;
	CvJsonBoolBlock m_attributes;
	CvJsonBoolBlock m_capabilities;
};

#endif // CV_JSON_BUILDING_INFO_H
