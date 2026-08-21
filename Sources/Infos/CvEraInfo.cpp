//
//	CvEraInfo -- the era poco's exemplar reads on top of the base section dispatch (see the header). The
//	legacy modifier-family scalar MIRRORS are DEAD (wave D): the base dispatch compiles the world-scope
//	families into m_modifiers and the point getters read the compiled sums (docs/architecture/patterns.md §Materialize at mapFrom --
//	no raw-JSON family walker survives). mapFrom materializes ONCE, idempotently: the §5 grants views, the
//	identity pacing config, and the era audio (the two runtime-resolved audio-tag arrays).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, gDLL, AUDIOTAG_*, SAFE_DELETE_ARRAY
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"    // GC (getNumCitySizeTypes)
#include "CvEraInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId


CvEraInfo::CvEraInfo()
	: m_iStartingUnitMultiplier(0)
	, m_iStartingDefenseUnits(0)
	, m_iStartingWorkerUnits(0)
	, m_iStartingExploreUnits(0)
	, m_iStartingGold(0)
	, m_iFreePopulation(0)
	, m_iOrder(0)
	, m_iAdvancedStartPoints(0)
	, m_iHistoricalStartYear(0)
	, m_iHistoricalEndYear(0)
	, m_iNormalSpeedTurns(0)
	, m_iSoundtrackSpace(0)
	, m_iNumSoundtracks(0)
	, m_bFirstSoundtrackFirst(false)
	, m_paiSoundtracks(NULL)
	, m_paiCitySoundscapeScriptIds(NULL)
{
}


CvEraInfo::~CvEraInfo()
{
	SAFE_DELETE_ARRAY(m_paiCitySoundscapeScriptIds);
	SAFE_DELETE_ARRAY(m_paiSoundtracks);
}


int CvEraInfo::getSoundtracks(int iIndex) const
{
	FASSERT_BOUNDS(0, getNumSoundtracks(), iIndex);
	if (m_paiSoundtracks == NULL)
	{
		return -1;
	}
	return m_paiSoundtracks[iIndex];
}


int CvEraInfo::getCitySoundscapeScriptId(int iCitySize) const
{
	FAssertMsg(iCitySize > -1, "Index out of bounds");
	if (m_paiCitySoundscapeScriptIds == NULL)
	{
		return -1;
	}
	return m_paiCitySoundscapeScriptIds[iCitySize];
}


