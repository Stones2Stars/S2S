
#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "AI/CvPlayerAI.h"
#include "Engine/CvTeam.h"
#include "CyArea.h"
#include "CyTeam.h"

//
// Python wrapper class for CvTeam
//

CyTeam::CyTeam() : m_pTeam(NULL) {}

CyTeam::CyTeam(CvTeam* pTeam) : m_pTeam(pTeam) {}


bool CyTeam::isRebel() const
{
	return m_pTeam->isRebel();
}

bool CyTeam::isSingleCityTeam() const
{
	return m_pTeam->isSingleCityTeam();
}

bool CyTeam::isRebelAgainst(int iTeam) const
{
	return m_pTeam->isRebelAgainst((TeamTypes)iTeam);
}

void CyTeam::setRebelAgainst(int iTeam, bool bNewValue)
{
	m_pTeam->setRebelAgainst((TeamTypes)iTeam, bNewValue);
}

int CyTeam::countRebelAgainst() const
{
	return m_pTeam->countRebelAgainst();
}

void CyTeam::addTeam(int /*TeamTypes*/ eTeam)
{
	m_pTeam->addTeam((TeamTypes)eTeam);
}

bool CyTeam::canChangeWarPeace(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->canChangeWarPeace((TeamTypes)eTeam);
}

bool CyTeam::canDeclareWar(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->canDeclareWar((TeamTypes)eTeam);
}

void CyTeam::declareWar(int /*TeamTypes*/ eTeam, bool bNewDiplo, int /*WarPlanTypes*/ eWarPlan)
{
	m_pTeam->declareWar((TeamTypes)eTeam, bNewDiplo, (WarPlanTypes)eWarPlan);
}

void CyTeam::makePeace(int /*TeamTypes*/ eTeam)
{
	m_pTeam->makePeace((TeamTypes)eTeam);
}

bool CyTeam::canContact(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->canContact((TeamTypes)eTeam);
}

void CyTeam::meet(int /*TeamTypes*/ eTeam, bool bNewDiplo)
{
	m_pTeam->meet((TeamTypes)eTeam, bNewDiplo);
}

void CyTeam::signOpenBorders(int /*TeamTypes*/ eTeam)
{
	m_pTeam->signOpenBorders((TeamTypes)eTeam);
}

void CyTeam::signDefensivePact(int /*TeamTypes*/ eTeam)
{
	m_pTeam->signDefensivePact((TeamTypes)eTeam);
}

int CyTeam::getAssets() const
{
	return m_pTeam->getAssets();
}

int CyTeam::getPower(bool bIncludeVassals) const
{
	return m_pTeam->getPower(bIncludeVassals);
}

int64_t CyTeam::getTotalVictoryScore() const
{
	return m_pTeam->getTotalVictoryScore();
}

int CyTeam::getAtWarCount(bool bIgnoreMinors) const
{
	return m_pTeam->getAtWarCount(bIgnoreMinors);
}

bool CyTeam::isAtWar(bool bCountMinors) const
{
	return m_pTeam->isAtWar(bCountMinors);
}

int CyTeam::getHasMetCivCount(bool bIgnoreMinors) const
{
	return m_pTeam->getHasMetCivCount(bIgnoreMinors);
}

bool CyTeam::isAVassal() const
{
	return m_pTeam->isAVassal();
}

int CyTeam::getUnitMaking(int /*UnitTypes*/ eUnit) const
{
	return m_pTeam->getUnitMaking((UnitTypes)eUnit);
}

int CyTeam::getUnitCountPlusMaking(int /*UnitTypes*/ eUnit) const
{
	return m_pTeam->getUnitCountPlusMaking((UnitTypes)eUnit);
}

int CyTeam::getBuildingCountPlusMaking(int /*BuildingTypes*/ eBuilding) const
{
	return m_pTeam->getBuildingCountPlusMaking((BuildingTypes)eBuilding);
}

int CyTeam::getHasReligionCount(int /*ReligionTypes*/ eReligion) const
{
	return m_pTeam->getHasReligionCount((ReligionTypes)eReligion);
}

