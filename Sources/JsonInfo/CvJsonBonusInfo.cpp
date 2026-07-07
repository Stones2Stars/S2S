//
//	CvJsonBonusInfo::mapFrom -- base core reading + availability (enables.* rides the base), then the bonus's real
//	values + map-gen placement. FK resolution via the kept type registry. ⏳-flagged shapes to confirm. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonBonusInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString

CvJsonBonusInfo::CvJsonBonusInfo()
	: m_iBonusClassType(-1), m_iHealth(0), m_iHappiness(0),
	  m_iMinAreaSize(0), m_iMinLatitude(0), m_iMaxLatitude(90), m_iPlacementOrder(0), m_iTilesPer(0),
	  m_iUniqueRange(0), m_iGroupRange(0), m_iGroupRand(0),
	  m_bOneArea(false), m_bHills(false), m_bPeaks(false), m_bFlatlands(false), m_bBonusCoastalOnly(false),
	  m_bNoRiverSide(false), m_bNormalize(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
}

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
static int  id_int (const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }
static int id_fk(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<std::string>()) ? GC.getInfoTypeForString(it->second.get<std::string>().c_str(), true) : -1; }

void CvJsonBonusInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (enables.units/buildings)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_aiYieldChange[YIELD_FOOD]       = fam_val(o, "food", "plot", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = fam_val(o, "production", "plot", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = fam_val(o, "commerce", "plot", "flat");
	m_iHealth    = fam_val(o, "health", "empire", "flat");     // ⏳ scope to confirm (bonus presence-gated)
	m_iHappiness = fam_val(o, "happiness", "empire", "flat");  // ⏳

	if (const picojson::object* mg = child_obj(o, "mapGeneration"))
	{
		m_iMinAreaSize     = id_int(*mg, "minAreaSize");
		m_iMinLatitude     = id_int(*mg, "minLatitude");
		m_iMaxLatitude     = id_int(*mg, "maxLatitude");
		m_iPlacementOrder  = id_int(*mg, "placementOrder");
		m_iTilesPer        = id_int(*mg, "tilesPer");
		m_iUniqueRange     = id_int(*mg, "uniqueRange");
		m_iGroupRange      = id_int(*mg, "groupRange");
		m_iGroupRand       = id_int(*mg, "groupRand");
		m_bOneArea         = id_bool(*mg, "area");              // curate_bonus BONUS_MAP_GEN: bArea -> area
		m_bHills           = id_bool(*mg, "hills");
		m_bPeaks           = id_bool(*mg, "peaks");
		m_bFlatlands       = id_bool(*mg, "flatlands");
		m_bBonusCoastalOnly= id_bool(*mg, "bonusCoastalOnly");  // bBonusCoastalOnly -> bonusCoastalOnly
		m_bNoRiverSide     = id_bool(*mg, "noRiverSide");
		m_bNormalize       = id_bool(*mg, "normalize");
	}

	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iBonusClassType = id_fk(*io, "bonusClassType");
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}
}
