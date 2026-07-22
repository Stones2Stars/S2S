//------------------------------------------------------------------------------------------------
//  FILE:    CvCivilizationInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, CvString/CvWString, gDLL, SAFE_DELETE_ARRAY
#include "CvInfos.h"              // umbrella: CvCivicInfo (getCivicOptionType) + CvArtInfoCivilization + leakage guard
#include "AI/CvGameAI.h"
#include "CvCivilizationInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdStr / jsonResolveId
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- getFlagTexture / getArtInfo / getButton
#include "Defines/CvGlobals.h"    // GC (getNumCivicOptionInfos / getCivicInfo)


//======================================================================================================
//					CvCivilizationInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//  FUNCTION:   CvCivilizationInfo()  -- default constructor (all members at their legacy load defaults)
//------------------------------------------------------------------------------------------------------
CvCivilizationInfo::CvCivilizationInfo()
	: m_iDefaultPlayerColor(-1)     // NO_PLAYERCOLOR (enum-as-int default)
	, m_iArtStyleType(-1)
	, m_iUnitArtStyleType(-1)
	, m_iNumCityNames(0)
	, m_iNumLeaders(0)
	, m_iSelectionSoundScriptId(0)
	, m_iActionSoundScriptId(0)
	, m_iDerivativeCiv(NO_CIVILIZATION)
	, m_iSpawnRateModifier(0)
	, m_iSpawnRateNPCPeaceModifier(0)
	, m_bStronglyRestricted(false)
	, m_bAIPlayable(false)
	, m_bPlayable(false)
	, m_piCivilizationInitialCivics(NULL)
	, m_paszCityNames(NULL)
{
}


//------------------------------------------------------------------------------------------------------
//  FUNCTION:   ~CvCivilizationInfo()  -- default destructor
//------------------------------------------------------------------------------------------------------
CvCivilizationInfo::~CvCivilizationInfo()
{
	SAFE_DELETE_ARRAY(m_piCivilizationInitialCivics);
	SAFE_DELETE_ARRAY(m_paszCityNames);
}


void CvCivilizationInfo::reset()
{
	CvInfoBase::reset();
	m_aszAdjective.clear();
	m_aszShortDescription.clear();
}


int CvCivilizationInfo::getDefaultPlayerColor() const
{
	return m_iDefaultPlayerColor;
}


int CvCivilizationInfo::getArtStyleType() const
{
	return m_iArtStyleType;
}


int CvCivilizationInfo::getUnitArtStyleType() const
{
	return m_iUnitArtStyleType;
}


int CvCivilizationInfo::getNumCityNames() const
{
	return m_iNumCityNames;
}


int CvCivilizationInfo::getNumLeaders() const// the number of leaders the Civ has, this is needed so that random leaders can be generated easily
{
	return m_iNumLeaders;
}


int CvCivilizationInfo::getSelectionSoundScriptId() const
{
	return m_iSelectionSoundScriptId;
}


int CvCivilizationInfo::getActionSoundScriptId() const
{
	return m_iActionSoundScriptId;
}


bool CvCivilizationInfo::isAIPlayable() const
{
	return m_bAIPlayable;
}


bool CvCivilizationInfo::isPlayable() const
{
	return m_bPlayable;
}


const wchar_t* CvCivilizationInfo::getShortDescription(uint uiForm)
{
	PROFILE_EXTRA_FUNC();
	while(m_aszShortDescription.size() <= uiForm)
	{
		m_aszShortDescription.push_back(gDLL->getObjectText(m_szShortDescriptionKey, m_aszShortDescription.size()));
	}

	return m_aszShortDescription[uiForm];
}


const wchar_t* CvCivilizationInfo::getShortDescriptionKey() const
{
	return m_szShortDescriptionKey;
}


const wchar_t* CvCivilizationInfo::getAdjective(uint uiForm)
{
	PROFILE_EXTRA_FUNC();
	while(m_aszAdjective.size() <= uiForm)
	{
		m_aszAdjective.push_back(gDLL->getObjectText(m_szAdjectiveKey, m_aszAdjective.size()));
	}

	return m_aszAdjective[uiForm];
}


const wchar_t* CvCivilizationInfo::getAdjectiveKey() const
{
	return m_szAdjectiveKey;
}


const char* CvCivilizationInfo::getFlagTexture() const
{
	return ARTFILEMGR.getCivilizationArtInfo( getArtDefineTag() )->getPath();
}


const char* CvCivilizationInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}



int CvCivilizationInfo::getCivilizationInitialCivics(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumCivicOptionInfos(), i);
	return (m_piCivilizationInitialCivics ? m_piCivilizationInitialCivics[i] : -1);
}


void CvCivilizationInfo::setCivilizationInitialCivics(int iCivicOption, int iCivic)
{
	FASSERT_BOUNDS(0, GC.getNumCivicOptionInfos(), iCivicOption);
	FASSERT_BOUNDS(0, GC.getNumCivicInfos(), iCivic);

	if ( NULL == m_piCivilizationInitialCivics )
	{
		const int iNum = GC.getNumCivicOptionInfos();
		m_piCivilizationInitialCivics = new int[iNum];
		for (int i = 0; i < iNum; i++)
		{
			m_piCivilizationInitialCivics[i] = -1;
		}
	}

	m_piCivilizationInitialCivics[iCivicOption] = iCivic;
}


