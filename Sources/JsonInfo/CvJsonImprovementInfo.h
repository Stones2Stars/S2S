#pragma once
#ifndef CV_JSON_IMPROVEMENT_INFO_H
#define CV_JSON_IMPROVEMENT_INFO_H

//
//	CvJsonImprovementInfo -- the JSON real poco for tile IMPROVEMENTS (the CvXInfo replacement). Live-caller surface,
//	mapped from the curator's real shapes. The terrain/feature/irrigation VALIDITY prereqs live in `requires.build`
//	on the CvJsonInfo base (store-inverted by the curator, like routes) -- the cascade GENERATE gate reads that tree
//	directly, but the legacy placement/validity getters (isWaterImprovement/getTerrainMakesValid/...) ALSO walk it
//	on demand (below) so the EXE-shim consumer surface keeps working. HUMAN-native values (the cascade ×100s on its
//	side). No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange/getDefenseModifier -> CvPlot; getHealthPercent/getHappiness ->
//	CvCity; getCulture -> BuildsRepo; getUpgradeTime/getImprovementUpgrade -> upgrade chain; isActsAsCity/
//	isImprovementBonusTrade/isWaterImprovement/isMilitaryStructure -> worker+unit AI; getPillageGold -> pillage.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / ImprovementTypes / BonusTypes / MapCategoryTypes / NO_*
#include <vector>
#include <map>

