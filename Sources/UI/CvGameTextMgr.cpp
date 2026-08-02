//---------------------------------------------------------------------------------------
//
//  *****************   Civilization IV   ********************
//
//  FILE:	CvGameTextMgr.cpp
//
//  PURPOSE: Interfaces with GameText XML Files to manage the paths of art files
//
//---------------------------------------------------------------------------------------
//  Copyright (c) 2004 Firaxis Games, Inc. All rights reserved.
//---------------------------------------------------------------------------------------


#include "Tools/FProfiler.h"
#include "Infos/CvClassificationIds.h"   // the generated SKILL_/TAG_/CAPABILITY_ id table
#include "Engine/CvGameCoreUtils.h"
#include "CvGameCoreDLL.h"
#include "Engine/CvTraitSelection.h"

#include "Data/CvInfoValuation.h"   // InfoValuation::collectHealByUnitCombat + HealByUnitCombat
#include "Engine/CvGameSpeedScale.h"
#include "Engine/CvArea.h"
#include "CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "Infos/CvModifiers.h"        // entries() -- the compiled §3.9 deposits a composer renders
#include "UI/CvEntryText.h"           // entryDetailLine -- the ONE per-entry renderer
#include "CvBonusInfo.h"
#include "Engine/CvCity.h"
#include "AI/CvCityAI.h"
#include "Tools/CounterSet.h"
#include "Engine/CvDeal.h"
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvGameTextMgr.h"
#include "CvImprovementInfo.h"
#include "CvHeritageInfo.h"
#include "CvUnitCombatInfo.h"
#include "CvInfos.h"
#include "Engine/CvMap.h"
#include "AI/CvPlayerAI.h"
#include "CvPopupInfo.h"
#include "Infrastructure/CvPython.h"
#include "Engine/CvSelectionGroup.h"
#include "AI/CvTeamAI.h"
#include "Engine/CvUnit.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"
#include "Infrastructure/CvDLLSymbolIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "CvTraitInfo.h"

int shortenID(int iId)
{
	return iId;
}

// For displaying Asserts and error messages
static char* szErrorMsg;


//	Structure to hold aggregate info about an instances of a unit type and owener
//	on a stacked plot
class PlayerUnitInfo
{
public:
	PlayerUnitInfo()
		: m_eOwner(NO_PLAYER)
		, m_eUnitType(NO_UNIT)
		, m_iTotalStrength(0)
		, m_iTotalMaxStrength(0)
		, m_iCount(0)
	{
	}

	PlayerTypes						m_eOwner;
	UnitTypes						m_eUnitType;
	int								m_iTotalStrength;
	int								m_iTotalMaxStrength;
	int								m_iCount;
	std::map<PromotionTypes,int>	m_promotions;
};

//	Hash to provide a key for maps indexed by unit type and owner
#define	PLAYER_UNIT_KEY(ePlayer,eUnitType)	(((ePlayer) << 16) + (eUnitType))

//----------------------------------------------------------------------------
//
//	FUNCTION:	GetInstance()
//
//	PURPOSE:	Get the instance of this class.
//
//----------------------------------------------------------------------------
CvGameTextMgr& CvGameTextMgr::GetInstance()
{
	static CvGameTextMgr gs_GameTextMgr;
	return gs_GameTextMgr;
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	CvGameTextMgr()
//
//	PURPOSE:	Constructor
//
//----------------------------------------------------------------------------
CvGameTextMgr::CvGameTextMgr() : inspectUnitCombatCounters(NULL) { }

CvGameTextMgr::~CvGameTextMgr() { }

//----------------------------------------------------------------------------
//
//	FUNCTION:	Initialize()
//
//	PURPOSE:	Allocates memory
//
//----------------------------------------------------------------------------
void CvGameTextMgr::Initialize()
{
	inspectUnitCombatCounters = new CounterSet();
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	DeInitialize()
//
//	PURPOSE:	Clears memory
//
//----------------------------------------------------------------------------
void CvGameTextMgr::DeInitialize()
{
	SAFE_DELETE(inspectUnitCombatCounters);
}

//----------------------------------------------------------------------------
//
//	FUNCTION:	Reset()
//
//	PURPOSE:	Accesses CvXMLLoadUtility to clean global text memory and
//				reload the XML files
//
//----------------------------------------------------------------------------
void CvGameTextMgr::Reset()
{
	CvXMLLoadUtility pXML;
	pXML.LoadGlobalText();
}


// Returns the current language
int CvGameTextMgr::getCurrentLanguage() const
{
	return gDLL->getCurrentLanguage();
}

void CvGameTextMgr::setYearStr(CvWString& szString, int iGameTurn, bool bSave, CalendarTypes eCalendar, int iStartYear, GameSpeedTypes eSpeed)
{
	int iTurnYear = getTurnYearForGame(iGameTurn, iStartYear, eCalendar, eSpeed);

	setYearStrAC(szString, iTurnYear, bSave);
}

void CvGameTextMgr::setYearStrAC(CvWString& szString, int iTurnYear, bool bSave)
{
	if (iTurnYear < 0)
	{
		if (bSave)
		{
			szString = gDLL->getText("TXT_KEY_TIME_BC_SAVE", CvWString::format(L"%04d", -iTurnYear).GetCString());
		}
		else
		{
			szString = gDLL->getText("TXT_KEY_TIME_BC", -(iTurnYear));
		}
	}
	else if (iTurnYear > 0)
	{
		if (bSave)
		{
			szString = gDLL->getText("TXT_KEY_TIME_AD_SAVE", CvWString::format(L"%04d", iTurnYear).GetCString());
		}
		else
		{
			szString = gDLL->getText("TXT_KEY_TIME_AD", iTurnYear);
		}
	}
	else
	{
		if (bSave)
		{
			szString = gDLL->getText("TXT_KEY_TIME_AD_SAVE", L"0000");
		}
		else
		{
			szString = gDLL->getText("TXT_KEY_TIME_AD", 0);
		}
	}
}


void CvGameTextMgr::setDateStr(CvWString& szString, int iGameTurn, bool bSave, CalendarTypes eCalendar, int iStartYear, GameSpeedTypes eSpeed)
{
	CvWString szYearBuffer;
	CvWString szWeekBuffer;
	CvDate date;

	setYearStr(szYearBuffer, iGameTurn, bSave, eCalendar, iStartYear, eSpeed);
	const int iyear = date.getDate(iGameTurn, eSpeed).getYear();;

	const int numMonths = std::max(1, GC.getNumMonthInfos());
	const int numSeasons = std::max(1, GC.getNumSeasonInfos());
	const int weeksPerMonth = std::max(1, GC.getDefineINT("WEEKS_PER_MONTHS"));

	if (bSave && iyear < 0)
	{
		eCalendar = CALENDAR_YEARS;
	}

	switch (eCalendar)
	{
	case CALENDAR_DEFAULT:
	{
		if (GC.getGame().getGameTurn() == iGameTurn)
		{
			date = GC.getGame().getCurrentDate();
		}
		else
		{
			date = CvDate::getDate(iGameTurn, eSpeed);
		}
		// pick the display granularity from how much calendar a turn covers here
		const int iTicksPerTurn = date.getTicksPerTurn(eSpeed);
		if (iTicksPerTurn >= 30 * numMonths)
		{
			// Years
			szString = szYearBuffer;
		}
		else if (iTicksPerTurn >= 30 * numMonths / numSeasons)
		{
			// Seasons
			if (bSave)
			{
				szString = (szYearBuffer + "-" + GC.getSeasonInfo(date.getSeason()).getDescription());
			}
			else
			{
				szString = (GC.getSeasonInfo(date.getSeason())).getDescription() + CvString(", ") + szYearBuffer;
			}
		}
		else if (iTicksPerTurn >= 30)
		{
			// Months
			if (bSave)
			{
				szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription());
			}
			else
			{
				szString = (GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription() + CvString(", ") + szYearBuffer);
			}
		}
		else
		{
			// Exact date
			szString = gDLL->getText("TXT_KEY_TIME_DATE", szYearBuffer.GetCString(), GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription(), date.getDay());
		}
		break;
	}
	case CALENDAR_NO_SEASONS:
	{
		if (GC.getGame().getGameTurn() == iGameTurn)
		{
			date = GC.getGame().getCurrentDate();
		}
		else
		{
			date = CvDate::getDate(iGameTurn, eSpeed);
		}
		const int iTicksPerTurn = date.getTicksPerTurn(eSpeed);
		if (iTicksPerTurn >= 30 * numMonths)
		{
			// Years
			szString = szYearBuffer;
		}
		else if (iTicksPerTurn >= 30)
		{
			// Months
			if (bSave)
			{
				szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription());
			}
			else
			{
				szString = (GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription() + CvString(", ") + szYearBuffer);
			}
		}
		else
		{
			// Exact date
			szString = gDLL->getText("TXT_KEY_TIME_DATE", szYearBuffer.GetCString(), GC.getMonthInfo((MonthTypes)date.getMonth()).getDescription(), date.getDay());
		}
		break;
	}
	case CALENDAR_YEARS:
	case CALENDAR_BI_YEARLY:
		szString = szYearBuffer;
		break;

	case CALENDAR_TURNS:
		szString = gDLL->getText("TXT_KEY_TIME_TURN", (iGameTurn + 1));
		break;

	case CALENDAR_SEASONS:
		if (bSave)
		{
			szString = (szYearBuffer + "-" + GC.getSeasonInfo((SeasonTypes)(iGameTurn % numSeasons)).getDescription());
		}
		else
		{
			szString = (GC.getSeasonInfo((SeasonTypes)(iGameTurn % numSeasons)).getDescription() + CvString(", ") + szYearBuffer);
		}
		break;

	case CALENDAR_MONTHS:
		if (bSave)
		{
			szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)(iGameTurn % numMonths)).getDescription());
		}
		else
		{
			szString = (GC.getMonthInfo((MonthTypes)(iGameTurn % numMonths)).getDescription() + CvString(", ") + szYearBuffer);
		}
		break;

	case CALENDAR_WEEKS:
		szWeekBuffer = gDLL->getText("TXT_KEY_TIME_WEEK", ((iGameTurn % weeksPerMonth) + 1));
		if (bSave)
		{
			szString = (szYearBuffer + "-" + GC.getMonthInfo((MonthTypes)((iGameTurn / weeksPerMonth) % numMonths)).getDescription() + "-" + szWeekBuffer);
		}
		else
		{
			szString = (szWeekBuffer + ", " + GC.getMonthInfo((MonthTypes)((iGameTurn / weeksPerMonth) % numMonths)).getDescription() + ", " + szYearBuffer);
		}
		break;

	default:
		FErrorMsg("error");
	}

	//FR remove diacritics
	if (bSave)
	{
		szString = remove_diacritics(szString);
		CvString szTemp = "";
		szTemp.Format("%d - ", iGameTurn);
		szString = szTemp + szString;
	}

}


void CvGameTextMgr::setTimeStr(CvWString& szString, int iGameTurn, bool bSave)
{
	setDateStr(szString, iGameTurn, bSave, GC.getGame().getCalendar(), GC.getGame().getStartYear(), GC.getGame().getGameSpeedType());
}


void CvGameTextMgr::setInterfaceTime(CvWString& szString, PlayerTypes ePlayer)
{
	CvWString szTempBuffer;

	if (GET_PLAYER(ePlayer).isGoldenAge())
	{
		szString.Format(L"%c(%d) ", gDLL->getSymbolID(GOLDEN_AGE_CHAR), GET_PLAYER(ePlayer).getGoldenAgeTurns());
	}
	else
	{
		szString.clear();
	}
	setTimeStr(szTempBuffer, GC.getGame().getGameTurn(), false);
	szString += CvWString(szTempBuffer);
}


void CvGameTextMgr::setOOSSeeds(CvWString& szString, PlayerTypes ePlayer)
{
	if (GET_PLAYER(ePlayer).isHumanPlayer())
	{
		int iNetID = GET_PLAYER(ePlayer).getNetID();
		if (gDLL->isConnected(iNetID))
		{
			szString = gDLL->getText("TXT_KEY_PLAYER_OOS", gDLL->GetSyncOOS(iNetID), gDLL->GetOptionsOOS(iNetID));
		}
	}
}

void CvGameTextMgr::setNetStats(CvWString& szString, PlayerTypes ePlayer)
{
	if (ePlayer != GC.getGame().getActivePlayer())
	{
		if (GET_PLAYER(ePlayer).isHumanPlayer())
		{
			if (gDLL->getInterfaceIFace()->isNetStatsVisible())
			{
				int iNetID = GET_PLAYER(ePlayer).getNetID();
				if (gDLL->isConnected(iNetID))
				{
					szString = gDLL->getText("TXT_KEY_MISC_NUM_MS", gDLL->GetLastPing(iNetID));
				}
				else
				{
					szString = gDLL->getText("TXT_KEY_MISC_DISCONNECTED");
				}
			}
		}
		else
		{
			szString = gDLL->getText("TXT_KEY_MISC_AI");
		}
	}
}


void CvGameTextMgr::setEspionageMissionHelp(CvWStringBuffer &szBuffer, const CvUnit* pUnit)
{
	if (pUnit->isSpy())
	{
		const PlayerTypes eOwner =  pUnit->plot()->getOwner();
		if (NO_PLAYER != eOwner && GET_PLAYER(eOwner).getTeam() != pUnit->getTeam())
		{
			if (!pUnit->canEspionage(pUnit->plot()))
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_UNITHELP_NO_ESPIONAGE"));

				if (pUnit->hasMoved() || pUnit->isMadeAttack())
				{
					szBuffer.append(gDLL->getText("TXT_KEY_UNITHELP_NO_ESPIONAGE_REASON_MOVED"));
				}
				else if (!pUnit->isInvisible(GET_PLAYER(eOwner).getTeam(), false))
				{
					szBuffer.append(gDLL->getText("TXT_KEY_UNITHELP_NO_ESPIONAGE_REASON_VISIBLE", GET_PLAYER(eOwner).getNameKey()));
				}
			}
			else if (pUnit->getFortifyTurns() > 0)
			{
				int iModifier = -(pUnit->getFortifyTurns() * GC.getDefineINT("ESPIONAGE_EACH_TURN_UNIT_COST_DECREASE"));
				if (0 != iModifier)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_ESPIONAGE_COST", iModifier));
				}
			}
		}
	}
}


void CvGameTextMgr::setUnitHelp(CvWStringBuffer &szString, const CvUnit* pUnit, bool bOneLine, bool bShort, bool bdarkColor)
{
}


void CvGameTextMgr::setPlotListHelp(CvWStringBuffer &szString, CvPlot* pPlot, bool bOneLine, bool bShort)
{
}

namespace {
	void addModifierIfValid(CvWStringBuffer& szString, int modifier, const char* const txt)
	{
		if (modifier != 0)
		{
			szString.append(NEWLINE);
			szString.append(gDLL->getText(txt, modifier));
		}
	}

	void addModifierWithInfoIfValid(CvWStringBuffer& szString, int modifier, const char* const txt, const CvInfoBase& info)
	{
		if (modifier != 0)
		{
			szString.append(NEWLINE);
			szString.append(gDLL->getText(txt, modifier, info.getTextKeyWide()));
		}
	}

	void addCombatModifierHint(CvWStringBuffer& szString, int base, int coeff, const char* const txt)
	{
		if (base != 0 && coeff > 0)
		{
			addModifierIfValid(szString, base * coeff, txt);
		}
	}

	typedef  int (CvUnit::* GetCombatModifierFn)(UnitCombatTypes combatType) const;

	void addCombatTypeModifierHints(CvWStringBuffer& szString, const CvUnit* defender, const CvUnit* attacker, GetCombatModifierFn modifierFn, const char* const txt)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			UnitCombatTypes eUnitCombatType = static_cast<UnitCombatTypes>(iI);
			if (defender->isHasUnitCombat(eUnitCombatType))
			{
				int modifier = (attacker->*(modifierFn))(eUnitCombatType);
				if (modifier != 0)
				{
					szString.append(NEWLINE);
					szString.append(gDLL->getText(txt, modifier, GC.getUnitCombatInfo(eUnitCombatType).getTextKeyWide()));
				}
			}
		}
	}
}

// Draws the at-a-glance combat-odds bar: a fixed 200px strip (1% = 2px) split into
// green (attacker wins -- kills or reaches the combat limit), yellow (attacker
// retreats) and red (attacker is defeated), proportional to the outcome odds in
// the CombatPreview. Art ships in the C2C FPK (Art/ACO/{green,yellow,red}_bar_*.dds).
static void appendCombatOddsBar(CvWStringBuffer& szString, const CombatPreview& kP)
{
	const float prob1 = 100.0f * (kP.fAttackerKillOdds + kP.fPullOutOdds); // cumulative: up to a win
	const float prob2 = prob1 + 100.0f * kP.fRetreatOdds;                  // cumulative: up to a retreat

	CvWString szImg;
	int pixels_left = 199; // total bar is 200px; 1 reserved for the right end cap

	// Green (victory).
	int greenPixels = (2 * (int)(prob1 + 0.5f)) - 1; // -1 for the left end cap
	if (greenPixels < 0) greenPixels = 0;
	szString.append(L"<img=Art/ACO/green_bar_left_end.dds>");
	for (int i = 0; i < greenPixels / 10; ++i) { szString.append(L"<img=Art/ACO/green_bar_10.dds>"); pixels_left -= 10; }
	if (greenPixels % 10 > 0) { szImg.Format(L"<img=Art/ACO/green_bar_%d.dds>", greenPixels % 10); szString.append(szImg.GetCString()); pixels_left -= greenPixels % 10; }

	// Yellow (retreat) -- the span from the green end to the retreat end.
	int yellowPixels = 2 * (int)(prob2 + 0.5f) - (greenPixels + 1);
	if (yellowPixels < 0) yellowPixels = 0;
	for (int i = 0; i < yellowPixels / 10; ++i) { szString.append(L"<img=Art/ACO/yellow_bar_10.dds>"); pixels_left -= 10; }
	if (yellowPixels % 10 > 0) { szImg.Format(L"<img=Art/ACO/yellow_bar_%d.dds>", yellowPixels % 10); szString.append(szImg.GetCString()); pixels_left -= yellowPixels % 10; }

	// Red (defeat) -- fills the remainder.
	if (pixels_left < 0) pixels_left = 0;
	for (int i = 0; i < pixels_left / 10; ++i) { szString.append(L"<img=Art/ACO/red_bar_10.dds>"); }
	if (pixels_left % 10 > 0) { szImg.Format(L"<img=Art/ACO/red_bar_%d.dds>", pixels_left % 10); szString.append(szImg.GetCString()); }
	szString.append(L"<img=Art/ACO/red_bar_right_end.dds>");
}

// Returns true if help was given...
bool CvGameTextMgr::setCombatPlotHelp(CvWStringBuffer& szString, CvPlot* pPlot, bool bAssassinate)
{
	PROFILE_FUNC();

	if (gDLL->getInterfaceIFace()->getLengthSelectionList() == 0)
	{
		return false;
	}
	// Note that due to the large amount of extra content added to this function (setCombatPlotHelp),
	//	this should never be used in any function that needs to be called repeatedly (e.g. hundreds of times) quickly.
	// It is fine for a human player mouse-over (which is what it is used for).
	CvSelectionGroup* group = gDLL->getInterfaceIFace()->getSelectionList();

	switch (group->getDomainType())
	{
		case DOMAIN_SEA:
		{
			if (!pPlot->isWater() && !group->canMoveAllTerrain())
			{
				return false;
			}
			break;
		}
		case DOMAIN_AIR:
		{
			break;
		}
		case DOMAIN_LAND:
		{
			if (pPlot->isWater() && !pPlot->isSeaTunnel() && !group->canMoveAllTerrain())
			{
				return false;
			}
			break;
		}
		case DOMAIN_IMMOBILE:
		{
			return false;
		}
		default:
		{
			FErrorMsg("error");
			return false;
		}
	}

	CvUnit* pAttacker;

	if (GC.getGame().getActivePlayer() != NO_PLAYER)
	{
		pAttacker = GET_PLAYER(GC.getGame().getActivePlayer()).getUnit(GET_PLAYER(GC.getGame().getActivePlayer()).getAmbushingUnit());
	}
	if (!pAttacker)
	{
		const bool bIgnoreMadeAttack = !group->canAttackNow();
		int iOdds;
		pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, false, NULL, bAssassinate, false, bIgnoreMadeAttack);

		if (!pAttacker)
		{
			pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, true, NULL, bAssassinate, false, bIgnoreMadeAttack);
		}
	}

	if (pAttacker)
	{
		CvWString szTempBuffer2;
		CvWString szTempBuffer;

		bool bStealthAttack = false;
		bool bStealthDefense = false;
		bool bIsSamePlot = false;

		// Shift reveals the extra "needed rounds" detail line (iView == 2).
		const int iView = gDLL->shiftKey() ? 2 : 1;

		CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, pAttacker->getOwner(), pAttacker, !gDLL->altKey(), NO_TEAM == pAttacker->getDeclareWarMove(pPlot), false, bAssassinate);

		if (pDefender && pDefender != pAttacker && pDefender->canDefend(pPlot) && ((pAttacker->canAttack(*pDefender) || pAttacker->canAmbush(*pDefender, bAssassinate))))
		{
			bool bAttackerInvisible = pAttacker->isInvisible(GET_PLAYER(pDefender->getOwner()).getTeam(), false, false);
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING))
			{
				if (bAttackerInvisible || bIsSamePlot)
				{
					bStealthAttack = true;
				}
				if (bStealthAttack && bIsSamePlot && !bAttackerInvisible)
				{
					bStealthDefense = true;
				}
			}

			if (pAttacker->getDomainType() != DOMAIN_AIR)
			{
				//TB Combat Mod begin

				if (pAttacker->isBreakdownCombat(pPlot))
				{

					CvCity* tCity = pPlot->getPlotCity();

					int iChance = pAttacker->breakdownChanceTotal();
					int iTrueChance = std::max(5, iChance);

					int iBombardDefMod = std::max(0, (100 - tCity->getBombardDefense()));
					int iNormalDamage = pAttacker->breakdownDamageTotal();
					int iTrueDamage = (iNormalDamage * iBombardDefMod) / 100;

					szTempBuffer.Format(SETCOLR L"%s %d%%" ENDCOLR,
						TEXT_COLOR("COLOR_RED"), gDLL->getText("TXT_KEY_COMBAT_BREAKDOWN_EFFECTS").c_str(), iTrueChance, iTrueDamage);
					szString.append(szTempBuffer.GetCString());
					szString.append(NEWLINE);
				}

				// Lean combat preview. All combat math comes from CvCombatModel
				// (computeCombatPreview); this block only formats the result.
				// A future combat-mod rule adds its own rows via kP.detailLines,
				// so the renderer never needs per-rule edits. Holding Shift
				// (iView == 2) also requests the itemised modifier breakdown.
				const CombatPreview kP = computeCombatPreview(pAttacker, pDefender, iView == 2);
				if (kP.bValid)
				{
					const bool bCanKill = (kP.iDefenderHitLimitHP == 0);

					// --- Strengths (final, post-modifier) ---
					szTempBuffer.Format(L"%.2f", pAttacker->currCombatStrFloat(NULL, NULL));
					if (pAttacker->isHurt())
					{
						szTempBuffer.append(L" ");
						szTempBuffer.append(gDLL->getText("TXT_ACO_INJURED_HP", pAttacker->getHP(), pAttacker->getMaxHP()));
					}
					szTempBuffer2.Format(L"%.2f", pDefender->currCombatStrFloat(pPlot, pAttacker));
					if (pDefender->isHurt())
					{
						szTempBuffer2.append(L" ");
						szTempBuffer2.append(gDLL->getText("TXT_ACO_INJURED_HP", pDefender->getHP(), pDefender->getMaxHP()));
					}
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_ACO_VS", szTempBuffer.GetCString(), szTempBuffer2.GetCString()));

					// --- Visual odds bar: green = win, yellow = retreat, red = defeat ---
					szString.append(NEWLINE);
					appendCombatOddsBar(szString, kP);

					// --- Primary outcome: kill, or pull-out at the combat limit ---
					szString.append(NEWLINE);
					if (bCanKill)
					{
						szString.append(gDLL->getText("TXT_ACO_VICTORY"));
						szTempBuffer.Format(L": " SETCOLR L"%.2f%% %d" ENDCOLR,
							TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fAttackerKillOdds, kP.iVictoryXP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_XP"));
						szTempBuffer.Format(L"  (" SETCOLR L"%.1f" ENDCOLR,
							TEXT_COLOR("COLOR_POSITIVE_TEXT"), kP.fExpHPAttackerWin);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szString.append(L")");
					}
					else
					{
						szString.append(gDLL->getText("TXT_ACO_WITHDRAW"));
						szTempBuffer.Format(L": " SETCOLR L"%.2f%% %d" ENDCOLR,
							TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fPullOutOdds, kP.iVictoryXP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_XP"));
						szTempBuffer.Format(L"  (" SETCOLR L"%.1f" ENDCOLR,
							TEXT_COLOR("COLOR_POSITIVE_TEXT"), kP.fExpHPAttackerPullOut);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szTempBuffer.Format(L", " SETCOLR L"%d" ENDCOLR,
							TEXT_COLOR("COLOR_NEGATIVE_TEXT"), kP.iDefenderHitLimitHP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP"));
						szString.append(L")");
					}

					// --- Retreat (vanilla withdrawal) ---
					if (kP.fRetreatOdds > 0.0f)
					{
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_ACO_RETREAT"));
						szTempBuffer.Format(L": " SETCOLR L"%.2f%% %d" ENDCOLR,
							TEXT_COLOR("COLOR_UNIT_TEXT"), 100.0f * kP.fRetreatOdds, kP.iRetreatXP);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_XP"));
						szTempBuffer.Format(L"  (" SETCOLR L"%d" ENDCOLR,
							TEXT_COLOR("COLOR_UNIT_TEXT"), kP.iExpHPAttackerRetreat);
						szString.append(szTempBuffer.GetCString());
						szString.append(gDLL->getText("TXT_ACO_HP_NEUTRAL"));
						szString.append(L")");
					}

					// --- Defeat ---
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_ACO_DEFEAT"));
					szTempBuffer.Format(L": " SETCOLR L"%.2f%% %d" ENDCOLR,
						TEXT_COLOR("COLOR_NEGATIVE_TEXT"), 100.0f * kP.fDefenderKillOdds, kP.iDefenderKillXP);
					szString.append(szTempBuffer.GetCString());
					szString.append(gDLL->getText("TXT_ACO_XP"));
					szTempBuffer.Format(L"  (" SETCOLR L"%.1f" ENDCOLR,
						TEXT_COLOR("COLOR_NEGATIVE_TEXT"), kP.fExpHPDefenderWin);
					szString.append(szTempBuffer.GetCString());
					szString.append(gDLL->getText("TXT_ACO_HP"));
					szString.append(L")");

					// --- First strikes and their measured effect on the win chance ---
					if (kP.iAttackerFirstStrikes || kP.iAttackerFirstStrikeChances
						|| kP.iDefenderFirstStrikes || kP.iDefenderFirstStrikeChances)
					{
						const int iSwing = kP.iWinOddsWithFS - kP.iWinOddsNoFS; // out of 1000
						const char* szSwingColor = (iSwing >= 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT";
						CvWString szSwing;
						szSwing.Format(SETCOLR L"%+.1f%%" ENDCOLR, TEXT_COLOR(szSwingColor), iSwing / 10.0f);
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_ACO_FIRST_STRIKE",
							kP.iAttackerFirstStrikes + kP.iAttackerFirstStrikeChances,
							kP.iDefenderFirstStrikes + kP.iDefenderFirstStrikeChances,
							szSwing.GetCString()));
					}

					// --- Needed rounds (detail, shown while holding Shift) ---
					if (iView == 2)
					{
						szString.append(NEWLINE);
						szString.append(gDLL->getText("TXT_ACO_ROUNDS", kP.iNeededRoundsAttacker, kP.iNeededRoundsDefender));
					}

					// --- Detail rows: the Shift modifier breakdown and any plugin
					// rows. Each szLabel is already complete (the producer formats
					// its value in), so the renderer only colours and prints it. ---
					for (std::vector<CombatPreviewLine>::const_iterator it = kP.detailLines.begin(); it != kP.detailLines.end(); ++it)
					{
						const char* szLineColor = (it->eCategory == COMBAT_PREVIEW_POSITIVE) ? "COLOR_POSITIVE_TEXT"
							: (it->eCategory == COMBAT_PREVIEW_NEGATIVE) ? "COLOR_NEGATIVE_TEXT" : "COLOR_UNIT_TEXT";
						szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR(szLineColor), it->szLabel.c_str());
						szString.append(NEWLINE);
						szString.append(szTempBuffer.GetCString());
					}
				}

			}
			return true;
		}
	}

	return false;
}

