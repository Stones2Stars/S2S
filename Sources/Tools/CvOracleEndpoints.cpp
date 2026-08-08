//
//	OracleEndpoints -- the stored/oracle documents of the derived-state planes (see the header). ONE renderer
//	per plane, called for BOTH sides, so the two documents are diffable field by field OUTSIDE the DLL.
//

#include "CvGameCoreDLL.h"
#include "CvOracleEndpoints.h"
#include "Cascade/CvCascadeGather.h"        // the ORACLE leg: the gather*Into entry points
#include "Cascade/CvCascadeSlotValues.h"    // the served shape both sides answer in
#include "Cascade/CvCascadeChannelRegistry.h"
#include "Enabler/CvEnablerKernel.h"        // the operating set: the standing read + the from-source recompute
#include "Engine/CapabilityContext.h"        // the empire ability union: the stored read + the from-source oracle
#include "Infos/CvClassificationRegistry.h"  // id -> authored key, so a served ability names itself
#include "Engine/CvCity.h"
#include "Engine/CvPlot.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvTeam.h"
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"      // GC.getGame()
#include "AI/CvPlayerAI.h"    // GET_PLAYER
#include "AI/CvTeamAI.h"      // GET_TEAM
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h"
#include "CvYieldInfo.h"                    // the served yield key
#include "Data/CvInfoValuation.h"           // CityRateTerms + the refused-deposit walk -- the tooltip's own document
#include "Infos/CvCondition.h"              // the refusing atom's spelling
#include "Engine/CityContext.h"             // the live bonus stores
#include "Cascade/CvCascadePackage.h"       // the per-scope percent split
#include "CvTerrainInfo.h"
#include "CvRouteInfo.h"
#include "CvImprovementInfo.h"
#include "CvFeatureInfo.h"
#include <set>
#include <vector>

namespace
{
	// -1 asks for the ACTIVE player, the standing /computed selector convention.
	PlayerTypes oe_resolvePlayer(int iPlayer)
	{
		if (iPlayer >= 0 && iPlayer < MAX_PLAYERS)
		{
			return (PlayerTypes)iPlayer;
		}
		return GC.getGame().getActivePlayer();
	}

	const char* oe_scopeName(CvCascScope eScope)
	{
		switch (eScope)
		{
		case CASC_SCOPE_WORLD:  return "world";
		case CASC_SCOPE_TEAM:   return "team";
		case CASC_SCOPE_EMPIRE: return "empire";
		case CASC_SCOPE_CITY:   return "city";
		case CASC_SCOPE_PLOT:   return "plot";
		default:                return "?";
		}
	}

	const char* oe_sideName(OracleEndpoints::OracleSide eSide)
	{
		return (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE) ? "oracle" : "stored";
	}

	// The identity every served value carries, INTERPRETED PER SCOPE (CvCascadeSlotValues.h) -- spelled out
	// here under per-scope names so a consumer keys on "owner"/"id" for a city and "x"/"y" for a plot.
	void oe_renderIdentity(const CvCascadeSlotValues& kValues, picojson::value::object& kRow)
	{
		switch (kValues.scope)
		{
		case CASC_SCOPE_CITY:
			kRow["owner"] = picojson::value((double)kValues.identityFirst);
			kRow["id"] = picojson::value((double)kValues.identitySecond);
			kRow["globalId"] = picojson::value(std::string(
				CvString::format("%02d-%d", kValues.identityFirst, kValues.identitySecond).c_str()));
			break;
		case CASC_SCOPE_EMPIRE:
			kRow["player"] = picojson::value((double)kValues.identityFirst);
			break;
		case CASC_SCOPE_TEAM:
			kRow["team"] = picojson::value((double)kValues.identitySecond);
			break;
		case CASC_SCOPE_PLOT:
			kRow["x"] = picojson::value((double)kValues.identityFirst);
			kRow["y"] = picojson::value((double)kValues.identitySecond);
			break;
		default:
			break;
		}
	}

