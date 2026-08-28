
#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "Engine/CvUnit.h"   // getUnitIds -- the units standing here
#include "Engine/CvCity.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "Engine/CvMap.h"
#include "Engine/CvPlot.h"
#include "CyArea.h"
#include "CyCity.h"
#include "CyPlot.h"
#include "Infrastructure/CvPythonPlotLoader.h"
#include "CyUnit.h"

//
// Python wrapper class for CvPlot
//

CyPlot::CyPlot(CvPlot* pPlot, bool bInViewportSpace) : m_pPlot(pPlot), m_bIsInViewportSpace(bInViewportSpace) {}

CyPlot::CyPlot(CvPlot* pPlot) : m_pPlot(pPlot), m_bIsInViewportSpace(false) {}

CyPlot::CyPlot() : m_pPlot(NULL), m_bIsInViewportSpace(false) {}


void CyPlot::erase()
{
	if (m_pPlot) m_pPlot->erase();
}

NiPoint3 CyPlot::getPoint()
{
	return m_pPlot ? m_pPlot->getPoint() : NiPoint3(0,0,0);
}

int CyPlot::getTeam() const
{
	return m_pPlot ? m_pPlot->getTeam() : -1;
}

void CyPlot::nukeExplosion(int iRange, const CyUnit& kNukeUnit)
{
	if (m_pPlot) m_pPlot->nukeExplosion(iRange, kNukeUnit.getUnit());
}

bool CyPlot::isConnectedTo(const CyCity& kCity) const
{
	return m_pPlot ? m_pPlot->isConnectedTo(kCity.getCity()) : false;
}

bool CyPlot::isConnectedToCapital(int /*PlayerTypes*/ ePlayer) const
{
	return m_pPlot ? m_pPlot->isConnectedToCapital((PlayerTypes) ePlayer): false;
}

bool CyPlot::isAdjacentToArea(const CyArea& kArea) const
{
	return m_pPlot ? m_pPlot->isAdjacentToArea(kArea.getArea()) : false;
}

bool CyPlot::isCoastal() const
{
	return m_pPlot ? m_pPlot->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()) : false;
}

bool CyPlot::isLake() const
{
	return m_pPlot ? m_pPlot->isLake() : false;
}

bool CyPlot::isFreshWater() const
{
	return m_pPlot ? m_pPlot->isFreshWater() : false;
}

bool CyPlot::isRiverSide() const
{
	return m_pPlot ? m_pPlot->isRiverSide() : false;
}

bool CyPlot::isRiver() const
{
	return m_pPlot ? m_pPlot->isRiver() : false;
}

bool CyPlot::canHaveBonus(int /*BonusTypes*/ eBonus, bool bIgnoreLatitude) const
{
	return m_pPlot ? m_pPlot->canHaveBonus((BonusTypes)eBonus, bIgnoreLatitude) : false;
}

bool CyPlot::canHaveImprovement(int /*ImprovementTypes*/ eImprovement, int /*TeamTypes*/ eTeam, bool bPotential) const
{
	return m_pPlot ? m_pPlot->canHaveImprovement((ImprovementTypes)eImprovement, (TeamTypes)eTeam, bPotential) : false;
}

bool CyPlot::canBuild(int /*BuildTypes*/ eBuild, int /*PlayerTypes*/ ePlayer, bool bTestVisible) const
{
	return m_pPlot ? m_pPlot->canBuild((BuildTypes) eBuild, (PlayerTypes) ePlayer, bTestVisible) : false;
}

int CyPlot::getBuildTurnsLeft(int /*BuildTypes*/ eBuild, int iNowExtra, int iThenExtra) const
{
	return m_pPlot ? m_pPlot->getBuildTurnsLeft((BuildTypes) eBuild, iNowExtra, iThenExtra) : -1;
}

int CyPlot::getFeatureProduction(int /*BuildTypes*/ eBuild, int /*TeamTypes*/ eTeam, const CyCity* ppCity) const
{
	CvCity* tempCity = ppCity->getCity();
	return m_pPlot ? m_pPlot->getFeatureProduction((BuildTypes)eBuild, (TeamTypes)eTeam, &tempCity) : -1;
}

int CyPlot::getUnitPower(int /*PlayerTypes*/ eOwner) const
{
	return m_pPlot ? m_pPlot->getUnitPower((PlayerTypes)eOwner) : -1;
}

