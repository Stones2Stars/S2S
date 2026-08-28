#include "CvGameCoreDLL.h"
#include "CyUnitInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvClassificationIds.h"
#include "Defines/CvGlobals.h"

namespace
{
	//	The ONE bounds gate for this registry. The id arrives from script, so it is checked here rather than
	//	trusted (docs/architecture/patterns.md §WRITE-ONCE-AT-LOAD: a read never creates, and FASSERT_BOUNDS is compiled out of Release,
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

int CyUnitInfo::getDomain(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? (int)pUnit->getDomain() : (int)NO_DOMAIN;
}

int CyUnitInfo::getCost(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? pUnit->getProductionCost() : 0;
}

int CyUnitInfo::getDefaultUnitAI(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	return pUnit ? (int)pUnit->getDefaultUnitAI() : (int)NO_UNITAI;
}

namespace
{
	//	The two SPREAD maps are keyed (target id -> strength), and a consumer wants the ids it can actually
	//	spread -- so a zero-strength entry is dropped here rather than at every call site.
	python::list cyunit_spreadIds(const std::map<int, int>& kSpread)
	{
		python::list lIds;
		for (std::map<int, int>::const_iterator it = kSpread.begin(); it != kSpread.end(); ++it)
		{
			if (it->second > 0) lIds.append(it->first);
		}
		return lIds;
	}
}

python::list CyUnitInfo::getReligionSpreads(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return python::list();
	return cyunit_spreadIds(pUnit->getReligionSpread());
}

python::list CyUnitInfo::getCorporationSpreads(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return python::list();
	return cyunit_spreadIds(pUnit->getCorporationSpread());
}

python::list CyUnitInfo::getGrantedGreatPeople(int iUnit) const
{
	python::list lIds;
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return lIds;

	const std::vector<int>& aiSpecialists = pUnit->getGrantedGreatPeople();
	for (size_t iSpecialist = 0; iSpecialist < aiSpecialists.size(); ++iSpecialist)
	{
		lIds.append(aiSpecialists[iSpecialist]);
	}
	return lIds;
}

python::list CyUnitInfo::getHeritage(int iUnit) const
{
	python::list lIds;
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return lIds;

	const std::vector<int>& aiHeritage = pUnit->getHeritage();
	for (size_t iHeritage = 0; iHeritage < aiHeritage.size(); ++iHeritage)
	{
		lIds.append(aiHeritage[iHeritage]);
	}
	return lIds;
}

int CyUnitInfo::getCargoSpace(int iUnit) const
{
	const CvUnitInfo* pUnit = cyunit_unit(iUnit);
	if (pUnit == NULL) return 0;
	//	Reduced to the whole count here, which is where the boundary reduces -- CvUnit::cargoSpace performs the
	//	same division at its own point of use.
	return pUnit->getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100;
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
		.def("getDomain",               &CyUnitInfo::getDomain)
		.def("getCost",                 &CyUnitInfo::getCost)
		.def("getDefaultUnitAI",        &CyUnitInfo::getDefaultUnitAI)
		.def("getCargoSpace",           &CyUnitInfo::getCargoSpace)
		.def("getReligionSpreads",      &CyUnitInfo::getReligionSpreads)
		.def("getCorporationSpreads",   &CyUnitInfo::getCorporationSpreads)
		.def("getGrantedGreatPeople",   &CyUnitInfo::getGrantedGreatPeople)
		.def("getHeritage",             &CyUnitInfo::getHeritage)
		;
}
