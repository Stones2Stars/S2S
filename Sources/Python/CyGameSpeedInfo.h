#pragma once

#ifndef CyGameSpeedInfo_h__
#define CyGameSpeedInfo_h__

//
//	CyGameSpeedInfo -- the GAMESPEED accessor, a sibling of CyWorldInfo on the PER-INFO plane the Python read
//	boundary is built from ([patterns.md] THE PYTHON READ BOUNDARY). The id is the GAME SPEED, passed per call:
//	an accessor holds no bound entity, so it is a plain read surface rather than a handle with a lifetime.
//
//	⛔ WHY THESE ARE NAMED HERE RATHER THAN ON THE GENERIC PLANE. Era pacing belongs to ONE registry, so a
//	`getIntrinsic("GAMESPEED_", id, PYINT_...)` slot read would leave the call site naming a slot instead of the
//	value -- the opacity the `Cy*` cut was for. The prefix-addressed plane ([CyInfo]) stays reserved for what is
//	uniform across every registry: identity text, classification tests, edge families.
//
//	⚠ EACH READ IS EARNED BY A LIVE CALL SITE, which is the whole of why there are four: the TimeKeeper screen
//	(Ctrl+F3) renders an era x gamespeed pacing table and asks for exactly these. It is not a mirror of the
//	legacy per-field getter contract (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)), which is a different axis.
//
//	⚑ THESE ARE DERIVED READS OVER INFO DATA ONLY -- this speed's percent against CvEraInfo's Normal-speed turn
//	count and historical year span ([CvGameSpeedInfo]). Nothing calendar-related is stored, so there is no cache
//	behind them and no staleness question to ask.
//
class CyGameSpeedInfo
{
public:
	CyGameSpeedInfo() {}

	// Turns this era lasts at this speed, and the turn it begins on (the sum of every prior era's turns).
	int getTurnsInEra(int iGameSpeed, int iEra) const;
	int getEraStartTurn(int iGameSpeed, int iEra) const;
	// Calendar ticks one turn advances inside the era (days; 30/month, 360/year).
	int getTicksPerTurnInEra(int iGameSpeed, int iEra) const;
	// Turns the whole game runs at this speed.
	int getTotalTurns(int iGameSpeed) const;

	static void pythonPublish();
};

#endif // CyGameSpeedInfo_h__
