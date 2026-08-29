#include "CvGameCoreDLL.h"
#include "CyPyList.h"
#include "Defines/CvDefines.h"
#include "UI/CityOutputHistory.h"
#include "Engine/CvArea.h"
#include "Engine/CvCity.h"
#include "CyArea.h"
#include <boost/python/class.hpp>
#include "CyCity.h"
#include "CyPlot.h"
#include "AI/CvGameAI.h"
#include "CyUnit.h"
#include "Engine/CvGame.h"
#include "Engine/CvUnitSelectionCriteria.h"
#include "Infos/CvInfoKinds.h"   // the NUM_<FAMILY>_KINDS the group reads are sized by
#include "AI/BetterBTSAI.h"      // PERF_SCOPE -- the ONE instrument, gated by gPerfLogLevel
#include "Engine/CvPlot.h"          // the ring-ordered work-area read
#include "Data/CvInfoValuation.h"   // CityRateTerms -- the ONE city-yield decomposition
#include "Engine/CvUnit.h"                           // addUnitProductionExperience -- the unit it credits
#include "AI/CvPlayerAI.h"                           // GET_PLAYER
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"  // select -- the engine action this relays

//
// Python wrapper class for CvCity
//

CyCity::CyCity(CvCity* pCity) : m_pCity(pCity)
{
	FAssert(m_pCity != NULL);
}

void CyCity::kill()
{
	m_pCity->kill(true);
}

void CyCity::setRevolutionIndex(int iNewValue)
{
	m_pCity->setRevolutionIndex(iNewValue);
}

void CyCity::changeRevolutionIndex(int iChange)
{
	m_pCity->changeRevolutionIndex(iChange);
}

int CyCity::getLocalRevIndex() const
{
	return m_pCity->getLocalRevIndex();
}

void CyCity::setLocalRevIndex(int iNewValue)
{
	m_pCity->setLocalRevIndex(iNewValue);
}

void CyCity::setRevIndexAverage(int iNewValue)
{
	m_pCity->setRevIndexAverage(iNewValue);
}

void CyCity::updateRevIndexAverage()
{
	m_pCity->updateRevIndexAverage();
}

int CyCity::getRevIndexDistanceMod() const
{
	return m_pCity->getRevIndexDistanceMod();
}

int CyCity::getReinforcementCounter() const
{
	return m_pCity->getReinforcementCounter();
}

void CyCity::setReinforcementCounter(int iNewValue)
{
	m_pCity->setReinforcementCounter(iNewValue);
}

void CyCity::changeReinforcementCounter(int iChange)
{
	m_pCity->changeReinforcementCounter(iChange);
}

int CyCity::getRevolutionCounter() const
{
	return m_pCity->getRevolutionCounter();
}

void CyCity::setRevolutionCounter(int iNewValue)
{
	m_pCity->setRevolutionCounter(iNewValue);
}

void CyCity::changeRevolutionCounter(int iChange)
{
	m_pCity->changeRevolutionCounter(iChange);
}

int CyCity::countNumImprovedPlots() const
{
	return m_pCity->countNumImprovedPlots();
}

int CyCity::findBaseYieldRateRank(YieldTypes eYield) const
{
	return m_pCity->findBaseYieldRateRank(eYield);
}

bool CyCity::canCreate(ProjectTypes eProject, bool bContinue, bool bTestVisible) const
{
	return m_pCity->canCreate(eProject, bContinue, bTestVisible);
}

bool CyCity::canMaintain(ProcessTypes eProcess) const
{
	return m_pCity->canMaintain(eProcess);
}

int CyCity::getFoodTurnsLeft() const
{
	return m_pCity->getFoodTurnsLeft();
}

bool CyCity::isProduction() const
{
	return m_pCity->isProduction();
}

bool CyCity::isProductionUnit() const
{
	return m_pCity->isProductionUnit();
}

bool CyCity::isProductionBuilding() const
{
	return m_pCity->isProductionBuilding();
}

bool CyCity::isProductionProject() const
{
	return m_pCity->isProductionProject();
}

bool CyCity::isProductionProcess() const
{
	return m_pCity->isProductionProcess();
}

int CyCity::getProductionExperience(UnitTypes eUnit) const
{
	return m_pCity->getProductionExperience(eUnit);
}

void CyCity::addProductionExperience(const CyUnit& kUnit, bool bConscript)
{
	m_pCity->addProductionExperience(kUnit.getUnit(), bConscript);
}

UnitTypes CyCity::getProductionUnit() const
{
	return m_pCity->getProductionUnit();
}

BuildingTypes CyCity::getProductionBuilding() const
{
	return m_pCity->getProductionBuilding();
}

ProjectTypes CyCity::getProductionProject() const
{
	return m_pCity->getProductionProject();
}

ProcessTypes CyCity::getProductionProcess() const
{
	return m_pCity->getProductionProcess();
}

std::wstring CyCity::getProductionName() const
{
	return m_pCity->getProductionName();
}

std::wstring CyCity::getProductionNameKey() const
{
	return m_pCity->getProductionNameKey();
}

int CyCity::getFirstBuildingOrder(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getFirstBuildingOrder((BuildingTypes)eBuilding);
}

int CyCity::getProductionProgress() const
{
	return m_pCity->getProductionProgress();
}

int CyCity::getProductionNeeded() const
{
	return m_pCity->getProductionNeeded();
}

int CyCity::getProductionTurnsLeft() const
{
	return m_pCity->getProductionTurnsLeft();
}

void CyCity::setProductionProgress(int iNewValue)
{
	m_pCity->setProductionProgress(iNewValue);
}

void CyCity::changeProduction(int iChange)
{
	m_pCity->changeProduction(iChange);
}

int CyCity::getCurrentProductionDifference(bool bIgnoreFood, bool bOverflow) const
{
	return m_pCity->getCurrentProductionDifference(
		(bIgnoreFood? ProductionCalc::None : ProductionCalc::FoodProduction) |
		(bOverflow? ProductionCalc::Overflow : ProductionCalc::None)
	);
}

bool CyCity::canHurry(int /*HurryTypes*/ iHurry, bool bTestVisible) const
{
	return m_pCity->canHurry((HurryTypes)iHurry, bTestVisible);
}

int /*UnitTypes*/ CyCity::getConscriptUnit() const
{
	return m_pCity->getConscriptUnit();
}

int CyCity::flatConscriptAngerLength() const
{
	return m_pCity->flatConscriptAngerLength();
}

bool CyCity::canConscript() const
{
	return m_pCity->canConscript();
}

int /*HandicapTypes*/ CyCity::getHandicapType() const
{
	return m_pCity->getHandicapType();
}

int /*CivilizationTypes*/ CyCity::getCivilizationType() const
{
	return m_pCity->getCivilizationType();
}

int /*LeaderHeadTypes*/ CyCity::getPersonalityType() const
{
	return m_pCity->getPersonalityType();
}

int /*ArtStyleTypes*/ CyCity::getArtStyleType() const
{
	return m_pCity->getArtStyleType();
}

bool CyCity::isNPC() const
{
	return m_pCity->isNPC();
}

bool CyCity::isHuman() const
{
	return m_pCity->isHuman();
}

bool CyCity::isVisible(int /*TeamTypes*/ eTeam, bool bDebug) const
{
	return m_pCity->isVisible((TeamTypes) eTeam, bDebug);
}

bool CyCity::isCapital() const
{
	return m_pCity->isCapital();
}

bool CyCity::isCoastal(int iMinWaterSize) const
{
	return m_pCity->isCoastal(iMinWaterSize);
}

bool CyCity::isDisorder() const
{
	return m_pCity->isDisorder();
}

bool CyCity::isHolyCityByType(int /*ReligionTypes*/ iIndex) const
{
	return m_pCity->isHolyCity((ReligionTypes) iIndex);
}

bool CyCity::isHolyCity() const
{
	return m_pCity->isHolyCity();
}

bool CyCity::isHeadquartersByType(int /*CorporationTypes*/ iIndex) const
{
	return m_pCity->isHeadquarters((CorporationTypes) iIndex);
}

int CyCity::getWarWearinessPercentAnger() const
{
	return m_pCity->getWarWearinessPercentAnger();
}

int CyCity::getRevIndexPercentAnger() const
{
	return m_pCity->getRevIndexPercentAnger();
}

int CyCity::foodDifference(bool bBottom) const
{
	// THE READ EDGE -- the food plane is x100 native; Python sees whole food (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
	return m_pCity->foodDifference(bBottom) / 100;
}

int CyCity::growthThreshold() const
{
	return m_pCity->growthThreshold();
}

int CyCity::productionLeft() const
{
	return m_pCity->productionLeft();
}

int64_t CyCity::getHurryGold(int /*HurryTypes*/ iHurry) const
{
	return m_pCity->getHurryGold((HurryTypes)iHurry);
}

int CyCity::hurryPopulation(int /*HurryTypes*/ iHurry) const
{
	return m_pCity->hurryPopulation((HurryTypes)iHurry);
}

int CyCity::hurryProduction(int /*HurryTypes*/ iHurry) const
{
	return m_pCity->hurryProduction((HurryTypes)iHurry);
}

int CyCity::flatHurryAngerLength() const
{
	return m_pCity->flatHurryAngerLength();
}

int CyCity::hasBuilding(int /*BuildingTypes*/ iIndex) const
{
	return m_pCity->hasBuilding((BuildingTypes) iIndex);
}

bool CyCity::isActiveBuilding(int /*BuildingTypes*/ iIndex) const
{
	return m_pCity->isActiveBuilding((BuildingTypes) iIndex);
}


