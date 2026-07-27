//
//	CvCivilizationInfo -- the civilization poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity/world/sound set + the typed game-start grant views -- read
//	off the COMPOSED grants/edges units the base dispatch already parsed and FK-resolved (ONE representation,
//	never a second raw-JSON walk) ([DEC-materialize-at-mapfrom]). Idempotent by contract (unconditional assigns,
//	clear-first containers).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, CvString/CvWString, gDLL
#include "CvCivilizationInfo.h"
#include "CvCivicInfo.h"          // getCivicOption -- keys grants.civics into CivicOption slots
#include "CvArtInfoCivilization.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdBool/jsonIdStr/jsonWorldArt/jsonReadIdList)
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- getFlagTexture / getArtInfo / getButton
#include "Defines/CvGlobals.h"    // GC (getNumCivicOptionInfos / getCivicInfo)

CvCivilizationInfo::CvCivilizationInfo()
	: m_bPlayable(false)
	, m_bAIPlayable(false)
	, m_bStronglyRestricted(false)
	, m_eDerivativeCiv(NO_CIVILIZATION)
	, m_iDefaultPlayerColor(-1)     // NO_PLAYERCOLOR (enum-as-int default)
	, m_iArtStyleType(-1)
	, m_iUnitArtStyleType(-1)
	, m_iSelectionSoundScriptId(-1)
	, m_iActionSoundScriptId(-1)
{
}

void CvCivilizationInfo::reset()
{
	CvInfoBase::reset();
	m_aszAdjective.clear();
	m_aszShortDescription.clear();
}

