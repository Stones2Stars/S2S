#pragma once
#ifndef CV_JSON_PROMOTION_LINE_INFO_H
#define CV_JSON_PROMOTION_LINE_INFO_H

//
//	CvPromotionLineInfo -- the PROMOTION-LINE poco (an ordered promotion chain / grouping axis, units-only)
//	rebuilt to the exemplar surface (patterns.md par. THE GETTER SETUP). It is NOT a modifier source and enables
//	nothing; its tech prereq rides the base as tech.enables.promotionLines (the store-inverted FK reconstructed
//	at load by the loadJson reverse-index pass via the setters below), its game-option gates are the composed
//	entity-level `enabled`/`disabled` gate SERVED WHOLE (getGate -- no flattened option-list mirror survives,
//	[DEC-new-getter-surface]), and the par.9 `buildUp` module is a bare typed read. The member promotions are a
//	RUNTIME reverse index (rebuilt post-load from CvPromotionInfo::getPromotionLine), NOT JSON data.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // TechTypes / NO_TECH

class CvPromotionLineInfo : public CvInfo
{
public:
	CvPromotionLineInfo() : m_bBuildUp(false), m_eTechPrereq(NO_TECH), m_eObsoleteTech(NO_TECH) {}

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvGate* getGate() const { return &m_gate; }

	// ======================= 4. INTRINSIC -- bare typed reads (the census identity set) =======================
	bool isBuildUp() const { return m_bBuildUp; }      // buildUp.active (the dedicated build-up module, par.9)
	// identity.unitCombats -- the unit-combats the line applies to (feeds the promotion's qualified-set build).
	const std::vector<int>& getUnitCombats() const { return m_aiUnitCombats; }
	// identity.notOnUnitCombats -- the excluded unit-combats.
	const std::vector<int>& getNotOnUnitCombats() const { return m_aiNotOnUnitCombats; }
	// identity.notOnDomains -- the excluded domains (parked applicability gate).
	const std::vector<int>& getNotOnDomains() const { return m_aiNotOnDomains; }
	bool isNotOnDomain(int iDomain) const;

	// --- the store-inverted tech FKs: curate_promotionline.py DROPs PrereqTech, store-inverting it onto
	// tech.enables.promotionLines (a lone tech -> no `requires`); ObsoleteTech rides tech.obsoletes. Both
	// reconstructed at LOAD by the loadJson tech-FK reverse-index pass, which calls the setters (the
	// write-once-at-load window). ---
	TechTypes getPrereqTech() const { return m_eTechPrereq; }
	TechTypes getObsoleteTech() const { return m_eObsoleteTech; }
	void setTechPrereq(TechTypes eTech) { m_eTechPrereq = eTech; }
	void setObsoleteTech(TechTypes eTech) { m_eObsoleteTech = eTech; }

	// --- the member promotions: a RUNTIME reverse index (rebuilt post-load from
	// CvPromotionInfo::getPromotionLine), part of the load window -- never JSON-mapped. ---
	int getNumPromotions() const { return (int)m_aiPromotions.size(); }
	int getPromotion(int iIndex) const { return (iIndex >= 0 && iIndex < (int)m_aiPromotions.size()) ? m_aiPromotions[iIndex] : -1; }
	void addPromotion(int iPromotion) { m_aiPromotions.push_back(iPromotion); }
	void clearPromotions() { m_aiPromotions.clear(); }

protected:
	virtual CvGate* mutGate() { return &m_gate; }

private:
	// --- the composed section units ---
	CvGate m_gate;                       // entity-level enabled/disabled (the game-option gates)

	// --- the intrinsic identity members (materialized once at mapFrom) ---
	bool m_bBuildUp;                     // buildUp.active
	std::vector<int> m_aiUnitCombats;        // identity.unitCombats
	std::vector<int> m_aiNotOnUnitCombats;   // identity.notOnUnitCombats
	std::vector<int> m_aiNotOnDomains;       // identity.notOnDomains (DomainTypes ids)

	// --- load-window runtime members (see the section comments above) ---
	std::vector<int> m_aiPromotions;     // RUNTIME reverse index (not JSON)
	TechTypes m_eTechPrereq;             // store-inverted tech.enables.promotionLines
	TechTypes m_eObsoleteTech;           // store-inverted tech.obsoletes.promotionLines
};

#endif // CV_JSON_PROMOTION_LINE_INFO_H