int CyCity::getID() const
{
	return m_pCity->getID();
}

int CyCity::getX() const
{
	return m_pCity->getX();
}

int CyCity::getY() const
{
	return m_pCity->getY();
}

CyPlot* CyCity::plot() const
{
	if (m_pCity->plot())
	{
		return new CyPlot(m_pCity->plot());
	}
	return NULL;
}

bool CyCity::isConnectedTo(const CyCity& kCity) const
{
	return m_pCity->isConnectedTo(kCity.getCity());
}

bool CyCity::isConnectedToCapital(int /*PlayerTypes*/ ePlayer) const
{
	return m_pCity->isConnectedToCapital((PlayerTypes)ePlayer);
}

CyArea* CyCity::area() const
{
	return new CyArea(m_pCity->area());
}

int CyCity::getGameDateFounded() const
{
	CvGame& GAME = GC.getGame();
	bool bHistoricalCalendar = GAME.isModderGameOption(MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR);
	return m_pCity->getGameDateFounded(bHistoricalCalendar);
}

int CyCity::getPopulation() const
{
	return m_pCity->getPopulation();
}

int64_t CyCity::getRealPopulation() const
{
	return m_pCity->getRealPopulation();
}

int CyCity::getHighestPopulation() const
{
	return m_pCity->getHighestPopulation();
}

void CyCity::setHighestPopulation(int iNewValue)
{
	m_pCity->setHighestPopulation(iNewValue);
}

int CyCity::getWorkingPopulation() const
{
	return m_pCity->getWorkingPopulation();
}

int CyCity::getSpecialistPopulation() const
{
	return m_pCity->getSpecialistPopulation();
}

int CyCity::getNumGreatPeople() const
{
	return m_pCity->getNumGreatPeople();
}

int CyCity::getBaseGreatPeopleRate() const
{
	// THE READ EDGE -- ×100 becomes human here (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
	return m_pCity->getBaseGreatPeopleRate() / 100;
}

int CyCity::getGreatPeopleRate() const
{
	// THE READ EDGE -- the modified rate carries the base's ×100, so it reduces here too.
	return m_pCity->getGreatPeopleRate() / 100;
}

int CyCity::getGreatPeopleProgress() const
{
	return m_pCity->getGreatPeopleProgress();
}

void CyCity::changeGreatPeopleProgress(int iChange)
{
	m_pCity->changeGreatPeopleProgress(iChange);
}

bool CyCity::isGovernmentCenter() const
{
	return m_pCity->isGovernmentCenter();
}

int CyCity::getMaintenance() const
{
	return (int)m_pCity->getMaintenance();
}

int CyCity::getMaintenanceTimes100() const
{
	return (int)m_pCity->getMaintenanceTimes100();
}

void CyCity::changeEspionageHealthCounter(int iChange)
{
	m_pCity->changeEspionageHealthCounter(iChange);
}

void CyCity::changeEspionageHappinessCounter(int iChange)
{
	m_pCity->changeEspionageHappinessCounter(iChange);
}

int CyCity::getMilitaryHappinessUnits() const
{
	return m_pCity->getMilitaryHappinessUnits();
}

int CyCity::getExtraHappiness() const
{
	return m_pCity->getExtraHappiness();
}

void CyCity::changeExtraHappiness(int iChange)
{
	m_pCity->changeExtraHappiness(iChange);
}

int CyCity::getExtraHealth() const
{
	return m_pCity->getExtraHealth();
}

void CyCity::changeExtraHealth(int iChange)
{
	m_pCity->changeExtraHealth(iChange);
}

int CyCity::getHurryAngerTimer() const
{
	return m_pCity->getHurryAngerTimer();
}

int CyCity::getRevRequestAngerTimer() const
{
	return m_pCity->getRevRequestAngerTimer();
}

void CyCity::changeRevRequestAngerTimer(int iChange)
{
	m_pCity->changeRevRequestAngerTimer(iChange);
}

void CyCity::changeRevSuccessTimer(int iChange)
{
	m_pCity->changeRevSuccessTimer(iChange);
}

int CyCity::getConscriptAngerTimer() const
{
	return m_pCity->getConscriptAngerTimer();
}

void CyCity::changeConscriptAngerTimer(int iChange)
{
	m_pCity->changeConscriptAngerTimer(iChange);
}

void CyCity::changeDefyResolutionAngerTimer(int iChange)
{
	m_pCity->changeDefyResolutionAngerTimer(iChange);
}

void CyCity::changeHappinessTimer(int iChange)
{
	m_pCity->changeHappinessTimer(iChange);
}

int CyCity::getFood() const
{
	return m_pCity->getFood();
}

int CyCity::getMaxProductionOverflow() const
{
	return m_pCity->getMaxProductionOverflow();
}

int CyCity::getOverflowProduction() const
{
	return m_pCity->getOverflowProduction();
}

void CyCity::setOverflowProduction(int iNewValue)
{
	m_pCity->setOverflowProduction(iNewValue);
}

int CyCity::getFeatureProduction() const
{
	return m_pCity->getFeatureProduction();
}

void CyCity::setFeatureProduction(int iNewValue)
{
	m_pCity->setFeatureProduction(iNewValue);
}

int CyCity::getExtraTradeRoutes() const
{
	return m_pCity->getExtraTradeRoutes();
}

int CyCity::getMaxTradeRoutes() const
{
	return m_pCity->getMaxTradeRoutes();
}

int CyCity::getBuildingDefense() const
{
	return m_pCity->getBuildingDefense();
}

int CyCity::getMaxAirlift() const
{
	return m_pCity->getMaxAirlift();
}


bool CyCity::isPowered() const
{
	return m_pCity->isPowered();
}

void CyCity::changeDefenseDamage(int iChange)
{
	m_pCity->changeDefenseDamage(iChange);
}

int CyCity::getTotalDefense(bool bIgnoreBuilding) const
{
	return m_pCity->getTotalDefense(bIgnoreBuilding);
}

int CyCity::getDefenseModifier(bool bIgnoreBuilding) const
{
	return m_pCity->getDefenseModifier(bIgnoreBuilding);
}

bool CyCity::isOccupation() const
{
	return m_pCity->isOccupation();
}

void CyCity::changeOccupation(int iChange)
{
	m_pCity->changeOccupationTimer(iChange);
}

bool CyCity::isNeverLost() const
{
	return m_pCity->isNeverLost();
}

void CyCity::setNeverLost(bool bNewValue)
{
	m_pCity->setNeverLost(bNewValue);
}

bool CyCity::isBombarded() const
{
	return m_pCity->isBombarded();
}

void CyCity::setBombarded(bool bNewValue)
{
	m_pCity->setBombarded(bNewValue);
}

bool CyCity::isDrafted() const
{
	return m_pCity->isDrafted();
}

void CyCity::setDrafted(bool bNewValue)
{
	m_pCity->setDrafted(bNewValue);
}

bool CyCity::isAirliftTargeted() const
{
	return m_pCity->isAirliftTargeted();
}

void CyCity::setAirliftTargeted(bool bNewValue)
{
	m_pCity->setAirliftTargeted(bNewValue);
}

bool CyCity::isCitizensAutomated() const
{
	return m_pCity->isCitizensAutomated();
}

void CyCity::setCitizensAutomated(bool bNewValue)
{
	m_pCity->setCitizensAutomated(bNewValue);
}

bool CyCity::isProductionAutomated() const
{
	return m_pCity->isProductionAutomated();
}

void CyCity::setProductionAutomated(bool bNewValue)
{
	m_pCity->setProductionAutomated(bNewValue);
}

bool CyCity::isWallOverride() const
{
	return m_pCity->isWallOverride();
}

void CyCity::setWallOverride(bool bOverride)
{
	m_pCity->setWallOverride(bOverride);
}

bool CyCity::isPlundered() const
{
	return m_pCity->isPlundered();
}

void CyCity::setPlundered(bool bNewValue)
{
	m_pCity->setPlundered(bNewValue);
}

int /*PlayerTypes*/ CyCity::getOwner() const
{
	return m_pCity->getOwner();
}

int /*TeamTypes*/ CyCity::getTeam() const
{
	return m_pCity->getTeam();
}

int /*PlayerTypes*/ CyCity::getPreviousOwner() const
{
	return m_pCity->getPreviousOwner();
}

int /*PlayerTypes*/ CyCity::getOriginalOwner() const
{
	return m_pCity->getOriginalOwner();
}

int /*CultureLevelTypes*/ CyCity::getCultureLevel() const
{
	return m_pCity->getCultureLevel();
}

int CyCity::getCultureThreshold() const
{
	return m_pCity->getCultureThreshold();
}

int CyCity::getBaseYieldRateModifier(int /*YieldTypes*/ eIndex, int iExtra) const
{
	return m_pCity->getBaseYieldRateModifier((YieldTypes)eIndex, iExtra);
}

int CyCity::getCommerceRateModifier(int /*CommerceTypes*/ eIndex) const
{
	return m_pCity->getTotalCommerceRateModifier((CommerceTypes)eIndex);
}


PlayerTypes CyCity::findHighestCulture() const
{
	return m_pCity->findHighestCulture();
}

int CyCity::calculateCulturePercent(int /*PlayerTypes*/ eIndex) const
{
	return m_pCity->calculateCulturePercent((PlayerTypes)eIndex);
}

int CyCity::getNumRevolts(int playerIdx) const
{
	return m_pCity->getNumRevolts((PlayerTypes)playerIdx);
}

void CyCity::changeNumRevolts(int playerIdx, int iChange)
{
	m_pCity->changeNumRevolts((PlayerTypes)playerIdx, iChange);
}

