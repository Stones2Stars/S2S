#include "CvGameCoreDLL.h"
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

int CyCity::getRevolutionIndex() const
{
	return m_pCity->getRevolutionIndex();
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

int CyCity::getRevIndexAverage() const
{
	return m_pCity->getRevIndexAverage();
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

CyPlot* CyCity::getCityIndexPlot(int iIndex) const
{
	if (m_pCity->getCityIndexPlot(iIndex))
	{
		return new CyPlot(m_pCity->getCityIndexPlot(iIndex));
	}
	return NULL;
}

bool CyCity::canWork(const CyPlot* pPlot) const
{
	return m_pCity->canWork(pPlot ? pPlot->getPlot() : NULL);
}

int CyCity::countNumImprovedPlots() const
{
	return m_pCity->countNumImprovedPlots();
}

int CyCity::countNumWaterPlots() const
{
	return m_pCity->countNumWaterPlots();
}

int CyCity::findBaseYieldRateRank(YieldTypes eYield) const
{
	return m_pCity->findBaseYieldRateRank(eYield);
}

int CyCity::findYieldRateRank(YieldTypes eYield) const
{
	return m_pCity->findYieldRateRank(eYield);
}

int CyCity::findCommerceRateRank(CommerceTypes eCommerce) const
{
	return m_pCity->findCommerceRateRank(eCommerce);
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

int CyCity::getGeneralProductionTurnsLeft() const
{
	return m_pCity->getGeneralProductionTurnsLeft();
}

std::wstring CyCity::getProductionNameKey() const
{
	return m_pCity->getProductionNameKey();
}

bool CyCity::isFoodProduction() const
{
	return m_pCity->isFoodProduction();
}

int CyCity::getFirstUnitOrder(int /*UnitTypes*/ eUnit) const
{
	return m_pCity->getFirstUnitOrder((UnitTypes)eUnit);
}

int CyCity::getFirstBuildingOrder(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getFirstBuildingOrder((BuildingTypes)eBuilding);
}

int CyCity::getNumTrainUnitAI(int /*UnitAITypes*/ eUnitAI) const
{
	return m_pCity->getNumTrainUnitAI((UnitAITypes) eUnitAI);
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

int CyCity::getUnitProductionTurnsLeft(int /*UnitTypes*/ iUnit, int iNum) const
{
	return m_pCity->getProductionTurnsLeft((UnitTypes) iUnit, iNum);
}

int CyCity::getBuildingProductionTurnsLeft(int /*BuildingTypes*/ iBuilding, int iNum) const
{
	return m_pCity->getProductionTurnsLeft((BuildingTypes) iBuilding, iNum);
}

int CyCity::getProjectProductionTurnsLeft(int /*ProjectTypes*/ eProject, int iNum) const
{
	return m_pCity->getProductionTurnsLeft((ProjectTypes) eProject, iNum);
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

bool CyCity::hasTrait(int /*TraitTypes*/ iTrait) const
{
	return m_pCity->hasTrait((TraitTypes) iTrait);
}

bool CyCity::isNPC() const
{
	return m_pCity->isNPC();
}

bool CyCity::isHominid() const
{
	return m_pCity->isHominid();
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

int CyCity::getNoMilitaryPercentAnger() const
{
	return m_pCity->getNoMilitaryPercentAnger();
}

int CyCity::getWarWearinessPercentAnger() const
{
	return m_pCity->getWarWearinessPercentAnger();
}

int CyCity::getRevIndexPercentAnger() const
{
	return m_pCity->getRevIndexPercentAnger();
}

// The wellbeing reads index the realized channel array; ÷100 here because this is the reader boundary.
int CyCity::unhappyLevel(int iExtra) const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	m_pCity->realizedWellbeing(iExtra, aWellbeing);
	return aWellbeing[WELLBEING_ANGER] / 100;
}

int CyCity::happyLevel() const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	m_pCity->realizedWellbeing(0, aWellbeing);
	return aWellbeing[WELLBEING_HAPPINESS] / 100;
}

int CyCity::angryPopulation(int iExtra) const
{
	return m_pCity->angryPopulation(iExtra);
}

int CyCity::totalFreeSpecialists() const
{
	return m_pCity->totalFreeSpecialists();
}

int CyCity::goodHealth() const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	m_pCity->realizedWellbeing(0, aWellbeing);
	return aWellbeing[WELLBEING_HEALTH] / 100;
}

int CyCity::badHealth() const
{
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	m_pCity->realizedWellbeing(0, aWellbeing);
	return aWellbeing[WELLBEING_UNHEALTH] / 100;
}

int CyCity::healthRate(int iExtra) const
{
	return m_pCity->healthRate(iExtra);
}

int CyCity::foodConsumption(bool bNoAngry, int iExtra) const
{
	// THE READ EDGE -- the food plane is x100 native; Python sees whole food (docs/specs/curators/fixed-point-and-scales.md §1 (the x100 fixed-point model)).
	return m_pCity->foodConsumption(bNoAngry, iExtra) / 100;
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

void CyCity::changeHasBuilding(int /*BuildingTypes*/ iIndex, bool bNewValue)
{
	m_pCity->changeHasBuilding((BuildingTypes) iIndex, bNewValue);
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

CyArea* CyCity::waterArea() const
{
	CvArea* waterArea = m_pCity->waterArea();
	return waterArea ? new CyArea(waterArea) : NULL;
}

int CyCity::getGameTurnFounded() const
{
	CvGame& GAME = GC.getGame();
	bool bHistoricalCalendar = GAME.isModderGameOption(MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR);
	return m_pCity->getGameTurnFounded(bHistoricalCalendar);
}

int CyCity::getGameDateFounded() const
{
	CvGame& GAME = GC.getGame();
	bool bHistoricalCalendar = GAME.isModderGameOption(MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR);
	return m_pCity->getGameDateFounded(bHistoricalCalendar);
}

int CyCity::getGameTurnAcquired() const
{
	return m_pCity->getGameTurnAcquired();
}

int CyCity::getPopulation() const
{
	return m_pCity->getPopulation();
}

void CyCity::setPopulation(int iNewValue)
{
	m_pCity->setPopulation(iNewValue);
}

void CyCity::changePopulation(int iChange)
{
	m_pCity->changePopulation(iChange);
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

int CyCity::getNumWorldWonders() const
{
	return m_pCity->getNumWorldWonders();
}

int CyCity::getNumNationalWonders() const
{
	return m_pCity->getNumNationalWonders();
}

int CyCity::getMaxNumWorldWonders() const
{
	return m_pCity->getMaxNumWorldWonders();
}

int CyCity::getMaxNumNationalWonders() const
{
	return m_pCity->getMaxNumNationalWonders();
}

int CyCity::getNumBuildings() const
{
	return m_pCity->getNumBuildings();
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

int CyCity::getEspionageHealthCounter() const
{
	return m_pCity->getEspionageHealthCounter();
}

void CyCity::changeEspionageHealthCounter(int iChange)
{
	m_pCity->changeEspionageHealthCounter(iChange);
}

int CyCity::getEspionageHappinessCounter() const
{
	return m_pCity->getEspionageHappinessCounter();
}

void CyCity::changeEspionageHappinessCounter(int iChange)
{
	m_pCity->changeEspionageHappinessCounter(iChange);
}

int CyCity::getBuildingHealth(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHealth((BuildingTypes)eBuilding);
}

int CyCity::getMilitaryHappinessUnits() const
{
	return m_pCity->getMilitaryHappinessUnits();
}

int CyCity::getBuildingHappiness(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHappiness((BuildingTypes)eBuilding);
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

void CyCity::changeHurryAngerTimer(int iChange)
{
	m_pCity->changeHurryAngerTimer(iChange);
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

int CyCity::getDefyResolutionAngerTimer() const
{
	return m_pCity->getDefyResolutionAngerTimer();
}

void CyCity::changeDefyResolutionAngerTimer(int iChange)
{
	m_pCity->changeDefyResolutionAngerTimer(iChange);
}

int CyCity::flatDefyResolutionAngerLength() const
{
	return m_pCity->flatDefyResolutionAngerLength();
}

int CyCity::getHappinessTimer() const
{
	return m_pCity->getHappinessTimer();
}

void CyCity::changeHappinessTimer(int iChange)
{
	m_pCity->changeHappinessTimer(iChange);
}

bool CyCity::isNoUnhappiness() const
{
	return m_pCity->isNoUnhappiness();
}

int CyCity::getFood() const
{
	return m_pCity->getFood();
}

void CyCity::setFood(int iNewValue)
{
	m_pCity->setFood(iNewValue);
}

void CyCity::changeFood(int iChange)
{
	m_pCity->changeFood(iChange);
}

int CyCity::getFoodKept() const
{
	return m_pCity->getFoodKept();
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

int CyCity::getDefenseDamage() const
{
	return m_pCity->getDefenseDamage();
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

int CyCity::getOccupationTimer() const
{
	return m_pCity->getOccupationTimer();
}

bool CyCity::isOccupation() const
{
	return m_pCity->isOccupation();
}

void CyCity::setOccupationTimer(int iNewValue)
{
	m_pCity->setOccupationTimer(iNewValue);
}

void CyCity::changeOccupationTimer(int iChange)
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

void CyCity::setOriginalOwner(int iPlayer)
{
	return m_pCity->setOriginalOwner((PlayerTypes)iPlayer);
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

int CyCity::getYieldRateModifier(int /*YieldTypes*/ eIndex) const
{
	return m_pCity->getYieldRateModifier((YieldTypes)eIndex);
}


int CyCity::getProductionToCommerceModifier(int /*CommerceTypes*/ eIndex) const
{
	return m_pCity->getProductionToCommerceModifier((CommerceTypes)eIndex);
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

void CyCity::setCulture(int /*PlayerTypes*/ eIndex, int iNewValue, bool bPlots)
{
	m_pCity->setCulture((PlayerTypes)eIndex, iNewValue, bPlots, true);
}

void CyCity::setCultureTimes100(int /*PlayerTypes*/ eIndex, int iNewValue, bool bPlots)
{
	m_pCity->setCultureTimes100((PlayerTypes)eIndex, iNewValue, bPlots, true);
}

void CyCity::changeCulture(int /*PlayerTypes*/ eIndex, int iChange, bool bPlots)
{
	m_pCity->changeCulture((PlayerTypes)eIndex, iChange, bPlots, true);
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

void CyCity::setRevealed(int /*TeamTypes*/ eIndex, bool bNewValue)
{
	m_pCity->setRevealed((TeamTypes)eIndex, bNewValue);
}

bool CyCity::getEspionageVisibility(int /*TeamTypes*/ eIndex) const
{
	return m_pCity->getEspionageVisibility((TeamTypes)eIndex);
}

std::wstring CyCity::getName() const
{
	return m_pCity->getName();
}

std::wstring CyCity::getNameForm(int iForm) const
{
	return m_pCity->getName((uint)iForm);
}

std::wstring CyCity::getNameKey() const
{
	return m_pCity->getNameKey();
}

void CyCity::setName(std::wstring szNewValue, bool bFound)
{
	m_pCity->setName((CvWString)szNewValue, bFound);
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

int CyCity::getDelayOnBuilding(int /*BuildingTypes*/ eIndex) const
{
	return m_pCity->getDelayOnBuilding((BuildingTypes)eIndex);
}

bool CyCity::isBuildingProductionDecay(int /*BuildingTypes*/ eIndex) const
{
	return m_pCity->isBuildingProductionDecay((BuildingTypes)eIndex);
}

int CyCity::getBuildingProductionDecayTurns(int /*BuildingTypes*/ eIndex) const
{
	return m_pCity->getBuildingProductionDecayTurns((BuildingTypes)eIndex);
}

int CyCity::getBuildingOriginalOwner(int /*BuildingTypes*/ iType) const
{
	return m_pCity->getBuildingData((BuildingTypes) iType).eBuiltBy;
}

int CyCity::getBuildingOriginalTime(int /*BuildingTypes*/ iType) const
{
	return m_pCity->getBuildingData((BuildingTypes) iType).iTimeBuilt;
}

int CyCity::getProgressOnUnit(int iIndex) const
{
	return m_pCity->getProgressOnUnit((UnitTypes) iIndex);
}

void CyCity::setProgressOnUnit(int iIndex, int iNewValue)
{
	m_pCity->setProgressOnUnit((UnitTypes)iIndex, iNewValue);
}

int CyCity::getDelayOnUnit(int /*UnitTypes*/ eIndex) const
{
	return m_pCity->getDelayOnUnit((UnitTypes)eIndex);
}

bool CyCity::isUnitProductionDecay(int /*UnitTypes*/ eIndex) const
{
	return m_pCity->isUnitProductionDecay((UnitTypes)eIndex);
}

int CyCity::getUnitProductionDecayTurns(int /*UnitTypes*/ eIndex) const
{
	return m_pCity->getUnitProductionDecayTurns((UnitTypes)eIndex);
}

int CyCity::getProjectProduction(int /*ProjectTypes*/ iIndex) const
{
	return m_pCity->getProjectProduction((ProjectTypes) iIndex);
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

int CyCity::getMaxSpecialistCount(int /*SpecialistTypes*/ eIndex) const
{
	return m_pCity->getMaxSpecialistCount((SpecialistTypes)eIndex);
}

bool CyCity::isSpecialistValid(int /*SpecialistTypes*/ eIndex, int iExtra) const
{
	return m_pCity->isSpecialistValid((SpecialistTypes) eIndex, iExtra);
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

bool CyCity::isWorkingPlot(const CyPlot& kPlot) const
{
	return m_pCity->isWorkingPlot(kPlot.getPlot());
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

void CyCity::pushOrder(OrderTypes eOrder, int iData1, int iData2, bool bSave, bool bPop, bool bAppend, bool bForce)
{
	m_pCity->pushOrder(eOrder, iData1, iData2, bSave, bPop, bAppend, bForce);
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

bool CyCity::AI_isEmphasizeSpecialist(int /*SpecialistTypes*/ iIndex) const
{
	return m_pCity->AI_isEmphasizeSpecialist((SpecialistTypes)iIndex);
}

bool CyCity::AI_isEmphasize(int iEmphasizeType) const
{
	return m_pCity->AI_isEmphasize((EmphasizeTypes)iEmphasizeType);
}

int CyCity::AI_countBestBuilds(const CyArea& kArea) const
{
	return m_pCity->AI_countBestBuilds(const_cast<CvArea*>(kArea.getArea()));
}

int CyCity::AI_cityValue() const
{
	return m_pCity->AI_cityValue();
}

std::string CyCity::getScriptData() const
{
	return m_pCity->getScriptData();
}

void CyCity::setScriptData(std::string szNewValue)
{
	m_pCity->setScriptData(szNewValue);
}

int CyCity::getBuildingYieldChange(int /*BuildingTypes*/ eBuilding, int /*YieldTypes*/ eYield) const
{
	return m_pCity->getBuildingYieldChange((BuildingTypes)eBuilding, (YieldTypes)eYield);
}

void CyCity::setBuildingYieldChange(int /*BuildingTypes*/ eBuilding, int /*YieldTypes*/ eYield, int iChange)
{
	m_pCity->setBuildingYieldChange((BuildingTypes)eBuilding, (YieldTypes)eYield, iChange);
}

int CyCity::getBuildingCommerceChange(int /*BuildingTypes*/ eBuilding, int /*CommerceTypes*/ eCommerce) const
{
	return m_pCity->getBuildingCommerceChange((BuildingTypes)eBuilding, (CommerceTypes)eCommerce);
}

void CyCity::setBuildingCommerceChange(int /*BuildingTypes*/ eBuilding, int /*CommerceTypes*/ eCommerce, int iChange)
{
	m_pCity->setBuildingCommerceChange((BuildingTypes)eBuilding, (CommerceTypes)eCommerce, iChange);
}

int CyCity::getBuildingHappyChange(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHappyChange((BuildingTypes)eBuilding);
}

void CyCity::setBuildingHappyChange(int /*BuildingTypes*/ eBuilding, int iChange)
{
	m_pCity->setBuildingHappyChange((BuildingTypes)eBuilding, iChange);
}

int CyCity::getBuildingHealthChange(int /*BuildingTypes*/ eBuilding) const
{
	return m_pCity->getBuildingHealthChange((BuildingTypes)eBuilding);
}

void CyCity::setBuildingHealthChange(int /*BuildingTypes*/ eBuilding, int iChange)
{
	m_pCity->setBuildingHealthChange((BuildingTypes)eBuilding, iChange);
}


int CyCity::getArea() const
{
	return m_pCity->getArea();
}

bool CyCity::isWeLoveTheKingDay() const
{
	return m_pCity->isWeLoveTheKingDay();
}

void CyCity::setWeLoveTheKingDay(bool bWeLoveTheKingDay)
{
	m_pCity->setWeLoveTheKingDay(bWeLoveTheKingDay);
}

int64_t CyCity::calcCorporateMaintenance() const
{
	return m_pCity->calcCorporateMaintenance();
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


const CityOutputHistory* CyCity::getCityOutputHistory() const
{
	return m_pCity->getCityOutputHistory();
}

bool CyCity::getBuildingListFilterActive(int eFilter)
{
	return m_pCity->getBuildingListFilterActive((BuildingFilterTypes)eFilter);
}

void CyCity::setBuildingListFilterActive(int eFilter, bool bActive)
{
	m_pCity->setBuildingListFilterActive((BuildingFilterTypes)eFilter, bActive);
}

int CyCity::getBuildingListGrouping()
{
	return m_pCity->getBuildingListGrouping();
}

void CyCity::setBuildingListGrouping(int eGrouping)
{
	m_pCity->setBuildingListGrouping((BuildingGroupingTypes)eGrouping);
}


void CyCity::setBuildingListSorting(int eSorting)
{
	m_pCity->setBuildingListSorting((BuildingSortTypes)eSorting);
}

int CyCity::getBuildingListGroupNum()
{
	return m_pCity->getBuildingListGroupNum();
}

int CyCity::getBuildingListNumInGroup(int iGroup)
{
	return m_pCity->getBuildingListNumInGroup(iGroup);
}

int CyCity::getBuildingListType(int iGroup, int iPos)
{
	return m_pCity->getBuildingListType(iGroup, iPos);
}

void CyCity::setUnitListInvalid()
{
	m_pCity->setUnitListInvalid();
}

bool CyCity::getUnitListFilterActive(int eFilter)
{
	return m_pCity->getUnitListFilterActive((UnitFilterTypes)eFilter);
}

void CyCity::setUnitListFilterActive(int eFilter, bool bActive)
{
	m_pCity->setUnitListFilterActive((UnitFilterTypes)eFilter, bActive);
}


void CyCity::setUnitListGrouping(int eGrouping)
{
	m_pCity->setUnitListGrouping((UnitGroupingTypes)eGrouping);
}


void CyCity::setUnitListSorting(int eSorting)
{
	m_pCity->setUnitListSorting((UnitSortTypes)eSorting);
}

int CyCity::getUnitListGroupNum()
{
	return m_pCity->getUnitListGroupNum();
}

int CyCity::getUnitListNumInGroup(int iGroup)
{
	return m_pCity->getUnitListNumInGroup(iGroup);
}

int CyCity::getUnitListType(int iGroup, int iPos)
{
	return m_pCity->getUnitListType(iGroup, iPos);
}

bool CyCity::isEventOccured(int eEvent) const
{
	return m_pCity->isEventOccured((EventTypes)eEvent);
}

int CyCity::AI_bestUnit() const
{
	int iDummyValue;
	return m_pCity->AI_bestUnit(iDummyValue, -1, NULL, true, NULL, true, false, NULL);
}

int CyCity::AI_bestUnitAI(UnitAITypes eUnitAITypes) const
{
	int iDummyValue;
	return m_pCity->AI_bestUnitAI(eUnitAITypes, iDummyValue, true, true, &CvUnitSelectionCriteria().IgnoreGrowth(true));
}

namespace
{
	//	The whole group out, in one call. N is deduced from the array the group read filled, so a family that
	//	grows a channel needs no edit here.
	template <int N>
	python::list cyc_toList(const int (&values)[N])
	{
		python::list list = python::list();
		for (int i = 0; i < N; ++i)
		{
			list.append(values[i]);
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
	return cyc_toList(values);
}

python::list CyCity::getCommerces() const
{
	PERF_SCOPE("CyCity::getCommerces", -1);
	int values[NUM_COMMERCE_TYPES] = { 0 };
	if (m_pCity) m_pCity->getCommerces(values);
	return cyc_toList(values);
}

python::list CyCity::getWellbeing() const
{
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	if (m_pCity) m_pCity->getWellbeing(values);
	return cyc_toList(values);
}

python::list CyCity::getDefenseKinds() const
{
	int values[NUM_DEFENSE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getDefenseKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getMaintenanceKinds() const
{
	int values[NUM_MAINTENANCE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getMaintenanceKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getBuildRateKinds() const
{
	int values[NUM_BUILD_RATE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getBuildRateKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getCombatKinds() const
{
	int values[NUM_COMBAT_KINDS] = { 0 };
	if (m_pCity) m_pCity->getCombatKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getExperienceKinds() const
{
	int values[NUM_EXPERIENCE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getExperienceKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getRevolutionKinds() const
{
	int values[NUM_REVOLUTION_KINDS] = { 0 };
	if (m_pCity) m_pCity->getRevolutionKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getTradeRouteKinds() const
{
	int values[NUM_TRADE_ROUTE_KINDS] = { 0 };
	if (m_pCity) m_pCity->getTradeRouteKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getScalars() const
{
	int values[NUM_INFO_SCALARS] = { 0 };
	if (m_pCity) m_pCity->getScalars(values);
	return cyc_toList(values);
}

python::list CyCity::getHealKinds() const
{
	int values[NUM_HEAL_KINDS] = { 0 };
	if (m_pCity) m_pCity->getHealKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getUnderworldKinds() const
{
	int values[NUM_UNDERWORLD_KINDS] = { 0 };
	if (m_pCity) m_pCity->getUnderworldKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getVisionKinds() const
{
	int values[NUM_VISION_KINDS] = { 0 };
	if (m_pCity) m_pCity->getVisionKinds(values);
	return cyc_toList(values);
}

python::list CyCity::getRealizedWellbeing(int iExtraPopulation) const
{
	PERF_SCOPE("CyCity::getRealizedWellbeing", -1);
	int values[NUM_WELLBEING_CHANNELS] = { 0 };
	if (m_pCity) m_pCity->realizedWellbeing(iExtraPopulation, values);
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
}

python::list CyCity::getPosition() const
{
	int values[2] = { -1, -1 };   // -1,-1 = no city; a real plot coordinate is never negative
	if (m_pCity)
	{
		values[0] = m_pCity->getX();
		values[1] = m_pCity->getY();
	}
	return cyc_toList(values);
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
	return cyc_toList(values);
}

python::list CyCity::getGrowth() const
{
	PERF_SCOPE("CyCity::getGrowth", -1);
	int values[NUM_CITY_GROWTH_READS] = { 0 };
	if (m_pCity) m_pCity->getGrowthRead(values);
	return cyc_toList(values);
}

python::list CyCity::getCounts() const
{
	int values[NUM_CITY_COUNT_READS] = { 0 };
	if (m_pCity) m_pCity->getCityCounts(values);
	return cyc_toList(values);
}

python::list CyCity::getGrantedExtras() const
{
	int values[NUM_CITY_GRANTED_EXTRAS] = { 0 };
	if (m_pCity)
	{
		values[GRANTED_EXTRA_HAPPINESS] = m_pCity->getExtraHappiness();
		values[GRANTED_EXTRA_HEALTH]    = m_pCity->getExtraHealth();
	}
	return cyc_toList(values);
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
	return cyc_toList(values);
}

python::list CyCity::getUnitReads(int iUnit) const
{
	int values[NUM_CITY_UNIT_READS] = { 0 };
	if (m_pCity && iUnit >= 0 && iUnit < GC.getNumUnitInfos())
	{
		m_pCity->getUnitInCity((UnitTypes)iUnit, values);
	}
	return cyc_toList(values);
}

python::list CyCity::getSpecialistReads(int iSpecialist) const
{
	int values[NUM_CITY_SPECIALIST_READS] = { 0 };
	if (m_pCity && iSpecialist >= 0 && iSpecialist < GC.getNumSpecialistInfos())
	{
		m_pCity->getSpecialistInCity((SpecialistTypes)iSpecialist, values);
	}
	return cyc_toList(values);
}

python::list CyCity::getBuildingGrantedWellbeing(int iBuilding) const
{
	int values[NUM_BUILDING_GRANTED_KINDS] = { 0 };
	if (m_pCity && iBuilding >= 0 && iBuilding < GC.getNumBuildingInfos())
	{
		values[BUILDING_GRANTED_HAPPINESS] = m_pCity->getBuildingHappyChange((BuildingTypes)iBuilding);
		values[BUILDING_GRANTED_HEALTH]    = m_pCity->getBuildingHealthChange((BuildingTypes)iBuilding);
	}
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
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
	return cyc_toList(values);
}

python::list CyCity::getCultureReads() const
{
	int values[NUM_CITY_CULTURE_READS] = { 0 };
	if (m_pCity) m_pCity->getCultureRead(values);
	return cyc_toList(values);
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
	return cyc_toList(values);
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
void CyCity::pythonPublish()
{
	python::class_<CyCity>("CyCity", python::no_init)
		.def("getOwner", &CyCity::getOwner)
		.def("getID",    &CyCity::getID)
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
		.def("getBuildingListFilterActive", &CyCity::getBuildingListFilterActive)
		.def("getUnitListFilterActive",     &CyCity::getUnitListFilterActive)

		.def("getOrder",              &CyCity::getOrder)
		.def("getBuildingListSorting", &CyCity::getBuildingListSorting)
		.def("getUnitListGrouping",   &CyCity::getUnitListGrouping)
		.def("getUnitListSorting",    &CyCity::getUnitListSorting)
		;
}