// #430: the civilization's typed consumer surface from Assets/Data/civilizations/*.json. Base reads type +
// identity text keys + the section DISPATCH (the composed grants/edges/modifiers units parse there). This class
// then materializes: the world.art colors/styles + ART_DEF tag, the sound.* audio-tag indices, the typed
// game-start grant views (buildings/techs off grants; civics keyed into CivicOption slots; the disables.techs
// research ban off the edges), and the identity set (selectability / text keys / leaders / cityNames /
// derivativeCiv). IDEMPOTENT (CvInfo.h): mapFrom re-runs once the FK registry is complete, so every container
// is cleared-then-refilled and every scalar unconditionally redefined each call.
void CvCivilizationInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / description / civilopedia) + the base section dispatch

	// remap-idempotency (CvInfo.h contract): fully define every materialized member
	m_freeBuildings.clear();
	m_freeTechs.clear();
	m_initialCivics.clear();
	m_disabledTechs.clear();
	m_leaders.clear();
	m_cityNames.clear();
	m_aszShortDescription.clear();
	m_aszAdjective.clear();
	m_bPlayable = false;
	m_bAIPlayable = false;
	m_bStronglyRestricted = false;
	m_eDerivativeCiv = NO_CIVILIZATION;
	m_iDefaultPlayerColor = -1;
	m_iArtStyleType = -1;
	m_iUnitArtStyleType = -1;
	m_szArtDefineTag = "";
	m_szShortDescriptionKey.clear();
	m_szAdjectiveKey.clear();
	m_iSelectionSoundScriptId = -1;
	m_iActionSoundScriptId = -1;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	std::string parsedString;

	// --- world.art: playerColor/style/unitStyle are FK strings; define is the ART_DEF_* tag kept as a string ---
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		if (jsonIdStr(*pArt, "playerColor", parsedString))
		{
			m_iDefaultPlayerColor = jsonResolveId(parsedString);
		}
		if (jsonIdStr(*pArt, "style", parsedString))
		{
			m_iArtStyleType = jsonResolveId(parsedString);
		}
		if (jsonIdStr(*pArt, "unitStyle", parsedString))
		{
			m_iUnitArtStyleType = jsonResolveId(parsedString);
		}
		if (jsonIdStr(*pArt, "define", parsedString))
		{
			m_szArtDefineTag = parsedString.c_str();
		}
	}

	// --- sound.selection / sound.action -> RUNTIME audio-tag indices (NOT info-type ids), reproducing the archived
	//     read()'s gDLL->getAudioTagIndex(tag, AUDIOTAG_3DSCRIPT); absent/empty -> -1 (AUDIOTAG_NONE) ---
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		if (jsonIdStr(*pSound, "selection", parsedString) && !parsedString.empty())
		{
			m_iSelectionSoundScriptId = gDLL->getAudioTagIndex(parsedString.c_str(), AUDIOTAG_3DSCRIPT);
		}
		if (jsonIdStr(*pSound, "action", parsedString) && !parsedString.empty())
		{
			m_iActionSoundScriptId = gDLL->getAudioTagIndex(parsedString.c_str(), AUDIOTAG_3DSCRIPT);
		}
	}

	// (spawnRate.empire.npcPeace.percent compiles into the composed m_modifiers via the base dispatch and is
	//  read through the base getScalar(SCALAR_SPAWN_RATE_NPC_PEACE, ...) -- no raw walk, no mirror.)

	// --- the typed game-start grant views: read off the COMPOSED units (already parsed + FK-resolved by the
	//     base dispatch), never a second walk of the raw JSON ---
	{
		const CvGrants* pGrants = m_triggers.consideredGrant();
		const std::vector<int>* pBuildingList = (pGrants != NULL) ? pGrants->list("buildings") : NULL;
		if (pBuildingList != NULL)
		{
			for (size_t iEntry = 0; iEntry < pBuildingList->size(); ++iEntry)
			{
				m_freeBuildings.push_back((BuildingTypes)(*pBuildingList)[iEntry]);
			}
		}
		const std::vector<int>* pTechList = (pGrants != NULL) ? pGrants->list("techs") : NULL;
		if (pTechList != NULL)
		{
			for (size_t iEntry = 0; iEntry < pTechList->size(); ++iEntry)
			{
				m_freeTechs.push_back((TechTypes)(*pTechList)[iEntry]);
			}
		}
		// grants.civics fill the CivicOption-slot vector (each civic dropped in its own option's slot)
		const std::vector<int>* pCivicList = (pGrants != NULL) ? pGrants->list("civics") : NULL;
		if (pCivicList != NULL && !pCivicList->empty())
		{
			const int iNumOptions = GC.getNumCivicOptionInfos();
			m_initialCivics.assign(iNumOptions, NO_CIVIC);
			for (size_t iEntry = 0; iEntry < pCivicList->size(); ++iEntry)
			{
				const int iCivic = (*pCivicList)[iEntry];   // already FK-resolved by the section parse
				const int iOption = GC.getCivicInfo((CivicTypes)iCivic).getCivicOption();
				if (iOption >= 0 && iOption < iNumOptions)   // NO_CIVICOPTION / out-of-range -- skip
				{
					m_initialCivics[iOption] = (CivicTypes)iCivic;
				}
			}
		}
	}

	// --- disables.techs -> the per-civ research ban. Read off the COMPOSED edges unit, so this view and the
	//     enabler's generic edge surface are the same data, never two parses that can drift. ---
	{
		const std::vector<int>* pDisabledList = m_edges.find(EDGEF_DISABLES, EDGEB_TECHS);
		if (pDisabledList != NULL)
		{
			for (size_t iEntry = 0; iEntry < pDisabledList->size(); ++iEntry)
			{
				m_disabledTechs.push_back((TechTypes)(*pDisabledList)[iEntry]);
			}
		}
	}

	// --- identity: selectability, text keys, the leaders + cityNames pools, the derivative-civ FK ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		// selectability + the NPC build-lockdown (curator emits each only when true; absent -> false)
		m_bPlayable = jsonIdBool(*pIdentity, "playable");
		m_bAIPlayable = jsonIdBool(*pIdentity, "aiPlayable");
		m_bStronglyRestricted = jsonIdBool(*pIdentity, "stronglyRestricted");
		// NOTE: identity.isNpc is authored by the curator but has NO CvCivilizationInfo member (no engine consumer
		// on this class) -- deliberately unmapped.

		if (jsonIdStr(*pIdentity, "shortDescription", parsedString))
		{
			m_szShortDescriptionKey = CvWString(parsedString.c_str());
		}
		if (jsonIdStr(*pIdentity, "adjective", parsedString))
		{
			m_szAdjectiveKey = CvWString(parsedString.c_str());
		}
		if (jsonIdStr(*pIdentity, "derivativeCiv", parsedString))
		{
			m_eDerivativeCiv = (CivilizationTypes)jsonResolveId(parsedString);
		}

		// leaders (FK list; the count read serves off the vector's size). The shared reader appends plain
		// ids; the member keeps its typed LeaderHeadTypes element (the getLeaders surface), so cast-copy once.
		{
			std::vector<int> leaderIds;
			jsonReadIdList(*pIdentity, "leaders", leaderIds);
			for (size_t iLeader = 0; iLeader < leaderIds.size(); ++iLeader)
			{
				m_leaders.push_back((LeaderHeadTypes)leaderIds[iLeader]);
			}
		}

		// cityNames -- a TEXT-KEY pool (stored as strings, NOT resolved to ids)
		jsonReadStrList(*pIdentity, "cityNames", m_cityNames);
	}
}

