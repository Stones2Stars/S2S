#pragma once
#ifndef CV_JSON_TRAIT_INFO_H
#define CV_JSON_TRAIT_INFO_H

//
//	CvJsonTraitInfo -- the per-type cascade info for TRAITS (the COMMON base of the two trait sets). Composes the
//	section units a trait authors (edges / grants / modifier families / the §9 `policies` bool block -- the SAME
//	pure-empire-STATE set a civic enacts; a trait grants them permanently while held). Extension: the `negativeTrait`
//	alignment flag (the PURE_TRAITS gate). The two DISTINCT trait sets are CvJsonSimpleTraitInfo /
//	CvJsonComplexTraitInfo (their ids collide; the active set is chosen by GAMEOPTION_LEADER_COMPLEX_TRAITS). The
//	cascade NEVER reads the engine CvTraitInfo for trait values (its runtime CvInfoReplacements swap can't represent
//	this clean split).
//
//	⏳ Note (owner 2026-07-01): the legacy `freeSpecialistPer{World,National,Team}Wonder` keys under a trait `policies`
//	block are EFFECTS (free specialists scaled per wonder, CvCity:5764), not pure states -> they reclassify to a
//	`freeSpecialists` modifier family via the curator; until then they ride here in `policies` harmlessly (no consumer
//	this pass). (`nonStateReligionCommerce` is VERIFIED a pure STATE -- a Free-Church permission -- so it correctly stays.)
//

#include "CvJsonInfo.h"

class CvJsonTraitInfo : public CvJsonInfo
{
public:
	CvJsonTraitInfo() : negativeTrait(false) {}
	bool negativeTrait;                 // StoneBase NegativeTrait -- PURE_TRAITS drops a negative trait's positive values / a positive trait's negative
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

#endif // CV_JSON_TRAIT_INFO_H
