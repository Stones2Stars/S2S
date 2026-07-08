//
//	CvBonusInfo -- the EXE shim leaf (cascade-engine-430.md #3). The body mirrors the archived implementation
//	(SourceArchive/Infos/CvBonusInfo.cpp: ARTFILEMGR.getBonusArtInfo(getArtDefineTag())), re-based onto the
//	CvJsonBonusInfo art-define tag (world.art.icon).
//

#include "CvGameCoreDLL.h"
#include "CvBonusInfo.h"
#include "UI/CvArtFileMgr.h"

const CvArtInfoBonus* CvBonusInfo::getArtInfo() const
{
	return ARTFILEMGR.getBonusArtInfo(getArtDefineTag());
}
