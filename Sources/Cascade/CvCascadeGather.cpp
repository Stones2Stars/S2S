//
//	CascadeGather -- the ONE package-rebuild implementation (see the header). Per-scope refresh bodies: zero
//	the masked slots (contract rule 2), walk the scope's LIVE SOURCES, fold each source's compiled entries at
//	this scope through the shared per-info fold, then rebuild any masked RECEIVER sums through the ONE combine
//	seam (InfoValuation). Conditions evaluate at THIS rebuild cadence (modifier.md §3 -- the dormancy model);
//	reads stay bare fetches.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeGather.h"
#include "CvCascadeAreaSlot.h"
#include "CvCascadePackage.h"
#include "CvCascadeChannelRegistry.h"
#include "CvCascadeCalcCount.h"
#include "CvModifiers.h"
#include "CvModEntry.h"
#include "CvInfoKinds.h"
#include "Data/CvDepositRead.h"           // MMKernel -- applies / audienceOk / perScale (the ONE leaf surface)
#include "Data/CvInfoValuation.h"         // the combine seam (cityRate) + the per-scope group fold
#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx
#include "Enabler/CvEnablerKernel.h"      // wireOperatingBuildings -- the FED-IN active/dormant verdict
#include "Spine/CvEventSpine.h"           // emitCacheRebuilt -- the [CASCADE] rebuilt observability
#include "Defines/CvGlobals.h"
#include "Engine/CvPlot.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Engine/CvArea.h"
#include "AI/CvPlayerAI.h"   // GET_PLAYER
#include "AI/CvTeamAI.h"     // GET_TEAM
#include "CvTerrainInfo.h"
#include "CvFeatureInfo.h"
#include "CvRouteInfo.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvBuildingInfo.h"
#include "CvCivicInfo.h"
#include "CvTraitInfo.h"
#include "CvTechInfo.h"
#include "CvProjectInfo.h"
#include "CvHandicapInfo.h"
#include "CvHeritageInfo.h"
#include "CvReligionInfo.h"
#include "CvCorporationInfo.h"
#include "CvSpecialistInfo.h"

namespace
{
	// The dictionary a unit folds into (the whole type axis -- value vs percent; modifier.md §2). The legacy
	// per-* unit spellings are flat-side (their scaling is the §3.7 resolver's); multiplier units compose by
	// product and never slot (identity on every package channel today -- entry-list evaluated if ever authored).
	bool gt_unitIsFlatSide(CvCascUnit eUnit)
	{
		return eUnit == CASC_UNIT_FLAT
			|| eUnit == CASC_UNIT_COUNT
			|| eUnit == CASC_UNIT_PER_POPULATION
			|| eUnit == CASC_UNIT_PER_SPECIALIST
			|| eUnit == CASC_UNIT_PER_CORPORATION_LEVEL;
	}
	bool gt_unitIsPercentSide(CvCascUnit eUnit)
	{
		return eUnit == CASC_UNIT_PERCENT || eUnit == CASC_UNIT_RAW_PERCENT;
	}

	// Is this family a rate (yield/commerce) channel family? (The specialist §2a carve-out below keys on it.)
	bool gt_isRateFamily(ModifierFamily eFamily)
	{
		return infoFamilyYield(eFamily) >= 0 || infoFamilyCommerce(eFamily) >= 0;
	}

