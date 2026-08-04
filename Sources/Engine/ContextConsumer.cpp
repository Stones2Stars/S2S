//
//	ContextConsumer -- the contexts' spine consumer and the ONE maintainer of the stored context state (see the
//	header for the trigger table, the fan-out gate, and the deferred-drain rule).
//

#include "CvGameCoreDLL.h"
#include "ContextConsumer.h"
#include "Spine/CvEventSpine.h"     // IEventConsumer / SEVT_* / spineGameLoadInProgress
#include "Engine/CvCity.h"          // onCityPlotChanged -- the ONE plotAttrs applier
#include "Engine/CvPlot.h"          // getPlotContext -- the stored verdict bitset
#include "Engine/CityContext.h"     // the city stores' refresh entry points
#include "AI/CvGameAI.h"            // GC.getGame() -- isFinalInitialized, the map-settled guard the adjacency derivation needs
#include "Engine/CvMap.h"           // plotByIndex / numPlots -- the event's iSrcLoc resolution
#include "Engine/CvGameCoreUtils.h" // plotDirection / plotCity -- the adjacency fan-out + the radius-city inverse
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

		// ---- the plot SUBSTRATE facts: re-derive the announcing plot's stored verdicts, then the CITY-scope blocks
		// that read the same tile. Both halves ride the ONE case per fact -- a second switch over the same events
		// would be a second maintenance surface for one announcement. ----
		case SEVT_TERRAIN_CHANGED:
		case SEVT_FEATURE_CHANGED:
		case SEVT_PLOT_RIVER_CHANGED:
		case SEVT_PLOT_IRRIGATION_CHANGED:
		case SEVT_PLOT_LANDMARK_CHANGED:
			refreshPlotByIndex(kEvent.iSrcLoc);
			break;
		// A radius tile's RESOURCE or OWNERSHIP moved -- re-derive the vicinity tiers of every city that can see it.
		// IMPROVEMENT and ROUTE ride the same leg: both feed CvPlot::isHasValidBonus / isConnectedTo, so the OBTAINED
		// (connected) tier can flip without the bonus itself moving.
		case SEVT_PLOT_BONUS_CHANGED:
		case SEVT_PLOT_OWNER_CHANGED:
		case SEVT_IMPROVEMENT_CHANGED:
		case SEVT_ROUTE_CHANGED:
			refreshPlotByIndex(kEvent.iSrcLoc);
			break;
		// A citizen took or left a tile. Only the WORKING city's worked tier can move -- a tile is worked by exactly
		// one city -- so this hottest of the plot facts costs one city, not a neighbourhood.
		case SEVT_PLOT_WORKED_CHANGED:
			refreshPlotByIndex(kEvent.iSrcLoc);
			break;
		// A tile's TYPE changed (land <-> ocean): the neighbourhood's coastal water-body size can move, and with it
		// which cities read as coastal.
		case SEVT_PLOT_TYPE_CHANGED:
			refreshPlotByIndex(kEvent.iSrcLoc);
			refreshAreaFactsAroundPlot(kEvent.iSrcLoc);
			break;

		// ---- the EMPIRE store: the enacted-policy union over the player's live civics + held traits ----
		// PLAYER_INIT matters in its own right: the initial trait assignment writes the has-array directly rather
		// than going through the trait setter, so the init fact is the only announcement those traits ever make.
		case SEVT_TRAIT_CHANGED:
		case SEVT_PLAYER_INIT:
			break;
		// A civic SWAP moves what the empire confers on every one of its cities (json §8), so the empire half of each
		// city's amenity fold is re-derived.
		// ⛔ Play-time ONLY. At load the civic facts fire from CvPlayer::read, and the TRAIT ones fire AFTER the
		// cities deserialize -- so an unguarded fan would double-count against the load build below, which derives
		// the owner's standing grantors per city once the stream has ended.

		// ⛔ The CAPITAL-MOVED and CITY-CHANGED-HANDS routes are NOT here: both moved amenity state, which
		// this consumer no longer owns. They are declared by the amenity context itself
		// (Engine/AmenityContext.h, [DEC-dict-is-a-consumer]).

		// ---- the CITY stores ----
		// ⛔ The VICINITY routes are gone with the radius walk, and NOTHING has replaced them yet: the tier
		// dictionaries are unmaintained and read empty. That is the deliberate exposed state -- they are rebuilt
		// driven by the plot facts' own payloads (the bonus +-1, the owner old->new, worked 0/1, the network
		// fact), never by an applier that re-reads the plot the fact just named.
		// ⛔ The AMENITY dictionary's BUILDING leg is NOT routed from here, and its absence is the DESIGN rather
		// than a missing case: that dictionary is its own spine consumer with its own declared interest set
		// (Engine/AmenityContext.h, [DEC-dict-is-a-consumer]). A store fed by a central switch cannot state what
		// maintains it -- the answer lives in the router instead of at the store -- which is exactly what this
		// file is being emptied of, one dictionary at a time.
		// A government centre appeared or went: EVERY city of that player re-measures its distance to the nearest
		// one, because the nearest may now be this city or may have just stopped being a centre. The fan-out is
		// the whole point -- one rare fact, one bounded re-derivation, instead of the per-read min-over-cities
		// walk the legacy distance formula did ([contexts.md]).
		case SEVT_GOVERNMENT_CENTER_CHANGED:
			refreshGovernmentCenterDistanceForPlayer(kEvent.iC);
			break;
		// A religion's holy city moved, or a corporation's headquarters did: ±1 on the city that gained or lost
		// it. The fact NAMES the designation, so nothing re-derives -- the previous route asked CvGame once per
		// entry of the whole religion registry, which is the read the store exists to delete.
		case SEVT_CITY_HOLY_CITY_ADDED:
			changeHolyCityCountFor(kEvent.iC, kEvent.iSrcLoc, +1);
			break;
		case SEVT_CITY_HOLY_CITY_REMOVED:
			changeHolyCityCountFor(kEvent.iC, kEvent.iSrcLoc, -1);
			break;
		case SEVT_CITY_HEADQUARTERS_ADDED:
			changeHeadquartersCountFor(kEvent.iC, kEvent.iSrcLoc, +1);
			break;
		case SEVT_CITY_HEADQUARTERS_REMOVED:
			changeHeadquartersCountFor(kEvent.iC, kEvent.iSrcLoc, -1);
			break;
		// EVERY area id was reassigned, so every city re-reads its area facts. Rare by construction (terrain levelled
		// to sea level, map generation), and not addressable per-source -- which is why it is announced wholesale.
		case SEVT_AREAS_RECALCULATED:
			refreshAreaFactsForAllCities();
			break;
		// A new city has no stored blocks yet: derive them all once, here.
		case SEVT_CITY_FOUNDED:
			refreshAllStoresForCity(kEvent.iC, kEvent.iSrcLoc);
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
			// THE LOAD BUILD of the city-scope blocks. LOAD is the only full build there is (state-repositories.md
			// CAPSTONE) -- after this, the blocks are maintained purely by the facts above and NOTHING sweeps them
			// again. It runs here rather than off each city's own in-read emits because the blocks read state that is
			// only complete once the whole stream has ended: the areas deserialize after the plots, and the
			// obtained-vicinity tier reads plot-group connectivity that the load-end network rebuild establishes.
			refreshAllStoresForAllCities();
			m_bufferedFacts.clear();
			break;
		}
		default:
			break;
		}
	}

