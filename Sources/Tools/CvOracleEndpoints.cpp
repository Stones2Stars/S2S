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
#include "CvTerrainInfo.h"
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

	void oe_fillCity(const CvCity& kCity, OracleEndpoints::OracleSide eSide, CvCascadeSlotValues& kValues)
	{
		if (eSide == OracleEndpoints::ORACLE_SIDE_ORACLE)
		{
			CascadeGather::gatherCityInto(kCity, kValues);
		}
		else
		{
			kCity.getCascadePackage().readValuesInto(kValues);
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
