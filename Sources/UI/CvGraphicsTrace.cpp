#include "CvGameCoreDLL.h"

#include "UI/CvGraphicsTrace.h"
#include "Spine/CvEventSpine.h"
#include "Engine/CvUnit.h"

namespace
{
	//	The domain's own event ids and field tags. Both are DOMAIN-LOCAL: the spine never names them, and adding a
	//	third emitter here touches no shared file (event-spine.md -- per-domain isolation).
	enum GfxEventId
	{
		GFXEVT_CENTER_UNIT = 0,
		GFXEVT_ENTITY,
		GFXEVT_DEFENDER_REFUSED,
		GFXEVT_DEFENDER_SCAN,
		GFXEVT_DEFENDER_REJECT,
		GFXEVT_MOVE
	};

	enum GfxFieldTag
	{
		GFXF_X = 0,
		GFXF_Y,
		GFXF_GATE,
		GFXF_ACTIVE_VIS,
		GFXF_OLD_UNIT,
		GFXF_NEW_UNIT,
		GFXF_OWNER,
		GFXF_UNIT_TYPE,
		GFXF_REAL,
		GFXF_NUM_REAL,
		GFXF_NUM_DUMMY,
		GFXF_IS_CENTER,
		GFXF_IS_ACTIVE_PLAYER,
		GFXF_UNIT_ID,
		GFXF_IN_VIEWPORT,
		GFXF_USING_DUMMY,
		GFXF_UNITS_SEEN,
		GFXF_SCORED_POSITIVE,
		GFXF_CAN_DEFEND,
		GFXF_OWNER_FILTER,
		GFXF_ATTACKER_FILTER,
		GFXF_TEST_CAN_MOVE,
		GFXF_PREDICTED_HP,
		GFXF_IS_DEAD,
		GFXF_UNIT_OWNER,
		GFXF_REJECT_REASON,
		GFXF_FROM_X,
		GFXF_FROM_Y,
		GFXF_MOVE_OUTCOME,
		GFXF_GRAPHICS_INIT,
		GFXF_SHOW,
		GFXF_WATCHED
	};

	const char* gfx_domainPrefix(int iEventId)
	{
		switch (iEventId)
		{
		case GFXEVT_CENTER_UNIT:      return "[GFX] centerUnit";
		case GFXEVT_ENTITY:           return "[GFX] entity";
		case GFXEVT_DEFENDER_REFUSED: return "[GFX] defenderRefused";
		case GFXEVT_DEFENDER_SCAN:    return "[GFX] defenderScan";
		case GFXEVT_DEFENDER_REJECT:  return "[GFX] defenderReject";
		case GFXEVT_MOVE:             return "[GFX] move";
		default:                      return "[GFX] ?";
		}
	}

