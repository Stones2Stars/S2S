#pragma once
#ifndef CV_PLAYER_STATISTICS_H
#define CV_PLAYER_STATISTICS_H

//
//	CvPlayerStatistics -- the Info screen's statistics tables (docs/reference/info-screen.md § Statistics): one row
//	per type a player has something to show for.
//

///<summary>A unit type's row: how many the player has now, and how many it built, killed and lost.</summary>
struct UnitStatisticRow
{
	UnitTypes eUnit;
	int iCurrent;
	int iBuilt;
	int iKilled;
	int iLost;
};

///<summary>Each unit type ePlayer has now, built, killed or lost, in unit order.</summary>
void getUnitStatistics(PlayerTypes ePlayer, std::vector<UnitStatisticRow>& rows);

///<summary>Each building type ePlayer has built, with how many times, in building order.</summary>
void getBuildingStatistics(PlayerTypes ePlayer, std::vector<std::pair<BuildingTypes, int> >& rows);

///<summary>Each improvement type on ePlayer's land, with how many (CvPlayer::getImprovementCount), in improvement
/// order.</summary>
void getImprovementCounts(PlayerTypes ePlayer, std::vector<std::pair<ImprovementTypes, int> >& rows);

#endif // CV_PLAYER_STATISTICS_H