class CvJsonImprovementInfo : public CvJsonInfo
{
public:
	CvJsonImprovementInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int* getYieldChangeArray() const { return const_cast<int*>(m_aiYieldChange); }   // real member row (base plot yield)
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
	int getGoodyUniqueRange() const { return m_iGoodyUniqueRange; }   // mapGeneration.goodyRange
	int getTilesPerGoody() const { return m_iTilesPerGoody; }        // mapGeneration.tilesPerGoody
	int getSeeFrom() const { return m_iSeeFrom; }                    // vision.plot.seeFrom.flat
	int getVisibilityChange() const { return m_iVisibilityChange; }  // vision.plot.visibilityRange.flat
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }// identity.advancedStart.cost
	// sound.soundscape -> the runtime audio-manager index, resolved at info-load (mapFrom) via the SAME
	// gDLL->getAudioTagIndex(tag, AUDIOTAG_SOUNDSCAPE) the archived CvImprovementInfo::read used. -1 == "no sound"
	// (legacy's absent-tag read default) for the improvements that author no soundscape.
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }
	bool isUpgradeRequiresFortify() const { return m_bUpgradeRequiresFortify; }   // identity.upgradeRequiresFortify
	// EXE-bound surface (mapscript/EXE map gen -- served by the CvImprovementInfo shim leaf, cascade-engine-430.md §3)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	bool isGoody() const { return m_bGoody; }
	bool isRequiresRiverSide() const { return m_bRequiresRiverSide; }

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
	bool isPlacesBonus() const { return m_bPlacesBonus; }         // identity.placesBonus (placement-transform outcome; KEPT on identity)
	bool isPlacesFeature() const { return m_bPlacesFeature; }     // identity.placesFeature
	bool isPlacesTerrain() const { return m_bPlacesTerrain; }     // identity.placesTerrain
	bool isChangeRemove() const { return m_bChangeRemove; }       // identity.changeRemove
	bool isNational() const { return false; }                    // CURATOR-GAP (verified): bNational authored by 0 improvements (grep CIV4ImprovementInfos.xml)
	bool isGlobal() const { return false; }                      // CURATOR-GAP (verified): bGlobal  authored by 0 improvements (grep CIV4ImprovementInfos.xml)

	// --- placement/validity predicates -- PLACEMENT DOMAIN (curate_improvement.py requires_fn), real data walked
	// from `requires.build` (the CvJsonCondition tree the base already parses via mutRequires()/CJK_REQUIRES
	// dispatch). A length-1 "MakesValid" OR-alternative collapses to a bare token sitting DIRECTLY in `all` --
	// indistinguishable by shape alone from the true AND-mandatory token of the same name (HAS_PEAK, HAS_RIVER); the
	// .cpp tree-walk disambiguates these two using a direct-vs-nested occurrence count, verified against the live
	// CIV4ImprovementInfos.xml (every record where the OR-side flag is set, the AND-side flag is ALSO set, so the
	// occurrence count alone recovers both booleans correctly for every real record -- see .cpp for the citations).
	TechTypes getPrereqTech() const;              // requires.build's own PrereqTech PRESENCE atom (TECH_* id)
	bool isRequiresFeature() const;
	bool isRequiresFlatlands() const;
	bool isRequiresIrrigation() const;
	bool isWaterImprovement() const;
	bool isCanMoveSeaUnits() const;                // IS_LAND && HAS_COAST (coastal-land domain)
	bool isPeakImprovement() const;
	bool isNoFreshWater() const;                   // requires.build.noneOf HAS_FRESHWATER

	bool isHillsMakesValid() const;
	bool isFreshWaterMakesValid() const;
	bool isRiverSideMakesValid() const;
	bool isPeakMakesValid() const;
	bool getTerrainMakesValid(int i) const;        // requires.build.any {terrain:[...]} membership
	bool getFeatureMakesValid(int i) const;        // requires.build.any {feature:[...]} membership
	bool isImprovementBonusMakesValid(int i) const;// requires.build.any {bonus:[...]} membership (PRESENCE atom)

	int getImprovementBonusDiscoverRand(int i) const { return mapGet(m_bonusDiscoverRand, i); }   // identity.bonuses.{BONUS}.discoverRand
	int getImprovementBonusDepletionRand(int i) const { return mapGet(m_bonusDepletionRand, i); }  // identity.bonuses.{BONUS}.depletionRand

	int getAlternativeImprovementUpgradeType(int i) const
	{ return (i >= 0 && i < (int)m_aiAlternativeImprovementUpgradeTypes.size()) ? m_aiAlternativeImprovementUpgradeTypes[i] : -1; }
	int getNumAlternativeImprovementUpgradeTypes() const { return (int)m_aiAlternativeImprovementUpgradeTypes.size(); }
	bool isAlternativeImprovementUpgradeType(int i) const;   // membership test (i = an IMPROVEMENT_ engine id, not an index)

	// FeatureChangeTypes -> identity.featureChanges (placement-transform outcome list; KEPT on identity by the curator).
	// getFeatureChangeType(i) indexes the list (i = index); isFeatureChangeType(i) is a membership test (i = a FEATURE_ id).
	int getFeatureChangeType(int i) const
	{ return (i >= 0 && i < (int)m_aiFeatureChangeTypes.size()) ? m_aiFeatureChangeTypes[i] : -1; }
	int getNumFeatureChangeTypes() const { return (int)m_aiFeatureChangeTypes.size(); }
	bool isFeatureChangeType(int i) const;

	// CURATOR-GAP (by design): Categories are dropped by the curator (curate_improvement.py EXTRA_DROP: "Categories
	// ... -> drop") -- no source. Always-empty surface (num 0; getCategory never validly indexed -> -1; isCategory false).
	int getCategory(int /*i*/) const { return -1; }
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	// Conditional plot-yield deposits -- read from the yield families' `flat` ARRAYS (food/production/commerce ->
	// plot.flat), where the curator folds base + condition-gated bumps together (curate_improvement.py post_process
	// _inject): a bare number is the base YieldChange; a {value,enabled} entry is a conditional whose gate selects the
	// legacy getter -- "HAS_IRRIGATION"->Irrigated, "HAS_RIVER"->RiverSide, {HAS_BONUS:B}->ImprovementBonusYield,
	// {type:TECH,scope:team}->TechYieldChanges. mapFrom's readConditionalYields extracts all of them in one pass
	// (which also FIXES the base getYieldChange -- jsonFamVal reads only a bare scalar, so it returned 0 for arrays).
	int getRiverSideYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiRiverSideYieldChange[i] : 0; }
	int getIrrigatedYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiIrrigatedYieldChange[i] : 0; }
	int getImprovementBonusYield(int i, int j) const;   // per-bonus (i = BONUS id, j = yield); {HAS_BONUS:B}-gated entry
	int getTechYieldChanges(int i, int j) const;        // per-tech  (i = TECH id,  j = yield); {type:TECH,scope:team}-gated
	int* getRiverSideYieldChangeArray() const { return const_cast<int*>(m_aiRiverSideYieldChange); }
	int* getIrrigatedYieldChangeArray() const { return const_cast<int*>(m_aiIrrigatedYieldChange); }
	int* getTechYieldChangesArray(int i) const;         // row for TECH i, or NULL when it deposits none (legacy NULL semantics)
	// PrereqNatureYield -- the placement `{natureYield:{food:..}}` min-threshold atom in requires.build.all. The shared
	// condition parser drops it (CvJsonConditionParse.cpp has no natureYield case -> CASC_PRED_UNKNOWN, value lost), so
	// mapFrom reads it DIRECTLY from the raw requires.build.all JSON (readPrereqNatureYield).
	int getPrereqNatureYield(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiPrereqNatureYield[i] : 0; }
	// CURATOR-GAP (verified, by design): RouteYieldChanges is in curate_improvement.py EXTRA_DROP -- it "stays folded
	// onto the ROUTE" (the route governs which improvements it upgrades; curate_route). ZERO route-yield data is
	// emitted on the improvement side (grep: 0 "ROUTE_" in Assets/Data/improvements), so it cannot be served here.
	int getRouteYieldChanges(int /*i*/, int /*j*/) const { return 0; }
	int* getRouteYieldChangesArray(int /*i*/) const { return NULL; }   // legacy NULL-when-no-row semantics
	// CURATOR-GAP (verified): bObsoleteBonusMakesValid and bNotOnAnyBonus are authored by ZERO improvements (grep
	// CIV4ImprovementInfos.xml -> 0 matches each), so nothing is emitted and there is no data to serve.
	bool isImprovementObsoleteBonusMakesValid(int /*i*/) const { return false; }
	bool isNotOnAnyBonus() const { return false; }
	// CURATOR-GAP (by design): getBuildTypes is a RUNTIME cross-reference cache, not a curated field -- the legacy
	// doPostLoadCaching scans every BuildInfo for getImprovement()==this. Never authored on the improvement JSON;
	// rebuilding it needs the Build repo. Empty vector until that cache is rebuilt engine-side.
	const std::vector<BuildTypes>& getBuildTypes() const { return m_aeBuildTypes; }
	// CURATOR-GAP (by design): the improvement's PropertyManipulators are RELOCATED to grants.repeatable
	// (curate_improvement.py post_process; owner 2026-07-01 "property pulses are repeatable grants"), so the legacy
	// CvPropertyManipulators OBJECT is intentionally unpopulated here -- consumers read the pulse via getGrants().
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

	virtual void mapFrom(const picojson::value& entity);

