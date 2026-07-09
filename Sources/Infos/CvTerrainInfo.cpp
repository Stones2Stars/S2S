//
//	CvTerrainInfo -- the EXE shim leaf (cascade-engine-430.md §3). The DllExport getArtDefineTag exposes the poco's
//	art tag (world.art.icon) to the EXE map-gen art lookup.
//

#include "CvGameCoreDLL.h"
#include "CvTerrainInfo.h"
#include "UI/CvArtFileMgr.h"           // ARTFILEMGR.getTerrainArtInfo
#include "Infos/CvArtInfoTerrain.h"    // CvArtInfoTerrain complete type -- getButton() needs the full definition

const char* CvTerrainInfo::getArtDefineTag() const
{
	return CvJsonTerrainInfo::getArtDefineTag();
}

const CvArtInfoTerrain* CvTerrainInfo::getArtInfo() const
{
	return ARTFILEMGR.getTerrainArtInfo(getArtDefineTag());
}

const char* CvTerrainInfo::getButton() const   // art-define button (mirrors archived CvTerrainInfo::getButton)
{
	const CvArtInfoTerrain* p = getArtInfo();
	return p != NULL ? p->getButton() : "";
}
