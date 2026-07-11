#pragma once
#ifndef CV_JSON_FEATURE_INFO_H
#define CV_JSON_FEATURE_INFO_H

//
//	CvFeatureInfo -- the JSON real poco for terrain FEATURES (the CvXInfo replacement). Live-caller surface only,
//	mapped from the curator's real shapes: the plot yield/health/defense/culture/vision families, and the `identity`
//	placement fields. HUMAN-native values (the cascade ×100s on its side). Availability (requires/enables/…) rides the
//	CvInfo base. No cascade here.
//
//	Live callers (verified 2026-07-07): getYieldChange -> CvCity/CvPlot yields; getHealthPercent -> CvCity health;
//	getDefenseModifier -> CvPlot combat; getMovementCost -> unit move cost; getCultureDistance -> CvCity; validTerrains/
//	isTerrain -> feature placement; popDestroys/the placement flags -> CvPlot/CvGame worldgen; seeThroughChange -> vision.
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // NUM_YIELD_TYPES / TerrainTypes / MapCategoryTypes
#include <vector>

class CvArtInfoFeature;

class CvFeatureInfo : public CvInfo
{
public:
	CvFeatureInfo();

	int getYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYieldChange[i] : 0; }
	int getMovementCost() const { return m_iMovementCost; }
	int getDefenseModifier() const { return m_iDefenseModifier; }
	int getHealthPercent() const { return m_iHealthPercent; }
	int getCultureDistance() const { return m_iCultureDistance; }
	int getSeeThroughChange() const { return m_iSeeThroughChange; }
	int getPopDestroys() const { return m_iPopDestroys; }
	int getAppearanceProbability() const { return m_iAppearanceProbability; }         // identity.appearance
	int getDisappearanceProbability() const { return m_iDisappearanceProbability; }   // identity.disappearance
	int getGrowthProbability() const { return m_iGrowthProbability; }                 // identity.growth
	int getSpreadProbability() const { return m_iSpreadProbability; }                 // identity.spread
	int getAdvancedStartRemoveCost() const { return m_iAdvancedStartRemoveCost; }     // cost.advancedStartRemoveCost
	int getWarmingDefense() const { return 0; }   // CURATOR-GAP: curate_feature DROPs iWarmingDefense (dead field -- GLOBAL_WARMING is #defined out)

	bool isImpassable() const { return m_bImpassable; }
	bool isNoCity() const { return m_bNoCity; }
	bool isNoImprovement() const { return m_bNoImprovement; }
	bool isNoBonus() const { return m_bNoBonus; }
	bool isCountsAsPeak() const { return m_bCountsAsPeak; }
	bool isRequiresFlatlands() const { return m_bRequiresFlatlands; }
	bool isRequiresRiver() const { return m_bRequiresRiver; }                   // identity.requiresRiver
	bool isNoCoast() const { return m_bNoCoast; }                              // identity.noCoast
	bool isNoRiver() const { return m_bNoRiver; }                              // identity.noRiver
	bool isNoAdjacent() const { return m_bNoAdjacent; }                        // identity.noAdjacent
	bool isCoastalOnly() const { return m_bCoastalOnly; }                      // identity.coastalOnly
	bool isVisibleAlways() const { return m_bVisibleAlways; }                  // identity.visibleAlways
	bool isIgnoreTerrainCulture() const { return m_bIgnoreTerrainCulture; }    // identity.ignoreTerrainCulture
	bool isCanGrowAnywhere() const { return m_bCanGrowAnywhere; }              // identity.canGrowAnywhere
	bool isAddsFreshWater() const { return m_bAddsFreshWater; }
	bool isNukeImmune() const { return m_bNukeImmune; }
	// isOnlyBad -- COMPUTED (mirrors the archived CvFeatureInfo::isOnlyBad, BUG city-plot-status): no positive health,
	// no fresh water, and no positive yield.
	bool isOnlyBad() const;

	// valid terrains (the feature may appear on these) -- getNumVarieties()/isTerrain() consumers
	bool isTerrain(int iTerrain) const;
	const std::vector<TerrainTypes>& getValidTerrains() const { return m_aeValidTerrains; }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }
	// EXE-bound art surface (mapscript/EXE map gen -- served by the CvFeatureInfo shim leaf, cascade-engine-430.md §3)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport const CvArtInfoFeature* getArtInfo() const;   // EXE map-gen art (merged from the removed shim leaf)
	const char* getButton() const;                          // art-define button (else CvInfoBase's empty m_szButton)

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }   // property engine (self-contained; XML-era manip data deferred)

	// --- arrays / art / audio wired to their real curator addresses (see mapFrom for the reads) ---
	int getRiverYieldChange(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiRiverYieldChange[i] : 0; }  // the HAS_RIVER-gated yield.plot.flat entries
	const char* getEffectType() const { return m_szEffectType.c_str(); }          // world.art.effect.type (EFFECT_BIRDSCATTER)
	int getEffectProbability() const { return m_iEffectProbability; }             // world.art.effect.probability
	const char* getGrowthSound() const { return m_szGrowthSound.c_str(); }        // sound.growth
	const char* getOnUnitChangeTo() const { return m_szOnUnitChangeTo.c_str(); }  // grants.onUnitChangeTo (module-only; no base feature authors it)

	// On-map AUDIO -- resolved to the runtime audio-manager index at info-load (mapFrom), EXACTLY as the archived
	// CvFeatureInfo::read did: gDLL->getAudioTagIndex. The curator ships the source string tags (sound.soundscape;
	// sound.footsteps[{FOOTSTEP_AUDIO_*: AS3D_*}]) -- 63 features author a soundscape, 44 author footsteps -- and
	// mapFrom resolves each and stores the index (see the .cpp). CvPlot reads the FEATURE footstep index in preference
	// to the terrain's when a feature is present (CvPlot::get3DAudioScriptFootstepIndex), so this is a live surface.
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }
	int get3DAudioScriptFootstepIndex(int i) const
	{
		// Byte-faithful to the archived CvFeatureInfo::get3DAudioScriptFootstepIndex: NULL array (no footsteps
		// authored) -> -1 (NB: feature's NULL default is -1, NOT terrain's 0); else the per-footstep-type slot.
		if (m_ai3DAudioScriptFootstepIndex.empty()) return -1;
		return (i >= 0 && i < (int)m_ai3DAudioScriptFootstepIndex.size()) ? m_ai3DAudioScriptFootstepIndex[i] : -1;
	}
	// Art-DEFINE tier (CvArtInfoFeature via ARTFILEMGR): the variety count + secondary-art test live in the art define
	// files, served by the shim's DllExport getArtInfo() at runtime -- NOT feature-curator output, so delegate to the
	// art define exactly as the on-map callers / the archived CvFeatureInfo did (defined in the .cpp, where
	// CvArtInfoFeature is a complete type). getNumVarieties has a live Python-pedia consumer (CyInfoInterface2).
	int getNumVarieties() const;
	bool canBeSecondary() const;
	// Feature <Categories>: absent from CIV4FeatureInfos.xml entirely (no feature authors it), so the curator emits
	// nothing and m_aiCategories was always empty in legacy too.
	int getCategory(int /*i*/) const { return -1; }
	int getNumCategories() const { return 0; }
	bool isCategory(int /*i*/) const { return false; }

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonGrants*    getGrants()    const { return &m_grants; }
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonGrants*    mutGrants()    { return &m_grants; }
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonGrants    m_grants;
	CvJsonModifiers m_modifiers;
	int m_aiYieldChange[NUM_YIELD_TYPES];       // food/production/commerce .plot.flat (unconditional entries)
	int m_aiRiverYieldChange[NUM_YIELD_TYPES];  // the same families' HAS_RIVER-gated entries (legacy RiverYieldChange)
	int m_iMovementCost;                   // identity.movementCost
	int m_iDefenseModifier;                // defense.plot.amount.percent
	int m_iHealthPercent;                  // health.plot.percent
	int m_iCultureDistance;                // cultureDistance.plot.flat
	int m_iSeeThroughChange;               // vision.plot.seeThrough.flat
	int m_iPopDestroys;                    // identity.popDestroys
	int m_iAppearanceProbability;          // identity.appearance
	int m_iDisappearanceProbability;       // identity.disappearance
	int m_iGrowthProbability;              // identity.growth
	int m_iSpreadProbability;              // identity.spread
	int m_iAdvancedStartRemoveCost;        // cost.advancedStartRemoveCost
	int m_iEffectProbability;              // world.art.effect.probability
	int m_iZobristValue;                   // CURATOR-GAP: map-hash; needs the exact legacy zobrist computation (OOS -- out of scope)
	int m_iWorldSoundscapeScriptId;        // sound.soundscape -> audio-manager index (AUDIOTAG_SOUNDSCAPE); -1 when absent (legacy read default)
	std::vector<int> m_ai3DAudioScriptFootstepIndex;   // sound.footsteps: FootstepAudioType index -> AS3D_ script index (empty = none authored)
	bool m_bImpassable, m_bNoCity, m_bNoImprovement, m_bNoBonus, m_bCountsAsPeak;   // identity placement flags
	bool m_bRequiresFlatlands, m_bRequiresRiver, m_bAddsFreshWater, m_bNukeImmune;  // identity placement flags
	bool m_bNoCoast, m_bNoRiver, m_bNoAdjacent, m_bCoastalOnly, m_bVisibleAlways;   // identity placement flags
	bool m_bIgnoreTerrainCulture, m_bCanGrowAnywhere;                               // identity growth/culture flags
	std::string m_szArtDefineTag;          // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	std::string m_szEffectType;            // world.art.effect.type
	std::string m_szGrowthSound;           // sound.growth
	std::string m_szOnUnitChangeTo;        // grants.onUnitChangeTo
	std::vector<TerrainTypes> m_aeValidTerrains;   // identity.validTerrains (resolved TERRAIN_ ids)
	std::vector<MapCategoryTypes> m_aeMapCategories;
	CvPropertyManipulators m_PropertyManipulators;   // empty -- property engine (#429); XML-era manipulator data deferred to that pass
};

#endif // CV_JSON_FEATURE_INFO_H