	//	⛔ The DECLARED type here and the ADDER used at the emit must agree -- the renderer switches on the declared
	//	type, so a field declared SFT_STR and filled with addI is read as a char* and the process dies at the value's
	//	own address. `python Tools/verify-spine-fields.py` checks exactly this pair.
	const char* gfx_domainFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		switch (iFieldTag)
		{
		case GFXF_X:          *peType = SFT_INT;    return "x";
		case GFXF_Y:          *peType = SFT_INT;    return "y";
		case GFXF_GATE:       *peType = SFT_STR;    return "gate";
		case GFXF_ACTIVE_VIS: *peType = SFT_INT;    return "activeVis";
		case GFXF_OLD_UNIT:   *peType = SFT_INT;    return "oldUnit";
		case GFXF_NEW_UNIT:   *peType = SFT_INT;    return "newUnit";
		case GFXF_OWNER:      *peType = SFT_PLAYER; return "owner";
		case GFXF_UNIT_TYPE:  *peType = SFT_UNIT;   return "unit";
		case GFXF_REAL:       *peType = SFT_INT;    return "real";
		case GFXF_NUM_REAL:   *peType = SFT_INT;    return "numReal";
		case GFXF_NUM_DUMMY:  *peType = SFT_INT;    return "numDummy";
		case GFXF_IS_CENTER:         *peType = SFT_INT; return "isCenter";
		case GFXF_IS_ACTIVE_PLAYER:  *peType = SFT_INT; return "isActivePlayer";
		case GFXF_UNIT_ID:           *peType = SFT_INT; return "unitId";
		case GFXF_IN_VIEWPORT:       *peType = SFT_INT; return "inViewport";
		case GFXF_USING_DUMMY:       *peType = SFT_INT; return "usingDummy";
		case GFXF_UNITS_SEEN:        *peType = SFT_INT; return "unitsSeen";
		case GFXF_SCORED_POSITIVE:   *peType = SFT_INT; return "scoredPositive";
		case GFXF_CAN_DEFEND:        *peType = SFT_INT; return "sampleCanDefend";
		case GFXF_OWNER_FILTER:      *peType = SFT_INT; return "ownerFilter";
		case GFXF_ATTACKER_FILTER:   *peType = SFT_INT; return "attackerFilter";
		case GFXF_TEST_CAN_MOVE:     *peType = SFT_INT; return "testCanMove";
		case GFXF_PREDICTED_HP:      *peType = SFT_INT; return "predictedHP";
		case GFXF_IS_DEAD:           *peType = SFT_INT; return "isDead";
		case GFXF_UNIT_OWNER:        *peType = SFT_INT; return "unitOwner";
		case GFXF_REJECT_REASON:     *peType = SFT_STR; return "reason";
		case GFXF_FROM_X:            *peType = SFT_INT; return "fromX";
		case GFXF_FROM_Y:            *peType = SFT_INT; return "fromY";
		case GFXF_MOVE_OUTCOME:      *peType = SFT_STR; return "outcome";
		case GFXF_GRAPHICS_INIT:     *peType = SFT_INT; return "gfxInit";
		case GFXF_SHOW:              *peType = SFT_INT; return "show";
		case GFXF_WATCHED:           *peType = SFT_INT; return "watched";
		default:              *peType = SFT_INT;    return "?";
		}
	}

	//	Self-registration on first emit: the domain is live the moment something emits, and dead weight otherwise.
	void gfx_ensureRegistered()
	{
		static bool s_bRegistered = false;
		if (!s_bRegistered)
		{
			s_bRegistered = true;
			spineRegisterDomain(SD_GRAPHICS, gfx_domainPrefix, "Graphics.log", gfx_domainFieldInfo);
		}
	}

	//	A string LITERAL, so the pointee trivially outlives the synchronous emit (the render happens on the game
	//	thread, inside emit).
	const char* gfx_gateName(GfxCenterUnitGate eGate)
	{
		switch (eGate)
		{
		case GFX_GATE_INHIBITED:      return "inhibited";
		case GFX_GATE_NOT_VISIBLE:    return "notGraphicsVisible";
		case GFX_GATE_NOT_ACTIVE_VIS: return "notActiveVisible";
		default:                      return "none";
		}
	}
}

void gfxTraceCenterUnit(int iX, int iY, GfxCenterUnitGate eGate, bool bActiveVisible,
                        int iOldUnitId, int iNewUnitId)
{
	gfx_ensureRegistered();

	//	Level 3 -- the per-candidate tier (observability.md): updateCenterUnit runs per plot per membership change,
	//	so it costs nothing until someone asks for it.
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_CENTER_UNIT, 3)
		.addI(GFXF_X, iX)
		.addI(GFXF_Y, iY)
		.addStr(GFXF_GATE, gfx_gateName(eGate))
		.addI(GFXF_ACTIVE_VIS, bActiveVisible ? 1 : 0)
		.addI(GFXF_OLD_UNIT, iOldUnitId)
		.addI(GFXF_NEW_UNIT, iNewUnitId));
}

void gfxTraceEntity(const CvUnit* pUnit, bool bReal, int iNumReal, int iNumDummy,
                    bool bActiveVisible, bool bIsCenterUnit, bool bIsActivePlayer)
{
	if (pUnit == NULL)
	{
		return;
	}
	gfx_ensureRegistered();

	//	Level 2 -- per-decision: an entity attach is rarer than a centre-unit pass and it is the line the texture /
	//	scene-memory question is actually read off, so it wants to be available a tier earlier.
	//	⚑ numReal is a NET count: reloadEntity destroys the old entity before creating the new one, so attaches far
	//	outnumbering the net total is entity CHURN -- real scene nodes torn down and rebuilt, which is both a memory
	//	question and why an in-flight animation would never finish.
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_ENTITY, 2)
		.addI(GFXF_OWNER, (int)pUnit->getOwner())
		.addI(GFXF_UNIT_ID, pUnit->getID())
		.addI(GFXF_UNIT_TYPE, (int)pUnit->getUnitType())
		.addI(GFXF_REAL, bReal ? 1 : 0)
		.addI(GFXF_NUM_REAL, iNumReal)
		.addI(GFXF_NUM_DUMMY, iNumDummy)
		.addI(GFXF_ACTIVE_VIS, bActiveVisible ? 1 : 0)
		.addI(GFXF_IS_CENTER, bIsCenterUnit ? 1 : 0)
		.addI(GFXF_IS_ACTIVE_PLAYER, bIsActivePlayer ? 1 : 0));
}

