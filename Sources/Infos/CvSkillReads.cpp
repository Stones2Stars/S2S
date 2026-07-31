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

SKILL_READ(alwaysHeal,             "alwaysHeal")
SKILL_READ(alwaysHostile,          "alwaysHostile")
SKILL_READ(alwaysInvisible,        "alwaysInvisible")
SKILL_READ(amphib,                 "amphib")
SKILL_READ(animalIgnoresBorders,   "animalIgnoresBorders")
SKILL_READ(attackOnlyCities,       "attackOnlyCities")
SKILL_READ(blitz,                  "blitz")
SKILL_READ(canLeadThroughPeaks,    "canLeadThroughPeaks")
SKILL_READ(canMoveAllTerrain,      "canMoveAllTerrain")
SKILL_READ(canMoveImpassable,      "canMoveImpassable")
SKILL_READ(canPassPeaks,           "canPassPeaks")
SKILL_READ(celebrity,              "celebrity")
SKILL_READ(collateralImmune,       "collateralImmune")
SKILL_READ(counterSpy,             "counterSpy")
SKILL_READ(dcmFighterEngage,       "dcmFighterEngage")
SKILL_READ(defenseOnly,            "defenseOnly")
SKILL_READ(defensiveVictoryMove,   "defensiveVictoryMove")
SKILL_READ(destroy,                "destroy")
SKILL_READ(enemyRoute,             "enemyRoute")
SKILL_READ(flatMovementCost,       "flatMovementCost")
SKILL_READ(fliesToMove,            "fliesToMove")
SKILL_READ(food,                   "food")
SKILL_READ(found,                  "found")
SKILL_READ(freeDrop,               "freeDrop")
SKILL_READ(goldenAge,              "goldenAge")
SKILL_READ(greatGeneral,           "greatGeneral")
SKILL_READ(hiddenNationality,      "hiddenNationality")
SKILL_READ(hillsDoubleMove,        "hillsDoubleMove")
SKILL_READ(ignoreBuildingDefense,  "ignoreBuildingDefense")
SKILL_READ(ignoreNoEntryLevel,     "ignoreNoEntryLevel")
SKILL_READ(ignoreTerrainCost,      "ignoreTerrainCost")
SKILL_READ(ignoreZoneOfControl,    "ignoreZoneOfControl")
SKILL_READ(noBadGoodies,           "noBadGoodies")
SKILL_READ(noCapture,              "noCapture")
SKILL_READ(noDefensiveBonus,       "noDefensiveBonus")
SKILL_READ(blendIntoCity,          "blendIntoCity")
SKILL_READ(inquisitor,             "inquisitor")
SKILL_READ(investigate,            "investigate")
SKILL_READ(noNonOwnedCityEntry,    "noNonOwnedCityEntry")
SKILL_READ(noNonTypeProdMods,      "noNonTypeProdMods")
SKILL_READ(noSelfHeal,             "noSelfHeal")
SKILL_READ(nukeImmune,             "nukeImmune")
SKILL_READ(offensiveVictoryMove,   "offensiveVictoryMove")
SKILL_READ(oneUp,                  "oneUp")
SKILL_READ(onlyDefensive,          "onlyDefensive")
SKILL_READ(onslaught,              "onslaught")
SKILL_READ(pillage,                "pillage")
SKILL_READ(pillageEspionage,       "pillageEspionage")
SKILL_READ(pillageMarauder,        "pillageMarauder")
SKILL_READ(pillageOnMove,          "pillageOnMove")
SKILL_READ(pillageOnVictory,       "pillageOnVictory")
SKILL_READ(pillageResearch,        "pillageResearch")
SKILL_READ(rivalTerritory,         "rivalTerritory")
SKILL_READ(river,                  "river")
SKILL_READ(sabotage,               "sabotage")
SKILL_READ(stampede,               "stampede")
SKILL_READ(stateReligion,          "stateReligion")
SKILL_READ(stealPlans,             "stealPlans")
SKILL_READ(stealthDefense,         "stealthDefense")
SKILL_READ(suicide,                "suicide")
SKILL_READ(tradable,               "tradable")
SKILL_READ(unlimitedException,     "unlimitedException")
SKILL_READ(zoneOfControl,          "zoneOfControl")

#undef SKILL_READ

// ⚠ ONE skill, TWO authored spellings ([skills.md] §1 lists them as a single entry:
// "firstStrikeImmune / immuneToFirstStrikes"). Both are live in the data, and the shorter spelling carries the
// large majority, so a read of either alone answers false for most of the units that hold it. This is the ONE
// place that knows about the pair ([DEC-single-implementation]); a consumer asks the question, not a spelling.
// ⛔ Do NOT "simplify" this back to a single key until the curator emits one -- unifying the SPELLING is a data
// change, and until it lands, dropping either half silently loses those authorings.
bool CvSkillReads::immuneToFirstStrikes(const CvClassificationBlock* skills)
{
	static int s_idShort = -1;
	static int s_idLong = -1;
	if (skills == NULL)
	{
		return false;
	}
	return skills->hasKey(s_idShort, CLSD_SKILL, "firstStrikeImmune")
		|| skills->hasKey(s_idLong, CLSD_SKILL, "immuneToFirstStrikes");
}
