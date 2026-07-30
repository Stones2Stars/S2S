#pragma once
#ifndef CV_UNIT_STATUS_H
#define CV_UNIT_STATUS_H

//
//	UNIT STATUS -- the `status` classification block's runtime side (json.md §8, glossary state.md).
//
//	⚖ A STATUS IS A SPECIFIC COUNTER, DECREMENTED EVERY TURN (owner): something APPLIES it to the unit, it ticks
//	down, and it is OVER at zero. The unit holds an id -> TURNS-REMAINING dictionary and the read is `count > 0`.
//
//	⛔ A STATUS IS NOT A SKILL. A skill is an ability the unit HAS (`blitz`); a status is a condition something
//	PUT ON it for N turns (`paralyzed`). Mis-filing one as a skill is a standing error -- do not re-file it, and
//	do not add a status key to `skills`.
//	⚠ It shares the id->COUNT SHAPE with a city's `amenities` but NOT the model: an amenity's count is a refcount
//	of live grantors (events add or repeal one), a status's is TURNS REMAINING and moves on its own, every turn,
//	with nothing granting it after the moment it was applied. Same shape, different mechanism -- never merged.
//
//	⚑ GREENFIELD: authored fresh, never migrated from a legacy field -- there is no curator mapping for a status
//	and none is coming. The enum below is a HAND-MAINTAINED list; add a member when a status is identified.
//

enum UnitStatus
{
	STATUS_PARALYZED = 0,   // immobilised while it runs -- applied by an event (the legacy immobile timer)
	NUM_UNIT_STATUSES
};

#endif // CV_UNIT_STATUS_H
