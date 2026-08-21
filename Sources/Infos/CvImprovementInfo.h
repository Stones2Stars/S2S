#pragma once
#ifndef CV_JSON_IMPROVEMENT_INFO_H
#define CV_JSON_IMPROVEMENT_INFO_H

//
//	CvImprovementInfo -- the IMPROVEMENT poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the improvement's OWN tile output (modifier.md §5 plot-substrate
//	own-output -- base flats PLUS every conditioned entry: HAS_IRRIGATION / HAS_RIVER / HAS_BONUS / tech-gated,
//	and the reverse-landed building/civic/tech conditioned entries) lives on the compiled modifier surface; the
//	point reads fetch the unconditioned sums, the conditioned tail is the base conditioned-list access. The
//	placement/validity verdicts are MATERIALIZED at mapFrom from the composed requires.build tree
//	(docs/architecture/patterns.md §Materialize at mapFrom -- the getters are bare member reads, never per-call tree walks). The upgrade
//	chain and the identity.bonuses discovery/depletion/trade data are genuine bespoke build/upgrade data. No
//	legacy-mirror modifier member survives (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//

#include "CvInfo.h"
#include "CvClassificationBlock.h"   // the §8 held-boolean block + CLS_HAS
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / ImprovementTypes / BonusTypes / MapCategoryTypes / BuildTypes
#include <vector>
#include <map>
#include <set>

class CvArtInfoImprovement;