int CyTeam::getHasCorporationCount(int /*CorporationTypes*/ eReligion) const
{
	return m_pTeam->getHasCorporationCount((CorporationTypes)eReligion);
}

int64_t CyTeam::countTotalCulture() const
{
	return m_pTeam->countTotalCulture();
}

int CyTeam::countNumCitiesByArea(const CyArea* pArea) const
{
	return m_pTeam->countNumCitiesByArea(pArea->getArea());
}

int CyTeam::getResearchCost(int /*TechTypes*/ eTech) const
{
	return m_pTeam->getResearchCost((TechTypes)eTech);
}

bool CyTeam::hasHolyCity(int /*ReligionTypes*/ eReligion) const
{
	return m_pTeam->hasHolyCity((ReligionTypes)eReligion);
}

bool CyTeam::hasHeadquarters(int /*CorporationTypes*/ eCorporation) const
{
	return m_pTeam->hasHeadquarters((CorporationTypes)eCorporation);
}

bool CyTeam::isHuman() const
{
	return m_pTeam->isHuman();
}

bool CyTeam::isNPC() const
{
	return m_pTeam->isNPC();
}

bool CyTeam::isHominid() const
{
	return m_pTeam->isHominid();
}

bool CyTeam::isMinorCiv() const
{
	return m_pTeam->isMinorCiv();
}

void CyTeam::setIsMinorCiv(bool bNewValue)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).getTeam() == m_pTeam->getID() && GET_PLAYER((PlayerTypes)iI).getCivilizationType() < 0)
		{
			FErrorMsg("GET_PLAYER((PlayerTypes)iI) of m_pTeam should have a civilizationType");
		}
	}
	m_pTeam->setIsMinorCiv(bNewValue);
}

int /*PlayerTypes*/ CyTeam::getLeaderID() const
{
	return m_pTeam->getLeaderID();
}

int /*HandicapTypes*/ CyTeam::getHandicapType() const
{
	return m_pTeam->getHandicapType();
}

std::wstring CyTeam::getName() const
{
	return m_pTeam->getName();
}

int CyTeam::getNumMembers() const
{
	return m_pTeam->getNumMembers();
}

bool CyTeam::isAlive() const
{
	return m_pTeam->isAlive();
}

bool CyTeam::isEverAlive() const
{
	return m_pTeam->isEverAlive();
}

int CyTeam::getNumCities() const
{
	return m_pTeam->getNumCities();
}

int CyTeam::getTotalPopulation() const
{
	return m_pTeam->getTotalPopulation();
}

int CyTeam::getTotalLand() const
{
	return m_pTeam->getTotalLand();
}

int CyTeam::getForceTeamVoteEligibilityCount(int /*VoteSourceTypes*/ eVoteSource) const
{
	return m_pTeam->getForceTeamVoteEligibilityCount((VoteSourceTypes)eVoteSource);
}

bool CyTeam::isForceTeamVoteEligible(int /*VoteSourceTypes*/ eVoteSource) const
{
	return m_pTeam->isForceTeamVoteEligible((VoteSourceTypes)eVoteSource);
}

void CyTeam::changeForceTeamVoteEligibilityCount(int /*VoteSourceTypes*/ eVoteSource, int iChange)
{
	m_pTeam->changeForceTeamVoteEligibilityCount((VoteSourceTypes)eVoteSource, iChange);
}

bool CyTeam::isExtraWaterSeeFrom() const
{
	return m_pTeam->isExtraWaterSeeFrom();
}

bool CyTeam::isMapTrading() const
{
	return m_pTeam->isMapTrading();
}

bool CyTeam::isTechTrading() const
{
	return m_pTeam->isTechTrading();
}

bool CyTeam::isGoldTrading() const
{
	return m_pTeam->isGoldTrading();
}

bool CyTeam::isOpenBordersTrading() const
{
	return m_pTeam->isOpenBordersTrading();
}

bool CyTeam::isDefensivePactTrading() const
{
	return m_pTeam->isDefensivePactTrading();
}

bool CyTeam::isPermanentAllianceTrading() const
{
	return m_pTeam->isPermanentAllianceTrading();
}

