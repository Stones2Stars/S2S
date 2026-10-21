#pragma once
#ifndef CV_VICTORY_SCORE_PART_H
#define CV_VICTORY_SCORE_PART_H

///<summary>One part of a team's victory score, as CvTeam::getVictoryScoreParts fills it: what the team has of the
/// measure, what the whole world has, and the points the team earns from it.</summary>
struct VictoryScorePart
{
	int64_t team;
	int64_t world;
	int64_t points;
};

///<summary>Whether the part eKind measures an amount (culture, power), which a reader reduces; the others are
/// counts and percents.</summary>
inline bool isVictoryScoreAmount(VictoryScorePartKind eKind)
{
	return eKind == VICTORY_SCORE_CULTURE || eKind == VICTORY_SCORE_POWER;
}

#endif // CV_VICTORY_SCORE_PART_H