	// Land one applying value in its slot -- wellbeing sign-routes to the twin channel at fill
	// (modifier.md §2b: a negative happiness deposit lands as positive anger; a routing rule, never storage).
	void gt_landValue(CvCascScope eScope, int64_t iWantedBits, int iChannel, bool bPercentSide, long iValue,
		std::vector<int>& flatSlots, std::vector<int>& percentSlots)
	{
		int iLandChannel = iChannel;
		long iLandValue = iValue;
		if (iValue < 0)
		{
			const int iTwin = CascadeChannelRegistry::wellbeingTwin(iChannel);
			if (iTwin >= 0)
			{
				iLandChannel = iTwin;
				iLandValue = -iValue;
			}
		}
		const int iSlot = CascadeChannelRegistry::scopeSlotIndex(eScope, iLandChannel);
		if (iSlot < 0)
		{
			return;
		}
		if ((iWantedBits & CascadeChannelRegistry::scopeChannelBit(eScope, iLandChannel)) == 0)
		{
			return;   // this channel is not being refreshed -- its slot keeps its standing value
		}
		std::vector<int>& slots = bPercentSide ? percentSlots : flatSlots;
		if (iSlot < (int)slots.size())
		{
			slots[iSlot] += (int)iLandValue;
		}
	}

	// Fold ONE source info's compiled entries AT one scope into the package dictionaries. iMultiplier = the
	// source's live multiplicity (owned building count, project count, specialist count -- 1 for presence).
	// pKeyPlot = the plot whose substrate keys/filters plot-scope targeted entries (plot gather only).
	// iPureSign: the PURE_TRAITS value filter (+1 keep >= 0, -1 keep <= 0, 0 unfiltered).
	// bSkipRateChannels: the §2a specialist carve-out -- a specialist's yield/commerce output joins the rate
	// BASE with its own percent layer at the receiver combine, never the city package's flat tier.
	void gt_foldInfo(const CvModifiers* pModifiers, int iMultiplier, CvCascScope eScope, int64_t iWantedBits,
		const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot, int iPureSign, bool bSkipRateChannels,
		std::vector<int>& flatSlots, std::vector<int>& percentSlots)
	{
		if (pModifiers == NULL || pModifiers->empty() || iMultiplier == 0)
		{
			return;
		}
		const int iPlotsSeg = modSegmentLookup("plots");
		const int iImprovementsSeg = modSegmentLookup("improvements");
		const int iTerrainsSeg = modSegmentLookup("terrains");
		const int iFeaturesSeg = modSegmentLookup("features");
		const int iBonusesSeg = modSegmentLookup("bonuses");
		const int iRoutesSeg = modSegmentLookup("routes");
		const std::vector<CvModEntry*>& entries = pModifiers->entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			if (pEntry == NULL || pEntry->scope != eScope)
			{
				continue;
			}
			if (pEntry->unitQual != NULL)
			{
				continue;   // unit-carried values ride ON TOP live ([DEC-unit-modifiers-on-top]) -- never cached
			}
			const bool bPercentSide = gt_unitIsPercentSide(pEntry->unit);
			if (!bPercentSide && !gt_unitIsFlatSide(pEntry->unit))
			{
				continue;
			}
			if (bSkipRateChannels && gt_isRateFamily(pEntry->family))
			{
				continue;
			}
			const int iChannel = CascadeChannelRegistry::channelLookup(pEntry->family,
				(pEntry->family == MODFAM_PROPERTY) ? 0 : pEntry->kind, pEntry->propertyFk);
			if (iChannel < 0)
			{
				continue;   // outside the vocabulary / never a package channel -- drops out with no special-casing
			}
			// TARGETED entries: at the PLOT scope a keyed entry folds iff its key IS this plot's substrate
			// (the engine's improvement/terrain-keyed addends resolve INSIDE the isolated plot package,
			// modifier.md §2); a `plots`-target entry folds iff its per-plot filter holds HERE. At every other
			// scope a targeted entry stays an entry-list read (the reverse pass already landed the
			// building-keyed boosts on their targets; the specialist-keyed trait carve-out reads at the
			// receiver's specialist term).
			if (pEntry->targetSeg >= 0 || pEntry->targetFk >= 0)
			{
				if (eScope != CASC_SCOPE_PLOT || pKeyPlot == NULL)
				{
					continue;
				}
				if (pEntry->targetSeg == iPlotsSeg)
				{
					// falls through: applies() below evaluates the per-plot filter against THIS plot's ctx
				}
				else if (pEntry->targetSeg == iImprovementsSeg)
				{
					if (pEntry->targetFk < 0 || pEntry->targetFk != (int)pKeyPlot->getImprovementType())
					{
						continue;
					}
				}
				else if (pEntry->targetSeg == iTerrainsSeg)
				{
					if (pEntry->targetFk < 0 || pEntry->targetFk != (int)pKeyPlot->getTerrainType())
					{
						continue;
					}
				}
				else if (pEntry->targetSeg == iFeaturesSeg)
				{
					if (pEntry->targetFk < 0 || pEntry->targetFk != (int)pKeyPlot->getFeatureType())
					{
						continue;
					}
				}
				else if (pEntry->targetSeg == iBonusesSeg)
				{
					const TeamTypes eSeeingTeam = (evalCtx.team != NULL) ? evalCtx.team->getID() : NO_TEAM;
					if (pEntry->targetFk < 0 || pEntry->targetFk != (int)pKeyPlot->getBonusType(eSeeingTeam))
					{
						continue;
					}
				}
				else if (pEntry->targetSeg == iRoutesSeg)
				{
					if (pEntry->targetFk < 0 || pEntry->targetFk != (int)pKeyPlot->getRouteType())
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			if (iPureSign > 0 && pEntry->value < 0)
			{
				continue;   // PURE_TRAITS: a positive trait's downside values drop (modifier.md §4)
			}
			if (iPureSign < 0 && pEntry->value > 0)
			{
				continue;   // PURE_TRAITS: a negative trait's upside values drop
			}
			if (!MMKernel::audienceOk(pEntry->aiOnly, evalCtx))
			{
				continue;
			}
			if (!MMKernel::applies(pEntry->enabled, pEntry->disabled, evalCtx))
			{
				continue;   // the conditioned evaluation at rebuild cadence -- the dormancy model (modifier.md §3)
			}
			// the ONE §3.7 resolver applies the per scaler AND the religion: counted-kind filter
			long iValue = MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
			iValue *= iMultiplier;
			gt_landValue(eScope, iWantedBits, iChannel, bPercentSide, iValue, flatSlots, percentSlots);
		}
	}

	// Zero every masked channel slot in both dictionaries (contract rule 2: a refresh fully defines its
	// output; a partial write leaves stale values behind a clean flag) + count the recompute per channel.
	void gt_beginRefill(CvCascScope eScope, int64_t iWantedBits, std::vector<int>& flatSlots, std::vector<int>& percentSlots)
	{
		const int iChannels = CascadeChannelRegistry::scopeChannelCount(eScope);
		for (int iSlot = 0; iSlot < iChannels; ++iSlot)
		{
			const int iChannel = CascadeChannelRegistry::scopeSlotChannel(eScope, iSlot);
			if ((iWantedBits & CascadeChannelRegistry::scopeChannelBit(eScope, iChannel)) == 0)
			{
				continue;
			}
			if (iSlot < (int)flatSlots.size())
			{
				flatSlots[iSlot] = 0;
			}
			if (iSlot < (int)percentSlots.size())
			{
				percentSlots[iSlot] = 0;
			}
			CascadeCalcCount::count(eScope, iChannel);   // [DEC-calc-count-gate]: this (scope, channel) recomputed
		}
	}

	// Fold the EMPIRE-LEVEL source set's entries at eScope (the sources whose deposits can author any spine
	// scope: civics, traits with the PURE gate, held techs, projects, handicap, heritage, presence-gated
	// bonuses/religions/corporations, owned buildings x count). Shared by the empire/team/city/plot gathers --
	// which SCOPE'S entries fold is the eScope parameter; the source SET is the player's.
	void gt_foldPlayerSources(const CvPlayer& player, CvCascScope eScope, int64_t iWantedBits,
		const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot,
		std::vector<int>& flatSlots, std::vector<int>& percentSlots)
	{
		for (int iOption = 0; iOption < GC.getNumCivicOptionInfos(); ++iOption)
		{
			const CivicTypes eCivic = player.getCivics((CivicOptionTypes)iOption);
			if (eCivic != NO_CIVIC)
			{
				gt_foldInfo(GC.getCivicInfo(eCivic).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
			}
		}
		const bool bPureTraits = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
		for (int iTrait = 0; iTrait < GC.getNumTraitInfos(); ++iTrait)
		{
			if (!player.hasTrait((TraitTypes)iTrait))
			{
				continue;
			}
			const CvTraitInfo* pTrait = MMKernel::traitData(iTrait);   // the ACTIVE set (simple/complex by option)
			if (pTrait == NULL)
			{
				continue;
			}
			const int iPureSign = bPureTraits ? (pTrait->isNegativeTrait() ? -1 : 1) : 0;
			gt_foldInfo(pTrait->getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, iPureSign, false, flatSlots, percentSlots);
		}
		const CvTeam& team = GET_TEAM(player.getTeam());
		for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
		{
			if (team.isHasTech((TechTypes)iTech))
			{
				gt_foldInfo(GC.getTechInfo((TechTypes)iTech).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
			}
		}
		for (int iProject = 0; iProject < GC.getNumProjectInfos(); ++iProject)
		{
			const int iCount = team.getProjectCount((ProjectTypes)iProject);
			if (iCount > 0)
			{
				gt_foldInfo(GC.getProjectInfo((ProjectTypes)iProject).getModifiers(), iCount, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
			}
		}
		if (player.getHandicapType() != NO_HANDICAP)
		{
			gt_foldInfo(GC.getHandicapInfo(player.getHandicapType()).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
		}
		for (int iHeritage = 0; iHeritage < GC.getNumHeritageInfos(); ++iHeritage)
		{
			if (player.hasHeritage((HeritageTypes)iHeritage))
			{
				gt_foldInfo(GC.getHeritageInfo((HeritageTypes)iHeritage).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
			}
		}
		// presence-gated source classes: their entries carry their own presence conditions -- the fold's
		// ordinary gate evaluation keeps an absent source silent (no pre-filter walk needed here).
		for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); ++iBonus)
		{
			gt_foldInfo(GC.getBonusInfo((BonusTypes)iBonus).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
		}
		for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
		{
			gt_foldInfo(GC.getReligionInfo((ReligionTypes)iReligion).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
		}
		for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation)
		{
			gt_foldInfo(GC.getCorporationInfo((CorporationTypes)iCorporation).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
		}
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			const int iOwned = player.getBuildingCount((BuildingTypes)iBuilding);
			if (iOwned > 0)
			{
				gt_foldInfo(GC.getBuildingInfo((BuildingTypes)iBuilding).getModifiers(), iOwned, eScope, iWantedBits, evalCtx, pKeyPlot, 0, false, flatSlots, percentSlots);
			}
		}
	}
}

// The area slot's refresh delegate (declared in CvCascadeAreaSlot.h).
void CvCascadeAreaSlot::refreshCascadePackage(int64_t iMask) const
{
	if (area != NULL && player != NO_PLAYER)
	{
		CascadeGather::refreshArea(*area, player, iMask);
	}
}

void CascadeGather::refreshPlot(const CvPlot& plot, int64_t iMask)
{
	const CvCascadePackage<CvPlot>& package = plot.getCascadePackage();
	package.ensureSized();
	gt_beginRefill(CASC_SCOPE_PLOT, iMask, package.flat, package.percent);

	CvCascadeEvalCtx evalCtx;
	evalCtx.plot = &plot;
	const PlayerTypes eOwner = plot.getOwner();
	if (eOwner != NO_PLAYER)
	{
		evalCtx.player = &GET_PLAYER(eOwner);
		evalCtx.team = &GET_TEAM(evalCtx.player->getTeam());
	}
	const CvCity* pWorkingCity = plot.getWorkingCity();
	if (pWorkingCity != NULL)
	{
		evalCtx.city = pWorkingCity;
		EnablerKernel::wireOperatingBuildings(pWorkingCity, evalCtx);
	}

	// (1) the plot's own substrate -- each entity's own-output plot deposits (terrain/feature/route/
	// improvement/resource; the isolated per-plot base package, modifier.md §2)
	if (plot.getTerrainType() != NO_TERRAIN)
	{
		gt_foldInfo(GC.getTerrainInfo(plot.getTerrainType()).getModifiers(), 1, CASC_SCOPE_PLOT, iMask, evalCtx, &plot, 0, false, package.flat, package.percent);
	}
	if (plot.getFeatureType() != NO_FEATURE)
	{
		gt_foldInfo(GC.getFeatureInfo(plot.getFeatureType()).getModifiers(), 1, CASC_SCOPE_PLOT, iMask, evalCtx, &plot, 0, false, package.flat, package.percent);
	}
	if (plot.getRouteType() != NO_ROUTE)
	{
		gt_foldInfo(GC.getRouteInfo(plot.getRouteType()).getModifiers(), 1, CASC_SCOPE_PLOT, iMask, evalCtx, &plot, 0, false, package.flat, package.percent);
	}
	if (plot.getImprovementType() != NO_IMPROVEMENT)
	{
		gt_foldInfo(GC.getImprovementInfo(plot.getImprovementType()).getModifiers(), 1, CASC_SCOPE_PLOT, iMask, evalCtx, &plot, 0, false, package.flat, package.percent);
	}
	const TeamTypes eSeeingTeam = (evalCtx.team != NULL) ? evalCtx.team->getID() : NO_TEAM;
	if (plot.getBonusType(eSeeingTeam) != NO_BONUS)
	{
		gt_foldInfo(GC.getBonusInfo(plot.getBonusType(eSeeingTeam)).getModifiers(), 1, CASC_SCOPE_PLOT, iMask, evalCtx, &plot, 0, false, package.flat, package.percent);
	}

	// (2) the owner's sources' PLOT-scope deposits (keyed by this plot's substrate / `plots`-target / bare
	// plot flats) -- civics, traits, techs, owned buildings, ... (the keyed/plots flats of the plot base)
	if (eOwner != NO_PLAYER)
	{
		gt_foldPlayerSources(GET_PLAYER(eOwner), CASC_SCOPE_PLOT, iMask, evalCtx, &plot, package.flat, package.percent);
	}

	emitCacheRebuilt((int)CASC_SCOPE_PLOT, (int)eOwner, plot.getX() + plot.getY() * GC.getMap().getGridWidth(), iMask);
}

void CascadeGather::refreshCity(const CvCity& city, int64_t iMask)
{
	const CvCascadePackage<CvCity>& package = city.getCascadePackage();
	package.ensureSized();
	const int64_t iChannelBits = iMask & CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_CITY);
	const int64_t iReceiverBits = iMask & CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_CITY);

	const CvPlayer& owner = GET_PLAYER(city.getOwner());
	CvCascadeEvalCtx evalCtx;
	// NULL plotGroup: a city-bound ctx answers connection:"trade" through the city's own plot-group-backed reads
	InfoValuation::fillEvalCtx(city.getCityContext(), owner.getEmpireContext(), NULL, evalCtx);

	if (iChannelBits != 0)
	{
		gt_beginRefill(CASC_SCOPE_CITY, iChannelBits, package.flat, package.percent);

		// (1) the city's ACTIVE buildings (the enabler's operating verdict, FED IN -- a dormant building
		// deposits nothing; an obsolete one deposits its whenObsolete tree in place of its normal families)
		if (evalCtx.activeBuildings != NULL)
		{
			for (std::set<int>::const_iterator it = evalCtx.activeBuildings->begin(); it != evalCtx.activeBuildings->end(); ++it)
			{
				const CvBuildingInfo& building = GC.getBuildingInfo((BuildingTypes)(*it));
				const bool bObsolete = cascadeIsBuildingObsolete(*it, evalCtx);
				gt_foldInfo(bObsolete ? building.getWhenObsolete() : building.getModifiers(),
					1, CASC_SCOPE_CITY, iChannelBits, evalCtx, NULL, 0, false, package.flat, package.percent);
			}
		}
		// (2) assigned specialists x count -- EXCLUDING the rate channels (§2a: a specialist's yield/commerce
		// output joins the rate BASE with its own percent layer at the receiver combine, never the flat tier)
		for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
		{
			const int iCount = city.getSpecialistCount((SpecialistTypes)iSpecialist);
			if (iCount > 0)
			{
				gt_foldInfo(GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(),
					iCount, CASC_SCOPE_CITY, iChannelBits, evalCtx, NULL, 0, true, package.flat, package.percent);
			}
		}
		// (3) the empire-level source set's CITY-scope deposits (a civic's per-city flats, presence-gated
		// religion/corporation city entries -- the presence gates evaluate against THIS city's ctx)
		gt_foldPlayerSources(owner, CASC_SCOPE_CITY, iChannelBits, evalCtx, NULL, package.flat, package.percent);
	}

	// the RECEIVER sums -- the realized totals this city consumes, combined from the ~5 scope packages
	// through the ONE combine seam; each package read chains its own lazy dirty-check.
	if (iReceiverBits != 0)
	{
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_CITY);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_CITY, iReceiver);
			if ((iReceiverBits & CascadeChannelRegistry::scopeReceiverBit(CASC_SCOPE_CITY, iChannel)) == 0)
			{
				continue;
			}
			if ((int)package.sum.size() > iReceiver)
			{
				package.slotSum(iReceiver) = (int)rebuildCityChannelSum(city, iChannel, evalCtx);
			}
			CascadeCalcCount::countSum(CASC_SCOPE_CITY, iChannel);
		}
	}

