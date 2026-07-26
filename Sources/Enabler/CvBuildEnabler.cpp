//
//	BuildEnabler -- the worker-BUILDS domain on the standardized enabler component (see the header): the
//	per-player unlocked-builds vector's O(delta) tech applier, through the ONE kernel applier. Content is
//	event-built.
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvBuildEnabler.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain/PlayerEnabler -- the standardized domain (CvPlayer::m_enabler)
#include "Enabler/CvEnablerKernel.h"      // EnablerKernel::applyEdges -- the one domain-refcount edge applier
#include "CvInfo.h"
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (the from-start builds)
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"

static const CvInfo* bl_techInfo(int iTech)
{
	if (iTech == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// The lifecycle INIT: size + zero ONLY -- NO content. The content is built purely from the DOMAIN events (the
// play-time emits AND the load reseed's in-read per-held-tech emits, one mechanism -- the load-RESEED).
void BuildEnabler::initDomain(const CvPlayer& kPlayer)
{
	kPlayer.m_enabler.builds.init(GC.getNumBuildInfos());
}

void BuildEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = bl_techInfo((int)eTech);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;
		// the broad-emit flip guard: the PLAYER tech domain's held flag, read PRE-TechEnabler (the route's ordering)
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		EnablerDomain& d = kPlayer.m_enabler.builds;
		if (!d.isSeeded()) continue;   // pre-init window: the object's own read/init emits replay its facts
		EnablerKernel::applyEdges(d, jt, EDGEB_BUILDS, bHas ? +1 : -1);
	}
}
