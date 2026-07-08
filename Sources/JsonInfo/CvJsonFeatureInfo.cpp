//
//	CvJsonFeatureInfo::mapFrom -- base core reading + availability, then the feature's LIVE real members from the
//	curator's real shapes: the plot yield/health/defense/culture/vision families and the `identity` placement fields.
//	HUMAN-native values (the cascade ×100s on its side). FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonFeatureInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvJsonFeatureInfo::CvJsonFeatureInfo()
	: m_iMovementCost(0), m_iDefenseModifier(0), m_iHealthPercent(0), m_iCultureDistance(0),
	  m_iSeeThroughChange(0), m_iPopDestroys(0), m_iZobristValue(0),
	  m_bImpassable(false), m_bNoCity(false), m_bNoImprovement(false), m_bNoBonus(false), m_bCountsAsPeak(false),
	  m_bRequiresFlatlands(false), m_bAddsFreshWater(false), m_bNukeImmune(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

bool CvJsonFeatureInfo::isTerrain(int iTerrain) const
{
	for (size_t i = 0; i < m_aeValidTerrains.size(); ++i) if ((int)m_aeValidTerrains[i] == iTerrain) return true;
	return false;
}

void CvJsonFeatureInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability model
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot families
	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");
	m_iCultureDistance = jsonFamVal(o, "cultureDistance", "plot", "flat");
	m_iHealthPercent   = jsonFamVal(o, "health", "plot", "percent");
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");
	m_iSeeThroughChange = jsonFamMemberVal(o, "vision", "plot", "seeThrough", "flat");

	// identity: placement + relief fields
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iMovementCost      = jsonIdInt(*io, "movementCost");
		m_iPopDestroys       = jsonIdInt(*io, "popDestroys");
		m_bImpassable        = jsonIdBool(*io, "impassable");
		m_bNoCity            = jsonIdBool(*io, "noCity");
		m_bNoImprovement     = jsonIdBool(*io, "noImprovement");
		m_bNoBonus           = jsonIdBool(*io, "noBonus");
		m_bCountsAsPeak      = jsonIdBool(*io, "countsAsPeak");
		m_bRequiresFlatlands = jsonIdBool(*io, "requiresFlatlands");
		m_bAddsFreshWater    = jsonIdBool(*io, "addsFreshWater");
		m_bNukeImmune        = jsonIdBool(*io, "nukeImmune");

		picojson::object::const_iterator vt = io->find("validTerrains");
		if (vt != io->end() && vt->second.is<picojson::array>())
		{
			const picojson::array& a = vt->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeValidTerrains.push_back((TerrainTypes)id); }
		}
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
	// ⏳ m_iZobristValue left 0 (needs the exact legacy zobrist map-hash, OOS) -- flagged not faked.
}