	emitCacheRebuilt((int)CASC_SCOPE_CITY, (int)city.getOwner(), city.getID(), iMask);
}

void CascadeGather::refreshEmpire(const CvPlayer& player, int64_t iMask)
{
	const CvCascadePackage<CvPlayer>& package = player.getCascadePackage();
	package.ensureSized();
	const int64_t iChannelBits = iMask & CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_EMPIRE);
	const int64_t iReceiverBits = iMask & CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_EMPIRE);

	CvCascadeEvalCtx evalCtx;
	player.getEmpireContext().fillEvalCtx(evalCtx);   // player + team (no city at empire scope)

	if (iChannelBits != 0)
	{
		gt_beginRefill(CASC_SCOPE_EMPIRE, iChannelBits, package.flat, package.percent);
		gt_foldPlayerSources(player, CASC_SCOPE_EMPIRE, iChannelBits, evalCtx, NULL, package.flat, package.percent);
	}

	// the empire RECEIVER sums (gold/research/culture/espionage): Sigma over the player's cities' realized
	// sums -- each city's slot chains its own lazy rebuild (no dependency-ordered pass).
	if (iReceiverBits != 0)
	{
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_EMPIRE);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_EMPIRE, iReceiver);
			if ((iReceiverBits & CascadeChannelRegistry::scopeReceiverBit(CASC_SCOPE_EMPIRE, iChannel)) == 0)
			{
				continue;
			}
			long iTotal = 0;
			for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
			{
				const CvCity* pLoopCity = *cityIterator;
				// a channel the city ALSO receives (culture, the dual-consumer) reads its cached city sum (its
				// own lazy rebuild chains); an empire-only channel (gold/research/espionage) combines per city
				// HERE through the same ONE combine -- the city stores no sum it does not consume (the
				// receiving scope is the storing scope of its OWN total only).
				if (CascadeChannelRegistry::scopeReceiverIndex(CASC_SCOPE_CITY, iChannel) >= 0)
				{
					iTotal += pLoopCity->getCascadePackage().readSum(iChannel);
				}
				else
				{
					CvCascadeEvalCtx cityEvalCtx;
					InfoValuation::fillEvalCtx(pLoopCity->getCityContext(), player.getEmpireContext(), NULL, cityEvalCtx);
					iTotal += rebuildCityChannelSum(*pLoopCity, iChannel, cityEvalCtx);
				}
			}
			if ((int)package.sum.size() > iReceiver)
			{
				package.slotSum(iReceiver) = (int)iTotal;
			}
			CascadeCalcCount::countSum(CASC_SCOPE_EMPIRE, iChannel);
		}
	}

	emitCacheRebuilt((int)CASC_SCOPE_EMPIRE, (int)player.getID(), (int)player.getID(), iMask);
}

