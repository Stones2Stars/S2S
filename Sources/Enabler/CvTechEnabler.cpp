//
//	TechEnabler -- the TECH domain on the standardized enabler component (see the header): the maintained
//	researchable vector (seed + O(delta) event updates + bare reads).
//

#include "CvGameCoreDLL.h"
#include "Enabler/CvTechEnabler.h"
#include "Enabler/CvEnabler.h"
#include "CvInfo.h"
#include "Repos/InfoRepo.h"
#include "AI/CvPlayerAI.h"           // GET_PLAYER
#include "AI/CvTeamAI.h"             // GET_TEAM
#include "Defines/CvGlobals.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Enabler/CvEnablerKernel.h"           // EnablerKernel::standardGateReason -- the ONE gate evaluator
#include "Conditions/CvConditionEval.h"    // CvCascadeEvalCtx -- the gate evaluation's context
#include "CvTechInfo.h"                // the tech domain
#include "CvReligionInfo.h"            // a religion's EDGEF_ENABLED_BY -- the techs that found it

///<summary>
/// Applies one tech's source-side tech edges (enables, and the obsoletes/replaces/disables removals) to the
/// tech domain's refcounts with the given sign (enabler.md par.7.1 step 1). The load seed and the play-time event
/// both use it.
///</summary>
static void te_applyDelta(EnablerDomain& techDomain, int iTech, int iDelta)
{
	const CvInfo* pTechInfo = InfoRepo<CvTechInfo>::get().get(iTech);
	if (pTechInfo == NULL)
	{
		return;
	}
	const std::vector<int>* pEnabled = pTechInfo->edge(EDGEF_ENABLES, EDGEB_TECHS);
	if (pEnabled != NULL)
	{
		for (size_t iEnabled = 0; iEnabled < pEnabled->size(); ++iEnabled)
		{
			techDomain.addEnable((*pEnabled)[iEnabled], iDelta);
		}
	}
	static const EnEdgeFamily REMOVE_FAMILIES[] = { EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, NUM_EDGEF };
	for (int iFamily = 0; REMOVE_FAMILIES[iFamily] != NUM_EDGEF; ++iFamily)
	{
		const std::vector<int>* pRemoved = pTechInfo->edge(REMOVE_FAMILIES[iFamily], EDGEB_TECHS);
		if (pRemoved != NULL)
		{
			for (size_t iRemoved = 0; iRemoved < pRemoved->size(); ++iRemoved)
			{
				techDomain.addRemove((*pRemoved)[iRemoved], iDelta);
			}
		}
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
	// The PERMANENT bars -- a tech this player can never research whatever else changes. Both are static for the
	// life of the player (a civilization never changes), which is exactly what the static-exclusion plane is for;
	// bars that can LIFT belong on the gate instead, and bars that are consumed elsewhere in play belong on the
	// removal plane.
	const CvCivilizationInfo& kCiv = GC.getCivilizationInfo(kPlayer.getCivilizationType());
	for (int t = 0; t < GC.getNumTechInfos(); ++t)
	{
		if (GC.getTechInfo((TechTypes)t).isDisable()          // identity.disable -- law-ban techs + the root
		|| kCiv.isTechDisabled((TechTypes)t))     // this CIV can never research it
		{
			d.setStaticExcluded(t, true);
		}
	}
}

// THE GATE for one candidate (enabler.md par.7.1 steps 2+3): the requires.build evaluation (the multi-parent
// all/any -- techs are monotonic, no operate) through the ONE evaluator, AND the `allowed` cap
// (EnablerKernel::allowedOk's tech branch -- the engine-owned counts, e.g. the 29 world-unique founder techs).
// Either failing sets the domain's gate verdict: a tree member is GREYED (in the tree, unattainable now),
// never removed. The Limited Religions bar (CvPlayer::isResearchBarredByReligionLimit) is the tech domain's own
// extra clause; it hides, so it replaces any reason that only greys.
static void te_gate(const CvPlayer& kPlayer, EnablerDomain& techDomain, int iTech)
{
	const CvInfo* pTechInfo = InfoRepo<CvTechInfo>::get().get(iTech);
	CvCascadeEvalCtx evalCtx;
	kPlayer.getEmpireContext().fillEvalCtx(evalCtx);   // player+team -- the contexts fill the eval state (contexts.md)
	CvCascadeEvalFlags gateFlags;
	unsigned char eReason = EnablerKernel::standardGateReason(pTechInfo, iTech, kPlayer, evalCtx, gateFlags, /*bUnit*/ false, EDGEB_TECHS);
	if (!EnablerDomain::reasonHides(eReason) && kPlayer.isResearchBarredByReligionLimit((TechTypes)iTech))
	{
		eReason = EnablerDomain::GATEREASON_RELIGION_LIMIT;
	}
	techDomain.setGateReason(iTech, eReason);
}

// The TOUCHED candidate set of one tech event H -- everything whose state or gate can change with H, all read
// off H's OWN info (O(delta)): its enables proposals, its removal-family targets (a withdrawal re-admits), and
// its EDGEF_REQUIRED_BY dependents (the requires-reverse-index, docs/cascade.md §1 (reverse lookups are populated once, at load) -- the in-tree techs whose
// gate references H).
static void te_touched(int iTech, std::set<int>& touched)
{
	const CvInfo* pTechInfo = InfoRepo<CvTechInfo>::get().get(iTech);
	if (pTechInfo == NULL)
	{
		return;
	}
	static const EnEdgeFamily TOUCHING_FAMILIES[] = { EDGEF_ENABLES, EDGEF_OBSOLETES, EDGEF_REPLACES, EDGEF_DISABLES, EDGEF_REQUIRED_BY, NUM_EDGEF };
	for (int iFamily = 0; TOUCHING_FAMILIES[iFamily] != NUM_EDGEF; ++iFamily)
	{
		const std::vector<int>* pTargets = pTechInfo->edge(TOUCHING_FAMILIES[iFamily], EDGEB_TECHS);
		if (pTargets != NULL)
		{
			touched.insert(pTargets->begin(), pTargets->end());
		}
	}
}

void TechEnabler::onTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas)
{
	if (eTeam == NO_TEAM || eTech == NO_TECH) return;
	std::set<int> touched;
	te_touched((int)eTech, touched);
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
		// reseed this runs per event as it arrives: the `requires` atoms are team techs, final before any player
		// reads. The Limited Religions clause reads the player's cities, which have not loaded yet, so for that
		// clause the domain takes the other par.7.1 option -- it is gated once after the stream ends
		// (onReligionLimitChangedForAll at SEVT_GAME_LOAD_FINISHED).
		for (std::set<int>::const_iterator it = touched.begin(); it != touched.end(); ++it)
			te_gate(kPlayer, d, *it);
	}
	// the CAP crossing (par.7.1 step 3 -- "a count event re-checks allowed for that one type"): eTech's world
	// count changed for EVERY team, so a capped eTech re-gates on ALL seeded players' domains, not just the
	// acquiring team's (the founder techs vanish from every rival's researchable list the moment one team
	// takes them).
	const CvInfo* pChangedTechInfo = InfoRepo<CvTechInfo>::get().get((int)eTech);
	if (pChangedTechInfo != NULL && pChangedTechInfo->getAllowed() != NULL && !pChangedTechInfo->getAllowed()->isEmpty())
	{
		for (int iP = 0; iP < MAX_PLAYERS; iP++)
		{
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iP);
			EnablerDomain& d = kPlayer.m_enabler.techs;
			if (!d.isSeeded()) continue;
			te_gate(kPlayer, d, (int)eTech);
		}
	}
}

// The candidates of the Limited Religions bar are the religion-founding techs, read off each religion's own
// EDGEF_ENABLED_BY (the techs whose `enables` names it) -- the reverse view the load pass built, never a sweep
// of the tech registry.
void TechEnabler::onReligionLimitChanged(PlayerTypes ePlayer)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS) return;
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	EnablerDomain& techDomain = kPlayer.m_enabler.techs;
	if (!techDomain.isSeeded()) return;
	for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
	{
		const CvInfo* pReligion = InfoRepo<CvReligionInfo>::get().get(iReligion);
		if (pReligion == NULL) continue;
		const std::vector<int>* pFoundingTechs = pReligion->edge(EDGEF_ENABLED_BY, EDGEB_TECHS);
		if (pFoundingTechs == NULL) continue;
		for (size_t iFounder = 0; iFounder < pFoundingTechs->size(); ++iFounder)
		{
			te_gate(kPlayer, techDomain, (*pFoundingTechs)[iFounder]);
		}
	}
}

void TechEnabler::onReligionLimitChangedForAll()
{
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
	{
		onReligionLimitChanged((PlayerTypes)iPlayer);
	}
}
