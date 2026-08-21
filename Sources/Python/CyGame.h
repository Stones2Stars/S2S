#pragma once

#ifndef CyGame_h
#define CyGame_h
//
// Python wrapper class for CvGame
// SINGLETON
// updated 6-5

class CvGame;
class CvGameAI;
class CyCity;
class CvRandom;
class CyDeal;
class CyReplayInfo;
class CyPlot;

class CyGame
{
public:
	CyGame();

	// Publishes the HANDLE (not an info).
	static void pythonPublish();
	explicit CyGame(CvGame& pGame);			// Call from C++
	explicit CyGame(CvGameAI& pGame);		// Call from C++;

	MapTypes getCurrentMap() const;

	bool isMultiplayer() const;

	void updateScore(bool bForce);

	void selectedCitiesGameNetMessage(int eMessage, int iData2, int iData3, int iData4, bool bOption, bool bAlt, bool bShift, bool bCtrl);

	int getSymbolID(int iSymbol) const;


	int getAdjustedPopulationPercent(VictoryTypes eVictory) const;
	int getAdjustedLandPercent( VictoryTypes eVictory) const;

	bool isChooseElection(VoteTypes eVote) const;
	bool isTeamVoteEligible(TeamTypes eTeam, VoteSourceTypes eVoteSource) const;
	int countPossibleVote(VoteTypes eVote, VoteSourceTypes eVoteSource) const;
	int getVoteRequired(VoteTypes eVote, VoteSourceTypes eVoteSource) const;
	int getSecretaryGeneral(VoteSourceTypes eVoteSource) const;
	bool canHaveSecretaryGeneral(VoteSourceTypes eVoteSource) const;
	int getVoteSourceReligion(VoteSourceTypes eVoteSource) const;

	int countCivPlayersAlive() const;
	int countCivPlayersEverAlive() const;
	int countCivTeamsAlive() const;
	int countCivTeamsEverAlive() const;

	int countKnownTechNumTeams(TechTypes eTech) const;

	int countReligionLevels(ReligionTypes eReligion) const;
	int calculateReligionPercent(ReligionTypes eReligion) const;
	int countCorporationLevels(CorporationTypes eCorporation) const;

	int goldenAgeLength() const;

	EraTypes getHighestEra() const;
	EraTypes getCurrentEra() const;

	int getActiveTeam() const;
	bool isNetworkMultiPlayer() const;
	bool isGameMultiPlayer() const;

	bool isModem() const;
	void setModem(bool bModem);

	int getGameTurn() const;
	void setGameTurn(int iNewValue);
	int getTurnYear(int iGameTurn) const;
	int getGameTurnYear() const;

	int getElapsedGameTurns() const;
	int getMaxTurns() const;
	void setMaxTurns(int iNewValue);
	int getMaxCityElimination() const;
	void setMaxCityElimination(int iNewValue);
	int getNumAdvancedStartPoints() const;
	int getStartTurn() const;
	int getStartYear() const;
	void setStartYear(int iNewValue);
	int getEstimateEndTurn() const;
	void setEstimateEndTurn(int iNewValue);
	int getMinutesPlayed() const;
	int getTargetScore() const;
	void setTargetScore(int iNewValue);

	int getNumCities() const;
	int getTotalPopulation() const;

	int getTradeRoutes() const;
	void changeTradeRoutes(int iChange);
	void changeNoNukesCount(int iChange);
	int getSecretaryGeneralTimer(int iVoteSource) const;
	int getVoteTimer(int iVoteSource) const;
	int getNukesExploded() const;
	void changeNukesExploded(int iChange);

	int getMaxPopulation() const;
	int getMaxLand() const;
	int getMaxTech() const;
	int getMaxWonders() const;
	int getInitPopulation() const;
	int getInitLand() const;
	int getInitTech() const;
	int getInitWonders() const;

	int getAIAutoPlay(int iPlayer) const;
	void setAIAutoPlay(int iPlayer, int iNewValue);

	bool isForcedAIAutoPlay(int iPlayer) const;
	void setForcedAIAutoPlay(int iPlayer, int iNewValue, bool bForced = false);


	int getCircumnavigatedTeam() const;
	void setCircumnavigatedTeam(int iTeamType);

	bool isDiploVote(VoteSourceTypes eVoteSource) const;
	bool isDebugMode() const;
	void toggleDebugMode();

	int getPitbossTurnTime() const;
	bool isHotSeat() const;
	bool isPbem() const;
	bool isPitboss() const;

	bool isFinalInitialized() const;
	void onFinalInitialized(const bool bNewGame);

	PlayerTypes getActivePlayer() const;
	void setActivePlayer(PlayerTypes eNewValue, bool bForceHotSeat);
	int getPausePlayer() const;
	bool isPaused() const;

	bool getStarshipLaunched(int ID) const;

