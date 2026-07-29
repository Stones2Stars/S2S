#include "CvGameCoreDLL.h"          // getModifiedIntValue
#include "Engine/CvGameSpeedScale.h"
#include "Defines/CvGlobals.h"
#include "AI/CvGameAI.h"            // GC.getGame()
#include "Infos/CvGameSpeedInfo.h"
#include "Infos/CvInfoKinds.h"      // SCALAR_SPEED / SCALAR_MISSION_YIELD_MULTIPLIER, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT

namespace
{
	// The live speed's straggler percent. A percent is NOT scaled ([DEC-fixedpoint-x100]), so nothing
	// is reduced here -- the value is already the human percent every caller wants.
	int liveSpeedScalarAsPercent(InfoScalar eScalar)
	{
		const CvGameSpeedInfo& kSpeed = GC.getGameSpeedInfo(GC.getGame().getGameSpeedType());
		return kSpeed.getScalar(eScalar, CASC_SCOPE_WORLD, CASC_UNIT_PERCENT);
	}
}

int CvGameSpeedScale::speedPercent()
{
	return liveSpeedScalarAsPercent(SCALAR_SPEED);
}

int CvGameSpeedScale::hammerCostPercent()
{
	const int iSpeedPercent = speedPercent();
	if (GC.getGame().isOption(GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS))
	{
		return getModifiedIntValue(iSpeedPercent, GC.getUPSCALED_HAMMER_COST_MODIFIER());
	}
	return iSpeedPercent;
}

int CvGameSpeedScale::missionYieldPercent()
{
	return liveSpeedScalarAsPercent(SCALAR_MISSION_YIELD_MULTIPLIER);
}
