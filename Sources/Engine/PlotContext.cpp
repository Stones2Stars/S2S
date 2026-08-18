//
//	PlotContext -- the PER-BIT derivation table for the plot's stored CASC_PRED_* verdicts, and the store's own
//	spine consumer (see the header for the two blocks, the symmetric HAS_COAST ruling and the one-hop fan-out).
//
//	⛔ THE TABLE IS THE DESIGN. Each row states ONE bit: its id, the substrate AXES it reads, and its derivation.
//	The routing is then DERIVED from the table -- a fact re-derives exactly the rows whose axes it moved, and
//	nothing else. That is what replaces the whole-block re-derivation contexts.md bans by name, and it answers the
//	hazard the retired justification was right about (a hand-written per-event bit mask drifting from what the bits
//	actually read) by putting the dependency BESIDE the derivation instead of in a switch somewhere else.
//
//	docs/architecture/patterns.md §DRY (single implementation): every own-plot row derives by calling the SAME CvPlot accessor a read used to call --
//	once, when a fact that feeds it arrives, instead of once per read. No predicate's logic is re-implemented here.
//	The ONE row that does not call an accessor is HAS_COAST, and it does not because the accessor's whole content is
//	a neighbour walk this store already holds the answer to (the header's ruling).
//

#include "CvGameCoreDLL.h"
#include "PlotContext.h"
#include "CvPlot.h"
#include "CvMap.h"                  // plotByIndex / plotNum -- the fact's iSrcLoc resolution
#include "CvCity.h"                 // the fresh-water counter's city -> its plot
#include "AI/CvPlayerAI.h"          // GET_PLAYER -- a city fact names its owner, never a map index
#include "CvGameCoreUtils.h"        // plotDirection -- the one-hop neighbour fan-out
#include "CvImprovementInfo.h"      // isImprovementBonusTrade -- the SERVED-RESOURCE verdict's second leg
#include "Defines/CvGlobals.h"      // GC
#include "Spine/CvEventSpine.h"     // IEventConsumer / SEVT_* / the crossing emit
#include "Infos/CvClassificationIds.h"  // CLS_AMENITY_PROVIDES_FRESH_WATER -- which amenity crossing this reads

namespace
{
	// --- THE PER-BIT DERIVATIONS ------------------------------------------------------------------------------
	// Own-plot rows: one accessor call each, so the verdict is the engine's own and cannot drift from it.
	bool pc_deriveIsWater(const CvPlot* pPlot)       { return pPlot->isWater(); }
	bool pc_deriveIsLand(const CvPlot* pPlot)        { return !pPlot->isWater(); }
	// Relief-free, NOT CvPlot::isFlatlands (which is the PLOT_LAND plot type): water carries no relief either,
	// json par.3.5 -- this is the verdict the forwarding read produced and it is preserved exactly.
	bool pc_deriveIsFlatlands(const CvPlot* pPlot)   { return !pPlot->isHills() && !pPlot->isPeak(); }
	bool pc_deriveHasHills(const CvPlot* pPlot)      { return pPlot->isHills(); }
	bool pc_deriveHasPeak(const CvPlot* pPlot)       { return pPlot->isPeak(); }
	bool pc_deriveHasRiver(const CvPlot* pPlot)      { return pPlot->isRiver(); }
	bool pc_deriveHasIrrigation(const CvPlot* pPlot) { return pPlot->isIrrigated(); }
	bool pc_deriveHasFeature(const CvPlot* pPlot)    { return pPlot->getFeatureType() != NO_FEATURE; }
	bool pc_deriveHasLandmark(const CvPlot* pPlot)   { return pPlot->getLandmarkType() != NO_LANDMARK; }
	bool pc_deriveIsOwned(const CvPlot* pPlot)       { return pPlot->isOwned(); }
	bool pc_deriveIsWorked(const CvPlot* pPlot)      { return pPlot->isBeingWorked(); }

