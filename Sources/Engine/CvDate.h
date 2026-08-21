#pragma once

//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:    CvDate.h
//
//  PURPOSE: Class to keep a Civ4 date and methods related to the time/turn relationship
//
//  A date is a tick count from the calendar epoch (the first era's historical start
//  year): 30 ticks per month, 360 per year. Dates are derived from the per-era pacing
//  data on CvEraInfo (historical year span + Normal-speed turn count) scaled by the
//  game speed (CvGameSpeedInfo) — there is no stored turn->date table.
//
//------------------------------------------------------------------------------------------------
#ifndef CV_DATE_H
#define CV_DATE_H

class CvDate
{
public:

	CvDate();
	explicit CvDate(uint32_t iTick);

	int getYear() const;
	int getMonth() const;
	int getWeek() const; // number of the week in a month
	SeasonTypes getSeason() const;
	int getDay() const;
	uint32_t GetTick() const;
	void setTick(uint32_t newTick);

	// Calendar ticks one turn advances at this date (rate of the era containing it).
	int getTicksPerTurn(GameSpeedTypes eGameSpeed = NO_GAMESPEED) const;

	void increment(GameSpeedTypes eGameSpeed = NO_GAMESPEED);
	void increment(int iTurns, GameSpeedTypes eGameSpeed = NO_GAMESPEED); // inefficient
	static CvDate getDate(int iTurn, GameSpeedTypes eGameSpeed = NO_GAMESPEED);
	static CvDate getStartingDate();

	bool operator<(const CvDate& kDate) const;
	bool operator<=(const CvDate& kDate) const;
	bool operator>(const CvDate& kDate) const;
	bool operator>=(const CvDate& kDate) const;
	bool operator==(const CvDate& kDate) const;
	bool operator!=(const CvDate& kDate) const;

protected:
	uint32_t m_iTick;
};

#endif
