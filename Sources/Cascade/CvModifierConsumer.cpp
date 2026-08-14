//
//	CvModifierConsumer -- the modifier cascade's own spine consumer (see the header). The switch below names
//	only the event's SEMANTICS (which repo its iType indexes; which state a stateful event embodies); what a
//	source DEPOSITS is never decided here -- it comes from that source's own compiled entries, resolved through
//	the ONE per-entry resolve (MMKernel::resolveEntry).
//
//	⚖ A FACT APPLIES; NOTHING IS MARKED AND NOTHING IS REBUILT ([DEC-maintained-sum]). The DOMAIN fact names the
//	SOURCE and carries the DIRECTION as a signed multiplicity (+1 arriving, -1 leaving, ±N for a count), the
//	compiled index names that source's deposits, and applying them IS the maintenance -- so every slot is correct
//	at the instant the fact arrives, with nothing deferred and no load-bracket drain to order.
//	⛔ A MISSED EMIT therefore leaves a loud compounding error that nothing re-derives. That is the design, not a
//	weakness of it ([DEC-no-self-heal]): it is how the missing fact gets found.
//
//	⚠ WHAT IS NOT WIRED HERE, and is a HOLE rather than a decision: the COUNT route (plane B), and the ATOM route
//	(plane C) for every axis EXCEPT the two that are wired -- plot predicates (mc_applyPlotPredicate) and BONUS
//	atoms (mc_applyBonusAtom). Plane B still answers as a MASK, and a mask names channels while a channel has
//	nothing to apply; it lands when DepositIndex returns the DEPOSITS a count scales. Until then a deposit scaled
//	by a count -- or conditioned on an unrouted atom -- is not maintained when that count or atom moves.
//	⚑ The bonus axis is the worked example of what wiring one costs: the index was never the obstacle, the
//	PAST-TENSE withdrawal was, and the AS-IF-HELD hypothetical is what makes withdraw-then-apply exact.
//

#include "CvGameCoreDLL.h"
#include "Engine/CvGameSpeedScale.h"   // speedPercent -- the growth-threshold census term
#include "CvModifierConsumer.h"
#include "CvCascadePackage.h"
#include "CvCascadeChannelRegistry.h"
#include "Data/CvDepositIndex.h"        // routeFor + the dependency routes -- the ONE mark derivation
#include "Data/CvDepositRead.h"         // MMKernel::resolveEntry -- the ONE per-entry resolve
#include "Data/CvInfoValuation.h"       // the eval-ctx fill seam (the contexts ARE the eval state)
#include "Enabler/CvEnablerKernel.h"    // wireOperatingBuildings -- the THIRD leg of the eval state
#include "Spine/CvEventSpine.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvMap.h"
#include "Engine/CvGameCoreUtils.h"     // plotDirection -- the one-hop adjacency fan of the two adjacency verdicts
#include "Engine/CvPlot.h"
#include "Engine/CvCity.h"

namespace
{
	//	⚖ APPLY A PLOT SEGMENT, AND FOLD THE RESOLVED DELTA INTO THE PLOT'S WORKING CITY.
	//	This is the RESOLVE leg of the city's worked-plot Σ ([CvCascadePackage] plotBaseFlat): the city's
	//	`basePlotYield` is a maintained slot rather than a per-read ring walk, and the only exact delta available
	//	is the one the resolve itself moved -- the §2a floors make the slot non-linear in a DEPOSIT delta.
	//	⛔ Every plot-segment apply goes through here. A direct applyPlotSegment call would move the plot and
	//	leave the city's Σ short, permanently and silently ([DEC-no-self-heal]).
	//	⚠ WORKED, not owned: only a worked plot is in the city's base, so an unworked plot moves its own slot and
	//	folds nothing. Its value joins when the worked fact arrives (the MEMBERSHIP leg, applyWorkedPlot).
	void foldPlotSegment(const CvPlot& kPlot, CvCascadePackage<CvPlot>::PlotSegment eSegment,
	                     int iChannel, int64_t iValue)
	{
		const int64_t iResolvedDelta = kPlot.getCascadePackage().applyPlotSegment(eSegment, iChannel, iValue);
		if (iResolvedDelta == 0 || !kPlot.isBeingWorked())
		{
			return;
		}
		const CvCity* pWorkingCity = kPlot.getWorkingCity();
		if (pWorkingCity != NULL)
		{
			pWorkingCity->getPlotYields().applyPlotBaseFlat(iChannel, iResolvedDelta);
		}
	}
}
#include "Engine/CvUnit.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "AI/CvPlayerAI.h"              // GET_PLAYER
#include "AI/CvTeamAI.h"                // GET_TEAM
#include "CvBuildingInfo.h"
#include "CvReligionInfo.h"
#include "CvCorporationInfo.h"
#include "CvSpecialistInfo.h"
#include "CvBonusInfo.h"
#include "CvTechInfo.h"
#include "CvCivicInfo.h"
#include "CvProjectInfo.h"
#include "CvHeritageInfo.h"
#include "CvTraitInfo.h"
#include "CvImprovementInfo.h"
#include "CvTerrainInfo.h"
#include "CvFeatureInfo.h"
#include "CvRouteInfo.h"
#include "CvPropertyInfo.h"

namespace
{
	// ---- the ONE mark application: a derived route x the owner objects the event names ----

	// ⚖ THE TRADE-ROUTE RECOMPUTE TRIGGER -- TARGETED, at the owner an event actually hit (owner ruling).
	//
	// ⛔ THE TRADE-ROUTE YIELD IS THE ONE VALUE THE CASCADE FEEDS BUT DOES NOT HOLD. The engine owns the network
	// calculation, so the cascade supplies its INPUTS -- the route count and the profit / per-channel modifiers --
	// and `CvCity::m_aiTradeYield` is the engine's OUTPUT, folded into TIER-1 BASE at the combine
	// ([modifier.md] §2a). It is not a package slot, so the maintained sum does not reach it and no compiled
	// deposit index can name what moves it -- it is REBUILT rather than delta'd.
	// ⚑ THIS IS ONE OF ITS FOUR REBUILD MOMENTS, not the only one ([modifier.md] §2a carries the whole set): the
	// load-end pass, the plot-group / network changes, the per-player `doTurn`, and THIS -- targeted at whichever
	// owner a fact just moved a tradeRoutes channel for. What this route buys is MID-TURN precision for the
	// cascade-driven half; the fact names the owner, so the recompute follows the fact and never sweeps.
	// ⚠ It is DEFERRED to the end of the event rather than fired per deposit: one civic swap moves several
	// channels, and rebuilding a player's whole route network once per channel would pay the network walk many
	// times over for one happening.
	std::set<int> s_tradeRoutePendingOwners;

	bool mc_isTradeRouteChannel(int iChannel)
	{
		static std::set<int> s_channels;
		static bool s_built = false;
		if (!s_built)
		{
			for (int iKind = 0; iKind < (int)NUM_TRADE_ROUTE_KINDS; ++iKind)
			{
				const int iCh = CascadeChannelRegistry::channelLookup(MODFAM_TRADE_ROUTES, iKind, -1);
				if (iCh >= 0)
				{
					s_channels.insert(iCh);
				}
			}
			s_built = true;
		}
		return iChannel >= 0 && s_channels.find(iChannel) != s_channels.end();
	}

	// ⚖ THE PLOT YIELD-THRESHOLD IS AN OPERAND OF THE *RESOLVE*, NOT A DEPOSIT -- so when it moves, the plots it
	// governs must re-resolve. A trait gained or lost changes what
	// `CvPlayer::getExtraYieldThresholds` answers, and that fact names no plot at all: the consequence is
	// non-local, which is precisely the sanctioned event-triggered recalc ([contexts.md] -- a genuine DOMAIN
	// fact, a consequence the fact cannot name, and no finer route to derive).
	// ⛔ Not a rebuild: the plot's SEGMENTS are untouched and nothing is re-derived from a source; only the
	// non-linear step over the already-stored sums is recomputed.
	std::set<int> s_thresholdPendingOwners;

	bool mc_isPlotThresholdChannel(int iChannel)
	{
		static std::set<int> s_channels;
		static bool s_built = false;
		if (!s_built)
		{
			bool bAnswered = false;
			for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
			{
				const int iExtra = CascadeChannelRegistry::channelLookup(MODFAM_EXTRA_YIELD_THRESHOLD, iYield, -1);
				const int iLess = CascadeChannelRegistry::channelLookup(MODFAM_LESS_YIELD_THRESHOLD, iYield, -1);
				if (iExtra >= 0) { s_channels.insert(iExtra); bAnswered = true; }
				if (iLess >= 0) { s_channels.insert(iLess); bAnswered = true; }
			}
			s_built = bAnswered;
		}
		return iChannel >= 0 && s_channels.find(iChannel) != s_channels.end();
	}