void CascadeGather::refreshTeam(const CvTeam& team, int64_t iMask)
{
	const CvCascadePackage<CvTeam>& package = team.getCascadePackage();
	package.ensureSized();
	gt_beginRefill(CASC_SCOPE_TEAM, iMask, package.flat, package.percent);

	CvCascadeEvalCtx evalCtx;
	evalCtx.team = &team;
	if (team.getLeaderID() != NO_PLAYER)
	{
		evalCtx.player = &GET_PLAYER(team.getLeaderID());
	}

	// team-scope deposits: projects (team-held, x count) + held techs
	for (int iProject = 0; iProject < GC.getNumProjectInfos(); ++iProject)
	{
		const int iCount = team.getProjectCount((ProjectTypes)iProject);
		if (iCount > 0)
		{
			gt_foldInfo(GC.getProjectInfo((ProjectTypes)iProject).getModifiers(), iCount, CASC_SCOPE_TEAM, iMask, evalCtx, NULL, 0, false, package.flat, package.percent);
		}
	}
	for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
	{
		if (team.isHasTech((TechTypes)iTech))
		{
			gt_foldInfo(GC.getTechInfo((TechTypes)iTech).getModifiers(), 1, CASC_SCOPE_TEAM, iMask, evalCtx, NULL, 0, false, package.flat, package.percent);
		}
	}

	emitCacheRebuilt((int)CASC_SCOPE_TEAM, (int)team.getLeaderID(), (int)team.getID(), iMask);
}