	// A dictionary keyed by CHANNEL NAME rather than by local slot index: the index is minted per scope at
	// load and means nothing to an external consumer, while the name is stable and diffable.
	// Templated over the slot WIDTH, because the two dictionaries no longer share one: an AMOUNT accumulates and
	// carries 64 bits, a PERCENT is a small whole number and stays 32 (CvCascadePackage.h). One implementation
	// still renders both -- the alternative, a per-width copy, is the duplication a width seam always invites.
	template <class SlotType>
	picojson::value oe_renderChannelMap(const CvCascadeSlotValues& kValues, const std::vector<SlotType>& aSlots, bool bReceiver)
	{
		picojson::value::object kMap;
		for (size_t iSlot = 0; iSlot < aSlots.size(); ++iSlot)
		{
			const int iChannel = bReceiver
				? CascadeChannelRegistry::scopeReceiverChannel(kValues.scope, (int)iSlot)
				: CascadeChannelRegistry::scopeSlotChannel(kValues.scope, (int)iSlot);
			const char* szChannel = CascadeChannelRegistry::channelName(iChannel);
			if (szChannel == NULL)
			{
				continue;
			}
			kMap[std::string(szChannel)] = picojson::value((double)aSlots[iSlot]);
		}
		return picojson::value(kMap);
	}

	picojson::value oe_renderSlotValues(const CvCascadeSlotValues& kValues)
	{
		picojson::value::object kRow;
		kRow["scope"] = picojson::value(std::string(oe_scopeName(kValues.scope)));
		oe_renderIdentity(kValues, kRow);
		kRow["flat"] = oe_renderChannelMap(kValues, kValues.flat, false);
		kRow["percent"] = oe_renderChannelMap(kValues, kValues.percent, false);
		kRow["sum"] = oe_renderChannelMap(kValues, kValues.sum, true);
		return picojson::value(kRow);
	}

	CvString oe_serialize(picojson::value::object& kRoot)
	{
		CvString szBody(picojson::value(kRoot).serialize().c_str());
		szBody += "\n";
		return szBody;
	}

	CvString oe_error(const char* szReason)
	{
		picojson::value::object kRoot;
		kRoot["error"] = picojson::value(std::string(szReason));
		return oe_serialize(kRoot);
	}

	// Building / bonus id sets rendered as TYPE NAMES, sorted by id (std::set order), so the two sides compare
	// element by element and a difference names the entity.
	picojson::value oe_renderBuildingSet(const std::set<int>& kIds)
	{
		picojson::value::array kOut;
		for (std::set<int>::const_iterator it = kIds.begin(); it != kIds.end(); ++it)
		{
			const char* szType = (*it >= 0 && *it < GC.getNumBuildingInfos())
				? GC.getBuildingInfo((BuildingTypes)*it).getType() : "?";
			kOut.push_back(picojson::value(std::string(szType)));
		}
		return picojson::value(kOut);
	}

	picojson::value oe_renderBonusSet(const std::set<int>& kIds)
	{
		picojson::value::array kOut;
		for (std::set<int>::const_iterator it = kIds.begin(); it != kIds.end(); ++it)
		{
			const char* szType = (*it >= 0 && *it < GC.getNumBonusInfos())
				? GC.getBonusInfo((BonusTypes)*it).getType() : "?";
			kOut.push_back(picojson::value(std::string(szType)));
		}
		return picojson::value(kOut);
	}

	picojson::value oe_renderOperatingSet(const CvCity& kCity, const OperatingBuildings& kSet)
	{
		picojson::value::object kRow;
		kRow["owner"] = picojson::value((double)kCity.getOwner());
		kRow["id"] = picojson::value((double)kCity.getID());
		kRow["globalId"] = picojson::value(std::string(
			CvString::format("%02d-%d", (int)kCity.getOwner(), kCity.getID()).c_str()));
		kRow["x"] = picojson::value((double)kCity.getX());
		kRow["y"] = picojson::value((double)kCity.getY());
		kRow["active"] = oe_renderBuildingSet(kSet.active);
		kRow["obsolete"] = oe_renderBuildingSet(kSet.obsolete);
		kRow["provided"] = oe_renderBonusSet(kSet.provided);
		picojson::value::object kCounts;
		for (std::map<int, int>::const_iterator it = kSet.providedCount.begin(); it != kSet.providedCount.end(); ++it)
		{
			const char* szType = (it->first >= 0 && it->first < GC.getNumBonusInfos())
				? GC.getBonusInfo((BonusTypes)it->first).getType() : "?";
			kCounts[std::string(szType)] = picojson::value((double)it->second);
		}
		kRow["providedCount"] = picojson::value(kCounts);
		return picojson::value(kRow);
	}

