#pragma once
#ifndef CV_JSON_PROPERTY_INFO_H
#define CV_JSON_PROPERTY_INFO_H

//
//	CvJsonPropertyInfo -- the JSON poco for PROPERTIES (crime/pollution/…-class plot+city scalars; uniformity
//	ruling: every info type has its own CvJson<X>Info home). Composes the section units the property data authors:
//	`grants` (property pulses) + its modifier families. Everything else is served by the CvJsonInfo base; no typed
//	members yet (the base dispatch covers the composed sections -- no mapFrom override).
//

#include "CvJsonInfo.h"

class CvJsonPropertyInfo : public CvJsonInfo
{
public:
	CvJsonPropertyInfo();

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
};

#endif // CV_JSON_PROPERTY_INFO_H
