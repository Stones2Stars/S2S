#include "CvGameCoreDLL.h"
#include "CyCultureLevelInfo.h"
#include "Infos/CvCultureLevelInfo.h"
#include "Defines/CvGlobals.h"

int CyCultureLevelInfo::getSpeedThreshold(int iCultureLevel, int iGameSpeed) const
{
	//	Both ids arrive from script and BOTH index something, so both are checked: the level selects the info,
	//	the speed indexes the per-speed threshold inside it.
	if (iCultureLevel < 0 || iCultureLevel >= GC.getNumCultureLevelInfos()) return -1;
	if (iGameSpeed < 0 || iGameSpeed >= GC.getNumGameSpeedInfos()) return -1;
	return GC.getCultureLevelInfo((CultureLevelTypes)iCultureLevel).getSpeedThreshold(iGameSpeed);
}

void CyCultureLevelInfo::pythonPublish()
{
	python::class_<CyCultureLevelInfo>("CyCultureLevelInfo")
		.def("getSpeedThreshold", &CyCultureLevelInfo::getSpeedThreshold)
		;
}
