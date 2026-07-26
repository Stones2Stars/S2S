//
//	PlotContext -- the ONE derivation of the plot's stored CASC_PRED_* verdicts (see the header for the two blocks
//	and the fan-out rule), plus the forwards for the raw substrate a parameterized predicate keys on.
//
//	DEC-single-implementation: each verdict is derived by calling the SAME CvPlot accessor a read used to call --
//	once, at maintenance time, instead of once per read. No predicate's logic is re-implemented here.
//
//	CONSTRAINT: refreshAdjacencyFacts reads the NEIGHBOURS' state (CvPlot::isCoastalLand / isFreshWater), so it is
//	valid only once the map is settled -- CvPlot::area() is NULL for a plot whose area has not been assigned, and
//	isCoastalLand dereferences an adjacent water plot's area. The maintainer (Engine/ContextConsumer) holds that
//	guarantee: it defers every derivation until CvGame::isFinalInitialized().
//

#include "CvGameCoreDLL.h"
#include "PlotContext.h"
#include "CvPlot.h"

unsigned int PlotContext::ownFactsMask()
{
	return bitFor(CASC_PRED_IS_WATER)
		| bitFor(CASC_PRED_IS_LAND)
		| bitFor(CASC_PRED_IS_FLATLANDS)
		| bitFor(CASC_PRED_HAS_HILLS)
		| bitFor(CASC_PRED_HAS_PEAK)
		| bitFor(CASC_PRED_HAS_RIVER)
		| bitFor(CASC_PRED_HAS_IRRIGATION)
		| bitFor(CASC_PRED_HAS_FEATURE)
		| bitFor(CASC_PRED_HAS_LANDMARK)
		| bitFor(CASC_PRED_IS_OWNED)
		| bitFor(CASC_PRED_IS_WORKED);
}

unsigned int PlotContext::adjacencyFactsMask()
{
	return bitFor(CASC_PRED_HAS_COAST)
		| bitFor(CASC_PRED_HAS_FRESHWATER);
}

// Every stored bit EXCEPT IS_WORKED (see the header): a citizen taking or leaving a plot cannot move any
// neighbour's coast / fresh-water verdict, so it must not pay for the one-hop rescan.
unsigned int PlotContext::fanOutTriggerMask()
{
	return (ownFactsMask() | adjacencyFactsMask()) & ~bitFor(CASC_PRED_IS_WORKED);
}

// Re-derive the OWN-PLOT block from the bound plot alone.
void PlotContext::refreshOwnFacts() const
{
	unsigned int derivedBits = 0;

	if (m_plot != NULL)
	{
		const bool bWater = m_plot->isWater();
		const bool bHills = m_plot->isHills();
		const bool bPeak = m_plot->isPeak();

		if (bWater)
		{
			derivedBits |= bitFor(CASC_PRED_IS_WATER);
		}
		else
		{
			derivedBits |= bitFor(CASC_PRED_IS_LAND);
		}
		// Relief-free, NOT CvPlot::isFlatlands (which is the PLOT_LAND plot type): water carries no relief either,
		// json par.3.5 -- this is the verdict the forwarding read produced and it is preserved exactly.
		if (!bHills && !bPeak)
		{
			derivedBits |= bitFor(CASC_PRED_IS_FLATLANDS);
		}
		if (bHills)
		{
			derivedBits |= bitFor(CASC_PRED_HAS_HILLS);
		}
		if (bPeak)
		{
			derivedBits |= bitFor(CASC_PRED_HAS_PEAK);
		}
		if (m_plot->isRiver())
		{
			derivedBits |= bitFor(CASC_PRED_HAS_RIVER);
		}
		if (m_plot->isIrrigated())
		{
			derivedBits |= bitFor(CASC_PRED_HAS_IRRIGATION);
		}
		if (m_plot->getFeatureType() != NO_FEATURE)
		{
			derivedBits |= bitFor(CASC_PRED_HAS_FEATURE);
		}
		if (m_plot->getLandmarkType() != NO_LANDMARK)
		{
			derivedBits |= bitFor(CASC_PRED_HAS_LANDMARK);
		}
		if (m_plot->isOwned())
		{
			derivedBits |= bitFor(CASC_PRED_IS_OWNED);
		}
		if (m_plot->isBeingWorked())
		{
			derivedBits |= bitFor(CASC_PRED_IS_WORKED);
		}
	}

	m_attributeBits = (m_attributeBits & ~ownFactsMask()) | derivedBits;
}

// Re-derive the ADJACENCY block. Both verdicts read the neighbours through the plot's own accessors, so this is
// the leg that must run for the 8 neighbours of any plot whose block moved.
void PlotContext::refreshAdjacencyFacts() const
{
	unsigned int derivedBits = 0;

	if (m_plot != NULL)
	{
		if (m_plot->isCoastalLand())
		{
			derivedBits |= bitFor(CASC_PRED_HAS_COAST);
		}
		if (m_plot->isFreshWater() || m_plot->isRiver())
		{
			derivedBits |= bitFor(CASC_PRED_HAS_FRESHWATER);
		}
	}

	m_attributeBits = (m_attributeBits & ~adjacencyFactsMask()) | derivedBits;
}

// --- forwarded: the raw substrate CvPlot already holds O(1); a parameterized predicate keys on the id, and the one
// eventless verdict (city presence) has no trigger to maintain a bit from (see the header) -----------------------
bool PlotContext::hasFeature(int eFeature) const         { return m_plot != NULL && (int)m_plot->getFeatureType() == eFeature; }
bool PlotContext::hasTerrain(int eTerrain) const         { return m_plot != NULL && (int)m_plot->getTerrainType() == eTerrain; }
bool PlotContext::hasImprovement(int eImprovement) const { return m_plot != NULL && (int)m_plot->getImprovementType() == eImprovement; }
bool PlotContext::hasRoute(int eRoute) const             { return m_plot != NULL && (int)m_plot->getRouteType() == eRoute; }
bool PlotContext::hasBonus(int eBonus, int eTeam) const  { return m_plot != NULL && (int)m_plot->getBonusType((TeamTypes)eTeam) == eBonus; }
bool PlotContext::isCity() const         { return m_plot != NULL && m_plot->isCity(); }
int  PlotContext::owner() const          { return m_plot != NULL ? (int)m_plot->getOwner() : (int)NO_PLAYER; }
int  PlotContext::latitude() const       { return m_plot != NULL ? m_plot->getLatitude() : 0; }
int  PlotContext::natureYield(int eYield, int eTeam) const
{
	if (m_plot == NULL || eYield < 0)
	{
		return 0;
	}
	return m_plot->calculateNatureYield((YieldTypes)eYield, (TeamTypes)eTeam);
}