	// ⚖ HAS_COAST -- LAND WITH ADJACENT WATER, OR WATER WITH ADJACENT LAND (owner). Off the stored bits that is
	// one statement: a neighbour whose IS_WATER differs from mine, i.e. this plot sits on the land/water boundary.
	// ⛔ It reads the NEIGHBOURS' STORED BLOCKS, never a walk back through CvPlot ([contexts.md]) -- which is also
	// why nothing here touches CvArea, and why this file needs no unsettled-map deferral.
	// ⚠ It reads THIS plot's stored IS_WATER too, so the table's ORDER is load-bearing: the IS_WATER row sits above
	// this one and is therefore already committed when a TYPE fact reaches this row.
	bool pc_deriveHasCoast(const CvPlot* pPlot)
	{
		const bool bWater = pPlot->getPlotContext().isWater();
		for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
		{
			const CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), (DirectionTypes)iDirection);
			if (pAdjacentPlot != NULL && pAdjacentPlot->getPlotContext().isWater() != bWater)
			{
				return true;
			}
		}
		return false;
	}

	// ⚖ HAS_FRESHWATER keeps calling CvPlot::isFreshWater (the header states why: it is a seven-leg verdict the
	// ENGINE still consults for irrigation and farm gates, so a second expression of it would fork a live
	// predicate). What changed is the CADENCE -- it now runs only when a fact that actually feeds it arrives.
	// ⚠ TERRAIN IS THE ONE PRECONDITION: isFreshWater's first act is GC.getTerrainInfo(getTerrainType()), so a
	// plot whose terrain is not set yet (mid world-generation) must not be asked. Terrain is mandatory on every
	// plot and announces its own fact, which re-derives this row the moment it lands -- so this is a precondition
	// on ONE derivation, never a staleness mechanism (docs/cascade.md §Maintained EVENT-DRIVEN (a context is never marked)).
	bool pc_deriveHasFreshWater(const CvPlot* pPlot)
	{
		if (pPlot->getTerrainType() == NO_TERRAIN)
		{
			return false;
		}
		return pPlot->isFreshWater() || pPlot->isRiver();
	}

	// ⚖ THE SERVED-RESOURCE VERDICT -- which resource this tile makes available ON SITE, or -1.
	// Two legs and both are necessary: the tile CARRIES a bonus, and the improvement standing on it TRADES that
	// bonus. Either can move without the other, which is exactly why this is a verdict of its own rather than
	// something a consumer could read off the bonus fact.
	// ⚠ NO worked test and NO ownership test, deliberately. A fort cannot be worked by definition and a fort is
	// precisely how a resource gets served (owner); and ownership is a per-ASKER question -- "is this tile MY
	// owner's" has a different answer for each city that can work it, so no single per-plot verdict can hold it.
	// The CITY applies that half where it knows its own owner.
	// ⛔ The bonus is read UNFILTERED BY REVEAL (NO_TEAM), matching the vicinity store this feeds: these are
	// per-CITY live state, not a per-team view.
	int pc_deriveServedBonus(const CvPlot* pPlot)
	{
		const int iBonus = (int)pPlot->getBonusType(NO_TEAM);
		if (iBonus < 0)
		{
			return -1;
		}
		const ImprovementTypes eImprovement = pPlot->getImprovementType();
		if (eImprovement == NO_IMPROVEMENT)
		{
			return -1;
		}
		return GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(iBonus) ? iBonus : -1;
	}

	// The axes the served-resource verdict reads -- stated once, beside the derivation, exactly as a bit row states
	// its own ([contexts.md]: the dependency lives next to the derivation, never in a switch somewhere else).
	const int PLOT_SERVED_BONUS_AXES = PLOTAXIS_BONUS | PLOTAXIS_IMPROVEMENT;

	typedef bool (*PlotBitDerive)(const CvPlot*);

	struct PlotBitRule
	{
		int           iPredicateId;      // the CASC_PRED_* this row owns
		int           iAxes;             // the substrate axes whose movement re-derives it
		bool          bReadsNeighbours;  // an ADJACENCY row: re-derivable by a neighbour's move alone
		PlotBitDerive pfnDerive;
	};

	// ⚑ ORDER MATTERS ONCE, and only here: HAS_COAST reads this plot's own stored IS_WATER, so the own-plot rows
	// precede the adjacency rows. Everything else is independent.
	const PlotBitRule s_plotBitRules[] =
	{
		{ CASC_PRED_IS_WATER,       PLOTAXIS_TYPE,       false, pc_deriveIsWater       },
		{ CASC_PRED_IS_LAND,        PLOTAXIS_TYPE,       false, pc_deriveIsLand        },
		{ CASC_PRED_IS_FLATLANDS,   PLOTAXIS_TYPE,       false, pc_deriveIsFlatlands   },
		{ CASC_PRED_HAS_HILLS,      PLOTAXIS_TYPE,       false, pc_deriveHasHills      },
		{ CASC_PRED_HAS_PEAK,       PLOTAXIS_TYPE,       false, pc_deriveHasPeak       },
		{ CASC_PRED_HAS_RIVER,      PLOTAXIS_RIVER,      false, pc_deriveHasRiver      },
		{ CASC_PRED_HAS_IRRIGATION, PLOTAXIS_IRRIGATION, false, pc_deriveHasIrrigation },
		{ CASC_PRED_HAS_FEATURE,    PLOTAXIS_FEATURE,    false, pc_deriveHasFeature    },
		{ CASC_PRED_HAS_LANDMARK,   PLOTAXIS_LANDMARK,   false, pc_deriveHasLandmark   },
		{ CASC_PRED_IS_OWNED,       PLOTAXIS_OWNER,      false, pc_deriveIsOwned       },
		{ CASC_PRED_IS_WORKED,      PLOTAXIS_WORKED,     false, pc_deriveIsWorked      },
		// --- the ADJACENCY rows: derived from this plot AND its neighbours ---
		{ CASC_PRED_HAS_COAST,      PLOTAXIS_TYPE,       true,  pc_deriveHasCoast      },
		{ CASC_PRED_HAS_FRESHWATER,
		  PLOTAXIS_TYPE | PLOTAXIS_TERRAIN | PLOTAXIS_RIVER | PLOTAXIS_FEATURE | PLOTAXIS_CITY,
		                                                 true,  pc_deriveHasFreshWater }
	};

	const int NUM_PLOT_BIT_RULES = sizeof(s_plotBitRules) / sizeof(s_plotBitRules[0]);

	// The fact -> AXIS map. One axis per substrate fact PAIR: the DIRECTION is the fact's own id and never a
	// payload, so both halves of a pair land on the same axis and the row's derivation answers which way it went.
	int pc_axisFor(int iEventId)
	{
		switch (iEventId)
		{
		case SEVT_PLOT_TYPE_ADDED:
		case SEVT_PLOT_TYPE_REMOVED:         return PLOTAXIS_TYPE;
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_TERRAIN_REMOVED:      return PLOTAXIS_TERRAIN;
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_FEATURE_REMOVED:      return PLOTAXIS_FEATURE;
		case SEVT_PLOT_RIVER_ADDED:
		case SEVT_PLOT_RIVER_REMOVED:        return PLOTAXIS_RIVER;
		case SEVT_PLOT_IRRIGATION_ADDED:
		case SEVT_PLOT_IRRIGATION_REMOVED:   return PLOTAXIS_IRRIGATION;
		case SEVT_PLOT_LANDMARK_ADDED:
		case SEVT_PLOT_LANDMARK_REMOVED:     return PLOTAXIS_LANDMARK;
		case SEVT_PLOT_OWNER_ADDED:
		case SEVT_PLOT_OWNER_REMOVED:        return PLOTAXIS_OWNER;
		case SEVT_PLOT_WORKED_ADDED:
		case SEVT_PLOT_WORKED_REMOVED:       return PLOTAXIS_WORKED;
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:  return PLOTAXIS_IMPROVEMENT;
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_PLOT_BONUS_REMOVED:        return PLOTAXIS_BONUS;
		case SEVT_PLOT_CITY_ADDED:
		case SEVT_PLOT_CITY_REMOVED:         return PLOTAXIS_CITY;
		default:                             return 0;
		}
	}

	// ⚖ A CITY-SCOPE FACT THAT A PLOT VERDICT READS. `CvPlot::isFreshWater` consults the city STANDING ON the plot
	// (`hasFreshWater`, the provider-BUILDING access counter), so that counter crossing moves the plot's
	// HAS_FRESHWATER while no plot fact fires at all. Without this route the verdict goes stale with nothing to
	// re-derive it (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT) -- the missing-CONSUMER-ROUTE gap form, which has no other signature.
	// ⛔ It is kept SEPARATE from pc_axisFor because the two resolve their plot differently: a plot fact carries a
	// map INDEX in iSrcLoc, a city fact carries a city ID plus its owner in iC.
	// ⚠ The interest test is by EVENT ID only, so it admits every amenity crossing; WHICH amenity is filtered at
	// the apply (only PROVIDES_FRESH_WATER moves a plot verdict). Over-inclusion in an interest set is safe --
	// a miss is the bug ([enabler.md] §5) -- and the id is the fact's own payload, not a second lookup.
	int pc_cityAxisFor(int iEventId)
	{
		switch (iEventId)
		{
		case SEVT_CITY_AMENITY_ADDED:
		case SEVT_CITY_AMENITY_REMOVED:  return PLOTAXIS_CITY;
		default:                         return 0;
		}
	}

	class PlotContextSpineConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
		void onEvent(const CvSpineEvent& kEvent) { PlotContext::onSpineEvent(kEvent); }
	};

	PlotContextSpineConsumer s_plotContextConsumer;
	bool s_bPlotContextRegistered = false;
}