private:
	// mapFrom helpers (defined in the .cpp where the full picojson type is available):
	void readConditionalYields(const picojson::value& entity);   // base + gated deposits from the yield-family `flat` arrays
	void readPrereqNatureYield(const picojson::value& entity);   // requires.build.all {natureYield:{...}} thresholds (raw JSON)

public:
	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonRequires*  getRequires()  const { return &m_requires; }
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonRequires*  mutRequires()  { return &m_requires; }
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	static int mapGet(const std::map<int, int>& m, int k) { std::map<int, int>::const_iterator it = m.find(k); return it != m.end() ? it->second : 0; }

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
	bool m_bGoody;                         // mapGeneration.goody (EXE-bound isGoody)
	bool m_bRequiresRiverSide;             // mapGeneration.requiresRiverSide (EXE-bound isRequiresRiverSide; also a HAS_RIVER requires.build predicate)
	std::string m_szArtDefineTag;          // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	int m_iWorldSoundscapeScriptId;        // sound.soundscape -> audio-manager index (AUDIOTAG_SOUNDSCAPE); -1 when absent (legacy read default)
	std::vector<MapCategoryTypes> m_aeMapCategories;

	int m_iGoodyUniqueRange;               // mapGeneration.goodyRange
	int m_iTilesPerGoody;                  // mapGeneration.tilesPerGoody
	int m_iSeeFrom;                        // vision.plot.seeFrom.flat
	int m_iVisibilityChange;               // vision.plot.visibilityRange.flat
	int m_iAdvancedStartCost;              // identity.advancedStart.cost
	bool m_bUpgradeRequiresFortify;        // identity.upgradeRequiresFortify
	bool m_bPlacesBonus, m_bPlacesFeature, m_bPlacesTerrain, m_bChangeRemove;   // identity placement-transform flags
	std::vector<int> m_aiAlternativeImprovementUpgradeTypes;   // identity.alternativeUpgrades (FK-resolved IMPROVEMENT_ ids)
	std::vector<int> m_aiFeatureChangeTypes;                   // identity.featureChanges (FK-resolved FEATURE_ ids)
	std::map<int, int> m_bonusDiscoverRand;    // identity.bonuses.{BONUS}.discoverRand
	std::map<int, int> m_bonusDepletionRand;   // identity.bonuses.{BONUS}.depletionRand

	int m_aiRiverSideYieldChange[NUM_YIELD_TYPES];   // <yield>.plot.flat "HAS_RIVER"-gated entries (RiverSideYieldChange)
	int m_aiIrrigatedYieldChange[NUM_YIELD_TYPES];   // <yield>.plot.flat "HAS_IRRIGATION"-gated entries (IrrigatedYieldChange)
	int m_aiPrereqNatureYield[NUM_YIELD_TYPES];      // requires.build.all {natureYield:{...}} min thresholds
	std::map<int, std::vector<int> > m_techYieldChanges;   // TECH id -> NUM_YIELD_TYPES row ({type:TECH,scope:team}-gated)
	std::map<int, std::vector<int> > m_bonusYieldChanges;  // BONUS id -> NUM_YIELD_TYPES row ({HAS_BONUS:B}-gated)

	std::vector<BuildTypes> m_aeBuildTypes;          // CURATOR-GAP always empty -- see getBuildTypes
	CvPropertyManipulators m_PropertyManipulators;   // CURATOR-GAP always empty (relocated to grants.repeatable) -- see getPropertyManipulators
};

#endif // CV_JSON_IMPROVEMENT_INFO_H
