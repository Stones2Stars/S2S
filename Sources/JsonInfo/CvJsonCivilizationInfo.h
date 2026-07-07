#pragma once
#ifndef CV_JSON_CIVILIZATION_INFO_H
#define CV_JSON_CIVILIZATION_INFO_H

//
//	CvJsonCivilizationInfo -- the JSON poco for CIVILIZATIONS (uniformity ruling: every info type has its own
//	CvJson<X>Info home). Composes the section units the civilization data authors: `edges` (enables.*) + `grants`
//	(the game-start civics/techs/buildings seeds the grants machine resolves). Everything else is served by the
//	CvJsonInfo base; no typed members yet (the base dispatch covers the composed sections -- no mapFrom override).
//

#include "CvJsonInfo.h"

class CvJsonCivilizationInfo : public CvJsonInfo
{
public:
	CvJsonCivilizationInfo();

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonEdges*  getEdges()  const { return &m_edges; }
	virtual const CvJsonGrants* getGrants() const { return &m_grants; }

protected:
	virtual CvJsonEdges*  mutEdges()  { return &m_edges; }
	virtual CvJsonGrants* mutGrants() { return &m_grants; }

private:
	CvJsonEdges  m_edges;
	CvJsonGrants m_grants;
};

#endif // CV_JSON_CIVILIZATION_INFO_H
