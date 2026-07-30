#pragma once
#ifndef CV_STATUS_H
#define CV_STATUS_H

//
//	STATUS -- an APPLIED COUNTER THAT TICKS DOWN, one enum per SCOPE that carries them.
//
//	⚖ THE MODEL (owner): something applies a status, it decrements EVERY TURN, and it is OVER at zero. The scope
//	owner holds an id -> TURNS-REMAINING dictionary and the gate is simply `count > 0` -- so expiry needs no
//	second fact, and no per-status named accessor exists (`hasStatus(STATUS_X)` IS the read; a named twin per
//	status is the per-channel getter shape this rebuild deletes).
//
//	⚖ IT IS NOT A UNIT CONCEPT -- IT IS A SCOPE ONE (owner). A unit is paralyzed, a PLAYER is in a golden age, a
//	CITY is celebrating. All three are the same mechanic: applied, ticking, over. So each scope that carries
//	statuses gets its own enum below and the identical store/accessor/tick shape on its owner, rather than each
//	timer being a hand-named member with its own getter, setter, decrement and save field.
//	⚑ That is the whole point: the legacy engine wrote this mechanic out longhand once per timer.
//
//	⛔ A STATUS IS NOT A SKILL, and not an amenity either. A skill is an ability the holder HAS; an amenity's
//	count is a REFCOUNT of live grantors; a status's count is TURNS REMAINING and moves on its own, every turn,
//	with nothing granting it after the moment it was applied. Same id->COUNT shape, different model -- never
//	merged.
//
//	⚑ GREENFIELD and OPEN: statuses are authored fresh, never migrated from a legacy field, and the enums are
//	HAND-MAINTAINED -- when we find more, we add more (owner). More arriving is the normal state, never a gap to
//	close. Glossary: docs/specs/state.md.
//

enum UnitStatus
{
	STATUS_PARALYZED = 0,   // immobilised while it runs -- applied by an event
	NUM_UNIT_STATUSES
};

enum PlayerStatus
{
	PLAYERSTATUS_GOLDEN_AGE = 0,   // the empire-wide boost period; mutually exclusive with anarchy
	NUM_PLAYER_STATUSES
};

enum CityStatus
{
	// We Love the King/Emperor Day. ⚖ A ONE-TURN status, RE-APPLIED EVERY TURN BY A TRIGGER while its
	// conditions match (owner) -- so it lapses by simply not being re-applied, and needs no separate clear.
	// ⚑ That is why a duration-1 status is not a degenerate case but the natural shape for a "while X holds"
	// condition: the trigger owns the test, the counter owns the ending.
	// ⚖ ITS LEGACY TRIGGER WIRING STAYS (owner) -- re-applying it from the trigger plane is funky, so the
	// per-turn CONDITION test keeps its existing home. The status owns the STORAGE and the READ; the legacy
	// code owns deciding whether the conditions match. ⛔ Not a half-migration to finish opportunistically --
	// an owner-ruled carve-out.
	CITYSTATUS_WE_LOVE_THE_KING_DAY = 0,
	NUM_CITY_STATUSES
};

#endif // CV_STATUS_H
