//
//	TechEnabler -- the TECH domain on the standardized enabler component (see the header): the maintained
//	researchable vector (seed + O(delta) event updates + bare reads), with the pure function kept as the oracle.
//

#include "CvGameCoreDLL.h"
#include "CvTechEnabler.h"
#include "CvEnabler.h"
#include "CvInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"           // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "CvEnablerKernel.h"           // EnablerKernel::generate/requiresMet -- the oracle GENERATE + the ONE gate evaluator
#include "CvCascadeConditionEval.h"    // CvCascadeEvalCtx -- the gate evaluation's context
#include "CvTechInfo.h"                // cascadeStartNode -- the synthetic TECH_GAME_START root (off the InfoRepo)

// The tech's cascade info: the synthetic TECH_GAME_START root lives OFF the InfoRepo (cascadeStartNode) --
// but it IS a held engine tech (the guaranteed root, enabler.md par.2), so its edges apply through the same
// delta path as every other held tech, exactly once.
static const CvInfo* te_json(int iTech)
{
	const int iGameStart = GC.getInfoTypeForString("TECH_GAME_START", true);
	if (iTech == iGameStart) return &cascadeStartNode();
	return InfoRepo<CvTechInfo>::get().get(iTech);
}

// ONE tech's source-side tech edges into the domain refcounts (enabler.md par.7.1 step 1) -- the ONE delta
// applier the load seed REPLAYS and the play-time event applies. Order-independent by the membership formula
// (removal wins), so root-last / root-first / any event order converges to the same vector.
static void te_applyDelta(EnablerDomain& d, int iTech, int iDelta)
{
	const CvInfo* j = te_json(iTech);
	if (j == NULL) return;
	const std::vector<int>* p = j->edge(EDGEF_ENABLES, EDGEB_TECHS);
	if (p != NULL)
		for (size_t i = 0; i < p->size(); ++i) d.addEnable((*p)[i], iDelta);
	static const EnEdgeFamily REMOVE_FAMILIES[] = { EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, NUM_EDGEF };
	for (int e = 0; REMOVE_FAMILIES[e] != NUM_EDGEF; ++e)
	{
		p = j->edge(REMOVE_FAMILIES[e], EDGEB_TECHS);
		if (p != NULL)
			for (size_t i = 0; i < p->size(); ++i) d.addRemove((*p)[i], iDelta);
	}
}

// The lifecycle INIT: size + zero + the static exclusions ONLY -- NO content. The content is built purely from
// the DOMAIN events (the play-time emits AND the load reseed's in-read per-held-tech emits, one mechanism --
// event-spine.md the load-RESEED). Called at the start of CvPlayer::read (before the in-read emits stream) and
// at CvPlayer::init/initInGame (new/mid-game creation; the real init emits populate).
void TechEnabler::initDomain(const CvPlayer& kPlayer)
{
	EnablerDomain& d = kPlayer.m_enabler.techs;
	d.init(GC.getNumTechInfos());
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
		// the static never-researchable class (identity.disable -- the law-ban techs + the root itself)
		if (GC.getTechInfo((TechTypes)t).isDisable()) d.setStaticExcluded(t, true);
}

// THE GATE for one candidate (enabler.md par.7.1 steps 2+3): the requires.build evaluation (the multi-parent
// all/any -- techs are monotonic, no operate) through the ONE evaluator, AND the `allowed` cap
// (EnablerKernel::allowedOk's tech branch -- the engine-owned counts, e.g. the 29 world-unique founder techs).
// Either failing sets the domain's gate verdict: a tree member is GREYED (in the tree, unattainable now),
// never removed.
static void te_gate(const CvPlayer& kPlayer, const CvTeam& kTeam, EnablerDomain& d, int iTech)
{
	const CvInfo* j = te_json(iTech);
	CvCascadeEvalCtx ec;
	ec.player = &kPlayer;
	ec.team = &kTeam;
	CvCascadeEvalFlags gateFlags;
	d.setGateFailed(iTech, (j != NULL && !cascadeGateOk(j->getGate(), ec, gateFlags))   // entity-level enabled/disabled (DEC-entity-gate)
	                    || !EnablerKernel::requiresMet(j, ec)
	                    || !EnablerKernel::allowedOk(j, iTech, kPlayer, false, EDGEB_TECHS));
}

