//
//	ContextConsumer -- the contexts' spine consumer (see the header): the CityContext.plotAttrs load reseed.
//	Buffers the load bracket's in-read SEVT_WORKING_CITY_CHANGED facts and drains them through the ONE applier
//	(CvCity::onCityPlotChanged) at SEVT_GAME_LOAD_FINISHED, once the cities are id-resolvable.
//

#include "CvGameCoreDLL.h"
#include "ContextConsumer.h"
#include "Spine/CvEventSpine.h"     // IEventConsumer / SEVT_* / spineGameLoadInProgress
#include "Engine/CvCity.h"          // onCityPlotChanged -- the ONE plotAttrs applier
#include "Engine/CvPlot.h"
#include "Engine/CvMap.h"           // plotByIndex -- the event's iSrcLoc resolution
#include "AI/CvPlayerAI.h"          // GET_PLAYER -- the (owner, cityId) resolution
#include "Defines/CvGlobals.h"      // GC
#include <vector>

namespace
{
	// One buffered in-read working-city fact: plot iSrcLoc + the assigned city's (owner, id). COUNT facts only
	// -- the drain re-reads the plot's attributes live (the fold reads the fully-deserialized substrate).
	struct ContextWorkingCityFact
	{
		int iPlotIndex;
		int iOwner;
		int iCityId;
	};
}

class ContextSpineConsumer : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }

	// LOAD-ACTIVE: the in-read emits BUILD the aggregate (DEC-spine-reseed). Outside the bracket the play-time
	// choke point (CvPlot::updateWorkingCity) has already folded the fact -- the consumer must not double-apply.
	void onEvent(const CvSpineEvent& kEvent)
	{
		switch (kEvent.iEventId)
		{
		case SEVT_GAME_LOAD_STARTED:
			m_bufferedFacts.clear();   // a fresh load: no stale facts from a previous stream
			break;
		case SEVT_WORKING_CITY_CHANGED:
		{
			if (!spineGameLoadInProgress())
			{
				break;   // play-time: the updateWorkingCity choke point applied the fold at the mutation
			}
			// the in-read fact (iA = old city, iB = new city, iC = owner, iSrcLoc = plot index): only the
			// ASSIGNMENT folds at load (a deserializing plot has no prior working city to unfold)
			if (kEvent.iB >= 0 && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS && kEvent.iSrcLoc >= 0)
			{
				ContextWorkingCityFact kFact;
				kFact.iPlotIndex = kEvent.iSrcLoc;
				kFact.iOwner = kEvent.iC;
				kFact.iCityId = kEvent.iB;
				m_bufferedFacts.push_back(kFact);
			}
			break;
		}
		case SEVT_GAME_LOAD_FINISHED:
		{
			// THE DRAIN (the "apply once after the stream ends" option): every buffered fact folds through the
			// ONE applier against the fully-read map + cities. Unresolvable facts (a razed-mid-read city id)
			// drop -- the same not-present convention as the enabler's cityForEvent.
			for (size_t iFact = 0; iFact < m_bufferedFacts.size(); ++iFact)
			{
				const ContextWorkingCityFact& kFact = m_bufferedFacts[iFact];
				CvCity* pCity = GET_PLAYER((PlayerTypes)kFact.iOwner).getCity(kFact.iCityId);
				if (pCity == NULL)
				{
					continue;
				}
				const CvPlot* pPlot = GC.getMap().plotByIndex(kFact.iPlotIndex);
				if (pPlot == NULL)
				{
					continue;
				}
				pCity->onCityPlotChanged(pPlot, +1);
			}
			m_bufferedFacts.clear();
			break;
		}
		default:
			break;
		}
	}

private:
	std::vector<ContextWorkingCityFact> m_bufferedFacts;
};

static ContextSpineConsumer s_contextConsumer;
static bool s_bContextConsumerRegistered = false;

void contextRegisterConsumer()
{
	if (s_bContextConsumerRegistered)
	{
		return;
	}
	s_bContextConsumerRegistered = true;
	eventSpine().registerConsumer(&s_contextConsumer);
}
