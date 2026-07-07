//
//	CvJsonProjectInfo::mapFrom -- base core reading + availability (tech/prereq-project/world-scope/specialBuilding
//	edges ride the base), then the project's team/empire/world values + the bespoke `victory` block + placement. FK
//	resolution via the kept type registry. HUMAN-native. ⏳-flagged shapes are curator-PROVISIONAL. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonProjectInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString

CvJsonProjectInfo::CvJsonProjectInfo()
	: m_iProductionCost(0), m_iNukeInterception(0), m_iTechShare(0),
	  m_iGlobalMaintenanceModifier(0), m_iDistanceMaintenanceModifier(0), m_iNumCitiesMaintenanceModifier(0),
	  m_iConnectedCityMaintenanceModifier(0), m_iInflationModifier(0),
	  m_iGlobalHappiness(0), m_iGlobalHealth(0), m_iWorldHappiness(0), m_iWorldHealth(0), m_iWorldTradeRoutes(0),
	  m_iVictoryDelayPercent(0), m_iSuccessRate(0), m_eLaunchesVictory(-1), m_bSpaceship(false), m_bAllowsNukes(false)
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiCommerceModifier[i] = 0;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{ picojson::object::const_iterator it = o.find(key); return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL; }
static int fam_val(const picojson::object& o, const char* f, const char* s, const char* u)
{ const picojson::object* fo = child_obj(o, f); if (!fo) return 0; const picojson::object* so = child_obj(*fo, s); if (!so) return 0;
  picojson::object::const_iterator it = so->find(u); return (it != so->end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static int fam_member_val(const picojson::object& o, const char* f, const char* s, const char* m, const char* u)
{ const picojson::object* fo = child_obj(o, f); if (!fo) return 0; const picojson::object* so = child_obj(*fo, s); if (!so) return 0;
  const picojson::object* mo = child_obj(*so, m); if (!mo) return 0; picojson::object::const_iterator it = mo->find(u);
  return (it != mo->end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static int id_int(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }
static int id_fk(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<std::string>()) ? GC.getInfoTypeForString(it->second.get<std::string>().c_str(), true) : -1; }
static void id_str(const picojson::object& io, const char* k, std::string& out)
{ picojson::object::const_iterator it = io.find(k); if (it != io.end() && it->second.is<std::string>()) out = it->second.get<std::string>(); }
// FK-keyed int map: {"VICTORY_X": n} -> map[victoryId] = n.
static void read_fk_map(const picojson::object& parent, const char* key, std::map<int, int>& out)
{
	picojson::object::const_iterator it = parent.find(key);
	if (it == parent.end() || !it->second.is<picojson::object>()) return;
	const picojson::object& m = it->second.get<picojson::object>();
	for (picojson::object::const_iterator e = m.begin(); e != m.end(); ++e)
		if (e->second.is<double>()) { const int id = GC.getInfoTypeForString(e->first.c_str(), true); if (id >= 0) out[id] = (int)e->second.get<double>(); }
}

void CvJsonProjectInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (enables.projects, requires.build world-scope, enables.specialBuildings, allowed caps)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* co = child_obj(o, "cost")) m_iProductionCost = id_int(*co, "create");

	m_iNukeInterception  = fam_member_val(o, "combat", "team", "nukeInterception", "percent");   // ⏳ PROVISIONAL
	m_iTechShare         = fam_member_val(o, "diplomacy", "team", "techShare", "flat");            // ⏳ PROVISIONAL
	m_iGlobalMaintenanceModifier        = fam_member_val(o, "maintenance", "empire", "all", "percent");
	m_iDistanceMaintenanceModifier      = fam_member_val(o, "maintenance", "empire", "distance", "percent");
	m_iNumCitiesMaintenanceModifier     = fam_member_val(o, "maintenance", "empire", "numCities", "percent");
	m_iConnectedCityMaintenanceModifier = fam_member_val(o, "maintenance", "empire", "connectedCity", "percent");
	m_iInflationModifier = fam_member_val(o, "upkeep", "empire", "inflation", "percent");
	m_iGlobalHappiness   = fam_val(o, "happiness", "empire", "flat");
	m_iGlobalHealth      = fam_val(o, "health", "empire", "flat");
	m_iWorldHappiness    = fam_val(o, "happiness", "world", "flat");
	m_iWorldHealth       = fam_val(o, "health", "world", "flat");
	m_iWorldTradeRoutes  = fam_val(o, "tradeRoutes", "world", "flat");
	m_aiCommerceModifier[COMMERCE_GOLD]      = fam_val(o, "gold", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_RESEARCH]  = fam_val(o, "research", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_CULTURE]   = fam_val(o, "culture", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_ESPIONAGE] = fam_val(o, "espionage", "empire", "percent");

	// bespoke `victory` launch params
	if (const picojson::object* vo = child_obj(o, "victory"))
	{
		read_fk_map(*vo, "thresholds", m_victoryThreshold);
		read_fk_map(*vo, "minThresholds", m_victoryMinThreshold);
		m_iVictoryDelayPercent = id_int(*vo, "delayPercent");
		m_iSuccessRate         = id_int(*vo, "successRate");
	}

	// buildRate.self.percent -- a list of { value, enabled:{type:BONUS_X, ...} } => bonus-keyed production modifier ⏳
	if (const picojson::object* br = child_obj(o, "buildRate"))
		if (const picojson::object* self = child_obj(*br, "self"))
		{
			picojson::object::const_iterator pit = self->find("percent");
			if (pit != self->end() && pit->second.is<picojson::array>())
			{
				const picojson::array& a = pit->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					picojson::object::const_iterator ve = e.find("value");
					const picojson::object* en = child_obj(e, "enabled");
					if (ve == e.end() || !ve->second.is<double>() || !en) continue;
					const int bonus = id_fk(*en, "type");
					if (bonus >= 0) m_bonusProduction[bonus] = (int)ve->second.get<double>();
				}
			}
		}

	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_bSpaceship       = id_bool(*io, "spaceship");
		m_bAllowsNukes     = id_bool(*io, "allowsNukes");
		m_eLaunchesVictory = id_fk(*io, "launchesVictory");
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}

	if (const picojson::object* so = child_obj(o, "sound")) id_str(*so, "onCompletion", m_szCreateSound);
}
