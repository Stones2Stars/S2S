#pragma once

#ifndef CyGameTextMgr_h
#define CyGameTextMgr_h

//
// Python wrapper class for CyGameTextMgr
//

class CvGameTextMgr;
class CyCity;
class CyUnit;
class CyDeal;
struct TradeData;

class CyGameTextMgr
{
public:
	CyGameTextMgr();
	CyGameTextMgr(CvGameTextMgr* m_pGameTextMgr);			// Call from C++

	void Reset();

	// Publishes the TEXT boundary. Kept, not migrated: TXT is an unmigrated system boundary
	// (patterns.md § THE PYTHON READ BOUNDARY) and Python screen chrome calls it directly.
	static void pythonPublish();

	// The TXT KEYS a registry declares, handed over as ONE crossing for the whole column.
	//
	// ⚑ A key is TEXT-plane, which is why it is served here and not on the info surface: only the JSON-fed
	// infos carry that ([DEC-cy-not-fixed]), and the registries below are XML-era shells whose Description IS
	// the key. ⛔ Serving it at all is the load-bearing part -- the alternative is a consumer hand-listing the
	// keys, which defines the set in TWO places and has to be edited every time the registry gains a row.
	//
	// Addressed by infotype PREFIX so the surface grows by TABLE ROW rather than by method, the same rule the
	// rest of the read surface obeys. An unknown prefix answers an empty list rather than raising: a script may
	// legitimately probe a registry that carries no keys.
	python::list getTextKeys(const std::string& szTypePrefix) const;

	std::wstring getTimeStr(int iGameTurn, bool bSave);
	std::wstring getDateStr(int iGameTurn, bool bSave, int /*CalendarTypes*/ eCalendar, int iStartYear, int /*GameSpeedTypes*/ eSpeed);
	std::wstring getInterfaceTimeStr(int /*PlayerTypes*/ iPlayer);
	std::wstring getOOSSeeds(int /*PlayerTypes*/ iPlayer);
	std::wstring getNetStats(int /*PlayerTypes*/ iPlayer);
	std::wstring getTechHelp(int iTech, bool bCivilopediaText, bool bPlayerContext, bool bStrategyText, bool bTreeInfo, int iFromTech);
	std::wstring getUnitHelp(int iUnit, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, CyCity* pCity);
	std::wstring getSpecificUnitHelp(CyUnit* pUnit, bool bOneLine, bool bShort);
	std::wstring getBuildingHelp(int iBuilding, bool bActual, CyCity* pCity, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText);
	std::wstring getHeritageHelp(int iType, CyCity* pCity, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText);
	std::wstring getProjectHelp(int iProject, bool bCivilopediaText, CyCity* pCity);
	std::wstring getPromotionHelp(int iPromotion, bool bCivilopediaText);
	std::wstring getUnitCombatHelp(int iUnitCombat, bool bCivilopediaText);
	std::wstring getTraitHelp(int iTrait);
	std::wstring getBonusHelp(int iBonus, bool bCivilopediaText);
	std::wstring getProductionHelpCity(CyCity* pCity);
	std::wstring getReligionHelpCity(int iReligion, CyCity* pCity, bool bCityScreen, bool bForceReligion, bool bForceState, bool bNoStateReligion);
	std::wstring getCorporationHelpCity(int iCorporation, CyCity* pCity, bool bCityScreen, bool bForceCorporation);
	std::wstring getImprovementHelp(int iImprovement, bool bCivilopediaText);
	std::wstring getRouteHelp(int iRoute, bool bCivilopediaText);
	std::wstring getTerrainHelp(int iTerrain, bool bCivilopediaText);
	std::wstring getFeatureHelp(int iFeature, bool bCivilopediaText);
	std::wstring parseCivicInfo(int /*CivicTypes*/ iCivicType, bool bCivilopediaText, bool bPlayerContext, bool bSkipName);
	std::wstring parseReligionInfo(int /*ReligionTypes*/ iReligionType, bool bCivilopediaText);
	std::wstring parseCorporationInfo(int /*CorporationTypes*/ iCorporationType, bool bCivilopediaText);
	std::wstring parseCivInfos(int /*CivilizationTypes*/ iCivilization, bool bDawnOfMan);
	std::wstring parseLeaderTraits(int /*LeaderHeadTypes*/ iLeader, bool bDawnOfMan, bool bCivilopediaText);
	std::wstring parseTraits(int /*TraitTypes*/ eTrait, bool bDawnOfMan, bool bEffectsOnly);
	std::wstring getHappinessHelp();
	std::wstring getTradeString(TradeData* pTradeData, int iPlayer1, int iPlayer2);
	std::wstring getSpecialistHelp(int iSpecialist, bool bCivilopediaText);
	std::wstring buildHintsList();
	std::wstring getAttitudeString(int iPlayer, int iTargetPlayer);
	std::wstring setConvertHelp(int iPlayer, int iReligion);
	std::wstring setRevolutionHelp(int iPlayer);
	std::wstring setVassalRevoltHelp(int iMaster, int iVassal);
	std::wstring getActiveDealsString(int iThisPlayer, int iOtherPlayer);
	std::wstring getDealString(CyDeal* pDeal, int iPlayerPerspective);
	std::wstring getFinanceUnitUpkeepString(int iPlayer);
	std::wstring getDefenseHelp(CyCity* pCity);
	std::wstring getFlagHelp();

protected:
	CvGameTextMgr* m_pGameTextMgr;
};

#endif	// #ifndef CyGameTextMgr_h
