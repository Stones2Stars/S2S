//------------------------------------------------------------------------------------------------
//  FILE:    CvYieldInfo.cpp
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
#include "CvYieldInfo.h"


//======================================================================================================
//					CvYieldInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvYieldInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvYieldInfo::CvYieldInfo() :
// m_iChar is a non-XML, runtime-assigned GameFont symbol index (see setChar); m_paszSymbolPath is
// the hand-written SymbolPaths array. Every other field is declared in getDataMembers() and
// defaulted by initDataMembers() below.
m_iChar(0),
m_paszSymbolPath(NULL)
{
	CvInfoUtil(this).initDataMembers();
}



//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvYieldInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvYieldInfo::~CvYieldInfo()
{
	SAFE_DELETE_ARRAY(m_paszSymbolPath);
}


int CvYieldInfo::getChar() const
{
	return m_iChar;
}


void CvYieldInfo::setChar(int i)
{
	m_iChar = i;
}


int CvYieldInfo::getHillsChange() const
{
	return m_iHillsChange;
}


int CvYieldInfo::getPeakChange() const
{
	return m_iPeakChange;
}


int CvYieldInfo::getRiverChange() const
{
	return m_iRiverChange;
}


int CvYieldInfo::getCityChange() const
{
	return m_iCityChange;
}


int CvYieldInfo::getPopulationChangeDivisor() const
{
	return m_iPopulationChangeDivisor;
}


int CvYieldInfo::getMinCity() const
{
	return m_iMinCity;
}


int CvYieldInfo::getTradeModifier() const
{
	return m_iTradeModifier;
}


int CvYieldInfo::getGoldenAgeYield() const
{
	return m_iGoldenAgeYield;
}


int CvYieldInfo::getGoldenAgeYieldThreshold() const
{
	return m_iGoldenAgeYieldThreshold;
}


int CvYieldInfo::getAIWeightPercent() const
{
	return m_iAIWeightPercent;
}


int CvYieldInfo::getColorType() const
{
	return m_iColorType;
}


// Arrays

const char* CvYieldInfo::getSymbolPath(int i) const
{
	FASSERT_BOUNDS(0, GC.getDefineINT("MAX_YIELD_STACK"), i);
	return m_paszSymbolPath ? m_paszSymbolPath[i] : reinterpret_cast<const char*>(-1);
}


void CvYieldInfo::getDataMembers(CvInfoUtil& util)
{
	// HYBRID migration:
	// - m_iChar is a non-XML runtime GameFont field (setChar) — not declared, and (matching legacy)
	//   not checksummed either.
	// - m_paszSymbolPath stays hand-written in read()/copyNonDefaults()/dtor: a CvString array
	//   sized by GC.getDefineINT("MAX_YIELD_STACK") with no matching wrapper.
	// - getCheckSum() is NOT delegated (see comment there): the legacy checksum omits m_iColorType,
	//   which IS read; declared order = legacy checksum order, ColorType last.
	util
		.add(m_iHillsChange, L"iHillsChange")
		.add(m_iPeakChange, L"iPeakChange")
		.add(m_iRiverChange, L"iRiverChange")
		.add(m_iCityChange, L"iCityChange")
		.add(m_iPopulationChangeDivisor, L"iPopulationChangeDivisor")
		.add(m_iMinCity, L"iMinCity")
		.add(m_iTradeModifier, L"iTradeModifier")
		.add(m_iGoldenAgeYield, L"iGoldenAgeYield")
		.add(m_iGoldenAgeYieldThreshold, L"iGoldenAgeYieldThreshold")
		.add(m_iAIWeightPercent, L"iAIWeightPercent")
		.addEnumAsInt(m_iColorType, L"ColorType")
	;
}


bool CvYieldInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	int iNumSibs, j;

	CvInfoUtil(this).readXml(pXML);

	if (pXML->TryMoveToXmlFirstChild(L"SymbolPaths"))
	{
		iNumSibs = pXML->GetXmlChildrenNumber();
		FAssertMsg((0 < GC.getDefineINT("MAX_YIELD_STACK")) ,"Allocating zero or less memory in SetGlobalYieldInfo");
		m_paszSymbolPath = new CvString[GC.getDefineINT("MAX_YIELD_STACK")];

		if (0 < iNumSibs)
		{
			if (pXML->GetChildXmlVal(szTextVal))
			{
				FAssertMsg((iNumSibs <= GC.getDefineINT("MAX_YIELD_STACK")) ,"There are more siblings than memory allocated for them in SetGlobalYieldInfo");
				for (j=0;j<iNumSibs;j++)
				{
					m_paszSymbolPath[j] = szTextVal;
					if (!pXML->GetNextXmlVal(szTextVal))
					{
						break;
					}
				}

				pXML->MoveToXmlParent();
			}
		}

		pXML->MoveToXmlParent();
	}
	else
	{
		CvString cDefault = CvString::format("").GetCString();
		m_paszSymbolPath = new CvString[GC.getDefineINT("MAX_YIELD_STACK")];
		for ( int i = 0; i < GC.getDefineINT("MAX_YIELD_STACK"); i++)
		{
			m_paszSymbolPath[i] = cDefault;
		}
	}

	return true;
}


void CvYieldInfo::copyNonDefaults(const CvYieldInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	const CvString cDefault = CvString::format("").GetCString();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	for ( int i = 0; i < GC.getDefineINT("MAX_YIELD_STACK"); i++)
	{
		if ( m_paszSymbolPath[i] == cDefault)
		{
			m_paszSymbolPath[i] = pClassInfo->getSymbolPath(i);
		}
	}
}


void CvYieldInfo::getCheckSum(uint32_t& iSum) const
{
	// NOT delegated to CvInfoUtil: the legacy checksum omits m_iColorType (which read() does load),
	// and delegating would fold it in and change the asset checksum. Reproduces the legacy set.
	CheckSum(iSum, m_iHillsChange);
	CheckSum(iSum, m_iPeakChange);
	CheckSum(iSum, m_iRiverChange);
	CheckSum(iSum, m_iCityChange);
	CheckSum(iSum, m_iPopulationChangeDivisor);
	CheckSum(iSum, m_iMinCity);
	CheckSum(iSum, m_iTradeModifier);
	CheckSum(iSum, m_iGoldenAgeYield);
	CheckSum(iSum, m_iGoldenAgeYieldThreshold);
	CheckSum(iSum, m_iAIWeightPercent);
}

