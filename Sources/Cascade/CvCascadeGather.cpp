//
//	CascadeGather -- the ONE package-rebuild implementation (see the header). Per-scope refresh bodies: zero
//	the masked slots (contract rule 2), walk the scope's LIVE SOURCES, fold each source's compiled entries at
//	this scope through the shared per-info fold, then rebuild any masked RECEIVER sums through the ONE combine
//	seam (InfoValuation). Conditions evaluate at THIS rebuild cadence (modifier.md §3 -- the dormancy model);
//	reads stay bare fetches.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeGather.h"
#include "CvCascadePackage.h"
#include "CvCascadeChannelRegistry.h"
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
#include "CvProcessInfo.h"   // getProductionToCommerce -- the commerce split's EXTRA-tier conversion rate

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
	void gt_landValue(CvCascScope eScope, int64_t iWantedBits, int iChannel, bool bPercentSide, int64_t iValue,
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		int iLandChannel = iChannel;
		int64_t iLandValue = iValue;
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
		// ⛔ The two dictionaries no longer share a WIDTH (CvCascadePackage.h: width is per UNIT -- an amount
		// accumulates across sources and scopes at ×100, a percent is a small whole number that does not), so
		// the landing branches instead of selecting one reference across both. ⚑ The `(int)` cast that used to
		// sit on this line was the truncation: every accumulated amount was narrowed to 32 bits on its way
		// INTO storage, so widening the slot alone would have changed nothing.
		if (bPercentSide)
		{
			if (iSlot < (int)percentSlots.size())
			{
				percentSlots[iSlot] += (int)iLandValue;
			}
		}
		else if (iSlot < (int)flatSlots.size())
		{
			flatSlots[iSlot] += iLandValue;
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
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
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
		const int iEmpiresSeg = modSegmentLookup("empires");
		const std::vector<CvModEntry*>& entries = pModifiers->entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			if (pEntry == NULL)
			{
				continue;
			}
			// json §3.3's `empires` plural target: a WORLD-scope deposit onto EVERY empire lands in each
			// PLAYER's package, because WORLD is CONFIG and carries no package of its own
			// (state-repositories.md -- "a project granting something to every player is NOT world-scope
			// state"). So the empire gather accepts it even though the authored scope is world.
			const bool bWorldEmpires = (eScope == CASC_SCOPE_EMPIRE
				&& pEntry->scope == CASC_SCOPE_WORLD
				&& iEmpiresSeg >= 0
				&& pEntry->targetSeg == iEmpiresSeg);
			if (pEntry->scope != eScope && !bWorldEmpires)
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
				// The `empires` fan is the ONE targeted entry that folds outside plot scope: its target IS
				// the gathering owner, so landing it here is the deposit, not a keyed lookup.
				if (!bWorldEmpires && (eScope != CASC_SCOPE_PLOT || pKeyPlot == NULL))
				{
					continue;
				}
				if (bWorldEmpires)
				{
					// falls through: the fan IS the fold, no per-target test applies
				}
				else if (pEntry->targetSeg == iPlotsSeg)
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
			int64_t iValue = MMKernel::perScale(*pEntry, evalCtx, pEntry->value);
			iValue *= iMultiplier;
			gt_landValue(eScope, iWantedBits, iChannel, bPercentSide, iValue, flatSlots, percentSlots);
		}
	}

	// Zero every masked channel slot in both dictionaries (contract rule 2: a refresh fully defines its
	// output; a partial write leaves stale values behind a clean flag).
	void gt_beginRefill(CvCascScope eScope, int64_t iWantedBits, std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
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
		}
	}

	// Fold the EMPIRE-LEVEL source set's entries at eScope (the sources whose deposits can author any spine
	// scope: civics, traits with the PURE gate, held techs, projects, handicap, heritage, presence-gated
	// bonuses/religions/corporations, owned buildings x count). Shared by the empire/team/city/plot gathers --
	// which SCOPE'S entries fold is the eScope parameter; the source SET is the player's.
	void gt_foldPlayerSources(const CvPlayer& player, CvCascScope eScope, int64_t iWantedBits,
		const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot,
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
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

	// ---- THE PER-SCOPE CHANNEL FOLDS -- the ONE implementation, writing into WHATEVER dictionaries the caller
	// ---- hands them ([DEC-single-implementation]). The rebuild path passes the owner's own package storage;
	// ---- the oracle passes a caller-owned document. The rebuilt announcement does not live here: it belongs to
	// ---- the rebuild, and the oracle rebuilds nothing.
	// ---- ⚑ NOT ONE OF THESE READS A CASCADE PACKAGE. A channel fold's inputs are the compiled deposits plus
	// ---- LIVE GAME STATE (held techs, owned buildings, assigned specialists, the plot's substrate), so it is a
	// ---- pure function of the game and terminates unconditionally. Only the RECEIVER SUM legs below consume
	// ---- other scopes -- which is what bounds the oracle's recursion (see gt_freshEmpireDocument). ----

	void gt_gatherPlotChannels(const CvPlot& plot, int64_t iWantedBits, std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		gt_beginRefill(CASC_SCOPE_PLOT, iWantedBits, flatSlots, percentSlots);

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
			gt_foldInfo(GC.getTerrainInfo(plot.getTerrainType()).getModifiers(), 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, 0, false, flatSlots, percentSlots);
		}
		if (plot.getFeatureType() != NO_FEATURE)
		{
			gt_foldInfo(GC.getFeatureInfo(plot.getFeatureType()).getModifiers(), 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, 0, false, flatSlots, percentSlots);
		}
		if (plot.getRouteType() != NO_ROUTE)
		{
			gt_foldInfo(GC.getRouteInfo(plot.getRouteType()).getModifiers(), 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, 0, false, flatSlots, percentSlots);
		}
		if (plot.getImprovementType() != NO_IMPROVEMENT)
		{
			gt_foldInfo(GC.getImprovementInfo(plot.getImprovementType()).getModifiers(), 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, 0, false, flatSlots, percentSlots);
		}
		const TeamTypes eSeeingTeam = (evalCtx.team != NULL) ? evalCtx.team->getID() : NO_TEAM;
		if (plot.getBonusType(eSeeingTeam) != NO_BONUS)
		{
			gt_foldInfo(GC.getBonusInfo(plot.getBonusType(eSeeingTeam)).getModifiers(), 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, 0, false, flatSlots, percentSlots);
		}

		// (2) the owner's sources' PLOT-scope deposits (keyed by this plot's substrate / `plots`-target / bare
		// plot flats) -- civics, traits, techs, owned buildings, ... (the keyed/plots flats of the plot base)
		if (eOwner != NO_PLAYER)
		{
			gt_foldPlayerSources(GET_PLAYER(eOwner), CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, flatSlots, percentSlots);
		}
	}

	// The city-bound eval ctx both legs need -- built ONCE by the composite cores below and handed down.
	// NULL plotGroup: a city-bound ctx answers connection:"trade" through the city's own plot-group-backed reads.
	void gt_fillCityEvalCtx(const CvCity& city, CvCascadeEvalCtx& evalCtx)
	{
		const CvPlayer& owner = GET_PLAYER(city.getOwner());
		InfoValuation::fillEvalCtx(city.getCityContext(), owner.getEmpireContext(), NULL, evalCtx);
	}

	void gt_gatherCityChannels(const CvCity& city, int64_t iWantedBits, const CvCascadeEvalCtx& evalCtx,
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		gt_beginRefill(CASC_SCOPE_CITY, iWantedBits, flatSlots, percentSlots);

		// (1) the city's ACTIVE buildings (the enabler's operating verdict, FED IN -- a dormant building
		// deposits nothing; an obsolete one deposits its whenObsolete tree in place of its normal families)
		if (evalCtx.activeBuildings != NULL)
		{
			for (std::set<int>::const_iterator it = evalCtx.activeBuildings->begin(); it != evalCtx.activeBuildings->end(); ++it)
			{
				const CvBuildingInfo& building = GC.getBuildingInfo((BuildingTypes)(*it));
				const bool bObsolete = cascadeIsBuildingObsolete(*it, evalCtx);
				gt_foldInfo(bObsolete ? building.getWhenObsolete() : building.getModifiers(),
					1, CASC_SCOPE_CITY, iWantedBits, evalCtx, NULL, 0, false, flatSlots, percentSlots);
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
					iCount, CASC_SCOPE_CITY, iWantedBits, evalCtx, NULL, 0, true, flatSlots, percentSlots);
			}
		}
		// (3) the empire-level source set's CITY-scope deposits (a civic's per-city flats, presence-gated
		// religion/corporation city entries -- the presence gates evaluate against THIS city's ctx)
		gt_foldPlayerSources(GET_PLAYER(city.getOwner()), CASC_SCOPE_CITY, iWantedBits, evalCtx, NULL, flatSlots, percentSlots);
	}

	void gt_gatherEmpireChannels(const CvPlayer& player, int64_t iWantedBits, const CvCascadeEvalCtx& evalCtx,
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		gt_beginRefill(CASC_SCOPE_EMPIRE, iWantedBits, flatSlots, percentSlots);
		gt_foldPlayerSources(player, CASC_SCOPE_EMPIRE, iWantedBits, evalCtx, NULL, flatSlots, percentSlots);
	}

	void gt_gatherTeamChannels(const CvTeam& team, int64_t iWantedBits, std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		gt_beginRefill(CASC_SCOPE_TEAM, iWantedBits, flatSlots, percentSlots);

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
				gt_foldInfo(GC.getProjectInfo((ProjectTypes)iProject).getModifiers(), iCount, CASC_SCOPE_TEAM, iWantedBits, evalCtx, NULL, 0, false, flatSlots, percentSlots);
			}
		}
		for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
		{
			if (team.isHasTech((TechTypes)iTech))
			{
				gt_foldInfo(GC.getTechInfo((TechTypes)iTech).getModifiers(), 1, CASC_SCOPE_TEAM, iWantedBits, evalCtx, NULL, 0, false, flatSlots, percentSlots);
			}
		}
	}

	// ---- THE ORACLE'S FROM-SOURCE INPUT DOCUMENTS. Each recomputes one scope's WHOLE CHANNEL SET into a fresh
	// ---- document; NONE of them carries receiver sums, and that is the RECURSION BOUND: a channel fold reads
	// ---- only live game state, so it cannot ask for another scope, and the sum legs -- the only consumers of
	// ---- other scopes -- are reached from here NEVER. The oracle's call graph is therefore a three-level DAG
	// ---- (empire sums -> per-city realized rate -> channel documents) that cannot re-enter itself. ----

	void gt_freshPlotDocument(const CvPlot& plot, CvCascadeSlotValues& kDocument)
	{
		kDocument.reset(CASC_SCOPE_PLOT, plot.getX(), plot.getY());
		gt_gatherPlotChannels(plot, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_PLOT),
			kDocument.flat, kDocument.percent);
	}

	void gt_freshCityDocument(const CvCity& city, const CvCascadeEvalCtx& evalCtx, CvCascadeSlotValues& kDocument)
	{
		kDocument.reset(CASC_SCOPE_CITY, (int)city.getOwner(), city.getID());
		gt_gatherCityChannels(city, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_CITY), evalCtx,
			kDocument.flat, kDocument.percent);
	}

	void gt_freshEmpireDocument(const CvPlayer& player, CvCascadeSlotValues& kDocument)
	{
		kDocument.reset(CASC_SCOPE_EMPIRE, (int)player.getID(), -1);
		CvCascadeEvalCtx evalCtx;
		player.getEmpireContext().fillEvalCtx(evalCtx);
		gt_gatherEmpireChannels(player, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_EMPIRE), evalCtx,
			kDocument.flat, kDocument.percent);
	}

	void gt_freshTeamDocument(const CvTeam& team, CvCascadeSlotValues& kDocument)
	{
		kDocument.reset(CASC_SCOPE_TEAM, -1, (int)team.getID());
		gt_gatherTeamChannels(team, CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_TEAM),
			kDocument.flat, kDocument.percent);
	}

	// Every scope package one city's realized totals consume, recomputed FROM SOURCE. Collected ONCE per city
	// per oracle run and read for every channel: these documents are pure functions of the live game and no
	// game state changes inside one synchronous oracle call, so building them once is the SAME full recompute,
	// not a memoization that trades correctness for cost.
	struct CascadeOracleCityInputs
	{
		CvCascadeSlotValues empire;
		CvCascadeSlotValues team;
		std::vector<CvCascadeSlotValues> workedPlots;
	};

	void gt_collectFreshCityInputs(const CvCity& city, CascadeOracleCityInputs& kInputs)
	{
		const CvPlayer& owner = GET_PLAYER(city.getOwner());
		gt_freshEmpireDocument(owner, kInputs.empire);
		gt_freshTeamDocument(GET_TEAM(owner.getTeam()), kInputs.team);
		kInputs.workedPlots.clear();
		const int iNumPlots = city.getNumCityPlots();
		for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
		{
			if (!city.isWorkingPlot(iPlotIndex))
			{
				continue;
			}
			const CvPlot* pWorkedPlot = city.getCityIndexPlot(iPlotIndex);
			if (pWorkedPlot == NULL)
			{
				continue;
			}
			CvCascadeSlotValues kPlotDocument;
			gt_freshPlotDocument(*pWorkedPlot, kPlotDocument);
			kInputs.workedPlots.push_back(kPlotDocument);
		}
	}

	// The city receiver combine's INPUT TERMS for one channel. The two paths collect them from genuinely
	// different places -- the REBUILD reads the stored packages through their marks, the ORACLE reads freshly
	// recomputed documents -- while the MATH stays in one place below ([DEC-single-implementation]).
	//
	// TIER 1 `baseFlat` is the worked-plot Sigma (the plot packages ARE the per-plot base cache) PLUS the
	// EMPIRE/TEAM/AREA flats, which are genuine §2a BASE terms rolled down at the combine: the trait free-city
	// yield ({ch}.empire.flat) and, for the commerce channels, the baseExtra classes (civic/heritage
	// player-extra, empire-scope building grants) join the base the percent stack scales -- distinct from the
	// CITY package's flat tier, which is the post-percent EXTRA (`cityFlat`). No shipped data authors a
	// TEAM-scope rate flat (team carries combat/diplomacy families only), so the team term is the uniform
	// package shape's headroom rather than a live value.
	// `percentSum` is the ONE additive percent stack. PLOT percents are deliberately ABSENT from it: a per-plot
	// percentage applies INSIDE the isolated plot calc, never the city stack (modifier.md §2 plot-as-base). No
	// shipped deposit authors a yield/commerce plot percent, and the authored plot percents (health / defense /
	// property) belong to their own combines.
	struct CascadeCityCombineTerms
	{
		int64_t baseFlat;     // TIER 1: the worked-plot Sigma + the empire/team flats rolled down (modifier.md §2a)
		int64_t percentSum;   // the ONE additive percent stack (PLAIN percents): city + empire + team
		int64_t cityFlat;     // TIER 2: the city package's flat tier, added AFTER the percentages

		CascadeCityCombineTerms() : baseFlat(0), percentSum(0), cityFlat(0) {}
	};

	// The REBUILD path's terms: read through each input's own mark, so an input the same event marked rebuilds
	// before it is summed and the mark ORDER within one event cannot leave a sum stale.
	void gt_collectStoredCityTerms(const CvCity& city, int iChannel, CascadeCityCombineTerms& kTerms)
	{
		const CvPlayer& owner = GET_PLAYER(city.getOwner());
		const CvTeam& team = GET_TEAM(owner.getTeam());

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
				kTerms.baseFlat += pWorkedPlot->getCascadePackage().sourceFlat(iChannel);
			}
		}
		kTerms.baseFlat += owner.getCascadePackage().sourceFlat(iChannel);
		kTerms.baseFlat += team.getCascadePackage().sourceFlat(iChannel);
		kTerms.percentSum += city.getCascadePackage().sourcePercent(iChannel);
		kTerms.percentSum += owner.getCascadePackage().sourcePercent(iChannel);
		kTerms.percentSum += team.getCascadePackage().sourcePercent(iChannel);
		kTerms.cityFlat = city.getCascadePackage().sourceFlat(iChannel);
	}

	// The ORACLE's terms: every one of them off a document recomputed from source. Not a single stored slot is
	// read here -- an oracle that consumed one would be partly built on the state it exists to check, and would
	// silently INHERIT a wrong input (state-repositories.md: independence is the entire value of the oracle).
	// The city's own tiers come from the city document the same run just gathered.
	void gt_collectFreshCityTerms(int iChannel, const CvCascadeSlotValues& kCityDocument,
		const CascadeOracleCityInputs& kInputs, CascadeCityCombineTerms& kTerms)
	{
		for (size_t iPlot = 0; iPlot < kInputs.workedPlots.size(); ++iPlot)
		{
			kTerms.baseFlat += kInputs.workedPlots[iPlot].flatForChannel(iChannel);
		}
		kTerms.baseFlat += kInputs.empire.flatForChannel(iChannel);
		kTerms.baseFlat += kInputs.team.flatForChannel(iChannel);
		kTerms.percentSum += kCityDocument.percentForChannel(iChannel);
		kTerms.percentSum += kInputs.empire.percentForChannel(iChannel);
		kTerms.percentSum += kInputs.team.percentForChannel(iChannel);
		kTerms.cityFlat = kCityDocument.flatForChannel(iChannel);
	}

	// The ONE city RECEIVER combine (modifier.md §1: "the only live calculation is adding the packages together
	// at read"), through the ONE §2a rate shape (InfoValuation::cityRate). The SPECIALIST term is computed here
	// on both paths: it folds live assigned counts through the per-scope group fold and consumes no package, so
	// there is nothing about it for the two paths to differ on.
	int64_t gt_cityRateFromTerms(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx,
		const CascadeCityCombineTerms& kTerms)
	{
		// each assigned specialist's own output, folded per §2a with the city sources' per-specialist boosts
		// riding its conditioned entries (the ONE per-scope group fold)
		int64_t iSpecialists = 0;
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
		return InfoValuation::cityRate(kTerms.baseFlat, iSpecialists, (int)kTerms.percentSum, kTerms.cityFlat);
	}

	// ---- THE PER-CITY QUANTITY AN EMPIRE RECEIVER SUMS -- the COMMERCE SPLIT (modifier.md §2a's commerce
	// ---- paragraph). An empire receiver total is the Σ over the player's cities of each city's REALIZED value
	// ---- of that channel: the consuming scope stores its own realized TOTAL and nothing else
	// ---- (state-repositories.md), and the cached sum replaces the legacy per-read walk of every city without
	// ---- changing WHAT is summed. For a commerce channel that realized value is NOT the channel's own deposits
	// ---- alone: the city receives the COMMERCE YIELD and the empire's slider splits that yield across gold /
	// ---- research / culture / espionage, each channel adding its own deposits on top.
	// ---- ⛔ NO empire-scope addend ever joins the Σ: an empire-scope deposit ROLLS DOWN to every city of the
	// ---- player (modifier.md §1) and is therefore already inside each city's realized value, so adding the
	// ---- empire package's own sums here would count every empire-scope deposit once per city PLUS once more.
	// ---- The scope principle says the same thing from the other side: a lower scope never stores an upper
	// ---- scope's sums, and the downward roll is realized at the combine. ----

	// The city's ROLLED CHAIN legs for one channel, read the REBUILD way: the SAME chain
	// InfoValuation::rolledLegsAtCity describes on the read path -- team + empire + city --
	// through the rebuild-path accessors, so every input rebuilds through its own mark and the mark ORDER within
	// one event stays irrelevant. The worked plots are deliberately absent (a plot never enters an upper scope's
	// chain -- modifier.md §2 plot-as-base), and the city's own flats sit INSIDE the rolled sum rather than in a
	// post-percent tier: for commerce every term but the process conversion is scaled by the percent stack.
	void gt_collectStoredCityRolledLegs(const CvCity& city, int iChannel, int64_t& flatSum, int64_t& percentSum)
	{
		flatSum = 0;
		percentSum = 0;
		const CvPlayer& owner = GET_PLAYER(city.getOwner());
		const CvTeam& team = GET_TEAM(owner.getTeam());
		flatSum += team.getCascadePackage().sourceFlat(iChannel);
		percentSum += team.getCascadePackage().sourcePercent(iChannel);
		flatSum += owner.getCascadePackage().sourceFlat(iChannel);
		percentSum += owner.getCascadePackage().sourcePercent(iChannel);
		flatSum += city.getCascadePackage().sourceFlat(iChannel);
		percentSum += city.getCascadePackage().sourcePercent(iChannel);
	}

	// The ORACLE's twin of the chain above, off the documents this run recomputed from source -- not one stored
	// slot is read (state-repositories.md: independence is the entire value of the oracle).
	void gt_collectFreshCityRolledLegs(int iChannel, const CvCascadeSlotValues& kCityDocument,
		const CascadeOracleCityInputs& kInputs, int64_t& flatSum, int64_t& percentSum)
	{
		flatSum = 0;
		percentSum = 0;
		flatSum += kInputs.team.flatForChannel(iChannel);
		percentSum += kInputs.team.percentForChannel(iChannel);
		flatSum += kInputs.empire.flatForChannel(iChannel);
		percentSum += kInputs.empire.percentForChannel(iChannel);
		flatSum += kCityDocument.flatForChannel(iChannel);
		percentSum += kCityDocument.percentForChannel(iChannel);
	}

	// One city's commerce-split INPUT TERMS for one commerce channel. The two paths collect them from genuinely
	// different places -- the REBUILD reads the stored packages and sums through their marks, the ORACLE reads
	// freshly recomputed documents -- while the MATH stays in one place ([DEC-single-implementation]:
	// gt_cityCommerceFromTerms below, which is a call onto InfoValuation::commerceSplit and nothing else).
	struct CascadeCityCommerceTerms
	{
		int64_t commerceYieldRate;     // TIER 1: the city's realized COMMERCE yield -- what the slider divides
		int64_t productionYieldRate;   // TIER 2: the city's realized PRODUCTION yield -- what the process converts
		int64_t channelPercentSum;     // the ONE additive stack (plain percents) of THIS channel over the city's chain, for the
		                            // slider share alone: the deposits below already met that stack
		int64_t channelDeposits;       // this channel's own realized deposits, ALREADY scaled by the stack

		CascadeCityCommerceTerms() : commerceYieldRate(0), productionYieldRate(0), channelPercentSum(0), channelDeposits(0) {}
	};

	// This channel's own realized deposits at the city, given its rolled legs. A channel the CITY also consumes
	// (culture, the lone dual consumer) answers its MAINTAINED receiver sum -- the same preference
	// InfoValuation::realizedAtCity applies on the read path; re-rolling it would be a second derivation of a
	// total the gather already wrote. Which side of the channel IS the answer comes from the census verdict
	// declared beside the vocabulary (infoKindUnit), never re-decided here.
	int64_t gt_channelDepositsFromLegs(int iChannel, int64_t iRolledFlat, int64_t iRolledPercent)
	{
		const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
		const int iKind = CascadeChannelRegistry::channelKind(iChannel);
		return InfoValuation::realizedChannel(iRolledFlat, iRolledPercent, infoKindUnit(eFamily, iKind, CASC_SCOPE_CITY));
	}

	void gt_collectStoredCityCommerceTerms(const CvCity& city, int iChannel, CascadeCityCommerceTerms& kTerms)
	{
		const int iCommerceYieldChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily((int)YIELD_COMMERCE), (int)CHANNEL_AMOUNT, -1);
		const int iProductionYieldChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily((int)YIELD_PRODUCTION), (int)CHANNEL_AMOUNT, -1);
		kTerms.commerceYieldRate = city.getCascadePackage().sourceSum(iCommerceYieldChannel);
		kTerms.productionYieldRate = city.getCascadePackage().sourceSum(iProductionYieldChannel);
		int64_t iRolledFlat = 0;
		int64_t iRolledPercent = 0;
		gt_collectStoredCityRolledLegs(city, iChannel, iRolledFlat, iRolledPercent);
		kTerms.channelPercentSum = iRolledPercent;
		if (CascadeChannelRegistry::scopeReceiverIndex(CASC_SCOPE_CITY, iChannel) >= 0)
		{
			kTerms.channelDeposits = city.getCascadePackage().sourceSum(iChannel);
		}
		else
		{
			kTerms.channelDeposits = gt_channelDepositsFromLegs(iChannel, iRolledFlat, iRolledPercent);
		}
	}

	void gt_collectFreshCityCommerceTerms(const CvCity& city, int iChannel, const CvCascadeEvalCtx& evalCtx,
		const CvCascadeSlotValues& kCityDocument, const CascadeOracleCityInputs& kInputs,
		CascadeCityCommerceTerms& kTerms)
	{
		const int iCommerceYieldChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily((int)YIELD_COMMERCE), (int)CHANNEL_AMOUNT, -1);
		const int iProductionYieldChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily((int)YIELD_PRODUCTION), (int)CHANNEL_AMOUNT, -1);
		CascadeCityCombineTerms kCommerceYieldTerms;
		gt_collectFreshCityTerms(iCommerceYieldChannel, kCityDocument, kInputs, kCommerceYieldTerms);
		kTerms.commerceYieldRate = gt_cityRateFromTerms(city, iCommerceYieldChannel, evalCtx, kCommerceYieldTerms);
		CascadeCityCombineTerms kProductionYieldTerms;
		gt_collectFreshCityTerms(iProductionYieldChannel, kCityDocument, kInputs, kProductionYieldTerms);
		kTerms.productionYieldRate = gt_cityRateFromTerms(city, iProductionYieldChannel, evalCtx, kProductionYieldTerms);
		int64_t iRolledFlat = 0;
		int64_t iRolledPercent = 0;
		gt_collectFreshCityRolledLegs(iChannel, kCityDocument, kInputs, iRolledFlat, iRolledPercent);
		kTerms.channelPercentSum = iRolledPercent;
		if (CascadeChannelRegistry::scopeReceiverIndex(CASC_SCOPE_CITY, iChannel) >= 0)
		{
			CascadeCityCombineTerms kChannelTerms;
			gt_collectFreshCityTerms(iChannel, kCityDocument, kInputs, kChannelTerms);
			kTerms.channelDeposits = gt_cityRateFromTerms(city, iChannel, evalCtx, kChannelTerms);
		}
		else
		{
			kTerms.channelDeposits = gt_channelDepositsFromLegs(iChannel, iRolledFlat, iRolledPercent);
		}
	}

	// The ONE per-city commerce combine, through the ONE split (InfoValuation::commerceSplit). The SLIDER and
	// the process conversion are read here on BOTH paths: each is live player/city state consuming no package,
	// so there is nothing about them for the two paths to differ on (the same reason gt_cityRateFromTerms owns
	// the specialist term). The slider is a plain 0..100 counter, never a ×100 magnitude.
	int64_t gt_cityCommerceFromTerms(const CvCity& city, int iCommerce, const CascadeCityCommerceTerms& kTerms)
	{
		int aiCommerceRates[NUM_COMMERCE_TYPES];
		GET_PLAYER(city.getOwner()).getEmpireContext().commerceRates(aiCommerceRates);
		int iProductionToCommerce = 0;
		const ProcessTypes eProcess = city.getProductionProcess();
		if (eProcess != NO_PROCESS)
		{
			iProductionToCommerce = GC.getProcessInfo(eProcess).getProductionToCommerce((CommerceTypes)iCommerce, CASC_SCOPE_CITY);
		}
		return InfoValuation::commerceSplit(kTerms.commerceYieldRate, aiCommerceRates[iCommerce],
			kTerms.channelPercentSum, kTerms.channelDeposits, kTerms.productionYieldRate, iProductionToCommerce);
	}

	// ---- THE TWO RECEIVER-SUM LEGS. Same combine, different INPUT SOURCING -- which is why they are two
	// ---- functions rather than one with a switch: the rebuild path's inputs are the stored packages (read
	// ---- through their marks), the oracle's are recomputed in full. ----

	void gt_rebuildCitySums(const CvCity& city, int64_t iReceiverBits, const CvCascadeEvalCtx& evalCtx,
		std::vector<int64_t>& sumSlots)
	{
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_CITY);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_CITY, iReceiver);
			if ((iReceiverBits & CascadeChannelRegistry::scopeReceiverBit(CASC_SCOPE_CITY, iChannel)) == 0)
			{
				continue;
			}
			if (iReceiver >= (int)sumSlots.size())
			{
				continue;
			}
			CascadeCityCombineTerms kTerms;
			gt_collectStoredCityTerms(city, iChannel, kTerms);
			sumSlots[iReceiver] = gt_cityRateFromTerms(city, iChannel, evalCtx, kTerms);
		}
	}

	// Fills the receiver slots of a city document whose CHANNEL slots this run has already gathered -- every
	// receiver, always, since an oracle run is a full recalc and has no mask to honour.
	void gt_oracleCitySums(const CvCity& city, const CvCascadeEvalCtx& evalCtx, CvCascadeSlotValues& kCityDocument)
	{
		CascadeOracleCityInputs kInputs;
		gt_collectFreshCityInputs(city, kInputs);
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_CITY);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_CITY, iReceiver);
			if (iChannel < 0 || iReceiver >= (int)kCityDocument.sum.size())
			{
				continue;
			}
			CascadeCityCombineTerms kTerms;
			gt_collectFreshCityTerms(iChannel, kCityDocument, kInputs, kTerms);
			kCityDocument.sum[iReceiver] = gt_cityRateFromTerms(city, iChannel, evalCtx, kTerms);
		}
	}

	// The empire's realized totals: per receiver channel, the Σ over the player's cities of the CITY's realized
	// value of that channel -- and nothing beside the Σ (the double-count constraint stated at the commerce
	// section above). A COMMERCE receiver's per-city quantity is the commerce split; MAINTENANCE is the one
	// receiver whose realized value the packages cannot answer alone (it folds the engine components and the
	// status gate), so it asks the city; and the plain rate combine stands for anything else, since "the
	// city's realized value" is what the Σ takes in every case.
	void gt_rebuildEmpireSums(const CvPlayer& player, int64_t iReceiverBits, std::vector<int64_t>& sumSlots)
	{
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_EMPIRE);
		for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
		{
			const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_EMPIRE, iReceiver);
			if ((iReceiverBits & CascadeChannelRegistry::scopeReceiverBit(CASC_SCOPE_EMPIRE, iChannel)) == 0)
			{
				continue;
			}
			const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
			const int iCommerce = infoFamilyCommerce(eFamily);
			int64_t iTotal = 0;
			for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
			{
				const CvCity* pLoopCity = *cityIterator;
				if (eFamily == MODFAM_MAINTENANCE)
				{
					// ⚑ MAINTENANCE's per-city quantity is the city's REALIZED value, which the plain combine
					// below cannot answer: the status gate (WLTKD / disorder) suppresses the whole contribution
					// at the READ, and it is live city state no package carries. The Σ takes the member's
					// realized value ([state-repositories.md]), so it asks the city for exactly that.
					iTotal += pLoopCity->getMaintenanceTimes100();
				}
				else if (iCommerce >= 0)
				{
					CascadeCityCommerceTerms kCommerceTerms;
					gt_collectStoredCityCommerceTerms(*pLoopCity, iChannel, kCommerceTerms);
					iTotal += gt_cityCommerceFromTerms(*pLoopCity, iCommerce, kCommerceTerms);
				}
				else
				{
					CvCascadeEvalCtx cityEvalCtx;
					gt_fillCityEvalCtx(*pLoopCity, cityEvalCtx);
					CascadeCityCombineTerms kTerms;
					gt_collectStoredCityTerms(*pLoopCity, iChannel, kTerms);
					iTotal += gt_cityRateFromTerms(*pLoopCity, iChannel, cityEvalCtx, kTerms);
				}
			}
			if (iReceiver < (int)sumSlots.size())
			{
				// The slot is int64 and so is the Σ: a receiver total accumulates across every member city at
				// ×100, which is the case [fixed-point-and-scales.md §1b] says carries 64 bits. Narrowing here
				// would throw that away at the last step, on the plane (culture) already known to overflow.
				sumSlots[iReceiver] = iTotal;
			}
		}
	}

	// The oracle's empire totals: EVERY city recomputed in full -- its own channel document and every scope its
	// realized rate consumes -- then summed, over the same per-city quantity the rebuild sums. The rebuild's
	// shortcut for a channel the city also receives (read that city's stored sum) has no counterpart here: the
	// quantity is the city's realized value either way, and the oracle computes it rather than trusting it.
	void gt_oracleEmpireSums(const CvPlayer& player, std::vector<int64_t>& sumSlots)
	{
		const int iReceivers = CascadeChannelRegistry::scopeReceiverCount(CASC_SCOPE_EMPIRE);
		std::vector<int64_t> aTotals;
		aTotals.assign((size_t)((iReceivers > 0) ? iReceivers : 0), 0);
		for (CvPlayer::city_iterator cityIterator = player.beginCities(); cityIterator != player.endCities(); ++cityIterator)
		{
			const CvCity* pLoopCity = *cityIterator;
			if (pLoopCity == NULL)
			{
				continue;
			}
			CvCascadeEvalCtx cityEvalCtx;
			gt_fillCityEvalCtx(*pLoopCity, cityEvalCtx);
			CvCascadeSlotValues kCityDocument;
			gt_freshCityDocument(*pLoopCity, cityEvalCtx, kCityDocument);
			CascadeOracleCityInputs kInputs;
			gt_collectFreshCityInputs(*pLoopCity, kInputs);
			for (int iReceiver = 0; iReceiver < iReceivers; ++iReceiver)
			{
				const int iChannel = CascadeChannelRegistry::scopeReceiverChannel(CASC_SCOPE_EMPIRE, iReceiver);
				if (iChannel < 0)
				{
					continue;
				}
				const ModifierFamily eFamily = CascadeChannelRegistry::channelFamily(iChannel);
				const int iCommerce = infoFamilyCommerce(eFamily);
				if (iCommerce >= 0)
				{
					CascadeCityCommerceTerms kCommerceTerms;
					gt_collectFreshCityCommerceTerms(*pLoopCity, iChannel, cityEvalCtx, kCityDocument, kInputs, kCommerceTerms);
					aTotals[iReceiver] += gt_cityCommerceFromTerms(*pLoopCity, iCommerce, kCommerceTerms);
				}
				else
				{
					CascadeCityCombineTerms kTerms;
					gt_collectFreshCityTerms(iChannel, kCityDocument, kInputs, kTerms);
					aTotals[iReceiver] += gt_cityRateFromTerms(*pLoopCity, iChannel, cityEvalCtx, kTerms);
				}
			}
		}
		for (int iReceiver = 0; iReceiver < iReceivers && iReceiver < (int)sumSlots.size(); ++iReceiver)
		{
			sumSlots[iReceiver] = aTotals[iReceiver];
		}
	}
}

