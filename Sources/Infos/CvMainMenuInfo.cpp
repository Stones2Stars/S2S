//------------------------------------------------------------------------------------------------
//  FILE:    CvMainMenuInfo.cpp
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
#include "CvMainMenuInfo.h"


CvMainMenuInfo::CvMainMenuInfo()
{
	CvInfoUtil(this).initDataMembers();
}


// Declared in legacy read() order (this class has no getCheckSum).
void CvMainMenuInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szScene, L"Scene")
		.add(m_szSceneNoShader, L"SceneNoShader")
		.add(m_szSoundtrack, L"Soundtrack")
		.add(m_szLoading, L"Loading")
		.add(m_szLoadingSlideshow, L"LoadingSlideshow")
	;
}


CvMainMenuInfo::~CvMainMenuInfo()
{
}


std::string CvMainMenuInfo::getScene() const
{
	return m_szScene;
}


std::string CvMainMenuInfo::getSceneNoShader() const
{
	return m_szSceneNoShader;
}


std::string CvMainMenuInfo::getSoundtrack() const
{
	return m_szSoundtrack;
}


std::string CvMainMenuInfo::getLoading() const
{
	return m_szLoading;
}


std::string CvMainMenuInfo::getLoadingSlideshow() const
{
	return m_szLoadingSlideshow;
}


bool CvMainMenuInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvMainMenuInfo::copyNonDefaults(const CvMainMenuInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

