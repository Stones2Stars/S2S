#pragma once
#ifndef CV_CASCADE_GETTER_SHADOW_H
#define CV_CASCADE_GETTER_SHADOW_H

//	CvCascadeGetterShadow -- the #430 GETTER-CONTRACT instrumentation (cutover.md §Owner rulings 2026-07-02, item 5).
//
//	The getters are the stable CONTRACTS the cut goes through: each legacy getter the cascade replaces gets an
//	event-spine "cascadeValue" diff INSIDE the body -- the cascade's answer vs the value the getter is about to
//	return, AT THE REAL CALL MOMENT (validation.md: shadowed in the AI's real per-turn calls). At clean parity the
//	getter BODY flips to return the cascade value and the legacy accumulator behind it is deleted; consumers are
//	never rewired.
//
//	Discipline (these getters are hot paths):
//	 - gated on gPlayerLogLevel (a single int compare when logging is off);
//	 - ONE compare per (city, plane, channel) per turn -- the first real call wins, later calls memo out. Full-city
//	   coverage (unlike the doTurn shadow's per-player sample), bounded by a per-turn compute cap;
//	 - reentrancy-guarded: a cascade-internal read of an instrumented getter never recurses into a compare;
//	 - per-turn [GETTER/shadow] checked/diverging summary + capped [GETTER/diff] samples, flushed lazily on the
//	   first call of the next turn (no engine doTurn hook needed).
//
//	Instrumented today: CvCity::getYieldRate100 (plane 0, YieldTypes) + CvCity::getCommerceRateTimes100 (plane 1,
//	CommerceTypes). Extend per getter as its cascade counterpart lands (int params so this header stays
//	dependency-free; C++03 cannot forward-declare enums).

class CvCity;

void cascadeGetterShadowYield(const CvCity* pCity, int iYield, int iLegacy100);
void cascadeGetterShadowCommerce(const CvCity* pCity, int iCommerce, int iLegacy100);

#endif // CV_CASCADE_GETTER_SHADOW_H
