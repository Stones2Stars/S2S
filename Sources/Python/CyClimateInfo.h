#pragma once

#ifndef CyClimateInfo_h__
#define CyClimateInfo_h__

//
//	CyClimateInfo -- the CLIMATE accessor, one of the PER-INFO accessors the Python read boundary is built from
//	([patterns.md] THE PYTHON READ BOUNDARY). Sibling of CyWorldInfo, which already carries map-gen reads.
//
//	⚑ MAP GENERATION IS SERVED BY THE LIBRARY LIKE EVERY OTHER CONSUMER. What is separate about a map script is
//	its CALLBACK contract ([engine.md]) and its ENUMERATION -- it genuinely iterates every bonus, terrain and
//	feature -- never where its values come from. The old `GC.getClimateInfo(i)` endpoint is not coming back.
//
//	⚠ THE LATITUDE CHANGES ARE `float` AND STAY float. They are the legacy generator's own units, consumed in
//	float arithmetic by the scripts, and they run on the MAP rng before the game exists -- so
//	[DEC-fixedpoint-x100] does not reach them (it governs an AMOUNT the cascade carries and combines, and
//	nothing here enters a package, a deposit or a synced decision). Converting them would change generated maps.
//
class CyClimateInfo
{
public:
	CyClimateInfo() {}

	// Terrain shaping -- whole numbers.
	int getDesertPercentChange(int iClimate) const;
	int getJungleLatitude(int iClimate) const;
	int getHillRange(int iClimate) const;
	int getPeakPercent(int iClimate) const;

	// Latitude bands -- the generator's own float units (see the note above).
	float getSnowLatitudeChange(int iClimate) const;
	float getTundraLatitudeChange(int iClimate) const;
	float getGrassLatitudeChange(int iClimate) const;
	float getDesertBottomLatitudeChange(int iClimate) const;
	float getDesertTopLatitudeChange(int iClimate) const;
	float getIceLatitude(int iClimate) const;
	float getRandIceLatitude(int iClimate) const;

	static void pythonPublish();
};

#endif // CyClimateInfo_h__