void CascadeGather::refreshArea(const CvArea& area, PlayerTypes ePlayer, int64_t iMask)
{
	if (ePlayer == NO_PLAYER)
	{
		return;
	}
	const CvCascadeAreaSlot& slot = area.getCascadeSlot(ePlayer);
	const CvCascadePackage<CvCascadeAreaSlot>& package = slot.package;
	package.ensureSized();
	gt_beginRefill(CASC_SCOPE_AREA, iMask, package.flat, package.percent);

	// area-scope deposits realize per (area x player): the player's ACTIVE buildings in cities of this area
	const CvPlayer& player = GET_PLAYER(ePlayer);
	for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
	{
		const CvCity* pLoopCity = *cityIterator;
		if (pLoopCity == NULL || pLoopCity->area() != &area)
		{
			continue;
		}
		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtx(pLoopCity->getCityContext(), player.getEmpireContext(), NULL, evalCtx);
		if (evalCtx.activeBuildings == NULL)
		{
			continue;
		}
		for (std::set<int>::const_iterator it = evalCtx.activeBuildings->begin(); it != evalCtx.activeBuildings->end(); ++it)
		{
			const CvBuildingInfo& building = GC.getBuildingInfo((BuildingTypes)(*it));
			const bool bObsolete = cascadeIsBuildingObsolete(*it, evalCtx);
			gt_foldInfo(bObsolete ? building.getWhenObsolete() : building.getModifiers(),
				1, CASC_SCOPE_AREA, iMask, evalCtx, NULL, 0, false, package.flat, package.percent);
		}
	}

	emitCacheRebuilt((int)CASC_SCOPE_AREA, (int)ePlayer, area.getID(), iMask);
}

