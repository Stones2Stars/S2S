#pragma once

#ifndef CyArea_h
#define CyArea_h

//
// Python wrapper class for CvArea
//
// ⛔ HOLDS THE AREA ID, NEVER A CvArea POINTER -- a correctness requirement, not a style choice.
// CvMap::recalculateAreas does m_areas.removeAll(): every CvArea is DESTROYED and every id reassigned. So a
// cached CvArea* dangles the moment terrain is levelled to sea level (the WMD mechanic -- driven substantially
// from Python, which is exactly where a CyArea is obtained and held across turns). CvPlot caches an area
// pointer too, but nulls it in setArea; this wrapper had no such hook, which made it the one stored CvArea*
// with no invalidation path.
// The standing rule is state-repositories.md's: HOLD THE AREA ID, RESOLVE LATE, NEVER CACHE THE POINTER -- and
// the "areas recalculated" DOMAIN fact is what makes late resolution safe. Every read below resolves through
// getArea() and answers a neutral value when the id no longer names a live area, so a stale id degrades to
// "no area" rather than to freed memory.
//

class CyCity;
class CvArea;

class CyArea
{
public:
	CyArea();
	explicit CyArea(CvArea* pArea);					// Call from C++

	CvArea* getArea() const;						// Call from C++ -- resolves the id; NULL once it is stale
	bool isNone() const;

	int calculateTotalBestNatureYield() const;
	int countCoastalLand() const;
	int countNumUniqueBonusTypes() const;
	int getID() const;
	int getNumTiles() const;
	bool isLake() const;
	int getNumRiverEdges() const;
	int getNumCities() const;
	int getNumUnits() const;
	int getTotalPopulation() const;
	int getNumStartingPlots() const;
	bool isWater() const;

	int getUnitsPerPlayer(PlayerTypes eIndex) const;
	int getCitiesPerPlayer(PlayerTypes eIndex) const;
	int getPower(PlayerTypes eIndex) const;
	int getBestFoundValue(PlayerTypes eIndex) const;

	bool isCleanPower(TeamTypes eIndex) const;
	bool isBorderObstacle(TeamTypes eIndex) const;

	int getYieldRateModifier(PlayerTypes eIndex1, YieldTypes eIndex2) const;

	int getNumBonuses(BonusTypes eBonus) const;
	int getNumTotalBonuses() const;

	void changeCleanPowerCount(TeamTypes eIndex, int iChange) const;

protected:
	int m_iAreaID;
};

#endif	// #ifndef CyArea