int CyPlot::defenseModifier(int iDefendTeam, bool bIgnoreBuilding, bool bHelp) const
{
	return m_pPlot ? m_pPlot->defenseModifier((TeamTypes)iDefendTeam, bIgnoreBuilding, bHelp) : -1;
}

bool CyPlot::isAdjacentPlayer(int /*PlayerTypes*/ ePlayer, bool bLandOnly) const
{
	return m_pPlot ? m_pPlot->isAdjacentPlayer((PlayerTypes)ePlayer, bLandOnly) : false;
}

int /*PlayerTypes*/ CyPlot::calculateCulturalOwner() const
{
	return m_pPlot ? m_pPlot->calculateCulturalOwner() : -1;
}

bool CyPlot::isOwned() const
{
	return m_pPlot ? m_pPlot->isOwned() : false;
}

bool CyPlot::isNPC() const
{
	return m_pPlot ? m_pPlot->isNPC() : false;
}

bool CyPlot::isHominid() const
{
	return m_pPlot ? m_pPlot->isHominid() : false;
}

bool CyPlot::isVisible(int /*TeamTypes*/ eTeam, bool bDebug) const
{
	return m_pPlot ? m_pPlot->isVisible((TeamTypes)eTeam, bDebug) : false;
}

bool CyPlot::isActiveVisible(bool bDebug) const
{
	return m_pPlot ? m_pPlot->isActiveVisible(bDebug) : false;
}

bool CyPlot::isAdjacentRevealed(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->isAdjacentRevealed((TeamTypes) eTeam) : false;
}

bool CyPlot::isAdjacentNonrevealed(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->isAdjacentNonrevealed((TeamTypes) eTeam) : false;
}

bool CyPlot::isCity() const
{
	return m_pPlot ? m_pPlot->isCity() : false;
}

bool CyPlot::isOccupation() const
{
	return m_pPlot ? m_pPlot->isOccupation() : false;
}

int CyPlot::getNumDefenders(int /*PlayerTypes*/ ePlayer) const
{
	return m_pPlot ? m_pPlot->getNumDefenders((PlayerTypes) ePlayer) : -1;
}

bool CyPlot::isVisibleEnemyUnit(int /*PlayerTypes*/ ePlayer) const
{
	return m_pPlot ? m_pPlot->isVisibleEnemyUnit((PlayerTypes) ePlayer) : false;
}

bool CyPlot::canHaveFeature(int /*FeatureTypes*/ eFeature) const
{
	return m_pPlot ? m_pPlot->canHaveFeature((FeatureTypes)eFeature) : false;
}

bool CyPlot::isBonusNetwork(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->isBonusNetwork((TeamTypes) eTeam) : false;
}

bool CyPlot::isTradeNetworkImpassable(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->isTradeNetworkImpassable((TeamTypes) eTeam) : false;
}

bool CyPlot::isTradeNetwork(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->isTradeNetwork((TeamTypes)eTeam) : false;
}

bool CyPlot::isImpassable() const
{
	return m_pPlot ? m_pPlot->isImpassable() : false;
}

int CyPlot::getX() const
{
	return m_pPlot ? (m_bIsInViewportSpace ? m_pPlot->getViewportX() : m_pPlot->getX()) : -1;
}

int CyPlot::getY() const
{
	return m_pPlot ? (m_bIsInViewportSpace ? m_pPlot->getViewportY() : m_pPlot->getY()) : -1;
}

int CyPlot::getLatitude() const
{
	return m_pPlot ? m_pPlot->getLatitude() : -1;
}

CyArea* CyPlot::area() const
{
	return m_pPlot ? new CyArea(m_pPlot->area()) : NULL;
}

CyArea* CyPlot::waterArea() const
{
	CvArea* area = m_pPlot ? m_pPlot->waterArea() : NULL;
	return area ? new CyArea(area) : NULL;
}

int CyPlot::getArea() const
{
	return m_pPlot ? m_pPlot->getArea() : -1;
}

int CyPlot::getUpgradeTimeLeft(int /*ImprovementTypes*/ eImprovement, int /*PlayerTypes*/ ePlayer) const
{
	return m_pPlot ? m_pPlot->getUpgradeTimeLeft((ImprovementTypes) eImprovement, (PlayerTypes) ePlayer) : -1;
}

void CyPlot::changeImprovementUpgradeProgress(int iChange)
{
	if (m_pPlot) m_pPlot->changeImprovementUpgradeProgress(iChange*100);
}

bool CyPlot::isStartingPlot() const
{
	return m_pPlot ? m_pPlot->isStartingPlot() : false;
}

