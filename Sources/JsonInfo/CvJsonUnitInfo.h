#pragma once
#ifndef CV_JSON_UNIT_INFO_H
#define CV_JSON_UNIT_INFO_H

//
//	CvJsonUnitInfo -- the per-type cascade info for UNITS. Composes the section units a unit authors (requires /
//	edges / allowed / grants / modifier families / the §8 `skills` + `tags` bool blocks / the entity-level gate --
//	the data-grounded table). Extensions: the SpawnOnly / UnlimitedException flags the enabler reads + the `builds`
//	repertoire. These are the unit TYPE's definition; a unit INSTANCE's ACTIVE skill set (type-base skills + its
//	promotions' + unit-combat's) is resolved on the instance later (out of this static pass).
//

#include "CvJsonInfo.h"
#include <vector>

class CvJsonUnitInfo : public CvJsonInfo
{
public:
	CvJsonUnitInfo() : spawnOnly(false), unlimitedException(false) {}
	bool spawnOnly, unlimitedException;
	std::vector<int> builds;        // top-level `builds`: the unit type's build REPERTOIRE (resolved BUILD_* ids)
	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()  const { return &m_requires; }
	virtual const CvJsonEdges*     getEdges()     const { return &m_edges; }
	virtual const CvJsonAllowed*   getAllowed()   const { return &m_allowed; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getSkills()    const { return &m_skills; }   // §8 mutable abilities (blitz/amphib/…)
	virtual const CvJsonBoolBlock* getTags()      const { return &m_tags; }     // §8 immutable type membership (accounting)
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }

protected:
	virtual CvJsonRequires*  mutRequires()  { return &m_requires; }
	virtual CvJsonEdges*     mutEdges()     { return &m_edges; }
	virtual CvJsonAllowed*   mutAllowed()   { return &m_allowed; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutSkills()    { return &m_skills; }
	virtual CvJsonBoolBlock* mutTags()      { return &m_tags; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }

private:
	CvJsonRequires  m_requires;
	CvJsonEdges     m_edges;
	CvJsonAllowed   m_allowed;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_skills;
	CvJsonBoolBlock m_tags;
	CvJsonGate      m_gate;
};

#endif // CV_JSON_UNIT_INFO_H
