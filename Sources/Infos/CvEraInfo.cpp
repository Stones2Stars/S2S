//------------------------------------------------------------------------------------------------
//  FILE:    CvEraInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, gDLL, AUDIOTAG_*, SAFE_DELETE_ARRAY
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"    // GC (getNumCitySizeTypes)
#include "CvEraInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonFamVal / jsonFamMemberVal / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId


CvEraInfo::CvEraInfo() :
	m_iStartingUnitMultiplier(0),
	m_iStartingDefenseUnits(0),
	m_iStartingWorkerUnits(0),
	m_iStartingExploreUnits(0),
	m_iAdvancedStartPoints(0),
	m_iStartingGold(0),
	m_iFreePopulation(0),
	m_iHistoricalStartYear(0),
	m_iHistoricalEndYear(0),
	m_iNormalSpeedTurns(0),
	m_iGrowthPercent(0),
	m_iTrainPercent(0),
	m_iConstructPercent(0),
	m_iCreatePercent(0),
	m_iResearchPercent(0),
	m_iBuildPercent(0),
	m_iImprovementPercent(0),
	m_iGreatPeoplePercent(0),
	m_iAnarchyPercent(0),
	m_iEventChancePerTurn(0),
	m_iSoundtrackSpace(0),
	m_iNumSoundtracks(0),
	m_iCuttingEdgeCutsTechCostModifier(0),
	m_iInitialCityMaintenancePercent(0),
	m_bNoGoodies(false),
	m_bNoAnimals(false),
	m_bNoBarbUnits(false),
	m_bNoBarbCities(false),
	m_bFirstSoundtrackFirst(false),
	m_paiSoundtracks(NULL),
	m_paiCitySoundscapeSciptIds(NULL)
{
}


CvEraInfo::~CvEraInfo()
{
	SAFE_DELETE_ARRAY(m_paiCitySoundscapeSciptIds);
	SAFE_DELETE_ARRAY(m_paiSoundtracks);
}


// Arrays

int CvEraInfo::getSoundtracks(int i) const
{
	FASSERT_BOUNDS(0, getNumSoundtracks(), i);
	return m_paiSoundtracks ? m_paiSoundtracks[i] : -1;
}


int CvEraInfo::getCitySoundscapeSciptId(int i) const
{
//	FAssertMsg(i < ?, "Index out of bounds");
	FAssertMsg(i > -1, "Index out of bounds");
	return m_paiCitySoundscapeSciptIds ? m_paiCitySoundscapeSciptIds[i] : -1;
}