bool CvCivilizationInfo::isFreeTech(TechTypes eTech) const
{
	for (size_t iEntry = 0; iEntry < m_freeTechs.size(); ++iEntry)
	{
		if (m_freeTechs[iEntry] == eTech)
		{
			return true;
		}
	}
	return false;
}

CivicTypes CvCivilizationInfo::getInitialCivic(CivicOptionTypes eCivicOption) const
{
	if (eCivicOption < 0 || eCivicOption >= (int)m_initialCivics.size())
	{
		return NO_CIVIC;
	}
	return m_initialCivics[eCivicOption];
}

// Load-window writer -- cvInternalGlobals::checkInitialCivics defaults an option slot no authored civic filled
// (part of the write-once-at-load window; never called post-load).
void CvCivilizationInfo::setInitialCivic(CivicOptionTypes eCivicOption, CivicTypes eCivic)
{
	if ((int)m_initialCivics.size() < GC.getNumCivicOptionInfos())
	{
		m_initialCivics.resize(GC.getNumCivicOptionInfos(), NO_CIVIC);
	}
	FASSERT_BOUNDS(0, (int)m_initialCivics.size(), eCivicOption);
	m_initialCivics[eCivicOption] = eCivic;
}

bool CvCivilizationInfo::isTechDisabled(TechTypes eTech) const
{
	for (size_t iEntry = 0; iEntry < m_disabledTechs.size(); ++iEntry)
	{
		if (m_disabledTechs[iEntry] == eTech)
		{
			return true;
		}
	}
	return false;
}

bool CvCivilizationInfo::isLeaders(int i) const
{
	for (size_t iEntry = 0; iEntry < m_leaders.size(); ++iEntry)
	{
		if (m_leaders[iEntry] == (LeaderHeadTypes)i)
		{
			return true;
		}
	}
	return false;
}

std::string CvCivilizationInfo::getCityName(int iName) const
{
	FASSERT_BOUNDS(0, (int)m_cityNames.size(), iName);
	return m_cityNames[iName];
}

const wchar_t* CvCivilizationInfo::getShortDescription(uint uiForm)
{
	PROFILE_EXTRA_FUNC();
	while (m_aszShortDescription.size() <= uiForm)
	{
		m_aszShortDescription.push_back(gDLL->getObjectText(m_szShortDescriptionKey, m_aszShortDescription.size()));
	}
	return m_aszShortDescription[uiForm];
}

const wchar_t* CvCivilizationInfo::getAdjective(uint uiForm)
{
	PROFILE_EXTRA_FUNC();
	while (m_aszAdjective.size() <= uiForm)
	{
		m_aszAdjective.push_back(gDLL->getObjectText(m_szAdjectiveKey, m_aszAdjective.size()));
	}
	return m_aszAdjective[uiForm];
}

const CvArtInfoCivilization* CvCivilizationInfo::getArtInfo() const
{
	return ARTFILEMGR.getCivilizationArtInfo(getArtDefineTag());
}

const char* CvCivilizationInfo::getButton() const
{
	const CvArtInfoCivilization* pArtInfoCivilization = getArtInfo();
	return pArtInfoCivilization ? pArtInfoCivilization->getButton() : NULL;
}

const char* CvCivilizationInfo::getFlagTexture() const
{
	return ARTFILEMGR.getCivilizationArtInfo(getArtDefineTag())->getPath();
}