	picojson::value oe_renderStringSet(const std::set<std::string>& kKeys)
	{
		picojson::value::array kOut;
		for (std::set<std::string>::const_iterator it = kKeys.begin(); it != kKeys.end(); ++it)
		{
			kOut.push_back(picojson::value(*it));
		}
		return picojson::value(kOut);
	}

	// Render a dictionary by its AUTHORED KEY rather than by id, so an external differ names the ability that
	// moved instead of a bit position. The registry owns the id -> key direction ([DEC-classification-infos]).
	picojson::value oe_renderClsDict(const ContextDict& kDict, int eDomain)
	{
		picojson::value::array kOut;
		for (std::map<int, int>::const_iterator it = kDict.m.begin(); it != kDict.m.end(); ++it)
		{
			if (it->second > 0)
			{
				kOut.push_back(picojson::value(ClassificationRegistry::keyOf(eDomain, it->first)));
			}
		}
		return picojson::value(kOut);
	}

	picojson::value oe_renderCapabilities(int iPlayer, const CapabilityContext& kCaps)
	{
		picojson::value::object kRow;
		kRow["player"] = picojson::value((double)iPlayer);
		kRow["capabilities"] = oe_renderClsDict(kCaps.capabilityDict(), CLSD_CAPABILITY);
		kRow["canTrade"]     = oe_renderClsDict(kCaps.canTradeDict(),   CLSD_CANTRADE);
		kRow["canWorkOn"]    = oe_renderClsDict(kCaps.canWorkOnDict(),  CLSD_CANWORKON);
		picojson::value::array kTradeTerrains;
		const ContextDict& kTerrains = kCaps.canTradeOnDict();
		for (std::map<int, int>::const_iterator it = kTerrains.m.begin(); it != kTerrains.m.end(); ++it)
		{
			if (it->second > 0 && it->first >= 0 && it->first < GC.getNumTerrainInfos())
			{
				kTradeTerrains.push_back(picojson::value(std::string(
					GC.getTerrainInfo((TerrainTypes)it->first).getType())));
			}
		}
		kRow["canTradeOn"] = picojson::value(kTradeTerrains);
		kRow["corporationRevenueModifier"] = picojson::value((double)kCaps.corporationRevenueModifier());
		return picojson::value(kRow);
	}

	// ---- the per-scope fill: STORED copies the package out, ORACLE recomputes from source into the SAME
	// ---- document type. The oracle is handed this local buffer and never the package, which is what makes
	// ---- "serving the oracle cannot repair the stored slots" structural. ----

	// ⛔ THE IDENTITY IS STAMPED FROM THE LIVE OWNER ON BOTH SIDES, AND THE STORED SIDE CANNOT INHERIT IT FROM THE
	// PACKAGE. A package binds its identity in the owner's `reset()`, which on a LOAD runs from `readIdentity`
	// BEFORE the real id is off the stream -- so every loaded object's package carries the placeholder (-1 / 0)
	// while a FOUNDED one carries the truth. Serving that would make every city's row key on "-1-0", collapse
	// them onto one another, and leave the two documents undiffable -- which is the whole point of the pair
	// ([state-repositories.md]: a divergence must NAME the object that drifted).
	// ⚑ The gather already stamps the live identity on the oracle side, so doing the same here is what puts both
	// documents on ONE key space. `identityFirst/Second` is read by nothing but this renderer, so this is the
	// point of use and there is no second consumer to keep in step.
	void oe_stampIdentity(CvCascadeSlotValues& kValues, CvCascScope eScope, int iFirst, int iSecond)
	{
		kValues.scope = eScope;
		kValues.identityFirst = iFirst;
		kValues.identitySecond = iSecond;
	}

	void oe_fillCity(const CvCity& kCity, OracleEndpoints::OracleSide eSide, CvCascadeSlotValues& kValues)
	{
		if (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE)
		{
			CascadeGather::gatherCityInto(kCity, kValues);
		}
		else
		{
			kCity.getCascadePackage().readValuesInto(kValues);
			oe_stampIdentity(kValues, CASC_SCOPE_CITY, (int)kCity.getOwner(), kCity.getID());
		}
	}

