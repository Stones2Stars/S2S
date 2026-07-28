//
//	CvTerrainInfo -- the terrain poco's own typed reading on top of the base section dispatch (see the header).
//	The yield/defense/cultureDistance families compile into m_modifiers via the base dispatch -- no per-family
//	raw read survives here ([DEC-new-getter-surface]). mapFrom materializes the identity/art/sound census set
//	ONCE into typed members ([DEC-materialize-at-mapfrom]); idempotent by contract.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvTerrainInfo.h"
#include "UI/CvArtFileMgr.h"           // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoTerrain.h"    // complete CvArtInfoTerrain -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/jsonIdBool/jsonIdStr/jsonWorldArt)
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvTerrainInfo::CvTerrainInfo()
	, m_iBuildModifier(0)
	, m_iDistanceToLand(0)
	, m_iZobristValue(0)
	, m_iWorldSoundscapeScriptId(-1)
	, m_bFreshWaterTerrain(false)
	, m_bImpassable(false)
	, m_bFound(false)
	, m_bFoundCoast(false)
	, m_bFoundFreshWater(false)
	, m_eClimate(NO_CLIMATE_ZONE)
{
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvTerrainInfo ctor did. CvPlot XORs it into m_movementCharacteristicsHash; the value must be RNG-quality
	// and cross-client-identical (the pre-game RNG is deterministic), so it is stored, not defaulted.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

void CvTerrainInfo::mapFrom(const picojson::value& entity)
{
	m_aeMapCategories.clear();   // remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom
	CvInfo::mapFrom(entity);     // core reading + the section dispatch (compiles m_modifiers)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// identity: the terrain relief + climate fields (+ the worker build-time percent -- substrate self-data,
	// ruling 18 plane 1: identity.buildTimeModifier, never a modifier family)
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iBuildModifier = jsonIdInt(*pIdentity, "buildTimeModifier");
		m_iDistanceToLand = jsonIdInt(*pIdentity, "distanceToLand");
		m_bFreshWaterTerrain = jsonIdBool(*pIdentity, "freshWaterTerrain");
		m_bImpassable = jsonIdBool(*pIdentity, "impassable");
		m_bFound = jsonIdBool(*pIdentity, "found");
		m_bFoundCoast = jsonIdBool(*pIdentity, "foundCoast");
		m_bFoundFreshWater = jsonIdBool(*pIdentity, "foundFreshWater");

		// identity.climateZoneType (18 water terrains). CLIMATE_ZONE_ loads before TERRAIN_, so this
		// cross-class FK resolves at first-pass read time; the full-registry re-run re-resolves regardless.
		picojson::object::const_iterator climateIter = pIdentity->find("climateZoneType");
		if (climateIter != pIdentity->end() && climateIter->second.is<std::string>())
		{
			m_eClimate = (ClimateZoneTypes)jsonResolveId(climateIter->second.get<std::string>());
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

	// world.art.icon -- the ART_DEF_* tag (EXE map-gen art lookup). Cleared first: jsonIdStr only assigns when
	// the key is present (mapFrom idempotency, CvInfo.h).
	m_szArtDefineTag.clear();
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);
	}

	// sound: resolve the on-map audio string tags to runtime audio-manager indices at info-load -- EXACTLY the
	// archived read mechanism (gDLL->getAudioTagIndex). soundscape -> AUDIOTAG_SOUNDSCAPE; each footsteps entry
	// keys a FOOTSTEP_AUDIO_* type to its AS3D_* script tag.
	m_iWorldSoundscapeScriptId = -1;
	m_ai3DAudioScriptFootstepIndex.clear();
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
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
	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}

const CvArtInfoTerrain* CvTerrainInfo::getArtInfo() const
{
	return ARTFILEMGR.getTerrainArtInfo(getArtDefineTag());
}

const char* CvTerrainInfo::getButton() const
{
	const CvArtInfoTerrain* pArtInfo = getArtInfo();
	return pArtInfo != NULL ? pArtInfo->getButton() : "";
}
