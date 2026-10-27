#include "CvGameCoreDLL.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvSyncedScope.h"
#include "Spine/CvEventSpine.h"
#include "Tools/CvRandom.h"

#include <set>

int CvSyncedScope::s_iDepth = 0;

// [SYNC] draws on the synchronized RNG made outside every synced root.
namespace
{
	enum SyncEvent
	{
		SYNC_DRAW = 1,
		SYNC_HOST       // which machine decides the game-wide rules, as each host query answered it
	};
	enum SyncField
	{
		SYNCF_stream = 1, SYNCF_label, SYNCF_callerRva, SYNCF_turn, SYNCF_turnSlice, SYNCF_sites,
		SYNCF_activePlayer, SYNCF_network, SYNCF_fmpHost, SYNCF_pitbossHost, SYNCF_rulesHost
	};
	const char* syncLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case SYNC_DRAW: return "[SYNC/draw]";
		case SYNC_HOST: return "[SYNC/host]";
		default:        return NULL;
		}
	}
	const char* syncFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case SYNCF_stream:    *peType = SFT_STR; return "stream";
		case SYNCF_label:     *peType = SFT_STR; return "label";
		case SYNCF_callerRva: return "callerRva";
		case SYNCF_turn:      return "turn";
		case SYNCF_turnSlice: return "turnSlice";
		case SYNCF_sites:     return "sites";
		case SYNCF_activePlayer: return "activePlayer";
		case SYNCF_network:      return "network";
		case SYNCF_fmpHost:      return "fmpHost";
		case SYNCF_pitbossHost:  return "pitbossHost";
		case SYNCF_rulesHost:    return "rulesHost";
		default:              return NULL;
		}
	}
	struct SyncLogRegistrar { SyncLogRegistrar() { spineRegisterDomain(SD_SYNC, &syncLinePrefix, "Sync.log", &syncFieldInfo); } };
	SyncLogRegistrar s_syncLogRegistrar; // static-init registration
}

CvSyncedScope::CvSyncedScope()
{
	++s_iDepth;
}

CvSyncedScope::~CvSyncedScope()
{
	--s_iDepth;
}

bool CvSyncedScope::isActive()
{
	return s_iDepth > 0;
}

bool CvSyncedScope::isRulesHost()
{
	return !GC.getGame().isNetworkMultiPlayer() || gDLL->isFMPMgrHost() || gDLL->IsPitbossHost();
}

void CvSyncedScope::reportHost()
{
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_SYNC, SYNC_HOST, 1)
		.addI(SYNCF_activePlayer, (int)GC.getGame().getActivePlayer())
		.addI(SYNCF_network, GC.getGame().isNetworkMultiPlayer() ? 1 : 0)
		.addI(SYNCF_fmpHost, gDLL->isFMPMgrHost() ? 1 : 0)
		.addI(SYNCF_pitbossHost, gDLL->IsPitbossHost() ? 1 : 0)
		.addI(SYNCF_rulesHost, isRulesHost() ? 1 : 0));
}

void CvSyncedScope::checkDraw(const CvRandom* pStream, const char* szLabel, const void* pCaller)
{
	if (s_iDepth > 0)
	{
		return;
	}
	// Setup (info load, init, map generation, the load itself) runs identically on every client before the game
	// is final-initialized, so only draws after that point need a synced root.
	CvGameAI* pGame = GC.getGamePointer();
	if (pGame == NULL || !pGame->isFinalInitialized())
	{
		return;
	}
	const char* szStream = NULL;
	if (pStream == &pGame->getSorenRand())
	{
		szStream = "soren";
	}
	else if (pStream == &pGame->getMapRand())
	{
		szStream = "map";
	}
	else
	{
		return;
	}

	static const char* s_pModuleBase = (const char*)GetModuleHandle("CvGameCoreDLL.dll");
	const int iCallerRva = (int)((const char*)pCaller - s_pModuleBase);
	static std::set<int> s_reportedSites;
	if (!s_reportedSites.insert(iCallerRva).second)
	{
		return;
	}
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_SYNC, SYNC_DRAW, 1)
		.addStr(SYNCF_stream, szStream)
		.addStr(SYNCF_label, szLabel != NULL ? szLabel : "")
		.addI(SYNCF_callerRva, iCallerRva)
		.addI(SYNCF_turn, pGame->getGameTurn())
		.addI(SYNCF_turnSlice, pGame->getTurnSlice())
		.addI(SYNCF_sites, (int)s_reportedSites.size()));
}
