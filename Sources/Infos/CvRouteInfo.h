#pragma once
#ifndef CV_JSON_ROUTE_INFO_H
#define CV_JSON_ROUTE_INFO_H

//
//	CvRouteInfo -- the ROUTE poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the route's OWN tile output (modifier.md §5 plot-substrate
//	own-output) lives on the compiled modifier surface -- the point reads fetch the unconditioned plot flats;
//	the improvement-keyed yield boosts (the governing-deliverer shape, modifier.md §4: the route governs the
//	improvements it upgrades, {yield}.plot.improvements.{IMP}) and the tech-conditioned move deltas
//	(movement.plot.flat, enabled:{tech}) are compiled keyed/conditioned entries -- entry-list reads by design.
//	Base traversal cost is intrinsic self-data (identity.movementCost / flatMovementCost). The bonus
//	prerequisites are the LOAD-reconstructed forward FKs (store-inverted onto the bonus's enables.routes /
//	enables.routesAnd, un-inverted by CvReversePass). No legacy-mirror modifier member survives
//	([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // BonusTypes / NO_BONUS
#include <vector>

class CvRouteInfo : public CvInfo
{
public:
	CvRouteInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: food/production/commerce plot flats -- the route's own tile output; the
	// plot.improvements.{IMP} keyed rows and the tech-conditioned movement deltas stay entry-list reads.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }

	// ======================= 3. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getValue() const { return m_iValue; }                       // identity.value (route quality rank)
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; } // identity.advancedStart.cost
	int getMovementCost() const { return m_iMovementCost; }         // identity.movementCost (base traversal)
	bool isSeaTunnel() const { return m_bSeaTunnel; }               // identity.seaTunnel
	int getZobristValue() const { return m_iZobristValue; }

	// --- the LOAD-reconstructed bonus-prerequisite forward FKs (CvReversePass::
	// rp_reconstructRouteBonusPrereqs: the single AND prereq from the bonus's enables.routesAnd, the OR-list
	// from enables.routes -- the two buckets keep AND vs OR distinct, e.g. railroad needs steel-wares AND
	// coal/oil). The writers are the reverse pass's load-window setters. ---
	int getPrereqBonus() const { return m_ePrereqBonus; }
	void setPrereqBonus(BonusTypes eBonus) { m_ePrereqBonus = eBonus; }
	const std::vector<BonusTypes>& getPrereqOrBonuses() const { return m_aePrereqOrBonuses; }
	void addPrereqOrBonus(BonusTypes eBonus) { m_aePrereqOrBonuses.push_back(eBonus); }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iValue;
	int m_iAdvancedStartCost;
	int m_iMovementCost;
	int m_iZobristValue;               // map-hash drawn from the synced RNG in the ctor (OOS-load-bearing)
	bool m_bSeaTunnel;
	BonusTypes m_ePrereqBonus;                     // load-reconstructed single AND prereq (CvReversePass)
	std::vector<BonusTypes> m_aePrereqOrBonuses;   // load-reconstructed OR-list (CvReversePass)
};

#endif // CV_JSON_ROUTE_INFO_H