bool CyTeam::isVassalStateTrading() const
{
	return m_pTeam->isVassalStateTrading();
}

bool CyTeam::isBridgeBuilding() const
{
	return m_pTeam->isBridgeBuilding();
}

bool CyTeam::isIrrigation() const
{
	return m_pTeam->isIrrigation();
}

bool CyTeam::isIgnoreIrrigation() const
{
	return m_pTeam->isIgnoreIrrigation();
}

bool CyTeam::isWaterWork() const
{
	return m_pTeam->isWaterWork();
}

int CyTeam::getVassalPower() const
{
	return m_pTeam->getVassalPower();
}

void CyTeam::setVassalPower(int iPower)
{
	m_pTeam->setVassalPower(iPower);
}

int CyTeam::getMasterPower() const
{
	return m_pTeam->getMasterPower();
}

void CyTeam::setMasterPower(int iPower)
{
	m_pTeam->setMasterPower(iPower);
}

int CyTeam::getEnemyWarWearinessModifier() const
{
	return m_pTeam->getEnemyWarWearinessModifier();
}

void CyTeam::changeEnemyWarWearinessModifier(int iChange)
{
	m_pTeam->changeEnemyWarWearinessModifier(iChange);
}

bool CyTeam::isMapCentering() const
{
	return m_pTeam->isMapCentering();
}

void CyTeam::setMapCentering(bool bNewValue)
{
	m_pTeam->setMapCentering(bNewValue);
}

int CyTeam::getID() const
{
	return m_pTeam->getID();
}

int CyTeam::getWarWeariness(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->getWarWeariness((TeamTypes)eIndex);
}

void CyTeam::changeWarWeariness(int /*TeamTypes*/ eIndex, int iChange)
{
	m_pTeam->changeWarWeariness((TeamTypes)eIndex, iChange);
}

// The commerce-flexible counter is CUT (#430, capabilities.md): the sliders derive from the player's ability union.
// The count getter answers boolean-as-count; the changer is a documented dead poke (grant the tech instead).
bool CyTeam::isCommerceFlexible(int /*CommerceTypes*/ eIndex) const
{
	return m_pTeam->isCommerceFlexible((CommerceTypes)eIndex);
}

int CyTeam::getExtraMoves(int /*DomainTypes*/ eIndex) const
{
	return m_pTeam->getExtraMoves((DomainTypes)eIndex);
}

void CyTeam::changeExtraMoves(int /*DomainTypes*/ eIndex, int iChange)
{
	m_pTeam->changeExtraMoves((DomainTypes)eIndex, iChange);
}

bool CyTeam::isHasMet(int /*TeamTypes*/ eIndex) const
{
	//Fuyu: Catching Civ4lerts mess-ups
	FASSERT_BOUNDS(0, MAX_TEAMS, eIndex);
	if (eIndex < 0 || eIndex >= MAX_TEAMS)
	{
#ifdef _DEBUG
		throw new std::exception();
#endif
        return false;
	}

	return m_pTeam->isHasMet((TeamTypes)eIndex);
}

bool CyTeam::isAtWarWith(int /*TeamTypes*/ iIndex) const
{
	return iIndex != NO_TEAM ? m_pTeam->isAtWar((TeamTypes)iIndex) : false;
}

bool CyTeam::isPermanentWarPeace(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->isPermanentWarPeace((TeamTypes)eIndex);
}

void CyTeam::setPermanentWarPeace(int /*TeamTypes*/ eIndex, bool bNewValue)
{
	m_pTeam->setPermanentWarPeace((TeamTypes)eIndex, bNewValue);
}

bool CyTeam::isOpenBorders(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->isOpenBorders((TeamTypes)eIndex);
}

bool CyTeam::isForcePeace(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->isForcePeace((TeamTypes)eIndex);
}

bool CyTeam::isVassal(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->isVassal((TeamTypes)eIndex);
}

void CyTeam::assignVassal(int /*TeamTypes*/ eIndex, bool bSurrender)
{
	m_pTeam->assignVassal((TeamTypes)eIndex, bSurrender);
}

