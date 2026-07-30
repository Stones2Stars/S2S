//
//	CvWorldInfo -- the world (map size) poco's config materialization (see the header). Pure CONFIG: mapFrom
//	reads the authored `identity` block into the typed members; every getter is a bare member read
//	([DEC-materialize-at-mapfrom]). No section units are composed -- there is nothing per-turn here.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "CvWorldInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt


CvWorldInfo::CvWorldInfo()
	: m_iDefaultPlayers(0)
	, m_iTargetNumCities(0)
	, m_iGridWidth(0)
	, m_iGridHeight(0)
	, m_iTerrainGrainChange(0)
	, m_iFeatureGrainChange(0)
	, m_iOceanMinAreaSize(0)
	, m_iBuildingPrereqModifier(0)
	, m_iMaxConscriptModifier(0)
	, m_iWarWearinessModifier(0)
	, m_iTradeProfitPercent(0)
	, m_iCorporationMaintenancePercent(0)
	, m_iNumCitiesAnarchyPercent(0)
	, m_iAdvancedStartPointsMod(0)
	, m_iCityLimitsScalePercent(100)   // 100 = no change; an absent key must scale by 100, never 0
{
}


// EXE-bound (DllExport): the staging screens read the size's default player count through the export table.
int CvWorldInfo::getDefaultPlayers() const
{
	return m_iDefaultPlayers;
}


// #430: the ONE load hook -- idempotent (CvInfo.h): every member is unconditionally redefined each call
// (jsonIdInt serves the default when the key is absent).
void CvWorldInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / identity text keys / button); no sections composed

	const picojson::object* pIdentity = NULL;
	if (entity.is<picojson::object>())
	{
		pIdentity = jsonChildObj(entity.get<picojson::object>(), "identity");
	}
	static const picojson::object emptyObject;
	const picojson::object& identityObj = pIdentity ? *pIdentity : emptyObject;

	m_iDefaultPlayers               = jsonIdInt(identityObj, "defaultPlayers");
	m_iTargetNumCities              = jsonIdInt(identityObj, "targetNumCities");
	m_iGridWidth                    = jsonIdInt(identityObj, "gridWidth");
	m_iGridHeight                   = jsonIdInt(identityObj, "gridHeight");
	m_iTerrainGrainChange           = jsonIdInt(identityObj, "terrainGrainChange");
	m_iFeatureGrainChange           = jsonIdInt(identityObj, "featureGrainChange");
	m_iOceanMinAreaSize             = jsonIdInt(identityObj, "oceanMinAreaSize");
	m_iBuildingPrereqModifier       = jsonIdInt(identityObj, "buildingPrereqModifier");
	m_iMaxConscriptModifier         = jsonIdInt(identityObj, "maxConscriptModifier");
	m_iWarWearinessModifier         = jsonIdInt(identityObj, "warWearinessModifier");
	m_iTradeProfitPercent           = jsonIdInt(identityObj, "tradeProfitPercent");
	m_iCorporationMaintenancePercent = jsonIdInt(identityObj, "corporationMaintenancePercent");
	m_iNumCitiesAnarchyPercent      = jsonIdInt(identityObj, "numCitiesAnarchyPercent");
	m_iAdvancedStartPointsMod       = jsonIdInt(identityObj, "advancedStartPointsMod");
	m_iCityLimitsScalePercent       = jsonIdInt(identityObj, "cityLimitsScalePercent", 100);
}
