#pragma once
#ifndef CV_GRAPHICS_TRACE_H
#define CV_GRAPHICS_TRACE_H

//
//	CvGraphicsTrace -- the [GFX] spine domain: what the SCENE is asked to hold.
//
//	Two questions live here, and they are the same question at two scopes:
//
//	  1. WHICH unit a plot presents (`gfxTraceCenterUnit`). CvPlot::updateCenterUnit is gated three ways -- the
//	     inhibit flag, isGraphicsVisible(UNIT), and isActiveVisible() -- and every one of them can leave a plot
//	     holding no centre unit at all. The gates are invisible from outside, so a plot that never updates is
//	     indistinguishable from one that had nothing to update.
//	  2. HOW MANY unit entities are REAL rather than the shared dummy (`gfxTraceEntity`). A real entity is a
//	     scene node, and a scene node is where the EXE's per-instance model/texture memory goes
//	     (memory-footprint.md) -- so this count IS the memory question, asked at the moment it moves.
//
//	⛔ KIND is DIAGNOSTIC, always. These say CODE RAN and what it decided, never what the state IS
//	(event-spine.md § THE RECEIVED LINE: the test is "does the fact say what the STATE is, or what some CODE
//	did?"). No consumer may build state from one, which is what keeps this a lens rather than an input.
//
//	The domain self-registers on first emit, so adding a third emitter touches this file and nothing else.
//

class CvUnit;

//	The plot's centre-unit verdict. iOldUnitId / iNewUnitId are INSTANCE ids (-1 = none), not type indices.
//	iGate names WHICH gate ended it, so a plot that declines to update says why rather than going quiet.
enum GfxCenterUnitGate
{
	GFX_GATE_NONE = 0,       // no gate fired -- the verdict below is a real decision
	GFX_GATE_INHIBITED,      // m_bInhibitCenterUnitCalculation -- recalculation deliberately suspended
	GFX_GATE_NOT_VISIBLE,    // !isGraphicsVisible(ECvPlotGraphics::UNIT) -- the plot draws no units at all
	GFX_GATE_NOT_ACTIVE_VIS  // isActiveVisible() false -- nothing is presented to the active team
};

void gfxTraceCenterUnit(int iX, int iY, GfxCenterUnitGate eGate, bool bActiveVisible,
                        int iOldUnitId, int iNewUnitId);

//	A unit entity was attached. bReal distinguishes a genuine scene node from the shared dummy; the two running
//	totals ride along so a reader sees the ratio move without correlating separate lines.
//	The three flags are bNeedsRealEntity's OWN inputs -- it is partly circular (a unit needs a real entity because
//	it IS the plot's centre unit, and the centre unit is chosen from units that have one), so the decision is only
//	readable with its terms beside it.
void gfxTraceEntity(const CvUnit* pUnit, bool bReal, int iNumReal, int iNumDummy,
                    bool bActiveVisible, bool bIsCenterUnit, bool bIsActivePlayer);

//	getBestDefenderExternal -- the read the EXE itself makes -- REFUSED a defender it had already found. Both
//	filters answer NULL, so the plot presents nothing and every unit on it draws. This is the line that says
//	WHICH filter refused, which a NULL return cannot.
void gfxTraceDefenderRefused(int iX, int iY, int iUnitId, bool bInViewport, bool bUsingDummy);

//	⛔ THE ANOMALY, NOT THE TRAFFIC: emitted ONLY where getBestDefender walked a NON-EMPTY unit list and still
//	returned nothing. getPreferredCenterUnit falls through five of these, so a plot presenting no unit is either
//	an EMPTY list or an all-zero SCAN -- and a bare NULL cannot tell those apart. `unitsSeen` settles it, and the
//	sample names one unit that was there and did not qualify.
//	⚑ It is gated this way on purpose: getBestDefender is called several times per centre-unit pass, so emitting
//	every call would be a firehose that buries the handful of lines that mean anything.
void gfxTraceDefenderScan(int iX, int iY, int iUnitsSeen, int iScoredPositive,
                          int iSampleUnitId, int iSampleUnitType, bool bSampleCanDefend,
                          int iOwnerFilter, int iAttackerFilter, bool bTestCanMove);

//	⛔ WHAT THE SCENE NODE WAS TOLD WHEN THE UNIT MOVED — the fact that decides whether the model follows.
//	CvUnit::setXY repositions the entity inside `GC.IsGraphicsInitialized() && isInViewport()`, so when that
//	gate is false NEITHER QueueMove NOR SetPosition runs and the node is never told the unit left: it keeps
//	rendering on the tile the unit came from. A bare "the unit moved" says nothing about which of the three
//	happened, and the three are indistinguishable from outside.
enum GfxMoveOutcome
{
	GFX_MOVE_QUEUED = 0,   // an animated move was pushed onto the scene node
	GFX_MOVE_TELEPORTED,   // repositioned with no animation
	GFX_MOVE_SKIPPED       // the graphics block did not run at all
};

void gfxTraceMove(int iFromX, int iFromY, int iToX, int iToY, GfxMoveOutcome eOutcome,
                  bool bGraphicsInitialized, bool bInViewport, bool bRealEntity,
                  bool bShow, bool bVisibleToWatchingHuman);

//	⛔ THE LEAF. getDefenderScore has exactly three early returns, and with pAttacker == NULL a score of 0 can come
//	from NOWHERE ELSE (defenderValue cannot return 0 without an attacker). This says WHICH of the three fired and
//	carries the values it decided on, so the cause stops being a matter of reasoning.
//	⚠ It is emitted only on the REJECT, never on the pass, so its volume is the fault's own volume.
void gfxTraceDefenderReject(int iX, int iY, int iUnitId, int iUnitType,
                            int iPredictedHitPoints, bool bIsDead,
                            int iOwnerFilter, int iUnitOwner);

#endif // CV_GRAPHICS_TRACE_H
