#pragma once
#ifndef CV_JSON_HANDICAP_INFO_H
#define CV_JSON_HANDICAP_INFO_H

//
//	CvJsonHandicapInfo -- the JSON poco for HANDICAPS (uniformity ruling: every info type has its own CvJson<X>Info
//	home). Composes the section units the handicap data authors: `grants` (the game-start startingGold pulse the
//	grants machine resolves) + its modifier families. Everything else is served by the CvJsonInfo base; no typed
//	members yet (the base dispatch covers the composed sections -- no mapFrom override).
//

#include "CvJsonInfo.h"

class CvJsonHandicapInfo : public CvJsonInfo
{
public:
	CvJsonHandicapInfo();

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

#endif // CV_JSON_HANDICAP_INFO_H
