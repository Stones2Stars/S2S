//
//	CapabilityContext -- the empire's ability state (see the header for the rulings).
//

#include "CvGameCoreDLL.h"
#include "CapabilityContext.h"
#include "Spine/CvEventSpine.h"
#include "AI/CvPlayerAI.h"                    // GET_PLAYER
#include "AI/CvTeamAI.h"                      // GET_TEAM -- the play-time fan over the team's members
#include "Infos/CvClassificationBlock.h"
#include "Infos/CvTechInfo.h"
#include "Repos/InfoRepo.h"

namespace
{
	CapabilityContext* capabilitiesFor(int iPlayer)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
		{
			return NULL;
		}
		return &GET_PLAYER((PlayerTypes)iPlayer).capabilities();
	}

	//	The dispatch object the spine registers. It owns NO state -- the state is on the players.
	class CapabilitySpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { CapabilityContext::onSpineEvent(kEvent); }
	};

	CapabilitySpineConsumer s_capabilityConsumer;
	bool s_bCapabilityRegistered = false;

	// Walk what the GRANTOR carries (the index IS the generated id), never every minted id in the domain.
	void foldBlockInto(const CvClassificationBlock* pBlock, ContextDict& kDict, int iSign)
	{
		if (pBlock == NULL || iSign == 0)
		{
			return;
		}
		const std::vector<char>& kGranted = pBlock->grantedById();
		for (int iId = 0; iId < (int)kGranted.size(); ++iId)
		{
			if (kGranted[iId] != 0)
			{
				kDict.add(iId, iSign);
			}
		}
	}
}

// --- the reads: the player's own delta store, OR the universal start-node baseline (see the header) ----------
bool CapabilityContext::hasCapability(int iCapabilityId) const
{
	return m_capabilities.has(iCapabilityId)
		|| clsHasId(cascadeStartNode().getCapabilities(), iCapabilityId);
}

bool CapabilityContext::hasCanTrade(int iCanTradeId) const
{
	return m_canTrade.has(iCanTradeId)
		|| clsHasId(cascadeStartNode().getCanTrade(), iCanTradeId);
}

bool CapabilityContext::hasCanWorkOn(int iCanWorkOnId) const
{
	return m_canWorkOn.has(iCanWorkOnId)
		|| clsHasId(cascadeStartNode().getCanWorkOn(), iCanWorkOnId);
}

bool CapabilityContext::canTradeOnTerrain(int iTerrain) const
{
	if (m_canTradeOn.has(iTerrain))
	{
		return true;
	}
	const std::set<int>& kBase = static_cast<const CvTechInfo&>(cascadeStartNode()).getCanTradeOnTerrains();
	return kBase.find(iTerrain) != kBase.end();
}

void CapabilityContext::clear()
{
	m_capabilities.clear();
	m_canTrade.clear();
	m_canWorkOn.clear();
	m_canTradeOn.clear();
	m_corpRevenueMod = 0;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains empire-ability state is named here, at the store.
bool CapabilityContext::wantsEvent(int iEventId)
{
	switch (iEventId)
	{
	case SEVT_EMPIRE_TECH_ADDED:      // the broad both-directions tech fact, past setHasTech's no-change guard
	case SEVT_EMPIRE_TECH_REMOVED:
		return true;
	default:
		return false;
	}
}

void CapabilityContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (!wantsEvent(kEvent.iEventId))
	{
		return;
	}
	const int iSign = (kEvent.iEventId == SEVT_EMPIRE_TECH_ADDED) ? +1 : -1;

	// AT LOAD every member already emits per-self (CvPlayer::read walks its team's held techs), so the fact
	// reaches each player once and the fold is a plain delta. AT PLAY setHasTech emits ONCE for the acquiring
	// player, and tech is TEAM-held, so the delta FANS over that team's members. The guard is what keeps the fan
	// from counting every member twice against the load build (contexts.md: the amenity fold's play-time fan).
	if (spineGameLoadInProgress())
	{
		if (CapabilityContext* pCaps = capabilitiesFor(kEvent.iC))
		{
			pCaps->foldTech(kEvent.iType, iSign);
		}
		return;
	}

	if (kEvent.iC < 0 || kEvent.iC >= MAX_PLAYERS)
	{
		return;
	}
	const TeamTypes eTeam = GET_PLAYER((PlayerTypes)kEvent.iC).getTeam();
	if (eTeam == NO_TEAM)
	{
		return;
	}
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
	{
		const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kPlayer.isAlive() && kPlayer.getTeam() == eTeam)
		{
			if (CapabilityContext* pCaps = capabilitiesFor(iPlayer))
			{
				pCaps->foldTech(kEvent.iType, iSign);
			}
		}
	}
}

void CapabilityContext::foldTech(int iTech, int iSign)
{
	if (iTech < 0 || iSign == 0)
	{
		return;
	}
	const CvTechInfo* pTech = static_cast<const CvTechInfo*>(InfoRepo<CvTechInfo>::get().get(iTech));
	if (pTech == NULL)
	{
		return;
	}
	foldBlockInto(pTech->getCapabilities(), m_capabilities, iSign);
	foldBlockInto(pTech->getCanTrade(),     m_canTrade,     iSign);
	foldBlockInto(pTech->getCanWorkOn(),    m_canWorkOn,    iSign);

	// canTradeOn carries real TERRAIN_ FKs rather than authored keys, so it folds off the FK set directly.
	const std::set<int>& kTerrains = pTech->getCanTradeOnTerrains();
	for (std::set<int>::const_iterator it = kTerrains.begin(); it != kTerrains.end(); ++it)
	{
		m_canTradeOn.add(*it, iSign);
	}

	// The maintained corporation-revenue sum: the grantor's own compiled point read, applied as a delta.
	m_corpRevenueMod += iSign * pTech->getCorporationCommerceModifier(CASC_SCOPE_EMPIRE);
}

int CapabilityContext::commerceRateCapability(int eCommerce)
{
	switch (eCommerce)
	{
	case COMMERCE_RESEARCH:  return CLS_CAPABILITY_CAN_SET_SCIENCE_RATE;
	case COMMERCE_CULTURE:   return CLS_CAPABILITY_CAN_SET_CULTURE_RATE;
	case COMMERCE_ESPIONAGE: return CLS_CAPABILITY_CAN_SET_ESPIONAGE_RATE;
	default:                 return -1;   // GOLD is the residual channel -- no slider to unlock
	}
}


void capabilityContextRegisterConsumer()
{
	if (s_bCapabilityRegistered)
	{
		return;
	}
	s_bCapabilityRegistered = true;
	eventSpine().registerConsumer(&s_capabilityConsumer);
}
