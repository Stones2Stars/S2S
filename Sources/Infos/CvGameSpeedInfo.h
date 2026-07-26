#pragma once

#ifndef CV_GAME_SPEED_INFO_H
#define CV_GAME_SPEED_INFO_H

//
//	CvGameSpeedInfo -- the GAMESPEED poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP;
//	wave D, the config-heavy cut). A game speed is a CONFIG entity (state-repositories.md § WORLD is CONFIG):
//	it scales costs/durations by its speed percent and stretches the game over proportionally more turns,
//	read from its source, never cached behind a dirty protocol. JSON-fed (Assets/Data/gamespeeds/*.json via
//	mapFrom); no XML read (DEC-no-xml-into-game).
//
//	The legacy scalar MIRRORS are DEAD: the two authored world percents are 1-kind stragglers riding the base
//	getScalar --
//	  speed.world.percent                  -> getScalar(SCALAR_SPEED, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT)
//	  missionYieldMultiplier.world.percent -> getScalar(SCALAR_MISSION_YIELD_MULTIPLIER, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT)
//	(every gamespeed authors both -- verified across Assets/Data/gamespeeds; no default fill survives).
//	The option-gated AdaptHammerCost derivation (speed percent × the upscaled-costs define under
//	GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS) is NOT served here: an info never reads game state
//	(json.md §9 -- a game option gates AT THE CONSUMING SYSTEM); the gate moves to the consumers.
//
//	Era pacing at this speed stays a derived read over INFO data only (this speed percent + CvEraInfo's year
//	span and Normal-speed turn count -- see CvDate): turn counts and calendar ticks are interpolated, nothing
//	calendar-related is stored.
//

#include "CvInfo.h"   // the JSON-info base (mapFrom); on /I -> bare include

namespace picojson { class value; }

class CvGameSpeedInfo : public CvInfo
{
public:

	CvGameSpeedInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 4. DERIVED ERA PACING -- info-data-only reads (see the header) ============
	int getTurnsInEra(int iEra) const;
	int getEraStartTurn(int iEra) const;
	int getTotalTurns() const;
	int getTicksPerTurnInEra(int iEra) const;

protected:
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvModifiers m_modifiers;   // §6 families: speed.world.percent + missionYieldMultiplier.world.percent
};

#endif // CV_GAME_SPEED_INFO_H
