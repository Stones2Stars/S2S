//------------------------------------------------------------------------------------------------
//  FILE:    CvArtInfoMovie.cpp
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
#include "CvArtInfoMovie.h"


/////////////////////////////////////////////////////////////////////////////////////////////
// CvArtInfoMovie
/////////////////////////////////////////////////////////////////////////////////////////////

void CvArtInfoMovie::copyNonDefaults(const CvArtInfoMovie* pClassInfo)
{
	// Empty, for Art files we stick to FULL XML defintions
}

