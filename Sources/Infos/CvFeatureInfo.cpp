//
//	CvFeatureInfo -- the EXE shim leaf (cascade-engine-430.md §3). Body mirrors the archived implementation
//	(SourceArchive/Infos/CvFeatureInfo.cpp: ARTFILEMGR.getFeatureArtInfo(getArtDefineTag())), re-based onto the
//	CvJsonFeatureInfo art tag (world.art.icon).
//

#include "CvGameCoreDLL.h"
#include "CvFeatureInfo.h"
#include "UI/CvArtFileMgr.h"

const CvArtInfoFeature* CvFeatureInfo::getArtInfo() const
{
	return ARTFILEMGR.getFeatureArtInfo(getArtDefineTag());
}

const char* CvFeatureInfo::getButton() const   // art-define button (mirrors archived CvFeatureInfo::getButton)
{
	const CvArtInfoFeature* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
