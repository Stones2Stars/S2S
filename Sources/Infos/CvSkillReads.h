#pragma once
#ifndef CV_SKILL_READS_H
#define CV_SKILL_READS_H

//
//	CvSkillReads -- the ONE shared surface for the json.md §8 unit-SKILL reads a consumer makes on a
//	PROMOTION / UNITCOMBAT / UNIT info.
//
//	A purely-organizational static-methods class: no data members, never instantiated. A static class
//	rather than a namespace, because a namespace risks name-mangling under the frozen VC7.1 / Boost /
//	closed-EXE ABI (DEC-single-implementation).
//
//	An info exposes only the parameterized group read getSkills(); a consumer asks for a key HERE, and
//	this surface holds the memoized generated id. The ClassificationRegistry mints the SKILL_* ids at
//	LOAD from the union of authored keys, so there is no compile-time id a caller could pass -- which is
//	the whole reason the reads are per-key rather than parameterized.
//
//	⚑ The per-key form is TRANSITIONAL by ruling (patterns.md § THE GETTER SETUP): the parameterized read
//	is the destination, blocked on an id vocabulary the open registry does not hand out at compile time.
//	Having exactly ONE home is what makes that collapse a single edit instead of a sweep.
//
//	⚠ A block-less info answers FALSE. CvInfo::getSkills() returns NULL for any type that authors no
//	skills block, so the read guards it: carrying no skills is carrying no skill.
//

class CvClassificationBlock;

class CvSkillReads
{
public:
	static bool alwaysHeal(const CvClassificationBlock* skills);
	static bool alwaysHostile(const CvClassificationBlock* skills);
	static bool amphib(const CvClassificationBlock* skills);
	static bool attackOnlyCities(const CvClassificationBlock* skills);
	static bool blitz(const CvClassificationBlock* skills);
	static bool canLeadThroughPeaks(const CvClassificationBlock* skills);
	static bool canPassPeaks(const CvClassificationBlock* skills);
	static bool celebrity(const CvClassificationBlock* skills);
	static bool collateralImmune(const CvClassificationBlock* skills);
	static bool dcmFighterEngage(const CvClassificationBlock* skills);
	static bool defenseOnly(const CvClassificationBlock* skills);
	static bool defensiveVictoryMove(const CvClassificationBlock* skills);
	static bool destroy(const CvClassificationBlock* skills);
	static bool enemyRoute(const CvClassificationBlock* skills);
	static bool fliesToMove(const CvClassificationBlock* skills);
	static bool food(const CvClassificationBlock* skills);
	static bool found(const CvClassificationBlock* skills);
	static bool freeDrop(const CvClassificationBlock* skills);
	static bool greatGeneral(const CvClassificationBlock* skills);
	static bool hiddenNationality(const CvClassificationBlock* skills);
	static bool hillsDoubleMove(const CvClassificationBlock* skills);
	static bool ignoreBuildingDefense(const CvClassificationBlock* skills);
	static bool ignoreNoEntryLevel(const CvClassificationBlock* skills);
	static bool ignoreZoneOfControl(const CvClassificationBlock* skills);
	static bool immuneToFirstStrikes(const CvClassificationBlock* skills);
	static bool inquisitor(const CvClassificationBlock* skills);
	static bool noNonOwnedCityEntry(const CvClassificationBlock* skills);
	static bool noNonTypeProdMods(const CvClassificationBlock* skills);
	static bool noSelfHeal(const CvClassificationBlock* skills);
	static bool offensiveVictoryMove(const CvClassificationBlock* skills);
	static bool oneUp(const CvClassificationBlock* skills);
	static bool onlyDefensive(const CvClassificationBlock* skills);
	static bool onslaught(const CvClassificationBlock* skills);
	static bool paralyze(const CvClassificationBlock* skills);
	static bool pillage(const CvClassificationBlock* skills);
	static bool pillageEspionage(const CvClassificationBlock* skills);
	static bool pillageMarauder(const CvClassificationBlock* skills);
	static bool pillageOnMove(const CvClassificationBlock* skills);
	static bool pillageOnVictory(const CvClassificationBlock* skills);
	static bool pillageResearch(const CvClassificationBlock* skills);
	static bool river(const CvClassificationBlock* skills);
	static bool sabotage(const CvClassificationBlock* skills);
	static bool stampede(const CvClassificationBlock* skills);
	static bool stateReligion(const CvClassificationBlock* skills);
	static bool stealPlans(const CvClassificationBlock* skills);
	static bool stealthDefense(const CvClassificationBlock* skills);
	static bool suicide(const CvClassificationBlock* skills);
	static bool tradable(const CvClassificationBlock* skills);
	static bool unlimitedException(const CvClassificationBlock* skills);
	static bool zoneOfControl(const CvClassificationBlock* skills);

private:
	CvSkillReads();                                  // organization only -- never instantiated
	CvSkillReads(const CvSkillReads&);
	CvSkillReads& operator=(const CvSkillReads&);
};

#endif // CV_SKILL_READS_H
