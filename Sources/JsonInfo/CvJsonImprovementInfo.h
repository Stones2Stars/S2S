#pragma once
#ifndef CV_JSON_IMPROVEMENT_INFO_H
#define CV_JSON_IMPROVEMENT_INFO_H

//
//	CvJsonImprovementInfo -- the JSON real poco for tile IMPROVEMENTS (the CvXInfo replacement). Live-caller surface,
//	mapped from the curator's real shapes. NB the terrain/feature/irrigation VALIDITY prereqs live in `requires.build`
//	on the CvJsonInfo base (store-inverted by the curator, like routes) -- the cascade GENERATE gate reads them; they
//	are NOT poco getters. HUMAN-native values (the cascade ×100s on its side). No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange/getDefenseModifier -> CvPlot; getHealthPercent/getHappiness ->
//	CvCity; getCulture -> BuildsRepo; getUpgradeTime/getImprovementUpgrade -> upgrade chain; isActsAsCity/
//	isImprovementBonusTrade/isWaterImprovement/isMilitaryStructure -> worker+unit AI; getPillageGold -> pillage.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / ImprovementTypes / BonusTypes / MapCategoryTypes / NO_*
#include <vector>

class CvJsonImprovementInfo : public CvJsonInfo
{
public:
	CvJsonImprovementInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getDefenseModifier() const { return m_iDefenseModifier; }
	int getAirBombDefense() const { return m_iAirBombDefense; }
	int getHealthPercent() const { return m_iHealthPercent; }
	int getHappiness() const { return m_iHappiness; }
	int getCulture() const { return m_iCulture; }
	int getPillageGold() const { return m_iPillageGold; }
	int getUniqueRange() const { return m_iUniqueRange; }
	int getCultureRange() const { return m_iCultureRange; }
	int getFeatureGrowthProbability() const { return m_iFeatureGrowthProbability; }
	int getUpgradeTime() const { return m_iUpgradeTime; }

	ImprovementTypes getImprovementUpgrade() const { return m_eImprovementUpgrade; }
	ImprovementTypes getImprovementPillage() const { return m_eImprovementPillage; }
	BonusTypes getBonusChange() const { return m_eBonusChange; }

	bool isActsAsCity() const { return m_bActsAsCity; }
	bool isMilitaryStructure() const { return m_bMilitaryStructure; }
	bool isCarriesIrrigation() const { return m_bCarriesIrrigation; }   // KEPT in identity (propagation is live code)
	bool isOutsideBorders() const { return m_bOutsideBorders; }
	bool isBombardable() const { return m_bBombardable; }
	bool isZOCSource() const { return m_bZOCSource; }
	bool isExtraterresial() const { return m_bExtraterrestrial; }
	bool isImprovementBonusTrade(int /*iBonus*/ = -1) const { return m_bUniversalBonusTrade; }
	// NB isWaterImprovement / isRequiresIrrigation / isPeakImprovement / isRequiresFlatlands are NOT here: those are
	// PLACEMENT DOMAIN moved into requires.build (IS_WATER / HAS_IRRIGATION / HAS_PEAK / IS_FLATLANDS,
	// curate_improvement.py requires_fn) -- the cascade GENERATE gate reads them, not a poco getter.

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()  const { return &m_requires; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonRequires*  mutRequires()  { return &m_requires; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonRequires  m_requires;
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];  // food/production/commerce .plot.flat
	int m_iDefenseModifier;                // defense.plot.amount.percent
	int m_iAirBombDefense;                 // defense.plot.air.flat (data-migration ruling)
	int m_iHealthPercent;                  // BALANCE-CUT: iHealthPercent dropped from improvements (curate_improvement.py:59) -> always 0
	int m_iHappiness;                      // identity.happiness (leftover -> identity)
	int m_iCulture;                        // culture.plot.flat (Super Forts plot culture)
	int m_iPillageGold;                    // identity.pillageGold
	int m_iUniqueRange;                    // mapGeneration.uniqueRange
	int m_iCultureRange;                   // identity.cultureRange (stays identity)
	int m_iFeatureGrowthProbability;       // identity.featureGrowth (stays identity)
	int m_iUpgradeTime;                    // identity.upgradeTime
	ImprovementTypes m_eImprovementUpgrade;// identity.upgradesTo (FK)
	ImprovementTypes m_eImprovementPillage;// identity.pillageTo (FK)
	BonusTypes m_eBonusChange;             // identity.bonusChange (FK)
	bool m_bActsAsCity, m_bMilitaryStructure, m_bCarriesIrrigation;
	bool m_bOutsideBorders, m_bBombardable, m_bZOCSource, m_bExtraterrestrial, m_bUniversalBonusTrade;
	std::vector<MapCategoryTypes> m_aeMapCategories;
	// ⏳ NOT yet mapped (their keyed curator shapes): getRiverSideYieldChange / getIrrigatedYieldChange (HAS_RIVER/
	//    HAS_IRRIGATION-gated conditioned deposits) / getTechYieldChanges (tech-gated own yields) / per-bonus yields /
	//    getVisibilityChange+getSeeFrom (vision block) / the alternative-upgrade + feature-change lists.
};

#endif // CV_JSON_IMPROVEMENT_INFO_H
