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
bool EmpireContext::isRebel() const                { return m_player != NULL && m_player->isRebel(); }
int  EmpireContext::numCities() const              { return m_player != NULL ? m_player->getNumCities() : 0; }
int  EmpireContext::currentEra() const             { return m_player != NULL ? (int)m_player->getCurrentEra() : 0; }
int  EmpireContext::commerceRate(int eCommerce) const { return m_player != NULL ? m_player->getCommercePercent((CommerceTypes)eCommerce) : 0; }
bool EmpireContext::teamHasTech(int eTech) const
{
	return m_player != NULL && eTech >= 0 && GET_TEAM(m_player->getTeam()).isHasTech((TechTypes)eTech);
}
int  EmpireContext::playerId() const
{
	return m_player != NULL ? (int)m_player->getID() : -1;
}
int  EmpireContext::teamId() const
{
	return m_player != NULL ? (int)m_player->getTeam() : (int)NO_TEAM;
}
bool EmpireContext::isHuman() const
{
	return m_player != NULL && m_player->isHuman();
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

// The realized-commerce group forward: the bound player's own group read, handed on unchanged -- no store, no
// mirror, no second derivation (contexts.md STORES vs FORWARDS). The out-array is FULLY DEFINED on every path,
// so an unbound context zero-fills rather than leaving caller memory untouched.
void EmpireContext::commerces(int (&realizedCommerces)[NUM_COMMERCE_TYPES]) const
{
	if (m_player == NULL)
	{
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			realizedCommerces[iCommerce] = 0;
		}
		return;
	}
	m_player->getCommerces(realizedCommerces);
}

// The slider-percentage group forward: the same single forward the *_RATE counter tokens read, fanned out over
// the group's own enum -- one accessor names CvPlayer::getCommercePercent, so the token read and the split read
// cannot disagree. The out-array is FULLY DEFINED on every path (an unbound context answers 0 per channel).
void EmpireContext::commerceRates(int (&commerceRates)[NUM_COMMERCE_TYPES]) const
{
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		commerceRates[iCommerce] = commerceRate(iCommerce);
	}
}

// Fill the EMPIRE half of the eval ctx (player/team) from the bound player; CityContext::fillEvalCtx fills city/plot.
void EmpireContext::fillEvalCtx(CvCascadeEvalCtx& ec) const
{
	//	The empire silo answers for the player AND for its team: a team is the TECH BRIDGE and owns no
	//	live-state surface, so it contributes no second pointer here -- its facts are forwarded (teamHasTech /
	//	teamId / teamMemberCount / teamProjectCount) ([contexts.md]; the banner on CvTeam).
	ec.empireContext = this;
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
