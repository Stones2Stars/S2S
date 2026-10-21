#include "CvGameCoreDLL.h"
#include "CvPlayerStatistics.h"
#include "CvEventReporter.h"
#include "Defines/CvGlobals.h"
#include "AI/CvPlayerAI.h"

void getUnitStatistics(PlayerTypes ePlayer, std::vector<UnitStatisticRow>& rows)
{
	const CvPlayer& player = GET_PLAYER(ePlayer);
	const CvPlayerRecord& record = CvEventReporter::getInstance().getPlayerRecord(ePlayer);
	for (int iUnit = 0; iUnit < GC.getNumUnitInfos(); iUnit++)
	{
		UnitStatisticRow row;
		row.eUnit = (UnitTypes)iUnit;
		row.iCurrent = player.getUnitCount(row.eUnit);
		row.iBuilt = record.getNumUnitsBuilt(iUnit);
		row.iKilled = record.getNumUnitsKilled(iUnit);
		row.iLost = record.getNumUnitsWasKilled(iUnit);
		if (row.iCurrent > 0 || row.iBuilt > 0 || row.iKilled > 0 || row.iLost > 0)
		{
			rows.push_back(row);
		}
	}
}

void getBuildingStatistics(PlayerTypes ePlayer, std::vector<std::pair<BuildingTypes, int> >& rows)
{
	const CvPlayerRecord& record = CvEventReporter::getInstance().getPlayerRecord(ePlayer);
	for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); iBuilding++)
	{
		const int iBuilt = record.getNumBuildingsBuilt((BuildingTypes)iBuilding);
		if (iBuilt > 0)
		{
			rows.push_back(std::make_pair((BuildingTypes)iBuilding, iBuilt));
		}
	}
}

void getImprovementCounts(PlayerTypes ePlayer, std::vector<std::pair<ImprovementTypes, int> >& rows)
{
	const CvPlayer& player = GET_PLAYER(ePlayer);
	for (int iImprovement = 0; iImprovement < GC.getNumImprovementInfos(); iImprovement++)
	{
		const int iCount = player.getImprovementCount((ImprovementTypes)iImprovement);
		if (iCount > 0)
		{
			rows.push_back(std::make_pair((ImprovementTypes)iImprovement, iCount));
		}
	}
}