// ⚖ THE DECLARED INTEREST SET. Everything that maintains a plot's verdict bits is named here, at the store -- so
// a fact that does not reach it is visible HERE rather than inferable from a router (docs/cascade.md §What a context STORES vs FORWARDS (a dictionary is a spine consumer)).
// ⛔ SEVT_PLOT_PREDICATE_* is deliberately ABSENT: this store EMITS that fact, it does not consume it. Routing the
// fan-out on the AXIS instead of on a bit's own crossing is what bounds the fan-out to one hop and makes a cascade
// structurally impossible rather than merely avoided.
// ⚠ The set is NOT plot facts only -- a verdict reads what it reads, so the CITY fresh-water counter is in here too
// (pc_cityAxisFor). Scope is a property of how the fact RESOLVES its plot, never of whether it belongs.
bool PlotContext::wantsEvent(int iEventId)
{
	return pc_axisFor(iEventId) != 0 || pc_cityAxisFor(iEventId) != 0;
}

void PlotContext::onSpineEvent(const CvSpineEvent& kEvent)
{
	if (kEvent.iSrcLoc < 0)
	{
		return;
	}
	const CvPlot* pPlot = NULL;
	int iAxis = pc_axisFor(kEvent.iEventId);
	if (iAxis != 0)
	{
		pPlot = GC.getMap().plotByIndex(kEvent.iSrcLoc);
	}
	else
	{
		iAxis = pc_cityAxisFor(kEvent.iEventId);
		// ⛔ ONLY the fresh-water key moves a plot verdict. The generic amenity fact carries WHICH key in iType,
		// so the filter is a payload read rather than a second fact -- and without it every amenity crossing in
		// the city would re-derive its plot's bits for nothing.
		if (iAxis == 0 || kEvent.iC < 0 || kEvent.iType != CLS_AMENITY_PROVIDES_FRESH_WATER)
		{
			return;
		}
		const CvCity* pCity = GET_PLAYER((PlayerTypes)kEvent.iC).getCity(kEvent.iSrcLoc);
		pPlot = (pCity != NULL) ? pCity->plot() : NULL;
	}
	if (pPlot == NULL)
	{
		return;
	}
	pPlot->getPlotContext().applyAxes(iAxis);

	// THE ONE-HOP FAN-OUT. Only an axis a neighbour's adjacency verdict actually READS can move that neighbour, and
	// an adjacency verdict is itself read by nobody's adjacency verdict -- so this reaches exactly the 8 neighbours
	// and can never recurse. ⚑ It also makes the load order self-correcting: whichever plot of a boundary pair is
	// read second re-derives BOTH sides, so a stream that fills the map in any order converges with no drain pass.
	if ((iAxis & neighbourVisibleAxes()) != 0)
	{
		for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
		{
			const CvPlot* pAdjacentPlot = plotDirection(pPlot->getX(), pPlot->getY(), (DirectionTypes)iDirection);
			if (pAdjacentPlot != NULL)
			{
				pAdjacentPlot->getPlotContext().applyAdjacency();
			}
		}
	}
}