// The TOUCHED candidate set of one tech event H -- everything whose state or gate can change with H, all read
// off H's OWN info (O(delta)): its enables proposals, its removal-family targets (a withdrawal re-admits), and
// its EDGEF_REQUIRED_BY dependents (the requires-reverse-index, DEC-one-reverse-view -- the in-tree techs whose
// gate references H).
static void te_touched(int iTech, std::set<int>& touched)
{
	const CvInfo* j = te_json(iTech);
	if (j == NULL) return;
	static const EnEdgeFamily FAMS[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int f = 0; FAMS[f] != NUM_EDGEF; ++f)
	{
		const std::vector<int>* p = j->edge(FAMS[f], EDGEB_TECHS);
		if (p != NULL) touched.insert(p->begin(), p->end());
	}
}

void TechEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	std::set<int> touched;
	te_touched((int)eTech, touched);
	const CvTeam& kTeam = GET_TEAM(eTeam);
	for (int iP = 0; iP < MAX_PLAYERS; iP++)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
		if (kPlayer.getTeam() != eTeam) continue;   // seeded-but-dead players update too (a revival must read fresh)
		EnablerDomain& d = kPlayer.m_enabler.techs;
		if (!d.isSeeded()) continue;                // pre-init window: the object's own read/init emits replay its facts
		if (d.isHeld((int)eTech) == bHas) continue; // idempotency (the repeat-tech re-emit fires with no held flip)
		d.setHeld((int)eTech, bHas);
		te_applyDelta(d, (int)eTech, bHas ? +1 : -1);
		// gate-on-entry + the re-gates (par.7.1 step 2), over ONLY the touched candidates. During the load
		// reseed this runs per event as it arrives (the pure per-event option of the par.7.1 order rule); the
		// gate's atoms are team techs, final before any player reads, so every evaluation is stable.
		for (std::set<int>::const_iterator it = touched.begin(); it != touched.end(); ++it)
			te_gate(kPlayer, kTeam, d, *it);
	}
	// the CAP crossing (par.7.1 step 3 -- "a count event re-checks allowed for that one type"): eTech's world
	// count changed for EVERY team, so a capped eTech re-gates on ALL seeded players' domains, not just the
	// acquiring team's (the founder techs vanish from every rival's researchable list the moment one team
	// takes them).
	const CvInfo* jT = te_json((int)eTech);
	if (jT != NULL && jT->getAllowed() != NULL)
	{
		for (int iP = 0; iP < MAX_PLAYERS; iP++)
		{
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
			EnablerDomain& d = kPlayer.m_enabler.techs;
			if (!d.isSeeded()) continue;
			te_gate(kPlayer, GET_TEAM(kPlayer.getTeam()), d, (int)eTech);
		}
	}
}


// --- THE ORACLE (never the read path): the enable-side pure function. CAN GET = union(enables.techs) -
// (disables/obsoletes/replaces) over HAVE, minus already-held, minus identity.disable. Deliberately
// OVER-INCLUSIVE (the requires gate + allowed cap are later stages). Diff the maintained vector against this
// to catch a missed or mis-ordered delta.
void TechEnabler::available(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& avail)
{
	EnBucketSets cand;
	EnablerKernel::generate(kPlayer, NULL, cand);   // union(enables) - removals over HAVE (start node + held techs + civics)
	const std::set<int>& techs = cand[EDGEB_TECHS];
	for (std::set<int>::const_iterator it = techs.begin(); it != techs.end(); ++it)
	{
		const int t = *it;
		if (kTeam.isHasTech((TechTypes)t)) continue;              // already held -> not a CAN GET candidate
		if (GC.getTechInfo((TechTypes)t).isDisable()) continue;   // the static never-researchable class
		avail.insert(t);
	}
}
