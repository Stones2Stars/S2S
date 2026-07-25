#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

#include "CvInfo.h"

class CvBuildingInfo : public CvInfo
{
public:
	virtual const CvRequires*  getRequires()     const { return &m_requires; }
	virtual const CvEdges*     getEdges()        const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvGrants*    getGrants()       const { return &m_grants; }
	virtual const CvTriggers*      getTriggers()     const { return &m_triggers; }
	virtual const CvProvides*  getProvides()     const { return &m_provides; }
	virtual const CvModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	virtual const CvClassificationBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvClassificationBlock* getCapabilities() const { return &m_capabilities; }

	// --- the compiled modifier-group point reads (patterns.md § THE GETTER SETUP: one getter per group,
	// parameterized over kind and scope -- [DEC-scope-is-an-axis]; ×100 straight fetch, 0 calculation) ---
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum100(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum100(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	// the compiled conditioned list (prebuilt trees; the package rebuild / pedia / valuation walk)
	const std::vector<const CvModEntry*>& modifierConditioned() const { return m_modifiers.conditioned(); }

protected:
	virtual CvRequires*  mutRequires()     { return &m_requires; }
	virtual CvEdges*     mutEdges()        { return &m_edges; }
	virtual CvAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvGrants*    mutGrants()       { return &m_grants; }
	virtual CvTriggers*      mutTriggers()     { return &m_triggers; }
	virtual CvProvides*  mutProvides()     { return &m_provides; }
	virtual CvModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvClassificationBlock* mutAttributes()   { return &m_attributes; }
	virtual CvClassificationBlock* mutCapabilities() { return &m_capabilities; }

private:
	CvRequires  m_requires;
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvGrants    m_grants;
	CvTriggers      m_triggers;
	CvProvides  m_provides;
	CvModifiers m_modifiers;
	CvModifiers m_whenObsolete;
	CvClassificationBlock m_attributes;
	CvClassificationBlock m_capabilities;
};

#endif // CV_JSON_BUILDING_INFO_H