private:
	// ---- the CITY / EMPIRE store maintenance -------------------------------------------------------------------
	// Every entry below is reached ONLY from onEvent. There is deliberately no periodic sweep, no staleness test and
	// no recompute-on-read behind any of them: a fact that fails to fire leaves the block visibly wrong, which is how
	// the missing emit gets found (DEC-no-self-heal).

	static CvCity* cityFor(int iOwner, int iCityId)
	{
		if (iOwner < 0 || iOwner >= MAX_PLAYERS || iCityId < 0)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)iOwner).getCity(iCityId);
	}

	static void changeHolyCityCountFor(int iOwner, int iCityId, int iChange)
	{
		const CvCity* pCity = cityFor(iOwner, iCityId);
		if (pCity != NULL)
		{
			pCity->getCityContext().changeHolyCityCount(iChange);
		}
	}

	static void changeHeadquartersCountFor(int iOwner, int iCityId, int iChange)
	{
		const CvCity* pCity = cityFor(iOwner, iCityId);
		if (pCity != NULL)
		{
			pCity->getCityContext().changeHeadquartersCount(iChange);
		}
	}

	// ⚖ THE VERDICT CROSSING IS THE FACT, so whoever owns the verdict announces it. That used to be
	// CvCity::changeGovernmentCenterCount (a per-flag counter emitting at its own 0-crossing); the verdict is now
	// the amenity FOLD, so the crossing is watched around every fold that can move it. ⛔ The fact itself does NOT
	// change — one emit, at the genuine change — which is what keeps the counter's removal from opening an event
	// gap ([DEC-close-event-gaps-now]). The AI work-dirty rider the deleted changers carried rides here too
	// (save.md §6: a deleted changer's side effects must survive at the surviving trigger).
	static void refreshAllStoresForCity(int iOwner, int iCityId)
	{
		const CvCity* pCity = cityFor(iOwner, iCityId);
		if (pCity == NULL)
		{
			return;
		}
		const CityContext& kContext = pCity->getCityContext();
		kContext.refreshAreaFacts();
		// ⛔ The holy-city and headquarters counts are NOT built here, and adding them back would be a bug: they
		// are delta stores fed ±1 by their own facts, which a founding city has none of yet and a loading city
		// has already had. A build pass beside a delta store double-counts.
		kContext.refreshGovernmentCenterDistance();
		// A city founded mid-game starts empty, so it folds the empire-scope grantors its owner ALREADY has --
		// the same half the load build covers, at the other moment a city starts existing. Its own buildings
		// arrive later as ordinary per-building facts.
	}

	// The RADIUS-CITY INVERSE: which cities can see this plot. The workable fat cross is symmetric, so the cities
	// that may hold this plot in radius sit at exactly the same offsets around it. Deliberately over-inclusive --
	// a candidate whose radius has since shrunk simply re-derives its own radius and finds nothing changed.
	template <class TAction>
	static void forEachRadiusCity(int iPlotIndex, TAction action)
	{
		if (iPlotIndex < 0)
		{
			return;
		}
		const CvPlot* pPlot = GC.getMap().plotByIndex(iPlotIndex);
		if (pPlot == NULL)
		{
			return;
		}
		for (int iRingIndex = 0; iRingIndex < NUM_CITY_PLOTS; ++iRingIndex)
		{
			const CvPlot* pCandidatePlot = plotCity(pPlot->getX(), pPlot->getY(), iRingIndex);
			if (pCandidatePlot == NULL)
			{
				continue;
			}
			const CvCity* pCity = pCandidatePlot->getPlotCity();
			if (pCity != NULL)
			{
				action(pCity->getCityContext());
			}
		}
	}

	struct RefreshAreaFacts
	{
		void operator()(const CityContext& kContext) const { kContext.refreshAreaFacts(); }
	};

	// A land/ocean flip moves the water-body size its NEIGHBOURS read, so the affected cities are those whose CENTRE
	// is adjacent to the changed plot -- covered by the same radius sweep, which is a superset of adjacency.
	static void refreshAreaFactsAroundPlot(int iPlotIndex)
	{
		forEachRadiusCity(iPlotIndex, RefreshAreaFacts());
	}

	// Every city of ONE player re-measures. A city gaining or losing government-centre status moves the answer
	// for all of its siblings, never only for itself.
	static void refreshGovernmentCenterDistanceForPlayer(int iOwner)
	{
		if (iOwner < 0 || iOwner >= MAX_PLAYERS)
		{
			return;
		}
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iOwner);
		if (!kPlayer.isAlive())
		{
			return;
		}
		int iLoop = 0;
		for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
		{
			pCity->getCityContext().refreshGovernmentCenterDistance();
		}
	}

	static void refreshAreaFactsForAllCities()
	{
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive())
			{
				continue;
			}
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				pCity->getCityContext().refreshAreaFacts();
			}
		}
	}

	// The LOAD build -- the one full pass over every city's blocks, run once at the end of the load stream.
	static void refreshAllStoresForAllCities()
	{
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive())
			{
				continue;
			}
			int iLoop = 0;
			for (const CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
			{
				const CityContext& kContext = pCity->getCityContext();
				kContext.refreshAreaFacts();
				// (no holy-city / headquarters build: both are delta stores, and CvCity::read already announced
				// every designation this city holds -- rebuilding here would double every one)
				// AFTER the loop would be wrong only if it read another city's store; it reads government-centre
				// STATUS, which the save read has already set on every city by now, so measuring here is safe.
				kContext.refreshGovernmentCenterDistance();
				// The BUILDING half of the amenity fold needs no pass here -- it is a delta off the per-building
				// facts, which the save read already emitted. The EMPIRE half does: the civic facts fired from
				// CvPlayer::read, before this city existed to fan to, so the city folds its owner's standing
				// civics once, here.
					}
		}
	}

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
