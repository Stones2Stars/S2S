//------------------------------------------------------------------------------------------------
//  FILE:    CvLandscapeInfo.cpp
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
#include "CvLandscapeInfo.h"


//------------------------------------------------------------------------------------------------------
//
//	CvLandscapeInfo
//
//
CvLandscapeInfo::CvLandscapeInfo() :
// Derived fields, computed from the cell/plot counts after read/copy - not XML-backed.
m_iHorizontalVertCnt(0),
m_iVerticalVertCnt(0)
{
	CvInfoUtil(this).initDataMembers();
}


// Declared in legacy read() order (this class has no getCheckSum).
void CvLandscapeInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iWaterHeight, L"iWaterHeight")
		.add(m_bRandomMap, L"bRandomMap")
		.add(m_szHeightMap, L"HeightMap")
		.add(m_szTerrainMap, L"TerrainMap")
		.add(m_szNormalMap, L"NormalMap")
		.add(m_szBlendMap, L"BlendMap")
		.add(m_szSkyArt, L"SkyArt")
		.add(m_iFogR, L"iFogR")
		.add(m_iFogG, L"iFogG")
		.add(m_iFogB, L"iFogB")
		.add(m_fTextureScaleX, L"fTextureScaleX")
		.add(m_fTextureScaleY, L"fTextureScaleY")
		.add(m_iHorizontalGameCell, L"iGameCellSizeX")
		.add(m_iVerticalGameCell, L"iGameCellSizeY")
		.add(m_iPlotsPerCellX, L"iPlotsPerCellX")
		.add(m_iPlotsPerCellY, L"iPlotsPerCellY")
		.add(m_fZScale, L"fZScale")
		.add(m_bUseTerrainShader, L"bTerrainShader")
		.add(m_bUseLightmap, L"bUseLightmap")
		.add(m_fPeakScale, L"fPeakScale")
		.add(m_fHillScale, L"fHillScale")
	;
}


int CvLandscapeInfo::getFogR() const
{
	return m_iFogR;
}


int CvLandscapeInfo::getFogG() const
{
	return m_iFogG;
}


int CvLandscapeInfo::getFogB() const
{
	return m_iFogB;
}


int CvLandscapeInfo::getHorizontalGameCell() const
{
	return m_iHorizontalGameCell;
}


int CvLandscapeInfo::getVerticalGameCell() const
{
	return m_iVerticalGameCell;
}


int CvLandscapeInfo::getPlotsPerCellX() const
{
	return m_iPlotsPerCellX;
}


int CvLandscapeInfo::getPlotsPerCellY() const
{
	return m_iPlotsPerCellY;
}


int CvLandscapeInfo::getHorizontalVertCnt() const
{
	return m_iHorizontalVertCnt;
}


int CvLandscapeInfo::getVerticalVertCnt() const
{
	return m_iVerticalVertCnt;
}


int CvLandscapeInfo::getWaterHeight() const
{
	return m_iWaterHeight;
}


float CvLandscapeInfo::getTextureScaleX() const
{
	return m_fTextureScaleX;
}


float CvLandscapeInfo::getTextureScaleY() const
{
	return m_fTextureScaleY;
}


float CvLandscapeInfo::getZScale() const
{
	return m_fZScale;
}


bool CvLandscapeInfo::isUseTerrainShader() const
{
	return m_bUseTerrainShader;
}


bool CvLandscapeInfo::isUseLightmap() const
{
	return m_bUseLightmap;
}

float CvLandscapeInfo::getPeakScale() const
{
	return 	m_fPeakScale;
}


float CvLandscapeInfo::getHillScale() const
{
	return 	m_fHillScale;
}


bool CvLandscapeInfo::isRandomMap() const
{
	return m_bRandomMap;
}


//
// read from xml
//
bool CvLandscapeInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	m_iHorizontalVertCnt = m_iPlotsPerCellX * m_iHorizontalGameCell - (m_iPlotsPerCellX - 1);
	m_iVerticalVertCnt   = m_iPlotsPerCellY * m_iVerticalGameCell - (m_iPlotsPerCellY - 1);

	return true;
}


void CvLandscapeInfo::copyNonDefaults(const CvLandscapeInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	m_iHorizontalVertCnt = m_iPlotsPerCellX * m_iHorizontalGameCell - (m_iPlotsPerCellX - 1);
	m_iVerticalVertCnt   = m_iPlotsPerCellY * m_iVerticalGameCell - (m_iPlotsPerCellY - 1);
}

