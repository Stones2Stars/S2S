//
//	CvProjectInfo::mapFrom -- base core reading + availability (tech/prereq-project/world-scope/specialBuilding
//	edges ride the base), then the project's team/empire/world values + the bespoke `victory` block + placement. FK
//	resolution via the kept type registry. HUMAN-native. PROVISIONAL-flagged families are curator-tentative names. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvProjectInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/jsonReadFkMap/...)

CvProjectInfo::CvProjectInfo()
	: m_iProductionCost(0), m_iNukeInterception(0), m_iTechShare(0),
	  m_iGlobalMaintenanceModifier(0), m_iDistanceMaintenanceModifier(0), m_iNumCitiesMaintenanceModifier(0),
	  m_iConnectedCityMaintenanceModifier(0), m_iInflationModifier(0),
	  m_iGlobalHappiness(0), m_iGlobalHealth(0), m_iWorldHappiness(0), m_iWorldHealth(0), m_iWorldTradeRoutes(0),
	  m_iVictoryDelayPercent(0), m_iSuccessRate(0), m_eLaunchesVictory(-1), m_bSpaceship(false), m_bAllowsNukes(false),
	  m_iEveryoneSpecialUnit(-1), m_iAnyoneProjectPrereq(-1),   // NO_SPECIALUNIT / NO_PROJECT
	  m_eTechPrereq(NO_TECH)
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiCommerceModifier[i] = 0;
}

void CvProjectInfo::mapFrom(const picojson::value& entity)
{
	m_aeMapCategories.clear();   // remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	CvInfo::mapFrom(entity);   // core + availability (enables.projects, requires.build world-scope, enables.specialBuildings, allowed caps)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* co = jsonChildObj(o, "cost")) m_iProductionCost = jsonIdInt(*co, "create");

	m_iNukeInterception  = jsonFamMemberVal(o, "combat", "team", "nukeInterception", "percent");   // family name PROVISIONAL
	m_iTechShare         = jsonFamMemberVal(o, "diplomacy", "team", "techShare", "flat");            // family name PROVISIONAL
	m_iGlobalMaintenanceModifier        = jsonFamMemberVal(o, "maintenance", "empire", "all", "percent");
	m_iDistanceMaintenanceModifier      = jsonFamMemberVal(o, "maintenance", "empire", "distance", "percent");
	m_iNumCitiesMaintenanceModifier     = jsonFamMemberVal(o, "maintenance", "empire", "numCities", "percent");
	m_iConnectedCityMaintenanceModifier = jsonFamMemberVal(o, "maintenance", "empire", "connectedCity", "percent");
	m_iInflationModifier = jsonFamVal(o, "inflation", "empire", "percent");   // inflation is its OWN family (DEC-maintenance-bookkeeping), unified with tech/building
	m_iGlobalHappiness   = jsonFamVal(o, "happiness", "empire", "flat");
	m_iGlobalHealth      = jsonFamVal(o, "health", "empire", "flat");
	m_iWorldHappiness    = jsonFamVal(o, "happiness", "world", "flat");
	m_iWorldHealth       = jsonFamVal(o, "health", "world", "flat");
	m_iWorldTradeRoutes  = jsonFamVal(o, "tradeRoutes", "world", "flat");
	m_aiCommerceModifier[COMMERCE_GOLD]      = jsonFamVal(o, "gold", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_RESEARCH]  = jsonFamVal(o, "research", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_CULTURE]   = jsonFamVal(o, "culture", "empire", "percent");
	m_aiCommerceModifier[COMMERCE_ESPIONAGE] = jsonFamVal(o, "espionage", "empire", "percent");

	// bespoke `victory` launch params
	if (const picojson::object* vo = jsonChildObj(o, "victory"))
	{
		jsonReadFkMap(*vo, "thresholds", m_victoryThreshold);
		jsonReadFkMap(*vo, "minThresholds", m_victoryMinThreshold);
		m_iVictoryDelayPercent = jsonIdInt(*vo, "delayPercent");
		m_iSuccessRate         = jsonIdInt(*vo, "successRate");
	}

	// buildRate.self.percent -- a list of { value, enabled:{type:BONUS_X, ...} } => bonus-keyed production modifier
	if (const picojson::object* br = jsonChildObj(o, "buildRate"))
		if (const picojson::object* self = jsonChildObj(*br, "self"))
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
					const picojson::object* en = jsonChildObj(e, "enabled");
					if (ve == e.end() || !ve->second.is<double>() || !en) continue;
					const int bonus = jsonIdFk(*en, "type");
					if (bonus >= 0) m_bonusProduction[bonus] = (int)ve->second.get<double>();
				}
			}
		}

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bSpaceship       = jsonIdBool(*io, "spaceship");
		m_bAllowsNukes     = jsonIdBool(*io, "allowsNukes");
		m_eLaunchesVictory = jsonIdFk(*io, "launchesVictory");
		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id); }
		}
	}

	if (const picojson::object* so = jsonChildObj(o, "sound")) jsonIdStr(*so, "onCompletion", m_szCreateSound);

	// ui.art.movie.defineTag -- the completion movie art tag (MovieDefineTag)
	if (const picojson::object* uo = jsonChildObj(o, "ui"))
		if (const picojson::object* ao = jsonChildObj(*uo, "art"))
			if (const picojson::object* mo = jsonChildObj(*ao, "movie"))
				jsonIdStr(*mo, "defineTag", m_szMovieArtDef);

	// grants.grantsSpecialUnit -- completion makes a SPECIALUNIT game-wide valid (EveryoneSpecialUnit). FK, -1 if absent.
	if (const picojson::object* go = jsonChildObj(o, "grants"))
		m_iEveryoneSpecialUnit = jsonIdFk(*go, "grantsSpecialUnit");

	// requires.build{type,scope:world} -- the single "anyone built one" project gate (AnyonePrereqProject). FK, -1 if absent.
	// (enables.specialBuildings rides the composed m_edges; EveryoneSpecialBuilding reads it via edge() in the header.)
	if (const picojson::object* ro = jsonChildObj(o, "requires"))
		if (const picojson::object* bo = jsonChildObj(*ro, "build"))
			m_iAnyoneProjectPrereq = jsonIdFk(*bo, "type");
}
