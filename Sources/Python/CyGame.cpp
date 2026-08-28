#include "CvGameCoreDLL.h"
#include "CyPyList.h"
#include "Infos/CvBuildInfo.h"   // setBuildDisabled -- the build it toggles
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"   // the interface SELECTION reads
#include "Engine/CvUnit.h"
#include "Engine/CvCity.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "Infrastructure/CvInitCore.h"
#include "Infrastructure/CvDLLEngineIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "CyCity.h"
#include "CyDeal.h"
#include "CyGame.h"
#include "Engine/CvGameSpeedScale.h"  // the ONE game-speed pace calc these relay to
#include "CyPlot.h"
#include "CyReplayInfo.h"
#include "CvReplayInfo.h"
#include "Infos/CvGameText.h"   // getNumLanguages -- the options screen's language dropdown

//
// Python wrapper class for CvGame
//

CyGame::CyGame() : m_pGame(GC.getGame()) {}

CyGame::CyGame(CvGame& pGame) : m_pGame(pGame) {}

CyGame::CyGame(CvGameAI& pGame) : m_pGame(pGame) {}

MapTypes CyGame::getCurrentMap() const
{
	return m_pGame.getCurrentMap();
}

bool CyGame::isMultiplayer() const
{
	return GC.getInitCore().getMultiplayer();
}

void CyGame::updateScore(bool bForce)
{
	m_pGame.updateScore(bForce);
}

void CyGame::selectedCitiesGameNetMessage(int eMessage, int iData2, int iData3, int iData4, bool bOption, bool bAlt, bool bShift, bool bCtrl)
{
	GC.getGame().selectedCitiesGameNetMessage(eMessage, iData2, iData3, iData4, bOption, bAlt, bShift, bCtrl);
}

int CyGame::getSymbolID(int iSymbol) const
{
	return m_pGame.getSymbolID(iSymbol);
}

int CyGame::getAdjustedPopulationPercent(VictoryTypes eVictory) const
{
	return m_pGame.getAdjustedPopulationPercent(eVictory);
}

int CyGame::getAdjustedLandPercent(VictoryTypes eVictory) const
{
	return m_pGame.getAdjustedLandPercent(eVictory);
}

bool CyGame::isChooseElection(VoteTypes eVote) const
{
	return m_pGame.isChooseElection(eVote);
}

bool CyGame::isTeamVoteEligible(TeamTypes eTeam, VoteSourceTypes eVoteSource) const
{
	return m_pGame.isTeamVoteEligible(eTeam, eVoteSource);
}

int CyGame::countPossibleVote(VoteTypes eVote, VoteSourceTypes eVoteSource) const
{
	return m_pGame.countPossibleVote(eVote, eVoteSource);
}

int CyGame::getVoteRequired(VoteTypes eVote, VoteSourceTypes eVoteSource) const
{
	return m_pGame.getVoteRequired(eVote, eVoteSource);
}

int CyGame::getSecretaryGeneral(VoteSourceTypes eVoteSource) const
{
	return m_pGame.getSecretaryGeneral(eVoteSource);
}

bool CyGame::canHaveSecretaryGeneral(VoteSourceTypes eVoteSource) const
{
	return m_pGame.canHaveSecretaryGeneral(eVoteSource);
}

int CyGame::getVoteSourceReligion(VoteSourceTypes eVoteSource) const
{
	return m_pGame.getVoteSourceReligion(eVoteSource);
}

int CyGame::countCivPlayersAlive() const
{
	return m_pGame.countCivPlayersAlive();
}

int CyGame::countCivPlayersEverAlive() const
{
	return m_pGame.countCivPlayersEverAlive();
}

int CyGame::countCivTeamsAlive() const
{
	return m_pGame.countCivTeamsAlive();
}

int CyGame::countCivTeamsEverAlive() const
{
	return m_pGame.countCivTeamsEverAlive();
}

int CyGame::countKnownTechNumTeams(TechTypes eTech) const
{
	return m_pGame.countKnownTechNumTeams(eTech);
}

int CyGame::countReligionLevels(ReligionTypes eReligion) const
{
	return m_pGame.countReligionLevels(eReligion);
}

int CyGame::countCorporationLevels(CorporationTypes eCorporation) const
{
	return m_pGame.countCorporationLevels(eCorporation);
}

int CyGame::calculateReligionPercent(ReligionTypes eReligion) const
{
	return m_pGame.calculateReligionPercent(eReligion);
}

int CyGame::goldenAgeLength() const
{
	return m_pGame.goldenAgeLength();
}