void CyPlot::setStartingPlot(bool bNewValue)
{
	if (m_pPlot) m_pPlot->setStartingPlot(bNewValue);
}

bool CyPlot::isNOfRiver() const
{
	return m_pPlot ? m_pPlot->isNOfRiver() : false;
}

void CyPlot::setNOfRiver(bool bNewValue, CardinalDirectionTypes eRiverDir)
{
	if (m_pPlot) m_pPlot->setNOfRiver(bNewValue, eRiverDir);
}

bool CyPlot::isWOfRiver() const
{
	return m_pPlot ? m_pPlot->isWOfRiver() : false;
}

void CyPlot::setWOfRiver(bool bNewValue, CardinalDirectionTypes eRiverDir)
{
	if (m_pPlot) m_pPlot->setWOfRiver(bNewValue, eRiverDir);
}

CardinalDirectionTypes CyPlot::getRiverWEDirection() const
{
	return m_pPlot ? m_pPlot->getRiverWEDirection() : NO_CARDINALDIRECTION;
}

CardinalDirectionTypes CyPlot::getRiverNSDirection() const
{
	return m_pPlot ? m_pPlot->getRiverNSDirection() : NO_CARDINALDIRECTION;
}

bool CyPlot::isPotentialCityWork() const
{
	return m_pPlot ? m_pPlot->isPotentialCityWork() : false;
}

int CyPlot::getOwner() const
{
	return m_pPlot ? m_pPlot->getOwner() : -1;
}

void CyPlot::setOwner(int /*PlayerTypes*/ eNewValue)
{
	if (m_pPlot) m_pPlot->setOwner((PlayerTypes) eNewValue, true, true);
}

PlotTypes CyPlot::getPlotType() const
{
	return m_pPlot ? m_pPlot->getPlotType() : NO_PLOT;
}

bool CyPlot::isWater() const
{
	return m_pPlot ? m_pPlot->isWater() : false;
}

bool CyPlot::isFlatlands() const
{
	return m_pPlot ? m_pPlot->isFlatlands() : false;
}

bool CyPlot::isHills() const
{
	return m_pPlot ? m_pPlot->isHills() : false;
}

bool CyPlot::isPeak() const
{
	return m_pPlot ? m_pPlot->isAsPeak() : false;
}

void CyPlot::setPlotType(PlotTypes eNewValue, bool bRecalculate, bool bRebuildGraphics)
{
	if (m_pPlot) m_pPlot->setPlotType(eNewValue, bRecalculate, bRebuildGraphics);
}

int /*TerrainTypes*/ CyPlot::getTerrainType() const
{
	return m_pPlot ? m_pPlot->getTerrainType() : -1;
}

void CyPlot::setTerrainType(int /*TerrainTypes*/ eNewValue, bool bRecalculate, bool bRebuildGraphics)
{
	if (m_pPlot) m_pPlot->setTerrainType((TerrainTypes)eNewValue, bRecalculate, bRebuildGraphics);
}

int /*FeatureTypes*/ CyPlot::getFeatureType() const
{
	return m_pPlot ? m_pPlot->getFeatureType() : -1;
}

void CyPlot::setFeatureType(int /*FeatureTypes*/ eNewValue, int iVariety)
{
	if (m_pPlot) m_pPlot->setFeatureType((FeatureTypes)eNewValue, iVariety);
}

int CyPlot::getFeatureVariety() const
{
	return m_pPlot ? m_pPlot->getFeatureVariety() : -1;
}

int /*BonusTypes*/ CyPlot::getBonusType(int /*TeamTypes*/ eTeam) const
{
	return m_pPlot ? m_pPlot->getBonusType((TeamTypes)eTeam) : -1;
}

void CyPlot::setBonusType(int /*BonusTypes*/ eNewValue)
{
	if (m_pPlot) m_pPlot->setBonusType((BonusTypes)eNewValue);
}

int /*ImprovementTypes*/ CyPlot::getImprovementType() const
{
	return m_pPlot ? m_pPlot->getImprovementType() : -1;
}

void CyPlot::setImprovementType(int /*ImprovementTypes*/ eNewValue)
{
	if (m_pPlot) m_pPlot->setImprovementType((ImprovementTypes)eNewValue);
}

int /*RouteTypes*/ CyPlot::getRouteType() const
{
	return m_pPlot ? m_pPlot->getRouteType() : -1;
}