bool CyCity::isRevealed(int /*TeamTypes*/ eIndex, bool bDebug) const
{
	return m_pCity->isRevealed((TeamTypes)eIndex, bDebug);
}

std::wstring CyCity::getName() const
{
	return m_pCity->getName();
}

std::wstring CyCity::getNameKey() const
{
	return m_pCity->getNameKey();
}

int CyCity::getFreeBonus(int /*BonusTypes*/ eIndex) const
{
	return m_pCity->getFreeBonus((BonusTypes)eIndex);
}

void CyCity::changeFreeBonus(int /*BonusTypes*/ eIndex, int iChange)
{
	// The Python callers (random events, WorldBuilder) ARE the event/WB grant path -- the persisted store.
	m_pCity->changeFreeBonusEvent((BonusTypes)eIndex, iChange);
}

int CyCity::getNumBonuses(int /*BonusTypes*/ iBonus) const
{
	return m_pCity->getNumBonuses((BonusTypes) iBonus);
}

bool CyCity::hasBonus(int /*BonusTypes*/ iBonus) const
{
	return m_pCity->hasBonus((BonusTypes) iBonus);
}

int CyCity::getProgressOnBuilding(int /*BuildingTypes*/ iIndex) const
{
	return m_pCity->getProgressOnBuilding((BuildingTypes) iIndex);
}

void CyCity::setProgressOnBuilding(int /*BuildingTypes*/ iIndex, int iNewValue)
{
	m_pCity->setProgressOnBuilding((BuildingTypes) iIndex, std::max(0, iNewValue));
}

int CyCity::getProgressOnUnit(int iIndex) const
{
	return m_pCity->getProgressOnUnit((UnitTypes) iIndex);
}

void CyCity::setProgressOnUnit(int iIndex, int iNewValue)
{
	m_pCity->setProgressOnUnit((UnitTypes)iIndex, iNewValue);
}

void CyCity::setGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iNewValue)
{
	m_pCity->setGreatPeopleUnitProgress((UnitTypes) iIndex, iNewValue);
}

void CyCity::changeGreatPeopleUnitProgress(int /*UnitTypes*/ iIndex, int iChange)
{
	m_pCity->changeGreatPeopleUnitProgress((UnitTypes) iIndex, iChange);
}

int CyCity::getSpecialistCount(int /*SpecialistTypes*/ eIndex) const
{
	return m_pCity->getSpecialistCount((SpecialistTypes)eIndex);
}

int CyCity::getForceSpecialistCount(int /*SpecialistTypes*/ eIndex) const
{
	return m_pCity->getForceSpecialistCount((SpecialistTypes)eIndex);
}

int CyCity::getReligionInfluence(int /*ReligionTypes*/ iIndex) const
{
	return m_pCity->getReligionInfluence((ReligionTypes) iIndex);
}

void CyCity::setForceSpecialistCount(int /*SpecialistTypes*/ eIndex, int iNewValue)
{
	m_pCity->setForceSpecialistCount((SpecialistTypes)eIndex, iNewValue);
}

int CyCity::getFreeSpecialistCount(int /*SpecialistTypes*/ eIndex) const
{
	return m_pCity->getFreeSpecialistCount((SpecialistTypes)eIndex);
}

void CyCity::changeFreeSpecialistCount(int /*SpecialistTypes*/ eIndex, int iChange)
{
	m_pCity->changeFreeSpecialistCount((SpecialistTypes)eIndex, iChange, true);
}

int CyCity::getAddedFreeSpecialistCount(int /*SpecialistTypes*/ eIndex) const
{
	return m_pCity->getAddedFreeSpecialistCount((SpecialistTypes)eIndex);
}

void CyCity::changeReligionInfluence(int /*ReligionTypes*/ iIndex, int iChange)
{
	m_pCity->changeReligionInfluence((ReligionTypes) iIndex, iChange);
}

int CyCity::getEspionageDefenseModifier() const
{
	return m_pCity->getEspionageDefenseModifier();
}

bool CyCity::isHasReligion(int /*ReligionTypes*/ iIndex) const
{
	return m_pCity->isHasReligion((ReligionTypes) iIndex);
}

void CyCity::setHasReligion(int /*ReligionTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows)
{
	m_pCity->setHasReligion((ReligionTypes) iIndex, bNewValue, bAnnounce, bArrows);
}

bool CyCity::isHasCorporation(int /*CorporationTypes*/ iIndex) const
{
	return m_pCity->isHasCorporation((CorporationTypes) iIndex);
}

void CyCity::setHasCorporation(int /*CorporationTypes*/ iIndex, bool bNewValue, bool bAnnounce, bool bArrows)
{
	m_pCity->setHasCorporation((CorporationTypes) iIndex, bNewValue, bAnnounce, bArrows);
}

bool CyCity::isActiveCorporation(int /*CorporationTypes*/ eCorporation) const
{
	return m_pCity->isActiveCorporation((CorporationTypes) eCorporation);
}

CyCity* CyCity::getTradeCity(int iIndex) const
{
	CvCity* city = m_pCity->getTradeCity(iIndex);
	return city ? new CyCity(city) : NULL;
}


void CyCity::clearOrderQueue()
{
	m_pCity->clearOrderQueue();
}

void CyCity::popOrder(int iNum, bool bFinish, bool bChoose)
{
	m_pCity->popOrder(iNum, bFinish, bChoose);
}

int CyCity::getOrderQueueLength() const
{
	return m_pCity->getOrderQueueLength();
}

OrderData CyCity::getOrderFromQueue(int iIndex) const
{
	return m_pCity->getOrderAt(iIndex);
}

bool CyCity::AI_isEmphasize(int iEmphasizeType) const
{
	return m_pCity->AI_isEmphasize((EmphasizeTypes)iEmphasizeType);
}

std::string CyCity::getScriptData() const
{
	return m_pCity->getScriptData();
}

void CyCity::setBuildingYieldChange(int /*BuildingTypes*/ eBuilding, int /*YieldTypes*/ eYield, int iChange)
{
	m_pCity->setBuildingYieldChange((BuildingTypes)eBuilding, (YieldTypes)eYield, iChange);
}

void CyCity::setBuildingCommerceChange(int /*BuildingTypes*/ eBuilding, int /*CommerceTypes*/ eCommerce, int iChange)
{
	m_pCity->setBuildingCommerceChange((BuildingTypes)eBuilding, (CommerceTypes)eCommerce, iChange);
}

int CyCity::getBuildingHappyChange(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHappyChange((BuildingTypes)eBuilding);
}

int CyCity::getBuildingHealthChange(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHealthChange((BuildingTypes)eBuilding);
}

int CyCity::getArea() const
{
	return m_pCity->getArea();
}

void CyCity::changeEventAnger(int iChange)
{
	m_pCity->changeEventAnger(iChange);
}


bool CyCity::isAutomatedCanBuild(int /*BuildTypes*/ eIndex) const
{
	return m_pCity->isAutomatedCanBuild((BuildTypes)eIndex);
}

void CyCity::setAutomatedCanBuild(int /*BuildTypes*/ eIndex, bool bNewValue)
{
	m_pCity->setAutomatedCanBuild((BuildTypes)eIndex, bNewValue);
}


bool CyCity::getBuildingListFilterActive(int eFilter)
{
	return m_pCity->getBuildingListFilterActive((BuildingFilterTypes)eFilter);
}

int CyCity::getBuildingListGrouping()
{
	return m_pCity->getBuildingListGrouping();
}

int CyCity::getBuildingListType(int iGroup, int iPos)
{
	return m_pCity->getBuildingListType(iGroup, iPos);
}

bool CyCity::getUnitListFilterActive(int eFilter)
{
	return m_pCity->getUnitListFilterActive((UnitFilterTypes)eFilter);
}

int CyCity::getUnitListType(int iGroup, int iPos)
{
	return m_pCity->getUnitListType(iGroup, iPos);
}

bool CyCity::isEventOccured(int eEvent) const
{
	return m_pCity->isEventOccured((EventTypes)eEvent);
}

namespace
{
	//	The whole group out, in one call. N is deduced from the array the group read filled, so a family that
	//	grows a channel needs no edit here.
	//	⚠ RAW relay -- for a group whose members are NOT ×100 amounts (counts, timers, percents, enum ids).
	//	An AMOUNT group uses cyc_toHuman below.

	//	⛔ THE READER BOUNDARY -- an AMOUNT converts HERE, once, and Python does no scale math.
	//	The Cy layer is the CONTROLLER (patterns.md § the Cy* layer is the controller): thin means no LOGIC, and
	//	representation is not logic -- turning the engine's internal ×100 fixed point into the external form is
	//	precisely this layer's job, and the only place it should happen. Push it outward and every consumer has
	//	to know the engine's internal scale, and they then disagree about it -- which is the state this replaces.
	//	⛔ IT REDUCES TO AN INT, AND A FLOAT HERE IS A CRASH-CLASS BUG -- the reason is the EXE, not the model.
	//	Python hands these straight to CyTranslator().getText(), which is the EXE's VARARGS text call: arguments
	//	match a TXT_KEY's placeholders positionally BY 4-BYTE SLOT (the rule Tools/verify-gettext-widths.py
	//	enforces on the C++ side). A Python float is an 8-byte double, so it eats TWO slots -- `%d1` renders the
	//	double's HIGH half (5.0 -> 1075052544, "over a billion") and every later placeholder reads one slot early,
	//	until a `%s` lands on an integer and the EXE walks it as a pointer.
	//	⚑ So "nothing downstream does deterministic math, therefore a float is safe" is true of the MATH and false
	//	of the ABI. Widening this to float requires first sweeping every getText call site that renders one of
	//	these values -- it is not a property of the getter alone.
	template <int N>
	python::list cyc_toHuman(const int (&values)[N])
	{
		python::list list = python::list();
		for (int i = 0; i < N; ++i)
		{
			list.append(values[i] / 100);
		}
		return list;
	}
}

