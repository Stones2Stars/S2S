//
//	CvImprovementInfo -- the EXE shim leaf (cascade-engine-430.md §3). Bodies mirror the archived implementation
//	(SourceArchive/Infos/CvImprovementInfo.cpp), re-based onto the CvJsonImprovementInfo poco members.
//

#include "CvGameCoreDLL.h"
#include "CvImprovementInfo.h"
#include "UI/CvArtFileMgr.h"

bool CvImprovementInfo::isGoody() const
{
	return CvJsonImprovementInfo::isGoody();
}

bool CvImprovementInfo::isRequiresRiverSide() const
{
	return CvJsonImprovementInfo::isRequiresRiverSide();
}

const CvArtInfoImprovement* CvImprovementInfo::getArtInfo() const
{
	return ARTFILEMGR.getImprovementArtInfo(getArtDefineTag());
}

const char* CvImprovementInfo::getButton() const   // art-define button (mirrors archived CvImprovementInfo::getButton)
{
	const CvArtInfoImprovement* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
