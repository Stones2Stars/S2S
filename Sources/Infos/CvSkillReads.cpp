//
//	CvSkillReads -- see the header. One memoized generated-id per key, resolved once and bit-tested
//	thereafter; a NULL block (an info type that authors no skills) answers false.
//

#include "CvGameCoreDLL.h"   // PCH umbrella
#include "CvSkillReads.h"
#include "CvClassificationBlock.h"

// Each read owns the id it memoizes. The id is minted at LOAD by the ClassificationRegistry, so the
// first call resolves it and every later call is an O(1) bitset test (the pre-resolve load window falls
// back to the string set inside hasKey, so an early consumer still reads correctly).
#define SKILL_READ(method, key)                                              \
	bool CvSkillReads::method(const CvClassificationBlock* skills)           \
	{                                                                        \
		static int s_id = -1;                                                \
		return skills != NULL && skills->hasKey(s_id, CLSD_SKILL, key);      \
	}

SKILL_READ(alwaysHeal,           "alwaysHeal")
SKILL_READ(alwaysHostile,        "alwaysHostile")
SKILL_READ(celebrity,            "celebrity")
SKILL_READ(collateralImmune,     "collateralImmune")
SKILL_READ(dcmFighterEngage,     "dcmFighterEngage")
SKILL_READ(defenseOnly,          "defenseOnly")
SKILL_READ(defensiveVictoryMove, "defensiveVictoryMove")
SKILL_READ(destroy,              "destroy")
SKILL_READ(food,                 "food")
SKILL_READ(found,                "found")
SKILL_READ(freeDrop,             "freeDrop")
SKILL_READ(greatGeneral,         "greatGeneral")
SKILL_READ(hiddenNationality,    "hiddenNationality")
SKILL_READ(ignoreBuildingDefense, "ignoreBuildingDefense")
SKILL_READ(immuneToFirstStrikes, "immuneToFirstStrikes")
SKILL_READ(inquisitor,           "inquisitor")
SKILL_READ(noNonOwnedCityEntry,  "noNonOwnedCityEntry")
SKILL_READ(noNonTypeProdMods,    "noNonTypeProdMods")
SKILL_READ(offensiveVictoryMove, "offensiveVictoryMove")
SKILL_READ(oneUp,                "oneUp")
SKILL_READ(onlyDefensive,        "onlyDefensive")
SKILL_READ(pillage,              "pillage")
SKILL_READ(pillageEspionage,     "pillageEspionage")
SKILL_READ(pillageMarauder,      "pillageMarauder")
SKILL_READ(pillageOnMove,        "pillageOnMove")
SKILL_READ(pillageOnVictory,     "pillageOnVictory")
SKILL_READ(pillageResearch,      "pillageResearch")
SKILL_READ(sabotage,             "sabotage")
SKILL_READ(stateReligion,        "stateReligion")
SKILL_READ(stealPlans,           "stealPlans")
SKILL_READ(stealthDefense,       "stealthDefense")
SKILL_READ(suicide,              "suicide")
SKILL_READ(tradable,             "tradable")
SKILL_READ(unlimitedException,   "unlimitedException")

#undef SKILL_READ
