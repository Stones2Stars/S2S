#pragma once

#ifndef CV_SIZE_MATTERS_RANK_H
#define CV_SIZE_MATTERS_RANK_H

//
//	CvSizeMattersRank -- the ONE home for "how far above its own base may this unit be ranked up, and what does
//	that cost".
//
//	⛔ A RANK-UP IS AN OFFSET FROM THE UNIT'S OWN BASE (`base + x`), NEVER AN ABSOLUTE RANK (owner).  A base
//	group rank is DERIVED per unit at load from its combat classes (json.md §9), so an absolute number means a
//	different thing for every unit -- and a DOWNGRADE for one whose base already exceeds it -- while an offset
//	stays correct when re-tagging a combat class moves that base.  The engine already agrees: its merge ceiling
//	was written in exactly this form long before the build side wanted it.
//
//	⚑ WHY THIS EXISTS AT ALL -- the merge GRIND, not the cost (owner).  Merging hundreds of units by hand in the
//	late game is the problem; building at `base + x` is the shortcut past it.  That framing is what fixes the
//	price: the shortcut has to come out EQUIVALENT to building `3^x` units and merging them, or it is a trap or
//	an exploit.  So the cost is not a free design choice, it is the equivalence.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

class CvUnitInfo;

class CvSizeMattersRank
{
public:
	//	The ceiling on a unit's GROUP rank: its own derived base plus what the owner's ERA allows.  The era is
	//	what decides how many merges are reachable, so it is the bound on `x`.
	//
	//	⚠ Takes the era as a plain value rather than a player, so the BUILD side can ask before a unit exists --
	//	which is the whole reason this left `CvUnit`.  A per-instance-only rule cannot answer a build menu.
	static int mergeLimit(const CvUnitInfo& kUnit, int iEra);

	//	How many rank-ups are available to offer: `mergeLimit - base`, floored at 0.  This is the range a build
	//	order's `x` is picked from.
	static int maxRankUps(const CvUnitInfo& kUnit, int iEra);

	//	What building at `base + x` costs, as a MULTIPLE of the unit's ordinary cost (1 = unchanged).
	//
	//	⚑ It is the EQUIVALENCE, derived rather than chosen: a unit at group rank g counts as `3^(g-1)` units
	//	(json.md §9), so base -> base+x is exactly `3^x` units' worth.  Expressed as the ratio of the ONE
	//	geometry derivation (`smGroupMultiplier`) rather than by restating the 3, so it cannot drift from the
	//	count side if that geometry ever changes ([DEC-single-implementation]).
	//
	//	⚠ This is a QUANTITY, deliberately not a percent, and NOT a term in CvBuildCostScale: that class answers
	//	the PACE each unit is built at, which a rank-up does not change.  The Size-Matters pace discount still
	//	applies on top, because a directly-built ranked unit IS the merged result and that discount exists
	//	precisely because units merge.
	static int rankUpCostMultiplier(const CvUnitInfo& kUnit, int iRankUps);

private:
	CvSizeMattersRank();                                       // never instantiated
	CvSizeMattersRank(const CvSizeMattersRank&);
	CvSizeMattersRank& operator=(const CvSizeMattersRank&);
};

#endif // CV_SIZE_MATTERS_RANK_H
