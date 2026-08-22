#pragma once

#ifndef CyGlobalContext_h
#define CyGlobalContext_h

//
//	CyGlobalContext -- the CONFIG / REGISTRY / DEFINES half of the old global context, and NOTHING ELSE.
//
//	⚖ REINTRODUCED DELIBERATELY (owner), minus the infos. The purge treated this class as one thing; it was
//	two. What it hands out here -- the registry COUNTS, the global DEFINES, the engine CONSTANTS, the BUG
//	bridge, name->id resolution -- is CONFIGURATION, not entity data, and a great deal of Python legitimately
//	needs it. Removing that half bought nothing and broke everything that reads a setting.
//
//	⛔ WHAT IT NO LONGER DOES, AND MUST NEVER DO AGAIN: hand out INFOS. The per-entity `get<X>Info(i)`
//	accessors and the `Cy*` object HANDLES are gone and stay gone -- that is the read surface the library
//	replaces (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract)), and it is precisely the half whose return values let a script reach the
//	whole legacy getter set. A script wanting entity data asks the library; a script wanting a SETTING asks
//	here. The split is the point: the escape hatch closes, the config stays.
//
//	⚑ The DEFINES are served BY NAME (getDefineINT/FLOAT) rather than as a getter per value. That is what makes
//	this extensible by DATA: a new define is a new XML row, never a new method.
//

#include "Defines/CvGlobals.h"

class CyGame;
class CyMap;
class CyPlayer;
class CvRandom;
class CyTeam;

class CyGlobalContext
{
public:
	static CyGlobalContext& getInstance();		// singleton accessor
	static void initStatics();

	// Publishes the config half. Called from DLLPublishToPython.
	static void pythonPublish();


	// The Cy* HANDLES. These are NOT infos -- they are the game-object wrappers the engine already hands to
	// Python callbacks, and the ruling that this class serves no INFOS does not reach them. Cutting them was
	// an over-reach: they are the most-used names in the tree.

	int getNumCivilizationInfos() const { return GC.getNumCivilizationInfos(); }   // the correctly-spelled twin

	// PLAYER + GRAPHIC OPTIONS are SETTINGS, so they are served here rather than by CyInfo, which answers
	// ENTITY data (CyInfo.h states the split). The options screen needs a label and a hover text per option and
	// is handed those two strings; it is NOT handed the info object, which is the legacy escape hatch the
	// rebuild closed. Their infos are XML-loaded and have no InfoRepo home, so the prefix plane cannot route
	// them at all.
	std::wstring getPlayerOptionDescription(int iOption) const;
	std::wstring getPlayerOptionHelp(int iOption) const;
	std::wstring getGraphicOptionDescription(int iOption) const;
	std::wstring getGraphicOptionHelp(int iOption) const;

	// The Cy* HANDLES. NOT infos -- they are the game-object wrappers the engine already hands to Python
	// callbacks, so the ruling that this class serves no INFOS does not reach them. Cutting them was an
	// over-reach: getPlayer/getTeam/getMap/getGame are the most-called names in the whole tree.
	CyGame* getCyGame() const;
	CyMap* getCyMap() const;
	CyMap* getMapByIndex(MapTypes eMap) const;
	CyPlayer* getCyPlayer(PlayerTypes ePlayer) const;
	CyPlayer* getCyActivePlayer() const;
	CvRandom& getCyASyncRand() const;
	CyTeam* getCyTeam(TeamTypes eTeam) const;
	const CvMainMenuInfo* getMainMenus(int i) const;

	int getInfoTypeForString(const char* szInfoType, bool bHideAssert = false) const;



	const char* getArtStyleTypes(int i) const { return GC.getArtStyleTypes((ArtStyleTypes) i); }

	int getMapBonus(int i) const { return GC.getMapBonus(i); }
	int getNumMapBonuses() const { return GC.getNumMapBonuses(); }