	VictoryTypes getVictory() const;
	GameStateTypes getGameState() const;
	HandicapTypes getHandicapType() const;
	CalendarTypes getCalendar() const;
	EraTypes getStartEra() const;
	GameSpeedTypes getGameSpeedType() const;
	// The running game's PACE percents. They belong to the GAME rather than the gamespeed INFO because two of
	// them compose LIVE GAME STATE -- hammerCostPercent folds GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS --
	// and an info never reads game state ([engine.md] Consuming-system calcs). All three relay to the ONE calc
	// (CvGameSpeedScale), so a caller can never re-derive a different answer
	// (docs/architecture/patterns.md §DRY (single implementation)). Each returns a HUMAN percent, already unscaled.
	int getSpeedPercent() const;
	int getHammerCostPercent() const;
	PlayerTypes getRankPlayer(int iRank) const;
	int getPlayerRank(PlayerTypes iIndex) const;
	int getPlayerScore(PlayerTypes iIndex) const;
	TeamTypes getRankTeam(int iRank) const;
	int getTeamScore(TeamTypes iIndex) const;
	bool isOption(GameOptionTypes eIndex) const;
	void setOption(GameOptionTypes eIndex, bool bEnabled);
	bool isMPOption(MultiplayerOptionTypes eIndex) const;
	bool isForcedControl(ForceControlTypes eIndex) const;
	bool isBuildingMaxedOut(BuildingTypes eIndex, int iExtra) const;
	bool isUnitMaxedOut(UnitTypes eIndex, int iExtra) const;

	int getProjectCreatedCount(ProjectTypes eIndex) const;



	int getReligionGameTurnFounded(ReligionTypes eIndex) const;


	int getCorporationGameTurnFounded(CorporationTypes eIndex) const;
	bool isCorporationFounded(CorporationTypes eIndex) const;
	bool isVotePassed(VoteTypes eIndex) const;
	bool isVictoryValid(VictoryTypes eIndex) const;

	bool isInAdvancedStart() const;

	CyCity* getHolyCity(ReligionTypes eIndex) const;
	void setHolyCity(ReligionTypes eIndex, CyCity* pNewValue, bool bAnnounce);
	void clearHolyCity(ReligionTypes eIndex);

	CyCity* getHeadquarters(CorporationTypes eIndex) const;
	void setHeadquarters(CorporationTypes eIndex, CyCity* pNewValue, bool bAnnounce);
	void clearHeadquarters(CorporationTypes eIndex);


	std::string getScriptData() const;
	void setScriptData(std::string szNewValue);

	void setName(const char* szName);
	std::wstring getName() const;
	int getIndexAfterLastDeal() const;
	int getNumDeals() const;
	CyDeal* getDeal(int iID) const;
	void deleteDeal(int iID);
	CvRandom& getMapRand() const;
	CvRandom& getSorenRand() const;
	int getSorenRandNum(int iNum, const char* pszLog) const;
	bool GetWorldBuilderMode() const;				// remove once CvApp is exposed
	bool isPitbossHost() const;				// remove once CvApp is exposed
	int getCurrentLanguage() const;				// remove once CvApp is exposed
	void setCurrentLanguage(int iNewLanguage);				// remove once CvApp is exposed

	int getReplayMessageTurn(int i) const;
	ReplayMessageTypes getReplayMessageType(int i) const;
	int getReplayMessagePlotX(int i) const;
	int getReplayMessagePlotY(int i) const;
	int getReplayMessagePlayer(int i) const;
	ColorTypes getReplayMessageColor(int i) const;
	std::wstring getReplayMessageText(int i) const;
	uint getNumReplayMessages() const;
	CyReplayInfo* getReplayInfo() const;
	void saveReplay(PlayerTypes ePlayer);
	void addReplayMessage(ReplayMessageTypes eType, PlayerTypes ePlayer, std::wstring pszText, int iPlotX, int iPlotY, ColorTypes eColor);

	bool hasSkippedSaveChecksum() const;

	void addPlayer(PlayerTypes eNewPlayer, LeaderHeadTypes eLeader, CivilizationTypes eCiv, bool bSetAlive);
	void changeHumanPlayer(PlayerTypes eOldHuman, PlayerTypes eNewHuman);

	void log(const char* file, char* str);

	bool isLeaderEverActive(LeaderHeadTypes eLeader) const;

	void doControl(ControlTypes iControl);


	void saveGame(std::string fileName) const;



	int getModderGameOption(ModderGameOptionTypes eIndex) const;
	void setModderGameOption(ModderGameOptionTypes eIndex, int iNewValue);


	const char* getC2CVersion() const;

	void assignStartingPlots(bool bScenario, bool bMapScript);
	void exitWorldBuilder();

protected:
	CvGame& m_pGame;
};

#endif	// #ifndef CyGame
