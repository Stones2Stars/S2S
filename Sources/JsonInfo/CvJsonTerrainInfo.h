#pragma once
#ifndef CV_JSON_TERRAIN_INFO_H
#define CV_JSON_TERRAIN_INFO_H

//
//	CvJsonTerrainInfo -- the JSON-populated real poco for TERRAINS (the CvXInfo replacement the engine reads normally).
//	Carries ONLY the surface LIVE callers read (owner ruling 2026-07-07: "we only care about live callers, and make the
//	infoclasses legible for humans") -- the dead legacy getters are dropped (e.g. getHealthPercent: every call is on a
//	specialist/feature/improvement, never a terrain). mapFrom maps each member straight from its JSON source; values are
//	HUMAN-native (like the XML poco) -- the cascade applies its own ×100 when it reads this object. No cascade here.
//
//	Live callers (verified 2026-07-07): yields -> CvPlot::calculateYield / CvGame worldgen; movementCost ->
//	CvUnitAI/CvWorkerAI plot move cost; buildModifier -> CvPlot::getBuildTime; defenseModifier -> CvPlot combat +
//	CvPlayerAI; cultureDistance -> CvCity::cultureDistance; distanceToLand/isWaterTerrain/isFreshWaterTerrain/climate ->
//	CvPlot relief + climate; mapCategories -> CvGameCoreUtils sharesMapCategory; zobristValue -> CvPlot map hash.
//

#include "CvJsonInfo.h"
#include "Defines/CvEnums.h"   // YieldTypes/NUM_YIELD_TYPES, ClimateZoneTypes, MapCategoryTypes
#include <vector>

class CvJsonTerrainInfo : public CvJsonInfo
{
public:
	CvJsonTerrainInfo();

	int getYield(int i) const { return (i >= 0 && i < NUM_YIELD_TYPES) ? m_aiYields[i] : 0; }
	int getMovementCost() const { return m_iMovementCost; }
	int getBuildModifier() const { return m_iBuildModifier; }
	int getDefenseModifier() const { return m_iDefenseModifier; }
	int getCultureDistance() const { return m_iCultureDistance; }

	int getDistanceToLand() const { return m_iDistanceToLand; }
	bool isWaterTerrain() const { return m_iDistanceToLand > 0; }
	bool isFreshWaterTerrain() const { return m_bFreshWaterTerrain; }
	ClimateZoneTypes getClimate() const { return m_eClimate; }

	bool isImpassable() const { return m_bImpassable; }               // identity.impassable
	bool isFound() const { return m_bFound; }                         // identity.found (city-foundability gate)
	bool isFoundCoast() const { return m_bFoundCoast; }               // identity.foundCoast (coastal found gate)
	bool isFoundFreshWater() const { return m_bFoundFreshWater; }     // identity.foundFreshWater (fresh-water found gate)

	const std::vector<MapCategoryTypes>& getMapCategories() const { return m_aeMapCategories; }
	int getZobristValue() const { return m_iZobristValue; }
	// EXE-bound art surface (mapscript/EXE map gen -- served by the CvTerrainInfo shim leaf, cascade-engine-430.md §3)
	const char* getArtDefineTag() const { return m_szArtDefineTag.c_str(); }

	// On-map AUDIO -- resolved to the runtime audio-manager index at info-load (mapFrom), EXACTLY as the archived
	// CvTerrainInfo::read did: gDLL->getAudioTagIndex. The curator ships the source string tags (sound.soundscape;
	// sound.footsteps[{FOOTSTEP_AUDIO_*: AS3D_*}]); mapFrom resolves each and stores the index (see the .cpp).
	int getWorldSoundscapeScriptId() const { return m_iWorldSoundscapeScriptId; }   // sound.soundscape -> gDLL->getAudioTagIndex(tag, AUDIOTAG_SOUNDSCAPE)
	int get3DAudioScriptFootstepIndex(int i) const                                  // sound.footsteps[footstepType] -> gDLL->getAudioTagIndex(scriptTag)
	{
		// Byte-faithful to the archived CvTerrainInfo::get3DAudioScriptFootstepIndex: NULL array (no footsteps
		// authored) -> 0; else the per-footstep-type slot, defaulting -1 for a type this terrain does not author.
		if (m_ai3DAudioScriptFootstepIndex.empty()) return 0;
		return (i >= 0 && i < (int)m_ai3DAudioScriptFootstepIndex.size()) ? m_ai3DAudioScriptFootstepIndex[i] : -1;
	}

	const CvPropertyManipulators* getPropertyManipulators() const { return &m_PropertyManipulators; }  // property engine (STUB empty -- 0/42 authored, deferred to properties-first-class pass)

	virtual void mapFrom(const picojson::value& entity);

	// --- the composed section units (by value; the base's mapFrom dispatch writes them via mut*) ---
	virtual const CvJsonModifiers* getModifiers() const { return &m_modifiers; }

protected:
	virtual CvJsonModifiers* mutModifiers() { return &m_modifiers; }

private:
	CvJsonModifiers m_modifiers;
	int m_aiYields[NUM_YIELD_TYPES];   // food/production/commerce .plot.flat
	int m_iMovementCost;               // identity.movementCost
	int m_iBuildModifier;              // buildTime.plot.percent
	int m_iDefenseModifier;            // defense.plot.amount.percent
	int m_iCultureDistance;            // cultureDistance.plot.flat
	int m_iDistanceToLand;             // identity.distanceToLand (0 = land; 1/2/… = coast/ocean tiers)
	int m_iZobristValue;               // STUB map-hash: needs the exact legacy zobrist computation (OOS-load-bearing)
	int m_iWorldSoundscapeScriptId;    // sound.soundscape -> audio-manager index (AUDIOTAG_SOUNDSCAPE); -1 when absent (legacy read default)
	std::vector<int> m_ai3DAudioScriptFootstepIndex;   // sound.footsteps: FootstepAudioType index -> AS3D_ script index (empty = none authored)
	bool m_bFreshWaterTerrain;         // identity.freshWaterTerrain
	bool m_bImpassable;                // identity.impassable
	bool m_bFound;                     // identity.found (city-foundability gate)
	bool m_bFoundCoast;                // identity.foundCoast (coastal found gate)
	bool m_bFoundFreshWater;           // identity.foundFreshWater (fresh-water found gate)
	ClimateZoneTypes m_eClimate;       // identity.climate (CLIMATE_ZONE_*)
	std::string m_szArtDefineTag;      // world.art.icon (ART_DEF_* tag; the EXE map-gen art lookup key)
	std::vector<MapCategoryTypes> m_aeMapCategories;   // identity.mapCategories (MAPCATEGORY_*)
	CvPropertyManipulators m_PropertyManipulators;     // STUB empty -- property engine, XML-era manipulator data deferred (0/42 authored)
};

#endif // CV_JSON_TERRAIN_INFO_H
