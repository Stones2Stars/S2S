//
//	ContextConsumer -- the contexts' spine consumer and the ONE maintainer of the stored context state (see the
//	header for the trigger table, the fan-out gate, and the deferred-drain rule).
//

#include "CvGameCoreDLL.h"
#include "ContextConsumer.h"
#include "Spine/CvEventSpine.h"     // IEventConsumer / SEVT_* / spineGameLoadInProgress
#include "Engine/CvCity.h"          // onCityPlotChanged -- the ONE plotAttrs applier
#include "Engine/CvPlot.h"          // getPlotContext -- the stored verdict bitset
#include "AI/CvGameAI.h"            // GC.getGame() -- isFinalInitialized, the map-settled guard the adjacency derivation needs
#include "Engine/CvMap.h"           // plotByIndex / numPlots -- the event's iSrcLoc resolution
#include "Engine/CvGameCoreUtils.h" // plotDirection -- the 8-neighbour adjacency fan-out
#include "AI/CvPlayerAI.h"          // GET_PLAYER -- the (owner, cityId) resolution
#include "Defines/CvGlobals.h"      // GC
#include <vector>

namespace
{
	// One buffered in-read working-city fact: plot iSrcLoc + the assigned city's (owner, id). COUNT facts only
	// -- the drain re-reads the plot's stored bitset (the fold reads the fully-derived verdicts).
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
	ContextSpineConsumer() : m_bPlotMarksPending(false) {}

	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }

	// LOAD-ACTIVE: the in-read emits BUILD the aggregates (DEC-spine-reseed).
	void onEvent(const CvSpineEvent& kEvent)
	{
		// Any event is a valid moment to notice the map has settled: the marks taken while it was unsettled drain
		// here, before this event is processed, so a live derivation never runs against a half-built map. The one
		// exception is the load OPEN -- the map it would derive against is the outgoing game's, and the next
		// statement discards those marks anyway.
		if (kEvent.iEventId != SEVT_GAME_LOAD_STARTED)
		{
			drainPlotMarks();
		}

		switch (kEvent.iEventId)
		{
		case SEVT_GAME_LOAD_STARTED:
			m_bufferedFacts.clear();   // a fresh load: no stale facts from a previous stream
			clearPlotMarks();
			break;

		// ---- the plot SUBSTRATE facts: re-derive the announcing plot's stored verdicts ----
		case SEVT_TERRAIN_CHANGED:
		case SEVT_FEATURE_CHANGED:
		case SEVT_IMPROVEMENT_CHANGED:
		case SEVT_ROUTE_CHANGED:
		case SEVT_PLOT_BONUS_CHANGED:
		case SEVT_PLOT_OWNER_CHANGED:
		case SEVT_PLOT_TYPE_CHANGED:
		case SEVT_PLOT_RIVER_CHANGED:
		case SEVT_PLOT_IRRIGATION_CHANGED:
		case SEVT_PLOT_LANDMARK_CHANGED:
		case SEVT_PLOT_WORKED_CHANGED:
			refreshPlotByIndex(kEvent.iSrcLoc);
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
			// ONE applier against the fully-read map + cities, and against plot bitsets the drain above has
			// already completed. Unresolvable facts (a razed-mid-read city id) drop -- the same not-present
			// convention as the enabler's cityForEvent.
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
	// The single implementation of "this plot changed, re-derive it". Reached ONLY from onEvent: every trigger is a
	// spine DOMAIN fact, so there is one trigger path rather than an event surface beside a direct-call surface.
	void refreshPlot(const CvPlot* pPlot)
	{
		if (pPlot == NULL)
		{
			return;
		}
		if (!GC.getGame().isFinalInitialized() || !isAdjacencyDerivable(pPlot))
		{
			markPlot(pPlot);   // the map is unsettled: record the announcement, derive it at the drain
			return;
		}
		// Inside the load bracket the cities' plotAttrs are not seeded yet (the working-city drain seeds them at
		// LOAD_FINISHED, from the then-final bitsets), so the city refold must not run here.
		const bool bFoldIntoCity = !spineGameLoadInProgress();

		const bool bBlockMoved = applyPlotRefresh(pPlot, bFoldIntoCity);
		if (bBlockMoved)
		{
			fanOutAdjacency(pPlot, bFoldIntoCity);
		}
	}

	// CvPlot::isCoastalLand dereferences an ADJACENT water plot's CvArea, and CvPlot::isFreshWater reads the
	// neighbours through the same map, so the adjacency block is derivable only while every area a neighbour
	// points at exists. That is false during world generation, mid-save-read (the areas deserialize AFTER every
	// plot) and inside a CvMap::recalculateAreas window -- each of which resolves before the next spine event, so
	// a plot deferred here drains on that event.
	static bool isAdjacencyDerivable(const CvPlot* pPlot)
	{
		if (pPlot->area() == NULL)
		{
			return false;
		}
		for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
		{
			const CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), (DirectionTypes)iDirection);
			if (pAdjacentPlot != NULL && pAdjacentPlot->isWater() && pAdjacentPlot->area() == NULL)
			{
				return false;
			}
		}
		return true;
	}

	// Re-derive one plot's WHOLE block, keeping its working city's counts in step: the city's plotAttrs is the sum
	// of its member plots' bits, so the old bits unfold and the new ones refold around the derivation. Returns
	// whether a bit that a NEIGHBOUR's adjacency verdict reads moved -- the fan-out gate
	// (PlotContext::fanOutTriggerMask; IS_WORKED is excluded there, it can move no neighbour's verdict).
	bool applyPlotRefresh(const CvPlot* pPlot, bool bFoldIntoCity)
	{
		CvCity* pWorkingCity = bFoldIntoCity ? pPlot->getWorkingCity() : NULL;
		if (pWorkingCity != NULL)
		{
			pWorkingCity->onCityPlotChanged(pPlot, -1);
		}
		const unsigned int previousBits = pPlot->getPlotContext().attributeBits();
		pPlot->getPlotContext().refreshOwnFacts();
		pPlot->getPlotContext().refreshAdjacencyFacts();
		const unsigned int movedBits = pPlot->getPlotContext().attributeBits() ^ previousBits;
		if (pWorkingCity != NULL)
		{
			pWorkingCity->onCityPlotChanged(pPlot, +1);
		}
		return (movedBits & PlotContext::fanOutTriggerMask()) != 0;
	}

	// The ONE-HOP fan-out: only the ADJACENCY block of the 8 neighbours can be affected, and only by facts that
	// live in the changed plot's own block -- so this never recurses.
	void fanOutAdjacency(const CvPlot* pPlot, bool bFoldIntoCity)
	{
		for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
		{
			CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), (DirectionTypes)iDirection);
			if (pAdjacentPlot == NULL)
			{
				continue;
			}
			if (!isAdjacencyDerivable(pAdjacentPlot))
			{
				markPlot(pAdjacentPlot);   // its own neighbourhood is mid-flight: derive it at the drain instead
				continue;
			}
			CvCity* pWorkingCity = bFoldIntoCity ? pAdjacentPlot->getWorkingCity() : NULL;
			if (pWorkingCity != NULL)
			{
				pWorkingCity->onCityPlotChanged(pAdjacentPlot, -1);
			}
			pAdjacentPlot->getPlotContext().refreshAdjacencyFacts();
			if (pWorkingCity != NULL)
			{
				pWorkingCity->onCityPlotChanged(pAdjacentPlot, +1);
			}
		}
	}

	void refreshPlotByIndex(int iPlotIndex)
	{
		if (iPlotIndex < 0)
		{
			return;
		}
		refreshPlot(GC.getMap().plotByIndex(iPlotIndex));
	}

	// One byte per plot -- exact dedup with no per-announcement allocation, released again at the drain.
	void markPlot(const CvPlot* pPlot)
	{
		const int iNumPlots = GC.getMap().numPlots();
		if (iNumPlots <= 0)
		{
			return;
		}
		const int iPlotIndex = GC.getMap().plotNum(pPlot->getX(), pPlot->getY());
		if (iPlotIndex < 0 || iPlotIndex >= iNumPlots)
		{
			return;
		}
		if ((int)m_plotMarks.size() != iNumPlots)
		{
			m_plotMarks.assign((size_t)iNumPlots, (unsigned char)0);
		}
		m_plotMarks[(size_t)iPlotIndex] = 1;
		m_bPlotMarksPending = true;
	}

	void clearPlotMarks()
	{
		m_bPlotMarksPending = false;
		std::vector<unsigned char>().swap(m_plotMarks);
	}

	// Derive every marked plot's own AND adjacency block against the now-settled map. No fan-out is needed here:
	// every path that defers -- the save read, world generation, a map regeneration -- writes EVERY plot, so every
	// plot announced itself and every plot is marked; a neighbour is therefore always covered in its own right.
	// (Neighbours are deliberately not marked alongside, so a plot missing from the drain stays a visible
	// divergence instead of a silently patched one.)
	void drainPlotMarks()
	{
		if (!m_bPlotMarksPending)
		{
			return;
		}
		if (!GC.getGame().isFinalInitialized())
		{
			return;
		}
		// Same rule as the live path: inside the load bracket the cities' plotAttrs are seeded later, by the
		// working-city drain, from the bitsets this pass finalizes.
		const bool bFoldIntoCity = !spineGameLoadInProgress();
		bool bAnyMarkRetained = false;

		for (size_t iPlotIndex = 0; iPlotIndex < m_plotMarks.size(); ++iPlotIndex)
		{
			if (m_plotMarks[iPlotIndex] == 0)
			{
				continue;
			}
			const CvPlot* pPlot = GC.getMap().plotByIndex((int)iPlotIndex);
			if (pPlot == NULL)
			{
				m_plotMarks[iPlotIndex] = 0;
				continue;
			}
			const bool bAdjacencyReady = isAdjacencyDerivable(pPlot);
			CvCity* pWorkingCity = bFoldIntoCity ? pPlot->getWorkingCity() : NULL;
			if (pWorkingCity != NULL)
			{
				pWorkingCity->onCityPlotChanged(pPlot, -1);
			}
			// The OWN block reads this plot alone, so it always derives; only the adjacency block waits on a
			// settled neighbourhood, and a plot that still cannot derive it keeps its mark for the next pass.
			pPlot->getPlotContext().refreshOwnFacts();
			if (bAdjacencyReady)
			{
				pPlot->getPlotContext().refreshAdjacencyFacts();
			}
			if (pWorkingCity != NULL)
			{
				pWorkingCity->onCityPlotChanged(pPlot, +1);
			}

			if (bAdjacencyReady)
			{
				m_plotMarks[iPlotIndex] = 0;
			}
			else
			{
				bAnyMarkRetained = true;
			}
		}

		if (!bAnyMarkRetained)
		{
			clearPlotMarks();
		}
	}

	std::vector<ContextWorkingCityFact> m_bufferedFacts;
	std::vector<unsigned char> m_plotMarks;
	bool m_bPlotMarksPending;
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
