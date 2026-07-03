#pragma once
#ifndef CV_CASCADE_WELLBEING_H
#define CV_CASCADE_WELLBEING_H

//
//	CascadeWellbeing -- the #430 modifier machine WELLBEING channel (modifier.md §2b): the city's four realized
//	health/happiness verdicts computed from the curated deposit data + the raw-state inputs, transcribed from the
//	StoneBase assembler that reached attributed parity (WellbeingLevels.cs -- the source-completeness proof; the
//	remaining legacy divergence classes are the improvement BALANCE-CUT and the STORED-ACCUMULATOR DRIFT, both
//	documented in modifier.md §2b as engine-wrong/attributed-accepted).
//
//	Calculator-first (the substrate discipline): compute() is the fresh full recompute; the [MODIFIER/wellbeing]
//	shadow (cvCascadeWellbeingShadow, hooked beside the modifier shadow) diffs it against the legacy verdicts
//	each turn. Slot storage + the getter flip follow once the shadow's residue is the two accepted classes only.
//

#include "CvCascadeConditionEval.h"

class CvCity;

struct CascadeWellbeingVerdicts
{
	// ⚠ MILITARY-FREE verdicts (owner ruling 2026-07-03): the unit-count happiness NEVER enters the cached
	// computation -- it rides ALONE on top at read time (perUnit × the live O(1) engine counter), so unit
	// moves invalidate nothing. iMilPerUnit is the epoch-stable civic/trait per-military-unit VALUE.
	int iHappy;      // happyLevel WITHOUT the military term
	int iUnhappy;    // unhappyLevel WITHOUT the military term
	int iGood;       // goodHealth (no military term exists)
	int iBad;        // badHealth (no military term exists)
	int iMilPerUnit; // Σ civic/trait perMilitaryUnit values (× the live count at read)
	CascadeWellbeingVerdicts() : iHappy(0), iUnhappy(0), iGood(0), iBad(0), iMilPerUnit(0) {}
};

class CascadeWellbeing
{
public:
	// The four verdicts from CURRENT state: deposit-computed terms (buildings/civics/traits/bonuses/corps/techs/
	// projects/specialists via the compiled deposit index) + raw-state inputs (anger percents, timers, gate
	// flags, the extra/religion accumulators) folded exactly where the engine folds them.
	static CascadeWellbeingVerdicts compute(const CvCity* pCity, const CvCascadeEvalCtx& ec);
};
// The per-turn [MODIFIER/wellbeing] shadow lives in the modifier shadow harness (CvCascadeModifierMath.cpp,
// inside cvCascadeModifierShadow's city loop) -- it rides the registered [MODIFIER] spine domain.

#endif // CV_CASCADE_WELLBEING_H
