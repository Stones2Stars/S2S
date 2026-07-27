//
//	CvModifierConsumer -- the modifier cascade's own spine consumer (see the header). The switch below names
//	only the event's SEMANTICS (which repo its iType indexes; which state a stateful event embodies); every
//	MASK is derived from the deposit index -- routeFor for a source-carrying event, the condition-dependency
//	routes for a state flip a deposit's gate reads. ONE application helper marks packages + receiver sums.
//	The ONLY non-derived masks are the RULED blankets, each carrying its justifying constraint at the case:
//	scope-object lifecycle/composition events (city founded / ownership moves) and the golden-age engine
//	member-mirror are not deposit-addressed, so no route exists in the index to derive.
//
//	A PLOT-FACT predicate route names its owner objects through mc_applyPlotPredicate, whose fan is read off
//	CascadeGather's per-scope ctx binding rather than guessed: the plot itself, the city SITTING ON it and that
//	city's package are the only ones a plot verdict can move, because those are the only folds
//	that bind a plot -- so a one-tile fact never reaches the player. HAS_COAST and HAS_FRESHWATER additionally
//	fan ONE HOP (the two adjacency verdicts, PlotContext::adjacencyFactsMask).
//
//	⚠ A MARK HERE IS ALSO THE REBUILD (state-repositories.md: "the rebuild moves onto the mark"). Reads are bare
//	fetches, so nothing downstream will recompute later -- the recompute happens at markMask, inside this
//	consumer, on the event that derived the mark. Inside the load bracket the marks are BANKED instead and
//	drained once at GAME_LOAD_FINISHED (mc_drainLoadMarks); the walk order there is irrelevant because a
//	rebuild reads its cross-scope inputs through the package's rebuild-path accessors.
//