// Re-derive exactly the rows the moved axes feed. Never the whole block.
void PlotContext::applyAxes(int iAxisMask) const
{
	if (m_plot == NULL)
	{
		return;
	}
	for (int iRule = 0; iRule < NUM_PLOT_BIT_RULES; ++iRule)
	{
		if ((s_plotBitRules[iRule].iAxes & iAxisMask) != 0)
		{
			setPredicate(s_plotBitRules[iRule].iPredicateId, s_plotBitRules[iRule].pfnDerive(m_plot));
		}
	}
	// The served-resource verdict rides the same routing rule as every bit row -- re-derived exactly when an axis it
	// READS moves -- and differs only in holding an id rather than a bit, so it needs its own write point rather
	// than a row in the table.
	if ((PLOT_SERVED_BONUS_AXES & iAxisMask) != 0)
	{
		setServedBonus(pc_deriveServedBonus(m_plot));
	}
}

// A NEIGHBOUR moved: only the rows that read neighbours can have changed.
void PlotContext::applyAdjacency() const
{
	if (m_plot == NULL)
	{
		return;
	}
	for (int iRule = 0; iRule < NUM_PLOT_BIT_RULES; ++iRule)
	{
		if (s_plotBitRules[iRule].bReadsNeighbours)
		{
			setPredicate(s_plotBitRules[iRule].iPredicateId, s_plotBitRules[iRule].pfnDerive(m_plot));
		}
	}
}