	int getNumEffectInfos() const { return GC.getNumEffectInfos(); }
	int getNumTerrainInfos() const { return GC.getNumTerrainInfos(); }
	int getNumBonusInfos() const { return GC.getNumBonusInfos(); }
	int getNumPlayableCivilizationInfos() const { return GC.getNumPlayableCivilizationInfos(); }
	int getNumCivilizatonInfos() const { return GC.getNumCivilizationInfos(); }
	int getNumLeaderHeadInfos() const { return GC.getNumLeaderHeadInfos(); }
	int getNumTraitInfos() const { return GC.getNumTraitInfos(); }
	int getNumUnitInfos() const { return GC.getNumUnitInfos(); }
	int getNumRouteInfos() const { return GC.getNumRouteInfos(); }
	int getNumFeatureInfos() const { return GC.getNumFeatureInfos(); }
	int getNumImprovementInfos() const { return GC.getNumImprovementInfos(); }
	int getNumBuildInfos() const { return GC.getNumBuildInfos(); }
	int getNumHandicapInfos() const { return GC.getNumHandicapInfos(); }
	int getNumGameSpeedInfos() const { return GC.getNumGameSpeedInfos(); }
	int getNumBuildingInfos() const { return GC.getNumBuildingInfos(); }
	int getNumUnitCombatInfos() const { return GC.getNumUnitCombatInfos(); }
	int getNumMissionInfos() const { return GC.getNumMissionInfos(); }
	int getNumPromotionInfos() const { return GC.getNumPromotionInfos(); }
	int getNumTechInfos() const { return GC.getNumTechInfos(); }
	int getNumReligionInfos() const { return GC.getNumReligionInfos(); }
	int getNumCorporationInfos() const { return GC.getNumCorporationInfos(); }
	int getNumSpecialistInfos() const { return GC.getNumSpecialistInfos(); }
	int getNumCivicInfos() const { return GC.getNumCivicInfos(); }
	int getNumCivicOptionInfos() const { return GC.getNumCivicOptionInfos(); }
	int getNumProjectInfos() const { return GC.getNumProjectInfos(); }
	int getNumVoteInfos() const { return GC.getNumVoteInfos(); }
	int getNumProcessInfos() const { return GC.getNumProcessInfos(); }
	int getNumEmphasizeInfos() const { return GC.getNumEmphasizeInfos(); }
	int getNumHurryInfos() const { return GC.getNumHurryInfos(); }
	int getNumCultureLevelInfos() const { return GC.getNumCultureLevelInfos(); }
	int getNumEraInfos() const { return GC.getNumEraInfos(); }
	int getNumVictoryInfos() const { return GC.getNumVictoryInfos(); }
	int getNumWorldInfos() const { return GC.getNumWorldInfos(); }
	int getNumGameOptionInfos() const { return GC.getNumGameOptionInfos(); }
	int getNumMPOptionInfos() const { return GC.getNumMPOptionInfos(); }
	int getNumForceControlInfos() const { return GC.getNumForceControlInfos(); }
	int getNumEventTriggerInfos() const { return GC.getNumEventTriggerInfos(); }
	int getNumEventInfos() const { return GC.getNumEventInfos(); }
	int getNumEspionageMissionInfos() const { return GC.getNumEspionageMissionInfos(); }
	int getNumMainMenus() const { return GC.getNumMainMenus(); }
	int getNumVoteSourceInfos() const { return GC.getNumVoteSourceInfos(); }
	int getNumPropertyInfos() const { return GC.getNumPropertyInfos(); }

	//////////////////////
	// Globals Defines
	//////////////////////

	int getDefineINT( const char * szName ) const { return GC.getDefineINT( szName ); }

	void setDefineINT( const char * szName, int iValue ) { return GC.setDefineINT( szName, iValue ); }

	bool isDCM_ACTIVE_DEFENSE() const { return GC.isDCM_ACTIVE_DEFENSE(); }
	bool isDCM_FIGHTER_ENGAGE() const { return GC.isDCM_FIGHTER_ENGAGE(); }

	bool isIDW_ENABLED() const { return GC.isIDW_ENABLED(); }
	bool isIDW_EMERGENCY_DRAFT_ENABLED() const { return GC.isIDW_EMERGENCY_DRAFT_ENABLED(); }
	bool isIDW_NO_BARBARIAN_INFLUENCE() const { return GC.isIDW_NO_BARBARIAN_INFLUENCE(); }
	bool isIDW_NO_NAVAL_INFLUENCE() const { return GC.isIDW_NO_NAVAL_INFLUENCE(); }
	bool isIDW_PILLAGE_INFLUENCE_ENABLED() const { return GC.isIDW_PILLAGE_INFLUENCE_ENABLED(); }

	bool isSS_ENABLED() const { return GC.isSS_ENABLED(); }
	bool isSS_BRIBE() const { return GC.isSS_BRIBE(); }
	bool isSS_ASSASSINATE() const { return GC.isSS_ASSASSINATE(); }

	int getMAX_PC_PLAYERS() const { return MAX_PC_PLAYERS; }
	int getMAX_PLAYERS() const { return MAX_PLAYERS; }
	int getMAX_PC_TEAMS() const { return MAX_PC_TEAMS; }
	int getMAX_TEAMS() const { return MAX_TEAMS; }
	int getBARBARIAN_PLAYER() const { return BARBARIAN_PLAYER; }
	int getBARBARIAN_TEAM() const { return BARBARIAN_TEAM; }

	int getNUM_CITY_PLOTS() const { return NUM_CITY_PLOTS; }

	void setIsBug() { GC.setIsBug(); }
	void refreshOptionsBUG() { GC.refreshOptionsBUG(); }

	void setNoUpdateDefineFLOAT( const char * szName, float fValue ) { return GC.setDefineFLOAT( szName, fValue, false ); }

#define DECLARE_CY_GET_METHOD(dataType, VAR) \
	int get##VAR() const { return (int)GC.get##VAR(); }

	DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE(DECLARE_CY_GET_METHOD)
	DO_FOR_EACH_EXPOSED_INFO_TYPE(DECLARE_CY_GET_METHOD)
};

#endif	// CyGlobalContext_h
