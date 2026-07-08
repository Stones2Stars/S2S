//
//	CvTerrainInfo -- the EXE shim leaf (cascade-engine-430.md §3). The DllExport getArtDefineTag exposes the poco's
//	art tag (world.art.icon) to the EXE map-gen art lookup.
//

#include "CvGameCoreDLL.h"
#include "CvTerrainInfo.h"

const char* CvTerrainInfo::getArtDefineTag() const
{
	return CvJsonTerrainInfo::getArtDefineTag();
}
