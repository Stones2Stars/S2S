//
//	CvCascadeTally -- the standardized aggregate-count access surface. See CvCascadeTally.h for the model: it READS the
//	object-owned counts (CvPlayer's O(1) maintained aggregates) and rolls them UP the scope spine. No store, no event
//	consumption, no load-time seed, no shadow (the object IS the authoritative source -- a shadow would be tautological).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeTally.h"
#include "AI/CvPlayerAI.h"   // GET_PLAYER + getBuildingCount/getUnitCount (the object-owned aggregate) + getTeam/isAlive

CvCascadeTally& cascadeTally()
{
	static CvCascadeTally s_tally;
	return s_tally;
}

// Does this alive player contribute to a roll-up at (eScope, iEntity)? EMPIRE = the one player; TEAM = the team's
// players; WORLD = all. The single place the spine roll-up membership is decided.
static bool tallyPlayerInScope(const CvPlayer& kPlayer, CascadeCountScope eScope, int iEntity)
{
	if (!kPlayer.isAlive())
	{
		return false;
	}
	switch (eScope)
	{
	case CASCADE_COUNT_EMPIRE: return (int)kPlayer.getID() == iEntity;
	case CASCADE_COUNT_TEAM:   return (int)kPlayer.getTeam() == iEntity;
	case CASCADE_COUNT_WORLD:  return true;
	}
	return false;
}

int CvCascadeTally::buildingCount(int iEntity, int iBuilding, CascadeCountScope eScope) const
{
	if (iBuilding < 0)
	{
		return 0;
	}
	const BuildingTypes eBuilding = (BuildingTypes)iBuilding;
	// EMPIRE is the hot, common case -> the player's own O(1) aggregate directly (no scan).
	if (eScope == CASCADE_COUNT_EMPIRE)
	{
		return (iEntity >= 0 && iEntity < MAX_PLAYERS) ? GET_PLAYER((PlayerTypes)iEntity).getBuildingCount(eBuilding) : 0;
	}
	int iSum = 0;
	for (int iP = 0; iP < MAX_PLAYERS; ++iP)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
		if (tallyPlayerInScope(kP, eScope, iEntity))
		{
			iSum += kP.getBuildingCount(eBuilding);
		}
	}
	return iSum;
}

int CvCascadeTally::unitCount(int iEntity, int iUnit, CascadeCountScope eScope) const
{
	if (iUnit < 0)
	{
		return 0;
	}
	const UnitTypes eUnit = (UnitTypes)iUnit;
	if (eScope == CASCADE_COUNT_EMPIRE)
	{
		return (iEntity >= 0 && iEntity < MAX_PLAYERS) ? GET_PLAYER((PlayerTypes)iEntity).getUnitCount(eUnit) : 0;
	}
	int iSum = 0;
	for (int iP = 0; iP < MAX_PLAYERS; ++iP)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
		if (tallyPlayerInScope(kP, eScope, iEntity))
		{
			iSum += kP.getUnitCount(eUnit);
		}
	}
	return iSum;
}