class CvImprovementInfo : public CvInfo
{
public:
	CvImprovementInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvRequires*  getRequires()  const { return &m_requires; }
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: food/production/commerce plot flats (own tile output); culture.plot.flat -- the Super
	// Forts plot culture, a CommerceTypes channel; defense.plot.{amount.percent, air.flat}. The conditioned
	// yield entries -- irrigated / riverside / per-bonus / tech-gated + the reverse-landed cross-entity
	// boosts -- are the compiled conditioned list, walked by the plot package rebuild and the pedia.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return flatWithFans(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope); }
	int getFlatCommerce(CommerceTypes eCommerce, CvCascScope eScope) const
	{ return flatWithFans(infoCommerceFamily(eCommerce), CHANNEL_AMOUNT, eScope); }
	// Scope-aware unit axis: DEFENSE_AIR is the one scope-split defense kind (plot = the FLAT rolled air-defense
	// magnitude the improvements author; city = the PERCENT damage modifier) -- the scope-blind overload would
	// answer the city plane for the plot air read.
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind, eScope)); }
	// How high this improvement stands whoever is on it ([vision.md] §5: an improvement's see-from IS its
	// elevation). POSITIONAL -- it belongs to the plot, never to the observer. Engine-native, so a reader
	// wanting PLOTS divides by VISION_OPEN_GROUND_COST at its use.
	int getFlatVision(VisionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_VISION, eKind, eScope, CASC_UNIT_FLAT); }

	// ======================= 3. INTRINSIC -- bare typed reads (the census identity set) ======================
	int getPillageGold() const { return m_iPillageGold; }                       // identity.pillageGold
	int getCultureRange() const { return m_iCultureRange; }                     // identity.cultureRange
	int getFeatureGrowthProbability() const { return m_iFeatureGrowthProbability; } // identity.featureGrowth
	int getUpgradeTime() const { return m_iUpgradeTime; }                       // identity.upgradeTime
	int getAdvancedStartCost() const { return m_iAdvancedStartCost; }           // identity.advancedStart.cost
	bool isUpgradeRequiresFortify() const { return m_bUpgradeRequiresFortify; } // identity.upgradeRequiresFortify
	// sound.soundscape -> the runtime audio-manager index, resolved at info-load via gDLL->getAudioTagIndex;
	// -1 == "no sound" (the legacy absent-tag read default).
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }
	// mapGeneration
	int getUniqueRange() const { return m_iUniqueRange; }         // mapGeneration.uniqueRange
	int getGoodyUniqueRange() const { return m_iGoodyUniqueRange; } // mapGeneration.goodyRange
	int getTilesPerGoody() const { return m_iTilesPerGoody; }     // mapGeneration.tilesPerGoody
	// EXE-bound surface (mapscript/EXE map gen)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport bool isGoody() const { return m_bGoody; }
	DllExport bool isRequiresRiverSide() const { return m_bRequiresRiverSide; }
	DllExport const CvArtInfoImprovement* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton)

	// --- the UPGRADE CHAIN + placement-transform outcomes (genuine bespoke build/upgrade data) ---
	ImprovementTypes getImprovementUpgrade() const { return m_eImprovementUpgrade; }   // identity.upgradesTo (FK)
	ImprovementTypes getImprovementPillage() const { return m_eImprovementPillage; }   // identity.pillageTo (FK)
	BonusTypes getBonusChange() const { return m_eBonusChange; }                       // identity.bonusChange (FK)
	const std::vector<int>& getAlternativeImprovementUpgradeTypes() const { return m_aiAlternativeImprovementUpgradeTypes; }
	bool isAlternativeImprovementUpgradeType(int iImprovement) const;   // membership (an IMPROVEMENT_ engine id)
	const std::vector<int>& getFeatureChangeTypes() const { return m_aiFeatureChangeTypes; }
	bool isFeatureChangeType(int iFeature) const;                       // membership (a FEATURE_ engine id)

	virtual const CvClassificationBlock* getCharacteristics() const { return &m_characteristics; }

	bool isMilitaryStructure() const { return m_bMilitaryStructure; }
	bool isCarriesIrrigation() const { return m_bCarriesIrrigation; }
	bool isOutsideBorders() const { return m_bOutsideBorders; }
	bool isExtraterresial() const { return m_bExtraterrestrial; }
	bool isPlacesBonus() const { return m_bPlacesBonus; }         // identity placement-transform outcome flags
	bool isPlacesFeature() const { return m_bPlacesFeature; }
	bool isPlacesTerrain() const { return m_bPlacesTerrain; }
	bool isChangeRemove() const { return m_bChangeRemove; }
	// OR the universal provider flag with the per-bonus `identity.bonuses.{BONUS}.trade` set. Feeds CvGlobals'
	// getProvidedByImprovementType / getTradeProvidingImprovements reverse-scans, so it must be per-bonus.
	bool isImprovementBonusTrade(int iBonus = -1) const
	{ return m_bUniversalBonusTrade || (iBonus >= 0 && m_bonusTradeIds.find(iBonus) != m_bonusTradeIds.end()); }
	int getImprovementBonusDiscoverRand(int iBonus) const { return mapGet(m_bonusDiscoverRand, iBonus); }  // identity.bonuses.{B}.discoverRand
	int getImprovementBonusDepletionRand(int iBonus) const { return mapGet(m_bonusDepletionRand, iBonus); } // identity.bonuses.{B}.depletionRand

	// --- the PLACEMENT/VALIDITY verdicts -- MATERIALIZED at mapFrom from the composed requires.build tree
	// (curate_improvement.py requires_fn store-inverts the legacy placement fields into requires.build; the
	// one walk in mapFrom recovers them into typed members, disambiguating the collapsed length-1 MakesValid
	// OR-alternative by the direct-vs-nested occurrence count -- see the .cpp). Getters are bare reads. ---
	TechTypes getPrereqTech() const { return m_ePrereqTech; }
	bool isRequiresFeature() const { return m_bRequiresFeature; }
	bool isRequiresFlatlands() const { return m_bRequiresFlatlands; }
	bool isRequiresIrrigation() const { return m_bRequiresIrrigation; }
	bool isWaterImprovement() const { return m_bWaterImprovement; }
	bool isCanMoveSeaUnits() const { return m_bCanMoveSeaUnits; }   // IS_LAND && HAS_COAST (coastal-land domain)
	bool isPeakImprovement() const { return m_bPeakImprovement; }
	bool isNoFreshWater() const { return m_bNoFreshWater; }         // requires.build.noneOf HAS_FRESHWATER
	bool isHillsMakesValid() const { return m_bHillsMakesValid; }
	bool isFreshWaterMakesValid() const { return m_bFreshWaterMakesValid; }
	bool isRiverSideMakesValid() const { return m_bRiverSideMakesValid; }
	bool isPeakMakesValid() const { return m_bPeakMakesValid; }
	bool getTerrainMakesValid(int iTerrain) const { return m_terrainMakesValid.count(iTerrain) != 0; }
	bool getFeatureMakesValid(int iFeature) const { return m_featureMakesValid.count(iFeature) != 0; }
	bool isImprovementBonusMakesValid(int iBonus) const { return m_bonusMakesValid.count(iBonus) != 0; }
	// PrereqNatureYield -- the placement `{natureYield:{food:..}}` min-threshold atoms in requires.build,
	// parsed by the shared condition parser as CASC_PRED_NATURE_YIELD predicate nodes (channel in `id`,
	// threshold in `min`) and MATERIALIZED at mapFrom from the composed tree (the materializeValidity walk).
	int getPrereqNatureYield(int iYield) const
	{ return (iYield >= 0 && iYield < NUM_YIELD_TYPES) ? m_aiPrereqNatureYield[iYield] : 0; }

	// --- RUNTIME cross-reference (not curated): the reverse pass scans every BuildInfo for
	// getImprovement()==this and rebuilds this list at load. Worker AI reads it. ---
	const std::vector<BuildTypes>& getBuildTypes() const { return m_aeBuildTypes; }
	void addBuildType(BuildTypes eBuild) { m_aeBuildTypes.push_back(eBuild); }   // load-window writer
	void clearBuildTypes() { m_aeBuildTypes.clear(); }   // clear-first: the reverse pass runs in BOTH load phases

	// The improvement's property sources are authored as `triggers` property-delta entries (json.md §5) and
	// BRIDGED back into this object in mapFrom (CascadePropertyBridge::bridgePulses) so the KEEP-legacy plot
	// gather delivers them.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }

