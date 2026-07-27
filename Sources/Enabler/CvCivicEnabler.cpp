//
//	CivicEnabler -- the CIVICS domain on the standardized enabler component (see the header): the per-player
//	adoptable vector's seed + O(delta) tech applier. The domain arrays are the ONLY mutable state; the one HAVE
//	axis (team techs incl. the root) applies through the ONE kernel applier.
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvCivicEnabler.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain/PlayerEnabler -- the standardized domain (CvPlayer::m_enabler)
#include "Enabler/CvEnablerKernel.h"      // EnablerKernel::applyEdges -- the one domain-refcount edge applier
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the gate evaluation's context
#include "CvCivicInfo.h"
#include "CvInfo.h"
#include <set>
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect (carries the start civics)
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "AI/CvTeamAI.h"          // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"

static const CvInfo* ce_techInfo(int iTech)
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

static const CvInfo* ce_info(int iId) { return InfoRepo<CvCivicInfo>::get().get(iId); }

// THE GATE for one adoptable candidate (enabler.md par.7.1 steps 2+3): the entity-level enabled/disabled pair, the
// `requires` gate, and the `allowed` cap -- a civic authors requires.build (the city-count limit rides its own live check). Gate atoms are player/team-level
// state, so per-event gating is stable through the load reseed (the projects-domain precedent).
// ⛔ Without this the domain sets NO gate flag, so every tree member stays LISTED: the enable-side OVER-OFFER
// enabler.md par.8 names as a visible defect, never a reason to fall back to a legacy can* check.
static void ce_gate(const CvPlayer& kPlayer, EnablerDomain& d, int iId)
{
	const CvInfo* j = ce_info(iId);
	CvCascadeEvalCtx ec;
	kPlayer.getEmpireContext().fillEvalCtx(ec);   // player+team -- the contexts fill the eval state (contexts.md)
	CvCascadeEvalFlags gateFlags;
	d.setGateFailed(iId, (j != NULL && !cascadeGateOk(j->getGate(), ec, gateFlags))   // DEC-entity-gate
	                  || !EnablerKernel::requiresMet(j, ec)
	                  || !EnablerKernel::allowedOk(j, iId, kPlayer, /*bUnit*/ false, EDGEB_CIVICS));
}

// The touched candidate set of one event source (its enables/removal targets + its EDGEF_REQUIRED_BY dependents).
static void ce_touched(const CvInfo* j, std::set<int>& touched)
{
	if (j == NULL) return;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = j->edge(FAMS[f], EDGEB_CIVICS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
}

static void ce_gateSet(const CvPlayer& kPlayer, EnablerDomain& d, const std::set<int>& ids)
{
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		if (d.inTree(*it)) ce_gate(kPlayer, d, *it);
}

void CivicEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = ce_techInfo((int)eTech);
	std::set<int> touched;
	ce_touched(jt, touched);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;   // seeded-but-dead players update too (a revival reads fresh)
		// the broad-emit flip guard: the PLAYER tech domain's held flag, read PRE-TechEnabler (the route's ordering)
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		EnablerDomain& d = kPlayer.m_enabler.civics;
		if (!d.isSeeded()) continue;   // pre-init window: the object's own read/init emits replay its facts
		EnablerKernel::applyEdges(d, jt, EDGEB_CIVICS, bHas ? +1 : -1);
		ce_gateSet(kPlayer, d, touched);   // gate-on-entry + the par.7.1 step-2 re-gates
	}
}
