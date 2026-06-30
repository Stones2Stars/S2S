//
//	CvJsonInfo -- see the header. Only the dtor needs an out-of-line body (it frees the owned BoolExpr trees). The home
//	is the per-info-type InfoRepo (Repos/InfoRepo.h); the old CvInfoBase*-keyed side-table is retired.
//

#include "CvGameCoreDLL.h"
#include "CvJsonInfo.h"

CvJsonInfo::~CvJsonInfo()
{
	clear();
}

void CvJsonInfo::clear()
{
	for (size_t i = 0; i < deposits.size(); ++i)
	{
		delete deposits[i].enabled;
		delete deposits[i].disabled;
	}
	deposits.clear();
	delete requiresBuild;   requiresBuild = NULL;
	delete requiresOperate; requiresOperate = NULL;
	edges.clear();
	allowed.clear();
	grantLists.clear();
	grantPulses.clear();
	capabilities.clear();
}

// The synthetic TECH_GAME_START root (see the header): a single process-static CvJsonInfo, off the InfoRepo (it has no
// engine id). readJson maps TECH_GAME_START's enables into it; the enabler seeds GENERATE from it for every player.
CvJsonInfo& cascadeStartNode()
{
	static CvJsonInfo s_startNode;
	return s_startNode;
}