// THE ONE WRITE POINT. Commits the bit and announces the CROSSING -- and only the crossing, so a derivation that
// lands on the value already held costs nothing and says nothing.
void PlotContext::setPredicate(int predicateId, bool bHeld) const
{
	const unsigned int iBit = bitFor(predicateId);
	if (iBit == 0 || m_plot == NULL)
	{
		return;
	}
	const bool bWasHeld = (m_attributeBits & iBit) != 0;
	if (bWasHeld == bHeld)
	{
		return;
	}
	m_attributeBits = bHeld ? (m_attributeBits | iBit) : (m_attributeBits & ~iBit);

	// The fact that carries this bit UP to the city ([contexts.md]: the plot sends it up the chain; the city never
	// reaches down for it). Emitted here because THIS is the maintenance path -- the store that owns the verdict
	// announces its crossing, exactly as the amenity fold does.
	const int iPlotIndex = GC.getMap().plotNum(m_plot->getX(), m_plot->getY());
	const int iOwner = (int)m_plot->getOwner();
	if (bHeld)
	{
		emitPlotPredicateAdded(iPlotIndex, iOwner, predicateId);
	}
	else
	{
		emitPlotPredicateRemoved(iPlotIndex, iOwner, predicateId);
	}
}

// THE SERVED-RESOURCE WRITE POINT, and the same contract as setPredicate: commit, then announce the CROSSING alone.
// ⚑ A tile MOVING from one served resource to another announces BOTH halves -- the old at REMOVED, the new at ADDED
// -- so a counting consumer withdraws exactly what it deposited and never has to reconstruct the old value
// ([state-repositories.md] § THE INVARIANT). A derivation landing on the id already held costs nothing and says
// nothing.
void PlotContext::setServedBonus(int iBonus) const
{
	if (m_plot == NULL || m_servedBonus == iBonus)
	{
		return;
	}
	const int iWasServing = m_servedBonus;
	m_servedBonus = iBonus;

	const int iPlotIndex = GC.getMap().plotNum(m_plot->getX(), m_plot->getY());
	const int iOwner = (int)m_plot->getOwner();
	if (iWasServing >= 0)
	{
		emitPlotServedBonusRemoved(iPlotIndex, iOwner, iWasServing);
	}
	if (iBonus >= 0)
	{
		emitPlotServedBonusAdded(iPlotIndex, iOwner, iBonus);
	}
}

