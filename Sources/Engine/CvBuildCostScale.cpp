#include "CvGameCoreDLL.h"
#include "Engine/CvBuildCostScale.h"
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvUnitInfo.h"
#include "Infos/CvClassificationIds.h"   // CLS_TAG_MERCHANT
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"   // GC.getGame()

//	One band field's contribution, in whole percent points. The authored value is a small BAND INDEX with 1 (and
//	an unauthored field, which reads 0 -- see below) as the neutral middle, so each field is read by equality
//	against its bands rather than scaled by its value.
//	⚠ 0 is BOTH "the cheapest band" and "unauthored". That ambiguity is inherited from the legacy data shape, not
//	introduced here: the fields are authored on most buildings and absent on the rest, and the legacy derivation
//	treated an absent field as the 0 band too. Preserved deliberately so this lands behaviour-identical; a data
//	pass that makes the neutral band explicit is the place to fix it, not this calc.
static int bc_band(int iValue, int iAtZero, int iAtTwo, int iAtThree)
{
	if (iValue == 0) return iAtZero;
	if (iValue == 2) return iAtTwo;
	if (iValue == 3) return iAtThree;
	return 0;
}

int CvBuildCostScale::buildingCostPercent(const CvBuildingInfo& kBuilding)
{
	if (!GC.getGame().isOption(GAMEOPTION_REALISTIC_BUILDING_COST))
	{
		return 100;   // the option gates the WHOLE composition; unmodified means exactly the authored cost
	}
	int iPercent = 100;
	iPercent += bc_band(kBuilding.getCostSizeModifier(),       -20, 15, 30);
	iPercent += bc_band(kBuilding.getCostCountModifier(),       -5, 15,  0);
	iPercent += bc_band(kBuilding.getCostMaterialsModifier(),  -10, 15,  0);
	iPercent += bc_band(kBuilding.getCostComplexityModifier(), -20, 20,  0);
	return iPercent;
}

int CvBuildCostScale::unitProductionPercent(const CvUnitInfo& kUnit)
{
	//	A MERCHANT keeps the ordinary pace even under Size Matters: the SM pace is a discount granted because
	//	units merge, and a unit that cannot merge would just be cheaper for nothing.
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) && !kUnit.hasTag(CLS_TAG_MERCHANT))
	{
		return GC.getUNIT_PRODUCTION_PERCENT_SM();
	}
	return GC.getUNIT_PRODUCTION_PERCENT();
}