// Returns true if help was given...
bool CvGameTextMgr::setMinimalCombatPlotHelp(CvWStringBuffer& szString, CvPlot* pPlot, bool bAssassinate)
{
	PROFILE_FUNC();

	if (gDLL->getInterfaceIFace()->getLengthSelectionList() == 0)
		return false;

	CvSelectionGroup* group = gDLL->getInterfaceIFace()->getSelectionList();

	switch (group->getDomainType())
	{
	case DOMAIN_SEA:
		if (!pPlot->isWater() && !group->canMoveAllTerrain())
			return false;
		break;
	case DOMAIN_LAND:
		if (pPlot->isWater() && !pPlot->isSeaTunnel() && !group->canMoveAllTerrain())
			return false;
		break;
	case DOMAIN_IMMOBILE:
		return false;
	default:
		FErrorMsg("error");
		return false;
	}

	CvUnit* pAttacker = nullptr;
	if (GC.getGame().getActivePlayer() != NO_PLAYER)
	{
		pAttacker = GET_PLAYER(GC.getGame().getActivePlayer()).getUnit(GET_PLAYER(GC.getGame().getActivePlayer()).getAmbushingUnit());
	}
	if (!pAttacker)
	{
		int iOdds;
		const bool bIgnoreMadeAttack = !group->canAttackNow();
		pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, false, NULL, bAssassinate, false, bIgnoreMadeAttack);

		if (!pAttacker)
			pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, true, NULL, bAssassinate, false, bIgnoreMadeAttack);
		}

	if (!pAttacker)
		return false;

	CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, pAttacker->getOwner(), pAttacker, true, NO_TEAM == pAttacker->getDeclareWarMove(pPlot), false, bAssassinate);

	if (!pDefender || pDefender == pAttacker || !pDefender->canDefend(pPlot) || !(pAttacker->canAttack(*pDefender) || pAttacker->canAmbush(*pDefender, bAssassinate)))
		return false;

	// Show attacker and defender info
	szString.append(gDLL->getText("TXT_ACO_ATTACKER"));
	szString.append(NEWLINE);
	setUnitHelp(szString, pAttacker, true, true, true);
	szString.append(NEWLINE);
	szString.append(gDLL->getText("TXT_ACO_CIBLE"));
	szString.append(NEWLINE);
	setUnitHelp(szString, pDefender, true, true, true);

	// Lean one-line outcome summary, all figures from CvCombatModel.
	const CombatPreview kP = computeCombatPreview(pAttacker, pDefender);
	if (kP.bValid)
	{
		CvWString szTmp;
		szString.append(NEWLINE);

		// Graphical odds bar (green = win, yellow = retreat, red = defeat).
		appendCombatOddsBar(szString, kP);
		szString.append(NEWLINE);

		if (kP.iDefenderHitLimitHP == 0)
		{
			szString.append(gDLL->getText("TXT_ACO_VICTORY"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fAttackerKillOdds);
		}
		else
		{
			szString.append(gDLL->getText("TXT_ACO_WITHDRAW"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fPullOutOdds);
		}
		szString.append(szTmp.GetCString());

		if (kP.fRetreatOdds > 0.0f)
		{
			szString.append(gDLL->getText("TXT_ACO_RETREAT"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_UNIT_TEXT"), 100.0f * kP.fRetreatOdds);
			szString.append(szTmp.GetCString());
		}

		szString.append(gDLL->getText("TXT_ACO_DEFEAT"));
		szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), 100.0f * kP.fDefenderKillOdds);
		szString.append(szTmp.GetCString());

		if (kP.iAttackerFirstStrikes || kP.iAttackerFirstStrikeChances
			|| kP.iDefenderFirstStrikes || kP.iDefenderFirstStrikeChances)
		{
			const int iSwing = kP.iWinOddsWithFS - kP.iWinOddsNoFS;
			const char* szSwingColor = (iSwing >= 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT";
			CvWString szSwing;
			szSwing.Format(SETCOLR L"%+.1f%%" ENDCOLR, TEXT_COLOR(szSwingColor), iSwing / 10.0f);
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_ACO_FIRST_STRIKE",
				kP.iAttackerFirstStrikes + kP.iAttackerFirstStrikeChances,
				kP.iDefenderFirstStrikes + kP.iDefenderFirstStrikeChances,
				szSwing.GetCString()));
		}
	}

	return true;
}



bool CvGameTextMgr::setAssassinatePlotHelp(CvWStringBuffer& szString, CvPlot* pPlot, CvUnit* pAttacker, CvUnit* pDefender)
{
	PROFILE_FUNC();
	bool bAssassinate = true;
	if (gDLL->getInterfaceIFace()->getLengthSelectionList() == 0)
		return false;

	CvSelectionGroup* group = gDLL->getInterfaceIFace()->getSelectionList();

	switch (group->getDomainType())
	{
	case DOMAIN_SEA:
		if (!pPlot->isWater() && !group->canMoveAllTerrain())
			return false;
		break;
	case DOMAIN_LAND:
		if (pPlot->isWater() && !pPlot->isSeaTunnel() && !group->canMoveAllTerrain())
			return false;
		break;
	case DOMAIN_IMMOBILE:
		return false;
	default:
		FErrorMsg("error");
		return false;
	}

	if (GC.getGame().getActivePlayer() != NO_PLAYER)
	{
		pAttacker = GET_PLAYER(GC.getGame().getActivePlayer()).getUnit(GET_PLAYER(GC.getGame().getActivePlayer()).getAmbushingUnit());
	}
	if (!pAttacker)
	{
		int iOdds;
		const bool bIgnoreMadeAttack = !group->canAttackNow();
		pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, false, NULL, bAssassinate, false, bIgnoreMadeAttack);

		if (!pAttacker)
			pAttacker = group->AI_getBestGroupAttacker(pPlot, false, iOdds, true, NULL, bAssassinate, false, bIgnoreMadeAttack);
	}

	if (!pAttacker)
		return false;

	if (!pDefender || pDefender == pAttacker || !pDefender->canDefend(pPlot) || !(pAttacker->canAttack(*pDefender) || pAttacker->canAmbush(*pDefender, bAssassinate)))
		return false;

	// Show the assassination target.
	szString.append(gDLL->getText("TXT_ACO_CIBLE"));
	szString.append(NEWLINE);
	setUnitHelp(szString, pDefender, true, true, true);

	// Lean one-line outcome summary, all figures from CvCombatModel.
	const CombatPreview kP = computeCombatPreview(pAttacker, pDefender);
	if (kP.bValid)
	{
		CvWString szTmp;
		szString.append(NEWLINE);

		// Graphical odds bar (green = win, yellow = retreat, red = defeat).
		appendCombatOddsBar(szString, kP);
		szString.append(NEWLINE);

		if (kP.iDefenderHitLimitHP == 0)
		{
			szString.append(gDLL->getText("TXT_ACO_VICTORY"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fAttackerKillOdds);
		}
		else
		{
			szString.append(gDLL->getText("TXT_ACO_WITHDRAW"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_POSITIVE_TEXT"), 100.0f * kP.fPullOutOdds);
		}
		szString.append(szTmp.GetCString());

		if (kP.fRetreatOdds > 0.0f)
		{
			szString.append(gDLL->getText("TXT_ACO_RETREAT"));
			szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR L"   ", TEXT_COLOR("COLOR_UNIT_TEXT"), 100.0f * kP.fRetreatOdds);
			szString.append(szTmp.GetCString());
		}

		szString.append(gDLL->getText("TXT_ACO_DEFEAT"));
		szTmp.Format(L" " SETCOLR L"%.1f%%" ENDCOLR, TEXT_COLOR("COLOR_NEGATIVE_TEXT"), 100.0f * kP.fDefenderKillOdds);
		szString.append(szTmp.GetCString());

		if (kP.iAttackerFirstStrikes || kP.iAttackerFirstStrikeChances
			|| kP.iDefenderFirstStrikes || kP.iDefenderFirstStrikeChances)
		{
			const int iSwing = kP.iWinOddsWithFS - kP.iWinOddsNoFS;
			const char* szSwingColor = (iSwing >= 0) ? "COLOR_POSITIVE_TEXT" : "COLOR_NEGATIVE_TEXT";
			CvWString szSwing;
			szSwing.Format(SETCOLR L"%+.1f%%" ENDCOLR, TEXT_COLOR(szSwingColor), iSwing / 10.0f);
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_ACO_FIRST_STRIKE",
				kP.iAttackerFirstStrikes + kP.iAttackerFirstStrikeChances,
				kP.iDefenderFirstStrikes + kP.iDefenderFirstStrikeChances,
				szSwing.GetCString()));
		}
	}

	return true;
}


// DO NOT REMOVE - needed for font testing - Moose
void createTestFontString(CvWStringBuffer& szString)
{
	PROFILE_EXTRA_FUNC();
	int iI;
	for (iI=0;iI<NUM_YIELD_TYPES;++iI)
		szString.append(CvWString::format(L"%c", GC.getYieldInfo((YieldTypes) iI).getChar()));

	szString.append(L"\n");
	for (iI=0;iI<NUM_COMMERCE_TYPES;++iI)
		szString.append(CvWString::format(L"%c", GC.getCommerceInfo((CommerceTypes) iI).getChar()));
	szString.append(L"\n");
	for (iI = 0; iI < GC.getNumReligionInfos(); ++iI)
	{
		szString.append(CvWString::format(L"%c", GC.getReligionInfo((ReligionTypes) iI).getChar()));
		szString.append(CvWString::format(L"%c", GC.getReligionInfo((ReligionTypes) iI).getHolyCityChar()));
	}
	szString.append(L"\n");
	for (iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
	{
		szString.append(CvWString::format(L"%c%d", GC.getCorporationInfo((CorporationTypes) iI).getChar(), GC.getCorporationInfo((CorporationTypes) iI).getChar()));
		szString.append(CvWString::format(L"%c%d", GC.getCorporationInfo((CorporationTypes) iI).getHeadquarterChar(), GC.getCorporationInfo((CorporationTypes) iI).getHeadquarterChar()));
	}
	szString.append(L"\n");
	for (iI = 0; iI < GC.getNumPropertyInfos(); ++iI)
	{
		szString.append(CvWString::format(L"%c%d", GC.getPropertyInfo((PropertyTypes) iI).getChar(), GC.getPropertyInfo((PropertyTypes) iI).getChar()));
	}
	szString.append(L"\n");
	for (iI = 0; iI < GC.getNumBonusInfos(); ++iI)
		szString.append(CvWString::format(L"%c%d", GC.getBonusInfo((BonusTypes) iI).getChar(), GC.getBonusInfo((BonusTypes) iI).getChar()));
	szString.append(L"\n");
	for (iI=0; iI<MAX_NUM_SYMBOLS; ++iI)
		szString.append(CvWString::format(L"%c%d", gDLL->getSymbolID(iI), gDLL->getSymbolID(iI)));
}

void CvGameTextMgr::setPlotHelp(CvWStringBuffer& szString, CvPlot* pPlot)
{
}


void CvGameTextMgr::setCityPlotYieldValueString(CvWStringBuffer& szString, CvCity* pCity, int iIndex, bool bAvoidGrowth, bool bIgnoreGrowth, bool bIgnoreFood) const
{
	PROFILE_FUNC();

	CvPlot* pPlot = NULL;

	if (iIndex >= 0 && iIndex < NUM_CITY_PLOTS)
		pPlot = pCity->getCityIndexPlot(iIndex);

	if (pPlot && pPlot->getWorkingCity() == pCity)
	{
		CvCityAI* pCityAI = static_cast<CvCityAI*>(pCity);
		bool bWorkingPlot = pCity->isWorkingPlot(iIndex);

		int iValue = pCityAI->AI_plotValue(pPlot, bAvoidGrowth, /*bRemove*/ bWorkingPlot, bIgnoreFood, bIgnoreGrowth);

		setYieldValueString(szString, iValue, /*bActive*/ bWorkingPlot);
	}
	else
		setYieldValueString(szString, 0, /*bActive*/ false, /*bMakeWhitespace*/ true);
}

void CvGameTextMgr::setYieldValueString(CvWStringBuffer &szString, int iValue, bool bActive, bool bMakeWhitespace) const
{
	PROFILE_FUNC();

	static bool bUseFloats = false;

	if (bActive)
		szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT")));
	else
		szString.append(CvWString::format(SETCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT")));

	if (!bMakeWhitespace)
	{
		if (bUseFloats)
		{
			float fValue = ((float) iValue) / 10000;
			szString.append(CvWString::format(L"%2.3f " ENDCOLR, fValue));
		}
		else
			szString.append(CvWString::format(L"%05d  " ENDCOLR, iValue/10));
	}
	else
		szString.append(CvWString::format(L"		   " ENDCOLR));
}

void CvGameTextMgr::setCityBarHelp(CvWStringBuffer &szString, CvCity* pCity)
{
	PROFILE_FUNC();

	CvWString szTempBuffer;
	CvWString szTempBuffer2;
	bool bFirst;
	int iFoodDifference;
	int iProductionDiffNoFood;
	int iProductionDiffJustFood;
	int iRate;
	int iI;
	// BUG - Base Production and Commerce - start
	bool bBaseValues = (gDLL->ctrlKey() && getBugOptionBOOL("CityBar__BaseValues", true, "BUG_CITYBAR_BASE_VALUES"));
	// BUG - Base Production and Commerce - end

	FAssert(pCity->isInViewport());

	iFoodDifference = pCity->foodDifference();

	szString.append(pCity->getName());

	// Globally-unique city reference -- the same "<PP>-<id>" snowflake the HTTP API emits as globalId (owner
	// zero-padded to 2 digits). Shown on the city-bar hover so a value can be cross-referenced to an API read.
	szTempBuffer.Format(L" [%02d-%d]", (int)pCity->getOwner(), pCity->getID());
	szString.append(szTempBuffer);

	// BUG - Health - start
	if (getBugOptionBOOL("CityBar__Health", true, "BUG_CITYBAR_HEALTH"))
	{
		iRate = pCity->netHealth();   // ÷100: verdicts ×100
		if (iRate > 0)
		{
			szTempBuffer.Format(L", %d %c", iRate, gDLL->getSymbolID(HEALTHY_CHAR));
			szString.append(szTempBuffer);
		}
		else if (iRate < 0)
		{
			szTempBuffer.Format(L", %d %c", -iRate, gDLL->getSymbolID(UNHEALTHY_CHAR));
			szString.append(szTempBuffer);
		}
	}
	// BUG - Health - end

	// BUG - Happiness - start
	if (getBugOptionBOOL("CityBar__Happiness", true, "BUG_CITYBAR_HAPPINESS"))
	{
		if (pCity->isDisorder())
		{
			int iAngryPop = pCity->angryPopulation();
			if (iAngryPop > 0)
			{
				szTempBuffer.Format(L", %d %c", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR));
				szString.append(szTempBuffer);
			}
		}
		else
		{
			iRate = pCity->netHappiness();   // ÷100: verdicts ×100
			if (iRate > 0)
			{
				szTempBuffer.Format(L", %d %c", iRate, gDLL->getSymbolID(HAPPY_CHAR));
				szString.append(szTempBuffer);
			}
			else if (iRate < 0)
			{
				szTempBuffer.Format(L", %d %c", -iRate, gDLL->getSymbolID(UNHAPPY_CHAR));
				szString.append(szTempBuffer);
			}
		}
	}
	// BUG - Happiness - end

	// BUG - Hurry Anger Turns - start
	if (getBugOptionBOOL("CityBar__HurryAnger", true, "BUG_CITYBAR_HURRY_ANGER") && pCity->getOwner() == GC.getGame().getActivePlayer())
	{
		iRate = pCity->getHurryAngerTimer();
		if (iRate > 0)
		{
			int iPop = ((iRate - 1) / pCity->flatHurryAngerLength() + 1) * GC.getDefineINT("HURRY_POP_ANGER");
			szTempBuffer.Format(L" (%d %c %d)", iPop, gDLL->getSymbolID(ANGRY_POP_CHAR), iRate);
			szString.append(szTempBuffer);
		}
	}
	// BUG - Anger Anger Turns - end

	// BUG - Draft Anger Turns - start
	if (getBugOptionBOOL("CityBar__DraftAnger", true, "BUG_CITYBAR_DRAFT_ANGER") && pCity->getOwner() == GC.getGame().getActivePlayer())
	{
		iRate = pCity->getConscriptAngerTimer();
		if (iRate > 0)
		{
			int iPop = ((iRate - 1) / pCity->flatConscriptAngerLength() + 1) * GC.getDefineINT("CONSCRIPT_POP_ANGER");
			szTempBuffer.Format(L" (%d %c %d)", iPop, gDLL->getSymbolID(CITIZEN_CHAR), iRate);
			szString.append(szTempBuffer);
		}
	}
	// BUG - Draft Anger Turns - end

	// BUG - Food Assist - start
	if ((iFoodDifference != 0 || !pCity->isFoodProduction()) && getBugOptionBOOL("CityBar__FoodAssist", true, "BUG_CITYBAR_FOOD_ASSIST"))
	{
		if (iFoodDifference > 0)
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_GROW", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), pCity->getFoodTurnsLeft()));
		}
		else if (iFoodDifference == 0)
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_STAGNATE", pCity->getFood(), pCity->growthThreshold()));
		}
		else if (pCity->getFood() + iFoodDifference >= 0)
		{
			int iTurnsToStarve = pCity->getFood() / -iFoodDifference + 1;
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_SHRINK", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), iTurnsToStarve));
		}
		else
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_STARVE", iFoodDifference, pCity->getFood(), pCity->growthThreshold()));
		}
	}
	else
	{
		// unchanged
		if (iFoodDifference <= 0)
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GROWTH", pCity->getFood(), pCity->growthThreshold()));
		}
		else
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_GROWTH", iFoodDifference, pCity->getFood(), pCity->growthThreshold(), pCity->getFoodTurnsLeft()));
		}
	}
	// BUG - Food Assist - end

	if (pCity->getProductionNeeded() != MAX_INT)
	{
		// BUG - Base Production - start
		int iBaseProductionDiffNoFood;
		if (bBaseValues)
		{
			// The city's REALIZED production in ONE read: the cascade folds the worked-plot Σ, the specialists
			// and the flat tier itself (modifier.md §2a), so the hand-assembled tiers are gone. ÷100 at the
			// reader -- the cascade carries ×100 natively ([DEC-fixedpoint-x100]).
			int aiRealizedYields[NUM_YIELD_TYPES];
			pCity->getYields(aiRealizedYields);
			iBaseProductionDiffNoFood = aiRealizedYields[YIELD_PRODUCTION] / 100;
		}
		else
		{
			iBaseProductionDiffNoFood = pCity->getCurrentProductionDifference(ProductionCalc::None);
		}
		// BUG - Base Production - end

		iProductionDiffNoFood = pCity->getCurrentProductionDifference(ProductionCalc::Overflow);
		iProductionDiffJustFood = (pCity->getCurrentProductionDifference(ProductionCalc::FoodProduction | ProductionCalc::Overflow) - iProductionDiffNoFood);

		if (iProductionDiffJustFood > 0)
		{
			// BUG - Base Production - start
			if ((iProductionDiffNoFood != iBaseProductionDiffNoFood) && getBugOptionBOOL("CityBar__BaseProduction", true, "BUG_CITYBAR_BASE_PRODUCTION"))
			{
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION_WITH_BASE", iProductionDiffJustFood, iProductionDiffNoFood, pCity->getProductionName(), pCity->getProductionProgress(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft(), iBaseProductionDiffNoFood));
			}
			else
			{
				// unchanged
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_FOOD_HAMMER_PRODUCTION", iProductionDiffJustFood, iProductionDiffNoFood, pCity->getProductionName(), pCity->getProductionProgress(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft()));
			}
			// BUG - Base Production - end
		}
		else if (iProductionDiffNoFood > 0)
		{
			// BUG - Base Production - start
			if ((iProductionDiffNoFood != iBaseProductionDiffNoFood) && getBugOptionBOOL("CityBar__BaseProduction", true, "BUG_CITYBAR_BASE_PRODUCTION"))
			{
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_HAMMER_PRODUCTION_WITH_BASE", iProductionDiffNoFood, pCity->getProductionName(), pCity->getProductionProgress(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft(), iBaseProductionDiffNoFood));
			}
			else
			{
				// unchanged
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_HAMMER_PRODUCTION", iProductionDiffNoFood, pCity->getProductionName(), pCity->getProductionProgress(), pCity->getProductionNeeded(), pCity->getProductionTurnsLeft()));
			}
			// BUG - Base Production - end
		}
		else
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_PRODUCTION", pCity->getProductionName(), pCity->getProductionProgress(), pCity->getProductionNeeded()));
		}

		// BUG - Building Actual Effects - start
		if (pCity->getOwner() == GC.getGame().getActivePlayer() && getBugOptionBOOL("CityBar__BuildingActualEffects", true, "BUG_CITYBAR_BUILDING_ACTUAL_EFFECTS"))
		{
			if (pCity->isProductionBuilding())
			{
				BuildingTypes eBuilding = pCity->getProductionBuilding();
				CvWString szStart;

				szStart.Format(NEWLINE L"<img=%S size=24></img>", GC.getBuildingInfo(eBuilding).getButton());
				setBuildingActualEffects(szString, szStart, eBuilding, pCity, false);
			}
		}
	// BUG - Building Actual Effects - end
	}
	// BUG - Base Production - start
	else if (getBugOptionBOOL("CityBar__BaseProduction", true, "BUG_CITYBAR_BASE_PRODUCTION"))
	{
		int iOverflow = pCity->getOverflowProduction();
		int iBaseProductionDiffNoFood;
		if (bBaseValues)
		{
			// The city's REALIZED production in ONE read: the cascade folds the worked-plot Σ, the specialists
			// and the flat tier itself (modifier.md §2a), so the hand-assembled tiers are gone. ÷100 at the
			// reader -- the cascade carries ×100 natively ([DEC-fixedpoint-x100]).
			int aiRealizedYields[NUM_YIELD_TYPES];
			pCity->getYields(aiRealizedYields);
			iBaseProductionDiffNoFood = aiRealizedYields[YIELD_PRODUCTION] / 100;
		}
		else
		{
			iBaseProductionDiffNoFood = pCity->getCurrentProductionDifference(ProductionCalc::Overflow);
		}
		if (iOverflow > 0 || iBaseProductionDiffNoFood > 0)
		{
			if (iOverflow > 0)
			{
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_BASE_PRODUCTION_WITH_OVERFLOW", iOverflow, iBaseProductionDiffNoFood));
			}
			else
			{
				szString.append(gDLL->getText("TXT_KEY_CITY_BAR_BASE_PRODUCTION", iBaseProductionDiffNoFood));
			}
		}
	}
	// BUG - Base Production - end

	// BUG - Hurry Assist - start
	if (getBugOptionBOOL("CityBar__HurryAssist", true, "BUG_CITYBAR_HURRY_ASSIST") && pCity->getOwner() == GC.getGame().getActivePlayer())
	{
		bool bFirstHurry = true;
		for (iI = 0; iI < GC.getNumHurryInfos(); iI++)
		{
			if (pCity->canHurry((HurryTypes)iI))
			{
				if (bFirstHurry)
				{
					szString.append(NEWLINE);
					szString.append("Hurry:");
					bFirstHurry = false;
				}
				bFirst = true;
				szString.append(L" (");
				const int iPopulation = pCity->hurryPopulation((HurryTypes)iI);
				if (iPopulation > 0)
				{
					szTempBuffer.Format(L"%d %c", -iPopulation, gDLL->getSymbolID(CITIZEN_CHAR));
					setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
				const int64_t iGold = pCity->getHurryGold((HurryTypes)iI);
				if (iGold > 0)
				{
					szTempBuffer.Format(L"%I64d %c", -iGold, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
					setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
					bFirst = false;
				}
				int iOverflowProduction = 0;
				int iOverflowGold = 0;
				if (pCity->hurryOverflow((HurryTypes)iI, &iOverflowProduction, &iOverflowGold, getBugOptionBOOL("CityBar__HurryAssistIncludeCurrent", false, "BUG_CITYBAR_HURRY_ASSIST_INCLUDE_CURRENT")))
				{
					if (iOverflowProduction > 0)
					{
						szTempBuffer.Format(L"%d %c", iOverflowProduction, GC.getYieldInfo(YIELD_PRODUCTION).getChar());
						setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
					if (iOverflowGold > 0)
					{
						szTempBuffer.Format(L"%d %c", iOverflowGold, GC.getCommerceInfo(COMMERCE_GOLD).getChar());
						setListHelp(szString, NULL, szTempBuffer, L", ", bFirst);
						bFirst = false;
					}
				}
				szString.append(L")");
			}
		}
	}
	// BUG - Hurry Assist - end

	// BUG - Trade Detail - start
	if (getBugOptionBOOL("CityBar__TradeDetail", true, "BUG_CITYBAR_TRADE_DETAIL"))
	{
		int iTotalTrade = 0;
		int iDomesticTrade = 0;
		int iDomesticRoutes = 0;
		int iForeignTrade = 0;
		int iForeignRoutes = 0;

		// BUG - Fractional Trade Routes - start
#ifdef _MOD_FRACTRADE
		bool bFractions = true;
#else
		bool bFractions = bBaseValues;
#endif
		// BUG - Fractional Trade Routes - end

		pCity->calculateTradeTotals(YIELD_COMMERCE, iDomesticTrade, iDomesticRoutes, iForeignTrade, iForeignRoutes, NO_PLAYER, !bFractions, bBaseValues);
		iTotalTrade = iDomesticTrade + iForeignTrade;

		bFirst = true;
		if (iTotalTrade != 0)
		{
			if (bFractions)
			{
				szTempBuffer.Format(L"%c: %d.%02d %c", gDLL->getSymbolID(TRADE_CHAR), iTotalTrade / 100, iTotalTrade % 100, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			else
			{
				szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(TRADE_CHAR), iTotalTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
		if (iDomesticTrade != 0)
		{
			if (bFractions)
			{
				szTempBuffer.Format(L"%c: %d.%02d %c", gDLL->getSymbolID(STAR_CHAR), iDomesticTrade / 100, iDomesticTrade % 100, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			else
			{
				szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(STAR_CHAR), iDomesticTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
		if (iForeignTrade != 0)
		{
			if (bFractions)
			{
				szTempBuffer.Format(L"%c: %d.%02d %c", gDLL->getSymbolID(SILVER_STAR_CHAR), iForeignTrade / 100, iForeignTrade % 100, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			else
			{
				szTempBuffer.Format(L"%c: %d %c", gDLL->getSymbolID(SILVER_STAR_CHAR), iForeignTrade, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			}
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	// BUG - Trade Detail - end

	bFirst = true;

	// BUG - Commerce - start
	if (getBugOptionBOOL("CityBar__Commerce", true, "BUG_CITYBAR_COMMERCE"))
	{
		// The city's REALIZED commerce yield in ONE read (see the production sites above); ÷100 at the reader.
		// ⚠ The base-vs-realized branch is GONE: both halves answered the same value once the base tier stopped
		// being a separate accessor, so the flag no longer distinguishes anything here.
		int aiRealizedYields[NUM_YIELD_TYPES];
		pCity->getYields(aiRealizedYields);
		iRate = aiRealizedYields[YIELD_COMMERCE] / 100;
		if (iRate != 0)
		{
			szTempBuffer.Format(L"%d %c", iRate, GC.getYieldInfo(YIELD_COMMERCE).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	// BUG - Commerce - end

	for (iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
	{
		// BUG - Base Values - start
		// The base-vs-realized branch is GONE: both halves answer the same value once the base tier stops
		// being a separate accessor, so the flag no longer distinguishes anything here.
		int aiCityCommerces[NUM_COMMERCE_TYPES];
		pCity->getCommerces(aiCityCommerces);
		iRate = aiCityCommerces[(CommerceTypes)iI];
		// BUG - Base Values - end

		if (iRate != 0)
		{
			szTempBuffer.Format(L"%d.%02d %c", iRate/100, iRate%100, GC.getCommerceInfo((CommerceTypes)iI).getChar());
			setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}

	// BUG - Base Values - start
	if (bBaseValues)
	{
		iRate = pCity->getBaseGreatPeopleRate();
	}
	else
	{
		// unchanged
		iRate = pCity->getGreatPeopleRate();
	}
	// BUG - Base Values - end

	if (iRate != 0)
	{
		szTempBuffer.Format(L"%d%c", iRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR));
		setListHelp(szString, NEWLINE, szTempBuffer, L", ", bFirst);
		bFirst = false;
	}

	if (!bFirst)
	{
		szString.append(gDLL->getText("TXT_KEY_PER_TURN"));
	}

	szString.append(NEWLINE);
	szString.append(gDLL->getText("INTERFACE_CITY_MAINTENANCE"));

	int iMaintenance = (int)pCity->getMaintenanceTimes100();

	szString.append(CvWString::format(L" -%d.%02d %c", iMaintenance/100, iMaintenance%100, GC.getCommerceInfo(COMMERCE_GOLD).getChar()));

	{
		const bool bBuildingIconBUG = getBugOptionBOOL("CityBar__BuildingIcons", true, "BUG_CITYBAR_BUILDING_ICONS");
		bFirst = true;
		foreach_(const BuildingTypes eType, pCity->getHasBuildings())
		{
			if (isWorldWonder(eType) || isNationalWonder(eType))
			{
				if (bBuildingIconBUG)
				{
					if (bFirst)
					{
						szString.append(NEWLINE);
						bFirst = false;
					}
					szTempBuffer.Format(L"<img=%S size=24></img>", GC.getBuildingInfo(eType).getButton());
					szString.append(szTempBuffer);
				}
				else
				{
					setListHelp(szString, NEWLINE, GC.getBuildingInfo(eType).getDescription(), L", ", bFirst);
					bFirst = false;
				}
			}
		}
	}

	{
		const int iThreshold = pCity->getCultureThreshold();
		if (iThreshold > 0)
		{
			szString.append(
				gDLL->getText(
					"TXT_KEY_CITY_BAR_CULTURE",
					pCity->getCulture(pCity->getOwner()), iThreshold,
					GC.getCultureLevelInfo(pCity->getCultureLevel()).getTextKeyWide(),
					GC.getCultureLevelInfo(pCity->getCultureLevel()).getLevel()
				)
			);
			int aiCityCommerces[NUM_COMMERCE_TYPES];
			pCity->getCommerces(aiCityCommerces);
			const int iCultureRate = aiCityCommerces[COMMERCE_CULTURE];
			if (iCultureRate > 0)
			{
				// all values are *100
				const int iCultureLeft = 100 * iThreshold - pCity->getCultureTimes100(pCity->getOwner());
				const int iCultureTurns = (iCultureLeft + iCultureRate - 1) / iCultureRate;
				szString.append(L" ");
				szString.append(gDLL->getText("INTERFACE_CITY_TURNS", iCultureTurns));
			}
		}
		else
		{
			szString.append(
				gDLL->getText(
					"TXT_KEY_CITY_BAR_CULTURE_MAX",
					pCity->getCulture(pCity->getOwner()),
					GC.getCultureLevelInfo(pCity->getCultureLevel()).getTextKeyWide(),
					GC.getCultureLevelInfo(pCity->getCultureLevel()).getLevel()
				)
			);
		}
	}

	// BUG - Great Person Turns - start
	int iGppRate = pCity->getGreatPeopleRate();
	if (iGppRate > 0 && getBugOptionBOOL("CityBar__GreatPersonTurns", true, "BUG_CITYBAR_GREAT_PERSON_TURNS"))
	{
		int iGpp = pCity->getGreatPeopleProgress();
		int iGppTotal = GET_PLAYER(pCity->getOwner()).greatPeopleThresholdNonMilitary();
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GREAT_PEOPLE", iGpp, iGppTotal));
		int iGppLeft = iGppTotal - iGpp;
		int iGppTurns = (iGppLeft + iGppRate - 1) / iGppRate;
		szString.append(L" ");
		szString.append(gDLL->getText("INTERFACE_CITY_TURNS", iGppTurns));
	}
	else
	{
		// unchanged
		if (pCity->getGreatPeopleProgress() > 0)
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_GREAT_PEOPLE", pCity->getGreatPeopleProgress(), GET_PLAYER(pCity->getOwner()).greatPeopleThresholdNonMilitary()));
		}
	}
	// BUG - Great Person Turns - end

	// BUG - Specialists - start
	if (getBugOptionBOOL("CityBar__Specialists", true, "BUG_CITYBAR_SPECIALISTS"))
	{
		// regular specialists
		bFirst = true;
		for (int iI = 0; iI < GC.getNumSpecialistInfos(); ++iI)
		{
			const int iCount = pCity->getSpecialistCount((SpecialistTypes)iI);
			if (iCount > 0)
			{
				if (bFirst)
				{
					szString.append(NEWLINE);
				}
				const CvSpecialistInfo& kSpecialistInfo = GC.getSpecialistInfo((SpecialistTypes)iI);
				//for (int iJ = 0; iJ < iCount; ++iJ)
				//{
				if (!bFirst)
				{
					szString.append(gDLL->getText("TXT_KEY_COMMA", iCount));
				}
				szTempBuffer.Format(L"<img=%S size=24></img>", kSpecialistInfo.getButton());
				szString.append(szTempBuffer);
				szString.append(gDLL->getText("TXT_KEY_INTERFACE_CITY_BAR_SPECIALIST_ADDENDUM", iCount));
				if (bFirst)
				{
					bFirst = false;
				}

				//}
			}
		}

		// free specialists (ToA, GL) and settled great people
		bFirst = true;
		for (int iI = 0; iI < GC.getNumSpecialistInfos(); ++iI)
		{
			const int iCount = pCity->getFreeSpecialistCount((SpecialistTypes)iI);
			if (iCount > 0)
			{
				if (bFirst)
				{
					szString.append(NEWLINE);
				}
				const CvSpecialistInfo& kSpecialistInfo = GC.getSpecialistInfo((SpecialistTypes)iI);
				//for (int iJ = 0; iJ < iCount; ++iJ)
				//{
				if (!bFirst)
				{
					szString.append(gDLL->getText("TXT_KEY_COMMA", iCount));
				}
				szTempBuffer.Format(L"<img=%S size=24></img>", kSpecialistInfo.getButton());
				szString.append(szTempBuffer);
				szString.append(gDLL->getText("TXT_KEY_INTERFACE_CITY_BAR_SPECIALIST_ADDENDUM", iCount));
				if (bFirst)
				{
					bFirst = false;
				}

				//}
			}
		}
	}
	// BUG - Specialists - end

	int iNumUnits = pCity->plot()->countNumAirUnits(GC.getGame().getActiveTeam());
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		iNumUnits = pCity->plot()->countNumAirUnitCargoVolume(GC.getGame().getActiveTeam());
		if (pCity->getSMAirUnitCapacity(GC.getGame().getActiveTeam()) > 0 && iNumUnits > 0)
		{
			szString.append(NEWLINE);
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getSMAirUnitCapacity(GC.getGame().getActiveTeam())));
		}
	}
	else if (pCity->getAirUnitCapacity(GC.getGame().getActiveTeam()) > 0 && iNumUnits > 0)
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getAirUnitCapacity(GC.getGame().getActiveTeam())));
	}

	// BUG - Revolt Chance - start
	if (getBugOptionBOOL("CityBar__RevoltChance", true, "BUG_CITYBAR_REVOLT_CHANCE"))
	{
		const PlayerTypes eCulturalOwner = pCity->plot()->calculateCulturalOwner();

		if (eCulturalOwner != NO_PLAYER)
		{
			if (GET_PLAYER(eCulturalOwner).getTeam() != pCity->getTeam())
			{
				int iNetRevoltRisk100 = pCity->netRevoltRisk(eCulturalOwner);
				int iOriginal100 = pCity->baseRevoltRisk(eCulturalOwner);
				int iSpeedAdjustment = GC.getREVOLT_TEST_PROB() * 100 /
					CvGameSpeedScale::speedPercent();
				int iGarrison = pCity->unitRevoltRiskModifier(eCulturalOwner);

				if (iNetRevoltRisk100 > 0)
				{
					szString.append(NEWLINE);
					szString.append(gDLL->getText("TXT_KEY_MISC_CHANCE_OF_REVOLT",
						CvWString::format(L"" SETCOLR L"%.2f%%" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), ((float)iNetRevoltRisk100*iSpeedAdjustment)/10000).GetCString(),
						CvWString::format(L"" SETCOLR L"%.1f%%" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), (float)iOriginal100/100).GetCString(),
						CvWString::format(L"" SETCOLR L"%d%%" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iSpeedAdjustment).GetCString(),
						CvWString::format(L"" SETCOLR L"%d%%" ENDCOLR, TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), iGarrison).GetCString()
					));
				}
			}
		}
	}
	// BUG - Revolt Chance - end

	// Citybar revolution info
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		szString.append(NEWLINE);
		szString.append(L"<img=Art/Interface/Buttons/revbtn.dds size=23></img>");
		szString.append(CvWString::format(L":%d", pCity->getRevolutionIndex()));
		szString.append(NEWLINE);
	}

	pCity->getProperties()->buildDisplayString(szString);

	// BUG - Hide UI Instructions - start
	if (!getBugOptionBOOL("CityBar__HideInstructions", true, "BUG_CITYBAR_HIDE_INSTRUCTIONS"))
	{
		if (getBugOptionBOOL("CityBar__BaseValues", true, "BUG_CITYBAR_BASE_VALUES"))
		{
			szString.append(gDLL->getText("TXT_KEY_CITY_BAR_CTRL_BASE_VALUES"));
		}
		// unchanged
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT", pCity->getNameKey()));
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT_CTRL"));
		szString.append(gDLL->getText("TXT_KEY_CITY_BAR_SELECT_ALT"));
	}
	// BUG - Hide UI Instructions - end
}

void CvGameTextMgr::parseBuildUp(CvWStringBuffer &szHelpString, PromotionLineTypes ePromotionLine, CivilizationTypes eCivilization)
{
	// TB Traits
	PROFILE_FUNC();
	CvWString szTempBuffer;
	CvWString szText;

	// Trait Name
	szText = GC.getPromotionLineInfo(ePromotionLine).getDescription();
	szTempBuffer.Format(NEWLINE SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), szText.GetCString());
	szHelpString.append(szTempBuffer);

	if (!CvWString(GC.getPromotionLineInfo(ePromotionLine).getHelp()).empty())
	{
		szHelpString.append(NEWLINE);
		szHelpString.append(GC.getPromotionLineInfo(ePromotionLine).getHelp());
	}
}

void CvGameTextMgr::parseTraits(CvWStringBuffer &szHelpString, TraitTypes eTrait, bool bDawnOfMan, bool bEffectsOnly)
{
	PROFILE_FUNC();

	const CvTraitInfo& kTrait = GC.getTraitInfo(eTrait);

	if (!bEffectsOnly)
	{
		// Trait Name
		CvWString name;
		if (bDawnOfMan)
		{
			name.Format(L"%s", kTrait.getDescription());
		}
		else name.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_ALT_HIGHLIGHT_TEXT"), kTrait.getDescription());

		szHelpString.append(name);
	}
	if (bDawnOfMan) return;

	if (!CvWString(kTrait.getHelp()).empty())
	{
		szHelpString.append(kTrait.getHelp());
	}
	//Negative Trait denotation
	if (kTrait.isNegativeTrait())
	{
		szHelpString.append(NEWLINE);
		szHelpString.append(gDLL->getText("TXT_KEY_TRAITHELP_NEGATIVE"));
	}

	// json.md par.9 `excludes` -- the same-tier traits this one cannot be held alongside.
	const std::vector<int>& aiExcludes = kTrait.getExcludes();
	for (std::vector<int>::const_iterator itExclude = aiExcludes.begin(); itExclude != aiExcludes.end(); ++itExclude)
	{
		szHelpString.append(NEWLINE);
		szHelpString.append(
			gDLL->getText(
				"TXT_KEY_TRAITHELP_DISALLOWED",
				GC.getTraitInfo(static_cast<TraitTypes>(*itExclude)).getTextKeyWide()
			)
		);
	}

	// The EFFECT blocks, in display order. A trait is a SINGLE source, so each family is one block and the
	// composer's whole remaining job is choosing which families appear and in what order -- never how a value
	// reads. Every entry renders itself through the ONE renderer, carrying its own magnitude, unit, target,
	// scope, per-scaler and conditions ([patterns.md] the per-entry TEXT render), so a newly authored family
	// needs no edit here beyond its place in this list.
	static const ModifierFamily aeDisplayFamilies[] =
	{
		// wellbeing
		MODFAM_HAPPINESS, MODFAM_HEALTH,
		// the yield + commerce channels
		MODFAM_FOOD, MODFAM_PRODUCTION, MODFAM_COMMERCE,
		MODFAM_GOLD, MODFAM_RESEARCH, MODFAM_CULTURE, MODFAM_ESPIONAGE,
		MODFAM_EXTRA_YIELD_THRESHOLD, MODFAM_LESS_YIELD_THRESHOLD,
		// city economy
		MODFAM_GROWTH, MODFAM_MAINTENANCE, MODFAM_UPKEEP, MODFAM_HURRY, MODFAM_COSTS,
		MODFAM_TRADE_ROUTES, MODFAM_DEFENSE, MODFAM_ESPIONAGE_DEFENSE,
		// people
		MODFAM_GREAT_PEOPLE_RATE, MODFAM_GREAT_GENERAL_RATE, MODFAM_FREE_SPECIALISTS,
		MODFAM_EXPERIENCE, MODFAM_CONSCRIPT,
		// work + build rates
		MODFAM_WORK_RATE, MODFAM_IMPROVEMENT_UPGRADE_RATE, MODFAM_BUILD_RATE, MODFAM_RESEARCH_RATE,
		// empire-level
		MODFAM_DURATIONS, MODFAM_GOLDEN_AGE, MODFAM_DIPLOMACY, MODFAM_STATE_RELIGION, MODFAM_REVOLUTION,
		// unit plane
		MODFAM_CAPTURE, MODFAM_AIR, MODFAM_CARGO, MODFAM_RANGE,
		// the property plane (one channel per PROPERTY_* info)
		MODFAM_PROPERTY
	};
	for (size_t iFamily = 0; iFamily < sizeof(aeDisplayFamilies) / sizeof(aeDisplayFamilies[0]); ++iFamily)
	{
		appendEntryLines(szHelpString, kTrait, aeDisplayFamilies[iFamily]);
	}
}



void CvGameTextMgr::parseLeaderTraits(CvWStringBuffer &szHelpString, LeaderHeadTypes eLeader, CivilizationTypes eCivilization, bool bDawnOfMan, bool bCivilopediaText)
{
	PROFILE_FUNC();

	// Build help string
	if (eLeader != NO_LEADER)
	{
		bool bFirst = true;
		if (!bDawnOfMan && !bCivilopediaText)
		{
			szHelpString.append(CvWString::format(SETCOLR L"%s" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getLeaderHeadInfo(eLeader).getDescription()));
			bFirst = false;
		}
		FAssertMsg(
			GC.getNumTraitInfos() > 0,
			"GC.getNumTraitInfos() is less than or equal to zero but is expected to be larger than zero in CvSimpleCivPicker::setLeaderText"
		);


		// The leader's authored assignment for whichever trait set is ACTIVE (CvTraitSelection owns the
		// GAMEOPTION_LEADER_COMPLEX_TRAITS composition), rendered in order.
		foreach_(const int iTrait, CvTraitSelection::leaderTraits(GC.getLeaderHeadInfo(eLeader)))
		{
			const TraitTypes eTrait = (TraitTypes)iTrait;
			if (!CvTraitSelection::isSelectable(GC.getTraitInfo(eTrait), true))
			{
				continue;
			}
			if (!bFirst)
			{
				if (bDawnOfMan)
					szHelpString.append(L", ");
				else if (bCivilopediaText)
					szHelpString.append(L"\n\n");
				else szHelpString.append(L"\n");
			}
			else bFirst = false;

			parseTraits(szHelpString, eTrait, bDawnOfMan);
		}
	}
	else //	Random leader
	{
		szHelpString.append(CvWString::format(SETCOLR L"%s" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), gDLL->getText("TXT_KEY_TRAITHELP_PLAYER_UNKNOWN").c_str()));
	}
}


void CvGameTextMgr::parseLeaderShortTraits(CvWStringBuffer &szHelpString, LeaderHeadTypes eLeader)
{
	PROFILE_FUNC();

	//	Build help string
	if (eLeader != NO_LEADER)
	{
		FAssertMsg((GC.getNumTraitInfos() > 0),
			"GC.getNumTraitInfos() is less than or equal to zero but is expected to be larger than zero in CvSimpleCivPicker::setLeaderText");

		bool bFirst = true;

		// The leader's authored assignment for whichever trait set is ACTIVE (CvTraitSelection owns the
		// GAMEOPTION_LEADER_COMPLEX_TRAITS composition).
		foreach_(const int iTrait, CvTraitSelection::leaderTraits(GC.getLeaderHeadInfo(eLeader)))
		{
			const TraitTypes eTrait = (TraitTypes)iTrait;
			if (CvTraitSelection::isSelectable(GC.getTraitInfo(eTrait), true))
			{
				if (!bFirst)
				{
					szHelpString.append(L"/");
				}
				szHelpString.append(gDLL->getText(GC.getTraitInfo(eTrait).getShortDescriptionKey()));
				bFirst = false;
			}
		}
	}
	else
	{
		//	Random leader
		szHelpString.append(CvWString("???/???"));
	}

	//	return szHelpString;
}

//
// Build Civilization Info Help Text
//
void CvGameTextMgr::parseCivInfos(CvWStringBuffer &szInfoText, CivilizationTypes eCivilization, bool bDawnOfMan, bool bLinks)
{
}


// BUG - Specialist Actual Effects - start
void CvGameTextMgr::parseSpecialistHelp(CvWStringBuffer &szHelpString, SpecialistTypes eSpecialist, CvCity* pCity, bool bCivilopediaText)
{
	parseSpecialistHelpActual(szHelpString, eSpecialist, pCity, bCivilopediaText, 0);
}

void CvGameTextMgr::parseSpecialistHelpActual(CvWStringBuffer &szHelpString, SpecialistTypes eSpecialist, CvCity* pCity, bool bCivilopediaText, int iChange)
// BUG - Specialist Actual Effects - end
{
}


void CvGameTextMgr::parseFreeSpecialistHelp(CvWStringBuffer &szHelpString, const CvCity& kCity)
{
}


//
// Promotion Help
//
void CvGameTextMgr::parsePromotionHelp(CvWStringBuffer &szBuffer, PromotionTypes ePromotion, const wchar_t* pcNewline)
{
	parsePromotionHelpInternal(szBuffer, ePromotion, pcNewline, true);
}

void CvGameTextMgr::parsePromotionHelpInternal(CvWStringBuffer &szBuffer, PromotionTypes ePromotion, const wchar_t* pcNewline, bool bAccrueLines)
{
}

//	Function:			parseCivicInfo()
//	Description:	Will parse the civic info help
//	Parameters:		szHelpText -- the text to put it into
//								civicInfo - what to parse
//	Returns:			nothing
void CvGameTextMgr::parseCivicInfo(CvWStringBuffer &szHelpText, CivicTypes eCivic, bool bCivilopediaText, bool bPlayerContext, bool bSkipName)
{
}


void CvGameTextMgr::setTechHelp(CvWStringBuffer &szBuffer, TechTypes eTech, bool bCivilopediaText, bool bPlayerContext, bool bStrategyText, bool bTreeInfo, TechTypes eFromTech)
{
}


void CvGameTextMgr::setBasicUnitHelp(CvWStringBuffer &szBuffer, UnitTypes eUnit, bool bCivilopediaText)
{
}

void CvGameTextMgr::setBasicUnitHelpWithCity(CvWStringBuffer &szBuffer, UnitTypes eUnit, bool bCivilopediaText, CvCity* pCity, bool bConscript, bool bTBUnitView1, bool bTBUnitView2, bool bTBUnitView3)
{
}

// BUG - Starting Experience - start
/*
 * Appends the starting experience and number of promotions the given unit will have
 * when trained or conscripted in the given city.
 */
void CvGameTextMgr::setUnitExperienceHelp(CvWStringBuffer &szBuffer, CvWString szStart, UnitTypes eUnit, CvCity* pCity, bool bConscript)
{
	if (GC.getUnitInfo(eUnit).canAcquireExperience())
	{
		const int iExperience = pCity->getProductionExperience(eUnit) / (bConscript ? 2 : 1);

		if (iExperience > 0)
		{
			szBuffer.append(szStart);
			if (bConscript)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_EXPERIENCE_DRAFT", iExperience));
			}
			else szBuffer.append(gDLL->getText("TXT_KEY_MISC_EXPERIENCE", iExperience));

			const int iLevel = calculateLevel(iExperience, pCity->getOwner());
			if (iLevel > 1)
			{
				szBuffer.append(L", ");
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_PROMOTIONS", iLevel - 1));
			}
		}
	}
}
// BUG - Starting Experience - end


void CvGameTextMgr::setUnitHelp(CvWStringBuffer &szBuffer, UnitTypes eUnit, bool bCivilopediaText, bool bStrategyText, bool bTechChooserText, CvCity* pCity)
{
}

// BUG - Building Actual Effects - start
/*
 * Adds the actual effects of adding a building to the city.
 */
void CvGameTextMgr::setBuildingActualEffects(CvWStringBuffer &szBuffer, const CvWString& szStart, BuildingTypes eBuilding, const CvCity* pCity, bool bNewLine)
{
	PROFILE_EXTRA_FUNC();
	if (pCity)
	{
		bool bStarted = false;

		// Defense
		int iDefense = pCity->getAdditionalDefenseByBuilding(eBuilding);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iDefense, gDLL->getSymbolID(DEFENSE_CHAR), true, bNewLine, bStarted);

		// Happiness
		int iGood = 0;
		int iBad = 0;
		int iAngryPop = 0;
		pCity->getAdditionalHappinessByBuilding(eBuilding, iGood, iBad, iAngryPop);
		bStarted = setResumableGoodBadChangeHelp(szBuffer, szStart, L": ", L"", iGood, gDLL->getSymbolID(HAPPY_CHAR), iBad, gDLL->getSymbolID(UNHAPPY_CHAR), false, bNewLine, bStarted);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR), false, bNewLine, bStarted);

		// Health
		iGood = 0;
		iBad = 0;
		int iSpoiledFood = 0;
		int iStarvation = 0;
		pCity->getAdditionalHealthByBuilding(eBuilding, iGood, iBad, iSpoiledFood, iStarvation);
		bStarted = setResumableGoodBadChangeHelp(szBuffer, szStart, L": ", L"", iGood, gDLL->getSymbolID(HEALTHY_CHAR), iBad, gDLL->getSymbolID(UNHEALTHY_CHAR), false, bNewLine, bStarted);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iSpoiledFood, gDLL->getSymbolID(EATEN_FOOD_CHAR), false, bNewLine, bStarted);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iStarvation, gDLL->getSymbolID(BAD_FOOD_CHAR), false, bNewLine, bStarted);

		// Yield
		int aiYields[NUM_YIELD_TYPES];
		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
		{
			aiYields[iI] = pCity->getAdditionalYieldByBuilding((YieldTypes)iI, eBuilding);
		}

		int iCommerce = aiYields[YIELD_COMMERCE];
		aiYields[YIELD_COMMERCE] = 0;

		bStarted = setResumableYieldChangeHelp(szBuffer, szStart, L": ", L"", aiYields, false, bNewLine, bStarted);

		// Commerce
		int aiCommerces[NUM_COMMERCE_TYPES];
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
		{
			aiCommerces[iI] = pCity->getAdditionalCommerceByBuilding((CommerceTypes)iI, eBuilding);

			aiCommerces[iI] += iCommerce * GET_PLAYER(pCity->getOwner()).getCommercePercent((CommerceTypes)iI);
		}

		// Maintenance - add to gold
		aiCommerces[COMMERCE_GOLD] += pCity->getSavedMaintenanceTimes100ByBuilding(eBuilding);
		bStarted = setResumableCommerceTimes100ChangeHelp(szBuffer, szStart, L": ", L"", aiCommerces, bNewLine, bStarted);

		// Great People
		int iGreatPeopleRate = pCity->getAdditionalGreatPeopleRateByBuilding(eBuilding);
		bStarted = setResumableValueChangeHelp(szBuffer, szStart, L": ", L"", iGreatPeopleRate, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), false, bNewLine, bStarted);
	}
}


