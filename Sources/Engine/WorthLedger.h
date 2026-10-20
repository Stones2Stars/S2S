#pragma once
#ifndef CV_WORTH_LEDGER_H
#define CV_WORTH_LEDGER_H

///<summary>One ledger's military worth and worth, summed over what it holds. Written by the demographics consumer
/// (Engine/DemographicsContext.h) and nothing else.</summary>
struct WorthLedger
{
	///<summary>The summed military worth: what the ledger adds to power.</summary>
	int64_t militaryWorth;
	///<summary>The summed worth: what the ledger adds to assets.</summary>
	int64_t worth;

	WorthLedger() : militaryWorth(0), worth(0) {}
	///<summary>Moves both sums by the signed amounts given.</summary>
	void change(int64_t iMilitaryWorth, int64_t iWorth);
	///<summary>Zeroes both sums, for an owner reset.</summary>
	void clear();
};

#endif // CV_WORTH_LEDGER_H
