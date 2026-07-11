#pragma once
#ifndef CV_JSON_PROMOTION_LINE_INFO_H
#define CV_JSON_PROMOTION_LINE_INFO_H

//
//	CvPromotionLineInfo -- the JSON real poco for PROMOTION LINES (an ordered promotion chain / grouping axis,
//	units-only). It is NOT a modifier source and enables nothing; its tech prereq rides the base as
//	tech.enables.promotionLines, and its game-option gates are the composed entity-level `enabled`/`disabled` gate
//	(CvJsonGate). The member promotions are a RUNTIME reverse index (rebuilt post-load from
//	CvPromotionInfo::getPromotionLine), NOT JSON data. No cascade here.
//
//	Live callers (verified 2026-07-07): isBuildUp / getNumPromotions / getPromotion / isNotOnDomainType -> CvUnit
//	build-up + promotion-line traversal.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // DomainTypes

class CvPromotionLineInfo : public CvInfo
{
public:
	CvPromotionLineInfo() : m_bBuildUp(false), m_eTechPrereq(NO_TECH), m_eObsoleteTech(NO_TECH) {}

	bool isBuildUp() const { return m_bBuildUp; }      // buildUp.active (the dedicated build-up module)
	bool isNotOnDomainType(int iDomain) const;         // identity.notOnDomains membership (parked applicability gate)

	// --- mirrored legacy CvPromotionLineInfo getters ---
	// tech FKs: curate_promotionline.py DROPs PrereqTech, store-inverting it onto tech.enables.promotionLines (a lone
	// tech -> no `requires`); ObsoleteTech is 0/unregistered but the store-invert path exists. Reconstructed at LOAD by
	// the cascadeLoadJson tech-FK reverse-index pass (CvCascadeReadJson.cpp enables/obsoletes.promotionLines), which
	// calls the setters below. Consumers: CvPlayer/CvUnit promotion-line availability gates.
	TechTypes getPrereqTech() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes e) { m_eTechPrereq = e; }       // load-time reverse-index writers (cascadeLoadJson)
	void setObsoleteTech(TechTypes e) { m_eObsoleteTech = e; }

	// entity-level game-option gate -> the flat NotOnGameOption list, walked out of the composed CvJsonGate `disabled`
	// condition tree in mapFrom (the GAMEOPTION_ presence-atom ids -- the exact CvPromotionInfo idiom).
	// curate_promotionline.py: NotOnGameOptions -> entity-level `disabled` (0 populated in shipped data; walk faithful).
	int getNotOnGameOption(int i) const { return (i >= 0 && i < (int)m_aiNotOnGameOptions.size()) ? m_aiNotOnGameOptions[i] : -1; }
	int getNumNotOnGameOptions() const { return (int)m_aiNotOnGameOptions.size(); }

	// unitcombat-prereq surface (consumed by CvPromotionInfo::getQualifiedUnitCombatType reconstruction):
	// UnitCombatPrereqTypes -> identity.unitCombats (unit-combats the line applies to); NotOnUnitCombatTypes ->
	// identity.notOnUnitCombats (excluded unit-combats). BOTH emitted by curate_promotionline.py (parked `identity`).
	int getNumUnitCombatPrereqTypes() const { return (int)m_aiUnitCombats.size(); }
	int getUnitCombatPrereqType(int i) const { return (i >= 0 && i < (int)m_aiUnitCombats.size()) ? m_aiUnitCombats[i] : -1; }
	int getNumNotOnUnitCombatTypes() const { return (int)m_aiNotOnUnitCombats.size(); }
	int getNotOnUnitCombatType(int i) const { return (i >= 0 && i < (int)m_aiNotOnUnitCombats.size()) ? m_aiNotOnUnitCombats[i] : -1; }

	// curator-gap: curate_promotionline.py DROPs UnitCombat/TechContractChanceChanges (dead unimplemented system --
	// ranking #27, 0 records; see the curator's DROP(fields) note). No JSON address -> safe defaults, not stubs-in-waiting.
	int getUnitCombatContractChanceChange(int /*iUnitCombat*/) const { return 0; }   // curator-gap: dead ContractChanceChange system
	int getTechContractChanceChange(int /*iTech*/) const { return 0; }               // curator-gap: dead ContractChanceChange system
	bool isTechContractChanceChange(int /*iTech*/) const { return false; }           // curator-gap: dead ContractChanceChange system

	// the member promotions -- a RUNTIME reverse index (rebuilt post-load from CvPromotionInfo::getPromotionLine),
	// NOT JSON-mapped. Populated by the reverse-index build; read by CvUnit's promotion-line traversal.
	int getNumPromotions() const { return (int)m_aiPromotions.size(); }
	int getPromotion(int i) const { return (i >= 0 && i < (int)m_aiPromotions.size()) ? m_aiPromotions[i] : -1; }
	void addPromotion(int iPromotion) { m_aiPromotions.push_back(iPromotion); }
	void clearPromotions() { m_aiPromotions.clear(); }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonGate* getGate() const { return &m_gate; }

protected:
	virtual CvJsonGate* mutGate() { return &m_gate; }

private:
	bool m_bBuildUp;                     // buildUp.active
	std::vector<int> m_aeNotOnDomains;   // identity.notOnDomains (DomainTypes ids) -- parked applicability gate
	std::vector<int> m_aiUnitCombats;        // identity.unitCombats (UnitCombatPrereqTypes) -- unit-combats the line applies to
	std::vector<int> m_aiNotOnUnitCombats;   // identity.notOnUnitCombats (NotOnUnitCombatTypes) -- excluded unit-combats
	std::vector<int> m_aiNotOnGameOptions;   // walked from m_gate.disabled in mapFrom (GAMEOPTION_ presence ids)
	std::vector<int> m_aiPromotions;     // RUNTIME reverse index (not JSON)
	CvJsonGate m_gate;                   // entity-level enabled/disabled (the game-option gates)
	TechTypes m_eTechPrereq;             // store-inverted tech.enables.promotionLines, reconstructed at load (cascadeLoadJson)
	TechTypes m_eObsoleteTech;           // store-inverted tech.obsoletes.promotionLines, reconstructed at load (cascadeLoadJson)
};

#endif // CV_JSON_PROMOTION_LINE_INFO_H
