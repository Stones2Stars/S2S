#pragma once
#ifndef CV_CASCADE_MODIFIER_H
#define CV_CASCADE_MODIFIER_H

#include <vector>
#include "CvCascadeCondition.h" // CvCascadeCondition -- the per-deposit enabled/disabled gate (reused verbatim)

class CvCity; // cascadeModifierCityBase takes a const CvCity* (pointer param -- forward-decl, no include)

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

// Build a city's modifier slot for one family at MODSCOPE_CITY: fold the city's PRESENT BUILDINGS' city-scope deposits +
// the player's active CIVICS' empire-scope deposits (empire = the player = all cities -- the empire->city roll-down),
// each whose `enabled` holds and `disabled` doesn't (re-evaluated against kCtx -- the dormancy model). iFamily is a
// YieldTypes for the pilot. PILOT; widens to plot + trait/tech/building-empire sources + other families later.
void cascadeModifierCitySlot(int iFamily, const CvCascadeContext& kCtx, CvModifierSlot& slotOut);

// PER-SOURCE decomposition of the city slot (the observability twin of cascadeModifierCitySlot): runs the SAME
// active-building (city) + active-civic (empire) loops, but captures each contributing entity's own slot separately,
// so the diagnostic dump can set the cascade's deposits source-by-source against the legacy per-source decomposition
// ("know what we mirror before extending the cascade" -- owner ruling 2026-06-20). Only non-identity contributors are
// returned. bCivic distinguishes the entity id (BuildingTypes when false, CivicTypes when true).
struct CvModifierSourceContribution
{
	int iEntity;
	bool bCivic;
	CvModifierSlot slot;
};
void cascadeModifierCitySources(int iFamily, const CvCascadeContext& kCtx, std::vector<CvModifierSourceContribution>& out);

// The pre-modifier BASE the cascade applies the city slot to: legacy getYieldRate100 multiplies (getBaseYieldRate +
// getSpecialistYieldTotal) by the modifier (CvCity.cpp:11253 -- specialist yield gets the city modifier like worked
// tiles), so the cascade base matches that pair. Shadow stand-in (the eventual model computes specialist output from
// MODSCOPE_SPECIALIST deposits). ONE definition -- the shadow + both endpoints use it so the base can't drift.
int cascadeModifierCityBase(const CvCity* pCity, int iFamily);

// The CALCULATION FLOW -- HOW a built slot folds into a base. Kept as ONE named dispatch point so the flow is EASILY
// MODIFIED / SWAPPED later (owner 2026-06-19): add or change a flow = add an enum value + a case in cascadeModifierApply;
// nothing else in the engine changes. (C++03 poor-man's strategy -- the if/switch composition root AGENTS.md prescribes,
// not a virtual hierarchy.) The flow is a deliberate MODEL decision the shadow surfaced (legacy's actual arithmetic was
// not fully known until measured -- owner 2026-06-19).
enum ModifierCalcFlow
{
	CALCFLOW_LEGACY_FLAT_OUTSIDE = 0, // base × (100+Σpercent)/100 + Σflat -- MATCHES LEGACY (building flat added AFTER the
	                                  // modifier: `(base)×modifier + extraYield`). The CURRENT flow: parity can reach zero,
	                                  // and the data values are balanced for it. (owner 2026-06-19)
	CALCFLOW_UNIFIED_FLAT_INSIDE,     // (base+Σflat) × (100+Σpercent)/100 × Π(mult/100) -- the spec's unified model
	                                  // (slot.effective). Multiplies every building flat by the city's yield % -> carries a
	                                  // full DATA REBALANCE, so DEFERRED; choosing legacy-flat now does NOT foreclose it.
	NUM_MODIFIER_CALC_FLOWS
};

// The ACTIVE calculation flow (build-time const for now; promote to a runtime/BUG switch if live swapping is wanted).
extern const ModifierCalcFlow cascadeModifierCalcFlow;

// Apply a built slot to a base via the active calculation flow (the single dispatch point above). Both the shadow and the
// per-entity endpoint compare THIS against legacy getYieldRate100.
int cascadeModifierApply(const CvModifierSlot& slot, int iBase);

// The effective city-scope value for (family, ctx): cascadeModifierApply(citySlot, getBaseYieldRate). PILOT scope =
// MODSCOPE_CITY (other scopes return 0 for now). The single read the shadow + the per-entity endpoint use.
int cascadeModifierEffective(int iFamily, int iScope, const CvCascadeContext& kCtx);

