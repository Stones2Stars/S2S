//------------------------------------------------------------------------------------------------
//  FILE:    CvCivilizationInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvCivilizationInfo.h"


//======================================================================================================
//					CvCivilizationInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvCivilizationInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvCivilizationInfo::CvCivilizationInfo():
m_iNumCityNames(0),
m_iNumLeaders(0),
m_iSelectionSoundScriptId(0),
m_iActionSoundScriptId(0),
m_piCivilizationInitialCivics(NULL),
m_paszCityNames(NULL)
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvCivilizationInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvCivilizationInfo::~CvCivilizationInfo()
{
	SAFE_DELETE_ARRAY(m_piCivilizationInitialCivics);
	SAFE_DELETE_ARRAY(m_paszCityNames);
	// m_iDerivativeCiv's pending delayed-resolution entry is removed by its wrapper's uninitVar.
	CvInfoUtil(this).uninitDataMembers();
}


// Fields declared here are read/copied by CvInfoUtil (#196). Still hand-written:
// - m_szShortDescriptionKey / m_szAdjectiveKey: CvWString members (wrappers only support CvString);
// - m_szArtDefineTag: its modular merge must run BEFORE CvInfoBase::copyNonDefaults (see there);
// - m_iSelectionSoundScriptId / m_iActionSoundScriptId: resolved via the runtime audio-tag table;
// - Cities string list (m_paszCityNames + m_iNumCityNames);
// - InitialCivics: bespoke read keyed into a fixed CivicOption-length array;
// - getCheckSum: explicit, because the legacy checksum omits several read fields (see there).
void CvCivilizationInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.addEnumAsInt(m_iDefaultPlayerColor, L"DefaultPlayerColor")
		.addEnumAsInt(m_iArtStyleType, L"ArtStyleType")
		.addEnumAsInt(m_iUnitArtStyleType, L"UnitArtStyleType")
		.add(m_bPlayable, L"bPlayable")
		.add(m_bAIPlayable, L"bAIPlayable")
		.add(m_aiCivilizationBuildings, L"FreeBuildings")
		.add(m_aeCivilizationFreeTechs, L"FreeTechs")
		.add(m_aeCivilizationDisableTechs, L"DisableTechs")
		.add(m_aeLeaders, L"Leaders")
		.add(m_iSpawnRateModifier, L"iSpawnRateModifier")
		.add(m_iSpawnRateNPCPeaceModifier, L"iSpawnRateNPCPeaceModifier")
		.add(m_bStronglyRestricted, L"bStronglyRestricted")
		.addEnum(m_iDerivativeCiv, L"DerivativeCiv")
	;
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
		CvXMLLoadUtility::InitList(&m_piCivilizationInitialCivics,GC.getNumCivicOptionInfos(),-1);
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


// Explicit, NOT delegated to CvInfoUtil (#196): the legacy checksum omits several read fields
// (m_iDefaultPlayerColor, m_iArtStyleType, m_iUnitArtStyleType), and the hand-written
// InitialCivics array sits mid-order; delegating would change the savegame asset checksum.
void CvCivilizationInfo::getCheckSum(uint32_t& iSum) const
{
	CheckSum(iSum, m_iDerivativeCiv);
	CheckSum(iSum, m_bAIPlayable);
	CheckSum(iSum, m_bPlayable);
	CheckSum(iSum, m_iSpawnRateModifier);
	CheckSum(iSum, m_iSpawnRateNPCPeaceModifier);
	CheckSum(iSum, m_bStronglyRestricted);
	CheckSumI(iSum, GC.getNumCivicOptionInfos(), m_piCivilizationInitialCivics);
	CheckSumC(iSum, m_aeLeaders);
	CheckSumC(iSum, m_aeCivilizationFreeTechs);
	CheckSumC(iSum, m_aeCivilizationDisableTechs);
	CheckSumC(iSum, m_aiCivilizationBuildings);
}


