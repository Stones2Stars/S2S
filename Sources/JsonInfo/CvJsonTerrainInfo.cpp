//
//	CvJsonTerrainInfo::mapFrom -- base core reading (type + identity text + button) then the terrain's LIVE real members
//	mapped from JSON: the plot-scope yield families, the plot modifier families, and the `identity` terrain fields.
//	HUMAN-native values (the cascade ×100s on its own side). FK resolution via the kept type registry
//	(GC.getInfoTypeForString) -- NOT a cascade helper (CvJsonInfo has zero cascade dependency). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonTerrainInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- the kept type registry (climate / mapCategory FKs)

CvJsonTerrainInfo::CvJsonTerrainInfo()
	: m_iMovementCost(0), m_iBuildModifier(0), m_iDefenseModifier(0), m_iCultureDistance(0),
	  m_iDistanceToLand(0), m_iZobristValue(0), m_bFreshWaterTerrain(false), m_eClimate(NO_CLIMATE_ZONE)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYields[i] = 0;
}

// --- local JSON helpers (no cascade dependency) ---
static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
// entity[family][scope][unit] as a human int (0 if any hop is missing).
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
// entity[family][scope][member][unit] (the grouped-family case, e.g. defense.plot.amount.percent).
static int fam_member_val(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);   if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope);  if (!so) return 0;
	const picojson::object* mo = child_obj(*so, member); if (!mo) return 0;
	picojson::object::const_iterator u = mo->find(unit);
	return (u != mo->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
static int  id_int (const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }

void CvJsonTerrainInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading: type + identity text + button
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// yields: the plot-scope flat of each split yield family (human ints)
	m_aiYields[YIELD_FOOD]       = fam_val(o, "food", "plot", "flat");
	m_aiYields[YIELD_PRODUCTION] = fam_val(o, "production", "plot", "flat");
	m_aiYields[YIELD_COMMERCE]   = fam_val(o, "commerce", "plot", "flat");

	// plot modifier families
	m_iBuildModifier   = fam_val(o, "buildTime", "plot", "percent");
	m_iCultureDistance = fam_val(o, "cultureDistance", "plot", "flat");
	m_iDefenseModifier = fam_member_val(o, "defense", "plot", "amount", "percent");

	// identity: the terrain relief + climate fields
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iMovementCost      = id_int(*io, "movementCost");
		m_iDistanceToLand    = id_int(*io, "distanceToLand");
		m_bFreshWaterTerrain = id_bool(*io, "freshWaterTerrain");

		picojson::object::const_iterator cl = io->find("climate");
		if (cl != io->end() && cl->second.is<std::string>())
			m_eClimate = (ClimateZoneTypes)GC.getInfoTypeForString(cl->second.get<std::string>().c_str(), true);

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true);
					if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id);
				}
		}
	}

	// ⏳ m_iZobristValue: CvPlot reads getZobristValue() for the map hash -- it needs the EXACT legacy zobrist
	// computation (OOS-load-bearing), not a stand-in. Left 0 pending that port; flagged, not silently faked.
}
