#pragma once
#ifndef CV_JSON_ROUTE_INFO_H
#define CV_JSON_ROUTE_INFO_H

//
//	CvRouteInfo -- the JSON real poco for ROUTES (the CvXInfo replacement). LIVE surface only (owner ruling
//	2026-07-07): the getters a getRouteInfo(...) caller actually reads. Base traversal cost is intrinsic
//	(identity.movementCost / flatMovementCost -- the established curator pattern, curate_route.py:10); the tech-gated
//	move DELTAS are the `movement` family on the route (curate_route.py:13), reconstructed here into getTechMovementChange.
//	The route's improvement-keyed yield boost (production.plot.improvements.{X}) has NO getRouteInfo getter -> it is
//	cascade-only, NOT a poco member. Values human-native; the cascade ×100s on its side.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / BonusTypes / NO_BONUS
#include <map>
#include <vector>

class CvRouteInfo : public CvInfo
{
public:
	CvRouteInfo();

	int getValue() const { return m_iValue; }
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }
	int getMovementCost() const { return m_iMovementCost; }
	int getFlatMovementCost() const { return m_iFlatMovementCost; }
	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getTechMovementChange(int iTech) const;   // per-tech route move delta (from the `movement.plot.flat` family)
	bool isSeaTunnel() const { return m_bSeaTunnel; }
	int getZobristValue() const { return m_iZobristValue; }

	int* getYieldChangeArray() const { return const_cast<int*>(m_aiYieldChange); }   // real (food/production/commerce .plot.flat)

	// property engine (self-contained, #429); XML-era manipulator data deferred -- empty for now.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// Route<-bonus prerequisite (compat surface for the getRouteInfo(...) callers -- CvPlot route validity,
	// CvPlayerAI/CvDLLWidgetData). Both relationships are stored INVERTED onto the bonus and reverse-mapped at load
	// (cascadeLoadJson): the single AND-prereq via the bonus's `enables.routesAnd` (-> getPrereqBonus, the CvPlot.cpp
	// build gate), the OR-list via `enables.routes` (-> getPrereqOrBonuses). The two buckets keep AND vs OR distinct
	// (a route needs its single AND bonus AND one of its OR bonuses -- e.g. railroad needs steel-wares AND coal/oil).
	int getPrereqBonus() const { return m_ePrereqBonus; }
	void setPrereqBonus(BonusTypes eBonus) { m_ePrereqBonus = eBonus; }   // load-time reverse-index writer (single AND)
	const std::vector<BonusTypes>& getPrereqOrBonuses() const { return m_aePrereqOrBonuses; }
	void addPrereqOrBonus(BonusTypes eBonus) { m_aePrereqOrBonuses.push_back(eBonus); }   // load-time reverse-index writer (OR-list)

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
	int m_iZobristValue;                 // map-hash drawn from the synced RNG in the ctor (exact legacy CvRouteInfo behavior, OOS); CvPlot XORs it into m_movementCharacteristicsHash
	int m_aiYieldChange[NUM_YIELD_TYPES];// food/production/commerce .plot.flat
	bool m_bSeaTunnel;                   // identity.seaTunnel
	std::map<int, int> m_techMovementChange;   // techId -> move-cost delta (movement.plot.flat enabled:{tech})
	BonusTypes m_ePrereqBonus;                 // single AND prereq bonus (reverse index from bonuses' enables.routesAnd)
	std::vector<BonusTypes> m_aePrereqOrBonuses;   // OR-list reverse index (from bonuses' enables.routes; built at load)
	CvPropertyManipulators m_PropertyManipulators;   // STUB empty -- property engine, XML-era manipulator data deferred
};

#endif // CV_JSON_ROUTE_INFO_H
