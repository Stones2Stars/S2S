//
// Python wrapper class for CyGameTextMgr
//
#include "CvGameCoreDLL.h"
#include "UI/CvGameTextMgr.h"
#include "CyCity.h"
#include "CyDeal.h"
#include "CyGameTextMgr.h"
#include "CyUnit.h"
#include "Infos/CvInfoBase.h"   // getTextKeyWide -- the MEMORY_ registry is bare CvInfoBase shells
// The symbol-bearing registries, included SPECIFICALLY rather than through the CvInfos.h umbrella
// (AGENTS.md Conventions) -- these carry the glyph the symbol pass assigns via setChar.
#include "Infos/CvYieldInfo.h"
#include "Infos/CvCommerceInfo.h"
#include "Infos/CvReligionInfo.h"
#include "Infos/CvCorporationInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Engine/CvCity.h"
#include "Engine/CvUnit.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"      // GET_PLAYER

extern const char* g_szLastCyRead;

namespace
{
	//	A composer names its subject by the (owner, id) PAIR, like every other surface script reaches. The
	//	CyCity* / CyUnit* these used to take are unreachable from script -- both wrappers carry zero defs, so
	//	nothing can hold or build one (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract)) -- which made every city-context tooltip and the
	//	unit help uncallable.
	//	⚠ A NULL answer is legitimate and always was: the pedia asks these with no city bound, and the composers
	//	below already branch on it. So an unresolvable pair reads as "no context", never as an error.
	CvUnit* cgt_unit(int iPlayer, int iUnit)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS) return NULL;
		return GET_PLAYER((PlayerTypes)iPlayer).getUnit(iUnit);
	}

	CvCity* cgt_city(int iPlayer, int iCity)
	{
		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS || iCity < 0)
		{
			return NULL;
		}
		return GET_PLAYER((PlayerTypes)iPlayer).getCity(iCity);
	}
}

CyGameTextMgr::CyGameTextMgr() :
m_pGameTextMgr(NULL)
{
	m_pGameTextMgr = &CvGameTextMgr::GetInstance();
}

CyGameTextMgr::CyGameTextMgr(CvGameTextMgr* pGameTextMgr) : m_pGameTextMgr(pGameTextMgr)
{

}

int CyGameTextMgr::getSymbolChar(const std::string& szTypePrefix, int iId) const
{
	if (iId < 0) return 0;

	// The five VARIABLE-COUNT-or-fixed registries addressed BY ID. An explicit table rather than a generic info
	// read, because the glyph is NOT on the info surface: these straddle the JSON/XML line and several are not
	// CvInfo-derived at all. The set is the spec's, so it does not grow by accident.
	// ⚠ PROPERTY_ and INVISIBLE_ are deliberately ABSENT and are NOT a gap to close here: the symbol pass builds
	// them a per-entity [ICON_<TYPE>] token instead (CvDllTranslator::initializeTags), so a caller resolves those
	// two through the translator. ⛔ A prefix this table does not serve returns 0, and a "%c" of 0 embeds a NUL
	// that truncates whatever string it is concatenated into -- so an unserved prefix fails as MISSING TEXT
	// rather than as a missing icon.
	if (szTypePrefix == "YIELD_")
	{
		if (iId < NUM_YIELD_TYPES) return GC.getYieldInfo((YieldTypes)iId).getChar();
	}
	else if (szTypePrefix == "COMMERCE_")
	{
		if (iId < NUM_COMMERCE_TYPES) return GC.getCommerceInfo((CommerceTypes)iId).getChar();
	}
	else if (szTypePrefix == "RELIGION_")
	{
		if (iId < GC.getNumReligionInfos()) return GC.getReligionInfo((ReligionTypes)iId).getChar();
	}
	else if (szTypePrefix == "CORPORATION_")
	{
		if (iId < GC.getNumCorporationInfos()) return GC.getCorporationInfo((CorporationTypes)iId).getChar();
	}
	else if (szTypePrefix == "BONUS_")
	{
		if (iId < GC.getNumBonusInfos()) return GC.getBonusInfo((BonusTypes)iId).getChar();
	}
	return 0;
}

int CyGameTextMgr::getHolyCitySymbolChar(int iReligion) const
{
	if (iReligion < 0 || iReligion >= GC.getNumReligionInfos()) return 0;
	return GC.getReligionInfo((ReligionTypes)iReligion).getHolyCityChar();
}

int CyGameTextMgr::getHeadquarterSymbolChar(int iCorporation) const
{
	if (iCorporation < 0 || iCorporation >= GC.getNumCorporationInfos()) return 0;
	return GC.getCorporationInfo((CorporationTypes)iCorporation).getHeadquarterChar();
}

