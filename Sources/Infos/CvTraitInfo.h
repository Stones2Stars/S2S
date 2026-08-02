#pragma once
#ifndef CV_JSON_TRAIT_INFO_H
#define CV_JSON_TRAIT_INFO_H

//
//	CvTraitInfo -- the TRAIT poco rebuilt to the full exemplar surface (patterns.md § THE GETTER SETUP: the four
//	read categories, nothing else); base of CvSimpleTraitInfo / CvComplexTraitInfo (the two option-selected sets
//	live in SEPARATE repos -- modifier.md §4: a consumer picks the ACTIVE set via MMKernel::traitData, never a
//	runtime info swap). Styled for the JSON anatomy (json.md §2); no legacy getter name returns
//	([DEC-new-getter-surface]).
//

#include "CvInfo.h"

class CvTraitInfo : public CvInfo
{
public:
	CvTraitInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }
	virtual const CvClassificationBlock* getPolicies()  const { return &m_policies; }

	// ======================= 2. CLASSIFICATION -- O(1) bitset tests, hold-vs-provide in the NAME (json §9) ====
	// A trait PROVIDES a policy to the empire while held (the grantor direction, exactly the civic's).
	bool providesPolicies() const            { return !m_policies.isEmpty(); }

	// ======================= 3. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. NB the PURE
	// TRAITS sign filter and the simple/complex set selection are CONSUMER-side reads over the active set --
	// MMKernel::traitData selects the set, and the gather drops off-alignment values per entry (CascadeGather's
	// iPureSign) -- never baked into these compiled sums.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getYieldModifier(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT); }
	int getCommerceModifier(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT); }
	// The ledgered PERMANENT golden-age member-mirror (modifier.md §3): the channel families' goldenAge kind.
	int getGoldenAgeYield(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(infoYieldFamily(eYield), CHANNEL_GOLDEN_AGE, eScope, CASC_UNIT_FLAT); }
	int getGoldenAgeCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return m_modifiers.sum(infoCommerceFamily(eCommerce), CHANNEL_GOLDEN_AGE, eScope, CASC_UNIT_FLAT); }
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
	int getDurations(DurationsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DURATIONS, eKind, eScope, CASC_UNIT_PERCENT); }
	int getAir(AirKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_AIR, eKind, eScope, infoKindUnit(MODFAM_AIR, eKind)); }
	int getCargo(CargoKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_CARGO, eKind, eScope, CASC_UNIT_FLAT); }
	int getCostsModifier(CostsKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_COSTS, eKind, eScope, CASC_UNIT_PERCENT); }
	// (researchRate / improvementUpgradeRate / workRate / conscript / greatGeneralRate / greatPeopleRate /
	// growth / goldenAge / espionageDefense / hurry / range are the 1-2-kind stragglers: the base
	// getScalar(SCALAR_*) covers them. Keyed targets stay entry-list reads.)
	// The threshold families are MIN-SELECTED ACROSS SOURCES, and the selection is the PLAYER's: each trait
	// serves only its OWN authored value here, and `CvPlayer::updateExtraYieldThreshold` picks the smallest
	// positive one over the held traits. The two levels are distinct -- a compiled point read cannot serve the
	// player's realized threshold, but it is exactly what the selection reads per candidate.
	int getExtraYieldThreshold(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_EXTRA_YIELD_THRESHOLD, (int)eYield, eScope, CASC_UNIT_FLAT); }
	int getLessYieldThreshold(YieldTypes eYield, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_LESS_YIELD_THRESHOLD, (int)eYield, eScope, CASC_UNIT_FLAT); }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) ======================
	// The pure-traits alignment (the MMKernel PureFilter's source-sign input) + the trait-option curation flags.
	bool isNegativeTrait() const              { return m_bNegativeTrait; }
	bool isBarbarianSelectionOnly() const     { return m_bBarbarianSelectionOnly; }
	bool isImpurePropertyManipulators() const { return m_bImpurePropertyManipulators; }
	bool isImpurePromotions() const           { return m_bImpurePromotions; }
	int getMinAnarchy() const                 { return m_iMinAnarchy; }   // anarchy-turn clamp (plain counts)
	int getMaxAnarchy() const                 { return m_iMaxAnarchy; }
	const CvWString& getShortDescriptionKey() const { return m_szShortDescriptionKey; }   // TXT_KEY

	// json.md par.9 `succession` -- the promotion-LINE this trait belongs to and its ordering rank within it.
	// The line's own levels advance by gameplay progression, never by tech: the HELD trait is the authoritative
	// level (modifier.md par.4), so these are pure ordering data, never a gate.
	int getSuccessionPromotionLine() const { return m_iSuccessionPromotionLine; }
	int getSuccessionPriority() const      { return m_iSuccessionPriority; }
	// json.md par.9 `excludes` -- the same-tier traits this one is mutually exclusive with.
	const std::vector<int>& getExcludes() const { return m_aiExcludes; }

	// The KEEP-legacy property engine's per-turn SOURCES, bridged from this trait's PROPERTY_* families
	// (property-audit.md). Player-gathered and fanned to every owner city -- RELATION_ASSOCIATED.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }
	virtual CvClassificationBlock* mutPolicies()  { return &m_policies; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;
	CvClassificationBlock m_policies;

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	bool m_bNegativeTrait;
	bool m_bBarbarianSelectionOnly;
	bool m_bImpurePropertyManipulators;
	bool m_bImpurePromotions;
	int m_iMinAnarchy;
	int m_iMaxAnarchy;
	CvWString m_szShortDescriptionKey;
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
	int m_iSuccessionPromotionLine;
	int m_iSuccessionPriority;
	std::vector<int> m_aiExcludes;
};

#endif // CV_JSON_TRAIT_INFO_H
