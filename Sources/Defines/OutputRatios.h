#pragma once

#ifndef CIV_OUTPUTRATIOS_H
#define CIV_OUTPUTRATIOS_H
struct OutputRatios
{
	// The weights are the city owner's networked settings (MODDEROPTION_BASE_WEIGHT_*).
	OutputRatios(int food, int production, int commerce, int foodWeight, int productionWeight, int commerceWeight);
	void IncreaseOutputWeights(int foodWeight, int productionWeight, int commerceWeight);
	void IncreaseFoodWeight(const int foodWeight);
	void DecraseFoodWeight(const int foodWeight);
	void IncreaseProductionWeight(const int productionWeight);
	void DecraseProductionWeight(const int productionWeight);
	void IncreaseCommerceWeight(const int commerceWeight);
	void DecraseCommerceWeight(const int commerceWeight);
	int CalculateOutputValue(int food, int production, int commerce) const;
	int food_ratio;
	int production_ratio;
	int commerce_ratio;
	int food_weight;
	int production_weight;
	int commerce_weight;
};

#endif