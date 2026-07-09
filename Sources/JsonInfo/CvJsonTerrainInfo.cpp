//
//	CvJsonTerrainInfo::mapFrom -- base core reading (type + identity text + button) then the terrain's LIVE real members
//	mapped from JSON: the plot-scope yield families, the plot modifier families, and the `identity` terrain fields.
//	HUMAN-native values (the cascade ×100s on its own side). FK resolution via jsonResolveId (the JsonInfo-side
//	CvJsonParse primitive over the kept type registry, with the load-time diagnostics). See the header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonTerrainInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...)
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvJsonTerrainInfo::CvJsonTerrainInfo()
	: m_iMovementCost(0), m_iBuildModifier(0), m_iDefenseModifier(0), m_iCultureDistance(0),
	  m_iDistanceToLand(0), m_iZobristValue(0), m_bFreshWaterTerrain(false),
	  m_bImpassable(false), m_bFound(false), m_bFoundCoast(false), m_bFoundFreshWater(false),
	  m_eClimate(NO_CLIMATE_ZONE), m_iWorldSoundscapeScriptId(-1)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYields[i] = 0;
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvTerrainInfo ctor did (SourceArchive/Infos/CvTerrainInfo.cpp:42). CvPlot XORs it into m_movementCharacteristicsHash;
	// the value must be RNG-quality + cross-client-identical (the pre-game RNG is deterministic), so it is stored, not defaulted.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

void CvJsonTerrainInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading: type + identity text + button
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// yields: the plot-scope flat of each split yield family (human ints)
	m_aiYields[YIELD_FOOD]       = jsonFamVal(o, "food", "plot", "flat");
	m_aiYields[YIELD_PRODUCTION] = jsonFamVal(o, "production", "plot", "flat");
	m_aiYields[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "plot", "flat");

	// plot modifier families
	m_iBuildModifier   = jsonFamVal(o, "buildTime", "plot", "percent");
	m_iCultureDistance = jsonFamVal(o, "cultureDistance", "plot", "flat");
	m_iDefenseModifier = jsonFamMemberVal(o, "defense", "plot", "amount", "percent");

	// identity: the terrain relief + climate fields
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iMovementCost      = jsonIdInt(*io, "movementCost");
		m_iDistanceToLand    = jsonIdInt(*io, "distanceToLand");
		m_bFreshWaterTerrain = jsonIdBool(*io, "freshWaterTerrain");
		m_bImpassable        = jsonIdBool(*io, "impassable");
		m_bFound             = jsonIdBool(*io, "found");
		m_bFoundCoast        = jsonIdBool(*io, "foundCoast");
		m_bFoundFreshWater   = jsonIdBool(*io, "foundFreshWater");

		picojson::object::const_iterator cl = io->find("climate");
		if (cl != io->end() && cl->second.is<std::string>())
			m_eClimate = (ClimateZoneTypes)jsonResolveId(cl->second.get<std::string>());

		picojson::object::const_iterator mc = io->find("mapCategories");
		if (mc != io->end() && mc->second.is<picojson::array>())
		{
			const picojson::array& a = mc->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				if (a[i].is<std::string>())
				{
					const int id = jsonResolveId(a[i].get<std::string>());
					if (id >= 0) m_aeMapCategories.push_back((MapCategoryTypes)id);
				}
		}
	}

	// world.art.icon -- the ART_DEF_* tag (EXE map-gen art lookup, via the CvTerrainInfo shim's getArtDefineTag)
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "define", m_szArtDefineTag);

	// sound: resolve the on-map audio string tags to runtime audio-manager indices at info-load -- EXACTLY the archived
	// CvTerrainInfo::read mechanism (gDLL->getAudioTagIndex). soundscape -> AUDIOTAG_SOUNDSCAPE; each footsteps entry
	// keys a FOOTSTEP_AUDIO_* type (jsonResolveId == legacy GetInfoClass) to its AS3D_* script tag (default iScriptType).
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		picojson::object::const_iterator ss = snd->find("soundscape");
		if (ss != snd->end() && ss->second.is<std::string>() && ss->second.get<std::string>().length() > 0)
			m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(ss->second.get<std::string>().c_str(), AUDIOTAG_SOUNDSCAPE);
		// else: m_iWorldSoundscapeScriptId stays -1 (the legacy absent-tag read default).

		picojson::object::const_iterator fs = snd->find("footsteps");
		if (fs != snd->end() && fs->second.is<picojson::array>())
		{
			const picojson::array& a = fs->second.get<picojson::array>();
			m_ai3DAudioScriptFootstepIndex.assign(GC.getNumFootstepAudioTypes(), -1);   // legacy InitList default -1
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& e = a[i].get<picojson::object>();
				for (picojson::object::const_iterator it = e.begin(); it != e.end(); ++it)
				{
					const int iType = jsonResolveId(it->first);   // FOOTSTEP_AUDIO_* -> footstep audio type index (legacy GetInfoClass)
					if (iType < 0 || iType >= (int)m_ai3DAudioScriptFootstepIndex.size()) continue;   // mirrors legacy's iIndexVal != -1 skip
					if (it->second.is<std::string>() && it->second.get<std::string>().length() > 0)
						m_ai3DAudioScriptFootstepIndex[iType] = gDLL->getAudioTagIndex(it->second.get<std::string>().c_str());
					// empty script tag -> slot stays -1 (legacy szTemp.GetLength() > 0 ? ... : -1)
				}
			}
		}
	}
	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}
