#pragma once
#ifndef CV_JSON_CIVIC_INFO_H
#define CV_JSON_CIVIC_INFO_H

//
//	CvCivicInfo -- the CIVIC poco rebuilt to the full exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else). Styled for the JSON anatomy (json.md §2); every magnitude read is a
//	load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope are separate parameters
//	([DEC-scope-is-an-axis]); no legacy getter name returns ([DEC-new-getter-surface]).
//

#include "CvInfo.h"

class CvCivicInfo : public CvInfo
{
public:
	CvCivicInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getPolicies()  const { return &m_policies; }
	virtual const CvClassificationBlock* getAmenities() const { return &m_amenities; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json §9) ====
	// A civic PROVIDES a policy to the empire while adopted (the grantor direction -- the empire HOLDS it;
	// EmpireContext.policies is the derived union over the live grantors).
	bool providesPolicies() const            { return !m_policies.isEmpty(); }
	// A civic also PROVIDES amenities -- to every CITY of the empire rather than to the empire itself (json §8:
	// "a city or cities"). The city HOLDS the fold; this is only the grantor's side of it. A civic's grant may be
	// CONDITIONED (`abolishedAnger` while IS_CAPITAL), which is why the block carries the §3.9 entry form and the
	// fold evaluates it per receiving city.
	bool providesAmenities() const             { return !m_amenities.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getCommerceModifier(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	// The authored wellbeing families' SIGNED sums. ⛔ ANGER/UNHEALTH read 0 here BY CONSTRUCTION and that is
	// never a gap to chase: an INFO keeps a negative in its POSITIVE family (happiness -1, not anger +1) --
	// the sign ROUTING to the opposing channel happens at FILL, on the city PACKAGE, not on authored data
	// (modifier.md §2b). So this read already carries the negatives; there is nothing to verify in the JSON.
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}
	// The grouped families this type's census participation covers.
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_PERCENT); }
	// The free-allowance upkeep kinds are FLAT amounts (ruling 28: signed free-amount entries; the group floor
	// is combine metadata -- infoCombineFloorAtZero -- applied at the ONE combine seam, InfoValuation).
	int getFlatUpkeep(UpkeepKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UPKEEP, eKind, eScope, CASC_UNIT_FLAT); }
	int getCapture(CaptureKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CAPTURE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getDiplomacy(DiplomacyKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DIPLOMACY, eKind, eScope, infoKindUnit(MODFAM_DIPLOMACY, eKind)); }
	int getStateReligion(StateReligionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_STATE_RELIGION, eKind, eScope, infoKindUnit(MODFAM_STATE_RELIGION, eKind)); }
	int getRevolution(RevolutionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_REVOLUTION, eKind, eScope, infoKindUnit(MODFAM_REVOLUTION, eKind)); }
	int getTradeRoute(TradeRouteKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, eKind, eScope, infoKindUnit(MODFAM_TRADE_ROUTES, eKind)); }
	int getTradeRouteYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, TRADE_ROUTE_MODIFIER_FOOD + (int)eYield, eScope, CASC_UNIT_PERCENT); }
	int getBuildRateModifier(BuildRateKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BUILD_RATE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getExperience(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, infoKindUnit(MODFAM_EXPERIENCE, eKind)); }
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, eKind, eScope, CASC_UNIT_PERCENT); }
	// (growth / inflation / improvementUpgradeRate / workRate / conscript / greatGeneralRate / greatPeopleRate /
	// hurry are the 1-2-kind stragglers: the base getScalar(SCALAR_*) covers them. freeSpecialists / the
	// keyed buildings targets are entry-list reads by design.)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getCivicOption() const  { return m_iCivicOption; }    // CIVICOPTION_* FK -- the civic's column
	int getAnarchyLength() const { return m_iAnarchyLength; } // revolution anarchy turns (plain count)
	int getUpkeepLevel() const   { return m_iUpkeepLevel; }   // UPKEEP_* FK -- the upkeep class
	// The BASE city-limit config (`identity.cityLimit`, ruling 26 -- the anarchyLength convention: a plain
	// intrinsic count, human, 0 = the civic carries no limit). The RESOLVED limit is base × the world-size
	// scale percent under the overexpansion option -- InfoValuation::resolvedCityLimit is the ONE engine-side
	// read; the CITY_LIMIT per.above eval leg (MMKernel::perApply) applies the same scale.
	int getCityLimit() const { return m_iCityLimit; }
	// Ruling 26, option (a): does this civic carry the over-limit ANGER (a compiled CITY_LIMIT `per.above`
	// wellbeing entry)? Materialized at mapFrom from the compiled entries -- the presence verdict the engine's
	// hard-cap sites read (a limit WITHOUT the anger deposit is a HARD cap; with it, exceeding is allowed and
	// the cascade's deposit carries the anger).
	bool hasCityOverLimitAnger() const { return m_bCityOverLimitAnger; }
	const CvWString& getWeLoveTheKingKey() const { return m_szWeLoveTheKingKey; }   // TXT_KEY (celebration text)

	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }   // §5 -- triggers + the folded grants

	// The KEEP-legacy property engine's per-turn SOURCES, bridged from this civic's PROPERTY_* families
	// (property-audit.md). Player-gathered and fanned to every owner city -- RELATION_ASSOCIATED.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutPolicies()  { return &m_policies; }
	virtual CvClassificationBlock* mutAmenities() { return &m_amenities; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;
	CvClassificationBlock m_policies;
	CvClassificationBlock m_amenities;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	int m_iCivicOption;
	int m_iAnarchyLength;
	int m_iUpkeepLevel;
	int m_iCityLimit;
	bool m_bCityOverLimitAnger;
	CvWString m_szWeLoveTheKingKey;
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_CIVIC_INFO_H
