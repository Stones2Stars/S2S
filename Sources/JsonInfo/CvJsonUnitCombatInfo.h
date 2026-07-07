#pragma once
#ifndef CV_JSON_UNITCOMBAT_INFO_H
#define CV_JSON_UNITCOMBAT_INFO_H

//
//	CvJsonUnitCombatInfo -- the per-type cascade info for UNITCOMBATS. Composes the section units a unitcombat
//	authors (modifier families / the §8 `skills` bool block / the entity-level gate). Like a promotion, a
//	unitcombat is a grantor of unit skills (e.g. healsAs / defenders / rBombardDirect -- the unit-combat-scoped
//	abilities); the unit's ACTIVE set folds them in on the instance later.
//

#include "CvJsonInfo.h"

class CvJsonUnitCombatInfo : public CvJsonInfo
{
public:
	CvJsonUnitCombatInfo() {}
	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvJsonBoolBlock* getSkills()    const { return &m_skills; }
	virtual const CvJsonGate*      getGate()      const { return &m_gate; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvJsonBoolBlock* mutSkills()    { return &m_skills; }
	virtual CvJsonGate*      mutGate()      { return &m_gate; }

private:
	CvJsonModifiers m_modifiers;
	CvJsonBoolBlock m_skills;
	CvJsonGate      m_gate;
};

#endif // CV_JSON_UNITCOMBAT_INFO_H
