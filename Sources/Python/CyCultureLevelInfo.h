#pragma once

#ifndef CyCultureLevelInfo_h__
#define CyCultureLevelInfo_h__

//
//	CyCultureLevelInfo -- the CULTURELEVEL accessor, on the PER-INFO plane ([patterns.md] THE PYTHON READ
//	BOUNDARY). The id is the culture level, passed per call.
//
//	⚠ THE READ IS PER (LEVEL x GAMESPEED), which is why it takes two ids rather than one: the stored threshold is
//	a base that the speed scales (`base x GameSpeed.speedPercent / 100`), so a caller that asked for "the
//	threshold" without naming a speed would be asking a question the data does not answer.
//
//	⚑ EARNED BY A LIVE CALL SITE -- the victory screen's culture-victory projection needs the threshold a city is
//	counting toward in order to say how many turns away it is. ⛔ It is NOT a mirror of CvCultureLevelInfo's
//	field set (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)): the level's own `allowed` wonder caps and its modifier families are
//	read where those planes are read, not here.
//
class CyCultureLevelInfo
{
public:
	CyCultureLevelInfo() {}

	// The culture a city must bank to reach this level at the given game speed. -1 if either id is not real,
	// so a caller can tell "no such level" from a genuinely free level.
	int getSpeedThreshold(int iCultureLevel, int iGameSpeed) const;

	/// <summary>This level's runtime TIER ORDINAL, assigned at load; -1 when the id names no level. An editor
	/// lists only the levels a city can actually be set to, which is what the ordinal distinguishes.</summary>
	int getLevel(int iCultureLevel) const;

	static void pythonPublish();
};

#endif // CyCultureLevelInfo_h__