//	==== THE CITY READ SURFACE (CyCity.h) ====
//	Every body is a BARE RELAY of a maintained group read: fill the caller-owned array off this city, hand it
//	back as a list. Nothing gates, ensures or recomputes.
//	⚠ The handle is constructed from a live CvCity and asserts on NULL, but a wrapper can outlive its city, so
//	each read tests the pointer and answers an all-zero group rather than crashing a screen.

python::list CyCity::getYields() const
{
	PERF_SCOPE("CyCity::getYields", -1);
	int values[NUM_YIELD_TYPES] = { 0 };
	if (m_pCity) m_pCity->getYields(values);
	return cyToList(values);
}

python::list CyCity::getCommerces() const
{
	PERF_SCOPE("CyCity::getCommerces", -1);
	int values[NUM_COMMERCE_TYPES] = { 0 };
	if (m_pCity) m_pCity->getCommerces(values);
	return cyToList(values);
}

python::list CyCity::getWellbeing() const
{
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	if (m_pCity) m_pCity->getWellbeing(values);
	return cyc_toHuman(values);
}

python::list CyCity::getDefenseKinds() const
{
	int values[NUM_DEFENSE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getDefenseKinds(values);
	return cyToList(values);
}

python::list CyCity::getMaintenanceKinds() const
{
	int values[NUM_MAINTENANCE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getMaintenanceKinds(values);
	return cyToList(values);
}

python::list CyCity::getBuildRateKinds() const
{
	int values[NUM_BUILD_RATE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getBuildRateKinds(values);
	return cyToList(values);
}

python::list CyCity::getCombatKinds() const
{
	int values[NUM_COMBAT_KINDS] = { 0 };
	if (m_pCity) m_pCity->getCombatKinds(values);
	return cyToList(values);
}

python::list CyCity::getExperienceKinds() const
{
	int values[NUM_EXPERIENCE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getExperienceKinds(values);
	return cyToList(values);
}

python::list CyCity::getRevolutionKinds() const
{
	int values[NUM_REVOLUTION_KINDS] = { 0 };
	if (m_pCity) m_pCity->getRevolutionKinds(values);
	return cyToList(values);
}

python::list CyCity::getTradeRouteKinds() const
{
	int values[NUM_TRADE_ROUTE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getTradeRouteKinds(values);
	return cyToList(values);
}

python::list CyCity::getScalars() const
{
	int values[NUM_INFO_SCALARS] = { 0 };
	if (m_pCity) m_pCity->getScalars(values);
	return cyToList(values);
}

python::list CyCity::getHealKinds() const
{
	int values[NUM_HEAL_KINDS] = { 0 };
	if (m_pCity) m_pCity->getHealKinds(values);
	return cyToList(values);
}

python::list CyCity::getUnderworldKinds() const
{
	int values[NUM_UNDERWORLD_KINDS] = { 0 };
	if (m_pCity) m_pCity->getUnderworldKinds(values);
	return cyToList(values);
}

python::list CyCity::getVisionKinds() const
{
	int values[NUM_VISION_KINDS] = { 0 };
	if (m_pCity) m_pCity->getVisionKinds(values);
	return cyToList(values);
}

python::list CyCity::getRealizedWellbeing(int iExtraPopulation) const
{
	PERF_SCOPE("CyCity::getRealizedWellbeing", -1);
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	if (m_pCity) m_pCity->realizedWellbeing(iExtraPopulation, values);
	return cyc_toHuman(values);
}

int CyCity::getHealthRate(int iExtraPopulation) const
{
	return m_pCity ? m_pCity->healthRate(iExtraPopulation) : 0;
}

int CyCity::getAngryPopulation(int iExtraPopulation) const
{
	return m_pCity ? m_pCity->angryPopulation(iExtraPopulation) : 0;
}

python::list CyCity::getYieldModifiers() const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	if (m_pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = m_pCity->getBaseYieldRateModifier((YieldTypes)iYield);
		}
	}
	return cyToList(values);
}

python::list CyCity::getYieldTerms(int iYield) const
{
	PERF_SCOPE("CyCity::getYieldTerms", -1);
	python::list terms = python::list();
	if (m_pCity == NULL || iYield < 0 || iYield >= NUM_YIELD_TYPES) return terms;

	InfoValuation::CityRateTerms t;
	InfoValuation::cityReceiverRate(*m_pCity, iYield, &t);
	terms.append((double)t.plotBase);
	terms.append((double)t.plotNature);
	terms.append((double)t.plotImprovement);
	terms.append((double)t.plotRest);
	terms.append((double)t.tradeYield);
	terms.append((double)t.goldenAge);
	terms.append((double)t.upperFlat);
	terms.append((double)t.specialists);
	terms.append((double)t.cityFlat);
	terms.append(t.percentSum);
	terms.append(t.workedPlots);
	terms.append((double)t.rate);
	return terms;
}

int CyCity::getSight() const
{
	return m_pCity ? m_pCity->sight() : 0;
}

int CyCity::getLiberationPlayer() const
{
	return m_pCity ? (int)m_pCity->getLiberationPlayer(false) : -1;
}

int64_t CyCity::getRealizedMaintenance() const
{
	return m_pCity ? m_pCity->getMaintenanceTimes100() : 0;
}

python::list CyCity::getYieldRateRanks() const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	if (m_pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = m_pCity->findYieldRateRank((YieldTypes)iYield);
		}
	}
	return cyToList(values);
}

python::list CyCity::getBaseYieldRateRanks() const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	if (m_pCity)
	{
		for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
		{
			values[iYield] = m_pCity->findBaseYieldRateRank((YieldTypes)iYield);
		}
	}
	return cyToList(values);
}

python::list CyCity::getCommerceRateRanks() const
{
	int values[NUM_COMMERCE_TYPES] = { 0 };
	if (m_pCity)
	{
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			values[iCommerce] = m_pCity->findCommerceRateRank((CommerceTypes)iCommerce);
		}
	}
	return cyToList(values);
}

python::list CyCity::getPosition() const
{
	int values[2] = { -1, -1 };   // -1,-1 = no city; a real plot coordinate is never negative
	if (m_pCity)
	{
		values[0] = m_pCity->getX();
		values[1] = m_pCity->getY();
	}
	return cyToList(values);
}

//	The POTENTIAL work area as [(x, y), …], RING-ORDERED from the centre outward (contexts.md). A plot the map
//	does not hold is SKIPPED rather than reported as a hole, so a caller never tests for one.
python::list CyCity::getPlots() const
{
	python::list plots = python::list();
	if (m_pCity == NULL) return plots;

	for (int iIndex = 0; iIndex < NUM_CITY_PLOTS; ++iIndex)
	{
		const CvPlot* pPlot = m_pCity->getCityIndexPlot(iIndex);
		if (pPlot != NULL)
		{
			plots.append(python::make_tuple(pPlot->getX(), pPlot->getY()));
		}
	}
	return plots;
}

int CyCity::getImprovedPlotCount() const
{
	return m_pCity ? m_pCity->countNumImprovedPlots() : 0;
}

int CyCity::getWaterPlotCount() const
{
	return m_pCity ? m_pCity->countNumWaterPlots() : 0;
}

int CyCity::getAiValue() const
{
	return m_pCity ? m_pCity->AI_cityValue() : 0;
}

int CyCity::getAiBestBuildCount() const
{
	return m_pCity ? m_pCity->AI_countBestBuilds(m_pCity->area()) : 0;
}

python::list CyCity::getCountdowns() const
{
	PERF_SCOPE("CyCity::getCountdowns", -1);
	int values[NUM_CITY_COUNTDOWN_KINDS] = { 0 };
	if (m_pCity) m_pCity->getCountdowns(values);
	return cyToList(values);
}

python::list CyCity::getGrowth() const
{
	PERF_SCOPE("CyCity::getGrowth", -1);
	int values[NUM_CITY_GROWTH_READS] = { 0 };
	if (m_pCity) m_pCity->getGrowthRead(values);
	return cyToList(values);
}

python::list CyCity::getCounts() const
{
	int values[NUM_CITY_COUNT_READS] = { 0 };
	if (m_pCity) m_pCity->getCityCounts(values);
	return cyToList(values);
}

python::list CyCity::getGrantedExtras() const
{
	int values[NUM_CITY_GRANTED_EXTRAS] = { 0 };
	if (m_pCity)
	{
		values[GRANTED_EXTRA_HAPPINESS] = m_pCity->getExtraHappiness();
		values[GRANTED_EXTRA_HEALTH]    = m_pCity->getExtraHealth();
	}
	return cyToList(values);
}

