#pragma once
#ifndef CV_CASCADE_ENABLER_H
#define CV_CASCADE_ENABLER_H

//
//	CvCascadeEnabler -- the #430 "can I?" machine (enabler.md §1: GENERATE, then GATE). It reads the JSON data mapped
//	into the per-type InfoRepo (`enables`/`obsoletes`/`replaces`/`disables` edges + the `requires` BoolExpr trees +
//	the `allowed` caps) and the live HAVE state, and produces the availability frontier.
//
//	FIRST CUT (this file): per city, emit the **canConstruct** (buildings) + **canTrain** (units) pointer vectors --
//	   1. GENERATE  CAN GET = union(`enables`.{buildings,units}) over what the player/city HAS (team techs + city
//	      buildings + adopted civics), minus (`obsoletes` ∪ `replaces` ∪ `disables`).
//	   2. GATE      each candidate by its `requires` (BoolExpr evaluated against the city's game object) + its
//	      `allowed` cap (count from the tally).
//	The two passes are kept DISTINCT (not a per-entity output-match shortcut -- the spec-divergence trap,
//	DEC-stonebase-follows-spec). Shadowed vs the legacy `CvCity::canConstruct` / `canTrain` (the oracle), driven to
//	parity, then extended to the other gates (canAcquirePromotion / canResearch / …) on the same machine.
//
//	Names mirror the existing engine gates (canConstruct / canTrain) so the output + shadow align with no translation.
//

//	One-shot, gated (gPlayerLogLevel) per-turn shadow at doTurn: for a sample of cities, compute the cascade frontier
//	(GENERATE->GATE) and diff its canConstruct/canTrain verdict against the live engine per building/unit, emitting
//	[ENABLER/shadow] + [ENABLER/diff] lines.
void cvCascadeEnablerShadow();

#endif // CV_CASCADE_ENABLER_H
