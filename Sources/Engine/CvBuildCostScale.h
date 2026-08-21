#pragma once

#ifndef CV_BUILD_COST_SCALE_H
#define CV_BUILD_COST_SCALE_H

//
//	CvBuildCostScale -- the ONE consuming-system calc for "what do this entity's OWN authored data and the live
//	GAME OPTIONS do to its build cost".
//
//	An info serves its authored `cost` section and NOTHING else: an info never reads game state, so an
//	option-gated composition cannot live there (json.md §9 -- a game option gates AT THE CONSUMING SYSTEM). This
//	class IS that consuming system, the sibling of CvGameSpeedScale, which already owns the other half of the
//	same calculation (the gamespeed/upscaled-cost pace percent).
//
//	It covers BOTH build domains, because they are one question asked of two entity kinds: a BUILDING composes
//	its authored cost bands under GAMEOPTION_REALISTIC_BUILDING_COST, and a UNIT composes the Size-Matters
//	production pace under GAMEOPTION_COMBAT_SIZE_MATTERS. Neither belongs at a call site
//	(docs/architecture/patterns.md §DRY (single implementation)).
//
//	⚑ THE THREE COST PLANES stay separate (json.md §6): the entity's own AUTHORED cost is the `cost` section; what
//	CHANGES a cost is the `costs` MODIFIER family; the derived PRICE is engine-computed from the two. This class is
//	the third plane's building half, and it composes ONLY plane 1 with its game option -- it is not a place to
//	fold `costs` deposits, which the cascade already carries.
//
//	⚠ EVERY method returns a PERCENT (100 = the authored cost unmodified), matching CvGameSpeedScale's contract,
//	so a caller multiplies and divides by 100 once. A PERCENT IS NOT SCALED (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

class CvBuildingInfo;
class CvUnitInfo;

class CvBuildCostScale
{
public:
	//	The REALISTIC-BUILDING-COST composition: a building's size / count / materials / complexity bands read as
	//	one percent, while GAMEOPTION_REALISTIC_BUILDING_COST is live (100 = no change, and 100 whenever the option
	//	is off, so a caller needs no branch of its own).
	//
	//	⚠ INTEGER, deliberately. The inline derivation this replaces accumulated a `float` multiplier
	//	(`1.0f`, `-= 0.2f`, `iBaseCost * totalModifier`) on a path every build decision runs. Civ4 multiplayer is
	//	deterministic lockstep and CPU-dependent float math desyncs (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model): all engine math is
	//	integer, no float), so carrying it as whole percent points removes a real OOS hazard. The bands are all
	//	multiples of 5%, so nothing is lost in the conversion and the resulting cost is unchanged.
	//
	//	⚠ The band values are the AUTHORED enum-ish codes, not magnitudes: each field is a small band index where
	//	1 (or an unauthored field) is the neutral middle. That is why this reads them by equality rather than
	//	scaling by them.
	static int buildingCostPercent(const CvBuildingInfo& kBuilding);

	//	The UNIT training-pace percent: which of the two authored production paces a unit is trained at
	//	(`UNIT_PRODUCTION_PERCENT` normally, `UNIT_PRODUCTION_PERCENT_SM` while Size Matters is live), with the
	//	MERCHANT carve-out applied. 100 leaves the authored cost unchanged, so a caller needs no branch.
	//
	//	⚠ The carve-out is a TAG read, which is what tags are FOR ([modifier.md §6](../../docs/specs/modifier.md):
	//	the "what" is always a tag predicate). Trade units are exempted because the SM pace is a discount for
	//	units that MERGE, and a unit that cannot merge would simply be cheaper for nothing -- so the question is
	//	"is this a merchant", and a merchant is one whether the class arrives as its primary or as a sub
	//	([tags.md](../../docs/specs/tags.md): a unit's effective tags are its own ∪ its combat classes').
	//
	//	⚠ SIZE-MATTERS RANK IS NOT AN INPUT HERE, deliberately. A rank changes HOW MANY units' worth is being
	//	built, never the pace each is built at, so a future ranked build is a QUANTITY term at the order rather
	//	than a term in this percent -- which is why taking a rank parameter now would be modelling it in the
	//	wrong place ([json.md §9](../../docs/specs/json.md): the rank is a count axis, `count / 3^(groupRank-1)`).
	static int unitProductionPercent(const CvUnitInfo& kUnit);

private:
	CvBuildCostScale();                                    // never instantiated
	CvBuildCostScale(const CvBuildCostScale&);
	CvBuildCostScale& operator=(const CvBuildCostScale&);
};

#endif // CV_BUILD_COST_SCALE_H
