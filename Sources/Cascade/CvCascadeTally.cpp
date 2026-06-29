//
//	CvCascadeTally -- the #430 count machine (tally.md) + its in-engine shadow. See CvCascadeTally.h for the model.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeTally.h"
#include "AI/BetterBTSAI.h"        // gPlayerLogLevel + streamLogTee (the shared /events tee + the slice-1 gate)
#include "Defines/CvGlobals.h"     // GC -- resolve Type indices to names in the (gated) shadow line
#include "CvBuildingInfo.h"
#include "CvUnitInfo.h"

CvCascadeTally& cascadeTally()
{
	static CvCascadeTally s_tally;
	return s_tally;
}

void cascadeTallyShadow()
{
	cascadeTally().shadowDiff();
}

// ===================== the accumulator (player-leaf, only nonzero entries) =====================

void CvCascadeTally::setCount(std::map<int, CountMap>& kDomain, int iPlayer, int iType, int iCount)
{
	if (iPlayer < 0 || iType < 0)
	{
		return;
	}
	if (iCount > 0)
	{
		kDomain[iPlayer][iType] = iCount;
	}
	else // a count of 0 drops the entry (kept sparse) -- the empire no longer has any
	{
		std::map<int, CountMap>::iterator it = kDomain.find(iPlayer);
		if (it != kDomain.end())
		{
			it->second.erase(iType);
		}
	}
}

int CvCascadeTally::getCount(const std::map<int, CountMap>& kDomain, int iPlayer, int iType)
{
	std::map<int, CountMap>::const_iterator it = kDomain.find(iPlayer);
	if (it == kDomain.end())
	{
		return 0;
	}
	CountMap::const_iterator jt = it->second.find(iType);
	return jt == it->second.end() ? 0 : jt->second;
}

int CvCascadeTally::buildingCount(int iPlayer, int iBuilding) const { return getCount(m_buildings, iPlayer, iBuilding); }
int CvCascadeTally::unitCount(int iPlayer, int iUnit) const { return getCount(m_units, iPlayer, iUnit); }

// ===================== incremental maintenance from DOMAIN events =====================

void CvCascadeTally::onEvent(const CvCascadeEvent& kEvent)
{
	if (kEvent.eKind != EVENTKIND_DOMAIN)
	{
		return;
	}
	// The count events carry the AUTHORITATIVE new empire count in iA (= getBuildingCount/getUnitCount at emit), the
	// player in iC, the type in iType -- so we SET (idempotent), never accumulate a delta. NAME_CHANGE et al are not
	// counts (tally.md: only counted DOMAIN events feed the tally), so the switch default ignores them.
	switch (kEvent.iEventId)
	{
	case CASCADE_EVT_BUILDING_COUNT: setCount(m_buildings, kEvent.iC, kEvent.iType, kEvent.iA); break;
	case CASCADE_EVT_UNIT_COUNT:     setCount(m_units,     kEvent.iC, kEvent.iType, kEvent.iA); break;
	default: break;
	}
}

// ===================== object scan (candidate key discovery) =====================

void CvCascadeTally::gather(const CvPlayer& kPlayer, std::set<int>& kBuildings, std::set<int>& kUnits)
{
	int iLoop = 0;
	for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
	{
		const std::vector<BuildingTypes> aHas = pCity->getHasBuildings();
		for (std::vector<BuildingTypes>::const_iterator it = aHas.begin(); it != aHas.end(); ++it)
		{
			kBuildings.insert((int)*it);
		}
	}
	for (const CvUnit* pUnit = kPlayer.firstUnit(&iLoop); pUnit != NULL; pUnit = kPlayer.nextUnit(&iLoop))
	{
		kUnits.insert((int)pUnit->getUnitType());
	}
}

// ===================== rebuild-on-load (deterministic seed) =====================

void CvCascadeTally::rebuild()
{
	m_buildings.clear();
	m_units.clear();
	for (int iP = 0; iP < MAX_PLAYERS; ++iP)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
		if (!kP.isEverAlive())
		{
			continue;
		}
		std::set<int> kB, kU;
		gather(kP, kB, kU);
		// Seed each candidate type to the AUTHORITATIVE legacy empire count -> the tally starts exactly == the engine
		// (a clean shadow at load), then DOMAIN events keep it in step.
		for (std::set<int>::const_iterator it = kB.begin(); it != kB.end(); ++it)
		{
			setCount(m_buildings, iP, *it, kP.getBuildingCount((BuildingTypes)*it));
		}
		for (std::set<int>::const_iterator it = kU.begin(); it != kU.end(); ++it)
		{
			setCount(m_units, iP, *it, kP.getUnitCount((UnitTypes)*it));
		}
	}
}

// ===================== the in-engine SHADOW (cascade-vs-legacy) =====================

void CvCascadeTally::shadowDiff() const
{
	if (gPlayerLogLevel < 1)
	{
		return; // gated like the logging consumer -- off in normal play, no cost
	}

	int iChecked = 0, iDiverging = 0, iShown = 0;
	const int iMaxShown = 12;
	char szBuf[512];

	for (int iP = 0; iP < MAX_PLAYERS; ++iP)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
		if (!kP.isEverAlive())
		{
			continue;
		}
		std::set<int> kB, kU;
		gather(kP, kB, kU);
		// Union the tally's OWN keys too, so a STALE entry (event-tally still holds a type the engine no longer does)
		// is caught as well, not only a missing / short one.
		{
			std::map<int, CountMap>::const_iterator it = m_buildings.find(iP);
			if (it != m_buildings.end())
				for (CountMap::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) kB.insert(jt->first);
			it = m_units.find(iP);
			if (it != m_units.end())
				for (CountMap::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) kU.insert(jt->first);
		}
		for (std::set<int>::const_iterator it = kB.begin(); it != kB.end(); ++it)
		{
			++iChecked;
			const int iTally = buildingCount(iP, *it);
			const int iLegacy = kP.getBuildingCount((BuildingTypes)*it);
			if (iTally != iLegacy)
			{
				++iDiverging;
				if (iShown < iMaxShown && *it >= 0 && *it < GC.getNumBuildingInfos())
				{
					sprintf(szBuf, "[TALLY/diff] player=%d building=%s tally=%d legacy=%d",
						iP, GC.getBuildingInfo((BuildingTypes)*it).getType(), iTally, iLegacy);
					gDLL->logMsg("Cascade.log", szBuf);
					streamLogTee(1, szBuf);
					++iShown;
				}
			}
		}
		for (std::set<int>::const_iterator it = kU.begin(); it != kU.end(); ++it)
		{
			++iChecked;
			const int iTally = unitCount(iP, *it);
			const int iLegacy = kP.getUnitCount((UnitTypes)*it);
			if (iTally != iLegacy)
			{
				++iDiverging;
				if (iShown < iMaxShown && *it >= 0 && *it < GC.getNumUnitInfos())
				{
					sprintf(szBuf, "[TALLY/diff] player=%d unit=%s tally=%d legacy=%d",
						iP, GC.getUnitInfo((UnitTypes)*it).getType(), iTally, iLegacy);
					gDLL->logMsg("Cascade.log", szBuf);
					streamLogTee(1, szBuf);
					++iShown;
				}
			}
		}
	}

	sprintf(szBuf, "[TALLY/shadow] turn=%d checked=%d diverging=%d", GC.getGame().getGameTurn(), iChecked, iDiverging);
	gDLL->logMsg("Cascade.log", szBuf);
	streamLogTee(1, szBuf);
}
