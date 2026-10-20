#pragma once
#ifndef CV_DEMOGRAPHIC_STANDING_H
#define CV_DEMOGRAPHIC_STANDING_H

///<summary>What a demographics measure is: an amount (commerce, power), a count (land plots, people) or a percent
/// (the happiness and health shares).</summary>
enum DemographicUnit
{
	DEMOGRAPHIC_UNIT_AMOUNT = 0,
	DEMOGRAPHIC_UNIT_COUNT,
	DEMOGRAPHIC_UNIT_PERCENT
};

///<summary>The unit of the measure eKind.</summary>
inline DemographicUnit demographicUnitOf(DemographicKind eKind)
{
	switch (eKind)
	{
	case DEMOGRAPHIC_LAND_AREA:
	case DEMOGRAPHIC_POPULATION:
		return DEMOGRAPHIC_UNIT_COUNT;
	case DEMOGRAPHIC_HAPPINESS:
	case DEMOGRAPHIC_HEALTH:
		return DEMOGRAPHIC_UNIT_PERCENT;
	default:
		return DEMOGRAPHIC_UNIT_AMOUNT;
	}
}

///<summary>One row of the demographics table, as CvPlayer::getDemographicStanding fills it: the player's own
/// measure, the best, average and worst of its rivals (every other living civ), and the player's rank among all,
/// each in the measure's unit.</summary>
struct DemographicStanding
{
	DemographicUnit unit;
	int64_t value;
	int64_t bestRival;
	int64_t rivalAverage;
	int64_t worstRival;
	int rank;

	DemographicStanding() : unit(DEMOGRAPHIC_UNIT_AMOUNT), value(0), bestRival(0), rivalAverage(0), worstRival(0), rank(1) {}
};

#endif // CV_DEMOGRAPHIC_STANDING_H
