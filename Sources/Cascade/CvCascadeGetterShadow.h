#pragma once
#ifndef CV_CASCADE_GETTER_SHADOW_H
#define CV_CASCADE_GETTER_SHADOW_H

//	CvCascadeGetterShadow -- the #430 GETTER-CONTRACT net (cutover.md §Owner rulings 2026-07-02, item 5).
//
//	FLIPPED (increment C, modifier-substrate.md): CvCity::getYieldRate100 / getCommerceRateTimes100 RETURN the
//	ACCUMULATOR's value in a running game, with the legacy expression kept in-body as the oracle. This module is
//	the [GETTER] NET: the flipped getter hands in BOTH values and this counts/samples the slot-vs-legacy diff --
//	no compute happens here (the [SLOT] sweep separately verifies slot-vs-CALCULATOR; this net carries the
//	standing accepted-class residue vs legacy plus anything new).
//
//	Discipline: gated on gPlayerLogLevel (free when logging is off); ONE count per (city, plane, channel) per
//	turn; per-turn [GETTER/shadow] checked/diverging summary + capped [GETTER/diff] samples, flushed lazily on
//	the first call of the next turn.

class CvCity;

void cascadeGetterShadowYield(const CvCity* pCity, int iYield, int iLegacy100, int iSlot100);
void cascadeGetterShadowCommerce(const CvCity* pCity, int iCommerce, int iLegacy100, int iSlot100);

#endif // CV_CASCADE_GETTER_SHADOW_H
