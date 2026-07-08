//
//	CvBuildInfo -- the EXE shim leaf (cascade-engine-430.md §3). The DllExport getters delegate to the
//	CvJsonBuildInfo poco (getEntityEvent = world.art.entityEvent; getMissionType = the runtime setMissionType value).
//

#include "CvGameCoreDLL.h"
#include "CvBuildInfo.h"

int CvBuildInfo::getEntityEvent() const
{
	return CvJsonBuildInfo::getEntityEvent();
}

int CvBuildInfo::getMissionType() const
{
	return CvJsonBuildInfo::getMissionType();
}
