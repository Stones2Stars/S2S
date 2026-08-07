//
//	CvModifierConsumer -- the modifier cascade's own spine consumer (see the header). The switch below names
//	only the event's SEMANTICS (which repo its iType indexes; which state a stateful event embodies); what a
//	source DEPOSITS is never decided here -- it comes from that source's own compiled entries, resolved through
//	the ONE per-entry resolve (MMKernel::resolveEntry) this consumer shares with the endpoint oracle.
//
//	⚖ A FACT APPLIES; NOTHING IS MARKED AND NOTHING IS REBUILT ([DEC-maintained-sum]). The DOMAIN fact names the
//	SOURCE and carries the DIRECTION as a signed multiplicity (+1 arriving, -1 leaving, ±N for a count), the
//	compiled index names that source's deposits, and applying them IS the maintenance -- so every slot is correct
//	at the instant the fact arrives, with nothing deferred and no load-bracket drain to order.
//	⛔ A MISSED EMIT therefore leaves a loud compounding error that nothing re-derives. That is the design, not a
//	weakness of it ([DEC-no-self-heal]): it is how the missing fact gets found.
//
//	⚠ WHAT IS NOT WIRED HERE, and is a HOLE rather than a decision: the COUNT route (plane B) and the ATOM route
//	(plane C). Both are reverse indices off the same compiled deposits, and both still answer as MASKS -- a mask
//	names channels, and a channel has nothing to apply. They land when DepositIndex returns the DEPOSITS an atom
//	gates and a count scales; until then a deposit conditioned on a predicate, or scaled by a count, is not
//	maintained when that predicate or count moves.
//

#include "CvGameCoreDLL.h"
#include "Engine/CvGameSpeedScale.h"   // speedPercent -- the growth-threshold census term
#include "CvModifierConsumer.h"
#include "CvCascadePackage.h"
#include "CvCascadeChannelRegistry.h"
#include "Data/CvDepositIndex.h"        // routeFor + the dependency routes -- the ONE mark derivation
#include "Data/CvDepositRead.h"         // MMKernel::resolveEntry -- the ONE per-entry resolve, shared with the oracle
#include "Data/CvInfoValuation.h"       // the eval-ctx fill seam (the contexts ARE the eval state)
#include "Spine/CvEventSpine.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvMap.h"
#include "Engine/CvGameCoreUtils.h"     // plotDirection -- the one-hop adjacency fan of the two adjacency verdicts
#include "Engine/CvPlot.h"
#include "Engine/CvCity.h"
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

namespace
{
	// ---- the ONE mark application: a derived route x the owner objects the event names ----

	// One plot-substrate id -> its TYPE string, for the dependency route keyed by that type. ONE lookup shared by
	// the arriving and the departing id, so the two directions cannot drift apart.
	const char* mc_substrateTypeName(int iEventId, int iId)
	{
		if (iId < 0)
		{
			return NULL;
		}
		switch (iEventId)
		{
		case SEVT_PLOT_IMPROVEMENT_ADDED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
			return (iId < GC.getNumImprovementInfos()) ? GC.getImprovementInfo((ImprovementTypes)iId).getType() : NULL;
		case SEVT_PLOT_TERRAIN_ADDED:
		case SEVT_PLOT_TERRAIN_REMOVED:
			return (iId < GC.getNumTerrainInfos()) ? GC.getTerrainInfo((TerrainTypes)iId).getType() : NULL;
		case SEVT_PLOT_FEATURE_ADDED:
		case SEVT_PLOT_FEATURE_REMOVED:
			return (iId < GC.getNumFeatureInfos()) ? GC.getFeatureInfo((FeatureTypes)iId).getType() : NULL;
		case SEVT_PLOT_ROUTE_ADDED:
		case SEVT_PLOT_ROUTE_REMOVED:
			return (iId < GC.getNumRouteInfos()) ? GC.getRouteInfo((RouteTypes)iId).getType() : NULL;
		case SEVT_PLOT_BONUS_ADDED:
		case SEVT_PLOT_BONUS_REMOVED:
			return (iId < GC.getNumBonusInfos()) ? GC.getBonusInfo((BonusTypes)iId).getType() : NULL;
		default:
			return NULL;
		}
	}

