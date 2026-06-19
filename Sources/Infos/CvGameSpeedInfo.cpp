//------------------------------------------------------------------------------------------------
//  FILE:    CvGameSpeedInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "CvEraInfo.h"
#include "CvGameSpeedInfo.h"


//======================================================================================================
//					CvGameSpeedInfo
//======================================================================================================

CvGameSpeedInfo::CvGameSpeedInfo()
{
	CvInfoUtil(this).initDataMembers();
}


// Every XML-backed field is declared here (#196); read/copy/checksum all derive from it.
void CvGameSpeedInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_iSpeedPercent, L"iSpeedPercent")
		.add(m_iUnitYieldScalePercent, L"iUnitYieldScalePercent", 100)
	;
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


bool CvGameSpeedInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}
	CvInfoUtil(this).readXml(pXML);

	return true;
}


void CvGameSpeedInfo::copyNonDefaults(const CvGameSpeedInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);
	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}


void CvGameSpeedInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}