EraTypes CyGame::getHighestEra() const
{
	return m_pGame.getHighestEra();
}

EraTypes CyGame::getCurrentEra() const
{
	return m_pGame.getCurrentEra();
}

int CyGame::getActiveTeam() const
{
	return m_pGame.getActiveTeam();
}

bool CyGame::isNetworkMultiPlayer() const
{
	return m_pGame.isNetworkMultiPlayer();
}

bool CyGame::isGameMultiPlayer() const
{
	return m_pGame.isGameMultiPlayer();
}

bool CyGame::isModem() const
{
	return m_pGame.isModem();
}

void CyGame::setModem(bool bModem)
{
	m_pGame.setModem(bModem);
}

int CyGame::getGameTurn() const
{
	return m_pGame.getGameTurn();
}

void CyGame::setGameTurn(int iNewValue)
{
	m_pGame.setGameTurn(iNewValue);
}

int CyGame::getTurnYear(int iGameTurn) const
{
	return m_pGame.getTurnYear(iGameTurn);
}

int CyGame::getGameTurnYear() const
{
	return m_pGame.getGameTurnYear();
}

int CyGame::getElapsedGameTurns() const
{
	return m_pGame.getElapsedGameTurns();
}

int CyGame::getMaxTurns() const
{
	return m_pGame.getMaxTurns();
}

void CyGame::setMaxTurns(int iNewValue)
{
	m_pGame.setMaxTurns(iNewValue);
}

int CyGame::getMaxCityElimination() const
{
	return m_pGame.getMaxCityElimination();
}

void CyGame::setMaxCityElimination(int iNewValue)
{
	m_pGame.setMaxCityElimination(iNewValue);
}

int CyGame::getNumAdvancedStartPoints() const
{
	return m_pGame.getNumAdvancedStartPoints();
}

int CyGame::getStartTurn() const
{
	return m_pGame.getStartTurn();
}

int CyGame::getStartYear() const
{
	return m_pGame.getStartYear();
}

void CyGame::setStartYear(int iNewValue)
{
	m_pGame.setStartYear(iNewValue);
}

int CyGame::getEstimateEndTurn() const
{
	return m_pGame.getEstimateEndTurn();
}

void CyGame::setEstimateEndTurn(int iNewValue)
{
	m_pGame.setEstimateEndTurn(iNewValue);
}

int CyGame::getMinutesPlayed() const
{
	return m_pGame.getMinutesPlayed();
}

int CyGame::getTargetScore() const
{
	return m_pGame.getTargetScore();
}

void CyGame::setTargetScore(int iNewValue)
{
	m_pGame.setTargetScore(iNewValue);
}

int CyGame::getNumCities() const
{
	return m_pGame.getNumCities();
}

int CyGame::getTotalPopulation() const
{
	return m_pGame.getTotalPopulation();
}

int CyGame::getTradeRoutes() const
{
	return m_pGame.getTradeRoutes();
}

void CyGame::changeTradeRoutes(int iChange)
{
	m_pGame.changeTradeRoutes(iChange);
}

void CyGame::changeNoNukesCount(int iChange)
{
	m_pGame.changeNoNukesCount(iChange);
}

int CyGame::getSecretaryGeneralTimer(int iVoteSource) const
{
	return m_pGame.getSecretaryGeneralTimer((VoteSourceTypes)iVoteSource);
}

int CyGame::getVoteTimer(int iVoteSource) const
{
	return m_pGame.getVoteTimer((VoteSourceTypes)iVoteSource);
}

int CyGame::getNukesExploded() const
{
	return m_pGame.getNukesExploded();
}

void CyGame::changeNukesExploded(int iChange)
{
	m_pGame.changeNukesExploded(iChange);
}

int CyGame::getMaxPopulation() const
{
	return m_pGame.getMaxPopulation();
}

int CyGame::getMaxLand() const
{
	return m_pGame.getMaxLand();
}

int CyGame::getMaxTech() const
{
	return m_pGame.getMaxTech();
}

int CyGame::getMaxWonders() const
{
	return m_pGame.getMaxWonders();
}

int CyGame::getInitPopulation() const
{
	return m_pGame.getInitPopulation();
}

int CyGame::getInitLand() const
{
	return m_pGame.getInitLand();
}

int CyGame::getInitTech() const
{
	return m_pGame.getInitTech();
}

int CyGame::getInitWonders() const
{
	return m_pGame.getInitWonders();
}

int CyGame::getAIAutoPlay(int iPlayer) const
{
	return m_pGame.getAIAutoPlay((PlayerTypes)iPlayer);
}

