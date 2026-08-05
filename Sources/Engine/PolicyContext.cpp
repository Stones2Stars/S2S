//
//	PolicyContext -- the empire's enacted-policy state (see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "PolicyContext.h"
#include "Spine/CvEventSpine.h"
#include "AI/CvPlayerAI.h"                     // GET_PLAYER
#include "Infos/CvClassificationBlock.h"
#include "Infos/CvClassificationRegistry.h"
#include "Infos/CvCivicInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Data/CvDepositRead.h"                // MMKernel::traitData -- the option-selected active trait set

namespace
{
	PolicyContext* policyFor(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
		{
			return NULL;
		}
		return &GET_PLAYER((PlayerTypes)iPlayer).policies();
	}

	//	The dispatch object the spine registers. It owns NO state -- the state is on the players.
	class PolicySpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { PolicyContext::onSpineEvent(kEvent); }
	};

	PolicySpineConsumer s_policyConsumer;
	bool s_bPolicyRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains enacted-policy state is named here, at the store.
bool PolicyContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	case SEVT_CIVIC_ADOPTED:          // the swap: iType adopted, iB displaced
	case SEVT_EMPIRE_TRAIT_ADDED:     // iType trait; the direction is the fact's identity
	case SEVT_EMPIRE_TRAIT_REMOVED:
	case SEVT_PLAYER_INIT:            // the initial traits, which no setter announces
		return true;
	default:
		return false;
	}
}

void PolicyContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	PolicyContext* pPolicies = policyFor(kEvent.iC);
	if (pPolicies == NULL)
	{
		return;
	}
	switch (kEvent.iEventId)
	{
	// A civic SWAP. Both ends ride the fact, so the withdrawal is exact with nothing remembered. NOT guarded on
	// the load bracket: a player exists when its own facts stream, and the reseed passes NO_CIVIC as the
	// displaced side, so the same two calls build the store on a load.
	case SEVT_CIVIC_ADOPTED:
		pPolicies->foldCivic(kEvent.iB, -1);
		pPolicies->foldCivic(kEvent.iType, +1);
		break;
	case SEVT_EMPIRE_TRAIT_ADDED:
		pPolicies->foldTrait(kEvent.iType, +1);
		break;
	case SEVT_EMPIRE_TRAIT_REMOVED:
		pPolicies->foldTrait(kEvent.iType, -1);
		break;
	case SEVT_PLAYER_INIT:
		pPolicies->foldHeldTraits(+1);
		break;
	default:
		break;
	}
}

void PolicyContext::foldCivic(int iCivic, int iSign)
{
	if (iCivic < 0)
	{
		return;
	}
	const CvCivicInfo* pCivic = static_cast<const CvCivicInfo*>(InfoRepo<CvCivicInfo>::get().get(iCivic));
	foldBlock((pCivic != NULL) ? pCivic->getPolicies() : NULL, iSign);
}

// The TRAIT's block comes from the option-selected ACTIVE set (simple vs complex), through the one resolver --
// never the raw record, which may be the wrong set's ([modifier.md] §4).
void PolicyContext::foldTrait(int iTrait, int iSign)
{
	if (iTrait < 0)
	{
		return;
	}
	const CvTraitInfo* pTrait = MMKernel::traitData(iTrait);
	foldBlock((pTrait != NULL) ? pTrait->getPolicies() : NULL, iSign);
}

void PolicyContext::foldHeldTraits(int iSign)
{
	if (m_player == NULL)
	{
		return;
	}
	const int iNumTraits = GC.getNumTraitInfos();
	for (int iTrait = 0; iTrait < iNumTraits; ++iTrait)
	{
		if (m_player->hasTrait((TraitTypes)iTrait))
		{
			foldTrait(iTrait, iSign);
		}
	}
}

// Walk what the GRANTOR carries (the index IS the generated id), never every minted policy id.
// ⛔ A policy grant is boolean on the grantor's side -- a civic either enacts it or it does not. The COUNT is the
// holder's, and it counts GRANTORS.
void PolicyContext::foldBlock(const CvClassificationBlock* pBlock, int iSign)
{
	if (pBlock == NULL || iSign == 0)
	{
		return;
	}
	const std::vector<char>& kGranted = pBlock->grantedById();
	for (int iPolicyId = 0; iPolicyId < (int)kGranted.size(); ++iPolicyId)
	{
		if (kGranted[iPolicyId] != 0)
		{
			add(iPolicyId, iSign);
		}
	}
}

void policyContextRegisterConsumer()
{
	if (s_bPolicyRegistered)
	{
		return;
	}
	s_bPolicyRegistered = true;
	eventSpine().registerConsumer(&s_policyConsumer);
}
