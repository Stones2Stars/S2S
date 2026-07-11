//
//	CvBuildInfo::mapFrom -- base core reading + availability, then the build's real members from the curator's
//	`produces` / `cost` / `identity` shapes. FK resolution via the kept type registry. See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvBuildInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/...)

namespace {
	const FeatureStruct* findFeatureStruct(const std::vector<FeatureStruct>& vec, FeatureTypes e)
	{
		for (size_t i = 0; i < vec.size(); ++i)
			if (vec[i].eFeature == e) return &vec[i];
		return NULL;
	}

	// The build's own tech MEANS gate lives in requires.build as a team-scoped PRESENCE clause (curate_build.py's
	// _requires(): {type:PrereqTech, scope:"team"}, sitting alongside any plot-scoped bonus-connectivity clauses in
	// the SAME "all" list) -- so the team-scoped presence node's resolved id IS the tech prereq. Faithful, not a
	// guess: it is the exact shape the curator writes, and the only TEAM-scoped clause a build ever authors.
	TechTypes findBuildTechPrereq(const CvJsonCondition* c)
	{
		if (c == NULL) return NO_TECH;
		if (c->kind == CASC_COND_PRESENCE && c->scope == CASC_SCOPE_TEAM)
			return (c->id >= 0) ? (TechTypes)c->id : NO_TECH;
		if (c->kind == CASC_COND_GROUP)
		{
			for (size_t i = 0; i < c->all.size(); ++i)
			{
				const TechTypes t = findBuildTechPrereq(c->all[i]);
				if (t != NO_TECH) return t;
			}
		}
		return NO_TECH;
	}

	// The build's PrereqBonusTypes are authored in requires.build.all as plot-scoped BONUS_ presence clauses
	// (curate_build.py _requires(): {type:BONUS_x, scope:"plot", connection:"trade"} -- ALL must be plot-connected,
	// so they ride the AND-list, exactly like the tech clause). Collect every BONUS_ presence id in the all-tree ->
	// the legacy positive-prereq view. Filter on the BONUS_ type prefix (the semantically exact test): a build's
	// only other all-clause is the team-scoped tech, so this never mis-collects.
	void collectPrereqBonuses(const CvJsonCondition* c, std::vector<BonusTypes>& out)
	{
		if (c == NULL) return;
		if (c->kind == CASC_COND_PRESENCE)
		{
			if (c->id >= 0 && c->type.compare(0, 6, "BONUS_") == 0) out.push_back((BonusTypes)c->id);
		}
		else if (c->kind == CASC_COND_GROUP)
		{
			for (size_t i = 0; i < c->all.size(); ++i) collectPrereqBonuses(c->all[i], out);
		}
	}
}

CvBuildInfo::CvBuildInfo()
	: m_eImprovement(NO_IMPROVEMENT), m_eRoute(NO_ROUTE), m_eTerrainChange(NO_TERRAIN), m_eFeatureChange(NO_FEATURE),
	  m_iTime(0), m_iCost(0), m_iEntityEvent(ENTITY_EVENT_NONE), m_iMissionType(NO_MISSION), m_bKill(false),
	  m_bDisabled(false), m_eTechPrereq(NO_TECH), m_eObsoleteTech(NO_TECH)
{}

bool CvBuildInfo::isFeatureRemove(FeatureTypes e) const
{
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->bRemove : false;
}

TechTypes CvBuildInfo::getFeatureTech(FeatureTypes e) const
{
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->ePrereqTech : NO_TECH;
}

int CvBuildInfo::getFeatureTime(FeatureTypes e) const
{
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->iTime : 0;
}

int CvBuildInfo::getFeatureProduction(FeatureTypes e) const
{
	const FeatureStruct* p = findFeatureStruct(m_aFeatureStructs, e);
	return p ? p->iProduction : 0;
}

void CvBuildInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + availability model (populates m_requires, incl. requires.build)
	m_eTechPrereq = findBuildTechPrereq(m_requires.build);        // real data: the team-scoped clause of requires.build
	collectPrereqBonuses(m_requires.build, m_aePrereqBonusTypes); // real data: the plot-scoped BONUS_ clauses
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// produces: what laying the build creates (json §9)
	if (const picojson::object* pr = jsonChildObj(o, "produces"))
	{
		picojson::object::const_iterator im = pr->find("improvement");
		if (im != pr->end() && im->second.is<std::string>()) m_eImprovement = (ImprovementTypes)jsonResolveId(im->second.get<std::string>());
		picojson::object::const_iterator ro = pr->find("route");
		if (ro != pr->end() && ro->second.is<std::string>()) m_eRoute = (RouteTypes)jsonResolveId(ro->second.get<std::string>());
		// single-FK plot-type-change outcomes: terraform-to terrain + feature planted/changed-to
		const int idTerrainChange = jsonIdFk(*pr, "terrainChange");
		if (idTerrainChange >= 0) m_eTerrainChange = (TerrainTypes)idTerrainChange;
		const int idFeatureChange = jsonIdFk(*pr, "featureChange");
		if (idFeatureChange >= 0) m_eFeatureChange = (FeatureTypes)idFeatureChange;
		// features[]: FeatureStructs {feature, tech?, time?, production?, remove?} -- the per-feature add/REMOVE
		// (remove=true is the chop: +production hammers, +time; a remove=false entry with only a tech is the
		// per-feature TECH GATE, e.g. "road on a swamp needs Canal Systems"). curate_build.py's _struct_list.
		picojson::object::const_iterator fe = pr->find("features");
		if (fe != pr->end() && fe->second.is<picojson::array>())
		{
			const picojson::array& a = fe->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& fo = a[i].get<picojson::object>();
				const int idFeature = jsonIdFk(fo, "feature");
				if (idFeature < 0) continue;
				FeatureStruct fs;
				fs.eFeature = (FeatureTypes)idFeature;
				const int idTech = jsonIdFk(fo, "tech");
				fs.ePrereqTech = (idTech >= 0) ? (TechTypes)idTech : NO_TECH;
				fs.iTime = jsonIdInt(fo, "time");
				fs.iProduction = jsonIdInt(fo, "production");
				fs.bRemove = jsonIdBool(fo, "remove");
				m_aFeatureStructs.push_back(fs);
			}
		}
		// terraform[]: TerrainStructs {terrain, tech?, time?} -- per-terrain terraform time + tech gate
		// (CvPlayer::canBuild + CvPlot::getBuildTime walk these).
		picojson::object::const_iterator tf = pr->find("terraform");
		if (tf != pr->end() && tf->second.is<picojson::array>())
		{
			const picojson::array& a = tf->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& to = a[i].get<picojson::object>();
				const int idTerrain = jsonIdFk(to, "terrain");
				if (idTerrain < 0) continue;
				TerrainStructs ts;
				ts.eTerrain = (TerrainTypes)idTerrain;
				const int idTech = jsonIdFk(to, "tech");
				ts.ePrereqTech = (idTech >= 0) ? (TechTypes)idTech : NO_TECH;
				ts.iTime = jsonIdInt(to, "time");
				m_aTerrainStructs.push_back(ts);
			}
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