python::list CyGameTextMgr::getTextKeys(const std::string& szTypePrefix) const
{
	python::list keys;

	// MEMORY_ -- the diplomacy grievance keys. The registry is a bare CvInfoBase shell (Type + Description, the
	// Description being the key), so there is nothing here for the info surface to serve; the count is the enum
	// bound because the types are enumerated in the SDK, which is what the XML itself warns about.
	if (szTypePrefix == "MEMORY_")
	{
		for (int iMemory = 0; iMemory < NUM_MEMORY_TYPES; ++iMemory)
		{
			keys.append(std::wstring(GC.getMemoryInfo((MemoryTypes)iMemory).getTextKeyWide()));
		}
	}

	return keys;
}

void CyGameTextMgr::Reset()
{
	GAMETEXT.Reset();
}

std::wstring CyGameTextMgr::getTimeStr(int iGameTurn, bool bSave)
{
	CvWString str;
	GAMETEXT.setTimeStr(str, iGameTurn, bSave);
	return str;
}

std::wstring CyGameTextMgr::getDateStr(int iGameTurn, bool bSave, int /*CalendarTypes*/ eCalendar, int iStartYear, int /*GameSpeedTypes*/ eSpeed)
{
	CvWString str;
	GAMETEXT.setDateStr(str, iGameTurn, bSave, (CalendarTypes)eCalendar, iStartYear, (GameSpeedTypes)eSpeed);
	return str;
}

std::wstring CyGameTextMgr::getOOSSeeds(int /*PlayerTypes*/ iPlayer)
{
	CvWString szBuffer;
	GAMETEXT.setOOSSeeds(szBuffer, ((PlayerTypes)iPlayer));
	return szBuffer;
}

std::wstring CyGameTextMgr::getNetStats(int /*PlayerTypes*/ iPlayer)
{
	CvWString szBuffer;
	GAMETEXT.setNetStats(szBuffer, ((PlayerTypes)iPlayer));
	return szBuffer;
}