bool CvCivilizationInfo::isLeaders(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumLeaderHeadInfos(), i);
	return algo::any_of_equal(m_aeLeaders, static_cast<LeaderHeadTypes>(i));
}


int CvCivilizationInfo::getNumCivilizationBuildings() const
{
	return (int)m_aiCivilizationBuildings.size();
}

int CvCivilizationInfo::getCivilizationBuilding(int i) const
{
	return m_aiCivilizationBuildings[i];
}

bool CvCivilizationInfo::isCivilizationBuilding(int i) const
{
	return algo::any_of_equal(m_aiCivilizationBuildings, i);
}


bool CvCivilizationInfo::isCivilizationFreeTechs(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), i);
	return algo::any_of_equal(m_aeCivilizationFreeTechs, static_cast<TechTypes>(i));
}


bool CvCivilizationInfo::isCivilizationDisableTechs(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTechInfos(), i);
	return algo::any_of_equal(m_aeCivilizationDisableTechs, static_cast<TechTypes>(i));
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


std::string CvCivilizationInfo::getCityNames(int i) const
{
	FASSERT_BOUNDS(0, getNumCityNames(), i);
	return m_paszCityNames[i];
}


//TB Tags

int CvCivilizationInfo::getSpawnRateModifier() const
{
	return m_iSpawnRateModifier;
}


int CvCivilizationInfo::getSpawnRateNPCPeaceModifier() const
{
	return m_iSpawnRateNPCPeaceModifier;
}


bool CvCivilizationInfo::isStronglyRestricted() const
{
	return m_bStronglyRestricted;
}


// ------------------------------------------------------------------------------------------------------
// JSON mapping helpers (file-local -- the shared CvJsonParse.h has no array walker; these keep it there).
// ------------------------------------------------------------------------------------------------------

// o[key] as a JSON array child, or NULL.
static const picojson::array* jsonChildArr(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::array>()) ? &it->second.get<picojson::array>() : NULL;
}

// Append every RESOLVED (>=0) FK id from a string array parent[key] into out (unresolved ids skipped, as the
// archived wrapper did). Templated so the int / TechTypes / LeaderHeadTypes vectors share one walk (C++03-safe).
template <class T>
static void jsonAppendResolvedIds(const picojson::object& parent, const char* key, std::vector<T>& out)
{
	const picojson::array* a = jsonChildArr(parent, key);
	if (a == NULL) return;
	for (size_t i = 0; i < a->size(); ++i)
	{
		if (!(*a)[i].is<std::string>()) continue;
		const int iId = jsonResolveId((*a)[i].get<std::string>());
		if (iId >= 0) out.push_back(static_cast<T>(iId));
	}
}