/*
 * Calls new function below without displaying actual effects.
 */
void CvGameTextMgr::setBuildingHelp(CvWStringBuffer &szBuffer, const BuildingTypes eBuilding, const bool bActual, CvCity* pCity, const bool bCivilopediaText, const bool bStrategyText, const bool bTechChooserText)
{
}

void CvGameTextMgr::setHeritageHelp(CvWStringBuffer &szBuffer, const HeritageTypes eType, CvCity* pCity, const bool bCivilopediaText, const bool bStrategyText, const bool bTechChooserText)
{
}


// #195 Phase 2: render one terrain / feature / improvement "requires ... in city vicinity"
// requirement straight from the unified prerequisite model, replacing four near-identical
// hand-rolled loops. Same TXT keys, separator (AND for REQUIRE_ALL, OR otherwise) and the
// IN_CITY_VICINITY suffix as the code it supersedes.
void CvGameTextMgr::appendVicinityRequirementHelp(CvWStringBuffer& szBuffer, const ConstructRequirement& req)
{
}

// #195 Phase 2: format one GOM (type, id) as a clickable <link>description</link>. Returns
// false for GOM types this renderer does not display (caller skips them).
bool CvGameTextMgr::buildRequirementItemLink(GOMTypes eGOM, int iId, CvWString& szOut) const
{
	switch (eGOM)
	{
	case GOM_BUILDING:    szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getBuildingInfo(static_cast<BuildingTypes>(iId)).getType()).GetCString(), GC.getBuildingInfo(static_cast<BuildingTypes>(iId)).getDescription()); return true;
	case GOM_TECH:        szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getTechInfo(static_cast<TechTypes>(iId)).getType()).GetCString(), GC.getTechInfo(static_cast<TechTypes>(iId)).getDescription()); return true;
	case GOM_BONUS:       szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getBonusInfo(static_cast<BonusTypes>(iId)).getType()).GetCString(), GC.getBonusInfo(static_cast<BonusTypes>(iId)).getDescription()); return true;
	case GOM_RELIGION:    szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getReligionInfo(static_cast<ReligionTypes>(iId)).getType()).GetCString(), GC.getReligionInfo(static_cast<ReligionTypes>(iId)).getDescription()); return true;
	case GOM_CORPORATION: szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getCorporationInfo(static_cast<CorporationTypes>(iId)).getType()).GetCString(), GC.getCorporationInfo(static_cast<CorporationTypes>(iId)).getDescription()); return true;
	case GOM_CIVIC:       szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getCivicInfo(static_cast<CivicTypes>(iId)).getType()).GetCString(), GC.getCivicInfo(static_cast<CivicTypes>(iId)).getDescription()); return true;
	case GOM_TERRAIN:     szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getTerrainInfo(static_cast<TerrainTypes>(iId)).getType()).GetCString(), GC.getTerrainInfo(static_cast<TerrainTypes>(iId)).getDescription()); return true;
	case GOM_FEATURE:     szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getFeatureInfo(static_cast<FeatureTypes>(iId)).getType()).GetCString(), GC.getFeatureInfo(static_cast<FeatureTypes>(iId)).getDescription()); return true;
	case GOM_IMPROVEMENT: szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getImprovementInfo(static_cast<ImprovementTypes>(iId)).getType()).GetCString(), GC.getImprovementInfo(static_cast<ImprovementTypes>(iId)).getDescription()); return true;
	case GOM_HERITAGE:    szOut.Format(L"<link=%s>%s</link>", CvWString(GC.getHeritageInfo(static_cast<HeritageTypes>(iId)).getType()).GetCString(), GC.getHeritageInfo(static_cast<HeritageTypes>(iId)).getDescription()); return true;
	default:              return false;
	}
}

// #195 Phase 2: status-aware renderer for a model requirement. Filters by what the city
// already has via CvGameObject::hasGOM -- the same oracle the construct-condition evaluates
// against -- so the displayed have/need status matches actual constructibility. Renders the
// unmet items as a "Requires: <links>" list (AND for REQUIRE_ALL, OR otherwise). pCity NULL
// (Civilopedia) treats nothing as satisfied, so the full requirement is shown.
void CvGameTextMgr::appendRequirementHelp(CvWStringBuffer& szBuffer, const ConstructRequirement& req, const CvCity* pCity, const char* szRequiresKey)
{
}

// #195 Phase 2: civic requirement renderer. Unlike appendRequirementHelp, civics show EVERY
// listed civic, coloured by whether the active player has it (POSITIVE = have, WARNING = need),
// joined by AND (REQUIRE_ALL) / OR (REQUIRE_ANY) -- the "show-all status" UX the old hand-rolled
// blocks used. Returns whether the requirement is met, for the "requires active civics" note.
bool CvGameTextMgr::appendCivicRequirementHelp(CvWStringBuffer& szBuffer, const ConstructRequirement& req)
{
	return 0;
}

void CvGameTextMgr::buildBuildingRequiresString(CvWStringBuffer& szBuffer, BuildingTypes eBuilding, bool bCivilopediaText, bool bTechChooserText, const CvCity* pCity)
{
}


void CvGameTextMgr::setProjectHelp(CvWStringBuffer &szBuffer, ProjectTypes eProject, bool bCivilopediaText, CvCity* pCity)
{
}


void CvGameTextMgr::setProcessHelp(CvWStringBuffer &szBuffer, ProcessTypes eProcess)
{
	PROFILE_EXTRA_FUNC();
	szBuffer.append(GC.getProcessInfo(eProcess).getDescription());

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
	{
		if (GC.getProcessInfo(eProcess).getProductionToCommerce((CommerceTypes)iI, CASC_SCOPE_CITY) != 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_PROCESS_CONVERTS", GC.getProcessInfo(eProcess).getProductionToCommerce((CommerceTypes)iI, CASC_SCOPE_CITY), GC.getYieldInfo(YIELD_PRODUCTION).getChar(), GC.getCommerceInfo((CommerceTypes) iI).getChar()));
		}
	}
}

// The four wellbeing composers are BLOCKS, not per-source sub-blocks ([patterns.md] the per-entry TEXT
// render): a block is "different sources put together" and choosing/heading them is the text manager's job,
// while the per-source render inside it is never hand-built. So each renders ONE line for the cascade CHANNEL
// (every authored deposit -- buildings, civics, traits, features, bonuses, specialists, corporations, techs)
// and one line per RAW-STATE input the city can still be asked for directly. Per-SOURCE attribution is the
// ORACLE endpoint's job, never the read surface's.
// > Nothing here re-derives a term: the deposits come from the group read and the raw-state inputs from their
// own accessors, so the lines sum toward the realized total by construction rather than by a maintained tally.
void CvGameTextMgr::setBadHealthHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	PROFILE_EXTRA_FUNC();
	int aRealized[NUM_WELLBEING_CHANNELS];
	city.realizedWellbeing(0, aRealized);
	const int iBadHealthTotal = aRealized[WELLBEING_UNHEALTH] / 100;
	if (iBadHealthTotal < 1)
	{
		return;
	}
	int aDeposits[NUM_WELLBEING_CHANNELS];
	city.getWellbeing(aDeposits);
	int iSum = 0;

	int iHealth = aDeposits[WELLBEING_UNHEALTH] / 100;
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_BUILDINGS", iHealth));
		szBuffer.append(NEWLINE);
	}
	iHealth = city.unhealthyPopulation();
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_POP", iHealth));
		szBuffer.append(NEWLINE);
	}
	iHealth = city.getEspionageHealthCounter();
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_ESPIONAGE", iHealth));
		szBuffer.append(NEWLINE);
	}
	iHealth = -std::min(0, city.getExtraHealth() + GET_PLAYER(city.getOwner()).getExtraHealth());
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_BAD_HEALTH_FROM_EVENTS", iHealth));
		szBuffer.append(NEWLINE);
	}
	if (iBadHealthTotal > iSum)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MISC", iBadHealthTotal - iSum));
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(L"-----------------------\n");
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_TOTAL_UNHEALTHY", iBadHealthTotal));
}

void CvGameTextMgr::setGoodHealthHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	PROFILE_EXTRA_FUNC();
	int aRealized[NUM_WELLBEING_CHANNELS];
	city.realizedWellbeing(0, aRealized);
	const int iGoodHealthTotal = aRealized[WELLBEING_HEALTH] / 100;
	if (iGoodHealthTotal < 1)
	{
		return;
	}
	int aDeposits[NUM_WELLBEING_CHANNELS];
	city.getWellbeing(aDeposits);
	int iSum = 0;

	int iHealth = aDeposits[WELLBEING_HEALTH] / 100;
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HEALTH_FROM_BUILDINGS", iHealth));
		szBuffer.append(NEWLINE);
	}
	iHealth = std::max(0, city.getExtraHealth() + GET_PLAYER(city.getOwner()).getExtraHealth());
	if (iHealth > 0)
	{
		iSum += iHealth;
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOOD_HEALTH_FROM_EVENTS", iHealth));
		szBuffer.append(NEWLINE);
	}
	if (iGoodHealthTotal > iSum)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MISC", iGoodHealthTotal - iSum));
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(L"-----------------------\n");
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_TOTAL_HEALTHY", iGoodHealthTotal));
}


