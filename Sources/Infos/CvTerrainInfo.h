#pragma once

#ifndef CV_TERRAININFO_H
#define CV_TERRAININFO_H

//
//	CvTerrainInfo -- the EXE SHIM LEAF for terrains (cascade-engine-430.md §3: mapscripts are launched by the EXE,
//	which reads terrain art during map generation). The old class name returns as a THIN LEAF on the JSON chain
//	(CvInfoBase -> CvJsonInfo -> CvJsonTerrainInfo -> CvTerrainInfo) carrying ONLY the EXE-called DllExport method --
//	the EXE import binds on decorated class-name + signature, never layout. The art tag lives on the poco.
//

#include "CvJsonTerrainInfo.h"

class CvTerrainInfo : public CvJsonTerrainInfo
{
public:
	DllExport const char* getArtDefineTag() const;
};

#endif // CV_TERRAININFO_H
