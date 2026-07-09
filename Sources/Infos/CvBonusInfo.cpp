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

// The button lives in the art define (CIV4ArtDefines_Bonus.xml), not the bonus JSON -- reproduce legacy so the UI
// icon resolves (CvInfoBase::getButton would return the empty m_szButton). Mirrors archived CvBonusInfo::getButton.
const char* CvBonusInfo::getButton() const
{
	const CvArtInfoBonus* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