bool CvGameTextMgr::setBuildingAdditionalHealthHelp(CvWStringBuffer &szBuffer, CvCity& city, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(iI);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			int iGood = 0, iBad = 0, iSpoiledFood = 0, iStarvation = 0;
			city.getAdditionalHealthByBuilding(eBuilding, iGood, iBad, iSpoiledFood, iStarvation);

			if (iGood != 0 || iBad != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				const bool bStartedLine = setResumableGoodBadChangeHelp(szBuffer, szLabel, L": ", L"", iGood, gDLL->getSymbolID(HEALTHY_CHAR), iBad, gDLL->getSymbolID(UNHEALTHY_CHAR), false, true);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iSpoiledFood, gDLL->getSymbolID(EATEN_FOOD_CHAR), false, true, bStartedLine);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iStarvation, gDLL->getSymbolID(BAD_FOOD_CHAR), false, true, bStartedLine);
			}
		}
	}

	return bStarted;
}


void CvGameTextMgr::parseHappinessHelp(CvWStringBuffer &szBuffer)
{
	CvCity* pHeadSelectedCity = gDLL->getInterfaceIFace()->getHeadSelectedCity();

	if (pHeadSelectedCity)
	{
		setAngerHelp(szBuffer, *pHeadSelectedCity);
		szBuffer.append(L"\n=======================\n");
		setHappyHelp(szBuffer, *pHeadSelectedCity);

		if (pHeadSelectedCity->getOwner() == GC.getGame().getActivePlayer() && getBugOptionBOOL("MiscHover__BuildingAdditionalHappiness", true, "BUG_BUILDING_ADDITIONAL_HAPPINESS_HOVER"))
		{
			setBuildingAdditionalHappinessHelp(szBuffer, *pHeadSelectedCity, DOUBLE_SEPARATOR);
		}
	}
}

void CvGameTextMgr::setAngerHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	PROFILE_EXTRA_FUNC();
	if (city.isOccupation())
	{
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_RESISTANCE"));
		return;
	}
	CvPlayer& kPlayer = GET_PLAYER(city.getOwner());
	if (kPlayer.isAnarchy())
	{
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ANARCHY"));
		return;
	}
	int aRealized[NUM_WELLBEING_CHANNELS];
	city.realizedWellbeing(0, aRealized);
	const int iUnhappy = aRealized[WELLBEING_ANGER] / 100;
	if (iUnhappy < 1)
	{
		return;
	}
	const int iPop = city.getPopulation();
	const int iDivisor = GC.getPERCENT_ANGER_DIVISOR();
	int iTotal = 0;

	int aDeposits[NUM_WELLBEING_CHANNELS];
	city.getWellbeing(aDeposits);
	int iAnger = aDeposits[WELLBEING_ANGER] / 100;
	if (iAnger > 0)
	{
		iTotal += iAnger;
		szBuffer.append(gDLL->getText("TXT_KEY_UNHAPPY_CITY_BUILDINGS", iAnger));
		szBuffer.append(NEWLINE);
	}

	// The anger PERCENTS are raw runtime state with no entry list to render from, so each keeps its own line.
	int aAngerPercent[10];
	aAngerPercent[0] = city.getOvercrowdingPercentAnger();
	aAngerPercent[1] = city.getNoMilitaryPercentAnger();
	aAngerPercent[2] = city.getCulturePercentAnger();
	aAngerPercent[3] = city.getReligionPercentAnger();
	aAngerPercent[4] = city.getHurryPercentAnger();
	aAngerPercent[5] = city.getConscriptPercentAnger();
	aAngerPercent[6] = city.getDefyResolutionPercentAnger();
	aAngerPercent[7] = city.getWarWearinessPercentAnger();
	aAngerPercent[8] = city.getRevRequestPercentAnger();
	aAngerPercent[9] = city.getRevIndexPercentAnger();
	const char* aAngerKey[10];
	aAngerKey[0] = "TXT_KEY_ANGER_OVERCROWDING";
	aAngerKey[1] = "TXT_KEY_ANGER_MILITARY_PROTECTION";
	aAngerKey[2] = "TXT_KEY_ANGER_OCCUPIED";
	aAngerKey[3] = "TXT_KEY_ANGER_RELIGION_FIGHT";
	aAngerKey[4] = "TXT_KEY_ANGER_OPPRESSION";
	aAngerKey[5] = "TXT_KEY_ANGER_DRAFT";
	aAngerKey[6] = "TXT_KEY_ANGER_DEFY_RESOLUTION";
	aAngerKey[7] = "TXT_KEY_ANGER_WAR_WEAR";
	aAngerKey[8] = "TXT_KEY_REV_REQUEST_ANGER";
	aAngerKey[9] = "TXT_KEY_REV_INDEX_ANGER";
	for (int iLine = 0; iLine < 10; ++iLine)
	{
		iAnger = aAngerPercent[iLine] * iPop / iDivisor;
		if (iAnger > 0)
		{
			iTotal += iAnger;
			szBuffer.append(gDLL->getText(aAngerKey[iLine], iAnger));
			szBuffer.append(NEWLINE);
		}
	}
	// The remaining raw-state inputs, each read directly off the state that produces it.
	iAnger = city.getVassalUnhappiness();
	if (iAnger > 0)
	{
		iTotal += iAnger;
		szBuffer.append(gDLL->getText("TXT_KEY_UNHAPPY_VASSAL", iAnger));
		szBuffer.append(NEWLINE);
	}
	iAnger = city.getEspionageHappinessCounter();
	if (iAnger > 0)
	{
		iTotal += iAnger;
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ESPIONAGE", iAnger));
		szBuffer.append(NEWLINE);
	}
	iAnger = kPlayer.calculateTaxRateUnhappiness();
	if (iAnger > 0)
	{
		iTotal += iAnger;
		szBuffer.append(gDLL->getText("TXT_KEY_CITY_TAXATION_ANGER", iAnger));
		szBuffer.append(NEWLINE);
	}
	iAnger = city.getEventAnger();
	if (iAnger > 0)
	{
		iTotal += iAnger;
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_ARGH", iAnger));
		szBuffer.append(NEWLINE);
	}
	if (kPlayer.getCityLimit() > 0 && kPlayer.getCityOverLimitUnhappy() > 0)
	{
		iAnger = kPlayer.getCityOverLimitUnhappy() * (kPlayer.getNumCities() - kPlayer.getCityLimit());
		if (iAnger > 0)
		{
			iTotal += iAnger;
			szBuffer.append(gDLL->getText("TXT_KEY_ANGER_TOO_MANY_CITIES", iAnger));
			szBuffer.append(NEWLINE);
		}
	}
	if (iUnhappy > iTotal)
	{
		// The MISC residual -- what the named lines do not account for (the foreign-culture and landmark terms,
		// and integer rounding across them).
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MISC", iUnhappy - iTotal));
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(L"-----------------------\n");
	szBuffer.append(gDLL->getText("TXT_KEY_ANGER_TOTAL_UNHAPPY", iUnhappy));
}


void CvGameTextMgr::setHappyHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	PROFILE_EXTRA_FUNC();
	if (city.isDisorder())
	{
		return;
	}
	int aRealized[NUM_WELLBEING_CHANNELS];
	city.realizedWellbeing(0, aRealized);
	const int iTotalHappy = aRealized[WELLBEING_HAPPINESS] / 100;
	if (iTotalHappy < 1)
	{
		return;
	}
	CvPlayer& kPlayer = GET_PLAYER(city.getOwner());
	int aDeposits[NUM_WELLBEING_CHANNELS];
	city.getWellbeing(aDeposits);
	int iSum = 0;

	int iHappy = aDeposits[WELLBEING_HAPPINESS] / 100;
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BUILDINGS", iHappy));
		szBuffer.append(NEWLINE);
	}
	iHappy = city.getMilitaryHappiness();
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_MILITARY_PRESENCE", iHappy));
		szBuffer.append(NEWLINE);
	}
	iHappy = city.getCelebrityHappiness();
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_BIG_CITY", iHappy));
		szBuffer.append(NEWLINE);
	}
	iHappy = city.getVassalHappiness();
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_VASSAL", iHappy));
		szBuffer.append(NEWLINE);
	}
	iHappy = city.getRevSuccessHappiness();
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_REV_SUCCESS_HAPPINESS", iHappy));
		szBuffer.append(NEWLINE);
	}
	iHappy = std::max(0, city.getExtraHappiness() + kPlayer.getExtraHappiness());
	if (iHappy > 0)
	{
		iSum += iHappy;
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_TEMP", iHappy));
		szBuffer.append(NEWLINE);
	}
	if (city.getHappinessTimer() > 0)
	{
		iSum += GC.getTEMP_HAPPY();
		szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_TEMP", GC.getTEMP_HAPPY()));
		szBuffer.append(NEWLINE);
	}
	if (iTotalHappy > iSum)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_ANGER_MISC", iTotalHappy - iSum));
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(L"-----------------------\n");
	szBuffer.append(gDLL->getText("TXT_KEY_HAPPY_TOTAL_HAPPY", iTotalHappy));
}

// BUG - Building Additional Happiness - start
bool CvGameTextMgr::setBuildingAdditionalHappinessHelp(CvWStringBuffer &szBuffer, CvCity& city, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			int iGood = 0, iBad = 0, iAngryPop = 0;
			city.getAdditionalHappinessByBuilding(eBuilding, iGood, iBad, iAngryPop);

			if (iGood != 0 || iBad != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				const bool bStartedLine = setResumableGoodBadChangeHelp(szBuffer, szLabel, L": ", L"", iGood, gDLL->getSymbolID(HAPPY_CHAR), iBad, gDLL->getSymbolID(UNHAPPY_CHAR), false, true);
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iAngryPop, gDLL->getSymbolID(ANGRY_POP_CHAR), false, true, bStartedLine);
			}
		}
	}

	return bStarted;
}
// BUG - Building Additional Happiness - end


// BUG - Resumable Value Change Help - start
void CvGameTextMgr::setYieldChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piYieldChange, bool bPercent, bool bNewLine)
{
	setResumableYieldChangeHelp(szBuffer, szStart, szSpace, szEnd, piYieldChange, bPercent, bNewLine);
}
void CvGameTextMgr::setYieldPerPopChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piYieldChange, bool bPercent, bool bNewLine)
{
	setResumableYieldChangeHelp(szBuffer, szStart, szSpace, szEnd, piYieldChange, bPercent, bNewLine, false, true);
}

/*
 * Adds the ability to pass in and get back the value of bStarted so that
 * it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
 */
bool CvGameTextMgr::setResumableYieldChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piYieldChange, bool bPercent, bool bNewLine, bool bStarted, bool bPerPop)
{
	PROFILE_EXTRA_FUNC();
	CvWString szPerPop;

	if (bPerPop)
	{
		szPerPop.append(gDLL->getText("TXT_KEY_PER_POP"));
	}

	if (piYieldChange)
	{
		for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
		{
			if (piYieldChange[iI] != 0)
			{
				CvWString szTempBuffer;
				if (!bStarted)
				{
					if (bNewLine)
					{
						szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
					}
					if (!bPerPop)
					{
						szTempBuffer += CvWString::format(
							L"%s%s%s%d%s%c",
							szStart.GetCString(),
							szSpace.GetCString(),
							piYieldChange[iI] > 0 ? L"+" : L"",
							piYieldChange[iI],
							bPercent ? L"%" : L"",
							GC.getYieldInfo((YieldTypes)iI).getChar()
						);
					}
					else if (piYieldChange[iI] % 100 == 0)
					{
						szTempBuffer += CvWString::format(
							L"%s%s%s%d%s%c%s",
							szStart.GetCString(),
							szSpace.GetCString(),
							piYieldChange[iI] > 0 ? L"+" : L"",
							piYieldChange[iI] / 100,
							bPercent ? L"%" : L"",
							GC.getYieldInfo((YieldTypes)iI).getChar(),
							szPerPop.GetCString()
						);
					}
					else
					{
						szTempBuffer += CvWString::format(
							L"%s%s%s%d.%02d%s%c%s",
							szStart.GetCString(),
							szSpace.GetCString(),
							piYieldChange[iI] > 0 ? L"+" : L"",
							piYieldChange[iI] / 100,
							piYieldChange[iI] % 100,
							bPercent ? L"%" : L"",
							GC.getYieldInfo((YieldTypes)iI).getChar(),
							szPerPop.GetCString()
						);
					}
				}
				else if (!bPerPop)
				{
					szTempBuffer.Format(
						L", %s%d%s%c",
						piYieldChange[iI] > 0 ? L"+" : L"",
						piYieldChange[iI],
						bPercent ? L"%" : L"",
						GC.getYieldInfo((YieldTypes)iI).getChar()
					);
				}
				else if (piYieldChange[iI] % 100 == 0)
				{
					szTempBuffer.Format(
						L", %s%d%s%c%s",
						piYieldChange[iI] > 0 ? L"+" : L"",
						piYieldChange[iI] / 100,
						bPercent ? L"%" : L"",
						GC.getYieldInfo((YieldTypes)iI).getChar(),
						szPerPop.GetCString()
					);
				}
				else
				{
					szTempBuffer.Format(
						L", %s%d.%02d%s%c%s",
						piYieldChange[iI] > 0 ? L"+" : L"",
						piYieldChange[iI] / 100,
						piYieldChange[iI] % 100,
						bPercent ? L"%" : L"",
						GC.getYieldInfo((YieldTypes)iI).getChar(),
						szPerPop.GetCString()
					);
				}
				szBuffer.append(szTempBuffer);
				bStarted = true;
			}
		}
		if (bStarted)
		{
			szBuffer.append(szEnd);
		}
	}
	return bStarted;
}


void CvGameTextMgr::listCommerceChange(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szEnd, const int* aList, bool bPercent)
{
	PROFILE_EXTRA_FUNC();
	bool bStarted = false;
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
	{
		if (aList[iI] != 0)
		{
			if (bStarted)
			{
				szBuffer.append(L", ");
			}
			else
			{
				szBuffer.append(szStart);
				bStarted = true;
			}
			szBuffer.append(CvWString::format(L"%s%d%s%c", (aList[iI] > 0) ? L"+" : L"", aList[iI], (bPercent) ? L"%" : L"", GC.getCommerceInfo((CommerceTypes)iI).getChar()));
		}
	}
	if (bStarted)
	{
		szBuffer.append(szEnd);
	}
}

/*
 * Displays float values by dividing each value by 100.
 */
void CvGameTextMgr::setCommerceTimes100ChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bNewLine, bool bStarted)
{
	setResumableCommerceTimes100ChangeHelp(szBuffer, szStart, szSpace, szEnd, piCommerceChange, bNewLine);
}

/*
 * Adds the ability to pass in and get back the value of bStarted so that
 * it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
 */
bool CvGameTextMgr::setResumableCommerceTimes100ChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, const int* piCommerceChange, bool bNewLine, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	CvWString szTempBuffer;

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; ++iI)
	{
		int iChange = piCommerceChange[iI];
		if (iChange != 0)
		{
			if (!bStarted)
			{
				if (bNewLine)
				{
					szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
				}
				szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
				bStarted = true;
			}
			else
			{
				szTempBuffer.Format(L", ");
			}
			szBuffer.append(szTempBuffer);

			if (iChange % 100 == 0)
			{
				szTempBuffer.Format(L"%+d%c", iChange / 100, GC.getCommerceInfo((CommerceTypes) iI).getChar());
			}
			else
			{
				if (iChange >= 0)
				{
					szBuffer.append(L"+");
				}
				else
				{
					iChange = - iChange;
					szBuffer.append(L"-");
				}
				szTempBuffer.Format(L"%d.%02d%c", iChange / 100, iChange % 100, GC.getCommerceInfo((CommerceTypes) iI).getChar());
			}
			szBuffer.append(szTempBuffer);
		}
	}

	if (bStarted)
	{
		szBuffer.append(szEnd);
	}

	return bStarted;
}

/*
 * Adds the ability to pass in and get back the value of bStarted so that
 * it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
 */
bool CvGameTextMgr::setResumableGoodBadChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iGood, int iGoodSymbol, int iBad, int iBadSymbol, bool bPercent, bool bNewLine, bool bStarted)
{
	bStarted = setResumableValueChangeHelp(szBuffer, szStart, szSpace, szEnd, iGood, iGoodSymbol, bPercent, bNewLine, bStarted);
	bStarted = setResumableValueChangeHelp(szBuffer, szStart, szSpace, szEnd, iBad, iBadSymbol, bPercent, bNewLine, bStarted);

	return bStarted;
}

/*
 * Adds the ability to pass in and get back the value of bStarted so that
 * it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
 */
bool CvGameTextMgr::setResumableValueChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iValue, int iSymbol, bool bPercent, bool bNewLine, bool bStarted)
{
	CvWString szTempBuffer;

	if (iValue != 0)
	{
		if (!bStarted)
		{
			if (bNewLine)
			{
				szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
			}
			szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
		}
		else
		{
			szTempBuffer = L", ";
		}
		szBuffer.append(szTempBuffer);

		szTempBuffer.Format(L"%+d%s%c", iValue, bPercent ? L"%" : L"", iSymbol);
		szBuffer.append(szTempBuffer);

		bStarted = true;
	}

	return bStarted;
}

/*
 * Adds the ability to pass in and get back the value of bStarted so that
 * it can be used with other setResumable<xx>ChangeHelp() calls on a single line.
 */
bool CvGameTextMgr::setResumableValueTimes100ChangeHelp(CvWStringBuffer &szBuffer, const CvWString& szStart, const CvWString& szSpace, const CvWString& szEnd, int iValue, int iSymbol, bool bNewLine, bool bStarted)
{
	CvWString szTempBuffer;

	if (iValue != 0)
	{
		if (!bStarted)
		{
			if (bNewLine)
			{
				szTempBuffer.Format(L"\n%c", gDLL->getSymbolID(BULLET_CHAR));
			}
			szTempBuffer += CvWString::format(L"%s%s", szStart.GetCString(), szSpace.GetCString());
		}
		else
		{
			szTempBuffer = L", ";
		}
		szBuffer.append(szTempBuffer);

		if (iValue % 100 == 0)
		{
			szTempBuffer.Format(L"%+d%c", iValue / 100, iSymbol);
		}
		else
		{
			if (iValue >= 0)
			{
				szBuffer.append(L"+");
			}
			else
			{
				iValue = - iValue;
				szBuffer.append(L"-");
			}
			szTempBuffer.Format(L"%d.%02d%c", iValue / 100, iValue % 100, iSymbol);
		}
		szBuffer.append(szTempBuffer);

		bStarted = true;
	}

	return bStarted;
}
// BUG - Resumable Value Change Help - end

/************************************************************************************************/
/* REVOLUTION_MOD								 ?/?/?						   DPII		  */
/*																							  */
/* BUG																						  */
/************************************************************************************************/
void CvGameTextMgr::setBonusHelp(CvWStringBuffer &szBuffer, BonusTypes eBonus, bool bCivilopediaText)
{
	setBonusTradeHelp(szBuffer, eBonus, bCivilopediaText, NO_PLAYER);
}

void CvGameTextMgr::setBonusTradeHelp(CvWStringBuffer &szBuffer, BonusTypes eBonus, bool bCivilopediaText, PlayerTypes eTradePlayer)
{
}

void CvGameTextMgr::setReligionHelp(CvWStringBuffer &szBuffer, ReligionTypes eReligion, bool bCivilopedia)
{
}

void CvGameTextMgr::setReligionHelpCity(CvWStringBuffer &szBuffer, ReligionTypes eReligion, CvCity *pCity, bool bCityBar, bool bForceReligion, bool bForceState, bool bNoStateReligion)
{
}

void CvGameTextMgr::setCorporationHelp(CvWStringBuffer &szBuffer, CorporationTypes eCorporation, bool bCivilopedia)
{
}

void CvGameTextMgr::setCorporationHelpCity(CvWStringBuffer &szBuffer, CorporationTypes eCorporation, CvCity *pCity, bool bCityBar, bool bForceCorporation)
{
}

void CvGameTextMgr::buildObsoleteString(CvWStringBuffer &szBuffer, int iItem, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (bList)
	{
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_OBSOLETES", CvWString(GC.getBuildingInfo((BuildingTypes)iItem).getType()).c_str(), GC.getBuildingInfo((BuildingTypes) iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildObsoleteBonusString(CvWStringBuffer &szBuffer, int iItem, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (bList)
	{
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_OBSOLETES", CvWString(GC.getBonusInfo((BonusTypes)iItem).getType()).GetCString(), GC.getBonusInfo((BonusTypes) iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildObsoleteSpecialString(CvWStringBuffer &szBuffer, int iItem, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (bList)
	{
		szBuffer.append(NEWLINE);
	}
	szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_OBSOLETES_NO_LINK", GC.getSpecialBuildingInfo((SpecialBuildingTypes) iItem).getTextKeyWide()));
}

void CvGameTextMgr::buildMoveString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildFreeUnitString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildFeatureProductionString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildWorkerRateString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildTradeRouteString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildHealthRateString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

//Team Project (1)
void CvGameTextMgr::buildSpecialistHealthString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildHappinessRateString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

//Team Project (1)
void CvGameTextMgr::buildSpecialistHappinessString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildFreeTechString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildLOSString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildMapCenterString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildMapRevealString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList)
{
}

void CvGameTextMgr::buildMapTradeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildTechTradeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildGoldTradeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildOpenBordersString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildDefensivePactString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildPermanentAllianceString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildVassalStateString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildEmbassyString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildBridgeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildIrrigationString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildIgnoreIrrigationString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildWaterWorkString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildImprovementString(CvWStringBuffer &szBuffer, TechTypes eTech, BuildTypes eBuild, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildDomainExtraMovesString(CvWStringBuffer &szBuffer, TechTypes eTech, int iDomainType, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildAdjustString(CvWStringBuffer &szBuffer, TechTypes eTech, int iCommerceType, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildTerrainTradeString(CvWStringBuffer &szBuffer, TechTypes eTech, int iTerrainType, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildRiverTradeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildSpecialBuildingString(CvWStringBuffer &szBuffer, TechTypes eTech, int iBuildingType, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildYieldChangeString(CvWStringBuffer &szBuffer, TechTypes eTech, int iYieldType, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildBuildingTechSpecialistChangeString(CvWStringBuffer &szBuffer, TechTypes eTech, int iBuildingType, bool bList, bool bPlayerContext)
{
}


void CvGameTextMgr::buildBuildingTechHappinessChangesString(CvWStringBuffer &szBuffer, TechTypes eTech, int iBuildingType, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildBuildingTechHealthChangesString(CvWStringBuffer &szBuffer, TechTypes eTech, int iBuildingType, bool bList, bool bPlayerContext)
{
}


bool CvGameTextMgr::buildBonusRevealString(CvWStringBuffer &szBuffer, TechTypes eTech, int iBonusType, bool bFirst, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (GC.getBonusInfo((BonusTypes) iBonusType).getTechReveal() == eTech)
	{
		if (bList && bFirst)
		{
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CvWString(GC.getBonusInfo((BonusTypes) iBonusType).getType()).GetCString(), GC.getBonusInfo((BonusTypes) iBonusType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_REVEALS").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

bool CvGameTextMgr::buildCivicRevealString(CvWStringBuffer &szBuffer, TechTypes eTech, int iCivicType, bool bFirst, bool bList, bool bPlayerContext)
{
	return 0;
}

bool CvGameTextMgr::buildProcessInfoString(CvWStringBuffer &szBuffer, TechTypes eTech, int iProcessType, bool bFirst, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (GC.getProcessInfo((ProcessTypes) iProcessType).getTechPrereq() == eTech)
	{
		if (bList && bFirst)
		{
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CvWString(GC.getProcessInfo((ProcessTypes) iProcessType).getType()).GetCString(), GC.getProcessInfo((ProcessTypes) iProcessType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_CAN_BUILD").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

bool CvGameTextMgr::buildFoundReligionString(CvWStringBuffer &szBuffer, TechTypes eTech, int iReligionType, bool bFirst, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (GC.getReligionInfo((ReligionTypes) iReligionType).getTechPrereq() == eTech)
	{
/************************************************************************************************/
/* REVDCM								 04/29/10								phungus420	*/
/*																							  */
/* Player Functions																			 */
/************************************************************************************************/
		if (!bPlayerContext ||
		(!(GC.getGame().isReligionSlotTaken((ReligionTypes)iReligionType))
		&& GET_PLAYER(GC.getGame().getActivePlayer()).canFoundReligion()) )
/************************************************************************************************/
/* LIMITED_RELIGIONS			   END														  */
/************************************************************************************************/
		{
			if (bList && bFirst)
			{
				szBuffer.append(NEWLINE);
			}

			if (GC.getGame().isOption(GAMEOPTION_RELIGION_PICK))
			{
				szTempBuffer = gDLL->getText("TXT_KEY_RELIGION_UNKNOWN");
			}
			else
			{
				szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CvWString(GC.getReligionInfo((ReligionTypes) iReligionType).getType()).GetCString(), GC.getReligionInfo((ReligionTypes) iReligionType).getDescription());
			}
			setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_FIRST_DISCOVER_FOUNDS").c_str(), szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	return bFirst;
}

bool CvGameTextMgr::buildFoundCorporationString(CvWStringBuffer &szBuffer, TechTypes eTech, int iCorporationType, bool bFirst, bool bList, bool bPlayerContext)
{
	CvWString szTempBuffer;

	if (GC.getCorporationInfo((CorporationTypes) iCorporationType).getTechPrereq() == eTech)
	{
		if (!bPlayerContext || (GC.getGame().countKnownTechNumTeams(eTech) == 0))
		{
			if (bList && bFirst)
			{
				szBuffer.append(NEWLINE);
			}
			szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CvWString(GC.getCorporationInfo((CorporationTypes) iCorporationType).getType()).GetCString(), GC.getCorporationInfo((CorporationTypes) iCorporationType).getDescription());
			setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_FIRST_DISCOVER_INCORPORATES").c_str(), szTempBuffer, L", ", bFirst);
			bFirst = false;
		}
	}
	return bFirst;
}

bool CvGameTextMgr::buildPromotionString(CvWStringBuffer &szBuffer, TechTypes eTech, int iPromotionType, bool bFirst, bool bList) const
{
	CvWString szTempBuffer;

	if (GC.getPromotionInfo((PromotionTypes) iPromotionType).getTechPrereq() == eTech)
	{
		if (bList && bFirst)
		{
			szBuffer.append(NEWLINE);
		}
		szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), CvWString(GC.getPromotionInfo((PromotionTypes) iPromotionType).getType()).GetCString(), GC.getPromotionInfo((PromotionTypes) iPromotionType).getDescription());
		setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_ENABLES").c_str(), szTempBuffer, L", ", bFirst);
		bFirst = false;
	}
	return bFirst;
}

// Displays a list of derived technologies - no distinction between AND/OR prerequisites
void CvGameTextMgr::buildSingleLineTechTreeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bPlayerContext)
{
	PROFILE_EXTRA_FUNC();
	CvWString szTempBuffer;	// Formatting

	if (NO_TECH == eTech)
	{
		// you need to specify a tech of origin for this method to do anything
		return;
	}

	bool bFirst = true;
	for (int iI = 0; iI < GC.getNumTechInfos(); ++iI)
	{
		bool bTechAlreadyAccessible = false;
		if (bPlayerContext)
		{
			bTechAlreadyAccessible = (GET_TEAM(GC.getGame().getActiveTeam()).isHasTech((TechTypes)iI) || (GET_PLAYER(GC.getGame().getActivePlayer()).getTechAvailability((TechTypes)iI) == EnablerDomain::STATE_LISTED));
		}
		if (!bTechAlreadyAccessible)
		{
			if (algo::any_of_equal(GC.getTechInfo((TechTypes)iI).getPrereqOrTechs(), eTech)
			||  algo::any_of_equal(GC.getTechInfo((TechTypes)iI).getPrereqAndTechs(), eTech))
			{
				szTempBuffer.Format( SETCOLR L"<link=%s>%s</link>" ENDCOLR , TEXT_COLOR("COLOR_TECH_TEXT"), CvWString(GC.getTechInfo((TechTypes)iI).getType()).GetCString(), GC.getTechInfo((TechTypes) iI).getDescription());
				setListHelp(szBuffer, gDLL->getText("TXT_KEY_MISC_LEADS_TO").c_str(), szTempBuffer, L", ", bFirst);
				bFirst = false;
			}
		}
	}
}

// Information about other prerequisite technologies to eTech besides eFromTech
void CvGameTextMgr::buildTechTreeString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bPlayerContext, TechTypes eFromTech)
{
	PROFILE_EXTRA_FUNC();
	CvWString szTempBuffer;	// Formatting

	if (NO_TECH == eTech || NO_TECH == eFromTech)
	{
		return;
	}

	szTempBuffer.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTech).getDescription());
	szBuffer.append(szTempBuffer);

	// Loop through OR prerequisites to make list
	CvWString szOtherOrTechs;
	int nOtherOrTechs = 0;
	bool bOrTechFound = false;
	foreach_(const TechTypes eTestTech, GC.getTechInfo(eTech).getPrereqOrTechs())
	{
		bool bTechAlreadyResearched = false;
		if (bPlayerContext)
		{
			bTechAlreadyResearched = GET_TEAM(GC.getGame().getActiveTeam()).isHasTech(eTestTech);
		}
		if (!bTechAlreadyResearched)
		{
			if (eTestTech == eFromTech)
			{
				bOrTechFound = true;
			}
			else
			{
				szTempBuffer.Format( SETCOLR L"%s" ENDCOLR , TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTestTech).getDescription());
				setListHelp(szOtherOrTechs, L"", szTempBuffer, gDLL->getText("TXT_KEY_OR").c_str(), 0 == nOtherOrTechs);
				nOtherOrTechs++;
			}
		}
	}

	// Loop through AND prerequisites to make list
	CvWString szOtherAndTechs;
	int nOtherAndTechs = 0;
	bool bAndTechFound = false;
	foreach_(const TechTypes eTestTech, GC.getTechInfo(eTech).getPrereqAndTechs())
	{
		bool bTechAlreadyResearched = false;
		if (bPlayerContext)
		{
			bTechAlreadyResearched = GET_TEAM(GC.getGame().getActiveTeam()).isHasTech(eTestTech);
		}
		if (!bTechAlreadyResearched)
		{
			if (eTestTech == eFromTech)
			{
				bAndTechFound = true;
			}
			else
			{
				szTempBuffer.Format( SETCOLR L"%s" ENDCOLR , TEXT_COLOR("COLOR_TECH_TEXT"), GC.getTechInfo(eTestTech).getDescription());
				setListHelp(szOtherAndTechs, L"", szTempBuffer, L", ", 0 == nOtherAndTechs);
				nOtherAndTechs++;
			}
		}
	}

	if (bOrTechFound || bAndTechFound)
	{
		if (nOtherAndTechs > 0 || nOtherOrTechs > 0)
		{
			szBuffer.append(L' ');

			if (nOtherAndTechs > 0)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_WITH"));
				szBuffer.append(szOtherAndTechs);
			}

			if (nOtherOrTechs > 0)
			{
				if (bAndTechFound)
				{
					if (nOtherAndTechs > 0)
					{
						szBuffer.append(gDLL->getText("TXT_KEY_AND_SPACE"));
					}
					else
					{
						szBuffer.append(gDLL->getText("TXT_KEY_WITH"));
					}
					szBuffer.append(szOtherOrTechs);
				}
				else if (bOrTechFound)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_MISC_ALTERNATIVELY_DERIVED", GC.getTechInfo(eTech).getTextKeyWide(), szOtherOrTechs.GetCString()));
				}
			}
		}
	}
}

void CvGameTextMgr::setPromotionHelp(CvWStringBuffer &szBuffer, PromotionTypes ePromotion, bool bCivilopediaBodyText)
{
	CvUnit*	pUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();

	// Hide overridden promotions if there is a selected unit and this is not a pedia request
	if (GC.getIsInPedia() || !pUnit || !pUnit->isPromotionOverriden(ePromotion))
	{
		if (!bCivilopediaBodyText)
		{
			if (NO_PROMOTION == ePromotion)
			{
				return;
			}
			CvWString szTempBuffer;
			szTempBuffer.Format( SETCOLR L"%s" ENDCOLR , TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), GC.getPromotionInfo(ePromotion).getDescription());
			szBuffer.append(szTempBuffer);
		}
		parsePromotionHelpInternal(szBuffer, ePromotion, NEWLINE, !GC.getIsInPedia());
	}
}

void CvGameTextMgr::setBuildUpHelp(CvWStringBuffer &szBuffer, PromotionLineTypes ePromotionLine)
{
	CivilizationTypes eCivilization;

	if (GC.getGame().getActivePlayer() != NO_PLAYER)
	{
		eCivilization = GET_PLAYER(GC.getGame().getActivePlayer()).getCivilizationType();
	}
	else
	{
		eCivilization = NO_CIVILIZATION;
	}

	if (NO_PROMOTIONLINE == ePromotionLine)
	{
		return;
	}
	parseBuildUp(szBuffer, ePromotionLine, eCivilization);
}

void CvGameTextMgr::setTraitHelp(CvWStringBuffer &szBuffer, TraitTypes eTrait)
{
	if (NO_TRAIT == eTrait)
	{
		return;
	}
	parseTraits(szBuffer, eTrait, false, false);
}

void CvGameTextMgr::setUnitCombatHelp(CvWStringBuffer& szBuffer, UnitCombatTypes eUnitCombat, bool bCivilopediaText) const
{
}

void CvGameTextMgr::appendEntryLines(CvWStringBuffer& szBuffer, const CvInfo& info, ModifierFamily eFamily)
{
	const CvModifiers* pModifiers = info.getModifiers();
	if (pModifiers == NULL)
	{
		return;
	}
	const std::vector<CvModEntry*>& aEntries = pModifiers->entries();
	for (std::vector<CvModEntry*>::const_iterator it = aEntries.begin(); it != aEntries.end(); ++it)
	{
		if (*it != NULL && (*it)->family == eFamily)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(entryDetailLine(**it));
		}
	}
}

void CvGameTextMgr::setImprovementHelp(CvWStringBuffer &szBuffer, ImprovementTypes eImprovement, FeatureTypes eFeature, bool bCivilopediaText)
{
}

void CvGameTextMgr::setRouteHelp(CvWStringBuffer &szBuffer, RouteTypes eRoute, bool bCivilopediaText)
{
}


void CvGameTextMgr::getDealString(CvWStringBuffer& szBuffer, CvDeal& deal, PlayerTypes ePlayerPerspective)
{
	PlayerTypes ePlayer1 = deal.getFirstPlayer();
	PlayerTypes ePlayer2 = deal.getSecondPlayer();

	const CLinkList<TradeData>* pListPlayer1 = deal.getFirstTrades();
	const CLinkList<TradeData>* pListPlayer2 = deal.getSecondTrades();

	getDealString(szBuffer, ePlayer1, ePlayer2, pListPlayer1,  pListPlayer2, ePlayerPerspective);
}

void CvGameTextMgr::getDealString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer1, PlayerTypes ePlayer2, const CLinkList<TradeData>* pListPlayer1, const CLinkList<TradeData>* pListPlayer2, PlayerTypes ePlayerPerspective)
{
	PROFILE_EXTRA_FUNC();
	if (NO_PLAYER == ePlayer1 || NO_PLAYER == ePlayer2)
	{
		FErrorMsg("Deal needs two parties");
		return;
	}

	CvWStringBuffer szDealOne;
	if (pListPlayer1 && pListPlayer1->getLength() > 0)
	{
		CLLNode<TradeData>* pTradeNode;
		bool bFirst = true;
		for (pTradeNode = pListPlayer1->head(); pTradeNode; pTradeNode = pListPlayer1->next(pTradeNode))
		{
			CvWStringBuffer szTrade;
			getTradeString(szTrade, pTradeNode->m_data, ePlayer1, ePlayer2);
			setListHelp(szDealOne, L"", szTrade.getCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	CvWStringBuffer szDealTwo;
	if (pListPlayer2 && pListPlayer2->getLength() > 0)
	{
		CLLNode<TradeData>* pTradeNode;
		bool bFirst = true;
		for (pTradeNode = pListPlayer2->head(); pTradeNode; pTradeNode = pListPlayer2->next(pTradeNode))
		{
			CvWStringBuffer szTrade;
			getTradeString(szTrade, pTradeNode->m_data, ePlayer2, ePlayer1);
			setListHelp(szDealTwo, L"", szTrade.getCString(), L", ", bFirst);
			bFirst = false;
		}
	}

	if (!szDealOne.isEmpty())
	{
		if (!szDealTwo.isEmpty())
		{
			if (ePlayerPerspective == ePlayer1)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_OUR_DEAL", szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString()));
			}
			else if (ePlayerPerspective == ePlayer2)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_OUR_DEAL", szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString()));
			}
			else
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL", GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString()));
			}
		}
		else
		{
			if (ePlayerPerspective == ePlayer1)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_OURS", szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
			}
			else if (ePlayerPerspective == ePlayer2)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_THEIRS", szDealOne.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
			}
			else
			{
				szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED", GET_PLAYER(ePlayer1).getNameKey(), szDealOne.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
			}
		}
	}
	else if (!szDealTwo.isEmpty())
	{
		if (ePlayerPerspective == ePlayer1)
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_THEIRS", szDealTwo.getCString(), GET_PLAYER(ePlayer2).getNameKey()));
		}
		else if (ePlayerPerspective == ePlayer2)
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED_OURS", szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
		}
		else
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEAL_ONESIDED", GET_PLAYER(ePlayer2).getNameKey(), szDealTwo.getCString(), GET_PLAYER(ePlayer1).getNameKey()));
		}
	}
}

