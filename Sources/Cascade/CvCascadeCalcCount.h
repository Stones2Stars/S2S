#pragma once
#ifndef CV_CASCADE_CALC_COUNT_H
#define CV_CASCADE_CALC_COUNT_H

//
//	CascadeCalcCount -- the (scope, channel) calculation counter ([DEC-calc-count-gate]): every package-slot
//	rebuild logs its (scope, channel), and the per-turn totals are the standing acceptance gate + regression
//	tripwire (steady-state tracks EVENT volume, thousands; >50k/turn is near-certainly a blanket recompute; a
//	quiet turn approaches zero). Minimal + clean: counters here, ONE [MODIFIER] report line per scope per turn
//	(+ per-channel detail at level 2) through the registered SD_MODIFIER render path, flushed + reset at the
//	game-turn boundary by the modifier consumer.
//
//	Purely-organizational static-methods class (patterns.md); game-thread only; never serialized.
//

#include "CvCondition.h"   // CvCascScope

class CascadeCalcCount
{
public:
	// One package slot (scope, channel) was recomputed. Called by the gather per refreshed channel.
	static void count(CvCascScope eScope, int iChannel);
	// A receiver sum slot was recomputed.
	static void countSum(CvCascScope eScope, int iChannel);
	// Emit the per-turn report ([MODIFIER] calcCount ...) and reset. Called at the game-scope TURN_ENDED.
	static void reportAndReset();
	// Emit the per-scope CHANNEL-SET census ([MODIFIER] channels scope=... authored=... slots=... receivers=...)
	// -- the KEYS-ONLY-WHERE-NEEDED derivation made observable (the measured expectation: plot 13 / city 40 /
	// empire 50 / area 3 / team 3 authored channels). Once per load (guarded); called at GAME_LOAD_FINISHED.
	static void reportChannelCensus();
};

#endif // CV_CASCADE_CALC_COUNT_H
