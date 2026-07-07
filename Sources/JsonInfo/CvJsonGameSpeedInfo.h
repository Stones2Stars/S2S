#pragma once
#ifndef CV_JSON_GAMESPEED_INFO_H
#define CV_JSON_GAMESPEED_INFO_H

//
//	CvJsonGameSpeedInfo -- the JSON poco for GAME SPEEDS (uniformity ruling: every info type has its own
//	CvJson<X>Info home). Composes the section unit the game-speed data authors: its modifier families (the
//	speed.world.percent scaling family). Everything else is served by the CvJsonInfo base; no typed members yet
//	(the base dispatch covers the composed section -- no mapFrom override).
//

#include "CvJsonInfo.h"

class CvJsonGameSpeedInfo : public CvJsonInfo
{
public:
	CvJsonGameSpeedInfo();

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonModifiers m_modifiers;
};

#endif // CV_JSON_GAMESPEED_INFO_H
