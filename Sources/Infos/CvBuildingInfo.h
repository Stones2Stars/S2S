#pragma once
#ifndef CV_JSON_BUILDING_INFO_H
#define CV_JSON_BUILDING_INFO_H

//
//	CvBuildingInfo -- the BUILDING poco rebuilt to the full exemplar surface (patterns.md § THE GETTER SETUP:
//	the four read categories, nothing else). Styled for the JSON anatomy (json.md §2), members grouped by the
//	entity anatomy; every magnitude read is a load-compiled fetch ([DEC-materialize-at-mapfrom]); kind and scope
//	are separate parameters everywhere ([DEC-scope-is-an-axis]); every magnitude getter IS ×100
//	([DEC-fixedpoint-x100]); no legacy getter name returns ([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include <map>

class CvBuildingInfo : public CvInfo
{
public:
	CvBuildingInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires*  getRequires()     const { return &m_requires; }
	virtual const CvEdges*     getEdges()        const { return &m_edges; }
	virtual const CvAllowed*   getAllowed()      const { return &m_allowed; }
	virtual const CvTriggers*  getTriggers()     const { return &m_triggers; }
	virtual const CvProvides*  getProvides()     const { return &m_provides; }
	virtual const CvModifiers* getModifiers()    const { return &m_modifiers; }
	virtual const CvModifiers* getWhenObsolete() const { return &m_whenObsolete; }
	virtual const CvClassificationBlock* getAttributes()   const { return &m_attributes; }
	virtual const CvClassificationBlock* getCapabilities() const { return &m_capabilities; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json §8) ====
	// What the building HAS (held city-scope intrinsics: nukeImmune, zoneOfControl, governmentCenter, ...).
	bool hasAttribute(int iAttributeId) const { return m_attributes.hasId(iAttributeId); }
	bool hasAttributes() const                { return !m_attributes.isEmpty(); }
	// What the building PROVIDES to the empire (grantor-provided capabilities: setCultureRate, ...).
	bool providesCapability(int iCapabilityId) const { return m_capabilities.hasId(iCapabilityId); }
	bool providesCapabilities() const                { return !m_capabilities.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (The conditioned-list access + the expected* what-if valuations are the base surface: CvInfo's
	// modifierConditioned()/modifierConditionedRange() + expectedFlatYields()/... -- one declaration for every
	// rebuilt info, delegating to the ONE calc unit.)
	// The engine-enum channel groups (ruling 1: the engine enum IS the kind axis; the flat-vs-modifier split
	// lives in the NAME, never a scale suffix -- [DEC-fixedpoint-x100]).
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getCommerceModifier(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	// The wellbeing point read exposes the AUTHORED families' signed compiled sums (happiness/health); the
	// four-channel sign ROUTING is a fill/valuation rule (modifier.md §2b -- expectedWellbeing), so the two
	// unauthored channels (ANGER/UNHEALTH) hold no slot and read 0 here.
	int getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT);
	}
	// The grouped families this type's census participation covers -- one getter per group, parameterized over
	// the group's kind enum; the canonical authored unit resolves in the vocabulary (infoKindUnit), never here.
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }
	int getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MAINTENANCE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getTradeRoute(TradeRouteKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, eKind, eScope, infoKindUnit(MODFAM_TRADE_ROUTES, eKind)); }
	// The ruling-27 per-channel route-yield modifier (the §2a tradeYield input fold's per-source term): the
	// channel kinds sit contiguous in YieldTypes order, so the parameterized read is FOOD + eYield.
	int getTradeRouteYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_TRADE_ROUTES, TRADE_ROUTE_MODIFIER_FOOD + (int)eYield, eScope, CASC_UNIT_PERCENT); }
	int getExperience(ExperienceKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXPERIENCE, eKind, eScope, infoKindUnit(MODFAM_EXPERIENCE, eKind)); }
	int getRevolution(RevolutionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_REVOLUTION, eKind, eScope, infoKindUnit(MODFAM_REVOLUTION, eKind)); }
	int getUnderworld(UnderworldKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_UNDERWORLD, eKind, eScope, CASC_UNIT_FLAT); }
	int getHeal(HealKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_HEAL, eKind, eScope, CASC_UNIT_FLAT); }
	int getCombatModifier(CombatKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COMBAT, eKind, eScope, CASC_UNIT_PERCENT); }
	int getBuildRateModifier(BuildRateKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_BUILD_RATE, eKind, eScope, CASC_UNIT_PERCENT); }
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, eKind, eScope, CASC_UNIT_PERCENT); }
	// (The remaining census families -- anarchy/foodKept/hurryAnger/occupationTime/populationGrowthRate/
	// espionageDefense/greatGeneralRate/greatPeopleRate/goldenAge/warWeariness/workRate -- are the 1-2-kind
	// stragglers: the base getScalar(SCALAR_*) covers them. Keyed/targeted groups -- allowedSpecialists /
	// freeSpecialists / religion.{RELIGION_*} / the keyed buildRate targets -- are entry-list reads by design.)

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	// Counts/configs/FKs/flags are plain typed values (the identity.cityLimit convention: a config is human, a
	// MAGNITUDE is ×100 -- the two ×100 members here are the commerce magnitudes).
	int getWorth() const                    { return m_iWorth; }                    // AI trade/conquest valuation config
	int getMilitaryWorth() const            { return m_iMilitaryWorth; }
	int getConquestProbability() const      { return m_iConquestProbability; }      // survive-conquest percent config
	int getVisibilityPriority() const       { return m_iVisibilityPriority; }
	int getAirlift() const                  { return m_iAirlift; }
	int getAirUnitCapacity() const          { return m_iAirUnitCapacity; }
	int getWorkableRadius() const           { return m_iWorkableRadius; }
	int getMaxPlayerInstancesExtra() const  { return m_iMaxPlayerInstancesExtra; }
	int getDcmAirbombMission() const        { return m_iDcmAirbombMission; }
	bool isCenterInCity() const             { return m_bCenterInCity; }
	bool isNotConstructible() const         { return m_bNotConstructible; }         // json §7: excluded from the queue
	bool isAutoBuild() const                { return m_bAutoBuild; }                // json §7: autoBuild ⊂ notConstructible
	bool isNoInstanceLimit() const          { return m_bNoInstanceLimit; }
	bool isAllowsNukes() const              { return m_bAllowsNukes; }
	bool isForceNoPrereqScaling() const     { return m_bForceNoPrereqScaling; }
	int getGreatPeopleUnitType() const      { return m_iGreatPeopleUnitType; }      // UNIT_* FK
	int getAdvisor() const                  { return m_iAdvisor; }                  // ADVISOR_* FK
	int getSpecialBuildingType() const      { return m_iSpecialBuildingType; }      // SPECIALBUILDING_* FK (group cap, json §4.4)
	int getFreeStartEra() const             { return m_iFreeStartEra; }             // ERA FK
	int getDiploVoteType() const            { return m_iDiploVoteType; }            // DIPLOVOTE_* FK
	int getReligion() const                 { return m_iReligion; }                 // RELIGION_* FK (the religious-building association)
	int getShrineReligion() const           { return m_iShrineReligion; }           // json §9 `shrine`: the religion this is the SHRINE of
	int getHeadquartersCorporation() const  { return m_iHeadquartersCorporation; }  // json §9 `headquarters`: the corp-HQ FK
	const std::vector<int>& getMapCategories() const        { return m_aiMapCategories; }         // MAPCATEGORY_* FKs
	const std::vector<int>& getEnabledCivilizations() const { return m_aiEnabledCivilizations; }  // CIVILIZATION_* FKs
	const std::map<int, int>& getVictoryThresholds() const  { return m_victoryThresholds; }       // VICTORY_* FK -> threshold
	// The two commerce-magnitude identity configs (×100 -- magnitudes, not counts).
	int getStateReligionCommerce(CommerceTypes eCommerce) const { return m_aiStateReligionCommerce[(int)eCommerce]; }
	// commerceDoubleTime is TURNS (a count, plain): the legacy CommerceChangeDoubleTime age-doubling threshold
	// (the deposit half is the §3 existedFor-conditioned second entry; this is the intrinsic display config).
	int getCommerceDoubleTime(CommerceTypes eCommerce) const { return m_aiCommerceDoubleTime[(int)eCommerce]; }

