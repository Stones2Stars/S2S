//
//	⛔ TEMPORARY -- PURGE BEFORE wiring the new readJson data (see CvCascadeSelfTest.h).
//	Validates the #430 cascade machinery (EventSpine dispatch + CvScopedAccumulator counting + shadow-compare)
//	against the OLD/live engine data. Gated by gPlayerLogLevel; results flow through the spine to Cascade.log +
//	the live /events stream, so the tally results ARE the test events.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeSelfTest.h"
#include "CvEventSpine.h"
#include "CvScopedAccumulator.h"
#include "CvCascadeTally.h"
#include "AI/BetterBTSAI.h"   // gPlayerLogLevel
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"
#include "AI/CvPlayerAI.h"
#include "Engine/CvCity.h"
#include "Engine/CvUnit.h"

// Temporary test-event ids (all DIAGNOSTIC -- unsynced, logging-only; never gate).
enum
{
	TEST_EVT_TURN_START = 1, // {a = game turn}
	TEST_EVT_CITY_TALLY = 2, // {type = playerId, a = recomputed tally, b = engine truth}  (a == b => MATCH)
	TEST_EVT_UNIT_TALLY = 3  // {type = playerId, a = unit-types checked, b = mismatches}   (b == 0 => MATCH)
};

void cascadeSelfTest()
{
	if (gPlayerLogLevel < 1)
	{
		return; // gated: only runs while AI logging is on -- free otherwise
	}

	CvEventSpine& kSpine = eventSpine();

	// TEST EVENT: turn start.
	kSpine.emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, TEST_EVT_TURN_START, -1, GC.getGame().getGameTurn(), 0));

	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (!kPlayer.isAlive())
		{
			continue;
		}

		// --- TEST TALLY 1: city count (empire scope) -- recompute by walking the player's cities, shadow vs
		//     getNumCities(). Proves the player->city roll-up against ground truth.
		int iCityTally = 0;
		int iLoop = 0;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
		{
			++iCityTally;
		}
		kSpine.emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, TEST_EVT_CITY_TALLY, iPlayer, iCityTally, kPlayer.getNumCities()));

		// --- TEST TALLY 2: unit count by type (empire scope) -- recompute into a CvScopedAccumulator over the
		//     player's units, shadow each counted type vs getUnitCount(type). Proves the per-Type substrate.
		CvScopedAccumulator kUnitTally;
		for (const CvUnit* pUnit = kPlayer.firstUnit(&iLoop); pUnit != NULL; pUnit = kPlayer.nextUnit(&iLoop))
		{
			kUnitTally.deposit((int)pUnit->getUnitType(), 1);
		}
		int iTypesChecked = 0;
		int iMismatches = 0;
		for (CvScopedAccumulator::const_iterator it = kUnitTally.begin(); it != kUnitTally.end(); ++it)
		{
			++iTypesChecked;
			if (it->second != kPlayer.getUnitCount((UnitTypes)it->first))
			{
				++iMismatches;
			}
		}
		kSpine.emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, TEST_EVT_UNIT_TALLY, iPlayer, iTypesChecked, iMismatches));
	}

	// Shadow the event-maintained TALLY (the first real DOMAIN consumer) against ground truth each gated turn:
	// recompute the world per-building counts + diff vs the accumulator. TEMPORARY, like the rest of this harness.
	cascadeTally().shadowVerify();
}
