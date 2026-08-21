#include "CvGameCoreDLL.h"
#include "CyGameSpeedInfo.h"
#include "Infos/CvGameSpeedInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry, and it carries the ERA axis too. Both ids arrive from script, so
	//	both are checked here rather than trusted: the engine's own era guard is FASSERT_BOUNDS, which is
	//	compiled out of Release (docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD) -- i.e. absent from the build this actually runs in.
	const CvGameSpeedInfo* cygs_speed(int iGameSpeed)
	{
		if (iGameSpeed < 0 || iGameSpeed >= GC.getNumGameSpeedInfos())
		{
			return NULL;
		}
		return &GC.getGameSpeedInfo((GameSpeedTypes)iGameSpeed);
	}

	bool cygs_eraOk(int iEra)
	{
		return iEra >= 0 && iEra < GC.getNumEraInfos();
	}
}

int CyGameSpeedInfo::getTurnsInEra(int iGameSpeed, int iEra) const
{
	const CvGameSpeedInfo* pSpeed = cygs_speed(iGameSpeed);
	if (pSpeed == NULL || !cygs_eraOk(iEra)) return 0;
	return pSpeed->getTurnsInEra(iEra);
}

int CyGameSpeedInfo::getEraStartTurn(int iGameSpeed, int iEra) const
{
	const CvGameSpeedInfo* pSpeed = cygs_speed(iGameSpeed);
	if (pSpeed == NULL || !cygs_eraOk(iEra)) return 0;
	return pSpeed->getEraStartTurn(iEra);
}

int CyGameSpeedInfo::getTicksPerTurnInEra(int iGameSpeed, int iEra) const
{
	const CvGameSpeedInfo* pSpeed = cygs_speed(iGameSpeed);
	if (pSpeed == NULL || !cygs_eraOk(iEra)) return 0;
	return pSpeed->getTicksPerTurnInEra(iEra);
}

int CyGameSpeedInfo::getTotalTurns(int iGameSpeed) const
{
	const CvGameSpeedInfo* pSpeed = cygs_speed(iGameSpeed);
	if (pSpeed == NULL) return 0;
	return pSpeed->getTotalTurns();
}

void CyGameSpeedInfo::pythonPublish()
{
	python::class_<CyGameSpeedInfo>("CyGameSpeedInfo")
		.def("getTurnsInEra",         &CyGameSpeedInfo::getTurnsInEra)
		.def("getEraStartTurn",       &CyGameSpeedInfo::getEraStartTurn)
		.def("getTicksPerTurnInEra",  &CyGameSpeedInfo::getTicksPerTurnInEra)
		.def("getTotalTurns",         &CyGameSpeedInfo::getTotalTurns)
		;
}
