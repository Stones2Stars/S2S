#include "CvGameCoreDLL.h"
#include "CyEspionageMissionInfo.h"
#include "Infos/CvEspionageMissionInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. The id arrives from script, so it is checked here rather than
	//	trusted -- an out-of-range read reaches the info plane, which fails LOUD by design
	//	([DEC-info-plane-read-only]) and takes the process with it.
	const CvEspionageMissionInfo* cyem_mission(int iMission)
	{
		if (iMission < 0 || iMission >= GC.getNumEspionageMissionInfos())
		{
			return NULL;
		}
		return &GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
	}
}

int CyEspionageMissionInfo::getCost(int iMission) const
{
	const CvEspionageMissionInfo* pMission = cyem_mission(iMission);
	if (pMission == NULL) return -1;
	return pMission->getCost();
}

bool CyEspionageMissionInfo::isPassive(int iMission) const
{
	const CvEspionageMissionInfo* pMission = cyem_mission(iMission);
	if (pMission == NULL) return false;
	return pMission->isPassive();
}

bool CyEspionageMissionInfo::isInvestigateCity(int iMission) const
{
	const CvEspionageMissionInfo* pMission = cyem_mission(iMission);
	if (pMission == NULL) return false;
	return pMission->isInvestigateCity();
}

bool CyEspionageMissionInfo::isSeeDemographics(int iMission) const
{
	const CvEspionageMissionInfo* pMission = cyem_mission(iMission);
	if (pMission == NULL) return false;
	return pMission->isSeeDemographics();
}

bool CyEspionageMissionInfo::isSeeResearch(int iMission) const
{
	const CvEspionageMissionInfo* pMission = cyem_mission(iMission);
	if (pMission == NULL) return false;
	return pMission->isSeeResearch();
}

void CyEspionageMissionInfo::pythonPublish()
{
	python::class_<CyEspionageMissionInfo>("CyEspionageMissionInfo")
		.def("getCost",            &CyEspionageMissionInfo::getCost)
		.def("isPassive",          &CyEspionageMissionInfo::isPassive)
		.def("isInvestigateCity",  &CyEspionageMissionInfo::isInvestigateCity)
		.def("isSeeDemographics",  &CyEspionageMissionInfo::isSeeDemographics)
		.def("isSeeResearch",      &CyEspionageMissionInfo::isSeeResearch)
		;
}