void CyGame::setAIAutoPlay(int iPlayer, int iNewValue)
{
	m_pGame.setAIAutoPlay((PlayerTypes)iPlayer, iNewValue);
}

bool CyGame::isForcedAIAutoPlay(int iPlayer) const
{
	return m_pGame.isForcedAIAutoPlay((PlayerTypes)iPlayer);
}

void CyGame::setForcedAIAutoPlay(int iPlayer, int iNewValue, bool bForced)
{
	m_pGame.setForcedAIAutoPlay((PlayerTypes)iPlayer, iNewValue, bForced);
}

int CyGame::getCircumnavigatedTeam() const
{
	return m_pGame.getCircumnavigatedTeam();
}

void CyGame::setCircumnavigatedTeam(int iTeamType)
{
	m_pGame.setCircumnavigatedTeam((TeamTypes) iTeamType);
}

bool CyGame::isDiploVote(VoteSourceTypes eVoteSource) const
{
	return m_pGame.isDiploVote((VoteSourceTypes)eVoteSource);
}

bool CyGame::isDebugMode() const
{
	return m_pGame.isDebugMode();
}

void CyGame::toggleDebugMode()
{
	m_pGame.toggleDebugMode();
}

int CyGame::getPitbossTurnTime() const
{
	return m_pGame.getPitbossTurnTime();
}

bool CyGame::isHotSeat() const
{
	return m_pGame.isHotSeat();
}

bool CyGame::isPbem() const
{
	return m_pGame.isPbem();
}

bool CyGame::isPitboss() const
{
	return m_pGame.isPitboss();
}

bool CyGame::isFinalInitialized() const
{
	return m_pGame.isFinalInitialized();
}

void CyGame::onFinalInitialized(const bool bNewGame)
{
	m_pGame.onFinalInitialized(bNewGame);
}

PlayerTypes CyGame::getActivePlayer() const
{
	return m_pGame.getActivePlayer();
}

void CyGame::setActivePlayer(PlayerTypes eNewValue, bool bForceHotSeat)
{
	m_pGame.setActivePlayer(eNewValue, bForceHotSeat);
}

int CyGame::getPausePlayer() const
{
	return m_pGame.getPausePlayer();
}

bool CyGame::isPaused() const
{
	return m_pGame.isPaused();
}

bool CyGame::getStarshipLaunched(int playaID) const
{
	return m_pGame.getStarshipLaunched(playaID);
}

VictoryTypes CyGame::getVictory() const
{
	return m_pGame.getVictory();
}

GameStateTypes CyGame::getGameState() const
{
	return m_pGame.getGameState();
}

HandicapTypes CyGame::getHandicapType() const
{
	return m_pGame.getHandicapType();
}

CalendarTypes CyGame::getCalendar() const
{
	return m_pGame.getCalendar();
}

EraTypes CyGame::getStartEra() const
{
	return m_pGame.getStartEra();
}

GameSpeedTypes CyGame::getGameSpeedType() const
{
	return m_pGame.getGameSpeedType();
}

int CyGame::getSpeedPercent() const
{
	return CvGameSpeedScale::speedPercent();
}

int CyGame::getHammerCostPercent() const
{
	return CvGameSpeedScale::hammerCostPercent();
}

PlayerTypes CyGame::getRankPlayer(int iRank) const
{
	return m_pGame.getRankPlayer(iRank);
}

int CyGame::getPlayerRank(PlayerTypes ePlayer) const
{
	return m_pGame.getPlayerRank(ePlayer);
}

int CyGame::getPlayerScore(PlayerTypes ePlayer) const
{
	return m_pGame.getPlayerScore(ePlayer);
}

TeamTypes CyGame::getRankTeam(int iRank) const
{
	return m_pGame.getRankTeam(iRank);
}

int CyGame::getTeamScore(TeamTypes eTeam) const
{
	return m_pGame.getTeamScore(eTeam);
}

bool CyGame::isOption(GameOptionTypes eIndex) const
{
	return m_pGame.isOption(eIndex);
}

void CyGame::setOption(GameOptionTypes eIndex, bool bEnabled)
{
	m_pGame.setOption(eIndex, bEnabled);
	if (bEnabled)
		m_pGame.enforceOptionCompatibility(eIndex);
}

bool CyGame::isMPOption(MultiplayerOptionTypes eIndex) const
{
	return m_pGame.isMPOption(eIndex);
}

bool CyGame::isForcedControl(ForceControlTypes eIndex) const
{
	return m_pGame.isForcedControl(eIndex);
}

