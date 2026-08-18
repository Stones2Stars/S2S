#include "CvGameCoreDLL.h"
#include "CyVictoryInfo.h"
#include "Infos/CvVictoryInfo.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. Both the id and the KEY arrive from script, so both are checked
	//	here rather than trusted -- an out-of-range id reaches the info plane, which fails LOUD by design
	//	(docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD), and an out-of-range key would index straight off the block's array.
	const CvVictoryInfo* cyv_victory(int iVictory)
	{
		if (iVictory < 0 || iVictory >= GC.getNumVictoryInfos())
		{
			return NULL;
		}
		return &GC.getVictoryInfo((VictoryTypes)iVictory);
	}
}

bool CyVictoryInfo::conditionFlag(int iVictory, int iFlag) const
{
	const CvVictoryInfo* pVictory = cyv_victory(iVictory);
	if (pVictory == NULL || iFlag < 0 || iFlag >= NUM_VICTORY_CONDITION_FLAGS) return false;
	return pVictory->conditionFlag((VictoryConditionFlag)iFlag);
}

int CyVictoryInfo::conditionValue(int iVictory, int iValue) const
{
	const CvVictoryInfo* pVictory = cyv_victory(iVictory);
	if (pVictory == NULL || iValue < 0 || iValue >= NUM_VICTORY_CONDITION_VALUES) return 0;
	return pVictory->conditionValue((VictoryConditionValue)iValue);
}

int CyVictoryInfo::getCityCulture(int iVictory) const
{
	const CvVictoryInfo* pVictory = cyv_victory(iVictory);
	if (pVictory == NULL) return -1;
	return pVictory->getCityCulture();
}

std::string CyVictoryInfo::getMovie(int iVictory) const
{
	const CvVictoryInfo* pVictory = cyv_victory(iVictory);
	if (pVictory == NULL) return std::string();
	return std::string(pVictory->getMovie());
}

void CyVictoryInfo::pythonPublish()
{
	python::class_<CyVictoryInfo>("CyVictoryInfo")
		.def("conditionFlag",   &CyVictoryInfo::conditionFlag)
		.def("conditionValue",  &CyVictoryInfo::conditionValue)
		.def("getCityCulture",  &CyVictoryInfo::getCityCulture)
		.def("getMovie",        &CyVictoryInfo::getMovie)
		;
}
