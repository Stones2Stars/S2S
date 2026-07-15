//
//	CivicEnabler -- the CIVICS domain on the standardized enabler component (see the header): the per-player
//	adoptable vector's seed + O(delta) tech applier. The domain arrays are the ONLY mutable state; the one HAVE
//	axis (team techs incl. the root) applies through the ONE kernel applier.
//

#include "CvGameCoreDLL.h"
#include "CvCivicEnabler.h"
#include "CvEnabler.h"            // EnablerDomain/PlayerEnabler -- the standardized domain (CvPlayer::m_enabler)
#include "CvEnablerKernel.h"      // EnablerKernel::applyEdges -- the one domain-refcount edge applier
#include "CvInfo.h"
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (carries the start civics)
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "AI/CvTeamAI.h"          // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"

static const CvInfo* ce_techJson(int iTech)
{
	if (iTech == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// The lifecycle INIT: size + zero ONLY -- NO content. The content is built purely from the DOMAIN events (the
// play-time tech emits AND the load reseed's in-read per-held-tech emits, one mechanism -- the load-RESEED).
void CivicEnabler::initDomain(const CvPlayer& kPlayer)
{
	kPlayer.m_enabler.civics.init(GC.getNumCivicInfos());
}

void CivicEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = ce_techJson((int)eTech);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;   // seeded-but-dead players update too (a revival reads fresh)
		// the broad-emit flip guard: the PLAYER tech domain's held flag, read PRE-TechEnabler (the route's ordering)
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		EnablerDomain& d = kPlayer.m_enabler.civics;
		if (!d.isSeeded()) continue;   // pre-init window: the object's own read/init emits replay its facts
		EnablerKernel::applyEdges(d, jt, EDGEB_CIVICS, bHas ? +1 : -1);
	}
}