bool CvCivilizationInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	// CvWString members are not wrapper-expressible (only CvString is)
	pXML->GetOptionalChildXmlValByName(m_szShortDescriptionKey, L"ShortDescription");
	pXML->GetOptionalChildXmlValByName(m_szAdjectiveKey, L"Adjective");
	// Hand-written because its modular merge must run before the base copy (see copyNonDefaults)
	pXML->GetOptionalChildXmlValByName(m_szArtDefineTag, L"ArtDefineTag");

	// Audio script ids resolve through the runtime audio-tag table, not the info-type registry
	pXML->GetOptionalChildXmlValByName(szTextVal, L"CivilizationSelectionSound");
	m_iSelectionSoundScriptId = (szTextVal.GetLength() > 0) ? gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_3DSCRIPT ) : -1;
	pXML->GetOptionalChildXmlValByName(szTextVal, L"CivilizationActionSound");
	m_iActionSoundScriptId = (szTextVal.GetLength() > 0) ? gDLL->getAudioTagIndex( szTextVal.GetCString(), AUDIOTAG_3DSCRIPT ) : -1;

	if (pXML->TryMoveToXmlFirstChild(L"Cities"))
	{
		pXML->SetStringList(&m_paszCityNames, &m_iNumCityNames);
		pXML->MoveToXmlParent();
	}

	if (pXML->TryMoveToXmlFirstChild(L"InitialCivics"))
	{
		if (const int iNumSibs = pXML->GetXmlChildrenNumber())
		{
			CvXMLLoadUtility::InitList(&m_piCivilizationInitialCivics, GC.getNumCivicOptionInfos());
			if (pXML->GetChildXmlVal(szTextVal))
			{
				for (int j = 0; j < iNumSibs; j++)
				{
					const CivicTypes eCivic = (CivicTypes)pXML->GetInfoClass(szTextVal);//, true);
					if ( eCivic != NO_CIVIC )
					{
						const CivicOptionTypes eCivicOption = (CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOptionType();

						if ( eCivicOption != NO_CIVICOPTION )
						{
							FAssertMsg((eCivicOption < GC.getNumCivicOptionInfos()),"Bad default civic");
							m_piCivilizationInitialCivics[eCivicOption] = eCivic;
						}
					}

					if (!pXML->GetNextXmlVal(szTextVal))
					{
						break;
					}
				}

				pXML->MoveToXmlParent();
			}
		}
		else
		{
			SAFE_DELETE_ARRAY(m_piCivilizationInitialCivics);
		}

		pXML->MoveToXmlParent();
	}
	else
	{
		SAFE_DELETE_ARRAY(m_piCivilizationInitialCivics);
	}

	return true;
}


void CvCivilizationInfo::copyNonDefaults(const CvCivilizationInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const CvString cDefault = CvString::format("").GetCString();
	const CvWString wDefault = CvWString::format(L"").GetCString();

	// must be before we set the InfoBaseClass else it can't find the button to to corresponding arttag
	if ( getArtDefineTag() == cDefault ) // "ArtDefineTag"
	{
		m_szArtDefineTag = pClassInfo->getArtDefineTag();
	}

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if ( getShortDescriptionKey() == wDefault )
	{
		m_szShortDescriptionKey = pClassInfo->getShortDescriptionKey();
	}

	if ( getAdjectiveKey() == wDefault ) // "Adjective"
	{
		m_szAdjectiveKey = pClassInfo->getAdjectiveKey();
	}

	if ( getSelectionSoundScriptId() == AUDIOTAG_NONE ) // "CivilizationSelectionSound"
	{
		m_iSelectionSoundScriptId = (pClassInfo->getSelectionSoundScriptId());
	}
	if ( getActionSoundScriptId() == AUDIOTAG_NONE ) // "CivilizationActionSound"
	{
		m_iActionSoundScriptId = (pClassInfo->getActionSoundScriptId());
	}

	for ( int i = 0; i < GC.getNumCivicOptionInfos(); i++)
	{
		if ( getCivilizationInitialCivics(i) == -1 && pClassInfo->getCivilizationInitialCivics(i) != -1 )
		{
			if ( NULL == m_piCivilizationInitialCivics )
			{
				CvXMLLoadUtility::InitList(&m_piCivilizationInitialCivics,GC.getNumCivicOptionInfos(),-1);
			}
			m_piCivilizationInitialCivics[i] = pClassInfo->getCivilizationInitialCivics(i);
		}
	}

	// First we check if there are different Unique Names in the Modules(we want to keep all of them)
	// So we have to set the Arraysize properly, knowing the amount of Unique Names
	if ( pClassInfo->getNumCityNames() != 0 )
	{
		CvString* m_paszOldNames = new CvString[pClassInfo->getNumCityNames()];
		for ( int i = 0; i < pClassInfo->getNumCityNames(); i++)
		{
			m_paszOldNames[i] = pClassInfo->getCityNames(i);
		}

		CvXMLLoadUtilityModTools::StringArrayExtend(&m_paszCityNames, &m_iNumCityNames, &m_paszOldNames, pClassInfo->getNumCityNames());
		SAFE_DELETE_ARRAY(m_paszOldNames)
	}
}