python::list CyCity::getOutputHistory() const
{
	python::list rows = python::list();
	if (m_pCity == NULL) return rows;
	const CityOutputHistory* pHistory = m_pCity->getCityOutputHistory();
	if (pHistory == NULL) return rows;
	const int iSize = (int)CityOutputHistory::getCityOutputHistorySize();
	for (int iHistory = 0; iHistory < iSize; ++iHistory)
	{
		const int iTurn = (int)pHistory->getRecentOutputTurn((uint16_t)iHistory);
		if (iTurn < 1)
		{
			break;   // the history is filled front-to-back; the first empty slot ends it
		}
		python::list entries = python::list();
		const int iNum = (int)pHistory->getCityOutputHistoryNumEntries((uint16_t)iHistory);
		for (int iEntry = 0; iEntry < iNum; ++iEntry)
		{
			python::list pair = python::list();
			pair.append((int)pHistory->getCityOutputHistoryEntry((uint16_t)iHistory, (uint16_t)iEntry, true));
			pair.append((int)pHistory->getCityOutputHistoryEntry((uint16_t)iHistory, (uint16_t)iEntry, false));
			entries.append(pair);
		}
		python::list row = python::list();
		row.append(iTurn);
		row.append(entries);
		rows.append(row);
	}
	return rows;
}

python::list CyCity::getBuildingReads(int iBuilding) const
{
	int values[NUM_CITY_BUILDING_READS] = { 0 };
	if (m_pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		m_pCity->getBuildingInCity((BuildingTypes)iBuilding, values);
	}
	return cyToList(values);
}

python::list CyCity::getUnitReads(int iUnit) const
{
	int values[NUM_CITY_UNIT_READS] = { 0 };
	if (m_pCity && iUnit >= 0 && iUnit < GC.getNumUnitInfos())
	{
		m_pCity->getUnitInCity((UnitTypes)iUnit, values);
	}
	return cyToList(values);
}

python::list CyCity::getSpecialistReads(int iSpecialist) const
{
	int values[NUM_CITY_SPECIALIST_READS] = { 0 };
	if (m_pCity && iSpecialist >= 0 && iSpecialist < GC.getNumSpecialistInfos())
	{
		m_pCity->getSpecialistInCity((SpecialistTypes)iSpecialist, values);
	}
	return cyToList(values);
}

python::list CyCity::getBuildingGrantedWellbeing(int iBuilding) const
{
	int values[NUM_BUILDING_GRANTED_KINDS] = { 0 };
	if (m_pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		values[BUILDING_GRANTED_HAPPINESS] = m_pCity->getBuildingHappyChange((BuildingTypes)iBuilding);
		values[BUILDING_GRANTED_HEALTH]    = m_pCity->getBuildingHealthChange((BuildingTypes)iBuilding);
	}
	return cyToList(values);
}

python::list CyCity::getBuildingGrantedYields(int iBuilding) const
{
	int values[NUM_YIELD_TYPES] = { 0 };
	if (m_pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			values[i] = m_pCity->getBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)i);
		}
	}
	return cyToList(values);
}

python::list CyCity::getBuildingGrantedCommerces(int iBuilding) const
{
	int values[NUM_COMMERCE_TYPES] = { 0 };
	if (m_pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		{
			values[i] = m_pCity->getBuildingCommerceChange((BuildingTypes)iBuilding, (CommerceTypes)i);
		}
	}
	return cyToList(values);
}

int CyCity::getBuildingBuiltTime(int iBuilding) const
{
	if (m_pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return -1;
	return m_pCity->getBuildingData((BuildingTypes)iBuilding).iTimeBuilt;
}

int CyCity::getAddedFreeSpecialists(int iSpecialist) const
{
	if (m_pCity == NULL || iSpecialist < 0 || iSpecialist >= GC.getNumSpecialistInfos()) return 0;
	return m_pCity->getAddedFreeSpecialistCount((SpecialistTypes)iSpecialist);
}

int CyCity::getGreatPeopleUnitProgress(int iUnitType) const
{
	if (m_pCity == NULL || iUnitType < 0 || iUnitType >= GC.getNumUnitInfos())
	{
		return 0;
	}
	return m_pCity->getGreatPeopleUnitProgress((UnitTypes)iUnitType);
}

namespace
{
	//	The two build lists differ only in which accessors they walk, so the walk itself is written once.
	template <class TGroupNum, class TInGroup, class TAt>
	python::list cyc_listGroups(CvCity* pCity, TGroupNum groupNum, TInGroup inGroup, TAt at)
	{
		python::list groups = python::list();
		if (pCity == NULL)
		{
			return groups;
		}
		const int iGroups = (pCity->*groupNum)();
		for (int iGroup = 0; iGroup < iGroups; ++iGroup)
		{
			python::list entries = python::list();
			const int iCount = (pCity->*inGroup)(iGroup);
			for (int iPos = 0; iPos < iCount; ++iPos)
			{
				entries.append((int)(pCity->*at)(iGroup, iPos));
			}
			groups.append(entries);
		}
		return groups;
	}
}

python::list CyCity::getUnitListGroups() const
{
	return cyc_listGroups(m_pCity, &CvCity::getUnitListGroupNum, &CvCity::getUnitListNumInGroup,
	                      &CvCity::getUnitListType);
}

python::list CyCity::getBuildingListGroups() const
{
	return cyc_listGroups(m_pCity, &CvCity::getBuildingListGroupNum, &CvCity::getBuildingListNumInGroup,
	                      &CvCity::getBuildingListType);
}

bool CyCity::isUnitQueued(int iUnitType) const
{
	return m_pCity ? (m_pCity->getFirstUnitOrder((UnitTypes)iUnitType) != -1) : false;
}

int CyCity::getBestUnit() const
{
	if (m_pCity == NULL) return -1;
	int iBestValue = 0;
	return (int)m_pCity->AI_bestUnit(iBestValue);
}

int CyCity::getBestUnitForRole(int iUnitAI) const
{
	if (m_pCity == NULL || iUnitAI < 0 || iUnitAI >= NUM_UNITAI_TYPES) return -1;
	int iBestValue = 0;
	return (int)m_pCity->AI_bestUnitAI((UnitAITypes)iUnitAI, iBestValue);
}

python::list CyCity::getOrder() const
{
	PERF_SCOPE("CyCity::getOrder", -1);
	int values[NUM_CITY_ORDER_READS] = { 0 };
	values[ORDER_READ_TYPE] = NO_ORDER;
	values[ORDER_READ_ID]   = -1;
	if (m_pCity) m_pCity->getOrderRead(values);
	return cyToList(values);
}

int CyCity::getBuildingListSorting() const
{
	return m_pCity ? (int)m_pCity->getBuildingListSorting() : 0;
}

int CyCity::getUnitListGrouping() const
{
	return m_pCity ? (int)m_pCity->getUnitListGrouping() : 0;
}

int CyCity::getUnitListSorting() const
{
	return m_pCity ? (int)m_pCity->getUnitListSorting() : 0;
}

python::list CyCity::getFlags() const
{
	PERF_SCOPE("CyCity::getFlags", -1);
	int values[NUM_CITY_FLAGS] = { 0 };
	if (m_pCity) m_pCity->getCityFlags(values);
	return cyToList(values);
}

python::list CyCity::getCultureReads() const
{
	int values[NUM_CITY_CULTURE_READS] = { 0 };
	if (m_pCity) m_pCity->getCultureRead(values);
	return cyToList(values);
}

python::list CyCity::getProperties() const
{
	python::list rows = python::list();
	if (m_pCity == NULL) return rows;
	CvProperties* pProperties = m_pCity->getProperties();
	if (pProperties == NULL) return rows;
	const int iNum = pProperties->getNumProperties();
	for (int i = 0; i < iNum; ++i)
	{
		const PropertyTypes eProperty = pProperties->getProperty(i);
		python::list row = python::list();
		row.append((int)eProperty);
		row.append(pProperties->getValueByProperty(eProperty));
		row.append(pProperties->getChangeByProperty(eProperty));
		rows.append(row);
	}
	return rows;
}

python::list CyCity::getReligions() const
{
	python::list rows = python::list();
	if (m_pCity == NULL) return rows;
	for (int i = 0; i < GC.getNumReligionInfos(); ++i)
	{
		if (!m_pCity->isHasReligion((ReligionTypes)i)) continue;
		python::list row = python::list();
		row.append(i);
		row.append(m_pCity->isHolyCity((ReligionTypes)i) ? 1 : 0);
		rows.append(row);
	}
	return rows;
}

python::list CyCity::getCorporations() const
{
	python::list rows = python::list();
	if (m_pCity == NULL) return rows;
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i)
	{
		if (!m_pCity->isHasCorporation((CorporationTypes)i)) continue;
		python::list row = python::list();
		row.append(i);
		row.append(m_pCity->isHeadquarters((CorporationTypes)i) ? 1 : 0);
		rows.append(row);
	}
	return rows;
}

python::list CyCity::getTradeRoutes() const
{
	python::list rows = python::list();
	if (m_pCity == NULL) return rows;
	const int iMax = m_pCity->getNumTradeRouteSlots();
	for (int i = 0; i < iMax; ++i)
	{
		CvCity* pPartner = m_pCity->getTradeCity(i);
		if (pPartner == NULL) continue;
		python::list row = python::list();
		row.append((int)pPartner->getOwner());
		row.append(pPartner->getID());
		row.append(m_pCity->calculateTradeProfit(pPartner));
		rows.append(row);
	}
	return rows;
}

python::list CyCity::getHurryQuote(int iHurry) const
{
	int values[NUM_CITY_HURRY_QUOTES] = { 0 };
	if (m_pCity && iHurry >= 0 && iHurry < GC.getNumHurryInfos())
	{
		m_pCity->getHurryQuote((HurryTypes)iHurry, values);
	}
	return cyToList(values);
}

int CyCity::getDateFounded(bool bHistoricalCalendar) const
{
	return m_pCity ? m_pCity->getGameDateFounded(bHistoricalCalendar) : 0;
}

