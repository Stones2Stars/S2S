//
//	CvBonusInfo::mapFrom -- base core reading + availability (enables.* rides the base), then the bonus's real
//	values + map-gen placement. FK resolution via the kept type registry. See header for field->curator addresses.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvBonusInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoBonus.h"   // complete CvArtInfoBonus -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)
#include "Defines/CvGlobals.h"    // GC (getGame)
#include "AI/CvGameAI.h"          // complete CvGameAI (GC.getGame()) -> CvGame::getMapRandNum, the map-gen appearance roll

// mapGeneration.{validTerrains,validFeatures,validPlacementOn} -- FK string arrays -> resolved id sets (mirrors
// CvTechInfo.cpp's canTradeOnTerrains loop; kept local here since only this poco composes all three).
static void jsonReadFkIntSet(const picojson::object& o, const char* key, std::set<int>& out)
{
	picojson::object::const_iterator it = o.find(key);
	if (it == o.end() || !it->second.is<picojson::array>()) return;
	const picojson::array& a = it->second.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) out.insert(id); }
}

CvBonusInfo::CvBonusInfo()
	: m_iBonusClassType(-1), m_iAIObjective(0) /* legacy load default 0 (plain .add) -- -1 fed unclamped AI valuation */, m_iAITradeModifier(0), m_iHealth(0), m_iHappiness(0), m_iChar(0),
	  m_iMinAreaSize(0), m_iMinLatitude(0), m_iMaxLatitude(90), m_iPlacementOrder(-1), m_iTilesPer(0),
	  m_iUniqueRange(0), m_iGroupRange(0), m_iGroupRand(0),
	  m_iConstAppearance(0), m_iRandAppearance1(0), m_iRandAppearance2(0), m_iRandAppearance3(0), m_iRandAppearance4(0),
	  m_iMinLandPercent(0), m_iPercentPerPlayer(0),
	  m_bOneArea(false), m_bHills(false), m_bPeaks(false), m_bFlatlands(false), m_bBonusCoastalOnly(false),
	  m_bNoRiverSide(false), m_bNormalize(false),
	  m_eTechReveal(NO_TECH), m_eTechCityTrade(NO_TECH), m_eTechObsolete(NO_TECH)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

void CvBonusInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom. NB m_providedByImprovementTypes is
	// NOT cleared here -- it is populated by a separate derived-cache pass, not by this parse.
	m_aeMapCategories.clear();
	CvInfo::mapFrom(entity);   // core reading + availability (enables.units/buildings)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");
	m_iHealth    = jsonFamVal(o, "health", "empire", "flat");     // curate_bonus.py BONUS_FAMILIES: iHealth -> health.empire.flat
	m_iHappiness = jsonFamVal(o, "happiness", "empire", "flat");  // curate_bonus.py BONUS_FAMILIES: iHappiness -> happiness.empire.flat

	// ai.behaviour.{objective,tradeModifier} -- real per-bonus AI weights (curate_bonus.py; e.g. BONUS_CHEMICALS
	// objective=10, BONUS_OIL tradeModifier=20). objective is read manually (not via jsonIdInt) so an ABSENT block
	// leaves the -1 constructor sentinel instead of collapsing to 0; tradeModifier's 0-default is correct.
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		if (const picojson::object* be = jsonChildObj(*ai, "behaviour"))
		{
			picojson::object::const_iterator obj = be->find("objective");
			if (obj != be->end() && obj->second.is<double>()) m_iAIObjective = (int)obj->second.get<double>();
			m_iAITradeModifier = jsonIdInt(*be, "tradeModifier");
		}
	}

	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
	{
		m_iMinAreaSize     = jsonIdInt(*mg, "minAreaSize");
		m_iMinLatitude     = jsonIdInt(*mg, "minLatitude");
		m_iMaxLatitude     = jsonIdInt(*mg, "maxLatitude", 90);      // legacy load default 90 (archive .add) -- 0 equator-locks placement
		m_iPlacementOrder  = jsonIdInt(*mg, "placementOrder", -1);   // legacy load default -1 = not a map-placed bonus
		m_iTilesPer        = jsonIdInt(*mg, "tilesPer");
		m_iMinLandPercent  = jsonIdInt(*mg, "minLandPercent");
		m_iConstAppearance = jsonIdInt(*mg, "constAppearance");
		m_iUniqueRange     = jsonIdInt(*mg, "uniqueRange");
		m_iGroupRange      = jsonIdInt(*mg, "groupRange");
		m_iGroupRand       = jsonIdInt(*mg, "groupRand");

		// mapGeneration.rands.{iRandApp1..4} -- the per-pass appearance dice (summed with constAppearance by
		// getRandAppearance). Absent -> the 0 constructor defaults hold.
		if (const picojson::object* rd = jsonChildObj(*mg, "rands"))
		{
			m_iRandAppearance1 = jsonIdInt(*rd, "iRandApp1");
			m_iRandAppearance2 = jsonIdInt(*rd, "iRandApp2");
			m_iRandAppearance3 = jsonIdInt(*rd, "iRandApp3");
			m_iRandAppearance4 = jsonIdInt(*rd, "iRandApp4");
		}
		m_bOneArea         = jsonIdBool(*mg, "area");              // curate_bonus BONUS_MAP_GEN: bArea -> area
		m_bHills           = jsonIdBool(*mg, "hills");
		m_bPeaks           = jsonIdBool(*mg, "peaks");
		m_bFlatlands       = jsonIdBool(*mg, "flatlands");
		m_bBonusCoastalOnly= jsonIdBool(*mg, "bonusCoastalOnly");  // bBonusCoastalOnly -> bonusCoastalOnly
		m_bNoRiverSide     = jsonIdBool(*mg, "noRiverSide");
		m_bNormalize       = jsonIdBool(*mg, "normalize");

		// legacy TerrainBooleans/FeatureBooleans/FeatureTerrainBooleans, renamed by engine.py -> the map-gen
		// placement predicates (isTerrain/isFeature/isFeatureTerrain).
		jsonReadFkIntSet(*mg, "validTerrains", m_terrainSet);
		jsonReadFkIntSet(*mg, "validFeatures", m_featureSet);
		jsonReadFkIntSet(*mg, "validPlacementOn", m_featureTerrainSet);
	}

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iBonusClassType = jsonIdFk(*io, "bonusClassType");
		m_iPercentPerPlayer = jsonIdInt(*io, "player");   // legacy iPlayer -> identity.player (de_i); 0 today (no bonus authors it)
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}

	// world.art.icon -- the ART_DEF_* tag the EXE map-gen art lookup keys on (via the CvBonusInfo shim's getArtInfo)
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "define", m_szArtDefineTag);
}

void CvBonusInfo::setProvidedByImprovementTypes(const ImprovementTypes eType)
{
	// Populated post-load by CvGlobals's derived-caching pass, once per improvement whose isImprovementBonusTrade
	// flags this bonus (CvGlobals.cpp ~L3200). Real runtime data -- feeds the get/num/is read surface.
	m_providedByImprovementTypes.push_back(eType);
}

int CvBonusInfo::getRandAppearance() const
{
	// Map-gen placement roll (faithful mirror of the archived class): the flat constAppearance plus one RNG draw per
	// rand band. GC.getGame().getMapRandNum(n, tag) returns [0,n) off the deterministic map RNG.
	return m_iConstAppearance
		+ GC.getGame().getMapRandNum(m_iRandAppearance1, "random1")
		+ GC.getGame().getMapRandNum(m_iRandAppearance2, "random2")
		+ GC.getGame().getMapRandNum(m_iRandAppearance3, "random3")
		+ GC.getGame().getMapRandNum(m_iRandAppearance4, "random4");
}

const CvArtInfoBonus* CvBonusInfo::getArtInfo() const
{
	return ARTFILEMGR.getBonusArtInfo(getArtDefineTag());
}
const char* CvBonusInfo::getButton() const
{
	const CvArtInfoBonus* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
