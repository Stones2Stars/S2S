#pragma once

// team.h

#ifndef CIV4_TEAM_H
#define CIV4_TEAM_H

#include "CvGameObject.h"
#include "CvProperties.h"
#include "CvDerivedData.h"
#include "CvCascadePackage.h"      // the TEAM-scope cascade package (state-repositories.md)
#include "Enabler/CvCapabilities.h"   // CascadeTeamCaps -- the derived-on-query capability union (capabilities.md)

class CvArea;

class CvTeam
	: private bst::noncopyable
{
public:
	CvTeam();
	virtual ~CvTeam();

	DllExport void init(TeamTypes eID);
	DllExport void reset(TeamTypes eID = NO_TEAM, bool bConstructorCall = false);

	CvGameObjectTeam* getGameObject() { return &m_GameObject; }

	//	Team-level derived-data repository (see CvDerivedData.h). Tech/war-shared facts; empty for now.
	CvTeamDataRepository&       dataRepository()       { return m_dataRepository; }
	const CvTeamDataRepository& dataRepository() const { return m_dataRepository; }

	// The TEAM-scope cascade package -- the uniform CvCascadePackage on every scoped item
	// ([DEC-uniform-cache-shape]; three channels measured at team scope -- trivial as keyed slots).
	// Marked ONLY by the modifier consumer's derived masks; recompute-only, never serialized.
	const CvCascadePackage<CvTeam>& getCascadePackage() const { return m_cascadePackage; }
	// The package's refresh delegate (the CvDerivedCacheSet contract) -- delegates to the ONE gather.
	void refreshCascadePackage(int64_t iMask) const;

	// ---- THE EMPIRE-CAPABILITY UNION (capabilities.md; json.md §8) -- the team's active capability set, derived
	// on query as the UNION over its live HAVE sources rather than granted or stored: nothing is handed out, so a
	// capability lapses with its last live source. It is CACHED because the queries are hot (isTerrainTrade rides
	// the pathing/trade-network loops), on the ONE CvDerivedCacheSet protocol -- setHasTech/reset MARK, and THE
	// MARK IS WHAT REBUILDS; a query is a BARE FETCH. Never serialized.
	// PUBLIC + MUTABLE by the same requirement as the enabler domains: CascadeCapabilities is the query surface
	// and owns the derivation; the team owns only the storage.
	mutable CascadeTeamCaps m_cascadeTeamCaps;
	// The union's refresh delegate (the CvDerivedCacheSet contract) -- delegates to the ONE derivation.
	void cascadeRefreshCaps(int iMask) const;

	// THE TEAM'S GROUP READ SURFACE -- the GAME-OBJECT read role's answer to "what do I HAVE, right now?"
	// (patterns.md § THE TWO READ ROLES), one getter per modifier FAMILY the TEAM scope carries channels of --
	// combat and diplomacy, which is the whole of it (CvInfoKinds.h's census scope masks; three channels
	// measured at team scope). The call carries NO channel argument and NO scope argument (the object IS the
	// scope); the group's kind enum indexes the RESULT; there is no scalar getter per channel. ×100 NATIVE.
	// Each folds through the ONE cross-scope roll-up, whose team entry is this package alone -- TEAM is the top
	// scope carrying a package (WORLD is CONFIG and has none). The team consumes no channel, so no receiver sum
	// exists here to prefer over the combine.
	void getCombatKinds(int (&combats)[NUM_COMBAT_KINDS]) const;
	void getDiplomacyKinds(int (&diplomacies)[NUM_DIPLOMACY_KINDS]) const;

protected:
	CvGameObjectTeam m_GameObject;
	CvTeamDataRepository m_dataRepository;
	// the TEAM-scope cascade package (see getCascadePackage); recompute-only, never serialized
	CvCascadePackage<CvTeam> m_cascadePackage;
	void uninit();

public:
	void resetPlotAndCityData();
	void addTeam(TeamTypes eTeam);
	void shareItems(TeamTypes eTeam);
	void shareCounters(TeamTypes eTeam);

	void processBuilding(BuildingTypes eBuilding, int iChange, bool bReligiouslyDisabling = false);

	void doTurn();

	void updateYield();

	bool canChangeWarPeace(TeamTypes eTeam, bool bAllowVassal = false) const;
	DllExport bool canDeclareWar(TeamTypes eTeam) const;

	bool canEventuallyDeclareWar(TeamTypes eTeam) const;
	void declareWar(TeamTypes eTeam, bool bNewDiplo, WarPlanTypes eWarPlan);

	void makePeace(TeamTypes eTeam, bool bBumpUnits = true);
	bool canContact(TeamTypes eTeam) const;
	void meet(TeamTypes eTeam, bool bNewDiplo);
	void signOpenBorders(TeamTypes eTeam);
	void signDefensivePact(TeamTypes eTeam);
	bool canSignDefensivePact(TeamTypes eTeam) const;

	int getAssets() const;
	int getPower(bool bIncludeVassals) const;
	int getDefensivePower(TeamTypes eExcludeTeam = NO_TEAM) const;
	int getEnemyPower() const;
	int getNumNukeUnits() const;

	bool isUnitBonusEnabledByTech(const CvUnitInfo& unit, const bool bNoWorldBonuses=false) const;

	int64_t getTotalVictoryScore() const;

	int getVotes(VoteTypes eVote, VoteSourceTypes eVoteSource) const;
	bool isVotingMember(VoteSourceTypes eVoteSource) const;
	bool isFullMember(VoteSourceTypes eVoteSource) const;

	int getCurrentMasterPower(bool bIncludeVassals) const;
	bool isMasterPlanningLandWar(const CvArea* pArea) const;
	bool isMasterPlanningSeaWar(const CvArea* pArea) const;
	int getAtWarCount(bool bIgnoreMinors, bool bIgnoreVassals = false) const;

	int getWarPlanCount(WarPlanTypes eWarPlan, bool bIgnoreMinors) const;
	int getAnyWarPlanCount(bool bIgnoreMinors) const;
	bool hasWarPlan(bool bIgnoreMinors) const;
	int getChosenWarCount(bool bIgnoreMinors) const;
	int getHasMetCivCount(bool bIgnoreMinors) const;
	bool hasMetAnyCiv(bool bIgnoreMinors = true) const;
	int getDefensivePactCount(TeamTypes eTeam = NO_TEAM) const;
	int getVassalCount(TeamTypes eTeam = NO_TEAM) const;
	bool isAVassal() const;
	bool canVassalRevolt(TeamTypes eMaster) const;

	int getBuildingMaking(BuildingTypes eBuilding) const;
	int getBuildingCountPlusMaking(BuildingTypes eIndex) const;
	int getUnitMaking(UnitTypes eIndex) const;
	int getUnitCountPlusMaking(UnitTypes eIndex) const;
	int getHasReligionCount(ReligionTypes eReligion) const;
	int getHasCorporationCount(CorporationTypes eCorporation) const;

	uint64_t countTotalCulture() const;

	int countNumUnitsByArea(const CvArea* pArea) const;
	int countNumCitiesByArea(const CvArea* pArea) const;
	int countTotalPopulationByArea(const CvArea* pArea) const;
	int countPowerByArea(const CvArea* pArea) const;
	int countEnemyPowerByArea(const CvArea* pArea) const;

	int countEnemyPopulationByArea(const CvArea* pArea) const;
	int countNumAIUnitsByArea(const CvArea* pArea, UnitAITypes eUnitAI) const;
	int countEnemyDangerByArea(const CvArea* pArea, TeamTypes eEnemyTeam = NO_TEAM) const;

	int getTypicalUnitValue(UnitAITypes eUnitAI) const;

	int getResearchCost(TechTypes eTech) const;
	int getResearchLeft(TechTypes eTech) const;

	bool hasHolyCity(ReligionTypes eReligion) const;
	bool hasHeadquarters(CorporationTypes eCorporation) const;
	bool hasBonus(BonusTypes eBonus) const;
	bool isBonusObsolete(BonusTypes eBonus) const;

	bool isHuman(const bool bCountDisabledHuman = false) const;
	bool isNPC() const;
	bool isHominid() const;
	bool isMinorCiv() const;
	bool isSingleCityTeam() const;
	bool isRebel() const;
	bool isRebelAgainst(TeamTypes eTeam) const;
	void setRebelAgainst(TeamTypes eTeam, bool bNewValue);
	int countRebelAgainst() const;
	void setIsMinorCiv(bool bNewValue);
	void declareWarAsMinor();

	int getNumMilitaryUnits() const;

	PlayerTypes getLeaderID() const;
	PlayerTypes getSecretaryID() const;
	HandicapTypes getHandicapType() const;
	CvWString getName() const;

	DllExport int getNumMembers() const;
	void changeNumMembers(int iChange);

	int getAliveCount() const;
	bool isAlive() const;
	void changeAliveCount(int iChange);

	int isEverAlive() const;
	void changeEverAliveCount(int iChange);

	int getNumCities() const;
	void changeNumCities(int iChange);

	int getTotalPopulation(bool bCheckVassals = true) const;
	void changeTotalPopulation(int iChange);

	int getTotalLand(bool bCheckVassals = true) const;
	void changeTotalLand(int iChange);


	int getForceTeamVoteEligibilityCount(VoteSourceTypes eVoteSource) const;
	bool isForceTeamVoteEligible(VoteSourceTypes eVoteSource) const;
	void changeForceTeamVoteEligibilityCount(VoteSourceTypes eVoteSource, int iChange);

	bool isExtraWaterSeeFrom() const;

	bool isMapTrading() const;

	bool isCanPassPeaks() const;

	bool isMoveFastPeaks() const;

	bool isCanFoundOnPeaks() const;

	bool isRebaseAnywhere() const;






	int getTechExtraBuildingHappiness(BuildingTypes eIndex) const;

	void ObsoletePromotions(TechTypes eObsoleteTech);
	void ObsoleteCorporations(TechTypes eObsoleteTech);

	bool isEmbassyTrading() const;

	bool isHasEmbassy(TeamTypes eIndex) const;
	void setHasEmbassy(TeamTypes eIndex, bool bNewValue);
	int getBuildingCommerceChange(BuildingTypes eIndex1, CommerceTypes eIndex2) const;

	int getBuildingSpecialistChange(BuildingTypes eIndex1, SpecialistTypes eIndex2) const;
	void changeBuildingSpecialistChange(BuildingTypes eIndex1, SpecialistTypes eIndex2, int iChange);

	bool isLimitedBordersTrading() const;

	bool isCanFarmDesert() const;

	void signLimitedBorders(TeamTypes eTeam);
	bool canSignOpenBorders(TeamTypes eTeam) const;
	void sendAmbassador(TeamTypes eTeam);


	bool isAnyVassal() const;
	ImprovementTypes getImprovementUpgrade(ImprovementTypes eImprovement) const;
	ImprovementTypes finalImprovementUpgrade(ImprovementTypes eImprovement) const;

	bool isFreeTradeAgreement(TeamTypes eIndex) const;
	void setFreeTradeAgreement(TeamTypes eIndex, bool bNewValue);

	bool isLimitedBorders(TeamTypes eIndex) const;
	void setLimitedBorders(TeamTypes eIndex, bool bNewValue);

	void AI_updateBonusValue(BonusTypes eBonus);

	void AI_setAssignWorkDirtyInEveryPlayerCityWithActiveBuilding(BuildingTypes eBuilding);

	bool isTechTrading() const;

	bool isGoldTrading() const;

	bool isOpenBordersTrading() const;

	bool isDefensivePactTrading() const;

	bool isPermanentAllianceTrading() const;

	bool isVassalStateTrading() const;

	bool isBridgeBuilding() const;

	bool isIrrigation() const;

	bool isIgnoreIrrigation() const;

	bool isWaterWork() const;

	int getVassalPower() const;
	void setVassalPower(int iPower);
	int getMasterPower() const;
	void setMasterPower(int iPower);

	int getEnemyWarWearinessModifier() const;
	void changeEnemyWarWearinessModifier(int iChange);
	void changeWarWeariness(TeamTypes eOtherTeam, const CvPlot& kPlot, int iFactor);
	void changeWarWearinessTimes100(TeamTypes eOtherTeam, const CvPlot& kPlot, int iFactor);

	bool isMapCentering() const;
	void setMapCentering(bool bNewValue);

	TeamTypes getID() const;

	int getStolenVisibilityTimer(TeamTypes eIndex) const;
	bool isStolenVisibility(TeamTypes eIndex) const;
	void setStolenVisibilityTimer(TeamTypes eIndex, int iNewValue);
	void changeStolenVisibilityTimer(TeamTypes eIndex, int iChange);

	int getWarWeariness(TeamTypes eIndex) const;
	int getWarWearinessTimes100(TeamTypes eIndex) const;
	void setWarWeariness(TeamTypes eIndex, int iNewValue);
	void setWarWearinessTimes100(TeamTypes eIndex, int iNewValue);
	void changeWarWeariness(TeamTypes eIndex, int iChange);
	void changeWarWearinessTimes100(TeamTypes eIndex, int iChange);


	bool isCommerceFlexible(CommerceTypes eIndex) const;

	int getExtraMoves(DomainTypes eIndex) const;
	void changeExtraMoves(DomainTypes eIndex, int iChange);

	bool isHasMet(TeamTypes eIndex) const;
	void setHasMet(TeamTypes eIndex, bool bNewValue);
	void makeHasMet(TeamTypes eIndex, bool bNewDiplo);

	bool isAtWar(const bool bCountMinors = false) const;
	DllExport bool isAtWar(TeamTypes eIndex) const;
	void setAtWar(TeamTypes eIndex, bool bNewValue);

	bool isPermanentWarPeace(TeamTypes eIndex) const;
	void setPermanentWarPeace(TeamTypes eIndex, bool bNewValue);

	bool isFreeTrade(TeamTypes eIndex) const;
	bool isOpenBorders(TeamTypes eIndex) const;
	void setOpenBorders(TeamTypes eIndex, bool bNewValue);

	bool isDefensivePact(TeamTypes eIndex) const;
	void setDefensivePact(TeamTypes eIndex, bool bNewValue);

	bool isForcePeace(TeamTypes eIndex) const;
	void setForcePeace(TeamTypes eIndex, bool bNewValue);

	bool isVassal(TeamTypes eIndex) const;
	void setVassal(TeamTypes eIndex, bool bNewValue, bool bCapitulated);

	void assignVassal(TeamTypes eVassal, bool bSurrender) const;
	void freeVassal(TeamTypes eVassal) const;

	bool isCapitulated() const;

	int getRouteChange(RouteTypes eIndex) const;
	void changeRouteChange(RouteTypes eIndex, int iChange);

	int getProjectCount(ProjectTypes eIndex) const;
	DllExport int getProjectDefaultArtType(ProjectTypes eIndex) const;
	DllExport void setProjectDefaultArtType(ProjectTypes eIndex, int value);
	DllExport int getProjectArtType(ProjectTypes eIndex, int number) const;
	DllExport void setProjectArtType(ProjectTypes eIndex, int number, int value);
	bool isProjectMaxedOut(ProjectTypes eIndex, int iExtra = 0) const;
	DllExport bool isProjectAndArtMaxedOut(ProjectTypes eIndex) const;
	void changeProjectCount(ProjectTypes eIndex, int iChange);
	void processProjectChange(ProjectTypes eIndex, int iChange, int iOldProjectCount);
	DllExport void finalizeProjectArtTypes();

	int getProjectMaking(ProjectTypes eIndex) const;
	void changeProjectMaking(ProjectTypes eIndex, int iChange);

	int getUnitCount(const UnitTypes eIndex) const;
	void changeUnitCount(const UnitTypes eIndex, const int iChange);

	int getBuildingCount(BuildingTypes eIndex) const;
	bool isBuildingMaxedOut(BuildingTypes eIndex, int iExtra = 0) const;
	void changeBuildingCount(BuildingTypes eIndex, int iChange);

	bool isObsoleteBuilding(BuildingTypes eIndex) const;

	int getResearchProgress(TechTypes eIndex) const;
	void setResearchProgress(TechTypes eIndex, int iNewValue, PlayerTypes ePlayer);
	void changeResearchProgress(TechTypes eIndex, int iChange, PlayerTypes ePlayer);
	int changeResearchProgressPercent(TechTypes eIndex, int iPercent, PlayerTypes ePlayer);

	int getTechCount(TechTypes eIndex) const;
	int getBestKnownTechScorePercent() const;

	bool isTerrainTrade(TerrainTypes eIndex) const;

	bool isRiverTrade() const;

	bool isHasTech(TechTypes eIndex) const;
	void setHasTech(TechTypes eIndex, bool bNewValue, PlayerTypes ePlayer, bool bFirst, bool bAnnounce);
	bool isInvisibleSeerUnlocked(InvisibleTypes eInvisible) const;

	bool isNoTradeTech(short iTech) const;
	void setNoTradeTech(short iTech, bool bNewValue);


	void addPropertiesAllCities(const CvProperties* pProp);
	void subtractPropertiesAllCities(const CvProperties* pProp);

	bool isFriendlyTerritory(TeamTypes eTeam) const;

	int getVictoryCountdown(VictoryTypes eIndex) const;
	void setVictoryCountdown(VictoryTypes eIndex, int iTurnsLeft);
	void changeVictoryCountdown(VictoryTypes eIndex, int iChange);
	int getVictoryDelay(VictoryTypes eVictory) const;
	bool canLaunch(VictoryTypes eVictory) const;
	void setCanLaunch(VictoryTypes eVictory, bool bCan);
	int getLaunchSuccessRate(VictoryTypes eVictory) const;
	void resetVictoryProgress();

	bool isParent(TeamTypes eTeam) const;

	int getEspionagePointsAgainstTeam(TeamTypes eIndex) const;
	void setEspionagePointsAgainstTeam(TeamTypes eIndex, int iValue);
	void changeEspionagePointsAgainstTeam(TeamTypes eIndex, int iChange);

	int getEspionagePointsEver() const;
	void setEspionagePointsEver(int iValue);
	void changeEspionagePointsEver(int iChange);

	int getCounterespionageTurnsLeftAgainstTeam(TeamTypes eIndex) const;
	void setCounterespionageTurnsLeftAgainstTeam(TeamTypes eIndex, int iValue);
	void changeCounterespionageTurnsLeftAgainstTeam(TeamTypes eIndex, int iChange);

	int getCounterespionageModAgainstTeam(TeamTypes eIndex) const;
	void setCounterespionageModAgainstTeam(TeamTypes eIndex, int iValue);
	void changeCounterespionageModAgainstTeam(TeamTypes eIndex, int iChange);

	void verifySpyUnitsValidPlot();

	void setCircumnavigated(bool bNewValue);

	void setForceRevealedBonus(BonusTypes eBonus, bool bRevealed);
	bool isForceRevealedBonus(BonusTypes eBonus) const;

	int countNumHumanGameTurnActive() const;
	void setTurnActive(bool bNewValue, bool bTurn = true);
	bool isTurnActive() const;

	bool hasShrine(ReligionTypes eReligion) const;

	DllExport void getCompletedSpaceshipProjects(std::map<ProjectTypes, int>& mapProjects) const;
	DllExport int getProjectPartNumber(ProjectTypes projectType, bool bAssert) const;
	DllExport bool hasLaunched() const;

	virtual void AI_init() = 0;
	virtual void AI_reset(bool bConstructor) = 0;
	virtual void AI_doTurnPre() = 0;
	virtual void AI_doTurnPost() = 0;
	virtual void AI_makeAssignWorkDirty() = 0;
	virtual void AI_updateAreaStragies(const bool bTargets = true) = 0;
	virtual bool AI_shareWar(TeamTypes eTeam) const = 0;
	virtual void AI_updateWorstEnemy() = 0;
	virtual int AI_getAtWarCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setAtWarCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getAtPeaceCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setAtPeaceCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getHasMetCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setHasMetCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getOpenBordersCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setOpenBordersCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getDefensivePactCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setDefensivePactCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getShareWarCounter(TeamTypes eIndex) const = 0;
	virtual void AI_setShareWarCounter(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getWarSuccess(TeamTypes eIndex) const = 0;
	virtual void AI_setWarSuccess(TeamTypes eIndex, int iNewValue) = 0;
	virtual void AI_changeWarSuccess(TeamTypes eIndex, int iChange) = 0;
	virtual int AI_getEnemyPeacetimeTradeValue(TeamTypes eIndex) const = 0;
	virtual void AI_setEnemyPeacetimeTradeValue(TeamTypes eIndex, int iNewValue) = 0;
	virtual int AI_getEnemyPeacetimeGrantValue(TeamTypes eIndex) const = 0;
	virtual void AI_setEnemyPeacetimeGrantValue(TeamTypes eIndex, int iNewValue) = 0;
	virtual WarPlanTypes AI_getWarPlan(TeamTypes eIndex) const = 0;
	virtual bool AI_isChosenWar(TeamTypes eIndex) const = 0;
	virtual bool AI_isSneakAttackPreparing(TeamTypes eIndex) const = 0;
	virtual bool AI_isSneakAttackReady(TeamTypes eIndex) const = 0;
	virtual void AI_setWarPlan(TeamTypes eIndex, WarPlanTypes eNewValue, bool bWar = true, bool bInFull = true) = 0;


	int getLastRoundOfValidImprovementCacheUpdate() const;
	void setLastRoundOfValidImprovementCacheUpdate();
	int getWinForLosingResearchModifier() const;

protected:
	TeamTypes m_eID;

	bool m_bMapCentering;
	bool m_bCapitulated;

	int m_iNumMembers;
	int m_iAliveCount;
	int m_iEverAliveCount;
	int m_iNumCities;
	int m_iTotalPopulation;
	int m_iTotalLand;
	int m_iLastRoundOfValidImprovementCacheUpdate;
	int m_iVassalPower;
	int m_iMasterPower;
	int m_iEnemyWarWearinessModifier;
	int m_iEspionagePointsEver;

	bool* m_abEmbassy;
	bool* m_abLimitedBorders;
	bool* m_abFreeTrade;
	bool* m_abAtWar;
	bool* m_abHasMet;
	bool* m_abPermanentWarPeace;
	bool* m_abOpenBorders;
	bool* m_abDefensivePact;
	bool* m_abForcePeace;
	bool* m_abVassal;
	bool* m_abCanLaunch;
	bool* m_abIsRebelAgainst;
	bool* m_pabHasTech;

	int** m_ppiBuildingSpecialistChange;
	int** m_ppiBuildingCommerceModifier;
	int** m_ppiBuildingYieldModifier;

	int* m_aiStolenVisibilityTimer;
	int* m_aiWarWearinessTimes100;
	int* m_aiExtraMoves;
	int* m_aiForceTeamVoteEligibilityCount;
	int* m_paiRouteChange;
	int* m_paiProjectCount;
	int* m_paiProjectDefaultArtTypes;
	int* m_paiProjectMaking;
	int* m_paiBuildingCount;
	int* m_paiResearchProgress;
	int* m_paiTechCount;
	int* m_aiVictoryCountdown;
	int* m_aiEspionagePointsAgainstTeam;
	int* m_aiCounterespionageTurnsLeftAgainstTeam;
	int* m_aiCounterespionageModAgainstTeam;

	std::vector<int> *m_pavProjectArtTypes;
	std::vector<short> m_vNoTradeTech;
	std::vector<BonusTypes> m_aeRevealedBonuses;

	std::map<short, uint32_t> m_unitCount;

	void doWarWeariness();
	void testCircumnavigated();
	void processTech(TechTypes eTech, int iChange, bool bAnnounce = false);
	void cancelDefensivePacts();
	void announceTechToPlayers(TechTypes eIndex, bool bPartial = false);


	virtual void read(FDataStreamBase* pStream);
	virtual void write(FDataStreamBase* pStream);

// AIAndy: Properties
protected:
	CvProperties m_Properties;
public:
	CvProperties* getProperties();
	const CvProperties* getPropertiesConst() const;

// Toffer - Cache current research choices.
private:
public:
};

#endif