protected:
	virtual CvRequires*  mutRequires()  { return &m_requires; }
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// mapFrom helper (defined in the .cpp):
	void materializeValidity();   // the ONE requires.build walk -> typed members (incl. the natureYield thresholds)

	static int mapGet(const std::map<int, int>& valueMap, int iKey)
	{
		std::map<int, int>::const_iterator valueIter = valueMap.find(iKey);
		return valueIter != valueMap.end() ? valueIter->second : 0;
	}

	// --- the composed section units ---
	CvRequires  m_requires;
	CvEdges     m_edges;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iPillageGold;
	int m_iCultureRange;
	int m_iFeatureGrowthProbability;
	int m_iUpgradeTime;
	int m_iAdvancedStartCost;
	int m_iWorldSoundscapeScriptId;
	int m_iUniqueRange;
	int m_iGoodyUniqueRange;
	int m_iTilesPerGoody;
	ImprovementTypes m_eImprovementUpgrade;   // identity.upgradesTo (FK; self-FKs resolve at the full-registry re-run)
	ImprovementTypes m_eImprovementPillage;   // identity.pillageTo (FK)
	BonusTypes m_eBonusChange;                // identity.bonusChange (FK)
	bool m_bMilitaryStructure;
	virtual CvClassificationBlock* mutCharacteristics() { return &m_characteristics; }
	CvClassificationBlock m_characteristics;   // json.md §8 -- the plot-substrate held booleans
	bool m_bCarriesIrrigation;
	bool m_bOutsideBorders;
	bool m_bExtraterrestrial;
	bool m_bUniversalBonusTrade;
	bool m_bUpgradeRequiresFortify;
	bool m_bPlacesBonus;
	bool m_bPlacesFeature;
	bool m_bPlacesTerrain;
	bool m_bChangeRemove;
	bool m_bGoody;                 // mapGeneration.goody (EXE-bound isGoody)
	bool m_bRequiresRiverSide;     // mapGeneration.requiresRiverSide (EXE-bound; also a HAS_RIVER requires.build predicate)
	std::string m_szArtDefineTag;  // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	std::vector<MapCategoryTypes> m_aeMapCategories;
	std::vector<int> m_aiAlternativeImprovementUpgradeTypes;   // identity.alternativeUpgrades (FK-resolved IMPROVEMENT_ ids)
	std::vector<int> m_aiFeatureChangeTypes;                   // identity.featureChanges (FK-resolved FEATURE_ ids)
	std::map<int, int> m_bonusDiscoverRand;    // identity.bonuses.{BONUS}.discoverRand
	std::map<int, int> m_bonusDepletionRand;   // identity.bonuses.{BONUS}.depletionRand
	std::set<int> m_bonusTradeIds;             // identity.bonuses.{BONUS}.trade (OR'd with the universal flag)

	// --- the materialized placement/validity verdicts (the mapFrom requires.build walk) ---
	TechTypes m_ePrereqTech;
	bool m_bRequiresFeature;
	bool m_bRequiresFlatlands;
	bool m_bRequiresIrrigation;
	bool m_bWaterImprovement;
	bool m_bCanMoveSeaUnits;
	bool m_bPeakImprovement;
	bool m_bNoFreshWater;
	bool m_bHillsMakesValid;
	bool m_bFreshWaterMakesValid;
	bool m_bRiverSideMakesValid;
	bool m_bPeakMakesValid;
	std::set<int> m_terrainMakesValid;   // requires.build.any {terrain:[...]} membership (TERRAIN_ ids)
	std::set<int> m_featureMakesValid;   // requires.build.any {feature:[...]} membership (FEATURE_ ids)
	std::set<int> m_bonusMakesValid;     // requires.build.any {bonus:[...]} membership (BONUS_ presence atoms)
	int m_aiPrereqNatureYield[NUM_YIELD_TYPES];   // requires.build {natureYield:{...}} min thresholds (CASC_PRED_NATURE_YIELD atoms)

	std::vector<BuildTypes> m_aeBuildTypes;          // runtime cross-reference (the reverse pass)
	CvPropertyManipulators m_PropertyManipulators;   // fed from the triggers PROPERTY pulses (CascadePropertyBridge)
};

#endif // CV_JSON_IMPROVEMENT_INFO_H
