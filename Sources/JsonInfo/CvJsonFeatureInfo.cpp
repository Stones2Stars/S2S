//
//	CvJsonFeatureInfo::mapFrom -- base core reading + availability, then the feature's LIVE real members from the
//	curator's real shapes: the plot yield/health/defense/culture/vision families and the `identity` placement fields.
//	HUMAN-native values (the cascade ×100s on its side). FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonFeatureInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- terrain / mapCategory FKs

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

// --- local JSON helpers (no cascade dependency) ---
static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? (int)u->second.get<double>() : 0;
}
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

void CvJsonFeatureInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability model
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// plot families
	m_aiYieldChange[YIELD_FOOD]       = fam_val(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = fam_val(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = fam_val(o, "commerce", "plot", "flat");
	m_iCultureDistance = fam_val(o, "cultureDistance", "plot", "flat");
	m_iHealthPercent   = fam_val(o, "health", "plot", "percent");
	m_iDefenseModifier = fam_member_val(o, "defense", "plot", "amount", "percent");
	m_iSeeThroughChange = fam_member_val(o, "vision", "plot", "seeThrough", "flat");

	// identity: placement + relief fields
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iMovementCost      = id_int(*io, "movementCost");
		m_iPopDestroys       = id_int(*io, "popDestroys");
		m_bImpassable        = id_bool(*io, "impassable");
		m_bNoCity            = id_bool(*io, "noCity");
		m_bNoImprovement     = id_bool(*io, "noImprovement");
		m_bNoBonus           = id_bool(*io, "noBonus");
		m_bCountsAsPeak      = id_bool(*io, "countsAsPeak");
		m_bRequiresFlatlands = id_bool(*io, "requiresFlatlands");
		m_bAddsFreshWater    = id_bool(*io, "addsFreshWater");
		m_bNukeImmune        = id_bool(*io, "nukeImmune");

		picojson::object::const_iterator vt = io->find("validTerrains");
		if (vt != io->end() && vt->second.is<picojson::array>())
		{
			const picojson::array& a = vt->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeValidTerrains.push_back((TerrainTypes)id); }
		}
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
	// ⏳ m_iZobristValue left 0 (needs the exact legacy zobrist map-hash, OOS) -- flagged not faked.
}
