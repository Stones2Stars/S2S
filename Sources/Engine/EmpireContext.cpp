#include "CvGameCoreDLL.h"
#include "EmpireContext.h"
#include "CvPlayer.h"
#include "Defines/CvGlobals.h"          // GC -- civic-option / trait counts
#include "Repos/InfoRepo.h"             // InfoRepo<CvCivicInfo> (the same civic read the L1 policy walk uses)
#include "CvCivicInfo.h"                // the civic §9 policies block
#include "CvTraitInfo.h"                // the trait §9 policies block
#include "Data/CvDepositRead.h"         // MMKernel::traitData -- the active-set (simple/complex) trait resolver
#include "CvClassificationRegistry.h"   // count(CLSD_POLICY) -- the minted POLICY id space
#include "CvJsonBoolBlock.h"            // CLSD_POLICY + hasId (the O(1) policy bitset)

// forwarding accessor: the empire's state religion is already O(1) on CvPlayer -- read it through, no stored copy.
int EmpireContext::stateReligion() const { return m_player != NULL ? (int)m_player->getStateReligion() : -1; }

// Rebuild the enacted-policy UNION from the player's LIVE grantors -- adopted civics + held (active-set) traits --
// exactly the grantor set the one policy read walks (CvConditionEval ev_playerHasPolicy). This is a WHOLE rebuild,
// called on civic/trait change and at load (never per read); the union is then an O(1) `policies.has(pid)` for every
// consumer. Keyed by the ClassificationRegistry domain-local POLICY id (CvJsonBoolBlock::hasId space).
void EmpireContext::rebuildPolicies()
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
		const CvJsonBoolBlock* b = (d != NULL) ? d->getPolicies() : NULL;
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
		const CvJsonBoolBlock* b = (d != NULL) ? d->getPolicies() : NULL;
		if (b == NULL)
			continue;
		for (int pid = 0; pid < nPolicies; ++pid)
			if (b->hasId(pid))
				policies.add(pid, 1);
	}
}