bool CyGame::isUnitMaxedOut(UnitTypes eIndex, int iExtra) const
{
	return m_pGame.isUnitMaxedOut(eIndex, iExtra);
}

bool CyGame::isBuildingMaxedOut(BuildingTypes eIndex, int iExtra) const
{
	return m_pGame.isBuildingMaxedOut(eIndex, iExtra);
}

int CyGame::getProjectCreatedCount(ProjectTypes eIndex) const
{
	return m_pGame.getProjectCreatedCount(eIndex);
}

int CyGame::getReligionGameTurnFounded(ReligionTypes eIndex) const
{
	return m_pGame.getReligionGameTurnFounded(eIndex);
}

int CyGame::getCorporationGameTurnFounded(CorporationTypes eIndex) const
{
	return m_pGame.getCorporationGameTurnFounded(eIndex);
}

bool CyGame::isCorporationFounded(CorporationTypes eIndex) const
{
	return m_pGame.isCorporationFounded(eIndex);
}

bool CyGame::isVotePassed(VoteTypes eIndex) const
{
	return m_pGame.isVotePassed(eIndex);
}

bool CyGame::isVictoryValid(VictoryTypes eIndex) const
{
	return m_pGame.isVictoryValid(eIndex);
}

bool CyGame::isInAdvancedStart() const
{
	return m_pGame.isInAdvancedStart();
}

CyCity* CyGame::getHolyCity(ReligionTypes eIndex) const
{
	CvCity* city = m_pGame.getHolyCity(eIndex);
	return city ? new CyCity(city) : NULL;
}

void CyGame::setHolyCity(ReligionTypes eIndex, CyCity* pNewValue, bool bAnnounce)
{
	m_pGame.setHolyCity(eIndex, pNewValue->getCity(), bAnnounce);
}

void CyGame::clearHolyCity(ReligionTypes eIndex)
{
	m_pGame.setHolyCity(eIndex, NULL, false);
}

CyCity* CyGame::getHeadquarters(CorporationTypes eIndex) const
{
	CvCity* city = m_pGame.getHeadquarters(eIndex);
	return city ? new CyCity(city) : NULL;
}

void CyGame::setHeadquarters(CorporationTypes eIndex, CyCity* pNewValue, bool bAnnounce)
{
	m_pGame.setHeadquarters(eIndex, pNewValue->getCity(), bAnnounce);
}

void CyGame::clearHeadquarters(CorporationTypes eIndex)
{
	m_pGame.setHeadquarters(eIndex, NULL, false);
}

std::string CyGame::getScriptData() const
{
	return m_pGame.getScriptData();
}

void CyGame::setScriptData(std::string szNewValue)
{
	m_pGame.setScriptData(szNewValue);
}

void CyGame::setName(const char* szNewValue)
{
	m_pGame.setName(szNewValue);
}

std::wstring CyGame::getName() const
{
	return m_pGame.getName();
}

int CyGame::getIndexAfterLastDeal() const
{
	return m_pGame.getIndexAfterLastDeal();
}

int CyGame::getNumDeals() const
{
	return m_pGame.getNumDeals();
}

CyDeal* CyGame::getDeal(int iID) const
{
	return new CyDeal(m_pGame.getDeal(iID));
}

void CyGame::deleteDeal(int iID)
{
	m_pGame.deleteDeal(iID);
}

CvRandom& CyGame::getMapRand() const
{
	return (m_pGame.getMapRand());
}

CvRandom& CyGame::getSorenRand() const
{
	return (m_pGame.getSorenRand());
}

int CyGame::getSorenRandNum(int iNum, const char* pszLog) const
{
	return m_pGame.getSorenRandNum(iNum, pszLog);
}

bool CyGame::GetWorldBuilderMode() const				// remove once CvApp is exposed
{
	return gDLL->GetWorldBuilderMode();
}

bool CyGame::isPitbossHost() const				// remove once CvApp is exposed
{
	return gDLL->IsPitbossHost();
}

int CyGame::getCurrentLanguage() const				// remove once CvApp is exposed
{
	return gDLL->getCurrentLanguage();
}

void CyGame::setCurrentLanguage(int iNewLanguage)			// remove once CvApp is exposed
{
	gDLL->setCurrentLanguage(iNewLanguage);
}

int CyGame::getNumLanguages() const			// remove once CvApp is exposed
{
	//	The options screen's language dropdown needs the COUNT beside the current selection it already reads
	//	here. CvGameText is a C++ class Python was never published, so the call site named it directly and
	//	raised NameError -- taking the whole Game tab, and with it the rest of the options screen, blank.
	return CvGameText::getNumLanguages();
}