// #430: the civilization's typed consumer surface from Assets/Data/civilizations/*.json. Base reads type +
// identity.description/civilopedia text keys + the section DISPATCH (grants/enables/disables are recorded as
// unconsumed-census here -- this class reads them as typed members below, not composed section units). This class then maps the getCivilizationInfo(...) members:
// world.art.* colors/style + ART_DEF tag, sound.* audio-tag indices, spawnRate.* modifiers, the grants.* game-start
// grants (buildings/techs, and civics keyed into the CivicOption-slot InitialCivics array), disables.techs, and the
// identity.* text keys / leaders / cityNames / derivativeCiv. IDEMPOTENT (CvInfo.h): mapFrom re-runs once the FK
// registry is complete, so every owned array is rebuilt and every vector cleared-then-refilled each call.
void CvCivilizationInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / description / civilopedia) + the base section dispatch
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// reset owned/accumulating members so the re-run cannot double-append or leak
	SAFE_DELETE_ARRAY(m_piCivilizationInitialCivics);
	SAFE_DELETE_ARRAY(m_paszCityNames);
	m_iNumCityNames = 0;
	m_iNumLeaders = 0;
	m_aeLeaders.clear();
	m_aeCivilizationFreeTechs.clear();
	m_aeCivilizationDisableTechs.clear();
	m_aiCivilizationBuildings.clear();
	m_aszShortDescription.clear();
	m_aszAdjective.clear();

	std::string s;

	// --- world.art.*: playerColor/style/unitStyle are FK strings; icon is the ART_DEF tag kept as a string ---
	if (const picojson::object* world = jsonChildObj(o, "world"))
		if (const picojson::object* art = jsonChildObj(*world, "art"))
		{
			if (jsonIdStr(*art, "playerColor", s)) m_iDefaultPlayerColor = jsonResolveId(s);
			if (jsonIdStr(*art, "style", s))       m_iArtStyleType       = jsonResolveId(s);
			if (jsonIdStr(*art, "unitStyle", s))   m_iUnitArtStyleType   = jsonResolveId(s);
			if (jsonIdStr(*art, "icon", s))        m_szArtDefineTag      = s.c_str();
		}

	// --- sound.selection / sound.action -> RUNTIME audio-tag indices (NOT info-type ids), reproducing the archived
	//     read()'s gDLL->getAudioTagIndex(tag, AUDIOTAG_3DSCRIPT); absent/empty -> -1 (AUDIOTAG_NONE) ---
	m_iSelectionSoundScriptId = -1;
	m_iActionSoundScriptId = -1;
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		if (jsonIdStr(*snd, "selection", s) && !s.empty())
			m_iSelectionSoundScriptId = gDLL->getAudioTagIndex(s.c_str(), AUDIOTAG_3DSCRIPT);
		if (jsonIdStr(*snd, "action", s) && !s.empty())
			m_iActionSoundScriptId = gDLL->getAudioTagIndex(s.c_str(), AUDIOTAG_3DSCRIPT);
	}

	// --- spawnRate.empire.{general,npcPeace}.percent -> the two spawn-rate modifiers (RAW percents; curator wrote int()) ---
	if (const picojson::object* sr = jsonChildObj(o, "spawnRate"))
		if (const picojson::object* emp = jsonChildObj(*sr, "empire"))
		{
			if (const picojson::object* g = jsonChildObj(*emp, "general"))  m_iSpawnRateModifier         = jsonIdInt(*g, "percent");
			if (const picojson::object* n = jsonChildObj(*emp, "npcPeace")) m_iSpawnRateNPCPeaceModifier = jsonIdInt(*n, "percent");
		}

	// --- grants.*: buildings + techs are FK lists (curator already drops BUILDING_PALACE and prepends TECH_GAME_START);
	//     civics fill the CivicOption-slot InitialCivics array (each civic dropped in its own option slot) ---
	if (const picojson::object* grants = jsonChildObj(o, "grants"))
	{
		jsonAppendResolvedIds(*grants, "buildings", m_aiCivilizationBuildings);
		jsonAppendResolvedIds(*grants, "techs", m_aeCivilizationFreeTechs);

		if (const picojson::array* civics = jsonChildArr(*grants, "civics"))
			if (!civics->empty())
			{
				const int iNumOptions = GC.getNumCivicOptionInfos();
				m_piCivilizationInitialCivics = new int[iNumOptions];
				for (int i = 0; i < iNumOptions; i++)
				{
					m_piCivilizationInitialCivics[i] = -1;
				}
				for (size_t i = 0; i < civics->size(); ++i)
				{
					if (!(*civics)[i].is<std::string>()) continue;
					const int iCivic = jsonResolveId((*civics)[i].get<std::string>());
					if (iCivic < 0) continue;   // NO_CIVIC (unresolved) -- skip
					const int iOption = GC.getCivicInfo((CivicTypes)iCivic).getCivicOptionType();
					if (iOption >= 0 && iOption < iNumOptions)   // NO_CIVICOPTION / out-of-range -- skip
					{
						m_piCivilizationInitialCivics[iOption] = iCivic;
					}
				}
			}
	}

	// --- disables.techs -> the per-civ research ban (canEverResearch=false while active) ---
	if (const picojson::object* dis = jsonChildObj(o, "disables"))
		jsonAppendResolvedIds(*dis, "techs", m_aeCivilizationDisableTechs);

	// --- identity.*: the short-desc / adjective text keys, the leaders + cityNames pools, the derivative-civ FK ---
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		// selectability + the NPC build-lockdown (curator emits each only when true; absent -> false)
		m_bPlayable           = jsonIdBool(*io, "playable");
		m_bAIPlayable         = jsonIdBool(*io, "aiPlayable");
		m_bStronglyRestricted = jsonIdBool(*io, "stronglyRestricted");
		// NOTE: identity.isNpc is authored by the curator but has NO CvCivilizationInfo member (no engine consumer
		// on this class) -- deliberately unmapped.

		if (jsonIdStr(*io, "shortDescription", s)) m_szShortDescriptionKey = CvWString(s.c_str());
		if (jsonIdStr(*io, "adjective", s))        m_szAdjectiveKey        = CvWString(s.c_str());
		if (jsonIdStr(*io, "derivativeCiv", s))    m_iDerivativeCiv        = (CivilizationTypes)jsonResolveId(s);

		// leaders (FK list) -- and the (formerly latent-dead, always-0) leader count off its size
		jsonAppendResolvedIds(*io, "leaders", m_aeLeaders);
		m_iNumLeaders = (int)m_aeLeaders.size();

		// cityNames -- a TEXT-KEY pool (stored as strings, NOT resolved to ids); count -> m_iNumCityNames
		if (const picojson::array* ca = jsonChildArr(*io, "cityNames"))
		{
			const int iNum = (int)ca->size();
			if (iNum > 0)
			{
				m_paszCityNames = new CvString[iNum];
				for (int i = 0; i < iNum; i++)
				{
					if ((*ca)[i].is<std::string>())
					{
						m_paszCityNames[i] = (*ca)[i].get<std::string>().c_str();
					}
				}
				m_iNumCityNames = iNum;
			}
		}
	}
}