	// ⛔ THE LOAD BUILD FOR THE THRESHOLD, AND ITS ABSENCE MADE THE WHOLE FOLD INERT. A plot resolves when its
	// substrate streams, and the MAP streams BEFORE the players -- so at that instant the plot's owner holds no
	// traits, the threshold reads 0, and the step is correctly not applied. Nothing then re-resolves the plot
	// once the traits arrive, so every plot keeps the answer it computed against an empty player.
	// ⚑ MEASURED before this: player 0 carried a production threshold of 700 with TEN worked plots above it in
	// one city, and the plot base gained exactly nothing.
	// ⚠ The play-time route is deliberately guarded to the non-load path (a per-fact mark during the reseed would
	// re-resolve the same plots once per streaming trait); this pass is its load-time twin, run once at the end
	// against final state -- the same shape the trade-route rebuild takes.
	void mc_markAllThresholdOwners()
	{
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			if (GET_PLAYER((PlayerTypes)iPlayer).isAlive())
			{
				s_thresholdPendingOwners.insert(iPlayer);
			}
		}
	}

	void mc_flushThresholdResolves()
	{
		if (s_thresholdPendingOwners.empty())
		{
			return;
		}
		// the base-yield channels are the only ones a threshold governs
		std::vector<int> yieldChannels;
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			const int iCh = CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1);
			if (iCh >= 0) { yieldChannels.push_back(iCh); }
		}
		for (std::set<int>::const_iterator it = s_thresholdPendingOwners.begin();
			it != s_thresholdPendingOwners.end(); ++it)
		{
			if (*it < 0 || *it >= MAX_PLAYERS) { continue; }
			const CvPlayer& kOwner = GET_PLAYER((PlayerTypes)*it);
			// The owner's two numbers, resolved ONCE per owner rather than per plot.
			// ⚠ The INTERVAL is the smallest positive one the owner holds, NOT a sum -- two sources at 7 and 5
			// mean "per 5", never "per 12" ([modifier.md] §2a). CvPlayer::updateExtraYieldThreshold already makes
			// that selection and reduces the ×100 authoring to whole units, so it is the correct feed and the
			// summed cascade roll-up is not.
			std::vector<int> aiInterval;
			for (size_t iCh = 0; iCh < yieldChannels.size(); ++iCh)
			{
				aiInterval.push_back(kOwner.getExtraYieldThreshold((YieldTypes)iCh));
			}
			// The AMOUNT each whole interval grants. It is a SEPARATE number by ruling and is meant to be movable
			// data; until an authoring surface carries it, the engine define supplies it (×100 for the plot plane).
			const int64_t iAmount = (int64_t)GC.getEXTRA_YIELD() * 100;
			// every plot the owner's cities can work -- the reach the scaling actually has on the yield plane
			std::set<const CvPlot*> kSeen;
			for (CvPlayer::city_iterator cityIterator = kOwner.beginCities();
				cityIterator != kOwner.endCities(); ++cityIterator)
			{
				const CvCity* pCity = *cityIterator;
				if (pCity == NULL) { continue; }
				const int iNumPlots = pCity->getNumCityPlots();
				for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
				{
					const CvPlot* pPlot = pCity->getCityIndexPlot(iPlotIndex);
					if (pPlot == NULL || kSeen.find(pPlot) != kSeen.end()) { continue; }
					kSeen.insert(pPlot);
					for (size_t iCh = 0; iCh < yieldChannels.size(); ++iCh)
					{
						// FEED the plot its two numbers; the package resolves itself off them. The plot never
						// reaches back for the owner -- that is the whole point of storing them here.
						pPlot->getCascadePackage().setYieldScaling(yieldChannels[iCh],
							(int64_t)aiInterval[iCh] * 100, (int64_t)iAmount);
					}
				}
			}
		}
		s_thresholdPendingOwners.clear();
	}

	void mc_noteChannelApplied(int iChannel, int iOwner)
	{
		if (iOwner >= 0 && mc_isPlotThresholdChannel(iChannel) && !spineGameLoadInProgress())
		{
			s_thresholdPendingOwners.insert(iOwner);
		}
		// Inside the load bracket the load-end pass rebuilds every player once, so noting here would only make
		// that pass run twice.
		if (iOwner >= 0 && mc_isTradeRouteChannel(iChannel) && !spineGameLoadInProgress())
		{
			s_tradeRoutePendingOwners.insert(iOwner);
		}
	}

	void mc_flushTradeRouteUpdates()
	{
		for (std::set<int>::const_iterator it = s_tradeRoutePendingOwners.begin();
			it != s_tradeRoutePendingOwners.end(); ++it)
		{
			if (*it >= 0 && *it < MAX_PLAYERS)
			{
				GET_PLAYER((PlayerTypes)*it).updateTradeRoutes();
			}
		}
		s_tradeRoutePendingOwners.clear();
	}

	// ⚖ THE CARRIER SLOT'S RESOLVE -- a source info back to its BUILDING id, for the one predicate class that
	// asks about the DEPOSITING entity rather than the target.
	// ⛔ AN ENTRY CANNOT NAME ITSELF ([contexts.md] § THE SOURCE SLOTS): neither a compiled entry nor an info
	// knows its own engine id, so a condition like `existedFor` -- how long has THIS building stood -- has no way
	// to ask unless the walk that knows the id puts it in the ctx. `ctx.sourceBuilding` defaults to -1 and the
	// predicate answers FALSE there, deliberately, because resolving it against whichever building the walk
	// reached last would be worse than declining.
	// ⚑ Built ONCE, lazily, from the building registry -- a pointer compare per lookup rather than a type-string
	// hash, and nothing to keep in step: infos do not reload mid-session.
	int mc_buildingIdOf(const CvInfo* pSource)
	{
		static std::map<const CvInfo*, int> s_buildingIds;
		static bool s_built = false;
		if (pSource == NULL)
		{
			return -1;
		}
		if (!s_built)
		{
			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
			{
				s_buildingIds[(const CvInfo*)&GC.getBuildingInfo((BuildingTypes)iBuilding)] = iBuilding;
			}
			s_built = true;
		}
		const std::map<const CvInfo*, int>::const_iterator it = s_buildingIds.find(pSource);
		return (it == s_buildingIds.end()) ? -1 : it->second;
	}

	// The plot-resident SOURCE a substrate fact names. (Its type-NAME twin is deleted: it existed only to key a
	// substrate type-atom route, and no authored condition anywhere names an improvement / terrain / feature /
	// route -- see the census at the substrate case in onEvent.)
	// returning the INFO rather than its name, because plane A applies the source's own compiled deposits and
	// needs the object. ONE id per fact (iType): _ADDED names what arrived, _REMOVED what left.
	const CvInfo* mc_substrateInfo(int iEventId, int iId)
	{
		if (iId < 0)
		{
			return NULL;
		}
		switch (iEventId)
		{
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
			return (iId < GC.getNumImprovementInfos()) ? &GC.getImprovementInfo((ImprovementTypes)iId) : NULL;
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_TERRAIN_REMOVED:
			return (iId < GC.getNumTerrainInfos()) ? &GC.getTerrainInfo((TerrainTypes)iId) : NULL;
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_FEATURE_REMOVED:
			return (iId < GC.getNumFeatureInfos()) ? &GC.getFeatureInfo((FeatureTypes)iId) : NULL;
		case SEVT_PLOT_ROUTE_ADDED:
		case SEVT_PLOT_ROUTE_REMOVED:
			return (iId < GC.getNumRouteInfos()) ? &GC.getRouteInfo((RouteTypes)iId) : NULL;
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_PLOT_BONUS_REMOVED:
			return (iId < GC.getNumBonusInfos()) ? &GC.getBonusInfo((BonusTypes)iId) : NULL;
		default:
			return NULL;
		}
	}

	// ---- PLANE A: THE SOURCE ROUTE -- a source arrived or left, so its deposits are applied ([DEC-maintained-sum]) ----
	//
	// iMultiplicity is SIGNED and is the whole of the direction: +1 for a source arriving, -1 for one leaving,
	// ±N where the fact moves a count (an owned-building count, a project count). resolveEntry multiplies the
	// resolved value by it, so a withdrawal is the same walk with the sign flipped -- there is no separate
	// removal path to keep in step.
	//
	// ⚠ WITHDRAWAL EXACTNESS IS AN EMIT-ORDERING CONTRACT, not something this function can enforce. The `per`
	// scaler resolves against the ctx AS IT STANDS, so a removal must be announced while the source's other
	// operands still hold the values its deposit was computed against (state-repositories.md § THE INVARIANT).
	// A fact emitted after the state moved withdraws a different number than it deposited and leaves a residue
	// nothing re-derives.

	// ⚖ THE SIGNED MULTIPLICITY -- +1 for a source arriving, -1 for one leaving, ±N where the fact carries a
	// count. This is the ONE place it is resolved, so a call site never repeats it and the two directions of one
	// fact cannot drift apart.
	// ⛔ Returning 0 is NOT a default to fall back on: it applies nothing. A source-carrying fact missing from
	// this switch would silently stop maintaining its deposits, so an unlisted one is a hole to close here,
	// never a case to let fall through.
	// ⛔ THE DIRECTION IS WHICH EVENT ARRIVED. Nothing here decodes a payload to find out which way a source
	// moved: this function used to carry THREE conventions at once -- an id pairing, a presence boolean in iA,
	// and a signed count delta in iB -- and all three went with the *_CHANGED facts that needed them
	// ([DEC-facts-name-happenings]). The payload is now read for HOW MANY and never for which way.
	int mc_sourceDirection(const CvSpineEvent& kEvent)
	{
		switch (kEvent.iEventId)
		{
		case SEVT_CITY_BUILDING_ACTIVATED:
		case SEVT_EMPIRE_BUILDING_ACTIVATED:
		case SEVT_CITY_BONUS_ADDED:
		case SEVT_CITY_RELIGION_ADDED:
		case SEVT_CITY_CORPORATION_ADDED:
		// The PLOT-RESIDENT sources. These ARE the yield base (modifier.md: the origin rule -- yields come from
		// plot, specialists and buildings, and a plot's package is the base every city rate is built on), so a
		// missing direction here is not a wrong number, it is a map with no yields at all.
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_ROUTE_ADDED:
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_CITY_POWER_ADDED:
		case SEVT_CITY_CORPORATION_ACTIVE_ADDED:
		case SEVT_CITY_HOLY_CITY_ADDED:
		case SEVT_CITY_OWNER_ADDED:
		case SEVT_PLOT_OWNER_ADDED:
		case SEVT_EMPIRE_CAPITAL_ADDED:
		case SEVT_EMPIRE_ERA_ADDED:
		case SEVT_WORLD_NUKES_BANNED_ADDED:
		case SEVT_EMPIRE_TECH_ADDED:
		case SEVT_EMPIRE_TRAIT_ADDED:
		case SEVT_EMPIRE_HERITAGE_ADDED:
		case SEVT_EMPIRE_GOLDEN_AGE_ADDED:
		case SEVT_EMPIRE_STATE_RELIGION_ADDED: return 1;

		case SEVT_CITY_BUILDING_DORMANTED:
		case SEVT_EMPIRE_BUILDING_DORMANTED:
		case SEVT_CITY_BONUS_REMOVED:
		case SEVT_CITY_RELIGION_REMOVED:
		case SEVT_CITY_CORPORATION_REMOVED:
		case SEVT_PLOT_TERRAIN_REMOVED:
		case SEVT_PLOT_FEATURE_REMOVED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
		case SEVT_PLOT_ROUTE_REMOVED:
		case SEVT_PLOT_BONUS_REMOVED:
		case SEVT_CITY_POWER_REMOVED:
		case SEVT_CITY_CORPORATION_ACTIVE_REMOVED:
		case SEVT_CITY_HOLY_CITY_REMOVED:
		case SEVT_CITY_OWNER_REMOVED:
		case SEVT_PLOT_OWNER_REMOVED:
		case SEVT_EMPIRE_CAPITAL_REMOVED:
		case SEVT_EMPIRE_ERA_REMOVED:
		case SEVT_WORLD_NUKES_BANNED_REMOVED:
		case SEVT_EMPIRE_TECH_REMOVED:
		case SEVT_EMPIRE_TRAIT_REMOVED:
		case SEVT_EMPIRE_HERITAGE_REMOVED:
		case SEVT_EMPIRE_GOLDEN_AGE_REMOVED:
		case SEVT_EMPIRE_STATE_RELIGION_REMOVED: return -1;

		// These carry a MAGNITUDE (iA), so the multiplicity is the payload and the SIGN is still the id --
		// "CITY_SPECIALIST_REMOVED 3" withdraws three times over ([event-spine.md]: the event is the operator).
		// ⚠ The VICINITY pair is here for its SIGN alone: it drives a gate re-check and deposits nothing, so its
		// count is read as a direction and never as a multiplicity.
		case SEVT_CITY_SPECIALIST_ADDED:
		case SEVT_CITY_POPULATION_ADDED:
		case SEVT_EMPIRE_PROJECT_ADDED:
		case SEVT_CITY_VICINITY_BONUS_ADDED:   return  kEvent.iA;

		case SEVT_CITY_SPECIALIST_REMOVED:
		case SEVT_CITY_POPULATION_REMOVED:
		case SEVT_EMPIRE_PROJECT_REMOVED:
		case SEVT_CITY_VICINITY_BONUS_REMOVED: return -kEvent.iA;

		default:                               return 0;
		}
	}

	// The plot SEGMENT a plot-resident source deposits into. The §2a floors make the resolved plot slot
	// non-linear, so it cannot itself be a maintained sum -- each SEGMENT is a plain sum and is what the delta
	// lands in (CvCascadePackage: applyPlotSegment re-derives the floored slot from the three).
	CvCascadePackage<CvPlot>::PlotSegment mc_plotSegmentFor(int iEventId)
	{
		switch (iEventId)
		{
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_TERRAIN_REMOVED:
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_FEATURE_REMOVED:
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_PLOT_BONUS_REMOVED:
			return CvCascadePackage<CvPlot>::PLOTSEG_NATURE;        // the PRE-improvement substrate
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
			return CvCascadePackage<CvPlot>::PLOTSEG_IMPROVEMENT;   // floored at -nature at resolve
		default:
			return CvCascadePackage<CvPlot>::PLOTSEG_REST;          // route + the owner's plot-scope flats
		}
	}

	// One city's share of a source's deposits. Its OWN eval ctx, so the entry's `per` scalers and conditions
	// resolve against THIS city -- which is the whole reason an above-city deposit is folded per city rather
	// than resolved once and handed out.
	// Apply ONE resolved entry into whichever yield plane its ORIGIN names. Templated because the planes are
	// DIFFERENT TYPES ([DEC-hard-typing-or-rollerskate]): no runtime reference could hold either, which is the
	// property that stops a specialist deposit ever reaching the building plane.
	template <class TPkg>
	void mc_applyValueToPlane(const TPkg& kPackage, int iChannel, bool bPercentSide, int64_t iValue)
	{
		if (bPercentSide)
		{
			kPackage.applyPercent(iChannel, (int)iValue);
		}
		else
		{
			kPackage.applyFlat(iChannel, iValue);
		}
	}

	// ⛔ THE BOOK HAS ONE HOME, WHATEVER PLANE THE VALUE LANDS ON. It is a per-entry record keyed by entry
	// pointer, not a yield slot -- and plane C (mc_rebookCity) looks it up to see what plane A already applied.
	// Split the book across the planes and plane C stops finding plane A's entry and applies a SECOND copy,
	// which is precisely the double-apply the booking exists to prevent ([DEC-maintained-sum]).
	void mc_bookCityEntry(const CvCity& city, const CvModEntry* pEntry, int iChannel, bool bPercentSide,
		int64_t iValue, int iMultiplicity)
	{
		if (pEntry->enabled == NULL && pEntry->disabled == NULL)
		{
			return;   // unconditioned: plane C never asks about it
		}
		const CvCascadePackage<CvCity, CASC_ORIGIN_BUILDING>::BookedDeposit kPrev =
			city.getBuildingYields().bookedDeposit(pEntry);
		city.getBuildingYields().setBookedDeposit(pEntry, iChannel, bPercentSide,
			(iMultiplicity > 0) ? (kPrev.iValue + iValue) : 0);
	}

	// ⛔ eOrigin decides WHICH yield plane these deposits join, and it is REQUIRED -- a new source kind must
	// state its plane rather than inherit one ([state-repositories.md] § THE ORIGIN RULE).
	void mc_applyCityDeposits(const std::vector<CvModEntry*>& entries, int iMultiplicity, int iSourceIndex, const CvCity& city,
		const char* szSource, const char* szOnFact, int iSourceBuilding, CvCascOrigin eOrigin)
	{
		CvCascadeEvalCtx evalCtx;
		// ⛔ THE CTX MUST CARRY THE PLOT GROUP **AND** THE ENABLER'S OPERATING SET, or the conditioned half of
		// every deposit silently evaluates to FALSE and never applies. An under-filled ctx does not error and
		// does not warn: a `connection:"trade"` atom with no plot group, and a BUILDING_/vicinity-provides atom
		// with no active set, both simply answer NO ([triggers.md]: "the operating-set legs sit EMPTY and any
		// condition asking an active-building or vicinity-provides question evaluates against nothing and
		// quietly answers false").
		// ⚑ It is the APPLY path, so the loss is permanent rather than momentary — the deposit is never added to
		// the package at all, and nothing re-derives it ([DEC-no-self-heal]). BOTH legs are wired here for that
		// reason, not as a fix for any particular channel: adding the second moved no observed rate, so do NOT
		// read it as the cause of a divergence.
		InfoValuation::fillEvalCtx(city.getCityContext(), GET_PLAYER(city.getOwner()).getEmpireContext(),
			city.plotGroup(city.getOwner()), evalCtx);
		EnablerKernel::wireOperatingBuildings(&city, evalCtx);
		// THE CARRIER SLOT. A source-relative predicate (`existedFor` -- how long has THIS building stood) can only
		// be answered by the walk that knows the depositing entity's id, and it answers FALSE when nothing set it
		// ([contexts.md] § THE SOURCE SLOTS). Every walk that resolves a building's entries must therefore set
		// it: unset here, every age-gated deposit in the class silently resolves false.
		evalCtx.sourceBuilding = iSourceBuilding;
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (pEntry == NULL || !MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_CITY,
				evalCtx, NULL, false, iChannel, bPercentSide, iValue))
			{
				continue;
			}
			// THE ORIGIN DECIDES THE PLANE, BUT ONLY FOR THE FLATS. A specialist's yield is TIER 1 and a
			// building's is TIER 2 ([modifier.md] 2a), and a summed slot cannot tell them apart afterwards --
			// so the FLATS never share a plane.
			// ⛔ A PERCENT AUTHORED BY A SPECIALIST IS AN ORDINARY CITY MODIFIER (owner) and joins the ONE
			// additive stack exactly like every other source's -- it is NOT confined to the specialist's own
			// yields. ⚠ Do not confuse it with a percent TARGETING specialists, which is what scales the
			// specialist term; that is a KEYED deposit and a different mechanism.
			if (bPercentSide)
			{
				// EVERY source's percent joins the ONE additive stack -- that is its combine position, and it
				// has no origin to keep apart.
				mc_applyValueToPlane(city.getCityPercents(), iChannel, bPercentSide, iValue);
			}
			else if (eOrigin == CASC_ORIGIN_SPECIALIST)
			{
				mc_applyValueToPlane(city.getSpecialistYields(), iChannel, bPercentSide, iValue);
			}
			else
			{
				mc_applyValueToPlane(city.getBuildingYields(), iChannel, bPercentSide, iValue);
			}
			mc_bookCityEntry(city, pEntry, iChannel, bPercentSide, iValue, iMultiplicity);
			mc_noteChannelApplied(iChannel, (int)city.getOwner());
			// The per-source ATTRIBUTION line (DIAGNOSTIC, level 3 -- free until asked for).
			CascadeChannelRegistry::reportDepositApply(szSource, iChannel, CASC_SCOPE_CITY, bPercentSide,
				iValue, (int)city.getOwner(), city.getID(), szOnFact);
		}
		// A specialist reaches BOTH planes -- its flats the specialist plane, its percents the city stack on
		// the building plane -- so it is noted on both, or a later withdrawal would not find one of them.
		if (eOrigin == CASC_ORIGIN_SPECIALIST)
		{
			city.getSpecialistYields().noteSourceApplied(iSourceIndex, iMultiplicity);
		}
		city.getBuildingYields().noteSourceApplied(iSourceIndex, iMultiplicity);
	}

	// A `plots`-TARGET deposit, resolved once PER PLOT so the entry's own filter (`enabled: IS_WATER`) decides
	// which tiles take it (modifier.md §5).
	//
	// ⛔ eEntryScope IS THE WHOLE CORRECTNESS OF THIS FAN, and getting it wrong is not subtle: a CITY-scope entry
	// belongs to the plots of the city HOLDING the source, an EMPIRE-scope one to every city's. Admitting both
	// here made every Pier in the empire buff every city's tiles -- measured at ~231 food per plot, on land as
	// well as water. The authored scope decides WHOSE plots; the entry's predicate decides WHICH.
	//
	// ⚖ THE TARGET IS THE PLOT'S **WORKING** CITY, never every city whose radius covers it. Radii overlap, so
	// fanning over the radius would deposit a shared tile twice; the spec's own example is "every WORKED water
	// plot", which is exactly this relation, and it rides a fact that already exists
	// (SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED).
	// ⚑ It lands in PLOTSEG_REST -- documented as "route + the OWNER'S PLOT-SCOPE FLATS", precisely this class.
	// ⚖ THE LOAD-BRACKET BUFFER FOR `plots` DEPOSITS -- an ORDERING fact, never a staleness mechanism.
	//
	// ⛔ A `plots` fan CANNOT run during the save read, and this is the defect it exists to close: the empire-scope
	// grantors announce from `CvPlayer::read`, which streams BEFORE the cities deserialize ([contexts.md]: "at load
	// the civic facts fire from CvPlayer::read BEFORE the cities deserialize, so there is no city to fan to"). The
	// fan iterated an EMPTY city list and every trait/civic water-plot deposit was applied to nothing and lost --
	// silently, because applying to no owner raises nothing. Only the CITY leg worked, because a building fact
	// comes from `CvCity::read` where its plots already exist.
	//
	// ⚑ So the load bracket's `plots` facts are BANKED and drained once at GAME_LOAD_FINISHED, when every city and
	// plot stands -- the identical buffer-then-fold CityContext uses for its own membership facts, for the same
	// reason. ⛔ It is NOT a deferred recompute: what is banked is the FACT (its source + signed multiplicity), and
	// the drain applies exactly the deposits that fact names ([DEC-maintained-sum]). Nothing is re-derived from
	// live state.
	struct PlotsFanFact
	{
		const CvInfo* pSource;
		int iMultiplicity;
		PlayerTypes eOwner;
		int iCityId;        // -1 = the source is not city-bound (an empire grantor)
	};
	std::vector<PlotsFanFact> s_bankedPlotsFans;

	// ⛔ THE SOURCE'S OWN ENTRIES ARE SELECTED **ONCE**, BEFORE ANY OWNER IS WALKED -- a consumer applies exactly
	// the deposits the fact names and never sweeps a scope to find out whether it cares
	// ([DEC-maintained-sum]: "emit liberally, apply precisely"). The overwhelming majority of sources author no
	// `plots` deposit at all, and for those this must cost NOTHING: selecting inside the plot loop instead made
	// every building fact walk every city's plots to discover it had nothing to do, which is a blanket sweep
	// wearing an event's clothes.
	// Returns false when this source has no such deposit at this scope, so the caller skips the fan entirely.
	bool mc_selectPlotsTargetEntries(const std::vector<CvModEntry*>& entries, CvCascScope eEntryScope,
		std::vector<const CvModEntry*>& selected)
	{
		static int s_iPlotsSeg = -2;
		if (s_iPlotsSeg == -2)
		{
			s_iPlotsSeg = modSegmentLookup(std::string("plots"));
		}
		selected.clear();
		if (s_iPlotsSeg < 0)
		{
			return false;   // no entity anywhere authored a `plots` target
		}
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			if (pEntry != NULL && pEntry->scope == (int)eEntryScope && pEntry->targetSeg == s_iPlotsSeg)
			{
				selected.push_back(pEntry);
			}
		}
		return !selected.empty();
	}

	// Returns how many PLOTS actually took a deposit -- the fan's only readback, since no served surface carries a
	// plot package (CascadeChannelRegistry::reportPlotsFan). A zero here with a non-empty `selected` is the
	// signature of an entry that SELECTED but never RESOLVED, which nothing else in the engine would show.
	int g_iFanResolved = 0;   // scratch: how many (plot × entry) pairs resolveEntry accepted, per reported fan

	// ONE plot's application of a source's selected `plots` entries -- the shared body of the source-move fan
	// below and the WORKING-CITY membership fold. iMultiplicity is signed: the fan passes the source's own
	// direction, the membership fold ±1 per crossing.
	bool mc_applyPlotsEntriesToPlot(const std::vector<const CvModEntry*>& selected, int iMultiplicity,
		int iSourceIndex, const CvPlot& kPlot, PlayerTypes eReportOwner, int iReportCity)
	{
		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtxAtPlot(kPlot, evalCtx);
		bool bApplied = false;
		for (size_t iEntry = 0; iEntry < selected.size(); ++iEntry)
		{
			const CvModEntry* pEntry = selected[iEntry];
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (!MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_PLOT,
				evalCtx, &kPlot, false, iChannel, bPercentSide, iValue))
			{
				continue;
			}
			++g_iFanResolved;
			// the ORIGIN RULE: plot is yield-only, so a plot-landing percent has no side to land on
			if (!bPercentSide)
			{
				foldPlotSegment(kPlot,
					CvCascadePackage<CvPlot>::PLOTSEG_REST, iChannel, iValue);
				CascadeChannelRegistry::reportDepositApply("<plotsFan>", iChannel, CASC_SCOPE_PLOT,
					false, iValue, (int)eReportOwner, iReportCity, "plotsFan");
				bApplied = true;
			}
		}
		if (bApplied)
		{
			kPlot.getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
		}
		return bApplied;
	}

	int mc_applyPlotsTargetDeposits(const std::vector<const CvModEntry*>& selected, int iMultiplicity,
		int iSourceIndex, const CvCity& city)
	{
		int iPlotsApplied = 0;
		const int iNumPlots = city.getNumCityPlots();
		for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
		{
			CvPlot* pLoopPlot = city.getCityIndexPlot(iPlotIndex);
			if (pLoopPlot == NULL || pLoopPlot->getWorkingCity() != &city)
			{
				continue;
			}
			if (mc_applyPlotsEntriesToPlot(selected, iMultiplicity, iSourceIndex, *pLoopPlot,
				city.getOwner(), city.getID()))
			{
				++iPlotsApplied;
			}
		}
		return iPlotsApplied;
	}

	// Drain the load bracket's banked `plots` facts, ONCE, with every city and plot standing. Each banked fact
	// applies exactly the deposits it named -- the same two legs the play-time path runs, in the same order.
	void mc_drainBankedPlotsFans()
	{
		if (s_bankedPlotsFans.empty())
		{
			return;
		}
		for (size_t iFact = 0; iFact < s_bankedPlotsFans.size(); ++iFact)
		{
			const PlotsFanFact& kFact = s_bankedPlotsFans[iFact];
			if (kFact.pSource == NULL || kFact.eOwner == NO_PLAYER)
			{
				continue;
			}
			const CvModifiers* pModifiers = kFact.pSource->getModifiers();
			if (pModifiers == NULL || pModifiers->empty())
			{
				continue;
			}
			const std::vector<CvModEntry*>& entries = pModifiers->entries();
			const int iSourceIndex = DepositIndex::sourceIndexOf(kFact.pSource);
			const CvPlayer& kOwner = GET_PLAYER(kFact.eOwner);

			std::vector<const CvModEntry*> selected;
			if (kFact.iCityId >= 0 && mc_selectPlotsTargetEntries(entries, CASC_SCOPE_CITY, selected))
			{
				const CvCity* pCity = kOwner.getCity(kFact.iCityId);
				int iPlots = 0;
				g_iFanResolved = 0;
				if (pCity != NULL)
				{
					iPlots = mc_applyPlotsTargetDeposits(selected, kFact.iMultiplicity, iSourceIndex, *pCity);
				}
				CascadeChannelRegistry::reportPlotsFan(kFact.pSource->getType(), CASC_SCOPE_CITY,
					(int)selected.size(), (pCity != NULL) ? 1 : 0, iPlots,
					g_iFanResolved, kFact.iMultiplicity);
			}
			if (mc_selectPlotsTargetEntries(entries, CASC_SCOPE_EMPIRE, selected))
			{
				int iCities = 0;
				int iPlots = 0;
				g_iFanResolved = 0;
				for (CvPlayer::city_iterator cityIterator = kOwner.beginCities();
					cityIterator != kOwner.endCities(); ++cityIterator)
				{
					if (*cityIterator != NULL)
					{
						++iCities;
						iPlots += mc_applyPlotsTargetDeposits(selected, kFact.iMultiplicity, iSourceIndex, **cityIterator);
					}
				}
				CascadeChannelRegistry::reportPlotsFan(kFact.pSource->getType(), CASC_SCOPE_EMPIRE,
					(int)selected.size(), iCities, iPlots,
					g_iFanResolved, kFact.iMultiplicity);
			}
		}
		s_bankedPlotsFans.clear();
		std::vector<PlotsFanFact>().swap(s_bankedPlotsFans);   // the bank is a load-time scratch, not resident state
	}

	// ONE-SHOT growth census: every term of the two numbers a city grows on (the threshold and the consumption),
	// for the ACTIVE player's cities. Neither quantity is on any served surface, so a report that cities need
	// less food than they used to cannot otherwise be attributed to a term ([validation.md]: a value not on the
	// surface is not verifiable, and emitting it is step one of its fix). DIAGNOSTIC, so it costs nothing until
	// the stream gate asks for it, and nothing builds state from it.
	void mc_reportGrowthCensus()
	{
		// ⛔ EVERY player, each line NAMING its owner. Walking getActivePlayer() alone reads whichever player the
		// save happens to sit on -- which is not necessarily the HUMAN, and the AI handicap discount applies only
		// to an AI (isNormalAI), so a census of the wrong player attributes a legitimate discount to a defect.
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
		{
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!kPlayer.isAlive())
			{
				continue;
			}
			const int iSpeedPercent = CvGameSpeedScale::speedPercent();
			const int iEraPercent = GC.getEraInfo(kPlayer.getCurrentEra()).getScalar(
				SCALAR_GROWTH, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT);
			for (CvPlayer::city_iterator cityIterator = kPlayer.beginCities();
				cityIterator != kPlayer.endCities(); ++cityIterator)
			{
				const CvCity* pCity = *cityIterator;
				if (pCity == NULL)
				{
					continue;
				}
				CascadeChannelRegistry::reportGrowthRead(
					iPlayer, kPlayer.isHuman() ? 1 : 0,
					pCity->getID(), pCity->getPopulation(), pCity->getFood(), pCity->growthThreshold(),
					iSpeedPercent, iEraPercent, kPlayer.getGrowthThreshold(pCity->getPopulation()),
					pCity->foodConsumption(), pCity->getFoodConsumedPerPopulation(), pCity->foodDifference(),
					GC.getDefineINT("BASE_CITY_GROWTH_THRESHOLD"), GC.getDefineINT("CITY_GROWTH_MULTIPLIER"),
					kPlayer.isNormalAI() ? 1 : 0, kPlayer.isGoldenAge() ? 1 : 0);
				// ⚑ The RATE beside the threshold, decomposed. The two numbers a city grows on are what it NEEDS
				// and what it MAKES; the census carried only the first, so a food deficit could be attributed to
				// the threshold and never to a term of the rate.
				// ⛔ The terms come OUT of the real combine rather than being re-derived here
				// ([DEC-single-implementation]) -- a census that recomputed its own decomposition could disagree
				// with the value it claims to explain, which is the one thing it must never do.
				for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
				{
					const int iChannel = CascadeChannelRegistry::channelLookup(
						infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1);
					if (iChannel < 0)
					{
						continue;
					}
					InfoValuation::CityRateTerms kTerms;
					InfoValuation::cityReceiverRate(*pCity, iChannel, &kTerms);
					CascadeChannelRegistry::reportRateRead(
						iPlayer, kPlayer.isHuman() ? 1 : 0, pCity->getID(), iChannel,
						(int)kTerms.plotBase, (int)kTerms.plotNature, (int)kTerms.plotImprovement,
						(int)kTerms.plotRest, (int)kTerms.tradeYield, (int)kTerms.goldenAge,
						(int)kTerms.upperFlat, (int)kTerms.specialists, (int)kTerms.cityFlat,
						kTerms.percentSum, kTerms.workedPlots, (int)kTerms.rate);
					// ⚑ The `specialists` term DECOMPOSED, one line per held type. It reaches the line above as a
					// single int over every type at once, so a term that is short -- or that disagrees with what
					// the package plane holds for the same channel -- could be attributed to nothing.
					for (size_t iRow = 0; iRow < kTerms.specialistRows.size(); ++iRow)
					{
						const InfoValuation::SpecialistTermRow& kRow = kTerms.specialistRows[iRow];
						CascadeChannelRegistry::reportSpecialistRead(
							iPlayer, pCity->getID(), iChannel, kRow.specialist,
							kRow.assigned, kRow.freeTyped, (int)kRow.perUnit, (int)kRow.contribution);
					}
					// ⚑ The TRAIT IMPROVEMENT leg, one line per (trait x improvement). Unlike the rows above this
					// decomposes NO field of rateRead -- the leg joins BASE after `plotBase` is captured, so it
					// appears in no term at all and the terms do not sum to the rate without it.
					for (size_t iRow = 0; iRow < kTerms.traitImprovementRows.size(); ++iRow)
					{
						const InfoValuation::TraitImprovementRow& kTraitRow = kTerms.traitImprovementRows[iRow];
						CascadeChannelRegistry::reportTraitImprovementRead(
							iPlayer, pCity->getID(), iChannel, kTraitRow.trait, kTraitRow.improvement,
							kTraitRow.workedTiles, (int)kTraitRow.perTile, (int)kTraitRow.contribution);
					}
				}
			}
		}
	}

	// Apply ONE source's compiled deposits into every package they feed. The scopes a source reaches come from
	// its own entries (resolveEntry declines any entry not at the scope being applied), so nothing here decides
	// what a source deposits -- only WHERE the owner objects are.
	// ⛔ eOrigin is REQUIRED and has NO DEFAULT: a new yield source must state which plane it deposits into
	// ([DEC-hard-typing-or-rollerskate]). A default would silently send the next provider to the building plane,
	// which is the exact bug this split exists to end.
	void mc_applySourceDeposits(const CvInfo* pSourceInfo, int iMultiplicity,
		const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot,
		CvCascadePackage<CvPlot>::PlotSegment ePlotSegment, const char* szOnFact, CvCascOrigin eOrigin)
	{
		if (pSourceInfo == NULL || iMultiplicity == 0)
		{
			return;
		}
		const CvModifiers* pModifiers = pSourceInfo->getModifiers();
		if (pModifiers == NULL || pModifiers->empty())
		{
			return;
		}
		const std::vector<CvModEntry*>& entries = pModifiers->entries();
		// the apply path's own liveness key -- resolved once, recorded wherever these deposits land
		const int iSourceIndex = DepositIndex::sourceIndexOf(pSourceInfo);

		// PLOT: the plot the fact NAMES, and no other. A plot-scope deposit is authored only by a PLOT-RESIDENT
		// source, whose output is its own tile's -- an effect on a NEIGHBOURING tile is the deliveryguy's and is
		// authored on that tile's improvement ([DEC-deliveryguy]). So a plot-scope entry with no named plot has
		// no target by construction and nothing is dropped by declining.
		if (pPlot != NULL)
		{
			CvCascadeEvalCtx evalCtx;
			InfoValuation::fillEvalCtxAtPlot(*pPlot, evalCtx);
			for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
			{
				const CvModEntry* pEntry = entries[iEntry];
				int iChannel = -1;
				bool bPercentSide = false;
				int64_t iValue = 0;
				if (pEntry == NULL || !MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_PLOT,
					evalCtx, pPlot, false, iChannel, bPercentSide, iValue))
				{
					continue;
				}
				// the ORIGIN RULE: plot is yield-only, so a plot-scope percent has no side to land on
				if (!bPercentSide)
				{
					foldPlotSegment(*pPlot, ePlotSegment, iChannel, iValue);
					// THE BOOK, on the plot plane -- the same record mc_applyCityDeposits keeps, and it has to be
					// kept HERE or the book is not a faithful account of the slot. mc_bookGatedPlot moves only the
					// DIFFERENCE between what is booked and what the gate now owes, so an entry plane A applied
					// while leaving the book at zero would be re-applied in full by the next crossing (a double) and
					// never withdrawn when its gate later turns off (a miss). Booking every conditioned entry the
					// moment it is applied is what makes that difference exact ([DEC-maintained-sum]).
					if (pEntry->enabled != NULL || pEntry->disabled != NULL)
					{
						const CvCascadePackage<CvPlot>::BookedDeposit kPrev =
							pPlot->getCascadePackage().bookedDeposit(pEntry);
						pPlot->getCascadePackage().setBookedDeposit(pEntry, iChannel, false,
							(iMultiplicity > 0) ? (kPrev.iValue + iValue) : 0);
					}
					// ⛔ THE PLOT PLANE'S ONLY ATTRIBUTION, and its absence is what made a whole class of question
					// unanswerable rather than merely unlogged: no served surface carries a plot package, so a
					// plot-scope deposit left no trace anywhere. "Does a resource fold its yield when its fact
					// fires?" is the shape of it -- an EVENT-DRIVEN fold that cannot be observed is one nobody can
					// tell apart from a fold that never happens ([DEC-obs-scale]).
					CascadeChannelRegistry::reportDepositApply(
						(pSourceInfo != NULL) ? pSourceInfo->getType() : "?", iChannel, CASC_SCOPE_PLOT,
						false, iValue, (int)pPlot->getOwner(), -1, szOnFact);
				}
			}
			pPlot->getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
		}

		// CITY: the city the fact names, else every city of the owner -- an above-city deposit rolls DOWN, and
		// the `cities` plural fan resolves PER CITY so each one's own `per` scalers and conditions apply.
		// ⚑ THE `plots` FANS ARE BANKED WHILE THE SAVE IS STREAMING and drained at GAME_LOAD_FINISHED (see
		// PlotsFanFact): during the read the cities a fan needs may not exist yet, and applying to an absent
		// owner loses the deposit with nothing raised.
		const bool bBankPlotsFan = spineGameLoadInProgress();
		if (bBankPlotsFan && (pCity != NULL || pPlayer != NULL))
		{
			PlayerTypes eOwner = (pPlayer != NULL) ? pPlayer->getID()
				: ((pCity != NULL) ? pCity->getOwner() : NO_PLAYER);
			if (eOwner != NO_PLAYER)
			{
				PlotsFanFact kFact;
				kFact.pSource = pSourceInfo;
				kFact.iMultiplicity = iMultiplicity;
				kFact.eOwner = eOwner;
				kFact.iCityId = (pCity != NULL) ? pCity->getID() : -1;
				s_bankedPlotsFans.push_back(kFact);
			}
		}

		if (pCity != NULL)
		{
			mc_applyCityDeposits(entries, iMultiplicity, iSourceIndex, *pCity, pSourceInfo->getType(), szOnFact,
				mc_buildingIdOf(pSourceInfo), eOrigin);
			// a CITY-scope `plots` deposit reaches THIS city's worked plots and no other city's
			std::vector<const CvModEntry*> cityPlotEntries;
			if (!bBankPlotsFan && mc_selectPlotsTargetEntries(entries, CASC_SCOPE_CITY, cityPlotEntries))
			{
				g_iFanResolved = 0;
				const int iPlots = mc_applyPlotsTargetDeposits(cityPlotEntries, iMultiplicity, iSourceIndex, *pCity);
				CascadeChannelRegistry::reportPlotsFan(pSourceInfo->getType(), CASC_SCOPE_CITY,
					(int)cityPlotEntries.size(), 1, iPlots, g_iFanResolved, iMultiplicity);
			}
		}
		else if (pPlayer != NULL)
		{
			for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities();
				cityIterator != pPlayer->endCities(); ++cityIterator)
			{
				if (*cityIterator != NULL)
				{
					mc_applyCityDeposits(entries, iMultiplicity, iSourceIndex, **cityIterator, pSourceInfo->getType(), szOnFact,
						mc_buildingIdOf(pSourceInfo), eOrigin);
				}
			}
		}

		// An EMPIRE-scope `plots` deposit (the Colossus's water commerce, the Seafaring traits' water food) is
		// the one that genuinely reaches every city's plots -- so it fans off the OWNER regardless of which
		// object the fact named, and never rides the city branch above.
		const CvPlayer* pPlotsOwner = (pPlayer != NULL) ? pPlayer
			: ((pCity != NULL && pCity->getOwner() != NO_PLAYER) ? &GET_PLAYER(pCity->getOwner()) : NULL);
		std::vector<const CvModEntry*> empirePlotEntries;
		// selected ONCE, before any city is walked -- a source with no empire-scope `plots` deposit costs nothing
		if (!bBankPlotsFan && pPlotsOwner != NULL
			&& mc_selectPlotsTargetEntries(entries, CASC_SCOPE_EMPIRE, empirePlotEntries))
		{
			int iCities = 0;
			int iPlots = 0;
			g_iFanResolved = 0;
			for (CvPlayer::city_iterator cityIterator = pPlotsOwner->beginCities();
				cityIterator != pPlotsOwner->endCities(); ++cityIterator)
			{
				if (*cityIterator != NULL)
				{
					++iCities;
					iPlots += mc_applyPlotsTargetDeposits(empirePlotEntries, iMultiplicity, iSourceIndex, **cityIterator);
				}
			}
			CascadeChannelRegistry::reportPlotsFan(pSourceInfo->getType(), CASC_SCOPE_EMPIRE,
				(int)empirePlotEntries.size(), iCities, iPlots, g_iFanResolved, iMultiplicity);
		}

		// EMPIRE and TEAM: both read the empire ctx (team is the TECH BRIDGE and holds no context of its own --
		// every team fact is asked of the player, [DEC-scope-contexts]).
		if (pPlayer != NULL)
		{
			CvCascadeEvalCtx evalCtx;
			pPlayer->getEmpireContext().fillEvalCtx(evalCtx);
			for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
			{
				const CvModEntry* pEntry = entries[iEntry];
				int iChannel = -1;
				bool bPercentSide = false;
				int64_t iValue = 0;
				if (pEntry != NULL && MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_EMPIRE,
					evalCtx, NULL, false, iChannel, bPercentSide, iValue))
				{
					if (bPercentSide)
					{
						pPlayer->getCascadePackage().applyPercent(iChannel, (int)iValue);
					}
					else
					{
						pPlayer->getCascadePackage().applyFlat(iChannel, iValue);
					}
					// THE BOOK, on the empire plane -- for the same reason as the city and plot planes above. It is
					// what the ERA route reads: an era atom is a THRESHOLD, not a presence crossing, so its deposits
					// cannot be applied by a ±1 on a pinned verdict and are re-booked against the new era instead.
					if (pEntry->enabled != NULL || pEntry->disabled != NULL)
					{
						const CvCascadePackage<CvPlayer>::BookedDeposit kPrev =
							pPlayer->getCascadePackage().bookedDeposit(pEntry);
						pPlayer->getCascadePackage().setBookedDeposit(pEntry, iChannel, bPercentSide,
							(iMultiplicity > 0) ? (kPrev.iValue + iValue) : 0);
					}
					mc_noteChannelApplied(iChannel, (int)pPlayer->getID());
					CascadeChannelRegistry::reportDepositApply(pSourceInfo->getType(), iChannel, CASC_SCOPE_EMPIRE,
						bPercentSide, iValue, (int)pPlayer->getID(), -1, szOnFact);
				}
				iChannel = -1;
				bPercentSide = false;
				iValue = 0;
				if (pEntry != NULL && MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_TEAM,
					evalCtx, NULL, false, iChannel, bPercentSide, iValue))
				{
					const CvTeam& team = GET_TEAM(pPlayer->getTeam());
					if (bPercentSide)
					{
						team.getCascadePackage().applyPercent(iChannel, (int)iValue);
					}
					else
					{
						team.getCascadePackage().applyFlat(iChannel, iValue);
					}
					CascadeChannelRegistry::reportDepositApply(pSourceInfo->getType(), iChannel, CASC_SCOPE_TEAM,
						bPercentSide, iValue, (int)pPlayer->getID(), -1, szOnFact);
				}
			}
			pPlayer->getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
			GET_TEAM(pPlayer->getTeam()).getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
		}
	}

	// ⚖ THE SECOND LEG OF THE EMPIRE→CITIES FAN -- what a city folds when it STARTS EXISTING.
	//
	// ⛔ THE FAN ALONE CANNOT BE THE WHOLE MECHANISM, and the reason is an ordering fact rather than a judgement
	// ([contexts.md] § THE FOLD HAS TWO LEGS -- the amenity fold hit exactly this and answered it exactly this
	// way). An empire-level grantor fans its CITY-scope deposits over the cities that stand AT THAT MOMENT:
	//   - at LOAD the order is not uniform. Measured in CvPlayer::read: techs (l.191), projects (l.201) and civics
	//     (l.445) emit BEFORE the cities stream (l.524), so their fan iterates an EMPTY list and every city-scope
	//     deposit they carry is applied to nothing; traits (l.1022) and heritages (l.1349) emit AFTER, so theirs
	//     land. One mechanism, two outcomes, decided by where a member sits in a read.
	//   - at PLAY a city FOUNDED later never receives anything from what its owner already holds, permanently.
	//
	// ⚑ IT IS IDEMPOTENT BY CONSTRUCTION, WHICH IS WHY IT NEEDS NO LOAD GUARD AND NO BANK. The package already
	// records which sources have deposited into it (noteSourceApplied -- plane B and C's own liveness key), so a
	// source the fan already delivered is SKIPPED here by the same test those planes use. The alternative shape --
	// suppressing the fan during load and folding everything at the end -- would work too, but it makes
	// correctness depend on a guard staying in step with an emit order nobody controls; this depends on the
	// package's own record of what it holds, which cannot disagree with what was applied.
	// ⛔ Not a recompute and not a sweep: the worklist is the owner's HELD sources, and each one goes through the
	// ONE per-entry resolve ([DEC-single-implementation]). A source nobody holds is never reached.
	// The owner's HELD empire-level sources (team techs + projects, adopted civics, active traits, heritages) --
	// the ONE enumeration, shared by the city fold below and the plots-fan membership fold
	// ([DEC-single-implementation]: a second copy of this walk would drift the moment a source kind is added).
	void mc_collectOwnerHeldSources(const CvPlayer& kOwner, std::vector<const CvInfo*>& heldSources)
	{
		const CvTeam& kTeam = GET_TEAM(kOwner.getTeam());
		for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
		{
			if (kTeam.isHasTech((TechTypes)iTech))
			{
				heldSources.push_back(&GC.getTechInfo((TechTypes)iTech));
			}
		}
		for (int iProject = 0; iProject < GC.getNumProjectInfos(); ++iProject)
		{
			if (kTeam.getProjectCount((ProjectTypes)iProject) > 0)
			{
				heldSources.push_back(&GC.getProjectInfo((ProjectTypes)iProject));
			}
		}
		for (int iCivicOption = 0; iCivicOption < GC.getNumCivicOptionInfos(); ++iCivicOption)
		{
			const CivicTypes eCivic = kOwner.getCivics((CivicOptionTypes)iCivicOption);
			if (eCivic != NO_CIVIC)
			{
				heldSources.push_back(&GC.getCivicInfo(eCivic));
			}
		}
		for (int iTrait = 0; iTrait < GC.getNumTraitInfos(); ++iTrait)
		{
			// the ACTIVE set's record -- a complex trait carries different values from its simple twin
			// ([modifier.md] §4), so the fold must read the same record the fan did.
			if (kOwner.hasTrait((TraitTypes)iTrait))
			{
				const CvTraitInfo* pTrait = MMKernel::traitData(iTrait);
				if (pTrait != NULL)
				{
					heldSources.push_back(pTrait);
				}
			}
		}
		const std::vector<HeritageTypes> heritages = kOwner.getHeritage();
		for (size_t iHeritage = 0; iHeritage < heritages.size(); ++iHeritage)
		{
			if (heritages[iHeritage] >= 0 && heritages[iHeritage] < GC.getNumHeritageInfos())
			{
				heldSources.push_back(&GC.getHeritageInfo(heritages[iHeritage]));
			}
		}
	}

	void mc_foldOwnerSourcesIntoCity(const CvPlayer& kOwner, const CvCity& kCity, const char* szOnFact)
	{
		std::vector<const CvInfo*> heldSources;
		mc_collectOwnerHeldSources(kOwner, heldSources);

		for (size_t iSource = 0; iSource < heldSources.size(); ++iSource)
		{
			const CvInfo* pSourceInfo = heldSources[iSource];
			const CvModifiers* pModifiers = (pSourceInfo != NULL) ? pSourceInfo->getModifiers() : NULL;
			if (pModifiers == NULL || pModifiers->empty())
			{
				continue;
			}
			const int iSourceIndex = DepositIndex::sourceIndexOf(pSourceInfo);
			if (kCity.getBuildingYields().hasAppliedSource(iSourceIndex))
			{
				continue;   // the fan already delivered this one -- folding it again is the double
			}
			mc_applyCityDeposits(pModifiers->entries(), 1, iSourceIndex, kCity,
				pSourceInfo->getType(), szOnFact, mc_buildingIdOf(pSourceInfo), CASC_ORIGIN_BUILDING);
		}
	}

	// One source's half of the plots-fan MEMBERSHIP fold below: select its `plots` entries at the scope its
	// kind authors, test the plot package's OWN applied-source record for the direction, and move the one plot.
	// The record -- not the HAVE axis -- is the liveness key, exactly as it is for planes B and C: a source
	// whose fan never resolved here (a dormant building, a land tile against an IS_WATER filter) is absent from
	// it, so an ADDED retry costs a resolve and a REMOVED withdraws nothing it never held.
	void mc_foldPlotsFanSource(const CvInfo* pSource, CvCascScope eEntryScope, const CvPlot& kPlot,
		const CvCity& kCity, int iDirection)
	{
		const CvModifiers* pModifiers = (pSource != NULL) ? pSource->getModifiers() : NULL;
		if (pModifiers == NULL || pModifiers->empty())
		{
			return;
		}
		std::vector<const CvModEntry*> selected;
		if (!mc_selectPlotsTargetEntries(pModifiers->entries(), eEntryScope, selected))
		{
			return;
		}
		const int iSourceIndex = DepositIndex::sourceIndexOf(pSource);
		const bool bAlreadyApplied = kPlot.getCascadePackage().hasAppliedSource(iSourceIndex);
		if ((iDirection > 0) ? bAlreadyApplied : !bAlreadyApplied)
		{
			return;
		}
		mc_applyPlotsEntriesToPlot(selected, iDirection, iSourceIndex, kPlot, kCity.getOwner(), kCity.getID());
	}

	// ⚖ THE MEMBERSHIP LEG OF THE `plots` FAN -- what ONE plot folds when it ENTERS or LEAVES a city's working
	// set (SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED). The source-move fan above delivers a `plots` deposit to the
	// plots standing in the working set AT THAT MOMENT; without this leg a plot that changes hands afterwards
	// keeps the departed owner's deposits in PLOTSEG_REST and never receives the new owner's -- the two legs
	// together are what keep the segment total, and dropping either leaves it permanently wrong with nothing to
	// re-derive it ([DEC-no-self-heal]).
	// ⚑ This is also where the DEPARTED owner's obligation on an ownership flip lives: CvPlot::setOwner calls
	// updateWorkingCity ([contexts.md]: a city cannot work a plot it does not own), so the REMOVED crossing names
	// the departing city and owner while the ADDED names the arriving ones -- one route, both ends, and the
	// owner facts themselves carry no modifier work at all.
	void mc_foldPlotsFanMembership(const CvPlot& kPlot, const CvCity& kCity, const CvPlayer& kOwner, int iDirection)
	{
		// CITY-scope authors: the city's ACTIVE buildings -- a dormant or obsolete building's fan never ran, and
		// the per-source record above is what filters an active one whose entries never resolved here.
		const OperatingBuildings& kOperating = EnablerKernel::operatingBuildings(&kCity);
		for (std::set<int>::const_iterator itBuilding = kOperating.active.begin();
			itBuilding != kOperating.active.end(); ++itBuilding)
		{
			if (*itBuilding >= 0 && *itBuilding < GC.getNumBuildingInfos())
			{
				mc_foldPlotsFanSource(&GC.getBuildingInfo((BuildingTypes)*itBuilding), CASC_SCOPE_CITY,
					kPlot, kCity, iDirection);
			}
		}
		// EMPIRE-scope authors: the owner's held sources -- the same enumeration the city fold walks.
		std::vector<const CvInfo*> heldSources;
		mc_collectOwnerHeldSources(kOwner, heldSources);
		for (size_t iSource = 0; iSource < heldSources.size(); ++iSource)
		{
			mc_foldPlotsFanSource(heldSources[iSource], CASC_SCOPE_EMPIRE, kPlot, kCity, iDirection);
		}
	}

	// ---- PLANES B and C: the COUNT route and the ATOM route ----
	//
	// Both move deposits ALREADY IN a slot, which is what makes them one function: plane B scales a deposit by
	// the count's own delta, plane C withdraws or re-deposits one whose gate crossed. They differ only in what
	// the caller passes -- the delta, and whether the `per` scaler applies (MMKernel::PerScaling).
	//
	// ⛔ THE LIVENESS TEST IS THE PACKAGE'S OWN RECORD, NOT THE HAVE AXIS. Applying for a source that never
	// deposited here would invent a contribution from nothing, and asking whether the OWNER HOLDS the source is
	// a different question: a present-but-DORMANT building deposits nothing, so the HAVE axis answers yes where
	// the truth is no. hasAppliedSource cannot disagree with what was applied, because it IS what was applied.
	//
	// ⚠ Plane C's SIGN is the crossing direction the fact carries, and its withdrawal is exact only if that fact
	// is emitted while the old state still holds (§ THE INVARIANT) -- the same emit-ordering contract plane A
	// answers to, and the one thing this function cannot enforce for itself.
	// pHypothetical PINS an atom's verdict for this pass (CvConditionEval.h's AS-IF-HELD overlay), and it is what
	// makes an atom crossing EXACT rather than approximate. A DOMAIN fact is PAST TENSE -- it announces the
	// crossing once the state has already moved -- so a withdrawal evaluated against the LIVE ctx asks a gate that
	// no longer holds, resolves false, and withdraws nothing ([state-repositories.md] § THE INVARIANT: a
	// withdrawal is exact only if it resolves against the state the deposit was booked under). Pinning the atom
	// back to its OLD verdict restores exactly that state for the length of one pass, without writing a context
	// (the overlay is read-only by construction -- a hypothetical that mutated a store would leave every other
	// reader evaluating a game that never happened, with no self-heal to put it back).
	// ⛔ THE LOAD-BRACKET BANK FOR ATOM-ROUTED DEPOSITS -- an ORDERING fact, never a staleness mechanism.
	// MEASURED: 749,264 of the bonus route's deposits are dropped during the load because the gated deposit's
	// SOURCE is not in that city's package yet -- the atom crossing arrives while the buildings it would modify
	// are still activating. The drop is silent and permanent ([DEC-no-self-heal]).
	// ⚖ WHAT IS BANKED IS THE **SKIPPED DEPOSIT**, NOT THE CROSSING, and that is what makes the drain exact.
	// A replayed CROSSING would have to withdraw before it applies, and the withdrawal can only be exact if it
	// resolves against the verdict the deposit was BOOKED under -- which at load is whatever held when its
	// source activated, and is not knowable afterwards. Pinning the wrong one either over-withdraws or
	// double-applies. A skipped deposit has no such ambiguity: it was never booked, so the drain owes it exactly
	// one arrival and nothing else ([state-repositories.md] § THE INVARIANT).
	// ⚑ WHAT IS BANKED IS THE CROSSING, NEVER ITS MISSES -- and neither thing a per-miss entry held needs holding.
	// The deposit list comes back from the reverse index at the drain (`DepositIndex::gatedBy*`, which is what the
	// index is FOR) and the cities are enumerable from the owner; only that the crossing HAPPENED inside the
	// bracket cannot be recovered afterwards. So the key is (atom's compiled list, owner) -- the shape
	// s_bankedAtomFans beside it already uses -- and the size stops tracking (deposits x cities that skipped them).
	// ⛔ Re-deriving at the drain CANNOT double-apply, which is what lets the misses go unrecorded: the
	// GATED-DEPOSIT BOOK (CvCascadePackage::bookedGated) holds what each package already carries per conditioned
	// deposit, so a deposit plane A booked as its source arrived is found booked here and moves nothing.
	struct BankedAtomCrossing
	{
		const std::vector<DepositIndex::GatedDeposit>* pList;
		int iOwner;
		bool operator<(const BankedAtomCrossing& kOther) const
		{
			if (pList != kOther.pList) return pList < kOther.pList;
			return iOwner < kOther.iOwner;
		}
	};
	std::set<BankedAtomCrossing> s_bankedAtomCrossings;

	// ⛔ THE FAN'S OWN BANK, and what it is FOR is the PLOT plane. An EMPIRE-level crossing (a tech, a civic) is
	// announced from CvPlayer::read, which streams before the map's plot packages can take it, and no per-deposit
	// skip is recorded because mc_applyTypeAtom's empire branch returns at the load guard above its plot call --
	// so without this bank the crossing reaches the plot plane through nothing at all.
	// ⛔ It does NOT owe the CITY plane anything: a city that starts existing folds the city-scope deposits of
	// every source its owner already holds ([modifier.md] §5, the two-leg fold), so the city half is served
	// before this drain runs. Do not re-add a city replay here -- it applied NOTHING while it existed, against
	// 420,990 considerations, because plane A and the fold had already booked every one of them.
	// ⚑ Only the HELD side is worth banking: a crossing that ends the load un-held owes no withdrawal, so the
	// drain replays an ARRIVAL and never a swap ([state-repositories.md] § THE INVARIANT).
	// ⚑ Keyed on (atom, owner) so a civic swapped several times during one load drains once, at its FINAL verdict.
	std::map<std::pair<std::string, int>, bool> s_bankedAtomFans;

	// The per-pass OUTCOME tally. ⛔ It exists because "the route fired" and "the route MOVED something" are
	// different claims, and only the second one matters: a pass that finds 389 deposits and applies none looks
	// exactly like a pass that finds none, from the outside. Counting the three ways a deposit is dropped is what
	// separates an empty index from an unapplied source from a refused condition ([DEC-no-guessing]).
	struct McGatedTally
	{
		int iFound;
		int iNoSource;    // the source is not in this package -- it never applied here, so there is nothing to move
		int iRefused;     // resolveEntry declined: the condition, the scope or the audience said no
		int iApplied;
		McGatedTally() : iFound(0), iNoSource(0), iRefused(0), iApplied(0) {}
	};

	void mc_applyGated(const std::vector<DepositIndex::GatedDeposit>* pGated, int iDelta,
		MMKernel::PerScaling ePerScaling, const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot,
		const CvCascadeHypothetical* pHypothetical = NULL, McGatedTally* pTally = NULL)
	{
		if (pGated == NULL || iDelta == 0)
		{
			return;
		}
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			const DepositIndex::GatedDeposit& kGated = (*pGated)[iGated];
			if (kGated.deposit == NULL || kGated.deposit->entry == NULL)
			{
				continue;
			}
			if (pTally != NULL) { pTally->iFound++; }
			const CvCascScope eScope = (CvCascScope)kGated.deposit->scopeIdx;
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (eScope == CASC_SCOPE_PLOT)
			{
				if (pPlot == NULL || !pPlot->getCascadePackage().hasAppliedSource(kGated.sourceIndex))
				{
					continue;
				}
				CvCascadeEvalCtx evalCtx;
				InfoValuation::fillEvalCtxAtPlot(*pPlot, evalCtx);
				evalCtx.hypothetical = pHypothetical;
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, pPlot, false,
					iChannel, bPercentSide, iValue, ePerScaling) && !bPercentSide)
				{
					foldPlotSegment(*pPlot, 
						CvCascadePackage<CvPlot>::PLOTSEG_REST, iChannel, iValue);
				}
				continue;
			}
			if (eScope == CASC_SCOPE_CITY)
			{
				if (pCity == NULL || !pCity->getBuildingYields().hasAppliedSource(kGated.sourceIndex))
				{
					if (pCity != NULL)
					{
						if (pTally != NULL) { pTally->iNoSource++; }
						// Inside the load bracket the source has simply not streamed yet, so this is an ORDERING
						// miss and the deposit is owed an application once everything stands. Outside it, the
						// source genuinely is not here and there is nothing to move.
						if (iDelta > 0 && spineGameLoadInProgress())
						{
							BankedAtomCrossing kBanked;
							kBanked.pList = pGated;
							kBanked.iOwner = (int)pCity->getOwner();
							s_bankedAtomCrossings.insert(kBanked);
						}
					}
					continue;
				}
				CvCascadeEvalCtx evalCtx;
				// Same full fill as the source apply above — the plane-B/C routes evaluate the SAME conditions,
				// so an under-filled ctx here would withdraw against a verdict the apply never used.
				InfoValuation::fillEvalCtx(pCity->getCityContext(),
					GET_PLAYER(pCity->getOwner()).getEmpireContext(), pCity->plotGroup(pCity->getOwner()), evalCtx);
				EnablerKernel::wireOperatingBuildings(pCity, evalCtx);
				evalCtx.hypothetical = pHypothetical;
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, false,
					iChannel, bPercentSide, iValue, ePerScaling))
				{
					if (bPercentSide) pCity->getCityPercents().applyPercent(iChannel, (int)iValue);
					else              pCity->getBuildingYields().applyFlat(iChannel, iValue);
					// Planes B and C attribute too -- a count or an atom crossing moving a deposit is exactly the
					// class a total can never explain, so leaving it out would make the decomposition lie by omission.
					CascadeChannelRegistry::reportDepositApply(
						(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_CITY,
						bPercentSide, iValue, (int)pCity->getOwner(), pCity->getID(), "gated");
					if (pTally != NULL) { pTally->iApplied++; }
				}
				else if (pTally != NULL) { pTally->iRefused++; }
				continue;
			}
			if (pPlayer == NULL)
			{
				continue;
			}
			CvCascadeEvalCtx evalCtx;
			pPlayer->getEmpireContext().fillEvalCtx(evalCtx);
			evalCtx.hypothetical = pHypothetical;
			if (eScope == CASC_SCOPE_TEAM)
			{
				const CvTeam& team = GET_TEAM(pPlayer->getTeam());
				if (!team.getCascadePackage().hasAppliedSource(kGated.sourceIndex))
				{
					continue;
				}
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, false,
					iChannel, bPercentSide, iValue, ePerScaling))
				{
					if (bPercentSide) team.getCascadePackage().applyPercent(iChannel, (int)iValue);
					else              team.getCascadePackage().applyFlat(iChannel, iValue);
					// ⛔ The census covers EVERY plane, and the two that did not report are exactly the two that
					// hid a defect: an empire slot can be wrong by orders of magnitude with the whole deposit
					// census reading clean, because nothing on it ever named this write ([DEC-obs-scale] -- an
					// apply nobody can see is one nobody can attribute).
					CascadeChannelRegistry::reportDepositApply(
						(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_TEAM,
						bPercentSide, iValue, (int)pPlayer->getID(), -1, "gated");
					if (pTally != NULL) { pTally->iApplied++; }
				}
				else if (pTally != NULL) { pTally->iRefused++; }
				continue;
			}
			if (!pPlayer->getCascadePackage().hasAppliedSource(kGated.sourceIndex))
			{
				if (pTally != NULL) { pTally->iNoSource++; }
				continue;
			}
			if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, false,
				iChannel, bPercentSide, iValue, ePerScaling))
			{
				if (bPercentSide) pPlayer->getCascadePackage().applyPercent(iChannel, (int)iValue);
				else              pPlayer->getCascadePackage().applyFlat(iChannel, iValue);
				CascadeChannelRegistry::reportDepositApply(
					(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_EMPIRE,
					bPercentSide, iValue, (int)pPlayer->getID(), -1, "gated");
				if (pTally != NULL) { pTally->iApplied++; }
			}
			else if (pTally != NULL) { pTally->iRefused++; }
		}
	}

	// The source-carrying application: the source's own deposits (PLANE A, applied here) plus everything
	// conditioned ON the source -- a deposit gated on this entity's presence, which is the ATOM route (plane C)
	// and lives in mc_applyBonusAtom for the BONUS axis.
	// ⛔ WHAT MADE THAT ROUTE HARD WAS THE WITHDRAWAL, NEVER THE INDEX: DepositIndex::gatedByType has always
	// answered with the deposit list an atom gates (di_scanConditionTree interns every CASC_COND_PRESENCE type),
	// so the ARRIVAL half was only ever one mc_applyGated call away. The block was that a DOMAIN fact is PAST
	// TENSE -- CvCity::processBonus announces the crossing once the count has already moved -- so a withdrawal
	// resolved against the LIVE ctx asks a gate that no longer holds and withdraws nothing, and an arrival wired
	// ALONE would compound on every re-acquisition instead of merely being absent.
	// ⚑ The AS-IF-HELD hypothetical is what dissolves it: pinning the atom back to its OLD verdict restores the
	// state the deposit was booked under for exactly one pass, so withdraw-then-apply is exact
	// ([state-repositories.md] § THE INVARIANT). ⛔ The OTHER axes are still unrouted -- plane B (the COUNT route)
	// and plane C for the non-bonus atoms -- and remain a HOLE rather than a decision.
	void mc_applySource(const CvInfo* pSourceInfo, int iMultiplicity, int iEventId,
		const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot, CvCascOrigin eOrigin)
	{
		mc_applySourceDeposits(pSourceInfo, iMultiplicity, pPlayer, pCity, pPlot, mc_plotSegmentFor(iEventId),
			spineEventName(iEventId), eOrigin);
	}


	// A PLOT PREDICATE's verdict moved, so the deposits it gates are re-applied on the plot and the city SITTING
	// on it (the only two folds that bind a plot; a one-tile fact never reaches the player, which is what keeps
	// "emit liberally, apply precisely" honest).
	//
	// The crossing is +1 the verdict became true, -1 it became false, and it comes from the FACT'S IDENTITY --
	// SEVT_PLOT_PREDICATE_ADDED or _REMOVED, one per bit that moved. A substrate swap that moves several verdicts
	// in opposite directions emits several facts, so there is never a sign to derive here.
	// ⚠ The withdrawal is exact only if the fact reaches here while the OLD state still holds
	// (state-repositories.md § THE INVARIANT): resolveEntry evaluates the deposit's whole gate against the ctx
	// as it stands, so a -1 announced after the substrate already moved resolves a different number than it
	// deposited. That is an emit-ordering contract, not something this function can enforce.
	void mc_applyPlotPredicate(CvCascPredKind ePredicate, int iCrossing, const CvPlot* pPlot)
	{
		if (pPlot == NULL || iCrossing == 0)
		{
			return;
		}
		const CvCity* pPlotCity = pPlot->getPlotCity();
		const CvPlayer* pPlayer = (pPlotCity != NULL) ? &GET_PLAYER(pPlotCity->getOwner()) : NULL;
		mc_applyGated(DepositIndex::gatedByPredicate(ePredicate), iCrossing,
			MMKernel::PER_SCALE_APPLIED, pPlayer, pPlotCity, pPlot);
	}

	// ⚖ THE RE-BOOK -- one operation, and it is what makes planes B and C the SAME mechanism.
	// `owed` is what the deposit resolves to against live state; `booked` is what this package already holds for
	// it. Only the DIFFERENCE moves. A gate turning off resolves to 0 and is withdrawn; a COUNT moving resolves
	// to a new magnitude with the gate unchanged and the difference is applied -- which is precisely plane B, and
	// it needed no separate route, only a book that remembers an amount instead of a yes/no.
	// ⛔ A channel or side change is handled as a full withdraw-then-apply rather than a signed delta: the two
	// amounts live in different slots, so differencing them would move neither correctly.
	void mc_rebookCity(const CvCity& kCity, const DepositIndex::GatedDeposit& kGated,
		int iOwedChannel, bool bOwedPercent, int64_t iOwedValue, McGatedTally* pTally)
	{
		const CvCascadePackage<CvCity, CASC_ORIGIN_BUILDING>::BookedDeposit kBooked =
			kCity.getBuildingYields().bookedDeposit(kGated.deposit->entry);
		if (kBooked.iChannel == iOwedChannel && kBooked.bPercent == bOwedPercent && kBooked.iValue == iOwedValue)
		{
			if (pTally != NULL && iOwedValue == 0) { pTally->iRefused++; }
			return;   // already exactly what the data calls for
		}
		if (kBooked.iValue != 0 && (kBooked.iChannel != iOwedChannel || kBooked.bPercent != bOwedPercent))
		{
			if (kBooked.bPercent) kCity.getCityPercents().applyPercent(kBooked.iChannel, (int)-kBooked.iValue);
			else                  kCity.getBuildingYields().applyFlat(kBooked.iChannel, -kBooked.iValue);
			if (iOwedValue != 0)
			{
				if (bOwedPercent) kCity.getCityPercents().applyPercent(iOwedChannel, (int)iOwedValue);
				else              kCity.getBuildingYields().applyFlat(iOwedChannel, iOwedValue);
			}
		}
		else
		{
			const int64_t iDelta = iOwedValue - kBooked.iValue;
			if (iDelta != 0)
			{
				const int iSlot = (iOwedChannel >= 0) ? iOwedChannel : kBooked.iChannel;
				if (iSlot >= 0)
				{
					if (bOwedPercent || kBooked.bPercent) kCity.getCityPercents().applyPercent(iSlot, (int)iDelta);
					else                                  kCity.getBuildingYields().applyFlat(iSlot, iDelta);
				}
			}
		}
		kCity.getBuildingYields().setBookedDeposit(kGated.deposit->entry, iOwedChannel, bOwedPercent, iOwedValue);
		mc_noteChannelApplied(iOwedChannel, (int)kCity.getOwner());
		if (pTally != NULL) { pTally->iApplied++; }
		if (iOwedValue != 0)
		{
			CascadeChannelRegistry::reportDepositApply(
				(kGated.source != NULL) ? kGated.source->getType() : "?", iOwedChannel, CASC_SCOPE_CITY,
				bOwedPercent, iOwedValue, (int)kCity.getOwner(), kCity.getID(), "gated");
		}
	}

	// ⚖ THE IDEMPOTENT BOOKING -- plane C's whole apply, and it is a DIFF rather than a pair of additions.
	// For each deposit an atom gates: what SHOULD be booked is what the gate says against LIVE state; what IS
	// booked is the package's own record. Only the difference moves.
	//   want && !booked  -> apply  (+), book it
	//   !want && booked  -> apply  (-), unbook it
	//   otherwise        -> nothing
	// ⛔ THIS IS WHY IT CANNOT DOUBLE-APPLY, and the additive form did. Plane A books a deposit when its SOURCE
	// arrives, evaluating the same gate against the same live state; a crossing arriving afterwards found the
	// gate true and simply applied it AGAIN, because nothing recorded that it was already in the slot. Measured
	// before this: London's food percent held 155 against 82 authorized, production 841 against 576, while
	// commerce -- barely atom-gated, so this route never touched it -- reconciled exactly.
	// ⚑ It also needs NO as-if-held pin. The pin existed to reconstruct the verdict a deposit was booked under,
	// because a PAST-TENSE fact has already moved the state; the BOOK remembers it directly, which is both exact
	// and cheaper ([state-repositories.md] § THE INVARIANT).
	// ⛔ Still not a recompute: the worklist is exactly the deposits the arriving FACT names, never a sweep of the
	// package or a re-derivation of state ([DEC-no-self-heal]).
	void mc_bookGated(const std::vector<DepositIndex::GatedDeposit>* pGated, const CvCity& kCity,
		McGatedTally* pTally)
	{
		if (pGated == NULL)
		{
			return;
		}
		// ⚑ The ctx is filled LAZILY, on the first entry that actually has a source in this package. Filling it is
		// the expensive part (the whole eval fill plus the enabler's operating set), and the turn-cadence route
		// asks this of EVERY city of EVERY player -- where the overwhelming majority hold none of the atom's
		// sources and bail on an O(1) dictionary test ([DEC-turn-time-is-king]).
		CvCascadeEvalCtx evalCtx;
		bool bCtxFilled = false;
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			const DepositIndex::GatedDeposit& kGated = (*pGated)[iGated];
			if (kGated.deposit == NULL || kGated.deposit->entry == NULL)
			{
				continue;
			}
			if (pTally != NULL) { pTally->iFound++; }
			if (!kCity.getBuildingYields().hasAppliedSource(kGated.sourceIndex))
			{
				if (pTally != NULL) { pTally->iNoSource++; }
				if (spineGameLoadInProgress())
				{
					BankedAtomCrossing kBanked;
					kBanked.pList = pGated;
					kBanked.iOwner = (int)kCity.getOwner();
					s_bankedAtomCrossings.insert(kBanked);
				}
				continue;
			}
			if (!bCtxFilled)
			{
				InfoValuation::fillEvalCtx(kCity.getCityContext(), GET_PLAYER(kCity.getOwner()).getEmpireContext(),
					kCity.plotGroup(kCity.getOwner()), evalCtx);
				EnablerKernel::wireOperatingBuildings(&kCity, evalCtx);
				bCtxFilled = true;
			}
			// THE CARRIER, per entry: a source-relative predicate resolves against the entity that DEPOSITED, so
			// the walk that knows the id sets it here and it is -1 for every non-building source.
			evalCtx.sourceBuilding = mc_buildingIdOf(kGated.source);
			// WHAT THE DATA SAYS NOW. A decline resolves to nothing owed -- which is the same state as never
			// having been booked, so the difference below withdraws it without a second code path.
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (!MMKernel::resolveEntry(*kGated.deposit->entry, 1, CASC_SCOPE_CITY, evalCtx, NULL,
				false, iChannel, bPercentSide, iValue, MMKernel::PER_SCALE_APPLIED))
			{
				iChannel = -1;
				iValue = 0;
			}
			mc_rebookCity(kCity, kGated, iChannel, bPercentSide, iValue, pTally);
		}
	}

	// Which plot SEGMENT a source's plot-scope output belongs to. The segment is a property of the SOURCE (the
	// same rule mc_plotSegmentFor encodes for facts), so it is resolved ONCE per gated deposit rather than per
	// plot -- an atom crossing can touch every worked tile of every city.
	CvCascadePackage<CvPlot>::PlotSegment mc_segmentForSource(const CvInfo* pSource)
	{
		const char* szType = (pSource != NULL) ? pSource->getType() : NULL;
		if (szType != NULL)
		{
			if (strncmp(szType, "IMPROVEMENT_", 12) == 0)
			{
				return CvCascadePackage<CvPlot>::PLOTSEG_IMPROVEMENT;
			}
			if (strncmp(szType, "TERRAIN_", 8) == 0 || strncmp(szType, "FEATURE_", 8) == 0
				|| strncmp(szType, "BONUS_", 6) == 0)
			{
				return CvCascadePackage<CvPlot>::PLOTSEG_NATURE;
			}
		}
		return CvCascadePackage<CvPlot>::PLOTSEG_REST;   // route + the owner's plot-scope flats
	}

	// ⛔ PLANE C ON THE PLOT PLANE -- the half that did not exist. mc_applyGated's plot branch needs a PLOT, and
	// every atom route passed NULL, so NO plot-scope deposit was ever re-booked by ANY crossing: a tech acquired
	// after a farm is built never upgraded that farm's yield, and nothing would ever have corrected it
	// ([DEC-no-self-heal]). At LOAD it hid, because the improvement facts stream while every tech is already
	// held, so the conditions resolved true on the way in -- which is exactly why it survived: it is invisible on
	// the one path anybody measures.
	// ⚑ Same IDEMPOTENT booking as the city plane: want-vs-booked, so it can never stack on the apply that the
	// plot's own substrate fact already made.
	void mc_bookGatedPlot(const std::vector<DepositIndex::GatedDeposit>* pGated,
		const std::vector<CvCascadePackage<CvPlot>::PlotSegment>& kSegments, const CvPlot& kPlot,
		McGatedTally* pTally)
	{
		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtxAtPlot(kPlot, evalCtx);
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			const DepositIndex::GatedDeposit& kGated = (*pGated)[iGated];
			if (kGated.deposit == NULL || kGated.deposit->entry == NULL
				|| !kPlot.getCascadePackage().hasAppliedSource(kGated.sourceIndex))
			{
				continue;   // this source never deposited on this tile -- there is nothing of its to move
			}
			if (pTally != NULL) { pTally->iFound++; }
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (!MMKernel::resolveEntry(*kGated.deposit->entry, 1, CASC_SCOPE_PLOT, evalCtx, &kPlot,
				false, iChannel, bPercentSide, iValue, MMKernel::PER_SCALE_APPLIED))
			{
				iChannel = -1;
				iValue = 0;
			}
			// the ORIGIN RULE: plot is yield-only, so a plot-scope percent has no side to land on
			if (bPercentSide)
			{
				iChannel = -1;
				iValue = 0;
			}
			// Same value DIFFERENCE as the city plane -- a gate turning off resolves to 0 and is withdrawn; a
			// count moving resolves to a new magnitude and only the difference lands.
			const CvCascadePackage<CvPlot>::BookedDeposit kBooked =
				kPlot.getCascadePackage().bookedDeposit(kGated.deposit->entry);
			if (kBooked.iChannel == iChannel && kBooked.iValue == iValue)
			{
				if (pTally != NULL && iValue == 0) { pTally->iRefused++; }
				continue;
			}
			if (kBooked.iValue != 0 && kBooked.iChannel != iChannel)
			{
				foldPlotSegment(kPlot, kSegments[iGated], kBooked.iChannel, -kBooked.iValue);
				if (iValue != 0)
				{
					foldPlotSegment(kPlot, kSegments[iGated], iChannel, iValue);
				}
			}
			else
			{
				const int64_t iDelta = iValue - kBooked.iValue;
				const int iSlot = (iChannel >= 0) ? iChannel : kBooked.iChannel;
				if (iDelta != 0 && iSlot >= 0)
				{
					foldPlotSegment(kPlot, kSegments[iGated], iSlot, iDelta);
				}
			}
			kPlot.getCascadePackage().setBookedDeposit(kGated.deposit->entry, iChannel, false, iValue);
			if (pTally != NULL) { pTally->iApplied++; }
		}
	}

	// ⚖ THE RE-BOOK AT EMPIRE SCOPE -- the same value DIFFERENCE as the city and plot planes, for the gates that
	// are THRESHOLDS rather than presence crossings.
	// ⛔ A THRESHOLD CANNOT RIDE THE ±1 CROSSING ROUTE, and that is why this exists rather than reusing
	// mc_applyGated. `{type: "ERA", max: 1}` is not "held" or "not held": when the era advances, some deposits
	// turn OFF and others turn ON, and there is no atom verdict to pin an as-if-held hypothetical to. Resolving
	// each entry against the NEW state and moving only its difference from what is booked handles both directions
	// in one pass, and is idempotent if the same fact is seen twice.
	void mc_bookGatedEmpire(const std::vector<DepositIndex::GatedDeposit>* pGated, const CvPlayer& kPlayer,
		McGatedTally* pTally)
	{
		if (pGated == NULL)
		{
			return;
		}
		CvCascadeEvalCtx evalCtx;
		kPlayer.getEmpireContext().fillEvalCtx(evalCtx);
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			const DepositIndex::GatedDeposit& kGated = (*pGated)[iGated];
			if (kGated.deposit == NULL || kGated.deposit->entry == NULL
				|| (CvCascScope)kGated.deposit->scopeIdx != CASC_SCOPE_EMPIRE)
			{
				continue;
			}
			if (pTally != NULL) { pTally->iFound++; }
			if (!kPlayer.getCascadePackage().hasAppliedSource(kGated.sourceIndex))
			{
				if (pTally != NULL) { pTally->iNoSource++; }
				continue;   // this source never deposited here -- there is nothing of its to move
			}
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (!MMKernel::resolveEntry(*kGated.deposit->entry, 1, CASC_SCOPE_EMPIRE, evalCtx, NULL, false,
				iChannel, bPercentSide, iValue, MMKernel::PER_SCALE_APPLIED))
			{
				iChannel = -1;
				iValue = 0;
			}
			const CvCascadePackage<CvPlayer>::BookedDeposit kBooked =
				kPlayer.getCascadePackage().bookedDeposit(kGated.deposit->entry);
			if (kBooked.iChannel == iChannel && kBooked.bPercent == bPercentSide && kBooked.iValue == iValue)
			{
				if (pTally != NULL) { pTally->iRefused++; }
				continue;   // already exactly what the data calls for
			}
			if (kBooked.iValue != 0 && (kBooked.iChannel != iChannel || kBooked.bPercent != bPercentSide))
			{
				if (kBooked.bPercent) kPlayer.getCascadePackage().applyPercent(kBooked.iChannel, (int)-kBooked.iValue);
				else                  kPlayer.getCascadePackage().applyFlat(kBooked.iChannel, -kBooked.iValue);
				if (iValue != 0)
				{
					if (bPercentSide) kPlayer.getCascadePackage().applyPercent(iChannel, (int)iValue);
					else              kPlayer.getCascadePackage().applyFlat(iChannel, iValue);
				}
			}
			else
			{
				const int64_t iDelta = iValue - kBooked.iValue;
				const int iSlot = (iChannel >= 0) ? iChannel : kBooked.iChannel;
				if (iDelta != 0 && iSlot >= 0)
				{
					if (bPercentSide || kBooked.bPercent) kPlayer.getCascadePackage().applyPercent(iSlot, (int)iDelta);
					else                                  kPlayer.getCascadePackage().applyFlat(iSlot, iDelta);
				}
			}
			kPlayer.getCascadePackage().setBookedDeposit(kGated.deposit->entry, iChannel, bPercentSide, iValue);
			mc_noteChannelApplied(iChannel, (int)kPlayer.getID());
			if (pTally != NULL) { pTally->iApplied++; }
			if (iValue != 0)
			{
				CascadeChannelRegistry::reportDepositApply(
					(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_EMPIRE,
					bPercentSide, iValue, (int)kPlayer.getID(), -1, "gated");
			}
		}
	}

	// Does this atom gate anything that lands on the PLOT plane at all? The plot pass is the expensive half (an
	// eval ctx per plot per atom, over every tile the owner's cities can work), and the overwhelming majority of
	// atoms gate nothing there -- so asking the compiled list first turns a whole-map walk into a no-op for them.
	// ⚑ It is a pure filter over the SAME list the pass would walk, never a second index: an atom that answers
	// yes is walked exactly as before, and one that answers no had nothing on that plane to move.
	bool mc_gatesAnyPlotDeposit(const std::vector<DepositIndex::GatedDeposit>* pGated)
	{
		if (pGated == NULL)
		{
			return false;
		}
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			const DepositIndex::GatedDeposit& kGated = (*pGated)[iGated];
			if (kGated.deposit != NULL && (CvCascScope)kGated.deposit->scopeIdx == CASC_SCOPE_PLOT)
			{
				return true;
			}
		}
		return false;
	}

	// Every plot an owner's cities can work, deduped -- the reach of an empire-level atom on the plot plane.
	void mc_bookGatedOwnerPlots(const std::vector<DepositIndex::GatedDeposit>* pGated, const CvPlayer& kOwner,
		McGatedTally* pTally)
	{
		if (pGated == NULL || pGated->empty() || !mc_gatesAnyPlotDeposit(pGated))
		{
			return;
		}
		std::vector<CvCascadePackage<CvPlot>::PlotSegment> kSegments;
		kSegments.reserve(pGated->size());
		for (size_t iGated = 0; iGated < pGated->size(); ++iGated)
		{
			kSegments.push_back(mc_segmentForSource((*pGated)[iGated].source));
		}
		std::set<const CvPlot*> kSeen;
		for (CvPlayer::city_iterator cityIterator = kOwner.beginCities();
			cityIterator != kOwner.endCities(); ++cityIterator)
		{
			const CvCity* pCity = *cityIterator;
			if (pCity == NULL)
			{
				continue;
			}
			const int iNumPlots = pCity->getNumCityPlots();
			for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
			{
				const CvPlot* pPlot = pCity->getCityIndexPlot(iPlotIndex);
				if (pPlot == NULL || kSeen.find(pPlot) != kSeen.end())
				{
					continue;
				}
				kSeen.insert(pPlot);
				mc_bookGatedPlot(pGated, kSegments, *pPlot, pTally);
			}
		}
	}

	// PLANE C for a BONUS atom: this city's holding of one resource crossed, so every deposit that atom GATES is
	// re-booked -- withdrawn under the verdict it was booked at, re-applied under the verdict now in force.
	//
	// ⛔ TWO PASSES, AND THE PAIR IS WHAT MAKES IT EXACT -- a single "apply the arrival" pass would be WORSE than
	// no route at all, because the deposit would then compound on every re-acquisition where today it is merely
	// absent. Pass 1 pins the atom to its OLD verdict and withdraws what WAS applying; pass 2 pins the NEW verdict
	// and applies what SHOULD be. Nothing is re-derived and nothing is swept: each pass is the ordinary resolve
	// against a pinned atom, so this is a DELTA route like plane A and plane B -- never a re-evaluation of state
	// ([DEC-no-self-heal] stands: no reader repairs itself, a FACT moves the sum).
	// ⚑ The pair is also what makes `enabled` and `disabled` gates uniform without a special case: a
	// `disabled: BONUS_X` deposit was applying while the bonus was ABSENT, so pass 1 withdraws it on arrival and
	// pass 2 re-applies it on loss -- the same two lines, read in the other direction.
	// ⚖ Both halves are announced even when one is empty, exactly as the served-resource write point announces
	// both halves of a tile changing which resource it serves ([state-repositories.md] § THE INVARIANT).
	// THE ONE TYPE-ATOM ROUTE. szType is the atom's own spelling, which is how di_scanConditionTree interned
	// every PRESENCE node -- so this reaches the SAME deposit list the gate compiled into, never a second index.
	// eBucket/iId name the axis the hypothetical pins; a kind whose ev_present branch is not pin-aware CANNOT be
	// routed here, because its withdrawal would silently resolve false (see ev_present's note).
	void mc_applyTypeAtom(const char* szType, EnEdgeBucket eBucket, int iId, bool bHeldNow,
		const CvPlayer* pPlayer, const CvCity* pCity)
	{
		if (szType == NULL || iId < 0)
		{
			return;
		}
		const std::vector<DepositIndex::GatedDeposit>* pGated = DepositIndex::gatedByType(std::string(szType));
		if (pGated == NULL)
		{
			return;   // nothing anywhere is conditioned on this entity
		}
		CvCascadeHypothetical kWasHeld;
		CvCascadeHypothetical kIsHeld;
		// OLD verdict = the opposite of what now holds; NEW verdict = what now holds.
		(bHeldNow ? kWasHeld.absent : kWasHeld.present)[eBucket].insert(iId);
		(bHeldNow ? kIsHeld.present : kIsHeld.absent)[eBucket].insert(iId);
		McGatedTally kTally;
		if (pCity != NULL)
		{
			mc_bookGated(pGated, *pCity, &kTally);
			// ...and the PLOT plane of the same crossing: a city-bound atom reaches the tiles that city works.
			{
				std::vector<CvCascadePackage<CvPlot>::PlotSegment> kSegments;
				kSegments.reserve(pGated->size());
				for (size_t iSeg = 0; iSeg < pGated->size(); ++iSeg)
				{
					kSegments.push_back(mc_segmentForSource((*pGated)[iSeg].source));
				}
				const int iNumPlots = pCity->getNumCityPlots();
				for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
				{
					const CvPlot* pAtomPlot = pCity->getCityIndexPlot(iPlotIndex);
					if (pAtomPlot != NULL)
					{
						mc_bookGatedPlot(pGated, kSegments, *pAtomPlot, &kTally);
					}
				}
			}
			CascadeChannelRegistry::reportAtomRoute(szType, (int)pGated->size(), kTally.iFound,
				kTally.iNoSource, kTally.iRefused, kTally.iApplied,
				(int)pCity->getOwner(), pCity->getID());
			return;
		}
		if (pPlayer == NULL)
		{
			return;
		}
		// ⛔ AN EMPIRE-LEVEL CROSSING GATES CITY-SCOPE DEPOSITS IN EVERY CITY, so it must fan -- a tech naming no
		// city would otherwise skip mc_applyGated's city branch entirely and move nothing where the deposits
		// actually live. ⚠ The fan is SPLIT, and the split is what keeps it exact: the owner pass carries NO city
		// (so mc_applyGated declines every city-scope deposit) and each city pass carries NO player (so it
		// declines every empire/team-scope one). Passing both together would apply each empire-scope deposit once
		// PER CITY -- a compounding over-apply that nothing later withdraws.
		// The OWNER pass runs regardless: the player exists even mid-read, so empire/team-scope deposits land now.
		mc_applyGated(pGated, -1, MMKernel::PER_SCALE_APPLIED, pPlayer, NULL, NULL, &kWasHeld, &kTally);
		mc_applyGated(pGated, +1, MMKernel::PER_SCALE_APPLIED, pPlayer, NULL, NULL, &kIsHeld, &kTally);
		if (spineGameLoadInProgress())
		{
			// The CITY half is banked whole -- the cities it would walk may not exist yet, and a fan over an
			// empty list is indistinguishable from a fan with nothing to do.
			s_bankedAtomFans[std::make_pair(std::string(szType), (int)pPlayer->getID())] = bHeldNow;
			CascadeChannelRegistry::reportAtomRoute(szType, (int)pGated->size(), kTally.iFound,
				kTally.iNoSource, kTally.iRefused, kTally.iApplied, (int)pPlayer->getID(), -1);
			return;
		}
		for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities();
			cityIterator != pPlayer->endCities(); ++cityIterator)
		{
			if (*cityIterator != NULL)
			{
				mc_bookGated(pGated, **cityIterator, &kTally);
			}
		}
		mc_bookGatedOwnerPlots(pGated, *pPlayer, &kTally);
		CascadeChannelRegistry::reportAtomRoute(szType, (int)pGated->size(), kTally.iFound,
			kTally.iNoSource, kTally.iRefused, kTally.iApplied, (int)pPlayer->getID(), -1);
	}

	void mc_applyBonusAtom(int iBonus, bool bHeldNow, const CvPlayer* pPlayer, const CvCity* pCity)
	{
		if (iBonus < 0 || iBonus >= GC.getNumBonusInfos())
		{
			return;
		}
		mc_applyTypeAtom(GC.getBonusInfo((BonusTypes)iBonus).getType(), EDGEB_BONUSES, iBonus,
			bHeldNow, pPlayer, pCity);
	}

	// THE DRAIN. Every deposit an atom crossing had to skip for want of its source is applied ONCE now, against
	// the fully-read game -- the sources are all in their packages and every store is final.
	// ⛔ It is NOT a recompute and NOT a sweep: the worklist is the exact set of deposits the banked FACTS named
	// and could not reach, and each one goes through the ONE per-entry resolve like every other apply
	// ([DEC-single-implementation]). Nothing is re-derived from live state, so [DEC-no-self-heal] stands -- a
	// deposit nobody's fact named is still never applied here.
	// ⚑ NO HYPOTHETICAL: the pin exists to reconstruct a verdict that has already moved on, and at the drain the
	// live verdict IS the one in force. A deposit whose atom ended the load un-held simply resolves false and is
	// correctly not applied.
	// ONE banked deposit applied at the drain: the source is present, the state is final, so the live verdict is
	// the one in force and no pin is needed. Shared by both drains so the two can never disagree about what
	// "apply an owed arrival" means ([DEC-single-implementation]).
	// A drain's outcome, PER CHANNEL. ⛔ A bulk refusal count is unfalsifiable: 566,224 refusals are exactly what
	// a correct drain looks like AND exactly what a broken condition looks like. Split by channel it stops being
	// either -- a channel refusing far out of proportion to its siblings is a finding, and one refusing in
	// proportion is evidence the refusals are real ([DEC-no-guessing]).
	struct McDrainByChannel
	{
		std::map<int, int> refused;
		std::map<int, int> applied;
	};

	// ⛔ WHY it refused, not only how many -- the same argument as the per-channel split above, on the other axis.
	// mc_drainApplyOne declines for three unrelated reasons and the route line summed them into one number, which
	// cannot tell a bank storing NEGATIVE SPACE (the city simply never holds that source) from a bank duplicating
	// plane A (the deposit is already booked). Those two have OPPOSITE fixes -- shrink what is banked, versus
	// delete the bank -- so collapsing them makes the number unactionable ([DEC-no-guessing]: at a gap, EMIT the
	// decomposition; a bare total supports neither VERIFY nor ASK).
	struct McDrainReasons
	{
		int iNoSource;
		int iBooked;
		int iResolveRefused;
		int iApplied;
		McDrainReasons() : iNoSource(0), iBooked(0), iResolveRefused(0), iApplied(0) {}
	};

	void mc_reportDrainByChannel(const char* szWhich, const McDrainByChannel& kTally)
	{
		std::map<int, int> keys;
		std::map<int, int>::const_iterator it;
		for (it = kTally.refused.begin(); it != kTally.refused.end(); ++it) { keys[it->first] = 1; }
		for (it = kTally.applied.begin(); it != kTally.applied.end(); ++it) { keys[it->first] = 1; }
		for (it = keys.begin(); it != keys.end(); ++it)
		{
			const int iChannel = it->first;
			const std::map<int, int>::const_iterator itRef = kTally.refused.find(iChannel);
			const std::map<int, int>::const_iterator itApp = kTally.applied.find(iChannel);
			const int iRefused = (itRef == kTally.refused.end()) ? 0 : itRef->second;
			const int iApplied = (itApp == kTally.applied.end()) ? 0 : itApp->second;
			std::string szLabel(szWhich);
			szLabel += ":";
			szLabel += CascadeChannelRegistry::channelName(iChannel);
			CascadeChannelRegistry::reportAtomRoute(szLabel.c_str(), iRefused + iApplied, iRefused + iApplied,
				0, iRefused, iApplied, -1, -1);
		}
	}

	int mc_channelOfEntry(const CvModEntry& kEntry)
	{
		return CascadeChannelRegistry::channelLookup(kEntry.family, kEntry.kind, kEntry.propertyFk);
	}

	bool mc_drainApplyOne(const DepositIndex::GatedDeposit& kGated, const CvPlayer& kOwner, const CvCity& kCity,
		McDrainByChannel* pByChannel, McDrainReasons* pReasons)
	{
		if (kGated.deposit == NULL || kGated.deposit->entry == NULL)
		{
			return false;
		}
		if (!kCity.getBuildingYields().hasAppliedSource(kGated.sourceIndex))
		{
			if (pReasons != NULL) { pReasons->iNoSource++; }
			return false;
		}
		// The drain owes an arrival ONLY for a deposit that is not already booked -- otherwise it stacks on top of
		// whatever plane A or an earlier crossing already put in the slot, which is the same double-apply the book
		// exists to stop.
		if (kCity.getBuildingYields().isGatedBooked(kGated.deposit->entry))
		{
			if (pReasons != NULL) { pReasons->iBooked++; }
			return false;
		}
		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtx(kCity.getCityContext(), kOwner.getEmpireContext(),
			kCity.plotGroup(kCity.getOwner()), evalCtx);
		EnablerKernel::wireOperatingBuildings(&kCity, evalCtx);
		int iChannel = -1;
		bool bPercentSide = false;
		int64_t iValue = 0;
		if (!MMKernel::resolveEntry(*kGated.deposit->entry, 1, CASC_SCOPE_CITY, evalCtx, NULL, false,
			iChannel, bPercentSide, iValue, MMKernel::PER_SCALE_APPLIED))
		{
			if (pByChannel != NULL)
			{
				// resolveEntry leaves the out-params untouched when it declines, so the channel comes from the
				// ENTRY's own address -- the refusal still has to be attributable to a channel.
				const int iEntryChannel = mc_channelOfEntry(*kGated.deposit->entry);
				if (iEntryChannel >= 0) { pByChannel->refused[iEntryChannel]++; }
			}
			if (pReasons != NULL) { pReasons->iResolveRefused++; }
			return false;
		}
		if (pReasons != NULL) { pReasons->iApplied++; }
		if (pByChannel != NULL) { pByChannel->applied[iChannel]++; }
		if (bPercentSide) kCity.getCityPercents().applyPercent(iChannel, (int)iValue);
		else              kCity.getBuildingYields().applyFlat(iChannel, iValue);
		kCity.getBuildingYields().setBookedDeposit(kGated.deposit->entry, iChannel, bPercentSide, iValue);
		CascadeChannelRegistry::reportDepositApply(
			(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_CITY,
			bPercentSide, iValue, (int)kCity.getOwner(), kCity.getID(), "atomDrain");
		return true;
	}

	// The FAN drain, and it is the PLOT PLANE ONLY. An empire-level crossing's plot half is the one thing no other
	// route reaches during a save read; its CITY half is served by the two-leg fold -- a city that starts existing
	// folds the city-scope deposits of every source its owner already holds ([modifier.md] §5) -- so replaying the
	// crossing over the owner's cities here would be a second maintenance surface for work another route already
	// does ([roadmap.md]: a wrong wiring is removed on sight).
	void mc_drainBankedAtomFanPlots()
	{
		for (std::map<std::pair<std::string, int>, bool>::const_iterator it = s_bankedAtomFans.begin();
			it != s_bankedAtomFans.end(); ++it)
		{
			if (!it->second)
			{
				continue;   // ended the load un-held: it owes no arrival
			}
			const int iOwner = it->first.second;
			if (iOwner < 0 || iOwner >= MAX_PLAYERS)
			{
				continue;
			}
			const std::vector<DepositIndex::GatedDeposit>* pGated = DepositIndex::gatedByType(it->first.first);
			if (pGated == NULL)
			{
				continue;
			}
			const CvPlayer& kOwner = GET_PLAYER((PlayerTypes)iOwner);
			// ⛔ THE PLOT HALF OF THE SAME CROSSING, AND ITS ABSENCE WAS THE LARGEST HOLE ON THE YIELD PLANE.
			// mc_applyTypeAtom's empire branch returns at the load guard ABOVE its mc_bookGatedOwnerPlots call, so
			// during a save read an empire-level atom reached the plot plane through nothing at all -- while the
			// CITY-bound atoms (a bonus obtained) did, because their branch carries no such guard. The asymmetry is
			// what made it look wired.
			// ⚑ WHY IT COSTS SO MUCH: 1,074 of the 1,373 plot-scope deposit entries in Assets/Data are CONDITIONED,
			// and 305 of those name a TECH. A tech-gated improvement yield is a TIER-1 BASE term (modifier.md §2a --
			// the plot package IS the base the whole percent stack multiplies), so every one of them missing
			// understates the city rate BEFORE any modifier is applied.
			// ⚑ NO PIN, for the same reason the city drain needs none: the atom ended the load HELD (the un-held
			// ones are skipped above), so the live verdict IS the one in force and mc_bookGatedPlot's own value
			// difference makes the apply idempotent against whatever plane A already booked.
			// ⚠ It reports PER OWNER rather than as one total: a pass that applied thousands for one player and
			// nothing for the next is a finding, and a single summed line cannot express it.
			McGatedTally kPlotTally;
			mc_bookGatedOwnerPlots(pGated, kOwner, &kPlotTally);
			if (kPlotTally.iFound > 0)
			{
				CascadeChannelRegistry::reportAtomRoute("<atomFanDrainPlots>", kPlotTally.iFound,
					kPlotTally.iFound, kPlotTally.iNoSource, kPlotTally.iRefused, kPlotTally.iApplied,
					iOwner, -1);
			}
		}
		s_bankedAtomFans.clear();
	}

	void mc_drainBankedAtomCrossings()
	{
		int iApplied = 0;
		int iFound = 0;
		McDrainByChannel kByChannel;
		McDrainReasons kReasons;
		for (std::set<BankedAtomCrossing>::const_iterator it = s_bankedAtomCrossings.begin();
			it != s_bankedAtomCrossings.end(); ++it)
		{
			const std::vector<DepositIndex::GatedDeposit>* pList = it->pList;
			if (pList == NULL || it->iOwner < 0 || it->iOwner >= MAX_PLAYERS)
			{
				continue;
			}
			const CvPlayer& kOwner = GET_PLAYER((PlayerTypes)it->iOwner);
			// The owner's cities are walked LIVE, so a city that has since gone is simply absent and a city
			// founded after the crossing is included -- neither needs a stored id to chase.
			foreach_(const CvCity* pCity, kOwner.cities())
			{
				if (pCity == NULL)
				{
					continue;
				}
				for (size_t iGated = 0; iGated < pList->size(); ++iGated)
				{
					const DepositIndex::GatedDeposit& kGated = (*pList)[iGated];
					if (kGated.deposit == NULL || kGated.deposit->entry == NULL)
					{
						continue;
					}
					++iFound;
					// The source and book tests are mc_drainApplyOne's, not repeated here: duplicating one made
					// the refusal unattributable, since the pre-filter consumed the case before the reason could
					// be counted.
					if (mc_drainApplyOne(kGated, kOwner, *pCity, &kByChannel, &kReasons))
					{
						++iApplied;
					}
				}
			}
		}
		// ⚑ noSource is now the honest COST of re-deriving rather than the bank's own waste: the drain offers every
		// (deposit x city) pair the crossing could reach and the source test declines the ones this city does not
		// hold. It is paid in tests, not in resident memory -- which is the whole of the re-key.
		const int iBanked = (int)s_bankedAtomCrossings.size();
		CascadeChannelRegistry::reportAtomRoute("<atomDrain>", iBanked, iFound, kReasons.iNoSource,
			kReasons.iBooked + kReasons.iResolveRefused, iApplied, -1, -1);
		CascadeChannelRegistry::reportAtomRoute("<atomDrainBooked>", kReasons.iBooked, kReasons.iBooked,
			0, kReasons.iResolveRefused, kReasons.iBooked, -1, -1);
		mc_reportDrainByChannel("depDrain", kByChannel);
		s_bankedAtomCrossings.clear();
		std::set<BankedAtomCrossing>().swap(s_bankedAtomCrossings);   // load-time scratch, not resident state
	}

	const CvPlayer* mc_player(int iPlayer)
	{
		return (iPlayer >= 0 && iPlayer < MAX_PLAYERS) ? &GET_PLAYER((PlayerTypes)iPlayer) : NULL;
	}

	const CvCity* mc_city(const CvPlayer* pPlayer, int iCityId)
	{
		return (pPlayer != NULL && iCityId >= 0) ? pPlayer->getCity(iCityId) : NULL;
	}

	const CvPlot* mc_plot(int iPlotId)
	{
		return (iPlotId >= 0 && iPlotId < GC.getMap().numPlots()) ? GC.getMap().plotByIndex(iPlotId) : NULL;
	}

	//
	//	The consumer. DOMAIN-only; LOAD-ACTIVE (the reseed's in-read emits build the dirty picture).
	//
	class CvModifierConsumer : public IEventConsumer
	{
	public:
		int wantedKinds() const
		{
			return 1 << EVENTKIND_DOMAIN;
		}

		void onEvent(const CvSpineEvent& kEvent)
		{
			const char* szSource = spineEventName(kEvent.iEventId);
			const CvPlayer* pPlayer = mc_player(kEvent.iC);
			switch (kEvent.iEventId)
			{
			// ---- the two NON-deposit-addressed facts: a source's basis moved, not a deposit ----
			// A GAME OPTION flipped. Options are read BELOW the entity-gate level too -- civics carry
			// option-gated production / happiness / commerce deposits -- so a flip changes what live sources
			// deposit, for every player at once, with no per-source route to derive it from.
			// ⚠ The GAME space ONLY -- no authored deposit condition names a MODDERGAMEOPTION_, so a modder-option
			// flip changes no deposit and marking for it would be a blanket bought with nothing.
			case SEVT_GAME_OPTION_ADDED:
			case SEVT_GAME_OPTION_REMOVED:
				if (kEvent.iB == GAMEOPTSPACE_GAME)
				{
					for (int iP = 0; iP < MAX_PLAYERS; iP++)
					{
						const CvPlayer& kPlayerX = GET_PLAYER((PlayerTypes)iP);
						if (kPlayerX.isAlive())
						{
							mc_markPlayerWhole(&kPlayerX, szSource);
						}
					}
				}
				break;
			// A player's DIFFICULTY moved (flexible difficulty). The handicap is a modifier SOURCE the gather
			// folds per scope, so this one player's whole basis re-derives.
			case SEVT_EMPIRE_HANDICAP_ADDED:
			case SEVT_EMPIRE_HANDICAP_REMOVED:
				mc_markPlayerWhole(pPlayer, szSource);
				break;
			// ---- source-carrying state changes: the mask IS the source's compiled route ----
			// The OPERATE CROSSING -- deposits flow only while the building is operating, so this is the fact that
			// starts and stops them. ⛔ Deliberately NOT the "processed" completion notice, which is DIAGNOSTIC and
			// says only that an apply ran (event-spine.md § THE RECEIVED LINE).
			case SEVT_CITY_BUILDING_ACTIVATED:
			case SEVT_CITY_BUILDING_DORMANTED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					mc_applySource(&GC.getBuildingInfo((BuildingTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL, CASC_ORIGIN_BUILDING);
				}
				break;
			}
			// The PRESENCE happenings -- re-resolve everything CONDITIONED on holding this building. Both
			// directions do the same work here and that is not the fact being non-specific: a condition atom
			// naming the building is re-resolved against CURRENT state either way, so the direction is genuinely
			// not an input to this consumer. The building's OWN deposits ride the operate crossing instead, since
			// a present-but-dormant building deposits nothing ([enabler.md] §3.2).
			case SEVT_CITY_BUILDING_ADDED:
			case SEVT_CITY_BUILDING_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					// PLANE C: everything conditioned on HOLDING this building. The building's own deposits ride
					// the operate crossing above; this is the other half -- what everything ELSE deposits because
					// this city has it.
					mc_applyTypeAtom(GC.getBuildingInfo((BuildingTypes)kEvent.iType).getType(), EDGEB_BUILDINGS,
						kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, pCity);
					// ...and the WONDER-CATEGORY count route (plane B): a wonder arriving or leaving moves this
					// city's category count, which the §3.1 count tokens read (the trait free-specialist-per-wonder
					// scaler). The category comes from the ONE classification the count maintenance itself uses --
					// derived from which self-cap the building authors, never an isWorldWonder mirror
					// ([enabler.md] §4). Re-BOOKED for the same reason every count route is: perApply is a step
					// function, so the re-book against the count as it stands is the only exact form.
					if (pCity != NULL)
					{
						const char* szWonderToken = NULL;
						if      (isWorldWonder((BuildingTypes)kEvent.iType))    { szWonderToken = "WORLD_WONDER"; }
						else if (isTeamWonder((BuildingTypes)kEvent.iType))     { szWonderToken = "TEAM_WONDER"; }
						else if (isNationalWonder((BuildingTypes)kEvent.iType)) { szWonderToken = "NATIONAL_WONDER"; }
						const std::vector<DepositIndex::GatedDeposit>* pWonderGated =
							(szWonderToken != NULL) ? DepositIndex::gatedByToken(szWonderToken) : NULL;
						if (pWonderGated != NULL)
						{
							McGatedTally kWonderTally;
							mc_bookGated(pWonderGated, *pCity, &kWonderTally);
							CascadeChannelRegistry::reportAtomRoute(szWonderToken, (int)pWonderGated->size(),
								kWonderTally.iFound, kWonderTally.iNoSource, kWonderTally.iRefused,
								kWonderTally.iApplied, (int)pCity->getOwner(), pCity->getID());
						}
					}
				}
				break;
			}
			// The EMPIRE-LEVEL building pairs (DEC-empire-level-buildings) -- the city pair's split, one scope
			// up: the member's OWN deposits ride its player-side OPERATE crossing (a held-but-dormant member
			// deposits nothing), and plane-C atom re-resolution rides the held crossing. No city -- the deposits
			// are empire-scope by curation and land in the player's package, rolling down at the read.
			case SEVT_EMPIRE_BUILDING_ACTIVATED:
			case SEVT_EMPIRE_BUILDING_DORMANTED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					mc_applySource(&GC.getBuildingInfo((BuildingTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
				}
				break;
			}
			case SEVT_EMPIRE_BUILDING_ADDED:
			case SEVT_EMPIRE_BUILDING_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					mc_applyTypeAtom(GC.getBuildingInfo((BuildingTypes)kEvent.iType).getType(), EDGEB_BUILDINGS,
						kEvent.iType, kEvent.iEventId == SEVT_EMPIRE_BUILDING_ADDED, pPlayer, NULL);
				}
				break;
			}
			case SEVT_CITY_RELIGION_ADDED:
			case SEVT_CITY_RELIGION_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumReligionInfos())
				{
					mc_applySource(&GC.getReligionInfo((ReligionTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL, CASC_ORIGIN_BUILDING);
				}
				mc_applyGated(DepositIndex::gatedByReligionCounts(), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumReligionInfos())
				{
					mc_applyTypeAtom(GC.getReligionInfo((ReligionTypes)kEvent.iType).getType(), EDGEB_RELIGIONS,
						kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, pCity);
				}
				break;
			}
			case SEVT_CITY_CORPORATION_ADDED:
			case SEVT_CITY_CORPORATION_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumCorporationInfos())
				{
					mc_applySource(&GC.getCorporationInfo((CorporationTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL, CASC_ORIGIN_BUILDING);
					mc_applyTypeAtom(GC.getCorporationInfo((CorporationTypes)kEvent.iType).getType(),
						EDGEB_CORPORATIONS, kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, pCity);
				}
				break;
			}
			case SEVT_CITY_SPECIALIST_ADDED:
			case SEVT_CITY_SPECIALIST_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumSpecialistInfos())
				{
					// THE SPECIALIST PLANE. A specialist's YIELD is TIER 1 -- inside the city stack, unlike a
					// building's ([modifier.md] 2a) -- so its flats land on their own plane. Its PERCENTS do not:
					// a modifier a specialist authors is an ordinary city modifier (owner) and joins the one
					// additive stack with everyone else's. Buildings issue free specialist COUNTS; the SPECIALIST
					// issues the yield.
					mc_applySource(&GC.getSpecialistInfo((SpecialistTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL, CASC_ORIGIN_SPECIALIST);
				}
				break;
			}
			case SEVT_CITY_BONUS_ADDED:              // the city obtained a bonus over the network
			case SEVT_CITY_BONUS_REMOVED:            // ... or lost it
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
				{
					// PLANE A: what the RESOURCE itself deposits at this city.
					// ⛔ THIS FACT IS THE SOLE CARRIER, because it is the only one of the three that is a CITY's
					// has-verdict. The other two describe the same holding from further out -- the city's local
					// supply, and the network component it hangs off -- so each one that deposits here counts one
					// resource again ([enabler.md] §8). The vicinity case below re-gates only; the plot-group case
					// declines entirely.
					mc_applySource(&GC.getBonusInfo((BonusTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL, CASC_ORIGIN_BUILDING);
					// PLANE C: what everything ELSE deposits BECAUSE this city holds the resource -- the building
					// yields gated on it. ⛔ Without this the two are silently different questions with one answer:
					// the resource's own deposits move and every deposit CONDITIONED on it keeps whatever verdict it
					// was booked at, forever ([DEC-no-self-heal] -- nothing re-derives a maintained sum).
					// ⚑ It falls hardest on FOOD, whose building deposits are gated overwhelmingly on BONUS_* atoms
					// where production's are gated on TECH_* -- techs sit on the team and are already correct when
					// the cities deserialize, so food alone read short while every other channel looked right.
					// ⚑ THIS IS ALSO WHAT MAKES LOAD ORDER IRRELEVANT. The onSite store fills at the membership
					// drain and the network list assembles from its own facts, both AFTER the buildings activate --
					// so at the instant a deposit first resolved, the lists it asks were empty or half-built. Every
					// one of those fills is itself a crossing, and a crossing now re-books the deposits it gates.
					if (pCity != NULL)
					{
						mc_applyBonusAtom(kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, pCity);
					}
				}
				break;
			}
			// A city's LOCAL (vicinity) supply count moved -- PLANE C ONLY, so what moves here is the VERDICT of
			// the deposits gated on this bonus and never the bonus's own ([enabler.md] §8, [json.md] §3.4).
			// ⚠ Its payload is a COUNT, which is what made depositing here COMPOUND rather than merely double: the
			// count is the multiplicity, so a city with three local copies scaled the resource's own deposit by
			// three -- and the supply only ever grew on this save, so nothing came back out.
			case SEVT_CITY_VICINITY_BONUS_ADDED:
			case SEVT_CITY_VICINITY_BONUS_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pCity != NULL && kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
				{
					mc_applyBonusAtom(kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, pCity);
				}
				break;
			}
			// The trade network's resource set. ⛔ DELIBERATELY NO DEPOSIT AND NO RE-GATE -- declining IS the
			// handler. `CvPlotGroup::changeNumBonuses` already fans its member cities so each fires its own
			// crossing, so every city this could reach is served by the case above, naming itself.
			// ⚠ It could not have been served here anyway, and that is the part worth knowing: `iSrcLoc` is a
			// PLOT-GROUP id rather than a city, so there is no city to resolve and the owner-wide fan it fell back
			// on reached the player's OTHER plot groups too -- cities that never held the resource at all.
			case SEVT_PLOTGROUP_BONUS_ADDED:
			case SEVT_PLOTGROUP_BONUS_REMOVED:
				break;
			// ⛔ SEVT_CITY_NETWORK_CHANGED is deliberately NOT handled here, and its absence is a DECISION rather
			// than a missing route. The membership move's resource consequences are announced individually and
			// FIRST, by the same choke point: CvCity::onNetworkSupplyChanged walks the old and new groups' holdings
			// and fires a per-bonus obtained/lost fact for every genuine presence crossing (the deferred path
			// replays them at endDeferredBonusProcessing against the entry snapshot). Every one of those lands in
			// the case above, naming its bonus. Re-gating the whole owner on the membership fact as well would be
			// the same work a second time -- a blanket bought because the fact used to name nothing
			// ([DEC-facts-name-happenings]).
			// ---- plot substrate changes: the plot's isolated base package refills whole (the substrate IS
			// ---- the base; the event carries no old-type to narrow by) + the working city's rates ----
			// ---- THE UNIT PLANE: resolved values, not a package (state-repositories.md). The model names
			// ---- these two facts EXACTLY -- "they dirty on a different trigger from everything else: ONLY
			// ---- when a promotion or combat class changes" -- so there is no route derivation here and no
			// ---- blanket: the held set moved, so the unit re-resolves. Unit MOVEMENT never reaches this.
			case SEVT_UNIT_PROMOTION_ADDED:
			case SEVT_UNIT_PROMOTION_REMOVED:
			case SEVT_UNIT_COMBAT_ADDED:
			case SEVT_UNIT_COMBAT_REMOVED:
			// ⚑ BIRTH IS THE THIRD TRIGGER, and it is what serves the unit's OWN info's share of the plane. The
			// vision slot (and every non-delta slot) carries the unit's base, so a unit holding no promotion and
			// no extra combat class must still gather ONCE -- without this fact it read 0 sight forever and saw
			// only the plot it stood on. The in-read half of the same emit covers every loaded unit.
			case SEVT_UNIT_CREATED:
			{
				if (pPlayer != NULL && kEvent.iA >= 0)
				{
					const CvUnit* pUnit = pPlayer->getUnit(kEvent.iA);
					if (pUnit != NULL)
					{
						pUnit->markResolvedValuesDirty();
					}
				}
				break;
			}
			case SEVT_PLOT_IMPROVEMENT_ADDED:
			case SEVT_PLOT_IMPROVEMENT_REMOVED:
			case SEVT_PLOT_TERRAIN_ADDED:
			case SEVT_PLOT_TERRAIN_REMOVED:
			case SEVT_PLOT_FEATURE_ADDED:
			case SEVT_PLOT_FEATURE_REMOVED:
			case SEVT_PLOT_ROUTE_ADDED:
			case SEVT_PLOT_ROUTE_REMOVED:
			case SEVT_PLOT_BONUS_ADDED:
			case SEVT_PLOT_BONUS_REMOVED:
			{
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				if (pPlot != NULL)
				{
					// ⚖ PLANE A FOR THE PLOT-RESIDENT SOURCES -- this IS the yield base. The plot's package is what
					// every city rate is built on (modifier.md, the origin rule + plot-as-base), so a substrate source
					// depositing nothing here is not a slightly wrong number: the map has no yields at all.
					// ⚑ THE PAIRS ARE WHAT MAKE IT ONE CALL. The hole that stood here called for a WITHDRAW AND
					// REAPPLY -- the same plane-A walk run twice with opposite signs -- because ONE *_CHANGED fact
					// carried a swap and the departing source had to be recovered from a second payload field. One
					// source per fact with the direction in its id needs neither: _ADDED applies +1, _REMOVED applies
					// -1, and a swap is just the two facts the emitter already sends.
					const CvInfo* pSubstrate = mc_substrateInfo(kEvent.iEventId, kEvent.iType);
					if (pSubstrate != NULL)
					{
						// mc_plotSegmentFor routes it to the right SEGMENT of the plot package (nature / improvement /
						// rest), which is what keeps the §2a floors derivable from three plain sums.
						mc_applySource(pSubstrate, mc_sourceDirection(kEvent), kEvent.iEventId,
							pPlayer, pPlot->getPlotCity(), pPlot, CASC_ORIGIN_BUILDING);
					}
					// ⛔ The fresh-water verdict is NOT re-derived here, and the ring is not walked. Terrain and
					// feature are two of the axes PlotContext derives HAS_FRESHWATER from, so it re-derives the bit
					// and announces the crossing -- including on the 8 neighbours, whose own leg reads this plot.
					// Re-deriving it here as well would apply the crossing twice.

					// ⛔ The blanket above marks this PLOT's own package. It does NOT reach a deposit at CITY or
					// EMPIRE scope that is CONDITIONED on -- or `per`-scaled by -- this substrate TYPE; that is
					// exactly what the dependency route addresses, and without it such a deposit was never
					// re-marked when the substrate appeared, staying wrong until something unrelated dirtied it.
					// ⚑ ONE SOURCE PER FACT, named in iType: _ADDED names what arrived, _REMOVED what left. The
					// old-value decode this used to carry -- a departing id in iA beside the arriving one, with a
					// carve-out for the one fact that put a delta in iB instead -- is gone with the *_CHANGED shape
					// it existed for, and with it the bonus special case that shape needed.
					// ⚖ THERE IS NO SUBSTRATE TYPE-ATOM ROUTE, AND THAT IS A MEASUREMENT RATHER THAN A GAP.
					// This carried a "⛔ HOLE" for the deposits CONDITIONED on holding a substrate source. Counted
					// across the whole of Assets/Data, the type-prefixes naming an atom in any `enabled`/`disabled`
					// condition are: TECH 3205 · BONUS 3191 · RELIGION 183 · BUILDING 68 · CORPORATION 46 · CIVIC 22
					// (plus the HAS_/IS_ predicates, which route by predicate id). **IMPROVEMENT / TERRAIN / FEATURE /
					// ROUTE appear ZERO times.** Nothing anywhere is gated on holding one, so there is no deposit for
					// such a route to move ([triggers.md]: a verb with zero authorings is an EXAMPLE in the spec, not
					// live data -- do not build machinery for it). Should data ever author one, mc_applyTypeAtom is
					// the route and its bucket must first be made pin-aware in ev_present.
					//
					// ⚑ WHAT THE BONUS AXIS DOES NEED is the OTHER direction, and it is wired just below: 737 of the
					// plot-scope conditions read {HAS_BONUS: X} -- an improvement's own yield gated on the tile's
					// resource (the deliveryguy shape). At LOAD the ordering already works (CvPlot::read emits the
					// bonus BEFORE the improvement, so the improvement's plane-A apply sees it), but a resource
					// DISCOVERED on an already-improved tile moves the predicate with nothing to re-resolve it.
					if (kEvent.iEventId == SEVT_PLOT_BONUS_ADDED || kEvent.iEventId == SEVT_PLOT_BONUS_REMOVED)
					{
						const std::vector<DepositIndex::GatedDeposit>* pBonusGated =
							DepositIndex::gatedByPredicate(CASC_PRED_HAS_BONUS);
						if (pBonusGated != NULL && !pBonusGated->empty())
						{
							std::vector<CvCascadePackage<CvPlot>::PlotSegment> kSegments;
							kSegments.reserve(pBonusGated->size());
							for (size_t iSeg = 0; iSeg < pBonusGated->size(); ++iSeg)
							{
								kSegments.push_back(mc_segmentForSource((*pBonusGated)[iSeg].source));
							}
							// The value DIFFERENCE makes this idempotent against whatever plane A already booked, so
							// running it on the load path as well costs a resolve and changes nothing.
							mc_bookGatedPlot(pBonusGated, kSegments, *pPlot, NULL);
						}
					}
					// ⚑ THE COUNT INDEX IS A SECOND KEY SPACE, and the type-atom measurement above does not reach
					// it. That census counted deposits CONDITIONED on HOLDING a substrate (the atom index,
					// gatedByType); these are deposits SCALED BY HOW MANY plots carry one --
					// `per: {type: IMPROVEMENT_X, scope: city}`, which cascadeCountOf answers through
					// CityContext::improvedPlotCount. Both index by a plain string, so asking the atom index about
					// an improvement answers EMPTY and reads exactly like "nothing is keyed on this"
					// ([modifier.md] §3: the two indices are keyed alike and are NOT interchangeable).
					if (kEvent.iEventId == SEVT_PLOT_IMPROVEMENT_ADDED || kEvent.iEventId == SEVT_PLOT_IMPROVEMENT_REMOVED)
					{
						// _ADDED names what arrived and _REMOVED what left, so either way iType names the token
						// whose count just moved. The re-book resolves against the count as it stands NOW and moves
						// the DIFFERENCE -- which is what lets ONE call serve both directions, and is required
						// rather than tidy: perApply divides by `each`, so it is a STEP function and no multiple of
						// a delta reproduces a step (the POPULATION route beside this one is the same shape).
						const std::vector<DepositIndex::GatedDeposit>* pImprovementGated =
							(pSubstrate != NULL) ? DepositIndex::gatedByToken(pSubstrate->getType()) : NULL;
						if (pImprovementGated != NULL && !pImprovementGated->empty())
						{
							McGatedTally kImprovementTally;
							// The cities come from the plot's OWN workableBy list -- the same set
							// improvedPlotCount walks -- so a tile no city can work re-books nobody, and at load
							// the map streams before the players so the fan reaches nobody by construction.
							const std::vector<IDInfo>& kWorkableByCities = pPlot->workableByCities();
							for (size_t iWorkable = 0; iWorkable < kWorkableByCities.size(); ++iWorkable)
							{
								const CvCity* pWorkableCity =
									GET_PLAYER((PlayerTypes)kWorkableByCities[iWorkable].eOwner)
										.getCity(kWorkableByCities[iWorkable].iID);
								if (pWorkableCity != NULL)
								{
									mc_bookGated(pImprovementGated, *pWorkableCity, &kImprovementTally);
								}
							}
							CascadeChannelRegistry::reportAtomRoute(pSubstrate->getType(),
								(int)pImprovementGated->size(), kImprovementTally.iFound,
								kImprovementTally.iNoSource, kImprovementTally.iRefused,
								kImprovementTally.iApplied,
								(pPlayer != NULL) ? (int)pPlayer->getID() : -1, -1);
						}
					}
				}
				break;
			}
			// ---- THE PLOT'S VERDICTS ARRIVE AS ONE FACT, PER PREDICATE, WITH THE DIRECTION IN ITS ID. ----
			// `PlotContext` owns the bits and announces each 0-crossing as SEVT_PLOT_PREDICATE_ADDED / _REMOVED
			// carrying the CASC_PRED_* id, so this consumer never re-derives a verdict from a substrate fact and
			// never fans to a neighbour: a substrate swap that moves several verdicts in opposite directions
			// (grassland to hills LOSES IS_FLATLANDS and GAINS HAS_HILLS) simply emits one fact per bit, and the
			// emitter is what guarantees both are sent. That is the whole reason the pairs exist
			// ([DEC-facts-name-happenings]).
			// ⛔ So there are NO cases here for plot TYPE / river / irrigation / landmark / city / worked. They are
			// not deposit KEYS (gt_foldInfo keys targeted entries on improvement / terrain / feature / bonus /
			// route ONLY), so a CONDITIONED deposit was the only thing that could read them -- which is exactly
			// what this fact addresses, from the store that knows which bit moved.
			case SEVT_PLOT_PREDICATE_ADDED:
			case SEVT_PLOT_PREDICATE_REMOVED:
			{
				// Plane C, the ATOM route: the verdict crossed, so the deposits that atom gates are applied on the
				// plot and the city sitting on it. The sign is the fact's identity and is never read off a payload.
				const int iCrossing = (kEvent.iEventId == SEVT_PLOT_PREDICATE_ADDED) ? +1 : -1;
				mc_applyPlotPredicate((CvCascPredKind)kEvent.iType, iCrossing, mc_plot(kEvent.iSrcLoc));
				break;
			}
			// ⚖ THE WORKING-CITY FACT CARRIES THE `plots` FAN'S MEMBERSHIP LEG -- and nothing else. IS_WORKED's
			// DEPOSITS are not routed on either membership fact: IS_WORKED is a plot VERDICT, so the deposits it
			// gates re-resolve through the predicate fact above like every other bit, and routing them here as
			// well would apply the same crossing twice.
			case SEVT_PLOT_WORKING_CITY_ADDED:
			case SEVT_PLOT_WORKING_CITY_REMOVED:
			{
				// Inside the load bracket the membership facts are DECLINED, not banked: the banked-fan drain at
				// GAME_LOAD_FINISHED applies every plots-authoring source over the FINAL working set, so a fold
				// here would double with it -- the same final-state shape CityContext takes for its per-bit facts.
				if (!spineGameLoadInProgress())
				{
					const CvPlot* pWorkingPlot = mc_plot(kEvent.iSrcLoc);
					// The fact NAMES its city (iA) and owner (iC) -- on the REMOVE end the departing pair, which
					// getWorkingCity() can no longer answer.
					if (pWorkingPlot != NULL && pPlayer != NULL)
					{
						const CvCity* pMemberCity = pPlayer->getCity(kEvent.iA);
						if (pMemberCity != NULL)
						{
							mc_foldPlotsFanMembership(*pWorkingPlot, *pMemberCity, *pPlayer,
								(kEvent.iEventId == SEVT_PLOT_WORKING_CITY_ADDED) ? +1 : -1);
						}
					}
				}
				break;
			}
			// ⚖ THE WORKED FACT MOVES the city's worked-plot Σ. That Σ used to be
			// re-summed at the combine (cityReceiverRate walking getCityIndexPlot over the whole ring), which is
			// the per-read walk [state-repositories.md] bans and which hung a late-game turn inside the citizen
			// assignment. It is a MAINTAINED SLOT (CvCascadePackage::plotBaseFlat), and this is its MEMBERSHIP
			// leg; the RESOLVE leg is foldPlotSegment above. ⚠ The two together are total -- drop either and the
			// slot goes permanently short, with nothing to re-derive it ([DEC-no-self-heal]).
			case SEVT_PLOT_WORKED_ADDED:
			case SEVT_PLOT_WORKED_REMOVED:
			{
				// The fact NAMES its city (iB) and owner (iC), so the fold never depends on getWorkingCity()
				// still answering -- which on the REMOVE end it may not.
				const CvPlot* pWorkedPlot = mc_plot(kEvent.iSrcLoc);
				if (pWorkedPlot != NULL && kEvent.iC >= 0 && kEvent.iC < MAX_PLAYERS)
				{
					const CvCity* pCity = GET_PLAYER((PlayerTypes)kEvent.iC).getCity(kEvent.iB);
					if (pCity != NULL)
					{
						// The PLOT origin folds into the city's PLOT plane -- the third of the three
					// ([state-repositories.md] § THE ORIGIN RULE), typed so it cannot share a slot with the
					// specialist or building flats.
					pCity->getPlotYields().applyWorkedPlot(
							pWorkedPlot->getCascadePackage(),
							(kEvent.iEventId == SEVT_PLOT_WORKED_ADDED) ? +1 : -1);
					}
				}
				break;
			}
			case SEVT_CITY_ORDER_ADDED:     // the queue HEAD is the active process (the production->commerce conversion)
			case SEVT_CITY_ORDER_REMOVED:
			{
				// A city's active PROCESS is its head order, and the conversion it drives is a TIER-2 term of the
				// per-city commerce split -- so the mark is exactly the empire's COMMERCE receiver sums, derived
				// from the minted receiver set. The process reaches no package channel (it is live state read at
				// the combine, never a deposit) and no city receiver sum.
				// ⛔ The order TYPE cannot narrow this: pushOrder inserts at the FRONT whenever the head is
				// ORDER_MAINTAIN, so pushing an ordinary build DISPLACES a running process -- a filter on
				// ORDER_MAINTAIN would miss exactly that. The head order moving is what this fact announces, and
				// the head order IS the process, so the mark is the process's own reach, unfiltered.
				break;
			}
			// ---- empire-level source flips ----
			case SEVT_EMPIRE_TECH_ADDED:
			case SEVT_EMPIRE_TECH_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumTechInfos())
				{
					mc_applySource(&GC.getTechInfo((TechTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					mc_applyTypeAtom(GC.getTechInfo((TechTypes)kEvent.iType).getType(), EDGEB_TECHS,
						kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, NULL);
				}
				break;
			}
			case SEVT_EMPIRE_TRAIT_ADDED:
			case SEVT_EMPIRE_TRAIT_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumTraitInfos())
				{
					const CvTraitInfo* pTrait = MMKernel::traitData(kEvent.iType);   // the ACTIVE set's record
					mc_applySource(pTrait, mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					if (pTrait != NULL)
					{
						mc_applyTypeAtom(pTrait->getType(), EDGEB_TRAITS, kEvent.iType,
							mc_sourceDirection(kEvent) > 0, pPlayer, NULL);
					}
				}
				break;
			}
			case SEVT_CIVIC_ADOPTED:   // the SWAP fact: one civic arrives (iType) and the one it displaces leaves (iB)
			{
				// ⛔ THE TWO SIDES TAKE OPPOSITE SIGNS, and they are written here rather than asked of
				// mc_sourceDirection because that function answers per FACT and this one fact carries both ends.
				// Applying one direction to both would deposit the displaced civic's modifiers a second time
				// instead of withdrawing them -- a compounding phantom nothing later clears
				// ([state-repositories.md] § THE MAINTAINED SUM).
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumCivicInfos())
				{
					mc_applySource(&GC.getCivicInfo((CivicTypes)kEvent.iType), 1, kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					// PLANE C, the ARRIVING side: it is now HELD.
					mc_applyTypeAtom(GC.getCivicInfo((CivicTypes)kEvent.iType).getType(), EDGEB_CIVICS,
						kEvent.iType, true, pPlayer, NULL);
				}
				if (kEvent.iB >= 0 && kEvent.iB < GC.getNumCivicInfos())
				{
					mc_applySource(&GC.getCivicInfo((CivicTypes)kEvent.iB), -1, kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					// ...and the DISPLACED side: it is now NOT held. Same reason the source halves take opposite
					// signs -- one fact carries both ends, so the verdict is written per end, never asked of the fact.
					mc_applyTypeAtom(GC.getCivicInfo((CivicTypes)kEvent.iB).getType(), EDGEB_CIVICS,
						kEvent.iB, false, pPlayer, NULL);
				}
				break;
			}
			case SEVT_EMPIRE_PROJECT_ADDED:
			case SEVT_EMPIRE_PROJECT_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumProjectInfos())
				{
					mc_applySource(&GC.getProjectInfo((ProjectTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					mc_applyTypeAtom(GC.getProjectInfo((ProjectTypes)kEvent.iType).getType(), EDGEB_PROJECTS,
						kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, NULL);
				}
				break;
			}
			case SEVT_EMPIRE_HERITAGE_ADDED:
			case SEVT_EMPIRE_HERITAGE_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumHeritageInfos())
				{
					mc_applySource(&GC.getHeritageInfo((HeritageTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL, CASC_ORIGIN_BUILDING);
					mc_applyTypeAtom(GC.getHeritageInfo((HeritageTypes)kEvent.iType).getType(), EDGEB_HERITAGES,
						kEvent.iType, mc_sourceDirection(kEvent) > 0, pPlayer, NULL);
				}
				break;
			}
			// ---- state flips a deposit's GATE reads: the dependency routes ----
			case SEVT_CITY_POPULATION_ADDED:
			case SEVT_CITY_POPULATION_REMOVED:
			{
				// ⛔ RE-BOOKED, NEVER DELTA'D -- the ERA route beside this one is the same shape for the same
				// reason ([modifier.md] §3 names POPULATION among the thresholds).
				// ⚑ WHY a linear delta cannot serve it, which is the part that is not obvious from the fact: a
				// `per` scaler resolves through MMKernel::perApply as `value × (count / each)` -- INTEGER division,
				// so it is a STEP function. A count fact carries Δcount, and no multiple of Δ reproduces a step:
				// with `each: 100` a city growing 0 → 20 owes NOTHING, while `value × Δ` applied 2500 × 20 per
				// city and nothing ever withdrew it. Re-resolving and moving the DIFFERENCE handles both
				// directions, lands the step exactly, and is idempotent if the fact is seen twice.
				McGatedTally kPopTally;
				const std::vector<DepositIndex::GatedDeposit>* pPopGated = DepositIndex::gatedByToken("POPULATION");
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pPopGated != NULL)
				{
					// BOTH planes: the city's own per-population deposits, and the owner's -- a population move is
					// a city fact whose count an empire-scope deposit may equally scale on.
					if (pCity != NULL)
					{
						mc_bookGated(pPopGated, *pCity, &kPopTally);
					}
					if (pPlayer != NULL)
					{
						mc_bookGatedEmpire(pPopGated, *pPlayer, &kPopTally);
					}
					CascadeChannelRegistry::reportAtomRoute("POPULATION", (int)pPopGated->size(), kPopTally.iFound,
						kPopTally.iNoSource, kPopTally.iRefused, kPopTally.iApplied,
						(pPlayer != NULL) ? (int)pPlayer->getID() : -1, (pCity != NULL) ? pCity->getID() : -1);
				}
				break;
			}
			case SEVT_CITY_POWER_ADDED:
			case SEVT_CITY_POWER_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_HAS_POWER), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				break;
			}
			// A city PROPERTY crossed an authored boundary -- the HOLDER's crossing fact, announced beside the raw
			// value fact precisely so a consumer never gates on the value firehose ([event-spine.md]: the solver
			// moves nearly every property of every city every turn; the boundaries are ONE registry and are tested
			// once, at the emit). The deposits a `{PROPERTY_X, min/max}` gate governs re-book HERE.
			// ⚑ Re-BOOKED, never crossed: a property gate is a THRESHOLD, so it has no held/not-held verdict to pin
			// (the POPULATION / ERA shape), and the fact is deliberately DIRECTION-LESS in effect -- the re-book
			// reads the live value against each gate, so which way the boundary was crossed is redundant.
			// ⛔ The registry the emit tests carries the DEPOSIT-declared boundaries too
			// (DepositIndex::propertyGateThresholds, unioned into EnablerKernel::propertyBandThresholds) -- without
			// that half a gate at a value between two operate-band boundaries would never see its crossing fire.
			// ⛔ NO case for SEVT_PROPERTY_ADDED / _REMOVED itself, and that is the DESIGN, not a hole: the raw
			// value fact is the highest-volume mutation in the engine, and the holder already reduced it to the
			// crossings a gate can act on. A `per: {PROPERTY_X}` SCALED deposit would need the value fact's delta,
			// and no modifier deposit authors one -- the property-scaled `per` lives on the trigger plane's chance
			// ([json.md] §5), which is not this consumer's.
			case SEVT_CITY_PROPERTY_BAND_ADDED:
			case SEVT_CITY_PROPERTY_BAND_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pCity != NULL && kEvent.iType >= 0 && kEvent.iType < GC.getNumPropertyInfos())
				{
					const char* szPropertyType = GC.getPropertyInfo((PropertyTypes)kEvent.iType).getType();
					const std::vector<DepositIndex::GatedDeposit>* pPropertyGated =
						(szPropertyType != NULL) ? DepositIndex::gatedByType(std::string(szPropertyType)) : NULL;
					if (pPropertyGated != NULL)
					{
						McGatedTally kPropertyTally;
						mc_bookGated(pPropertyGated, *pCity, &kPropertyTally);
						CascadeChannelRegistry::reportAtomRoute(szPropertyType, (int)pPropertyGated->size(),
							kPropertyTally.iFound, kPropertyTally.iNoSource, kPropertyTally.iRefused,
							kPropertyTally.iApplied, (int)pCity->getOwner(), pCity->getID());
					}
				}
				break;
			}
			// ⛔ NO case for the CITY fresh-water counter. `CvPlot::isFreshWater` reads
			// `getPlotCity()->hasFreshWater()`, so the counter crossing moves the CENTRE PLOT's own bit --
			// PlotContext consumes that city fact for exactly this reason (pc_cityAxisFor) and announces the
			// crossing, and the predicate route above then binds the plot AND the city standing on it, which is
			// the same reach this case had. Applying it here as well would double the crossing.
			case SEVT_EMPIRE_GOLDEN_AGE_ADDED:
			case SEVT_EMPIRE_GOLDEN_AGE_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_GOLDEN_AGE), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				// the golden-age member-mirror (the ledgered engine carve-out) applies at the receiver
				// combine -- the flip re-realizes every rate of the player
				break;
			}
			// The rebel maintenance discount's ONLY route. Its source (TECH_GAME_START) is held from turn one and
			// never moves again, so plane A can never re-resolve it -- the atom's crossing is the whole mechanism.
			case SEVT_EMPIRE_REBEL_ADDED:
			case SEVT_EMPIRE_REBEL_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_REBEL), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				break;
			}
			case SEVT_EMPIRE_STATE_RELIGION_ADDED:
			case SEVT_EMPIRE_STATE_RELIGION_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_HAS_STATE_RELIGION), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_STATE_RELIGION_IN_CITY), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_STATE_RELIGION), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_STATE_RELIGION), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByReligionCounts(), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				break;
			}
			case SEVT_CITY_HOLY_CITY_ADDED:
			case SEVT_CITY_HOLY_CITY_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_HOLY_CITY), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_STATE_RELIGION_HOLY_CITY), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				break;
			}
			case SEVT_CITY_HEADQUARTERS_ADDED:
			case SEVT_CITY_HEADQUARTERS_REMOVED:
			{
				// The designation IS the verdict, so this is a clean plane-C crossing: setHeadquarters emits
				// REMOVED for the losing city and ADDED for the gaining one, and a relocation is those two facts
				// rather than a move anything has to model.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_HEADQUARTERS), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				break;
			}
			case SEVT_CITY_CORPORATION_ACTIVE_ADDED:
			case SEVT_CITY_CORPORATION_ACTIVE_REMOVED:
			{
				// The {HAS_CORPORATION} verdict crossing -- announced by CityContext's verdict store, which
				// re-reads the four-leg engine verdict on each leg's fact. ⛔ The corporation PRESENCE pair is one
				// leg of that verdict and must not route these deposits: a present-but-dormant corporation is
				// exactly the case that separates the two facts.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_HAS_CORPORATION), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				break;
			}
			case SEVT_EMPIRE_ERA_ADDED:
			case SEVT_EMPIRE_ERA_REMOVED:
			{
				// ⛔ THE ERA ROUTE WAS ASKING THE WRONG INDEX AND MOVED NOTHING AT ALL.
				// It read ONLY gatedByToken("ERA") -- the `per`-SCALER index (di_scanRecordDependencies) -- while
				// an era CONDITION is `{type: "ERA", min/max: N}`, whose node type interns into s_gatedBy**Type**.
				// So the list this asked for was empty on every era advance, and the 1130 era-gated deposits in
				// Assets/Data (all empire-scope: research 565, culture 565) kept whatever verdict they were booked
				// at when their source arrived -- for the rest of the game.
				// ⚑ It reads as wired, which is why it survived: a token and a type are the same string here, and an
				// empty list is indistinguishable from a list with nothing to do ([DEC-no-guessing] -- the tally is
				// what tells them apart).
				// ⚑ Re-BOOKED, not crossed: a threshold has no held/not-held verdict to pin (see mc_bookGatedEmpire).
				McGatedTally kEraTally;
				const std::vector<DepositIndex::GatedDeposit>* pEraGated = DepositIndex::gatedByType("ERA");
				if (pPlayer != NULL && pEraGated != NULL)
				{
					mc_bookGatedEmpire(pEraGated, *pPlayer, &kEraTally);
					// ⚠ The LIST SIZE is the real one, not 0: reportAtomRoute returns early on `iListSize <= 0`
					// ("nothing is conditioned on this atom -- silence is the honest report"), so passing a
					// placeholder makes the route report NOTHING and look exactly like a route that does not exist.
					CascadeChannelRegistry::reportAtomRoute("ERA", (int)pEraGated->size(), kEraTally.iFound,
						kEraTally.iNoSource, kEraTally.iRefused, kEraTally.iApplied, (int)pPlayer->getID(), -1);
				}
				// The `per: {type: ERA}` scaler route beside it -- the handicap AI percents scale on it
				// (`{value: -3, per: "ERA"}`), and an era advance is Δ1, so applying the per-unit value once per
				// crossing IS the exact delta.
				mc_applyGated(DepositIndex::gatedByToken("ERA"), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, NULL, NULL);
				break;
			}
			// A commerce SLIDER moved -- the COUNT route for the §3.1 rate tokens ("happiness per 10% culture
			// rate" / "anger per gold rate"). ⚠ ONE slider move emits SEVERAL of these, one per channel the
			// setter rebalanced ([event-spine.md]), and each names its own channel -- so the route maps the
			// channel to ITS token and never widens to the whole family.
			// ⚑ Re-BOOKED, never delta'd: `each: 100` makes the scaler a STEP function (the POPULATION shape), so
			// the re-book resolves against the rate as it stands and moves only the difference -- idempotent when
			// the same fact is seen twice, and correct in both directions off a direction-less pass.
			// Both planes: the civic's empire-scope deposit re-books at the player, the building's city-scope one
			// per city (a slider is PLAYER state, so every city of this owner reads the same new rate).
			case SEVT_EMPIRE_COMMERCE_PERCENT_ADDED:
			case SEVT_EMPIRE_COMMERCE_PERCENT_REMOVED:
			{
				const char* szRateToken = NULL;
				switch (kEvent.iType)
				{
				case COMMERCE_GOLD:      szRateToken = "GOLD_RATE"; break;
				case COMMERCE_RESEARCH:  szRateToken = "RESEARCH_RATE"; break;
				case COMMERCE_CULTURE:   szRateToken = "CULTURE_RATE"; break;
				case COMMERCE_ESPIONAGE: szRateToken = "ESPIONAGE_RATE"; break;
				default: break;
				}
				const std::vector<DepositIndex::GatedDeposit>* pRateGated =
					(szRateToken != NULL) ? DepositIndex::gatedByToken(szRateToken) : NULL;
				if (pPlayer != NULL && pRateGated != NULL)
				{
					McGatedTally kRateTally;
					mc_bookGatedEmpire(pRateGated, *pPlayer, &kRateTally);
					for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities();
						cityIterator != pPlayer->endCities(); ++cityIterator)
					{
						if (*cityIterator != NULL)
						{
							mc_bookGated(pRateGated, **cityIterator, &kRateTally);
						}
					}
					CascadeChannelRegistry::reportAtomRoute(szRateToken, (int)pRateGated->size(),
						kRateTally.iFound, kRateTally.iNoSource, kRateTally.iRefused, kRateTally.iApplied,
						(int)pPlayer->getID(), -1);
				}
				break;
			}
			// ⚠ The WORLD ban, not the empire's own nuke capability: CASC_PRED_NO_NUKES is the world verdict
			// ([json.md] §3.5), so the ban ARRIVING is the predicate becoming true.
			case SEVT_WORLD_NUKES_BANNED_ADDED:
			case SEVT_WORLD_NUKES_BANNED_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_NO_NUKES), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				break;
			}
			case SEVT_EMPIRE_CAPITAL_ADDED:
			case SEVT_EMPIRE_CAPITAL_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_CAPITAL), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByPredicate(CASC_PRED_IS_GOVERNMENT_CENTER), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, NULL, NULL);
				break;
			}
			// ⚖ NOTHING FOR THE MODIFIER, AND THAT IS A DEDUPLICATION RATHER THAN A HOLE.
			// CvPlayer::found emits SEVT_CITY_OWNER_ADDED *before* this fact ("OWNERSHIP first, then the founding"),
			// and CvCity::readBody does the same on a load -- so the owner-source fold and the CITY-count delta both
			// already ran there, on the ONE fact that covers founding, conquest and the reseed alike. Doing either
			// again here would be the duplicate the spine bans ([event-spine.md]: one happening, one application).
			case SEVT_CITY_FOUNDED:
				break;
			// ---- ownership moves: the pair names ONE owner each, so each end is its own work ----
			// ⛔ THE PAIR IS NOT ONE FACT WITH TWO OWNERS, and reading it as one is what left this dead. Both
			// emitters pass only (city, owner) -- emitCityOwnerAdded / _REMOVED set iA = 0 and iB = 0 -- so the old
			// handler's mc_player(kEvent.iA) resolved to PLAYER 0 on every transfer, and its CITY-count route ran
			// with a delta of iB == 0, i.e. never ran at all. The direction is in the fact's IDENTITY
			// ([event-spine.md] § A FACT NAMES THE HAPPENING); reading an owner out of the payload was the
			// discriminator that rule bans, and here it was not even populated.
			case SEVT_CITY_OWNER_ADDED:
			{
				// The city now belongs to this player, so it folds what that player already holds -- the SAME second
				// leg a founded city takes, for the same reason (the owner's sources arrived before this city was
				// this owner's). ⚑ A conquered city is a NEW CvCity object with a freshly-zeroed package, so there
				// is nothing of the previous owner's left in it to withdraw -- the fold builds it from empty.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pCity != NULL && pPlayer != NULL)
				{
					mc_foldOwnerSourcesIntoCity(*pPlayer, *pCity, szSource);
				}
				// Plane B: this empire's CITY count gained one.
				mc_applyGated(DepositIndex::gatedByToken("CITY"), +1, MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, NULL, NULL);
				break;
			}
			case SEVT_CITY_OWNER_REMOVED:
			{
				// Plane B only: this empire's CITY count lost one. The city object itself is leaving this owner, and
				// its package goes with it -- there is no slot of this player's to withdraw from here.
				mc_applyGated(DepositIndex::gatedByToken("CITY"), -1, MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, NULL, NULL);
				break;
			}
			// ⚖ NOTHING FOR THE MODIFIER, AND THAT IS THE ONE-FACT RULE RATHER THAN A HOLE. CvPlot::setOwner
			// CALLS updateWorkingCity ([contexts.md]: a city cannot work a plot it does not own, so the two
			// facts cannot come apart), and every modifier consequence of an ownership flip rides those facts:
			// the WORKING-CITY pair moves the plots-fan deposits of the departing and arriving owners, and the
			// WORKED pair moves the city's worked-plot Σ. Routing any of it here as well would apply one
			// happening twice. The IS_OWNED verdict is PlotContext's -- it derives the bit off this fact and
			// announces the crossing, and the predicate route serves the deposits gated on it. Cross-scope
			// receiver totals store nothing ([DEC-uniform-cache-shape]), so there is no empire-side slot to
			// move for either owner.
			case SEVT_PLOT_OWNER_ADDED:
			case SEVT_PLOT_OWNER_REMOVED:
				break;
			// ⚖ THE TURN IS A FACT LIKE ANY OTHER, AND IT CARRIES WHAT THE AGE GATE NEEDS (owner).
			// `existedFor` is the one condition class whose dependency is ELAPSED TIME: no source moves, no count
			// moves, no atom crosses -- the deposit simply becomes due. Nothing else in the engine can announce
			// that, so the turn boundary is its fact, and the turn number is the whole of the input.
			// ⚑ It satisfies the SANCTIONED event-triggered recalc test ([contexts.md]): a genuine DOMAIN fact
			// triggers it, the consequence is NON-LOCAL (the fact cannot name which builds crossed their year),
			// and no finer route exists to derive because the quantity is a function of time rather than of any
			// announced state. ⛔ It is NOT the banned per-turn blanket: the worklist is exactly the deposits the
			// `existedFor` reverse index names, never a sweep of the package.
			// ⚠ The PLAYER-scoped boundary is the grain (the game-scope pair carries iC == -1 and is skipped): a
			// player's own turn is where its cities' work already happens ([triggers.md]).
			// ⚑ Re-BOOKED rather than crossed, for the same reason ERA is: an age gate has no held/not-held
			// verdict to pin, and the value difference makes a turn that changes nothing cost nothing.
			case SEVT_TURN_STARTED:
			{
				const std::vector<DepositIndex::GatedDeposit>* pAgeGated =
					DepositIndex::gatedByPredicate(CASC_PRED_EXISTED_FOR);
				if (kEvent.iC >= 0 && pPlayer != NULL && pAgeGated != NULL && !pAgeGated->empty())
				{
					McGatedTally kAgeTally;
					for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities();
						cityIterator != pPlayer->endCities(); ++cityIterator)
					{
						if (*cityIterator != NULL)
						{
							mc_bookGated(pAgeGated, **cityIterator, &kAgeTally);
						}
					}
					// Reported only when it MOVED something: a turn line per player per turn would drown the
					// domain, while a turn on which an age gate actually came due is worth seeing.
					if (kAgeTally.iApplied > 0)
					{
						CascadeChannelRegistry::reportAtomRoute("existedFor", (int)pAgeGated->size(),
							kAgeTally.iFound, kAgeTally.iNoSource, kAgeTally.iRefused, kAgeTally.iApplied,
							(int)pPlayer->getID(), -1);
					}
				}
				break;
			}
			// ---- the turn boundary (game scope only): the channel-set census's new-game fallback, for the run
			// ---- that never passes through a load. NOT a self-heal site -- nothing is marked here
			// ---- ([DEC-no-self-heal]). ----
			case SEVT_TURN_ENDED:
			{
				if (kEvent.iC == -1)
				{
					CascadeChannelRegistry::reportChannelCensus();   // once-guarded, so the load path wins when there is one
				}
				break;
			}
			// ---- the load bracket end: DRAIN the banked marks, then the channel-set census ----
			case SEVT_GAME_LOAD_FINISHED:
			{
				mc_drainBankedPlotsFans();
				// The plot yield-THRESHOLD load build: every plot resolved while the map streamed, which is before
				// its owner held a single trait, so the step was computed against an empty player. Re-resolve now,
				// against final state, for every alive owner.
				mc_markAllThresholdOwners();
				mc_drainBankedAtomCrossings();
				mc_drainBankedAtomFanPlots();
				mc_reportGrowthCensus();
				CascadeChannelRegistry::reportChannelCensus();
				// The per-SOURCE decomposition of what the reseed actually applied -- who deposited into each
				// channel, how many times, and on which fact. This is the attribution a package total cannot give.
				CascadeChannelRegistry::reportDepositCensus();
				break;
			}
			default:
				break;   // events with no deposit reach (name changes, counts, unit lifecycle, load bracket)
			}

			// ONE targeted rebuild per FACT, for the owners this event actually moved a tradeRoutes channel for.
			// The engine owns the route network, so its output has to be re-derived rather than delta'd -- but only
			// where the fact reached, never as a sweep (owner: "it needs to run targeted, for where an event has hit").
			mc_flushTradeRouteUpdates();
			// ...and the plots whose RESOLVE operand moved (a trait changing an owner's yield threshold).
			mc_flushThresholdResolves();
		}

	private:
		// A whole PLAYER's deposit basis moved -- every package it owns re-derives (its cities' channels + sums and
		// its own empire channels + sums). The two callers are the facts that move a source felt at EVERY scope
		// rather than at a deposit-addressed one:
		//   - a DIFFICULTY change: the handicap's own modifier families feed this player's packages, so
		//     flexible difficulty moving the handicap moves that whole basis;
		//   - a GAME OPTION flip, per player (below).
		// ⚠ This IS a whole-scope blanket, and it is the SANCTIONED kind: the fact is not deposit-addressed, so no
		// union of per-source routes can express it -- exactly the SEVT_AREAS_RECALCULATED shape. It is NOT the
		// banned self-heal, which papers over a MISSED invalidation ([DEC-no-self-heal]); this ANNOUNCES a genuine
		// wholesale one. Both callers are also vanishingly rare (a WB toggle, a flexible-difficulty step), so the
		// "emit liberally, mark precisely" cost argument does not bite: there is nothing finer to derive.
		static void mc_markPlayerWhole(const CvPlayer* pPlayer, const char* szSource)
		{
			if (pPlayer == NULL)
			{
				return;
			}
			for (CvPlayer::city_iterator it = pPlayer->beginCities(); it != pPlayer->endCities(); ++it)
			{
				// â HOLE (city lifecycle / ownership): the blanket that stood here is gone -- a blanket recompute does not exist
				// under the maintained sum and is never to be built. What this needs instead is a WITHDRAW AND
				// REAPPLY over every source this city holds and every above-city
				// source of its owner, which is the same plane-A
				// walk run twice with opposite signs. Deliberately left unmaintained rather than swept.
			}
		}

	};

	CvModifierConsumer s_modifierConsumer;
}

void modifierRegisterConsumer()
{
	static bool s_bRegistered = false;
	if (s_bRegistered)
	{
		return;
	}
	s_bRegistered = true;
	eventSpine().registerConsumer(&s_modifierConsumer);
}