std::wstring CyGameTextMgr::getTechHelp(int iTech, bool bCivilopediaText, bool bPlayerContext, bool bStrategyText, bool bTreeInfo, int iFromTech)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setTechHelp(szBuffer, (TechTypes)iTech, bCivilopediaText, bPlayerContext, bStrategyText, bTreeInfo, (TechTypes)iFromTech);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getUnitHelp(int iUnit, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, int iPlayer, int iCity)
{
	g_szLastCyRead = "CyGameTextMgr::getUnitHelp";
	CvWStringBuffer szBuffer;
	GAMETEXT.setUnitHelp(szBuffer, (UnitTypes)iUnit, bCivilopediaText, bStrategyText, bTechChooserText, cgt_city(iPlayer, iCity));
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getSpecificUnitHelp(int iPlayer, int iUnit, bool bOneLine, bool bShort)
{
	g_szLastCyRead = "CyGameTextMgr::getSpecificUnitHelp";
	CvWStringBuffer szBuffer;
	CvUnit* pUnit = cgt_unit(iPlayer, iUnit);
	if (pUnit != NULL)
	{
		GAMETEXT.setUnitHelp(szBuffer, pUnit, bOneLine, bShort);
	}
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getBuildingHelp(int iBuilding, bool bActual, int iPlayer, int iCity, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText)
{
	g_szLastCyRead = "CyGameTextMgr::getBuildingHelp";
	CvWStringBuffer szBuffer;
	GAMETEXT.setBuildingHelp(szBuffer, (BuildingTypes)iBuilding, bActual, cgt_city(iPlayer, iCity), bCivilopediaText, bStrategyText, bTechChooserText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getHeritageHelp(int iType, int iPlayer, int iCity, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setHeritageHelp(szBuffer, (HeritageTypes)iType, cgt_city(iPlayer, iCity), bCivilopediaText, bStrategyText, bTechChooserText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getProjectHelp(int iProject, bool bCivilopediaText, int iPlayer, int iCity)
{
	g_szLastCyRead = "CyGameTextMgr::getProjectHelp";
	CvWStringBuffer szBuffer;
	GAMETEXT.setProjectHelp(szBuffer, (ProjectTypes)iProject, bCivilopediaText, cgt_city(iPlayer, iCity));
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getPromotionHelp(int iPromotion, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setPromotionHelp(szBuffer, (PromotionTypes)iPromotion, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getUnitCombatHelp(int iUnitCombat, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setUnitCombatHelp(szBuffer, (UnitCombatTypes)iUnitCombat, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getTraitHelp(int iTrait)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setTraitHelp(szBuffer, (TraitTypes)iTrait);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getBonusHelp(int iBonus, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setBonusHelp(szBuffer, (BonusTypes)iBonus, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getProductionHelpCity(int iPlayer, int iCity)
{
	g_szLastCyRead = "CyGameTextMgr::getProductionHelpCity";
	CvWStringBuffer szBuffer;
	CvCity* pCity = cgt_city(iPlayer, iCity);
	if (pCity == NULL)
	{
		return std::wstring();
	}
	GAMETEXT.setProductionHelp(szBuffer, *pCity);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getReligionHelpCity(int iReligion, int iPlayer, int iCity, bool bCityScreen, bool bForceReligion, bool bForceState, bool bNoStateReligion)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setReligionHelpCity(szBuffer, (ReligionTypes)iReligion, cgt_city(iPlayer, iCity), bCityScreen, bForceReligion, bForceState, bNoStateReligion);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getCorporationHelpCity(int iCorporation, int iPlayer, int iCity, bool bCityScreen, bool bForceCorporation)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setCorporationHelpCity(szBuffer, (CorporationTypes)iCorporation, cgt_city(iPlayer, iCity), bCityScreen, bForceCorporation);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getImprovementHelp(int iImprovement, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setImprovementHelp(szBuffer, (ImprovementTypes)iImprovement, NO_FEATURE, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getRouteHelp(int iRoute, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setRouteHelp(szBuffer, (RouteTypes)iRoute, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getFeatureHelp(int iFeature, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setFeatureHelp(szBuffer, (FeatureTypes)iFeature, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getTerrainHelp(int iTerrain, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setTerrainHelp(szBuffer, (TerrainTypes)iTerrain, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseCivicInfo(int /*CivicTypes*/ iCivicType, bool bCivilopediaText, bool bPlayerContext, bool bSkipName)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseCivicInfo(szBuffer, (CivicTypes) iCivicType, bCivilopediaText, bPlayerContext, bSkipName);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseReligionInfo(int /*ReligionTypes*/ iReligionType, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setReligionHelp(szBuffer, (ReligionTypes) iReligionType, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseCorporationInfo(int /*CorporationTypes*/ iCorporationType, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setCorporationHelp(szBuffer, (CorporationTypes) iCorporationType, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseCivInfos(int /*CivilizationTypes*/ iCivilization, bool bDawnOfMan)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseCivInfos(szBuffer, (CivilizationTypes) iCivilization, bDawnOfMan);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseLeaderTraits(int /*LeaderHeadTypes*/ iLeader, bool bDawnOfMan, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseLeaderTraits(szBuffer, (LeaderHeadTypes)iLeader, NO_CIVILIZATION, bDawnOfMan, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::parseTraits(int /*TraitTypes*/ eTrait, bool bDawnOfMan, bool bEffectsOnly)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseTraits(szBuffer, (TraitTypes)eTrait, bDawnOfMan, bEffectsOnly);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getHappinessHelp()
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseHappinessHelp(szBuffer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getSpecialistHelp(int iSpecialist, bool bCivilopediaText)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.parseSpecialistHelp(szBuffer, (SpecialistTypes) iSpecialist, NULL, bCivilopediaText);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::buildHintsList()
{
	CvWStringBuffer szBuffer;
	GAMETEXT.buildHintsList(szBuffer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getAttitudeString(int iPlayer, int iTargetPlayer)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.getAttitudeString(szBuffer, (PlayerTypes)iPlayer, (PlayerTypes) iTargetPlayer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::setConvertHelp(int iPlayer, int iReligion)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setConvertHelp(szBuffer, (PlayerTypes)iPlayer, (ReligionTypes) iReligion);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::setRevolutionHelp(int iPlayer)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setRevolutionHelp(szBuffer, (PlayerTypes)iPlayer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getActiveDealsString(int iThisPlayer, int iOtherPlayer)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.getActiveDealsString(szBuffer, (PlayerTypes)iThisPlayer, (PlayerTypes)iOtherPlayer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getDealString(CyDeal* pDeal, int iPlayerPerspective)
{
	CvWStringBuffer szBuffer;
	if (pDeal && pDeal->getDeal())
	{
		GAMETEXT.getDealString(szBuffer, *(pDeal->getDeal()), (PlayerTypes)iPlayerPerspective);
	}
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getFinanceUnitUpkeepString(int iPlayer)
{
	CvWStringBuffer szBuffer;
	GAMETEXT.buildFinanceUnitUpkeepString(szBuffer, (PlayerTypes) iPlayer);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getDefenseHelp(int iPlayer, int iCity)
{
	g_szLastCyRead = "CyGameTextMgr::getDefenseHelp";
	CvWStringBuffer szBuffer;
	CvCity* pCity = cgt_city(iPlayer, iCity);
	if (pCity == NULL)
	{
		return std::wstring();
	}
	GAMETEXT.getDefenseHelp(szBuffer, *pCity);
	return szBuffer.getCString();
}

std::wstring CyGameTextMgr::getFlagHelp()
{
	CvWStringBuffer szBuffer;
	GAMETEXT.setFlagHelp(szBuffer);
	return szBuffer.getCString();
}


//
//	THE TEXT BOUNDARY, republished. TXT/localization is an UNMIGRATED SYSTEM BOUNDARY that STAYS
//	(patterns.md § THE PYTHON READ BOUNDARY: "resolution stays with the existing managers and Python screen
//	chrome keeps calling the text system directly") -- so this is not the library, and not the banned binding
//	surface either. It was collateral in the Cy BINDING purge, which took the kept boundary out along with the
//	read surface it was aimed at.
//	⚠ The composer BODIES were cut and are being rebuilt on appendEntryLines + the requires block composer, so a
//	method here answers empty until its composer lands. That is the correct exposed state -- the hole is visible
//	rather than masked (docs/specs/validation.md §Legacy must fail loud, never mask a cascade gap) -- and it is why nothing is "restored" beyond the publication.
//
void CyGameTextMgr::pythonPublish()
{
	python::class_<CyGameTextMgr>("CyGameTextMgr")
		.def("Reset", &CyGameTextMgr::Reset)
		.def("getTextKeys", &CyGameTextMgr::getTextKeys)
		.def("getSymbolChar", &CyGameTextMgr::getSymbolChar)
		.def("getHolyCitySymbolChar", &CyGameTextMgr::getHolyCitySymbolChar)
		.def("getHeadquarterSymbolChar", &CyGameTextMgr::getHeadquarterSymbolChar)
		.def("getTimeStr", &CyGameTextMgr::getTimeStr)
		.def("getDateStr", &CyGameTextMgr::getDateStr)
		.def("getOOSSeeds", &CyGameTextMgr::getOOSSeeds)
		.def("getNetStats", &CyGameTextMgr::getNetStats)
		.def("getTechHelp", &CyGameTextMgr::getTechHelp)
		.def("getUnitHelp", &CyGameTextMgr::getUnitHelp)
		.def("getSpecificUnitHelp", &CyGameTextMgr::getSpecificUnitHelp)
		.def("getBuildingHelp", &CyGameTextMgr::getBuildingHelp)
		.def("getHeritageHelp", &CyGameTextMgr::getHeritageHelp)
		.def("getProjectHelp", &CyGameTextMgr::getProjectHelp)
		.def("getPromotionHelp", &CyGameTextMgr::getPromotionHelp)
		.def("getUnitCombatHelp", &CyGameTextMgr::getUnitCombatHelp)
		.def("getBonusHelp", &CyGameTextMgr::getBonusHelp)
		.def("getProductionHelpCity", &CyGameTextMgr::getProductionHelpCity)
		.def("getReligionHelpCity", &CyGameTextMgr::getReligionHelpCity)
		.def("getCorporationHelpCity", &CyGameTextMgr::getCorporationHelpCity)
		.def("getImprovementHelp", &CyGameTextMgr::getImprovementHelp)
		.def("getRouteHelp", &CyGameTextMgr::getRouteHelp)
		.def("getTerrainHelp", &CyGameTextMgr::getTerrainHelp)
		.def("getFeatureHelp", &CyGameTextMgr::getFeatureHelp)
		.def("parseCivicInfo", &CyGameTextMgr::parseCivicInfo)
		.def("parseReligionInfo", &CyGameTextMgr::parseReligionInfo)
		.def("parseCorporationInfo", &CyGameTextMgr::parseCorporationInfo)
		.def("parseCivInfos", &CyGameTextMgr::parseCivInfos)
		.def("parseLeaderTraits", &CyGameTextMgr::parseLeaderTraits)
		.def("parseTraits", &CyGameTextMgr::parseTraits)
		.def("getHappinessHelp", &CyGameTextMgr::getHappinessHelp)
		.def("getSpecialistHelp", &CyGameTextMgr::getSpecialistHelp)
		.def("buildHintsList", &CyGameTextMgr::buildHintsList)
		.def("getAttitudeString", &CyGameTextMgr::getAttitudeString)
		.def("setConvertHelp", &CyGameTextMgr::setConvertHelp)
		.def("setRevolutionHelp", &CyGameTextMgr::setRevolutionHelp)
		.def("getActiveDealsString", &CyGameTextMgr::getActiveDealsString)
		.def("getDealString", &CyGameTextMgr::getDealString)
		.def("getFinanceUnitUpkeepString", &CyGameTextMgr::getFinanceUnitUpkeepString)
		.def("getDefenseHelp", &CyGameTextMgr::getDefenseHelp)
		.def("getFlagHelp", &CyGameTextMgr::getFlagHelp)
	;
}
