#include "CvGameCoreDLL.h"
#include "EmpireContext.h"
#include "CvPlayer.h"
#include "AI/CvTeamAI.h"                 // GET_TEAM -- fillEvalCtx
#include "Conditions/CvConditionEval.h"  // CvCascadeEvalCtx -- fillEvalCtx
#include "Defines/CvGlobals.h"          // GC -- civic-option / trait counts
#include "Repos/InfoRepo.h"             // InfoRepo<CvCivicInfo> (the same civic read the L1 policy walk uses)
#include "CvCivicInfo.h"                // the civic §9 policies block
#include "CvTraitInfo.h"                // the trait §9 policies block
#include "Data/CvDepositRead.h"         // MMKernel::traitData -- the active-set (simple/complex) trait resolver
#include "CvClassificationRegistry.h"   // count(CLSD_POLICY) -- the minted POLICY id space
#include "CvClassificationBlock.h"            // CLSD_POLICY + hasId (the O(1) policy bitset)

// --- forwarding accessors: read the bound player / its team; no stored copy (owner: don't duplicate available
// state). These are the HAVE-axis reads of the empire scope (contexts.md) -- the evaluator's atoms and the
// enabler's gates ask HERE, never CvPlayer/CvTeam directly.
int EmpireContext::stateReligion() const { return m_player != NULL ? (int)m_player->getStateReligion() : -1; }

bool EmpireContext::hasCivic(int eCivic) const
{
	if (m_player == NULL || eCivic < 0)
		return false;
	for (int iOption = 0; iOption < GC.getNumCivicOptionInfos(); ++iOption)
	{
		if ((int)m_player->getCivics((CivicOptionTypes)iOption) == eCivic)
			return true;
	}
	return false;
}

bool EmpireContext::hasTrait(int eTrait) const     { return m_player != NULL && eTrait >= 0 && m_player->hasTrait((TraitTypes)eTrait); }
bool EmpireContext::hasHeritage(int eHeritage) const { return m_player != NULL && eHeritage >= 0 && m_player->hasHeritage((HeritageTypes)eHeritage); }
bool EmpireContext::isGoldenAge() const            { return m_player != NULL && m_player->isGoldenAge(); }
bool EmpireContext::isAnarchy() const              { return m_player != NULL && m_player->isAnarchy(); }
int  EmpireContext::numCities() const              { return m_player != NULL ? m_player->getNumCities() : 0; }
int  EmpireContext::currentEra() const             { return m_player != NULL ? (int)m_player->getCurrentEra() : 0; }
int  EmpireContext::commerceRate(int eCommerce) const { return m_player != NULL ? m_player->getCommercePercent((CommerceTypes)eCommerce) : 0; }
bool EmpireContext::teamHasTech(int eTech) const
{
	return m_player != NULL && eTech >= 0 && GET_TEAM(m_player->getTeam()).isHasTech((TechTypes)eTech);
}
int  EmpireContext::teamProjectCount(int eProject) const
{
	if (m_player == NULL || eProject < 0)
		return 0;
	return GET_TEAM(m_player->getTeam()).getProjectCount((ProjectTypes)eProject);
}
int  EmpireContext::teamMemberCount() const
{
	return m_player != NULL ? GET_TEAM(m_player->getTeam()).getNumMembers() : 0;
}

// Fill the EMPIRE half of the eval ctx (player/team) from the bound player; CityContext::fillEvalCtx fills city/plot.
void EmpireContext::fillEvalCtx(CvCascadeEvalCtx& ec) const
{
	ec.player = m_player;
	ec.team = (m_player != NULL) ? &GET_TEAM(m_player->getTeam()) : NULL;
}

// Rebuild the enacted-policy UNION from the player's LIVE grantors -- adopted civics + held (active-set) traits --
// exactly the grantor set the one policy read walks (CvConditionEval ev_playerHasPolicy). A WHOLE rebuild from
// source, never a per-read walk; the union is then an O(1) `policies.has(pid)` for every consumer. Keyed by the
// ClassificationRegistry domain-local POLICY id (CvClassificationBlock::hasId space).
//
// THE ONLY CALLER is the contexts' spine consumer, driven by the civic / trait / player-init DOMAIN facts (at play
// AND through the load reseed's in-read emits). It is deliberately NOT called from CvPlayer's choke points: an
// event-maintained store with a direct hook beside the event has two maintenance surfaces for one fact.
void EmpireContext::rebuildPolicies() const
{
	policies.clear();
	if (m_player == NULL)
		return;
	const int nPolicies = ClassificationRegistry::count(CLSD_POLICY);
	if (nPolicies <= 0)
		return;   // no POLICY_* minted -> nothing to union

	// adopted civics
	for (int i = 0; i < GC.getNumCivicOptionInfos(); ++i)
	{
		const CivicTypes eCivic = m_player->getCivics((CivicOptionTypes)i);
		if (eCivic == NO_CIVIC)
			continue;
		const CvCivicInfo* d = static_cast<const CvCivicInfo*>(InfoRepo<CvCivicInfo>::get().get(eCivic));
		const CvClassificationBlock* b = (d != NULL) ? d->getPolicies() : NULL;
		if (b == NULL)
			continue;
		for (int pid = 0; pid < nPolicies; ++pid)
			if (b->hasId(pid))
				policies.add(pid, 1);
	}
	// held traits (the game-option-selected simple/complex set, via the shared active-set resolver)
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!m_player->hasTrait((TraitTypes)t))
			continue;
		const CvTraitInfo* d = MMKernel::traitData(t);
		const CvClassificationBlock* b = (d != NULL) ? d->getPolicies() : NULL;
		if (b == NULL)
			continue;
		for (int pid = 0; pid < nPolicies; ++pid)
			if (b->hasId(pid))
				policies.add(pid, 1);
	}
}
