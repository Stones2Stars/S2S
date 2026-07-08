#pragma once

#ifndef CV_BUILDINFO_H
#define CV_BUILDINFO_H

//
//	CvBuildInfo -- the EXE SHIM LEAF for worker builds (cascade-engine-430.md §3: mapscripts are launched by the
//	EXE, which reads the build's entity-event + mission type during map generation / worker actions). The old class
//	name returns as a THIN LEAF on the JSON chain (CvInfoBase -> CvJsonInfo -> CvJsonBuildInfo -> CvBuildInfo)
//	carrying ONLY the EXE-called DllExport methods -- the EXE import binds on decorated class-name + signature,
//	never layout. getEntityEvent reads the poco (world.art.entityEvent); getMissionType reads the runtime-assigned
//	value (setMissionType, on the poco, called at load by CvXMLLoadUtilitySet).
//

#include "CvJsonBuildInfo.h"

class CvBuildInfo : public CvJsonBuildInfo
{
public:
	DllExport int getEntityEvent() const;
	DllExport int getMissionType() const;
};

#endif // CV_BUILDINFO_H
