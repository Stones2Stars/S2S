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
//	⚖ THE STORE IS SERIALIZED (owner). Turns-remaining is genuine NON-DERIVABLE state -- nothing can reconstruct
//	"three turns of blackout left" from anything else -- so it is exactly the class save.md par.5 keeps a
//	serialized store for, and [DEC-derived-never-trusted] does not reach it (that rule bans serializing DERIVED
//	data). ⛔ What is deliberately NOT carried is the CONVERSION of a legacy timer into this store: re-homing one
//	drops its old save field, the in-flight value is lost on existing saves, and that is accepted rather than
//	migrated. Re-home, name the old tag in savemigration.txt, take the loss.
//	⚠ A loaded status must LAND THROUGH setStatus, never straight into the array: the array read is wholesale, so
//	a status written directly announces nothing and every consumer gating on it reads a holder that is not held.
//
//	⚖ A STATUS ACTS ON ITS OWN OBJECT, and its consumers are NOTIFICATIONS and LOGGING (owner). A paralyzed unit
//	cannot move, a blackout city is not powered -- the effect lands where the status is HELD, so almost nothing
//	downstream needs to hear about it. ⛔ Hence NO context store, NO dictionary, NO mirror: it is object-owned and
//	O(1), so it is FORWARDED, and a second copy is the duplication the contexts split exists to prevent. The
//	wiring is a hasStatus() call at the point of use. ⚑ It is also why ONE generic fact per scope is enough --
//	with no machine folding on a status, per-status facts would buy precision nobody consumes and cost an engine
//	change per authored status. ⚠ The one cross-machine reader is CITYSTATUS_POWER_DISABLED, a leg of
//	CvCity::isPowered() that HAS_POWER gates on -- and even there the consumer reads the verdict off the CITY while
//	the fact only tells it to re-gate. The fact carries the TRIGGER, never the value.
//

enum UnitStatus
{
	STATUS_PARALYZED = 0,   // immobilised while it runs -- applied by an event
	NUM_UNIT_STATUSES
};

// ⛔ THE PLAYER SCOPE IS DELIBERATELY NOT WIRED, AND THIS ENUM IS FORWARD INTENT ONLY (owner). `CvPlayer`
// carries NO status store: GOLDEN AGE and ANARCHY are still the hand-named m_iGoldenAgeTurns / m_iAnarchyTurns,
// and the EXISTING ENGINE handles their empire-wide effect today. That is a HELD DECISION, not an unfinished
// conversion -- do not read the enum entry as a wired thing.
// ⚑ THE DESIGN IS SETTLED, so this is sequencing and not an open question (owner): the two are EMPIRE-WIDE ON
// ALL CITIES, and they resolve by LANDING A STATUS ON EACH CITY off the empire-scope happening --
// SEVT_EMPIRE_GOLDEN_AGE_ADDED / _REMOVED and SEVT_EMPIRE_ANARCHY_ADDED / _REMOVED, which already exist. The
// player holds the SOURCE (am I in one, for how long); each city holds the EFFECT as an ordinary city status.
// ⚑ So the object-local rule above is not broken by them after all -- it is restored by the fan: once the
// status is city-held, hasStatus() at the point of use is the whole wiring again, exactly as everywhere else.
// ⛔ AND IT IS BUILT AT THE END, WHEN THE STRUCTURE IS SET -- NOT AS PART OF INITIAL SETUP, "because that is how
// rollerskating happens" (owner). Do not wire the consumer now, and do not re-home the two members onto a
// player store to "prepare" for it. Both are the shape that looks like progress and pre-commits the structure.
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
	// A blackout: the city's power is out for N turns and comes back on its own. Applied by an event, ticking
	// down, over at zero -- the model exactly, and previously written out longhand as a hand-named member with
	// its own getter, changer, per-turn decrement and save field.
	// ⚠ What is not carried is the CONVERSION from that old member -- an existing save's in-flight blackout is
	// simply lost, because migrating a transient counter is not worth it for a short-term changeover (owner).
	// The status STORE itself serializes like every other, so a blackout applied from here on survives a save.
	CITYSTATUS_POWER_DISABLED,
	NUM_CITY_STATUSES
};

#endif // CV_STATUS_H