void CvGameTextMgr::getWarplanString(CvWStringBuffer& szString, WarPlanTypes eWarPlan)
{
	switch (eWarPlan)
	{
		case WARPLAN_ATTACKED_RECENT: szString.assign(L"new defensive war"); break;
		case WARPLAN_ATTACKED: szString.assign(L"defensive war"); break;
		case WARPLAN_PREPARING_LIMITED: szString.assign(L"preparing limited war"); break;
		case WARPLAN_PREPARING_TOTAL: szString.assign(L"preparing total war"); break;
		case WARPLAN_LIMITED: szString.assign(L"limited war"); break;
		case WARPLAN_TOTAL: szString.assign(L"total war"); break;
		case WARPLAN_DOGPILE: szString.assign(L"dogpile war"); break;
		case NO_WARPLAN: szString.assign(L"unplanned war"); break;
		default:  szString.assign(L"unknown war"); break;
	}
}

void CvGameTextMgr::getAttitudeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eTargetPlayer)
{
}

void CvGameTextMgr::getEspionageString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eTargetPlayer)
{
	const TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
	const CvPlayer& kTargetPlayer = GET_PLAYER(eTargetPlayer);

	szBuffer.append(
		gDLL->getText(
			"TXT_KEY_ESPIONAGE_AGAINST_PLAYER",
			kTargetPlayer.getNameKey(),
			GET_TEAM(eTeam).getEspionagePointsAgainstTeam(kTargetPlayer.getTeam()),
			GET_TEAM(kTargetPlayer.getTeam()).getEspionagePointsAgainstTeam(eTeam)
		)
	);
}

void CvGameTextMgr::getTradeString(CvWStringBuffer& szBuffer, const TradeData& tradeData, PlayerTypes ePlayer1, PlayerTypes ePlayer2)
{
	switch (tradeData.m_eItemType)
	{
	case TRADE_GOLD:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOLD", tradeData.m_iData));
		break;
	case TRADE_GOLD_PER_TURN:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_GOLD_PER_TURN", tradeData.m_iData));
		break;
	case TRADE_MAPS:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WORLD_MAP"));
		break;
	case TRADE_SURRENDER:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CAPITULATE"));
		break;
	case TRADE_VASSAL:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL"));
		break;
	case TRADE_OPEN_BORDERS:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_OPEN_BORDERS"));
		break;
	case TRADE_DEFENSIVE_PACT:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_DEFENSIVE_PACT"));
		break;
	case TRADE_PERMANENT_ALLIANCE:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PERMANENT_ALLIANCE"));
		break;
	case TRADE_PEACE_TREATY:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_PEACE_TREATY", getTreatyLength()));
		break;
	case TRADE_TECHNOLOGIES:
		szBuffer.assign(CvWString::format(L"%s", GC.getTechInfo((TechTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_RESOURCES:
		szBuffer.assign(CvWString::format(L"%s", GC.getBonusInfo((BonusTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_CITIES:
		szBuffer.assign(CvWString::format(L"%s", GET_PLAYER(ePlayer1).getCity(tradeData.m_iData)->getName().GetCString()));
		break;
	case TRADE_PEACE:
	case TRADE_WAR:
	case TRADE_CONTACT:
	case TRADE_EMBARGO:
		szBuffer.assign(CvWString::format(L"%s", GET_TEAM((TeamTypes)tradeData.m_iData).getName().GetCString()));
		break;
	case TRADE_CIVIC:
		szBuffer.assign(CvWString::format(L"%s", GC.getCivicInfo((CivicTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_RELIGION:
		szBuffer.assign(CvWString::format(L"%s", GC.getReligionInfo((ReligionTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_WORKER:
	case TRADE_MILITARY_UNIT:
		szBuffer.assign(CvWString::format(L"%s", GET_PLAYER(ePlayer1).getUnit(tradeData.m_iData)->getName().GetCString()));
		break;
	case TRADE_EMBASSY:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_EMBASSY"));
		break;
	case TRADE_CORPORATION:
		szBuffer.assign(CvWString::format(L"%s", GC.getCorporationInfo((CorporationTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_SECRETARY_GENERAL_VOTE:
		szBuffer.assign(CvWString::format(L"%s", GC.getVoteSourceInfo((VoteSourceTypes)tradeData.m_iData).getDescription()));
		break;
	case TRADE_RITE_OF_PASSAGE:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_LIMITED_BORDERS"));
		break;
	case TRADE_FREE_TRADE_ZONE:
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_FREE_TRADE_ZONE"));
		break;
	default:
		FErrorMsg("error");
		break;
	}
}

void CvGameTextMgr::setFeatureHelp(CvWStringBuffer &szBuffer, FeatureTypes eFeature, bool bCivilopediaText)
{
}


void CvGameTextMgr::setTerrainHelp(CvWStringBuffer &szBuffer, TerrainTypes eTerrain, bool bCivilopediaText)
{
}


void CvGameTextMgr::buildFinanceSpecialistGoldString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	const CvPlayer& player = GET_PLAYER(ePlayer);

	int* iCounts = new int[GC.getNumSpecialistInfos()];
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		iCounts[iI] = 0;
	}
	int iTotalSpecialists = 0;
	foreach_(const CvCity* pCity, player.cities())
	{
		int iCityGold = 0;
		if (!pCity->isDisorder())
		{
			for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
			{
				iCounts[iI] += pCity->getSpecialistCount((SpecialistTypes)iI) + pCity->getFreeSpecialistCount((SpecialistTypes)iI);

				iCityGold += ((pCity->getSpecialistCount((SpecialistTypes)iI) + pCity->getFreeSpecialistCount((SpecialistTypes)iI))*player.specialistCommerceTimes100((SpecialistTypes)iI, COMMERCE_GOLD))/100;
			}

			iTotalSpecialists += pCity->getSpecialistPopulation() + pCity->getNumGreatPeople();
		}
	}

	bool bFirst = true;
	int iTotal = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		int iGold = (iCounts[iI] * player.specialistCommerceTimes100((SpecialistTypes)iI, COMMERCE_GOLD));
		if (iGold != 0)
		{
			if (bFirst)
			{
				szBuffer.append(NEWLINE);
				bFirst = false;
			}

			CvWString buf;

			buf.Format(L"%d.%02d",iGold/100, iGold%100);
			szBuffer.append(gDLL->getText("TXT_KEY_BUG_FINANCIAL_ADVISOR_SPECIALIST_GOLD", buf.GetCString(), iCounts[iI], GC.getSpecialistInfo((SpecialistTypes)iI).getDescription()));
			iTotal += iGold;
		}
	}

	szBuffer.append(gDLL->getText("TXT_KEY_BUG_FINANCIAL_ADVISOR_SPECIALIST_TOTAL_GOLD", iTotal/100));
	SAFE_DELETE_ARRAY(iCounts);
}


void CvGameTextMgr::buildFinanceInflationString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	const long long iInflationCost = kPlayer.getInflationCost();
	if (iInflationCost > 0)
	{
		const int iInflationRate10000 = kPlayer.getInflationMod10000();
		const int iInflationRateInt = iInflationRate10000 / 100 - 100;
		const int iInflationRateDec = iInflationRate10000 % 100;
		const long long iPreInflation = kPlayer.calculatePreInflatedCosts();
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_INFLATION_0", iPreInflation));
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_INFLATION_1", iInflationRateInt, iInflationRateDec, iInflationCost));
	}
}

void CvGameTextMgr::buildFinanceUnitUpkeepString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
}

void CvGameTextMgr::buildFinanceAwaySupplyString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	const CvPlayer& player = GET_PLAYER(ePlayer);

	int iPaidUnits = 0;
	int iBaseCost = 0;
	const int iCost = player.calculateUnitSupply(iPaidUnits, iBaseCost);
	const int iHandicap = iCost - iBaseCost;

	CvWString szHandicap;
	if (iHandicap != 0)
	{
		szHandicap = gDLL->getText("TXT_KEY_FINANCE_ADVISOR_HANDICAP_COST", iHandicap);
	}
	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_SUPPLY_COST", iPaidUnits, GC.getDefineINT("INITIAL_FREE_OUTSIDE_UNITS"), iBaseCost, szHandicap.GetCString(), iCost));
}

void CvGameTextMgr::buildFinanceCityMaintString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	int iDistanceMaint = 0;
	int iNumCityMaint = 0;
	int iColonyMaint = 0;
	int iCorporationMaint = 0;
	int iBuildingMaint = 0;

	const CvPlayer& player = GET_PLAYER(ePlayer);

	foreach_(const CvCity* pLoopCity, player.cities())
	{
		if (!pLoopCity->isDisorder() && !pLoopCity->isWeLoveTheKingDay() && pLoopCity->getPopulation() > 0)
		{
			const int iMod = pLoopCity->maintenancePercentStack((int)MAINTENANCE_AMOUNT);

			iDistanceMaint += (int)pLoopCity->maintenanceOfKind(MAINTENANCE_DISTANCE);
			iNumCityMaint += (int)pLoopCity->maintenanceOfKind(MAINTENANCE_NUM_CITIES);
			iColonyMaint += (int)pLoopCity->maintenanceOfKind(MAINTENANCE_COLONY);
			iCorporationMaint += (int)pLoopCity->maintenanceOfKind(MAINTENANCE_CORPORATION);
			int64_t iBuildingFlat = 0;
			int64_t iBuildingPercent = 0;
			pLoopCity->maintenanceLegs((int)MAINTENANCE_AMOUNT, iBuildingFlat, iBuildingPercent);
			iBuildingMaint += getModifiedIntValue((int)iBuildingFlat, iMod);
		}
	}

	if (iDistanceMaint != 0)
	{
		CvWString szMaint = CvWString::format(L"%d.%02d", iDistanceMaint/100, iDistanceMaint%100);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_CITY_MAINT_COST_DISTANCE", szMaint.GetCString()));
	}
	if (iNumCityMaint != 0)
	{
		CvWString szMaint = CvWString::format(L"%d.%02d", iNumCityMaint/100, iNumCityMaint%100);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_NUM_CITIES_FLOAT", szMaint.GetCString()));
	}
	if (iColonyMaint != 0)
	{
		CvWString szMaint = CvWString::format(L"%d.%02d", iColonyMaint/100, iColonyMaint%100);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_COLONY_MAINT_FLOAT", szMaint.GetCString()));
	}
	if (iCorporationMaint != 0)
	{
		CvWString szMaint = CvWString::format(L"%d.%02d", iCorporationMaint/100, iCorporationMaint%100);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_CORPORATION_MAINT_FLOAT", szMaint.GetCString()));
	}
	if (iBuildingMaint != 0)
	{
		CvWString szMaint = CvWString::format(L"%d.%02d", iBuildingMaint/100, iBuildingMaint%100);
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_BUILDING_MAINT_FLOAT", szMaint.GetCString()));
	}
	const int iTotal = (int)player.getTotalMaintenance();
	if (iTotal != 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_CITY_MAINT_COST_TOTAL", iTotal));
	}
}

void CvGameTextMgr::buildFinanceCivicUpkeepString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	const CvPlayer& player = GET_PLAYER(ePlayer);
	CvWString szCivicOptionCosts;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); ++iI)
	{
		const CivicTypes eCivic = player.getCivics((CivicOptionTypes)iI);
		if (NO_CIVIC != eCivic)
		{
			CvWString szTemp;
			szTemp.Format(L"%d%c: %s", player.getSingleCivicUpkeep(eCivic), GC.getCommerceInfo(COMMERCE_GOLD).getChar(),  GC.getCivicInfo(eCivic).getDescription());
			szCivicOptionCosts += NEWLINE + szTemp;
		}
	}

	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_CIVIC_UPKEEP_COST", szCivicOptionCosts.GetCString(), player.getCivicUpkeep()));
}

void CvGameTextMgr::buildFinanceForeignIncomeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	if (NO_PLAYER == ePlayer)
	{
		return;
	}
	const CvPlayer& player = GET_PLAYER(ePlayer);

	CvWString szPlayerIncome;
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& otherPlayer = GET_PLAYER((PlayerTypes)iI);
		if (otherPlayer.isAlive() && player.getGoldPerTurnByPlayer((PlayerTypes)iI) != 0)
		{
			CvWString szTemp;
			szTemp.Format(L"%d%c: %s", player.getGoldPerTurnByPlayer((PlayerTypes)iI), GC.getCommerceInfo(COMMERCE_GOLD).getChar(), otherPlayer.getCivilizationShortDescription());
			szPlayerIncome += NEWLINE + szTemp;
		}
	}
	if (!szPlayerIncome.empty())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_FINANCE_ADVISOR_FOREIGN_INCOME", szPlayerIncome.GetCString(), player.getGoldPerTurn()));
	}
}


/*
	+14 from Worked Tiles
	+2 from Specialists
	+5 from Corporations
	+1 from Buildings
	----------------------- |
	Base Food Produced: 22  |-- only if there are modifiers
	+25% from Buildings	 |
	-----------------------
	Total Food Produced: 27
	=======================
	+16 for Population
	+2 for Health
	-----------------------
	Total Food Consumed: 18
	=======================
	Net Food: +9			or
	Net Food for Settler: 9
	=======================
	* Lighthouse: +3
	* Supermarket: +1
*/
void CvGameTextMgr::setFoodHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	FAssertMsg(NO_PLAYER != city.getOwner(), "City must have an owner");

	// A displayed whole-food figure, so the rate reduces at this reader ([DEC-fixedpoint-x100]).
	int aiRealizedYields[NUM_YIELD_TYPES];
	city.getYields(aiRealizedYields);
	int iRate = aiRealizedYields[YIELD_FOOD] / 100;

	// shows Base Food and lists all modifiers
	setYieldHelp(szBuffer, city, YIELD_FOOD);

	szBuffer.append(DOUBLE_SEPARATOR);

	int iFoodConsumed = 0;

	// Eaten
	int iEatenFood = city.getFoodConsumedByPopulation();
	if (iEatenFood != 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_EATEN_FOOD", iEatenFood));
		iFoodConsumed += iEatenFood;
	}

	// Wasted
	int iWastedFood = (int)city.foodWastage();
	if (iWastedFood != 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_WASTED_FOOD", iWastedFood));
		iFoodConsumed += iWastedFood;
	}

	// Health
	int iSpoiledFood = - city.healthRate();
	if (iSpoiledFood != 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_SPOILED_FOOD", iSpoiledFood));
		iFoodConsumed += iSpoiledFood;
	}

	// Total Consumed
	szBuffer.append(NEWLINE);
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_TOTAL_FOOD_CONSUMED", iFoodConsumed));

	// ==========================
	szBuffer.append(DOUBLE_SEPARATOR NEWLINE);
	iRate -= iFoodConsumed;

	// Production
	if (city.isFoodProduction() && iRate > 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_PRODUCTION", iRate, city.getProductionNameKey()));
	}
	else
	{
		// cannot starve a size 1 city with no food in
		if (iRate < 0 && city.getPopulation() == 1 && city.getFood() == 0)
		{
			iRate = 0;
		}

		// Net Food
		if (iRate > 0)
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_GROW", iRate));
		}
		else if (iRate < 0)
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_SHRINK", iRate));
		}
		else
		{
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_HELP_NET_FOOD_STAGNATE"));
		}
	}

	if (city.getOwner() == GC.getGame().getActivePlayer() && getBugOptionBOOL("MiscHover__BuildingAdditionalFood", true, "BUG_BUILDING_ADDITIONAL_FOOD_HOVER"))
	{
		setBuildingAdditionalYieldHelp(szBuffer, city, YIELD_FOOD, DOUBLE_SEPARATOR);
	}
}


// BUG - Building Additional Yield - start
bool CvGameTextMgr::setBuildingAdditionalYieldHelp(CvWStringBuffer &szBuffer, CvCity& city, YieldTypes eIndex, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	const CvYieldInfo& info = GC.getYieldInfo(eIndex);

	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			const int iChange = city.getAdditionalYieldByBuilding(eIndex, eBuilding);

			if (iChange != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, info.getChar(), false, true);
			}
		}
	}

	return bStarted;
}
// BUG - Building Additional Yield - end

// BUG - Building Additional Commerce - start
bool CvGameTextMgr::setBuildingAdditionalCommerceHelp(CvWStringBuffer &szBuffer, CvCity& city, CommerceTypes eIndex, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	const CvCommerceInfo& info = GC.getCommerceInfo(eIndex);

	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			int iChange = city.getAdditionalCommerceByBuilding(eIndex, eBuilding);
			const int iCommerce = city.getAdditionalYieldByBuilding(YIELD_COMMERCE, eBuilding);
			iChange += iCommerce * GET_PLAYER(city.getOwner()).getCommercePercent(eIndex);

			if (iChange != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueTimes100ChangeHelp(szBuffer, szLabel, L": ", L"", iChange, info.getChar(), true);
			}
		}
	}
	// BUG jdog5000 - need to verify building type is correct for this player to avoid asserts - end

	return bStarted;
}
// BUG - Building Additional Commerce - end

// BUG - Building Saved Maintenance - start
bool CvGameTextMgr::setBuildingSavedMaintenanceHelp(CvWStringBuffer &szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	const CvCommerceInfo& info = GC.getCommerceInfo(COMMERCE_GOLD);

	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			const int iChange = -city.getSavedMaintenanceTimes100ByBuilding(eBuilding); //Afforess: saved maintenance is displayed as negative in hover

			if (iChange != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				CvWString szLabel;
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueTimes100ChangeHelp(szBuffer, szLabel, L": ", L"", iChange, info.getChar(), true);
			}
		}
	}

	return bStarted;
}
// BUG - Building Saved Maintenance - end

void CvGameTextMgr::setProductionHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
	FAssertMsg(NO_PLAYER != city.getOwner(), "City must have an owner");

	setYieldHelp(szBuffer, city, YIELD_PRODUCTION);

	if (city.getOwner() == GC.getGame().getActivePlayer()
	&& getBugOptionBOOL("MiscHover__BuildingAdditionalProduction", true, "BUG_BUILDING_ADDITIONAL_PRODUCTION_HOVER"))
	{
		setBuildingAdditionalYieldHelp(szBuffer, city, YIELD_PRODUCTION, DOUBLE_SEPARATOR);
	}
}


void CvGameTextMgr::parsePlayerTraits(CvWStringBuffer &szBuffer, PlayerTypes ePlayer)
{
	PROFILE_EXTRA_FUNC();

	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	{
		bool bStarted = false;
		const int iNumTraitInfos = GC.getNumTraitInfos();
		for (int i = 0; i < iNumTraitInfos; ++i)
		{
			if (kPlayer.hasTrait(static_cast<TraitTypes>(i)))
			{
				if (bStarted) szBuffer.append(L", ");
				else bStarted = true;

				szBuffer.append(GC.getTraitInfo(static_cast<TraitTypes>(i)).getDescription());
			}
		}
	}
	if (GC.getGame().isOption(GAMEOPTION_LEADER_DEVELOPING))
	{
		const int iLevel = kPlayer.getLeaderHeadLevel();
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_LEADER_LEVEL", iLevel));
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_LEADER_LEVEL_PROGRESS_1", CvWString::format(L"%I64d", kPlayer.getCulture()).GetCString()));
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_LEADER_LEVEL_PROGRESS_2", CvWString::format(L"%I64u", kPlayer.getLeaderLevelupNextCultureTotal()).GetCString(), iLevel+1));
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_LEADER_LEVEL_PROGRESS_3", CvWString::format(L"%I64u", kPlayer.getLeaderLevelupCultureToEarn()).GetCString()));
	}
	else
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_LEADER_LEVEL_PROGRESS_1", CvWString::format(L"%I64d", kPlayer.getCulture()).GetCString()));
	}
}

void CvGameTextMgr::parsePlayerHasFixedBorders(CvWStringBuffer &szBuffer, PlayerTypes ePlayer)
{
	bool bHasFixedBorders = GET_PLAYER(ePlayer).hasFixedBorders();
	szBuffer.append(gDLL->getText(bHasFixedBorders ? "TXT_KEY_PLAYER_HAS_FIXED_BORDERS" : "TXT_KEY_PLAYER_HAS_NOT_FIXED_BORDERS"));
}

void CvGameTextMgr::parseLeaderHeadHelp(CvWStringBuffer &szBuffer, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer)
{
	if (NO_PLAYER == eThisPlayer)
	{
		return;
	}

	szBuffer.append(CvWString::format(L"%s\n", GET_PLAYER(eThisPlayer).getName()));

	parsePlayerTraits(szBuffer, eThisPlayer);

	szBuffer.append(L"\n");

	if ( GC.getGame().isOption(GAMEOPTION_CULTURE_FIXED_BORDERS) )
	{
		parsePlayerHasFixedBorders(szBuffer, eThisPlayer);

		szBuffer.append(L"\n");
	}

// BUG - Leaderhead Relations - start
	PlayerTypes eActivePlayer = GC.getGame().getActivePlayer();
	TeamTypes eThisTeam = GET_PLAYER(eThisPlayer).getTeam();
	CvTeam& kThisTeam = GET_TEAM(eThisTeam);

	if (eOtherPlayer == NO_PLAYER)
	{
		eOtherPlayer = eActivePlayer;
	}
	if (eThisPlayer != eOtherPlayer && kThisTeam.isHasMet(GET_PLAYER(eOtherPlayer).getTeam()))
	{
		getEspionageString(szBuffer, eThisPlayer, eOtherPlayer);

		getAttitudeString(szBuffer, eThisPlayer, eOtherPlayer);

		if (gDLL->ctrlKey())
		{
			getActiveDealsString(szBuffer, eThisPlayer, eOtherPlayer);
		}
	}

	getAllRelationsString(szBuffer, eThisTeam);
// BUG - Leaderhead Relations - end
}


void CvGameTextMgr::getActiveDealsString(CvWStringBuffer &szBuffer, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer)
{
	PROFILE_EXTRA_FUNC();
	foreach_(CvDeal& kDeal, GC.getGame().deals())
	{
		if ((kDeal.getFirstPlayer() == eThisPlayer && kDeal.getSecondPlayer() == eOtherPlayer)
		|| (kDeal.getFirstPlayer() == eOtherPlayer && kDeal.getSecondPlayer() == eThisPlayer))
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(BULLET_CHAR)));
			getDealString(szBuffer, kDeal, eThisPlayer);
		}
	}
}

