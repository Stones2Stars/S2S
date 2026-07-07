//
//	CvJsonBuildInfo::mapFrom -- base core reading + availability, then the build's real members from the curator's
//	`produces` / `cost` / `identity` shapes. FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonBuildInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString -- improvement/route/feature FKs

CvJsonBuildInfo::CvJsonBuildInfo()
	: m_eImprovement(NO_IMPROVEMENT), m_eRoute(NO_ROUTE), m_iTime(0), m_iCost(0), m_bKill(false)
{}

bool CvJsonBuildInfo::isFeatureRemove(int iFeature) const
{
	for (size_t i = 0; i < m_aeFeatureRemove.size(); ++i) if ((int)m_aeFeatureRemove[i] == iFeature) return true;
	return false;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static int  obj_int (const picojson::object& o, const char* key)
{ picojson::object::const_iterator it = o.find(key); return (it != o.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool obj_bool(const picojson::object& o, const char* key)
{ picojson::object::const_iterator it = o.find(key); return (it != o.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }
static int resolve(const std::string& s) { return GC.getInfoTypeForString(s.c_str(), true); }

void CvJsonBuildInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability model (requires.build carries the tech prereq)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// produces: what laying the build creates (json §9)
	if (const picojson::object* pr = child_obj(o, "produces"))
	{
		picojson::object::const_iterator im = pr->find("improvement");
		if (im != pr->end() && im->second.is<std::string>()) m_eImprovement = (ImprovementTypes)resolve(im->second.get<std::string>());
		picojson::object::const_iterator ro = pr->find("route");
		if (ro != pr->end() && ro->second.is<std::string>()) m_eRoute = (RouteTypes)resolve(ro->second.get<std::string>());
		picojson::object::const_iterator fr = pr->find("featureRemove");
		if (fr != pr->end() && fr->second.is<picojson::array>())
		{
			const picojson::array& a = fr->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>()) { const int id = resolve(a[i].get<std::string>()); if (id >= 0) m_aeFeatureRemove.push_back((FeatureTypes)id); }
		}
	}

	// cost: gold + time
	if (const picojson::object* co = child_obj(o, "cost"))
	{
		m_iCost = obj_int(*co, "gold");
		m_iTime = obj_int(*co, "time");
	}

	// identity: does it consume the worker?
	if (const picojson::object* io = child_obj(o, "identity"))
		m_bKill = obj_bool(*io, "consumesUnit");
}
