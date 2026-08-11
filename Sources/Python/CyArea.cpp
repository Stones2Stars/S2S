#include "CvGameCoreDLL.h"
#include "Engine/CvArea.h"
#include "Engine/CvMap.h"
#include "Defines/CvGlobals.h"
#include "CyArea.h"

//
// Python wrapper class for CvArea -- see the header: the ID is held, the pointer is resolved late, never cached.
//

CyArea::CyArea() : m_iAreaID(FFreeList::INVALID_INDEX)
{
}

CyArea::CyArea(CvArea* pArea) : m_iAreaID(pArea != NULL ? pArea->getID() : FFreeList::INVALID_INDEX)
{
	FAssert(pArea != NULL);
}

CvArea* CyArea::getArea() const
{
	// The ONE resolution point. A recalculateAreas run destroys every CvArea and reassigns every id, so this
	// answers NULL for an id that no longer names a live area instead of handing back freed memory.
	return (m_iAreaID != FFreeList::INVALID_INDEX) ? GC.getMap().getArea(m_iAreaID) : NULL;
}

bool CyArea::isNone() const
{
	return getArea() == NULL;
}

int CyArea::calculateTotalBestNatureYield() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->calculateTotalBestNatureYield() : 0;
}

int CyArea::countCoastalLand() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->countCoastalLand() : 0;
}

int CyArea::countNumUniqueBonusTypes() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->countNumUniqueBonusTypes() : 0;
}

int CyArea::getID() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getID() : -1;
}

int CyArea::getNumTiles() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumTiles() : 0;
}

bool CyArea::isLake() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->isLake() : false;
}

int CyArea::getNumRiverEdges() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumRiverEdges() : 0;
}

int CyArea::getNumCities() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumCities() : 0;
}

int CyArea::getNumUnits() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumUnits() : 0;
}

int CyArea::getTotalPopulation() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getTotalPopulation() : 0;
}

int CyArea::getNumStartingPlots() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumStartingPlots() : 0;
}

bool CyArea::isWater() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->isWater() : false;
}

int CyArea::getUnitsPerPlayer(PlayerTypes eIndex) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getUnitsPerPlayer(eIndex) : 0;
}

int CyArea::getCitiesPerPlayer(PlayerTypes eIndex) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getCitiesPerPlayer(eIndex) : 0;
}

int CyArea::getPower(PlayerTypes eIndex) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getPower(eIndex) : 0;
}

int CyArea::getBestFoundValue(PlayerTypes eIndex) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getBestFoundValue(eIndex) : 0;
}

bool CyArea::isBorderObstacle(TeamTypes eIndex) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->isBorderObstacle(eIndex) : false;
}

int CyArea::getNumBonuses(BonusTypes eBonus) const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumBonuses(eBonus) : 0;
}

int CyArea::getNumTotalBonuses() const
{
	const CvArea* pArea = getArea();
	return (pArea != NULL) ? pArea->getNumTotalBonuses() : 0;
}

//
//	THE MAP-SCRIPT BOUNDARY, republished. Map scripts are their OWN boundary (patterns.md THE PYTHON READ
//	BOUNDARY: they read map-gen types nothing else reads, run before most game state exists, are WRITE-
//	dominated, and their contract is the named Python CALLBACKS) -- so third-party scripts were never meant
//	to be affected by the Cy* cut. These are HANDLES, not infos: the ruling that GC hands out no infos is
//	untouched by them.
//
void CyArea::pythonPublish()
{
	python::class_<CyArea>("CyArea", python::no_init)
		.def("calculateTotalBestNatureYield", &CyArea::calculateTotalBestNatureYield)
		.def("countCoastalLand", &CyArea::countCoastalLand)
		.def("countNumUniqueBonusTypes", &CyArea::countNumUniqueBonusTypes)
		.def("getID", &CyArea::getID)
		.def("getNumTiles", &CyArea::getNumTiles)
		.def("isLake", &CyArea::isLake)
		.def("getNumRiverEdges", &CyArea::getNumRiverEdges)
		.def("getNumCities", &CyArea::getNumCities)
		.def("getNumUnits", &CyArea::getNumUnits)
		.def("getTotalPopulation", &CyArea::getTotalPopulation)
		.def("getNumStartingPlots", &CyArea::getNumStartingPlots)
		.def("isWater", &CyArea::isWater)

		.def("getUnitsPerPlayer", &CyArea::getUnitsPerPlayer)
		.def("getCitiesPerPlayer", &CyArea::getCitiesPerPlayer)
		.def("getPower", &CyArea::getPower)
		.def("getBestFoundValue", &CyArea::getBestFoundValue)


		.def("isBorderObstacle", &CyArea::isBorderObstacle)


		.def("getNumBonuses", &CyArea::getNumBonuses)
		.def("getNumTotalBonuses", &CyArea::getNumTotalBonuses)
/************************************************************************************************/
/* Afforess	                  Start		 07/15/10                                               */
/*                                                                                              */
/*                                                                                              */
/************************************************************************************************/
/************************************************************************************************/
/* Afforess	                     END                                                            */
/************************************************************************************************/

	;
}
