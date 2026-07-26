//
//	CvBuildInfo -- the build poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the §9 `produces` section + the `cost` / `identity` / `world.art` rows ONCE into typed
//	members ([DEC-materialize-at-mapfrom]); the tech MEANS gate + the positive bonus prereqs are derived from
//	the composed requires.build tree (typed condition walk, never a raw-parse re-read). Idempotent by contract
//	(unconditional assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvBuildInfo.h"
#include "CvJsonParse.h"   // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/jsonIdFk/jsonIdBool/jsonIdStr/jsonWorldArt)

namespace
{
	const FeatureStruct* findFeatureRow(const std::vector<FeatureStruct>& rows, FeatureTypes eFeature)
	{
		for (size_t iRow = 0; iRow < rows.size(); ++iRow)
		{
			if (rows[iRow].eFeature == eFeature)
			{
				return &rows[iRow];
			}
		}
		return NULL;
	}

	const TerrainStructs* findTerraformRow(const std::vector<TerrainStructs>& rows, TerrainTypes eTerrain)
	{
		for (size_t iRow = 0; iRow < rows.size(); ++iRow)
		{
			if (rows[iRow].eTerrain == eTerrain)
			{
				return &rows[iRow];
			}
		}
		return NULL;
	}

	// The build's own tech MEANS gate lives in requires.build as a team-scoped PRESENCE clause
	// (curate_build.py's _requires(): {type:PrereqTech, scope:"team"}, beside any plot-scoped
	// bonus-connectivity clauses in the SAME "all" list) -- so the team-scoped presence node's resolved id IS
	// the tech prereq. Faithful, not a guess: it is the exact shape the curator writes, and the only
	// TEAM-scoped clause a build ever authors.
	TechTypes findBuildTechPrereq(const CvCondition* pCondition)
	{
		if (pCondition == NULL)
		{
			return NO_TECH;
		}
		if (pCondition->kind == CASC_COND_PRESENCE && pCondition->scope == CASC_SCOPE_TEAM)
		{
			return (pCondition->id >= 0) ? (TechTypes)pCondition->id : NO_TECH;
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				const TechTypes eTech = findBuildTechPrereq(pCondition->all[iChild]);
				if (eTech != NO_TECH)
				{
					return eTech;
				}
			}
		}
		return NO_TECH;
	}

	// The build's positive bonus prereqs are authored in requires.build.all as plot-scoped BONUS_ presence
	// clauses (curate_build.py _requires(): {type:BONUS_x, scope:"plot", connection:"trade"} -- ALL must be
	// plot-connected, so they ride the AND-list, exactly like the tech clause). Collect every BONUS_ presence
	// id in the all-tree -> the positive-prereq view. Filter on the BONUS_ type prefix (the semantically exact
	// test): a build's only other all-clause is the team-scoped tech.
	void collectPrereqBonuses(const CvCondition* pCondition, std::vector<BonusTypes>& bonusesOut)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PRESENCE)
		{
			if (pCondition->id >= 0 && pCondition->type.compare(0, 6, "BONUS_") == 0)
			{
				bonusesOut.push_back((BonusTypes)pCondition->id);
			}
		}
		else if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				collectPrereqBonuses(pCondition->all[iChild], bonusesOut);
			}
		}
	}
}

CvBuildInfo::CvBuildInfo()
	: m_eTechPrereq(NO_TECH)
	, m_iGoldCost(0)
	, m_iTime(0)
	, m_iEntityEvent(ENTITY_EVENT_NONE)
	, m_iMissionType(NO_MISSION)
	, m_bConsumesUnit(false)
	, m_bDisabled(false)
{
}

bool CvBuildInfo::isFeatureRemove(FeatureTypes eFeature) const
{
	const FeatureStruct* pRow = findFeatureRow(m_produces.featureRows, eFeature);
	return pRow ? pRow->bRemove : false;
}

TechTypes CvBuildInfo::getFeatureTech(FeatureTypes eFeature) const
{
	const FeatureStruct* pRow = findFeatureRow(m_produces.featureRows, eFeature);
	return pRow ? pRow->ePrereqTech : NO_TECH;
}

int CvBuildInfo::getFeatureTime(FeatureTypes eFeature) const
{
	const FeatureStruct* pRow = findFeatureRow(m_produces.featureRows, eFeature);
	return pRow ? pRow->iTime : 0;
}

int CvBuildInfo::getFeatureProduction(FeatureTypes eFeature) const
{
	const FeatureStruct* pRow = findFeatureRow(m_produces.featureRows, eFeature);
	return pRow ? pRow->iProduction : 0;
}

TechTypes CvBuildInfo::getTerraformTech(TerrainTypes eTerrain) const
{
	const TerrainStructs* pRow = findTerraformRow(m_produces.terraformRows, eTerrain);
	return pRow ? pRow->ePrereqTech : NO_TECH;
}

int CvBuildInfo::getTerraformTime(TerrainTypes eTerrain) const
{
	const TerrainStructs* pRow = findTerraformRow(m_produces.terraformRows, eTerrain);
	return pRow ? pRow->iTime : 0;
}