void CascadeGather::refreshPlot(const CvPlot& plot, int64_t iMask)
{
	const CvCascadePackage<CvPlot>& package = plot.getCascadePackage();
	package.ensureSized();
	gt_gatherPlotChannels(plot, iMask, package.flat, package.percent);
	emitCacheRebuilt((int)CASC_SCOPE_PLOT, (int)plot.getOwner(),
		plot.getX() + plot.getY() * GC.getMap().getGridWidth(), iMask);
}

void CascadeGather::gatherPlotInto(const CvPlot& plot, CvCascadeSlotValues& kValues)
{
	gt_freshPlotDocument(plot, kValues);
}

void CascadeGather::refreshCity(const CvCity& city, int64_t iMask)
{
	const CvCascadePackage<CvCity>& package = city.getCascadePackage();
	package.ensureSized();
	CvCascadeEvalCtx evalCtx;
	gt_fillCityEvalCtx(city, evalCtx);
	const int64_t iChannelBits = iMask & CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_CITY);
	const int64_t iReceiverBits = iMask & CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_CITY);
	if (iChannelBits != 0)
	{
		gt_gatherCityChannels(city, iChannelBits, evalCtx, package.flat, package.percent);
	}
	if (iReceiverBits != 0)
	{
		gt_rebuildCitySums(city, iReceiverBits, evalCtx, package.sum);
	}
	emitCacheRebuilt((int)CASC_SCOPE_CITY, (int)city.getOwner(), city.getID(), iMask);
}