void CyPlot::setRouteType(int /*RouteTypes*/ eNewValue)
{
	if (m_pPlot) m_pPlot->setRouteType((RouteTypes) eNewValue, true);
}

CyCity* CyPlot::getPlotCity() const
{
	CvCity* city = m_pPlot ? m_pPlot->getPlotCity() : NULL;
	return city ? new CyCity(city) : NULL;
}

CyCity* CyPlot::getWorkingCity() const
{
	CvCity* city = m_pPlot ? m_pPlot->getWorkingCity() : NULL;
	return city ? new CyCity(city) : NULL;
}

int CyPlot::getRiverID() const
{
	return m_pPlot ? m_pPlot->getRiverID() : -1;
}

void CyPlot::setRiverID(int iNewValue)
{
	if (m_pPlot) m_pPlot->setRiverID(iNewValue);
}

int CyPlot::getYield(YieldTypes eIndex) const
{
	// THE READ EDGE -- ×100 becomes human here and nowhere upstream (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
	return m_pPlot ? m_pPlot->getYield(eIndex) : -1;   // getYield IS the human edge now -- no second reduce
}

void CyPlot::setExtraYield(YieldTypes eYield, short iExtraYield)
{
	if (m_pPlot) m_pPlot->setExtraYield(eYield, iExtraYield);
}

int CyPlot::calculateBestNatureYield(YieldTypes eIndex, TeamTypes eTeam) const
{
	return m_pPlot ? m_pPlot->calculateBestNatureYield(eIndex, eTeam) : -1;
}

int CyPlot::calculateTotalBestNatureYield(TeamTypes eTeam) const
{
	return m_pPlot ? m_pPlot->calculateTotalBestNatureYield(eTeam) : -1;
}

int CyPlot::calculateImprovementYieldChange(int /*ImprovementTypes*/ eImprovement, YieldTypes eYield) const
{
	return m_pPlot ? m_pPlot->calculateImprovementYieldChange((ImprovementTypes) eImprovement, eYield) : -1;
}

bool CyPlot::hasYield() const
{
	return m_pPlot ? m_pPlot->hasYield() : false;
}

int64_t CyPlot::getCulture(int /*PlayerTypes*/ eIndex) const
{
	return m_pPlot ? m_pPlot->getCulture((PlayerTypes)eIndex) : -1;
}

int64_t CyPlot::countTotalCulture() const
{
	return m_pPlot ? m_pPlot->countTotalCulture() : -1;
}

int CyPlot::calculateCulturePercent(int /*PlayerTypes*/ eIndex) const
{
	return m_pPlot ? m_pPlot->calculateCulturePercent((PlayerTypes)eIndex) : -1;
}

void CyPlot::setCulture(int /*PlayerTypes*/ eIndex, int iChange, bool bUpdate)
{
	if (m_pPlot) m_pPlot->setCulture((PlayerTypes)eIndex, iChange, bUpdate, true);
}

void CyPlot::changeCulture(int /*PlayerTypes*/ eIndex, int iChange, bool bUpdate)
{
	if (m_pPlot) m_pPlot->changeCulture((PlayerTypes)eIndex, iChange, bUpdate);
}

int CyPlot::getFoundValue(int /*PlayerTypes*/ eIndex) const
{
	return m_pPlot ? m_pPlot->getFoundValue((PlayerTypes)eIndex) : -1;
}

int /*PlayerTypes*/ CyPlot::getRevealedOwner(int /*TeamTypes*/ eTeam, bool bDebug) const
{
	return m_pPlot ? m_pPlot->getRevealedOwner((TeamTypes)eTeam, bDebug) : -1;
}

bool CyPlot::isRiverCrossing(DirectionTypes eIndex) const
{
	return m_pPlot ? m_pPlot->isRiverCrossing(eIndex) : false;
}

bool CyPlot::isRevealed(int /*TeamTypes*/ eTeam, bool bDebug) const
{
	return m_pPlot && eTeam != NO_TEAM ? m_pPlot->isRevealed((TeamTypes)eTeam, bDebug) : false;
}

void CyPlot::setRevealed(int /*TeamTypes*/ eTeam, bool bNewValue, bool bTerrainOnly, int /*TeamTypes*/ eFromTeam)
{
	if (m_pPlot) m_pPlot->setRevealed((TeamTypes)eTeam, bNewValue, bTerrainOnly, (TeamTypes)eFromTeam, true);
}

