#pragma once
#ifndef CV_CASCADE_MODIFIER_MATH_H
#define CV_CASCADE_MODIFIER_MATH_H

//
//	The #430 MODIFIER machine -- INCREMENT 1: the percent stack (modifier.md §2a "how the percentages smash together").
//	Reads the mapped CvJsonInfo (the deposits readJson mapped into the per-type InfoRepo) + the live active-source sets,
//	and SHADOWS the cascade's per-channel `modifier = max(0, 100 + Σ percent)` against the legacy
//	`CvCity::getBaseYieldRateModifier`. Build plan: docs/plans/structural-cleanup/modifier-machine.md.
//

//	One-shot, gated (gPlayerLogLevel) per-turn shadow: for a sample of cities × {food,production,commerce}, diff the
//	cascade percent stack vs legacy getBaseYieldRateModifier and emit [MODIFIER/shadow] + [MODIFIER/diff] lines. Hooked
//	at CvGame::doTurn; reads the JSON info mapped at LOAD (onFinalInitialized).
void cvCascadeModifierShadow();          // #430 DISCONNECTED -- the legacy diff; re-enable only to re-verify vs legacy
void cvCascadeModifierPerfCensus();      // the [MODIFIER/perf] census (ruled perf surface) -- called every turn

#endif // CV_CASCADE_MODIFIER_MATH_H
