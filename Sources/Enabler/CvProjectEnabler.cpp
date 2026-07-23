//
//	ProjectEnabler -- the PROJECTS domain on the standardized enabler component (see the header): the per-player
//	creatable vector's O(delta) tech/project appliers, through the ONE kernel applier. Content is event-built.
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvProjectEnabler.h"
#include "Enabler/CvEnabler.h"            // EnablerDomain/PlayerEnabler -- the standardized domain (CvPlayer::m_enabler)
#include "Enabler/CvEnablerKernel.h"      // EnablerKernel::applyEdges/requiresMet/allowedOk -- the one applier + gate surfaces
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the gate evaluation's context
#include "CvInfo.h"
#include <set>
#include "CvTechInfo.h"           // cascadeStartNode -- the TECH_GAME_START root redirect
#include "CvProjectInfo.h"        // the project->project enables source (the Apollo chain)
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"        // GET_PLAYER
#include "AI/CvTeamAI.h"          // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"

static const CvInfo* pj_techJson(int iTech)
{
	if (iTech == GC.getInfoTypeForString("TECH_GAME_START", true)) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// The lifecycle INIT: size + zero ONLY -- NO content. The content is built purely from the DOMAIN events (the
// play-time emits AND the load reseed's in-read emits, one mechanism -- the load-RESEED).
void ProjectEnabler::initDomain(const CvPlayer& kPlayer)
{
	kPlayer.m_enabler.projects.init(GC.getNumProjectInfos());
}

// THE GATE for one project candidate (enabler.md par.7.1 steps 2+3): requiresMet (no project authors requires
// today -- future data rides for free) AND the allowed caps through allowedOk's PROJECT branch (the
// engine-owned counts: world = created-ever, team = held -- the already-completed Encyclopedia/Evolution class
// gates out). Gate atoms are game/team-level state, loaded before any player reads, so per-event gating is
// stable through the load reseed (the techs-domain precedent).
static void pj_gate(const CvPlayer& kPlayer, EnablerDomain& d, int iProject)
{
	const CvInfo* j = InfoRepo<CvProjectInfo>::get().get(iProject);
	CvCascadeEvalCtx ec;
	ec.player = &kPlayer;
	ec.team = &GET_TEAM(kPlayer.getTeam());
	CvCascadeEvalFlags gateFlags;
	d.setGateFailed(iProject, (j != NULL && !cascadeGateOk(j->getGate(), ec, gateFlags))   // entity-level enabled/disabled (DEC-entity-gate)
	                       || !EnablerKernel::requiresMet(j, ec)
	                       || !EnablerKernel::allowedOk(j, iProject, kPlayer, /*bUnit*/ false, EDGEB_PROJECTS));
}

// The touched candidate set of one event source (its enables/removal project targets + its EDGEF_REQUIRED_BY
// project dependents -- all off the source's own info, O(delta)).
static void pj_touched(const CvInfo* j, std::set<int>& touched)
{
	if (j == NULL) return;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = j->edge(FAMS[f], EDGEB_PROJECTS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
}

static void pj_gateSet(const CvPlayer& kPlayer, EnablerDomain& d, const std::set<int>& ids)
{
	for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
		if (d.inTree(*it)) pj_gate(kPlayer, d, *it);
}

void ProjectEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	const CvInfo* jt = pj_techJson((int)eTech);
	std::set<int> touched;
	pj_touched(jt, touched);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;
		// the broad-emit flip guard: the PLAYER tech domain's held flag, read PRE-TechEnabler (the route's ordering)
		if (!kPlayer.m_enabler.techs.isSeeded() || kPlayer.m_enabler.techs.isHeld((int)eTech) == bHas) continue;
		EnablerDomain& d = kPlayer.m_enabler.projects;
		if (!d.isSeeded()) continue;   // pre-init window: the object's own read/init emits replay its facts
		EnablerKernel::applyEdges(d, jt, EDGEB_PROJECTS, bHas ? +1 : -1);
		pj_gateSet(kPlayer, d, touched);   // gate-on-entry + the par.7.1 step-2 re-gates
	}
}

void ProjectEnabler::onProjectChanged(PlayerTypes ePlayer, ProjectTypes eProject, int iDelta)
{
	if (ePlayer == NO_PLAYER || eProject == NO_PROJECT || iDelta == 0) return;
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	EnablerDomain& d = kPlayer.m_enabler.projects;
	if (!d.isSeeded()) return;   // pre-init window: the object's own read/init emits replay its facts
	const CvInfo* jp = InfoRepo<CvProjectInfo>::get().get((int)eProject);
	// HAVE = team count > 0: apply only on a crossing (the emit carries the applied count delta). Per-member
	// emits make this exactly-once per player -- the team-wide fan-out would double-apply already-read players
	// during the load reseed.
	const int iNew = GET_TEAM(kPlayer.getTeam()).getProjectCount(eProject);
	const int iOld = iNew - iDelta;
	if ((iOld > 0) != (iNew > 0))
		EnablerKernel::applyEdges(d, jp, EDGEB_PROJECTS, iNew > 0 ? +1 : -1);
	// gate-on-entry + the step-2 re-gates for THIS member's domain (the project's own enables chain)
	std::set<int> touched;
	pj_touched(jp, touched);
	touched.insert((int)eProject);   // the CAP crossing (par.7.1 step 3): its own count changed
	pj_gateSet(kPlayer, d, touched);
	// the WORLD-cap crossing reaches RIVALS too (getProjectCreatedCount is game-wide): a capped project
	// completed by any team re-gates on every seeded player's domain (Encyclopedia vanishes everywhere).
	if (jp != NULL && jp->getAllowed() != NULL)
	{
		for (int iP = 0; iP < MAX_PLAYERS; iP++)
		{
			const CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
			EnablerDomain& dp = kP.m_enabler.projects;
			if (dp.isSeeded() && dp.inTree((int)eProject)) pj_gate(kP, dp, (int)eProject);
		}
	}
}
