#pragma once
#ifndef CV_CASCADE_MODIFIER_MATH_H
#define CV_CASCADE_MODIFIER_MATH_H

//
//	The #430 MODIFIER machine -- INCREMENT 1: the percent stack (modifier.md §2a "how the percentages smash together").
//	Reads the mapped CvCascadeData (the deposits readJson attached per game object) + the live active-source sets, and
//	SHADOWS the cascade's per-channel `modifier = max(0, 100 + Σ percent)` against the legacy
//	`CvCity::getBaseYieldRateModifier`. Build plan: docs/plans/structural-cleanup/modifier-machine.md.
//

//	One-shot, gated (gPlayerLogLevel) per-turn shadow: for a sample of cities × {food,production,commerce}, diff the
//	cascade percent stack vs legacy getBaseYieldRateModifier and emit [MODIFIER/shadow] + [MODIFIER/diff] lines. Hooked
//	at CvGame::doTurn AFTER cascadeReadJsonProbe (which maps the data it reads).
void cvCascadeModifierShadow();

#endif // CV_CASCADE_MODIFIER_MATH_H