#include "CvGameCoreDLL.h"
#include "CvModifierConsumer.h"
#include "CvCascadePackage.h"
#include "CvCascadeChannelRegistry.h"
#include "Data/CvDepositIndex.h"        // routeFor + the dependency routes -- the ONE mark derivation
#include "Data/CvDepositRead.h"         // MMKernel::traitData -- the active trait set's info
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

	void mc_invalidate(CvCascScope eScope, int iOwner, int iId, int64_t iMask, const char* szSource)
	{
		if (iMask != 0)
		{
			emitCacheInvalidate((int)eScope, iOwner, iId, iMask, szSource);
		}
	}

	void mc_markCity(const CvCity* pCity, int64_t iMask, const char* szSource)
	{
		if (pCity == NULL || iMask == 0)
		{
			return;
		}
		pCity->getCascadePackage().markMask(iMask);
		mc_invalidate(CASC_SCOPE_CITY, (int)pCity->getOwner(), pCity->getID(), iMask, szSource);
	}

	void mc_markPlot(const CvPlot* pPlot, int64_t iMask, const char* szSource)
	{
		if (pPlot == NULL || iMask == 0)
		{
			return;
		}
		pPlot->getCascadePackage().markMask(iMask);
		mc_invalidate(CASC_SCOPE_PLOT, (int)pPlot->getOwner(), GC.getMap().plotNum(pPlot->getX(), pPlot->getY()), iMask, szSource);
	}

	void mc_markEmpire(const CvPlayer* pPlayer, int64_t iMask, const char* szSource)
	{
		if (pPlayer == NULL || iMask == 0)
		{
			return;
		}
		pPlayer->getCascadePackage().markMask(iMask);
		mc_invalidate(CASC_SCOPE_EMPIRE, (int)pPlayer->getID(), (int)pPlayer->getID(), iMask, szSource);
	}

	// A re-based plot's realized sums, ONE EVENT MARKS BOTH LEVELS: the working city's rates AND the owner's
	// empire sums fed by the plot-authored channels (culture, the dual-consumer, reaches both). The masks
	// derive from the minted plot channel set (scopeReceiversFedBy), never a hand-listed all-receivers.
	void mc_markPlotFedSums(const CvPlot* pPlot, const char* szSource)
	{
		if (pPlot == NULL)
		{
			return;
		}
		mc_markCity(pPlot->getWorkingCity(),
			CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_CITY, CASC_SCOPE_PLOT), szSource);
		if (pPlot->getOwner() != NO_PLAYER)
		{
			mc_markEmpire(&GET_PLAYER(pPlot->getOwner()),
				CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_EMPIRE, CASC_SCOPE_PLOT), szSource);
		}
	}

	// The EMPIRE receiver bits whose channel is a COMMERCE channel -- derived from the MINTED receiver set, so
	// a receiver that is not a commerce channel is never marked and no bit is hand-listed. This is the reach of
	// the city's production->commerce PROCESS conversion: the gather folds getProductionProcess()'s
	// productionToCommerce into the per-city commerce SPLIT, and that split is the per-city quantity ONLY an
	// empire commerce receiver sums (a city's own receiver rates are the plain §2a combine, which reads no
	// process and no slider).
	int64_t mc_empireCommerceReceiverMask()
	{
		int64_t iMask = 0;
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_EMPIRE);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_EMPIRE, iReceiver);
			if (iChannel < 0)
			{
				continue;
			}
			if (infoFamilyCommerce(CascadeChannelRegistry::channelFamily(iChannel)) >= 0)
			{
				iMask |= CascadeChannelRegistry::scopeReceiverBit(CASC_SCOPE_EMPIRE, iChannel);
			}
		}
		return iMask;
	}

	// Apply a derived route to the owner objects the event names. pCity/pPlot narrow the fan to the event's
	// own object; a NULL city with city-reaching bits fans to every owner city (an above-city deposit rolls
	// DOWN -- SourceRoute::cityFanAll is the per-CHANNEL statement of the same fact for the sums).
	void mc_applyRoute(const SourceRoute* pRoute, const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot,
		const char* szSource)
	{
		if (pRoute == NULL || pRoute->empty())
		{
			return;
		}
		// PLOT packages: the event's own plot; a plot-authoring source with no event plot (a civic's plot
		// flats) reaches every plot the owner works -- fanned through the owner's cities' radii.
		const int64_t iPlotMask = pRoute->packageMask[(int)CASC_SCOPE_PLOT];
		if (iPlotMask != 0)
		{
			if (pPlot != NULL)
			{
				mc_markPlot(pPlot, iPlotMask, szSource);
			}
			else if (pPlayer != NULL)
			{
				for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
				{
					const CvCity* pLoopCity = *cityIterator;
					const int iNumPlots = pLoopCity->getNumCityPlots();
					for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
					{
						mc_markPlot(pLoopCity->getCityIndexPlot(iPlotIndex), iPlotMask, szSource);
					}
				}
			}
		}
		// CITY packages
		const int64_t iCityMask = pRoute->packageMask[(int)CASC_SCOPE_CITY];
		if (iCityMask != 0)
		{
			if (pCity != NULL)
			{
				mc_markCity(pCity, iCityMask, szSource);
			}
			else if (pPlayer != NULL)
			{
				for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
				{
					mc_markCity(*cityIterator, iCityMask, szSource);
				}
			}
		}
		// EMPIRE package
		const int64_t iEmpireMask = pRoute->packageMask[(int)CASC_SCOPE_EMPIRE];
		if (iEmpireMask != 0 && pPlayer != NULL)
		{
			pPlayer->getCascadePackage().markMask(iEmpireMask);
			mc_invalidate(CASC_SCOPE_EMPIRE, (int)pPlayer->getID(), (int)pPlayer->getID(), iEmpireMask, szSource);
		}
		// TEAM package
		const int64_t iTeamMask = pRoute->packageMask[(int)CASC_SCOPE_TEAM];
		if (iTeamMask != 0 && pPlayer != NULL)
		{
			const CvTeam& team = GET_TEAM(pPlayer->getTeam());
			team.getCascadePackage().markMask(iTeamMask);
			mc_invalidate(CASC_SCOPE_TEAM, (int)pPlayer->getID(), (int)team.getID(), iTeamMask, szSource);
		}
		// the RECEIVER sums the touched channels feed (ONE derivation marks BOTH levels)
		if (pRoute->citySumMask != 0)
		{
			const CvCity* pSumCity = (pCity != NULL) ? pCity : ((pPlot != NULL) ? pPlot->getWorkingCity() : NULL);
			if (pSumCity != NULL && !pRoute->cityFanAll)
			{
				pSumCity->getCascadePackage().markMask(pRoute->citySumMask);
				mc_invalidate(CASC_SCOPE_CITY, (int)pSumCity->getOwner(), pSumCity->getID(), pRoute->citySumMask, szSource);
			}
			else if (pPlayer != NULL)
			{
				for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
				{
					(*cityIterator)->getCascadePackage().markMask(pRoute->citySumMask);
				}
				mc_invalidate(CASC_SCOPE_CITY, (int)pPlayer->getID(), -1, pRoute->citySumMask, szSource);
			}
		}
		if (pRoute->empireSumMask != 0 && pPlayer != NULL)
		{
			pPlayer->getCascadePackage().markMask(pRoute->empireSumMask);
			mc_invalidate(CASC_SCOPE_EMPIRE, (int)pPlayer->getID(), (int)pPlayer->getID(), pRoute->empireSumMask, szSource);
		}
	}

	// The source-carrying application: the source info's own deposits + everything conditioned ON the source
	// (a deposit gated on this entity's presence re-evaluates -- the dependency route keyed by its TYPE).
	void mc_applySource(const CvInfo* pSourceInfo, const char* szTypeName,
		const CvPlayer* pPlayer, const CvCity* pCity, const CvPlot* pPlot, const char* szSource)
	{
		if (pSourceInfo != NULL)
		{
			mc_applyRoute(&DepositIndex::routeFor(pSourceInfo), pPlayer, pCity, pPlot, szSource);
		}
		if (szTypeName != NULL)
		{
			mc_applyRoute(DepositIndex::dependencyForType(std::string(szTypeName)), pPlayer, pCity, pPlot, szSource);
		}
	}

	// ---- the PLOT-FACT predicate routes: the owner objects whose rebuild READS the announcing plot ----
	//
	// A plot predicate (HAS_IRRIGATION / HAS_LANDMARK / IS_OWNED / HAS_COAST / HAS_FRESHWATER / the relief block)
	// is answered off the eval ctx's PLOT (CvConditionEval::ev_evalPredicate -> PlotContext), and CascadeGather
	// binds that plot PER SCOPE -- which IS the whole derivation of where a flip can reach:
	//   PLOT   fold -- evalCtx.plot = the plot itself                         (gt_gatherPlotChannels)
	//   CITY   fold -- evalCtx.plot = THAT CITY'S OWN CENTRE plot             (gt_fillCityEvalCtx -> InfoValuation::
	//                  fillEvalCtx -> CityContext::fillEvalCtx: ec.plot = m_city->plot())
	//   EMPIRE and TEAM folds bind NO plot at all (EmpireContext::fillEvalCtx fills player/team only; the team
	//   gather sets team/player), so a plot predicate is constantly not-present there -- the evaluator's
	//   NULL-object convention -- and those packages cannot move on a plot fact.
	// So the objects a plot fact names are: the plot, the city SITTING ON it (if any) and that city's
	// own package. ⛔ The PLAYER is therefore passed ONLY together with that city: every player-reaching
	// leg of mc_applyRoute (the empire/team packages, the city-less all-cities fan, the cityFanAll sum fan) would
	// otherwise turn a ONE-TILE fact into an empire-wide sweep -- the widening "emit liberally, mark precisely"
	// forbids (event-spine.md; [DEC-no-self-heal]).
	// The realized SUMS still reach the city that WORKS the plot: mc_applyRoute's receiver leg falls back to
	// pPlot->getWorkingCity(), which is the city whose rate sums this plot's package (the worked-plot Sigma the
	// gather reads through CvCity::isWorkingPlot).
	void mc_applyPlotPredicate(CvCascPredKind ePredicate, const CvPlot* pPlot, bool bPlotPackageAlreadyMarked,
		const char* szSource)
	{
		if (pPlot == NULL)
		{
			return;
		}
		const SourceRoute* pRoute = DepositIndex::dependencyForPredicate(ePredicate);
		if (pRoute == NULL || pRoute->empty())
		{
			return;
		}
		const CvCity* pPlotCity = pPlot->getPlotCity();
		const CvPlayer* pPlayer = (pPlotCity != NULL) ? &GET_PLAYER(pPlotCity->getOwner()) : NULL;
		SourceRoute kRoute = *pRoute;
		if (bPlotPackageAlreadyMarked)
		{
			// The caller's case already marked this plot's package WHOLE (the substrate blanket below), and a
			// mark is ALSO the rebuild (CvCascadePackage::markMask -> CvDerivedCacheSet::markDirty) -- re-marking
			// a bit marked moments ago is a SECOND full gather of the same plot, not a no-op. The blanket is a
			// superset of every plot-scope bit this route can name, so the plot leg drops and the rest applies.
			kRoute.packageMask[(int)CASC_SCOPE_PLOT] = 0;
		}
		if (pPlotCity == NULL)
		{
			// No city sits here, so no CITY fold -- which binds a city's CENTRE plot -- can have
			// moved: the only package that did is the plot's own, and exactly ONE city consumes its realized
			// value (the one working it). An above-city deposit's cityFanAll thus has nothing to fan, and leaving
			// it set would instead DROP the working city's sums, mc_applyRoute's fan-all branch needing the
			// player this fact does not name.
			kRoute.cityFanAll = false;
		}
		mc_applyRoute(&kRoute, pPlayer, pPlotCity, pPlot, szSource);
	}

	// The ADJACENCY predicates fan ONE HOP. HAS_COAST and HAS_FRESHWATER are the two verdicts a plot derives from
	// its NEIGHBOURS (PlotContext::adjacencyFactsMask), so a fact moving what a neighbour reads moves that
	// neighbour's verdict too -- the same one-hop fan-out the contexts' maintainer runs
	// (Engine/ContextConsumer::fanOutAdjacency). It cannot cascade: a neighbour's coast / fresh-water verdict
	// reads nothing but facts held in the announcing plot's OWN block (CvPlot::isCoastalLand reads a neighbour's
	// isWater plus its water area's tile count; CvPlot::isFreshWater's rect(1,1) leg reads a neighbour's isWater
	// plus its fresh-water TERRAIN), so the derivation terminates at one ring.
	void mc_applyAdjacentPlotPredicate(CvCascPredKind ePredicate, const CvPlot* pPlot, const char* szSource)
	{
		if (pPlot == NULL)
		{
			return;
		}
		for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; ++iDirection)
		{
			mc_applyPlotPredicate(ePredicate, plotDirection(pPlot, (DirectionTypes)iDirection), false, szSource);
		}
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
			// ---- source-carrying state changes: the mask IS the source's compiled route ----
			case SEVT_BUILDING_PROCESSED:   // the operating-contribution flip -- deposits flow only while processed
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					mc_applySource(&GC.getBuildingInfo((BuildingTypes)kEvent.iType), NULL, pPlayer, pCity, NULL, szSource);
				}
				break;
			}
			case SEVT_BUILDING_CHANGED:     // the PRESENCE fact -- re-evaluates everything gated on this building
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBuildingInfos())
				{
					mc_applySource(NULL, GC.getBuildingInfo((BuildingTypes)kEvent.iType).getType(), pPlayer, pCity, NULL, szSource);
				}
				break;
			}
			case SEVT_RELIGION_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumReligionInfos())
				{
					mc_applySource(&GC.getReligionInfo((ReligionTypes)kEvent.iType),
						GC.getReligionInfo((ReligionTypes)kEvent.iType).getType(), pPlayer, pCity, NULL, szSource);
				}
				mc_applyRoute(DepositIndex::dependencyForReligionCounts(), pPlayer, pCity, NULL, szSource);
				break;
			}
			case SEVT_CORPORATION_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumCorporationInfos())
				{
					mc_applySource(&GC.getCorporationInfo((CorporationTypes)kEvent.iType),
						GC.getCorporationInfo((CorporationTypes)kEvent.iType).getType(), pPlayer, pCity, NULL, szSource);
				}
				break;
			}
			case SEVT_SPECIALIST_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumSpecialistInfos())
				{
					mc_applySource(&GC.getSpecialistInfo((SpecialistTypes)kEvent.iType),
						GC.getSpecialistInfo((SpecialistTypes)kEvent.iType).getType(), pPlayer, pCity, NULL, szSource);
				}
				break;
			}
			case SEVT_BONUS_CHANGED:          // a city's connected-bonus count changed
			case SEVT_VICINITY_BONUS_CHANGED: // a city's local (vicinity) supply flipped
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
				{
					mc_applySource(&GC.getBonusInfo((BonusTypes)kEvent.iType),
						GC.getBonusInfo((BonusTypes)kEvent.iType).getType(), pPlayer, pCity, NULL, szSource);
				}
				break;
			}
			case SEVT_PLOTGROUP_BONUS_CHANGED:   // the trade network's resource set -- reaches every connected city
			case SEVT_CITY_NETWORK_CHANGED:      // a city's whole network membership changed
			{
				const char* szBonusType = (kEvent.iEventId == SEVT_PLOTGROUP_BONUS_CHANGED
					&& kEvent.iType >= 0 && kEvent.iType < GC.getNumBonusInfos())
					? GC.getBonusInfo((BonusTypes)kEvent.iType).getType() : NULL;
				if (szBonusType != NULL)
				{
					mc_applySource(&GC.getBonusInfo((BonusTypes)kEvent.iType), szBonusType, pPlayer, NULL, NULL, szSource);
				}
				else
				{
					// membership flip: every bonus-conditioned deposit of the owner re-gates (the network set
					// is unknown here -- the connection dependency class fans the owner's cities)
					mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_HAS_BONUS), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			// ---- plot substrate changes: the plot's isolated base package refills whole (the substrate IS
			// ---- the base; the event carries no old-type to narrow by) + the working city's rates ----
			// ---- THE UNIT PLANE: resolved values, not a package (state-repositories.md). The model names
			// ---- these two facts EXACTLY -- "they dirty on a different trigger from everything else: ONLY
			// ---- when a promotion or combat class changes" -- so there is no route derivation here and no
			// ---- blanket: the held set moved, so the unit re-resolves. Unit MOVEMENT never reaches this.
			case SEVT_UNIT_PROMOTION_CHANGED:
			case SEVT_UNIT_COMBAT_CHANGED:
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
			case SEVT_IMPROVEMENT_CHANGED:
			case SEVT_TERRAIN_CHANGED:
			case SEVT_FEATURE_CHANGED:
			case SEVT_ROUTE_CHANGED:
			case SEVT_PLOT_BONUS_CHANGED:
			{
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				if (pPlot != NULL)
				{
					// the whole-plot-package mark is the RULED blanket, not a derivable route: the event
					// carries the NEW type only, and the departed substrate's deposits (own-output AND every
					// owner-source entry keyed on it) are unaddressable without the old type -- the isolated
					// base package refills whole (modifier.md §2 plot-as-base; the plot set is the smallest).
					pPlot->getCascadePackage().markMask(CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_PLOT));
					mc_invalidate(CASC_SCOPE_PLOT, kEvent.iC, kEvent.iSrcLoc, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_PLOT), szSource);
					mc_markPlotFedSums(pPlot, szSource);
					// The blanket covers this plot's OWN package and nothing else, so the two substrate facts a
					// FRESH-WATER verdict reads carry legs it cannot reach. CvPlot::isFreshWater reads this plot's
					// TERRAIN (isFreshWaterTerrain, and isImpassable) and its FEATURE (isAddsFreshWater) -- moving
					// the verdict a CITY sitting here folds through its own centre plot
					// slot with it. TERRAIN additionally moves what the NEIGHBOURS read: isFreshWater's rect(1,1)
					// leg tests an adjacent plot's isWater AND fresh-water terrain, so a terrain flip re-gates the
					// ring's HAS_FRESHWATER deposits. The other three facts of this group reach neither leg --
					// improvement / route / bonus appear nowhere in isFreshWater or isCoastalLand, and the
					// irrigation they drive announces itself through CvPlot::setIrrigated's own fact.
					if (kEvent.iEventId == SEVT_TERRAIN_CHANGED || kEvent.iEventId == SEVT_FEATURE_CHANGED)
					{
						mc_applyPlotPredicate(CASC_PRED_HAS_FRESHWATER, pPlot, true, szSource);
					}
					if (kEvent.iEventId == SEVT_TERRAIN_CHANGED)
					{
						mc_applyAdjacentPlotPredicate(CASC_PRED_HAS_FRESHWATER, pPlot, szSource);
					}

					// ⛔ The blanket above marks this PLOT's own package. It does NOT reach a deposit at CITY or
					// EMPIRE scope that is CONDITIONED on -- or `per`-scaled by -- this substrate TYPE; that is
					// exactly what the dependency route addresses, and without it such a deposit was never
					// re-marked when the substrate appeared, staying wrong until something unrelated dirtied it.
					// ⚠ ADDRESSES THE ARRIVING TYPE ONLY. The fact carries iType = the NEW substrate and no old
					// value, so a deposit keyed on the DEPARTING one cannot be routed -- the same limit the
					// blanket comment states, and the reason the plot package refills whole rather than by route.
					const char* szSubstrateType = NULL;
					if (kEvent.iType >= 0)
					{
						switch (kEvent.iEventId)
						{
						case SEVT_IMPROVEMENT_CHANGED:
							if (kEvent.iType < GC.getNumImprovementInfos())
								szSubstrateType = GC.getImprovementInfo((ImprovementTypes)kEvent.iType).getType();
							break;
						case SEVT_TERRAIN_CHANGED:
							if (kEvent.iType < GC.getNumTerrainInfos())
								szSubstrateType = GC.getTerrainInfo((TerrainTypes)kEvent.iType).getType();
							break;
						case SEVT_FEATURE_CHANGED:
							if (kEvent.iType < GC.getNumFeatureInfos())
								szSubstrateType = GC.getFeatureInfo((FeatureTypes)kEvent.iType).getType();
							break;
						case SEVT_ROUTE_CHANGED:
							if (kEvent.iType < GC.getNumRouteInfos())
								szSubstrateType = GC.getRouteInfo((RouteTypes)kEvent.iType).getType();
							break;
						case SEVT_PLOT_BONUS_CHANGED:
							if (kEvent.iType < GC.getNumBonusInfos())
								szSubstrateType = GC.getBonusInfo((BonusTypes)kEvent.iType).getType();
							break;
						default:
							break;
						}
					}
					if (szSubstrateType != NULL)
					{
						mc_applySource(NULL, szSubstrateType, pPlayer, pPlot->getWorkingCity(), pPlot, szSource);
					}
				}
				break;
			}
			// ---- the remaining plot substrate facts. NO blanket here, and that is derived rather than
			// ---- conservative: plot TYPE / river / irrigation / landmark are not deposit KEYS (gt_foldInfo keys
			// ---- targeted entries on improvement / terrain / feature / bonus / route ONLY), so nothing but a
			// ---- CONDITIONED deposit can read them -- and a conditioned deposit is exactly what the dependency
			// ---- route addresses. ----
			case SEVT_PLOT_TYPE_CHANGED:
			{
				// The land/water/relief axis. CvPlot::isWater() IS getPlotType() == PLOT_OCEAN, so this one fact
				// carries the whole own-plot relief block (PlotContext::refreshOwnFacts reads isWater/isHills/
				// isPeak, all of them getPlotType) AND both adjacency verdicts: isCoastalLand reads this plot's
				// isWater and every neighbour's, and isFreshWater reads isWater on this plot and, through its
				// rect(1,1) leg, on the ring. Hence self for all of them, plus the one-hop fan for the two
				// adjacency verdicts alone.
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				mc_applyPlotPredicate(CASC_PRED_IS_WATER, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_IS_LAND, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_IS_FLATLANDS, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_HAS_HILLS, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_HAS_PEAK, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_HAS_COAST, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_HAS_FRESHWATER, pPlot, false, szSource);
				mc_applyAdjacentPlotPredicate(CASC_PRED_HAS_COAST, pPlot, szSource);
				mc_applyAdjacentPlotPredicate(CASC_PRED_HAS_FRESHWATER, pPlot, szSource);
				break;
			}
			case SEVT_PLOT_RIVER_CHANGED:
			{
				// CvPlot::changeRiverCrossingCount crossing zero is the ONE mutation of isRiver(), which both
				// HAS_RIVER and the fresh-water verdict read (PlotContext's stored fresh-water bit is
				// isFreshWater() || isRiver(), and isFreshWater tests isRiver in its own right). Own-plot only:
				// no neighbour verdict reads this plot's river -- isFreshWater's ring leg tests isWater plus
				// fresh-water TERRAIN, never a crossing -- and each plot whose crossing count moves announces
				// itself through its own emit, so there is nothing for a fan to cover.
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				mc_applyPlotPredicate(CASC_PRED_HAS_RIVER, pPlot, false, szSource);
				mc_applyPlotPredicate(CASC_PRED_HAS_FRESHWATER, pPlot, false, szSource);
				break;
			}
			case SEVT_PLOT_IRRIGATION_CHANGED:
			{
				// CvPlot::setIrrigated is the ONE mutation of the flag PlotContext::hasIrrigation reads (the
				// improvement-driven updateIrrigated and the city fresh-water counter's sweep both funnel through
				// it, so this fact carries every source of the change). An OWN fact -- PlotContext::ownFactsMask,
				// no neighbour's verdict reads it -- so no adjacency fan.
				mc_applyPlotPredicate(CASC_PRED_HAS_IRRIGATION, mc_plot(kEvent.iSrcLoc), false, szSource);
				break;
			}
			case SEVT_PLOT_LANDMARK_CHANGED:
			{
				// CvPlot::setLandmarkType is the ONE mutation behind PlotContext::hasLandmark
				// (getLandmarkType() != NO_LANDMARK). An OWN fact, so again no adjacency fan.
				mc_applyPlotPredicate(CASC_PRED_HAS_LANDMARK, mc_plot(kEvent.iSrcLoc), false, szSource);
				break;
			}
			case SEVT_PLOT_CITY_CHANGED:
			{
				// A city arriving on / leaving a plot moves that PLOT's fresh-water verdict: CvPlot::isFreshWater
				// reads getPlotCity()->hasFreshWater(), the provider-building access counter, which an ACQUIRED
				// city brings with its buildings. The city's own packages are not this fact's business -- a
				// founding builds them whole (SEVT_CITY_FOUNDED) and an acquisition re-derives them
				// (SEVT_CITY_OWNER_CHANGED); what neither of those reaches is the plot underneath.
				mc_applyPlotPredicate(CASC_PRED_HAS_FRESHWATER, mc_plot(kEvent.iSrcLoc), false, szSource);
				break;
			}
			case SEVT_WORKING_CITY_CHANGED:   // the plot's yield moves between cities: both cities' rates
			{
				const CvCity* pOldCity = mc_city(pPlayer, kEvent.iA);
				const CvCity* pNewCity = mc_city(pPlayer, kEvent.iB);
				// derived, not blanket: exactly the realized sums the plot-authored channels feed
				const int64_t iSumMask = CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_CITY, CASC_SCOPE_PLOT);
				mc_markCity(pOldCity, iSumMask, szSource);
				mc_markCity(pNewCity, iSumMask, szSource);
				mc_markEmpire(pPlayer, CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_EMPIRE, CASC_SCOPE_PLOT), szSource);
				break;
			}
			case SEVT_PLOT_WORKED_CHANGED:   // a citizen took / left the plot: the city's worked-plot Sigma moves
			{
				// The worked-plot Sigma is the BASE of every city receiver rate (modifier.md §2a TIER 1): the
				// rebuild sums the packages of exactly the plots city.isWorkingPlot() answers, so an assignment
				// flip moves the realized sums the PLOT-authored channels feed -- and nothing besides them. The
				// plot's OWN package is untouched: worked-ness changes no deposit the plot holds, which is why
				// the substrate blanket has no business here. Same derivation as the WORKING-CITY membership
				// fact, whose subset this assignment fact is (membership = may work it, worked = a citizen does).
				// The event names its city (iB), so the fan never widens past it.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iB);
				mc_markCity(pCity,
					CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_CITY, CASC_SCOPE_PLOT), szSource);
				mc_markEmpire(pPlayer,
					CascadeChannelRegistry::scopeReceiversFedBy(CASC_SCOPE_EMPIRE, CASC_SCOPE_PLOT), szSource);
				// IS_WORKED is a live PREDICATE a deposit's gate may read (PlotContext::isWorked), so the
				// deposits conditioned on it re-evaluate -- through the route the index derives for that state,
				// never a widened mask.
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_WORKED), pPlayer, pCity,
					mc_plot(kEvent.iSrcLoc), szSource);
				break;
			}
			case SEVT_CITY_ORDER_CHANGED:   // the queue HEAD is the active process (the production->commerce conversion)
			{
				// A city's active PROCESS is its head order, and the conversion it drives is a TIER-2 term of the
				// per-city commerce split -- so the mark is exactly the empire's COMMERCE receiver sums, derived
				// from the minted receiver set. The process reaches no package channel (it is live state read at
				// the combine, never a deposit) and no city receiver sum.
				// ⛔ The order TYPE cannot narrow this: pushOrder inserts at the FRONT whenever the head is
				// ORDER_MAINTAIN, so pushing an ordinary build DISPLACES a running process -- a filter on
				// ORDER_MAINTAIN would miss exactly that. The head order moving is what this fact announces, and
				// the head order IS the process, so the mark is the process's own reach, unfiltered.
				mc_markEmpire(pPlayer, mc_empireCommerceReceiverMask(), szSource);
				break;
			}
			// ---- empire-level source flips ----
			case SEVT_TECH_CHANGED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumTechInfos())
				{
					mc_applySource(&GC.getTechInfo((TechTypes)kEvent.iType),
						GC.getTechInfo((TechTypes)kEvent.iType).getType(), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			case SEVT_TRAIT_CHANGED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumTraitInfos())
				{
					const CvTraitInfo* pTrait = MMKernel::traitData(kEvent.iType);   // the ACTIVE set's record
					mc_applySource(pTrait, (pTrait != NULL) ? pTrait->getType() : NULL, pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			case SEVT_CIVIC_ADOPTED:   // the swap fact: the adopted civic AND the swapped-out one both re-route
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumCivicInfos())
				{
					mc_applySource(&GC.getCivicInfo((CivicTypes)kEvent.iType),
						GC.getCivicInfo((CivicTypes)kEvent.iType).getType(), pPlayer, NULL, NULL, szSource);
				}
				if (kEvent.iB >= 0 && kEvent.iB < GC.getNumCivicInfos())
				{
					mc_applySource(&GC.getCivicInfo((CivicTypes)kEvent.iB),
						GC.getCivicInfo((CivicTypes)kEvent.iB).getType(), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			case SEVT_PROJECT_CHANGED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumProjectInfos())
				{
					mc_applySource(&GC.getProjectInfo((ProjectTypes)kEvent.iType),
						GC.getProjectInfo((ProjectTypes)kEvent.iType).getType(), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			case SEVT_HERITAGE_CHANGED:
			{
				if (kEvent.iType >= 0 && kEvent.iType < GC.getNumHeritageInfos())
				{
					mc_applySource(&GC.getHeritageInfo((HeritageTypes)kEvent.iType),
						GC.getHeritageInfo((HeritageTypes)kEvent.iType).getType(), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			// ---- state flips a deposit's GATE reads: the dependency routes ----
			case SEVT_POPULATION_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyRoute(DepositIndex::dependencyForToken("POPULATION"), pPlayer, pCity, NULL, szSource);
				break;
			}
			case SEVT_POWER_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_HAS_POWER), pPlayer, pCity, NULL, szSource);
				break;
			}
			case SEVT_CITY_FRESH_WATER_CHANGED:
			{
				// The CITY leg of HAS_FRESHWATER: the predicate is satisfied by the plot's own access OR by the
				// city's provider-building counter (CvConditionEval: plotContext->hasFreshWater() ||
				// cityContext->hasFreshWaterAccess()), and CvCity::changeFreshWater is that counter's one
				// crossing. The reach is this city's CENTRE plot, which is precisely what the plot-fact helper
				// resolves: the centre plot's getPlotCity() IS this city, so the one call marks the plot package
				// (CvPlot::isFreshWater reads getPlotCity()->hasFreshWater(), so the tile's own verdict moves
				// too), and this city's package and receiver sums. No adjacency fan --
				// a neighbour's fresh-water leg reads isWater plus fresh-water TERRAIN, never a city's counter.
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				if (pCity != NULL)
				{
					mc_applyPlotPredicate(CASC_PRED_HAS_FRESHWATER, pCity->plot(), false, szSource);
				}
				break;
			}
			case SEVT_GOLDEN_AGE_CHANGED:
			{
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_GOLDEN_AGE), pPlayer, NULL, NULL, szSource);
				// the golden-age member-mirror (the ledgered engine carve-out) applies at the receiver
				// combine -- the flip re-realizes every rate of the player
				mc_markAllPlayerSums(pPlayer, szSource);
				break;
			}
			case SEVT_STATE_RELIGION_CHANGED:
			{
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_HAS_STATE_RELIGION), pPlayer, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_STATE_RELIGION_IN_CITY), pPlayer, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_STATE_RELIGION), pPlayer, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_STATE_RELIGION), pPlayer, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForReligionCounts(), pPlayer, NULL, NULL, szSource);
				break;
			}
			case SEVT_HOLY_CITY_CHANGED:
			{
				const CvCity* pCity = mc_city(pPlayer, kEvent.iSrcLoc);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_HOLY_CITY), pPlayer, pCity, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_STATE_RELIGION_HOLY_CITY), pPlayer, pCity, NULL, szSource);
				break;
			}
			case SEVT_ERA_CHANGED:
			{
				mc_applyRoute(DepositIndex::dependencyForToken("ERA"), pPlayer, NULL, NULL, szSource);
				break;
			}
			case SEVT_NUKES_CHANGED:
			{
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_NO_NUKES), pPlayer, NULL, NULL, szSource);
				break;
			}
			case SEVT_CAPITAL_CHANGED:
			{
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_CAPITAL), pPlayer, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForPredicate(CASC_PRED_IS_GOVERNMENT_CENTER), pPlayer, NULL, NULL, szSource);
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
					pCity->getCascadePackage().markMask(
						CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_CITY)
						| CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_CITY));
					mc_applyRoute(DepositIndex::dependencyForToken("CITY"), pPlayer, NULL, NULL, szSource);
				}
				break;
			}
			// ---- ownership moves: the entity's packages change scope owner; both empires' aggregates move ----
			case SEVT_CITY_OWNER_CHANGED:
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
					pCity->getCascadePackage().markMask(
						CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_CITY)
						| CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_CITY));
				}
				mc_markEmpireWhole(pOldOwner, szSource);
				mc_markEmpireWhole(pNewOwner, szSource);
				mc_applyRoute(DepositIndex::dependencyForToken("CITY"), pOldOwner, NULL, NULL, szSource);
				mc_applyRoute(DepositIndex::dependencyForToken("CITY"), pNewOwner, NULL, NULL, szSource);
				break;
			}
			case SEVT_PLOT_OWNER_CHANGED:
			{
				// an owner flip changes the plot's evaluated SOURCE SET (refreshPlot folds gt_foldPlayerSources
				// over the NEW owner), so the whole isolated base package re-derives -- the substrate blanket's
				// ruling applies (no per-source route can address the departed owner's folded deposits) -- and
				// the realized sums the plot feeds go stale with it ([DEC-no-self-heal]: marked here or never).
				const CvPlot* pPlot = mc_plot(kEvent.iSrcLoc);
				if (pPlot != NULL)
				{
					mc_markPlot(pPlot, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_PLOT), szSource);
					mc_markPlotFedSums(pPlot, szSource);
					// IS_OWNED is a live PREDICATE a deposit's gate may read (PlotContext::isOwned = getOwner() !=
					// NO_PLAYER), and setOwner is its ONE mutation. The plot leg is suppressed -- the blanket
					// above already marked this package WHOLE, and a mark is the rebuild -- so what this adds is
					// the legs the blanket cannot reach: the package of a CITY sitting here (its fold binds this
					// same plot as its centre).
					mc_applyPlotPredicate(CASC_PRED_IS_OWNED, pPlot, true, szSource);
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
				mc_drainLoadMarks();
				CascadeChannelRegistry::reportChannelCensus();
				break;
			}
			default:
				break;   // events with no deposit reach (name changes, counts, unit lifecycle, load bracket)
			}
		}

	private:
		// THE LOAD DRAIN. Inside the load bracket a mark does NOT rebuild (CvDerivedCache.h: mid-read the state a
		// rebuild would read is half-deserialized -- the context stores, the areas and the plot-group network
		// complete only when the stream ends), so the reseed's marks are BANKED. This drains them once, here.
		// ⛔ It is NOT a blanket rebuild and must never become one ([state-repositories.md] CAPSTONE): every
		// package walked rebuilds ONLY the bits an in-read event actually marked, and a package no event reached
		// stays unbuilt -- visibly wrong, which is how its missing emit gets found. The walk order is
		// irrelevant: a package's rebuild reads its cross-scope inputs through the rebuild-path accessors, so
		// each input rebuilds its own banked marks first.
		static void mc_drainLoadMarks()
		{
			const int iNumPlots = GC.getMap().numPlots();
			for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
			{
				const CvPlot* pPlot = GC.getMap().plotByIndex(iPlotIndex);
				if (pPlot != NULL)
				{
					pPlot->getCascadePackage().rebuildMarked();
				}
			}
			for (int iTeam = 0; iTeam < MAX_TEAMS; ++iTeam)
			{
				const CvTeam& team = GET_TEAM((TeamTypes)iTeam);
				if (team.isAlive())
				{
					team.getCascadePackage().rebuildMarked();
				}
			}
			for (int iPlayer = 0; iPlayer < MAX_PLAYERS; ++iPlayer)
			{
				const CvPlayer& player = GET_PLAYER((PlayerTypes)iPlayer);
				if (!player.isAlive())
				{
					continue;
				}
				for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
				{
					(*cityIterator)->getCascadePackage().rebuildMarked();
				}
				player.getCascadePackage().rebuildMarked();
			}
		}

		// Every rate of the player re-realizes. All-receivers blanket KEPT: the golden-age yield/commerce
		// effect is the OWNER-RULED engine member-mirror (modifier.md §3, the goldenAge carve-out) -- applied
		// at the receiver combine, NOT authored as deposits, so the DepositIndex holds no route to derive
		// (the deposit-authored golden-age data -- IS_GOLDEN_AGE-gated entries, the goldenAge kinds -- routes
		// separately via dependencyForPredicate at the caller). The receiver sums ARE the combine's outputs,
		// so the all-receivers mask IS the honest realization of the mirror's reach.
		static void mc_markAllPlayerSums(const CvPlayer* pPlayer, const char* szSource)
		{
			if (pPlayer == NULL)
			{
				return;
			}
			for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
			{
				(*cityIterator)->getCascadePackage().markMask(CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_CITY));
			}
			pPlayer->getCascadePackage().markMask(CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_EMPIRE));
			mc_invalidate(CASC_SCOPE_EMPIRE, (int)pPlayer->getID(), (int)pPlayer->getID(),
				CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_EMPIRE), szSource);
		}

		// An empire changed composition wholesale (conquest): its empire package + sums re-derive.
		// Whole-scope blankets KEPT: the event is a scope-object COMPOSITION move, not deposit-addressed --
		// the empire's live source multiplicities (owned-building counts, presence sets, per-city scalers)
		// shift at once, and no union of per-source routes can address the DEPARTED side's folded deposits.
		static void mc_markEmpireWhole(const CvPlayer* pPlayer, const char* szSource)
		{
			if (pPlayer == NULL)
			{
				return;
			}
			pPlayer->getCascadePackage().markMask(
				CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_EMPIRE)
				| CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_EMPIRE));
			mc_invalidate(CASC_SCOPE_EMPIRE, (int)pPlayer->getID(), (int)pPlayer->getID(),
				CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_EMPIRE), szSource);
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
