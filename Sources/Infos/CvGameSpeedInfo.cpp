//------------------------------------------------------------------------------------------------
//  FILE:    CvGameSpeedInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvEraInfo.h"
#include "CvGameSpeedInfo.h"
#include "CvJsonParse.h"          // jsonFamVal (raw human value; the ×100 lives only in the cascade deposit tree)


//======================================================================================================
//					CvGameSpeedInfo
//======================================================================================================

CvGameSpeedInfo::CvGameSpeedInfo()
	: m_iSpeedPercent(0)
	, m_iUnitYieldScalePercent(100)
{
}


int CvGameSpeedInfo::getSpeedPercent() const
{
	return m_iSpeedPercent;
}


int CvGameSpeedInfo::getHammerCostPercent() const
{
	if (GC.getGame().isOption(GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS))
	{
		return getModifiedIntValue(m_iSpeedPercent, GC.getUPSCALED_HAMMER_COST_MODIFIER());
	}
	return m_iSpeedPercent;
}


int CvGameSpeedInfo::getUnitYieldScalePercent() const
{
	return m_iUnitYieldScalePercent;
}


int CvGameSpeedInfo::getTurnsInEra(int iEra) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	return std::max(1, (GC.getEraInfo((EraTypes)iEra).getNormalSpeedTurns() * m_iSpeedPercent + 50) / 100);
}


int CvGameSpeedInfo::getEraStartTurn(int iEra) const
{
	PROFILE_EXTRA_FUNC();
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	int iTurn = 0;
	for (int i = 0; i < iEra; i++)
	{
		iTurn += getTurnsInEra(i);
	}
	return iTurn;
}


int CvGameSpeedInfo::getTotalTurns() const
{
	PROFILE_EXTRA_FUNC();
	int iTurns = 0;
	for (int i = 0; i < GC.getNumEraInfos(); i++)
	{
		iTurns += getTurnsInEra(i);
	}
	return iTurns;
}


// Calendar ticks (days; 30/month, 360/year) that one turn advances within the era.
int CvGameSpeedInfo::getTicksPerTurnInEra(int iEra) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), iEra);
	const CvEraInfo& kEra = GC.getEraInfo((EraTypes)iEra);
	const int iSpanTicks = (kEra.getHistoricalEndYear() - kEra.getHistoricalStartYear()) * 360;
	const int iTurns = getTurnsInEra(iEra);
	return std::max(1, (iSpanTicks + iTurns / 2) / iTurns);
}


// #430: the JSON load hook. speed.world.percent -> iSpeedPercent (raw percent, Normal=100);
// missionYieldMultiplier.world.percent -> iUnitYieldScalePercent (raw percent, curator elides the
// 100-default). jsonFamVal returns the RAW human value -- the ×100 fixed-point lives only in the
// cascade deposit tree, never on these engine-getter members.
void CvGameSpeedInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	m_iSpeedPercent = jsonFamVal(o, "speed", "world", "percent");
	// legacy default 100 when the key is absent (curator elides values equal to the legacy load default).
	const int iUnitYield = jsonFamVal(o, "missionYieldMultiplier", "world", "percent");
	m_iUnitYieldScalePercent = (iUnitYield != 0) ? iUnitYield : 100;
}
