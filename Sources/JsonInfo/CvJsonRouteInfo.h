#pragma once
#ifndef CV_JSON_ROUTE_INFO_H
#define CV_JSON_ROUTE_INFO_H

//
//	CvJsonRouteInfo -- the JSON real poco for ROUTES (the CvXInfo replacement). LIVE surface only (owner ruling
//	2026-07-07): the getters a getRouteInfo(...) caller actually reads. Base traversal cost is intrinsic
//	(identity.movementCost / flatMovementCost -- the established curator pattern, curate_route.py:10); the tech-gated
//	move DELTAS are the `movement` family on the route (curate_route.py:13), reconstructed here into getTechMovementChange.
//	The route's improvement-keyed yield boost (production.plot.improvements.{X}) has NO getRouteInfo getter -> it is
//	cascade-only, NOT a poco member. Values human-native; the cascade ×100s on its side.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / BonusTypes / NO_BONUS
#include <map>
#include <vector>

class CvJsonRouteInfo : public CvJsonInfo
{
public:
	CvJsonRouteInfo();

	int getValue() const { return m_iValue; }
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }
	int getMovementCost() const { return m_iMovementCost; }
	int getFlatMovementCost() const { return m_iFlatMovementCost; }
	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getTechMovementChange(int iTech) const;   // per-tech route move delta (from the `movement.plot.flat` family)
	bool isSeaTunnel() const { return m_bSeaTunnel; }
	int getZobristValue() const { return m_iZobristValue; }

	// Route<-bonus prerequisite (compat surface for the getRouteInfo(...) callers -- CvPlot route validity,
	// CvPlayerAI/CvDLLWidgetData). The relationship is stored INVERTED as the bonus's `enables.routes`
	// (curate_route.py), so it is reconstructed here at load into a reverse index (cascadeLoadJson).
	// getPrereqBonus (the legacy single AND-prereq) is authored by NO route -> always NO_BONUS.
	int getPrereqBonus() const { return NO_BONUS; }
	const std::vector<BonusTypes>& getPrereqOrBonuses() const { return m_aePrereqOrBonuses; }
	void addPrereqOrBonus(BonusTypes eBonus) { m_aePrereqOrBonuses.push_back(eBonus); }   // the load-time reverse-index writer

	// NB the route's bonus prerequisite is NOT here: it is a `requires`-type relationship, modelled as the BONUS's
	// `enables.routes` (curate_route.py:25/56 store-inversion). The cascade reads that off the bonus info and gates
	// buildability via GENERATE. It is availability data on the info side, never a stubbed getter on the route.

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonModifiers m_modifiers;
	int m_iValue;                        // identity.value
	int m_iAdvancedStartCost;            // identity.advancedStart.cost
	int m_iMovementCost;                 // identity.movementCost
	int m_iFlatMovementCost;             // identity.flatMovementCost
	int m_iZobristValue;                 // ⏳ map-hash: needs the exact legacy zobrist computation (OOS)
	int m_aiYieldChange[NUM_YIELD_TYPES];// food/production/commerce .plot.flat
	bool m_bSeaTunnel;                   // identity.seaTunnel
	std::map<int, int> m_techMovementChange;   // techId -> move-cost delta (movement.plot.flat enabled:{tech})
	std::vector<BonusTypes> m_aePrereqOrBonuses;   // reverse index from the bonuses' enables.routes (built at load)
};

#endif // CV_JSON_ROUTE_INFO_H
