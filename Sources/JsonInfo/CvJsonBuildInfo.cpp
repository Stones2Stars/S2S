//
//	CvJsonBuildInfo::mapFrom -- base core reading + availability, then the build's real members from the curator's
//	`produces` / `cost` / `identity` shapes. FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonBuildInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/...)

CvJsonBuildInfo::CvJsonBuildInfo()
	: m_eImprovement(NO_IMPROVEMENT), m_eRoute(NO_ROUTE), m_iTime(0), m_iCost(0),
	  m_iEntityEvent(ENTITY_EVENT_NONE), m_iMissionType(NO_MISSION), m_bKill(false)
{}

bool CvJsonBuildInfo::isFeatureRemove(int iFeature) const
{
	for (size_t i = 0; i < m_aeFeatureRemove.size(); ++i) if ((int)m_aeFeatureRemove[i] == iFeature) return true;
	return false;
}

void CvJsonBuildInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability model (requires.build carries the tech prereq)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// produces: what laying the build creates (json §9)
	if (const picojson::object* pr = jsonChildObj(o, "produces"))
	{
		picojson::object::const_iterator im = pr->find("improvement");
		if (im != pr->end() && im->second.is<std::string>()) m_eImprovement = (ImprovementTypes)jsonResolveId(im->second.get<std::string>());
		picojson::object::const_iterator ro = pr->find("route");
		if (ro != pr->end() && ro->second.is<std::string>()) m_eRoute = (RouteTypes)jsonResolveId(ro->second.get<std::string>());
		picojson::object::const_iterator fr = pr->find("featureRemove");
		if (fr != pr->end() && fr->second.is<picojson::array>())
		{
			const picojson::array& a = fr->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeFeatureRemove.push_back((FeatureTypes)id); }
		}
	}

	// cost: gold + time
	if (const picojson::object* co = jsonChildObj(o, "cost"))
	{
		m_iCost = jsonIdInt(*co, "gold");
		m_iTime = jsonIdInt(*co, "time");
	}

	// identity: does it consume the worker?
	if (const picojson::object* io = jsonChildObj(o, "identity"))
		m_bKill = jsonIdBool(*io, "consumesUnit");

	// world.art.entityEvent -- the on-map worker animation (EXE-bound getEntityEvent, an ENTITY_EVENT_* id).
	// getMissionType() is runtime-assigned (setMissionType at load), NOT read here.
	if (const picojson::object* art = jsonWorldArt(o))
	{
		std::string s;
		if (jsonIdStr(*art, "entityEvent", s)) m_iEntityEvent = jsonResolveId(s);
	}
}