	void oe_fillEmpire(const CvPlayer& kPlayer, OracleEndpoints::OracleSide eSide, CvCascadeSlotValues& kValues)
	{
		if (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE)
		{
			CascadeGather::gatherEmpireInto(kPlayer, kValues);
		}
		else
		{
			kPlayer.getCascadePackage().readValuesInto(kValues);
			oe_stampIdentity(kValues, CASC_SCOPE_EMPIRE, (int)kPlayer.getID(), -1);
		}
	}

	void oe_fillTeam(const CvTeam& kTeam, OracleEndpoints::OracleSide eSide, CvCascadeSlotValues& kValues)
	{
		if (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE)
		{
			CascadeGather::gatherTeamInto(kTeam, kValues);
		}
		else
		{
			kTeam.getCascadePackage().readValuesInto(kValues);
			oe_stampIdentity(kValues, CASC_SCOPE_TEAM, -1, (int)kTeam.getID());
		}
	}

	void oe_fillPlot(const CvPlot& kPlot, OracleEndpoints::OracleSide eSide, CvCascadeSlotValues& kValues)
	{
		if (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE)
		{
			CascadeGather::gatherPlotInto(kPlot, kValues);
		}
		else
		{
			kPlot.getCascadePackage().readValuesInto(kValues);
			oe_stampIdentity(kValues, CASC_SCOPE_PLOT, kPlot.getX(), kPlot.getY());
		}
	}
}

