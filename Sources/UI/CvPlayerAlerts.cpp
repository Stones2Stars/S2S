// CvPlayerAlerts.cpp -- the PLAYER-ALERT spine consumer (event-spine.md § PLAYER ALERTS ARE A SPINE CONSUMER).

#include "CvGameCoreDLL.h"
#include "Engine/CvDeal.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "AI/CvPlayerAI.h"
#include "AI/CvTeamAI.h"
#include "Spine/CvEventSpine.h"
#include "UI/CvPlayerAlerts.h"

namespace
{
	//	⚖ THE DEAL-CANCELLED ALERT. Both parties are told, each in ITS OWN wording -- getDealString renders the
	//	deal from the asked player's side, so the two messages are not one string sent twice.
	//	⚠ It reads the deal by id, which is sound ONLY because emit() dispatches SYNCHRONOUSLY at the mutation
	//	site: CvDeal::kill announces BEFORE CvGame::deleteDeal, so the deal -- and both trade lists -- are still
	//	addressable here. The lists survive the endTrade loops above the emit; those unwind effects and remove
	//	no nodes.
	void onDealRemoved(const CvSpineEvent& kEvent)
	{
		const PlayerTypes eFirst = (PlayerTypes)kEvent.iB;
		const PlayerTypes eSecond = (PlayerTypes)kEvent.iC;

		if (eFirst == NO_PLAYER || eSecond == NO_PLAYER)
		{
			return;
		}
		//	Non-const because getDealString takes a mutable reference; nothing here mutates the deal.
		CvDeal* pDeal = GC.getGame().getDeal(kEvent.iType);
		if (pDeal == NULL)
		{
			return;
		}
		//	An empty deal cancels nothing a player can be told about.
		if (pDeal->getLengthFirstTrades() <= 0 && pDeal->getLengthSecondTrades() <= 0)
		{
			return;
		}
		CvWString szString;
		CvWStringBuffer szDealString;
		const CvWString szCancelString = gDLL->getText("TXT_KEY_POPUP_DEAL_CANCEL");

		//	Each side is told only if it has MET the other -- otherwise the message names a civ the player has
		//	never heard of.
		if (GET_TEAM(GET_PLAYER(eFirst).getTeam()).isHasMet(GET_PLAYER(eSecond).getTeam()))
		{
			szDealString.clear();
			GAMETEXT.getDealString(szDealString, *pDeal, eFirst);
			szString.Format(L"%s: %s", szCancelString.GetCString(), szDealString.getCString());
			AddDLLMessage(eFirst, true, GC.getEVENT_MESSAGE_TIME(), szString, "AS2D_DEAL_CANCELLED");
		}

		if (GET_TEAM(GET_PLAYER(eSecond).getTeam()).isHasMet(GET_PLAYER(eFirst).getTeam()))
		{
			szDealString.clear();
			GAMETEXT.getDealString(szDealString, *pDeal, eSecond);
			szString.Format(L"%s: %s", szCancelString.GetCString(), szDealString.getCString());
			AddDLLMessage(eSecond, true, GC.getEVENT_MESSAGE_TIME(), szString, "AS2D_DEAL_CANCELLED");
		}
	}

	//	⚖ THE POWER-RESTORED ALERT. A blackout is a city STATUS that ticks down and ends on its own, so the
	//	player is told when it lifts -- the message the per-turn maintainer used to emit before it was cut.
	//	⚑ It rides the STATUS fact, not the amenity fold's power crossing, and the two are genuinely different
	//	events: the fold announces the GATED verdict (isPowered), which also moves when a plant is built or lost,
	//	while this is specifically the blackout ENDING ([state.md] § A STATUS IS MIDDLEWARE -- the status gates
	//	delivery and is never folded into the store it gates).
	void onCityStatusRemoved(const CvSpineEvent& kEvent)
	{
		if (kEvent.iType != CITYSTATUS_POWER_DISABLED)
		{
			return;
		}
		const PlayerTypes eOwner = (PlayerTypes)kEvent.iC;
		if (eOwner == NO_PLAYER)
		{
			return;
		}
		const CvCity* pCity = GET_PLAYER(eOwner).getCity(kEvent.iSrcLoc);
		if (pCity == NULL)
		{
			return;
		}
		const CvWString szBuffer = gDLL->getText("TXT_KEY_MISC_POWER_RESTORED", pCity->getNameKey());
		//	⚠ AS2D_REVOLTEND is a REAL tag (audio XML + in-tree use) chosen for the shape it shares -- a bad city
		//	condition ending. What the cut maintainer sounded is not recoverable; this is not it restored.
		AddDLLMessage(eOwner, false, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_REVOLTEND",
			MESSAGE_TYPE_MINOR_EVENT, NULL, GC.getCOLOR_WHITE(), pCity->getX(), pCity->getY(), true, true);
	}

	//	The dispatch object the spine registers. It owns NO state -- an alert is a pure output.
	class CvPlayerAlertConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }

		void onEvent(const CvSpineEvent& kEvent)
		{
			//	⛔ SUPPRESSED FOR THE LOAD, and this IS the result-producer case rather than a guard copied from
			//	a neighbour (event-spine.md § the guard TEST): acting on the fact PRODUCES a player-facing
			//	result, and a load is not a cancellation -- it restores deals that were already struck. A
			//	load-path verify that drops a deal whose resource no longer resolves must not tell the player
			//	their deal was cancelled.
			//	⚠ It suppresses only the HANDLER; the fact itself always emits, so every other consumer and the
			//	log/stream still see it.
			if (spineGameLoadInProgress())
			{
				return;
			}
			switch (kEvent.iEventId)
			{
			case SEVT_EMPIRE_DEAL_REMOVED:
				onDealRemoved(kEvent);
				break;
			case SEVT_CITY_STATUS_REMOVED:
				onCityStatusRemoved(kEvent);
				break;
			default:
				break;
			}
		}
	};

	CvPlayerAlertConsumer s_playerAlertConsumer;
	bool s_bPlayerAlertsRegistered = false;
}

void playerAlertsRegisterConsumer()
{
	if (s_bPlayerAlertsRegistered)
	{
		return;
	}
	s_bPlayerAlertsRegistered = true;
	eventSpine().registerConsumer(&s_playerAlertConsumer);
}
