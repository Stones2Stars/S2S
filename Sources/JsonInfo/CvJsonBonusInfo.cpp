//
//	CvJsonBonusInfo::mapFrom -- base core reading + availability (enables.* rides the base), then the bonus's real
//	values + map-gen placement. FK resolution via the kept type registry. ⏳-flagged shapes to confirm. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonBonusInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvJsonBonusInfo::CvJsonBonusInfo()
	: m_iBonusClassType(-1), m_iHealth(0), m_iHappiness(0),
	  m_iMinAreaSize(0), m_iMinLatitude(0), m_iMaxLatitude(90), m_iPlacementOrder(0), m_iTilesPer(0),
	  m_iUniqueRange(0), m_iGroupRange(0), m_iGroupRand(0),
	  m_bOneArea(false), m_bHills(false), m_bPeaks(false), m_bFlatlands(false), m_bBonusCoastalOnly(false),
	  m_bNoRiverSide(false), m_bNormalize(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

void CvJsonBonusInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (enables.units/buildings)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");
	m_iHealth    = jsonFamVal(o, "health", "empire", "flat");     // ⏳ scope to confirm (bonus presence-gated)
	m_iHappiness = jsonFamVal(o, "happiness", "empire", "flat");  // ⏳

	if (const picojson::object* mg = jsonChildObj(o, "mapGeneration"))
	{
		m_iMinAreaSize     = jsonIdInt(*mg, "minAreaSize");
		m_iMinLatitude     = jsonIdInt(*mg, "minLatitude");
		m_iMaxLatitude     = jsonIdInt(*mg, "maxLatitude");
		m_iPlacementOrder  = jsonIdInt(*mg, "placementOrder");
		m_iTilesPer        = jsonIdInt(*mg, "tilesPer");
		m_iUniqueRange     = jsonIdInt(*mg, "uniqueRange");
		m_iGroupRange      = jsonIdInt(*mg, "groupRange");
		m_iGroupRand       = jsonIdInt(*mg, "groupRand");
		m_bOneArea         = jsonIdBool(*mg, "area");              // curate_bonus BONUS_MAP_GEN: bArea -> area
		m_bHills           = jsonIdBool(*mg, "hills");
		m_bPeaks           = jsonIdBool(*mg, "peaks");
		m_bFlatlands       = jsonIdBool(*mg, "flatlands");
		m_bBonusCoastalOnly= jsonIdBool(*mg, "bonusCoastalOnly");  // bBonusCoastalOnly -> bonusCoastalOnly
		m_bNoRiverSide     = jsonIdBool(*mg, "noRiverSide");
		m_bNormalize       = jsonIdBool(*mg, "normalize");
	}

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iBonusClassType = jsonIdFk(*io, "bonusClassType");
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
}