void CyTeam::freeVassal(int /*TeamTypes*/ eIndex)
{
	m_pTeam->freeVassal((TeamTypes)eIndex);
}

bool CyTeam::isDefensivePact(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->isDefensivePact((TeamTypes)eIndex);
}

int CyTeam::getProjectCount(int /*ProjectTypes*/ eIndex) const
{
	return m_pTeam->getProjectCount((ProjectTypes)eIndex);
}

int CyTeam::getProjectDefaultArtType(int /*ProjectTypes*/ eIndex) const
{
	return m_pTeam->getProjectDefaultArtType((ProjectTypes)eIndex);
}

void CyTeam::setProjectArtType(int /*ProjectTypes*/ eIndex, int number, int value)
{
	m_pTeam->setProjectArtType((ProjectTypes)eIndex, number, value);
}

void CyTeam::changeProjectCount(int /*ProjectTypes*/ eIndex, int iChange)
{
	m_pTeam->changeProjectCount((ProjectTypes)eIndex, iChange);
}

int CyTeam::getProjectMaking(int /*ProjectTypes*/ eIndex) const
{
	return m_pTeam->getProjectMaking((ProjectTypes)eIndex);
}

int CyTeam::getUnitCount(int /*UnitTypes*/ eIndex) const
{
	return m_pTeam->getUnitCount((UnitTypes)eIndex);
}

int CyTeam::getBuildingCount(int /*BuildingTypes*/ eIndex) const
{
	return m_pTeam->getBuildingCount((BuildingTypes)eIndex);
}

bool CyTeam::isBuildingMaxedOut(int /*BuildingTypes*/ eIndex, int iExtra) const
{
	return m_pTeam->isBuildingMaxedOut((BuildingTypes)eIndex, iExtra);
}

bool CyTeam::isObsoleteBuilding(int /*BuildingTypes*/ eIndex) const
{
	return m_pTeam->isObsoleteBuilding((BuildingTypes)eIndex);
}

int CyTeam::getResearchProgress(int /*TechTypes*/ eIndex) const
{
	return m_pTeam->getResearchProgress((TechTypes)eIndex);
}

void CyTeam::setResearchProgress(int /*TechTypes*/ eIndex, int iNewValue, int /*PlayerTypes*/ ePlayer)
{
	if (eIndex != NO_TECH)
		m_pTeam->setResearchProgress((TechTypes)eIndex, iNewValue, (PlayerTypes)ePlayer);
}

void CyTeam::changeResearchProgress(int /*TechTypes*/ eIndex, int iChange, int /*PlayerTypes*/ ePlayer)
{
	if (eIndex != NO_TECH)
		m_pTeam->changeResearchProgress((TechTypes)eIndex, iChange, (PlayerTypes)ePlayer);
}

int CyTeam::getTechCount(int /*TechTypes*/ eIndex) const
{
	return m_pTeam->getTechCount((TechTypes)eIndex);
}

bool CyTeam::isTerrainTrade(int /*TerrainTypes*/ eIndex) const
{
	return m_pTeam->isTerrainTrade((TerrainTypes)eIndex);
}

bool CyTeam::isRiverTrade() const
{
	return m_pTeam->isRiverTrade();
}

bool CyTeam::isHasTech(int /*TechTypes*/ iIndex) const
{
	return m_pTeam->isHasTech((TechTypes)iIndex);
}

void CyTeam::setHasTech(int /*TechTypes*/ eIndex, bool bNewValue, int /*PlayerTypes*/ ePlayer, bool bFirst, bool bAnnounce)
{
	m_pTeam->setHasTech((TechTypes)eIndex, bNewValue, (PlayerTypes)ePlayer, bFirst, bAnnounce);
}

bool CyTeam::isNoTradeTech(int /*TechTypes*/ iIndex) const
{
	return m_pTeam->isNoTradeTech((TechTypes)iIndex);
}


int CyTeam::getVictoryCountdown(int /*VictoryTypes*/ eVictory) const
{
	return m_pTeam->getVictoryCountdown((VictoryTypes)eVictory);
}

int CyTeam::getVictoryDelay(int /*VictoryTypes*/ eVictory) const
{
	return m_pTeam->getVictoryDelay((VictoryTypes)eVictory);
}

