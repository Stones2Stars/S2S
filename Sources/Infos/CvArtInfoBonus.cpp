//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoBonus.cpp
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
#include "CvArtInfoBonus.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoBonus
/////////////////////////////////////////////////////////////////////////////////////////////

void CvArtInfoBonus::getDataMembers(CvInfoUtil& util)
{
	CvArtInfoScalableAsset::getDataMembers(util);
	// FontButtonIndex was a MANDATORY read (GetChildXmlValByName) in the legacy loader; the
	// declarative read is optional, so a missing tag now loads 0 silently instead of also
	// emitting an FErrorMsg. Same loaded value either way.
	util
		.add(m_szShaderNIF, L"SHADERNIF")
		.add(m_iFontButtonIndex, L"FontButtonIndex")
	;
}


void CvArtInfoBonus::copyNonDefaults(const CvArtInfoBonus* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}


CvArtInfoBonus::CvArtInfoBonus()
{
	CvInfoUtil(this).initDataMembers();
}


int CvArtInfoBonus::getFontButtonIndex() const
{
	return m_iFontButtonIndex;
}


const char* CvArtInfoBonus::getShaderNIF() const
{
	return m_szShaderNIF;
}

void CvArtInfoBonus::setShaderNIF(const char* szDesc)
{
	m_szShaderNIF = szDesc;
}