void CvBuildInfo::mapFrom(const picojson::value& entity)
{
	// idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- clear-first on every container,
	// unconditional redefinition of every conditionally-parsed scalar. NOT reset: m_bDisabled (a runtime
	// Python toggle, never JSON) and m_iMissionType (assigned after the last mapFrom of a load).
	m_produces.clear();
	m_aePrereqBonusTypes.clear();
	m_eTechPrereq = NO_TECH;
	m_iGoldCost = 0;
	m_iTime = 0;
	m_iEntityEvent = ENTITY_EVENT_NONE;
	m_bConsumesUnit = false;

	CvInfo::mapFrom(entity);   // core reading + availability (populates m_requires, incl. requires.build)
	m_eTechPrereq = findBuildTechPrereq(m_requires.build);          // the team-scoped clause of requires.build
	collectPrereqBonuses(m_requires.build, m_aePrereqBonusTypes);   // the plot-scoped BONUS_ clauses

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// produces: what laying the build creates (json §9) -> the ONE typed section member
	if (const picojson::object* pProduces = jsonChildObj(entityObj, "produces"))
	{
		const int iImprovement = jsonIdFk(*pProduces, "improvement");
		if (iImprovement >= 0)
		{
			m_produces.eImprovement = (ImprovementTypes)iImprovement;
		}
		const int iRoute = jsonIdFk(*pProduces, "route");
		if (iRoute >= 0)
		{
			m_produces.eRoute = (RouteTypes)iRoute;
		}
		// single-FK plot-type-change outcomes: terraform-to terrain + feature planted/changed-to
		const int iTerrainChange = jsonIdFk(*pProduces, "terrainChange");
		if (iTerrainChange >= 0)
		{
			m_produces.eTerrainChange = (TerrainTypes)iTerrainChange;
		}
		const int iFeatureChange = jsonIdFk(*pProduces, "featureChange");
		if (iFeatureChange >= 0)
		{
			m_produces.eFeatureChange = (FeatureTypes)iFeatureChange;
		}
		// features[]: {feature, tech?, time?, production?, remove?} -- the per-feature add/REMOVE (remove=true
		// is the chop: +production hammers, +time; a remove=false entry with only a tech is the per-feature
		// TECH GATE, e.g. "road on a swamp needs Canal Systems").
		picojson::object::const_iterator featuresIter = pProduces->find("features");
		if (featuresIter != pProduces->end() && featuresIter->second.is<picojson::array>())
		{
			const picojson::array& featureRows = featuresIter->second.get<picojson::array>();
			for (size_t iRow = 0; iRow < featureRows.size(); ++iRow)
			{
				if (!featureRows[iRow].is<picojson::object>())
				{
					continue;
				}
				const picojson::object& featureRow = featureRows[iRow].get<picojson::object>();
				const int iFeature = jsonIdFk(featureRow, "feature");
				if (iFeature < 0)
				{
					continue;
				}
				FeatureStruct featureStruct;
				featureStruct.eFeature = (FeatureTypes)iFeature;
				const int iFeatureTech = jsonIdFk(featureRow, "tech");
				featureStruct.ePrereqTech = (iFeatureTech >= 0) ? (TechTypes)iFeatureTech : NO_TECH;
				featureStruct.iTime = jsonIdInt(featureRow, "time");
				featureStruct.iProduction = jsonIdInt(featureRow, "production");
				featureStruct.bRemove = jsonIdBool(featureRow, "remove");
				m_produces.featureRows.push_back(featureStruct);
			}
		}
		// terraform[]: {terrain, tech?, time?} -- per-terrain terraform time + tech gate
		picojson::object::const_iterator terraformIter = pProduces->find("terraform");
		if (terraformIter != pProduces->end() && terraformIter->second.is<picojson::array>())
		{
			const picojson::array& terrainRows = terraformIter->second.get<picojson::array>();
			for (size_t iRow = 0; iRow < terrainRows.size(); ++iRow)
			{
				if (!terrainRows[iRow].is<picojson::object>())
				{
					continue;
				}
				const picojson::object& terrainRow = terrainRows[iRow].get<picojson::object>();
				const int iTerrain = jsonIdFk(terrainRow, "terrain");
				if (iTerrain < 0)
				{
					continue;
				}
				TerrainStructs terrainStruct;
				terrainStruct.eTerrain = (TerrainTypes)iTerrain;
				const int iTerrainTech = jsonIdFk(terrainRow, "tech");
				terrainStruct.ePrereqTech = (iTerrainTech >= 0) ? (TechTypes)iTerrainTech : NO_TECH;
				terrainStruct.iTime = jsonIdInt(terrainRow, "time");
				m_produces.terraformRows.push_back(terrainStruct);
			}
		}
	}

	// cost: gold + time
	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iGoldCost = jsonIdInt(*pCost, "gold");
		m_iTime = jsonIdInt(*pCost, "time");
	}

	// identity: does the build consume the worker?
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bConsumesUnit = jsonIdBool(*pIdentity, "consumesUnit");
	}

	// world.art.entityEvent -- the on-map worker animation (EXE-bound getEntityEvent, an ENTITY_EVENT_* id).
	// getMissionType() is runtime-assigned (setMissionType at load), NOT read here.
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		std::string szEntityEvent;
		if (jsonIdStr(*pArt, "entityEvent", szEntityEvent))
		{
			m_iEntityEvent = jsonResolveId(szEntityEvent);
		}
	}
}