// ===================== the MODIFIER-FAMILY registry (ALL channels) =====================
// Every modifier family the cascade + shadow cover (modifier-spec §2.1 / data-model §4 / legacy-value-calc-map). A
// deposit's iFamily is one of these. The first three == YieldTypes (food/production/commerce) by VALUE, so the yield
// pilot ids are UNCHANGED. This is the structure that makes "all channels" first-class -- the engine is no longer
// yield-only; readJson tags every family, the sweep iterates every family, each via its combine mode below.
enum ModifierFamily
{
	MODFAM_FOOD = 0, MODFAM_PRODUCTION = 1, MODFAM_COMMERCE = 2, // == YIELD_FOOD/PRODUCTION/COMMERCE (the yield triple)
	MODFAM_GOLD, MODFAM_RESEARCH, MODFAM_CULTURE, MODFAM_ESPIONAGE, // the commerce split (data-model §4)
	MODFAM_HEALTH, MODFAM_HAPPINESS, MODFAM_DEFENSE, MODFAM_MAINTENANCE, MODFAM_GREATPEOPLE,
	NUM_MODIFIER_FAMILIES
};

// How a family's deposits fold onto its base (modifier-spec §7 -- combine is family metadata, not the per-value unit):
enum ModifierCombine
{
	MODCOMBINE_YIELD = 0, // base × (100+Σ%)/100 + Σflat (legacy-flat-outside) -- the 3 yields
	MODCOMBINE_ADDITIVE,  // Σflat (signed) -- the additive ledgers (commerce-split base, health/happiness/defense)
	MODCOMBINE_BASExMOD,  // base × (100+Σ%)/100 -- great-people (base × totalModifier/100)
	MODCOMBINE_COST       // cost-asymmetric -- maintenance/upkeep (the getModifiedIntValue hub)
};

// A registry row: the family's JSON key + its combine mode. Out-of-range -> a {"?", additive} sentinel.
struct ModifierFamilyInfo { const char* szKey; ModifierCombine eCombine; };
const ModifierFamilyInfo& cascadeModifierFamilyInfo(int iFamily);

// The ALL-CHANNEL SHADOW entry: for (city, family) at city scope, build the cascade slot from the migrated deposits,
// resolve the family's pre-modifier base, apply its combine mode -> iCascade, and read the legacy REALIZED value ->
// iLegacy (both same x1 scale). ONE definition the sweep + the per-city endpoint share so they can't drift. A family
// with no migrated deposits reconstructs to its base only -> the divergence is the map-before-delete "missingDeposit"
// signal (DEC-map-before-delete), the expected parity work the owner adjudicates -- never a fabricated clean.
void cascadeModifierFamilyShadow(const CvCity* pCity, const CvCascadeContext& kCtx, int iFamily,
	CvModifierSlot& slotOut, int& iBaseOut, int& iCascadeOut, int& iLegacyOut);

// ===================== the SHADOW classifier (cause-tag + care level) =====================

// The CARE SCALE -- Fine -> Meltdown (modifier-cascade-shadow-spec §4). ONE WORD each, chosen so the name ALONE
// conveys severity + the implied action to an agent reading it cold (no table lookup): Fine->ignore, Rounding->accept,
// Better->accept-as-a-win, Weird->investigate/ask, Bug->fix, Meltdown->stop-everything. The shadow auto-SUGGESTS a
// provisional rung from the cause-tag; the OWNER's verdict sets the final one (R-M3). Six rungs (0..5).
enum ModifierCareLevel
{
	CARE_FINE = 0,  // exact parity, or a diff blessed identical-enough -- ignore
	CARE_ROUNDING,  // cosmetic int-rounding / off-by-one within tolerance -- accept, note
	CARE_BETTER,    // deliberate correction (multiplier composition etc.) -- accept as a WIN, document
	CARE_WEIRD,     // unexplained divergence, cause not yet found -- investigate -> ASK the owner
	CARE_BUG,       // confirmed wiring bug (deposit missing / extra / mis-scoped) -- must-fix before cutover
	CARE_MELTDOWN,  // systemic -- whole channel garbage / overflow -- stop-the-line; DESPAIR_INDEX candidate
	NUM_MODIFIER_CARE_LEVELS
};

// The care rung's one-word name ("Fine".."Meltdown"). Out-of-range -> "?".
const char* cascadeModifierCareName(int iCare);

// Classify a city-yield divergence into a CAUSE-TAG (§3.4) + provisional CARE level (§4). ONE definition, used by
// BOTH /diagnostic/modifierSweep and the per-turn [MODSHADOW] line so they can't drift. iCascade / iLegacy are the
// realized per-turn yields in the SAME (x1) scale; slot carries the flat/percent/mult decomposition for localizing.
// Provisional only -- the owner confirms/overrides (R-M3). Returns the cause-tag string; writes the care into iCareOut.
const char* cascadeModifierClassify(int iCascade, int iLegacy, const CvModifierSlot& slot, int& iCareOut);

#endif // CV_CASCADE_MODIFIER_H
