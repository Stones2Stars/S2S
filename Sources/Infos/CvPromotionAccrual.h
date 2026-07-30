#pragma once
#ifndef CV_PROMOTION_ACCRUAL_H
#define CV_PROMOTION_ACCRUAL_H

//
//	CvPromotionAccrual -- the ONE implementation of "what does a UNIT actually get from this promotion", i.e.
//	the sum down its promotion LINE.
//
//	THE TWO READS, and why both exist
//	---------------------------------
//	  * the promotion's OWN getters (CvPromotionInfo::getFlatCombat(...) etc.) -- what THIS RUNG contributes.
//	    That is what the pedia says ABOUT a promotion, and it is the honest answer there.
//	  * this surface -- what a unit HOLDING it has, compounded down the line. A promotion line is a LADDER and
//	    holding a rung implies the rungs beneath it (each level's `requires.build` names the level below:
//	    ACCURACY3 -> ACCURACY2 -> ACCURACY), so the unit genuinely carries the whole chain and the compounded
//	    figure is the one that matches its actual stats.
//	⛔ Neither read approximates the other, and a consumer picking the wrong one shows a number the unit does
//	not have. Ask which QUESTION is being answered: about the promotion, or about the unit holding it.
//
//	A purely-organizational static-methods class: no data members, never instantiated, and a static class rather
//	than a namespace (VC7.1 / Boost / closed-EXE ABI mangling) -- [DEC-single-implementation]. The membership it
//	sums over is materialized at load (CvPromotionInfo::getLineAccrual), so this walks a handful of ids and never
//	the registry.
//
//	⚠ It sums the UNCONDITIONED compiled slot of each rung, which is the same thing the per-rung point getters
//	return. A CONDITIONED deposit is not folded here -- it resolves against a live context through the ONE
//	evaluator, which is the valuation's job, not a static line sum.
//

#include "CvInfoKinds.h"    // ModifierFamily / CvCascUnit
#include "CvCondition.h"    // CvCascScope

class CvPromotionInfo;
class CvClassificationBlock;

class CvPromotionAccrual
{
public:
	// The compounded magnitude for one (family, kind, scope, unit) slot: this promotion plus every lower rung.
	static int sum(const CvPromotionInfo& kPromotion, ModifierFamily eFamily, int iKind,
	               CvCascScope eScope, CvCascUnit eUnit);

	// The compounded SKILL verdict: does ANY rung in the accrual carry it? Takes one of the CvSkillReads
	// functions, so the key vocabulary stays in its own single home rather than being duplicated here.
	typedef bool (*SkillRead)(const CvClassificationBlock* skills);
	static bool skill(const CvPromotionInfo& kPromotion, SkillRead fnRead);

private:
	CvPromotionAccrual();                                        // organization only -- never instantiated
	CvPromotionAccrual(const CvPromotionAccrual&);
	CvPromotionAccrual& operator=(const CvPromotionAccrual&);
};

#endif // CV_PROMOTION_ACCRUAL_H