bool CyTeam::canLaunch(int /*VictoryTypes*/ eVictory) const
{
	return m_pTeam->canLaunch((VictoryTypes)eVictory);
}

int CyTeam::getLaunchSuccessRate(int /*VictoryTypes*/ eVictory) const
{
	return m_pTeam->getLaunchSuccessRate((VictoryTypes)eVictory);
}

int CyTeam::getEspionagePointsAgainstTeam(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->getEspionagePointsAgainstTeam((TeamTypes) eIndex);
}

void CyTeam::setEspionagePointsAgainstTeam(int /*TeamTypes*/ eIndex, int iValue)
{
	m_pTeam->setEspionagePointsAgainstTeam((TeamTypes) eIndex, iValue);
}

void CyTeam::changeEspionagePointsAgainstTeam(int /*TeamTypes*/ eIndex, int iChange)
{
	m_pTeam->changeEspionagePointsAgainstTeam((TeamTypes) eIndex, iChange);
}

int CyTeam::getEspionagePointsEver() const
{
	return m_pTeam->getEspionagePointsEver();
}

void CyTeam::setEspionagePointsEver(int iValue)
{
	m_pTeam->setEspionagePointsEver(iValue);
}

void CyTeam::changeEspionagePointsEver(int iChange)
{
	m_pTeam->changeEspionagePointsEver(iChange);
}

int CyTeam::getCounterespionageTurnsLeftAgainstTeam(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->getCounterespionageTurnsLeftAgainstTeam((TeamTypes) eIndex);
}

void CyTeam::changeCounterespionageTurnsLeftAgainstTeam(int /*TeamTypes*/ eIndex, int iChange)
{
	m_pTeam->changeCounterespionageTurnsLeftAgainstTeam((TeamTypes) eIndex, iChange);
}

int CyTeam::getCounterespionageModAgainstTeam(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->getCounterespionageModAgainstTeam((TeamTypes) eIndex);
}

void CyTeam::changeCounterespionageModAgainstTeam(int /*TeamTypes*/ eIndex, int iChange)
{
	m_pTeam->changeCounterespionageModAgainstTeam((TeamTypes) eIndex, iChange);
}

bool CyTeam::AI_shareWar(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->AI_shareWar((TeamTypes)eTeam);
}

int CyTeam::AI_getAtWarCounter(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->AI_getAtWarCounter((TeamTypes)eTeam);
}

int CyTeam::AI_getAtPeaceCounter(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->AI_getAtPeaceCounter((TeamTypes)eTeam);
}

int CyTeam::AI_getWarSuccess(int /*TeamTypes*/ eIndex) const
{
	return m_pTeam->AI_getWarSuccess((TeamTypes)eIndex);
}

bool CyTeam::isHasEmbassy(int /*TeamTypes*/ eTeam) const
{
	return m_pTeam->isHasEmbassy((TeamTypes)eTeam);
}