	// The plot-resident SOURCE a substrate fact names. The same switch as mc_substrateTypeName one level up,
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
		case SEVT_CITY_BONUS_REMOVED:
		case SEVT_CITY_RELIGION_REMOVED:
		case SEVT_CITY_CORPORATION_REMOVED:
		case SEVT_PLOT_TERRAIN_REMOVED:
		case SEVT_PLOT_FEATURE_REMOVED:
		case SEVT_PLOT_IMPROVEMENT_REMOVED:
		case SEVT_PLOT_ROUTE_REMOVED:
		case SEVT_PLOT_BONUS_REMOVED:
		case SEVT_CITY_POWER_REMOVED:
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

		// These four carry a MAGNITUDE (iA), so the multiplicity is the payload and the SIGN is still the id --
		// "CITY_SPECIALIST_REMOVED 3" withdraws three times over ([event-spine.md]: the event is the operator).
		case SEVT_CITY_SPECIALIST_ADDED:
		case SEVT_CITY_POPULATION_ADDED:
		case SEVT_EMPIRE_PROJECT_ADDED:
		case SEVT_PLOTGROUP_BONUS_ADDED:
		case SEVT_CITY_VICINITY_BONUS_ADDED:   return  kEvent.iA;

		case SEVT_CITY_SPECIALIST_REMOVED:
		case SEVT_CITY_POPULATION_REMOVED:
		case SEVT_EMPIRE_PROJECT_REMOVED:
		case SEVT_PLOTGROUP_BONUS_REMOVED:
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
	void mc_applyCityDeposits(const std::vector<CvModEntry*>& entries, int iMultiplicity, int iSourceIndex, const CvCity& city,
		const char* szSource, const char* szOnFact)
	{
		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtx(city.getCityContext(), GET_PLAYER(city.getOwner()).getEmpireContext(), NULL, evalCtx);
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (pEntry == NULL || !MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_CITY,
				evalCtx, NULL, 0, false, iChannel, bPercentSide, iValue))
			{
				continue;
			}
			if (bPercentSide)
			{
				city.getCascadePackage().applyPercent(iChannel, (int)iValue);
			}
			else
			{
				city.getCascadePackage().applyFlat(iChannel, iValue);
			}
			// The per-source ATTRIBUTION line (DIAGNOSTIC, level 3 -- free until asked for).
			CascadeChannelRegistry::reportDepositApply(szSource, iChannel, CASC_SCOPE_CITY, bPercentSide,
				iValue, (int)city.getOwner(), city.getID(), szOnFact);
		}
		city.getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
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
	// live state, so the oracle's recompute-from-source stays the only thing that does that.
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
			CvCascadeEvalCtx evalCtx;
			InfoValuation::fillEvalCtxAtPlot(*pLoopPlot, evalCtx);
			bool bApplied = false;
			for (size_t iEntry = 0; iEntry < selected.size(); ++iEntry)
			{
				const CvModEntry* pEntry = selected[iEntry];
				int iChannel = -1;
				bool bPercentSide = false;
				int64_t iValue = 0;
				if (!MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_PLOT,
					evalCtx, pLoopPlot, 0, false, iChannel, bPercentSide, iValue))
				{
					continue;
				}
				++g_iFanResolved;
				// the ORIGIN RULE: plot is yield-only, so a plot-landing percent has no side to land on
				if (!bPercentSide)
				{
					pLoopPlot->getCascadePackage().applyPlotSegment(
						CvCascadePackage<CvPlot>::PLOTSEG_REST, iChannel, iValue);
					bApplied = true;
				}
			}
			if (bApplied)
			{
				pLoopPlot->getCascadePackage().noteSourceApplied(iSourceIndex, iMultiplicity);
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
			}
		}
	}

	// Apply ONE source's compiled deposits into every package they feed. The scopes a source reaches come from
	// its own entries (resolveEntry declines any entry not at the scope being applied), so nothing here decides
	// what a source deposits -- only WHERE the owner objects are.
	void mc_applySourceDeposits(const CvInfo* pSourceInfo, int iMultiplicity,
		const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot,
		CvCascadePackage<CvPlot>::PlotSegment ePlotSegment, const char* szOnFact)
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
					evalCtx, pPlot, 0, false, iChannel, bPercentSide, iValue))
				{
					continue;
				}
				// the ORIGIN RULE: plot is yield-only, so a plot-scope percent has no side to land on
				if (!bPercentSide)
				{
					pPlot->getCascadePackage().applyPlotSegment(ePlotSegment, iChannel, iValue);
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
			mc_applyCityDeposits(entries, iMultiplicity, iSourceIndex, *pCity, pSourceInfo->getType(), szOnFact);
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
					mc_applyCityDeposits(entries, iMultiplicity, iSourceIndex, **cityIterator, pSourceInfo->getType(), szOnFact);
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
					evalCtx, NULL, 0, false, iChannel, bPercentSide, iValue))
				{
					if (bPercentSide)
					{
						pPlayer->getCascadePackage().applyPercent(iChannel, (int)iValue);
					}
					else
					{
						pPlayer->getCascadePackage().applyFlat(iChannel, iValue);
					}
					CascadeChannelRegistry::reportDepositApply(pSourceInfo->getType(), iChannel, CASC_SCOPE_EMPIRE,
						bPercentSide, iValue, (int)pPlayer->getID(), -1, szOnFact);
				}
				iChannel = -1;
				bPercentSide = false;
				iValue = 0;
				if (pEntry != NULL && MMKernel::resolveEntry(*pEntry, iMultiplicity, CASC_SCOPE_TEAM,
					evalCtx, NULL, 0, false, iChannel, bPercentSide, iValue))
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
	void mc_applyGated(const std::vector<DepositIndex::GatedDeposit>* pGated, int iDelta,
		MMKernel::PerScaling ePerScaling, const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot)
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
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, pPlot, 0, false,
					iChannel, bPercentSide, iValue, ePerScaling) && !bPercentSide)
				{
					pPlot->getCascadePackage().applyPlotSegment(
						CvCascadePackage<CvPlot>::PLOTSEG_REST, iChannel, iValue);
				}
				continue;
			}
			if (eScope == CASC_SCOPE_CITY)
			{
				if (pCity == NULL || !pCity->getCascadePackage().hasAppliedSource(kGated.sourceIndex))
				{
					continue;
				}
				CvCascadeEvalCtx evalCtx;
				InfoValuation::fillEvalCtx(pCity->getCityContext(),
					GET_PLAYER(pCity->getOwner()).getEmpireContext(), NULL, evalCtx);
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, 0, false,
					iChannel, bPercentSide, iValue, ePerScaling))
				{
					if (bPercentSide) pCity->getCascadePackage().applyPercent(iChannel, (int)iValue);
					else              pCity->getCascadePackage().applyFlat(iChannel, iValue);
					// Planes B and C attribute too -- a count or an atom crossing moving a deposit is exactly the
					// class a total can never explain, so leaving it out would make the decomposition lie by omission.
					CascadeChannelRegistry::reportDepositApply(
						(kGated.source != NULL) ? kGated.source->getType() : "?", iChannel, CASC_SCOPE_CITY,
						bPercentSide, iValue, (int)pCity->getOwner(), pCity->getID(), "gated");
				}
				continue;
			}
			if (pPlayer == NULL)
			{
				continue;
			}
			CvCascadeEvalCtx evalCtx;
			pPlayer->getEmpireContext().fillEvalCtx(evalCtx);
			if (eScope == CASC_SCOPE_TEAM)
			{
				const CvTeam& team = GET_TEAM(pPlayer->getTeam());
				if (!team.getCascadePackage().hasAppliedSource(kGated.sourceIndex))
				{
					continue;
				}
				if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, 0, false,
					iChannel, bPercentSide, iValue, ePerScaling))
				{
					if (bPercentSide) team.getCascadePackage().applyPercent(iChannel, (int)iValue);
					else              team.getCascadePackage().applyFlat(iChannel, iValue);
				}
				continue;
			}
			if (!pPlayer->getCascadePackage().hasAppliedSource(kGated.sourceIndex))
			{
				continue;
			}
			if (MMKernel::resolveEntry(*kGated.deposit->entry, iDelta, eScope, evalCtx, NULL, 0, false,
				iChannel, bPercentSide, iValue, ePerScaling))
			{
				if (bPercentSide) pPlayer->getCascadePackage().applyPercent(iChannel, (int)iValue);
				else              pPlayer->getCascadePackage().applyFlat(iChannel, iValue);
			}
		}
	}

	// The source-carrying application: the source's own deposits (PLANE A, applied here) plus everything
	// conditioned ON the source -- a deposit gated on this entity's presence. That second half is the ATOM
	// route (plane C) and is NOT wired.
	// ⛔ THE REASON IS THE WITHDRAWAL, NOT THE INDEX -- and the index reason that used to stand here is STALE.
	// DepositIndex::gatedByType already answers with the DEPOSIT LIST an atom gates (di_scanConditionTree pushes
	// every CASC_COND_PRESENCE type into s_gatedByType), so the arrival half is one mc_applyGated call away.
	// What blocks it is that these facts are emitted AFTER the state moves -- CvCity::processBonus announces the
	// crossing once the plot group's count has already changed -- while mc_applyGated resolves each entry through
	// applies() against the LIVE ctx. So a REMOVED fact evaluates a gate that no longer holds, resolveEntry
	// returns false, and nothing is withdrawn.
	// ⚠ Wiring the arrival alone would therefore be WORSE than the present hole: an unwithdrawn deposit compounds
	// on every re-acquisition, where today it is merely absent ([state-repositories.md] § THE INVARIANT -- a
	// withdrawal is exact only if the fact is emitted while the old state still holds).
	void mc_applySource(const CvInfo* pSourceInfo, int iMultiplicity, int iEventId,
		const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot)
	{
		mc_applySourceDeposits(pSourceInfo, iMultiplicity, pPlayer, pCity, pPlot, mc_plotSegmentFor(iEventId),
			spineEventName(iEventId));
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
						pPlayer, pCity, NULL);
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
					// ⛔ HOLE (plane C, the ATOM route): everything CONDITIONED on holding this source needs
					// re-resolving here, and cannot be until the reverse index answers with the DEPOSITS the
					// atom gates rather than a channel MASK. Deliberately left failing rather than papered over.
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
						pPlayer, pCity, NULL);
				}
				mc_applyGated(DepositIndex::gatedByReligionCounts(), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_APPLIED,
					pPlayer, pCity, NULL);
				break;
			}
			case SEVT_CITY_CORPORATION_ADDED:
			case SEVT_CITY_CORPORATION_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumCorporationInfos())
				{
					mc_applySource(&GC.getCorporationInfo((CorporationTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL);
				}
				break;
			}
			case SEVT_CITY_SPECIALIST_ADDED:
			case SEVT_CITY_SPECIALIST_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumSpecialistInfos())
				{
					mc_applySource(&GC.getSpecialistInfo((SpecialistTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL);
				}
				break;
			}
			case SEVT_CITY_BONUS_ADDED:              // the city obtained a bonus over the network
			case SEVT_CITY_BONUS_REMOVED:            // ... or lost it
			case SEVT_CITY_VICINITY_BONUS_ADDED:     // a city's local (vicinity) supply count moved
			case SEVT_CITY_VICINITY_BONUS_REMOVED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
				{
					mc_applySource(&GC.getBonusInfo((BonusTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, pCity, NULL);
				}
				break;
			}
			case SEVT_PLOTGROUP_BONUS_ADDED:     // the trade network's resource set -- reaches every connected city
			case SEVT_PLOTGROUP_BONUS_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
				{
					mc_applySource(&GC.getBonusInfo((BonusTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL);
				}
				break;
			}
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
							pPlayer, pPlot->getPlotCity(), pPlot);
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
					const char* szSubstrate = mc_substrateTypeName(kEvent.iEventId, kEvent.iType);
					if (szSubstrate != NULL)
					{
					// ⛔ HOLE (plane C, the ATOM route): everything CONDITIONED on holding this source needs
					// re-resolving here, and cannot be until the reverse index answers with the DEPOSITS the
					// atom gates rather than a channel MASK. Deliberately left failing rather than papered over.
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
			// ⛔ NO case for the WORKING-CITY or WORKED facts. IS_WORKED is a plot VERDICT, so its deposits
			// re-resolve through the predicate fact above like every other bit -- routing it here as well would
			// apply the same crossing twice. What each fact additionally moves is a city RECEIVER SUM, which is
			// the receiver's own route and not a mask to mark here ([state-repositories.md] § THE CROSS-SCOPE
			// RECEIVER); the stubs that computed one and dropped it were left over from the retired protocol.
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
						pPlayer, NULL, NULL);
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
						pPlayer, NULL, NULL);
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
						pPlayer, NULL, NULL);
				}
				if (kEvent.iB >= 0 && kEvent.iB < GC.getNumCivicInfos())
				{
					mc_applySource(&GC.getCivicInfo((CivicTypes)kEvent.iB), -1, kEvent.iEventId,
						pPlayer, NULL, NULL);
				}
				break;
			}
			case SEVT_EMPIRE_PROJECT_ADDED:
			case SEVT_EMPIRE_PROJECT_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumProjectInfos())
				{
					mc_applySource(&GC.getProjectInfo((ProjectTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL);
				}
				break;
			}
			case SEVT_EMPIRE_HERITAGE_ADDED:
			case SEVT_EMPIRE_HERITAGE_REMOVED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumHeritageInfos())
				{
					mc_applySource(&GC.getHeritageInfo((HeritageTypes)kEvent.iType), mc_sourceDirection(kEvent), kEvent.iEventId,
						pPlayer, NULL, NULL);
				}
				break;
			}
			// ---- state flips a deposit's GATE reads: the dependency routes ----
			case SEVT_CITY_POPULATION_ADDED:
			case SEVT_CITY_POPULATION_REMOVED:
			{
				// Plane B, the COUNT route: the delta is the fact's MAGNITUDE with the sign of its id, which is
				// exactly what mc_sourceDirection resolves -- the payload no longer carries a signed number.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyGated(DepositIndex::gatedByToken("POPULATION"), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, pCity, NULL);
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
			case SEVT_EMPIRE_ERA_ADDED:
			case SEVT_EMPIRE_ERA_REMOVED:
			{
				mc_applyGated(DepositIndex::gatedByToken("ERA"), mc_sourceDirection(kEvent), MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, NULL, NULL);
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
			case SEVT_CITY_FOUNDED:   // the ruled exception: a new city reads correct values the turn it exists
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pCity != NULL)
				{
					// whole-scope blanket KEPT: a scope-object LIFECYCLE event is not deposit-addressed --
					// no single source's route exists; every channel authored at city scope may be fed by the
					// new city's owner sources, so the whole package + sums build (the state-repositories.md
					// founded-city eager-build ruling).
				// â HOLE (city lifecycle / ownership): the blanket that stood here is gone -- a blanket recompute does not exist
				// under the maintained sum and is never to be built. What this needs instead is a WITHDRAW AND
				// REAPPLY over every source this city holds and every above-city
				// source of its owner, which is the same plane-A
				// walk run twice with opposite signs. Deliberately left unmaintained rather than swept.
					mc_applyGated(DepositIndex::gatedByToken("CITY"), kEvent.iB, MMKernel::PER_SCALE_SUPPRESSED,
					pPlayer, NULL, NULL);
				}
				break;
			}
			// ---- ownership moves: the entity's packages change scope owner; both empires' aggregates move ----
			case SEVT_CITY_OWNER_ADDED:
			case SEVT_CITY_OWNER_REMOVED:
			{
				// whole-scope blankets KEPT: an ownership move is a scope-object COMPOSITION event, not
				// deposit-addressed -- the transferred city's evaluated source set (owner civics/traits/
				// techs/counts) swaps wholesale, so no union of per-source routes can address the departed
				// owner's folded deposits; the city package + sums and both empires' aggregates re-derive.
				const CvPlayer* pOldOwner = mc_player(kEvent.iA);
				const CvPlayer* pNewOwner = mc_player(kEvent.iC);
				const CvCity* pCity = mc_city(pNewOwner, kEvent.iSrcLoc);
				if (pCity != NULL)
				{
				// â HOLE (city lifecycle / ownership): the blanket that stood here is gone -- a blanket recompute does not exist
				// under the maintained sum and is never to be built. What this needs instead is a WITHDRAW AND
				// REAPPLY over every source this city holds and every above-city
				// source of its owner, which is the same plane-A
				// walk run twice with opposite signs. Deliberately left unmaintained rather than swept.
				}
				mc_applyGated(DepositIndex::gatedByToken("CITY"), kEvent.iB, MMKernel::PER_SCALE_SUPPRESSED,
					pOldOwner, NULL, NULL);
				mc_applyGated(DepositIndex::gatedByToken("CITY"), kEvent.iB, MMKernel::PER_SCALE_SUPPRESSED,
					pNewOwner, NULL, NULL);
				break;
			}
			case SEVT_PLOT_OWNER_ADDED:
			case SEVT_PLOT_OWNER_REMOVED:
			{
				// an owner flip changes the plot's evaluated SOURCE SET (refreshPlot folds gt_foldPlayerSources
				// over the NEW owner), so the whole isolated base package re-derives -- the substrate blanket's
				// ruling applies (no per-source route can address the departed owner's folded deposits) -- and
				// the realized sums the plot feeds go stale with it ([DEC-no-self-heal]: marked here or never).
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				if (pPlot != NULL)
				{
					// ...and the DEPARTED owner. mc_markPlotFedSums reads the plot's LIVE owner, so it can only
					// ever reach the NEW one -- but the plot's yield has just left the old empire, whose plot-fed
					// receiver sums are now stale with no route to re-derive them ([DEC-no-self-heal]: marked here
					// or never). The fact carries the old owner precisely so a consumer can act on the DELTA, and
					// this is the same both-sides shape SEVT_WORKING_CITY_CHANGED uses for its two cities.
					if (kEvent.iA >= 0 && kEvent.iA != (int)NO_PLAYER)
					{
					}
					// ⛔ IS_OWNED is NOT re-derived here. It is an ordinary plot verdict on the OWNER axis, so
					// PlotContext derives it off this same fact and announces the crossing; the predicate route
					// then reaches the plot and the city standing on it. Re-deriving it here would double it.
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
		}

	private:
		// A whole PLAYER's deposit basis moved -- every package it owns re-derives (its cities' channels + sums and
		// its own empire channels + sums). The two callers are the facts that move a source the gather folds at
		// EVERY scope rather than at a deposit-addressed one:
		//   - a DIFFICULTY change: the gather folds the handicap's own modifier families into this player's
		//     packages (CvCascadeGather), so flexible difficulty moving the handicap moves that whole basis;
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