// BUG - Leaderhead Relations - start
/*
 * Shows the peace/war/enemy/pact status between eThisTeam and all rivals known to the active player.
 * Relations for the active player are shown first.
 */
void CvGameTextMgr::getAllRelationsString(CvWStringBuffer& szString, TeamTypes eThisTeam)
{
	getActiveTeamRelationsString(szString, eThisTeam);
	getOtherRelationsString(szString, eThisTeam, NO_TEAM, GC.getGame().getActiveTeam());
}

/*
 * Shows the peace/war/enemy/pact status between eThisTeam and the active player.
 */
void CvGameTextMgr::getActiveTeamRelationsString(CvWStringBuffer& szString, TeamTypes eThisTeam)
{
	CvTeamAI& kThisTeam = GET_TEAM(eThisTeam);
	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
	CvTeamAI& kActiveTeam = GET_TEAM(eActiveTeam);


	if (!kThisTeam.isHasMet(eActiveTeam))
	{
		return;
	}

	if (kThisTeam.isAtWar(eActiveTeam))
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_AT_WAR_WITH_YOU"));
	}
	else if (kThisTeam.isForcePeace(eActiveTeam))
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_PEACE_TREATY_WITH_YOU"));
	}

	if (!kThisTeam.isHuman() && kThisTeam.AI_getWorstEnemy() == eActiveTeam)
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_WORST_ENEMY_IS_YOU"));
	}

	if (kThisTeam.isDefensivePact(eActiveTeam))
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_DEFENSIVE_PACT_WITH_YOU"));
	}

	if (!kThisTeam.isAtWar(eActiveTeam))
	{

		if (kActiveTeam.AI_getWarPlan(eThisTeam) == WARPLAN_PREPARING_TOTAL)
		{
			szString.append(NEWLINE);
			szString.append(gDLL->getText(L"TXT_KEY_WARPLAN_TARGET_OF_YOU"));
		}

		if (GC.getGame().isDebugMode())
		{
			if (kThisTeam.AI_getWarPlan(eActiveTeam) == WARPLAN_PREPARING_TOTAL || kThisTeam.AI_getWarPlan(eActiveTeam) == WARPLAN_TOTAL)
			{
				szString.append(NEWLINE);
				szString.append(gDLL->getText(L"TXT_KEY_WARPLAN_TARGET_IS_YOU"));
			}
			else if (kThisTeam.AI_getWarPlan(eActiveTeam) == WARPLAN_PREPARING_LIMITED || kThisTeam.AI_getWarPlan(eActiveTeam) == WARPLAN_LIMITED)
			{
				szString.append(NEWLINE);
				szString.append(gDLL->getText(L"TXT_KEY_WARPLAN_LIMITED_TARGET_IS_YOU"));
			}
		}
	}
}

/*
 * Shows the peace/war/enemy/pact status between eThisPlayer and eOtherPlayer (both must not be NO_PLAYER).
 * If eOtherTeam is not NO_TEAM, only relations between it and eThisTeam are shown.
 * if eSkipTeam is not NO_TEAM, relations involving it are not shown.
 */
void CvGameTextMgr::getOtherRelationsString(CvWStringBuffer& szString, PlayerTypes eThisPlayer, PlayerTypes eOtherPlayer)
{
	if (eThisPlayer == NO_PLAYER || eOtherPlayer == NO_PLAYER)
	{
		return;
	}
	getOtherRelationsString(szString, GET_PLAYER(eThisPlayer).getTeam(), GET_PLAYER(eOtherPlayer).getTeam(), NO_TEAM);
}

/*
 * Shows the peace/war/enemy/pact status between eThisPlayer and all rivals known to the active player.
 * If eOtherTeam is not NO_TEAM, only relations between it and eThisTeam are shown.
 * if eSkipTeam is not NO_TEAM, relations involving it are not shown.
 */
void CvGameTextMgr::getOtherRelationsString(CvWStringBuffer& szString, TeamTypes eThisTeam, TeamTypes eOtherTeam, TeamTypes eSkipTeam)
{
	PROFILE_EXTRA_FUNC();
	if (eThisTeam == NO_TEAM)
	{
		return;
	}
	const CvTeamAI& kThisTeam = GET_TEAM(eThisTeam);
	CvWString szWar, szPeace, szEnemy, szPact, szWarPlanTotal, szWarPlanLimited;
	bool bFirstWar = true, bFirstPeace = true, bFirstEnemy = true, bFirstPact = true, bFirstWarPlanTotal = true, bFirstWarPlanLimited = true;

	for (int iTeam = 0; iTeam < MAX_PC_TEAMS; ++iTeam)
	{
		const CvTeamAI& kTeam = GET_TEAM((TeamTypes) iTeam);

		if (kTeam.isAlive() && !kTeam.isMinorCiv()
		&& iTeam != eThisTeam && iTeam != eSkipTeam
		&& (eOtherTeam == NO_TEAM || iTeam == eOtherTeam)
		&& kTeam.isHasMet(eThisTeam)
		&& kTeam.isHasMet(GC.getGame().getActiveTeam()))
		{
			if (kTeam.isAtWar(eThisTeam))
			{
				setListHelp(szWar, L"", kTeam.getName().GetCString(), L", ", bFirstWar);
				bFirstWar = false;
			}
			else if (kTeam.isForcePeace(eThisTeam))
			{
				setListHelp(szPeace, L"", kTeam.getName().GetCString(), L", ", bFirstPeace);
				bFirstPeace = false;
			}

			if (!kTeam.isHuman() && kTeam.AI_getWorstEnemy() == eThisTeam)
			{
				setListHelp(szEnemy, L"", kTeam.getName().GetCString(), L", ", bFirstEnemy);
				bFirstEnemy = false;
			}

			if (kTeam.isDefensivePact(eThisTeam))
			{
				setListHelp(szPact, L"", kTeam.getName().GetCString(), L", ", bFirstPact);
				bFirstPact = false;
			}

			//Show own war plans
			if (eThisTeam == GC.getGame().getActiveTeam() || GC.getGame().isDebugMode() && !kTeam.isAtWar(eThisTeam))
			{
				if (kThisTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_PREPARING_TOTAL || kThisTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_TOTAL)
				{
					setListHelp(szWarPlanTotal, L"", kTeam.getName().GetCString(), L", ", bFirstWarPlanTotal);
					bFirstWarPlanTotal = false;
				}
				else if (kThisTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_PREPARING_LIMITED || kThisTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_LIMITED || kThisTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_DOGPILE)
				{
					setListHelp(szWarPlanLimited, L"", kTeam.getName().GetCString(), L", ", bFirstWarPlanLimited);
					bFirstWarPlanLimited = false;
				}
			}
		}
	}

	if (!szWar.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_AT_WAR_WITH", szWar.GetCString()));
	}
	if (!kThisTeam.isHuman())
	{
		TeamTypes eWorstEnemy = kThisTeam.AI_getWorstEnemy();
		if (eWorstEnemy != NO_TEAM && eWorstEnemy != eSkipTeam && (eOtherTeam == NO_TEAM || eWorstEnemy == eOtherTeam) && GET_TEAM(eWorstEnemy).isHasMet(GC.getGame().getActiveTeam()))
		{
			szString.append(NEWLINE);
			szString.append(gDLL->getText(L"TXT_KEY_WORST_ENEMY_IS", GET_TEAM(eWorstEnemy).getName().GetCString()));
		}
	}
	if (!szEnemy.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_WORST_ENEMY_OF", szEnemy.GetCString()));
	}
	if (!szPeace.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_PEACE_TREATY_WITH", szPeace.GetCString()));
	}
	if (!szPact.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_DEFENSIVE_PACT_WITH", szPact.GetCString()));
	}
	if (!szWarPlanTotal.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_WARPLAN_TARGET_IS", szWarPlanTotal.GetCString()));
	}
	if (!szWarPlanLimited.empty())
	{
		szString.append(NEWLINE);
		szString.append(gDLL->getText(L"TXT_KEY_WARPLAN_LIMITED_TARGET_IS", szWarPlanLimited.GetCString()));
	}
}
// BUG - Leaderhead Relations - end

void CvGameTextMgr::buildHintsList(CvWStringBuffer& szBuffer)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < GC.getNumHints(); i++)
	{
		szBuffer.append(CvWString::format(L"%c%s", gDLL->getSymbolID(BULLET_CHAR), GC.getHints(i).getText()));
		szBuffer.append(NEWLINE);
		szBuffer.append(NEWLINE);
	}
}

void CvGameTextMgr::setCommerceHelp(CvWStringBuffer &szBuffer, CvCity& city, CommerceTypes eCommerceType)
{
}

void CvGameTextMgr::setYieldHelp(CvWStringBuffer &szBuffer, CvCity& city, YieldTypes eYieldType)
{
}


void CvGameTextMgr::setConvertHelp(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, ReligionTypes eReligion)
{
	CvWString szReligion = L"TXT_KEY_MISC_NO_STATE_RELIGION";

	if (eReligion != NO_RELIGION)
	{
		szReligion = GC.getReligionInfo(eReligion).getTextKeyWide();
	}

	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_CANNOT_CONVERT_TO", szReligion.GetCString()));

	if (GET_PLAYER(ePlayer).isAnarchy())
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WHILE_IN_ANARCHY"));
	}
	else if (GET_PLAYER(ePlayer).getStateReligion() == eReligion)
	{
		szBuffer.append(L". ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ALREADY_STATE_REL"));
	}
	else if (GET_PLAYER(ePlayer).getConversionTimer() > 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY"));
		szBuffer.append(L". ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WAIT_MORE_TURNS", GET_PLAYER(ePlayer).getConversionTimer()));
	}
}

void CvGameTextMgr::setRevolutionHelp(CvWStringBuffer& szBuffer, PlayerTypes ePlayer)
{
	szBuffer.assign(gDLL->getText("TXT_KEY_MISC_CANNOT_CHANGE_CIVICS"));

	if (GET_PLAYER(ePlayer).isAnarchy())
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WHILE_IN_ANARCHY"));
	}
	else if (GET_PLAYER(ePlayer).getRevolutionTimer() > 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_ANOTHER_REVOLUTION_RECENTLY"));
		szBuffer.append(L" : ");
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_WAIT_MORE_TURNS", GET_PLAYER(ePlayer).getRevolutionTimer()));
	}
}

void CvGameTextMgr::setVassalRevoltHelp(CvWStringBuffer& szBuffer, TeamTypes eMaster, TeamTypes eVassal)
{
	if (NO_TEAM == eMaster || NO_TEAM == eVassal)
	{
		return;
	}

	if (!GET_TEAM(eVassal).isCapitulated())
	{
		return;
	}

	if (GET_TEAM(eMaster).isParent(eVassal))
	{
		return;
	}

	CvTeam& kMaster = GET_TEAM(eMaster);
	CvTeam& kVassal = GET_TEAM(eVassal);

	int iMasterLand = kMaster.getTotalLand(false);
	int iVassalLand = kVassal.getTotalLand(false);
	if (iMasterLand > 0 && GC.getDefineINT("FREE_VASSAL_LAND_PERCENT") >= 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_LAND_STATS", (iVassalLand * 100) / iMasterLand, GC.getDefineINT("FREE_VASSAL_LAND_PERCENT")));
	}

	int iMasterPop = kMaster.getTotalPopulation(false);
	int iVassalPop = kVassal.getTotalPopulation(false);
	if (iMasterPop > 0 && GC.getDefineINT("FREE_VASSAL_POPULATION_PERCENT") >= 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_POPULATION_STATS", (iVassalPop * 100) / iMasterPop, GC.getDefineINT("FREE_VASSAL_POPULATION_PERCENT")));
	}

	if (GC.getDefineINT("VASSAL_REVOLT_OWN_LOSSES_FACTOR") > 0 && kVassal.getVassalPower() > 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_VASSAL_AREA_LOSS", (iVassalLand * 100) / kVassal.getVassalPower(), GC.getDefineINT("VASSAL_REVOLT_OWN_LOSSES_FACTOR")));
	}

	if (GC.getDefineINT("VASSAL_REVOLT_MASTER_LOSSES_FACTOR") > 0 && kVassal.getMasterPower() > 0)
	{
		szBuffer.append(gDLL->getText("TXT_KEY_MISC_MASTER_AREA_LOSS", (iMasterLand * 100) / kVassal.getMasterPower(), GC.getDefineINT("VASSAL_REVOLT_MASTER_LOSSES_FACTOR")));
	}
}

void CvGameTextMgr::parseGreatPeopleHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
}

// BUG - Building Additional Great People - start
bool CvGameTextMgr::setBuildingAdditionalGreatPeopleHelp(CvWStringBuffer &szBuffer, CvCity& city, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	CvWString szLabel;

	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);

		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			const int iChange = city.getAdditionalGreatPeopleRateByBuilding(eBuilding);

			if (iChange != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}

				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, gDLL->getSymbolID(GREAT_PEOPLE_CHAR), false, true);
			}
		}
	}

	return bStarted;
}
// BUG - Building Additional Great People - end


void CvGameTextMgr::parseGreatGeneralHelp(CvWStringBuffer &szBuffer, CvPlayer& kPlayer)
{
	PROFILE_EXTRA_FUNC();
	szBuffer.append(gDLL->getText("TXT_KEY_MISC_GREAT_MILITARY_PERSON", kPlayer.getCombatExperience(), kPlayer.greatPeopleThresholdMilitary(), GC.getUnitInfo(kPlayer.getGreatGeneralTypetoAssign()).getTextKeyWide()));

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		UnitTypes eGGType = ((UnitTypes)iI);
		if (kPlayer.getGreatGeneralPointsForType(eGGType) != 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_MISC_GREAT_MILITARY_PERSON_BREAKDOWN", GC.getUnitInfo(eGGType).getTextKeyWide(), kPlayer.getGreatGeneralPointsForType(eGGType)));
		}
	}
}


//------------------------------------------------------------------------------------------------

#include "AI/CvCityLogTags.h"   // citEmitBillboardPoll -- the billboard-feed trace (every entry point emits)

// The json.md §8 classification reads this file makes. The consumer holds the memoized generated-id
// (the CvUnitFilters precedent): the info exposes only the parameterized group read, never a named
// getter per key (patterns.md -- a per-key boolean getter is the shape the rebuild deletes).
void CvGameTextMgr::buildCityBillboardIconString( CvWStringBuffer& szBuffer, CvCity* pCity)
{
	citEmitBillboardPoll(8, pCity->getID());
	PROFILE_EXTRA_FUNC();
	szBuffer.clear();
/************************************************************************************************/
/* TGA_INDEXATION						  02/18/08								MRGENIE	  */
/*																							  */
/* adding link to resources in the Pedia														*/
/************************************************************************************************/

	CvString szDebugBuffer;
	CvWString szTempBuffer;
/************************************************************************************************/
/* TGA_INDEXATION						  END												  */
/************************************************************************************************/

	// government center icon
	if (pCity->isGovernmentCenter() && !(pCity->isCapital()))
	{
		szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(SILVER_STAR_CHAR)));
	}

	// happiness, healthiness, superlative icons
	if (pCity->canBeSelected())
	{
		if (pCity->angryPopulation() > 0)
		{
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(UNHAPPY_CHAR)));
		}

		if (pCity->healthRate() < 0)
		{
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(UNHEALTHY_CHAR)));
		}

		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL))
		{
			if (GET_PLAYER(pCity->getOwner()).getNumCities() > 2)
			{
				if (pCity->findYieldRateRank(YIELD_PRODUCTION) == 1)
				{
					szBuffer.append(CvWString::format(L"%c", GC.getYieldInfo(YIELD_PRODUCTION).getChar()));
				}
				if (pCity->findCommerceRateRank(COMMERCE_GOLD) == 1)
				{
					szBuffer.append(CvWString::format(L"%c", GC.getCommerceInfo(COMMERCE_GOLD).getChar()));
				}
				if (pCity->findCommerceRateRank(COMMERCE_RESEARCH) == 1)
				{
					szBuffer.append(CvWString::format(L"%c", GC.getCommerceInfo(COMMERCE_RESEARCH).getChar()));
				}
			}
		}

		if (pCity->isConnectedToCapital())
		{
			if (GET_PLAYER(pCity->getOwner()).countNumCitiesConnectedToCapital() > 1)
			{
				szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(TRADE_CHAR)));
			}
		}

		if (getBugOptionBOOL("CityBar__AirportIcons", true, "BUG_CITYBAR_AIRPORT_ICONS"))
		{
			foreach_(const BuildingTypes eTypeX, pCity->getHasBuildings())
			{
				if (GC.getBuildingInfo(eTypeX).getAirlift() > 0
				&& !pCity->isDormantBuilding(eTypeX)
				&& !pCity->isDormantBuilding(eTypeX))
				{
					szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(AIRPORT_CHAR)));
					break;
				}
			}
		}
	}

	// religion icons
	for (int iI = 0; iI < GC.getNumReligionInfos(); ++iI)
	{
		if (pCity->isHasReligion((ReligionTypes)iI))
		{
			if (pCity->isHolyCity((ReligionTypes)iI))
			{
				const CvReligionInfo& pInfo = GC.getReligionInfo((ReligionTypes) iI);
				szBuffer.append(CvWString::format(L"%c", pInfo.getHolyCityChar()));
			}
			else
			{
				const CvReligionInfo& pInfo = GC.getReligionInfo((ReligionTypes) iI);
				szBuffer.append(CvWString::format(L"%c", pInfo.getChar()));
			}
		}
	}

	// corporation icons
	for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
	{
		if (pCity->isHeadquarters((CorporationTypes)iI))
		{
			if (pCity->isHasCorporation((CorporationTypes)iI))
			{
				szBuffer.append(CvWString::format(L"%c", GC.getCorporationInfo((CorporationTypes) iI).getHeadquarterChar()));
			}
		}
		else
		{
			if (pCity->isActiveCorporation((CorporationTypes)iI))
			{
				szBuffer.append(CvWString::format(L"%c", GC.getCorporationInfo((CorporationTypes) iI).getChar()));
			}
		}
	}

	if (pCity->getTeam() == GC.getGame().getActiveTeam())
	{
		if (pCity->isPower())
		{
			szBuffer.append(CvWString::format(L"%c", gDLL->getSymbolID(POWER_CHAR)));
		}
	}

	// XXX out this in bottom bar???
	if (pCity->isOccupation())
	{
		szBuffer.append(CvWString::format(L" (%c:%d)", gDLL->getSymbolID(OCCUPATION_CHAR), pCity->getOccupationTimer()));
	}

	// defense icon and text
	//if (pCity->getTeam() != GC.getGame().getActiveTeam())
	{
		if (pCity->isVisible(GC.getGame().getActiveTeam(), true))
		{
			int iDefenseModifier = pCity->getDefenseModifier(GC.getGame().selectionListIgnoreBuildingDefense());

			if (iDefenseModifier != 0)
			{
				bool bRed = (iDefenseModifier == pCity->getExtraMinDefense());
				if (!bRed)
				{
					szBuffer.append(CvWString::format(L" %c:%s%d%%", gDLL->getSymbolID(DEFENSE_CHAR), ((iDefenseModifier > 0) ? L"+" : L""), iDefenseModifier));
				}
				else
				{
					szBuffer.append(CvWString::format(SETCOLR L" %c:%s%d%%" ENDCOLR, TEXT_COLOR("COLOR_WARNING_TEXT"), gDLL->getSymbolID(DEFENSE_CHAR), ((iDefenseModifier > 0) ? L"+" : L""), iDefenseModifier));
				}
			}
		}
	}

	if (pCity->getCivilizationType() != GET_PLAYER(pCity->getOwner()).getCivilizationType())
	{
		szBuffer.append(CvWString::format(L" (%s)", GC.getCivilizationInfo(pCity->getCivilizationType()).getShortDescription()));
	}
}

void CvGameTextMgr::buildCityBillboardCityNameString( CvWStringBuffer& szBuffer, CvCity* pCity)
{
	citEmitBillboardPoll(9, pCity->getID());
	szBuffer.assign(pCity->getName());

	if (pCity->canBeSelected())
	{
		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL))
		{
			if (pCity->foodDifference() > 0)
			{
				int iTurns = pCity->getFoodTurnsLeft();

				if ((iTurns > 1) || !(pCity->AI_isEmphasizeAvoidGrowth()))
				{
					if (iTurns < MAX_INT)
					{
						szBuffer.append(CvWString::format(L" (%d)", iTurns));
					}
				}
			}
// BUG - Starvation Turns - start
			else if (pCity->foodDifference() < 0 && getBugOptionBOOL("CityBar__StarvationTurns", true, "BUG_CITYBAR_STARVATION_TURNS"))
			{
				int iFoodDifference = pCity->foodDifference();
				if (pCity->getFood() + iFoodDifference >= 0)
				{
					int iTurns = pCity->getFood() / -iFoodDifference + 1;
					szBuffer.append(CvWString::format(L" (%d)", iTurns));
				}
				else
				{
					szBuffer.append(L" (!!!)");
				}
			}
// BUG - Starvation Turns - end
		}
	}
}

void CvGameTextMgr::buildCityBillboardProductionString( CvWStringBuffer& szBuffer, CvCity* pCity)
{
	citEmitBillboardPoll(10, pCity->getID());
	if (pCity->getOrderQueueLength() > 0)
	{
		szBuffer.assign(pCity->getProductionName());

		if (gDLL->getGraphicOption(GRAPHICOPTION_CITY_DETAIL))
		{
			int iTurns = pCity->getProductionTurnsLeft();

			if (iTurns < MAX_INT)
			{
				szBuffer.append(CvWString::format(L" (%d)", iTurns));
			}
		}
	}
	else
	{
		szBuffer.clear();
	}
}


void CvGameTextMgr::buildCityBillboardCitySizeString( CvWStringBuffer& szBuffer, CvCity* pCity, const NiColorA& kColor)
{
	citEmitBillboardPoll(11, pCity->getID());
#define CAPARAMS(c) (int)((c).r * 255.0f), (int)((c).g * 255.0f), (int)((c).b * 255.0f), (int)((c).a * 255.0f)
	szBuffer.assign(CvWString::format(SETCOLR L"%d" ENDCOLR, CAPARAMS(kColor), pCity->getPopulation()));
#undef CAPARAMS
}

void CvGameTextMgr::getCityBillboardFoodbarColors(CvCity* pCity, std::vector<NiColorA>& aColors)
{
	citEmitBillboardPoll(12, pCity->getID());
	aColors.resize(NUM_INFOBAR_TYPES);
	aColors[INFOBAR_STORED] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_FOOD).getColorType())).getColor();
	aColors[INFOBAR_RATE] = aColors[INFOBAR_STORED];
	aColors[INFOBAR_RATE].a = 0.5f;
	aColors[INFOBAR_RATE_EXTRA] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_NEGATIVE_RATE")).getColor();
	aColors[INFOBAR_EMPTY] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_EMPTY")).getColor();
}

void CvGameTextMgr::getCityBillboardProductionbarColors(CvCity* pCity, std::vector<NiColorA>& aColors)
{
	citEmitBillboardPoll(13, pCity->getID());
	aColors.resize(NUM_INFOBAR_TYPES);
	aColors[INFOBAR_STORED] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_PRODUCTION).getColorType())).getColor();
	aColors[INFOBAR_RATE] = aColors[INFOBAR_STORED];
	aColors[INFOBAR_RATE].a = 0.5f;
	aColors[INFOBAR_RATE_EXTRA] = GC.getColorInfo((ColorTypes)(GC.getYieldInfo(YIELD_FOOD).getColorType())).getColor();
	aColors[INFOBAR_RATE_EXTRA].a = 0.5f;
	aColors[INFOBAR_EMPTY] = GC.getColorInfo((ColorTypes)GC.getInfoTypeForString("COLOR_EMPTY")).getColor();
}


void CvGameTextMgr::setScoreHelp(CvWStringBuffer &szString, PlayerTypes ePlayer)
{
	if (NO_PLAYER != ePlayer)
	{
		CvPlayer& player = GET_PLAYER(ePlayer);

		int iPop = player.getPopScore();
		int iMaxPop = GC.getGame().getMaxPopulation();
		int iPopScore = 0;
		if (iMaxPop > 0)
		{
			iPopScore = (GC.getDefineINT("SCORE_POPULATION_FACTOR") * iPop) / iMaxPop;
		}
		int iLand = player.getLandScore();
		int iMaxLand = GC.getGame().getMaxLand();
		int iLandScore = 0;
		if (iMaxLand > 0)
		{
			iLandScore = (GC.getDefineINT("SCORE_LAND_FACTOR") * iLand) / iMaxLand;
		}
		int iTech = player.getTechScore();
		int iMaxTech = GC.getGame().getMaxTech();
		int iTechScore = 0;
		if (iMaxTech > 0)
		{
			iTechScore = (GC.getDefineINT("SCORE_TECH_FACTOR") * iTech) / std::max(1, iMaxTech);
		}
		int iWonders = player.getWondersScore();
		int iMaxWonders = GC.getGame().getMaxWonders();
		int iWondersScore = 0;
		if (iMaxWonders > 0)
		{
			iWondersScore = (GC.getDefineINT("SCORE_WONDER_FACTOR") * iWonders) / std::max(1, iMaxWonders);
		}
		int iTotalScore = iPopScore + iLandScore + iTechScore + iWondersScore;
		int iVictoryScore = player.calculateScore(true, true);
		szString.append(gDLL->getText("TXT_KEY_SCORE_BREAKDOWN",
			iPopScore, iPop, iMaxPop, iLandScore, iLand, iMaxLand, iTechScore,
			iTech, iMaxTech, iWondersScore, iWonders, iMaxWonders, iTotalScore, iVictoryScore)
		);
	}
}

