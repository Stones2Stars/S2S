#include "CvGameCoreDLL.h"
#include "Engine/CvSizeMattersRank.h"
#include "Infos/CvUnitInfo.h"

int CvSizeMattersRank::mergeLimit(const CvUnitInfo& kUnit, const int iEra)
{
	//	+1 so a unit is mergeable at all in the starting era: the gate is `groupRank < mergeLimit`, so a base-rank
	//	unit in era 0 would otherwise already sit at its ceiling.
	return kUnit.getBaseGroupRank() + iEra + 1;
}

int CvSizeMattersRank::maxRankUps(const CvUnitInfo& kUnit, const int iEra)
{
	return std::max(0, mergeLimit(kUnit, iEra) - kUnit.getBaseGroupRank());
}

int CvSizeMattersRank::rankUpCostMultiplier(const CvUnitInfo& kUnit, const int iRankUps)
{
	if (iRankUps <= 0)
	{
		return 1;
	}
	const int iBaseGroupRank = kUnit.getBaseGroupRank();
	const int iBaseGeometry = smGroupMultiplier(iBaseGroupRank);
	if (iBaseGeometry < 1)
	{
		//	smGroupMultiplier clamps, so this cannot fire on real data; guarding rather than dividing by it keeps
		//	a derivation defect from turning into a division by zero here.
		return 1;
	}
	return smGroupMultiplier(iBaseGroupRank + iRankUps) / iBaseGeometry;
}
