#pragma once
#ifndef CV_JSON_TERRAIN_INFO_H
#define CV_JSON_TERRAIN_INFO_H

//
//	CvTerrainInfo -- the TERRAIN poco rebuilt to the exemplar surface (patterns.md § THE GETTER SETUP).
//	Styled for the JSON anatomy (json.md §2): the terrain's OWN tile output (modifier.md §5 plot-substrate
//	own-output -- the base flats plus the reverse-landed cross-entity conditioned entries) lives on the
//	compiled modifier surface; the point reads fetch the unconditioned sums, the conditioned tail is the base
//	conditioned-list access (cultureDistance.plot.flat is the getScalar(SCALAR_CULTURE_DISTANCE)
//	straggler). The relief/climate/foundability fields are identity self-description -- incl. the worker
//	build-time percent, substrate self-data per ruling 18 plane 1 (identity.buildTimeModifier, never a
//	modifier family). No legacy-mirror modifier member survives ([DEC-new-getter-surface]).
//

#include "CvInfo.h"
#include "Defines/CvEnums.h"   // ClimateZoneTypes / MapCategoryTypes
#include <vector>

class CvArtInfoTerrain;

class CvTerrainInfo : public CvInfo
{
public:
	CvTerrainInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ======================= 1. SECTIONS -- whole typed objects =======================
	virtual const CvEdges*     getEdges()     const { return &m_edges; }
	virtual const CvModifiers* getModifiers() const { return &m_modifiers; }

	// ======================= 2. MODIFIER GROUPS -- point reads over the compiled sums ========================
	// (Conditioned-list access + the expected* what-if valuations are the base CvInfo surface. Census
	// participation: food/production/commerce plot flats (own tile output); defense.plot.amount.percent.)
	int getFlatYield(YieldTypes eYield, CvCascScope eScope) const
	{ return flatWithFans(infoYieldFamily(eYield), CHANNEL_AMOUNT, eScope); }
	int getDefense(DefenseKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_DEFENSE, eKind, eScope, infoDefenseUnit(eKind)); }

	// ======================= 3. INTRINSIC -- bare typed reads (the census identity set) ======================
	// The substrate's own base movement cost, served as the family it is authored in
	// ([modifier.md] par.6: a plot substrate's base movement cost IS the `movement` family). x100 native like
	// every compiled sum -- the reader reduces at its point of use ([DEC-fixedpoint-x100]).
	int getFlatMovement(MovementKind eKind, CvCascScope eScope) const
	{ return m_modifiers.sum(MODFAM_MOVEMENT, eKind, eScope, CASC_UNIT_FLAT); }
	int getBuildModifier() const { return m_iBuildModifier; }     // identity.buildTimeModifier (ruling 18 plane 1)
	int getDistanceToLand() const { return m_iDistanceToLand; }   // identity.distanceToLand (0 = land; 1/2/... = coast/ocean tiers)
	bool isWaterTerrain() const { return m_iDistanceToLand > 0; }
	bool isFreshWaterTerrain() const { return m_bFreshWaterTerrain; }   // identity.freshWaterTerrain
	ClimateZoneTypes getClimate() const { return m_eClimate; }          // identity.climateZoneType (CLIMATE_ZONE_*)
	bool isImpassable() const { return m_bImpassable; }                 // identity.impassable
	bool isFound() const { return m_bFound; }                           // identity.found (city-foundability gate)
	bool isFoundCoast() const { return m_bFoundCoast; }                 // identity.foundCoast
	bool isFoundFreshWater() const { return m_bFoundFreshWater; }       // identity.foundFreshWater

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }
	// EXE-bound art surface (mapscript/EXE map gen)
	DllExport const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }
	DllExport const CvArtInfoTerrain* getArtInfo() const;
	const char* getButton() const;   // art-define button (else CvInfoBase's empty m_szButton)

	// On-map AUDIO -- resolved to the runtime audio-manager index at info-load (gDLL->getAudioTagIndex).
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }
	int get3DAudioScriptFootstepIndex(int iFootstepType) const
	{
		// Byte-faithful to the archived read: NULL array (no footsteps authored) -> 0; else the
		// per-footstep-type slot, defaulting -1 for a type this terrain does not author.
		if (m_ai3DAudioScriptFootstepIndex.empty())
		{
			return 0;
		}
		if (iFootstepType >= 0 && iFootstepType < (int)m_ai3DAudioScriptFootstepIndex.size())
		{
			return m_ai3DAudioScriptFootstepIndex[iFootstepType];
		}
		return -1;
	}

	// The KEEP-legacy property engine's per-turn SOURCES (property-audit.md). The gather roster walks
	// a terrain, so the container must exist -- but NO terrain authors a PROPERTY_* family today, so it is
	// accurately EMPTY rather than stubbed: it fills the moment such a deposit is curated.
	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }

protected:
	virtual CvEdges*     mutEdges()     { return &m_edges; }
	virtual CvModifiers* mutModifiers() { return &m_modifiers; }

private:
	// --- the composed section units ---
	CvEdges     m_edges;
	CvModifiers m_modifiers;

	// --- the intrinsic identity members (materialized once at mapFrom; getters are bare reads) ---
	int m_iBuildModifier;
	int m_iDistanceToLand;
	int m_iZobristValue;               // map-hash drawn from the synced RNG in the ctor (OOS-load-bearing)
	int m_iWorldSoundscapeScriptId;    // sound.soundscape -> audio-manager index; -1 when absent
	std::vector<int> m_ai3DAudioScriptFootstepIndex;   // sound.footsteps: FootstepAudioType index -> AS3D_ script index
	bool m_bFreshWaterTerrain;
	bool m_bImpassable;
	bool m_bFound;
	bool m_bFoundCoast;
	bool m_bFoundFreshWater;
	ClimateZoneTypes m_eClimate;       // identity.climateZoneType (CLIMATE_ZONE_*)
	std::string m_szArtDefineTag;      // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	std::vector<MapCategoryTypes> m_aeMapCategories;   // identity.mapCategories (MAPCATEGORY_*)
	CvPropertyManipulators m_PropertyManipulators;   // fed from the PROPERTY_* families (CascadePropertyBridge)
};

#endif // CV_JSON_TERRAIN_INFO_H
