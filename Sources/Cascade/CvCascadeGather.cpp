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

	// Fold ONE source info's compiled entries AT one scope into the ORACLE's scratch dictionaries.
	// ⛔ THE RESOLVE IS NOT HERE. What an entry deposits -- the scope test, the plural fans, the target keys, the
	// gates, the per scaler -- is MMKernel::resolveEntry, shared verbatim with the apply path that maintains the
	// stored slots ([DEC-single-implementation]). This function is now ONLY the oracle's SINK: the wanted-channel
	// mask and the scratch vectors, neither of which the apply path has. That split is what keeps the
	// stored-vs-oracle tripwire honest -- the two sides build the same numbers from ONE derivation, so a
	// disagreement can only be a missed emit and never a drifted second copy of the fold.
	// iMultiplier / pKeyPlot / bSkipRateChannels are passed straight through (documented on resolveEntry).
	void gt_foldInfo(const CvModifiers* pModifiers, int iMultiplier, CvCascScope eScope, int64_t iWantedBits,
		const CvCascadeEvalCtx& evalCtx, const CvPlot* pKeyPlot, bool bSkipRateChannels,
		std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots)
	{
		if (pModifiers == NULL || pModifiers->empty() || iMultiplier == 0)
		{
			return;
		}
		const std::vector<CvModEntry*>& entries = pModifiers->entries();
		for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
		{
			const CvModEntry* pEntry = entries[iEntry];
			if (pEntry == NULL)
			{
				continue;
			}
			int iChannel = -1;
			bool bPercentSide = false;
			int64_t iValue = 0;
			if (!MMKernel::resolveEntry(*pEntry, iMultiplier, eScope, evalCtx, pKeyPlot,
				bSkipRateChannels, iChannel, bPercentSide, iValue))
			{
				continue;
			}
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
	// scope: civics, traits, held techs, projects, handicap, heritage, presence-gated
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
				gt_foldInfo(GC.getCivicInfo(eCivic).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
			}
		}
		// ⛔ NO PURE_TRAITS FILTER HERE, deliberately. The alignment rule is applied ONCE, as a parse transform
		// that gates an off-alignment entry on GAMEOPTION_LEADER_PURE_TRAITS (CvModifiers::applyPureTraitGate),
		// so the ordinary condition evaluation this fold already performs enforces it -- the same way, through
		// the same evaluator, as it does for every other consumer ([DEC-single-implementation]).
		// ⚠ Filtering again here would be a SECOND implementation of one rule, and the dangerous kind: the
		// oracle exists to disagree with the stored plane when a fact is missed, so a rule it applies privately
		// is a rule the diff can never report on.
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
			gt_foldInfo(pTrait->getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
		}
		const CvTeam& team = GET_TEAM(player.getTeam());
		for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
		{
			if (team.isHasTech((TechTypes)iTech))
			{
				gt_foldInfo(GC.getTechInfo((TechTypes)iTech).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
			}
		}
		for (int iProject = 0; iProject < GC.getNumProjectInfos(); ++iProject)
		{
			const int iCount = team.getProjectCount((ProjectTypes)iProject);
			if (iCount > 0)
			{
				gt_foldInfo(GC.getProjectInfo((ProjectTypes)iProject).getModifiers(), iCount, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
			}
		}
		if (player.getHandicapType() != NO_HANDICAP)
		{
			gt_foldInfo(GC.getHandicapInfo(player.getHandicapType()).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
		}
		for (int iHeritage = 0; iHeritage < GC.getNumHeritageInfos(); ++iHeritage)
		{
			if (player.hasHeritage((HeritageTypes)iHeritage))
			{
				gt_foldInfo(GC.getHeritageInfo((HeritageTypes)iHeritage).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
			}
		}
		// presence-gated source classes: their entries carry their own presence conditions -- the fold's
		// ordinary gate evaluation keeps an absent source silent (no pre-filter walk needed here).
		for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); ++iBonus)
		{
			gt_foldInfo(GC.getBonusInfo((BonusTypes)iBonus).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
		}
		for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
		{
			gt_foldInfo(GC.getReligionInfo((ReligionTypes)iReligion).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
		}
		for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation)
		{
			gt_foldInfo(GC.getCorporationInfo((CorporationTypes)iCorporation).getModifiers(), 1, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
		}
		// ⛔ This EMPIRE-scope fold deliberately leaves ctx.sourceBuilding unset (-1), unlike the city fold. The
		// player may own N copies of one building, each built in a different year, so "how long has THIS
		// building stood" has no single answer here -- a source-predicate must decline rather than age-gate
		// against whichever copy the walk happened to reach. Every authored existedFor sits at CITY scope, so
		// nothing is lost; an empire-scope one would be unanswerable by construction, not merely unwired.
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			const int iOwned = player.getBuildingCount((BuildingTypes)iBuilding);
			if (iOwned > 0)
			{
				gt_foldInfo(GC.getBuildingInfo((BuildingTypes)iBuilding).getModifiers(), iOwned, eScope, iWantedBits, evalCtx, pKeyPlot, false, flatSlots, percentSlots);
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

	// Zero the wanted slots of a FLAT-only vector -- gt_beginRefill's single-dictionary twin, for the segment
	// below (only the flat side is segmented; a percent belongs to the plot as a whole).
	void gt_beginRefillFlat(CvCascScope eScope, int64_t iWantedBits, std::vector<int64_t>& flatSlots)
	{
		const int iChannels = CascadeChannelRegistry::scopeChannelCount(eScope);
		for (int iSlot = 0; iSlot < iChannels && iSlot < (int)flatSlots.size(); ++iSlot)
		{
			const int iChannel = CascadeChannelRegistry::scopeSlotChannel(eScope, iSlot);
			if ((iWantedBits & CascadeChannelRegistry::scopeChannelBit(eScope, iChannel)) != 0)
			{
				flatSlots[iSlot] = 0;
			}
		}
	}

	// pSubstrateSlots receives the SUBSTRATE-ONLY segment of the flat side -- terrain + feature + bonus, i.e.
	// what the GROUND yields before anything is built on it. That is the plot's nature value, and it is a
	// SEGMENT of this same package rather than a second walk ([contexts.md]). NULL for a caller wanting only
	// the total. Only the flat side segments; a percent belongs to the plot as a whole.
	void gt_gatherPlotChannels(const CvPlot& plot, int64_t iWantedBits, std::vector<int64_t>& flatSlots, std::vector<int>& percentSlots,
		std::vector<int64_t>* pSubstrateSlots)
	{
		gt_beginRefill(CASC_SCOPE_PLOT, iWantedBits, flatSlots, percentSlots);
		if (pSubstrateSlots != NULL)
		{
			gt_beginRefillFlat(CASC_SCOPE_PLOT, iWantedBits, *pSubstrateSlots);
		}

		CvCascadeEvalCtx evalCtx;
		InfoValuation::fillEvalCtxAtPlot(plot, evalCtx);
		const PlayerTypes eOwner = plot.getOwner();

		const TeamTypes eSeeingTeam = (evalCtx.empireContext != NULL) ? (TeamTypes)evalCtx.empireContext->teamId() : NO_TEAM;
		const CvModifiers* pTerrain     = plot.getTerrainType()      != NO_TERRAIN     ? GC.getTerrainInfo(plot.getTerrainType()).getModifiers()         : NULL;
		const CvModifiers* pFeature     = plot.getFeatureType()      != NO_FEATURE     ? GC.getFeatureInfo(plot.getFeatureType()).getModifiers()         : NULL;
		const CvModifiers* pBonus       = plot.getBonusType(eSeeingTeam) != NO_BONUS   ? GC.getBonusInfo(plot.getBonusType(eSeeingTeam)).getModifiers()  : NULL;
		const CvModifiers* pImprovement = plot.getImprovementType()  != NO_IMPROVEMENT ? GC.getImprovementInfo(plot.getImprovementType()).getModifiers() : NULL;
		const CvModifiers* pRoute       = plot.getRouteType()        != NO_ROUTE       ? GC.getRouteInfo(plot.getRouteType()).getModifiers()             : NULL;

		// (1) every PLOT-scope source, folded generically. This serves the channels whose combine IS a plain
		// sum (health, defense, the property plane, ...). The YIELD channels are RECOMPUTED in (2): their
		// combine is the §2a plot-as-base package, which floors PER SEGMENT and cannot be expressed as a sum.
		gt_foldInfo(pTerrain,     1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, false, flatSlots, percentSlots);
		gt_foldInfo(pFeature,     1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, false, flatSlots, percentSlots);
		gt_foldInfo(pBonus,       1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, false, flatSlots, percentSlots);
		gt_foldInfo(pRoute,       1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, false, flatSlots, percentSlots);
		gt_foldInfo(pImprovement, 1, CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, false, flatSlots, percentSlots);

		// The owner's PLOT-scope deposits (keyed by this plot's substrate / `plots`-target / bare plot flats)
		// land in a SCRATCH as well as the totals: the §2a package puts them AFTER the substrate legs, so the
		// yield recompute needs them as their own term rather than mixed into the substrate sum.
		std::vector<int64_t> ownerFlat(flatSlots.size(), 0);
		if (eOwner != NO_PLAYER)
		{
			gt_foldPlayerSources(GET_PLAYER(eOwner), CASC_SCOPE_PLOT, iWantedBits, evalCtx, &plot, ownerFlat, percentSlots);
			for (size_t iSlot = 0; iSlot < ownerFlat.size() && iSlot < flatSlots.size(); ++iSlot)
			{
				flatSlots[iSlot] += ownerFlat[iSlot];
			}
		}

		// (2) THE YIELD CHANNELS -- the isolated plot-as-base package (modifier.md §2a basePlotYield):
		// max(0, terrain+feature+bonus) nature · improvement floored at −nature · + route · then the owner's
		// keyed/plots flats · floored. The per-segment floors live in the ONE implementation of this calc
		// ([DEC-single-implementation]); the gather never re-derives them, and it takes the substrate segment
		// back from the same call rather than summing those legs a second time.
		int aiBaseYields[NUM_YIELD_TYPES];
		int aiNatureYields[NUM_YIELD_TYPES];
		InfoValuation::plotBaseYields(pTerrain, pFeature, pBonus, pImprovement, pRoute, evalCtx, aiBaseYields, &aiNatureYields);
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			const int iChannel = CascadeChannelRegistry::channelLookup(infoYieldFamily(iYield), (int)CHANNEL_AMOUNT, -1);
			if (iChannel < 0 || (iWantedBits & CascadeChannelRegistry::scopeChannelBit(CASC_SCOPE_PLOT, iChannel)) == 0)
			{
				continue;
			}
			const int iSlot = CascadeChannelRegistry::scopeSlotIndex(CASC_SCOPE_PLOT, iChannel);
			if (iSlot < 0)
			{
				continue;
			}
			int64_t iTotal = (int64_t)aiBaseYields[iYield] + (iSlot < (int)ownerFlat.size() ? ownerFlat[iSlot] : 0);
			if (iTotal < 0)
			{
				iTotal = 0;
			}
			if (iSlot < (int)flatSlots.size())
			{
				flatSlots[iSlot] = iTotal;
			}
			if (pSubstrateSlots != NULL && iSlot < (int)pSubstrateSlots->size())
			{
				(*pSubstrateSlots)[iSlot] = aiNatureYields[iYield];
			}
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
			// The fold carries WHICH building it is resolving, so a source-predicate can ask about the carrier
			// (existedFor -- how long has THIS building stood). Same shape as the per-religion walk: a local
			// copy with the slot set per iteration, never a mutation of the shared ctx.
			CvCascadeEvalCtx perBuildingCtx = evalCtx;
			for (std::set<int>::const_iterator it = evalCtx.activeBuildings->begin(); it != evalCtx.activeBuildings->end(); ++it)
			{
				const CvBuildingInfo& building = GC.getBuildingInfo((BuildingTypes)(*it));
				const bool bObsolete = cascadeIsBuildingObsolete(*it, evalCtx);
				perBuildingCtx.sourceBuilding = *it;
				gt_foldInfo(bObsolete ? building.getWhenObsolete() : building.getModifiers(),
					1, CASC_SCOPE_CITY, iWantedBits, perBuildingCtx, NULL, false, flatSlots, percentSlots);
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
					iCount, CASC_SCOPE_CITY, iWantedBits, evalCtx, NULL, true, flatSlots, percentSlots);
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
		//	A team owns no live-state silo, so the ctx is anchored on the team LEADER's empire context -- which
		//	is what answers the team facts a team-scope deposit's conditions ask ([contexts.md]).
		if (team.getLeaderID() != NO_PLAYER)
		{
			evalCtx.empireContext = &GET_PLAYER(team.getLeaderID()).getEmpireContext();
		}

		// team-scope deposits: projects (team-held, x count) + held techs
		for (int iProject = 0; iProject < GC.getNumProjectInfos(); ++iProject)
		{
			const int iCount = team.getProjectCount((ProjectTypes)iProject);
			if (iCount > 0)
			{
				gt_foldInfo(GC.getProjectInfo((ProjectTypes)iProject).getModifiers(), iCount, CASC_SCOPE_TEAM, iWantedBits, evalCtx, NULL, false, flatSlots, percentSlots);
			}
		}
		for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
		{
			if (team.isHasTech((TechTypes)iTech))
			{
				gt_foldInfo(GC.getTechInfo((TechTypes)iTech).getModifiers(), 1, CASC_SCOPE_TEAM, iWantedBits, evalCtx, NULL, false, flatSlots, percentSlots);
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
			kDocument.flat, kDocument.percent, &kDocument.substrateFlat);
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

void CascadeGather::gatherPlotInto(const CvPlot& plot, CvCascadeSlotValues& kValues)
{
	gt_freshPlotDocument(plot, kValues);
}

void CascadeGather::gatherCityInto(const CvCity& city, CvCascadeSlotValues& kValues)
{
	CvCascadeEvalCtx evalCtx;
	gt_fillCityEvalCtx(city, evalCtx);
	gt_freshCityDocument(city, evalCtx, kValues);
	gt_oracleCitySums(city, evalCtx, kValues);
}

void CascadeGather::gatherEmpireInto(const CvPlayer& player, CvCascadeSlotValues& kValues)
{
	gt_freshEmpireDocument(player, kValues);
	gt_oracleEmpireSums(player, kValues.sum);
}

void CascadeGather::gatherTeamInto(const CvTeam& team, CvCascadeSlotValues& kValues)
{
	gt_freshTeamDocument(team, kValues);
}

