//
//	CvReligionInfo -- the religion poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity set + the §9 shrine value plane ONCE
//	([DEC-materialize-at-mapfrom]); the conditioned per-commerce bonuses live on the compiled entries (base
//	surface), never as mirrored arrays. Idempotent by contract (unconditional assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvReligionInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdStr / jsonReadFlavours + infoFamily* (via CvInfoKinds)

CvReligionInfo::CvReligionInfo()
	: m_iSpreadFactor(0)
	, m_iTGAIndex(-1)   // -1 = the TGA-filler sentinel (RemoveTGAFiller erases fillers whose index stayed -1)
	, m_iFreeUnit(-1)
	, m_iNumFreeUnits(0)
	, m_eTechPrereq(NO_TECH)
	, m_iMissionType(-1)
	, m_iChar(-1)
	, m_iHolyCityChar(-1)
{
}

int CvReligionInfo::getFlavorValue(int iFlavor) const
{
	return mapValueOrDefault(m_flavours, iFlavor);
}

const char* CvReligionInfo::getButtonDisabled() const
{
	// Mirror the legacy derivation: the base button (ui.art.icon) with its ".dds" extension replaced by the
	// "_D.dds" disabled variant. Empty base button -> empty disabled path. Game-thread only (static buffer).
	static char szDisabled[512];
	szDisabled[0] = '\0';
	const char* szButton = getButton();
	const size_t iLen = szButton ? strlen(szButton) : 0;
	if (iLen > 4 && iLen + 3 <= sizeof(szDisabled))   // result is (iLen-4)+"_D.dds"+NUL = iLen+3 bytes
	{
		strncpy(szDisabled, szButton, iLen - 4);
		szDisabled[iLen - 4] = '\0';
		strcat(szDisabled, "_D.dds");
	}
	return szDisabled;
}

void CvReligionInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers, fills edges/grants)

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	// (the reverse-pass-fed tech FK + the runtime glyph/registry members are NOT reset here)
	m_flavours.clear();
	m_iSpreadFactor = 0;
	m_iTGAIndex = -1;
	m_szAdjectiveKey.clear();
	m_szSound.clear();
	m_szTechButton.clear();
	m_szGenericTechButton.clear();
	m_szMovieFile.clear();
	m_szMovieSound.clear();

	// grants-materialized reads (bucket-string reads are load-time only; pulses store ×100, /100 = human count)
	const CvGrants* pGrants = m_triggers.consideredGrant();
	m_iFreeUnit = (pGrants != NULL) ? pGrants->firstListId("freeUnit") : -1;
	m_iNumFreeUnits = (pGrants != NULL) ? pGrants->pulse("numFreeUnits") / 100 : 0;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iSpreadFactor = jsonIdInt(*pIdentity, "spreadFactor");
		std::string szAdjective;
		if (jsonIdStr(*pIdentity, "adjective", szAdjective))
		{
			m_szAdjectiveKey = CvWString(szAdjective.c_str());
		}
	}

	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			jsonIdStr(*pArt, "techButton", m_szTechButton);
			jsonIdStr(*pArt, "genericTechButton", m_szGenericTechButton);
			m_iTGAIndex = jsonIdInt(*pArt, "tgaIndex");
			if (const picojson::object* pMovie = jsonChildObj(*pArt, "movie"))
			{
				jsonIdStr(*pMovie, "file", m_szMovieFile);
				jsonIdStr(*pMovie, "sound", m_szMovieSound);
			}
		}
	}

	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		jsonIdStr(*pSound, "sound", m_szSound);
	}

	// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map)
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		jsonReadFlavours(*pAi, m_flavours);
	}
}