// The city RECEIVER combine -- one realized channel total from the ~5 scope packages (modifier.md §1: "the
// only live calculation is adding the packages together at read"), through the ONE §2a rate shape
// (InfoValuation::cityRate). The worked-plot base is the Sigma of the worked plots' OWN isolated base
// packages (the plot packages ARE the per-plot base cache -- summed here at RECEIVER-REBUILD cadence, never
// per read). Defined last so the per-scope refreshes above read as the file's spine.
long CascadeGather::rebuildCityChannelSum(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx)
{
	const CvPlayer& owner = GET_PLAYER(city.getOwner());
	const CvTeam& team = GET_TEAM(owner.getTeam());
	const CvArea* pArea = city.area();

	// TIER 1 BASE -- the worked-plot Sigma + the upper-scope flats rolled down at the combine (§2a)
	long iBase = 0;
	const int iNumPlots = city.getNumCityPlots();
	for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
	{
		if (!city.isWorkingPlot(iPlotIndex))
		{
			continue;
		}
		const CvPlot* pWorkedPlot = city.getCityIndexPlot(iPlotIndex);
		if (pWorkedPlot != NULL)
		{
			iBase += pWorkedPlot->getCascadePackage().readFlat(iChannel);
		}
	}
	// the EMPIRE/TEAM/AREA flats are genuine §2a BASE terms rolled down at the combine (modifier.md §2a):
	// the trait free-city yield ({ch}.empire.flat) and, for the commerce channels, the baseExtra classes
	// (civic/heritage player-extra, empire-scope building grants) join the base the percent stack scales --
	// distinct from the CITY package's flat tier, which is the post-percent EXTRA below. No shipped data
	// authors a TEAM-scope rate flat (team carries combat/diplomacy families only), so the team read is the
	// uniform-package shape's headroom, not a live term.
	iBase += owner.getCascadePackage().readFlat(iChannel);
	iBase += team.getCascadePackage().readFlat(iChannel);
	if (pArea != NULL)
	{
		iBase += pArea->getCascadeSlot(city.getOwner()).package.readFlat(iChannel);
	}

	// the specialist term -- each assigned specialist's own output, folded per §2a with the city sources'
	// per-specialist boosts riding its conditioned entries (the ONE per-scope group fold)
	long iSpecialists = 0;
	const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
	const int iKind = CascadeChannelRegistry::channelKind(iChannel);
	for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
	{
		const int iCount = city.getSpecialistCount((SpecialistTypes)iSpecialist);
		if (iCount > 0)
		{
			iSpecialists += iCount * InfoValuation::groupSumAt(
				GC.getSpecialistInfo((SpecialistTypes)iSpecialist).getModifiers(),
				eFamily, iKind, CASC_UNIT_FLAT, CASC_SCOPE_CITY, evalCtx);
		}
	}

	// the ONE additive percent stack -- the scope packages' percent slots summed (x100 storage -> human).
	// PLOT percents are deliberately ABSENT: a per-plot percentage applies INSIDE the isolated plot calc,
	// never the city stack (modifier.md §2 plot-as-base). No shipped deposit authors a yield/commerce plot
	// percent; the authored plot percents (health/defense/property) belong to their own combines, not here.
	long iPercentSum = 0;
	iPercentSum += city.getCascadePackage().readPercent(iChannel);
	iPercentSum += owner.getCascadePackage().readPercent(iChannel);
	iPercentSum += team.getCascadePackage().readPercent(iChannel);
	if (pArea != NULL)
	{
		iPercentSum += pArea->getCascadeSlot(city.getOwner()).package.readPercent(iChannel);
	}

	// TIER 2 EXTRA -- the city package's flat tier (building flats), added AFTER the percentages (§2a)
	const long iExtra = city.getCascadePackage().readFlat(iChannel);

	return InfoValuation::cityRate(iBase, iSpecialists, (int)(iPercentSum / 100), iExtra);
}