// ======================= mapFrom -- the ONE load hook (idempotent by contract) ============================
// The §6 families (costs.world.* incl. researchCutBelowEra / growth / greatPeopleRate / durations.anger /
// eventChance) compile into m_modifiers via the base dispatch -- no per-family read here. grants.* are the
// one-shot starting pulses; identity.* the pacing/advanced-start/order config; sound.* the era audio:
// unitVictory/unitDefeat scripts, soundtrackSpace, introSoundtrack, and the two runtime-resolved audio-tag
// arrays -- soundtracks (AUDIOTAG_2DSCRIPT, gated on !getAudioDisabled, exactly as the archived read()) and
// citySoundscapes (CITYSIZE_* -> city-size index -> getAudioTagIndex, default -1).
void CvEraInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the section dispatch (compiles m_modifiers)

	// idempotent re-run (loadJson re-runs mapFrom once the registry is complete): fully redefine everything.
	SAFE_DELETE_ARRAY(m_paiSoundtracks);
	SAFE_DELETE_ARRAY(m_paiCitySoundscapeScriptIds);
	m_iNumSoundtracks = 0;
	m_iSoundtrackSpace = 0;
	m_bFirstSoundtrackFirst = false;
	m_szAudioUnitVictoryScript.clear();
	m_szAudioUnitDefeatScript.clear();
	m_iOrder = 0;
	m_iAdvancedStartPoints = 0;
	m_iHistoricalStartYear = 0;
	m_iHistoricalEndYear = 0;
	m_iNormalSpeedTurns = 0;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- grants views: the one-shot starting grants, read off the COMPOSED unit (§5 numeric pulses, stored
	//     ×100 by the section parse; /100 back to the human count). ONE representation: these views and the
	//     grants machine read the same parsed pulses, so they cannot drift. ---
	const CvGrants* pGrants = m_triggers.consideredGrant();
	m_iStartingGold           = (pGrants != NULL) ? pGrants->pulse("startingGold") / 100 : 0;
	m_iStartingUnitMultiplier = (pGrants != NULL) ? pGrants->pulse("startingUnitMultiplier") / 100 : 0;
	m_iStartingDefenseUnits   = (pGrants != NULL) ? pGrants->pulse("startingDefenseUnits") / 100 : 0;
	m_iStartingWorkerUnits    = (pGrants != NULL) ? pGrants->pulse("startingWorkerUnits") / 100 : 0;
	m_iStartingExploreUnits   = (pGrants != NULL) ? pGrants->pulse("startingExploreUnits") / 100 : 0;
	m_iFreePopulation         = (pGrants != NULL) ? pGrants->pulse("freePopulation") / 100 : 0;

	// --- identity: the sequence position + pacing inputs + the advanced-start budget (plain ints) ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iOrder               = jsonIdInt(*pIdentity, "order");
		m_iHistoricalStartYear = jsonIdInt(*pIdentity, "historicalStartYear");
		m_iHistoricalEndYear   = jsonIdInt(*pIdentity, "historicalEndYear");
		m_iNormalSpeedTurns    = jsonIdInt(*pIdentity, "normalSpeedTurns");
		m_iAdvancedStartPoints = jsonIdInt(*pIdentity, "advancedStart");
	}

	// --- sound: the era audio (scalars, flags, and the two runtime-resolved audio-tag arrays) ---
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		m_iSoundtrackSpace      = jsonIdInt(*pSound, "soundtrackSpace");
		m_bFirstSoundtrackFirst = jsonIdBool(*pSound, "introSoundtrack");
		std::string szScript;
		if (jsonIdStr(*pSound, "unitVictory", szScript))
		{
			m_szAudioUnitVictoryScript = szScript.c_str();
		}
		if (jsonIdStr(*pSound, "unitDefeat", szScript))
		{
			m_szAudioUnitDefeatScript = szScript.c_str();
		}

		// soundtracks: the era's 2D script tags, resolved to audio-manager indices at load (AUDIOTAG_2DSCRIPT),
		// -1 when audio is disabled -- exactly the archived read() mechanism.
		picojson::object::const_iterator soundtracksIt = pSound->find("soundtracks");
		if (soundtracksIt != pSound->end() && soundtracksIt->second.is<picojson::array>())
		{
			const picojson::array& trackList = soundtracksIt->second.get<picojson::array>();
			m_iNumSoundtracks = (int)trackList.size();
			if (m_iNumSoundtracks > 0)
			{
				m_paiSoundtracks = new int[m_iNumSoundtracks];
				for (int iTrack = 0; iTrack < m_iNumSoundtracks; iTrack++)
				{
					m_paiSoundtracks[iTrack] = -1;
					if (trackList[iTrack].is<std::string>())
					{
						const std::string& szTag = trackList[iTrack].get<std::string>();
						if (!gDLL->getAudioDisabled())
						{
							m_paiSoundtracks[iTrack] = gDLL->getAudioTagIndex(szTag.c_str(), AUDIOTAG_2DSCRIPT);
						}
					}
				}
			}
		}

		// citySoundscapes: a CITYSIZE_* -> script map, one entry per {CITYSIZE_X: "ASSS_..."} object -- the
		// SetVariableListTagPairForAudioScripts port. Array sized by GC.getNumCitySizeTypes(), default -1.
		picojson::object::const_iterator soundscapesIt = pSound->find("citySoundscapes");
		if (soundscapesIt != pSound->end() && soundscapesIt->second.is<picojson::array>())
		{
			const int iNumSizes = GC.getNumCitySizeTypes();
			if (iNumSizes > 0)
			{
				m_paiCitySoundscapeScriptIds = new int[iNumSizes];
				for (int iSize = 0; iSize < iNumSizes; iSize++)
				{
					m_paiCitySoundscapeScriptIds[iSize] = -1;   // legacy InitList default
				}
				const picojson::array& soundscapeList = soundscapesIt->second.get<picojson::array>();
				for (size_t iRow = 0; iRow < soundscapeList.size(); ++iRow)
				{
					if (!soundscapeList[iRow].is<picojson::object>())
					{
						continue;
					}
					const picojson::object& rowObj = soundscapeList[iRow].get<picojson::object>();
					for (picojson::object::const_iterator rowIt = rowObj.begin(); rowIt != rowObj.end(); ++rowIt)
					{
						const int iSizeIndex = jsonResolveId(rowIt->first);   // CITYSIZE_* -> size index (legacy GetInfoClass)
						if (iSizeIndex < 0 || iSizeIndex >= iNumSizes)
						{
							continue;
						}
						if (rowIt->second.is<std::string>() && rowIt->second.get<std::string>().length() > 0)
						{
							m_paiCitySoundscapeScriptIds[iSizeIndex] = gDLL->getAudioTagIndex(rowIt->second.get<std::string>().c_str());
						}
						// empty tag -> slot stays -1 (legacy szTemp.GetLength() > 0 ? ... : -1)
					}
				}
			}
		}
	}
}