// #430: the JSON load hook. costs.world.<thing>.percent -> the per-produced-thing cost multipliers
// (train/construct/create/research/build/improvementUpgrade) + researchCutBelowEra; growth/greatPeopleRate/
// eventChance are singleton world families; durations.world.anger.percent is the anarchy multiplier;
// maintenance.city.initial.flat is the initial-city-maintenance percent (clamped >= 0, as read() did).
// grants.* are the one-shot starting grants; identity.* the pacing/advanced-start scalars; worldGen.* the
// barbarian/goody world-rule gates. sound.* carries the era audio: unitVictory/unitDefeat scripts (strings),
// soundtrackSpace, introSoundtrack (bFirstSoundtrackFirst), and the two runtime-resolved audio-tag arrays --
// soundtracks (AUDIOTAG_2DSCRIPT, gated on !getAudioDisabled, EXACTLY as read()) and citySoundscapes (the
// SetVariableListTagPairForAudioScripts port: CITYSIZE_* -> city-size index -> getAudioTagIndex, default -1).
// jsonFamVal / jsonFamMemberVal return the RAW human value (the x100 fixed-point lives only in the cascade
// deposit tree, never on these engine-getter members).
void CvEraInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// idempotent re-run (loadJson re-runs mapFrom once the registry is complete): drop any prior arrays.
	SAFE_DELETE_ARRAY(m_paiSoundtracks);
	SAFE_DELETE_ARRAY(m_paiCitySoundscapeSciptIds);
	m_iNumSoundtracks = 0;

	// costs.world.<member>.percent -- the per-produced-thing cost multipliers + the summed-band research cut.
	m_iTrainPercent                    = jsonFamMemberVal(o, "costs", "world", "train", "percent");
	m_iConstructPercent                = jsonFamMemberVal(o, "costs", "world", "construct", "percent");
	m_iCreatePercent                   = jsonFamMemberVal(o, "costs", "world", "create", "percent");
	m_iResearchPercent                 = jsonFamMemberVal(o, "costs", "world", "research", "percent");
	m_iBuildPercent                    = jsonFamMemberVal(o, "costs", "world", "build", "percent");
	m_iImprovementPercent              = jsonFamMemberVal(o, "costs", "world", "improvementUpgrade", "percent");
	m_iCuttingEdgeCutsTechCostModifier = jsonFamMemberVal(o, "costs", "world", "researchCutBelowEra", "percent");

	// singleton world families + the grouped anarchy/maintenance members.
	m_iGrowthPercent                 = jsonFamVal(o, "growth", "world", "percent");
	m_iGreatPeoplePercent            = jsonFamVal(o, "greatPeopleRate", "world", "percent");
	m_iEventChancePerTurn            = jsonFamVal(o, "eventChance", "world", "flat");
	m_iAnarchyPercent                = jsonFamMemberVal(o, "durations", "world", "anger", "percent");
	m_iInitialCityMaintenancePercent = jsonFamMemberVal(o, "maintenance", "city", "initial", "flat");
	if (m_iInitialCityMaintenancePercent < 0)   // read() clamp preserved
		m_iInitialCityMaintenancePercent = 0;

	// grants.* -- the one-shot starting grants, read off the COMPOSED unit (§5 numeric pulses, stored ×100 by the
	// section parse; /100 back to the human count). ONE representation: these scalars and the grants machine both
	// read the same parsed pulses, so they cannot drift and the scalars retire with the legacy apply sites.
	m_iStartingGold           = m_grants.pulse100("startingGold") / 100;
	m_iStartingUnitMultiplier = m_grants.pulse100("startingUnitMultiplier") / 100;
	m_iStartingDefenseUnits   = m_grants.pulse100("startingDefenseUnits") / 100;
	m_iStartingWorkerUnits    = m_grants.pulse100("startingWorkerUnits") / 100;
	m_iStartingExploreUnits   = m_grants.pulse100("startingExploreUnits") / 100;
	m_iFreePopulation         = m_grants.pulse100("freePopulation") / 100;

	// identity.* -- pacing inputs + the advanced-start budget (plain ints, may be negative; default 0).
	if (const picojson::object* id = jsonChildObj(o, "identity"))
	{
		m_iHistoricalStartYear = jsonIdInt(*id, "historicalStartYear");
		m_iHistoricalEndYear   = jsonIdInt(*id, "historicalEndYear");
		m_iNormalSpeedTurns    = jsonIdInt(*id, "normalSpeedTurns");
		m_iAdvancedStartPoints = jsonIdInt(*id, "advancedStart");
	}

	// worldGen.* -- barbarian/goody world-rule gates (bools; false/absent in every current era -> default false).
	if (const picojson::object* wg = jsonChildObj(o, "worldGen"))
	{
		m_bNoGoodies    = jsonIdBool(*wg, "noGoodies");
		m_bNoBarbUnits  = jsonIdBool(*wg, "noBarbUnits");
		m_bNoBarbCities = jsonIdBool(*wg, "noBarbCities");
	}

	// sound.* -- era audio (scalars, flags, and the two runtime-resolved audio-tag arrays).
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		m_iSoundtrackSpace      = jsonIdInt(*snd, "soundtrackSpace");
		m_bFirstSoundtrackFirst = jsonIdBool(*snd, "introSoundtrack");
		std::string sv;
		if (jsonIdStr(*snd, "unitVictory", sv)) m_szAudioUnitVictoryScript = sv.c_str();
		std::string sd;
		if (jsonIdStr(*snd, "unitDefeat", sd)) m_szAudioUnitDefeatScript = sd.c_str();

		// soundtracks: the era's 2D script tags, resolved to audio-manager indices at load (AUDIOTAG_2DSCRIPT),
		// -1 when audio is disabled -- EXACTLY the archived read() mechanism.
		picojson::object::const_iterator st = snd->find("soundtracks");
		if (st != snd->end() && st->second.is<picojson::array>())
		{
			const picojson::array& a = st->second.get<picojson::array>();
			m_iNumSoundtracks = (int)a.size();
			if (m_iNumSoundtracks > 0)
			{
				m_paiSoundtracks = new int[m_iNumSoundtracks];
				for (int j = 0; j < m_iNumSoundtracks; j++)
				{
					m_paiSoundtracks[j] = -1;
					if (a[j].is<std::string>())
					{
						const std::string& s = a[j].get<std::string>();
						m_paiSoundtracks[j] = (!gDLL->getAudioDisabled())
							? gDLL->getAudioTagIndex(s.c_str(), AUDIOTAG_2DSCRIPT) : -1;
					}
				}
			}
		}

		// citySoundscapes: a CITYSIZE_* -> script map, one entry per {CITYSIZE_X: "ASSS_..."} object -- the
		// SetVariableListTagPairForAudioScripts port. Array sized by GC.getNumCitySizeTypes(), default -1.
		picojson::object::const_iterator cs = snd->find("citySoundscapes");
		if (cs != snd->end() && cs->second.is<picojson::array>())
		{
			const int iNum = GC.getNumCitySizeTypes();
			if (iNum > 0)
			{
				m_paiCitySoundscapeSciptIds = new int[iNum];
				for (int k = 0; k < iNum; k++) m_paiCitySoundscapeSciptIds[k] = -1;   // legacy InitList default

				const picojson::array& a = cs->second.get<picojson::array>();
				for (int i = 0; i < (int)a.size(); i++)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					for (picojson::object::const_iterator it = e.begin(); it != e.end(); ++it)
					{
						const int iIndexVal = jsonResolveId(it->first);   // CITYSIZE_* -> size index (legacy GetInfoClass)
						if (iIndexVal < 0 || iIndexVal >= iNum) continue;
						if (it->second.is<std::string>() && it->second.get<std::string>().length() > 0)
							m_paiCitySoundscapeSciptIds[iIndexVal] = gDLL->getAudioTagIndex(it->second.get<std::string>().c_str());
						// empty tag -> slot stays -1 (legacy szTemp.GetLength() > 0 ? ... : -1)
					}
				}
			}
		}
	}
}
