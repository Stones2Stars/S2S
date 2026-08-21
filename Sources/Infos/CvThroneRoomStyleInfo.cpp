//------------------------------------------------------------------------------------------------
//  FILE:    CvThroneRoomStyleInfo.cpp
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
#include "CvThroneRoomStyleInfo.h"


//======================================================================================================
//					CvThroneRoomStyleInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvThroneRoomStyleInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomStyleInfo::CvThroneRoomStyleInfo()
{
	CvInfoUtil(this).initDataMembers();
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvThroneRoomStyleInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvThroneRoomStyleInfo::~CvThroneRoomStyleInfo()
{
}


// Declarative fields (#196); m_aNodeNames and m_aTextureNames stay hand-written in read():
// they are std::vector<CvString> lists read by a bespoke sibling-walking pattern, and the
// legacy copyNonDefaults deliberately does not merge them.
// No legacy getCheckSum, so declaration order follows the legacy read() order.
void CvThroneRoomStyleInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szArtStyleType, L"ArtStyleType")
		.add(m_szEraType, L"EraType")
		.add(m_szFileName, L"FileName")
	;
}


bool CvThroneRoomStyleInfo::read(CvXMLLoadUtility* pXML)
{
	PROFILE_EXTRA_FUNC();
	CvString szTextVal;
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	//node names
	if(pXML->TryMoveToXmlFirstChild())
	{
		while(pXML->TryMoveToXmlNextSibling(L"NodeName"))
		{
			pXML->GetXmlVal(szTextVal);
			m_aNodeNames.push_back(szTextVal);
		}
		pXML->MoveToXmlParent();
	}

	//texture names
	if(pXML->TryMoveToXmlFirstChild())
	{
		while(pXML->TryMoveToXmlNextSibling(L"TextureName"))
		{
			pXML->GetXmlVal(szTextVal);
			m_aTextureNames.push_back(szTextVal);
		}
		pXML->MoveToXmlParent();
	}

	return true;
}


void CvThroneRoomStyleInfo::copyNonDefaults(const CvThroneRoomStyleInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	/*
	m_aNodeNames and m_aTextureNames don't seem to be used?
	since i hardly doubt anyone ever touches those XML's anyway, i just leave them out
	FOR NOW! */
}

