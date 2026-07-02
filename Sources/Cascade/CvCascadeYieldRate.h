#pragma once
#ifndef CV_CASCADE_YIELD_RATE_H
#define CV_CASCADE_YIELD_RATE_H

//
//	YieldRate -- StoneBase YieldRate.cs: the §1 YIELD-RATE ASSEMBLER that combines the BASE/AFTER packages + the percent
//	stack into the final ×100 rate. See patterns.md (single-source law) + docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx -- the eval target for deposit conditions
#include <string>

class CvCity;

class YieldRate
{
public:
	// The §1 YIELD-RATE ASSEMBLER (YieldRate.cs / modifier.md §2a, calc-map §1.2):
	//   rate100 = min(CAP, max(100, (Σ BASE + specialist) × max(0,modifier) + 100·⌊AFTER100/100⌋))
	// All §1 BASE/AFTER terms now ported (PlotPackage + Specialist + trade + free-city + golden-age + building-flat); the
	// combine + clamp mirror StoneBase YieldRate.cs (the verified-final order). The holistic shadow (vs getYieldRate100)
	// follows; per StoneBase strategy (modifier-machine §0) parity is judged AFTER the whole calc is in.
	static long yieldRate100(const std::string& channel, YieldTypes eY, const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// Drop the turn-scoped rate memo (perf 2026-07-02). The doTurn SHADOW calls this first so its comparison is
	// temporally consistent -- the memo may hold values frozen from EARLY-turn calls (the getter instrument), and
	// comparing those against END-of-turn legacy read as false divergences (memo skew).
	static void memoClear();
};

#endif // CV_CASCADE_YIELD_RATE_H