int CyGame::getReplayMessageTurn(int i) const
{
	return m_pGame.getReplayMessageTurn(i);
}

ReplayMessageTypes CyGame::getReplayMessageType(int i) const
{
	return m_pGame.getReplayMessageType(i);
}

int CyGame::getReplayMessagePlotX(int i) const
{
	return m_pGame.getReplayMessagePlotX(i);
}

int CyGame::getReplayMessagePlotY(int i) const
{
	return m_pGame.getReplayMessagePlotY(i);
}

int CyGame::getReplayMessagePlayer(int i) const
{
	return m_pGame.getReplayMessagePlayer(i);
}

ColorTypes CyGame::getReplayMessageColor(int i) const
{
	return m_pGame.getReplayMessageColor(i);
}

std::wstring CyGame::getReplayMessageText(int i) const
{
	return m_pGame.getReplayMessageText(i);
}

uint32_t CyGame::getNumReplayMessages() const
{
	return m_pGame.getNumReplayMessages();
}

CyReplayInfo* CyGame::getReplayInfo() const
{
	return new CyReplayInfo(m_pGame.getReplayInfo());
}

void CyGame::saveReplay(PlayerTypes ePlayer)
{
	m_pGame.saveReplay(ePlayer);
}

void CyGame::addReplayMessage(ReplayMessageTypes eType, PlayerTypes ePlayer, std::wstring pszText, int iPlotX, int iPlotY, ColorTypes eColor)
{
	m_pGame.addReplayMessage(eType, ePlayer, pszText, iPlotX, iPlotY, eColor);
}

bool CyGame::hasSkippedSaveChecksum() const
{
	return gDLL->hasSkippedSaveChecksum();
}

void CyGame::addPlayer(PlayerTypes eNewPlayer, LeaderHeadTypes eLeader, CivilizationTypes eCiv, bool bSetAlive)
{
	m_pGame.addPlayer(eNewPlayer, eLeader, eCiv, bSetAlive);
}

void CyGame::changeHumanPlayer(PlayerTypes eOldHuman, PlayerTypes eNewHuman)
{
	m_pGame.changeHumanPlayer(eOldHuman, eNewHuman);
}

void CyGame::log(const char* file, char* str)
{
	gDLL->logMsg(file, str, false, false);
#ifdef _DEBUG
	strcat(str, "\n");
	OutputDebugString(str);
#endif
}

bool CyGame::isLeaderEverActive(LeaderHeadTypes eLeader) const
{
	return m_pGame.isLeaderEverActive(eLeader);
}

void CyGame::doControl(ControlTypes iControl)
{
	m_pGame.doControl(iControl);
}

void CyGame::saveGame(std::string fileName) const
{
	if (fileName.empty())
	{
		gDLL->getEngineIFace()->AutoSave(true);
	}
	else gDLL->getEngineIFace()->SaveGame((CvString &)fileName, SAVEGAME_NORMAL);
}

int CyGame::getModderGameOption(ModderGameOptionTypes eIndex) const
{
	return m_pGame.getModderGameOption(eIndex);
}

void CyGame::setModderGameOption(ModderGameOptionTypes eIndex, int iNewValue)
{
	m_pGame.setModderGameOption(eIndex, iNewValue);
}

const char* CyGame::getC2CVersion() const
{
	return GC.getDefineSTRING("C2C_VERSION");
}

void CyGame::assignStartingPlots(bool bScenario, bool bMapScript)
{
	m_pGame.assignStartingPlots(bScenario, bMapScript);
}

void CyGame::exitWorldBuilder()
{
	m_pGame.setWorldBuilder(false);
}

//
//	A game-object HANDLE, not an info -- the wrappers the engine hands to Python callbacks.
//
python::list CyGame::getHeadSelectedCityId() const
{
	int values[2] = { -1, -1 };
	const CvCity* pCity = gDLL->getInterfaceIFace()->getHeadSelectedCity();
	if (pCity != NULL)
	{
		values[0] = (int)pCity->getOwner();
		values[1] = pCity->getID();
	}
	return cyToList(values);
}

python::list CyGame::getHeadSelectedUnitId() const
{
	int values[2] = { -1, -1 };
	const CvUnit* pUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
	if (pUnit != NULL)
	{
		values[0] = (int)pUnit->getOwner();
		values[1] = pUnit->getID();
	}
	return cyToList(values);
}

