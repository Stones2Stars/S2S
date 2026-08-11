#pragma once
#ifndef CV_JSON_FEATURE_INFO_H
#define CV_JSON_FEATURE_INFO_H

//
//	CvFeatureInfo -- the FEATURE poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the feature's OWN tile output (modifier.md §5 plot-substrate
//	own-output -- the base flats PLUS the HAS_RIVER-conditioned entries and the reverse-landed cross-entity
//	boosts) lives on the compiled modifier surface; the point reads fetch the unconditioned sums, the
//	conditioned tail is the base conditioned-list access. The identity placement/growth fields, the on-map
//	art/audio, and the grants/triggers provisions are the genuine bespoke set. No legacy-mirror modifier
//	member survives ([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "CvClassificationBlock.h"   // the §8 held-boolean block + CLS_HAS
#include "Defines/CvEnums.h"   // TerrainTypes / MapCategoryTypes
#include <vector>

class CvArtInfoFeature;

class CvFeatureInfo : public CvInfo
{
public:
	CvFeatureInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvTriggers*  getTriggers()  const { return &m_triggers; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: food/production/commerce plot flats (own tile output; the HAS_RIVER extras are
	// conditioned entries); defense.plot.amount.percent; health.plot.percent -- the fallout class, summed
	// over radius plots ÷100 by the wellbeing calc (modifier.md §2b); cultureDistance.plot.flat is the
	// getScalar(SCALAR_CULTURE_DISTANCE) straggler.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return flatWithFans(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope); }
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }
	// The authored wellbeing families' SIGNED sums. ⛔ ANGER/UNHEALTH read 0 here BY CONSTRUCTION and that is
	// never a gap to chase: an INFO keeps a negative in its POSITIVE family (happiness -1, not anger +1) --
	// the sign ROUTING to the opposing channel happens at FILL, on the city PACKAGE, not on authored data
	// (modifier.md §2b). So this read already carries the negatives; there is nothing to verify in the JSON.
	int getWellbeingModifier(WellbeingChannel eChannel, CvCascScope eScope) const
	{
		if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
		{
			return 0;
		}
		return m_modifiers.sum(infoWellbeingFamily(eChannel), CHANNEL_AMOUNT, eScope, CASC_UNIT_PERCENT);
	}

	// ======================= 3. INTRINSIC -- bare typed reads (the census identity set) ======================
	// The substrate's own base movement cost, served as the family it is authored in
	// ([modifier.md] par.6: a plot substrate's base movement cost IS the `movement` family). x100 native like
	// every compiled sum -- the reader reduces at its point of use ([DEC-fixedpoint-x100]).
	int getFlatMovement(MovementKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MOVEMENT, eKind, eScope, CASC_UNIT_FLAT); }
	// What this ground costs to see THROUGH, served as the family it is authored in ([vision.md] §1: a
	// feature's see-through value IS its `obstruction`). Same shape as the movement read above.
	int getFlatVision(VisionKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_VISION, eKind, eScope, CASC_UNIT_FLAT); }
	int getPopDestroys() const { return m_iPopDestroys; }                            // identity.popDestroys (-1 = never)
	int getAppearanceProbability() const { return m_iAppearanceProbability; }        // identity.appearance
	int getDisappearanceProbability() const { return m_iDisappearanceProbability; }  // identity.disappearance
	int getGrowthProbability() const { return m_iGrowthProbability; }                // identity.growth
	int getSpreadProbability() const { return m_iSpreadProbability; }                // identity.spread
	int getAdvancedStartRemoveCost() const { return m_iAdvancedStartRemoveCost; }    // cost.advancedStartRemoveCost

	virtual const CvClassificationBlock* getCharacteristics() const { return &m_characteristics; }

	bool isImpassable() const { return m_bImpassable; }
	bool isRequiresFlatlands() const { return m_bRequiresFlatlands; }
	bool isRequiresRiver() const { return m_bRequiresRiver; }
	bool isNoCoast() const { return m_bNoCoast; }
	bool isNoRiver() const { return m_bNoRiver; }
	bool isNoAdjacent() const { return m_bNoAdjacent; }
	bool isCoastalOnly() const { return m_bCoastalOnly; }
	bool isVisibleAlways() const { return m_bVisibleAlways; }
	bool isCanGrowAnywhere() const { return m_bCanGrowAnywhere; }
	bool isAddsFreshWater() const { return m_bAddsFreshWater; }
	// isOnlyBad -- COMPUTED over the compiled reads (the BUG city-plot-status test): no positive health, no
	// fresh water, no positive tile yield.
	bool isOnlyBad() const;

	// valid terrains (the feature may appear on these)
	bool isTerrain(int iTerrain) const;
	const std::vector<TerrainTypes>& getValidTerrains() const { return m_aeValidTerrains; }
	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }
	// EXE-bound art surface (mapscript/EXE map gen)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport const CvArtInfoFeature* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton)

	// fed from the triggers PROPERTY pulses in mapFrom (the KEEP-legacy plot gather)
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

	// --- on-map art/audio (world.art effect + the sound block) ---
	const char* getEffectType() const { return m_szEffectType.c_str(); }          // world.art.effect.type
	int getEffectProbability() const { return m_iEffectProbability; }             // world.art.effect.probability
	const char* getGrowthSound() const { return m_szGrowthSound.c_str(); }        // sound.growth
	const char* getOnUnitChangeTo() const { return m_szOnUnitChangeTo.c_str(); }  // grants.onUnitChangeTo (module-only)
	// On-map AUDIO -- resolved to the runtime audio-manager index at info-load (gDLL->getAudioTagIndex).
	// CvPlot reads the FEATURE footstep index in preference to the terrain's when a feature is present.
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }
	int get3DAudioScriptFootstepIndex(int iFootstepType) const
	{
		// Byte-faithful to the archived read: NULL array (no footsteps authored) -> -1 (NB the feature's
		// NULL default is -1, NOT the terrain's 0); else the per-footstep-type slot.
		if (m_ai3DAudioScriptFootstepIndex.empty())
		{
			return -1;
		}
		if (iFootstepType >= 0 && iFootstepType < (int)m_ai3DAudioScriptFootstepIndex.size())
		{
			return m_ai3DAudioScriptFootstepIndex[iFootstepType];
		}
		return -1;
	}
	// Art-DEFINE tier (CvArtInfoFeature via ARTFILEMGR): the variety count + secondary-art test live in the
	// art-define files -- delegate exactly as the on-map callers do (defined in the .cpp).
	int getNumVarieties() const;
	bool canBeSecondary() const;

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvTriggers*  mutTriggers()  { return &m_triggers; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvTriggers  m_triggers;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iPopDestroys;
	int m_iAppearanceProbability;
	int m_iDisappearanceProbability;
	int m_iGrowthProbability;
	int m_iSpreadProbability;
	int m_iAdvancedStartRemoveCost;
	int m_iEffectProbability;
	int m_iZobristValue;               // map-hash drawn from the synced RNG in the ctor (OOS-load-bearing)
	int m_iWorldSoundscapeScriptId;    // sound.soundscape -> audio-manager index; -1 when absent
	std::vector<int> m_ai3DAudioScriptFootstepIndex;   // sound.footsteps: FootstepAudioType index -> AS3D_ script index
	virtual CvClassificationBlock* mutCharacteristics() { return &m_characteristics; }

	CvClassificationBlock m_characteristics;   // json.md §8 -- the plot-substrate held booleans
	bool m_bImpassable;
	bool m_bRequiresFlatlands;
	bool m_bRequiresRiver;
	bool m_bAddsFreshWater;
	bool m_bNoCoast;
	bool m_bNoRiver;
	bool m_bNoAdjacent;
	bool m_bCoastalOnly;
	bool m_bVisibleAlways;
	bool m_bCanGrowAnywhere;
	std::string m_szArtDefineTag;      // world.art.icon (ART_DEF_* tag)
	std::string m_szEffectType;        // world.art.effect.type
	std::string m_szGrowthSound;       // sound.growth
	std::string m_szOnUnitChangeTo;    // grants.onUnitChangeTo
	std::vector<TerrainTypes> m_aeValidTerrains;   // identity.validTerrains (resolved TERRAIN_ ids)
	std::vector<MapCategoryTypes> m_aeMapCategories;
	CvPropertyManipulators m_PropertyManipulators; // fed from the triggers PROPERTY pulses (CascadePropertyBridge)
};

#endif // CV_JSON_FEATURE_INFO_H
