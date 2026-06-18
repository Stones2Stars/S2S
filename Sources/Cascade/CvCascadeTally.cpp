//
//	CvCascadeTally -- out-of-line parts of the #428/#430 count machine (see CvCascadeTally.h).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeTally.h"
#include "CvGlobals.h"      // GC
#include "CvGame.h"         // getUnitCreatedCount (the historic created counter for the UNIT world-cap)
#include "CvPlayerAI.h"     // GET_PLAYER / getBuildingCount / getUnitCount / getTeam

// DIAGNOSTIC shadow-result ids (unsynced; logging-only -- never gate). One per domain so the log is unambiguous.
enum
{
	TALLY_EVT_SHADOW_BUILDING = 10, // {type=-1, a=checked, b=mismatches}; per-mismatch {type, a=tally, b=truth, c=player}
	TALLY_EVT_SHADOW_UNIT     = 11
};

// The number of type indices in a domain (the engine resolution of "how big is this domain's index space").
static int domainNumTypes(CountDomain eDomain)
{
	switch (eDomain)
	{
	case COUNTDOMAIN_BUILDING: return GC.getNumBuildingInfos();
	case COUNTDOMAIN_UNIT:     return GC.getNumUnitInfos();
	default:                   return 0;
	}
}

int CvCascadeTally::seededTruth(CountDomain eDomain, int iPlayer, int iType) const
{
	const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
	switch (eDomain)
	{
	case COUNTDOMAIN_BUILDING: return kPlayer.getBuildingCount((BuildingTypes)iType);
	case COUNTDOMAIN_UNIT:     return kPlayer.getUnitCount((UnitTypes)iType);
	default:                   return 0;
	}
}

void CvCascadeTally::onEvent(const CvCascadeEvent& kEvent)
{
	if (kEvent.iC < 0 || kEvent.iC >= MAX_PLAYERS)
	{
		return;
	}
	// Map the DOMAIN event to its count domain (the engine side of the JSON type prefix). iB = delta, iC = player.
	CountDomain eDomain;
	switch (kEvent.iEventId)
	{
	case CASCADE_EVT_BUILDING_COUNT: eDomain = COUNTDOMAIN_BUILDING; break;
	case CASCADE_EVT_UNIT_COUNT:     eDomain = COUNTDOMAIN_UNIT;     break;
	default:                         return; // not a counted-domain event
	}
	m_counts[eDomain][kEvent.iC].deposit(kEvent.iType, kEvent.iB);
}

void CvCascadeTally::rebuildDomain(CountDomain eDomain)
{
	const int iNumTypes = domainNumTypes(eDomain);
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
	{
		m_counts[eDomain][iPlayer].clear();
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (!kPlayer.isAlive())
		{
			continue;
		}
		for (int iType = 0; iType < iNumTypes; ++iType)
		{
			const int iCount = seededTruth(eDomain, iPlayer, iType);
			if (iCount != 0)
			{
				m_counts[eDomain][iPlayer].deposit(iType, iCount);
			}
		}
	}
}

void CvCascadeTally::rebuild()
{
	// §9: serialize nothing -- seed every wired domain from the authoritative objects; events maintain after.
	rebuildDomain(COUNTDOMAIN_BUILDING);
	rebuildDomain(COUNTDOMAIN_UNIT);
}

int CvCascadeTally::count(CountDomain eDomain, int iType, CountScope eScope, int iContext) const
{
	if (eDomain < 0 || eDomain >= NUM_COUNT_DOMAINS)
	{
		return 0;
	}
	switch (eScope)
	{
	case COUNTSCOPE_EMPIRE:
		return (iContext >= 0 && iContext < MAX_PLAYERS) ? m_counts[eDomain][iContext].get(iType) : 0;
	case COUNTSCOPE_TEAM:
	{
		int iSum = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			if ((int)GET_PLAYER((PlayerTypes)iPlayer).getTeam() == iContext)
			{
				iSum += m_counts[eDomain][iPlayer].get(iType);
			}
		}
		return iSum;
	}
	case COUNTSCOPE_WORLD:
	{
		int iSum = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			iSum += m_counts[eDomain][iPlayer].get(iType);
		}
		return iSum;
	}
	}
	return 0;
}

int CvCascadeTally::countForCap(CountDomain eDomain, int iType, CountScope eScope, int iContext) const
{
	// UNIT world-cap reads LIFETIME-CREATED (the engine's historic counter), not currently-alive: a hero born
	// once and now gone still consumes its world slot (legacy isUnitMaxedOut reads getUnitCreatedCount,
	// CvGame.cpp:5104). The storage stays in the engine; this is the single located place the cascade reads it.
	if (eDomain == COUNTDOMAIN_UNIT && eScope == COUNTSCOPE_WORLD
		&& iType >= 0 && iType < GC.getNumUnitInfos())
	{
		return GC.getGame().getUnitCreatedCount((UnitTypes)iType);
	}
	return count(eDomain, iType, eScope, iContext);
}

void CvCascadeTally::shadowDomain(CountDomain eDomain, int iShadowEventId) const
{
	const int iNumTypes = domainNumTypes(eDomain);
	int iChecked = 0;
	int iMismatch = 0;
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kPlayer.isAlive())
		{
			for (int iType = 0; iType < iNumTypes; ++iType)
			{
				const int iTruth = seededTruth(eDomain, iPlayer, iType);
				const int iTallied = m_counts[eDomain][iPlayer].get(iType);
				if (iTruth != 0 || iTallied != 0)
				{
					++iChecked;
					if (iTruth != iTallied)
					{
						++iMismatch;
						eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, iShadowEventId, iType, iTallied, iTruth, iPlayer));
					}
				}
			}
		}
		else
		{
			// dead player: the tally must be empty; any residue is a phantom mismatch.
			for (CvScopedAccumulator::const_iterator it = m_counts[eDomain][iPlayer].begin();
				it != m_counts[eDomain][iPlayer].end(); ++it)
			{
				++iChecked;
				++iMismatch;
				eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, iShadowEventId, it->first, it->second, 0, iPlayer));
			}
		}
	}
	eventSpine().emit(CvCascadeEvent(EVENTKIND_DIAGNOSTIC, iShadowEventId, -1, iChecked, iMismatch));
}

void CvCascadeTally::shadowVerify() const
{
	shadowDomain(COUNTDOMAIN_BUILDING, TALLY_EVT_SHADOW_BUILDING);
	shadowDomain(COUNTDOMAIN_UNIT, TALLY_EVT_SHADOW_UNIT);
}

CvCascadeTally& cascadeTally()
{
	static CvCascadeTally s_tally;
	return s_tally;
}
