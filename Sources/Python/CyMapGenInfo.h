#pragma once

#ifndef CyMapGenInfo_h__
#define CyMapGenInfo_h__

//
//	The remaining PER-INFO accessors map GENERATION reads ([patterns.md] THE PYTHON READ BOUNDARY). Siblings of
//	CyWorldInfo and CyClimateInfo; each is its own type so a script's bindings list stays its dependency list.
//
//	⚑ They carry the map-gen half of their registries -- the reads generation actually makes -- not a mirror of
//	the legacy field set. A read joins one when a call site asks for it.
//
//	⚑ MAP GENERATION IS THE SECOND LEGITIMATE FULL-SCAN CONSUMER (owner), after the pedia: placing resources and
//	laying terrain is a decision over the WHOLE registry, so the caller's sweep over every bonus / terrain /
//	feature is correct and stays. Only where each value comes from changes.
//

//	What a resource is, and where it may be placed.
class CyBonusInfo
{
public:
	CyBonusInfo() {}

	int  getPlacementOrder(int iBonus) const;   // which pass places it; -1 = never placed by the generator
	int  getTilesPer(int iBonus) const;         // one per N land tiles
	int  getPercentPerPlayer(int iBonus) const; // extra share scaled by the player count
	int  getUniqueRange(int iBonus) const;      // keep this many tiles between instances
	bool isOneArea(int iBonus) const;           // confine every instance to a single landmass
	int  getTechCityTrade(int iBonus) const;    // TECH_ FK -- the tech that puts it on the trade network

	//	Placement validity, asked per substrate id (the sets are sparse, so these are membership tests).
	bool isTerrain(int iBonus, int iTerrain) const;
	bool isFeature(int iBonus, int iFeature) const;
	bool isFeatureTerrain(int iBonus, int iTerrain) const;

	//	⚠ THE BANDS ARE FOUR DICE THAT ARE SUMMED, NEVER ALTERNATIVES TO PICK BETWEEN: the appearance count is
	//	`getConstAppearance` plus ONE DRAW PER BAND, which is exactly what the legacy single-value getter
	//	returned. This getter serves the band's roll CEILING; the DRAW is the caller's.
	//	⛔ A band whose ceiling is 0 is still drawn -- the draw advances the shared map-RNG seed, and that
	//	sequence is what the generated map is made of, so skipping one silently changes the map.
	//	`getNumRandAppearanceBands` is published so a caller walks them rather than assuming the count.
	int getRandAppearance(int iBonus, int iBand) const;
	int getNumRandAppearanceBands() const;
	int getConstAppearance(int iBonus) const;

	static void pythonPublish();
};

class CyFeatureInfo
{
public:
	CyFeatureInfo() {}

	int getAppearanceProbability(int iFeature) const;  // -1 = the generator never adds it
	int getNumVarieties(int iFeature) const;           // how many art variants a placed feature picks from

	static void pythonPublish();
};

class CyTerrainInfo
{
public:
	CyTerrainInfo() {}

	bool isWaterTerrain(int iTerrain) const;

	static void pythonPublish();
};

class CySeaLevelInfo
{
public:
	CySeaLevelInfo() {}

	int getSeaLevelChange(int iSeaLevel) const;

	static void pythonPublish();
};

#endif // CyMapGenInfo_h__