int CyPlot::getInvisibleVisibilityCount(int /*TeamTypes*/ eTeam, int /*InvisibleTypes*/ eInvisible) const
{
	return m_pPlot ? m_pPlot->getInvisibleVisibilityCount((TeamTypes) eTeam, (InvisibleTypes) eInvisible) : -1;
}

bool CyPlot::isSpotterInSight(int /*TeamTypes*/ eTeam, int /*InvisibleTypes*/ eInvisible) const
{
	return m_pPlot ? m_pPlot->isSpotterInSight((TeamTypes) eTeam, (InvisibleTypes) eInvisible) : false;
}

void CyPlot::changeInvisibleVisibilityCount(int iTeam, int iInvisible, int iChange)
{
	if (m_pPlot) m_pPlot->changeInvisibleVisibilityCount((TeamTypes) iTeam, (InvisibleTypes) iInvisible, iChange);
}

python::list CyPlot::units() const
{
	PROFILE_EXTRA_FUNC();
	python::list list = python::list();

	if (m_pPlot)
	{
		foreach_(CvUnit* unit, m_pPlot->units())
		{
			list.append(CyUnit(unit));
		}
	}
	return list;
}

int CyPlot::getNumUnits() const
{
	return m_pPlot ? m_pPlot->getNumUnits() : -1;
}

CyUnit* CyPlot::getUnit(int iIndex) const
{
	return m_pPlot ? new CyUnit(m_pPlot->getUnitByIndex(iIndex)) : NULL;
}

std::string CyPlot::getScriptData() const
{
	return m_pPlot ? m_pPlot->getScriptData() : "";
}

void CyPlot::setScriptData(std::string szNewValue)
{
	if (m_pPlot) m_pPlot->setScriptData(szNewValue.c_str());
}

int CyPlot::getRevoltProtection() const
{
	return m_pPlot ? m_pPlot->getRevoltProtection() : 0;
}

bool CyPlot::isInViewport() const
{
	return m_pPlot ? m_pPlot->isInViewport() : false;
}

CyPlot* CyPlot::cloneToViewport() const
{
	return new CyPlot(m_pPlot, true);
}

python::list CyPlot::adjacent() const
{
	PROFILE_EXTRA_FUNC();
	python::list list = python::list();

	if (m_pPlot)
	{
		foreach_(CvPlot* plot, m_pPlot->adjacent())
		{
			list.append(CyPlot(plot));
		}
	}
	return list;
}

python::list CyPlot::rect(int halfWid, int halfHgt) const
{
	PROFILE_EXTRA_FUNC();
	python::list list = python::list();

	if (m_pPlot)
	{
		foreach_(CvPlot* plot, m_pPlot->rect(halfWid, halfHgt))
		{
			list.append(CyPlot(plot));
		}
	}
	return list;
}

//
//	THE MAP-SCRIPT BOUNDARY (handles, not infos) -- see CyMap.cpp for the ruling.
//
python::list CyPlot::getUnitIds() const
{
	python::list list = python::list();
	if (m_pPlot == NULL)
	{
		return list;
	}
	const int iNumUnits = m_pPlot->getNumUnits();
	for (int i = 0; i < iNumUnits; ++i)
	{
		const CvUnit* pUnit = m_pPlot->getUnitByIndex(i);
		if (pUnit == NULL)
		{
			continue;
		}
		python::list pair = python::list();
		pair.append((int)pUnit->getOwner());
		pair.append(pUnit->getID());
		list.append(pair);
	}
	return list;
}

void CyPlot::pythonPublish()
{
	// The 3D-point VALUE TYPE the kept point surfaces traffic in: getPoint hands it out, and the graphics-side
	// consumers (CyEngine::triggerEffect, CyCamera::JustLookAt, CyAudioGame's 3D sound coords) take it back.
	// Its registration was lost with the legacy structs interface while all of those surfaces stayed published,
	// so every plot.getPoint() raised "No to_python converter found for C++ type: class NiPoint3".
	python::class_<NiPoint3>("NiPoint3")
		.def_readwrite("x", &NiPoint3::x)
		.def_readwrite("y", &NiPoint3::y)
		.def_readwrite("z", &NiPoint3::z);
	// A game-object HANDLE, not an info. Its def set publishes through the loader, whose split across
	// translation units keeps any single one inside the VC7.1 compiler's limits.
	python::class_<CyPlot> plot("CyPlot", python::no_init);
	plot.def("getUnitIds", &CyPlot::getUnitIds);
	CvPythonPlotLoader::CyPlotPythonInterface1(plot);
}