void CascadeGather::gatherCityInto(const CvCity& city, CvCascadeSlotValues& kValues)
{
	CvCascadeEvalCtx evalCtx;
	gt_fillCityEvalCtx(city, evalCtx);
	gt_freshCityDocument(city, evalCtx, kValues);
	gt_oracleCitySums(city, evalCtx, kValues);
}

void CascadeGather::refreshEmpire(const CvPlayer& player, int64_t iMask)
{
	const CvCascadePackage<CvPlayer>& package = player.getCascadePackage();
	package.ensureSized();
	const int64_t iChannelBits = iMask & CascadeChannelRegistry::scopeAllChannelsMask(CASC_SCOPE_EMPIRE);
	const int64_t iReceiverBits = iMask & CascadeChannelRegistry::scopeAllReceiversMask(CASC_SCOPE_EMPIRE);
	if (iChannelBits != 0)
	{
		CvCascadeEvalCtx evalCtx;
		player.getEmpireContext().fillEvalCtx(evalCtx);   // player + team (no city at empire scope)
		gt_gatherEmpireChannels(player, iChannelBits, evalCtx, package.flat, package.percent);
	}
	if (iReceiverBits != 0)
	{
		gt_rebuildEmpireSums(player, iReceiverBits, package.sum);
	}
	emitCacheRebuilt((int)CASC_SCOPE_EMPIRE, (int)player.getID(), (int)player.getID(), iMask);
}

void CascadeGather::gatherEmpireInto(const CvPlayer& player, CvCascadeSlotValues& kValues)
{
	gt_freshEmpireDocument(player, kValues);
	gt_oracleEmpireSums(player, kValues.sum);
}

void CascadeGather::refreshTeam(const CvTeam& team, int64_t iMask)
{
	const CvCascadePackage<CvTeam>& package = team.getCascadePackage();
	package.ensureSized();
	gt_gatherTeamChannels(team, iMask, package.flat, package.percent);
	emitCacheRebuilt((int)CASC_SCOPE_TEAM, (int)team.getLeaderID(), (int)team.getID(), iMask);
}

void CascadeGather::gatherTeamInto(const CvTeam& team, CvCascadeSlotValues& kValues)
{
	gt_freshTeamDocument(team, kValues);
}

