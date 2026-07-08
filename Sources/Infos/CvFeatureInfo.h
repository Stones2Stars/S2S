#pragma once

#ifndef CV_FEATUREINFO_H
#define CV_FEATUREINFO_H

//
//	CvFeatureInfo -- the EXE SHIM LEAF for terrain features (cascade-engine-430.md §3: mapscripts are launched by
//	the EXE, which reads feature art during map generation). The old class name returns as a THIN LEAF on the JSON
//	chain (CvInfoBase -> CvJsonInfo -> CvJsonFeatureInfo -> CvFeatureInfo) carrying ONLY the EXE-called DllExport
//	method -- the EXE import binds on decorated class-name + signature, never layout. Data lives on the poco.
//

#include "CvJsonFeatureInfo.h"

class CvArtInfoFeature;

class CvFeatureInfo : public CvJsonFeatureInfo
{
public:
	DllExport const CvArtInfoFeature* getArtInfo() const;
};

#endif // CV_FEATUREINFO_H