void CvGameTextMgr::setEventHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, int iEventTriggeredId, PlayerTypes ePlayer)
{
	PROFILE_EXTRA_FUNC();
	if (NO_EVENT == eEvent || NO_PLAYER == ePlayer)
	{
		return;
	}

	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);
	CvPlayer& kActivePlayer = GET_PLAYER(ePlayer);
	const EventTriggeredData* pTriggeredData = kActivePlayer.getEventTriggered(iEventTriggeredId);

	if (!pTriggeredData)
	{
		return;
	}

	CvCity* pCity = kActivePlayer.getCity(pTriggeredData->m_iCityId);
	CvCity* pOtherPlayerCity = NULL;
	CvPlot* pPlot = GC.getMap().plot(pTriggeredData->m_iPlotX, pTriggeredData->m_iPlotY);
	CvUnit* pUnit = kActivePlayer.getUnit(pTriggeredData->m_iUnitId);

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer)
	{
		pOtherPlayerCity = GET_PLAYER(pTriggeredData->m_eOtherPlayer).getCity(pTriggeredData->m_iOtherPlayerCityId);
	}

	CvWString szCity = gDLL->getText("TXT_KEY_EVENT_THE_CITY");
	if (pCity && kEvent.isCityEffect())
	{
		szCity = pCity->getNameKey();
	}
	else if (pOtherPlayerCity && kEvent.isOtherPlayerCityEffect())
	{
		szCity = pOtherPlayerCity->getNameKey();
	}

	CvWString szUnit = gDLL->getText("TXT_KEY_EVENT_THE_UNIT");
	if (pUnit)
	{
		szUnit = pUnit->getNameKey();
	}

	CvWString szReligion = gDLL->getText("TXT_KEY_EVENT_THE_RELIGION");
	if (NO_RELIGION != pTriggeredData->m_eReligion)
	{
		szReligion = GC.getReligionInfo(pTriggeredData->m_eReligion).getTextKeyWide();
	}

	eventGoldHelp(szBuffer, eEvent, ePlayer, pTriggeredData->m_eOtherPlayer);

	eventTechHelp(szBuffer, eEvent, kActivePlayer.getBestEventTech(eEvent, pTriggeredData->m_eOtherPlayer), ePlayer, pTriggeredData->m_eOtherPlayer);

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer && NO_BONUS != kEvent.getBonusGift())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GIFT_BONUS_TO_PLAYER", GC.getBonusInfo((BonusTypes)kEvent.getBonusGift()).getTextKeyWide(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
	}

	if (kEvent.getHappy() != 0)
	{
		if (NO_PLAYER != pTriggeredData->m_eOtherPlayer)
		{
			if (kEvent.getHappy() > 0)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_FROM_PLAYER", kEvent.getHappy(), kEvent.getHappy(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_TO_PLAYER", -kEvent.getHappy(), -kEvent.getHappy(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
		}
		else
		{
			if (kEvent.getHappy() > 0)
			{
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY_CITY", kEvent.getHappy(), szCity.GetCString()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HAPPY", kEvent.getHappy()));
				}
			}
			else
			{
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHAPPY_CITY", -kEvent.getHappy(), szCity.GetCString()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHAPPY", -kEvent.getHappy()));
				}
			}
		}
	}

	if (kEvent.getHealth() != 0)
	{
		if (NO_PLAYER != pTriggeredData->m_eOtherPlayer)
		{
			if (kEvent.getHealth() > 0)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_FROM_PLAYER", kEvent.getHealth(), kEvent.getHealth(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_TO_PLAYER", -kEvent.getHealth(), -kEvent.getHealth(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
			}
		}
		else
		{
			if (kEvent.getHealth() > 0)
			{
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH_CITY", kEvent.getHealth(), szCity.GetCString()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HEALTH", kEvent.getHealth()));
				}
			}
			else
			{
				if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHEALTH", -kEvent.getHealth(), szCity.GetCString()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNHEALTH_CITY", -kEvent.getHealth()));
				}
			}
		}
	}

	if (kEvent.getHurryAnger() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HURRY_ANGER_CITY", kEvent.getHurryAnger(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_HURRY_ANGER", kEvent.getHurryAnger()));
		}
	}

	if (kEvent.getHappyTurns() > 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TEMP_HAPPY_CITY", GC.getTEMP_HAPPY(), kEvent.getHappyTurns(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TEMP_HAPPY", GC.getTEMP_HAPPY(), kEvent.getHappyTurns()));
		}
	}

	if (kEvent.getFood() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_CITY", kEvent.getFood(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD", kEvent.getFood()));
		}
	}

	if (kEvent.getFoodPercent() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_PERCENT_CITY", kEvent.getFoodPercent(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FOOD_PERCENT", kEvent.getFoodPercent()));
		}
	}

	if (kEvent.getRevoltTurns() > 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REVOLT_TURNS", kEvent.getRevoltTurns(), szCity.GetCString()));
		}
	}

	if (0 != kEvent.getSpaceProductionModifier())
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_SPACE_PRODUCTION_CITY", kEvent.getSpaceProductionModifier(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_BUILDINGHELP_SPACESHIP_MOD_ALL_CITIES", kEvent.getSpaceProductionModifier()));
		}
	}

	if (kEvent.getMaxPillage() > 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			if (kEvent.getMaxPillage() == kEvent.getMinPillage())
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_CITY", kEvent.getMinPillage(), szCity.GetCString()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_RANGE_CITY", kEvent.getMinPillage(), kEvent.getMaxPillage(), szCity.GetCString()));
			}
		}
		else
		{
			if (kEvent.getMaxPillage() == kEvent.getMinPillage())
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE", kEvent.getMinPillage()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_PILLAGE_RANGE", kEvent.getMinPillage(), kEvent.getMaxPillage()));
			}
		}
	}

	for (int i = 0; i < GC.getNumSpecialistInfos(); ++i)
	{
		if (kEvent.getFreeSpecialistCount(i) > 0)
		{
			if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FREE_SPECIALIST", kEvent.getFreeSpecialistCount(i), GC.getSpecialistInfo((SpecialistTypes)i).getTextKeyWide(), szCity.GetCString()));
			}
		}
	}

	if (kEvent.getPopulationChange() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_POPULATION_CHANGE_CITY", kEvent.getPopulationChange(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_POPULATION_CHANGE", kEvent.getPopulationChange()));
		}
	}

	if (kEvent.getCulture() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CULTURE_CITY", kEvent.getCulture(), szCity.GetCString()));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CULTURE", kEvent.getCulture()));
		}
	}

	if (kEvent.getFreeUnit() != NO_UNIT)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_UNIT", kEvent.getNumUnits(), GC.getUnitInfo((UnitTypes) kEvent.getFreeUnit()).getTextKeyWide()));
	}

	const BuildingTypes eBuilding = static_cast<BuildingTypes>(kEvent.getBuilding());
	if (eBuilding != NO_BUILDING)
	{
		if (kEvent.getBuildingChange() > 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide()));
		}
		else if (kEvent.getBuildingChange() < 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REMOVE_BUILDING", GC.getBuildingInfo(eBuilding).getTextKeyWide()));
		}
	}

	if (kEvent.getNumBuildingYieldChanges() > 0)
	{
		if (pCity)
		{
			foreach_(const BuildingTypes eType, pCity->getHasBuildings())
			{
				int aiYields[NUM_YIELD_TYPES];
				for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
				{
					aiYields[iYield] = kEvent.getBuildingYieldChange(eType, iYield);
				}
				CvWStringBuffer szYield;
				szYield.clear();
				setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
				if (!szYield.isEmpty())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_YIELD_CHANGE_BUILDING",
							GC.getBuildingInfo(eType).getTextKeyWide(),
							szYield.getCString()
						)
					);
				}
			}
		}
		else
		{
			for (int i = GC.getNumBuildingInfos() - 1; i > -1; i--)
			{
				int aiYields[NUM_YIELD_TYPES];
				for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
				{
					aiYields[iYield] = kEvent.getBuildingYieldChange(i, iYield);
				}
				CvWStringBuffer szYield;
				szYield.clear();
				setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
				if (!szYield.isEmpty())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_YIELD_CHANGE_BUILDING",
							GC.getBuildingInfo(static_cast<BuildingTypes>(i)).getTextKeyWide(),
							szYield.getCString()
						)
					);
				}
			}
		}
	}

	if (kEvent.getNumBuildingCommerceChanges() > 0)
	{
		for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
		{
			const BuildingTypes eBuilding = static_cast<BuildingTypes>(iBuilding);
			if (!pCity || pCity->hasBuilding(eBuilding))
			{
				int aiCommerces[NUM_COMMERCE_TYPES];
				for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
				{
					aiCommerces[iCommerce] = kEvent.getBuildingCommerceChange(iBuilding, iCommerce);
				}
				listCommerceChange(szBuffer, CvWString::format(L"\n%c%s: ", gDLL->getSymbolID(BULLET_CHAR), GC.getBuildingInfo(eBuilding).getDescription()), L"", aiCommerces);
			}
		}
	}

	if (kEvent.getNumBuildingHappyChanges() > 0)
	{
		if (pCity)
		{
			foreach_(const BuildingTypes eType, pCity->getHasBuildings())
			{
				const int iHappy = kEvent.getBuildingHappyChange(eType);
				if (iHappy > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(eType).getTextKeyWide(),
							iHappy, gDLL->getSymbolID(HAPPY_CHAR)
						)
					);
				}
				else if (iHappy < 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(eType).getTextKeyWide(),
							-iHappy, gDLL->getSymbolID(UNHAPPY_CHAR)
						)
					);
				}
			}
		}
		else
		{
			for (int i = GC.getNumBuildingInfos() - 1; i > -1; i--)
			{
				const int iHappy = kEvent.getBuildingHappyChange(i);
				if (iHappy > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(static_cast<BuildingTypes>(i)).getTextKeyWide(),
							iHappy, gDLL->getSymbolID(HAPPY_CHAR)
						)
					);
				}
				else if (iHappy < 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(static_cast<BuildingTypes>(i)).getTextKeyWide(),
							-iHappy, gDLL->getSymbolID(UNHAPPY_CHAR)
						)
					);
				}
			}
		}
	}

	if (kEvent.getNumBuildingHealthChanges() > 0)
	{
		if (pCity)
		{
			foreach_(const BuildingTypes eType, pCity->getHasBuildings())
			{
				const int iHealth = kEvent.getBuildingHealthChange(eType);
				if (iHealth > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(eType).getTextKeyWide(),
							iHealth, gDLL->getSymbolID(HEALTHY_CHAR)
						)
					);
				}
				else if (iHealth < 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(eType).getTextKeyWide(),
							-iHealth, gDLL->getSymbolID(UNHEALTHY_CHAR)
						)
					);
				}
			}
		}
		else
		{
			for (int i = 0; i < GC.getNumBuildingInfos(); ++i)
			{
				const int iHealth = kEvent.getBuildingHealthChange(i);
				if (iHealth > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(static_cast<BuildingTypes>(i)).getTextKeyWide(),
							iHealth, gDLL->getSymbolID(HEALTHY_CHAR)
						)
					);
				}
				else if (iHealth < 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(
						gDLL->getText(
							"TXT_KEY_EVENT_HAPPY_BUILDING",
							GC.getBuildingInfo(static_cast<BuildingTypes>(i)).getTextKeyWide(),
							-iHealth, gDLL->getSymbolID(UNHEALTHY_CHAR)
						)
					);
				}
			}
		}
	}

	if (kEvent.getRevolutionIndexChange() != 0)
	{
		if (kEvent.isCityEffect() || kEvent.isOtherPlayerCityEffect())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REVOLUTION_INDEX_CITY"));
		}
		else
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_REVOLUTION_INDEX"));
		}
	}

	if (kEvent.getFeatureChange() > 0)
	{
		if (kEvent.getFeature() != NO_FEATURE)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FEATURE_GROWTH", GC.getFeatureInfo((FeatureTypes)kEvent.getFeature()).getTextKeyWide()));
		}
	}
	else if (kEvent.getFeatureChange() < 0)
	{
		if (pPlot && NO_FEATURE != pPlot->getFeatureType())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FEATURE_REMOVE", GC.getFeatureInfo(pPlot->getFeatureType()).getTextKeyWide()));
		}
	}

	if (kEvent.getImprovementChange() > 0)
	{
		if (kEvent.getImprovement() != NO_IMPROVEMENT)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMPROVEMENT_GROWTH", GC.getImprovementInfo(kEvent.getImprovement()).getTextKeyWide()));
		}
	}
	else if (kEvent.getImprovementChange() < 0)
	{
		if (pPlot && NO_IMPROVEMENT != pPlot->getImprovementType())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMPROVEMENT_REMOVE", GC.getImprovementInfo(pPlot->getImprovementType()).getTextKeyWide()));
		}
	}

	if (kEvent.getBonusChange() > 0)
	{
		if (kEvent.getBonus() != NO_BONUS)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_GROWTH", GC.getBonusInfo((BonusTypes)kEvent.getBonus()).getTextKeyWide()));
		}
	}
	else if (kEvent.getBonusChange() < 0)
	{
		if (pPlot && NO_BONUS != pPlot->getBonusType())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_REMOVE", GC.getBonusInfo(pPlot->getBonusType()).getTextKeyWide()));
		}
	}

	if (kEvent.getRouteChange() > 0)
	{
		if (kEvent.getRoute() != NO_ROUTE)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ROUTE_GROWTH", GC.getRouteInfo((RouteTypes)kEvent.getRoute()).getTextKeyWide()));
		}
	}
	else if (kEvent.getRouteChange() < 0)
	{
		if (pPlot && NO_ROUTE != pPlot->getRouteType())
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ROUTE_REMOVE", GC.getRouteInfo(pPlot->getRouteType()).getTextKeyWide()));
		}
	}

	int aiYields[NUM_YIELD_TYPES];
	for (int i = 0; i < NUM_YIELD_TYPES; ++i)
	{
		aiYields[i] = kEvent.getPlotExtraYield(i);
	}

	CvWStringBuffer szYield;
	setYieldChangeHelp(szYield, L"", L"", L"", aiYields, false, false);
	if (!szYield.isEmpty())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_YIELD_CHANGE_PLOT", szYield.getCString()));
	}

	if (NO_BONUS != kEvent.getBonusRevealed())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_BONUS_REVEALED", GC.getBonusInfo((BonusTypes)kEvent.getBonusRevealed()).getTextKeyWide()));
	}

	if (0 != kEvent.getUnitExperience())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_EXPERIENCE", kEvent.getUnitExperience(), szUnit.GetCString()));
	}

	if (0 != kEvent.isDisbandUnit())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_DISBAND", szUnit.GetCString()));
	}

	for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
	{
		if (NO_PROMOTION != kEvent.getUnitCombatPromotion(i))
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_COMBAT_PROMOTION", GC.getUnitCombatInfo((UnitCombatTypes)i).getTextKeyWide(), GC.getPromotionInfo((PromotionTypes)kEvent.getUnitCombatPromotion(i)).getTextKeyWide()));
		}
	}

	for (int i = 0; i < GC.getNumUnitInfos(); ++i)
	{
		if (NO_PROMOTION != kEvent.getUnitPromotion(i))
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_UNIT_CLASS_PROMOTION", GC.getUnitInfo((UnitTypes) i).getTextKeyWide(), GC.getPromotionInfo((PromotionTypes)kEvent.getUnitPromotion(i)).getTextKeyWide()));
		}
	}

	if (kEvent.getConvertOwnCities() > 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CONVERT_OWN_CITIES", kEvent.getConvertOwnCities(), szReligion.GetCString()));
	}

	if (kEvent.getConvertOtherCities() > 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_CONVERT_OTHER_CITIES", kEvent.getConvertOtherCities(), szReligion.GetCString()));
	}

	if (NO_PLAYER != pTriggeredData->m_eOtherPlayer)
	{
		if (kEvent.getAttitudeModifier() > 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_GOOD", kEvent.getAttitudeModifier(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		}
		else if (kEvent.getAttitudeModifier() < 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_BAD", kEvent.getAttitudeModifier(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		}

		TeamTypes eWorstEnemy = GET_TEAM(GET_PLAYER(pTriggeredData->m_eOtherPlayer).getTeam()).AI_getWorstEnemy();
		if (NO_TEAM != eWorstEnemy)
		{
			if (kEvent.getTheirEnemyAttitudeModifier() > 0)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_GOOD", kEvent.getTheirEnemyAttitudeModifier(), GET_TEAM(eWorstEnemy).getName().GetCString()));
			}
			else if (kEvent.getTheirEnemyAttitudeModifier() < 0)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ATTITUDE_BAD", kEvent.getTheirEnemyAttitudeModifier(), GET_TEAM(eWorstEnemy).getName().GetCString()));
			}
		}

		if (kEvent.getEspionagePoints() > 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ESPIONAGE_POINTS", kEvent.getEspionagePoints(), GET_PLAYER(pTriggeredData->m_eOtherPlayer).getNameKey()));
		}
		else if (kEvent.getEspionagePoints() < 0)
		{
			szBuffer.append(NEWLINE);
			szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ESPIONAGE_COST", -kEvent.getEspionagePoints()));
		}
	}

	if (kEvent.isGoldenAge())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLDEN_AGE"));
	}

	if (0 != kEvent.getFreeUnitSupport())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_FREE_UNIT_SUPPORT", kEvent.getFreeUnitSupport()));
	}

	if (0 != kEvent.getInflationModifier())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_INFLATION_MODIFIER", kEvent.getInflationModifier()));
	}

	if (kEvent.isDeclareWar())
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_DECLARE_WAR", GET_PLAYER(pTriggeredData->m_eOtherPlayer).getCivilizationAdjectiveKey()));
	}

	if (kEvent.getUnitImmobileTurns() > 0)
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_EVENT_IMMOBILE_UNIT", kEvent.getUnitImmobileTurns(), szUnit.GetCString()));
	}

	if (kEvent.isCityEffect())
		kEvent.getProperties()->buildChangesString(szBuffer, &szCity);

	kEvent.getPropertiesAllCities()->buildChangesAllCitiesString(szBuffer);

	if (!CvWString(kEvent.getPythonHelp()).empty())
	{
		CvWString szHelp = Cy::call<CvWString>(PYRandomEventModule, kEvent.getPythonHelp(), Cy::Args() << eEvent << pTriggeredData);
		szBuffer.append(NEWLINE);
		szBuffer.append(szHelp);
	}

	CvWStringBuffer szTemp;
	for (int i = 0; i < GC.getNumEventInfos(); ++i)
	{
		if (0 == kEvent.getAdditionalEventTime(i))
		{
			if (kEvent.getAdditionalEventChance(i) > 0 && kActivePlayer.canDoEvent((EventTypes)i, *pTriggeredData))
			{
				szTemp.clear();
				setEventHelp(szTemp, (EventTypes)i, iEventTriggeredId, ePlayer);

				if (!szTemp.isEmpty())
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ADDITIONAL_CHANCE", kEvent.getAdditionalEventChance(i), L""));
					szBuffer.append(NEWLINE);
					szBuffer.append(szTemp);
				}
			}
		}
		else
		{
			szTemp.clear();
			setEventHelp(szTemp, (EventTypes)i, iEventTriggeredId, ePlayer);

			if (!szTemp.isEmpty())
			{
				CvWString szDelay = gDLL->getText("TXT_KEY_EVENT_DELAY_TURNS", kEvent.getAdditionalEventTime((EventTypes)i) * CvGameSpeedScale::speedPercent() / 100);
				szBuffer.append(NEWLINE);

				if (kEvent.getAdditionalEventChance(i) > 0)
				{
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_ADDITIONAL_CHANCE", kEvent.getAdditionalEventChance(i), szDelay.GetCString()));
				}
				else
				{
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_DELAY", szDelay.GetCString()));
				}
				szBuffer.append(NEWLINE);
				szBuffer.append(szTemp);
			}
		}
	}

	if (NO_TECH != kEvent.getPrereqTech() && !GET_TEAM(kActivePlayer.getTeam()).isHasTech(kEvent.getPrereqTech()))
	{
		szBuffer.append(NEWLINE);
		szBuffer.append(gDLL->getText("TXT_KEY_REQUIRES_LINK", CvWString(GC.getTechInfo(kEvent.getPrereqTech()).getType()).GetCString(), GC.getTechInfo(kEvent.getPrereqTech()).getTextKeyWide()));
	}

	bool done = false;
	while(!done)
	{
		done = true;
		if(!szBuffer.isEmpty())
		{
			const wchar_t* wideChar = szBuffer.getCString();
			if(wideChar[0] == L'\n')
			{
				CvWString tempString(&wideChar[1]);
				szBuffer.clear();
				szBuffer.append(tempString);
				done = false;
			}
		}
	}
}

void CvGameTextMgr::eventTechHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, TechTypes eTech, PlayerTypes eActivePlayer, PlayerTypes eOtherPlayer) const
{
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	if (eTech != NO_TECH)
	{
		if (100 == kEvent.getTechPercent())
		{
			if (NO_PLAYER != eOtherPlayer)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER", GC.getTechInfo(eTech).getTextKeyWide(), GET_PLAYER(eOtherPlayer).getNameKey()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED", GC.getTechInfo(eTech).getTextKeyWide()));
			}
		}
		else if (0 != kEvent.getTechPercent())
		{
			CvTeam& kTeam = GET_TEAM(GET_PLAYER(eActivePlayer).getTeam());
			int iBeakers = (kTeam.getResearchCost(eTech) * kEvent.getTechPercent()) / 100;
			if (kEvent.getTechPercent() > 0)
			{
				iBeakers = std::min(kTeam.getResearchLeft(eTech), iBeakers);
			}
			else if (kEvent.getTechPercent() < 0)
			{
				iBeakers = std::max(kTeam.getResearchLeft(eTech) - kTeam.getResearchCost(eTech), iBeakers);
			}

			if (NO_PLAYER != eOtherPlayer)
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_FROM_PLAYER_PERCENT", iBeakers, GC.getTechInfo(eTech).getTextKeyWide(), GET_PLAYER(eOtherPlayer).getNameKey()));
			}
			else
			{
				szBuffer.append(NEWLINE);
				szBuffer.append(gDLL->getText("TXT_KEY_EVENT_TECH_GAINED_PERCENT", iBeakers, GC.getTechInfo(eTech).getTextKeyWide()));
			}
		}
	}
}

void CvGameTextMgr::eventGoldHelp(CvWStringBuffer& szBuffer, EventTypes eEvent, PlayerTypes ePlayer, PlayerTypes eOtherPlayer) const
{
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);
	CvPlayer& kPlayer = GET_PLAYER(ePlayer);

	int iGold1 = kPlayer.getEventCost(eEvent, eOtherPlayer, false);
	int iGold2 = kPlayer.getEventCost(eEvent, eOtherPlayer, true);

	if (iGold1 != iGold2) iGold2 = abs(iGold2);

	if (0 != iGold1 || 0 != iGold2)
	{
		if (iGold1 == iGold2)
		{
			if (NO_PLAYER != eOtherPlayer && kEvent.isGoldToPlayer())
			{
				if (iGold1 > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_FROM_PLAYER", iGold1, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_TO_PLAYER", -iGold1, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
			}
			else
			{
				if (iGold1 > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_GAINED", iGold1));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_LOST", -iGold1));
				}
			}
		}
		else
		{
			if (NO_PLAYER != eOtherPlayer && kEvent.isGoldToPlayer())
			{
				if (iGold1 > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_FROM_PLAYER", iGold1, iGold2, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_TO_PLAYER", -iGold1, -iGold2, GET_PLAYER(eOtherPlayer).getNameKey()));
				}
			}
			else
			{
				if (iGold1 > 0)
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_GAINED", iGold1, iGold2));
				}
				else
				{
					szBuffer.append(NEWLINE);
					szBuffer.append(gDLL->getText("TXT_KEY_EVENT_GOLD_RANGE_LOST", -iGold1, iGold2));
				}
			}
		}
	}
}

void CvGameTextMgr::setTradeRouteHelp(CvWStringBuffer &szBuffer, int iRoute, CvCity* pCity)
{
}

void CvGameTextMgr::setEspionageCostHelp(CvWStringBuffer &szBuffer, EspionageMissionTypes eMission, PlayerTypes eTargetPlayer, const CvPlot* pPlot, int iExtraData, const CvUnit* pSpyUnit)
{
}

void CvGameTextMgr::getTradeScreenTitleIcon(CvString& szButton, CvWidgetDataStruct& widgetData, PlayerTypes ePlayer)
{
	szButton.clear();

	ReligionTypes eReligion = GET_PLAYER(ePlayer).getStateReligion();
	if (eReligion != NO_RELIGION)
	{
		szButton = GC.getReligionInfo(eReligion).getButton();
		widgetData.m_eWidgetType = WIDGET_HELP_RELIGION;
		widgetData.m_iData1 = eReligion;
		widgetData.m_iData2 = -1;
		widgetData.m_bOption = false;
	}
}

void CvGameTextMgr::getTradeScreenIcons(std::vector< std::pair<CvString, CvWidgetDataStruct> >& aIconInfos, PlayerTypes ePlayer)
{
	PROFILE_EXTRA_FUNC();
	aIconInfos.clear();
	for (int i = 0; i < GC.getNumCivicOptionInfos(); i++)
	{
		CivicTypes eCivic = GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)i);
		CvWidgetDataStruct widgetData;
		widgetData.m_eWidgetType = WIDGET_PEDIA_JUMP_TO_CIVIC;
		widgetData.m_iData1 = eCivic;
		widgetData.m_iData2 = -1;
		widgetData.m_bOption = false;
		aIconInfos.push_back(std::make_pair(GC.getCivicInfo(eCivic).getButton(), widgetData));
	}

}

void CvGameTextMgr::getTradeScreenHeader(CvWString& szHeader, PlayerTypes ePlayer, PlayerTypes eOtherPlayer, bool bAttitude)
{
	const CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	szHeader.Format(L"%s - %s", CvWString(kPlayer.getName()).GetCString(), CvWString(kPlayer.getCivilizationDescription()).GetCString());
	if (bAttitude)
	{
		szHeader += CvWString::format(L" (%s)", GC.getAttitudeInfo(kPlayer.AI_getAttitude(eOtherPlayer)).getDescription());
	}
}

// BUG - Trade Hover - start
void CvGameTextMgr::buildDomesticTradeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	buildTradeString(szBuffer, ePlayer, NO_PLAYER, true, false, false);
}

void CvGameTextMgr::buildForeignTradeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer) const
{
	buildTradeString(szBuffer, ePlayer, NO_PLAYER, false, true, false);
}

void CvGameTextMgr::buildTradeString(CvWStringBuffer& szBuffer, PlayerTypes ePlayer, PlayerTypes eWithPlayer, bool bDomestic, bool bForeign, bool bHeading) const
{
	if (NO_PLAYER == ePlayer)
	{
		return;
	}

	const CvPlayer& player = GET_PLAYER(ePlayer);
	if (bHeading)
	{
		if (ePlayer == eWithPlayer)
		{
			szBuffer.append(gDLL->getText("TXT_KEY_BUG_DOMESTIC_TRADE_HEADING"));
		}
		else if (NO_PLAYER != eWithPlayer)
		{
			if (player.canHaveTradeRoutesWith(eWithPlayer))
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_FOREIGN_TRADE_HEADING", GET_PLAYER(eWithPlayer).getNameKey(), GET_PLAYER(eWithPlayer).getCivilizationShortDescription()));
			}
			else
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_HEADING", GET_PLAYER(eWithPlayer).getNameKey(), GET_PLAYER(eWithPlayer).getCivilizationShortDescription()));
			}
		}
		else
		{
			szBuffer.append(gDLL->getText("TXT_KEY_BUG_TRADE_HEADING"));
		}
		szBuffer.append(NEWLINE);
	}

	if (NO_PLAYER != eWithPlayer)
	{
		bDomestic = ePlayer == eWithPlayer;
		bForeign = ePlayer != eWithPlayer;

		if (bForeign && !player.canHaveTradeRoutesWith(eWithPlayer))
		{
			const CvPlayer& withPlayer = GET_PLAYER(eWithPlayer);
			bool bCanTrade = true;
			if (!GET_PLAYER(eWithPlayer).isAlive())
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_DEAD"));
				return;
			}
			if (!player.canTradeNetworkWith(eWithPlayer))
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_NETWORK_NOT_CONNECTED"));
				bCanTrade = false;
			}
			if (!GET_TEAM(player.getTeam()).isFreeTrade(withPlayer.getTeam()))
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_CLOSED_BORDERS"));
				bCanTrade = false;
			}
			if (player.isNoForeignTrade())
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_FOREIGN_YOU"));
				bCanTrade = false;
			}
			if (withPlayer.isNoForeignTrade())
			{
				szBuffer.append(gDLL->getText("TXT_KEY_BUG_CANNOT_TRADE_FOREIGN_THEM"));
				bCanTrade = false;
			}

			if (!bCanTrade)
			{
				return;
			}
		}
	}

	int iDomesticYield = 0;
	int iDomesticRoutes = 0;
	int iForeignYield = 0;
	int iForeignRoutes = 0;

	player.calculateTradeTotals(YIELD_COMMERCE, iDomesticYield, iDomesticRoutes, iForeignYield, iForeignRoutes, eWithPlayer, false);

	int iTotalYield = 0;
	int iTotalRoutes = 0;
	if (bDomestic)
	{
		iTotalYield += iDomesticYield;
		iTotalRoutes += iDomesticRoutes;
	}
	if (bForeign)
	{
		iTotalYield += iForeignYield;
		iTotalRoutes += iForeignRoutes;
	}

	CvWString szYield;
// BUG - Fractional Trade Routes - start
#ifdef _MOD_FRACTRADE
	szYield.Format(L"%d.%02d", iTotalYield / 100, iTotalYield % 100);
#else
	szYield.Format(L"%d", iTotalYield);
#endif
// BUG - Fractional Trade Routes - end
	szBuffer.append(gDLL->getText("TXT_KEY_BUG_TOTAL_TRADE_YIELD", szYield.GetCString()));
	szBuffer.append(gDLL->getText("TXT_KEY_BUG_TOTAL_TRADE_ROUTES", iTotalRoutes));

	if (iTotalRoutes > 0)
	{
// BUG - Fractional Trade Routes - start
#ifdef _MOD_FRACTRADE
		int iAverage = iTotalYield / iTotalRoutes;
#else
		int iAverage = 100 * iTotalYield / iTotalRoutes;
#endif
// BUG - Fractional Trade Routes - end
		CvWString szAverage;
		szAverage.Format(L"%d.%02d", iAverage / 100, iAverage % 100);
		szBuffer.append(gDLL->getText("TXT_KEY_BUG_AVERAGE_TRADE_YIELD", szAverage.GetCString()));
	}
}
// BUG - Trade Hover - end

void CvGameTextMgr::getGlobeLayerName(GlobeLayerTypes eType, int iOption, CvWString& strName)
{
	switch (eType)
	{
	case GLOBE_LAYER_STRATEGY:
		switch(iOption)
		{
		case 0:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW");
			break;
		case 1:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_NEW_LINE");
			break;
		case 2:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_NEW_SIGN");
			break;
		case 3:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_DELETE");
			break;
		case 4:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_STRATEGY_DELETE_LINES");
			break;
		}
		break;
	case GLOBE_LAYER_UNIT:
		switch(iOption)
		{
		case SHOW_ALL_MILITARY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ALLMILITARY");
			break;
		case SHOW_TEAM_MILITARY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_TEAMMILITARY");
			break;
		case SHOW_ENEMIES_IN_TERRITORY:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ENEMY_TERRITORY_MILITARY");
			break;
		case SHOW_ENEMIES:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_ENEMYMILITARY");
			break;
		case SHOW_PLAYER_DOMESTICS:
			strName = gDLL->getText("TXT_KEY_GLOBELAYER_UNITS_DOMESTICS");
			break;
		}
		break;
	case GLOBE_LAYER_RESOURCE:
		switch(iOption)
		{
		case SHOW_RESOURCES_ALL:
			strName = gDLL->getText("TXT_KEY_ALL_RESOURCES");
			break;
		case SHOW_RESOURCES_STRATEGIC:
			strName = gDLL->getText("TXT_KEY_STRATEGIC");
			break;
		case SHOW_RESOURCES_LUXURY:
			strName = gDLL->getText("TXT_KEY_LUXURY");
			break;
		case SHOW_RESOURCES_PRODUCTION:
			strName = gDLL->getText("TXT_KEY_PRODUCTION");
			break;
		case SHOW_RESOURCES_GROWTH:
			strName = gDLL->getText("TXT_KEY_GROWTH");
			break;
		case SHOW_RESOURCES_MISC:
			strName = gDLL->getText("TXT_KEY_MISC");
			break;
		case SHOW_RESOURCES_UNCLAIMED:
			strName = gDLL->getText("TXT_KEY_UNCLAIMED");
			break;
		case SHOW_RESOURCES_CANCLAIM:
			strName = gDLL->getText("TXT_KEY_CANCLAIM");
			break;
		}
		break;
	case GLOBE_LAYER_RELIGION:
		strName = GC.getReligionInfo((ReligionTypes) iOption).getDescription();
		break;
	case GLOBE_LAYER_CULTURE:
	case GLOBE_LAYER_TRADE:
		// these have no sub-options
		strName.clear();
		break;
	}
}

void CvGameTextMgr::getPlotHelp(CvPlot* mousePlot, CvCity* city, CvPlot* flagPlot, bool bAlt, CvWStringBuffer& strHelp)
{
}

void CvGameTextMgr::getRebasePlotHelp(const CvPlot* pPlot, CvWString& strHelp) const
{
	if (pPlot)
	{
		const CvUnit* pHeadSelectedUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
		if (pHeadSelectedUnit)
		{
			if (pPlot->isFriendlyCity(*pHeadSelectedUnit, true))
			{
				const CvCity* pCity = pPlot->getPlotCity();
				if (pCity)
				{
					int iNumUnits = pCity->plot()->countNumAirUnits(GC.getGame().getActiveTeam());
					bool bFull = (iNumUnits >= pCity->getAirUnitCapacity(GC.getGame().getActiveTeam()));
					if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
					{
						iNumUnits = pCity->plot()->countNumAirUnitCargoVolume(GC.getGame().getActiveTeam());
						bFull = (iNumUnits >= pCity->getSMAirUnitCapacity(GC.getGame().getActiveTeam()));
					}

					if (bFull)
					{
						strHelp += CvWString::format(SETCOLR, TEXT_COLOR("COLOR_WARNING_TEXT"));
					}

					if (!GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
					{
						strHelp +=  NEWLINE + gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getAirUnitCapacity(GC.getGame().getActiveTeam()));
					}
					else
					{
						strHelp +=  NEWLINE + gDLL->getText("TXT_KEY_CITY_BAR_AIR_UNIT_CAPACITY", iNumUnits, pCity->getSMAirUnitCapacity(GC.getGame().getActiveTeam()));
					}

					if (bFull)
					{
						strHelp += ENDCOLR;
					}

					strHelp += NEWLINE;
				}
			}
		}
	}
}


