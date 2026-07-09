//------------------------------------------------------------------------------------------------
//  FILE:    CvWorldPickerInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvJsonBuildingInfo.h"
#include "CvJsonHeritageInfo.h"
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
#include "CvWorldPickerInfo.h"


//======================================================================================================
//					CvWorldPickerInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvWorldPickerInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvWorldPickerInfo::CvWorldPickerInfo()
{
	CvInfoUtil(this).initDataMembers();
}


// Declared in legacy read() order (this class has no getCheckSum). The four non-enum
// vectors (float sizes, string paths) are not wrapper-expressible (SetOptionalVector
// resolves elements via GetInfoClass) and stay hand-written in read()/copyNonDefaults().
void CvWorldPickerInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szMapName, L"MapName")
		.add(m_szModelFile, L"ModelFile")
	;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvWorldPickerInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvWorldPickerInfo::~CvWorldPickerInfo()
{
}


const char* CvWorldPickerInfo::getMapName()
{
	return m_szMapName;
}


const char* CvWorldPickerInfo::getModelFile()
{
	return m_szModelFile;
}


int CvWorldPickerInfo::getNumSizes()
{
	return m_aSizes.size();
}


float CvWorldPickerInfo::getSize(int index)
{
	return m_aSizes[index];
}


int CvWorldPickerInfo::getNumClimates()
{
	return m_aClimates.size();
}


const char* CvWorldPickerInfo::getClimatePath(int index)
{
	return m_aClimates[index];
}


int CvWorldPickerInfo::getNumWaterLevelDecals()
{
	return m_aWaterLevelDecals.size();
}


const char* CvWorldPickerInfo::getWaterLevelDecalPath(int index)
{
	return m_aWaterLevelDecals[index];
}


int CvWorldPickerInfo::getNumWaterLevelGloss()
{
	return m_aWaterLevelGloss.size();
}


const char* CvWorldPickerInfo::getWaterLevelGlossPath(int index)
{
	return m_aWaterLevelGloss[index];
}


bool CvWorldPickerInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	float fVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	//sizes
	if(pXML->TryMoveToXmlFirstChild(L"Sizes"))
	{
		if(pXML->TryMoveToXmlFirstChild(L"Size"))
		{
			do
			{
				pXML->GetXmlVal(&fVal);
				m_aSizes.push_back(fVal);
			} while(pXML->TryMoveToXmlNextSibling(L"Size"));

			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	//climates
	if(pXML->TryMoveToXmlFirstChild(L"Climates"))
	{
		if(pXML->TryMoveToXmlFirstChild(L"ClimatePath"))
		{
			do
			{
				pXML->GetXmlVal(szTextVal);
				m_aClimates.push_back(szTextVal);
			} while(pXML->TryMoveToXmlNextSibling(L"ClimatePath"));

			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	//water level decals
	if(pXML->TryMoveToXmlFirstChild(L"WaterLevelDecals"))
	{
		if(pXML->TryMoveToXmlFirstChild(L"WaterLevelDecalPath"))
		{
			do
			{
				pXML->GetXmlVal(szTextVal);
				m_aWaterLevelDecals.push_back(szTextVal);
			} while(pXML->TryMoveToXmlNextSibling(L"WaterLevelDecalPath"));

			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	//water level gloss
	if(pXML->TryMoveToXmlFirstChild(L"WaterLevelGloss"))
	{
		if(pXML->TryMoveToXmlFirstChild(L"WaterLevelGlossPath"))
		{
			do
			{
				pXML->GetXmlVal(szTextVal);
				m_aWaterLevelGloss.push_back(szTextVal);
			} while(pXML->TryMoveToXmlNextSibling(L"WaterLevelGlossPath"));

			pXML->MoveToXmlParent();
		}
		pXML->MoveToXmlParent();
	}

	return true;
}

void CvWorldPickerInfo::copyNonDefaults(CvWorldPickerInfo* pClassInfo)
{
	PROFILE_EXTRA_FUNC();
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if ( getNumSizes() == 0 )
	{
		for ( int i = 0; i < pClassInfo->getNumSizes(); i++ )
		{
			m_aSizes.push_back(pClassInfo->getSize(i));
		}
	}

	if ( getNumClimates() == 0 )
	{
		for ( int i = 0; i < pClassInfo->getNumClimates(); i++ )
		{
			m_aClimates.push_back(pClassInfo->getClimatePath(i));
		}
	}

	if ( getNumWaterLevelDecals() == 0 )
	{
		for ( int i = 0; i < pClassInfo->getNumWaterLevelDecals(); i++ )
		{
			m_aWaterLevelDecals.push_back(pClassInfo->getWaterLevelDecalPath(i));
		}
	}

	if ( getNumWaterLevelGloss() == 0 )
	{
		for ( int i = 0; i < pClassInfo->getNumWaterLevelGloss(); i++ )
		{
			// XXX pre-existing copy-paste bug: gloss paths are merged into the DECALS
			// vector, so m_aWaterLevelGloss never receives merged entries. Preserved
			// as-is by the #196 loader migration (pure-loader change, no semantics).
			m_aWaterLevelDecals.push_back(pClassInfo->getWaterLevelGlossPath(i));
		}
	}
}

