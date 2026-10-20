#pragma once
#ifndef CV_WORTH_LEDGER_H
#define CV_WORTH_LEDGER_H

///<summary>One ledger's military worth and worth, summed over what it holds. Written by the demographics consumer
/// (Engine/DemographicsContext.h) and nothing else.</summary>
struct WorthLedger
{
	int militaryWorth;
	int worth;

	WorthLedger() : militaryWorth(0), worth(0) {}
	///<summary>Moves both sums by the signed amounts given.</summary>
	void change(int iMilitaryWorth, int iWorth);
	///<summary>Zeroes both sums, for an owner reset.</summary>
	void clear();
};

#endif // CV_WORTH_LEDGER_H