CvString OracleEndpoints::cascadePackages(int iPlayer, int iCity, OracleSide eSide)
{
	const PlayerTypes ePlayer = oe_resolvePlayer(iPlayer);
	if (ePlayer == NO_PLAYER)
	{
		return oe_error("no player");
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	picojson::value::array kPackages;
	CvCascadeSlotValues kValues;

	oe_fillEmpire(kPlayer, eSide, kValues);
	kPackages.push_back(oe_renderSlotValues(kValues));

	const TeamTypes eTeam = kPlayer.getTeam();
	if (eTeam >= 0 && eTeam < MAX_TEAMS)
	{
		oe_fillTeam(GET_TEAM(eTeam), eSide, kValues);
		kPackages.push_back(oe_renderSlotValues(kValues));
	}

	for (CvPlayer::city_iterator cityIterator = kPlayer.beginCities(); cityIterator != kPlayer.endCities(); ++cityIterator)
	{
		const CvCity* pLoopCity = *cityIterator;
		if (pLoopCity == NULL)
		{
			continue;
		}
		if (iCity >= 0 && pLoopCity->getID() != iCity)
		{
			continue;
		}
		oe_fillCity(*pLoopCity, eSide, kValues);
		kPackages.push_back(oe_renderSlotValues(kValues));
		// The PLOT scope's way in: one named city's workable plots. Without a city selector the plot rows are
		// left out -- the whole map's plot packages are a different question, asked one city at a time.
		if (iCity < 0)
		{
			continue;
		}
		const int iNumPlots = pLoopCity->getNumCityPlots();
		for (int iPlotIndex = 0; iPlotIndex < iNumPlots; ++iPlotIndex)
		{
			const CvPlot* pLoopPlot = pLoopCity->getCityIndexPlot(iPlotIndex);
			if (pLoopPlot == NULL)
			{
				continue;
			}
			oe_fillPlot(*pLoopPlot, eSide, kValues);
			kPackages.push_back(oe_renderSlotValues(kValues));
		}
	}

	picojson::value::object kRoot;
	kRoot["side"] = picojson::value(std::string(oe_sideName(eSide)));
	kRoot["player"] = picojson::value((double)ePlayer);
	kRoot["packages"] = picojson::value(kPackages);
	return oe_serialize(kRoot);
}

CvString OracleEndpoints::enablerOperating(int iPlayer, int iCity, OracleSide eSide)
{
	const PlayerTypes ePlayer = oe_resolvePlayer(iPlayer);
	if (ePlayer == NO_PLAYER)
	{
		return oe_error("no player");
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	picojson::value::array kCities;
	for (CvPlayer::city_iterator cityIterator = kPlayer.beginCities(); cityIterator != kPlayer.endCities(); ++cityIterator)
	{
		const CvCity* pLoopCity = *cityIterator;
		if (pLoopCity == NULL)
		{
			continue;
		}
		if (iCity >= 0 && pLoopCity->getID() != iCity)
		{
			continue;
		}
		if (eSide == ORACLE_SIDE_ORACLE)
		{
			OperatingBuildings kFresh;   // the oracle's own buffer -- the maintained set is never passed in
			EnablerKernel::recomputeOperatingSetInto(pLoopCity, kFresh);
			kCities.push_back(oe_renderOperatingSet(*pLoopCity, kFresh));
		}
		else
		{
			kCities.push_back(oe_renderOperatingSet(*pLoopCity, EnablerKernel::operatingBuildings(pLoopCity)));
		}
	}

	picojson::value::object kRoot;
	kRoot["side"] = picojson::value(std::string(oe_sideName(eSide)));
	kRoot["player"] = picojson::value((double)ePlayer);
	kRoot["cities"] = picojson::value(kCities);
	return oe_serialize(kRoot);
}

CvString OracleEndpoints::teamCapabilities(int iPlayer, OracleSide eSide)
{
	const PlayerTypes ePlayer = oe_resolvePlayer(iPlayer);
	if (ePlayer == NO_PLAYER)
	{
		return oe_error("no player");
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	if (kPlayer.getTeam() == NO_TEAM)
	{
		return oe_error("no team");
	}

	picojson::value::object kRoot;
	kRoot["side"] = picojson::value(std::string(oe_sideName(eSide)));
	kRoot["player"] = picojson::value((double)ePlayer);
	if (eSide == ORACLE_SIDE_ORACLE)
	{
		CapabilityContext kFresh;   // the oracle's own buffer -- the stored union is never passed in
		CapabilityContext::recomputeInto(kPlayer, kFresh);
		kRoot["capabilityUnion"] = oe_renderCapabilities((int)ePlayer, kFresh);
	}
	else
	{
		kRoot["capabilityUnion"] = oe_renderCapabilities((int)ePlayer, kPlayer.capabilities());
	}
	return oe_serialize(kRoot);
}

// ---- THE CITY YIELD CENSUS -- the tooltip's own document, served ------------------------------------------------

CvString OracleEndpoints::cityYield(int iPlayer, int iCity)
{
	const PlayerTypes ePlayer = oe_resolvePlayer(iPlayer);
	if (ePlayer == NO_PLAYER)
	{
		return oe_error("no player");
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	picojson::value::array kCities;
	for (CvPlayer::city_iterator cityIterator = kPlayer.beginCities();
		cityIterator != kPlayer.endCities(); ++cityIterator)
	{
		const CvCity* pLoopCity = *cityIterator;
		if (pLoopCity == NULL || (iCity >= 0 && pLoopCity->getID() != iCity))
		{
			continue;
		}
		picojson::value::object kCity;
		kCity["city"] = picojson::value((double)pLoopCity->getID());
		kCity["name"] = picojson::value(std::string(CvString(pLoopCity->getName()).c_str()));
		kCity["population"] = picojson::value((double)pLoopCity->getPopulation());

		// THE TWO BONUS LISTS, LIVE. Read here rather than trusted from the load-end census, because that is
		// exactly the difference that has been invisible: full at load, empty in play, and only the second one
		// is what a deposit gate actually asks.
		std::vector<int> kTraded;
		std::vector<int> kOnSite;
		pLoopCity->getCityContext().collectBonusStores(kTraded, kOnSite);
		picojson::value::array kTradedNames;
		picojson::value::array kOnSiteNames;
		size_t iIndex = 0;
		for (iIndex = 0; iIndex < kTraded.size(); ++iIndex)
		{
			kTradedNames.push_back(picojson::value(std::string(GC.getBonusInfo((BonusTypes)kTraded[iIndex]).getType())));
		}
		for (iIndex = 0; iIndex < kOnSite.size(); ++iIndex)
		{
			kOnSiteNames.push_back(picojson::value(std::string(GC.getBonusInfo((BonusTypes)kOnSite[iIndex]).getType())));
		}
		kCity["tradedCount"] = picojson::value((double)kTraded.size());
		kCity["onSiteCount"] = picojson::value((double)kOnSite.size());
		kCity["traded"] = picojson::value(kTradedNames);
		kCity["onSite"] = picojson::value(kOnSiteNames);

		// EVERY TERM OF THE COMBINE, per yield -- out of the real combine, never re-derived beside it
		// ([DEC-single-implementation]).
		picojson::value::object kYields;
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			const int iChannel = CascadeChannelRegistry::channelLookup(
				infoYieldFamily((YieldTypes)iYield), (int)CHANNEL_AMOUNT, -1);
			if (iChannel < 0)
			{
				continue;
			}
			InfoValuation::CityRateTerms kTerms;
			InfoValuation::cityReceiverRate(*pLoopCity, iChannel, &kTerms);
			picojson::value::object kTermsOut;
			kTermsOut["plotBase"] = picojson::value((double)kTerms.plotBase);
			kTermsOut["plotNature"] = picojson::value((double)kTerms.plotNature);
			kTermsOut["plotImprovement"] = picojson::value((double)kTerms.plotImprovement);
			kTermsOut["plotRest"] = picojson::value((double)kTerms.plotRest);
			kTermsOut["workedPlots"] = picojson::value((double)kTerms.workedPlots);
			kTermsOut["tradeYield"] = picojson::value((double)kTerms.tradeYield);
			kTermsOut["goldenAge"] = picojson::value((double)kTerms.goldenAge);
			kTermsOut["upperFlat"] = picojson::value((double)kTerms.upperFlat);
			kTermsOut["specialists"] = picojson::value((double)kTerms.specialists);
			kTermsOut["cityFlat"] = picojson::value((double)kTerms.cityFlat);
			kTermsOut["percentSum"] = picojson::value((double)kTerms.percentSum);
			kTermsOut["rate"] = picojson::value((double)kTerms.rate);

			// THE PERCENT STACK, SPLIT BY SCOPE. percentSum alone cannot say which LEVEL a modifier came from,
			// and the three move for completely different reasons -- so a stack that looks too big is
			// unattributable without this.
			kTermsOut["percentCity"] = picojson::value((double)pLoopCity->getCascadePackage().readPercent(iChannel));
			kTermsOut["percentEmpire"] = picojson::value((double)kPlayer.getCascadePackage().readPercent(iChannel));
			kTermsOut["percentTeam"] = picojson::value(
				(double)GET_TEAM(kPlayer.getTeam()).getCascadePackage().readPercent(iChannel));

			// EVERY city-scope entry the city's ACTIVE buildings author for this channel -- APPLIED and REFUSED.
			// ⛔ Both halves, because one alone answers nothing: a refusal list shows what is missing and hides
			// what is WRONG, and a percent that should not be applying is exactly as invisible in a total as one
			// that should be and is not. The applied Σ below is what the city-scope slot should hold, so this is
			// RECONCILABLE against the number it explains rather than merely narrating beside it.
			std::vector<InfoValuation::RefusedDeposit> kAudit;
			InfoValuation::cityRefusedDeposits(*pLoopCity, iChannel, kAudit);
			int64_t iAppliedFlat = 0;
			int64_t iAppliedPercent = 0;
			int iRefusedCount = 0;
			picojson::value::array kAppliedOut;
			picojson::value::array kAppliedPercentOut;
			picojson::value::array kRefusedOut;
			for (size_t iEntry = 0; iEntry < kAudit.size(); ++iEntry)
			{
				const InfoValuation::RefusedDeposit& kOneEntry = kAudit[iEntry];
				picojson::value::object kOne;
				kOne["source"] = picojson::value(std::string(kOneEntry.szSource != NULL ? kOneEntry.szSource : "?"));
				kOne["value"] = picojson::value((double)kOneEntry.iValue);
				kOne["percent"] = picojson::value(kOneEntry.bPercentSide);
				kOne["needs"] = picojson::value(std::string(
					(kOneEntry.pCondition != NULL && !kOneEntry.pCondition->type.empty())
						? kOneEntry.pCondition->type.c_str() : (kOneEntry.pCondition != NULL ? "<predicate>" : "")));
				if (kOneEntry.bApplied)
				{
					if (kOneEntry.bPercentSide) iAppliedPercent += kOneEntry.iValue;
					else                        iAppliedFlat += kOneEntry.iValue;
					// capped PER SIDE: one shared cap let the flats crowd the percents out entirely, so the
					// listing showed none of the half that multiplies
					if (kOneEntry.bPercentSide ? (kAppliedPercentOut.size() < 40) : (kAppliedOut.size() < 40))
					{
						if (kOneEntry.bPercentSide) kAppliedPercentOut.push_back(picojson::value(kOne));
						else                        kAppliedOut.push_back(picojson::value(kOne));
					}
				}
				else
				{
					++iRefusedCount;
					if (kRefusedOut.size() < 60) kRefusedOut.push_back(picojson::value(kOne));
				}
			}
			kTermsOut["auditCount"] = picojson::value((double)kAudit.size());
			kTermsOut["appliedFlatSum"] = picojson::value((double)iAppliedFlat);
			kTermsOut["appliedPercentSum"] = picojson::value((double)iAppliedPercent);
			kTermsOut["refusedCount"] = picojson::value((double)iRefusedCount);
			kTermsOut["applied"] = picojson::value(kAppliedOut);
			kTermsOut["appliedPercent"] = picojson::value(kAppliedPercentOut);
			kTermsOut["refused"] = picojson::value(kRefusedOut);
			kYields[std::string(GC.getYieldInfo((YieldTypes)iYield).getType())] = picojson::value(kTermsOut);
		}
		kCity["yields"] = picojson::value(kYields);

		// ⛔ THE WORKED-PLOT CENSUS -- each tile with WHAT IS ON IT beside WHAT IT YIELDS. Raw x/y could not
		// answer the only question that matters here ("is 5.9 food a correct number for THIS tile?"), because a
		// yield is only checkable against its substrate. The plot plane has no served surface of its own, so this
		// is the one place a tile's stored package can be read at all.
		picojson::value::array kPlotsOut;
		const int iNumCityPlots = pLoopCity->getNumCityPlots();
		for (int iPlotIndex = 0; iPlotIndex < iNumCityPlots; ++iPlotIndex)
		{
			const CvPlot* pPlot = pLoopCity->getCityIndexPlot(iPlotIndex);
			if (pPlot == NULL)
			{
				continue;
			}
			picojson::value::object kPlotOut;
			kPlotOut["x"] = picojson::value((double)pPlot->getX());
			kPlotOut["y"] = picojson::value((double)pPlot->getY());
			kPlotOut["worked"] = picojson::value(pLoopCity->isWorkingPlot(iPlotIndex));
			kPlotOut["terrain"] = picojson::value(std::string(pPlot->getTerrainType() != NO_TERRAIN
				? GC.getTerrainInfo(pPlot->getTerrainType()).getType() : ""));
			kPlotOut["feature"] = picojson::value(std::string(pPlot->getFeatureType() != NO_FEATURE
				? GC.getFeatureInfo(pPlot->getFeatureType()).getType() : ""));
			kPlotOut["bonus"] = picojson::value(std::string(pPlot->getBonusType(NO_TEAM) != NO_BONUS
				? GC.getBonusInfo(pPlot->getBonusType(NO_TEAM)).getType() : ""));
			kPlotOut["improvement"] = picojson::value(std::string(pPlot->getImprovementType() != NO_IMPROVEMENT
				? GC.getImprovementInfo(pPlot->getImprovementType()).getType() : ""));
			kPlotOut["route"] = picojson::value(std::string(pPlot->getRouteType() != NO_ROUTE
				? GC.getRouteInfo(pPlot->getRouteType()).getType() : ""));
			picojson::value::object kPlotYields;
			for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
			{
				const int iPlotChannel = CascadeChannelRegistry::channelLookup(
					infoYieldFamily((YieldTypes)iYield), (int)CHANNEL_AMOUNT, -1);
				if (iPlotChannel < 0)
				{
					continue;
				}
				picojson::value::object kOneYield;
				kOneYield["total"] = picojson::value((double)pPlot->getCascadePackage().readFlat(iPlotChannel));
				kOneYield["nature"] = picojson::value((double)pPlot->getCascadePackage().readSubstrateFlat(iPlotChannel));
				kOneYield["improvement"] = picojson::value((double)pPlot->getCascadePackage().readImprovementFlat(iPlotChannel));
				kOneYield["rest"] = picojson::value((double)pPlot->getCascadePackage().readRestFlat(iPlotChannel));
				kPlotYields[std::string(GC.getYieldInfo((YieldTypes)iYield).getType())] = picojson::value(kOneYield);
			}
			kPlotOut["yields"] = picojson::value(kPlotYields);
			kPlotsOut.push_back(picojson::value(kPlotOut));
		}
		kCity["plots"] = picojson::value(kPlotsOut);
		kCities.push_back(picojson::value(kCity));
	}
	picojson::value::object kRoot;
	kRoot["player"] = picojson::value((double)ePlayer);
	kRoot["cities"] = picojson::value(kCities);
	return oe_serialize(kRoot);
}