void gfxTraceDefenderScan(int iX, int iY, int iUnitsSeen, int iScoredPositive,
                          int iSampleUnitId, int iSampleUnitType, bool bSampleCanDefend,
                          int iOwnerFilter, int iAttackerFilter, bool bTestCanMove)
{
	gfx_ensureRegistered();

	//	Level 2: the caller only reaches this where the scan already FAILED over a non-empty list, so it is rare by
	//	construction. unitsSeen>0 with scoredPositive==0 is the whole finding -- the list is fine and the SCORER
	//	rejected everything.
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_DEFENDER_SCAN, 2)
		.addI(GFXF_X, iX)
		.addI(GFXF_Y, iY)
		.addI(GFXF_UNITS_SEEN, iUnitsSeen)
		.addI(GFXF_SCORED_POSITIVE, iScoredPositive)
		.addI(GFXF_UNIT_ID, iSampleUnitId)
		.addI(GFXF_UNIT_TYPE, iSampleUnitType)
		.addI(GFXF_CAN_DEFEND, bSampleCanDefend ? 1 : 0)
		.addI(GFXF_OWNER_FILTER, iOwnerFilter)
		.addI(GFXF_ATTACKER_FILTER, iAttackerFilter)
		.addI(GFXF_TEST_CAN_MOVE, bTestCanMove ? 1 : 0));
}

void gfxTraceDefenderRefused(int iX, int iY, int iUnitId, bool bInViewport, bool bUsingDummy)
{
	gfx_ensureRegistered();

	//	Level 2: this fires only where a defender was FOUND and then thrown away, so it is rare by construction and
	//	is the line that turns "the plot presented nothing" into "this filter refused it".
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_DEFENDER_REFUSED, 2)
		.addI(GFXF_X, iX)
		.addI(GFXF_Y, iY)
		.addI(GFXF_UNIT_ID, iUnitId)
		.addI(GFXF_IN_VIEWPORT, bInViewport ? 1 : 0)
		.addI(GFXF_USING_DUMMY, bUsingDummy ? 1 : 0));
}

void gfxTraceMove(int iFromX, int iFromY, int iToX, int iToY, GfxMoveOutcome eOutcome,
                  bool bGraphicsInitialized, bool bInViewport, bool bRealEntity,
                  bool bShow, bool bVisibleToWatchingHuman)
{
	gfx_ensureRegistered();

	//	A string LITERAL, so the pointee trivially outlives the synchronous emit.
	const char* szOutcome =
		(eOutcome == GFX_MOVE_QUEUED)     ? "queued" :
		(eOutcome == GFX_MOVE_TELEPORTED) ? "teleported" :
		                                    "skipped";

	//	Level 2 — one line per unit per plot change, which is move volume rather than frame volume. `skipped`
	//	is the whole question: the scene node was never told the unit left, so it keeps drawing where it was.
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_MOVE, 2)
		.addI(GFXF_FROM_X, iFromX)
		.addI(GFXF_FROM_Y, iFromY)
		.addI(GFXF_X, iToX)
		.addI(GFXF_Y, iToY)
		.addStr(GFXF_MOVE_OUTCOME, szOutcome)
		.addI(GFXF_GRAPHICS_INIT, bGraphicsInitialized ? 1 : 0)
		.addI(GFXF_IN_VIEWPORT, bInViewport ? 1 : 0)
		.addI(GFXF_REAL, bRealEntity ? 1 : 0)
		.addI(GFXF_SHOW, bShow ? 1 : 0)
		.addI(GFXF_WATCHED, bVisibleToWatchingHuman ? 1 : 0));
}

void gfxTraceDefenderReject(int iX, int iY, int iUnitId, int iUnitType,
                            int iPredictedHitPoints, bool bIsDead,
                            int iOwnerFilter, int iUnitOwner)
{
	gfx_ensureRegistered();

	//	Name the clause that fired. All three are tested in one `if`, so only the FIRST true one is the cause -- the
	//	order here mirrors the order in getDefenderScore exactly.
	const char* szReason =
		(iPredictedHitPoints == 0) ? "predictedDead" :
		bIsDead                    ? "isDead" :
		                             "ownerMismatch";

	//	Level 2: emitted only on the reject, so its volume IS the fault's volume.
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_GRAPHICS, GFXEVT_DEFENDER_REJECT, 2)
		.addI(GFXF_X, iX)
		.addI(GFXF_Y, iY)
		.addI(GFXF_UNIT_ID, iUnitId)
		.addI(GFXF_UNIT_TYPE, iUnitType)
		.addStr(GFXF_REJECT_REASON, szReason)
		.addI(GFXF_PREDICTED_HP, iPredictedHitPoints)
		.addI(GFXF_IS_DEAD, bIsDead ? 1 : 0)
		.addI(GFXF_OWNER_FILTER, iOwnerFilter)
		.addI(GFXF_UNIT_OWNER, iUnitOwner));
}
