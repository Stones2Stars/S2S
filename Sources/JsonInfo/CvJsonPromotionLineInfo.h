#pragma once
#ifndef CV_JSON_PROMOTION_LINE_INFO_H
#define CV_JSON_PROMOTION_LINE_INFO_H

//
//	CvJsonPromotionLineInfo -- the JSON real poco for PROMOTION LINES (an ordered promotion chain / grouping axis,
//	units-only). It is NOT a modifier source and enables nothing; its tech prereq rides the base as
//	tech.enables.promotionLines, and its game-option gates are the composed entity-level `enabled`/`disabled` gate
//	(CvJsonGate). The member promotions are a RUNTIME reverse index (rebuilt post-load from
//	CvPromotionInfo::getPromotionLine), NOT JSON data. No cascade here.
//
//	Live callers (verified 2026-07-07): isBuildUp / getNumPromotions / getPromotion / isNotOnDomainType -> CvUnit
//	build-up + promotion-line traversal.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // DomainTypes

class CvJsonPromotionLineInfo : public CvJsonInfo
{
public:
	CvJsonPromotionLineInfo() : m_bBuildUp(false) {}

	bool isBuildUp() const { return m_bBuildUp; }      // buildUp.active (the dedicated build-up module)
	bool isNotOnDomainType(int iDomain) const;         // identity.notOnDomains membership (parked applicability gate)

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
	std::vector<int> m_aiPromotions;     // RUNTIME reverse index (not JSON)
	CvJsonGate m_gate;                   // entity-level enabled/disabled (the game-option gates)
};

#endif // CV_JSON_PROMOTION_LINE_INFO_H