// --- forwarded: the raw substrate CvPlot already holds O(1); a parameterized predicate keys on the id, and the one
// eventless verdict (city presence) has no trigger to maintain a bit from (see the header) -----------------------
bool PlotContext::hasFeature(int eFeature) const         { return m_plot != NULL && (int)m_plot->getFeatureType() == eFeature; }
bool PlotContext::hasTerrain(int eTerrain) const         { return m_plot != NULL && (int)m_plot->getTerrainType() == eTerrain; }
int  PlotContext::terrainId() const                      { return m_plot != NULL ? (int)m_plot->getTerrainType() : -1; }
bool PlotContext::hasImprovement(int eImprovement) const { return m_plot != NULL && (int)m_plot->getImprovementType() == eImprovement; }
bool PlotContext::hasRoute(int eRoute) const             { return m_plot != NULL && (int)m_plot->getRouteType() == eRoute; }
bool PlotContext::hasBonus(int eBonus, int eTeam) const  { return m_plot != NULL && (int)m_plot->getBonusType((TeamTypes)eTeam) == eBonus; }
bool PlotContext::isCity() const         { return m_plot != NULL && m_plot->isCity(); }
int  PlotContext::owner() const          { return m_plot != NULL ? (int)m_plot->getOwner() : (int)NO_PLAYER; }
int  PlotContext::latitude() const       { return m_plot != NULL ? m_plot->getLatitude() : 0; }
int  PlotContext::natureYield(int eYield) const
{
	if (m_plot == NULL || eYield < 0 || eYield >= NUM_YIELD_TYPES)
	{
		return 0;
	}
	// The plot's own package SEGMENT -- a bare fetch, never a per-call walk (contexts.md: the number is already
	// in the package). ×100 native, and a placement threshold is authored as a whole yield, so the single
	// reduce is here at the point of use (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
	int aiNature[NUM_YIELD_TYPES];
	m_plot->getNatureYields(aiNature);
	return aiNature[eYield] / 100;
}

// The realized-yield group forward: the bound plot's own group read, handed on unchanged -- no store, no mirror,
// no second derivation (contexts.md STORES vs FORWARDS). The out-array is FULLY DEFINED on every path, so an
// unbound context zero-fills rather than leaving caller memory untouched.
void PlotContext::yields(int (&realizedYields)[NUM_YIELD_TYPES]) const
{
	if (m_plot == NULL)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			realizedYields[iYield] = 0;
		}
		return;
	}
	m_plot->getYields(realizedYields);
}

void plotContextRegisterConsumer()
{
	if (s_bPlotContextRegistered)
	{
		return;
	}
	s_bPlotContextRegistered = true;
	eventSpine().registerConsumer(&s_plotContextConsumer);
}