protected:
	virtual CvRequires*  mutRequires()     { return &m_requires; }
	virtual CvEdges*     mutEdges()        { return &m_edges; }
	virtual CvAllowed*   mutAllowed()      { return &m_allowed; }
	virtual CvTriggers*  mutTriggers()     { return &m_triggers; }
	virtual CvProvides*  mutProvides()     { return &m_provides; }
	virtual CvModifiers* mutModifiers()    { return &m_modifiers; }
	virtual CvModifiers* mutWhenObsolete() { return &m_whenObsolete; }
	virtual CvClassificationBlock* mutAttributes()   { return &m_attributes; }
	virtual CvClassificationBlock* mutCapabilities() { return &m_capabilities; }

private:
	// --- the composed section units (availability · provisions · effects · classification) ---
	CvRequires  m_requires;
	CvEdges     m_edges;
	CvAllowed   m_allowed;
	CvTriggers  m_triggers;
	CvProvides  m_provides;
	CvModifiers m_modifiers;
	CvModifiers m_whenObsolete;
	CvClassificationBlock m_attributes;
	CvClassificationBlock m_capabilities;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iWorth;
	int m_iMilitaryWorth;
	int m_iConquestProbability;
	int m_iVisibilityPriority;
	int m_iAirlift;
	int m_iAirUnitCapacity;
	int m_iWorkableRadius;
	int m_iMaxPlayerInstancesExtra;
	int m_iDcmAirbombMission;
	bool m_bCenterInCity;
	bool m_bNotConstructible;
	bool m_bAutoBuild;
	bool m_bNoInstanceLimit;
	bool m_bAllowsNukes;
	bool m_bForceNoPrereqScaling;
	int m_iGreatPeopleUnitType;
	int m_iAdvisor;
	int m_iSpecialBuildingType;
	int m_iFreeStartEra;
	int m_iDiploVoteType;
	int m_iReligion;
	int m_iShrineReligion;
	int m_iHeadquartersCorporation;
	std::vector<int> m_aiMapCategories;
	std::vector<int> m_aiEnabledCivilizations;
	std::map<int, int> m_victoryThresholds;
	int m_aiStateReligionCommerce[NUM_COMMERCE_TYPES];
	int m_aiCommerceDoubleTime[NUM_COMMERCE_TYPES];
};

#endif // CV_JSON_BUILDING_INFO_H
