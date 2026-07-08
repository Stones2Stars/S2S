#pragma once

#ifndef CV_IMPROVEMENTINFO_H
#define CV_IMPROVEMENTINFO_H

//
//	CvImprovementInfo -- the EXE SHIM LEAF for tile improvements (cascade-engine-430.md §3: mapscripts are launched
//	by the EXE, which reads improvement art + the goody/river map-gen flags during map generation). The old class
//	name returns as a THIN LEAF on the JSON chain (CvInfoBase -> CvJsonInfo -> CvJsonImprovementInfo ->
//	CvImprovementInfo) carrying ONLY the EXE-called DllExport methods -- the EXE import binds on decorated
//	class-name + signature, never layout. All data lives on the CvJsonImprovementInfo poco.
//

#include "CvJsonImprovementInfo.h"

class CvArtInfoImprovement;

class CvImprovementInfo : public CvJsonImprovementInfo
{
public:
	DllExport bool isGoody() const;
	DllExport bool isRequiresRiverSide() const;
	DllExport const CvArtInfoImprovement* getArtInfo() const;
};

#endif // CV_IMPROVEMENTINFO_H
