//
//	CvBonusInfo -- the bonus poco's own typed reading on top of the base section dispatch (see the header).
//	The plot-output and wellbeing families compile into m_modifiers via the base dispatch -- no per-family raw
//	read survives here (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)). mapFrom materializes the identity/ai/mapGeneration census
//	set ONCE into typed members (docs/architecture/patterns.md §Materialize at mapFrom); idempotent by contract. The tech forward FKs
//	are reset here and re-landed by CvReversePass after every re-map.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvBonusInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoBonus.h" // complete CvArtInfoBonus -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/jsonIdBool/jsonIdFk/jsonIdStr/jsonWorldArt)
#include "Defines/CvGlobals.h"    // GC (getGame)
#include "AI/CvGameAI.h"          // complete CvGameAI (GC.getGame()) -> CvGame::getMapRandNum, the map-gen appearance roll

namespace
{
	// mapGeneration.{validTerrains,validFeatures,validPlacementOn} -- FK string arrays -> resolved id sets.
	void bi_readFkIntSet(const picojson::object& parent, const char* szKey, std::set<int>& idsOut)
	{
		idsOut.clear();
		picojson::object::const_iterator listIter = parent.find(szKey);
		if (listIter == parent.end() || !listIter->second.is<picojson::array>())
		{
			return;
		}
		const picojson::array& idList = listIter->second.get<picojson::array>();
		for (size_t iEntry = 0; iEntry < idList.size(); ++iEntry)
		{
			if (!idList[iEntry].is<std::string>())
			{
				continue;
			}
			const int iResolved = jsonResolveId(idList[iEntry].get<std::string>());
			if (iResolved >= 0)
			{
				idsOut.insert(iResolved);
			}
		}
	}
}

CvBonusInfo::CvBonusInfo()
	: m_iBonusClassType(-1)
	, m_iAIObjective(0)   // legacy load default 0 (plain .add) -- -1 fed unclamped AI valuation
	, m_iAITradeModifier(0)
	, m_iPercentPerPlayer(0)
	, m_iChar(0)
	, m_iMinAreaSize(0)
	, m_iMinLatitude(0)
	, m_iMaxLatitude(90)
	, m_iPlacementOrder(-1)
	, m_iTilesPer(0)
	, m_iUniqueRange(0)
	, m_iGroupRange(0)
	, m_iGroupRand(0)
	, m_iConstAppearance(0)
	, m_iMinLandPercent(0)
	, m_bOneArea(false)
	, m_bHills(false)
	, m_bPeaks(false)
	, m_bFlatlands(false)
	, m_bBonusCoastalOnly(false)
	, m_bNoRiverSide(false)
	, m_bNormalize(false)
	, m_eTechReveal(NO_TECH)
	, m_eTechCityTrade(NO_TECH)
	, m_eTechObsolete(NO_TECH)
{
	for (int iBand = 0; iBand < NUM_RAND_APPEARANCE_BANDS; ++iBand)
	{
		m_aiRandAppearance[iBand] = 0;
	}
}

void CvBonusInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom. The tech forward FKs reset here
	// because CvReversePass re-lands them AFTER every re-map (its first-tech-wins check reads NO_TECH).
	// NB m_providedByImprovementTypes / m_tradeProvidingImprovements are NOT cleared here -- the general
	// reverse pass owns them (clear-first), not this parse.
	m_aeMapCategories.clear();
	m_eTechReveal = NO_TECH;
	m_eTechCityTrade = NO_TECH;
	m_eTechObsolete = NO_TECH;

	CvInfo::mapFrom(entity);   // core reading + availability + the modifier compile (plot yields / wellbeing)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// ai.behaviour.{objective,tradeModifier} -- real per-bonus AI weights. objective is read manually so an
	// ABSENT block leaves the constructor default instead of collapsing through a helper default.
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		if (const picojson::object* pBehaviour = jsonChildObj(*pAi, "behaviour"))
		{
			picojson::object::const_iterator objectiveIter = pBehaviour->find("objective");
			if (objectiveIter != pBehaviour->end() && objectiveIter->second.is<double>())
			{
				m_iAIObjective = (int)objectiveIter->second.get<double>();
			}
			m_iAITradeModifier = jsonIdInt(*pBehaviour, "tradeModifier");
		}
	}

	if (const picojson::object* pMapGen = jsonChildObj(entityObj, "mapGeneration"))
	{
		m_iMinAreaSize = jsonIdInt(*pMapGen, "minAreaSize");
		m_iMinLatitude = jsonIdInt(*pMapGen, "minLatitude");
		m_iMaxLatitude = jsonIdInt(*pMapGen, "maxLatitude", 90);      // legacy load default 90 -- 0 equator-locks placement
		m_iPlacementOrder = jsonIdInt(*pMapGen, "placementOrder", -1);// legacy load default -1 = not a map-placed bonus
		m_iTilesPer = jsonIdInt(*pMapGen, "tilesPer");
		m_iMinLandPercent = jsonIdInt(*pMapGen, "minLandPercent");
		m_iConstAppearance = jsonIdInt(*pMapGen, "constAppearance");
		m_iUniqueRange = jsonIdInt(*pMapGen, "uniqueRange");
		m_iGroupRange = jsonIdInt(*pMapGen, "groupRange");
		m_iGroupRand = jsonIdInt(*pMapGen, "groupRand");

		// mapGeneration.rands.{iRandApp1..4} -- the per-pass appearance band CEILINGS the map generator rolls
		// against. Absent -> the 0 defaults below hold (fully redefined every map, mapFrom being idempotent).
		for (int iBand = 0; iBand < NUM_RAND_APPEARANCE_BANDS; ++iBand)
		{
			m_aiRandAppearance[iBand] = 0;
		}
		if (const picojson::object* pRands = jsonChildObj(*pMapGen, "rands"))
		{
			static const char* const szRandBandKeys[NUM_RAND_APPEARANCE_BANDS] =
			{ "iRandApp1", "iRandApp2", "iRandApp3", "iRandApp4" };
			for (int iBand = 0; iBand < NUM_RAND_APPEARANCE_BANDS; ++iBand)
			{
				m_aiRandAppearance[iBand] = jsonIdInt(*pRands, szRandBandKeys[iBand]);
			}
		}
		m_bOneArea = jsonIdBool(*pMapGen, "area");
		m_bHills = jsonIdBool(*pMapGen, "hills");
		m_bPeaks = jsonIdBool(*pMapGen, "peaks");
		m_bFlatlands = jsonIdBool(*pMapGen, "flatlands");
		m_bBonusCoastalOnly = jsonIdBool(*pMapGen, "bonusCoastalOnly");
		m_bNoRiverSide = jsonIdBool(*pMapGen, "noRiverSide");
		m_bNormalize = jsonIdBool(*pMapGen, "normalize");

		// legacy TerrainBooleans/FeatureBooleans/FeatureTerrainBooleans -> the map-gen placement predicates
		bi_readFkIntSet(*pMapGen, "validTerrains", m_terrainSet);
		bi_readFkIntSet(*pMapGen, "validFeatures", m_featureSet);
		bi_readFkIntSet(*pMapGen, "validPlacementOn", m_featureTerrainSet);
	}

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iBonusClassType = jsonIdFk(*pIdentity, "bonusClassType");
		m_iPercentPerPlayer = jsonIdInt(*pIdentity, "player");   // legacy iPlayer; 0 today (no bonus authors it)
		picojson::object::const_iterator categoriesIter = pIdentity->find("mapCategories");
		if (categoriesIter != pIdentity->end() && categoriesIter->second.is<picojson::array>())
		{
			const picojson::array& categoryList = categoriesIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < categoryList.size(); ++iEntry)
			{
				if (!categoryList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(categoryList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aeMapCategories.push_back((MapCategoryTypes)iResolved);
				}
			}
		}
	}

	// world.art.icon -- the ART_DEF_* tag the EXE map-gen art lookup keys on. Cleared first: jsonIdStr only
	// assigns when the key is present, so without this an absent block would leave the PREVIOUS pass's tag
	// standing on the re-map (the mapFrom idempotency contract, CvInfo.h).
	m_szArtDefineTag.clear();
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);
	}
}

void CvBonusInfo::setProvidedByImprovementTypes(const ImprovementTypes eType)
{
	// Populated post-load by the general reverse pass, once per improvement whose
	// isImprovementBonusTrade flags this bonus. Real runtime data -- feeds the get/num/is read surface.
	m_providedByImprovementTypes.push_back(eType);
}

const CvArtInfoBonus* CvBonusInfo::getArtInfo() const
{
	return ARTFILEMGR.getBonusArtInfo(getArtDefineTag());
}

const char* CvBonusInfo::getButton() const
{
	const CvArtInfoBonus* pArtInfo = getArtInfo();
	return pArtInfo != NULL ? pArtInfo->getButton() : "";
}