//
//	A game-object HANDLE, not an info -- the wrappers the engine hands to Python callbacks.
//
void CyTeam::pythonPublish()
{
	python::class_<CyTeam>("CyTeam", python::no_init)
		.def("isRebelAgainst", &CyTeam::isRebelAgainst, "bool ( int iTeam )" )
		.def("setRebelAgainst", &CyTeam::setRebelAgainst, "void ( int iTeam, bool bNewValue )" )
		.def("countRebelAgainst", &CyTeam::countRebelAgainst, "int ( )" )

		.def("addTeam", &CyTeam::addTeam)

		.def("canChangeWarPeace", &CyTeam::canChangeWarPeace)
		.def("canDeclareWar", &CyTeam::canDeclareWar)
		.def("declareWar", &CyTeam::declareWar)
		.def("makePeace", &CyTeam::makePeace)
		.def("canContact", &CyTeam::canContact)
		.def("meet", &CyTeam::meet)
		.def("signOpenBorders", &CyTeam::signOpenBorders)
		.def("signDefensivePact", &CyTeam::signDefensivePact)

		.def("getAssets", &CyTeam::getAssets)
		.def("getPower", &CyTeam::getPower)

		.def("getTotalVictoryScore", &CyTeam::getTotalVictoryScore)

		.def("isAtWar", &CyTeam::isAtWar)
		.def("getAtWarCount", &CyTeam::getAtWarCount)
		.def("getHasMetCivCount", &CyTeam::getHasMetCivCount)
		.def("isAVassal", &CyTeam::isAVassal)

		.def("getBuildingCountPlusMaking", &CyTeam::getBuildingCountPlusMaking)
		.def("getUnitMaking", &CyTeam::getUnitMaking)
		.def("getUnitCountPlusMaking", &CyTeam::getUnitCountPlusMaking)
		.def("getHasReligionCount", &CyTeam::getHasReligionCount)
		.def("getHasCorporationCount", &CyTeam::getHasCorporationCount)

		.def("countTotalCulture", &CyTeam::countTotalCulture)

		.def("countNumCitiesByArea", &CyTeam::countNumCitiesByArea)

		.def("getResearchCost", &CyTeam::getResearchCost)

		.def("hasHolyCity", &CyTeam::hasHolyCity)
		.def("hasHeadquarters", &CyTeam::hasHeadquarters)

		.def("isHuman", &CyTeam::isHuman)
		.def("isNPC", &CyTeam::isNPC)
		.def("isHominid", &CyTeam::isNPC)
		.def("isMinorCiv", &CyTeam::isMinorCiv)
		.def("setIsMinorCiv", &CyTeam::setIsMinorCiv)

		.def("getLeaderID", &CyTeam::getLeaderID)
		.def("getHandicapType", &CyTeam::getHandicapType)
		.def("getName", &CyTeam::getName)

		.def("getNumMembers", &CyTeam::getNumMembers)
		.def("isAlive", &CyTeam::isAlive)
		.def("isEverAlive", &CyTeam::isEverAlive)
		.def("getNumCities", &CyTeam::getNumCities)
		.def("getTotalPopulation", &CyTeam::getTotalPopulation)
		.def("getTotalLand", &CyTeam::getTotalLand)

		.def("getForceTeamVoteEligibilityCount", &CyTeam::getForceTeamVoteEligibilityCount)
		.def("isForceTeamVoteEligible", &CyTeam::isForceTeamVoteEligible)
		.def("changeForceTeamVoteEligibilityCount", &CyTeam::changeForceTeamVoteEligibilityCount)
		.def("isExtraWaterSeeFrom", &CyTeam::isExtraWaterSeeFrom)
		.def("isMapTrading", &CyTeam::isMapTrading)
		.def("isTechTrading", &CyTeam::isTechTrading)
		.def("isGoldTrading", &CyTeam::isGoldTrading)
		.def("isOpenBordersTrading", &CyTeam::isOpenBordersTrading)
		.def("isDefensivePactTrading", &CyTeam::isDefensivePactTrading)
		.def("isPermanentAllianceTrading", &CyTeam::isPermanentAllianceTrading)
		.def("isVassalStateTrading", &CyTeam::isVassalStateTrading)
		.def("isBridgeBuilding", &CyTeam::isBridgeBuilding)
		.def("isIrrigation", &CyTeam::isIrrigation)
		.def("isIgnoreIrrigation", &CyTeam::isIgnoreIrrigation)
		.def("isWaterWork", &CyTeam::isWaterWork)

		.def("getVassalPower", &CyTeam::getVassalPower)
		.def("setVassalPower", &CyTeam::setVassalPower)
		.def("getMasterPower", &CyTeam::getMasterPower)
		.def("setMasterPower", &CyTeam::setMasterPower)

		.def("isMapCentering", &CyTeam::isMapCentering)
		.def("setMapCentering", &CyTeam::setMapCentering)

		.def("getEnemyWarWearinessModifier", &CyTeam::getEnemyWarWearinessModifier)
		.def("changeEnemyWarWearinessModifier", &CyTeam::changeEnemyWarWearinessModifier)

		.def("getID", &CyTeam::getID)

		.def("getWarWeariness", &CyTeam::getWarWeariness)
		.def("changeWarWeariness", &CyTeam::changeWarWeariness)
		.def("isCommerceFlexible", &CyTeam::isCommerceFlexible)

		.def("getExtraMoves", &CyTeam::getExtraMoves)
		.def("changeExtraMoves", &CyTeam::changeExtraMoves)

		.def("isHasMet", &CyTeam::isHasMet)
		.def("isAtWarWith", &CyTeam::isAtWarWith)
		.def("isPermanentWarPeace", &CyTeam::isPermanentWarPeace)
		.def("setPermanentWarPeace", &CyTeam::setPermanentWarPeace)
		.def("isOpenBorders", &CyTeam::isOpenBorders)
		.def("isForcePeace", &CyTeam::isForcePeace)
		.def("isVassal", &CyTeam::isVassal)
		.def("assignVassal", &CyTeam::assignVassal)
		.def("freeVassal", &CyTeam::freeVassal)
		.def("isDefensivePact", &CyTeam::isDefensivePact)
		.def("getProjectCount", &CyTeam::getProjectCount)
		.def("getProjectDefaultArtType", &CyTeam::getProjectDefaultArtType)
		.def("setProjectArtType", &CyTeam::setProjectArtType)
		.def("changeProjectCount", &CyTeam::changeProjectCount)
		.def("getProjectMaking", &CyTeam::getProjectMaking)
		.def("getBuildingCount", &CyTeam::getBuildingCount)
		.def("isBuildingMaxedOut", &CyTeam::isBuildingMaxedOut)
		.def("getUnitCount", &CyTeam::getUnitCount)
		.def("isObsoleteBuilding", &CyTeam::isObsoleteBuilding)

		.def("getResearchProgress", &CyTeam::getResearchProgress)
		.def("setResearchProgress", &CyTeam::setResearchProgress)
		.def("changeResearchProgress", &CyTeam::changeResearchProgress)
		.def("isTerrainTrade", &CyTeam::isTerrainTrade)
		.def("isRiverTrade", &CyTeam::isRiverTrade)
		.def("getTechCount", &CyTeam::getTechCount)

		.def("isHasTech", &CyTeam::isHasTech)
		.def("setHasTech", &CyTeam::setHasTech)
		.def("isNoTradeTech", &CyTeam::isNoTradeTech)



		.def("isHasEmbassy", &CyTeam::isHasEmbassy)

		.def("getVictoryCountdown", &CyTeam::getVictoryCountdown)
		.def("getVictoryDelay", &CyTeam::getVictoryDelay)
		.def("canLaunch", &CyTeam::canLaunch)
		.def("getLaunchSuccessRate", &CyTeam::getLaunchSuccessRate)

		.def("getEspionagePointsAgainstTeam", &CyTeam::getEspionagePointsAgainstTeam)
		.def("setEspionagePointsAgainstTeam", &CyTeam::setEspionagePointsAgainstTeam)
		.def("changeEspionagePointsAgainstTeam", &CyTeam::changeEspionagePointsAgainstTeam)
		.def("getEspionagePointsEver", &CyTeam::getEspionagePointsEver)
		.def("setEspionagePointsEver", &CyTeam::setEspionagePointsEver)
		.def("changeEspionagePointsEver", &CyTeam::changeEspionagePointsEver)
		.def("getCounterespionageTurnsLeftAgainstTeam", &CyTeam::getCounterespionageTurnsLeftAgainstTeam)
		.def("changeCounterespionageTurnsLeftAgainstTeam", &CyTeam::changeCounterespionageTurnsLeftAgainstTeam)
		.def("getCounterespionageModAgainstTeam", &CyTeam::getCounterespionageModAgainstTeam)
		.def("changeCounterespionageModAgainstTeam", &CyTeam::changeCounterespionageModAgainstTeam)
		.def("AI_shareWar", &CyTeam::AI_shareWar)
		.def("AI_getAtWarCounter", &CyTeam::AI_getAtWarCounter)
		.def("AI_getAtPeaceCounter", &CyTeam::AI_getAtPeaceCounter)
		.def("AI_getWarSuccess", &CyTeam::AI_getWarSuccess)
	;
}
