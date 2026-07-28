//
//	CvFeatureInfo -- the feature poco's own typed reading on top of the base section dispatch (see the header).
//	The yield/defense/health/cultureDistance families compile into m_modifiers via the base dispatch -- no
//	per-family raw read survives here ([DEC-new-getter-surface]); the HAS_RIVER yield extras are compiled
//	conditioned entries. mapFrom materializes the identity/cost/art/sound census set ONCE into typed members
//	([DEC-materialize-at-mapfrom]); idempotent by contract.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvFeatureInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoFeature.h"   // complete CvArtInfoFeature -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/jsonIdBool/jsonIdStr/jsonWorldArt)
#include "Property/CvPropertyBridge.h" // the shared `triggers` PROPERTY pulse -> manipulator walk
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvFeatureInfo::CvFeatureInfo()
	, m_iSeeThroughChange(0)
	, m_iPopDestroys(-1)
	, m_iAppearanceProbability(0)
	, m_iDisappearanceProbability(0)
	, m_iGrowthProbability(0)
	, m_iSpreadProbability(0)
	, m_iAdvancedStartRemoveCost(0)
	, m_iEffectProbability(0)
	, m_iZobristValue(0)
	, m_iWorldSoundscapeScriptId(-1)
	, m_bImpassable(false)
	, m_bRequiresFlatlands(false)
	, m_bRequiresRiver(false)
	, m_bAddsFreshWater(false)
	, m_bNoCoast(false)
	, m_bNoRiver(false)
	, m_bNoAdjacent(false)
	, m_bCoastalOnly(false)
	, m_bVisibleAlways(false)
	, m_bCanGrowAnywhere(false)
{
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvFeatureInfo ctor did. CvPlot XORs it into m_movementCharacteristicsHash.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

bool CvFeatureInfo::isOnlyBad() const
{
	// The BUG city-plot-status test, over the compiled reads: any positive health modifier, fresh water, or
	// positive own tile yield means the feature is not purely bad.
	if (getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT) > 0 || isAddsFreshWater())
	{
		return false;
	}
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		if (getFlatYield((YieldTypes)iYield, CASC_SCOPE_PLOT) > 0)
		{
			return false;
		}
	}
	return true;
}

bool CvFeatureInfo::isTerrain(int iTerrain) const
{
	for (size_t iEntry = 0; iEntry < m_aeValidTerrains.size(); ++iEntry)
	{
		if ((int)m_aeValidTerrains[iEntry] == iTerrain)
		{
			return true;
		}
	}
	return false;
}

void CvFeatureInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	m_aeValidTerrains.clear();
	m_aeMapCategories.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: the feature's `triggers` property pulses (bamboo/jungle air-pollution
	// sinks, reef water-pollution sinks) feed the KEEP-legacy solver via the plot gather. The ONE shared
	// pulse walk (clear-and-refill inside).
	CascadePropertyBridge::bridgePulses(getTriggers(), m_PropertyManipulators);

	// vision -- the §9 bespoke line-of-sight block (vision.plot.seeThrough.flat)
	m_iSeeThroughChange = 0;
	if (const picojson::object* pVision = jsonChildObj(entityObj, "vision"))
	{
		if (const picojson::object* pPlot = jsonChildObj(*pVision, "plot"))
		{
			if (const picojson::object* pSeeThrough = jsonChildObj(*pPlot, "seeThrough"))
			{
				m_iSeeThroughChange = jsonIdInt(*pSeeThrough, "flat");
			}
		}
	}

	// identity: placement + relief fields
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iPopDestroys = jsonIdInt(*pIdentity, "popDestroys", -1);   // legacy load default -1 = never destroyed
		m_iAppearanceProbability = jsonIdInt(*pIdentity, "appearance");
		m_iDisappearanceProbability = jsonIdInt(*pIdentity, "disappearance");
		m_iGrowthProbability = jsonIdInt(*pIdentity, "growth");
		m_iSpreadProbability = jsonIdInt(*pIdentity, "spread");
		m_bImpassable = jsonIdBool(*pIdentity, "impassable");
		m_bRequiresFlatlands = jsonIdBool(*pIdentity, "requiresFlatlands");
		m_bRequiresRiver = jsonIdBool(*pIdentity, "requiresRiver");
		m_bAddsFreshWater = jsonIdBool(*pIdentity, "addsFreshWater");
		m_bNoCoast = jsonIdBool(*pIdentity, "noCoast");
		m_bNoRiver = jsonIdBool(*pIdentity, "noRiver");
		m_bNoAdjacent = jsonIdBool(*pIdentity, "noAdjacent");
		m_bCoastalOnly = jsonIdBool(*pIdentity, "coastalOnly");
		m_bVisibleAlways = jsonIdBool(*pIdentity, "visibleAlways");
		m_bCanGrowAnywhere = jsonIdBool(*pIdentity, "canGrowAnywhere");

		picojson::object::const_iterator terrainsIter = pIdentity->find("validTerrains");
		if (terrainsIter != pIdentity->end() && terrainsIter->second.is<picojson::array>())
		{
			const picojson::array& terrainList = terrainsIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < terrainList.size(); ++iEntry)
			{
				if (!terrainList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(terrainList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aeValidTerrains.push_back((TerrainTypes)iResolved);
				}
			}
		}
		picojson::object::const_iterator categoriesIter = pIdentity->find("mapCategories");
		if (categoriesIter != pIdentity->end() && categoriesIter->second.is<picojson::array>())
		{
			const picojson::array& categoryList = categoriesIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < categoryList.size(); ++iEntry)
			{
				if (!categoryList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(categoryList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aeMapCategories.push_back((MapCategoryTypes)iResolved);
				}
			}
		}
	}

	// cost block: the advanced-start feature-removal cost
	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iAdvancedStartRemoveCost = jsonIdInt(*pCost, "advancedStartRemoveCost");
	}

	// world.art: the ART_DEF_* icon tag + the on-map effect (type + probability). Cleared first: jsonIdStr only
	// assigns when the key is present (mapFrom idempotency, CvInfo.h).
	m_szArtDefineTag.clear();
	m_szEffectType.clear();
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);
		if (const picojson::object* pEffect = jsonChildObj(*pArt, "effect"))
		{
			jsonIdStr(*pEffect, "type", m_szEffectType);
			m_iEffectProbability = jsonIdInt(*pEffect, "probability");
		}
	}

	// sound block: the feature-growth 2D sound + the on-map audio tags resolved to runtime audio-manager
	// indices at info-load (gDLL->getAudioTagIndex -- the archived read mechanism). soundscape ->
	// AUDIOTAG_SOUNDSCAPE; each footsteps entry keys a FOOTSTEP_AUDIO_* type to its AS3D_* script tag.
	m_iWorldSoundscapeScriptId = -1;
	m_ai3DAudioScriptFootstepIndex.clear();
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		jsonIdStr(*pSound, "growth", m_szGrowthSound);

		picojson::object::const_iterator soundscapeIter = pSound->find("soundscape");
		if (soundscapeIter != pSound->end() && soundscapeIter->second.is<std::string>()
			&& soundscapeIter->second.get<std::string>().length() > 0)
		{
			m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(soundscapeIter->second.get<std::string>().c_str(), AUDIOTAG_SOUNDSCAPE);
		}

		picojson::object::const_iterator footstepsIter = pSound->find("footsteps");
		if (footstepsIter != pSound->end() && footstepsIter->second.is<picojson::array>())
		{
			const picojson::array& footstepList = footstepsIter->second.get<picojson::array>();
			m_ai3DAudioScriptFootstepIndex.assign(GC.getNumFootstepAudioTypes(), -1);   // legacy InitList default -1
			for (size_t iEntry = 0; iEntry < footstepList.size(); ++iEntry)
			{
				if (!footstepList[iEntry].is<picojson::object>())
				{
					continue;
				}
				const picojson::object& footstepObj = footstepList[iEntry].get<picojson::object>();
				for (picojson::object::const_iterator footstepIter = footstepObj.begin(); footstepIter != footstepObj.end(); ++footstepIter)
				{
					const int iFootstepType = jsonResolveId(footstepIter->first);   // FOOTSTEP_AUDIO_* -> type index
					if (iFootstepType < 0 || iFootstepType >= (int)m_ai3DAudioScriptFootstepIndex.size())
					{
						continue;   // mirrors the legacy iIndexVal != -1 skip
					}
					if (footstepIter->second.is<std::string>() && footstepIter->second.get<std::string>().length() > 0)
					{
						m_ai3DAudioScriptFootstepIndex[iFootstepType] = gDLL->getAudioTagIndex(footstepIter->second.get<std::string>().c_str());
					}
					// empty script tag -> slot stays -1 (legacy szTemp.GetLength() > 0 ? ... : -1)
				}
			}
		}
	}

	// grants block: the on-entry unit transform (module-only; no base feature authors it)
	if (const picojson::object* pGrants = jsonChildObj(entityObj, "grants"))
	{
		jsonIdStr(*pGrants, "onUnitChangeTo", m_szOnUnitChangeTo);
	}

	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}

const CvArtInfoFeature* CvFeatureInfo::getArtInfo() const
{
	return ARTFILEMGR.getFeatureArtInfo(getArtDefineTag());
}

const char* CvFeatureInfo::getButton() const
{
	const CvArtInfoFeature* pArtInfo = getArtInfo();
	return pArtInfo != NULL ? pArtInfo->getButton() : "";
}

// Art-define tier: variety count + secondary-render test live in the art define (CvArtInfoFeature), not
// feature-curator output. Delegate exactly as the archived class did.
int CvFeatureInfo::getNumVarieties() const
{
	const CvArtInfoFeature* pArtInfo = getArtInfo();
	return pArtInfo != NULL ? pArtInfo->getNumVarieties() : 0;
}

bool CvFeatureInfo::canBeSecondary() const
{
	const CvArtInfoFeature* pArtInfo = getArtInfo();
	return pArtInfo != NULL && !(pArtInfo->isRiverArt() || pArtInfo->getTileArtType() != TILE_ART_TYPE_NONE);
}
