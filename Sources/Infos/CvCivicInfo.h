#pragma once
#ifndef CV_JSON_CIVIC_INFO_H
#define CV_JSON_CIVIC_INFO_H

#include "CvInfo.h"

class CvCivicInfo : public CvInfo
{
public:
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvGrants*    getGrants()    const { return &m_grants; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getPolicies()  const { return &m_policies; }

	// --- the compiled modifier-group point reads (patterns.md § THE GETTER SETUP; [DEC-scope-is-an-axis]) ---
	int getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum100(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_PERCENT); }
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum100(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	const std::vector<const CvModEntry*>& modifierConditioned() const { return m_modifiers.conditioned(); }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvGrants*    mutGrants()    { return &m_grants; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutPolicies()  { return &m_policies; }

private:
	CvEdges     m_edges;
	CvGrants    m_grants;
	CvModifiers m_modifiers;
	CvClassificationBlock m_policies;
};

#endif // CV_JSON_CIVIC_INFO_H
