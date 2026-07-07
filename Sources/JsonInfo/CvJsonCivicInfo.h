#pragma once
#ifndef CV_JSON_CIVIC_INFO_H
#define CV_JSON_CIVIC_INFO_H

//
//	CvJsonCivicInfo -- the per-type cascade info for CIVICS. Composes the section units a civic authors (edges /
//	grants / modifier families / the §9 `policies` bool block). `policies` are the pure empire STATES this civic
//	ENACTS (noForeignTrade / noCorporations / fixedBorders / …), active while the civic is adopted -- ONE meaning
//	with two grantors (a civic enacts them, a trait grants them permanently), so CvJsonTraitInfo composes the SAME
//	unit. A policy is a pure state, NEVER a parameterized/targeted rule (that is an enabler `requires` concern).
//

#include "CvJsonInfo.h"

class CvJsonCivicInfo : public CvJsonInfo
{
public:
	CvJsonCivicInfo() {}
	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getPolicies()  const { return &m_policies; }

protected:
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutPolicies()  { return &m_policies; }

private:
	CvJsonEdges     m_edges;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_policies;
};

#endif // CV_JSON_CIVIC_INFO_H
