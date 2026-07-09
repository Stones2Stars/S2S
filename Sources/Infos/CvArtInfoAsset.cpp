//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoAsset.cpp
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
#include "CvArtInfoAsset.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoAsset
/////////////////////////////////////////////////////////////////////////////////////////////

const char* CvArtInfoAsset::getNIF() const
{
	return m_szNIF;
}


const char* CvArtInfoAsset::getKFM() const
{
	return m_szKFM;
}


void CvArtInfoAsset::setNIF(const char* szDesc)
{
	m_szNIF = szDesc;
}


void CvArtInfoAsset::setKFM(const char* szDesc)
{
	m_szKFM = szDesc;
}


void CvArtInfoAsset::getDataMembers(CvInfoUtil& util)
{
	CvAssetInfoBase::getDataMembers(util);
	util
		.add(m_szNIF, L"NIF")
		.add(m_szKFM, L"KFM")
	;
}


// The ONE declarative delegation point for the whole art-info family (see the header comment).
// getDataMembers dispatches virtually, so this reads every declared field of the concrete leaf;
// derived read() overrides must forward here and must NOT add a second CvInfoUtil delegation.
bool CvArtInfoAsset::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}

void CvArtInfoAsset::copyNonDefaults(const CvArtInfoAsset* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

