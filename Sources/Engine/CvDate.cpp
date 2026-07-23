//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:    CvDate.cpp
//
//  PURPOSE: Class to keep a Civ4 date and methods related to the time/turn relationship
//
//------------------------------------------------------------------------------------------------

#include "FProfiler.h"

#include "CvGameCoreDLL.h"
#include "CvGameCoreUtils.h"
#include "CvGameAI.h"
#include "CvGlobals.h"
#include "CvInfos.h"
#include "CvEraInfo.h"

namespace
{
	GameSpeedTypes resolveGameSpeed(GameSpeedTypes eGameSpeed)
	{
		return eGameSpeed == NO_GAMESPEED ? GC.getGame().getGameSpeedType() : eGameSpeed;
	}

	// Calendar ticks spanned by an era's historical years (asserts contiguity once).
	int eraSpanTicks(int iEra)
	{
		const CvEraInfo& kEra = GC.getEraInfo((EraTypes)iEra);
		FAssertMsg(kEra.getHistoricalEndYear() > kEra.getHistoricalStartYear(), "era needs a positive historical year span");
		FAssertMsg(
			iEra == 0 || GC.getEraInfo((EraTypes)(iEra - 1)).getHistoricalEndYear() == kEra.getHistoricalStartYear(),
			"era historical years must be contiguous"
		);
		return (kEra.getHistoricalEndYear() - kEra.getHistoricalStartYear()) * 360;
	}
}

CvDate::CvDate()
{
	m_iTick = 0;
}

CvDate::CvDate(uint32_t iTick) :
m_iTick(iTick)
{
}

int CvDate::getYear() const
{
	return GC.getGame().getStartYear() + (m_iTick / 360);
}

int CvDate::getDay() const
{
	return ((m_iTick % 360) % 30) + 1;
}

uint32_t CvDate::GetTick() const
{
	return m_iTick;
}

void CvDate::setTick(uint32_t newTick)
{
	m_iTick = newTick;
}

int CvDate::getMonth() const
{
	return (m_iTick % 360) / 30;
}

int CvDate::getWeek() const
{
	return ((getDay() - 1) / 7) + 1;
}

SeasonTypes CvDate::getSeason() const
{
	const int month = getMonth();

	if (month <= 1 || month >= 11)
	{
		return SEASON_WINTER; // Winter
	}
	if (month >= 2 && month <= 4)
	{
		return SEASON_SPRING; // Spring
	}
	if (month >= 5 && month <= 7)
	{
		return SEASON_SUMMER; // Summer
	}
	return SEASON_AUTUMN; // Autumn (months 8-10)
}


int CvDate::getTicksPerTurn(GameSpeedTypes eGameSpeed) const
{
	PROFILE_EXTRA_FUNC();
	const CvGameSpeedInfo& kSpeed = GC.getGameSpeedInfo(resolveGameSpeed(eGameSpeed));

	uint32_t iEraStartTick = 0;
	for (int iEra = 0; iEra < GC.getNumEraInfos(); iEra++)
	{
		iEraStartTick += eraSpanTicks(iEra);
		if (m_iTick < iEraStartTick)
		{
			return kSpeed.getTicksPerTurnInEra(iEra);
		}
	}
	// past the end of history: keep the last era's rate
	return kSpeed.getTicksPerTurnInEra(GC.getNumEraInfos() - 1);
}

void CvDate::increment(GameSpeedTypes eGameSpeed)
{
	m_iTick += getTicksPerTurn(eGameSpeed);
}

void CvDate::increment(int iTurns, GameSpeedTypes eGameSpeed)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < iTurns; i++)
	{
		increment(eGameSpeed);
	}
}

bool CvDate::operator !=(const CvDate &kDate) const
{
	return !(*this == kDate);
}

bool CvDate::operator ==(const CvDate &kDate) const
{
	return m_iTick == kDate.GetTick();
}

bool CvDate::operator <(const CvDate &kDate) const
{
	return m_iTick < kDate.GetTick();
}

bool CvDate::operator >(const CvDate &kDate) const
{
	return m_iTick > kDate.GetTick();
}

bool CvDate::operator <=(const CvDate &kDate) const
{
	return ! (*this > kDate);
}

bool CvDate::operator >=(const CvDate &kDate) const
{
	return ! (*this < kDate);
}

CvDate CvDate::getStartingDate()
{
	return CvDate();
}

// Turn -> date: linear interpolation of the era's historical year span over the
// era's turn count at the given speed.
CvDate CvDate::getDate(int iTurn, GameSpeedTypes eGameSpeed)
{
	PROFILE_EXTRA_FUNC();
	if (iTurn <= 0)
	{
		return getStartingDate();
	}
	const CvGameSpeedInfo& kSpeed = GC.getGameSpeedInfo(resolveGameSpeed(eGameSpeed));

	int iTurnsBefore = 0;
	uint32_t iEraStartTick = 0;
	for (int iEra = 0; iEra < GC.getNumEraInfos(); iEra++)
	{
		const int iTurnsInEra = kSpeed.getTurnsInEra(iEra);
		const int iSpanTicks = eraSpanTicks(iEra);

		if (iTurn < iTurnsBefore + iTurnsInEra)
		{
			const int iTurnsIntoEra = iTurn - iTurnsBefore;
			return CvDate(iEraStartTick + (uint32_t)((int64_t)iSpanTicks * iTurnsIntoEra / iTurnsInEra));
		}
		iTurnsBefore += iTurnsInEra;
		iEraStartTick += iSpanTicks;
	}
	// past the end of history: extrapolate at the last era's rate
	const int iLastEra = GC.getNumEraInfos() - 1;
	return CvDate(iEraStartTick + (uint32_t)(kSpeed.getTicksPerTurnInEra(iLastEra) * (iTurn - iTurnsBefore)));
}