int64_t CyCity::getCultureForPlayer(int iForPlayer) const
{
	if (m_pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return 0;
	return m_pCity->getCultureTimes100((PlayerTypes)iForPlayer);
}

int CyCity::getCulturePercent(int iForPlayer) const
{
	if (m_pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return 0;
	const CvPlot* pPlot = m_pCity->plot();
	return pPlot ? pPlot->calculateCulturePercent((PlayerTypes)iForPlayer) : 0;
}

int CyCity::getTradeYield(int iYield, int iProfitTimes100) const
{
	if (m_pCity == NULL || iYield < 0 || iYield >= NUM_YIELD_TYPES) return 0;
	return m_pCity->calculateTradeYield((YieldTypes)iYield, iProfitTimes100);
}

int CyCity::getNumBonusesAvailable(int iBonus) const
{
	if (m_pCity == NULL || iBonus < 0 || iBonus >= GC.getNumBonusInfos()) return 0;
	return m_pCity->getCityContext().tradedBonusCount(iBonus);
}

bool CyCity::hasCorporationPresent(int iCorporation) const
{
	if (m_pCity == NULL || iCorporation < 0 || iCorporation >= GC.getNumCorporationInfos()) return false;
	return m_pCity->isHasCorporation((CorporationTypes)iCorporation);
}

int CyCity::getProjectProductionFor(int iProject) const
{
	if (m_pCity == NULL || iProject < 0 || iProject >= GC.getNumProjectInfos()) return 0;
	return m_pCity->getProjectProduction((ProjectTypes)iProject);
}

int CyCity::getHandicapLevel() const
{
	return m_pCity ? (int)m_pCity->getHandicapType() : -1;
}

bool CyCity::isRevealedTo(int iTeam) const
{
	if (m_pCity == NULL || iTeam < 0 || iTeam >= MAX_TEAMS) return false;
	return m_pCity->isRevealed((TeamTypes)iTeam, false);
}

bool CyCity::isCoastalTo(int iMinWaterSize) const
{
	//	Through the CONTEXT, which holds the largest adjacent water body as a maintained int, so every threshold
	//	is one comparison rather than an 8-neighbour walk.
	return m_pCity ? m_pCity->getCityContext().isCoastal(iMinWaterSize) : false;
}

bool CyCity::isEmphasizing(int iEmphasize) const
{
	if (m_pCity == NULL || iEmphasize < 0 || iEmphasize >= GC.getNumEmphasizeInfos()) return false;
	return m_pCity->AI_isEmphasize((EmphasizeTypes)iEmphasize);
}

int CyCity::getProductionTurnsLeftFor(int iOrder, int iType, int iNum) const
{
	if (m_pCity == NULL || iType < 0) return 0;
	switch (iOrder)
	{
	case ORDER_TRAIN:
		if (iType < GC.getNumUnitInfos()) return m_pCity->getProductionTurnsLeft((UnitTypes)iType, iNum);
		break;
	case ORDER_CONSTRUCT:
		if (iType < GC.getNumBuildingInfos()) return m_pCity->getProductionTurnsLeft((BuildingTypes)iType, iNum);
		break;
	case ORDER_CREATE:
		if (iType < GC.getNumProjectInfos()) return m_pCity->getProductionTurnsLeft((ProjectTypes)iType, iNum);
		break;
	default:
		break;
	}
	return 0;
}

//	⚖ THE IDENTITY SET plus THE CITY READS (CyCity.h). Owner + id + position ADDRESS the city so a consumer
//	holding a handle can say WHICH one it holds; the reads beside them answer what that city HAS, because a game
//	object's data is asked of the object itself (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (accessor homing)).
bool CyCity::addFreeSpecialist(int iSpecialist, int iChange)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iSpecialist < 0 || iSpecialist >= GC.getNumSpecialistInfos()) return false;
	pCity->changeFreeSpecialistCount((SpecialistTypes)iSpecialist, iChange, true);
	return true;
}
bool CyCity::addUnitProductionExperience(int iUnit, bool bConscript)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	CvUnit* pUnit = GET_PLAYER(pCity->getOwner()).getUnit(iUnit);
	if (pUnit == NULL) return false;
	pCity->addProductionExperience(pUnit, bConscript);
	return true;
}
bool CyCity::changeCulture(int iForPlayer, int64_t iChange, bool bPlots)
{
	if (iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return false;
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	//	bUpdatePlotGroups mirrors the engine's own callers: the culture change itself never moves the trade
	//	network, so it stays false and the plot-group pass is not paid per grant.
	pCity->changeCulture((PlayerTypes)iForPlayer, iChange, bPlots, false);
	return true;
}
bool CyCity::changeHurryAngerTimer(int iChange)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->changeHurryAngerTimer(iChange);
	return true;
}
bool CyCity::changePopulation(int iChange)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->changePopulation(iChange);
	return true;
}
bool CyCity::changeStoredFood(int iChange)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->changeFood(iChange);
	return true;
}
bool CyCity::disband()
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	GET_PLAYER(pCity->getOwner()).disband(pCity);
	return true;
}
bool CyCity::invalidateBuildingList()
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setBuildingListInvalid();
	return true;
}
bool CyCity::invalidateUnitList()
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setUnitListInvalid();
	return true;
}
bool CyCity::pushOrder(int iOrderType, int iId, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iId < 0) return false;
	pCity->pushOrder((OrderTypes)iOrderType, iId, iData2, bSave, bPop, bAppend, bForce);
	return true;
}
bool CyCity::select(bool bTestProduction)
{
	if (m_pCity == NULL)
	{
		return false;
	}
	gDLL->getInterfaceIFace()->selectCity(m_pCity, bTestProduction);
	return true;
}
bool CyCity::setBuildingGrantedCommerce(int iBuilding, int iCommerce, int iValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES) return false;
	pCity->setBuildingCommerceChange((BuildingTypes)iBuilding, (CommerceTypes)iCommerce, iValue);
	return true;
}
bool CyCity::setBuildingGrantedWellbeing(int iBuilding, int iKind, int iValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	switch (iKind)
	{
	case BUILDING_GRANTED_HAPPINESS:
		pCity->setBuildingHappyChange((BuildingTypes)iBuilding, iValue);
		return true;
	case BUILDING_GRANTED_HEALTH:
		pCity->setBuildingHealthChange((BuildingTypes)iBuilding, iValue);
		return true;
	}
	return false;
}
bool CyCity::setBuildingGrantedYield(int iBuilding, int iYield, int iValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	if (iYield < 0 || iYield >= NUM_YIELD_TYPES) return false;
	pCity->setBuildingYieldChange((BuildingTypes)iBuilding, (YieldTypes)iYield, iValue);
	return true;
}
bool CyCity::setBuildingListFilterActive(int iFilter, bool bActive)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_BUILDING_FILTERS)
	{
		return false;
	}
	pCity->setBuildingListFilterActive((BuildingFilterTypes)iFilter, bActive);
	return true;
}
bool CyCity::setBuildingListSorting(int iSorting)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iSorting < 0 || iSorting >= NUM_BUILDING_SORT)
	{
		return false;
	}
	pCity->setBuildingListSorting((BuildingSortTypes)iSorting);
	return true;
}
bool CyCity::setBuilding(int iBuilding, bool bNewValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iBuilding < 0 || iBuilding >= GC.getNumBuildingInfos()) return false;
	pCity->changeHasBuilding((BuildingTypes)iBuilding, bNewValue);
	return true;
}
bool CyCity::setCorporation(int iCorporation, bool bHeadquarters)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iCorporation < 0 || iCorporation >= GC.getNumCorporationInfos()) return false;
	pCity->setHasCorporation((CorporationTypes)iCorporation, true, false, true);
	if (bHeadquarters) GC.getGame().setHeadquarters((CorporationTypes)iCorporation, pCity, false);
	return true;
}
bool CyCity::setCulture(int iForPlayer, int64_t iCulture)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iForPlayer < 0 || iForPlayer >= MAX_PLAYERS) return false;
	pCity->setCultureTimes100((PlayerTypes)iForPlayer, iCulture, false, false);
	return true;
}
bool CyCity::setDefenseDamage(int iDamage)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->changeDefenseDamage(iDamage - pCity->getDefenseDamage());
	return true;
}
bool CyCity::setGrantedExtra(int iKind, int iValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	switch (iKind)
	{
	case GRANTED_EXTRA_HAPPINESS:    pCity->changeExtraHappiness(iValue - pCity->getExtraHappiness()); return true;
	case GRANTED_EXTRA_HEALTH:       pCity->changeExtraHealth(iValue - pCity->getExtraHealth()); return true;
	}
	return false;
}
bool CyCity::setName(std::wstring szName)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setName(szName.c_str(), false);
	return true;
}
bool CyCity::setOccupation(int iTurns)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setOccupationTimer(iTurns);
	return true;
}
bool CyCity::setOriginalOwner(int iOriginalOwner)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iOriginalOwner < 0 || iOriginalOwner >= MAX_PLAYERS) return false;
	pCity->setOriginalOwner((PlayerTypes)iOriginalOwner);
	return true;
}
bool CyCity::setPopulation(int iPopulation)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setPopulation(iPopulation);
	return true;
}
bool CyCity::setReligion(int iReligion, bool bHolyCity)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iReligion < 0 || iReligion >= GC.getNumReligionInfos()) return false;
	pCity->setHasReligion((ReligionTypes)iReligion, true, false, true);
	if (bHolyCity) GC.getGame().setHolyCity((ReligionTypes)iReligion, pCity, false);
	return true;
}
bool CyCity::setScriptData(std::string szData)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setScriptData(szData);
	return true;
}
bool CyCity::setStoredFood(int iFood)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setFood(iFood);
	return true;
}
bool CyCity::setWeLoveTheKingDay(bool bNewValue)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL) return false;
	pCity->setWeLoveTheKingDay(bNewValue);
	return true;
}
bool CyCity::setUnitListFilterActive(int iFilter, bool bActive)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iFilter < 0 || iFilter >= NUM_UNIT_FILTERS)
	{
		return false;
	}
	pCity->setUnitListFilterActive((UnitFilterTypes)iFilter, bActive);
	return true;
}
bool CyCity::setUnitListGrouping(int iGrouping)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iGrouping < 0 || iGrouping >= NUM_UNIT_GROUPING)
	{
		return false;
	}
	pCity->setUnitListGrouping((UnitGroupingTypes)iGrouping);
	return true;
}
bool CyCity::setUnitListSorting(int iSorting)
{
	CvCity* pCity = m_pCity;
	if (pCity == NULL || iSorting < 0 || iSorting >= NUM_UNIT_SORT)
	{
		return false;
	}
	pCity->setUnitListSorting((UnitSortTypes)iSorting);
	return true;
}

