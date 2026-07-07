#pragma once
#ifndef CV_JSON_PROMOTION_INFO_H
#define CV_JSON_PROMOTION_INFO_H

//
//	CvJsonPromotionInfo -- the per-type cascade info for PROMOTIONS. Composes the section units a promotion authors
//	(modifier families / the §8 `skills` bool block / the entity-level gate). A promotion is a grantor of unit
//	skills (the mutable, promotion-grantable abilities -- blitz / amphib / …); a unit's ACTIVE skill set is its
//	type's base skills + the skills of its held promotions, resolved on the unit INSTANCE later -- this static info
//	is just the definition of what THIS promotion contributes.
//

#include "CvJsonInfo.h"

class CvJsonPromotionInfo : public CvJsonInfo
{
public:
	CvJsonPromotionInfo() {}
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

#endif // CV_JSON_PROMOTION_INFO_H
