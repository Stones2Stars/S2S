//
//	CvModifierConsumer -- the modifier cascade's own spine consumer (see the header). The switch below names
//	only the event's SEMANTICS (which repo its iType indexes; which state a stateful event embodies); every
//	MASK is derived from the deposit index -- routeFor for a source-carrying event, the condition-dependency
//	routes for a state flip a deposit's gate reads. ONE application helper marks packages + receiver sums.
//	The ONLY non-derived masks are the RULED blankets, each carrying its justifying constraint at the case:
//	scope-object lifecycle/composition events (city founded / ownership moves) and the golden-age engine
//	member-mirror are not deposit-addressed, so no route exists in the index to derive.
//

#include "CvGameCoreDLL.h"
#include "CvModifierConsumer.h"
#include "CvCascadeCalcCount.h"
#include "CvCascadePackage.h"
#include "CvCascadeAreaSlot.h"
#include "CvCascadeChannelRegistry.h"
#include "Data/CvDepositIndex.h"        // routeFor + the dependency routes -- the ONE mark derivation
#include "Data/CvDepositRead.h"         // MMKernel::traitData -- the active trait set's info
#include "Spine/CvEventSpine.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvMap.h"
#include "Engine/CvPlot.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvArea.h"
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

	void mc_markAreaSlot(const CvArea* pArea, PlayerTypes ePlayer, int64_t iMask, const char* szSource)
	{
		if (pArea == NULL || ePlayer == NO_PLAYER || iMask == 0)
		{
			return;
		}
		pArea->getCascadeSlot(ePlayer).package.markMask(iMask);
		mc_invalidate(CASC_SCOPE_AREA, (int)ePlayer, pArea->getID(), iMask, szSource);
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
		// AREA slots: the event's own area, else every area the owner has a city in
		const int64_t iAreaMask = pRoute->packageMask[(int)CASC_SCOPE_AREA];
		if (iAreaMask != 0 && pPlayer != NULL)
		{
			if (pCity != NULL)
			{
				mc_markAreaSlot(pCity->area(), pPlayer->getID(), iAreaMask, szSource);
			}
			else if (pPlot != NULL)
			{
				mc_markAreaSlot(pPlot->area(), pPlayer->getID(), iAreaMask, szSource);
			}
			else
			{
				for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
				{
					mc_markAreaSlot((*cityIterator)->area(), pPlayer->getID(), iAreaMask, szSource);
				}
			}
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
				}
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
				}
				break;
			}
			// ---- the turn boundary: flush the calc-count report (game scope only). NOT a self-heal site --
			// ---- nothing is marked here ([DEC-no-self-heal]). ----
			case SEVT_TURN_ENDED:
			{
				if (kEvent.iC == -1)
				{
					CascadeCalcCount::reportChannelCensus();   // once-guarded (covers the no-load new-game path)
					CascadeCalcCount::reportAndReset();
				}
				break;
			}
			// ---- the load bracket end: the channel-set census (the layouts are complete) ----
			case SEVT_GAME_LOAD_FINISHED:
			{
				CascadeCalcCount::reportChannelCensus();
				break;
			}
			default:
				break;   // events with no deposit reach (name changes, counts, unit lifecycle, load bracket)
			}
		}

	private:
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

		// An empire changed composition wholesale (conquest): its empire package + sums + areas re-derive.
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
			for (CvPlayer::city_iterator cityIterator = pPlayer->beginCities(); cityIterator != pPlayer->endCities(); ++cityIterator)
			{
				mc_markAreaSlot((*cityIterator)->area(), pPlayer->getID(),
					CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_AREA), szSource);
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