void CyCity::pythonPublish()
{
	python::class_<CyCity>("CyCity", python::no_init)
		//	==== THE EDITOR PLANE ====
		//	Arbitrary engine fields a scenario editor pokes. Deliberately NOT read KINDS -- nothing in the game
		//	model asks 'is this city plundered', so they stay named verbs on the city rather than group-read
		//	slots ([python-read-map.md] par.7). Every write routes through the engine's own setter.
		.def("changeConscriptAngerTimer", &CyCity::changeConscriptAngerTimer)
		.def("changeDefenseDamage", &CyCity::changeDefenseDamage)
		.def("changeDefyResolutionAngerTimer", &CyCity::changeDefyResolutionAngerTimer)
		.def("changeEspionageHappinessCounter", &CyCity::changeEspionageHappinessCounter)
		.def("changeEspionageHealthCounter", &CyCity::changeEspionageHealthCounter)
		.def("changeFreeBonus", &CyCity::changeFreeBonus)
		.def("changeGreatPeopleProgress", &CyCity::changeGreatPeopleProgress)
		.def("changeGreatPeopleUnitProgress", &CyCity::changeGreatPeopleUnitProgress)
		.def("changeOccupation", &CyCity::changeOccupation)
		.def("changeProduction", &CyCity::changeProduction)
		.def("changeReligionInfluence", &CyCity::changeReligionInfluence)
		.def("clearOrderQueue", &CyCity::clearOrderQueue)
		.def("popOrder", &CyCity::popOrder)
		.def("setAirliftTargeted", &CyCity::setAirliftTargeted)
		.def("setBombarded", &CyCity::setBombarded)
		.def("setBuildingYieldChange", &CyCity::setBuildingYieldChange)
		.def("setCitizensAutomated", &CyCity::setCitizensAutomated)
		.def("setDrafted", &CyCity::setDrafted)
		.def("setFeatureProduction", &CyCity::setFeatureProduction)
		.def("setForceSpecialistCount", &CyCity::setForceSpecialistCount)
		.def("setGreatPeopleUnitProgress", &CyCity::setGreatPeopleUnitProgress)
		.def("setHighestPopulation", &CyCity::setHighestPopulation)
		.def("setNeverLost", &CyCity::setNeverLost)
		.def("setOverflowProduction", &CyCity::setOverflowProduction)
		.def("setPlundered", &CyCity::setPlundered)
		.def("setProductionAutomated", &CyCity::setProductionAutomated)
		.def("setProductionProgress", &CyCity::setProductionProgress)
		.def("setProgressOnBuilding", &CyCity::setProgressOnBuilding)
		.def("setProgressOnUnit", &CyCity::setProgressOnUnit)
		.def("setWallOverride", &CyCity::setWallOverride)
		.def("canMaintain", &CyCity::canMaintain)
		.def("getBaseGreatPeopleRate", &CyCity::getBaseGreatPeopleRate)
		.def("getBuildingHappyChange", &CyCity::getBuildingHappyChange)
		.def("getBuildingHealthChange", &CyCity::getBuildingHealthChange)
		.def("getCultureLevel", &CyCity::getCultureLevel)
		.def("getExtraTradeRoutes", &CyCity::getExtraTradeRoutes)
		.def("getForceSpecialistCount", &CyCity::getForceSpecialistCount)
		.def("getFreeBonus", &CyCity::getFreeBonus)
		.def("getHighestPopulation", &CyCity::getHighestPopulation)
		.def("getOrderFromQueue", &CyCity::getOrderFromQueue)
		.def("getOverflowProduction", &CyCity::getOverflowProduction)
		.def("getProductionProject", &CyCity::getProductionProject)
		.def("getProgressOnBuilding", &CyCity::getProgressOnBuilding)
		.def("getProgressOnUnit", &CyCity::getProgressOnUnit)
		.def("getReligionInfluence", &CyCity::getReligionInfluence)
		.def("isAirliftTargeted", &CyCity::isAirliftTargeted)
		.def("isBombarded", &CyCity::isBombarded)
		.def("isDrafted", &CyCity::isDrafted)
		.def("isNeverLost", &CyCity::isNeverLost)
		.def("isPlundered", &CyCity::isPlundered)
		.def("isWallOverride", &CyCity::isWallOverride)
		.def("addFreeSpecialist", &CyCity::addFreeSpecialist)
		.def("addUnitProductionExperience", &CyCity::addUnitProductionExperience)
		.def("changeCulture", &CyCity::changeCulture)
		.def("changeHurryAngerTimer", &CyCity::changeHurryAngerTimer)
		.def("changePopulation", &CyCity::changePopulation)
		.def("changeStoredFood", &CyCity::changeStoredFood)
		.def("disband", &CyCity::disband)
		.def("invalidateBuildingList", &CyCity::invalidateBuildingList)
		.def("invalidateUnitList", &CyCity::invalidateUnitList)
		.def("pushOrder", &CyCity::pushOrder)
		.def("select", &CyCity::select)
		.def("setBuildingGrantedCommerce", &CyCity::setBuildingGrantedCommerce)
		.def("setBuildingGrantedWellbeing", &CyCity::setBuildingGrantedWellbeing)
		.def("setBuildingGrantedYield", &CyCity::setBuildingGrantedYield)
		.def("setBuildingListFilterActive", &CyCity::setBuildingListFilterActive)
		.def("setBuildingListSorting", &CyCity::setBuildingListSorting)
		.def("setBuilding", &CyCity::setBuilding)
		.def("setCorporation", &CyCity::setCorporation)
		.def("setCulture", &CyCity::setCulture)
		.def("setDefenseDamage", &CyCity::setDefenseDamage)
		.def("setGrantedExtra", &CyCity::setGrantedExtra)
		.def("setName", &CyCity::setName)
		.def("setOccupation", &CyCity::setOccupation)
		.def("setOriginalOwner", &CyCity::setOriginalOwner)
		.def("setPopulation", &CyCity::setPopulation)
		.def("setReligion", &CyCity::setReligion)
		.def("setScriptData", &CyCity::setScriptData)
		.def("setStoredFood", &CyCity::setStoredFood)
		.def("setWeLoveTheKingDay", &CyCity::setWeLoveTheKingDay)
		.def("setUnitListFilterActive", &CyCity::setUnitListFilterActive)
		.def("setUnitListGrouping", &CyCity::setUnitListGrouping)
		.def("setUnitListSorting", &CyCity::setUnitListSorting)
		.def("getOwner", &CyCity::getOwner)
		.def("getID",    &CyCity::getID)
		.def("getOriginalOwner", &CyCity::getOriginalOwner)
		.def("getX",     &CyCity::getX)
		.def("getY",     &CyCity::getY)

		.def("getYields",           &CyCity::getYields)
		.def("getCommerces",        &CyCity::getCommerces)
		.def("getWellbeing",        &CyCity::getWellbeing)
		.def("getDefenseKinds",     &CyCity::getDefenseKinds)
		.def("getMaintenanceKinds", &CyCity::getMaintenanceKinds)
		.def("getBuildRateKinds",   &CyCity::getBuildRateKinds)
		.def("getCombatKinds",      &CyCity::getCombatKinds)
		.def("getExperienceKinds",  &CyCity::getExperienceKinds)
		.def("getRevolutionKinds",  &CyCity::getRevolutionKinds)
		.def("getTradeRouteKinds",  &CyCity::getTradeRouteKinds)
		.def("getScalars",          &CyCity::getScalars)
		.def("getHealKinds",        &CyCity::getHealKinds)
		.def("getUnderworldKinds",  &CyCity::getUnderworldKinds)
		.def("getVisionKinds",      &CyCity::getVisionKinds)

		.def("getRealizedWellbeing", &CyCity::getRealizedWellbeing)
		.def("getHealthRate",        &CyCity::getHealthRate)
		.def("getAngryPopulation",   &CyCity::getAngryPopulation)
		.def("getYieldModifiers",    &CyCity::getYieldModifiers)
		.def("getYieldTerms",        &CyCity::getYieldTerms)
		.def("getSight",             &CyCity::getSight)
		.def("getLiberationPlayer",  &CyCity::getLiberationPlayer)
		.def("getRealizedMaintenance", &CyCity::getRealizedMaintenance)

		.def("getYieldRateRanks",     &CyCity::getYieldRateRanks)
		.def("getBaseYieldRateRanks", &CyCity::getBaseYieldRateRanks)
		.def("getCommerceRateRanks",  &CyCity::getCommerceRateRanks)

		.def("getPosition",           &CyCity::getPosition)
		.def("getPlots",              &CyCity::getPlots)
		.def("getImprovedPlotCount",  &CyCity::getImprovedPlotCount)
		.def("getWaterPlotCount",     &CyCity::getWaterPlotCount)
		.def("getAiValue",            &CyCity::getAiValue)
		.def("getAiBestBuildCount",   &CyCity::getAiBestBuildCount)

		.def("getCountdowns",         &CyCity::getCountdowns)
		.def("getGrowth",             &CyCity::getGrowth)
		.def("getCounts",             &CyCity::getCounts)
		.def("getGrantedExtras",      &CyCity::getGrantedExtras)
		.def("getOutputHistory",      &CyCity::getOutputHistory)

		.def("getBuildingReads",      &CyCity::getBuildingReads)
		.def("getUnitReads",          &CyCity::getUnitReads)
		.def("getSpecialistReads",    &CyCity::getSpecialistReads)
		.def("getBuildingGrantedWellbeing", &CyCity::getBuildingGrantedWellbeing)
		.def("getBuildingGrantedYields",    &CyCity::getBuildingGrantedYields)
		.def("getBuildingGrantedCommerces", &CyCity::getBuildingGrantedCommerces)
		.def("getBuildingBuiltTime",  &CyCity::getBuildingBuiltTime)
		.def("getAddedFreeSpecialists", &CyCity::getAddedFreeSpecialists)
		.def("getGreatPeopleUnitProgress", &CyCity::getGreatPeopleUnitProgress)

		.def("getUnitListGroups",     &CyCity::getUnitListGroups)
		.def("getBuildingListGroups", &CyCity::getBuildingListGroups)
		.def("isUnitQueued",          &CyCity::isUnitQueued)

		.def("getBestUnit",           &CyCity::getBestUnit)
		.def("getBestUnitForRole",    &CyCity::getBestUnitForRole)
		.def("getProductionTurnsLeftFor", &CyCity::getProductionTurnsLeftFor)

		.def("isPowered",             &CyCity::isPowered)
		.def("getMaintenance",        &CyCity::getMaintenance)
		.def("foodDifference",        &CyCity::foodDifference)
		.def("getEspionageDefenseModifier", &CyCity::getEspionageDefenseModifier)
		.def("isCapital",             &CyCity::isCapital)
		.def("isGovernmentCenter",    &CyCity::isGovernmentCenter)
		.def("isDisorder",            &CyCity::isDisorder)
		.def("isOccupation",          &CyCity::isOccupation)
		.def("getFlags",              &CyCity::getFlags)
		.def("getProperties",         &CyCity::getProperties)
		.def("getReligions",          &CyCity::getReligions)
		.def("getCorporations",       &CyCity::getCorporations)
		.def("getCultureReads",       &CyCity::getCultureReads)
		.def("getTradeRoutes",        &CyCity::getTradeRoutes)
		.def("getHurryQuote",         &CyCity::getHurryQuote)
		.def("flatHurryAngerLength",  &CyCity::flatHurryAngerLength)
		.def("getDateFounded",        &CyCity::getDateFounded)
		.def("getCultureForPlayer",   &CyCity::getCultureForPlayer)
		.def("getCulturePercent",     &CyCity::getCulturePercent)
		.def("findHighestCulture",    &CyCity::findHighestCulture)
		.def("getConscriptUnit",      &CyCity::getConscriptUnit)
		.def("getTradeYield",         &CyCity::getTradeYield)
		.def("getNumBonusesAvailable", &CyCity::getNumBonusesAvailable)
		.def("getNumBonuses",         &CyCity::getNumBonuses)
		.def("hasCorporationPresent", &CyCity::hasCorporationPresent)
		.def("getProjectProductionFor", &CyCity::getProjectProductionFor)
		.def("getHandicapLevel",      &CyCity::getHandicapLevel)
		.def("isRevealedTo",          &CyCity::isRevealedTo)
		.def("isCoastalTo",           &CyCity::isCoastalTo)
		.def("isEmphasizing",         &CyCity::isEmphasizing)

		//	Reads the wrapper already implemented correctly and simply never published -- the re-home publishes
		//	them rather than retyping an identical body.
		.def("getName",               &CyCity::getName)
		.def("getPopulation",         &CyCity::getPopulation)
		.def("getRealPopulation",     &CyCity::getRealPopulation)
		.def("getGreatPeopleRate",    &CyCity::getGreatPeopleRate)
		.def("getGreatPeopleProgress", &CyCity::getGreatPeopleProgress)
		.def("getMilitaryHappinessUnits", &CyCity::getMilitaryHappinessUnits)
		.def("getScriptData",         &CyCity::getScriptData)
		.def("getProductionName",     &CyCity::getProductionName)
		.def("getProductionNameKey",  &CyCity::getProductionNameKey)
		.def("getOrderQueueLength",   &CyCity::getOrderQueueLength)
		.def("isProductionUnit",      &CyCity::isProductionUnit)
		.def("isProductionProcess",   &CyCity::isProductionProcess)
		.def("hasBuilding",           &CyCity::hasBuilding)

		// Wrappers that existed but were never registered, so every call raised AttributeError the moment its
		// branch ran -- a random event, an outcome, an advisor panel. Registered here because a call site needs
		// the read; the WorldBuilder/Revolution/debug-only names stay unpublished.
		.def("plot",                  &CyCity::plot, python::return_value_policy<python::manage_new_object>())
		.def("area",                  &CyCity::area, python::return_value_policy<python::manage_new_object>())
		.def("getTradeCity",          &CyCity::getTradeCity, python::return_value_policy<python::manage_new_object>())
		.def("getFreeSpecialistCount",   &CyCity::getFreeSpecialistCount)
		.def("changeFreeSpecialistCount", &CyCity::changeFreeSpecialistCount)
		.def("getSpecialistCount",    &CyCity::getSpecialistCount)
		.def("isHasReligion",         &CyCity::isHasReligion)
		.def("setHasReligion",        &CyCity::setHasReligion)
		.def("isHolyCityByType",      &CyCity::isHolyCityByType)
		.def("isHasCorporation",      &CyCity::isHasCorporation)
		.def("setHasCorporation",     &CyCity::setHasCorporation)
		.def("isHeadquartersByType",  &CyCity::isHeadquartersByType)
		.def("isActiveCorporation",   &CyCity::isActiveCorporation)
		.def("isActiveBuilding",      &CyCity::isActiveBuilding)
		.def("hasBonus",              &CyCity::hasBonus)
		.def("getNameKey",            &CyCity::getNameKey)
		.def("getFood",               &CyCity::getFood)
		.def("growthThreshold",       &CyCity::growthThreshold)
		.def("getCultureThreshold",   &CyCity::getCultureThreshold)
		.def("getMaintenanceTimes100", &CyCity::getMaintenanceTimes100)
		.def("getDefenseModifier",    &CyCity::getDefenseModifier)
		.def("getTotalDefense",       &CyCity::getTotalDefense)
		.def("isConnectedToCapital",  &CyCity::isConnectedToCapital)
		.def("getMaxTradeRoutes",     &CyCity::getMaxTradeRoutes)
		.def("getBaseYieldRateModifier", &CyCity::getBaseYieldRateModifier)
		.def("isProduction",          &CyCity::isProduction)
		.def("isProductionBuilding",  &CyCity::isProductionBuilding)
		.def("isProductionProject",   &CyCity::isProductionProject)
		.def("isProductionAutomated", &CyCity::isProductionAutomated)
		.def("getProductionUnit",     &CyCity::getProductionUnit)
		.def("getProductionBuilding", &CyCity::getProductionBuilding)
		.def("getProductionProcess",  &CyCity::getProductionProcess)
		.def("getProductionProgress", &CyCity::getProductionProgress)
		.def("getProductionNeeded",   &CyCity::getProductionNeeded)
		.def("getProductionTurnsLeft", &CyCity::getProductionTurnsLeft)
		.def("getProductionExperience", &CyCity::getProductionExperience)
		.def("getCurrentProductionDifference", &CyCity::getCurrentProductionDifference)
		.def("getMaxProductionOverflow", &CyCity::getMaxProductionOverflow)
		.def("productionLeft",        &CyCity::productionLeft)
		.def("canHurry",              &CyCity::canHurry)
		.def("hurryPopulation",       &CyCity::hurryPopulation)
		.def("hurryProduction",       &CyCity::hurryProduction)
		.def("getHurryGold",          &CyCity::getHurryGold)
		.def("canConscript",          &CyCity::canConscript)
		.def("getHurryAngerTimer",    &CyCity::getHurryAngerTimer)
		.def("getConscriptAngerTimer", &CyCity::getConscriptAngerTimer)
		.def("changeHappinessTimer",  &CyCity::changeHappinessTimer)
		.def("changeEventAnger",      &CyCity::changeEventAnger)
		.def("setBuildingCommerceChange", &CyCity::setBuildingCommerceChange)
		.def("isCitizensAutomated",   &CyCity::isCitizensAutomated)
		.def("AI_isEmphasize",        &CyCity::AI_isEmphasize)
		.def("getBuildingListFilterActive", &CyCity::getBuildingListFilterActive)
		.def("getUnitListFilterActive",     &CyCity::getUnitListFilterActive)

		.def("getOrder",              &CyCity::getOrder)
		.def("getBuildingListSorting", &CyCity::getBuildingListSorting)
		.def("getUnitListGrouping",   &CyCity::getUnitListGrouping)
		.def("getUnitListSorting",    &CyCity::getUnitListSorting)
		;
}
