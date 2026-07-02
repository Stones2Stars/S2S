#pragma once
#ifndef CV_CASCADE_GETTER_SHADOW_H
#define CV_CASCADE_GETTER_SHADOW_H

//	CvCascadeGetterShadow -- the #430 GETTER-CONTRACT net (cutover.md §Owner rulings 2026-07-02, item 5).
//
//	FLIPPED (2026-07-02, owner: "lets flip ... to see what happens"): CvCity::getYieldRate100 /
//	getCommerceRateTimes100 now RETURN the cascade value (CascadeRates) in a running game, with the legacy
//	expression kept in-body as the oracle. This module is the NET: the flipped getter hands in BOTH values and
//	this counts/samples the diff -- no compute happens here anymore (the old instrument computed the cascade
//	side itself, capped per turn; the flip made the cascade value the return, so the net is a plain compare).
//
//	Discipline: gated on gPlayerLogLevel (free when logging is off); ONE count per (city, plane, channel) per
//	turn; per-turn [GETTER/shadow] checked/diverging summary + capped [GETTER/diff] samples, flushed lazily on
//	the first call of the next turn.

class CvCity;

void cascadeGetterNetYield(const CvCity* pCity, int iYield, long lCascade, int iLegacy100);
void cascadeGetterNetCommerce(const CvCity* pCity, int iCommerce, long lCascade, int iLegacy100);

#endif // CV_CASCADE_GETTER_SHADOW_H
