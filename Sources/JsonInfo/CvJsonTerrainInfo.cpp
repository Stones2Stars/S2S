//
//	CvJsonTerrainInfo::mapFrom -- base core reading (type + identity text + button) then the terrain's LIVE real members
//	mapped from JSON: the plot-scope yield families, the plot modifier families, and the `identity` terrain fields.
//	HUMAN-native values (the cascade ×100s on its own side). FK resolution via jsonResolveId (the JsonInfo-side
//	CvJsonParse primitive over the kept type registry, with the load-time diagnostics). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonTerrainInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)

CvJsonTerrainInfo::CvJsonTerrainInfo()
	: m_iMovementCost(0), m_iBuildModifier(0), m_iDefenseModifier(0), m_iCultureDistance(0),
	  m_iDistanceToLand(0), m_iZobristValue(0), m_bFreshWaterTerrain(false), m_eClimate(NO_CLIMATE_ZONE)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYields[i] = 0;
}

void CvJsonTerrainInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading: type + identity text + button
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// yields: the plot-scope flat of each split yield family (human ints)
	m_aiYields[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYields[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYields[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");

	// plot modifier families
	m_iBuildModifier   = jsonFamVal(o, "buildTime", "plot", "percent");
	m_iCultureDistance = jsonFamVal(o, "cultureDistance", "plot", "flat");
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");

	// identity: the terrain relief + climate fields
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iMovementCost      = jsonIdInt(*io, "movementCost");
		m_iDistanceToLand    = jsonIdInt(*io, "distanceToLand");
		m_bFreshWaterTerrain = jsonIdBool(*io, "freshWaterTerrain");

		picojson::object::const_iterator cl = io->find("climate");
		if (cl != io->end() && cl->second.is<std::string>())
			m_eClimate = (ClimateZoneTypes)jsonResolveId(cl->second.get<std::string>());

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					const int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id);
				}
		}
	}

	// ⏳ m_iZobristValue: CvPlot reads getZobristValue() for the map hash -- it needs the EXACT legacy zobrist
	// computation (OOS-load-bearing), not a stand-in. Left 0 pending that port; flagged, not silently faked.
}
