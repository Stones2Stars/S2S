#pragma once
#ifndef CV_CASCADE_MODIFIER_H
#define CV_CASCADE_MODIFIER_H

//
//	CvCascadeModifier -- the MAGNITUDE machine's combine core (modifier-cascade-spec.md). The tally sums COUNTS and
//	rolls UP the scope spine; the modifier sums MAGNITUDES and flows DOWN it. This is the per-target accumulation
//	slot: the additive/multiplicative bucket that all deposits to one (family, member, target, unit) fold into,
//	plus the effective-value read. The family taxonomy, the scope deposit-flow, and the per-deposit `enabled`/
//	`disabled` + `per` conditioning are the data-driven layers built on top (fed by readJson).
//

// A modifier UNIT names what the value IS, not how it combines (data-model-spec §2.3):
//	flat       -- an additive amount (sums into base)
//	percent    -- an additive percent delta, +50% == 50 (summed, applied once)
//	multiplier -- a true ×factor, identity 100, ×2 == 200 (composed by product)
enum ModifierUnit
{
	MODUNIT_FLAT = 0,
	MODUNIT_PERCENT,
	MODUNIT_MULTIPLIER
};

// One accumulation slot. effective(base) = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100).
// (modifier-spec §2 -- the exact arithmetic/ordering & combine modes pin at #430; this is the default sum/product.)
struct CvModifierSlot
{
	int iFlat;           // Σ flat
	int iPercent;        // Σ percent (additive deltas)
	int iMultiplierX100; // Π(multiplier/100), stored ×100 (identity 100)

	CvModifierSlot() : iFlat(0), iPercent(0), iMultiplierX100(100) {}

	void deposit(ModifierUnit eUnit, int iValue);
	int  effective(int iBase) const;
	void clear() { iFlat = 0; iPercent = 0; iMultiplierX100 = 100; }
	bool isIdentity() const { return iFlat == 0 && iPercent == 0 && iMultiplierX100 == 100; }
};

#endif // CV_CASCADE_MODIFIER_H
