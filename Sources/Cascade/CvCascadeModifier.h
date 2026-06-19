#pragma once
#ifndef CV_CASCADE_MODIFIER_H
#define CV_CASCADE_MODIFIER_H

#include <vector>
#include "CvCascadeCondition.h" // CvCascadeCondition -- the per-deposit enabled/disabled gate (reused verbatim)

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

// ===================== the DATA-DRIVEN layer (fed by readJson) =====================
// The target scope a deposit lands at (the JSON `<scope>` axis) = the CONTAINMENT SPINE, high -> low (enabler-spec §8 /
// tally-spec §1). Encoded COMPLETE so the scope model has NO gaps to rediscover (owner 2026-06-19 -- build the proper
// structure once; the spine is a fixed, known model, so this is not speculation). The parser + deposit-flow CONSUME each
// scope as the data + families need it; the PILOT consumes CITY + PLOT (other scopes parsed + tagged-pending meanwhile).
enum ModifierScope
{
	MODSCOPE_WORLD = 0,   // game-wide (all players)
	MODSCOPE_TEAM,        // the player's TEAM -- shared across members (e.g. a wonder boosting the whole team's research)
	MODSCOPE_EMPIRE,      // the player
	MODSCOPE_AREA,        // the landmass / continent the city sits on
	MODSCOPE_CITY,        // the city's total output
	MODSCOPE_PLOT,        // a worked TILE's yield. Plots FEED the city's base yield AND are themselves modifiable by
	                      // outside sources (buildings/civics/terrain-feature-improvement yield changes) -- a FIRST-CLASS
	                      // scope (owner 2026-06-19). ENCAPSULATION: the plot SELF-CONTAINS its modifier detail and
	                      // reports ONE rolled-up yield to the city -- "yep, this is what you get from me this turn"; the
	                      // city NEVER sees which plot got buffed by which improvement (same report-isolation as the
	                      // tally). Keyed plot sub-targets (terrains/features/improvements/routes) resolve INSIDE the plot.
	MODSCOPE_SELF,        // the entity's own build cost (buildRate.self) -- not a city-output scope
	MODSCOPE_SPECIALIST,  // a specialist's output
	MODSCOPE_UNIT,        // a unit's stat (the unit-plane channel -- largest surface, last per the shadow-spec §2.2)
	NUM_MODIFIER_SCOPES
};

// One parsed modifier deposit (the in-memory form of a `<family>.<scope>.<unit> = value [+ enabled/disabled]` leaf).
// PILOT: city-scope yield flat/percent. The value folds into a CvModifierSlot per (family, scope, target) WHEN its
// `enabled` holds AND its `disabled` doesn't (re-evaluated per turn -- the dormancy model). iFamily is stored as a raw
// int so this header needs no enum coupling: the parser maps "food"/"production"/"commerce" -> YieldTypes (taxonomy
// widens to commerce-split / health/happiness / PROPERTY_* / unit stats later).
struct CvCascadeModifierDeposit
{
	int                iFamily; // PILOT: YieldTypes (YIELD_FOOD/PRODUCTION/COMMERCE)
	int                iScope;  // ModifierScope
	ModifierUnit       eUnit;   // MODUNIT_FLAT / MODUNIT_PERCENT (yields author no multipliers)
	int                iValue;  // magnitude
	CvCascadeCondition enabled; // empty = always on
	CvCascadeCondition disabled;// empty = never off

	CvCascadeModifierDeposit() : iFamily(-1), iScope(MODSCOPE_CITY), eUnit(MODUNIT_FLAT), iValue(0) {}
};

// One entity's parsed modifier deposits (readJson populates this; the deposit-flow folds them into per-target slots).
struct CvEntityModifiers
{
	std::vector<CvCascadeModifierDeposit> deposits;
	int iParsed;  // deposits wired (diagnostics)
	int iSkipped; // leaves dropped/unmodelled this pass (diagnostics)
	CvEntityModifiers() : iParsed(0), iSkipped(0) {}
};

// ===================== the DEPOSIT-FLOW + effective read (the data-driven layer's runtime) =====================

// Parity-first scaffold (R-M1, modifier-cascade-shadow-spec §6): a build-time const for now. When true, MULTIPLIER deposits
// are treated as identity (skipped) so the engine is ADDITIVE-ONLY -- matching legacy -- to prove the deposit-flow wiring
// before the new multiplier capability is enabled. (Yields author no multipliers, so it's a no-op for the pilot; the flag
// is the framework hook.) Promote to a BUG option only if live toggling is wanted.
extern const bool cascadeModifierParityMode;

// Build a city's modifier slot for one family at MODSCOPE_CITY: deposit each PRESENT building's city-scope flat/percent
// whose `enabled` holds and `disabled` doesn't (re-evaluated against kCtx -- the dormancy model). iFamily is a YieldTypes
// for the pilot. PILOT scope; widens to plot (the plot self-reports, owner 2026-06-19) + other scopes/families later.
void cascadeModifierCitySlot(int iFamily, const CvCascadeContext& kCtx, CvModifierSlot& slotOut);

// The effective city-scope value for (family, ctx): slot.effective(getBaseYieldRate). PILOT scope = MODSCOPE_CITY (other
// scopes return 0 for now). The single read the shadow + the per-entity endpoint compare against legacy getYieldRate100.
int cascadeModifierEffective(int iFamily, int iScope, const CvCascadeContext& kCtx);

#endif // CV_CASCADE_MODIFIER_H