void CvGameTextMgr::getInterfaceCenterText(CvWString& strText)
{
	strText.clear();
	if (!gDLL->getInterfaceIFace()->isCityScreenUp())
	{
		if (GC.getGame().getWinner() != NO_TEAM)
		{
			strText = gDLL->getText("TXT_KEY_MISC_WINS_VICTORY", GET_TEAM(GC.getGame().getWinner()).getName().GetCString(), GC.getVictoryInfo(GC.getGame().getVictory()).getTextKeyWide());
		}
		else if (!GET_PLAYER(GC.getGame().getActivePlayer()).isAlive())
		{
			strText = gDLL->getText("TXT_KEY_MISC_DEFEAT");
		}
	}
}

void CvGameTextMgr::getTurnTimerText(CvWString& strText)
{
	PROFILE_EXTRA_FUNC();
	strText.clear();
	if (gDLL->getInterfaceIFace()->getShowInterface() == INTERFACE_SHOW || gDLL->getInterfaceIFace()->getShowInterface() == INTERFACE_ADVANCED_START)
	{
		if (GC.getGame().isMPOption(MPOPTION_TURN_TIMER))
		{
			// Get number of turn slices remaining until end-of-turn
			int iTurnSlicesRemaining = GC.getGame().getTurnSlicesRemaining();

			if (iTurnSlicesRemaining > 0)
			{
				// Get number of seconds remaining
				int iTurnSecondsRemaining = ((int)floorf((float)(iTurnSlicesRemaining-1) * ((float)gDLL->getMillisecsPerTurn()/1000.0f)) + 1);
				int iTurnMinutesRemaining = (int)(iTurnSecondsRemaining/60);
				iTurnSecondsRemaining = (iTurnSecondsRemaining%60);
				int iTurnHoursRemaining = (int)(iTurnMinutesRemaining/60);
				iTurnMinutesRemaining = (iTurnMinutesRemaining%60);

				// Display time remaining
				CvWString szTempBuffer;
				szTempBuffer.Format(L"%d:%02d:%02d", iTurnHoursRemaining, iTurnMinutesRemaining, iTurnSecondsRemaining);
				strText += szTempBuffer;
			}
			else
			{
				// Flash zeroes
				if (iTurnSlicesRemaining % 2 == 0)
				{
					// Display 0
					strText+=L"0:00";
				}
			}
		}

		if (GC.getGame().getGameState() == GAMESTATE_ON)
		{
			int iMinVictoryTurns = MAX_INT;
			for (int i = 0; i < GC.getNumVictoryInfos(); ++i)
			{
				TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
				if (NO_TEAM != eActiveTeam)
				{
					int iCountdown = GET_TEAM(eActiveTeam).getVictoryCountdown((VictoryTypes)i);
					if (iCountdown > 0 && iCountdown < iMinVictoryTurns)
					{
						iMinVictoryTurns = iCountdown;
					}
				}
			}

			if (GC.getGame().isOption(GAMEOPTION_CORE_CUSTOM_START) && GC.getGame().getElapsedGameTurns() <= getTreatyLength())
			{
				if (!strText.empty())
				{
					strText += L" -- ";
				}

				strText += gDLL->getText("TXT_KEY_MISC_ADVANCED_START_PEACE_REMAINING", getTreatyLength() - GC.getGame().getElapsedGameTurns());
			}
			else if (iMinVictoryTurns < MAX_INT)
			{
				if (!strText.empty())
				{
					strText += L" -- ";
				}

				strText += gDLL->getText("TXT_KEY_MISC_TURNS_LEFT_TO_VICTORY", iMinVictoryTurns);
			}
			else if (GC.getGame().getMaxTurns() > 0)
			{
				if ((GC.getGame().getElapsedGameTurns() >= (GC.getGame().getMaxTurns() - 100)) && (GC.getGame().getElapsedGameTurns() < GC.getGame().getMaxTurns()))
				{
					if (!strText.empty())
					{
						strText += L" -- ";
					}

					strText += gDLL->getText("TXT_KEY_MISC_TURNS_LEFT", (GC.getGame().getMaxTurns() - GC.getGame().getElapsedGameTurns()));
				}
			}
		}
	}
}


void CvGameTextMgr::getFontSymbols(std::vector< std::vector<wchar_t> >& aacSymbols, std::vector<int>& aiMaxNumRows)
{
	PROFILE_EXTRA_FUNC();
	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(1);
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getYieldInfo((YieldTypes) iI).getChar());
	}

	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(2);
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getCommerceInfo((CommerceTypes) iI).getChar());
	}

	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(8); // There are 26 rows of 25 icons each from the start of religions to the start of the generic symbols, 23 to the beginning of property symbols
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getReligionInfo((ReligionTypes) iI).getChar());
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getReligionInfo((ReligionTypes) iI).getHolyCityChar());
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getCorporationInfo((CorporationTypes) iI).getChar());
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getCorporationInfo((CorporationTypes) iI).getHeadquarterChar());
	}
	// TB: Invisibility symbols
	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(1); // There is 1 row of 25 icons each from the start of invisibility symbols to the start of the property symbols
	for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getInvisibleInfo((InvisibleTypes) iI).getChar());
	}

	// AIAndy: Property symbols
	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(3); // There are 3 rows of 25 icons each from the start of property symbols to the start of the generic symbols
	for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) GC.getPropertyInfo((PropertyTypes) iI).getChar());
	}

	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back(2);
	for (int iI = 0; iI < MAX_NUM_SYMBOLS; iI++)
	{
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) gDLL->getSymbolID(iI));
	}

	aacSymbols.push_back(std::vector<wchar_t>());
	aiMaxNumRows.push_back((GC.getNumBonusInfos() / 25) + 1);
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		int iChar = GC.getBonusInfo((BonusTypes) iI).getChar();
		aacSymbols[aacSymbols.size() - 1].push_back((wchar_t) iChar);
	}

}

void CvGameTextMgr::assignFontIds(int iFirstSymbolCode, int iPadAmount)
{
	PROFILE_EXTRA_FUNC();
	int iCurSymbolID = iFirstSymbolCode;  // first symbol code = 8483
	int i;

	// set yield symbols
	for (i = 0; i < NUM_YIELD_TYPES; i++)
	{
		GC.getYieldInfo((YieldTypes) i).setChar(iCurSymbolID);
		++iCurSymbolID;
	}

	do
	{
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

	//8500

	// set commerce symbols
	for (i = 0; i < NUM_COMMERCE_TYPES; i++)
	{
		GC.getCommerceInfo((CommerceTypes) i).setChar(iCurSymbolID);
		++iCurSymbolID;
	}

	do
	{
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

	//8525
	if (NUM_COMMERCE_TYPES < iPadAmount)
	{
		do
		{
			++iCurSymbolID;
		} while (iCurSymbolID % iPadAmount != 0);
	}

	//8550
	for (i = 0; i < GC.getNumReligionInfos(); i++)
	{
		GC.getReligionInfo((ReligionTypes) i).setChar(iCurSymbolID);
		++iCurSymbolID;
		GC.getReligionInfo((ReligionTypes) i).setHolyCityChar(iCurSymbolID);
		++iCurSymbolID;
	}
	//int iRel = i; //138
	//8826
	for (i = 0; i < GC.getNumCorporationInfos(); i++)
	{
		GC.getCorporationInfo((CorporationTypes) i).setChar(iCurSymbolID);
		++iCurSymbolID;
		GC.getCorporationInfo((CorporationTypes) i).setHeadquarterChar(iCurSymbolID);
		++iCurSymbolID;
	}
	//int iCorp = i;//138
	//9102
	do
	{
		++iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);
	//9125
	if (2 * (GC.getNumReligionInfos() + GC.getNumCorporationInfos()) < iPadAmount)
	{
		do
		{
			++iCurSymbolID;
		} while (iCurSymbolID % iPadAmount != 0);
	}

	int iSavePosition=iCurSymbolID;
	int propertyBaseID = iSavePosition;
	// AIAndy: Property symbols
	for (i = 0; i < GC.getNumPropertyInfos(); i++)
	{
		int propertyID = propertyBaseID + GC.getPropertyInfo((PropertyTypes) i).getFontButtonIndex();
		GC.getPropertyInfo((PropertyTypes) i).setChar(propertyID);
		//++iCurSymbolID;
	}

// modified Sephi
// Symbol loading adjusted to WoC.

	// set bonus symbols
	int invisBaseID = iSavePosition -23;
	//++iCurSymbolID;
	for (i = 0; i < GC.getNumInvisibleInfos(); i++)
	{
		int invisID = invisBaseID + GC.getInvisibleInfo((InvisibleTypes)i).getFontButtonIndex();
		GC.getInvisibleInfo((InvisibleTypes) i).setChar(invisID);
		//++iCurSymbolID;
	}

	// set bonus symbols
	int bonusBaseID = iSavePosition + 125;
	//++iCurSymbolID;
	for (i = 0; i < GC.getNumBonusInfos(); i++)
	{
		int bonusID = bonusBaseID + GC.getBonusInfo((BonusTypes) i).getArtInfo()->getFontButtonIndex();
		GC.getBonusInfo((BonusTypes) i).setChar(bonusID);
		//++iCurSymbolID;
	}
	// 9206
	iCurSymbolID=iSavePosition+125;
	iCurSymbolID-=(MAX_NUM_SYMBOLS);
	do
	{
		--iCurSymbolID;
	} while (iCurSymbolID % iPadAmount != 0);

// modified Sephi
	// set extra symbols
	for (i=0; i < MAX_NUM_SYMBOLS; i++)
	{
		gDLL->setSymbolID(i, iCurSymbolID);
		++iCurSymbolID;
	}
} // 9226

void CvGameTextMgr::getCityDataForAS(std::vector<CvWBData>& mapCityList, std::vector<CvWBData>& mapBuildingList, std::vector<CvWBData>& mapAutomateList)
{
	PROFILE_EXTRA_FUNC();
	CvPlayer& kActivePlayer = GET_PLAYER(GC.getGame().getActivePlayer());

	CvWString szHelp;
	int iCost = kActivePlayer.getAdvancedStartCityCost(true);
	if (iCost > 0)
	{
		szHelp = gDLL->getText("TXT_WORD_CITY");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE");
		mapCityList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_BUTTONS_CITYSELECTION")->getPath()));
	}

	iCost = kActivePlayer.getAdvancedStartPopCost(true);
	if (iCost > 0)
	{
		szHelp = gDLL->getText("TXT_KEY_WB_AS_POPULATION");
		mapCityList.push_back(CvWBData(1, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_ANGRYCITIZEN_TEXTURE")->getPath()));
	}

	iCost = kActivePlayer.getAdvancedStartCultureCost(true);
	if (iCost > 0)
	{
		szHelp = gDLL->getText("TXT_KEY_ADVISOR_CULTURE");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE");
		mapCityList.push_back(CvWBData(2, szHelp, ARTFILEMGR.getInterfaceArtInfo("CULTURE_BUTTON")->getPath()));
	}

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);
		if (GC.getBuildingInfo(eBuilding).getFreeStartEra() == NO_ERA || GC.getGame().getStartEra() < GC.getBuildingInfo(eBuilding).getFreeStartEra())
		{
			// Building cost -1 denotes unit which may not be purchased
			iCost = kActivePlayer.getAdvancedStartBuildingCost(eBuilding, true);
			if (iCost > 0)
			{
				szBuffer.clear();
				setBuildingHelp(szBuffer, eBuilding, false);
				mapBuildingList.push_back(CvWBData(eBuilding, szBuffer.getCString(), GC.getBuildingInfo(eBuilding).getButton()));
			}
		}
	}

	szHelp = gDLL->getText("TXT_KEY_ACTION_AUTOMATE_BUILD");
	mapAutomateList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_AUTOMATE")->getPath()));
}

void CvGameTextMgr::getUnitDataForAS(std::vector<CvWBData>& mapUnitList)
{
}

void CvGameTextMgr::getImprovementDataForAS(std::vector<CvWBData>& mapImprovementList, std::vector<CvWBData>& mapRouteList)
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& kActivePlayer = GET_PLAYER(GC.getGame().getActivePlayer());

	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumRouteInfos(); i++)
	{
		const RouteTypes eRoute = (RouteTypes) i;
		if (eRoute != NO_ROUTE)
		{
			// Route cost -1 denotes route which may not be purchased
			const int iCost = kActivePlayer.getAdvancedStartRouteCost(eRoute, true);
			if (iCost > 0)
			{
				szBuffer.clear();
				setRouteHelp(szBuffer, eRoute);
				mapRouteList.push_back(CvWBData(eRoute, szBuffer.getCString(), GC.getRouteInfo(eRoute).getButton()));
			}
		}
	}

	for (int i = 0; i < GC.getNumImprovementInfos(); i++)
	{
		const ImprovementTypes eImprovement = (ImprovementTypes) i;
		if (eImprovement != NO_IMPROVEMENT)
		{
			// Improvement cost -1 denotes Improvement which may not be purchased
			const int iCost = kActivePlayer.getAdvancedStartImprovementCost(eImprovement, true);
			if (iCost > 0)
			{
				szBuffer.clear();
				setImprovementHelp(szBuffer, eImprovement);
				mapImprovementList.push_back(CvWBData(eImprovement, szBuffer.getCString(), GC.getImprovementInfo(eImprovement).getButton()));
			}
		}
	}
}

void CvGameTextMgr::getVisibilityDataForAS(std::vector<CvWBData>& mapVisibilityList)
{
	// Unit cost -1 denotes unit which may not be purchased
	const int iCost = GET_PLAYER(GC.getGame().getActivePlayer()).getAdvancedStartVisibilityCost();
	if (iCost > 0)
	{
		CvWString szHelp = gDLL->getText("TXT_KEY_WB_AS_VISIBILITY");
		szHelp += gDLL->getText("TXT_KEY_AS_UNREMOVABLE", iCost);
		mapVisibilityList.push_back(CvWBData(0, szHelp, ARTFILEMGR.getInterfaceArtInfo("INTERFACE_TECH_LOS")->getPath()));
	}
}

void CvGameTextMgr::getTechDataForAS(std::vector<CvWBData>& mapTechList)
{
	mapTechList.push_back(CvWBData(0, gDLL->getText("TXT_KEY_WB_AS_TECH"), ARTFILEMGR.getInterfaceArtInfo("INTERFACE_BTN_TECH")->getPath()));
}

void CvGameTextMgr::getUnitDataForWB(std::vector<CvWBData>& mapUnitData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;
	for (int i = 0; i < GC.getNumUnitInfos(); i++)
	{
		szBuffer.clear();
		setUnitHelp(szBuffer, (UnitTypes)i);
		mapUnitData.push_back(CvWBData(i, szBuffer.getCString(), GC.getUnitInfo((UnitTypes)i).getButton()));
	}
}

void CvGameTextMgr::getBuildingDataForWB(bool bStickyButton, std::vector<CvWBData>& mapBuildingData)
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	if (!bStickyButton)
	{
		mapBuildingData.push_back(CvWBData(iCount++, GC.getMissionInfo(MISSION_FOUND).getDescription(), GC.getMissionInfo(MISSION_FOUND).getButton()));
	}

	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumBuildingInfos(); i++)
	{
		szBuffer.clear();
		setBuildingHelp(szBuffer, (BuildingTypes)i, false);
		mapBuildingData.push_back(CvWBData(iCount++, szBuffer.getCString(), GC.getBuildingInfo((BuildingTypes)i).getButton()));
	}
}

void CvGameTextMgr::getTerrainDataForWB(std::vector<CvWBData>& mapTerrainData, std::vector<CvWBData>& mapFeatureData, std::vector<CvWBData>& mapPlotData, std::vector<CvWBData>& mapRouteData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;

	for (int i = 0; i < GC.getNumTerrainInfos(); i++)
	{
		if (!GC.getTerrainInfo((TerrainTypes)i).isGraphicalOnly())
		{
			szBuffer.clear();
			setTerrainHelp(szBuffer, (TerrainTypes)i);
			mapTerrainData.push_back(CvWBData(i, szBuffer.getCString(), GC.getTerrainInfo((TerrainTypes)i).getButton()));
		}
	}

	for (int i = 0; i < GC.getNumFeatureInfos(); i++)
	{
		for (int k = 0; k < GC.getFeatureInfo((FeatureTypes)i).getArtInfo()->getNumVarieties(); k++)
		{
			szBuffer.clear();
			setFeatureHelp(szBuffer, (FeatureTypes)i);
			mapFeatureData.push_back(CvWBData(i + GC.getNumFeatureInfos() * k, szBuffer.getCString(), GC.getFeatureInfo((FeatureTypes)i).getArtInfo()->getVariety(k).getVarietyButton()));
		}
	}

	mapPlotData.push_back(CvWBData(0, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_MOUNTAIN"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_MOUNTAIN")->getPath()));
	mapPlotData.push_back(CvWBData(1, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_HILL"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_HILL")->getPath()));
	mapPlotData.push_back(CvWBData(2, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_PLAINS"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_PLAINS")->getPath()));
	mapPlotData.push_back(CvWBData(3, gDLL->getText("TXT_KEY_WB_PLOT_TYPE_OCEAN"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_PLOT_TYPE_OCEAN")->getPath()));

	for (int i = 0; i < GC.getNumRouteInfos(); i++)
	{
		mapRouteData.push_back(CvWBData(i, GC.getRouteInfo((RouteTypes)i).getDescription(), GC.getRouteInfo((RouteTypes)i).getButton()));
	}
	mapRouteData.push_back(CvWBData(GC.getNumRouteInfos(), gDLL->getText("TXT_KEY_WB_RIVER_PLACEMENT"), ARTFILEMGR.getInterfaceArtInfo("WORLDBUILDER_RIVER_PLACEMENT")->getPath()));
}

void CvGameTextMgr::getTerritoryDataForWB(std::vector<CvWBData>& mapTerritoryData)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CvWString szName = gDLL->getText("TXT_KEY_MAIN_MENU_NONE");
		CvString szButton = GC.getCivilizationInfo(GET_PLAYER(BARBARIAN_PLAYER).getCivilizationType()).getButton();

		if (GET_PLAYER((PlayerTypes) i).isEverAlive())
		{
			szName = GET_PLAYER((PlayerTypes)i).getName();
			szButton = GC.getCivilizationInfo(GET_PLAYER((PlayerTypes)i).getCivilizationType()).getButton();
		}
		mapTerritoryData.push_back(CvWBData(i, szName, szButton));
	}
}


void CvGameTextMgr::getTechDataForWB(std::vector<CvWBData>& mapTechData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumTechInfos(); i++)
	{
		szBuffer.clear();
		setTechHelp(szBuffer, (TechTypes) i);
		mapTechData.push_back(CvWBData(i, szBuffer.getCString(), GC.getTechInfo((TechTypes) i).getButton()));
	}
}

void CvGameTextMgr::getPromotionDataForWB(std::vector<CvWBData>& mapPromotionData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumPromotionInfos(); i++)
	{
		szBuffer.clear();
		setPromotionHelp(szBuffer, (PromotionTypes) i, false);
		mapPromotionData.push_back(CvWBData(i, szBuffer.getCString(), GC.getPromotionInfo((PromotionTypes) i).getButton()));
	}
}

/*
void CvGameTextMgr::getTraitDataForWB(std::vector<CvWBData>& mapTraitData) const
{
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumTraitInfos(); i++)
	{
		szBuffer.clear();
		setTraitHelp(szBuffer, (TraitTypes) i);
		mapTraitData.push_back(CvWBData(i, szBuffer.getCString(), GC.getTraitInfo((TraitTypes) i).getButton()));
	}
}
*/

void CvGameTextMgr::getBonusDataForWB(std::vector<CvWBData>& mapBonusData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumBonusInfos(); i++)
	{
		szBuffer.clear();
		setBonusHelp(szBuffer, (BonusTypes)i);
		mapBonusData.push_back(CvWBData(i, szBuffer.getCString(), GC.getBonusInfo((BonusTypes) i).getButton()));
	}
}

void CvGameTextMgr::getImprovementDataForWB(std::vector<CvWBData>& mapImprovementData)
{
	PROFILE_EXTRA_FUNC();
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumImprovementInfos(); i++)
	{
		const CvImprovementInfo& kInfo = GC.getImprovementInfo((ImprovementTypes) i);
		if (!kInfo.isGraphicalOnly())
		{
			szBuffer.clear();
			setImprovementHelp(szBuffer, (ImprovementTypes) i);
			mapImprovementData.push_back(CvWBData(i, szBuffer.getCString(), kInfo.getButton()));
		}
	}
}

/*
void CvGameTextMgr::getRouteDataForWB(std::vector<CvWBData>& mapRouteData) const
{
	CvWStringBuffer szBuffer;
	for (int i=0; i < GC.getNumRouteInfos(); i++)
	{
		const CvRouteInfo& kInfo = GC.getRouteInfo((RouteTypes) i);
		if (!kInfo.isGraphicalOnly())
		{
			szBuffer.clear();
			setRouteHelp(szBuffer, (RouteTypes) i);
			mapRouteData.push_back(CvWBData(i, szBuffer.getCString(), kInfo.getButton()));
		}
	}
}
*/

void CvGameTextMgr::getReligionDataForWB(bool bHolyCity, std::vector<CvWBData>& mapReligionData)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < GC.getNumReligionInfos(); ++i)
	{
		const CvReligionInfo& kInfo = GC.getReligionInfo((ReligionTypes) i);
		CvWString strDescription = kInfo.getDescription();
		if (bHolyCity)
		{
			strDescription = gDLL->getText("TXT_KEY_WB_HOLYCITY", strDescription.GetCString());
		}
		mapReligionData.push_back(CvWBData(i, strDescription, kInfo.getButton()));
	}
}


void CvGameTextMgr::getCorporationDataForWB(bool bHeadquarters, std::vector<CvWBData>& mapCorporationData)
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < GC.getNumCorporationInfos(); ++i)
	{
		const CvCorporationInfo& kInfo = GC.getCorporationInfo((CorporationTypes) i);
		CvWString strDescription = kInfo.getDescription();
		if (bHeadquarters)
		{
			strDescription = gDLL->getText("TXT_KEY_CORPORATION_HEADQUARTERS", strDescription.GetCString());
		}
		mapCorporationData.push_back(CvWBData(i, strDescription, kInfo.getButton()));
	}
}


void CvGameTextMgr::buildCanPassPeaksString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildMoveFastPeaksString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildCanFoundOnPeaksString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}

void CvGameTextMgr::buildCanRebaseAnywhereString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList, bool bPlayerContext)
{
}


/*
	+50% from Buildings
	+25% from Wonders
	+10% from resources
	+25% from Civics
	+25% from Culture
	+10% from Terrain
	=======================
	Base Defense: 100%
	=======================
	Percent Siege Damaged: 25%
	Siege Damage 25%
	Current Defense: 75%
	=======================
	Base Bombard Defense: 60%
	=======================
	* Walls: +30%
*/
void CvGameTextMgr::getDefenseHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
}

bool CvGameTextMgr::setBuildingAdditionalDefenseHelp(CvWStringBuffer &szBuffer, const CvCity& city, const CvWString& szStart, bool bStarted)
{
	PROFILE_EXTRA_FUNC();
	CvWString szLabel;

	for (int i = 0; i < GC.getNumBuildingInfos(); i++)
	{
		const BuildingTypes eBuilding = static_cast<BuildingTypes>(i);
		if (city.getBuildingAvailability(eBuilding) == EnablerDomain::STATE_LISTED)
		{
			const int iChange = city.getAdditionalDefenseByBuilding(eBuilding);
			if (iChange != 0)
			{
				if (!bStarted)
				{
					szBuffer.append(szStart);
					bStarted = true;
				}
				szLabel.Format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_BUILDING_TEXT"), GC.getBuildingInfo(eBuilding).getDescription());
				setResumableValueChangeHelp(szBuffer, szLabel, L": ", L"", iChange, gDLL->getSymbolID(DEFENSE_CHAR), true, true);
			}
		}
	}
	return bStarted;
}

void CvGameTextMgr::setEmploymentHelp(CvWStringBuffer &szBuffer, CvCity& city)
{
}


void CvGameTextMgr::setFlagHelp(CvWStringBuffer &szBuffer)
{
	CvGame& GAME = GC.getGame();
	CvPlayer& player = GET_PLAYER(GAME.getActivePlayer());

	szBuffer.append(CvWString::format(SETCOLR L"%s\n", TEXT_COLOR("COLOR_HIGHLIGHT_TEXT"), player.getCivilizationDescription()));

	szBuffer.append(CvWString::format(SETCOLR L"Caveman2Cosmos %S\n" ENDCOLR, TEXT_COLOR("COLOR_YELLOW"), GC.getDefineSTRING("C2C_VERSION")));

	if (GAME.getNumHumanPlayers() > 1)
	{
		szBuffer.append(CvWString::format(SETCOLR L"%s\n" ENDCOLR, TEXT_COLOR("COLOR_MAGENTA"), gDLL->getText("TXT_KEY_SETTINGS_DIFFICULTY_PLAYER", GC.getHandicapInfo(player.getHandicapType()).getTextKeyWide()).GetCString()));
	}
	szBuffer.append(CvWString::format(SETCOLR L"%s" ENDCOLR, TEXT_COLOR("COLOR_MAGENTA"), gDLL->getText("TXT_KEY_SETTINGS_DIFFICULTY_GAME", GC.getHandicapInfo(GAME.getHandicapType()).getTextKeyWide()).GetCString()));

	// Traits
	szBuffer.append(NEWLINE L"==============================" NEWLINE);
	parsePlayerTraits(szBuffer, GAME.getActivePlayer());

	// Properties
	CvWStringBuffer szPeekBuffer;
	player.getProperties()->buildDisplayString(szPeekBuffer);

	if (!szPeekBuffer.isEmpty())
	{
		szBuffer.append(NEWLINE L"==============================" NEWLINE);
		szBuffer.append(szPeekBuffer);
		szPeekBuffer.clear();
	}
	GET_TEAM(player.getTeam()).getProperties()->buildDisplayString(szPeekBuffer);

	if (!szPeekBuffer.isEmpty())
	{
		szBuffer.append(NEWLINE L"==============================" NEWLINE);
		szBuffer.append(szPeekBuffer);
		szPeekBuffer.clear();
	}
	GAME.getProperties()->buildDisplayString(szPeekBuffer);

	if (!szPeekBuffer.isEmpty())
	{
		szBuffer.append(NEWLINE L"==============================" NEWLINE);
		szBuffer.append(szPeekBuffer);
		szPeekBuffer.clear();
	}
}


void CvGameTextMgr::buildMaintenanceModifiersString(CvWStringBuffer &szBuffer, TechTypes eTech, bool bList)
{
	if (GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_AMOUNT, CASC_SCOPE_EMPIRE) != 0)
	{
		if (bList)
		{
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_MAINT_MOD", GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_AMOUNT, CASC_SCOPE_EMPIRE)));
	}

	if (GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_DISTANCE, CASC_SCOPE_EMPIRE) != 0)
	{
		if (bList)
		{
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_DISTANCE_MAINT_MOD", GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_DISTANCE, CASC_SCOPE_EMPIRE)));
	}

	if (GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_NUM_CITIES, CASC_SCOPE_EMPIRE) != 0)
	{
		if (bList)
		{
			szBuffer.append(NEWLINE);
		}
		szBuffer.append(gDLL->getText("TXT_KEY_TECHHELP_NUM_CITIES_MAINT_MOD", GC.getTechInfo(eTech).getMaintenanceModifier(MAINTENANCE_NUM_CITIES, CASC_SCOPE_EMPIRE)));
	}
}

void CvGameTextMgr::getGameObjectName(CvWString &szString, GameObjectTypes eObject) const
{
	switch (eObject)
	{
		case GAMEOBJECT_GAME:
			szString.append(gDLL->getText("TXT_WORD_GAME"));
			break;
		case GAMEOBJECT_TEAM:
			szString.append(gDLL->getText("TXT_WORD_TEAM"));
			break;
		case GAMEOBJECT_PLAYER:
			szString.append(gDLL->getText("TXT_WORD_PLAYER"));
			break;
		case GAMEOBJECT_CITY:
			szString.append(gDLL->getText("TXT_WORD_CITY"));
			break;
		case GAMEOBJECT_UNIT:
			szString.append(gDLL->getText("TXT_WORD_UNIT"));
			break;
		case GAMEOBJECT_PLOT:
			szString.append(gDLL->getText("TXT_WORD_PLOT"));
			break;
	}
}

void CvGameTextMgr::buildGameObjectRelationString(CvWStringBuffer& szBuffer, GameObjectTypes eObject, RelationTypes eRelation, int iData) const
{
	if ((eObject == NO_GAMEOBJECT) || (eRelation == NO_RELATION))
		return;
	CvWString szObject;
	getGameObjectName(szObject, eObject);

	switch (eRelation)
	{
		case RELATION_ASSOCIATED:
			szBuffer.append(szObject);
			break;

		case RELATION_TRADE:
			szBuffer.append(gDLL->getText("TXT_KEY_RELATION_TRADE"));
			break;

		case RELATION_NEAR:
			szBuffer.append(gDLL->getText("TXT_KEY_RELATION_NEAR", szObject.c_str(), iData));
			break;

		case RELATION_SAME_PLOT:
			szBuffer.append(szObject);
			break;

		case RELATION_WORKING:
			if (eObject == GAMEOBJECT_CITY)
			{
				szBuffer.append(gDLL->getText("TXT_KEY_RELATION_WORKING_CITY"));
			}
			else
			{
				szBuffer.append(gDLL->getText("TXT_KEY_RELATION_WORKING_PLOT"));
			}
			break;
	}
}
