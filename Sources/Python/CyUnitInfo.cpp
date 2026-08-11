#include "CvGameCoreDLL.h"
#include "CyUnitInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvClassificationIds.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. The id arrives from script, so it is checked here rather than
	//	trusted ([DEC-info-plane-read-only]: a read never creates, and FASSERT_BOUNDS is compiled out of Release,
	//	which is where this runs).
	const CvUnitInfo* cyunit_unit(int iUnit)
	{
		if (iUnit < 0 || iUnit >= GC.getNumUnitInfos())
		{
			return NULL;
		}
		return &GC.getUnitInfo((UnitTypes)iUnit);
	}
}

python::list CyUnitInfo::getCombatClasses(int iUnit) const
{
	python::list lCombatClasses;

	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return lCombatClasses;

	//	The PRIMARY first -- a caller reading only the subs loses the class most units are actually identified by.
	const int iPrimary = pUnit->getCombatClass();
	if (iPrimary != -1)
	{
		lCombatClasses.append(iPrimary);
	}

	const std::vector<int>& aiSubClasses = pUnit->getCombatClasses();
	for (size_t iSubClass = 0; iSubClass < aiSubClasses.size(); ++iSubClass)
	{
		if (aiSubClasses[iSubClass] != iPrimary)
		{
			lCombatClasses.append(aiSubClasses[iSubClass]);
		}
	}
	return lCombatClasses;
}

python::list CyUnitInfo::getBuilds(int iUnit) const
{
	python::list lBuilds;

	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return lBuilds;

	const std::vector<int>& aiBuilds = pUnit->getBuilds();
	for (size_t iBuild = 0; iBuild < aiBuilds.size(); ++iBuild)
	{
		lBuilds.append(aiBuilds[iBuild]);
	}
	return lBuilds;
}

python::list CyUnitInfo::getGrantedPromotions(int iUnit) const
{
	python::list lPromotions;

	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return lPromotions;

	const std::vector<int>& aiPromotions = pUnit->getGrantedPromotions();
	for (size_t iPromotion = 0; iPromotion < aiPromotions.size(); ++iPromotion)
	{
		lPromotions.append(aiPromotions[iPromotion]);
	}
	return lPromotions;
}

bool CyUnitInfo::isFound(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? pUnit->hasSkill(CLS_SKILL_FOUND) : false;
}

bool CyUnitInfo::isIgnoreBuildingDefense(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? pUnit->hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE) : false;
}

int CyUnitInfo::getConscription(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? pUnit->getConscription() : 0;
}

int CyUnitInfo::getCaptureUnit(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? pUnit->getCaptures() : -1;
}

void CyUnitInfo::pythonPublish()
{
	python::class_<CyUnitInfo>("CyUnitInfo")
		.def("getCombatClasses",        &CyUnitInfo::getCombatClasses)
		.def("getBuilds",               &CyUnitInfo::getBuilds)
		.def("getGrantedPromotions",    &CyUnitInfo::getGrantedPromotions)
		.def("isFound",                 &CyUnitInfo::isFound)
		.def("isIgnoreBuildingDefense", &CyUnitInfo::isIgnoreBuildingDefense)
		.def("getConscription",         &CyUnitInfo::getConscription)
		.def("getCaptureUnit",          &CyUnitInfo::getCaptureUnit)
		;
}