python::list CyGame::getSelectedUnitIds() const
{
	python::list list = python::list();
	const int iCount = gDLL->getInterfaceIFace()->getLengthSelectionList();
	for (int i = 0; i < iCount; ++i)
	{
		const CvUnit* pUnit = gDLL->getInterfaceIFace()->getSelectionUnit(i);
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

bool CyGame::setBuildDisabled(int iBuild, bool bDisabled) const
{
	if (iBuild < 0 || iBuild >= GC.getNumBuildInfos()) return false;
	GC.getBuildInfo((BuildTypes)iBuild).setDisabled(bDisabled);
	return true;
}

void CyGame::pythonPublish()
{
	python::class_<CyGame>("CyGame")

		.def("getHeadSelectedCityId", &CyGame::getHeadSelectedCityId)
		.def("getHeadSelectedUnitId", &CyGame::getHeadSelectedUnitId)
		.def("getSelectedUnitIds", &CyGame::getSelectedUnitIds)
		.def("setBuildDisabled", &CyGame::setBuildDisabled)

		.def("getCurrentMap", &CyGame::getCurrentMap)

		.def("isMultiplayer", &CyGame::isMultiplayer)

		.def("updateScore", &CyGame::updateScore)

		.def("selectedCitiesGameNetMessage", &CyGame::selectedCitiesGameNetMessage)

		.def("getSymbolID", &CyGame::getSymbolID)


		.def("getAdjustedPopulationPercent", &CyGame::getAdjustedPopulationPercent)
		.def("getAdjustedLandPercent", &CyGame::getAdjustedLandPercent)

		.def("isChooseElection", &CyGame::isChooseElection)
		.def("isTeamVoteEligible", &CyGame::isTeamVoteEligible)
		.def("countPossibleVote", &CyGame::countPossibleVote)
		.def("getVoteRequired", &CyGame::getVoteRequired)
		.def("getSecretaryGeneral", &CyGame::getSecretaryGeneral)
		.def("canHaveSecretaryGeneral", &CyGame::canHaveSecretaryGeneral)
		.def("getVoteSourceReligion", &CyGame::getVoteSourceReligion)

		.def("countCivPlayersAlive", &CyGame::countCivPlayersAlive)
		.def("countCivPlayersEverAlive", &CyGame::countCivPlayersEverAlive)
		.def("countCivTeamsAlive", &CyGame::countCivTeamsAlive)
		.def("countCivTeamsEverAlive", &CyGame::countCivTeamsEverAlive)

		.def("countKnownTechNumTeams", &CyGame::countKnownTechNumTeams)

		.def("countReligionLevels", &CyGame::countReligionLevels)
		.def("calculateReligionPercent", &CyGame::calculateReligionPercent)
		.def("countCorporationLevels", &CyGame::countCorporationLevels)

		.def("goldenAgeLength100", &CyGame::goldenAgeLength)

		.def("getHighestEra", &CyGame::getHighestEra)
		.def("getCurrentEra", &CyGame::getCurrentEra)

		.def("getActiveTeam", &CyGame::getActiveTeam)
		.def("isNetworkMultiPlayer", &CyGame::isNetworkMultiPlayer)
		.def("isGameMultiPlayer", &CyGame::isGameMultiPlayer)

		.def("isModem", &CyGame::isModem)
		.def("setModem", &CyGame::setModem)

		.def("getGameTurn", &CyGame::getGameTurn)
		.def("setGameTurn", &CyGame::setGameTurn)
		.def("getTurnYear", &CyGame::getTurnYear)
		.def("getGameTurnYear", &CyGame::getGameTurnYear)
		.def("getElapsedGameTurns", &CyGame::getElapsedGameTurns)
		.def("getMaxTurns", &CyGame::getMaxTurns)
		.def("setMaxTurns", &CyGame::setMaxTurns)
		.def("getMaxCityElimination", &CyGame::getMaxCityElimination)
		.def("setMaxCityElimination", &CyGame::setMaxCityElimination)
		.def("getNumAdvancedStartPoints", &CyGame::getNumAdvancedStartPoints)
		.def("getStartTurn", &CyGame::getStartTurn)
		.def("getStartYear", &CyGame::getStartYear)
		.def("setStartYear", &CyGame::setStartYear)
		.def("getEstimateEndTurn", &CyGame::getEstimateEndTurn)
		.def("setEstimateEndTurn", &CyGame::setEstimateEndTurn)
		.def("getMinutesPlayed", &CyGame::getMinutesPlayed)
		.def("getTargetScore", &CyGame::getTargetScore)
		.def("setTargetScore", &CyGame::setTargetScore)

		.def("getNumCities", &CyGame::getNumCities)
		.def("getTotalPopulation", &CyGame::getTotalPopulation)

		.def("getTradeRoutes", &CyGame::getTradeRoutes)
		.def("changeTradeRoutes", &CyGame::changeTradeRoutes)
		.def("changeNoNukesCount", &CyGame::changeNoNukesCount)
		.def("getSecretaryGeneralTimer", &CyGame::getSecretaryGeneralTimer)
		.def("getVoteTimer", &CyGame::getVoteTimer)
		.def("getNukesExploded", &CyGame::getNukesExploded)
		.def("changeNukesExploded", &CyGame::changeNukesExploded)

		.def("getMaxPopulation", &CyGame::getMaxPopulation)
		.def("getMaxLand", &CyGame::getMaxLand)
		.def("getMaxTech", &CyGame::getMaxTech)
		.def("getMaxWonders", &CyGame::getMaxWonders)
		.def("getInitPopulation", &CyGame::getInitPopulation)
		.def("getInitLand", &CyGame::getInitLand)
		.def("getInitTech", &CyGame::getInitTech)
		.def("getInitWonders", &CyGame::getInitWonders)

		.def("getAIAutoPlay", &CyGame::getAIAutoPlay)
		.def("setAIAutoPlay", &CyGame::setAIAutoPlay)
		.def("isForcedAIAutoPlay", &CyGame::isForcedAIAutoPlay)
		.def("setForcedAIAutoPlay", &CyGame::setForcedAIAutoPlay)

		.def("getCircumnavigatedTeam", &CyGame::getCircumnavigatedTeam)
		.def("setCircumnavigatedTeam", &CyGame::setCircumnavigatedTeam)
		.def("isDiploVote", &CyGame::isDiploVote)
		.def("isDebugMode", &CyGame::isDebugMode)
		.def("toggleDebugMode", &CyGame::toggleDebugMode)

		.def("getPitbossTurnTime", &CyGame::getPitbossTurnTime)
		.def("isHotSeat", &CyGame::isHotSeat)
		.def("isPbem", &CyGame::isPbem)
		.def("isPitboss", &CyGame::isPitboss)

		.def("isFinalInitialized", &CyGame::isFinalInitialized)
		.def("onFinalInitialized", &CyGame::onFinalInitialized,
			"void (bool bNewGame) - dll is poor at homing in on the load save finished game event, so python will notify it about that and new game event as well."
		)
		.def("getActivePlayer", &CyGame::getActivePlayer)
		.def("setActivePlayer", &CyGame::setActivePlayer)
		.def("getPausePlayer", &CyGame::getPausePlayer)
		.def("isPaused", &CyGame::isPaused)
		.def("getVictory", &CyGame::getVictory)
		.def("getGameState", &CyGame::getGameState)
		.def("getHandicapType", &CyGame::getHandicapType)
		.def("getCalendar", &CyGame::getCalendar)
		.def("getStartEra", &CyGame::getStartEra)
		.def("getGameSpeedType", &CyGame::getGameSpeedType)
		.def("getSpeedPercent", &CyGame::getSpeedPercent)
		.def("getHammerCostPercent", &CyGame::getHammerCostPercent)
		.def("getRankPlayer", &CyGame::getRankPlayer)
		.def("getPlayerRank", &CyGame::getPlayerRank)
		.def("getPlayerScore", &CyGame::getPlayerScore)
		.def("getRankTeam", &CyGame::getRankTeam)
		.def("getTeamScore", &CyGame::getTeamScore)
		.def("isOption", &CyGame::isOption)
		.def("setOption", &CyGame::setOption)
		.def("isMPOption", &CyGame::isMPOption)
		.def("isForcedControl", &CyGame::isForcedControl)
		.def("isBuildingMaxedOut", &CyGame::isBuildingMaxedOut)
		.def("isUnitMaxedOut", &CyGame::isUnitMaxedOut)

		.def("getProjectCreatedCount", &CyGame::getProjectCreatedCount)



		.def("getReligionGameTurnFounded", &CyGame::getReligionGameTurnFounded)


		.def("getCorporationGameTurnFounded", &CyGame::getCorporationGameTurnFounded)
		.def("isCorporationFounded", &CyGame::isCorporationFounded)
		.def("isVictoryValid", &CyGame::isVictoryValid)
		.def("isVotePassed", &CyGame::isVotePassed)


		.def("isInAdvancedStart", &CyGame::isInAdvancedStart)

		.def("getHolyCity", &CyGame::getHolyCity, python::return_value_policy<python::manage_new_object>())
		.def("setHolyCity", &CyGame::setHolyCity)
		.def("clearHolyCity", &CyGame::clearHolyCity)

		.def("getHeadquarters", &CyGame::getHeadquarters, python::return_value_policy<python::manage_new_object>())
		.def("setHeadquarters", &CyGame::setHeadquarters)
		.def("clearHeadquarters", &CyGame::clearHeadquarters)


		.def("getScriptData", &CyGame::getScriptData)
		.def("setScriptData", &CyGame::setScriptData)

		.def("setName", &CyGame::setName)
		.def("getName", &CyGame::getName)
		.def("getIndexAfterLastDeal", &CyGame::getIndexAfterLastDeal)
		.def("getNumDeals", &CyGame::getNumDeals)
		.def("getDeal", &CyGame::getDeal, python::return_value_policy<python::manage_new_object>())
		.def("getMapRand", &CyGame::getMapRand, python::return_value_policy<python::reference_existing_object>())
		.def("getSorenRand", &CyGame::getSorenRand, python::return_value_policy<python::reference_existing_object>())
		.def("getSorenRandNum", &CyGame::getSorenRandNum)

		.def("GetWorldBuilderMode", &CyGame::GetWorldBuilderMode)
		.def("isPitbossHost", &CyGame::isPitbossHost)
		.def("getCurrentLanguage", &CyGame::getCurrentLanguage)
		.def("setCurrentLanguage", &CyGame::setCurrentLanguage)
		.def("getNumLanguages", &CyGame::getNumLanguages)

		.def("getReplayMessageTurn", &CyGame::getReplayMessageTurn)
		.def("getReplayMessageType", &CyGame::getReplayMessageType)
		.def("getReplayMessagePlotX", &CyGame::getReplayMessagePlotX)
		.def("getReplayMessagePlotY", &CyGame::getReplayMessagePlotY)
		.def("getReplayMessagePlayer", &CyGame::getReplayMessagePlayer)
		.def("getReplayMessageColor", &CyGame::getReplayMessageColor)
		.def("getReplayMessageText", &CyGame::getReplayMessageText)
		.def("getNumReplayMessages", &CyGame::getNumReplayMessages)
		.def("getReplayInfo", &CyGame::getReplayInfo, python::return_value_policy<python::manage_new_object>())
		.def("hasSkippedSaveChecksum", &CyGame::hasSkippedSaveChecksum)
		.def("saveReplay", &CyGame::saveReplay)

		.def("addPlayer", &CyGame::addPlayer)
		//.def("addPlayer", &CyGame::addPlayer)
		.def("changeHumanPlayer", &CyGame::changeHumanPlayer, "void ( int /*PlayerTypes*/ eOldHuman, int /*PlayerTypes*/ eNewHuman )" )
		.def("addReplayMessage", &CyGame::addReplayMessage, "void (int /*ReplayMessageTypes*/ eType, int /*PlayerTypes*/ ePlayer, std::wstring pszText, int iPlotX, int iPlotY, int /*ColorTypes*/ eColor)" )
		.def("log", &CyGame::log)

		.def("isLeaderEverActive", &CyGame::isLeaderEverActive)

		.def("doControl", &CyGame::doControl)


		.def("saveGame", &CyGame::saveGame)


		.def("getStarshipLaunched", &CyGame::getStarshipLaunched)

		.def("getModderGameOption", &CyGame::getModderGameOption)
		.def("setModderGameOption", &CyGame::setModderGameOption)


		.def("getC2CVersion", &CyGame::getC2CVersion)

		.def("assignStartingPlots", &CyGame::assignStartingPlots)

		.def("exitWorldBuilder", &CyGame::exitWorldBuilder)
	;


	python::class_<CyDeal>("CyDeal")

		.def("isNone", &CyDeal::isNone)
		.def("getID", &CyDeal::getID)
		.def("getInitialGameTurn", &CyDeal::getInitialGameTurn)
		.def("getFirstPlayer", &CyDeal::getFirstPlayer)
		.def("getSecondPlayer", &CyDeal::getSecondPlayer)
		.def("getLengthFirstTrades", &CyDeal::getLengthFirstTrades)
		.def("getLengthSecondTrades", &CyDeal::getLengthSecondTrades)
		.def("getFirstTrade", &CyDeal::getFirstTrade, python::return_value_policy<python::reference_existing_object>())
		.def("getSecondTrade", &CyDeal::getSecondTrade, python::return_value_policy<python::reference_existing_object>())
		.def("kill", &CyDeal::kill)

		.def("isCancelable", &CyDeal::isCancelable)
		.def("getCannotCancelReason", &CyDeal::getCannotCancelReason)
		.def("turnsToCancel", &CyDeal::turnsToCancel)
	;
}
