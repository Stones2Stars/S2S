// playerAI.cpp

#include "CvGameCoreDLL.h"
#include "Infos/CvClassificationIds.h"
#include "Cascade/CvUnitResolved.h"   // URS_WITHDRAWAL -- the unit resolved plane   // the generated SKILL_/TAG_/CAPABILITY_ id table
#include "Infos/CvGrants.h"
#include "Infos/CvTriggers.h"

#include "Data/CvInfoValuation.h"   // InfoValuation::collectHealByUnitCombat + HealByUnitCombat
#include "Engine/CvGameSpeedScale.h"
#include "Enabler/CvEnablerKernel.h"   // the compiled enables edges + the one gate (tech valuation)
#include "Enabler/CvEnablerOverlay.h"  // the AS-IF-HELD membership overlay -- the civic what-if's membership half
#include "Engine/CvArea.h"
#include "CvBonusInfo.h"
#include "CvBuildingInfo.h"
#include "Engine/CvCity.h"
#include "CvCityAI.h"
#include "Engine/CvDeal.h"
#include "UI/CvDiploParameters.h"
#include "UI/CvEventReporter.h"
#include "CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvHeritageInfo.h"
#include "CvProcessInfo.h"
#include "CvUnitCombatInfo.h"
#include "CvTraitInfo.h"
#include "CvVictoryInfo.h"
#include "Engine/CapabilityContext.h"     // the empire ability union: the commerce-slider capability map
#include "Infos/CvClassificationIds.h"      // the generated CLS_* ids a consumer reads by
#include "Infrastructure/CvInitCore.h"
#include "Engine/CvMap.h"
#include "Engine/CvPlot.h"
#include "Infrastructure/CvPathGenerator.h"
#include "CvPlayerAI.h"
#include "CvPopupInfo.h"
#include "Infrastructure/CvPython.h"
#include "Engine/CvSelectionGroup.h"
#include "CvTeamAI.h"
#include "Engine/CvUnit.h"
#include "Engine/CvUnitSelectionCriteria.h"
#include "Infrastructure/CvDLLFAStarIFaceBase.h"
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"
#include "Infrastructure/CvDLLUtilityIFaceBase.h"
#include "BetterBTSAI.h"
#include "Infrastructure/FAStarNode.h"
#include "Engine/CvArmy.h"
#include "Spine/CvEventSpine.h" // #430 logging consolidation: route [DIP]/[ESP] through the event spine (shadow)
#include "CvCityLogTags.h" // [CIT] tag enums -- the assign-dirty FAN attribution emitted from AI_makeAssignWorkDirty

// The compiler intrinsic behind the [CIT/assign/fan] caller attribution (the same one CvCityAI.cpp uses for the
// per-city line; re-declared here because the unity batching must not decide whether it is visible).
extern "C" void* _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

// #430 logging: [DIP] diplomacy/deals + [ESP] espionage -> event spine (CvPlayerAI). Both domains self-register their
// prefix providers so the spine stays domain-agnostic. Shadow discipline: emits run ALONGSIDE the legacy logDiploAI /
// logEspionageAI calls (diff on /events, then cut). Runtime-string verdict=%s fields (only "ACCEPT"/"reject") are split
// into distinct event ids per value so the constant lands in the prefix, not a string field (raw-field model has no
// string slots). Lines whose %s is runtime and cannot be enumerated are left on the legacy path only (none here).
namespace
{
	// -------------------------------------------------------------------------
	// [DIP] diplomacy / deals
	// -------------------------------------------------------------------------
	enum DipEvent
	{
		DIP_CAND = 0,                 // [DIP/cand]
		DIP_DEALVAL,                  // [DIP/dealval]
		DIP_BEGIN,                    // [DIP/begin]
		DIP_DECISION_REJECT_DENIAL,   // [DIP/decision] verdict=reject reason=denial
		DIP_SCORE,                    // [DIP/score]
		DIP_DECISION_ACCEPT_GRANT,    // [DIP/decision] verdict=ACCEPT reason=grant
		DIP_DECISION_REJECT_GRANT,    // [DIP/decision] verdict=reject reason=grant
		DIP_DECISION_ACCEPT_RENEW,    // [DIP/decision] verdict=ACCEPT reason=renew
		DIP_DECISION_REJECT_RENEW,    // [DIP/decision] verdict=reject reason=renew
		DIP_DECISION_ACCEPT,          // [DIP/decision] verdict=ACCEPT
		DIP_DECISION_REJECT,          // [DIP/decision] verdict=reject
		DIP_TRADE,                    // [DIP/trade] (CvDeal.cpp emits this id too)
		// --- war-ally purchase trace (CvPlayerAI::AI_doDiplo; migrated from C2C.log 2026-06-19, owner: keep this reasoning) ---
		DIP_WARALLY_CONSIDER,         // [DIP/warally] step=consider
		DIP_WARALLY_RANDADJ,          // [DIP/warally] step=randAdjusted
		DIP_WARALLY_PASSRAND,         // [DIP/warally] step=passedRand
		DIP_WARALLY_TEAM,             // [DIP/warally] step=bestTeam
		DIP_WARALLY_NOTEAM,           // [DIP/warally] step=noTeam
		DIP_WARALLY_TECH,             // [DIP/warally] step=bestTech
		DIP_WARALLY_NOTECH,           // [DIP/warally] step=noTech
		DIP_WARALLY_TECH2,            // [DIP/warally] step=tech2
		DIP_WARALLY_ASK,              // [DIP/warally] step=askGold
		DIP_WARALLY_OFFER,            // [DIP/warally] step=offerGold
		DIP_WARALLY_VALUES,           // [DIP/warally] step=values
		DIP_WARALLY_PROCEED           // [DIP/warally] step=proceed
	};
	const char* diploLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case DIP_CAND:                  return "[DIP/cand]";
		case DIP_DEALVAL:               return "[DIP/dealval]";
		case DIP_BEGIN:                 return "[DIP/begin]";
		case DIP_DECISION_REJECT_DENIAL:return "[DIP/decision] verdict=reject reason=denial";
		case DIP_SCORE:                 return "[DIP/score]";
		case DIP_DECISION_ACCEPT_GRANT: return "[DIP/decision] verdict=ACCEPT reason=grant";
		case DIP_DECISION_REJECT_GRANT: return "[DIP/decision] verdict=reject reason=grant";
		case DIP_DECISION_ACCEPT_RENEW: return "[DIP/decision] verdict=ACCEPT reason=renew";
		case DIP_DECISION_REJECT_RENEW: return "[DIP/decision] verdict=reject reason=renew";
		case DIP_DECISION_ACCEPT:       return "[DIP/decision] verdict=ACCEPT";
		case DIP_DECISION_REJECT:       return "[DIP/decision] verdict=reject";
		case DIP_TRADE:                 return "[DIP/trade]";
		case DIP_WARALLY_CONSIDER:      return "[DIP/warally] step=consider";
		case DIP_WARALLY_RANDADJ:       return "[DIP/warally] step=randAdjusted";
		case DIP_WARALLY_PASSRAND:      return "[DIP/warally] step=passedRand";
		case DIP_WARALLY_TEAM:          return "[DIP/warally] step=bestTeam";
		case DIP_WARALLY_NOTEAM:        return "[DIP/warally] step=noTeam";
		case DIP_WARALLY_TECH:          return "[DIP/warally] step=bestTech";
		case DIP_WARALLY_NOTECH:        return "[DIP/warally] step=noTech";
		case DIP_WARALLY_TECH2:         return "[DIP/warally] step=tech2";
		case DIP_WARALLY_ASK:           return "[DIP/warally] step=askGold";
		case DIP_WARALLY_OFFER:         return "[DIP/warally] step=offerGold";
		case DIP_WARALLY_VALUES:        return "[DIP/warally] step=values";
		case DIP_WARALLY_PROCEED:       return "[DIP/warally] step=proceed";
		default:                        return NULL;
		}
	}
	enum DipField
	{
		DIPF_player = 0, DIPF_from, DIPF_item, DIPF_data, DIPF_value,
		DIPF_items, DIPF_total, DIPF_atWar,
		DIPF_with, DIPF_give, DIPF_get, DIPF_iChange,
		DIPF_ourValue, DIPF_theirValue, DIPF_threshold,
		DIPF_to,
		// war-ally trace fields: actor/target/ally render as name(id) (SFT_PLAYER); tech as SFT_TECH; rest raw ints.
		DIPF_actor, DIPF_target, DIPF_ally, DIPF_tech, DIPF_minAtWar, DIPF_rand, DIPF_gold
	};
	const char* diploFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case DIPF_player:     return "player";
		case DIPF_from:       return "from";
		case DIPF_item:       return "item";
		case DIPF_data:       return "data";
		case DIPF_value:      return "value";
		case DIPF_items:      return "items";
		case DIPF_total:      return "total";
		case DIPF_atWar:      return "atWar";
		case DIPF_with:       return "with";
		case DIPF_give:       return "give";
		case DIPF_get:        return "get";
		case DIPF_iChange:    return "iChange";
		case DIPF_ourValue:   return "ourValue";
		case DIPF_theirValue: return "theirValue";
		case DIPF_threshold:  return "threshold";
		case DIPF_to:         return "to";
		case DIPF_actor:      *peType = SFT_PLAYER; return "actor";
		case DIPF_target:     *peType = SFT_PLAYER; return "target";
		case DIPF_ally:       *peType = SFT_PLAYER; return "ally";
		case DIPF_tech:       *peType = SFT_TECH;   return "tech";
		case DIPF_minAtWar:   return "minAtWar";
		case DIPF_rand:       return "rand";
		case DIPF_gold:       return "gold";
		default:            return NULL;
		}
	}
	struct DiploLogRegistrar { DiploLogRegistrar() { spineRegisterDomain(SD_DIPLO, &diploLinePrefix, "DiploAI.log", &diploFieldInfo); } };
	DiploLogRegistrar s_diploLogRegistrar;

	// -------------------------------------------------------------------------
	// [ESP] espionage
	// -------------------------------------------------------------------------
	enum EspEvent
	{
		ESP_BEST = 0   // [ESP/best]
	};
	const char* espionageLinePrefix(int iEventId)
	{
		switch (iEventId)
		{
		case ESP_BEST: return "[ESP/best]";
		default:       return NULL;
		}
	}
	enum EspField { ESPF_player = 0, ESPF_spyX, ESPF_spyY, ESPF_mission, ESPF_target, ESPF_value };
	const char* espionageFieldInfo(int iFieldTag, SpineFieldType* peType)
	{
		*peType = SFT_INT;
		switch (iFieldTag)
		{
		case ESPF_player:  return "player";
		case ESPF_spyX:    return "spyX";
		case ESPF_spyY:    return "spyY";
		case ESPF_mission: return "mission";
		case ESPF_target:  return "target";
		case ESPF_value:   return "value";
		default:         return NULL;
		}
	}
	struct EspionageLogRegistrar { EspionageLogRegistrar() { spineRegisterDomain(SD_ESPIONAGE, &espionageLinePrefix, "EspionageAI.log", &espionageFieldInfo); } };
	EspionageLogRegistrar s_espionageLogRegistrar;
}

// Plot danger cache
//#define DANGER_RANGE						(4)

#define GREATER_FOUND_RANGE			(5)
#define CIVIC_CHANGE_DELAY			(25)
#define RELIGION_CHANGE_DELAY		(15)

//	Koshling - save flag indicating this player has no data in the save as they have never been alive
#define	PLAYERAI_UI_FLAG_OMITTED 4

//	Koshling - to try to normalize the new tech building evaluation to the same magnitude as the old
//	(so that it doesn't change it's value as a component relative to other factors) a multiplier is needed
#define	BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER	30

// #395: the EVAL_MERGE_FACTOR blanket x2 on mergeable types is retired. It biased the
// unit mix toward mergeable types while the accounting treated every body the same;
// with strength-weighted force ledgers and need-driven merges, merge-ability no longer
// needs (or justifies) a thumb on the training scale.
// statics

CvPlayerAI* CvPlayerAI::m_aPlayers = NULL;

void CvPlayerAI::initStatics()
{
	PROFILE_EXTRA_FUNC();
	m_aPlayers = new CvPlayerAI[MAX_PLAYERS];
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aPlayers[iI].m_eID = ((PlayerTypes)iI);
	}
}

void CvPlayerAI::freeStatics()
{
	SAFE_DELETE_ARRAY(m_aPlayers);
}

bool CvPlayerAI::areStaticsInitialized()
{
	if (m_aPlayers == NULL)
	{
		return false;
	}

	return true;
}

DllExport CvPlayerAI& CvPlayerAI::getPlayerNonInl(PlayerTypes ePlayer)
{
	if (ePlayer <= NO_PLAYER || ePlayer >= MAX_PLAYERS)
	{
		return getPlayer(BARBARIAN_PLAYER);
	}
	return getPlayer(ePlayer);
}

// Public Functions...

CvPlayerAI::CvPlayerAI()
{
	PROFILE_EXTRA_FUNC();
	m_aiNumTrainAIUnits = new int[NUM_UNITAI_TYPES];
	m_aiNumAIUnits = new int[NUM_UNITAI_TYPES];
	m_aiEffNumAIUnitsTimes100 = new int[NUM_UNITAI_TYPES];
	m_aiSameReligionCounter = new int[MAX_PLAYERS];
	m_aiDifferentReligionCounter = new int[MAX_PLAYERS];
	m_aiFavoriteCivicCounter = new int[MAX_PLAYERS];
	m_aiBonusTradeCounter = new int[MAX_PLAYERS];
	m_aiPeacetimeTradeValue = new int[MAX_PLAYERS];
	m_aiPeacetimeGrantValue = new int[MAX_PLAYERS];
	m_aiGoldTradedTo = new int[MAX_PLAYERS];
	m_aiAttitudeExtra = new int[MAX_PLAYERS];

	m_abFirstContact = new bool[MAX_PLAYERS];

	m_aaiContactTimer = new int* [MAX_PLAYERS];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_aaiContactTimer[i] = new int[NUM_CONTACT_TYPES];
	}

	// @SAVEBREAK - change to MAX_PC_PLAYERS, no need to have memory of NPC actions.
	m_aaiMemoryCount = new int* [MAX_PLAYERS];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_aaiMemoryCount[i] = new int[NUM_MEMORY_TYPES];
	}
	// !SAVEBREAK

	m_aiAverageYieldMultiplier = new int[NUM_YIELD_TYPES];
	m_aiAverageCommerceMultiplier = new int[NUM_COMMERCE_TYPES];
	m_aiAverageCommerceExchange = new int[NUM_COMMERCE_TYPES];

	m_aiBonusValue = NULL;
	m_aiTradeBonusValue = NULL;
	m_abNonTradeBonusCalculated = NULL;
	m_aiUnitWeights = NULL;
	m_aiUnitCombatWeights = NULL;
	m_aiCloseBordersAttitudeCache = new int[MAX_PLAYERS];

	m_aiCivicValueCache = NULL;

	m_aiAttitudeCache = new int[MAX_PLAYERS];

	// Toffer - Transient Caches
	m_bonusClassRevealed = NULL;
	m_bonusClassUnrevealed = NULL;
	m_bonusClassHave = NULL;

	m_canTrainSettler = false;

	AI_reset(true);
}


CvPlayerAI::~CvPlayerAI()
{
	AI_uninit();

	SAFE_DELETE_ARRAY(m_aiNumTrainAIUnits);
	SAFE_DELETE_ARRAY(m_aiNumAIUnits);
	SAFE_DELETE_ARRAY(m_aiEffNumAIUnitsTimes100);
	SAFE_DELETE_ARRAY(m_aiSameReligionCounter);
	SAFE_DELETE_ARRAY(m_aiDifferentReligionCounter);
	SAFE_DELETE_ARRAY(m_aiFavoriteCivicCounter);
	SAFE_DELETE_ARRAY(m_aiBonusTradeCounter);
	SAFE_DELETE_ARRAY(m_aiPeacetimeTradeValue);
	SAFE_DELETE_ARRAY(m_aiPeacetimeGrantValue);
	SAFE_DELETE_ARRAY(m_aiGoldTradedTo);
	SAFE_DELETE_ARRAY(m_aiAttitudeExtra);
	SAFE_DELETE_ARRAY(m_abFirstContact);
	SAFE_DELETE_ARRAY2(m_aaiContactTimer, MAX_PLAYERS);
	SAFE_DELETE_ARRAY2(m_aaiMemoryCount, MAX_PLAYERS);
	SAFE_DELETE_ARRAY(m_aiAverageYieldMultiplier);
	SAFE_DELETE_ARRAY(m_aiAverageCommerceMultiplier);
	SAFE_DELETE_ARRAY(m_aiAverageCommerceExchange);
	SAFE_DELETE_ARRAY(m_aiCloseBordersAttitudeCache);
	SAFE_DELETE_ARRAY(m_aiAttitudeCache);
	// Toffer - Transient Caches
	SAFE_DELETE_ARRAY(m_bonusClassRevealed);
	SAFE_DELETE_ARRAY(m_bonusClassUnrevealed);
	SAFE_DELETE_ARRAY(m_bonusClassHave);
}


void CvPlayerAI::AI_init()
{
	AI_reset(false);

	//--------------------------------
	// Init other game data
	if ((GC.getInitCore().getSlotStatus(getID()) == SS_TAKEN) || (GC.getInitCore().getSlotStatus(getID()) == SS_COMPUTER))
	{
		FAssert(getPersonalityType() != NO_LEADER);
		AI_setPeaceWeight(GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() + GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getPeaceWeightRand(), "AI Peace Weight"));
		AI_setEspionageWeight(GC.getLeaderHeadInfo(getPersonalityType()).getEspionageWeight());
		//AI_setCivicTimer(((getMaxAnarchyTurns() == 0) ? (GC.getDefineINT("MIN_REVOLUTION_TURNS") * 2) : CIVIC_CHANGE_DELAY) / 2);
		AI_setReligionTimer(1);
		AI_setCivicTimer((getMaxAnarchyTurns() == 0) ? 1 : 2);  //from RI
		//AI_initStrategyRand(); // K-Mod
	}
}


void CvPlayerAI::AI_uninit()
{
	SAFE_DELETE_ARRAY(m_aiBonusValue);
	SAFE_DELETE_ARRAY(m_aiTradeBonusValue);
	SAFE_DELETE_ARRAY(m_abNonTradeBonusCalculated);
	SAFE_DELETE_ARRAY(m_aiUnitWeights);
	SAFE_DELETE_ARRAY(m_aiUnitCombatWeights);
	SAFE_DELETE_ARRAY(m_aiCivicValueCache);
}


void CvPlayerAI::AI_reset(bool bConstructor)
{
	PROFILE_EXTRA_FUNC();
	AI_uninit();

	m_iPeaceWeight = 0;
	m_iEspionageWeight = 0;
	m_iAttackOddsChange = 0;
	m_iCivicTimer = 0;
	m_iReligionTimer = 0;
	m_iExtraGoldTarget = 0;

	bUnitRecalcNeeded = false;
	m_bCitySitesNotCalculated = true;
	m_iCityGrowthValueBase = -1;
	m_turnsSinceLastRevolution = 50;	//	Start off at the functional max (larger makes no diff)
	m_iCivicSwitchMinDeltaThreshold = 0;

	m_eBestResearchTarget = NO_TECH;
	m_cachedTechValues.clear();

	// The AI_isFinancialTrouble memo -- cleared here because a player slot is REUSED, and a stale verdict
	// inherited from the previous occupant is a wrong answer no later derivation would correct.
	m_iFinancialTroubleCacheTurn = -1;
	m_iFinancialTroubleCacheGold = 0;
	m_bFinancialTroubleCachedValue = false;

	// Its sibling on the shared leaf, cleared for the same reason -- a reused slot must not inherit a verdict.
	m_iFundingHealthCacheTurn = -1;
	m_iFundingHealthCachedValue = 0;

	if (bConstructor || getNumUnits() == 0)
	{
		for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
		{
			m_aiNumTrainAIUnits[iI] = 0;
			m_aiNumAIUnits[iI] = 0;
			m_aiEffNumAIUnitsTimes100[iI] = 0;
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiSameReligionCounter[iI] = 0;
		m_aiDifferentReligionCounter[iI] = 0;
		m_aiFavoriteCivicCounter[iI] = 0;
		m_aiBonusTradeCounter[iI] = 0;
		m_aiPeacetimeTradeValue[iI] = 0;
		m_aiPeacetimeGrantValue[iI] = 0;
		m_aiGoldTradedTo[iI] = 0;
		m_aiAttitudeExtra[iI] = 0;
		m_abFirstContact[iI] = false;
		for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
		{
			m_aaiContactTimer[iI][iJ] = 0;
		}
		for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
		{
			m_aaiMemoryCount[iI][iJ] = 0;
		}

		if (!bConstructor && getID() != NO_PLAYER)
		{
			PlayerTypes eLoopPlayer = (PlayerTypes)iI;
			CvPlayerAI& kLoopPlayer = GET_PLAYER(eLoopPlayer);
			kLoopPlayer.m_aiSameReligionCounter[getID()] = 0;
			kLoopPlayer.m_aiDifferentReligionCounter[getID()] = 0;
			kLoopPlayer.m_aiFavoriteCivicCounter[getID()] = 0;
			kLoopPlayer.m_aiBonusTradeCounter[getID()] = 0;
			kLoopPlayer.m_aiPeacetimeTradeValue[getID()] = 0;
			kLoopPlayer.m_aiPeacetimeGrantValue[getID()] = 0;
			kLoopPlayer.m_aiGoldTradedTo[getID()] = 0;
			kLoopPlayer.m_aiAttitudeExtra[getID()] = 0;
			kLoopPlayer.m_abFirstContact[getID()] = false;
			for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
			{
				kLoopPlayer.m_aaiContactTimer[getID()][iJ] = 0;
			}
			for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
			{
				kLoopPlayer.m_aaiMemoryCount[getID()][iJ] = 0;
			}
		}
	}

	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		m_aiAverageYieldMultiplier[iI] = 0;
	}
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		m_aiAverageCommerceMultiplier[iI] = 0;
		m_aiAverageCommerceExchange[iI] = 0;
	}
	m_iAverageGreatPeopleMultiplier = 0;
	m_iAveragesCacheTurn = -1;

	m_iStrategyHash = 0;
	m_iStrategyHashCacheTurn = -1;

	m_iStrategyRand = 0;

	m_iVictoryStrategyHash = 0;
	m_iVictoryStrategyHashCacheTurn = -1;

	m_bWasFinancialTrouble = false;
	m_iTurnLastProductionDirty = -1;

	m_iUpgradeUnitsCacheTurn = -1;
	m_iUpgradeUnitsCachedExpThreshold = 0;
	m_iUpgradeUnitsCachedGold = 0;

	m_iMilitaryProductionCityCount = -1;
	m_iNavalMilitaryProductionCityCount = -1;

	FAssert(m_aiCivicValueCache == NULL);
	m_aiCivicValueCache = new int[GC.getNumCivicInfos() * 2];

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}
	m_aiAICitySites.clear();

	FAssert(m_aiBonusValue == NULL);
	FAssert(m_aiTradeBonusValue == NULL);
	m_aiBonusValue = new int[GC.getNumBonusInfos()];
	m_aiTradeBonusValue = new int[GC.getNumBonusInfos()];
	m_abNonTradeBonusCalculated = new bool[GC.getNumBonusInfos()];

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		m_aiBonusValue[iI] = -1;
		m_aiTradeBonusValue[iI] = -1;
		m_abNonTradeBonusCalculated[iI] = false;
	}

	FAssert(m_aiUnitWeights == NULL);
	m_aiUnitWeights = new int[GC.getNumUnitInfos()];

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		m_aiUnitWeights[iI] = 0;
	}

	FAssert(m_aiUnitCombatWeights == NULL);
	m_aiUnitCombatWeights = new int[GC.getNumUnitCombatInfos()];
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		m_aiUnitCombatWeights[iI] = 0;
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiCloseBordersAttitudeCache[iI] = 0;

		if (!bConstructor && getID() != NO_PLAYER)
		{
			GET_PLAYER((PlayerTypes)iI).m_aiCloseBordersAttitudeCache[getID()] = 0;
		}
		m_aiAttitudeCache[iI] = MAX_INT;

		if (!bConstructor && getID() != NO_PLAYER)
		{
			GET_PLAYER((PlayerTypes)iI).m_aiAttitudeCache[getID()] = MAX_INT;
		}
	}
	m_missionTargetCache.clear();

	if (!bConstructor && m_bonusClassRevealed == NULL)
	{
		// Can assume none of them are initialized if one of them isn't.
		FAssertMsg(m_bonusClassUnrevealed == NULL, "Memory leak");
		FAssertMsg(m_bonusClassHave == NULL, "Memory leak");
		m_bonusClassRevealed = new int[GC.getNumBonusClassInfos()];
		m_bonusClassUnrevealed = new int[GC.getNumBonusClassInfos()];
		m_bonusClassHave = new int[GC.getNumBonusClassInfos()];
	}
	m_iBonusClassTallyCachedTurn = -1;
}


int CvPlayerAI::AI_getFlavorValue(FlavorTypes eFlavor) const
{
	FAssertMsg((getPersonalityType() >= 0), "getPersonalityType() is less than zero");
	FAssertMsg((eFlavor >= 0), "eFlavor is less than zero");
	return GC.getLeaderHeadInfo(getPersonalityType()).getFlavorValue(eFlavor);
}


void CvPlayerAI::AI_doTurnPre()
{
	PROFILE_FUNC();

	m_cachedTechValues.clear();

	FAssertMsg(getPersonalityType() != NO_LEADER, "getPersonalityType() is not expected to be equal with NO_LEADER");
	FAssertMsg(getLeaderType() != NO_LEADER, "getLeaderType() is not expected to be equal with NO_LEADER");
	FAssertMsg(getCivilizationType() != NO_CIVILIZATION, "getCivilizationType() is not expected to be equal with NO_CIVILIZATION");

	// AIAndy: Calculate the strategy rand if needed
	AI_calculateStrategyRand();

	if (bUnitRecalcNeeded)
	{
		AI_recalculateUnitCounts();
	}

	//	Force recalculation of the mission target cache each turn for
	//	reliabilty reasons (more robust to bugs)
	m_missionTargetCache.clear();

#ifdef _DEBUG
	//	Validate AI unit counts
	{
		int iMilitary = 0;

		foreach_(const CvArea * pLoopArea, GC.getMap().areas())
		{
			for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
			{
				int iCount = 0;

				foreach_(const CvUnit * pLoopUnit, units())
				{
					if (pLoopUnit->isTempUnit()) continue;

					const UnitAITypes eAIType = pLoopUnit->AI_getUnitAIType();

					if ((UnitAITypes)iI == eAIType && pLoopUnit->area() == pLoopArea)
					{
						iCount++;

						if (GC.getUnitInfo(pLoopUnit->getUnitType()).hasTag(CLS_TAG_MILITARY))
						{
							iMilitary++;
						}
					}
				}

				if (iCount != 0)
				{
					OutputDebugString(CvString::format("Player %d, Area %d, unitAI %s count=%d\n", getID(), pLoopArea->getID(), GC.getUnitAIInfo((UnitAITypes)iI).getType(), iCount).c_str());
				}

				if (iCount != pLoopArea->getNumAIUnits(getID(), (UnitAITypes)iI))
				{
					FErrorMsg("UnitAI miscount");
				}
			}
		}

		FAssert(iMilitary == getNumMilitaryUnits());
	}
#endif

	AI_invalidateCloseBordersAttitudeCache();

	AI_doCounter();

	AI_invalidateAttitudeCache();

	m_numBuildingsNeeded.clear();

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}

	{ PERF_SCOPE("pre.AI_updateBonusValue", getID()); AI_updateBonusValue(); }

	{ PERF_SCOPE("pre.AI_doEnemyUnitData", getID()); AI_doEnemyUnitData(); }

	if (isHumanPlayer())
	{
		return;
	}

	//	Mark previous yield data as stale
#ifdef YIELD_VALUE_CACHING
	algo::for_each(cities(), CvCity::fn::ClearYieldValueCache());
#endif

	if (!isNPC() && getCurrentResearch() == NO_TECH)
	{
		PROFILE_FUNC()
			AI_chooseResearch();
		AI_forceUpdateStrategies(); //to account for current research.
	}

	{ PERF_SCOPE("pre.AI_doCommerce", getID()); AI_doCommerce(); }

	{ PERF_SCOPE("pre.AI_doMilitary", getID()); AI_doMilitary(); }

	{ PERF_SCOPE("pre.AI_doCivics", getID()); AI_doCivics(); }

	{ PERF_SCOPE("pre.AI_doReligion", getID()); AI_doReligion(); }

	AI_setPushReligiousVictory();
	AI_setConsiderReligiousVictory();
	AI_setHasInquisitionTarget();

	AI_doCheckFinancialTrouble();

	{ PERF_SCOPE("pre.AI_doMilitaryProductionCity", getID()); AI_doMilitaryProductionCity(); }

	if (isNPC())
	{
		return;
	}

	if (isMinorCiv())
	{
		return;
	}
}


void CvPlayerAI::AI_doTurnPost()
{
	PROFILE_FUNC();

	if (isHumanPlayer() || isNPC())
	{
		return;
	}
	if (!isMinorCiv())
	{
		AI_doDiplo();
	}

	for (int i = 0; i < GC.getNumVictoryInfos(); ++i)
	{
		AI_launch((VictoryTypes)i);
	}
}


void CvPlayerAI::AI_doTurnUnitsPre()
{
	PROFILE_FUNC();

	//	Clear cached defensive status info on each city
	algo::for_each(cities(), CvCity::fn::AI_preUnitTurn());

#ifdef PLOT_DANGER_CACHING
	//	Clear plot danger cache
	plotDangerCache.clear();
#endif

	AI_updateFoundValues(true);

	if (GC.getGame().getSorenRandNum(8, "AI Update Area Targets") == 0) // XXX personality???
	{
		AI_updateAreaTargets();
	}

	if (isHumanPlayer())
	{
		return;
	}

	if (isNPC())
	{
		return;
	}

#ifdef CVARMY_BREAKSAVE

	AI_formArmies();   // Nouvelle fonction de création des armées



	//Execute the army 
	FFreeListTrashArray<CvArmy>::iterator it;
	for (it = m_armies.begin(); it != m_armies.end(); ++it)
	{
		CvArmy& pArmy = *it;
		pArmy.doTurn();
		// Do something with pArmy
	}
#endif

	//k-mod uncommented
	if (AI_isDoStrategy(AI_STRATEGY_CRUSH) && GET_TEAM(getTeam()).getAnyWarPlanCount(true) > 0)
	{
		AI_convertUnitAITypesForCrush();
	}
}

// WIP scoring refactor
namespace {
	struct UnitAndUpgrade
	{
		UnitAndUpgrade() {}
		UnitAndUpgrade(const UnitAndUpgrade& other) : unit(other.unit), upgrade(other.upgrade) {}
		UnitAndUpgrade(CvUnit* unit, UnitTypes upgrade) : unit(unit), upgrade(upgrade) {}
		CvUnit* unit;
		UnitTypes upgrade;
	};
	typedef scoring::item_score< bst::optional< UnitAndUpgrade > > UnitUpgradeScore;

	UnitUpgradeScore scoreUpgradePrice(CvUnit* unit, int upgradeUnit)
	{
		if (unit->canUpgrade((UnitTypes)upgradeUnit))
		{
			return UnitUpgradeScore(UnitAndUpgrade(unit, (UnitTypes)upgradeUnit), unit->upgradePrice((UnitTypes)upgradeUnit));
		}
		return UnitUpgradeScore();
	}

	struct MostExpensiveUpgrade : std::unary_function<CvUnit*, UnitUpgradeScore>
	{
		MostExpensiveUpgrade() {}

		UnitUpgradeScore operator()(CvUnit* unit) const
		{
			const std::vector<int>& upgradeChain = GC.getUnitInfo(unit->getUnitType()).getUpgradeChain();
			bst::function<UnitUpgradeScore(int upgradeUnit)> sdsd = bind(scoreUpgradePrice, unit, _1);
			return algo::max_element(upgradeChain | transformed(sdsd)).get_value_or(UnitUpgradeScore());
		}
	};
}

void CvPlayerAI::AI_doTurnUnitsPost()
{
	PROFILE_FUNC();

	if (!isHumanPlayer() || isOption(PLAYEROPTION_AUTO_PROMOTION))
	{
		// Copy units as we will be removing and adding some now.
		foreach_(CvUnit * unit, units_safe() | filtered(CvUnit::fn::isPromotionReady()))
		{
			unit->AI_promote();
		}
	}

	if (isHumanPlayer())
	{
		int iMinGoldToUpgrade = getModderOption(MODDEROPTION_UPGRADE_MIN_GOLD);

		if (isModderOption(MODDEROPTION_UPGRADE_MOST_EXPENSIVE))
		{
			bool bUpgraded = true;

			// Keep looping while we have enough gold to upgrade, and we keep finding units to upgrade (cap at 2x num units to be safe)
			for (int i = 0; i < getNumUnits() * 2 && getGold() > iMinGoldToUpgrade && bUpgraded; ++i)
			{
				// Find unit with the highest cost upgrade available
				bst::optional<UnitUpgradeScore> bestUpgrade = algo::max_element(
					units() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade())
							| transformed(MostExpensiveUpgrade())
				);

				if (bestUpgrade && bestUpgrade->item)
				{
					CvUnit* unitToUpgrade = bestUpgrade->item->unit;
					const UnitTypes upgradeToApply = bestUpgrade->item->upgrade;
					bUpgraded = unitToUpgrade->upgrade(upgradeToApply);
					// Upgrade replaces the original unit with a new one, so old unit must be killed
					unitToUpgrade->doDelayedDeath();
				}
				else
				{
					bUpgraded = false;
				}

				FAssertMsg(i != getNumUnits() * 2 - 1, "Unit auto upgrade appears to have hit an infinite loop");
			}

		}
		else if (isModderOption(MODDEROPTION_UPGRADE_MOST_EXPERIENCED))
		{
			bool bUpgraded = true;

			// Keep looping while we have enough gold to upgrade, and we keep finding units to upgrade (cap at 2x num units to be safe)
			for (int i = 0; i < getNumUnits() * 2 && getGold() > iMinGoldToUpgrade && bUpgraded; ++i)
			{
				// Find unit with the highest available experience that has an upgrade available
				CvUnit* pBestUnit = nullptr;
				int iExperience = -1;
				foreach_(CvUnit * unit, units() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade()))
				{
					if (iExperience > unit->getExperience100())
						continue;

					foreach_(int iUpgrade, GC.getUnitInfo(unit->getUnitType()).getUpgradeChain())
					{
						if (unit->canUpgrade((UnitTypes)iUpgrade))
						{
							iExperience = unit->getExperience100();
							pBestUnit = unit;
							break;
						}
					}
				}

				if (pBestUnit != nullptr)
				{
					// Use AI upgrade to choose the best upgrade
					bUpgraded = pBestUnit->AI_upgrade();
					// Upgrade replaces the original unit with a new one, so old unit must be killed
					pBestUnit->doDelayedDeath();
				}
				else
				{
					bUpgraded = false;
				}
				FAssertMsg(i != getNumUnits() * 2 - 1, "Unit auto upgrade appears to have hit an infinite loop");
			}
		}
		else
		{
			// Copy units as we will be removing and adding some now.
			foreach_(CvUnit * unit, units_safe() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade()))
			{
				unit->AI_upgrade();
				// Upgrade replaces the original unit with a new one, so old unit must be killed
				unit->doDelayedDeath();
			}
		}
		return;
	}

	const bool bAnyWar = GET_TEAM(getTeam()).hasWarPlan(true);
	const int64_t iStartingGold = getGold();
	const int64_t iTargetGold = AI_goldTarget();
	int64_t iUpgradeBudget = std::max<int64_t>(iStartingGold - iTargetGold, AI_goldToUpgradeAllUnits()) / (bAnyWar ? 1 : 2);

	iUpgradeBudget = std::min<int64_t>(iUpgradeBudget, (iStartingGold - iTargetGold < iUpgradeBudget) ? (iStartingGold - iTargetGold) : iStartingGold / 2);
	iUpgradeBudget = std::max<int64_t>(0, iUpgradeBudget);

	if (AI_isFinancialTrouble())
	{
		iUpgradeBudget /= 3;
	}

	// Always willing to upgrade 1 unit if we have the money
	iUpgradeBudget = std::max<int64_t>(iUpgradeBudget, 1);

	CvPlot* pLastUpgradePlot = NULL;
	for (int iPass = 0; iPass < 3; iPass++)
	{
		if (isNPC())
		{
			iPass = 3;
		}
		foreach_(CvUnit* unitX, units())
		{
			if (unitX->isDead() || !unitX->isReadyForUpgrade())
			{
				continue;
			}
			// Koshling - never upgrade workers or subdued animals here as they typically have outcome
			//	missions and construction capabilities that must be evaluated comparatively.
			//	The UnitAI processing for these AI types handles upgrade explicitly.
			switch (unitX->AI_getUnitAIType())
			{
				case UNITAI_SUBDUED_ANIMAL:
				case UNITAI_WORKER:
					continue;
				default: break;
			}
			CvPlot* unitPlot = unitX->plot();
			bool bNoDisband = getGold() > iTargetGold;
			bool bValid = false;

			switch (iPass)
			{
				case 0:
				{
					if (unitPlot->isCity())
					{
						if (unitPlot->getBestDefender(getID()) == unitX
						// try to upgrade units which are in danger... but don't get obsessed
						|| pLastUpgradePlot != unitPlot && AI_getAnyPlotDanger(unitPlot, 1, false))
						{
							bNoDisband = true;
							bValid = true;
							pLastUpgradePlot = unitPlot;
						}
					}
					break;
				}
				case 1:
				{
					// Unit types which are limited in what terrain they can operate.
					if (AI_unitImpassableCount(unitX->getUnitType()) > 0)
					{
						bValid = true;
					}
					else if (unitX->cargoSpace() > 0
					// Only normal transports
					&&  unitX->getSpecialCargo() == NO_SPECIALUNIT
					// Also upgrade escort ships
					||  unitX->AI_getUnitAIType() == UNITAI_ESCORT_SEA)
					{
						bValid = bAnyWar || iStartingGold - getGold() < iUpgradeBudget;
					}
					break;
				}
				case 2:
				{
					bValid = iStartingGold - getGold() < iUpgradeBudget;
					break;
				}
				case 3: // Special case for NPC
				{
					bValid = true;
					bNoDisband = true;
					break;
				}
				default:
				{
					FErrorMsg("error");
					break;
				}
			}
			// Kill off units
			if (!bNoDisband && unitX->canFight() && !unitX->isAnimal() && getUnitUpkeepNet(unitX->isMilitaryBranch(), unitX->getUpkeep()) > 0)
			{
				CvCity* pPlotCity = unitPlot->getPlotCity();
				if (pPlotCity && pPlotCity->getOwner() == getID())
				{
					if ((unitX->getDomainType() != DOMAIN_LAND || unitPlot->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID()) > 1)
					// Strength-weighted surplus test (#395): merged defenders count as x1.5 per rank.
					&& unitPlot->plotCountSM(PUF_canDefend, -1, -1, NULL, getID()) > pPlotCity->AI_neededDefenders()
					&& (pPlotCity->getUnitAvailability(unitX->getUnitType()) == EnablerDomain::STATE_LISTED)
					&& (!unitX->canDefend() || !AI_getAnyPlotDanger(unitPlot, 2, false)))
					{
						// "Would a fresh unit of this type start with at least as much experience as this one has?"
						// ONE call: the city already answers exactly that, so this block's term-by-term
						// re-derivation of it was a second implementation of the same calculation
						// ([DEC-single-implementation]). Both sides are ×100, so nothing converts here.
						int iCityExp100 = 0;
						if (unitX->getExperience100() > 0)
						{
							iCityExp100 = pPlotCity->getProductionExperience(unitX->getUnitType());
						}
						if (unitX->getExperience100() <= iCityExp100)
						{
							if (unitX->hasCargo())
							{
								unitX->unloadAll();
							}
							unitX->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
							unitX->kill(true);
							pLastUpgradePlot = NULL;
							continue;
						}
					}
				}
			}
			if (bValid)
			{
				unitX->AI_upgrade(); // CAN DELETE UNIT!!!
			}
		}
	}
	if (isNPC())
	{
		return;
	}
	AI_doSplit();
}


void CvPlayerAI::AI_doPeace()
{
	PROFILE_FUNC();
	FAssert(!isHumanPlayer());
	FAssert(!isMinorCiv());
	FAssert(!isNPC());

	CLinkList<TradeData> ourList;
	CLinkList<TradeData> theirList;
	bool abContacted[MAX_TEAMS];

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		abContacted[iI] = false;
	}

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive()
		&& iI != getID()
		&& canContact((PlayerTypes)iI)
		&& AI_isWillingToTalk((PlayerTypes)iI)
		&& !GET_TEAM(getTeam()).isHuman()
		&& (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || !GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHuman())
		&& GET_TEAM(getTeam()).isAtWar(GET_PLAYER((PlayerTypes)iI).getTeam())
		&& (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (GET_TEAM(getTeam()).getLeaderID() == getID()))
		&& AI_getContactTimer((PlayerTypes)iI, CONTACT_PEACE_TREATY) == 0)
		{
			FAssertMsg(!(GET_PLAYER((PlayerTypes)iI).isNPC()), "(GET_PLAYER((PlayerTypes)iI).isNPC()) did not return false as expected");
			FAssertMsg(iI != getID(), "iI is not expected to be equal with getID()");
			FAssert(GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam());

			{
				bool bConsiderPeace;
				if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
				{
					bConsiderPeace = (
							GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER((PlayerTypes)iI).getTeam()) > 10
						||	GET_TEAM(getTeam()).getAtWarCount(false, true) > 1
						||	GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_getWarSuccess(getTeam())
						>	GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER((PlayerTypes)iI).getTeam()) * 2
					);
				}
				else
				{
					bConsiderPeace = GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER((PlayerTypes)iI).getTeam()) > 10;
				}
				if (!bConsiderPeace)
				{
					continue;
				}
			}

			bool bOffered = false;

			TradeData item;
			setTradeItem(&item, TRADE_SURRENDER);

			if (canTradeItem((PlayerTypes)iI, item, true))
			{
				ourList.clear();
				theirList.clear();

				ourList.insertAtEnd(item);

				bOffered = true;

				if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
				{
					if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
					{
						AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_TREATY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_TREATY));
						CvDiploParameters* pDiplo = new CvDiploParameters(getID());
						FAssertMsg(pDiplo, "pDiplo must be valid");
						pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_PEACE"));
						pDiplo->setAIContact(true);
						pDiplo->setOurOfferList(theirList);
						pDiplo->setTheirOfferList(ourList);
						AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
						abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
					}
				}
				else if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_acceptSurrender(getTeam()))
				{
					GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
				}
			}

			if (!bOffered && GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PEACE_TREATY), "AI Diplo Peace Treaty") == 0)
			{
				setTradeItem(&item, TRADE_PEACE_TREATY);

				if (canTradeItem((PlayerTypes)iI, item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
				{
					int iOurValue = GET_TEAM(getTeam()).AI_endWarVal(GET_PLAYER((PlayerTypes)iI).getTeam());
					int iTheirValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_endWarVal(getTeam());

					TechTypes eBestReceiveTech = NO_TECH;
					TechTypes eBestGiveTech = NO_TECH;

					int iReceiveGold = 0;
					int iGiveGold = 0;

					CvCity* pBestReceiveCity = NULL;
					CvCity* pBestGiveCity = NULL;

					if (iTheirValue > iOurValue)
					{
						int iBestValue = 0;

						// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
						// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
						// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
						std::vector<int> tradableTechs;
						getAvailableTechs(tradableTechs);

						for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
						{
							const int iJ = tradableTechs[iAt];
							setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

							if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
							{
								const int iValue = (1 + GC.getGame().getSorenRandNum(10000, "AI Peace Trading (Tech #1)"));

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestReceiveTech = (TechTypes)iJ;
								}
							}
						}

						if (eBestReceiveTech != NO_TECH)
						{
							iOurValue += GET_TEAM(getTeam()).AI_techTradeVal(eBestReceiveTech, GET_PLAYER((PlayerTypes)iI).getTeam());
						}

						const int iGold = std::min((iTheirValue - iOurValue), GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()));

						if (iGold > 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGold);

							if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
							{
								iReceiveGold = iGold;
								iOurValue += iGold;
							}
						}

						if (iTheirValue > iOurValue)
						{
							iBestValue = 0;

							foreach_(CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
							{
								setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

								if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									const int iValue = pLoopCity->plot()->calculateCulturePercent(getID());

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestReceiveCity = pLoopCity;
									}
								}
							}

							if (pBestReceiveCity)
							{
								iOurValue += AI_cityTradeVal(pBestReceiveCity);
							}
						}
					}
					else if (iOurValue > iTheirValue)
					{
						int iBestValue = 0;

						// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
						// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
						// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
						std::vector<int> tradableTechs;
						GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

						for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
						{
							const int iJ = tradableTechs[iAt];
							setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal((TechTypes)iJ, getTeam()) <= (iOurValue - iTheirValue))
								{
									const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Peace Trading (Tech #2)");

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestGiveTech = (TechTypes)iJ;
									}
								}
							}
						}

						if (eBestGiveTech != NO_TECH)
						{
							iTheirValue += GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam());
						}

						const int iGold = std::min((iOurValue - iTheirValue), AI_maxGoldTrade((PlayerTypes)iI));

						if (iGold > 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGold);

							if (canTradeItem((PlayerTypes)iI, item, true))
							{
								iGiveGold = iGold;
								iTheirValue += iGold;
							}
						}

						iBestValue = 0;

						foreach_(CvCity * pLoopCity, cities())
						{
							setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								if (GET_PLAYER((PlayerTypes)iI).AI_cityTradeVal(pLoopCity) <= (iOurValue - iTheirValue))
								{
									const int iValue = pLoopCity->plot()->calculateCulturePercent((PlayerTypes)iI);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestGiveCity = pLoopCity;
									}
								}
							}
						}

						if (pBestGiveCity)
						{
							iTheirValue += GET_PLAYER((PlayerTypes)iI).AI_cityTradeVal(pBestGiveCity);
						}
					}

					if ((GET_PLAYER((PlayerTypes)iI).isHumanPlayer()) ? (iOurValue >= iTheirValue) : ((iOurValue > ((iTheirValue * 3) / 5)) && (iTheirValue > ((iOurValue * 3) / 5))))
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_PEACE_TREATY);

						ourList.insertAtEnd(item);
						theirList.insertAtEnd(item);

						if (eBestGiveTech != NO_TECH)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
							ourList.insertAtEnd(item);
						}

						if (eBestReceiveTech != NO_TECH)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
							theirList.insertAtEnd(item);
						}

						if (iGiveGold != 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGiveGold);
							ourList.insertAtEnd(item);
						}

						if (iReceiveGold != 0)
						{
							setTradeItem(&item, TRADE_GOLD, iReceiveGold);
							theirList.insertAtEnd(item);
						}

						if (pBestGiveCity)
						{
							setTradeItem(&item, TRADE_CITIES, pBestGiveCity->getID());
							ourList.insertAtEnd(item);
						}

						if (pBestReceiveCity)
						{
							setTradeItem(&item, TRADE_CITIES, pBestReceiveCity->getID());
							theirList.insertAtEnd(item);
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_TREATY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_TREATY));
								CvDiploParameters* pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_PEACE"));
								pDiplo->setAIContact(true);
								pDiplo->setOurOfferList(theirList);
								pDiplo->setTheirOfferList(ourList);
								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else
						{
							GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
						}
					}
				}
			}
		}
	}
}

void CvPlayerAI::AI_updateFoundValues(bool bClear, const CvArea* area) const
{
	PROFILE_FUNC();

	const bool bSetup = getNumCities() == 0;

	if (bClear)
	{
		m_bCitySitesNotCalculated = true;

		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* plotX = GC.getMap().plotByIndex(iI);

			if (bSetup || plotX->isRevealed(getTeam(), false))
			{
				GC.getMap().plotByIndex(iI)->clearFoundValue(getID());
			}
		}
		algo::for_each(GC.getMap().areas(), CvArea::fn::setBestFoundValue(getID(), -1));
		return;
	}
	std::vector<CvArea*> aUncalculatedAreas;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		CvPlot* plotX = GC.getMap().plotByIndex(iI);
		CvArea* areaX = plotX->area();

		if ((area == NULL || areaX == area) && (bSetup || plotX->isRevealed(getTeam(), false)))
		{
			bool bNeedsCalculating = false;

			if (!areaX->hasBestFoundValue(getID()))
			{
				bNeedsCalculating = true;
				areaX->setBestFoundValue(getID(), 0);
				aUncalculatedAreas.push_back(areaX);
			}
			else if (algo::any_of_equal(aUncalculatedAreas, areaX))
			{
				bNeedsCalculating = true;
			}

			if (bNeedsCalculating)
			{
				const int iValue = AI_foundValue(plotX->getX(), plotX->getY());

				plotX->setFoundValue(getID(), iValue);

				if (iValue > areaX->getBestFoundValue(getID()))
				{
					areaX->setBestFoundValue(getID(), iValue);
				}
			}
		}
	}
}


void CvPlayerAI::AI_updateAreaTargets()
{
	PROFILE_EXTRA_FUNC();
	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		if (!pLoopArea->isWater())
		{
			if (GC.getGame().getSorenRandNum(3, "AI Target City") == 0)
			{
				pLoopArea->setTargetCity(getID(), NULL);
			}
			else pLoopArea->setTargetCity(getID(), AI_findTargetCity(pLoopArea));
		}
	}
}


// Returns priority for unit movement (lower values move first...)
int CvPlayerAI::AI_movementPriority(const CvSelectionGroup* pGroup) const
{
	const CvUnit* pHeadUnit = pGroup->getHeadUnit();

	if (pHeadUnit == NULL)
	{
		return 100; // Lowest priority
	}
	if (pHeadUnit->isSpy())
	{
		return 0; // Highest priority
	}

	if (pHeadUnit->hasCargo())
	{
		// General transport before specialized
		return pHeadUnit->getSpecialCargo() == NO_SPECIALUNIT ? 1 : 2;
	}

	if (pHeadUnit->getDomainType() == DOMAIN_AIR)
	{
		// Fighters before bombers, they are better at clearing out air defenses
		return pHeadUnit->canAirDefend() ? 3 : 4;
	}

	if (pHeadUnit->AI_getUnitAIType() == UNITAI_WORKER || pHeadUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA)
	{
		return 5;
	}

	if (pHeadUnit->AI_getUnitAIType() == UNITAI_EXPLORE || pHeadUnit->AI_getUnitAIType() == UNITAI_EXPLORE_SEA)
	{
		return 6;
	}

	if (pHeadUnit->getBombardRate() > 0)
	{
		return 7;
	}

	if (pHeadUnit->collateralDamage() > 0)
	{
		return 8;
	}

	if (pHeadUnit->canFight())
	{
		// Skirmishers should harass before regular fighters engage
		const int iWithdraw = pHeadUnit->withdrawalProbability();
		if (iWithdraw > 49)
		{
			return 9;
		}
		if (iWithdraw > 39)
		{
			return 10;
		}
		if (iWithdraw > 29)
		{
			return 11;
		}
		if (iWithdraw > 19)
		{
			return 12;
		}
		if (iWithdraw > 9)
		{
			return 13;
		}

		int iCurrCombat = pHeadUnit->currCombatStr(NULL, NULL);

		if (pHeadUnit->noDefensiveBonus())
		{
			// Move poor defenders first
			iCurrCombat *= 3;
			iCurrCombat /= 2;
		}

		if (pHeadUnit->AI_isCityAIType())
		{
			iCurrCombat /= 2;
		}
		const int iBestCombat = 100 * GC.getGame().getBestLandUnitCombat();

		if (iCurrCombat > iBestCombat)
		{
			return 20;
		}
		if (iCurrCombat > iBestCombat * 4 / 5)
		{
			return 25;
		}
		if (iCurrCombat > iBestCombat * 3 / 5)
		{
			return 30;
		}
		if (iCurrCombat > iBestCombat * 2 / 5)
		{
			return 35;
		}
		if (iCurrCombat > iBestCombat / 5)
		{
			return 40;
		}
		return 45;
	}
	return 50;
}


void CvPlayerAI::AI_unitUpdate()
{
	PROFILE_FUNC();

	//	Do delayed death here so that empty groups are removed now
	foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		pLoopSelectionGroup->doDelayedDeath(); // could destroy the selection group...
	}

	//	Should have the same set of groups as is represented by m_selectionGroups
	//	as (indirectly via their head units) by m_groupCycles.  These have been seen to
	//	get out of step, which results in a WFoC so if they contain differing member
	//	counts go through and fix it!
	//	Note - this is fixing a symptom rather than a cause which is distasteful, but as
	//	yet the cause remains elusive
	if (m_groupCycles[CURRENT_MAP]->getLength() != m_selectionGroups[CURRENT_MAP]->getCount() - (m_pTempUnit ? 1 : 0))
	{
		/*
		if (m_pTempUnit)
		{
			FAssert(m_pTempUnit->getGroup() != NULL);
			OutputDebugString(CvString::format("temp group id is %d\n", m_pTempUnit->getGroup()->getID()).c_str());
		}
		*/
		OutputDebugString("Group cycle:\n");
		for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL; pCurrUnitNode = nextGroupCycleNode(pCurrUnitNode))
		{
			CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
			OutputDebugString(CvString::format("	%d with %d units at (%d,%d)\n",
				pLoopSelectionGroup->getID(),
				pLoopSelectionGroup->getNumUnits(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getX(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getY()).c_str());
		}

		FAssert(m_selectionGroups[CURRENT_MAP]->getCount() > m_groupCycles[CURRENT_MAP]->getLength());	//	Other way round not seen - not handled currently
		OutputDebugString("Selection groups:\n");
		foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
		{
			OutputDebugString(CvString::format("	%d with %d units at (%d,%d)\n",
				pLoopSelectionGroup->getID(),
				pLoopSelectionGroup->getNumUnits(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getX(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getY()).c_str());
			if (pLoopSelectionGroup->getHeadUnit() != m_pTempUnit)
			{
				updateGroupCycle(pLoopSelectionGroup->getHeadUnit(), true);
			}
		}
	}

	if (!hasBusyUnit())
	{
		for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL;)
		{
			CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
			CLLNode<int>* pNextUnitNode = nextGroupCycleNode(pCurrUnitNode);

			//	Since we know the set of selection groups can (somehow - reason is unknown) get out of step
			//	with the set in the update cycle (hence the code in the section above this one that copes with
			//	selection groups that don't have group cycle entries), it follows that the converse may also
			//	occur.  Thus we must be prepared to handle cycle entries with no corresponding selection group
			//	still extant
			if (pLoopSelectionGroup == NULL)
			{
				deleteGroupCycleNode(pCurrUnitNode);
			}
			else if (pLoopSelectionGroup->AI_isForceSeparate())
			{
				// do not split groups that are in the midst of attacking
				if (pLoopSelectionGroup->isForceUpdate() || !pLoopSelectionGroup->AI_isGroupAttack())
				{
					pLoopSelectionGroup->AI_separate();	// pointers could become invalid...
				}
			}

			pCurrUnitNode = pNextUnitNode;
		}

		if (isHumanPlayer())
		{
			for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL;)
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
				CLLNode<int>* pNextUnitNode = nextGroupCycleNode(pCurrUnitNode);

				if (pLoopSelectionGroup == NULL)
				{
					deleteGroupCycleNode(pCurrUnitNode);
				}
				else if (pLoopSelectionGroup->AI_update())
				{
					break; // pointers could become invalid...
				}
				pCurrUnitNode = pNextUnitNode;
			}
		}
		else
		{
			std::vector< std::pair<int, int> > groupList;
			//Define a Priority Sorting (see AI_movementPriority)
			for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL; pCurrUnitNode = nextGroupCycleNode(pCurrUnitNode))
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
				FAssert(pLoopSelectionGroup != NULL);

				int iPriority = AI_movementPriority(pLoopSelectionGroup);
				groupList.push_back(std::make_pair(iPriority, pCurrUnitNode->m_data));
			}

			algo::sort(groupList);

			for (size_t i = 0; i < groupList.size(); i++)
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(groupList[i].second);

				if (pLoopSelectionGroup && pLoopSelectionGroup->AI_update())
				{
					FAssert(pLoopSelectionGroup && pLoopSelectionGroup->getNumUnits() > 0);
					break;
				}
			}
		}
	}
}


void CvPlayerAI::AI_makeAssignWorkDirty()
{
	// The FAN's own attribution. The per-city [CIT/assign/dirty] line cannot serve it: its return address is the
	// for_each below, so every whole-scope caller reports one identical RVA. Captured HERE, the address is the
	// site that actually asked for the fan, and one line covers the whole sweep instead of one per city.
	{
		static const char* s_pModuleBase = (const char*)GetModuleHandle("CvGameCoreDLL.dll");
		int iCities = 0;
		foreach_(const CvCity* pLoopCity, cities())
		{
			(void)pLoopCity;
			++iCities;
		}
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_CITY, CIT_ASSIGN_FAN, 3)
			.addI(CITF_owner, getID())
			.addI(CITF_cities, iCities)
			.addI(CITF_callerRva, (int)((const char*)_ReturnAddress() - s_pModuleBase)));
	}
	algo::for_each(cities(), CvCity::fn::AI_setAssignWorkDirty(true));
}


void CvPlayerAI::AI_assignWorkingPlots()
{
	if (isAnarchy())
	{
		return; // No point
	}
	algo::for_each(cities(), CvCity::fn::AI_assignWorkingPlots());
}


void CvPlayerAI::AI_updateAssignWork()
{
	algo::for_each(cities(), CvCity::fn::AI_updateAssignWork());
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  05/08/09								jdog5000	  */
/*																							  */
/* City AI																					  */
/************************************************************************************************/
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/


void CvPlayerAI::AI_makeProductionDirty()
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	algo::for_each(cities(), CvCity::fn::AI_setChooseProductionDirty(true));
}

// War tactics AI
void CvPlayerAI::AI_conquerCity(PlayerTypes eOldOwner, CvCity* pCity, bool bConquest, bool bTrade)
{
	PROFILE_EXTRA_FUNC();
	bool bRaze = false;

	if ( // Can raze
			!GC.getGame().isOption(GAMEOPTION_NO_CITY_RAZING)
		&& ( // Can't raze if same ID ever owned, or if random chance. Can raze max culture level cities.
			!pCity->isEverOwned(getID())
			|| pCity->calculateTeamCulturePercent(getTeam()) < GC.getDefineINT("RAZING_CULTURAL_PERCENT_THRESHOLD")
			)
	)
	{
		int iRazeValue = 0;
		const int iCloseness = pCity->AI_playerCloseness(getID());

		// Reasons to always raze
		if (GC.getGame().culturalVictoryValid() && GET_TEAM(getTeam()).AI_getEnemyPowerPercent(false) > 75)
		{
			const int iHighCultureThreshold = GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel()) * 3/5;

			if (pCity->getCulture(eOldOwner) > iHighCultureThreshold)
			{
				int iHighCultureCount = 1;

				foreach_(const CvCity * cityX, GET_PLAYER(eOldOwner).cities())
				{
					if (cityX->getCulture(eOldOwner) > iHighCultureThreshold)
					{
						iHighCultureCount++;
						if (iHighCultureCount >= GC.getGame().culturalVictoryNumCultureCities())
						{
							//Raze city enemy needs for cultural victory unless we greatly over power them
							OutputDebugString("  Razing enemy cultural victory city");
							bRaze = true;
						}
					}
				}
			}
		}

		if (!bRaze)
		{
			// Reasons to not raze
			if (getNumCities() <= 1 || getNumCities() < 5 && iCloseness > 0)
			{
				// Do not raze, few cities
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3) && GET_TEAM(getTeam()).AI_isPrimaryArea(pCity->area()))
			{
				// Do not raze, going for domination
			}
			else if (isHominid())
			{
				if (!pCity->isHolyCity() && !pCity->hasActiveWorldWonder()
				&& eOldOwner != BARBARIAN_PLAYER && eOldOwner != NEANDERTHAL_PLAYER
				&& pCity->getOriginalOwner() != BARBARIAN_PLAYER && pCity->getOriginalOwner() != NEANDERTHAL_PLAYER)
				{
					iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
					iRazeValue -= iCloseness;
				}
			}
			else
			{
				const bool bFinancialTrouble = AI_isFinancialTrouble();
				const bool bPrevOwnerBarb = (eOldOwner == BARBARIAN_PLAYER || eOldOwner == NEANDERTHAL_PLAYER);
				const bool bBarbCity = bPrevOwnerBarb && (pCity->getOriginalOwner() == BARBARIAN_PLAYER || pCity->getOriginalOwner() == NEANDERTHAL_PLAYER);

				if (GET_TEAM(getTeam()).countNumCitiesByArea(pCity->area()) == 0)
				{
					// Conquered city in new continent/island
					int iBestValue;
					if (pCity->area()->getNumCities() == 1 && AI_getNumAreaCitySites(pCity->area()->getID(), iBestValue) == 0)
					{
						if (iCloseness == 0) // Probably small island
						{
							// Safe to raze these now that AI can do pick up ...
							iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
						}
					}
					else if (iCloseness < 10) // At least medium sized island
					{
						if (bFinancialTrouble)
						{// Raze if we might start incuring colony maintenance
							iRazeValue = 100;
						}
						else if (eOldOwner != NO_PLAYER && !bPrevOwnerBarb
						&& GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).countNumCitiesByArea(pCity->area()) > 3)
						{
							if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
							{
								iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
							}
							else
							{
								iRazeValue += std::max(0, (GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb() - iCloseness));
							}
						}
					}
				}
				else
				{
					// Distance related aspects
					if (iCloseness > 0)
					{
						iRazeValue -= iCloseness;
					}
					else
					{
						iRazeValue += 40;

						CvCity* pNearestTeamAreaCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), NO_PLAYER, getTeam(), true, false, NO_TEAM, NO_DIRECTION, pCity);

						if (pNearestTeamAreaCity == NULL)
						{
							// Shouldn't happen
							iRazeValue += 30;
						}
						else
						{
							const int iDistance = plotDistance(pCity->getX(), pCity->getY(), pNearestTeamAreaCity->getX(), pNearestTeamAreaCity->getY()) - DEFAULT_PLAYER_CLOSENESS - 2;
							if (iDistance > 0)
							{
								iRazeValue += iDistance * (bBarbCity ? 8 : 5);
							}
						}
					}

					if (bFinancialTrouble)
					{
						iRazeValue += std::max(0, (70 - 15 * pCity->getPopulation()));
					}

					// Scale down distance/maintenance effects for organized
					if (iRazeValue > 0)
					{
						for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
						{
							if (hasTrait((TraitTypes)iI))
							{
								iRazeValue *= (100 - (GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier(UPKEEP_CIVIC, CASC_SCOPE_EMPIRE)));
								iRazeValue /= 100;
							}
						}
					}

					// Non-distance related aspects
					if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
					{
						iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
					}
					else
					{
						iRazeValue += std::max(0, ((GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb() / 2) - iCloseness));
					}
					if (getStateReligion() != NO_RELIGION && pCity->isHasReligion(getStateReligion()))
					{
						if (GET_TEAM(getTeam()).hasShrine(getStateReligion()))
						{
							iRazeValue -= 50;
						}
						else
						{
							iRazeValue -= 10;
						}
					}
				}


				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (pCity->isHolyCity((ReligionTypes)iI))
					{
						OutputDebugString("	  Reduction for holy city");

						if (getStateReligion() == iI)
						{
							iRazeValue -= 150;
						}
						else
						{
							iRazeValue -= 5 + GC.getGame().calculateReligionPercent((ReligionTypes)iI);
						}
					}
				}

				iRazeValue -= 25 * pCity->getNumActiveWorldWonders();

				iRazeValue -= pCity->calculateTeamCulturePercent(getTeam());

				for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
				{
					const CvPlot* pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);

					if (pLoopPlot != NULL && pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
					{
						iRazeValue -= std::max(2, AI_bonusVal(pLoopPlot->getBonusType(getTeam())) / 2);
					}
				}

				// More inclined to raze if we're unlikely to hold it
				if (GET_TEAM(getTeam()).getPower(false) * 10 < GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).getPower(true) * 8)
				{
					const int iTempValue = std::min(75,
						(GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).getPower(true) - GET_TEAM(getTeam()).getPower(false))
						* 20 / std::max(100, GET_TEAM(getTeam()).getPower(false))
					);
					iRazeValue += iTempValue;
				}
			}

			if (iRazeValue > 0 && GC.getGame().getSorenRandNum(100, "AI Raze City") < iRazeValue)
			{
				bRaze = true;
			}
		}
	}

	if (bRaze)
	{
		pCity->doTask(TASK_RAZE);
	}
	else
	{
		CvEventReporter::getInstance().cityAcquiredAndKept(eOldOwner, getID(), pCity, bConquest, bTrade);
	}
}


bool CvPlayerAI::AI_acceptUnit(const CvUnit* pUnit) const
{
	if (isHumanPlayer())
	{
		return true;
	}

	if (AI_isFinancialTrouble())
	{
		if (pUnit->AI_getUnitAIType() == UNITAI_WORKER)
		{
			if (AI_neededWorkers(pUnit->area()) > 0)
			{
				return true;
			}
		}

		if (pUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA)
		{
			return true;
		}

		if (pUnit->AI_getUnitAIType() == UNITAI_MISSIONARY)
		{
			return true; //XXX
		}
		return false;
	}

	return true;
}


DomainTypes CvPlayerAI::AI_unitAIDomainType(UnitAITypes eUnitAI) const
{
	switch (eUnitAI)
	{
	case UNITAI_UNKNOWN:
		return NO_DOMAIN;
		break;

	case UNITAI_ANIMAL:
	case UNITAI_SETTLE:
	case UNITAI_WORKER:
	case UNITAI_ATTACK:
	case UNITAI_ATTACK_CITY:
	case UNITAI_COLLATERAL:
	case UNITAI_PILLAGE:
	case UNITAI_RESERVE:
	case UNITAI_COUNTER:
	case UNITAI_PARADROP:
	case UNITAI_CITY_DEFENSE:
	case UNITAI_CITY_COUNTER:
	case UNITAI_CITY_SPECIAL:
	case UNITAI_EXPLORE:
	case UNITAI_MISSIONARY:
	case UNITAI_PROPHET:
	case UNITAI_ARTIST:
	case UNITAI_SCIENTIST:
	case UNITAI_GENERAL:
	case UNITAI_GREAT_HUNTER:
	case UNITAI_GREAT_ADMIRAL:
	case UNITAI_MERCHANT:
	case UNITAI_ENGINEER:
	case UNITAI_SPY:
	case UNITAI_ATTACK_CITY_LEMMING:
	case UNITAI_PILLAGE_COUNTER:
	case UNITAI_SUBDUED_ANIMAL:
	case UNITAI_HUNTER:
	case UNITAI_HUNTER_ESCORT:
	case UNITAI_HEALER:
	case UNITAI_PROPERTY_CONTROL:
	case UNITAI_BARB_CRIMINAL:
	case UNITAI_INVESTIGATOR:
	case UNITAI_INFILTRATOR:
	case UNITAI_SEE_INVISIBLE:
	case UNITAI_ESCORT:
		return DOMAIN_LAND;
		break;

	case UNITAI_ICBM:
		return DOMAIN_IMMOBILE;
		break;

	case UNITAI_WORKER_SEA:
	case UNITAI_ATTACK_SEA:
	case UNITAI_RESERVE_SEA:
	case UNITAI_ESCORT_SEA:
	case UNITAI_EXPLORE_SEA:
	case UNITAI_ASSAULT_SEA:
	case UNITAI_SETTLER_SEA:
	case UNITAI_MISSIONARY_SEA:
	case UNITAI_SPY_SEA:
	case UNITAI_CARRIER_SEA:
	case UNITAI_MISSILE_CARRIER_SEA:
	case UNITAI_PIRATE_SEA:
	case UNITAI_HEALER_SEA:
	case UNITAI_PROPERTY_CONTROL_SEA:
	case UNITAI_SEE_INVISIBLE_SEA:
		return DOMAIN_SEA;
		break;

	case UNITAI_ATTACK_AIR:
	case UNITAI_DEFENSE_AIR:
	case UNITAI_CARRIER_AIR:
	case UNITAI_MISSILE_AIR:
		return DOMAIN_AIR;
		break;

	default:
		FErrorMsg("error");
		break;
	}

	return NO_DOMAIN;
}


int CvPlayerAI::AI_yieldWeight(YieldTypes eYield) const
{
	if (eYield == YIELD_PRODUCTION)
	{
		const int iMod = 100 + 30 * std::max(0, GC.getGame().getCurrentEra() - 1) / std::max(1, GC.getNumEraInfos() - 2);
		return GC.getYieldInfo(eYield).getAIWeightPercent() * iMod / 100;
	}
	return GC.getYieldInfo(eYield).getAIWeightPercent();
}


int CvPlayerAI::AI_commerceWeight(CommerceTypes eCommerce, const CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	int iWeight = GC.getCommerceInfo(eCommerce).getAIWeightPercent();

	switch (eCommerce)
	{
	case COMMERCE_RESEARCH:
	{
		if (AI_avoidScience())
		{
			if (isNoResearchAvailable())
			{
				iWeight = 0;
			}
			else
			{
				iWeight /= 8;
			}
		}
		else if (!AI_isFinancialTrouble())
		{
			iWeight += 25;
		}
		break;
	}
	case COMMERCE_GOLD:
	{
		if (AI_isFinancialTrouble())
		{
			iWeight *= 2;
		}
		else if (getCommercePercent(COMMERCE_GOLD) < 25) // Low tax
		{
			//put more money towards other commerce types
			if (getGoldPerTurn() > getGold() / 40)
			{
				iWeight -= 50 - 2 * getCommercePercent(COMMERCE_GOLD);
			}
		}
		break;
	}
	case COMMERCE_CULTURE:
	{
		// COMMERCE_CULTURE AIWeightPercent is 25% in default xml
		// Adjustments for human player going for cultural victory (who won't have AI strategy set)
		// so that governors do smart things
		if (pCity != NULL)
		{
			if (GC.getGame().culturalVictoryValid() && pCity->getCultureTimes100(getID()) >= 100 * GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel()))
			{
				iWeight /= 50;
			}
			// Slider check works for detection of whether human player is going for cultural victory
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) || getCommercePercent(COMMERCE_CULTURE) >= 90)
			{
				int iCultureRateRank = pCity->findCommerceRateRank(COMMERCE_CULTURE);
				int iCulturalVictoryNumCultureCities = GC.getGame().culturalVictoryNumCultureCities();

				// if one of the currently best cities, then focus hard, *4 or more
				if (iCultureRateRank <= iCulturalVictoryNumCultureCities)
				{
					iWeight *= (3 + iCultureRateRank);
				}
				// if one of the 3 close to the top, then still emphasize culture some, *2
				else if (iCultureRateRank <= iCulturalVictoryNumCultureCities + 3)
				{
					iWeight *= 2;
				}
				else if (isHumanPlayer())
				{
					iWeight *= 2;
				}
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) || getCommercePercent(COMMERCE_CULTURE) >= 70)
			{
				iWeight *= 3;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1) || getCommercePercent(COMMERCE_CULTURE) >= 50)
			{
				iWeight *= 2;
			}
			iWeight += 2 * (100 - pCity->plot()->calculateCulturePercent(getID()));

			if (pCity->getCultureLevel() < (CultureLevelTypes)2)
			{
				iWeight = std::max(iWeight, 800);
			}
		}
		else // pCity == NULL
		{
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) || getCommercePercent(COMMERCE_CULTURE) >= 90)
			{
				iWeight *= 3;
				iWeight /= 4;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) || getCommercePercent(COMMERCE_CULTURE) >= 70)
			{
				iWeight *= 2;
				iWeight /= 3;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1) || getCommercePercent(COMMERCE_CULTURE) >= 50)
			{
				iWeight /= 2;
			}
			else
			{
				iWeight /= 3;
			}
		}
		break;
	}
	case COMMERCE_ESPIONAGE:
	{
		const TeamTypes eTeam = getTeam();
		const CvTeamAI& myTeam = GET_TEAM(eTeam);

		if (!myTeam.hasMetAnyCiv())
		{
			iWeight = 0;
			break;
		}
		int iNumValidTeamsMet = 0;
		int iEspBehindWeight = 0;
		for (int iI = 0; iI < MAX_PC_TEAMS; ++iI)
		{
			const TeamTypes eTeamX = static_cast<TeamTypes>(iI);
			CvTeam& teamX = GET_TEAM(eTeamX);

			if (teamX.isAlive() && eTeamX != eTeam && myTeam.isHasMet(eTeamX)
			// Don't bother with minor civs unless we are one too.
			&& (isMinorCiv() || !teamX.isMinorCiv())
			// Ignore vassals
			&& !teamX.isVassal(eTeam) && !myTeam.isVassal(eTeamX))
			{
				iNumValidTeamsMet++;
				// Behind in espionage
				if (teamX.getEspionagePointsAgainstTeam(eTeam) - myTeam.getEspionagePointsAgainstTeam(eTeamX) > 0)
				{
					iEspBehindWeight++;

					if (myTeam.AI_getAttitude(eTeamX) < ATTITUDE_CAUTIOUS)
					{
						iEspBehindWeight++;
					}
				}
			}
		}
		if (iNumValidTeamsMet == 0)
		{
			iWeight = 0;
			break;
		}
		iWeight *= AI_getEspionageWeight() * (3 * iEspBehindWeight + iNumValidTeamsMet / 2 + 1);
		iWeight /= 200 * iNumValidTeamsMet;

		// K-Mod
		if (AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
		{
			iWeight *= 2;
		}

		if (getCommercePercent(COMMERCE_ESPIONAGE) == 0)
		{
			iWeight *= 2;
			iWeight /= 3;
		}
		else if (isHumanPlayer())
		{
			// UNOFFICIAL_PATCH todo:  should this tweak come over in some form?
			// There's still an issue with upping espionage slider for human player.
			if (getCommercePercent(COMMERCE_ESPIONAGE) > 50)
			{
				iWeight *= getCommercePercent(COMMERCE_ESPIONAGE);
				iWeight /= 50;
			}
		}
		// AI Espionage slider use maxed out at 20 percent
		else if (getCommercePercent(COMMERCE_ESPIONAGE) >= 20)
		{
			iWeight *= 3;
			iWeight /= 2;
		}
		break;
	}
	default: break;
	}
	return iWeight;
}

// Improved as per Blake - thanks!
int CvPlayerAI::AI_foundValue(int iX, int iY, int iMinRivalRange, bool bStartingLoc) const
{
	PROFILE_EXTRA_FUNC();
	if (!canFound(iX, iY))
	{
		return 0;
	}
	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	bool bAdvancedStart = getAdvancedStartPoints() >= 0;
	const bool bIsCoastal = pPlot->isCoastalLand(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize());

	CvArea* pArea = pPlot->area();
	const int iNumAreaCities = pArea->getCitiesPerPlayer(getID());

	if (!bStartingLoc && !bAdvancedStart && !bIsCoastal && iNumAreaCities == 0)
	{
		return 0;
	}

	if (bAdvancedStart)
	{
		FAssert(GC.getGame().isOption(GAMEOPTION_CORE_CUSTOM_START));
		if (bStartingLoc)
		{
			bAdvancedStart = false;
		}
	}

	/* Explanation of city site adjustment:
		Any plot which is otherwise within the radius of a city site
		is basically treated as if it's within an existing city radius
	*/
	std::vector<bool> abCitySiteRadius(NUM_CITY_PLOTS, false);

	if (!bStartingLoc && !m_bCitySitesNotCalculated && !AI_isPlotCitySite(pPlot))
	{
		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);
			if (pLoopPlot == NULL)
			{
				continue;
			}
			for (int iJ = 0; iJ < AI_getNumCitySites(); iJ++)
			{
				const CvPlot* pCitySitePlot = AI_getCitySite(iJ);
				if (pCitySitePlot != pPlot && plotDistance(pLoopPlot->getX(), pLoopPlot->getY(), pCitySitePlot->getX(), pCitySitePlot->getY()) <= CITY_PLOTS_RADIUS)
				{ // Plot is inside the radius of a city site
					abCitySiteRadius[iI] = true;
					break;
				}
			}
		}
	}

	std::vector<int> paiBonusCount(GC.getNumBonusInfos(), 0);

	if (iMinRivalRange != -1)
	{
		foreach_(const CvPlot * pLoopPlot, pPlot->rect(iMinRivalRange, iMinRivalRange))
		{
			if (pLoopPlot->plotCheck(PUF_isOtherTeam, getID()) != NULL)
			{
				return 0;
			}
		}
	}

	if (bStartingLoc)
	{
		if (pPlot->isGoody())
		{
			return 0;
		}
		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			if (plotCity(iX, iY, iI) == NULL)
			{
				return 0;
			}
		}
	}
	else
	{
		int iOwnedTiles = 0;
		for (int iI = 0; iI < NUM_CITY_PLOTS_2; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot == NULL || pLoopPlot->isOwned() && pLoopPlot->getTeam() != getTeam())
			{
				iOwnedTiles++;
			}
		}
		if (iOwnedTiles > 10)
		{
			return 0;
		}
	}

	int iBadTile = 0;

	for (int iI = SKIP_CITY_HOME_PLOT; iI < NUM_CITY_PLOTS_2; iI++)
	{
		CvPlot* pLoopPlot = plotCity(iX, iY, iI);

		if (pLoopPlot == NULL)
		{
			iBadTile += 4;
		}
		else if (pLoopPlot->isImpassable(getTeam()))
		{
			iBadTile += 3;
		}
		else if (!pLoopPlot->isFreshWater() && !pLoopPlot->isHills())
		{
			if (pLoopPlot->isWater() && (!bIsCoastal || pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) < 2))
			{
				iBadTile++;
			}
			else if (pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) < 2 || pLoopPlot->calculateTotalBestNatureYield(getTeam()) < 3)
			{
				iBadTile += 2;
			}
		}
		else if (pLoopPlot->isOwned() && pLoopPlot->getTeam() == getTeam() && (pLoopPlot->isCityRadius() || abCitySiteRadius[iI]))
		{
			iBadTile += bAdvancedStart ? 2 : 1;
		}
	}
	iBadTile /= 2;

	if (!bStartingLoc && (iBadTile > 15 || pArea->getNumTiles() <= 2))
	{
		bool bHasGoodBonus = false;

		for (int iI = 0; iI < NUM_CITY_PLOTS_2; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot != NULL && !pLoopPlot->isOwned()
			&& (pLoopPlot->isWater() || (pLoopPlot->area() == pArea) || (pLoopPlot->area()->getCitiesPerPlayer(getID()) > 0)))
			{
				BonusTypes eBonus = pLoopPlot->getBonusType(getTeam());

				if (eBonus != NO_BONUS
				&& (getNumTradeableBonuses(eBonus) == 0 || AI_bonusVal(eBonus) > 10 || GC.getBonusInfo(eBonus).getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) > 0))
				{
					bHasGoodBonus = true;
					break;
				}
			}
		}
		if (!bHasGoodBonus)
		{
			return 0;
		}
	}

	// K-Mod - EasyCulture means that it will be easy for us to pop the culture to the 2nd border
	bool bEasyCulture = false;
	int iGreed = 100;

	if (bAdvancedStart)
	{
		iGreed = 150;
	}
	else if (!bStartingLoc)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (hasTrait((TraitTypes)iI))
			{
				//Greedy founding means getting the best possible sites - fitting maximum
				//resources into the fat cross.
				iGreed += (GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier(UPKEEP_CIVIC, CASC_SCOPE_EMPIRE) / 2);
				iGreed += 20 * (GC.getTraitInfo((TraitTypes)iI).getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_EMPIRE) / 100);
				// K-Mod note: I don't think this is the right way to calculate greed.
				// For example, if greed is high, the civ will end up having fewer, more spread out cities.
				// That's the opposite of what makes an upkeep reduction most useful.
				if (GC.getTraitInfo((TraitTypes)iI).getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_EMPIRE) > 0)
				{
					bEasyCulture = true;
				}
			}
		}
		//Fuyu: be greedier
		if (!isFoundedFirstCity())
		{
			//here I'm assuming that the capital gets a palace that has significant culture output, thus popping the final level 2 really quickly
			iGreed = std::max(iGreed, 145);
		}
		else
		{
			// The maintained PROCESS frontier is already gate-passed, so the availability test disappears with
			// the scan ([enabler.md] §6: the AI's decisions iterate ONLY the frontier).
			std::vector<int> availableProcesses;
			getAvailableProcesses(availableProcesses);

			for (std::vector<int>::const_iterator itProcess = availableProcesses.begin();
				iGreed < 140 && itProcess != availableProcesses.end(); ++itProcess)
			{
				const CvProcessInfo& kProcess = GC.getProcessInfo((ProcessTypes)*itProcess);
				iGreed = std::max(iGreed, 100 + 20 * std::min(2, (kProcess.getProductionToCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) / 100)));
			}

			for (int iI = 0; (iGreed < 140 && iI < GC.getNumSpecialistInfos()); iI++)
			{
				if (isSpecialistValid((SpecialistTypes)iI))
				{
					iGreed = std::max(iGreed, 100 + 10 * std::min(4, GC.getSpecialistInfo((SpecialistTypes)iI).getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) / 100));
				}
			}

			if (iGreed < 140 && AI_isAreaAlone(pArea))
			{
				//assuming that the player actually has access to something that can make a city produce culture
				iGreed = std::max(iGreed, 125 + (5 * std::min(3, pArea->getCitiesPerPlayer(getID()))));
			}
		}
		iGreed = range(iGreed, 100, 150);
	}

	// K-Mod, easy culture
	// culture building process
	if (!bEasyCulture)
	{
		// The hand-rederived tech gate goes with the scan: a LISTED process is one the enabler has already
		// gated, so the only question left is what the process converts into ([enabler.md] §6).
		std::vector<int> availableProcesses;
		getAvailableProcesses(availableProcesses);

		for (std::vector<int>::const_iterator itProcess = availableProcesses.begin();
			itProcess != availableProcesses.end(); ++itProcess)
		{
			const CvProcessInfo& kProcess = GC.getProcessInfo((ProcessTypes)*itProcess);

			if (kProcess.getProductionToCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) > 0)
			{
				bEasyCulture = true;
				break;
			}
		}
	}
	// free culture building -- a building the player HOLDS that GRANTS a culture one. The standing
	// "this building is free for me" flag went with its accumulator: a granted building is simply one the city
	// HAS, so the question is asked of the GRANTOR's own compiled payload (grants.buildings at empire scope,
	// json §5) rather than a player-side mirror. Own-data, never a scan of every building asking about this one.
	if (!bEasyCulture)
	{
		const int iBuildingsBucket = CvGrants::key("buildings");
		const int iEmpireScope = CvGrants::key("empire");
		for (int iJ = 0; iJ < GC.getNumBuildingInfos() && !bEasyCulture; iJ++)
		{
			if (getBuildingCount((BuildingTypes)iJ) <= 0)
			{
				continue;
			}
			const CvBuildingInfo& kSource = GC.getBuildingInfo((BuildingTypes)iJ);
			const CvGrants* pGrants = kSource.getTriggers() ? kSource.getTriggers()->consideredGrant() : NULL;
			const std::vector<int>* pGranted = (pGrants != NULL) ? pGrants->list(iBuildingsBucket) : NULL;
			if (pGranted == NULL)
			{
				continue;
			}
			for (size_t iEntry = 0; iEntry < pGranted->size(); ++iEntry)
			{
				if (pGrants->listScope(iBuildingsBucket, iEntry) != iEmpireScope)
				{
					continue;
				}
				if (GC.getBuildingInfo((BuildingTypes)(*pGranted)[iEntry])
						.getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) > 0)
				{
					bEasyCulture = true;
					break;
				}
			}
		}
	}
	// easy artists
	if (!bEasyCulture)
	{
		for (int iJ = 0; iJ < GC.getNumSpecialistInfos(); iJ++)
		{
			if (isSpecialistValid((SpecialistTypes)iJ) && specialistCommerce((SpecialistTypes)iJ, COMMERCE_CULTURE) > 0)
			{
				bEasyCulture = true;
				break;
			}
		}
	}
	// ! K-Mod

	//iClaimThreshold is the culture required to pop the 2nd borders.
	const int iClaimThreshold =
	(
		std::max(1, GC.getGame().getCultureThreshold((CultureLevelTypes)std::min(2, GC.getNumCultureLevelInfos() - 1)))
		* (bEasyCulture ? 140 : 100)
	);

	int iTakenTiles = 0;
	int iTeammateTakenTiles = 0;
	int iHealth = 0;
	int iValue = 1000;

	int iResourceValue = 0;
	int iSpecialFoodPlus = 0;
	int iSpecialFoodMinus = 0;

	bool bNeutralTerritory = true;

	for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		CvPlot* pLoopPlot = plotCity(iX, iY, iI);

		if (pLoopPlot == NULL)
		{
			iTakenTiles++;
		}
		else if (pLoopPlot->isPlayerCityRadius(getID()) || abCitySiteRadius[iI])
		{
			iTakenTiles++;

			if (abCitySiteRadius[iI])
			{
				iTeammateTakenTiles++;
			}
		}
		else
		{
			int iTempValue = 0;

			int iCultureMultiplier;
			if (!pLoopPlot->isOwned() || (pLoopPlot->getOwner() == getID()))
			{
				iCultureMultiplier = 100;
			}
			else
			{
				bNeutralTerritory = false;
				const int64_t iOtherCulture = std::max<int64_t>(1, pLoopPlot->getCulture(pLoopPlot->getOwner()));

				iCultureMultiplier = 100 * pLoopPlot->getCulture(getID()) + iClaimThreshold;
				iCultureMultiplier /= (100 * iOtherCulture + iClaimThreshold) / 100;

				iCultureMultiplier = std::min(100, iCultureMultiplier);
				//The multiplier is basically normalized...
				//100% means we own (or rightfully own) the tile.
				//50% means the hostile culture is fairly firmly entrenched.
			}

			if (iCultureMultiplier < 50 - 25 * (iNumAreaCities > 0))
			{
				//discourage hopeless cases, especially on other continents.
				iTakenTiles += 2 - (iNumAreaCities > 0);
			}

			const BonusTypes eBonus = pLoopPlot->getBonusType((bStartingLoc) ? NO_TEAM : getTeam());

			const FeatureTypes eFeature = pLoopPlot->getFeatureType();
			int aiYield[NUM_YIELD_TYPES];

			int aiPlotYields100[NUM_YIELD_TYPES];
			pLoopPlot->getYields(aiPlotYields100);   // ×100 group read (getYield is the EXE edge)
			for (int iYieldType = 0; iYieldType < NUM_YIELD_TYPES; ++iYieldType)
			{
				const YieldTypes eYield = (YieldTypes)iYieldType;
				aiYield[eYield] = aiPlotYields100[eYield];

				if (iI == CITY_HOME_PLOT)
				{
					int iBasePlotYield = aiYield[eYield];
					aiYield[eYield] += GC.getYieldInfo(eYield).getCityChange() * 100;  // CvYieldInfo is legacy XML => human; LIFT it

					if (eFeature != NO_FEATURE)
					{
						aiYield[eYield] -= GC.getFeatureInfo(eFeature).getFlatYield(eYield, CASC_SCOPE_PLOT);
						iBasePlotYield = std::max(iBasePlotYield, aiYield[eYield]);
					}

					if (eBonus != NO_BONUS)
					{
						const int iBonusYieldChange = GC.getBonusInfo(eBonus).getFlatYield(eYield, CASC_SCOPE_PLOT);
						aiYield[eYield] += iBonusYieldChange;
						iBasePlotYield += iBonusYieldChange;
					}
					aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity() * 100);  // XML => human; LIFT it
				}
			}

			if (iI == CITY_HOME_PLOT)
			{
				iTempValue += aiYield[YIELD_FOOD] * (120 + 30 * bStartingLoc);
				iTempValue += aiYield[YIELD_PRODUCTION] * 100;
				iTempValue += aiYield[YIELD_COMMERCE] * 80;
			}
			else
			{
				iTempValue += aiYield[YIELD_FOOD] * (60 + 15 * bStartingLoc);
				iTempValue += aiYield[YIELD_PRODUCTION] * 50;
				iTempValue += aiYield[YIELD_COMMERCE] * 40;
			}
			if (bStartingLoc) // Yield holds much value for new game starting positions.
			{
				iTempValue *= 2;
			}

			if (pLoopPlot->isWater())
			{
				if (aiYield[YIELD_COMMERCE] > 1)
				{
					// Upside is much higher based on multipliers above, with lighthouse a standard coast
					// plot moves up into the higher multiplier category.
					iTempValue += bIsCoastal ? 40 + 10 * aiYield[YIELD_COMMERCE] : -10 * aiYield[YIELD_COMMERCE];

					if (bIsCoastal && aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION())
					{
						iSpecialFoodPlus += 1;
					}
				}
				if (!bIsCoastal)
				{
					iTempValue -= 400;
				}
			}

			// Favor extra river tiles for eventual building yields, if we are on a river
			if (pLoopPlot->isRiver() && pPlot->isRiver())
			{
				iTempValue += 25 + 10 * GC.getGame().getRiverBuildings();
			}
			if (pLoopPlot->isAsPeak())
			{// Defense bonus...
				iTempValue += 25;
			}

			if (bEasyCulture)
			{
				// 5/4 * 21 ~= 9 * 1.5 + 12 * 1;
				iTempValue *= 5;
				iTempValue /= 4;
			}
			else if (pLoopPlot->getOwner() == getID() || stepDistance(iX, iY, pLoopPlot->getX(), pLoopPlot->getY()) <= 1)
			{
				iTempValue *= 3;
				iTempValue /= 2;
			}
			iTempValue *= iGreed; // (note: see comments about iGreed higher in the code)
			iTempValue /= 100;

			iTempValue *= iCultureMultiplier;
			iTempValue /= 100;

			iValue += iTempValue;

			if (iCultureMultiplier > 33) //ignore hopelessly entrenched tiles.
			{
				if (eFeature != NO_FEATURE && iI != CITY_HOME_PLOT)
				{
					iHealth += GC.getFeatureInfo(eFeature).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);

					iSpecialFoodPlus += std::max(0, aiYield[YIELD_FOOD] - GC.getFOOD_CONSUMPTION_PER_POPULATION());
				}

				if (eBonus != NO_BONUS
				&& (pLoopPlot->area() == pPlot->area() || pLoopPlot->area()->getCitiesPerPlayer(getID()) > 0 || pLoopPlot->isWater()))
				{
					paiBonusCount[eBonus]++;
					FAssert(paiBonusCount[eBonus] > 0);

					iTempValue = AI_bonusVal(eBonus) * (!bStartingLoc && getNumTradeableBonuses(eBonus) == 0 && paiBonusCount[eBonus] == 1 ? 80 : 20);
					iTempValue *= (bStartingLoc ? 100 : iGreed);
					iTempValue /= 100;

					if (iI != CITY_HOME_PLOT && !bStartingLoc
					&& pLoopPlot->getOwner() != getID() && stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) > 1)
					{
						iTempValue *= 2;
						iTempValue /= 3;

						iTempValue *= std::min(150, iGreed);
						iTempValue /= 100;
					}

					if (pLoopPlot->isWater() && !pLoopPlot->isAdjacentToLand())
					{
						iTempValue /= 2;
					}

					iValue += (iTempValue + 10);

					if (iI != CITY_HOME_PLOT && eFeature != NO_FEATURE && GC.getFeatureInfo(eFeature).getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) < 0)
					{
						iResourceValue -= 30;
					}
				}
			}
		}
	}

	if (bStartingLoc)
	{
		iResourceValue /= 2;
	}
	iValue += std::max(0, iResourceValue);

	if (iResourceValue < 250 && iTakenTiles > 12)
	{
		return 0;
	}

	if (iTeammateTakenTiles > 1)
	{
		return 0;
	}

	iValue += iHealth / 5;

	if (bStartingLoc && pArea->getNumStartingPlots() == 0)
	{
		iValue += 1000;
	}
	if (bIsCoastal)
	{
		if (bStartingLoc)
		{
			//let other penalties bring this down.
			iValue += 1000;
		}
		else if (pArea->getCitiesPerPlayer(getID()) != 0)
		{
			iValue += 200;

			// Push players to get more coastal cities so they can build navies
			CvArea* pWaterArea = pPlot->waterArea(true);
			if (pWaterArea != NULL)
			{
				iValue += 200;

				if (GET_TEAM(getTeam()).AI_isWaterAreaRelevant(pWaterArea))
				{
					iValue += 200;

					if (countNumCoastalCities() < getNumCities() / 2 || countNumCoastalCitiesByArea(pPlot->area()) == 0)
					{
						iValue *= 250 + 100 * (getNumCities() - countNumCoastalCities()) / getNumCities();
						iValue /= 200;
					}
					// If this location bridges two water areas that's worth a boost
					CvArea* pSecondWaterArea = pPlot->secondWaterArea();
					if (pSecondWaterArea != NULL)
					{
						if (GET_TEAM(getTeam()).AI_isWaterAreaRelevant(pSecondWaterArea))
						{
							iValue *= 4;
							iValue /= 3;
						}
						else // Just bridges a lake or something
						{
							iValue += 50;
						}
					}
					// If this will be our first city on this water area give another boost
					if (countNumCoastalCitiesByArea(pWaterArea) == 0)
					{
						iValue *= 5;
						iValue /= 4;
					}
				}
			}
		}
		else if (bNeutralTerritory)
		{
			iValue += (iResourceValue > 0 ? 800 : 100);
		}
	}

	if (pPlot->isHills())
	{
		iValue += 200;
	}

	if (pPlot->isRiver())
	{
		iValue += 400;
	}

	if (bIsCoastal)
	{
		iValue += 50 * GC.getGame().getCoastalBuildings();
	}

	if (pPlot->isFreshWater())
	{
		iValue += 200;
	}

	if (bStartingLoc)
	{
		const int iRange = GREATER_FOUND_RANGE;
		int iGreaterBadTile = 0;

		foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
		{
			if ((pLoopPlot->isWater() || pLoopPlot->area() == pArea)
			&& plotDistance(iX, iY, pLoopPlot->getX(), pLoopPlot->getY()) <= iRange)
			{
				int aiRangeYields100[NUM_YIELD_TYPES];
				pLoopPlot->getYields(aiRangeYields100);   // ×100 group read (getYield is the EXE edge)
				const int iTempValue =
					(
						13 * aiRangeYields100[YIELD_FOOD] +
						11 * aiRangeYields100[YIELD_PRODUCTION] +
						 7 * aiRangeYields100[YIELD_COMMERCE]
					);
				if (iTempValue < 28)
				{
					iGreaterBadTile += 2;
					if (pLoopPlot->getFeatureType() != NO_FEATURE
					&& pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) > 1)
					{
						iGreaterBadTile--;
					}
				}
				iValue += iTempValue;
			}
		}

		iGreaterBadTile /= 2;
		if (iGreaterBadTile > 12)
		{
			iValue *= 11;
			iValue /= iGreaterBadTile;
		}
		int iWaterCount = 0;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot != NULL && pLoopPlot->isWater())
			{
				iWaterCount++;
				int aiWaterYields100[NUM_YIELD_TYPES];
				pLoopPlot->getYields(aiWaterYields100);   // ×100 group read (getYield is the EXE edge)
				if (aiWaterYields100[YIELD_FOOD] <= 100)
				{
					iWaterCount++;
				}
			}
		}
		iWaterCount /= 2;

		int iLandCount = NUM_CITY_PLOTS - iWaterCount;

		if (iLandCount < NUM_CITY_PLOTS / 2)
		{
			//discourage very water-heavy starts.
			iValue *= 1 + iLandCount;
			iValue /= 1 + NUM_CITY_PLOTS / 2;
		}
	}

	if (bStartingLoc)
	{
		if (pPlot->getMinOriginalStartDist() == -1)
		{
			iValue += GC.getMap().maxStepDistance() * 100;
		}
		else
		{
			iValue *= 1 + 4 * pPlot->getMinOriginalStartDist();
			iValue /= 1 + 2 * GC.getMap().maxStepDistance();
		}

		// Nice hacky way to avoid this messing with normalizer, use elsewhere?
		int iMinDistanceFactor = MAX_INT;
		int iMinRange = startingPlotRange();

		iValue *= 100;
		for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
		{
			if (GET_PLAYER((PlayerTypes)iJ).isAlive() && iJ != getID())
			{
				int iClosenessFactor = GET_PLAYER((PlayerTypes)iJ).startingPlotDistanceFactor(pPlot, getID(), iMinRange);
				iMinDistanceFactor = std::min(iClosenessFactor, iMinDistanceFactor);

				if (iClosenessFactor < 1000)
				{
					iValue *= 5000 + iClosenessFactor;
					iValue /= 6000;
				}
			}
		}

		if (iMinDistanceFactor > 1000)
		{
			//give a maximum boost of 20% for somewhat distant locations, don't go overboard.
			iMinDistanceFactor = std::min(1500, iMinDistanceFactor);
			iValue *= 1500 + iMinDistanceFactor;
			iValue /= 2500;
		}
		else if (iMinDistanceFactor < 1000)
		{
			//this is too close so penalize again.
			iValue *= 1000 + iMinDistanceFactor;
			iValue /= 2000;
		}
		iValue /= 100;
	}

	if (bAdvancedStart && pPlot->getBonusType() != NO_BONUS)
	{
		iValue *= 70;
		iValue /= 100;
	}

	CvCity* pNearestCity = GC.getMap().findCity(iX, iY, isNPC() ? NO_PLAYER : getID());

	if (pNearestCity == NULL)
	{
		pNearestCity = GC.getMap().findCity(iX, iY, ((isNPC()) ? NO_PLAYER : getID()), isNPC() ? NO_TEAM : getTeam(), false);
		if (pNearestCity != NULL)
		{
			int iDistance = plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
			iValue -= std::min(500 * iDistance, (8000 * iDistance) / GC.getMap().maxPlotDistance());
		}
	}
	else if (isNPC())
	{
		iValue -= (std::max(0, (8 - plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY()))) * 200);
	}
	else
	{
		int iDistance = plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
		int iNumCities = getNumCities();
		if (iDistance > 5)
		{
			iValue -= (iDistance - 5) * 500;
		}
		else if (iDistance < 4)
		{
			iValue -= (4 - iDistance) * 2000;
		}
		iValue *= (8 + iNumCities * 4);
		iValue /= (2 + (iNumCities * 4) + iDistance);
		if (pNearestCity->isCapital())
		{
			iValue *= 150;
			iValue /= 100;
		}
		else if (getCapitalCity() != NULL)
		{
			//Provide up to a 50% boost to value (80% for adv.start)
			//for city sites which are relatively close to the core
			//compared with the most distance city from the core
			//(having a boost rather than distance penalty avoids some distortion)

			//This is not primarly about maitenance but more about empire
			//shape as such forbidden palace/state property are not big deal.
			int iMaxDistanceFromCapital = 0;

			int iCapitalX = getCapitalCity()->getX();
			int iCapitalY = getCapitalCity()->getY();

			foreach_(const CvCity * pLoopCity, cities())
			{
				iMaxDistanceFromCapital = std::max(iMaxDistanceFromCapital, plotDistance(iCapitalX, iCapitalY, pLoopCity->getX(), pLoopCity->getY()));
			}

			FAssert(iMaxDistanceFromCapital > 0);
			iValue *= 100 + (((bAdvancedStart ? 80 : 50) * std::max(0, (iMaxDistanceFromCapital - iDistance))) / iMaxDistanceFromCapital);
			iValue /= 100;
		}
	}

	if (iValue <= 0)
	{
		return 1;
	}

	if (pArea->getNumCities() != 0)
	{
		const int iTeamAreaCities = GET_TEAM(getTeam()).countNumCitiesByArea(pArea);

		if (pArea->getNumCities() == iTeamAreaCities)
		{
			iValue *= 3;
			iValue /= 2;
		}
		else if (pArea->getNumCities() == iTeamAreaCities
			+ GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(pArea)
			+ GET_TEAM(NEANDERTHAL_TEAM).countNumCitiesByArea(pArea))
		{
			iValue *= 4;
			iValue /= 3;
		}
		else if (iTeamAreaCities > 0)
		{
			iValue *= 5;
			iValue /= 4;
		}
	}
	else iValue *= 2;

	if (!bStartingLoc)
	{
		int iFoodSurplus = std::max(0, iSpecialFoodPlus - iSpecialFoodMinus);
		int iFoodDeficit = std::max(0, iSpecialFoodMinus - iSpecialFoodPlus);

		iValue *= 100 + 20 * std::max(0, std::min(iFoodSurplus, 2 * GC.getFOOD_CONSUMPTION_PER_POPULATION()));
		iValue /= 100 + 20 * std::max(0, iFoodDeficit);

		if (getNumCities() > 0)
		{
			int iBonusCount = 0;
			int iUniqueBonusCount = 0;
			for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
			{
				iBonusCount += paiBonusCount[iI];
				iUniqueBonusCount += (paiBonusCount[iI] > 0) ? 1 : 0;
			}
			if (iBonusCount > 4)
			{
				iValue *= 5;
				iValue /= (1 + iBonusCount);
			}
			else if (iUniqueBonusCount > 2)
			{
				iValue *= 5;
				iValue /= (3 + iUniqueBonusCount);
			}
		}
		const int iDeadLockCount = AI_countDeadlockedBonuses(pPlot);
		if (iDeadLockCount > 0)
		{
			if (bAdvancedStart)
				 iValue /= 3 + iDeadLockCount;
			else iValue /= 1 + iDeadLockCount;
		}
	}
	iValue /= 3 + std::max(0, iBadTile - NUM_CITY_PLOTS / 4);

	if (bStartingLoc)
	{
		int iDifferentAreaTile = 0;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot == NULL || !pLoopPlot->isWater() && pLoopPlot->area() != pArea)
			{
				iDifferentAreaTile++;
			}
		}
		iValue /= 2 + std::max(0, iDifferentAreaTile - NUM_CITY_PLOTS * 2 / 3);
	}
	return std::max(1, iValue);
}


bool CvPlayerAI::AI_isAreaAlone(const CvArea* pArea) const
{
	return (GET_TEAM(getTeam()).countNumCitiesByArea(pArea) == pArea->getNumCities()
		- GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(pArea)
		- GET_TEAM(NEANDERTHAL_TEAM).countNumCitiesByArea(pArea));
}


bool CvPlayerAI::AI_isCapitalAreaAlone() const
{
	const CvCity* pCapitalCity = getCapitalCity();
	return pCapitalCity ? AI_isAreaAlone(pCapitalCity->area()) : false;
}


bool CvPlayerAI::AI_isPrimaryArea(const CvArea* pArea) const
{
	if (pArea->isWater())
	{
		return false;
	}

	// Toffer
	const int iNumAreaCities = pArea->getCitiesPerPlayer(getID());
	if (iNumAreaCities < 1)
	{
		return false;
	}
	if (iNumAreaCities > 1 + GC.getMap().getWorldSize())
	{
		return true;
	}
	const int iNumCities = getNumCities();

	// Even 1 of 6 should be enough for it to be primary.
	if (iNumCities < 7)
	{
		return true;
	}
	// If 17% or more of my cities are on the landmass, then the landmass is a primary area.
	if (16 < 100 * iNumAreaCities / iNumCities)
	{
		return true;
	}
	// ! Toffer

	const CvCity* pCapitalCity = getCapitalCity();
	return pCapitalCity ? pCapitalCity->area() == pArea : false;
}


int CvPlayerAI::AI_militaryWeight(const CvArea* pArea) const
{
	return 1 + pArea->getPopulationPerPlayer(getID()) + 3 * pArea->getCitiesPerPlayer(getID());
}


int CvPlayerAI::AI_targetCityValue(const CvCity* pCity, bool bRandomize, bool bIgnoreAttackers) const
{
	PROFILE_FUNC();

	CvCity* pNearestCity;
	CvPlot* pLoopPlot;
	int iI;

	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	int iValue = 1;

	int iPopulation = ((pCity->getPopulation() * 5 * (50 + pCity->calculateCulturePercent(getID()))) / 100);

	iValue += iPopulation; 

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  06/30/10					 Mongoose & jdog5000	  */
	/*																							  */
	/* War strategy AI																			  */
	/************************************************************************************************/
		// Prefer lower defense
	const int iDefenseMod = std::max(0, (500 - pCity->getDefenseModifier(false)) / 2);
	int iDefenseDmg = 0;
	if (pCity->getDefenseDamage() > 0)  //Defense Damage is not Positif, it hurts
	{
		iDefenseDmg = ((pCity->getDefenseDamage() / 10) + 1);
	}
	iValue += iDefenseMod - iDefenseDmg;
	// Significant amounting of borrowing/adapting from Mongoose AITargetCityValueFix
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
	{
		iValue += 5;
	}

	int iWonderPts = 4 * pCity->getNumActiveWorldWonders();

	int iReligionPts = 0;
	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHolyCity((ReligionTypes)iI))
		{
			iReligionPts += std::max(2, ((GC.getGame().calculateReligionPercent((ReligionTypes)iI)) / 5));
			iReligionPts += ((AI_getFlavorValue(/* AI_FLAVOR_RELIGION */ (FlavorTypes)1) + 2) / 6);

			if (GET_PLAYER(pCity->getOwner()).getStateReligion() == iI)
			{
				iReligionPts += 2;
			}
			if (getStateReligion() == iI)
			{
				iReligionPts += 8;
			}
			if (GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion() == iI)
			{
				iReligionPts += 1;
			}
		}
	}
	iValue += iWonderPts + iReligionPts;

	if (pCity->isEverOwned(getID()))
	{
		iValue += 3;

		if (pCity->getOriginalOwner() == getID())
		{
			iValue += 3;
		}
	}
	if (!bIgnoreAttackers)
	{
		iValue += std::min(8, (AI_adjacentPotentialAttackers(pCity->plot()) + 2) / 3);
	}

	int iPlots = 0;
	for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);

		if (pLoopPlot != NULL)
		{
			if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
			{
				iPlots += std::min(8, std::max(1, AI_bonusVal(pLoopPlot->getBonusType(getTeam())) / 10));
			}

			if (pLoopPlot->getOwner() == getID())
			{
				iPlots++;
			}

			if (pLoopPlot->isAdjacentPlayer(getID(), true))
			{
				iPlots++;
			}
		}
	}

	int iSpecial = 0;
	if (GC.getGame().culturalVictoryValid()
	&& GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3)
	&& pCity->getCultureLevel() >= GC.getGame().culturalVictoryCultureLevel() - 1)
	{
		iSpecial += 15;

		if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		{
			iSpecial += 25;

			if (pCity->getCultureLevel() >= (GC.getGame().culturalVictoryCultureLevel()))
			{
				iSpecial += 10;
			}
		}
	}

	if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
	{
		if (pCity->isCapital())
		{
			iSpecial += 10;

			if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_SPACE4))
			{
				iSpecial += 20;

				if (GET_TEAM(pCity->getTeam()).getVictoryCountdown(GC.getGame().getSpaceVictory()) >= 0)
				{
					iSpecial += 30;
				}
			}
		}
	}

	pNearestCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), getID());

	if (pNearestCity != NULL)
	{
		// Now scales sensibly with map size, on large maps this term was incredibly dominant in magnitude
		int iTempValue = 30;
		iTempValue *= std::max(1, ((GC.getMap().maxStepDistance() * 2) - GC.getMap().calculatePathDistance(pNearestCity->plot(), pCity->plot())));
		iTempValue /= std::max(1, (GC.getMap().maxStepDistance() * 2));

		iSpecial += iTempValue;
	}

	iValue += iPlots + iSpecial;

	int iRandom = 0;
	if (bRandomize)
	{
		iRandom = GC.getGame().getSorenRandNum(((pCity->getPopulation() * 2) + 1), "AI Target City Value");
	}
	iValue += iRandom;
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/

		//	Prefer less defended
	int iDefense = 5 + (static_cast<const CvCityAI*>(pCity))->getGarrisonStrength();
	iDefense = std::max(1, iDefense);
	iValue = ((iValue * 100) / intSqrt(iDefense));

	return iValue;
}


CvCity* CvPlayerAI::AI_findTargetCity(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = 0;
	CvCity* pBestCity = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (isPotentialEnemy(getTeam(), GET_PLAYER((PlayerTypes)iI).getTeam()))
			{
				foreach_(CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
				{
					if (pLoopCity->area() == pArea)
					{
						const int iValue = AI_targetCityValue(pLoopCity, true);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestCity = pLoopCity;
						}
					}
				}
			}
		}
	}

	return pBestCity;
}


bool CvPlayerAI::AI_isCommercePlot(const CvPlot* pPlot) const
{
	int aiYields100[NUM_YIELD_TYPES];
	pPlot->getYields(aiYields100);   // ×100 group read (getYield is the EXE edge)
	return aiYields100[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION() * 100;
}

bool CvPlayerAI::AI_getVisiblePlotDanger(const CvPlot* pPlot, int iRange, bool bAnimalOnly, CvSelectionGroup* group, int acceptableOdds) const
{
	PROFILE_EXTRA_FUNC();
	const CvArea* pPlotArea = pPlot->area();

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange) | filtered(CvPlot::fn::area() == pPlotArea))
	{
		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			// No need to loop over tiles full of our own units
			if (pLoopUnit->getTeam() == getTeam())
			{
				if (!(pLoopUnit->alwaysInvisible()) && (pLoopUnit->getInvisibleType() == NO_INVISIBLE))
				{
					break;
				}
			}

			if (pLoopUnit->isEnemy(getTeam())
			&&
			(
				!bAnimalOnly
				|| pLoopUnit->isAnimal()
				&& pLoopUnit->canAnimalIgnoresBorders()
				&& !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT)
				)
			&& pLoopUnit->canAttack()
			&& !pLoopUnit->isInvisible(getTeam(), false)
			&& pLoopUnit->canEnterOrAttackPlot(pPlot))
			{
				if (group == NULL)
				{
					return true;
				}
				// #319: gate danger on the enemy's lead-attacker binomial win% (the engine
				// number), not the stack "goodness" loss-ratio. goodness compresses an
				// overwhelming attacker vs a weak target to a tiny value, so it under-reported
				// the danger from strong enemies; the win% captures "this enemy will likely beat us".
				int iEnemyWinOdds = 0;
				pLoopUnit->getGroup()->AI_attackOdds(pPlot, true, true, NULL, -1, &iEnemyWinOdds);
				if (iEnemyWinOdds > 100 - acceptableOdds)
				{
					return true;
				}
			}
		}
	}

	return false;
}


// Plot danger cache

// The vast majority of checks for plot danger are boolean checks during path planning for non-combat
// units like workers, settlers, and GP.  Since these are simple checks for any danger they can be
// cutoff early if danger is found.  To this end, the two caches tracked are for whether a given plot
// is either known to be safe for the player who is currently moving, or for whether the plot is
// known to be a plot bordering an enemy of this team and therefore unsafe.
//
// The safe plot cache is only for the active moving player and is only set if this is not a
// multiplayer game with simultaneous turns.  The safety cache for all plots is reset when the active
// player changes or a new game is loaded.
//
// The border cache is done by team and works for all game types.  The border cache is reset for all plots
// when war or peace are declared, and reset over a limited range whenever a ownership over a plot changes.

int CvPlayerAI::AI_plotDangerUnitCheck(
	const CvPlot* plot0, const CvPlot* plotX,
	const CvUnit* unitX, const TeamTypes eTeam,
	const int iDistance, const bool bTestMoves) const
{
	if (plotX == plot0)
	{
		if (unitX->getTeam() == eTeam)
		{
			if (unitX->canDefend()
			&& !unitX->isBarbCoExist()
			&& !unitX->isHiddenNationality()
			&& !unitX->canCoexistAlwaysOnPlot(*plot0))
			{
				return -1;
			}
		}
		else if (
			unitX->isEnemy(eTeam)
		&&  unitX->canAttack()
		&& !unitX->isInvisible(eTeam)
		&& (!plot0->isCity(true) || !unitX->isBlendIntoCity()))
		{
			return 1;
		}
	}
	else if (
		unitX->isEnemy(eTeam)
	&&  unitX->canAttack()
	&& !unitX->isInvisible(eTeam)
	&&  unitX->canEnterOrAttackPlot(plot0))
	{
		if (!bTestMoves)
		{
			return 1;
		}
		// Toffer - Would need a seperate path generator, or a second set of path generation cache, here to check if unitX can reach plot0 in one turn
		//	because generatePath calls this function so calling generatePath again inside a generatePath call would mess up the caching of it for the first call.
		if (iDistance <= unitX->baseMoves() + plotX->isValidRoute(unitX))
		{
			return 1;
		}
	}
	return 0;
}

bool CvPlayerAI::AI_getAnyPlotDanger(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	if (iRange == -1) iRange = DANGER_RANGE;

	if (isTurnActive())
	{
		PROFILE("CvPlayerAI::AI_getAnyPlotDanger.ActiveTurn");

		if (iRange <= pPlot->getActivePlayerSafeRangeCache(bTestMoves))
		{
			PROFILE("CvPlayerAI::AI_getAnyPlotDanger.NoDangerHit");
			return false;
		}
		if (iRange >= DANGER_RANGE && pPlot->getActivePlayerHasDangerCache(bTestMoves))
		{
			PROFILE("CvPlayerAI::AI_getAnyPlotDanger.HasDangerHit");
			return true;
		}
	}
	bool bResult = false;

	const TeamTypes eTeam = getTeam();
	const bool bCityOrFort = pPlot->isCity(true);

	bool bDefendedPlot = false;
	{
		int iCount = 0;
		foreach_(const CvUnit * unitX, pPlot->units())
		{
			iCount += AI_plotDangerUnitCheck(pPlot, pPlot, unitX, eTeam);
			if (iCount > 2)
			{
				break;
			}
			else if (iCount < -2)
			{
				bDefendedPlot = true;
				break;
			}
		}
		if (!bDefendedPlot)
		{
			if (iCount < 0)
			{
				bDefendedPlot = true;
			}
			else if (iCount > 0)
			{
				bResult = true;
			}
		}
	}
	// Exclude cities and defended plots from the hostile border proximity check.
	const bool bCheckBorder = !bResult && !bDefendedPlot && !pPlot->isCity();

	if (bCheckBorder && iRange >= 2 && pPlot->getBorderDangerCache(eTeam))
	{
		bResult = true;
	}

	// If we have plot danger count here over the same threhold that workers
	//	use to require escorts then consider that dangerous for the AI
	if (!bResult && !isHumanPlayer() && pPlot->getDangerCount(m_eID) > 20)
	{
		bResult = true;
	}

	if (!bResult)
	{
		foreach_(const CvPlot* plotX, pPlot->rect(iRange, iRange))
		{
			if (plotX->area() == pPlot->area())
			{
				const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), plotX->getX(), plotX->getY());

				if (bCheckBorder && iDistance <= 2
				&& plotX->getTeam() != NO_PLAYER
				// AI shouldn't be aware of hostile borders it hasn't revealed yet
				// No biggie that they think the danger is over after a owner change they are not aware of yet.
				&& plotX->getRevealedOwner(eTeam, false) == plotX->getTeam()
				&& atWar(plotX->getTeam(), eTeam))
				{
					if (pPlot != plotX)
					{
						pPlot->setBorderDangerCache(eTeam, true);
						plotX->setBorderDangerCache(eTeam, true);
						// Only set the cache for the plotX team if pPlot is owned by us! (ie. owned by their enemy)
						// It's ok to aproximate that pPlot is revealed by the other team if they have borders that close to our borders.
						if (pPlot->getTeam() == eTeam)
						{
							pPlot->setBorderDangerCache(plotX->getTeam(), true);
							plotX->setBorderDangerCache(plotX->getTeam(), true);
						}
					}
					else pPlot->setBorderDangerCache(eTeam, true);

					bResult = true;
					break;
				}

				if (plotX->isVisible(eTeam, false) && (pPlot != plotX || !bDefendedPlot))
				{
					foreach_(const CvUnit * unitX, plotX->units())
					{
						if (AI_plotDangerUnitCheck(pPlot, plotX, unitX, eTeam, iDistance, bTestMoves) > 0)
						{
							bResult = true;
							break;
						}
					}
				}
			}
		}
	}

	if (isTurnActive() && !GC.getGame().isMPOption(MPOPTION_SIMULTANEOUS_TURNS))
	{
		if (bResult)
		{
			if (iRange <= DANGER_RANGE)
			{
				pPlot->setActivePlayerHasDangerCache(true, bTestMoves);
			}
		}
		else if (iRange < pPlot->getActivePlayerSafeRangeCache(bTestMoves))
		{
			pPlot->setActivePlayerSafeRangeCache(iRange, bTestMoves);
		}
	}
	return bResult;
}

int CvPlayerAI::AI_getPlotDangerInternal(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_EXTRA_FUNC();
	const CvArea* pPlotArea = pPlot->area();
	const TeamTypes eTeam = getTeam();

	int iCount = 0;
	int iBorderDanger = 0;

	OutputDebugString(CvString::format("AI_getPlotDanger for (%d,%d) at range %d (bTestMoves=%d)\n",
		pPlot->getX(), pPlot->getY(),
		iRange,
		bTestMoves).c_str());

	foreach_(const CvPlot * plotX, pPlot->rect(iRange, iRange))
	{
		if (plotX->area() == pPlotArea)
		{
			const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), plotX->getX(), plotX->getY());

			if (iDistance <= 2
			&& plotX->getTeam() != NO_PLAYER // Owned by someone
			// AI shouldn't be aware of hostile borders it hasn't revealed yet
			// No biggie if they think the danger is gone after a owner change they are not yet aware of.
			&& plotX->getRevealedOwner(eTeam, false) == plotX->getTeam()
			&& atWar(plotX->getTeam(), eTeam))
			{
				if (iDistance == 0)
				{
					iBorderDanger += 4;
				}
				else if (iDistance == 1)
				{
					iBorderDanger += 2;
				}
				else iBorderDanger++;
			}

			if (plotX->isVisible(eTeam, false))
			{
				foreach_(const CvUnit * unitX, plotX->units())
				{
					iCount += AI_plotDangerUnitCheck(pPlot, plotX, unitX, eTeam, iDistance, bTestMoves);
				}
			}
		}
	}

	// Note that here we still count border danger in cities - because I want it for AI_cityThreat
	if (iBorderDanger > 0 && (!isHumanPlayer() || pPlot->plotCheck(PUF_canDefend, -1, -1, NULL, getID())))
	{
		iCount += (1 + iBorderDanger) / 2;
	}

	if (isTurnActive() && !GC.getGame().isMPOption(MPOPTION_SIMULTANEOUS_TURNS))
	{
		if (iCount > 0)
		{
			if (iRange <= DANGER_RANGE)
			{
				pPlot->setActivePlayerHasDangerCache(true, bTestMoves);
			}
		}
		else if (iRange < pPlot->getActivePlayerSafeRangeCache(bTestMoves))
		{
			pPlot->setActivePlayerSafeRangeCache(iRange, bTestMoves);
		}
	}
	return iCount;
}

#ifdef PLOT_DANGER_CACHING
plotDangerCache CvPlayerAI::plotDangerCache;
int CvPlayerAI::plotDangerCacheHits = 0;
int CvPlayerAI::plotDangerCacheReads = 0;
#endif

int CvPlayerAI::AI_getPlotDanger(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	if (isTurnActive() && iRange <= pPlot->getActivePlayerSafeRangeCache(bTestMoves))
	{
		return 0;
	}

#ifdef PLOT_DANGER_CACHING
#ifdef _DEBUG
	//	Uncomment this to perform functional verification
	//#define VERIFY_PLOT_DANGER_CACHE_RESULTS
#endif

	//	Check cache first
	int worstLRU = 0x7FFFFFFF;

	struct plotDangerCacheEntry* worstLRUEntry = NULL;
	plotDangerCacheReads++;

	//OutputDebugString(CvString::format("AI_yieldValue (%d,%d,%d) at seq %d\n", piYields[0], piYields[1], piYields[2], yieldValueCacheReads).c_str());
	//PROFILE_STACK_DUMP

	for (int i = 0; i < PLOT_DANGER_CACHE_SIZE; i++)
	{
		struct plotDangerCacheEntry* entry = &plotDangerCache.entries[i];
		if (entry->iLastUseCount == 0)
		{
			worstLRUEntry = entry;
			break;
		}

		if (pPlot->getX() == entry->plotX &&
			 pPlot->getY() == entry->plotY &&
			 iRange == entry->iRange &&
			 bTestMoves == entry->bTestMoves)
		{
			entry->iLastUseCount = ++plotDangerCache.currentUseCounter;
			plotDangerCacheHits++;
#ifdef VERIFY_PLOT_DANGER_CACHE_RESULTS
			int realValue = AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);

			if (realValue != entry->iResult)
			{
				FErrorMsg("Plot danger cache verification failure");
			}
#endif
			return entry->iResult;
		}
		else if (entry->iLastUseCount < worstLRU)
		{
			worstLRU = entry->iLastUseCount;
			worstLRUEntry = entry;
		}
	}

	worstLRUEntry->plotX = pPlot->getX();
	worstLRUEntry->plotY = pPlot->getY();
	worstLRUEntry->iRange = iRange;
	worstLRUEntry->bTestMoves = bTestMoves;
	worstLRUEntry->iLastUseCount = ++plotDangerCache.currentUseCounter;
	worstLRUEntry->iResult = AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);

	return worstLRUEntry->iResult;
#else
	return AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);
#endif
}


int CvPlayerAI::AI_countNumLocalNavy(const CvPlot* pPlot, int iRange) const
{
	PROFILE_FUNC();

	int iCount = 0;

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pLoopPlot->isWater() || pLoopPlot->getPlotCity() != NULL)
		{
			if (pPlot->area() == pLoopPlot->area() || pPlot->isAdjacentToArea(pLoopPlot->getArea()))
			{
				foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
				{
					const UnitAITypes aiType = pLoopUnit->AI_getUnitAIType();

					if (aiType == UNITAI_ATTACK_SEA || aiType == UNITAI_PIRATE_SEA || aiType == UNITAI_RESERVE_SEA)
					{
						if (pLoopUnit->getTeam() == getTeam())
						{
							iCount++;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_getWaterDanger(const CvPlot* pPlot, int iRange) const
{
	PROFILE_FUNC();

	int iCount = 0;

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pLoopPlot->isWater() && pPlot->isAdjacentToArea(pLoopPlot->getArea()))
		{
			iCount += algo::count_if(pLoopPlot->units(),
				CvUnit::fn::isEnemy(getTeam()) &&
				CvUnit::fn::canAttack() &&
				!CvUnit::fn::isInvisible(getTeam(), false)
			);
		}
	}

	return iCount;
}

bool CvPlayerAI::AI_avoidScience() const
{
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  03/08/10								jdog5000	  */
	/*																							  */
	/* Victory Strategy AI																		  */
	/************************************************************************************************/
	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/
	{
		return true;
	}
	if (isCurrentResearchRepeat())
	{
		return true;
	}

	if (isNoResearchAvailable())
	{
		return true;
	}

	return false;
}

short CvPlayerAI::AI_safeFunding() const
{
	short iSafePercent = GC.getDefineINT("SAFE_PROFIT_MARGIN_BASE_PERCENT");

	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		// Afforess - these calculations mimic the Revolution.py assessment for the revolutions mod (check Revolution.py ~ line 1870)
		// Higher safe percents mean AI has to earn more to be considered "safe"

		const int iRank = GC.getGame().getPlayerRank(getID());
		if (iRank < 3)
		{
			iSafePercent += 5 * (4 - iRank);
		}

		const int iAtWarCount = GET_TEAM(getTeam()).getAtWarCount(true);
		if (iAtWarCount > 0)
		{
			iSafePercent -= 10 + 2 * std::min(iAtWarCount, 5);
		}
		if (isCurrentResearchRepeat())
		{
			iSafePercent *= 2;
			iSafePercent /= 3;
		}
		if (getCommercePercent(COMMERCE_CULTURE) > 70)
		{
			iSafePercent *= 2;
			iSafePercent /= 3;
		}
	}
	else
	{
		const int iWarSuccessRatio = GET_TEAM(getTeam()).AI_getWarSuccessCapitulationRatio();
		if (iWarSuccessRatio < -30)
		{
			iSafePercent -= std::max(20, iWarSuccessRatio / 3);
		}

		if (AI_avoidScience())
		{
			iSafePercent -= 8;
		}

		if (isCurrentResearchRepeat())
		{
			iSafePercent -= 10;
		}
	}

	return iSafePercent;
}

// Toffer - Output mainly range from 0 to 100
//	Values above can signify particulary good funding levels,
//	either from a very large treasury or from positive income at 0-10% taxation.
short CvPlayerAI::AI_fundingHealth(int iExtraExpense, int iExtraExpenseMod) const
{
	// The plain question -- "how is my funding, as things stand" -- is the one every valuation asks, and it is a
	// property of the empire, so it cannot differ between two candidates scored in the same pass. It derives once
	// a turn ([patterns.md] § THE VALUATION PROTOCOL: a how-valuable weight is asked at most once per yield per
	// doTurn), NOT once per gold change: keying on the treasury would surrender the ceiling the moment gold moved.
	// ⚠ A HYPOTHETICAL (a war's extra expense CvTeamAI has not committed to) is a different question per argument
	// and is never served from here.
	if (iExtraExpense != 0 || iExtraExpenseMod != 0)
	{
		return AI_fundingHealthUncached(iExtraExpense, iExtraExpenseMod);
	}
	const int iTurn = GC.getGame().getGameTurn();
	if (m_iFundingHealthCacheTurn != iTurn)
	{
		m_iFundingHealthCachedValue = AI_fundingHealthUncached(0, 0);
		m_iFundingHealthCacheTurn = iTurn;
	}
	return m_iFundingHealthCachedValue;
}

short CvPlayerAI::AI_fundingHealthUncached(int iExtraExpense, int iExtraExpenseMod) const
{
	PROFILE_FUNC();
	if (isAnarchy() || isNPC())
	{
		return 100;
	}
	int64_t iNetExpenses;
	short iProfitMargin = getProfitMargin(iNetExpenses, iExtraExpense, iExtraExpenseMod);
	FASSERT_NOT_NEGATIVE(iProfitMargin);

	// Koshling - Never in financial difficulties if we can fund our ongoing expenses with zero taxation
	if (getMinTaxIncome() >= iNetExpenses)
	{
		return 10000; // A magic number in case we want this state to have some kind of significance.
	}
	// Toffer - Things should absolutly be worse off than this before one can claim financial trouble.
	if (iProfitMargin > 25)
	{
		return 200;
	}
	// Toffer - At low to mid tax levels, and with some profit margin to go on, evaluate treasury rather than profit margin.
	if (iProfitMargin > 15 && getCommercePercent(COMMERCE_GOLD) < 50)
	{
		// The empire's realized GOLD is read HERE, not at the top, because this is the only branch that consumes
		// it. The three branches above answer from getMinTaxIncome / getMaxTaxIncome -- bare maintained members --
		// against expenses, so the walk they used to pay for was discarded unread.
		// ⚑ It is the expensive term by a wide margin: CvPlayer::getCommerces answers a receiver channel by
		// re-summing every city's realized combine ([state-repositories.md] § A CROSS-SCOPE receiver total), which
		// on the standing save is 185 cities. Hoisting it into the branch that reads it costs nothing and is
		// exactly equivalent -- nothing between the old site and here consumed it.
		int aiOwnCommerces[NUM_COMMERCE_TYPES];
		getCommerces(aiOwnCommerces);
		const int64_t iNetIncome = aiOwnCommerces[COMMERCE_GOLD] / 100 + std::max(0, getGoldPerTurn());

		// Toffer - Gamespeed (GS) influence the value of gold, so scale gold treshold to GS, era is exponential factor.
		//	Prehistoric: 25 gold (ultrafast); 100 gold (normal); 1000 gold (eternity)
		//	Ancient: 50 gold (ultrafast); 200 gold (normal); 2000 gold (eternity)
		//	Classical: 125 gold (ultrafast); 500 gold (normal); 5000 gold (eternity)
		const int64_t iEraGoldThreshold = AI_goldTarget();
		int64_t iValue;
		if (iEraGoldThreshold < 1)
		{
			// Return a value based on how many turns we have left before strike happens.
			iValue = 400 * getGold() / (std::max(1, std::abs(calculateGoldRate())) * CvGameSpeedScale::hammerCostPercent());
		}
		else if (iNetIncome - iNetExpenses >= 0)
		{
			iValue = 100 * getGold() / iEraGoldThreshold;
		}
		else
		{
			// Losing gold per turn, can we keep this up for X number of turns without going below era treshold.
			// X is: 2 (ultrafast); 10 (normal); 100 (eternity). Need more time to react on  slower GS.
			// Koshling - we're never in financial trouble if we can run at current deficits for more than
			//	Toffer - X (GS scaled) turns and stay in healthy territory, so claim full or even excess funding in such a case!
			const int64_t iFutureGoldPrognosis = getGold() + (iNetIncome - iNetExpenses) * CvGameSpeedScale::hammerCostPercent() / 10;
			iValue = 100 * iFutureGoldPrognosis / iEraGoldThreshold;
		}
		if (iValue > 9999)
		{
			return 10001; // Funding level at 10 000% will be more than adequate to conclude that funding is a non-issue.
		}
		return static_cast<short>(iValue);
	}
	// Toffer - Finances are pretty poor if this point is reached, iProfitMargin is hard to get to a value above 50 in any scenario,
	//	if it is zero it means your expenditure is equal to, or greater than, your income at 100% taxation
	//	if it is *50* it means your expenditure is half the size of your income at 100% taxation.
	//	A value of 100 is close to impossible to reach, as that either means expenditure is zero, or that income is severeal orders of magnitude greater than your expenditure.
	//	Multiplying with 2 may not be needed, but it is to signify that 10% iProfitMargin is not actually all that bad.
	//	Max return value here is 50 as iProfitMargin is guaranteed 25 or less at this point.
	return iProfitMargin * 2;
}


// Calculate a (percentage) modifier the AI can apply to gold to determine how to value it
int CvPlayerAI::AI_goldValueAssessmentModifier() const
{
	// If we're only just funding at the safety level that's not good - rate that as 150% valuation for gold
	return std::max(1, AI_safeFunding() * 100 / std::max<short>(1, AI_fundingHealth()));
}


bool CvPlayerAI::AI_hasCriticalGold() const
{

	int64_t iGoldLimit1 = 100;
	int64_t iGoldLimit2 = 50;
	int64_t iGoldLimit3 = 20;
	int64_t iGoldperturnLimit1 = -5;
	int64_t iGoldperturnLimit2 = 5;
	int iGoldPerTurn = calculateGoldRate();
	const bool isGoldcritical = (m_iGold < iGoldLimit1 && iGoldPerTurn < iGoldperturnLimit1) || (m_iGold < iGoldLimit2 && iGoldPerTurn < iGoldperturnLimit2) || m_iGold < iGoldLimit3;
	if (isGoldcritical)
	{
		const int iEarlyGameTimeLimit = 50 * CvGameSpeedScale::speedPercent() / 100;
		if (GC.getGame().getGameTurn() < iEarlyGameTimeLimit) return false;
	}
	return isGoldcritical;
}

bool CvPlayerAI::AI_isFinancialTrouble() const
{
	PROFILE_FUNC();
	if (isNPC()) return false;

	// ⛔ MEMOIZED, and the memo is the whole point of this body -- see the members' comment in CvPlayerAI.h.
	// AI_fundingHealth reaches CvPlayer::getCommerces, which re-sums EVERY city's realized combine; asking that
	// once per building candidate is what wedged a late-game turn at 99% of one core for 45+ minutes.
	// ⚠ The key is (turn, gold) because gold is what moves the verdict inside a turn. A scoring pass moves
	// neither, so it derives ONCE; a genuine change re-derives immediately.
	const int iTurn = GC.getGame().getGameTurn();
	const int64_t iGold = getGold();

	if (m_iFinancialTroubleCacheTurn == iTurn && m_iFinancialTroubleCacheGold == iGold)
	{
		return m_bFinancialTroubleCachedValue;
	}
	const bool bFundingTrouble = AI_fundingHealth() < AI_safeFunding();
	const bool bGoldCritical = AI_hasCriticalGold();

	m_bFinancialTroubleCachedValue = bFundingTrouble || bGoldCritical;
	m_iFinancialTroubleCacheTurn = iTurn;
	m_iFinancialTroubleCacheGold = iGold;

	return m_bFinancialTroubleCachedValue;
}

int64_t CvPlayerAI::AI_goldTarget() const
{
	PROFILE_EXTRA_FUNC();
	if (getNumCities() < 1 || isNPC())
	{
		return 0;
	}
	const int iEra = GC.getGame().getCurrentEra() + 1;
	const int iModGS = (
		(
			CvGameSpeedScale::speedPercent()
			+
			CvGameSpeedScale::hammerCostPercent()
			+
			GC.getHandicapInfo(GC.getGame().getHandicapType()).getCostsModifier(COSTS_TRAIN, CASC_SCOPE_EMPIRE, true)
		)
	);
	int64_t iGold = iEra * (iEra * 2 * getNumCities() + getTotalPopulation()) * iModGS / 300;

	iGold *= getInflationMod10000();
	iGold /= 10000;

	const bool bAnyWar = GET_TEAM(getTeam()).hasWarPlan(true);
	if (bAnyWar)
	{
		iGold *= 3;
		iGold /= 2;
	}

	// Afforess 02/01/10
	if (!GET_TEAM(getTeam()).isGoldTrading() || !GET_TEAM(getTeam()).isTechTrading() || GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING))
	{ // Don't bother saving gold if we can't trade it for anything
		iGold /= 3;
	}
	else if (GC.getGame().isOption(GAMEOPTION_NO_TECH_BROKERING))
	{ // Gold is less useful without tech brokering
		iGold *= 3;
		iGold /= 4;
	}
	// ! Afforess

	if (AI_avoidScience())
	{
		iGold *= 10;
	}
	iGold += AI_goldToUpgradeAllUnits() / (!bAnyWar + 1);

	for (int iI = GC.getNumCorporationInfos() - 1; iI > -1; iI--)
	{
		if (getHasCorporationCount((CorporationTypes)iI) > 0)
		{
			iGold += std::max(0, 2*GC.getCorporationInfo((CorporationTypes)iI).getSpreadCost());
			break;
		}
	}
	return iGold + AI_getExtraGoldTarget();
}

// THE RESEARCH CANDIDATE WALK (enabler.md §6) -- the ONE place a tech search builds its candidate set.
// It starts at the enabler's LISTED techs (what is researchable NOW) and follows `leadsTo`, the load-built
// forward index naming the techs that list a tech as their prereq, one hop per step out to iWalkDepth. It
// replaces reading the whole tech database and computing a path length for every entry.
// ⚑ Hop depth <= findPathLength BY CONSTRUCTION: every tech on a shortest path is reached by following leadsTo
// from its own prereq. So the set is a SUPERSET of what a path-length test keeps -- it drops no candidate a
// database sweep would have found, and findBestPath still resolves the real path and cost per candidate.
void CvPlayerAI::AI_walkResearchFrontier(int iWalkDepth, std::set<int>& candidateTechs) const
{
	PROFILE_EXTRA_FUNC();

	const CvTeam& kTeam = GET_TEAM(getTeam());

	std::vector<int> researchableNow;
	m_enabler.techs.listedIds(researchableNow);

	candidateTechs.clear();
	candidateTechs.insert(researchableNow.begin(), researchableNow.end());

	std::vector<int> walkWave(researchableNow.begin(), researchableNow.end());

	for (int iWalkStep = 1; iWalkStep < iWalkDepth && !walkWave.empty(); ++iWalkStep)
	{
		std::vector<int> nextWave;

		for (size_t iAt = 0; iAt < walkWave.size(); ++iAt)
		{
			foreach_(const TechTypes eLeadsTo, GC.getTechInfo(static_cast<TechTypes>(walkWave[iAt])).getLeadsToTechs())
			{
				if (!kTeam.isHasTech(eLeadsTo) && candidateTechs.insert((int)eLeadsTo).second)
				{
					nextWave.push_back((int)eLeadsTo);
				}
			}
		}
		walkWave.swap(nextWave);
	}
}

TechTypes CvPlayerAI::AI_bestTech(int iMaxPathLength, bool bIgnoreCost, bool bAsync, TechTypes eIgnoreTech, AdvisorTypes eIgnoreAdvisor)
{
	PROFILE("CvPlayerAI::AI_bestTech");

	int iValue;
	int iBestValue = 0;
	TechTypes eBestTech = NO_TECH;
	TechTypes eFirstTech = NO_TECH;
	int iPathLength;
	const CvTeam& kTeam = GET_TEAM(getTeam());

	// Afforess 08/09/10
	// Forces AI to Beeline for Religious Techs if they have no religions
	bool bValid = GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS);
	if (!bValid)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (hasTrait((TraitTypes)iI) && GC.getTraitInfo((TraitTypes)iI).getMaxAnarchy() >= 0)
			{
				bValid = true;
				break;
			}
		}
	}
	if (bValid)
	{
		if (getCommercePercent(COMMERCE_RESEARCH) < 90)
		{
			bValid = false;
		}
		if (countHolyCities() > 0 && (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION) || GC.getGame().isOption(GAMEOPTION_RELIGION_LIMITED)))
		{
			bValid = false;
		}
		if (!GET_TEAM(getTeam()).hasWarPlan(true))
		{
			bValid = false;
		}
		if (getNumCities() == 1)
		{
			bValid = false;
		}
	}
	if (bValid)
	{
		eBestTech = AI_bestReligiousTech(iMaxPathLength * 3, eIgnoreTech, eIgnoreAdvisor);
		if (eBestTech != NO_TECH)
		{
			//	Don't retain the beeline persistently since we need to re-evaluate
			//	each turn in case someone has beaten us to it for religions
			return eBestTech;
		}
	}
	// ! Afforess

	// If we had already decided to beeline previously, stick with it
	if (m_eBestResearchTarget != NO_TECH && iMaxPathLength > 1)
	{
		if ((canEverResearch(m_eBestResearchTarget)))
		{
			techPath* path = findBestPath(m_eBestResearchTarget, iValue, bIgnoreCost, bAsync);

			eFirstTech = findStartTech(path);

			return eFirstTech;
		}
	}


	bool beeLine = false;
	int beeLineThreshold;

	// The candidates are WALKED OUTWARD FROM THE FRONTIER, never read off the whole tech database. iMaxPathLength
	// IS the AI's research search depth (hardcoded 3 today, an AI variable later), so it bounds the walk and
	// every test below -- nothing is scored deeper than the AI is configured to look.
	std::set<int> candidateTechs;
	AI_walkResearchFrontier(iMaxPathLength, candidateTechs);

	do
	{
		for (std::set<int>::const_iterator itCandidate = candidateTechs.begin(); itCandidate != candidateTechs.end(); ++itCandidate)
		{
			const TechTypes eTechX = static_cast<TechTypes>(*itCandidate);

			if (eIgnoreTech == NO_TECH || eTechX != eIgnoreTech)
			{
				if ((eIgnoreAdvisor == NO_ADVISOR || GC.getTechInfo(eTechX).getAdvisor() != eIgnoreAdvisor)
				&& (canEverResearch(eTechX))
				&& GC.getTechInfo(eTechX).getEra() <= getCurrentEra() + 1)
				{
					iPathLength = findPathLength(eTechX, false);

					bool bValid = false;

					if (!beeLine)
					{
						bValid = iPathLength <= iMaxPathLength;
					}
					// ⛔ THE BEELINE NO LONGER REACHES PAST THE AI'S OWN SEARCH DEPTH. It ran to
					// iMaxPathLength * 7 -- 21 hops at the AI's hardcoded 3, i.e. most of the tech tree -- which
					// is exactly the "beeline five techs deep" shape AGENTS.md names as the enablement
					// pathology. It now picks a FURTHER target within the same depth on value/cost, never a
					// deeper one, so the depth variable alone governs how far the AI commits.
					else if (iPathLength > 1 && iPathLength <= iMaxPathLength)
					{
						const int iTempValue = AI_TechValueCached(eTechX, bAsync);

						bValid = iTempValue * 100 / GC.getTechInfo(eTechX).getResearchCost() > iBestValue;
					}

					if (bValid)
					{
						techPath* path = findBestPath(eTechX, iValue, bIgnoreCost, bAsync);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eBestTech = eTechX;
							eFirstTech = findStartTech(path);
						}
						delete path;
					}
				}
			}
		}

		// Don't beeline for async human advisor calls to this method and especially don't get a synced rand to decide if you want to beeline
		if (bAsync)
			break;

		//	Most of the time check for good bee-lines.  The probability is so high because the AI will
		//	re-evaluate every time it earns a tech so it will re-decide to beeline (or not) each time
		//	Barbarians are stupid and never beeline
		if (isNPC() || iMaxPathLength == 1 || GC.getGame().getSorenRandNum(8, "AI tech beeline") == 0)
		{
			break;
		}

		beeLineThreshold = iBestValue;
		beeLine = !beeLine;
	} while (beeLine);

	if (eBestTech != NO_TECH && eFirstTech != NO_TECH)
	{
		logDecisionAI(1, "[DAI/tech/best] player=%d (%S) picks=%S value/cost=%d start=%S",
			getID(), getCivilizationDescription(0), GC.getTechInfo(eBestTech).getDescription(),
			iBestValue, GC.getTechInfo(eFirstTech).getDescription());
	}

	if (iMaxPathLength > 1)
	{
		//	Only cache tragets generated from non-immediate best-tech searches
		m_eBestResearchTarget = eBestTech;
	}

	return eFirstTech;
}

struct TechResearchDist
{
	TechResearchDist(TechTypes tech = NO_TECH, int dist = 0) : tech(tech), dist(dist) {}

	bool operator<(const TechResearchDist& other) const
	{
		return dist < other.dist;
	}

	TechTypes tech;
	int dist;
};

//	Calculate an estimate of the value of the average tech amongst those we could currently research
//	for performance reasons we just sample rather than measuring all possibilities
int CvPlayerAI::AI_averageCurrentTechValue(TechTypes eRelativeTo, bool bAsync)
{
	PROFILE_EXTRA_FUNC();
	const size_t MAX_SAMPLE_SIZE = 4;
	const CvTeamAI& team = GET_TEAM(getTeam());

	int iCost = team.getResearchCost(eRelativeTo);

	//	Determine the sample to use - we use the researchable techs closest in base cost to the one we are seeking to compare with the average
	std::vector<TechResearchDist> researchCosts;
	std::vector<int> researchableTechs;
	getAvailableTechs(researchableTechs);
	foreach_(const int iTechX, researchableTechs)
	{
		const TechTypes eTechX = (TechTypes)iTechX;
		if (eTechX != eRelativeTo)
		{
			researchCosts.push_back(TechResearchDist(eTechX, std::abs(team.getResearchCost(eTechX) - iCost)));
		}
	}

	// We couldn't find any techs to sample so return early
	if (researchCosts.empty())
	{
		return AI_TechValueCached(eRelativeTo, bAsync);
	}
	// Sort for closest first
	algo::sort(researchCosts);
	researchCosts.resize(std::min(researchCosts.size(), MAX_SAMPLE_SIZE));

	int iTotal = 0;
	int iDivisor = 1;
	foreach_(const TechResearchDist & itr, researchCosts)
	{
		const int iValue = AI_TechValueCached(itr.tech, bAsync);

		while (MAX_INT - iTotal < iValue / iDivisor)
		{
			iTotal /= 2;
			iDivisor *= 2;
		}

		iTotal += iValue / iDivisor;
	}

	return (iTotal / researchCosts.size()) * iDivisor;
}

int CvPlayerAI::AI_TechValueCached(TechTypes eTech, bool bAsync, bool considerFollowOns)
{
	PROFILE_FUNC();

	int iValue;

	TechTypesValueMap::const_iterator techValueItr = m_cachedTechValues.find(eTech);
	if (techValueItr == m_cachedTechValues.end())
	{
		PROFILE("CvPlayerAI::AI_TechValueCached.CacheMiss");

		iValue = AI_techValue(eTech, findPathLength(eTech, false), true, bAsync);

		if (!bAsync)
		{
			m_cachedTechValues[eTech] = iValue;
		}
	}
	else
	{
		iValue = m_cachedTechValues[eTech];
	}

	if (considerFollowOns)
	{
		int iTotalWeight = 100;

		// What does it (immediately) lead to?
		foreach_(const TechTypes eLeadsTo, GC.getTechInfo(eTech).getLeadsToTechs())
		{
			bool bIsORPreReq = false;
			foreach_(const TechTypes ePrereqOr, GC.getTechInfo(eLeadsTo).getPrereqOrTechs())
			{
				if (ePrereqOr == eTech)
				{
					bIsORPreReq = true;
				}
				else if (GET_TEAM(getTeam()).isHasTech(ePrereqOr))
				{
					// Already got an OR pre-req, another makes no difference
					bIsORPreReq = false;
					break;
				}
			}

			bool bIsANDPreReq = false;
			int iANDPrereqs = 0;
			foreach_(const TechTypes ePrereqAnd, GC.getTechInfo(eLeadsTo).getPrereqAndTechs())
			{
				if (!GET_TEAM(getTeam()).isHasTech(ePrereqAnd))
				{
					iANDPrereqs++;
					if (ePrereqAnd == eTech)
					{
						bIsANDPreReq = true;
					}
				}
			}

			if (bIsORPreReq || bIsANDPreReq)
			{
				// Consider all the AND pre-reqs as worth 33% of the follow on, and significant OR as 25%
				const int iANDPercentage = (bIsANDPreReq ? 33 / iANDPrereqs : 0);
				const int iORPercentage = (bIsORPreReq ? 25 : 0);

				iTotalWeight += iANDPercentage + iORPercentage;

				iValue += (iANDPercentage + iORPercentage) * AI_TechValueCached(eLeadsTo, bAsync) / 100;
			}
		}

		//	Normalize to an average to make it comparable with a tech evaluated without follow-ons
		while (iValue > MAX_INT / 100)
		{
			iValue /= 2;
			iTotalWeight /= 2;

			if (iTotalWeight == 0)
			{
				iTotalWeight = 1;
			}
		}
		iValue = iValue * 100 / iTotalWeight;
	}

	return iValue;
}

int CvPlayerAI::techPathValuePerUnitCost(techPath* path, TechTypes eTech, bool bIgnoreCost, bool bAsync)
{
	PROFILE_EXTRA_FUNC();
	int	iCost = 0;
	int	iValue = 0;
	int iScaleFactor = 1;

	foreach_(const TechTypes & loopTech, *path)
	{
		int iTempCost = std::max(1, GET_TEAM(getTeam()).getResearchCost(loopTech) - GET_TEAM(getTeam()).getResearchProgress(loopTech));
		int iTempValue = AI_TechValueCached(loopTech, bAsync);

		iCost += iTempCost;
		iValue += iTempValue / iScaleFactor;

		while (iValue > MAX_INT / 100)
		{
			iScaleFactor *= 2;
			iValue /= 2;
		}
	}

	int iCostFactor = iCost;
	int iIterator = ((int)GC.getGame().getCurrentEra() - 1);
	for (int iI = 0; iI < iIterator; iI++)
	{
		iCostFactor /= 2;
	}
	iCostFactor = std::max(1, iCostFactor);
	iValue = std::max(1, (100 * iValue) / (bIgnoreCost ? 1 : iCostFactor));

	if (iValue > MAX_INT / iScaleFactor)
	{
		return MAX_INT;
	}
	else
	{
		return iValue * iScaleFactor;
	}
}

techPath* CvPlayerAI::findBestPath(TechTypes eTech, int& valuePerUnitCost, bool bIgnoreCost, bool bAsync)
{
	PROFILE_FUNC();

	std::vector<techPath*> possiblePaths;
	techPath* initialSeed = new techPath();

	possiblePaths.push_back(initialSeed);

	constructTechPathSet(eTech, possiblePaths, *initialSeed);

	//	Find the lowest cost of the possible paths
	int	iBestValue = 0;
	techPath* bestPath = NULL;

	foreach_(techPath * path, possiblePaths)
	{
		const int iValue = techPathValuePerUnitCost(path, eTech, bIgnoreCost, bAsync);
		if (iValue >= iBestValue)
		{
			iBestValue = iValue;
			bestPath = path;
		}
	}

	foreach_(const techPath * path, possiblePaths)
	{
		if (path != bestPath)
		{
			delete path;
		}
	}

	valuePerUnitCost = iBestValue;
	return bestPath;
}

TechTypes CvPlayerAI::findStartTech(techPath* path) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const TechTypes & tech, *path)
	{
		if ((getTechAvailability(tech) == EnablerDomain::STATE_LISTED))
		{
			return tech;
		}
	}

	return NO_TECH;
}


void CvPlayerAI::resetBonusClassTallyCache(const int iTurn, const bool bFull)
{
	PROFILE_EXTRA_FUNC();
	if (m_iBonusClassTallyCachedTurn != iTurn)
	{
		if (bFull)
		{
			for (int iI = GC.getNumBonusClassInfos() - 1; iI > -1; iI--)
			{
				m_bonusClassRevealed[iI] = 0;
				m_bonusClassUnrevealed[iI] = 0;
				m_bonusClassHave[iI] = 0;
			}
			const CvTeam& team = GET_TEAM(getTeam());

			for (int iI = GC.getNumBonusInfos() - 1; iI > -1; iI--)
			{
				const CvBonusInfo& bonus = GC.getBonusInfo((BonusTypes)iI);
				if (bonus.getTechReveal() != NO_TECH)
				{
					if (team.isHasTech((TechTypes)bonus.getTechReveal()))
					{
						m_bonusClassRevealed[bonus.getBonusClassType()]++;
					}
					else m_bonusClassUnrevealed[bonus.getBonusClassType()]++;


					if (getNumAvailableBonuses((BonusTypes)iI) > 0 || countOwnedBonuses((BonusTypes)iI) > 0)
					{
						m_bonusClassHave[bonus.getBonusClassType()]++;
					}
				}
			}
		}
		m_iBonusClassTallyCachedTurn = iTurn;
	}
}

int CvPlayerAI::AI_techValue(TechTypes eTech, int iPathLength, bool bIgnoreCost, bool bAsync)
{
	PROFILE_FUNC();

	resetBonusClassTallyCache(GC.getGame().getGameTurn());

	const CvTechInfo& kTech = GC.getTechInfo(eTech);
	const CvTeam& kTeam = GET_TEAM(getTeam());

	
	CvCity* pCapitalCity = getCapitalCity();

	bool bCapitalAlone = (GC.getGame().getElapsedGameTurns() > 0) ? AI_isCapitalAreaAlone() : false;
	bool bFinancialTrouble = AI_isFinancialTrouble();
	bool bAdvancedStart = getAdvancedStartPoints() >= 0;

	int iHasMetCount = kTeam.getHasMetCivCount(true);
	int iCoastalCities = countNumCoastalCities();
	int iConnectedForeignCities = countPotentialForeignTradeCitiesConnected();

	const int iCityCount = getNumCities();
	const int iRCSMultiplier = 1 + GC.getGame().isOption(GAMEOPTION_CULTURE_REALISTIC_SPREAD);

	int iValue = 0;

	int iRandomMax = 2000;

	// Map stuff
	if (iCoastalCities > 0 && kTech.getCapabilities()->has("canSeeFurtherFromWater"))
	{
		iValue += 100 * iRCSMultiplier;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	if (kTech.getCapabilities()->has("hasCenteredMap"))
	{
		iValue += 100;
	}

	if (kTech.getCapabilities()->has("hasWholeMapRevealed"))
	{
		iValue += 100;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	// Expand trading options
	if (kTech.providesCanTrade(CLS_CANTRADE_MAPS))
	{
		iValue += 100;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_TECHS) && !GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING))
	{
		iValue += 500;

		iValue += 500 * iHasMetCount;
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_GOLD))
	{
		iValue += 200;

		if (iHasMetCount > 0)
		{
			iValue += 400;
		}
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_OPEN_BORDERS) && iHasMetCount > 0)
	{
		iValue += 500;

		if (iCoastalCities > 0)
		{
			iValue += 400;
		}

		if (isMinorCiv() && GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_START_AS_MINORS))
		{
			iValue += 250 + 120 * iHasMetCount;
		}
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_DEFENSIVE_PACT))
	{
		iValue += 400;
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_PERMANENT_ALLIANCE) && GC.getGame().isOption(GAMEOPTION_ENABLE_PERMANENT_ALLIANCES))
	{
		iValue += 200;
	}

	if (kTech.providesCanTrade(CLS_CANTRADE_VASSALS) && !GC.getGame().isOption(GAMEOPTION_NO_VASSAL_STATES))
	{
		iValue += 200;
	}

	// Tile improvement abilities
	if (kTech.getCapabilities()->has("canBuildBridges"))
	{
		iValue += 400 * iRCSMultiplier;
	}

	if (kTech.getCapabilities()->has("canSpreadIrrigation"))
	{
		iValue += 400;
	}

	if (kTech.getCapabilities()->has("canIgnoreIrrigation"))
	{
		iValue += 500;
	}

	if (kTech.providesCanWorkOn(CLS_CANWORKON_WATER))
	{
		iValue += (600 * iCoastalCities);
	}

	if (kTech.getCapabilities()->has("canPassPeaks"))
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			const CvPlot* pPlot = GC.getMap().plotByIndex(iI);
			if (pPlot->isAsPeak() && pPlot->getOwner() != NO_PLAYER
			&& GET_PLAYER(pPlot->getOwner()).getID() == getID())
			{
				iValue += 35 * iRCSMultiplier;
			}
		}
	}

	if (kTech.getCapabilities()->has("canMoveFastOnPeaks"))
	{
		iValue += 150 * iRCSMultiplier;
	}

	if (kTech.getCapabilities()->has("canFoundOnPeaks"))
	{
		iValue += 100 * iRCSMultiplier;
	}

	

	int iTempValue = 0;
	// WHAT DOES THIS TECH OBSOLETE? -- the tech's own `obsoletes` edge IS the answer, in place of asking every
	// corporation the reverse question (patterns.md § THE WHAT-IF DRIVER). The edge is the same one the load
	// pass reads to stamp each corporation's obsoleting tech, so the two cannot disagree.
	std::set<int> obsoletedCorporations;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_OBSOLETES, EDGEB_CORPORATIONS, obsoletedCorporations);

	for (std::set<int>::const_iterator itObsoleted = obsoletedCorporations.begin(); itObsoleted != obsoletedCorporations.end(); ++itObsoleted)
	{
		const CorporationTypes eLoopCorporation = static_cast<CorporationTypes>(*itObsoleted);

		foreach_(const CvCity * pLoopCity, cities())
		{
			if (pLoopCity->isHasCorporation(eLoopCorporation))
			{
				iTempValue -= AI_corporationValue(eLoopCorporation, pLoopCity);
			}
		}
	}
	iValue += iTempValue / 1000;

	

	iTempValue = 0;
	// Same forward read for the promotion plane -- the tech's `obsoletes` edge, not a sweep of all ~1,229
	// promotions per tech valued.
	std::set<int> obsoletedPromotions;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_OBSOLETES, EDGEB_PROMOTIONS, obsoletedPromotions);

	for (std::set<int>::const_iterator itObsoleted = obsoletedPromotions.begin(); itObsoleted != obsoletedPromotions.end(); ++itObsoleted)
	{
		const PromotionTypes eLoopPromotion = static_cast<PromotionTypes>(*itObsoleted);

		foreach_(const CvUnit * pLoopUnit, units())
		{
			if (pLoopUnit->isHasPromotion(eLoopPromotion))
			{
				iTempValue -= AI_promotionValue(eLoopPromotion, pLoopUnit->getUnitType(), pLoopUnit, pLoopUnit->AI_getUnitAIType());
			}
		}
	}
	iValue += iTempValue / 100;

	

	iTempValue = 0;

	if (kTech.getCapabilities()->has("canRebaseAnywhere") && GC.getMAX_AIRLIFT_RANGE() > 0)
	{
		iValue += 300;
	}

	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		const int iTechFreeSpecialists =
			InfoValuation::keyedTarget(kTech.getModifiers(), MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, -1, iI) / 100;
		if (iTechFreeSpecialists != 0)
		{
			iValue += 50 * getNumCities() * iTechFreeSpecialists;
		}
	}

	// featureProduction / workRate are PERCENT slots, so they read 1:1; the trade-route count is a FLAT and
	// reduces at its point of use ([DEC-fixedpoint-x100] -- a bare re-point would be 100x here).
	iValue += (kTech.getScalar(SCALAR_FEATURE_PRODUCTION, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * 2);
	iValue += (kTech.getScalar(SCALAR_WORK_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * 4);
	iValue += (kTech.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_CITY) / 100
	        * (std::max((getNumCities() + 2), iConnectedForeignCities) + 1) * ((bFinancialTrouble) ? 200 : 100));

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION4))
	{
		iValue += (kTech.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) / 100 * 350);
	}
	else
	{
		iValue += (kTech.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) / 100 * 200);
	}

	// A route's tech-gated movement cost is the route's OWN `movement` entry under a condition, so the worth of
	// the tech is the DELTA it makes to that cost -- and a CHEAPER route is better, hence the negation. Driven
	// from the tech's own edges rather than a scan of every route ([modifier.md] §5).
	// The as-if-held pair this tech is valued through, shared by every delta below: what an entity is worth
	// WITH this tech minus what it is worth WITHOUT. A tech-gated deposit is the target's OWN output under a
	// condition ([DEC-deliveryguy]), so the tech's worth is exactly that difference.
	CvCascadeHypothetical kWithTech;
	kWithTech.present[EDGEB_TECHS].insert((int)eTech);
	CvCascadeHypothetical kWithoutTech;
	kWithoutTech.absent[EDGEB_TECHS].insert((int)eTech);

	if (pCapitalCity != NULL)
	{
		const CityContext& kCityContext = pCapitalCity->getCityContext();
		const EmpireContext& kEmpireContext = getEmpireContext();
		const CvPlotGroup* pPlotGroup = pCapitalCity->plotGroup(getID());

		std::set<int> kRelatedRoutes;
		EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech),
			EDGEF_RELATED, EDGEB_ROUTES, kRelatedRoutes);

		for (std::set<int>::const_iterator it = kRelatedRoutes.begin(); it != kRelatedRoutes.end(); ++it)
		{
			const CvModifiers* pRouteModifiers = GC.getRouteInfo((RouteTypes)*it).getModifiers();
			const int iWith = InfoValuation::expectedSum(pRouteModifiers, MODFAM_MOVEMENT, 0, CASC_UNIT_FLAT,
				kCityContext, kEmpireContext, pPlotGroup, &kWithTech);
			const int iWithout = InfoValuation::expectedSum(pRouteModifiers, MODFAM_MOVEMENT, 0, CASC_UNIT_FLAT,
				kCityContext, kEmpireContext, pPlotGroup, &kWithoutTech);
			// a CHEAPER route is better, hence the negation
			iValue += -(iWith - iWithout);
		}
	}

	// The keyed entry list -- the handful this tech authored, never a walk of the domain enum ([modifier.md] §5).
	std::vector<std::pair<int, int> > kDomainMoves;
	InfoValuation::collectKeyedTarget(kTech.getModifiers(), MODFAM_DOMAIN_MOVES, 0,
		InfoValuation::keyedTargetSegment("domains"), kDomainMoves, CASC_SCOPE_EMPIRE);
	for (size_t iD = 0; iD < kDomainMoves.size(); ++iD)
	{
		iValue += kDomainMoves[iD].second * 200;
	}

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		// Does this tech unlock the channel's SLIDER? The grantor's own `capabilities` block, keyed by the
		// channel's slider capability -- GOLD is residual and has none, which answers false without a special
		// case here.
		if (kTech.providesCapability(CapabilityContext::commerceRateCapability((CommerceTypes)iI)))
		{
			iValue += 100;
			if (iI == COMMERCE_CULTURE && AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
			{
				iValue += 1000;
			}
		}
	}

	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
	{
		if (kTech.canTradeOnTerrain(iI))
		{
			if (GC.getTerrainInfo((TerrainTypes)iI).isWaterTerrain())
			{
				if (pCapitalCity != NULL)
				{
					iValue += (countPotentialForeignTradeCities(pCapitalCity->area()) * 100);
				}

				if (iCoastalCities > 0)
				{
					iValue += ((bCapitalAlone) ? 950 : 350) * iRCSMultiplier;
				}

				iValue += 50;
			}
			else iValue += 1000;
		}
	}

	if (kTech.getCapabilities()->has("hasRiverTrade"))
	{
		iValue += 1000 * iRCSMultiplier;
	}

	/* ------------------ Tile Improvement Value  ------------------ */
	// What this tech does to tile yields is the tech's OWN edge fetch, never a scan of every improvement asking
	// whether it happens to mention the tech ([modifier.md] §5 -- the own-data inversion; the todo's "drive them
	// from the tech's own edges"). An improvement's tech-gated yield is its OWN output under a condition
	// ([DEC-deliveryguy]), so the worth is the DELTA between holding this tech and not -- one hypothetical pair
	// per related improvement, not a table lookup that no longer exists.
	int iTileImprovementValue = 0;
	std::set<int> kRelatedImprovements;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech),
		EDGEF_RELATED, EDGEB_IMPROVEMENTS, kRelatedImprovements);

	if (!kRelatedImprovements.empty() && pCapitalCity != NULL)
	{
		const CityContext& kCityContext = pCapitalCity->getCityContext();
		const EmpireContext& kEmpireContext = getEmpireContext();
		const CvPlotGroup* pPlotGroup = pCapitalCity->plotGroup(getID());

		for (std::set<int>::const_iterator it = kRelatedImprovements.begin(); it != kRelatedImprovements.end(); ++it)
		{
			const CvImprovementInfo& kImprovement = GC.getImprovementInfo((ImprovementTypes)*it);
			int aiWith[NUM_YIELD_TYPES];
			int aiWithout[NUM_YIELD_TYPES];
			kImprovement.expectedFlatYields(kCityContext, kEmpireContext, pPlotGroup, aiWith, &kWithTech);
			kImprovement.expectedFlatYields(kCityContext, kEmpireContext, pPlotGroup, aiWithout, &kWithoutTech);

			for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
			{
				const int iDelta = (aiWith[iK] - aiWithout[iK]) / 100;
				if (iDelta == 0)
				{
					continue;
				}
				// Often an improvement only becomes viable once it HAS the tech bonus, so the score is not
				// proportional to how many we already own -- the existing count plus a per-city allowance.
				iTempValue = iDelta * (getImprovementCount((ImprovementTypes)*it) + 2 * getNumCities()) * 35;
				iTempValue *= AI_yieldWeight((YieldTypes)iK);
				iTempValue /= 100;
				iTileImprovementValue += iTempValue;
			}
		}
	}

	iValue += iTileImprovementValue;

	


	int iBuildValue = 0;
	// WHICH BUILDS DOES THIS TECH UNLOCK? -- the tech's own `enables` edge names them (patterns.md
	// § THE WHAT-IF DRIVER), in place of asking every build in the database the reverse question.
	std::set<int> unlockedBuilds;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_BUILDS, unlockedBuilds);

	for (std::set<int>::const_iterator itUnlockedBuild = unlockedBuilds.begin(); itUnlockedBuild != unlockedBuilds.end(); ++itUnlockedBuild)
	{
		const BuildTypes eLoopBuild = static_cast<BuildTypes>(*itUnlockedBuild);
		ImprovementTypes eImprovement = GC.getBuildInfo(eLoopBuild).getImprovement();

		if (eImprovement != NO_IMPROVEMENT)
		{
			//	If it's an upgradable improvement
			eImprovement = kTeam.finalImprovementUpgrade(eImprovement);
		}
		else
		{
			// only increment build value if it is not an improvement, otherwise handle it there
			iBuildValue += 200;
		}

		if (eImprovement != NO_IMPROVEMENT)
		{
			const CvImprovementInfo& kImprovement = GC.getImprovementInfo(eImprovement);

			int iImprovementValue = 300;

			iImprovementValue += ((kImprovement.hasCharacteristic(CLS_CHARACTERISTIC_ACTS_AS_CITY)) ? 75 : 0);
			iImprovementValue += ((kImprovement.isHillsMakesValid()) ? 100 : 0);
			iImprovementValue += ((kImprovement.isFreshWaterMakesValid()) ? 200 : 0);
			iImprovementValue += ((kImprovement.isRiverSideMakesValid()) ? 100 : 0);
			iImprovementValue += ((kImprovement.isCarriesIrrigation()) ? 300 : 0);

			for (int iK = 0; iK < GC.getNumTerrainInfos(); iK++)
			{
				iImprovementValue += (kImprovement.getTerrainMakesValid(iK) ? 50 : 0);

				//Desert has negative defense // Toffer - Not very well defined then...
				if (GC.getTerrainInfo((TerrainTypes)iK).getDefense(DEFENSE_AMOUNT, CASC_SCOPE_PLOT) < 0 && kTeam.isCanFarmDesert())
				{
					iImprovementValue += 50;
				}
			}

			for (int iK = 0; iK < GC.getNumFeatureInfos(); iK++)
			{
				iImprovementValue += (kImprovement.getFeatureMakesValid(iK) ? 50 : 0);
			}

			for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
			{
				iTempValue = 0;

				// The improvement's own PLOT yield, ×100 at the slot so it reduces here. ⚠ The legacy riverSide
				// and irrigated variants are CONDITIONED entries now (HAS_RIVER / HAS_IRRIGATION,
				// [DEC-conditions-are-predicates]), not kinds, so they are deliberately NOT in this base: this
				// scores an improvement generically, with no plot to evaluate those against. It therefore
				// UNDERVALUES a river/irrigation improvement, which is the accepted direction -- the
				// "assume every condition holds" read is an undecided mode ([todo.md]) and is not invented here.
				iTempValue += (kImprovement.getFlatYield((YieldTypes)iK, CASC_SCOPE_PLOT) / 100 * 200);

				// land food yield is more valueble
				if (iK == YIELD_FOOD && !kImprovement.isWaterImprovement())
				{
					iTempValue *= 3;
					iTempValue /= 2;
				}

				if (bFinancialTrouble && iK == YIELD_COMMERCE)
				{
					iTempValue *= 2;
				}

				iTempValue *= AI_yieldWeight((YieldTypes)iK);
				iTempValue /= 100;

				iImprovementValue += iTempValue;
			}

			for (int iK = 0; iK < GC.getNumBonusInfos(); iK++)
			{
				int iBonusValue = 0;

				iBonusValue += (kImprovement.isImprovementBonusMakesValid(iK) ? 450 : 0);
				iBonusValue += (kImprovement.isImprovementBonusTrade(iK) ? 45 * AI_bonusVal((BonusTypes)iK) : 0);

				if (iBonusValue > 0)
				{
					for (int iL = 0; iL < NUM_YIELD_TYPES; iL++)
					{
						iTempValue = 0;

						// The bonus-keyed yield is the improvement's own entry for THAT bonus -- an entry-list
						// read over the handful it authored, never a walk of the bonus registry ([modifier.md] §5).
						iTempValue += (InfoValuation::keyedTarget(kImprovement.getModifiers(),
							infoYieldFamily((YieldTypes)iL), CHANNEL_AMOUNT,
							InfoValuation::keyedTargetSegment("bonuses"), iK) / 100 * 300);

						// food bonuses are more valuable
						if (iL == YIELD_FOOD)
						{
							iTempValue *= 2;
						}
						// otherwise, devalue the bonus slightly
						else if (iL == YIELD_COMMERCE && bFinancialTrouble)
						{
							iTempValue *= 4;
							iTempValue /= 3;
						}
						else
						{
							iTempValue *= 3;
							iTempValue /= 4;
						}

						if (bAdvancedStart && getCurrentEra() < 2)
						{
							iTempValue *= (iL == YIELD_FOOD) ? 3 : 2;
						}

						iTempValue *= AI_yieldWeight((YieldTypes)iL);
						iTempValue /= 100;

						iBonusValue += iTempValue;
					}

					const int iNumBonuses = countOwnedBonuses((BonusTypes)iK);

					if (iNumBonuses > 0)
					{
						iBonusValue *= (iNumBonuses + 2);

						//Fuyu: massive bonus for early worker logic
						int iCityRadiusBonusCount = 0;
						if (getNumCities() <= 3 && GC.getGame().getElapsedGameTurns() < 30 * CvGameSpeedScale::hammerCostPercent() / 100)
						{
							//count bonuses inside city radius or easily claimed
							foreach_(const CvCity * pLoopCity, cities())
							{
								int aiLoopCommerces[NUM_COMMERCE_TYPES];
								pLoopCity->getCommerces(aiLoopCommerces);
								iCityRadiusBonusCount += pLoopCity->AI_countNumBonuses((BonusTypes)iK, true, aiLoopCommerces[COMMERCE_CULTURE] / 100 > 0, -1);
							}
						}
						if (iCityRadiusBonusCount > 1)
						{
							iTempValue *= 3 + iCityRadiusBonusCount - getNumCities();
						}
						iBonusValue /= kImprovement.isWaterImprovement() ? 4 : 3; // water resources are worthless

						iImprovementValue += iBonusValue;
					}
				}
			}

			// if water improvement, weight by coastal cities (weight the whole build)
			if (kImprovement.isWaterImprovement())
			{
				iImprovementValue *= iCoastalCities;
				iImprovementValue /= std::max(1, iCityCount / 2);
			}

			iBuildValue += iImprovementValue;
		}

		const RouteTypes eRoute = (RouteTypes)GC.getBuildInfo(eLoopBuild).getRoute();

		if (eRoute != NO_ROUTE)
		{
			iBuildValue += ((getBestRoute() == NO_ROUTE) ? 700 : 200) * (getNumCities() + (bAdvancedStart ? 4 : 0));

			for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
			{
				iTempValue = GC.getRouteInfo(eRoute).getFlatYield((YieldTypes)iK, CASC_SCOPE_PLOT) / 100 * 100;

				for (int iL = 0; iL < GC.getNumImprovementInfos(); iL++)
				{
					iTempValue += GC.getRouteInfo(eRoute).getImprovementYield((ImprovementTypes)iL, (YieldTypes)(iK)) * 50;
				}
				iTempValue *= AI_yieldWeight((YieldTypes)iK);
				iTempValue /= 100;

				iBuildValue += iTempValue;
			}
		}
	}

	//the way feature-remove is done in XML is pretty weird
	//I believe this code needs to be outside the general BuildTypes loop
	//to ensure the feature-remove is only counted once rather than once per build
	//which could be a lot since nearly every build clears jungle...

	//TB Note: I'm thinking buildinfo feature tech is NOT the right call here at all?
	// The builds that reference this tech are named by the tech's own RELATED edge -- the load pass lands a
	// build there for each feature-clearing tech it carries. RELATED is a candidate SUPERSET, so the exact
	// per-feature predicate stays; hoisting it out of the feature loop computes it once instead of re-sweeping
	// every build in the database per feature.
	std::set<int> techRelatedBuilds;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_RELATED, EDGEB_BUILDS, techRelatedBuilds);

	for (int iJ = 0; iJ < GC.getNumFeatureInfos(); iJ++)
	{
		bool bIsFeatureRemove = false;
		for (std::set<int>::const_iterator itRelatedBuild = techRelatedBuilds.begin(); itRelatedBuild != techRelatedBuilds.end(); ++itRelatedBuild)
		{
			if (GC.getBuildInfo(static_cast<BuildTypes>(*itRelatedBuild)).getFeatureTech((FeatureTypes)iJ) == eTech)
			{
				bIsFeatureRemove = true;
				break;
			}
		}

		if (bIsFeatureRemove)
		{
			iBuildValue += 100;

			//Fuyu - Tech Value for Feature Remove - bonus for early worker logic
			if ((GC.getFeatureInfo(FeatureTypes(iJ)).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT) < 0) ||
				((GC.getFeatureInfo(FeatureTypes(iJ)).getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) + GC.getFeatureInfo(FeatureTypes(iJ)).getFlatYield(YIELD_PRODUCTION, CASC_SCOPE_PLOT) + GC.getFeatureInfo(FeatureTypes(iJ)).getFlatYield(YIELD_COMMERCE, CASC_SCOPE_PLOT)) < 0))
			{
				if (getNumCities() <= 3)
				{
					iBuildValue += 25 * countCityFeatures((FeatureTypes)iJ) * (3 - getNumCities() / 2);
				}
				else
				{
					iBuildValue += 25 * countCityFeatures((FeatureTypes)iJ);
				}
			}
			else if (getNumCities() <= 3)
			{
				iBuildValue += 5 * countCityFeatures((FeatureTypes)iJ) * (3 - getNumCities() / 2);
			}
			else
			{
				iBuildValue += 5 * countCityFeatures((FeatureTypes)iJ);
			}
		}
	}
	iValue += iBuildValue;

	

	// does tech reveal bonus resources
	int iBonusRevealValue = 0;

	// WHICH BONUSES DOES THIS TECH REVEAL? -- the tech's `enables` edge is the same source the load pass reads
	// to stamp each bonus's reveal tech (patterns.md § THE WHAT-IF DRIVER). That stamp is FIRST-WINS, so the
	// edge is a SUPERSET of the bonuses whose reveal tech is actually this one -- the predicate below stays.
	std::set<int> enabledBonuses;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_BONUSES, enabledBonuses);

	for (std::set<int>::const_iterator itEnabledBonus = enabledBonuses.begin(); itEnabledBonus != enabledBonuses.end(); ++itEnabledBonus)
	{
		const BonusTypes eLoopBonus = static_cast<BonusTypes>(*itEnabledBonus);

		if (GC.getBonusInfo(eLoopBonus).getTechReveal() == eTech)
		{
			int iRevealValue = 150;
			iRevealValue += (AI_bonusVal(eLoopBonus) * 50);

			BonusClassTypes eBonusClass = (BonusClassTypes)GC.getBonusInfo(eLoopBonus).getBonusClassType();
			int iBonusClassTotal = (m_bonusClassRevealed[eBonusClass] + m_bonusClassUnrevealed[eBonusClass]);

			//iMultiplier is basically a desperation value
			//it gets larger as the AI runs out of options
			//Copper after failing to get horses is +66%
			//Iron after failing to get horse or copper is +200%
			//but with either copper or horse, Iron is only +25%
			int iMultiplier = 0;
			if (iBonusClassTotal > 0)
			{
				iMultiplier = (m_bonusClassRevealed[eBonusClass] - m_bonusClassHave[eBonusClass]);
				iMultiplier *= 100;
				iMultiplier /= iBonusClassTotal;

				iMultiplier *= (m_bonusClassRevealed[eBonusClass] + 1);
				iMultiplier /= ((m_bonusClassHave[eBonusClass] * iBonusClassTotal) + 1);
			}

			iMultiplier *= std::min(3, getNumCities());
			iMultiplier /= 3;

			iRevealValue *= 100 + iMultiplier;
			iRevealValue /= 100;

			// K-Mod
			// If we don't yet have the 'enable' tech, reduce the value of the reveal.
			if (GC.getBonusInfo(eLoopBonus).getTechCityTrade() != eTech && !kTeam.isHasTech((TechTypes)(GC.getBonusInfo(eLoopBonus).getTechCityTrade())))
				iRevealValue /= 3;
			// K-Mod end

			iBonusRevealValue += iRevealValue;
		}
		// K-Mod: Value for enabling resources that are already revealed
		else if (GC.getBonusInfo(eLoopBonus).getTechCityTrade() == eTech && kTeam.isHasTech((TechTypes)(GC.getBonusInfo(eLoopBonus).getTechReveal())))
		{
			int iOwned = countOwnedBonuses(eLoopBonus);
			if (iOwned > 0)
			{
				int iEnableValue = 150;
				iEnableValue += (AI_bonusVal(eLoopBonus) * 50);
				iEnableValue *= (iOwned > 1) ? 150 : 100;
				iEnableValue /= 100;

				iValue += iEnableValue;
			}
		}
		// K-Mod end
	}

	

	iValue += iBonusRevealValue;

	/* ------------------ Unit Value  ------------------ */
	bool bEnablesUnitWonder;
	int iUnitValue = AI_techUnitValue(eTech, iPathLength, bEnablesUnitWonder);

	

	iValue += iUnitValue;

	if (bEnablesUnitWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(400, "AI Research Wonder Unit ASYNC") : GC.getGame().getSorenRandNum(400, "AI Research Wonder Unit"));
		iValue += iWonderRandom + (bCapitalAlone ? 200 : 0);

		iRandomMax += 400;
	}


	/* ------------------ Building Value  ------------------ */
	bool bEnablesWonder;
	int iBuildingValue = AI_techBuildingValue(eTech, iPathLength, bEnablesWonder);

	// ⛔ ENABLEMENT DECAYS WITH DISTANCE. It previously did not: iPathLength reached AI_techBuildingValue and was
	// read by exactly one narrow guard inside it, so a building unlocked FIVE techs away was worth precisely as
	// much as one unlocked next turn -- which is how the AI came to beeline deep for a single unlock. What you can
	// build now is worth more than what you could build after four more techs, and the value is divided by the
	// distance to say so.
	iValue += iBuildingValue / std::max(1, iPathLength);


	// if it gives at least one wonder
	if (bEnablesWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(800, "AI Research Wonder Building ASYNC") : GC.getGame().getSorenRandNum(800, "AI Research Wonder Building"));
		iValue += (500 + iWonderRandom) / ((bAdvancedStart ? 5 : 1) * std::max(1, iPathLength));

		iRandomMax += 800;
	}

	/* ------------------ Project Value  ------------------ */
	bool bEnablesProjectWonder = false;
	std::set<int> unlockedProjects;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_PROJECTS, unlockedProjects);

	for (std::set<int>::const_iterator itUnlockedProject = unlockedProjects.begin(); itUnlockedProject != unlockedProjects.end(); ++itUnlockedProject)
	{
		const ProjectTypes eLoopProject = static_cast<ProjectTypes>(*itUnlockedProject);

		iValue += 1000;

		if (NO_VICTORY /* project victory membership: unauthored, see todo */ != NO_VICTORY)
		{
			if (!GC.getProjectInfo(eLoopProject).isSpaceship())
			{
				// Apollo
				iValue += (AI_isDoVictoryStrategy(AI_VICTORY_SPACE2) ? 2000 : 100);
			}
			// Space ship parts
			else if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
			{
				iValue += 1000;
			}
		}

		if (iPathLength <= 1 && getTotalPopulation() > 5 && isWorldProject(eLoopProject)
		&& !GC.getGame().isProjectMaxedOut(eLoopProject))
		{
			bEnablesProjectWonder = true;

			if (bCapitalAlone)
			{
				iValue += 100;
			}
		}
	}
	if (bEnablesProjectWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(200, "AI Research Wonder Project ASYNC") : GC.getGame().getSorenRandNum(200, "AI Research Wonder Project"));
		iValue += iWonderRandom;

		iRandomMax += 200;
	}


	/* ------------------ Process Value  ------------------ */
	bool bIsGoodProcess = false;
	std::set<int> unlockedProcesses;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_PROCESSES, unlockedProcesses);

	for (std::set<int>::const_iterator itUnlockedProcess = unlockedProcesses.begin(); itUnlockedProcess != unlockedProcesses.end(); ++itUnlockedProcess)
	{
		const ProcessTypes eLoopProcess = static_cast<ProcessTypes>(*itUnlockedProcess);

		iValue += 100;

		for (int iK = 0; iK < NUM_COMMERCE_TYPES; iK++)
		{
			iTempValue = (GC.getProcessInfo(eLoopProcess).getProductionToCommerce((CommerceTypes)iK, CASC_SCOPE_CITY) * 4);

			iTempValue *= AI_commerceWeight((CommerceTypes)iK);
			iTempValue /= 100;

			if (iK == COMMERCE_GOLD || iK == COMMERCE_RESEARCH)
			{
				bIsGoodProcess = true;
			}
			else if ((iK == COMMERCE_CULTURE) && AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
			{
				iTempValue *= 3;
			}
			iValue += iTempValue;
		}
	}

	if (bIsGoodProcess && bFinancialTrouble)
	{
		// The maintained frontier replaces the scan + its hand-rederived tech gate ([enabler.md] §6).
		bool bHaveGoodProcess = false;
		std::vector<int> availableProcesses;
		getAvailableProcesses(availableProcesses);

		for (std::vector<int>::const_iterator itProcess = availableProcesses.begin();
			itProcess != availableProcesses.end(); ++itProcess)
		{
			const CvProcessInfo& kProcess = GC.getProcessInfo((ProcessTypes)*itProcess);

			if ((kProcess.getProductionToCommerce(COMMERCE_GOLD, CASC_SCOPE_CITY)
				+ kProcess.getProductionToCommerce(COMMERCE_RESEARCH, CASC_SCOPE_CITY)) > 0)
			{
				bHaveGoodProcess = true;
				break;
			}
		}
		if (!bHaveGoodProcess)
		{
			iValue += 1500;
		}
	}

	/* ------------------ Civic Value  ------------------ */
	std::set<int> unlockedCivics;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_CIVICS, unlockedCivics);

	for (std::set<int>::const_iterator itUnlockedCivic = unlockedCivics.begin(); itUnlockedCivic != unlockedCivics.end(); ++itUnlockedCivic)
	{
		const CivicTypes eLoopCivic = static_cast<CivicTypes>(*itUnlockedCivic);

		iValue += 200;

		const CivicTypes eCivic = getCivics((CivicOptionTypes)(GC.getCivicInfo(eLoopCivic).getCivicOption()));
		if (NO_CIVIC != eCivic)
		{
			const int iCurrentCivicValue = AI_civicValue(eCivic);
			const int iNewCivicValue = AI_civicValue(eLoopCivic);
			int iTechCivicValue = 0;

			if (iNewCivicValue > iCurrentCivicValue)
			{
				//	Because civic values can be negative theer is no absolute scale so we cannot meaningfully scale
				//	this relative to the current value.  Aslo 2400 is not enough to matter for some critical changes
				//iValue += std::min(2400, (2400 * (iNewCivicValue - iCurrentCivicValue)) / std::max(1, iCurrentCivicValue));
				iTechCivicValue = std::min(50000, 50 * (iNewCivicValue - iCurrentCivicValue));
				iValue += iTechCivicValue;
			}

			if (eCivic == GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic())
			{
				iValue += 600;
			}

			
		}
	}

	if (iPathLength <= 10)
	{
		if (GC.getGame().countKnownTechNumTeams(eTech) == 0)
		{
			int iReligionValue = 0;
			int iPotentialReligions = 0;
			for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
			{
				const TechTypes eReligionTech = GC.getReligionInfo((ReligionTypes)iJ).getTechPrereq();

				if (kTeam.isHasTech(eReligionTech) && !GC.getGame().isReligionSlotTaken((ReligionTypes)iJ) && canFoundReligion())
				{
					iPotentialReligions++;
				}
				if (eReligionTech == eTech)
				{
					if (!GC.getGame().isReligionSlotTaken((ReligionTypes)iJ))
					{
						int iRoll = 10000;
						if (!GC.getGame().isOption(GAMEOPTION_RELIGION_PICK) && canFoundReligion())
						{
							const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
							if (eFavorite != NO_RELIGION)
							{
								if (iJ == eFavorite)
								{
									iReligionValue += 1 + ((bAsync) ? GC.getASyncRand().get(1200, "AI Research Religion (Favorite) ASYNC") : GC.getGame().getSorenRandNum(1200, "AI Research Religion (Favorite)"));
								}
								else
								{
									iRoll *= 2;
									iRoll /= 3;
								}
							}
						}
						iReligionValue += 1 + ((bAsync) ? GC.getASyncRand().get(iRoll, "AI Research Religion ASYNC") : GC.getGame().getSorenRandNum(iRoll, "AI Research Religion"));

						if (iPathLength < 2)
						{
							iReligionValue *= 3;
							iReligionValue /= 2;
						}
					}
				}
			}

			if (iReligionValue > 0)
			{
				if (countHolyCities() < 1)
				{
					iReligionValue *= 2;
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
				{
					iReligionValue += 1000;
				}
				else
				{
					iReligionValue /= (1 + countHolyCities() + ((iPotentialReligions > 0) ? 1 : 0));
				}

				if ((countTotalHasReligion() == 0) && (iPotentialReligions == 0))
				{
					iReligionValue *= 2;
					iReligionValue += 500;
				}

				if (AI_isDoStrategy(AI_STRATEGY_DAGGER))
				{
					iReligionValue /= 2;
				}

				iReligionValue = (5 * iReligionValue / (iPathLength + 4));

				

				iValue += iReligionValue;
			}

			int iCorporationValue = 0;
			std::set<int> unlockedCorporations;
			EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_CORPORATIONS, unlockedCorporations);

			for (std::set<int>::const_iterator itUnlockedCorporation = unlockedCorporations.begin(); itUnlockedCorporation != unlockedCorporations.end(); ++itUnlockedCorporation)
			{
				const CorporationTypes eLoopCorporation = static_cast<CorporationTypes>(*itUnlockedCorporation);

				if (!(GC.getGame().isCorporationFounded(eLoopCorporation)))
				{
					iCorporationValue += 100 + ((bAsync) ? GC.getASyncRand().get(2400, "AI Research Corporation ASYNC") : GC.getGame().getSorenRandNum(2400, "AI Research Corporation"));
				}
			}

			if (iCorporationValue > 0)
			{
				iCorporationValue = (5 * iCorporationValue / (iPathLength + 4));

				

				iValue += iCorporationValue;
			}

			int iFreeStuffValue = 0;

			// The first-discoverer provisions the grants machine hands over: `grants` compiles onto the trigger
			// plane as the CONSIDERED-ACTION entry ([triggers.md]).
			const CvGrants* pTechGrants = kTech.getTriggers() ? kTech.getTriggers()->consideredGrant() : NULL;
			const int iFirstFreeTechs = pTechGrants ? pTechGrants->pulse("freeTechs") : 0;

			if (pTechGrants && pTechGrants->firstListId("firstFreeUnit") != -1)
			{
				int iGreatPeopleRandom = ((bAsync) ? GC.getASyncRand().get(3200, "AI Research Great People ASYNC") : GC.getGame().getSorenRandNum(3200, "AI Research Great People"));
				iFreeStuffValue += iGreatPeopleRandom;

				iRandomMax += 3200;

				if (bCapitalAlone)
				{
					iFreeStuffValue += 400;
				}

				iFreeStuffValue += 200;
			}

			//	Free techs are REALLY valuable - as an estimate we assume they are at least as valuable as the enabling tech
			//	since we'll be able to choose anything accessible once it is researched and up to twice as much as that on a
			//	random scale
			if (iFirstFreeTechs > 0)
			{
				int	iPercentageMultiplier = iFirstFreeTechs * ((bCapitalAlone ? 150 : 100) + (bAsync ? GC.getASyncRand().get(100, "AI Research Free Tech ASYNC") : GC.getGame().getSorenRandNum(100, "AI Research Free Tech")));

				iFreeStuffValue += (iValue * iPercentageMultiplier) / 100;
			}

			if (iFreeStuffValue > 0)
			{
				iFreeStuffValue = (5 * iFreeStuffValue / (iPathLength + 4));

				

				iValue += iFreeStuffValue;
			}
		}
	}

	iValue += kTech.getAIWeight();

	if (!isHumanPlayer() && iValue > 0)
	{
		for (int iJ = 0; iJ < GC.getNumFlavorTypes(); iJ++)
		{
			const int iFlavorContribution = AI_getFlavorValue((FlavorTypes)iJ) * kTech.getFlavorValue(iJ) * 20;
			iValue += iFlavorContribution;
			if (iFlavorContribution != 0)
			{
				logDecisionAI(3, "[DAI/tech/cand] player=%d tech=%S flavor=%s contrib=%d running=%d",
					getID(), kTech.getDescription(), GC.getFlavorTypes((FlavorTypes)iJ).c_str(),
					iFlavorContribution, iValue);
			}
		}
	}

	if (kTech.isRepeat())
	{
		iValue /= 10;
	}

	if (!bIgnoreCost)
	{
		iValue *= (1 + (getResearchTurnsLeft((eTech), false)));
		iValue /= 10;
	}

	//Tech Whore
	if (!GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING) && (kTech.providesCanTrade(CLS_CANTRADE_TECHS) || kTeam.isTechTrading()))
	{
		if ((bAsync ? GC.getASyncRand().get(100, "AI Tech Whore ASYNC") : GC.getGame().getSorenRandNum(100, "AI Tech Whore")) < (GC.getGame().isOption(GAMEOPTION_NO_TECH_BROKERING) ? 20 : 10))
		{
			int iKnownCount = 0;
			int iPossibleKnownCount = 0;

			for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
			{
				if (GET_TEAM((TeamTypes)iTeam).isAlive())
				{
					if (kTeam.isHasMet((TeamTypes)iTeam) && GET_TEAM((TeamTypes)iTeam).isHasTech(eTech))
					{
						iKnownCount++;
					}
					iPossibleKnownCount++;
				}
			}

			if (iKnownCount == 0 && iPossibleKnownCount > 2)
			{
				// Trade value
				iValue *= 100 + std::min(150, 25 * (iPossibleKnownCount - 2));
				iValue /= 100;
			}
		}
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		int iCVValue = AI_cultureVictoryTechValue(eTech);
		iValue *= (iCVValue + 10);
		iValue /= ((iCVValue < 100) ? 400 : 100);
	}

	int iRandomFactor = ((bAsync) ? GC.getASyncRand().get(20, "AI Research ASYNC") : GC.getGame().getSorenRandNum(20, "AI Research"));
	iValue += (iValue * (iRandomFactor - 10)) / 100;

	iValue = std::max(1, iValue);

	return iValue;
}

int CvPlayerAI::AI_techBuildingValue(TechTypes eTech, int iPathLength, bool& bEnablesWonder)
{
	PROFILE_FUNC();

	
	int iValue = 0;
	int iExistingCultureBuildingCount = 0;

	bEnablesWonder = false;

	// WHAT DOES THIS TECH ENABLE? -- asked FORWARD, off the tech's own load-compiled `enables` edge family, which
	// IS the answer (patterns.md § THE WHAT-IF DRIVER: the fundamental enabler-tree read is a pure list fetch).
	// Membership in the set IS the unlock, so nothing re-tests it per candidate and no database sweep runs.
	std::set<int> unlockedBuildings;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_BUILDINGS, unlockedBuildings);

	// The two worlds this tech's value is the DELTA between, built once for the whole sweep. A city-less
	// valuation evaluates against the CAPITAL ([patterns.md] THE VALUATION PROTOCOL); a player without one
	// has no valuation to give, so those legs are skipped rather than guessed at.
	CvCascadeHypothetical kTechWith;
	kTechWith.present[EDGEB_TECHS].insert((int)eTech);
	CvCascadeHypothetical kTechWithout;
	kTechWithout.absent[EDGEB_TECHS].insert((int)eTech);
	const CvCity* pTechCapital = getCapitalCity();

	for (std::set<int>::const_iterator itUnlocked = unlockedBuildings.begin(); itUnlocked != unlockedBuildings.end(); ++itUnlocked)
	{
		const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(*itUnlocked);

		if (EnablerKernel::everAvailable(EDGEB_BUILDINGS, (int)eLoopBuilding))
		{
			const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
			{   // membership IS the edge -- being in this set is exactly "the tech unlocks it"

				if (isWorldWonder(eLoopBuilding))
				{
					if (!GC.getGame().isBuildingMaxedOut(eLoopBuilding))
					{
						bEnablesWonder = true;
					}
				}

				//	Now recalculate the new way
				int iBuildingValue = 0;
				int iEconomyFlags =
					(
						BUILDINGFOCUS_PRODUCTION |
						BUILDINGFOCUS_FOOD |
						BUILDINGFOCUS_GOLD |
						BUILDINGFOCUS_RESEARCH |
						BUILDINGFOCUS_MAINTENANCE |
						BUILDINGFOCUS_HAPPY |
						BUILDINGFOCUS_HEALTHY |
						BUILDINGFOCUS_CULTURE |
						BUILDINGFOCUS_SPECIALIST |
						BUILDINGFOCUS_DEFENSE |
						BUILDINGFOCUS_EXPERIENCE |
						BUILDINGFOCUS_MAINTENANCE |
						BUILDINGFOCUS_WORLDWONDER |
						BUILDINGFOCUS_DOMAINSEA |
						BUILDINGFOCUS_ESPIONAGE
					);
				CvCity* pRepresentativeCity = NULL;
				int	iBestValue = 0;
				int iTotalValue = 0;
				int iTotalWeight = 0;
				int iRepresentativeBuildingValueInCity = -1;
				bool bCanConstructCityFound = false;

				foreach_(CvCity * pLoopCity, cities())
				{
					int iWillGetProbability;

					if (pRepresentativeCity == NULL)
					{
						pRepresentativeCity = pLoopCity;
					}

					// The tech drives MEMBERSHIP via `enables`, never the gate (enabler.md §2: tech is authored in
					// `enables`, never as a generation driver in `requires`) -- so once the edge above says the
					// tech unlocks it, the only per-city question left is whether the GATE holds there, and that
					// needs no tech hypothetical at all.
					// The weight is the enabler's own distinction rather than a hand-tuned ladder: satisfied HERE,
					// versus satisfiable here once a greyable clause is met (a connectable resource, an unadopted
					// civic -- enabler.md §6).
					iWillGetProbability =
						EnablerKernel::requiresMetInCity(*pLoopCity, EDGEB_BUILDINGS, (int)eLoopBuilding) ? 100
						: (EnablerKernel::requiresMetInCity(*pLoopCity, EDGEB_BUILDINGS, (int)eLoopBuilding, /*bVisible*/ true) ? 50 : 0);

					if (iWillGetProbability == 100)
					{

						if (!bCanConstructCityFound)
						{
							bCanConstructCityFound = true;
							pRepresentativeCity = pLoopCity;
						}
					}

					iTotalWeight += iWillGetProbability;
				}

				if (iTotalWeight > 0)
				{
					//	2/3rds factor applied here since the representative city we are using is likely to be large (early id)
					//	and so over-represent a bit
					iRepresentativeBuildingValueInCity = (2 * BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER * pRepresentativeCity->AI_buildingValue(eLoopBuilding, iEconomyFlags, true)) / 3;
				}

				iBestValue = iRepresentativeBuildingValueInCity;
				iTotalValue = (iRepresentativeBuildingValueInCity * iTotalWeight) / 100;

				if (isWorldWonder(eLoopBuilding))
				{
					if (!GC.getGame().isBuildingMaxedOut(eLoopBuilding))
					{
						iBuildingValue += (3 * iBestValue) / 2;	//	Opportunity cost for denying others
					}
				}
				else if (isTeamWonder(eLoopBuilding) || isNationalWonder(eLoopBuilding))
				{
					iBuildingValue += iBestValue;
				}
				else iBuildingValue += iTotalValue;

				//	Average value per city
				iBuildingValue /= std::max(1, getNumCities());

				

				iValue += iBuildingValue;
			}
		}
	}

	// WHAT THIS TECH IMPROVES, as distinct from what it UNLOCKS -- two different questions off two different
	// edges. A tech-gated deposit is a CONDITIONED entry on the building, so the buildings a tech makes better
	// are exactly those whose compiled entries reference it: the tech's own RELATED family
	// ([DEC-one-reverse-view]), landed once at load. The loop above walks what the tech ENABLES and can never
	// reach these, which is why this is its own pass rather than a branch inside it.
	std::set<int> improvedBuildings;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_RELATED, EDGEB_BUILDINGS, improvedBuildings);

	for (std::set<int>::const_iterator itImproved = improvedBuildings.begin(); itImproved != improvedBuildings.end(); ++itImproved)
	{
		const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(*itImproved);

		if (unlockedBuildings.count((int)eLoopBuilding) != 0
		|| getBuildingAvailabilityAnywhere(eLoopBuilding) != EnablerDomain::STATE_LISTED)
		{
			continue;   // scored above as an unlock, or not offered at all
		}
		const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
		// "Is this a culture building?" -- one read over its own entries, which covers the flat, the
		// per-population scaler and every scope at once.
		if (!isLimitedWonder(eLoopBuilding)
		&& InfoValuation::authorsAnySigned(kLoopBuilding.getModifiers(), infoCommerceFamily(COMMERCE_CULTURE), +1))
		{
			iExistingCultureBuildingCount++;
		}

		const int iNumExisting = algo::count_if(cities(), bind(&CvCity::hasBuilding, _1, eLoopBuilding));

		if (iNumExisting > 0)
		{
			int iTempValue = 0;

			// WHAT MY EXISTING BUILDINGS GAIN FROM THIS TECH -- the DELTA between holding it and not, asked of
			// the ONE valuation ([patterns.md] THE VALUATION PROTOCOL). A tech-gated building deposit is an
			// ordinary CONDITIONED entry (`enabled: TECH_X`), so each hypothetical resolves it and nothing here
			// re-derives which entries the tech gates -- which is what the per-channel keyed tables did by hand.
			if (pTechCapital != NULL)
			{
				const CityContext& kCityCtx = pTechCapital->getCityContext();
				const EmpireContext& kEmpireCtx = getEmpireContext();
				const CvPlotGroup* pCapitalGroup = pTechCapital->plotGroup(getID());
				int aiWith[NUM_YIELD_TYPES];
				int aiWithout[NUM_YIELD_TYPES];
				int aiCWith[NUM_COMMERCE_TYPES];
				int aiCWithout[NUM_COMMERCE_TYPES];

				// The flats, ×100 -> whole.
				kLoopBuilding.expectedFlatCommerce(kCityCtx, kEmpireCtx, pCapitalGroup, aiCWith, &kTechWith);
				kLoopBuilding.expectedFlatCommerce(kCityCtx, kEmpireCtx, pCapitalGroup, aiCWithout, &kTechWithout);
				for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
				{
					iTempValue += 4 * (aiCWith[iJ] - aiCWithout[iJ]) / 100;
				}
				kLoopBuilding.expectedFlatYields(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kTechWith);
				kLoopBuilding.expectedFlatYields(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kTechWithout);
				for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
				{
					iTempValue += 4 * (aiWith[iJ] - aiWithout[iJ]) / 100;
				}

				// The percents, applied against what the empire actually produces. A percent is NOT scaled.
				int aiEmpireCommerces[NUM_COMMERCE_TYPES];
				getCommerces(aiEmpireCommerces);
				for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
				{
					const int iDelta =
						kLoopBuilding.expectedModifier(infoCommerceFamily((CommerceTypes)iJ), CHANNEL_AMOUNT,
							CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kTechWith)
						- kLoopBuilding.expectedModifier(infoCommerceFamily((CommerceTypes)iJ), CHANNEL_AMOUNT,
							CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kTechWithout);
					if (iDelta != 0)
					{
						iTempValue += 4 * aiEmpireCommerces[iJ] / 100 * iDelta / getNumCities();
					}
				}
				kLoopBuilding.expectedYieldModifiers(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kTechWith);
				kLoopBuilding.expectedYieldModifiers(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kTechWithout);
				for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
				{
					const int iYieldModifier = aiWith[iJ] - aiWithout[iJ];
					if (iYieldModifier != 0)
					{
						iTempValue += 4 * calculateTotalYield((YieldTypes)iJ) * iYieldModifier / getNumCities();
					}
				}

				// The manual-assign specialist SLOTS this tech would open on this building -- the same
				// with/without DELTA as every other term above, through the KEYED twin because
				// `allowedSpecialists.city.{SPECIALIST_X}` is keyed AND tech-conditioned. The hypothetical
				// reaches only the conditioned tail, so what comes back is exactly the slots the TECH adds
				// ([patterns.md] THE VALUATION PROTOCOL -- contexts in, delta out).
				// ⚠ THE READ EDGE: the deposit is ×100, so the delta reduces HERE. ⛔ The `/ 100` further below is
				// the BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER's own and does NOT serve this.
				for (int iJ = 0; iJ < GC.getNumSpecialistInfos(); iJ++)
				{
					const int iSpecialistChange =
						(InfoValuation::expectedKeyedTarget(kLoopBuilding.getModifiers(), MODFAM_ALLOWED_SPECIALISTS,
							CHANNEL_AMOUNT, -1, iJ, kCityCtx, kEmpireCtx, pCapitalGroup, &kTechWith)
						- InfoValuation::expectedKeyedTarget(kLoopBuilding.getModifiers(), MODFAM_ALLOWED_SPECIALISTS,
							CHANNEL_AMOUNT, -1, iJ, kCityCtx, kEmpireCtx, pCapitalGroup, &kTechWithout)) / 100;

					if (iSpecialistChange != 0)
					{
						iTempValue += 800 * iSpecialistChange;
					}
				}
			}

			if (iTempValue != 0)
			{
				iTempValue = iTempValue * BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER / 100;

				
				iValue += iTempValue;
			}
		}
	}
	return iValue;
}

// abstraction of the loop to check if civilization can already train a settler, and set a boolean if it can
bool CvPlayerAI::AI_canTrainSettler() {
	PROFILE_EXTRA_FUNC();
	if (!m_canTrainSettler) {
		// the trainable-anywhere UNION, not a per-id fan over the whole database (types x cities). The verdict is
		// a plain boolean with a break, so the union's ascending order is immaterial to the answer.
		std::vector<int> vecTrainable;
		getTrainableAnywhere(vecTrainable);
		for (std::vector<int>::const_iterator it = vecTrainable.begin(), itEnd = vecTrainable.end(); it != itEnd; ++it)
		{
			if (GC.getUnitInfo((UnitTypes)*it).getDefaultUnitAI() == UNITAI_SETTLE)
			{
				m_canTrainSettler = true;
				break;
			}
		}
	}
	return m_canTrainSettler;
}


int CvPlayerAI::AI_techUnitValue(TechTypes eTech, int iPathLength, bool& bEnablesUnitWonder)
{
	PROFILE_EXTRA_FUNC();
	const bool bWarPlan =
		(
			GET_TEAM(getTeam()).hasWarPlan(true)
			|| // or aggressive personality
			GET_TEAM(getTeam()).AI_getTotalWarOdds() > 400
		);
	const bool bCapitalAlone = (GC.getGame().getElapsedGameTurns() > 0) ? AI_isCapitalAreaAlone() : false;
	const int iNumCities = getNumCities();
	const int iHasMetCount = GET_TEAM(getTeam()).getHasMetCivCount(true);
	const int iCoastalCities = countNumCoastalCities();
	const CvCity* pCapitalCity = getCapitalCity();

	bEnablesUnitWonder = false;
	int iValue = 0;
	// WHAT DOES THIS TECH ENABLE? -- the tech's own load-compiled `enables` edge IS the answer (patterns.md
	// § THE WHAT-IF DRIVER: a pure list fetch), in place of a sweep of every unit in the database per tech
	// valued. The accumulation is a commutative sum with no cross-iteration best-tracking, so the set's
	// ascending order is equivalent to the old descending walk.
	std::set<int> unlockedUnits;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_UNITS, unlockedUnits);

	for (std::set<int>::const_iterator itUnlocked = unlockedUnits.begin(); itUnlocked != unlockedUnits.end(); ++itUnlocked)
	{
		const UnitTypes eUnitX = static_cast<UnitTypes>(*itUnlocked);

		if (EnablerKernel::everAvailable(EDGEB_UNITS, (int)eUnitX))
		{
			const CvUnitInfo& unitX = GC.getUnitInfo(eUnitX);
			iValue += 200;

			// Same as the building loop above: this loop IS eTech's `enables.units`, so the old
			// "is eTech this unit's prereq" test was the driver's own guarantee restated. Kept as a scope
			// block so the accumulation below reads unchanged.
			{
				int iUnitValue = 0;
				int iNavalValue = 0;
				int iMilitaryValue = 0;

				// BBAI TODO: Change this to evaluating all unitai types defined in XML for unit?
				// Without this change many unit types are hard to evaluate, like offensive value of rifles
				// or defensive value of collateral siege
				switch (unitX.getDefaultUnitAI())
				{
				case UNITAI_UNKNOWN:
				case UNITAI_ANIMAL:
				case UNITAI_SUBDUED_ANIMAL:
				case UNITAI_BARB_CRIMINAL:
					break;
				case UNITAI_HUNTER:
				case UNITAI_HUNTER_ESCORT:
				{
					iUnitValue += 200;
					break;
				}
				case UNITAI_SETTLE:
				{
					// FIRST settler unit has a much higher weighting
					iUnitValue += 10000;
					if (AI_canTrainSettler()) {
						iUnitValue -= 9000;
					}
					break;
				}
				case UNITAI_WORKER:
					iUnitValue += 800;
					break;

				case UNITAI_HEALER:
					iUnitValue += 200;
					iMilitaryValue += 200;

				case UNITAI_HEALER_SEA:
					iUnitValue += 200;
					iMilitaryValue += 200;

				case UNITAI_PROPERTY_CONTROL:
					iUnitValue += 400;
					iMilitaryValue += 50;

				case UNITAI_PROPERTY_CONTROL_SEA:
					iUnitValue += 350;
					iMilitaryValue += 50;

				case UNITAI_INVESTIGATOR:
					iUnitValue += 400;

				case UNITAI_INFILTRATOR:
					iUnitValue += 200;
					iMilitaryValue += 75;

				case UNITAI_ESCORT:
					iUnitValue += 100;
					iMilitaryValue += 400;

				case UNITAI_SEE_INVISIBLE:
					iUnitValue += 400;
					iMilitaryValue += 100;

				case UNITAI_ATTACK:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 800 : 0);
					iUnitValue += 100;
					break;

				case UNITAI_ATTACK_CITY:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 800 : 0);
					if (unitX.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) > 0)
					{
						iMilitaryValue += 200;

						if (AI_calculateTotalBombard(DOMAIN_LAND) == 0)
						{
							iMilitaryValue += 800;
							if (AI_isDoStrategy(AI_STRATEGY_DAGGER))
							{
								iMilitaryValue += 1000;
							}
						}
					}
					iUnitValue += 100;
					break;

				case UNITAI_COLLATERAL:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					break;

				case UNITAI_PILLAGE:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_PILLAGE_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 300 : 200);
					break;

				case UNITAI_RESERVE:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 600 : 0);
					break;

				case UNITAI_PARADROP:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					break;

				case UNITAI_CITY_DEFENSE:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					iMilitaryValue += ((!bCapitalAlone) ? 400 : 200);
					iUnitValue += ((iHasMetCount > 0) ? 800 : 200);
					break;

				case UNITAI_CITY_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					break;

				case UNITAI_CITY_SPECIAL:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					break;

				case UNITAI_EXPLORE:
					iUnitValue += ((bCapitalAlone) ? 100 : 200);
					break;

				case UNITAI_MISSIONARY:
					iUnitValue += ((getStateReligion() != NO_RELIGION) ? 600 : 300);
					break;

				case UNITAI_PROPHET:
				case UNITAI_ARTIST:
				case UNITAI_SCIENTIST:
				case UNITAI_GENERAL:
				case UNITAI_GREAT_HUNTER:
				case UNITAI_GREAT_ADMIRAL:
				case UNITAI_MERCHANT:
				case UNITAI_ENGINEER:
				case UNITAI_WORKER_SEA:
					break;

				case UNITAI_SPY:
					iMilitaryValue += ((bWarPlan) ? 100 : 50);
					break;

				case UNITAI_ICBM:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_SEE_INVISIBLE_SEA:
					// BBAI TODO: Boost value for maps where Barb ships are pestering us
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 400;
					}
					iNavalValue += 100;
					break;

				case UNITAI_ATTACK_SEA:
					// BBAI TODO: Boost value for maps where Barb ships are pestering us
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 200 : 100);
					}
					iNavalValue += 100;
					break;

				case UNITAI_RESERVE_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					iNavalValue += 100;
					break;

				case UNITAI_ESCORT_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					iNavalValue += 100;
					break;

				case UNITAI_EXPLORE_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += ((bCapitalAlone) ? 1800 : 600);
					}
					break;

				case UNITAI_ASSAULT_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan || bCapitalAlone) ? 400 : 200);
					}
					iNavalValue += 200;
					break;

				case UNITAI_SETTLER_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += ((bWarPlan || bCapitalAlone) ? 100 : 200);
					}
					iNavalValue += 200;
					break;

				case UNITAI_MISSIONARY_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += 100;
					}
					break;

				case UNITAI_SPY_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 100;
					}
					break;

				case UNITAI_CARRIER_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					break;

				case UNITAI_MISSILE_CARRIER_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					break;

				case UNITAI_PIRATE_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 100;
					}
					iNavalValue += 100;
					break;

				case UNITAI_ATTACK_AIR:
					iMilitaryValue += ((bWarPlan) ? 1200 : 800);
					break;

				case UNITAI_DEFENSE_AIR:
					iMilitaryValue += ((bWarPlan) ? 1200 : 800);
					break;

				case UNITAI_CARRIER_AIR:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 200 : 100);
					}
					iNavalValue += 400;
					break;

				case UNITAI_MISSILE_AIR:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				default:
					FErrorMsg("error");
					break;
				}

				if (AI_isDoStrategy(AI_STRATEGY_ALERT1))
				{
					if (unitX.hasUnitAI(UNITAI_COLLATERAL))
					{
						iUnitValue += 500;
					}
					if (unitX.hasUnitAI(UNITAI_CITY_DEFENSE))
					{
						iUnitValue += 10 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (AI_isDoStrategy(AI_STRATEGY_TURTLE) && iPathLength <= 1)
				{
					if (unitX.hasUnitAI(UNITAI_COLLATERAL))
					{
						iUnitValue += 1000;
					}
					if (unitX.hasUnitAI(UNITAI_CITY_DEFENSE))
					{
						iUnitValue += 20 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
				{
					if (unitX.hasUnitAI(UNITAI_ATTACK_CITY))
					{
						iUnitValue += 15 * GC.getGame().AI_combatValue(eUnitX);
					}
				}
				else if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST1))
				{
					if (unitX.hasUnitAI(UNITAI_ATTACK_CITY))
					{
						iUnitValue += 5 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (unitX.hasUnitAI(UNITAI_ASSAULT_SEA) && iCoastalCities > 0)
				{
					int iAssaultValue = 0;
					UnitTypes eExistingUnit = NO_UNIT;
					if (AI_bestAreaUnitAIValue(UNITAI_ASSAULT_SEA, NULL, &eExistingUnit) == 0)
					{
						iAssaultValue += 250;
					}
					else if (eExistingUnit != NO_UNIT)
					{
						iAssaultValue += 1000 * std::max(0, AI_unitImpassableCount(eUnitX) - AI_unitImpassableCount(eExistingUnit));

						const int iOld = (GC.getUnitInfo(eExistingUnit).getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * GC.getUnitInfo(eExistingUnit).getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100;

						iAssaultValue += 800 * ((unitX.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * unitX.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 - iOld) / std::max(1, iOld);
					}

					if (iAssaultValue > 0)
					{
						foreach_(CvArea * areaX, GC.getMap().areas())
						{
							if (AI_isPrimaryArea(areaX) && areaX->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
							{
								iAssaultValue *= 4; break;
							}
						}
						iUnitValue += iAssaultValue;
					}
				}

				if (iNavalValue > 0 && pCapitalCity != NULL)
				{
					iUnitValue += iNavalValue * 2 * (1 + iNumCities - pCapitalCity->area()->getCitiesPerPlayer(getID())) / iNumCities;
				}

				if (AI_totalUnitAIs(unitX.getDefaultUnitAI()) == 0)
				{
					// do not give bonus to seagoing units if they are worthless
					if (iUnitValue > 0)
					{
						iUnitValue *= 2;
					}

					if (pCapitalCity != NULL && unitX.getDefaultUnitAI() == UNITAI_EXPLORE)
					{
						iUnitValue += AI_neededExplorers(pCapitalCity->area()) * 400;
					}

					if (unitX.getDefaultUnitAI() == UNITAI_EXPLORE_SEA)
					{
						iUnitValue += 400;
						iUnitValue += ((GC.getGame().countCivTeamsAlive() - iHasMetCount) * 200);
					}
				}

				if (pCapitalCity != NULL && unitX.hasUnitAI(UNITAI_SETTLER_SEA))
				{
					UnitTypes eExistingUnit = NO_UNIT;
					int iBestAreaValue = 0;
					AI_getNumAreaCitySites(pCapitalCity->getArea(), iBestAreaValue);

					//Early Expansion by sea
					if (AI_bestAreaUnitAIValue(UNITAI_SETTLER_SEA, NULL, &eExistingUnit) == 0)
					{
						CvArea* pWaterArea = pCapitalCity->waterArea();
						if (pWaterArea != NULL)
						{
							if (iBestAreaValue == 0)
							{
								iUnitValue += 2000;
							}
							else
							{
								int iBestOtherValue = 0;
								AI_getNumAdjacentAreaCitySites(pWaterArea->getID(), pCapitalCity->getArea(), iBestOtherValue);

								if (iBestAreaValue < iBestOtherValue)
								{
									iUnitValue += 1000;
								}
								else if (iBestOtherValue > 0)
								{
									iUnitValue += 500;
								}
							}
						}
					}
					// Landlocked expansion over ocean
					else if (eExistingUnit != NO_UNIT
					&& AI_unitImpassableCount(eUnitX) < AI_unitImpassableCount(eExistingUnit)
					&& iBestAreaValue < AI_getMinFoundValue())
					{
						iUnitValue += (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION2) ? 2000 : 500);
					}
				}

				if (iMilitaryValue > 0)
				{
					if (iHasMetCount == 0)
					{
						iMilitaryValue /= 2;
					}
					if (bCapitalAlone)
					{
						iMilitaryValue *= 2;
						iMilitaryValue /= 3;
					}
					// K-Mod
					if (AI_isDoStrategy(AI_STRATEGY_GET_BETTER_UNITS))
					{
						iMilitaryValue *= 3;
						iMilitaryValue /= 2;
					}
					iUnitValue += iMilitaryValue;
				}

				if (iPathLength <= 1 && getTotalPopulation() > 5 && isWorldUnit(eUnitX) && !GC.getGame().isUnitMaxedOut(eUnitX))
				{
					bEnablesUnitWonder = true;
				}
				iValue += iUnitValue;
			}
		}
	}
	return iValue;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

void CvPlayerAI::AI_chooseFreeTech()
{
	clearResearchQueue();

	const TechTypes eBestTech = AI_bestTech(1, true);

	if (eBestTech != NO_TECH)
	{
		GET_TEAM(getTeam()).setHasTech(eBestTech, true, getID(), true, true);
	}
}

void CvPlayerAI::AI_startGoldenAge()
{
	// Golden age start - reconsider civics at the first opportunity
	AI_setCivicTimer(0);
}

void CvPlayerAI::AI_chooseResearch()
{
	PROFILE_EXTRA_FUNC();
	FAssert(!isNPC())
		clearResearchQueue();

	if (getCurrentResearch() == NO_TECH && !isNPC())
	{
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (iI != getID() && GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam())
			&& GET_PLAYER((PlayerTypes)iI).getCurrentResearch() != NO_TECH
			&& (getTechAvailability(GET_PLAYER((PlayerTypes)iI).getCurrentResearch()) == EnablerDomain::STATE_LISTED))
			{
				pushResearch(GET_PLAYER((PlayerTypes)iI).getCurrentResearch());
			}
		}
	}

	if (getCurrentResearch() == NO_TECH)
	{
		// HOW FAR AHEAD THIS LEADER COMMITS is the leader's own knob (enabler.md par.8: the research search depth
		// is a personality variable, not a constant) -- and it is the dial that governs BEELINING, since the
		// depth is exactly how many hops past the researchable frontier a single distant unlock can pull the AI
		// (AGENTS.md, AI valuation of ENABLEMENT).
		// ⚠ The two 1s are deliberate OVERRIDES of that knob rather than depths in their own right: a human's
		// picker offers the immediate best rather than a plan, and a culture-victory AI is already committed and
		// does not look ahead. Neither becomes personality-driven by this change.
		const int iSearchDepth = GC.getLeaderHeadInfo(getPersonalityType()).getResearchSearchDepth();
		const TechTypes eBestTech = AI_bestTech(
			isHumanPlayer() || AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) ? 1 : iSearchDepth);

		if (eBestTech != NO_TECH)
		{
			CvTechInfo& tech = GC.getTechInfo(eBestTech);

			OutputDebugString(CvString::format("Game turn %d, AI chooses tech %S\n", GC.getGame().getGameTurn(), tech.getDescription()).c_str());
			pushResearch(eBestTech);
		}
	}
}


DiploCommentTypes CvPlayerAI::AI_getGreeting(PlayerTypes ePlayer) const
{
	TeamTypes eWorstEnemy;

	if (GET_PLAYER(ePlayer).getTeam() != getTeam())
	{
		eWorstEnemy = GET_TEAM(getTeam()).AI_getWorstEnemy();

		if ((eWorstEnemy != NO_TEAM) && (eWorstEnemy != GET_PLAYER(ePlayer).getTeam()) && GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isHasMet(eWorstEnemy) && (GC.getASyncRand().get(4) == 0))
		{
			if (GET_PLAYER(ePlayer).AI_hasTradedWithTeam(eWorstEnemy) && !atWar(GET_PLAYER(ePlayer).getTeam(), eWorstEnemy))
			{
				return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_WORST_ENEMY_TRADING");
			}
			else
			{
				return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_WORST_ENEMY");
			}
		}
		else if ((getNumNukeUnits() > 0) && (GC.getASyncRand().get(4) == 0))
		{
			return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_NUKES");
		}
		else if ((GET_PLAYER(ePlayer).getPower() < getPower()) && AI_getAttitude(ePlayer) < ATTITUDE_PLEASED && (GC.getASyncRand().get(4) == 0))
		{
			return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_UNIT_BRAG");
		}
	}

	return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GREETINGS");
}


bool CvPlayerAI::AI_isWillingToTalk(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssertMsg(getPersonalityType() != NO_LEADER, "getPersonalityType() is not expected to be equal with NO_LEADER");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (GET_PLAYER(ePlayer).getTeam() == getTeam()
		|| GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam())
		|| GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return true;
	}

	if (GET_TEAM(getTeam()).isHuman())
	{
		return false;
	}
	bool bRuthlessAI = GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS);
	if (bRuthlessAI)
	{
		if (AI_getMemoryCount(ePlayer, MEMORY_BACKSTAB) > 0)
		{
			return false;
		}
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		int iRefuseDuration = (GC.getLeaderHeadInfo(getPersonalityType()).getRefuseToTalkWarThreshold() * ((GET_TEAM(getTeam()).AI_isChosenWar(GET_PLAYER(ePlayer).getTeam())) ? 2 : 1));

		int iOurSuccess = 1 + GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER(ePlayer).getTeam());
		int iTheirSuccess = 1 + GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWarSuccess(getTeam());
		if (iTheirSuccess > iOurSuccess * 2)
		{
			iRefuseDuration *= 20 + ((80 * iOurSuccess * 2) / iTheirSuccess);
			iRefuseDuration /= 100;
		}

		if (GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER(ePlayer).getTeam()) < iRefuseDuration)
		{
			return false;
		}

		if (GET_TEAM(getTeam()).isAVassal())
		{
			return false;
		}
		/************************************************************************************************/
		/* Afforess					  Start		 07/12/10											   */
		/*																							  */
		/*																							  */
		/************************************************************************************************/
		if (GET_PLAYER(ePlayer).getNumCities() == 0)
		{
			return false;
		}

		if (bRuthlessAI)
		{
			if (!AI_isFinancialTrouble())
			{
				if (iOurSuccess * 2 > iTheirSuccess * 3)
				{
					return false;
				}
			}
		}
		/************************************************************************************************/
		/* Afforess						 END															*/
		/************************************************************************************************/
	}
	else
	{
		if (AI_getMemoryCount(ePlayer, MEMORY_STOPPED_TRADING_RECENT) > 0)
		{
			return false;
		}
	}

	return true;
}


// XXX what if already at war???
// Returns true if the AI wants to sneak attack...
bool CvPlayerAI::AI_demandRebukedSneak(PlayerTypes ePlayer) const
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	FAssert(!(GET_TEAM(getTeam()).isAVassal()));
	FAssert(!(GET_TEAM(getTeam()).isHuman()));

	if (GC.getGame().getSorenRandNum(100, "AI Demand Rebuked") < GC.getLeaderHeadInfo(getPersonalityType()).getDemandRebukedSneakProb())
	{
		if (GET_TEAM(getTeam()).getPower(true) > GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePower(getTeam()))
		{
			return true;
		}
	}

	return false;
}


// XXX what if already at war???
// Returns true if the AI wants to declare war...
bool CvPlayerAI::AI_demandRebukedWar(PlayerTypes ePlayer) const
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	FAssert(!(GET_TEAM(getTeam()).isAVassal()));
	FAssert(!(GET_TEAM(getTeam()).isHuman()));

	// needs to be async because it only happens on the computer of the player who is in diplomacy...
	if (GC.getASyncRand().get(100, "AI Demand Rebuked ASYNC") < GC.getLeaderHeadInfo(getPersonalityType()).getDemandRebukedWarProb())
	{
		if (GET_TEAM(getTeam()).getPower(true) > GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePower())
		{
			if (GET_TEAM(getTeam()).AI_isAllyLandTarget(GET_PLAYER(ePlayer).getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}


// XXX maybe make this a little looser (by time...)
bool CvPlayerAI::AI_hasTradedWithTeam(TeamTypes eTeam) const
{
	PROFILE_EXTRA_FUNC();
	int iI;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == eTeam)
			{
				if ((AI_getPeacetimeGrantValue((PlayerTypes)iI) + AI_getPeacetimeTradeValue((PlayerTypes)iI)) > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// static // Toffer - Should perhaps be in CvGameCoreUtil?
AttitudeTypes CvPlayerAI::AI_getAttitudeFromValue(int iAttitudeVal)
{
	if (iAttitudeVal > 9)
	{
		return ATTITUDE_FRIENDLY;
	}
	if (iAttitudeVal > 2)
	{
		return ATTITUDE_PLEASED;
	}
	if (iAttitudeVal > -3)
	{
		return ATTITUDE_CAUTIOUS;
	}
	if (iAttitudeVal > -9)
	{
		return ATTITUDE_ANNOYED;
	}
	return ATTITUDE_FURIOUS;
}

AttitudeTypes CvPlayerAI::AI_getAttitude(PlayerTypes ePlayer, bool bForced) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (GET_PLAYER(ePlayer).isAlive())
	{
		return AI_getAttitudeFromValue(AI_getAttitudeVal(ePlayer, bForced));
	}
	FErrorMsg("ePlayer should ideally be alive");
	return NO_ATTITUDE;
}


int CvPlayerAI::AI_getAttitudeVal(PlayerTypes ePlayer, bool bForced) const
{
	PROFILE_FUNC();

	//	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	//AI Autoplay calls this
	if (isNPC() || GET_PLAYER(ePlayer).isNPC())
	{
		return -100;
	}
	if (bForced
	&& (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) && !GET_TEAM(getTeam()).isCapitulated()))
	{
		return 100;
	}

	if (m_aiAttitudeCache[ePlayer] != MAX_INT)
	{
		return m_aiAttitudeCache[ePlayer];
	}

	int iAttitude = GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttitude();

	iAttitude += GC.getHandicapInfo(GET_PLAYER(ePlayer).getHandicapType()).getDiplomacy(DIPLOMACY_ATTITUDE, CASC_SCOPE_EMPIRE, false) / 100;

	int aiDiplomacy[NUM_DIPLOMACY_KINDS];
	GET_PLAYER(ePlayer).getDiplomacyKinds(aiDiplomacy);
	iAttitude += aiDiplomacy[DIPLOMACY_ATTITUDE] / 100;

	if (!GET_PLAYER(ePlayer).isHumanPlayer())
	{
		iAttitude += (4 - abs(AI_getPeaceWeight() - GET_PLAYER(ePlayer).AI_getPeaceWeight()));
		iAttitude += std::min(GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect(), GC.getLeaderHeadInfo(GET_PLAYER(ePlayer).getPersonalityType()).getWarmongerRespect());
	}

	iAttitude -= std::max(0, (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getNumMembers() - GET_TEAM(getTeam()).getNumMembers()));

	const int iRankDifference = (GC.getGame().getPlayerRank(getID()) - GC.getGame().getPlayerRank(ePlayer));

	if (iRankDifference > 0)
	{
		iAttitude += ((GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_WORSE_RANK_DIFFERENCE) * iRankDifference) / (GC.getGame().countCivPlayersEverAlive() + 1));
	}
	else
	{
		iAttitude += ((GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_BETTER_RANK_DIFFERENCE) * -(iRankDifference)) / (GC.getGame().countCivPlayersEverAlive() + 1));
	}

	if ((GC.getGame().getPlayerRank(getID()) >= (GC.getGame().countCivPlayersEverAlive() / 2)) &&
		  (GC.getGame().getPlayerRank(ePlayer) >= (GC.getGame().countCivPlayersEverAlive() / 2)))
	{
		iAttitude++;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWarSuccess(getTeam()) > GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_LOST_WAR);
	}

	iAttitude += AI_getTraitAttitude(ePlayer);
	iAttitude += AI_getCloseBordersAttitude(ePlayer);
	iAttitude += AI_getWarAttitude(ePlayer);
	iAttitude += AI_getPeaceAttitude(ePlayer);
	iAttitude += AI_getSameReligionAttitude(ePlayer);
	iAttitude += AI_getDifferentReligionAttitude(ePlayer);
	iAttitude += AI_getBonusTradeAttitude(ePlayer);
	iAttitude += AI_getOpenBordersAttitude(ePlayer);
	iAttitude += AI_getDefensivePactAttitude(ePlayer);
	iAttitude += AI_getRivalDefensivePactAttitude(ePlayer);
	iAttitude += AI_getRivalVassalAttitude(ePlayer);
	iAttitude += AI_getShareWarAttitude(ePlayer);
	iAttitude += AI_getFavoriteCivicAttitude(ePlayer);
	iAttitude += AI_getTradeAttitude(ePlayer);
	iAttitude += AI_getRivalTradeAttitude(ePlayer);
	iAttitude += AI_getCivicShareAttitude(ePlayer);
	iAttitude += AI_getEmbassyAttitude(ePlayer);
	iAttitude += AI_getCivicAttitudeChange(ePlayer);

	for (int iI = 0; iI < NUM_MEMORY_TYPES; iI++)
	{
		iAttitude += AI_getMemoryAttitude(ePlayer, ((MemoryTypes)iI));
	}

	iAttitude += AI_getColonyAttitude(ePlayer);
	iAttitude += AI_getAttitudeExtra(ePlayer);

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isRebelAgainst(getTeam()))
	{
		iAttitude -= 5;
	}
	else if (GET_TEAM(getTeam()).isRebelAgainst(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude -= 3;
	}

	if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
	{
		if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWorstEnemy() == GET_TEAM(getTeam()).AI_getWorstEnemy())
		{
			iAttitude += 2;
		}
	}

	m_aiAttitudeCache[ePlayer] = range(iAttitude, -100, 100);

	return range(iAttitude, -100, 100);
}


// BEGIN: Show Hidden Attitude Mod 01/22/2009
bool isShowPersonalityModifiers()
{
	return true;
}

bool isShowSpoilerModifiers()
{
	return true;
}

int CvPlayerAI::AI_getFirstImpressionAttitude(PlayerTypes ePlayer) const
{
	bool bShowPersonalityAttitude = isShowPersonalityModifiers();
	CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
	int iAttitude = GC.getHandicapInfo(kPlayer.getHandicapType()).getDiplomacy(DIPLOMACY_ATTITUDE, CASC_SCOPE_EMPIRE, false) / 100;

	//ls612: If you Start as Minors the first impression is not important
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_START_AS_MINORS))
	{
		return 0;
	}

	if (bShowPersonalityAttitude)
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttitude();
		if (!kPlayer.isHumanPlayer())
		{
			if (isShowSpoilerModifiers())
			{
				// iBasePeaceWeight + iPeaceWeightRand
				iAttitude += (4 - abs(AI_getPeaceWeight() - kPlayer.AI_getPeaceWeight()));
			}
			else
			{
				// iBasePeaceWeight
				iAttitude += (4 - abs(GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() - GC.getLeaderHeadInfo(kPlayer.getPersonalityType()).getBasePeaceWeight()));
			}
			iAttitude += std::min(GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect(), GC.getLeaderHeadInfo(kPlayer.getPersonalityType()).getWarmongerRespect());
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getTeamSizeAttitude(PlayerTypes ePlayer) const
{
	return -std::max(0, (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getNumMembers() - GET_TEAM(getTeam()).getNumMembers()));
}


// Count only players visible on the active player's scoreboard
int CvPlayerAI::AI_getKnownPlayerRank(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	PlayerTypes eActivePlayer = GC.getGame().getActivePlayer();
	if (NO_PLAYER == eActivePlayer || GC.getGame().isDebugMode()) {
		// Use the full scoreboard
		return GC.getGame().getPlayerRank(ePlayer);
	}

	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
	int iRank = 0;
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		PlayerTypes eRankPlayer = GC.getGame().getRankPlayer(iI);
		if (eRankPlayer != NO_PLAYER)
		{
			CvTeam& kRankTeam = GET_TEAM(GET_PLAYER(eRankPlayer).getTeam());
			if (kRankTeam.isAlive() && (kRankTeam.isHasMet(eActiveTeam) || kRankTeam.isHuman()))
			{
				if (eRankPlayer == ePlayer) {
					return iRank;
				}
				iRank++;
			}
		}
	}

	// Should only get here if we tried to find the rank of an unknown player
	return iRank + 1;
}

int CvPlayerAI::AI_getTraitAttitude(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
	int aiDiplomacy[NUM_DIPLOMACY_KINDS];
	kPlayer.getDiplomacyKinds(aiDiplomacy);
	int iAttitude = aiDiplomacy[DIPLOMACY_ATTITUDE] / 100;

	if (iAttitude < 0)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			TraitTypes eTrait = ((TraitTypes)iI);
			if (GC.getTraitInfo(eTrait).getDiplomacy(DIPLOMACY_ATTITUDE, CASC_SCOPE_EMPIRE) < 0 && kPlayer.hasTrait(eTrait) && GET_PLAYER(getID()).hasTrait(eTrait))
			{
				iAttitude *= -1;
			}
		}
	}
	return iAttitude;
}

int CvPlayerAI::AI_getBetterRankDifferenceAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	int iRankDifference;
	if (isShowSpoilerModifiers())
	{
		iRankDifference = GC.getGame().getPlayerRank(ePlayer) - GC.getGame().getPlayerRank(getID());
	}
	else
	{
		iRankDifference = AI_getKnownPlayerRank(ePlayer) - AI_getKnownPlayerRank(getID());
	}

	if (iRankDifference > 0)
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_BETTER_RANK_DIFFERENCE) * iRankDifference / (GC.getGame().countCivPlayersEverAlive() + 1);
	}

	return 0;
}


int CvPlayerAI::AI_getWorseRankDifferenceAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	int iRankDifference;
	if (isShowSpoilerModifiers())
	{
		iRankDifference = GC.getGame().getPlayerRank(getID()) - GC.getGame().getPlayerRank(ePlayer);
	}
	else
	{
		iRankDifference = AI_getKnownPlayerRank(getID()) - AI_getKnownPlayerRank(ePlayer);
	}

	if (iRankDifference > 0)
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_WORSE_RANK_DIFFERENCE) * iRankDifference / (GC.getGame().countCivPlayersEverAlive() + 1);
	}

	return 0;
}


int CvPlayerAI::AI_getLowRankAttitude(PlayerTypes ePlayer) const
{
	int iThisPlayerRank;
	int iPlayerRank;
	if (isShowSpoilerModifiers())
	{
		iThisPlayerRank = GC.getGame().getPlayerRank(getID());
		iPlayerRank = GC.getGame().getPlayerRank(ePlayer);
	}
	else
	{
		iThisPlayerRank = AI_getKnownPlayerRank(getID());
		iPlayerRank = AI_getKnownPlayerRank(ePlayer);
	}

	int iMedianRank = GC.getGame().countCivPlayersEverAlive() / 2;
	return (iThisPlayerRank >= iMedianRank && iPlayerRank >= iMedianRank) ? 1 : 0;
}


int CvPlayerAI::AI_getLostWarAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
	if (!isShowSpoilerModifiers() && NO_PLAYER != GC.getGame().getActivePlayer())
	{
		// Hide war success for wars you are not involved in
		if (GC.getGame().getActiveTeam() != getTeam() && GC.getGame().getActiveTeam() != eTeam)
		{
			return 0;
		}
	}

	if (GET_TEAM(eTeam).AI_getWarSuccess(getTeam()) > GET_TEAM(getTeam()).AI_getWarSuccess(eTeam))
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_LOST_WAR);
	}

	return 0;
}
// END: Show Hidden Attitude Mod


int CvPlayerAI::AI_calculateStolenCityRadiusPlots(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssert(ePlayer != getID());

	int iCount = 0;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		const CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);

		if (pLoopPlot->getOwner() == ePlayer && pLoopPlot->isPlayerCityRadius(getID()))
		{
			iCount++;
		}
	}

	return iCount;
}


int CvPlayerAI::AI_getCloseBordersAttitude(PlayerTypes ePlayer) const
{
	if (m_aiCloseBordersAttitudeCache[ePlayer] == MAX_INT)
	{
		PROFILE_FUNC();
		int iPercent;

		if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
		{
			return 0;
		}

		iPercent = std::min(60, (AI_calculateStolenCityRadiusPlots(ePlayer) * 3));

		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					  06/12/10								jdog5000	  */
		/*																							  */
		/* Bugfix, Victory Strategy AI																  */
		/************************************************************************************************/
		if (GET_TEAM(getTeam()).AI_isLandTarget(GET_PLAYER(ePlayer).getTeam(), true))
		{
			iPercent += 40;
		}

		if (AI_isDoStrategy(AI_VICTORY_CONQUEST3))
		{
			iPercent = std::min(120, (3 * iPercent) / 2);
		}
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/

		m_aiCloseBordersAttitudeCache[ePlayer] = ((GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_CLOSE_BORDERS) * iPercent) / 100);
	}

	return m_aiCloseBordersAttitudeCache[ePlayer];
}


int CvPlayerAI::AI_getWarAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude -= 3;
	}

	if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_AT_WAR) != 0)
	{
		int iAttitudeChange = (GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_AT_WAR));
		iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_AT_WAR))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_AT_WAR)));
	}

	return iAttitude;
}


int CvPlayerAI::AI_getPeaceAttitude(PlayerTypes ePlayer) const
{
	if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_AT_PEACE) != 0)
	{
		int iAttitudeChange = (GET_TEAM(getTeam()).AI_getAtPeaceCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_AT_PEACE));
		return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_AT_PEACE))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_AT_PEACE)));
	}

	return 0;
}


int CvPlayerAI::AI_getSameReligionAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if ((getStateReligion() != NO_RELIGION) && (getStateReligion() == GET_PLAYER(ePlayer).getStateReligion()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_SAME_RELIGION);

		if (hasHolyCity(getStateReligion()))
		{
			iAttitude++;
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_SAME_RELIGION) != 0)
		{
			int iAttitudeChange = (AI_getSameReligionCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_SAME_RELIGION));
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_SAME_RELIGION))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_SAME_RELIGION)));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getDifferentReligionAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if ((getStateReligion() != NO_RELIGION) && (GET_PLAYER(ePlayer).getStateReligion() != NO_RELIGION) && (getStateReligion() != GET_PLAYER(ePlayer).getStateReligion()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_DIFFERENT_RELIGION);

		if (hasHolyCity(getStateReligion()))
		{
			iAttitude--;
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_DIFFERENT_RELIGION) != 0)
		{
			int iAttitudeChange = (AI_getDifferentReligionCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_DIFFERENT_RELIGION));
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_DIFFERENT_RELIGION))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_DIFFERENT_RELIGION)));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getBonusTradeAttitude(PlayerTypes ePlayer) const
{
	int iAttitudeChange;

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_BONUS_TRADE) != 0)
		{
			iAttitudeChange = (AI_getBonusTradeCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_BONUS_TRADE));
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_BONUS_TRADE))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_BONUS_TRADE)));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getOpenBordersAttitude(PlayerTypes ePlayer) const
{
	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_OPEN_BORDERS) != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getOpenBordersCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_OPEN_BORDERS));
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_OPEN_BORDERS))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_OPEN_BORDERS)));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getDefensivePactAttitude(PlayerTypes ePlayer) const
{
	if (getTeam() != GET_PLAYER(ePlayer).getTeam() && (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam())))
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_DEFENSIVE_PACT);
	}

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_DEFENSIVE_PACT) != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getDefensivePactCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_DEFENSIVE_PACT));
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_DEFENSIVE_PACT))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_DEFENSIVE_PACT)));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getRivalDefensivePactAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		return iAttitude;
	}

	if (!(GET_TEAM(getTeam()).isDefensivePact(GET_PLAYER(ePlayer).getTeam())))
	{
		iAttitude -= ((4 * GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePactCount(GET_PLAYER(ePlayer).getTeam())) / std::max(1, (GC.getGame().countCivTeamsAlive() - 2)));
	}

	return iAttitude;
}


int CvPlayerAI::AI_getRivalVassalAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		return iAttitude;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getVassalCount(getTeam()) > 0)
	{
		iAttitude -= (6 * GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getPower(true)) / std::max(1, GC.getGame().countTotalCivPower());
	}

	return iAttitude;
}


int CvPlayerAI::AI_getShareWarAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GET_TEAM(getTeam()).AI_shareWar(GET_PLAYER(ePlayer).getTeam()))
		{
			iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_SHARE_WAR);
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_SHARE_WAR) != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getShareWarCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_SHARE_WAR));
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_SHARE_WAR))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_SHARE_WAR)));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getFavoriteCivicAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() != NO_CIVIC)
	{
		if (isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()) && GET_PLAYER(ePlayer).isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()))
		{
			iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChange(DIPLO_RELATION_FAVORITE_CIVIC);

			if (GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_FAVORITE_CIVIC) != 0)
			{
				int iAttitudeChange = (AI_getFavoriteCivicCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeDivisor(DIPLO_RELATION_FAVORITE_CIVIC));
				iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_FAVORITE_CIVIC))), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAttitudeChangeLimit(DIPLO_RELATION_FAVORITE_CIVIC)));
			}
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getTradeAttitude(PlayerTypes ePlayer) const
{
	// XXX human only?
	return range(((AI_getPeacetimeGrantValue(ePlayer) + std::max(0, (AI_getPeacetimeTradeValue(ePlayer) - GET_PLAYER(ePlayer).AI_getPeacetimeTradeValue(getID())))) / ((GET_TEAM(getTeam()).AI_getHasMetCounter(GET_PLAYER(ePlayer).getTeam()) + 1) * 5)), 0, 4);
}


int CvPlayerAI::AI_getRivalTradeAttitude(PlayerTypes ePlayer) const
{
	// XXX human only?

	return -(range(((GET_TEAM(getTeam()).AI_getEnemyPeacetimeGrantValue(GET_PLAYER(ePlayer).getTeam()) + (GET_TEAM(getTeam()).AI_getEnemyPeacetimeTradeValue(GET_PLAYER(ePlayer).getTeam()) / 3)) / ((GET_TEAM(getTeam()).AI_getHasMetCounter(GET_PLAYER(ePlayer).getTeam()) + 1) * 10)), 0, 4));
}


int CvPlayerAI::AI_getMemoryAttitude(PlayerTypes ePlayer, MemoryTypes eMemory) const
{
	return ((AI_getMemoryCount(ePlayer, eMemory) * GC.getLeaderHeadInfo(getPersonalityType()).getMemoryAttitudePercent(eMemory)) / 100);
}

int CvPlayerAI::AI_getColonyAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getParent() == ePlayer)
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getFreedomAppreciation();
	}

	return iAttitude;
}



PlayerVoteTypes CvPlayerAI::AI_diploVote(const VoteSelectionSubData& kVoteData, VoteSourceTypes eVoteSource, bool bPropose)
{
	PROFILE_FUNC();

	const VoteTypes eVote = kVoteData.eVote;

	if (GC.getGame().isTeamVote(eVote))
	{
		if (GC.getGame().isTeamVoteEligible(getTeam(), eVoteSource))
		{
			return (PlayerVoteTypes)getTeam();
		}
		int iBestValue;

		if ((GC.getVoteInfo(eVote).getRole() == VOTE_ROLE_VICTORY))
		{
			iBestValue = 7;
		}
		else iBestValue = 0;

		PlayerVoteTypes eBestTeam = PLAYER_VOTE_ABSTAIN;

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if (GET_TEAM((TeamTypes)iI).isAlive()
			&& GC.getGame().isTeamVoteEligible((TeamTypes)iI, eVoteSource))
			{
				if (GET_TEAM(getTeam()).isVassal((TeamTypes)iI))
				{
					return (PlayerVoteTypes)iI;
				}

				const int iValue = GET_TEAM(getTeam()).AI_getAttitudeVal((TeamTypes)iI);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestTeam = (PlayerVoteTypes)iI;
				}
			}
		}
		return eBestTeam;
	}

	TeamTypes eSecretaryGeneral = GC.getGame().getSecretaryGeneral(eVoteSource);

	// Remove blanket auto approval for friendly secretary
	bool bFriendlyToSecretary = false;
	if (!bPropose && eSecretaryGeneral != NO_TEAM)
	{
		if (eSecretaryGeneral == getTeam())
		{
			return PLAYER_VOTE_YES;
		}
		bFriendlyToSecretary = (GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) == ATTITUDE_FRIENDLY);
	}

	bool bDefy = false;
	bool bValid = true;

	for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
	{
		const CivicTypes eCivic = (CivicTypes)iI;

		if (GC.getVoteInfo(eVote).forcesCivic(iI) && !isCivic(eCivic))
		{
			const CivicTypes eBestCivic = AI_bestCivic((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOption());

			if (eBestCivic != NO_CIVIC && eBestCivic != eCivic)
			{
				const int iBestCivicValue = AI_civicValue(eBestCivic);
				const int iNewCivicValue =
					(
						bFriendlyToSecretary
						?
						AI_civicValue(eCivic) * 6 / 5
						:
						AI_civicValue(eCivic)
					);

				if (iBestCivicValue > iNewCivicValue * 120 / 100)
				{
					bValid = false;

					// Increase odds of defiance, particularly on AggressiveAI
					if (iBestCivicValue > iNewCivicValue * (140 + GC.getGame().getSorenRandNum(GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 50 : 80, "AI Erratic Defiance (Force Civic)")) / 100)
					{
						bDefy = true;
					}
					break;
				}
			}
		}
	}

	if (bValid && GC.getVoteInfo(eVote).getTradeRoutes() > 0)
	{
		if (bFriendlyToSecretary)
		{
			return PLAYER_VOTE_YES;
		}

		if (getNumCities() > GC.getGame().getNumCities() * 2 / (1 + GC.getGame().countCivPlayersAlive()))
		{
			bValid = false;
		}
	}

	if (bValid && GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_NO_NUKES))
	{
		// The SDI term is gone with the team's nuke-interception accumulator (ruling-16 trigger-plane data --
		// see Assets/savemigration.txt). The threshold now rests on the leader's own personality alone.
		int iVoteBanThreshold = GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb();
		iVoteBanThreshold *= std::max(1, GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect());

		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iVoteBanThreshold *= 2;
		}

		// The "I alone hold SDI and have nukes" doubling went with the interception accumulator: with no
		// team-side stat there is no SDI monopoly to detect. It returns with the trigger-plane re-home.

		if (bFriendlyToSecretary)
		{
			iVoteBanThreshold *= 2;
			iVoteBanThreshold /= 3;
		}

		bValid = (GC.getGame().getSorenRandNum(100, "AI nuke ban vote") > iVoteBanThreshold);

		if (AI_isDoStrategy(AI_STRATEGY_OWABWNW))
		{
			bValid = false;
		}
		else if (
			GET_TEAM(getTeam()).getNumNukeUnits() / std::max(1, GET_TEAM(getTeam()).getNumMembers())
			<
			GC.getGame().countTotalNukeUnits() / std::max(1, GC.getGame().countCivPlayersAlive()))
		{
			bValid = false;
		}


		if (!bValid && AI_getNumTrainAIUnits(UNITAI_ICBM) > 0
		&& GC.getGame().getSorenRandNum(AI_isDoStrategy(AI_STRATEGY_OWABWNW) ? 2 : 3, "AI Erratic Defiance (No Nukes)") == 0)
		{
			bDefy = true;
		}
	}

	if (bValid && GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_FREE_TRADE))
	{
		if (bFriendlyToSecretary)
		{
			return PLAYER_VOTE_YES;
		}
		int iOpenCount = 0;
		int iClosedCount = 0;

		for (int iI = 0; iI < MAX_TEAMS; iI++)
		{
			if (GET_TEAM((TeamTypes)iI).isAlive() && iI != getTeam())
			{
				if (GET_TEAM(getTeam()).isOpenBorders((TeamTypes)iI))
				{
					iOpenCount += GET_TEAM((TeamTypes)iI).getNumCities();
				}
				else iClosedCount += GET_TEAM((TeamTypes)iI).getNumCities();
			}
		}

		if (iOpenCount >= getNumCities() * getTradeRoutes())
		{
			bValid = false;
		}

		if (iClosedCount == 0)
		{
			bValid = false;
		}
	}

	if (bValid)
	{
		if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_OPEN_BORDERS))
		{
			if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}
			bValid = true;

			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (iI != getTeam() && GET_TEAM((TeamTypes)iI).isVotingMember(eVoteSource)
				&& NO_DENIAL != GET_TEAM(getTeam()).AI_openBordersTrade((TeamTypes)iI))
				{
					bValid = false;
					break;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_DEFENSIVE_PACT))
		{
			if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}

			bValid = true;

			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (iI != getTeam() && GET_TEAM((TeamTypes)iI).isVotingMember(eVoteSource)
				&& NO_DENIAL != GET_TEAM(getTeam()).AI_defensivePactTrade((TeamTypes)iI))
				{
					bValid = false;
					break;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_FORCE_PEACE))
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes ePeaceTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			int iWarsWinning = 0;
			int iWarsLosing = 0;
			int iChosenWar = 0;

			bool bLosingBig = false;
			bool bWinningBig = false;
			bool bThisPlayerWinning = false;

			int iWinDeltaThreshold = 3 * GC.getWAR_SUCCESS_ATTACKING();
			int iLossAbsThreshold = std::max(3, getNumMilitaryUnits() / 40) * GC.getWAR_SUCCESS_ATTACKING();

			bool bAggressiveAI = GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE);
			if (bAggressiveAI)
			{
				iWinDeltaThreshold *= 2;
				iWinDeltaThreshold /= 3;

				iLossAbsThreshold *= 4;
				iLossAbsThreshold /= 3;
			}

			// Is ePeaceTeam winning wars?
			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (GET_TEAM((TeamTypes)iI).isAlive() && iI != ePeaceTeam
				&& GET_TEAM((TeamTypes)iI).isAtWar(ePeaceTeam))
				{
					const int iPeaceTeamSuccess = GET_TEAM(ePeaceTeam).AI_getWarSuccess((TeamTypes)iI);
					const int iOtherTeamSuccess = GET_TEAM((TeamTypes)iI).AI_getWarSuccess(ePeaceTeam);

					if (iPeaceTeamSuccess - iOtherTeamSuccess > iWinDeltaThreshold)
					{
						// Have to be ahead by at least a few victories to count as win
						++iWarsWinning;

						if (iPeaceTeamSuccess - iOtherTeamSuccess > 3 * iWinDeltaThreshold)
						{
							bWinningBig = true;
						}
					}
					else if (iOtherTeamSuccess >= iPeaceTeamSuccess)
					{
						if (iI == getTeam() && iOtherTeamSuccess - iPeaceTeamSuccess > iWinDeltaThreshold)
						{
							bThisPlayerWinning = true;
						}

						if (iOtherTeamSuccess > iLossAbsThreshold)
						{
							// Have to have non-trivial loses
							++iWarsLosing;

							if (iOtherTeamSuccess - iPeaceTeamSuccess > 3 * iLossAbsThreshold)
							{
								bLosingBig = true;
							}
						}
						else if (GET_TEAM(ePeaceTeam).AI_getAtWarCounter((TeamTypes)iI) < 10
						// Not winning, just recently attacked, and in multiple wars, be pessimistic
						// Counts ties from no actual battles)
						&& GET_TEAM(ePeaceTeam).getAtWarCount(true) > 1
						&& !GET_TEAM(ePeaceTeam).AI_isChosenWar((TeamTypes)iI))
						{
							++iWarsLosing;
						}
					}

					if (GET_TEAM(ePeaceTeam).AI_isChosenWar((TeamTypes)iI))
					{
						++iChosenWar;
					}
				}
			}

			if (ePeaceTeam == getTeam())
			{
				const int iPeaceRand =
					(
						GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight()
						/
						(
							(bAggressiveAI ? 2 : 1)
							*
							(AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST2) ? 2 : 1) // K-Mod
							)
					);

				// Always true for real war-mongers, rarely true for less aggressive types
				const bool bWarmongerRoll = 0 == GC.getGame().getSorenRandNum(iPeaceRand, "AI Erratic Defiance (Force Peace)");

				if (bLosingBig && (!bWarmongerRoll || bPropose))
				{
					// Non-warmongers want peace to escape loss
					bValid = true;
				}
				else if (!bLosingBig && (iChosenWar > iWarsLosing || AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))) // K-Mod
				{
					// If chosen to be in most wars, keep it going
					bValid = false;
				}
				else
				{
					// If losing most wars, vote for peace
					bValid = (iWarsLosing > iWarsWinning);
				}

				if (!bValid && !bLosingBig && bWinningBig && bWarmongerRoll && !AI_isFinancialTrouble())
				{
					bDefy = true;
				}
			}
			else if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (GET_TEAM(ePeaceTeam).isAtWar(getTeam()))
			{
				// Do we want to end this war?
				bValid =
					(
						GET_TEAM(getTeam()).AI_endWarVal(ePeaceTeam) > 3 * GET_TEAM(ePeaceTeam).AI_endWarVal(getTeam()) / 2
						&&
						(bWinningBig || iWarsWinning > iWarsLosing || GET_TEAM(getTeam()).getAtWarCount(true, true) > 1)
					);

				// Do we want to defy the peace resolution?
				if (!bValid && bThisPlayerWinning && iWarsLosing >= iWarsWinning && !bPropose
				&& !GET_TEAM(getTeam()).isAVassal()
				&& (GET_TEAM(getTeam()).getAtWarCount(true) == 1 || bLosingBig)
				// Can we continue this war with defiance penalties?
				&& !AI_isFinancialTrouble()
				&& (
					GC.getGame().getSorenRandNum
					(
						GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() / (bAggressiveAI ? 2 : 1),
						"AI Erratic Defiance (Force Peace)"
					) == 0
					)) bDefy = true;


				if (!bValid && !bDefy && !bPropose
				&& GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) > GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_VASSAL))
				{
					// Influence by secretary
					if (NO_DENIAL == GET_TEAM(getTeam()).AI_makePeaceTrade(ePeaceTeam, eSecretaryGeneral))
					{
						bValid = true;
					}
					else if (eSecretaryGeneral != NO_TEAM && GET_TEAM(getTeam()).isVassal(eSecretaryGeneral))
					{
						bValid = true;
					}
				}
			}
			else
			{
				if (GET_TEAM(getTeam()).AI_getWarPlan(ePeaceTeam) != NO_WARPLAN)
				{
					// Keep planned enemy occupied
					bValid = false;
				}
				else if (GET_TEAM(getTeam()).AI_shareWar(ePeaceTeam) && !GET_TEAM(getTeam()).isVassal(ePeaceTeam))
				{
					// Keep ePeaceTeam at war with our common enemies
					bValid = false;
				}
				else if (iWarsLosing > iWarsWinning)
				{
					// Feel pity for team that is losing (if like them enough to not declare war on them)
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(ePeaceTeam) >= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DECLARE_WAR_THEM));
				}
				else
				{
					// Stop a team that is winning (if don't like them enough to join them in war)
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(ePeaceTeam) < GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DECLARE_WAR));
				}

				if (!bValid && bFriendlyToSecretary && !GET_TEAM(getTeam()).isVassal(ePeaceTeam))
				{
					// Influence by secretary
					bValid = true;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_FORCE_NO_TRADE))
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes eEmbargoTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (eEmbargoTeam == getTeam())
			{
				bValid = false;
				if (!isNoForeignTrade())
				{
					bDefy = true;
				}
			}
			else if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}
			else if (canStopTradingWithTeam(eEmbargoTeam))
			{
				bValid = (NO_DENIAL == AI_stopTradingTrade(eEmbargoTeam, kVoteData.ePlayer));
				if (bValid)
				{
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(eEmbargoTeam) <= ATTITUDE_CAUTIOUS);
				}
			}
			else
			{
				bValid = (GET_TEAM(getTeam()).AI_getAttitude(eEmbargoTeam) < ATTITUDE_CAUTIOUS);
			}
		}
		else if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_FORCE_WAR))
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes eWarTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (eWarTeam == getTeam() || GET_TEAM(getTeam()).isVassal(eWarTeam))
			{
				// Explicit rejection by all who will definitely be attacked
				bValid = false;
			}
			else if (GET_TEAM(getTeam()).AI_getWarPlan(eWarTeam) != NO_WARPLAN)
			{
				bValid = true;
			}
			else
			{
				if (!bPropose && GET_TEAM(getTeam()).isAVassal())
				{
					// Vassals always deny war trade requests and thus previously always voted no
					bValid = false;

					if
					(
						!GET_TEAM(getTeam()).hasWarPlan(true)
						&&
						(
							eSecretaryGeneral == NO_TEAM
							||
							GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral)
							>
							GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DECLARE_WAR)
						)
					)
					{
						if (eSecretaryGeneral != NO_TEAM && GET_TEAM(getTeam()).isVassal(eSecretaryGeneral))
						{
							bValid = true;
						}
						else if
						(
							(
								GET_TEAM(getTeam()).isAVassal()
								?
								GET_TEAM(getTeam()).getCurrentMasterPower(true)
								:
								GET_TEAM(getTeam()).getPower(true)
							)
							> GET_TEAM(eWarTeam).getDefensivePower()
						)
						{
							bValid = true;
						}
					}
				}
				else
				{
					bValid = (bPropose || NO_DENIAL == GET_TEAM(getTeam()).AI_declareWarTrade(eWarTeam, eSecretaryGeneral));
				}

				if (bValid)
				{
					int iNoWarOdds = GC.getLeaderHeadInfo(getPersonalityType()).getNoWarAttitudeProb((GET_TEAM(getTeam()).AI_getAttitude(eWarTeam)));
					bValid = ((iNoWarOdds < 30) || (GC.getGame().getSorenRandNum(100, "AI War Vote Attitude Check (Force War)") > iNoWarOdds));
				}
				/*
				else
				{
					// Consider defying resolution
					if( !GET_TEAM(getTeam()).isAVassal() )
					{
						if( eSecretaryGeneral == NO_TEAM || GET_TEAM(getTeam()).AI_getAttitude(eWarTeam) > GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) )
						{
							if( GET_TEAM(getTeam()).AI_getAttitude(eWarTeam) > GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DEFENSIVE_PACT) )
							{
								int iDefyRand = GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight();
								iDefyRand /= (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 2 : 1);

								if (GC.getGame().getSorenRandNum(iDefyRand, "AI Erratic Defiance (Force War)") > 0)
								{
									bDefy = true;
								}
							}
						}
					}
				}
				*/
			}
		}
		else if (GC.getVoteInfo(eVote).effectApplies(VOTE_EFFECT_ASSIGN_CITY))
		{
			bValid = false;

			FAssert(kVoteData.ePlayer != NO_PLAYER);
			CvPlayer& kPlayer = GET_PLAYER(kVoteData.ePlayer);
			CvCity* pCity = kPlayer.getCity(kVoteData.iCityId);
			if (NULL != pCity && NO_PLAYER != kVoteData.eOtherPlayer && kVoteData.eOtherPlayer != pCity->getOwner())
			{
				if (!bPropose && eSecretaryGeneral == getTeam() || GET_PLAYER(kVoteData.eOtherPlayer).getTeam() == getTeam())
				{
					bValid = true;
				}
				else if (kPlayer.getTeam() == getTeam())
				{
					bValid = false;
					// BBAI TODO: Wonders, holy city, aggressive AI?
					if (GC.getGame().getSorenRandNum(3, "AI Erratic Defiance (Assign City)") == 0)
					{
						bDefy = true;
					}
				}
				else
				{
					bValid = (AI_getAttitude(kVoteData.ePlayer) < AI_getAttitude(kVoteData.eOtherPlayer));
				}
			}
		}
	}

	// Don't defy resolutions from friends
	if (bDefy && !bFriendlyToSecretary && canDefyResolution(eVoteSource, kVoteData))
	{
		return PLAYER_VOTE_NEVER;
	}
	return (bValid ? PLAYER_VOTE_YES : PLAYER_VOTE_NO);
}


// ---------------------------------------------------------------------------
// Diplomacy / trade-deal AI logging taxonomy ([DIP/*] -> DiploAI.log, gPlayerLogLevel).
// Mirrors the per-subsystem tagged-log family ([WAI]/[HAI]/[DAI]). Explains the AI's
// trade reasoning:
//   [DIP/cand]     (lvl 3) per trade-item value contribution within AI_dealVal
//   [DIP/dealval]  (lvl 2) total value the AI assigns to a trade list (AI_dealVal)
//   [DIP/begin]    (lvl 1) AI_considerOffer entry: who, give/get list sizes, iChange
//   [DIP/score]    (lvl 2) our-value vs their-value comparison in AI_considerOffer
//   [DIP/decision] (lvl 1) accept/reject verdict + the values/threshold behind it
// Item codes in [DIP/cand]/decision are the raw TradeableItems enum (see CvEnums.h).
// ---------------------------------------------------------------------------
int CvPlayerAI::AI_dealVal(PlayerTypes ePlayer, const CLinkList<TradeData>* pList, bool bIgnoreAnnual, int iChange) const
{
	PROFILE_EXTRA_FUNC();
	CLLNode<TradeData>* pNode;

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	int iValue = 0;

	const bool bAtWar = atWar(getTeam(), GET_PLAYER(ePlayer).getTeam());
	if (bAtWar)
	{
		iValue += GET_TEAM(getTeam()).AI_endWarVal(GET_PLAYER(ePlayer).getTeam());
	}

	for (pNode = pList->head(); pNode; pNode = pList->next(pNode))
	{
		FAssert(!pNode->m_data.m_bHidden);

		const int iItemBefore = iValue;
		switch (pNode->m_data.m_eItemType)
		{
			case TRADE_TECHNOLOGIES:
			{
				iValue += GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_RESOURCES:
			{
				if (!bIgnoreAnnual)
				{
					iValue += AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, iChange);

					// The partner player is also loosing value for it, which is good for us
					iValue += GET_PLAYER(ePlayer).AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), getID(), -iChange);
				}
				break;
			}
			case TRADE_CITIES:
			{
				CvCity* pCity = GET_PLAYER(ePlayer).getCity(pNode->m_data.m_iData);
				if (pCity != NULL)
				{
					iValue += AI_cityTradeVal(pCity);

					//	The partner player is also loosing value for it, which is good for us
					iValue += GET_PLAYER(ePlayer).AI_ourCityValue(pCity);
				}
				break;
			}
			case TRADE_GOLD:
			{
				iValue += AI_getGoldValue(pNode->m_data.m_iData, AI_goldTradeValuePercent());
				break;
			}
			case TRADE_GOLD_PER_TURN:
			{
				if (!bIgnoreAnnual)
				{
					iValue += AI_getGoldValue(pNode->m_data.m_iData * getTreatyLength(), AI_goldTradeValuePercent());
				}
				break;
			}
			case TRADE_MAPS:
			{
				iValue += GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_SURRENDER:
			{
				if (!bIgnoreAnnual)
				{
					iValue += GET_TEAM(getTeam()).AI_surrenderTradeVal(GET_PLAYER(ePlayer).getTeam());
				}
				break;
			}
			case TRADE_VASSAL:
			{
				if (!bIgnoreAnnual)
				{
					iValue += GET_TEAM(getTeam()).AI_vassalTradeVal(GET_PLAYER(ePlayer).getTeam());
				}
				break;
			}
			case TRADE_OPEN_BORDERS:
			{
				iValue += GET_TEAM(getTeam()).AI_openBordersTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_DEFENSIVE_PACT:
			{
				iValue += GET_TEAM(getTeam()).AI_defensivePactTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_PEACE:
			{
				iValue += GET_TEAM(getTeam()).AI_makePeaceTradeVal(((TeamTypes)(pNode->m_data.m_iData)), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_WAR:
			{
				iValue += GET_TEAM(getTeam()).AI_declareWarTradeVal(((TeamTypes)(pNode->m_data.m_iData)), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_EMBARGO:
			{
				iValue += AI_stopTradingTradeVal(((TeamTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_CIVIC:
			{
				iValue += AI_civicTradeVal(((CivicTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_RELIGION:
			{
				iValue += AI_religionTradeVal(((ReligionTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_EMBASSY:
			{
				iValue += GET_TEAM(getTeam()).AI_embassyTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_CONTACT:
			{
				iValue += GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_CORPORATION:
			{
				iValue += AI_corporationTradeVal((CorporationTypes)pNode->m_data.m_iData);

				//	Partner is losing it also
				iValue += GET_PLAYER(ePlayer).AI_corporationTradeVal((CorporationTypes)pNode->m_data.m_iData);
				break;
			}
			case TRADE_PLEDGE_VOTE:
			{
				iValue += AI_pledgeVoteTradeVal(GC.getGame().getVoteTriggered(GC.getGame().getCurrentVoteID()), ((PlayerVoteTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_SECRETARY_GENERAL_VOTE:
			{
				iValue += AI_secretaryGeneralTradeVal((VoteSourceTypes)(pNode->m_data.m_iData), ePlayer);
				break;
			}
			case TRADE_RITE_OF_PASSAGE:
			{
				iValue += GET_TEAM(getTeam()).AI_LimitedBordersTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_FREE_TRADE_ZONE:
			{
				iValue += GET_TEAM(getTeam()).AI_FreeTradeAgreementVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_WORKER:
			{
				const CvUnit* pUnit = GET_PLAYER(ePlayer).getUnit(pNode->m_data.m_iData);
				if (pUnit)
				{
					iValue += AI_workerTradeVal(pUnit);
				}
				break;
			}
			case TRADE_MILITARY_UNIT:
			{
				const CvUnit* pUnit = GET_PLAYER(ePlayer).getUnit(pNode->m_data.m_iData);
				if (pUnit)
				{
					iValue += AI_militaryUnitTradeVal(pUnit);
				}
				break;
			}
		}
		logDiploAI(3, "[DIP/cand] player=%d from=%d item=%d data=%d value=%d",
			getID(), (int)ePlayer, (int)pNode->m_data.m_eItemType, pNode->m_data.m_iData, iValue - iItemBefore);
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_CAND, 3)
			.addI(DIPF_player, (int)getID()).addI(DIPF_from, (int)ePlayer)
			.addI(DIPF_item, (int)pNode->m_data.m_eItemType).addI(DIPF_data, pNode->m_data.m_iData)
			.addI(DIPF_value, iValue - iItemBefore));
	}
	logDiploAI(2, "[DIP/dealval] player=%d from=%d items=%d total=%d atWar=%d",
		getID(), (int)ePlayer, pList->getLength(), iValue, bAtWar ? 1 : 0);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_DEALVAL, 2)
		.addI(DIPF_player, (int)getID()).addI(DIPF_from, (int)ePlayer)
		.addI(DIPF_items, pList->getLength()).addI(DIPF_total, iValue)
		.addI(DIPF_atWar, bAtWar ? 1 : 0));
	return iValue;
}


bool CvPlayerAI::AI_goldDeal(const CLinkList<TradeData>* pList) const
{
	PROFILE_EXTRA_FUNC();
	CLLNode<TradeData>* pNode;

	for (pNode = pList->head(); pNode; pNode = pList->next(pNode))
	{
		FAssert(!(pNode->m_data.m_bHidden));

		switch (pNode->m_data.m_eItemType)
		{
		case TRADE_GOLD:
		case TRADE_GOLD_PER_TURN:
			return true;
			break;
		}
	}

	return false;
}


/// \brief AI decision making on a proposal it is given
///
/// In this function the AI considers whether or not to accept another player's proposal.  This is used when
/// considering proposals from the human player made in the diplomacy window as well as a couple other places.
bool CvPlayerAI::AI_considerOffer(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, int iChange) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	logDiploAI(1, "[DIP/begin] player=%d with=%d give=%d get=%d iChange=%d",
		getID(), (int)ePlayer, pOurList->getLength(), pTheirList->getLength(), iChange);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_BEGIN, 1)
		.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
		.addI(DIPF_give, pOurList->getLength()).addI(DIPF_get, pTheirList->getLength())
		.addI(DIPF_iChange, iChange));

	if (AI_goldDeal(pTheirList) && AI_goldDeal(pOurList))
	{
		return false;
	}
	CLLNode<TradeData>* pNode;

	if (iChange > -1)
	{
		for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
		{
			if (getTradeDenial(ePlayer, pNode->m_data) != NO_DENIAL)
			{
				logDiploAI(1, "[DIP/decision] player=%d with=%d verdict=reject reason=denial item=%d",
					getID(), (int)ePlayer, (int)pNode->m_data.m_eItemType);
				eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_DECISION_REJECT_DENIAL, 1)
					.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
					.addI(DIPF_item, (int)pNode->m_data.m_eItemType));
				return false;
			}
		}
	}
	const CvPlayerAI& dealer = GET_PLAYER(ePlayer);

	if (dealer.getTeam() == getTeam())
	{
		return true;
	}

	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CORPORATION && pTheirList->getLength() == 0)
		{
			return false;
		}
	}
	const CvTeamAI& myTeam = GET_TEAM(getTeam());

	// Don't always accept giving deals, TRADE_VASSAL and TRADE_SURRENDER come with strings attached
	bool bVassalTrade = false;
	for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_VASSAL)
		{
			bVassalTrade = true;

			for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
			{
				if (GET_TEAM((TeamTypes)iTeam).isAlive() && iTeam != getTeam() && iTeam != dealer.getTeam()
				&& atWar(dealer.getTeam(), (TeamTypes)iTeam) && !atWar(getTeam(), (TeamTypes)iTeam)
				&& myTeam.AI_declareWarTrade((TeamTypes)iTeam, dealer.getTeam(), false) != NO_DENIAL)
				{
					return false;
				}
			}
		}
		else if (pNode->m_data.m_eItemType == TRADE_SURRENDER)
		{
			bVassalTrade = true;

			if (!myTeam.AI_acceptSurrender(dealer.getTeam()))
			{
				return false;
			}
		}
	}
	if (!bVassalTrade && pOurList->getLength() == 0 && pTheirList->getLength() > 0)
	{
		return true;
	}

	int iOurValue = dealer.AI_dealVal(getID(), pOurList, false, iChange);
	const int iTheirValue = AI_dealVal(ePlayer, pTheirList, false, iChange);

	logDiploAI(2, "[DIP/score] player=%d with=%d ourValue=%d theirValue=%d",
		getID(), (int)ePlayer, iOurValue, iTheirValue);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_SCORE, 2)
		.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
		.addI(DIPF_ourValue, iOurValue).addI(DIPF_theirValue, iTheirValue));

	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			if (pTheirList->getLength() == 0)
			{
				return false;
			}
			//only accept 1 time lump sums, continuing gold per turn or resource per turn could be backstabbed
			for (CLLNode<TradeData>* pTheirNode = pTheirList->head(); pTheirNode; pTheirNode = pTheirList->next(pTheirNode))
			{
				if (pTheirNode->m_data.m_eItemType == TRADE_GOLD_PER_TURN || pTheirNode->m_data.m_eItemType == TRADE_DEFENSIVE_PACT || pTheirNode->m_data.m_eItemType == TRADE_RESOURCES)
				{
					return false;
				}
			}
		}
	}

	if (iOurValue > 0 && 0 == pTheirList->getLength() && 0 == iTheirValue)
	{
		const CvTeamAI& dealerTeam = GET_TEAM(GET_PLAYER(ePlayer).getTeam());

		if (myTeam.isVassal(dealer.getTeam()) && CvDeal::isVassalTributeDeal(pOurList))
		{
			if (AI_getAttitude(ePlayer, false) > GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_VASSAL)
			// OR I'm at war OR dealer has any defensive pact
			|| myTeam.isAtWar() || dealerTeam.getDefensivePactCount() != 0)
			{
				return true;
			}
			iOurValue *= 10 + myTeam.getPower(false);
			iOurValue /= 10 + dealerTeam.getPower(false);
		}
		else if (AI_getMemoryCount(ePlayer, MEMORY_MADE_DEMAND_RECENT) > 0
		|| AI_getAttitude(ePlayer) < ATTITUDE_PLEASED && myTeam.getPower(false) > dealerTeam.getPower(false) * 4 / 3)
		{
			return false;
		}

		int iThreshold = 2 * (myTeam.AI_getHasMetCounter(dealer.getTeam()) + 50);

		if (dealerTeam.AI_isLandTarget(getTeam()))
		{
			iThreshold *= 3;
		}

		iThreshold *= 100 + dealerTeam.getPower(false);
		iThreshold /= 100 + myTeam.getPower(false);

		iThreshold -= dealer.AI_getPeacetimeGrantValue(getID());

		const bool bAcceptGrant = iOurValue < iThreshold;
		logDiploAI(1, "[DIP/decision] player=%d with=%d verdict=%s reason=grant ourValue=%d threshold=%d",
			getID(), (int)ePlayer, bAcceptGrant ? "ACCEPT" : "reject", iOurValue, iThreshold);
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO,
				bAcceptGrant ? DIP_DECISION_ACCEPT_GRANT : DIP_DECISION_REJECT_GRANT, 1)
			.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
			.addI(DIPF_ourValue, iOurValue).addI(DIPF_threshold, iThreshold));
		return bAcceptGrant;
	}

	if (iChange < 0)
	{
		const bool bAcceptRenew = iTheirValue * 110 >= iOurValue * 100;
		logDiploAI(1, "[DIP/decision] player=%d with=%d verdict=%s ourValue=%d theirValue=%d iChange<0",
			getID(), (int)ePlayer, bAcceptRenew ? "ACCEPT" : "reject", iOurValue, iTheirValue);
		eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO,
				bAcceptRenew ? DIP_DECISION_ACCEPT_RENEW : DIP_DECISION_REJECT_RENEW, 1)
			.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
			.addI(DIPF_ourValue, iOurValue).addI(DIPF_theirValue, iTheirValue));
		return bAcceptRenew;
	}
	const bool bAccept = iTheirValue >= iOurValue;
	logDiploAI(1, "[DIP/decision] player=%d with=%d verdict=%s ourValue=%d theirValue=%d",
		getID(), (int)ePlayer, bAccept ? "ACCEPT" : "reject", iOurValue, iTheirValue);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO,
			bAccept ? DIP_DECISION_ACCEPT : DIP_DECISION_REJECT, 1)
		.addI(DIPF_player, (int)getID()).addI(DIPF_with, (int)ePlayer)
		.addI(DIPF_ourValue, iOurValue).addI(DIPF_theirValue, iTheirValue));
	return bAccept;
}


bool CvPlayerAI::AI_counterPropose(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirInventory, CLinkList<TradeData>* pOurInventory, CLinkList<TradeData>* pTheirCounter, CLinkList<TradeData>* pOurCounter) const
{
	PROFILE_EXTRA_FUNC();
	const bool bTheirGoldDeal = AI_goldDeal(pTheirList);
	const bool bOurGoldDeal = AI_goldDeal(pOurList);

	if (bOurGoldDeal && bTheirGoldDeal)
	{
		return false;
	}
	bool* pabBonusDeal = new bool[GC.getNumBonusInfos()];

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		pabBonusDeal[iI] = false;
	}
	CLLNode<TradeData>* pNode;
	CLLNode<TradeData>* pGoldPerTurnNode = NULL;
	CLLNode<TradeData>* pGoldNode = NULL;

	int iHumanDealWeight = AI_dealVal(ePlayer, pTheirList);
	int iAIDealWeight = GET_PLAYER(ePlayer).AI_dealVal(getID(), pOurList);

	pTheirCounter->clear();
	pOurCounter->clear();

	bool bOfferingCity = false;
	bool bReceivingCity = false;
	for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			bReceivingCity = true;
			break;
		}
	}
	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			bOfferingCity = true;
			break;
		}
	}

	if (iAIDealWeight > iHumanDealWeight)
	{
		if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
		{
			int iBestValue = 0;
			int iBestWeight = 0;
			CLLNode<TradeData>* pBestNode = NULL;

			for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CITIES)
				{
					FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

					if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
					{
						CvCity* pCity = GET_PLAYER(ePlayer).getCity(pNode->m_data.m_iData);

						if (pCity != NULL)
						{
							const int iWeight = AI_cityTradeVal(pCity);

							if (iWeight > 0)
							{
								const int iValue = AI_targetCityValue(pCity, false);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestWeight = iWeight;
									pBestNode = pNode;
								}
							}
						}
					}
				}
			}
			if (pBestNode != NULL)
			{
				iHumanDealWeight += iBestWeight;
				pTheirCounter->insertAtEnd(pBestNode->m_data);
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_GOLD:
						if (!bOurGoldDeal)
						{
							pGoldNode = pNode;
						}
						break;
					case TRADE_GOLD_PER_TURN:
						if (!bOurGoldDeal)
						{
							pGoldPerTurnNode = pNode;
						}
						break;
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = GET_PLAYER(ePlayer).AI_maxGoldTrade(getID());

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					// If we can wrap this up with gold outright then do so.
					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iHumanDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pTheirCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
		{
			FAssert(!pNode->m_data.m_bHidden);

			switch (pNode->m_data.m_eItemType)
			{
			case TRADE_RESOURCES: pabBonusDeal[pNode->m_data.m_iData] = true;
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					int iWeight = 0;

					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_TECHNOLOGIES:
					{
						iWeight += GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
						break;
					}
					case TRADE_RESOURCES:
					{
						if (!bOfferingCity && !pabBonusDeal[pNode->m_data.m_iData]
						&& GET_PLAYER(ePlayer).getNumTradeableBonuses((BonusTypes)pNode->m_data.m_iData) > 1
						&& GET_PLAYER(ePlayer).AI_corporationBonusVal((BonusTypes)(pNode->m_data.m_iData)) == 0)
						{
							iWeight += AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, 1);
							pabBonusDeal[pNode->m_data.m_iData] = true;
						}
						break;
					}
					}
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MAPS)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CONTACT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL && pNode->m_data.m_iData > -1)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)pNode->m_data.m_iData, GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}
		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden
			&& (pNode->m_data.m_eItemType == TRADE_MILITARY_UNIT || pNode->m_data.m_eItemType == TRADE_WORKER))
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					CvUnit* pUnit = getUnit(pNode->m_data.m_iData);
					if (pUnit)
					{
						const int iWeight = std::max(GET_PLAYER(ePlayer).AI_militaryUnitTradeVal(pUnit), GET_PLAYER(ePlayer).AI_workerTradeVal(pUnit));
						if (iWeight > 0)
						{
							iHumanDealWeight += iWeight;
							pTheirCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = GET_PLAYER(ePlayer).AI_maxGoldTrade(getID());

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iHumanDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pTheirCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		if (!bOfferingCity && pGoldPerTurnNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iTurns = getTreatyLength();
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				OutputDebugString(CvString::format("%S (%d)\n\tValueDiff=%d, iGoldValuePercent=%d, iGoldData=%d\n\t\tAI_getGoldValue=%d\n", getCivilizationDescription(0), getID(), iValueDiff, iGoldValuePercent, iGoldData, AI_getGoldValue(iGoldData, iGoldValuePercent)).c_str());

				// Account for rounding errors
				while (iGoldData < MAX_INT && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
				{
					iGoldData++;
					OutputDebugString(CvString::format("\tValueDiff=%d, iGoldValuePercent=%d, iGoldData=%d\n\t\tAI_getGoldValue=%d\n", iValueDiff, iGoldValuePercent, iGoldData, AI_getGoldValue(iGoldData, iGoldValuePercent)).c_str());
				}
				iGoldData = std::min(iGoldData / iTurns, GET_PLAYER(ePlayer).AI_maxGoldPerTurnTrade(getID()));

				if (iGoldData > 0)
				{
					const int iValue = AI_getGoldValue(iGoldData * iTurns, iGoldValuePercent);
					if (iValue > 0)
					{
						iHumanDealWeight += iValue;
						pGoldPerTurnNode->m_data.m_iData = iGoldData;
						pTheirCounter->insertAtEnd(pGoldPerTurnNode->m_data);
						pGoldPerTurnNode = NULL;
					}
				}
			}
		}

		if (!bOfferingCity)
		{
			for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_RESOURCES)
				{
					FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

					if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL && !pabBonusDeal[pNode->m_data.m_iData]
					&& GET_PLAYER(ePlayer).getNumTradeableBonuses((BonusTypes)pNode->m_data.m_iData) > 0)
					{
						pabBonusDeal[pNode->m_data.m_iData] = true;

						const int iWeight = AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, 1);

						if (iWeight > 0)
						{
							iHumanDealWeight += iWeight;
							pTheirCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}
	}
	else if (iHumanDealWeight > iAIDealWeight)
	{
		if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
		{
			bool bSurrender = false;
			for (pNode = pOurInventory->head(); pNode; pNode = pOurInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
				{
					if (pNode->m_data.m_eItemType == TRADE_SURRENDER)
					{
						if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
						{
							iAIDealWeight += GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_surrenderTradeVal(getTeam());
							pOurCounter->insertAtEnd(pNode->m_data);
							bSurrender = true;
						}
						break;
					}
				}
			}

			if (!bSurrender)
			{
				for (pNode = pOurInventory->head(); pNode; pNode = pOurInventory->next(pNode))
				{
					if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
					{
						if (pNode->m_data.m_eItemType == TRADE_PEACE_TREATY)
						{
							pOurCounter->insertAtEnd(pNode->m_data);
							break;
						}
					}
				}
			}

			int iBestValue = 0;
			int iBestWeight = 0;
			CLLNode<TradeData>* pBestNode = NULL;

			for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CITIES)
				{
					FAssert(canTradeItem(ePlayer, pNode->m_data));

					if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
					{
						CvCity* pCity = getCity(pNode->m_data.m_iData);

						if (pCity != NULL)
						{
							const int iWeight = GET_PLAYER(ePlayer).AI_cityTradeVal(pCity);

							if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
							{
								const int iValue = GET_PLAYER(ePlayer).AI_targetCityValue(pCity, false);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestWeight = iWeight;
									pBestNode = pNode;
								}
							}
						}
					}
				}
			}
			if (pBestNode != NULL)
			{
				iAIDealWeight += iBestWeight;
				pOurCounter->insertAtEnd(pBestNode->m_data);
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					switch (pNode->m_data.m_eItemType)
					{
						case TRADE_GOLD:
						{
							if (!bTheirGoldDeal)
							{
								pGoldNode = pNode;
							}
							break;
						}
						case TRADE_GOLD_PER_TURN:
						{
							if (!bTheirGoldDeal)
							{
								pGoldPerTurnNode = pNode;
							}
							break;
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = AI_maxGoldTrade(ePlayer);

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					// If we can wrap this up with gold outright then do so.
					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iAIDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pOurCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
		{
			FAssert(!pNode->m_data.m_bHidden);

			switch (pNode->m_data.m_eItemType)
			{
			case TRADE_RESOURCES: pabBonusDeal[pNode->m_data.m_iData] = true;
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					int iWeight = 0;
					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_TECHNOLOGIES:
					{
						iWeight += GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), getTeam());
						break;
					}
					case TRADE_RESOURCES:
					{
						if (!pabBonusDeal[pNode->m_data.m_iData] && getNumTradeableBonuses((BonusTypes)(pNode->m_data.m_iData)) > 1)
						{
							iWeight += GET_PLAYER(ePlayer).AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), getID(), 1);
							pabBonusDeal[pNode->m_data.m_iData] = true;
						}
						break;
					}
					}
					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MAPS)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					const int iWeight = GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_mapTradeVal(getTeam());

					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CONTACT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL && pNode->m_data.m_iData > -1)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)pNode->m_data.m_iData, GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}
		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MILITARY_UNIT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					CvUnit* pUnit = getUnit(pNode->m_data.m_iData);
					if (pUnit)
					{
						const int iWeight = AI_militaryUnitTradeVal(pUnit);
						if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
						{
							iAIDealWeight += iWeight;
							pOurCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = AI_maxGoldTrade(ePlayer);

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iAIDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pOurCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}
		if (pGoldPerTurnNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iTurns = getTreatyLength();
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);

				// Account for rounding errors
				while (iGoldData < MAX_INT && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
				{
					iGoldData++;
				}
				iGoldData = std::min(iGoldData / iTurns, AI_maxGoldPerTurnTrade(ePlayer));

				if (iGoldData > 0)
				{
					const int iValue = AI_getGoldValue(iGoldData * iTurns, iGoldValuePercent);
					if (iValue > 0)
					{
						iAIDealWeight += iValue;
						pGoldPerTurnNode->m_data.m_iData = iGoldData;
						pOurCounter->insertAtEnd(pGoldPerTurnNode->m_data);
						pGoldPerTurnNode = NULL;
					}
				}
			}
		}
	}
	SAFE_DELETE_ARRAY(pabBonusDeal);

	return ((iAIDealWeight <= iHumanDealWeight) && ((pOurList->getLength() > 0) || (pOurCounter->getLength() > 0) || (pTheirCounter->getLength() > 0)));
}


int CvPlayerAI::AI_maxGoldTrade(PlayerTypes ePlayer) const
{
	int64_t iMaxGold;

	FAssert(ePlayer != getID());

	if (isHumanPlayer() || GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		const int64_t iMaxGold = getGold();
		return iMaxGold < MAX_INT ? static_cast<int>(iMaxGold) : MAX_INT;
	}
	const int64_t iGold = getGold();
	iMaxGold = iGold;

	iMaxGold *= GC.getLeaderHeadInfo(getPersonalityType()).getMaxGoldTradePercent();
	iMaxGold /= 100;

	const int iGoldRate = calculateGoldRate();

	if (iGoldRate < 0)
	{
		iMaxGold = std::min<int64_t>(iMaxGold, iGold + iGoldRate * CvGameSpeedScale::speedPercent() / 10);
	}
	if (iMaxGold > 0)
	{
		iMaxGold = std::min<int64_t>(iMaxGold, iGold);

		iMaxGold -= (iMaxGold % GC.getDIPLOMACY_VALUE_REMAINDER());

		return iMaxGold < MAX_INT ? static_cast<int>(iMaxGold) : MAX_INT;
	}
	return 0;
}


int CvPlayerAI::AI_maxGoldPerTurnTrade(PlayerTypes ePlayer) const
{
	int64_t iMaxGoldPerTurn;

	FAssert(ePlayer != getID());

	if (isHumanPlayer() || (GET_PLAYER(ePlayer).getTeam() == getTeam()))
	{
		iMaxGoldPerTurn = calculateGoldRate() + getGold() / getTreatyLength();
	}
	else
	{
		iMaxGoldPerTurn = getTotalPopulation();

		iMaxGoldPerTurn *= GC.getLeaderHeadInfo(getPersonalityType()).getMaxGoldPerTurnTradePercent();
		iMaxGoldPerTurn /= 100;

		iMaxGoldPerTurn += std::min(0, getGoldPerTurnByPlayer(ePlayer));
	}

	return std::max(0, (int)std::min<int64_t>(iMaxGoldPerTurn, calculateGoldRate()));
}


// Toffer - Gold 2 Value & Value 2 Gold
int CvPlayerAI::AI_getGoldValue(const int iGold, const int iValuePercent) const
{
	return static_cast<int>(std::min<uint64_t>(iGold * iValuePercent / CvGameSpeedScale::speedPercent(), MAX_INT));
}
int CvPlayerAI::AI_getGoldFromValue(const int iValue, const int iValuePercent) const
{
	return static_cast<int>(std::min<uint64_t>(iValue * CvGameSpeedScale::speedPercent() / iValuePercent, MAX_INT));
}
// ! Toffer


int CvPlayerAI::AI_bonusVal(BonusTypes eBonus, int iChange, bool bForTrade) const
{
	PROFILE_FUNC();

	if (iChange < 2 && iChange > -2)
	{
		if (iChange == 0)
		{
			return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade);
		}
		const int iBonusCount = getNumAvailableBonuses(eBonus);
		if (iChange == 1 && iBonusCount == 0 || iChange == -1 && iBonusCount == 1)
		{
			//This is assuming the none-to-one or one-to-none case.
			return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade);
		}
	}
	//This is basically the marginal value of an additional instance of a bonus.
	return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade) / 5;
}

//Value sans corporation
int CvPlayerAI::AI_baseBonusVal(BonusTypes eBonus, bool bForTrade) const
{
	PROFILE_FUNC();

	//recalculate if needed
	if (m_aiBonusValue[eBonus] == -1 || !bForTrade && !m_abNonTradeBonusCalculated[eBonus])
	{
		PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate");

		int iValue = 0;
		int iTradeValue = 0;
		//	If we've already calculated everythign except the not-currently-constructable
		//	buildings (which only appky to the non-trade value) and now we need the full
		//	non-trade value thn we just need to add the contriobution from those buildings
		const bool bJustNonTradeBuildings = (m_aiBonusValue[eBonus] != -1);
		CvTeam& kTeam = GET_TEAM(getTeam());

		if (!kTeam.isBonusObsolete(eBonus))
		{
			CvCity* pCapital = getCapitalCity();
			int iCoastalCityCount = countNumCoastalCities();

			// find the first coastal city
			CvCity* pCoastalCity = NULL;
			if (iCoastalCityCount > 0)
			{
				pCoastalCity = findBestCoastalCity();
			}
			const int iCityCount = getNumCities();
			const int iCityCountNonZero = std::max(1, iCityCount);

			if (!bJustNonTradeBuildings)
			{
				// The legacy weight was human ×100, which is exactly what the ×100-native read already is.
				iValue += GC.getBonusInfo(eBonus).getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_EMPIRE);
				iValue += GC.getBonusInfo(eBonus).getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE);

				iTradeValue = iValue;

				{
					PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Unit Value");
					// WHICH UNITS CARE ABOUT THIS BONUS? -- the bonus's own load-populated edge families answer
					// both halves, so neither is a database scan ([DEC-one-reverse-view]):
					//   RELATED     -- every unit referencing it on any compiled surface, INCLUDING a deposit's
					//                  `enabled` condition, which is how a buildRate GATED on the bonus is found
					//                  at all (a gated deposit is no part of `requires`, so the gate index alone
					//                  would miss every production-speed effect).
					//   REQUIRED_BY -- the subset whose `requires` names it: the only units it can UNLOCK.
					std::set<int> relatedUnits;
					std::set<int> gatedUnits;
					const CvInfo* pBonusInfo = EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus);
					EnablerKernel::addEdge(pBonusInfo, EDGEF_RELATED, EDGEB_UNITS, relatedUnits);
					EnablerKernel::addEdge(pBonusInfo, EDGEF_REQUIRED_BY, EDGEB_UNITS, gatedUnits);

					// The two worlds. Both halves below are the DELTA between them, which is what makes this a
					// valuation of the BONUS rather than of the unit ([patterns.md] THE VALUATION PROTOCOL).
					CvCascadeHypothetical kWithBonus;
					kWithBonus.present[EDGEB_BONUSES].insert((int)eBonus);
					CvCascadeHypothetical kWithoutBonus;
					kWithoutBonus.absent[EDGEB_BONUSES].insert((int)eBonus);

					for (std::set<int>::const_iterator itUnit = relatedUnits.begin();
						itUnit != relatedUnits.end(); ++itUnit)
					{
						const UnitTypes eLoopUnit = static_cast<UnitTypes>(*itUnit);
						const CvUnitInfo& kLoopUnit = GC.getUnitInfo(eLoopUnit);

						if (!EnablerKernel::everAvailable(EDGEB_UNITS, (int)eLoopUnit))
						{
							continue;
						}
						int iTempValue = 0;

						// (1) DOES IT UNLOCK THE UNIT? -- ask the gate in both worlds. This replaces the AND/OR
						// prereq-bonus bookkeeping wholesale: the old code scored 50 for a sole required bonus
						// and 40 for one of several, quartered per alternative already held, all reconstructed
						// from prereq getters. The gate answers it directly -- a bonus that leaves the verdict
						// unchanged (because an alternative covers it) contributes nothing, which is what the
						// quartering was approximating.
						if (pCapital != NULL && gatedUnits.count((int)eLoopUnit) != 0
						&& EnablerKernel::requiresMetInCity(*pCapital, EDGEB_UNITS, (int)eLoopUnit, false, &kWithBonus)
						&& !EnablerKernel::requiresMetInCity(*pCapital, EDGEB_UNITS, (int)eLoopUnit, false, &kWithoutBonus))
						{
							iTempValue += 50;
						}

						// (2) DOES IT BUILD THE UNIT FASTER? -- the buildRate delta between the two worlds. The
						// authored shape is `buildRate.self.percent` gated on the bonus, so the conditioned tail
						// resolving under each hypothetical IS the answer; nothing here re-derives which entries
						// the bonus gates.
						if (pCapital != NULL)
						{
							const int iRateWith = kLoopUnit.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
								CASC_UNIT_PERCENT, pCapital->getCityContext(), getEmpireContext(),
								pCapital->plotGroup(getID()), &kWithBonus);
							const int iRateWithout = kLoopUnit.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
								CASC_UNIT_PERCENT, pCapital->getCityContext(), getEmpireContext(),
								pCapital->plotGroup(getID()), &kWithoutBonus);

							iTempValue += (iRateWith - iRateWithout) / 10;
						}

						if (iTempValue <= 0)
						{
							continue;
						}
						// A sea unit is worth what the empire's COASTLINE can use, not what its city count says.
						// The domain is a TAG now ([tags.md]: DOMAIN_* is the engine enum, `seaUnit` the
						// queryable identity), so this asks the unit what it IS rather than an info getter.
						const bool bIsWater = kLoopUnit.getDomain() == DOMAIN_SEA;

						if (bIsWater && !isLimitedUnit(eLoopUnit))
						{
							iTempValue *= std::min(iCoastalCityCount * 2, iCityCount);
							iTempValue /= iCityCountNonZero;
						}
						int iTempTradeValue = 0;

						if (bIsWater && pCoastalCity == NULL)
						{
							iTempValue = 2;   // worthless with no coast to sail from
							iTempTradeValue = iTempValue;
						}
						else if (getUnitAvailabilityAnywhere(eLoopUnit) == EnablerDomain::STATE_LISTED)
						{
							// Obsolete here = the city does not OFFER it, which the upgrade-tree dormancy already
							// folds into the verdict (enabler.md par.8: a LISTED unit is one whose upgrade chain
							// does not dorm it).
							if ((bIsWater && pCoastalCity->getUnitAvailability(eLoopUnit) != EnablerDomain::STATE_LISTED)
							|| (pCapital != NULL && pCapital->getUnitAvailability(eLoopUnit) != EnablerDomain::STATE_LISTED))
							{
								iTempValue = 2;
							}
							else iTempValue *= 2;   // we could build it if we had this bonus

							iTempTradeValue = iTempValue;
						}
						// Trades are short-term: if we cannot train it now, assume we will not for the duration.
						else iTempTradeValue = 0;

						// HOW FAR OFF IS THE UNIT AT ALL? The techs it needs are atoms in its `requires` tree, read
						// through the ONE scanner
						// ([DEC-single-implementation]) rather than a prereq getter -- which is also why the
						// religion/corporation legs the old code carried separately are gone: their tech gates are
						// atoms in the same tree and arrive here already.
						CascadeCondDeps kUnitDeps;
						EnablerKernel::scanCondDeps(kLoopUnit.getRequires()->build, kUnitDeps, false, false);
						int iTechDistance = 0;

						for (std::set<int>::const_iterator itTech = kUnitDeps.techs.begin();
							itTech != kUnitDeps.techs.end(); ++itTech)
						{
							iTechDistance = std::max(iTechDistance, findPathLength((TechTypes)*itTech, false));
						}
						iTempValue = (iTempValue * 15) / (10 + iTechDistance);
						iTempTradeValue = (iTempTradeValue * 15) / (10 + iTechDistance);

						iValue += iTempValue;
						iTradeValue += iTempTradeValue;
					}
				}
			}

			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Building Value");
					// WHICH BUILDINGS CARE ABOUT THIS BONUS? -- the same two legs as the unit half above, off the
					// bonus's own load-populated edge families ([DEC-one-reverse-view]): RELATED catches every
					// compiled reference INCLUDING a deposit's `enabled` condition (which is where a
					// bonus-conditioned yield/commerce/wellbeing deposit lives), REQUIRED_BY the subset it can
					// UNLOCK. The whole-database sweep this replaces ran a findPathLength per surviving building.
					std::set<int> relatedBuildings;
					std::set<int> gatedBuildings;
					const CvInfo* pBonusInfoB = EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus);
					EnablerKernel::addEdge(pBonusInfoB, EDGEF_RELATED, EDGEB_BUILDINGS, relatedBuildings);
					EnablerKernel::addEdge(pBonusInfoB, EDGEF_REQUIRED_BY, EDGEB_BUILDINGS, gatedBuildings);

					CvCascadeHypothetical kBldWith;
					kBldWith.present[EDGEB_BONUSES].insert((int)eBonus);
					CvCascadeHypothetical kBldWithout;
					kBldWithout.absent[EDGEB_BONUSES].insert((int)eBonus);

					for (std::set<int>::const_iterator itBuilding = relatedBuildings.begin();
						itBuilding != relatedBuildings.end(); ++itBuilding)
					{
						const BuildingTypes eBuildingX = static_cast<BuildingTypes>(*itBuilding);

						if (GET_TEAM(getTeam()).isObsoleteBuilding(eBuildingX)
						|| !EnablerKernel::everAvailable(EDGEB_BUILDINGS, (int)eBuildingX))
						{
							continue;
						}
						const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eBuildingX);
						const bool bCanConstruct = getBuildingAvailabilityAnywhere(eBuildingX) >= EnablerDomain::STATE_GREYED;

						if ((bJustNonTradeBuildings || bForTrade) && bCanConstruct == bJustNonTradeBuildings)
						{
							continue;
						}
						if (pCapital == NULL)
						{
							continue;   // every read below is the capital's what-if
						}
						const CityContext& kCityCtx = pCapital->getCityContext();
						const EmpireContext& kEmpireCtx = getEmpireContext();
						const CvPlotGroup* pCapitalGroup = pCapital->plotGroup(getID());

						// (1) WHAT THE BONUS IS WORTH THROUGH THIS BUILDING -- the DELTA across the groups it
						// deposits into, between holding the bonus and not. This replaces a dozen bespoke
						// getBonus<Channel>Changes / getBonus<Channel>Modifier reads with the ONE valuation asked
						// twice ([patterns.md] THE VALUATION PROTOCOL): a bonus-conditioned deposit resolves
						// under each hypothetical, so nothing here re-derives which entries the bonus gates.
						// ⚠ The per-city weighting the legacy terms applied by hand is gone with them -- the
						// valuation already answers "here, in this city", which is what those divisors were
						// reconstructing from an empire-wide number.
						int iTempValue = 0;
						{
							int aiWith[NUM_YIELD_TYPES];
							int aiWithout[NUM_YIELD_TYPES];
							kLoopBuilding.expectedFlatYields(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kBldWith);
							kLoopBuilding.expectedFlatYields(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kBldWithout);
							for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
							{
								iTempValue += (aiWith[iYield] - aiWithout[iYield]) * 6 / 100;   // ×100 flat -> whole
							}
							kLoopBuilding.expectedYieldModifiers(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kBldWith);
							kLoopBuilding.expectedYieldModifiers(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kBldWithout);
							for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
							{
								iTempValue += (aiWith[iYield] - aiWithout[iYield]) / 2;   // a percent is unscaled
							}
						}
						{
							int aiWith[NUM_COMMERCE_TYPES];
							int aiWithout[NUM_COMMERCE_TYPES];
							kLoopBuilding.expectedFlatCommerce(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kBldWith);
							kLoopBuilding.expectedFlatCommerce(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kBldWithout);
							for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
							{
								iTempValue += (aiWith[iCommerce] - aiWithout[iCommerce]) * 6 / 100;
							}
						}
						{
							// The wellbeing pair nets before weighting: a bonus that brings +2 happy and +1 angry
							// through this building is worth the +1, never the +2 ([modifier.md] §2b -- the
							// opposing channels net at the verdict, and the net is what a valuation weighs).
							int aiWith[NUM_WELLBEING_CHANNELS];
							int aiWithout[NUM_WELLBEING_CHANNELS];
							kLoopBuilding.expectedWellbeing(kCityCtx, kEmpireCtx, pCapitalGroup, aiWith, &kBldWith);
							kLoopBuilding.expectedWellbeing(kCityCtx, kEmpireCtx, pCapitalGroup, aiWithout, &kBldWithout);
							const int iHappyDelta = (aiWith[WELLBEING_HAPPINESS] - aiWithout[WELLBEING_HAPPINESS])
								- (aiWith[WELLBEING_ANGER] - aiWithout[WELLBEING_ANGER]);
							const int iHealthDelta = (aiWith[WELLBEING_HEALTH] - aiWithout[WELLBEING_HEALTH])
								- (aiWith[WELLBEING_UNHEALTH] - aiWithout[WELLBEING_UNHEALTH]);
							iTempValue += iHappyDelta * 12 / 100;
							iTempValue += iHealthDelta * 8 / 100;
						}
						// Build-speed and defense, the two remaining channels the legacy terms weighted.
						iTempValue += (kLoopBuilding.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
								CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kBldWith)
							- kLoopBuilding.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
								CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kBldWithout)) / 10;

						iTempValue += (kLoopBuilding.expectedModifier(MODFAM_DEFENSE, DEFENSE_AMOUNT,
								CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kBldWith)
							- kLoopBuilding.expectedModifier(MODFAM_DEFENSE, DEFENSE_AMOUNT,
								CASC_UNIT_PERCENT, kCityCtx, kEmpireCtx, pCapitalGroup, &kBldWithout)) / 10;

						// (2) DOES IT UNLOCK THE BUILDING? -- the gate in both worlds, exactly as the unit half.
						const bool bUnlocks = gatedBuildings.count((int)eBuildingX) != 0
							&& EnablerKernel::requiresMetInCity(*pCapital, EDGEB_BUILDINGS, (int)eBuildingX, false, &kBldWith)
							&& !EnablerKernel::requiresMetInCity(*pCapital, EDGEB_BUILDINGS, (int)eBuildingX, false, &kBldWithout);

						int iTempNonTradeValue = 0;
						int iTempTradeValue = 0;

						if (bUnlocks)
						{
							iTempNonTradeValue += 100;
						}
						if (bCanConstruct)
						{
							// Double weight on something we could put up right now.
							iTempNonTradeValue += iTempValue;
							iTempNonTradeValue *= 2;
							iTempTradeValue += 2 * iTempValue;
						}

						// A non-limited COASTAL building is worth what the coastline can host. "Needs a coast" is
						// a `requires` CONDITION in the JSON model, not a property of the building -- a rebuilt
						// info carries no isWater() -- so the ONE scanner answers it off the same tree the tech
						// distance below reads.
						CascadeCondDeps kBldDeps;
						EnablerKernel::scanCondDeps(kLoopBuilding.getRequires()->build, kBldDeps, false, false);

						if (kBldDeps.coastal && !isLimitedWonder(eBuildingX))
						{
							iTempNonTradeValue *= iCoastalCityCount;
							iTempNonTradeValue /= std::max(1, iCityCount / 2);
							iTempTradeValue *= iCoastalCityCount;
							iTempTradeValue /= std::max(1, iCityCount / 2);
						}

						if (iTempNonTradeValue > 0 || iTempTradeValue > 0)
						{
							// How far off is the building at all? Its `requires` names the techs -- the same scan
							// as above -- which also retires the separate religion and corporation legs the old
							// code carried: their tech gates are atoms in the same tree and arrive with it.
							int iTechDistance = 0;

							for (std::set<int>::const_iterator itTech = kBldDeps.techs.begin();
								itTech != kBldDeps.techs.end(); ++itTech)
							{
								iTechDistance = std::max(iTechDistance, findPathLength((TechTypes)*itTech, false));
							}
							if (iTempNonTradeValue > 0 && !bCanConstruct)
							{
								iTempNonTradeValue = (iTempNonTradeValue * 15) / (10 + iTechDistance);
							}
							iTempTradeValue = (iTempTradeValue * 15) / (10 + iTechDistance);
						}
						// Trades are short-term: if we cannot construct it now, assume we will not for the duration.
						if (!bCanConstruct)
						{
							iTempTradeValue = 0;
						}
						iValue += iTempNonTradeValue;
						iTradeValue += iTempTradeValue / 3;
					}
			}

			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Project Value");

				// Only the projects that reference this bonus at all -- the same RELATED leg the unit and building
				// halves drive off ([DEC-one-reverse-view]), which for a project is where a bonus-conditioned
				// buildRate deposit lives.
				std::set<int> relatedProjects;
				EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus),
					EDGEF_RELATED, EDGEB_PROJECTS, relatedProjects);

				for (std::set<int>::const_iterator itProject = relatedProjects.begin();
					itProject != relatedProjects.end() && pCapital != NULL; ++itProject)
				{
					const ProjectTypes eProject = static_cast<ProjectTypes>(*itProject);
					const CvProjectInfo& kLoopProject = GC.getProjectInfo(eProject);

					// The build-speed the bonus brings: the buildRate delta between holding it and not, the same
					// read the unit and building halves make.
					CvCascadeHypothetical kPrjWith;
					kPrjWith.present[EDGEB_BONUSES].insert((int)eBonus);
					CvCascadeHypothetical kPrjWithout;
					kPrjWithout.absent[EDGEB_BONUSES].insert((int)eBonus);

					int iTempValue = (kLoopProject.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
							CASC_UNIT_PERCENT, pCapital->getCityContext(), getEmpireContext(),
							pCapital->plotGroup(getID()), &kPrjWith)
						- kLoopProject.expectedModifier(MODFAM_BUILD_RATE, BUILD_RATE_AMOUNT,
							CASC_UNIT_PERCENT, pCapital->getCityContext(), getEmpireContext(),
							pCapital->plotGroup(getID()), &kPrjWithout)) / 10;

					if (iTempValue > 0)
					{
						int iTempTradeValue = 0;
						int iTempNonTradeValue = 0;

						if (!GC.getGame().isProjectMaxedOut(eProject) && !kTeam.isProjectMaxedOut(eProject))
						{
							if (canCreate(eProject))
							{
								iTempValue *= 2;
								iTempTradeValue += iTempValue;
							}
							// Trades are short-term - if we can't construct the building now assume we won't be able to do so for the duration of the trade
							iTempNonTradeValue += iTempValue;
						}

						if (kLoopProject.getTechPrereq() != NO_TECH)
						{
							const int iDiff = abs(GC.getTechInfo(kLoopProject.getTechPrereq()).getEra() - getCurrentEra());

							if (iDiff == 0)
							{
								iTempTradeValue *= 3;
								iTempTradeValue /= 2;

								iTempNonTradeValue *= 3;
								iTempNonTradeValue /= 2;
							}
							else
							{
								iTempTradeValue /= iDiff;
								iTempNonTradeValue /= iDiff;
							}
						}
						iValue += iTempNonTradeValue;
						iTradeValue += iTempTradeValue;
					}
				}
			}


			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Route Value");
				RouteTypes eBestRoute = getBestRoute();
				for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
				{
					RouteTypes eRoute = (RouteTypes)(GC.getBuildInfo((BuildTypes)iI).getRoute());

					if (eRoute != NO_ROUTE)
					{
						int iTempValue = 0;
						if (GC.getRouteInfo(eRoute).getPrereqBonus() == eBonus)
						{
							iTempValue += 80;
						}
						if (algo::any_of_equal(GC.getRouteInfo(eRoute).getPrereqOrBonuses(), eBonus))
						{
							iTempValue += 40;
						}
						if (eBestRoute != NO_ROUTE && GC.getRouteInfo(getBestRoute()).getValue() > GC.getRouteInfo(eRoute).getValue())
						{
							iTempValue /= 2;
						}
						iValue += iTempValue;
						iTradeValue += iTempValue;
					}
				}
			}

			//Resource scarcity. If there are only limited quantities of this resource, treasure it.
			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Bonus Scarcity");
				int iTotalBonusCount = 0;
				for (int iI = 0; iI < MAX_PLAYERS; iI++)
				{
					if (GET_PLAYER((PlayerTypes)iI).isAlive()
					&& GET_TEAM(getTeam()).isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						iTotalBonusCount += GET_PLAYER((PlayerTypes)iI).getNumAvailableBonuses(eBonus);
					}
				}
				const int iTempValue =
					(
						GC.getBonusInfo(eBonus).getAIObjective() * 10
						+
						getNumAvailableBonuses(eBonus) * 300 / std::max(1, iTotalBonusCount)
					);
				iValue += iTempValue;
				iTradeValue += iTempValue;
			}
			iValue /= 10;
			// All these effects are only going to be with us for a short period so devalue
			iTradeValue /= 30;
		}

		//	Check there wasn't a race copndition that meant some other thread already did this
		if (m_aiBonusValue[eBonus] == -1 || !bForTrade && !m_abNonTradeBonusCalculated[eBonus])
		{
			if (!bJustNonTradeBuildings)
			{
				m_aiBonusValue[eBonus] = std::max(0, iValue);
				m_aiTradeBonusValue[eBonus] = std::max(0, iTradeValue);
			}
			else
			{
				m_aiBonusValue[eBonus] += std::max(0, iValue);
			}
			m_abNonTradeBonusCalculated[eBonus] |= !bForTrade;
		}
	}
	return (bForTrade ? m_aiTradeBonusValue[eBonus] : m_aiBonusValue[eBonus]);
}

int CvPlayerAI::AI_corporationBonusVal(BonusTypes eBonus) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	int iCityCount = getNumCities();
	iCityCount += iCityCount / 6 + 1;

	for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation)
	{
		int iCorpCount = getHasCorporationCount((CorporationTypes)iCorporation);
		if (iCorpCount > 0)
		{
			iCorpCount += getNumCities() / 6 + 1;
			const CvCorporationInfo& kCorp = GC.getCorporationInfo((CorporationTypes)iCorporation);
			foreach_(const int iPrereqBonus, kCorp.getConsumedBonuses())
			{
				if ((int)eBonus == iPrereqBonus)
				{
					iValue += (50 * (kCorp.getFlatYield(YIELD_FOOD, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;
					iValue += (50 * (kCorp.getFlatYield(YIELD_PRODUCTION, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;
					iValue += (30 * (kCorp.getFlatYield(YIELD_COMMERCE, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;

					iValue += (30 * (kCorp.getFlatCommerce(COMMERCE_GOLD, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;
					iValue += (30 * (kCorp.getFlatCommerce(COMMERCE_RESEARCH, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;
					iValue += (12 * (kCorp.getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;
					iValue += (20 * (kCorp.getFlatCommerce(COMMERCE_ESPIONAGE, CASC_SCOPE_CITY) / 100) * iCorpCount) / iCityCount;

					//Disabled since you can't found/spread a corp unless there is already a bonus,
					//and that bonus will provide the entirity of the bonusProduced benefit.

					/*if (NO_BONUS != kCorp.getBonusProduced())
					{
						if (getNumAvailableBonuses((BonusTypes)kCorp.getBonusProduced()) == 0)
						{
							iBonusValue += (1000 * iCorpCount * AI_baseBonusVal((BonusTypes)kCorp.getBonusProduced())) / (10 * iCityCount);
					}
					}*/
				}
			}
		}
	}

	iValue /= 100;	//percent
	iValue /= 10;	//match AI_baseBonusVal

	return iValue;
}


int CvPlayerAI::AI_bonusTradeVal(BonusTypes eBonus, PlayerTypes ePlayer, int iChange) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	int iValue = AI_bonusVal(eBonus, iChange, true);

	iValue *= 30 * (std::min(getNumCities(), GET_PLAYER(ePlayer).getNumCities()) + 3);
	iValue /= 100;

	iValue = getModifiedIntValue(iValue, GC.getBonusInfo(eBonus).getAITradeModifier());

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()) && !GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isCapitulated())
	{
		iValue /= 2;
	}
	return iValue * getTreatyLength();
}


DenialTypes CvPlayerAI::AI_bonusTrade(BonusTypes eBonus, PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (isHumanPlayer() && GET_PLAYER(ePlayer).isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getNumAvailableBonuses(eBonus) > 0 && GET_PLAYER(ePlayer).AI_corporationBonusVal(eBonus) <= 0)
	{
		return (GET_PLAYER(ePlayer).isHumanPlayer() ? DENIAL_JOKING : DENIAL_NO_GAIN);
	}

	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).AI_getWorstEnemy() == GET_PLAYER(ePlayer).getTeam())
	{
		return DENIAL_WORST_ENEMY;
	}

	if (AI_corporationBonusVal(eBonus) > 0)
	{
		return DENIAL_JOKING;
	}

	bool bStrategic = false;

	// Disregard obsolete units
	const CvCity* pCapitalCity = getCapitalCity();
	if (pCapitalCity == NULL)
	{
		return NO_DENIAL;
	}
	// WHICH UNITS NEED THIS BONUS? -- the bonus's own load-populated requires-reverse index answers it
	// directly ([DEC-one-reverse-view]), the same read the building half below makes. The capital then
	// answers "can I train it THERE" as a bare O(1) tri-state fetch (enabler.md par.8), so neither half
	// scans: walking the trainable set and asking each unit the reverse question was the whole-database
	// scan the reverse index exists to delete.
	std::set<int> dependentUnits;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus), EDGEF_REQUIRED_BY, EDGEB_UNITS, dependentUnits);

	for (std::set<int>::const_iterator itDependent = dependentUnits.begin(); itDependent != dependentUnits.end(); ++itDependent)
	{
		const UnitTypes eLoopUnit = static_cast<UnitTypes>(*itDependent);

		if (EnablerKernel::everAvailable(EDGEB_UNITS, (int)eLoopUnit)
		&& pCapitalCity->getUnitAvailability(eLoopUnit) == EnablerDomain::STATE_LISTED)
		{
			bStrategic = true;
			break;
		}
	}

	if (!bStrategic)
	{
		// WHICH BUILDINGS NEED THIS BONUS? -- the bonus's own load-populated requires-reverse index answers it
		// directly ([DEC-one-reverse-view]); asking every building the reverse question was the whole-database
		// scan enabler.md §6 exists to delete. Membership IS "its `requires` references this bonus", so only the
		// availability predicates remain.
		std::set<int> dependentBuildings;
		EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus), EDGEF_REQUIRED_BY, EDGEB_BUILDINGS, dependentBuildings);

		for (std::set<int>::const_iterator itDependent = dependentBuildings.begin(); itDependent != dependentBuildings.end(); ++itDependent)
		{
			const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(*itDependent);

			if (!GET_TEAM(getTeam()).isObsoleteBuilding(eLoopBuilding)
			&& EnablerKernel::everAvailable(EDGEB_BUILDINGS, (int)eLoopBuilding))
			{
				bStrategic = true;
				break;
			}
		}
	}
	// XXX marble and stone???

	const AttitudeTypes eAttitude = AI_getAttitude(ePlayer);

	if (bStrategic)
	{
		//If we are planning war, don't sell our resources!
		if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS) && GET_TEAM(getTeam()).hasWarPlan(true))
		{
			return DENIAL_MYSTERY;
		}

		if (eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_STRATEGIC_BONUS))
		{
			return DENIAL_ATTITUDE;
		}
	}

	if (GC.getBonusInfo(eBonus).getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_EMPIRE) / 100 > 0
	&& eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_HAPPINESS_BONUS))
	{
		return DENIAL_ATTITUDE;
	}

	if (GC.getBonusInfo(eBonus).getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) / 100 > 0
	&& eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_HEALTH_BONUS))
	{
		return DENIAL_ATTITUDE;
	}

	return NO_DENIAL;
}


int CvPlayerAI::AI_cityTradeVal(CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pCity->getOwner() != getID());

	int iValue = 500;
	//consider infrastructure
	{
		foreach_(const BuildingTypes eType, pCity->getHasBuildings())
		{
			if (isWorldWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 3;
			}
			else if (isLimitedWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 5;
			}
			else
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 10;
			}
		}
	}
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHasReligion((ReligionTypes)iI) && getStateReligion() == iI)
		{
			iValue += 100;
			if (pCity->isHolyCity((ReligionTypes)iI))
			{
				iValue += 500;
			}
			break;
		}
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (pCity->isHasCorporation((CorporationTypes)iI))
		{
			iValue += AI_corporationValue((CorporationTypes)iI, pCity) / 25;
			if (pCity->isHeadquarters((CorporationTypes)iI))
			{
				iValue += AI_corporationTradeVal((CorporationTypes)iI);
			}
		}
	}

	iValue += (pCity->getPopulation() * 50);

	iValue += (pCity->getCultureLevel() * 200);

	iValue += (((GC.getGame().getElapsedGameTurns() + 100) * 4) * pCity->plot()->calculateCulturePercent(pCity->getOwner())) / 400;

	foreach_(const CvPlot * pLoopPlot, pCity->plots())
	{
		if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
		{
			iValue += (AI_bonusVal(pLoopPlot->getBonusType(getTeam())) * 10);
		}
	}

	//	Add in a multiple of what it produces each turn
	int aiCityCommerces[NUM_COMMERCE_TYPES];
	pCity->getCommerces(aiCityCommerces);
	int iCommercePerTurn = aiCityCommerces[COMMERCE_GOLD] / 100 + aiCityCommerces[COMMERCE_RESEARCH] / 100 + aiCityCommerces[COMMERCE_CULTURE] / 100 + aiCityCommerces[COMMERCE_ESPIONAGE] / 100;
	iValue += 6 * iCommercePerTurn;

	//	Don't count food - it doesn't contribute globally to the civ so population is a proxy
	// Weighted against whole-count terms in the same score, so the rate reduces at this use.
	int aiRealizedYields[NUM_YIELD_TYPES];
	pCity->getYields(aiRealizedYields);
	int iYieldPerTurn = aiRealizedYields[YIELD_PRODUCTION] / 100;
	iValue += 12 * iYieldPerTurn;

	if (!(pCity->isEverOwned(getID())))
	{
		iValue *= 3;
		iValue /= 2;
	}
	//in danger
	if (pCity->AI_isDanger() && !pCity->AI_isDefended(strengthOfBestUnitAI(DOMAIN_LAND, UNITAI_CITY_DEFENSE)))
	{
		iValue *= 2;
		iValue /= 3;
	}
	//unstable
	if (pCity->getRevolutionIndex() > 1000)
	{
		iValue *= 2;
		iValue /= 3;
	}
	//colony
	if (!GC.getMap().getArea(pCity->getArea())->isHomeArea(GET_PLAYER(pCity->getOwner()).getID()))
	{
		iValue *= 4;
		iValue /= 5;
	}
	//This city costs money, and we can't afford it
	pCity->getCommerces(aiCityCommerces);   // refreshed; declared above in this scope
	if (AI_isFinancialTrouble() && (aiCityCommerces[COMMERCE_GOLD] - pCity->getMaintenanceTimes100() < 0))
	{
		iValue /= 2;
	}
	return iValue * 15;
}


int CvPlayerAI::AI_ourCityValue(CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 150;
	//consider infrastructure
	{
		foreach_(const BuildingTypes eType, pCity->getHasBuildings())
		{
			if (isWorldWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 3;
			}
			else if (isLimitedWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 5;
			}
			else
			{
				iValue += GC.getBuildingInfo(eType).getCost() / 10;
			}
		}
	}
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHasReligion((ReligionTypes)iI) && getStateReligion() == iI)
		{
			iValue += 100;
			if (pCity->isHolyCity((ReligionTypes)iI))
			{
				iValue += 500;
			}
			break;
		}
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (pCity->isHasCorporation((CorporationTypes)iI))
		{
			iValue += AI_corporationValue((CorporationTypes)iI, pCity) / 25;
			if (pCity->isHeadquarters((CorporationTypes)iI))
			{
				iValue += AI_corporationTradeVal((CorporationTypes)iI);
			}
		}
	}
	iValue += (pCity->getPopulation() * 50);

	iValue += (pCity->getCultureLevel() * 200);

	iValue += (((GC.getGame().getElapsedGameTurns() + 100) * 4) * pCity->plot()->calculateCulturePercent(pCity->getOwner())) / 400;

	foreach_(const CvPlot * pLoopPlot, pCity->plots())
	{
		if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
		{
			iValue += (AI_bonusVal(pLoopPlot->getBonusType(getTeam())) * 10);
		}
	}

	//in danger
	if (pCity->AI_isDanger() && !pCity->AI_isDefended(strengthOfBestUnitAI(DOMAIN_LAND, UNITAI_CITY_DEFENSE))) {
		iValue *= 2;
		iValue /= 3;
	}
	//unstable
	if (pCity->getRevolutionIndex() > 1000) {
		iValue *= 2;
		iValue /= 3;
	}
	//colony
	if (!GC.getMap().getArea(pCity->getArea())->isHomeArea(getID())) {
		iValue *= 4;
		iValue /= 5;
	}
	//This city is costing us money, and we can't afford it
	int aiCityCommerces[NUM_COMMERCE_TYPES];
	pCity->getCommerces(aiCityCommerces);
	if (AI_isFinancialTrouble() && (aiCityCommerces[COMMERCE_GOLD] - pCity->getMaintenanceTimes100() < 0)) {
		iValue /= 2;
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_cityTrade(CvCity* pCity, PlayerTypes ePlayer) const
{
	FAssert(pCity->getOwner() == getID()); // I can only sell cities that I own.

	if (pCity->getLiberationPlayer(false) == ePlayer)
	{
		return NO_DENIAL;
	}

	if (pCity->getID() == getCapitalCity()->getID())
	{
		return DENIAL_JOKING; // Toffer - Hmm, maybe this should be before the liberation "No Denial"?
	}

	if (isHumanPlayer() || atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).AI_isFinancialTrouble())
	{
		return DENIAL_MYSTERY;
	}
	{
		const CvCity* pNearestCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), ePlayer, NO_TEAM, true, false, NO_TEAM, NO_DIRECTION, pCity);

		if (pNearestCity == NULL || plotDistance(pCity->getX(), pCity->getY(), pNearestCity->getX(), pNearestCity->getY()) > 18)
		{
			return DENIAL_NO_GAIN;
		}
	}

	if (getNumCities() > 1 && AI_ourCityValue(pCity) < 600 * getCurrentEra())
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() != getTeam())
	{
		return DENIAL_NEVER;
	}

	if (pCity->calculateCulturePercent(getID()) > 50)
	{
		return DENIAL_TOO_MUCH;
	}
	return NO_DENIAL;
}


int CvPlayerAI::AI_stopTradingTradeVal(TeamTypes eTradeTeam, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_PLAYER(ePlayer).getTeam() != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(eTradeTeam != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_TEAM(eTradeTeam).isAlive(), "GET_TEAM(eWarTeam).isAlive is expected to be true");
	FAssertMsg(!atWar(eTradeTeam, GET_PLAYER(ePlayer).getTeam()), "eTeam should be at peace with eWarTeam");

	int iValue = 50 + GET_TEAM(eTradeTeam).getNumCities() * 5 + GC.getGame().getGameTurn() / 2;

	switch (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getAttitude(eTradeTeam))
	{
		case ATTITUDE_FURIOUS: break;
		case ATTITUDE_ANNOYED:
		{
			iValue *= 5; iValue /= 4; break;
		}
		case ATTITUDE_CAUTIOUS:
		{
			iValue *= 3; iValue /= 2; break;
		}
		case ATTITUDE_PLEASED:
		{
			iValue *= 2; break;
			break;
		}
		case ATTITUDE_FRIENDLY:
		{
			iValue *= 3; break;
			break;
		}
		default: FErrorMsg("error");
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isOpenBorders(eTradeTeam))
	{
		iValue *= 2;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isDefensivePact(eTradeTeam))
	{
		iValue *= 3;
	}

	foreach_(CvDeal & kLoopDeal, GC.getGame().deals())
	{
		if (kLoopDeal.isCancelable(getID()) && !kLoopDeal.isPeaceDeal())
		{
			if (GET_PLAYER(kLoopDeal.getFirstPlayer()).getTeam() == GET_PLAYER(ePlayer).getTeam())
			{
				if (kLoopDeal.getLengthSecondTrades() > 0)
				{
					iValue += (GET_PLAYER(kLoopDeal.getFirstPlayer()).AI_dealVal(kLoopDeal.getSecondPlayer(), kLoopDeal.getSecondTrades()) * ((kLoopDeal.getLengthFirstTrades() == 0) ? 2 : 1));
				}
			}

			if (GET_PLAYER(kLoopDeal.getSecondPlayer()).getTeam() == GET_PLAYER(ePlayer).getTeam())
			{
				if (kLoopDeal.getLengthFirstTrades() > 0)
				{
					iValue += (GET_PLAYER(kLoopDeal.getSecondPlayer()).AI_dealVal(kLoopDeal.getFirstPlayer(), kLoopDeal.getFirstTrades()) * ((kLoopDeal.getLengthSecondTrades() == 0) ? 2 : 1));
				}
			}
		}
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDIPLOMACY_VALUE_REMAINDER());

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDIPLOMACY_VALUE_REMAINDER());
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_stopTradingTrade(TeamTypes eTradeTeam, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_PLAYER(ePlayer).getTeam() != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(eTradeTeam != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_TEAM(eTradeTeam).isAlive(), "GET_TEAM(eTradeTeam).isAlive is expected to be true");
	FAssertMsg(!atWar(getTeam(), eTradeTeam), "should be at peace with eTradeTeam");

	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(eTradeTeam))
	{
		return DENIAL_POWER_THEM;
	}

	const AttitudeTypes eAttitude = GET_TEAM(getTeam()).AI_getAttitude(GET_PLAYER(ePlayer).getTeam());

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam())
			{
				if (eAttitude <= GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_STOP_TRADING))
				{
					return DENIAL_ATTITUDE;
				}
			}
		}
	}

	const AttitudeTypes eAttitudeThem = GET_TEAM(getTeam()).AI_getAttitude(eTradeTeam);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam())
			{
				if (eAttitudeThem > GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_STOP_TRADING_THEM))
				{
					return DENIAL_ATTITUDE_THEM;
				}
			}
		}
	}

	if (GET_TEAM(getTeam()).isOpenBorders(eTradeTeam) && GET_TEAM(getTeam()).hasWarPlan(true))
	{
		return DENIAL_MYSTERY;
	}

	return NO_DENIAL;
}


int CvPlayerAI::AI_civicTradeVal(CivicTypes eCivic, PlayerTypes ePlayer) const
{
	int iValue = (2 * (getTotalPopulation() + GET_PLAYER(ePlayer).getTotalPopulation())); // XXX

	const CivicTypes eBestCivic = GET_PLAYER(ePlayer).AI_bestCivic((CivicOptionTypes)(GC.getCivicInfo(eCivic).getCivicOption()));

	if (eBestCivic != NO_CIVIC && eBestCivic != eCivic)
	{
		iValue += std::max(0, ((GET_PLAYER(ePlayer).AI_civicValue(eBestCivic) - GET_PLAYER(ePlayer).AI_civicValue(eCivic)) * 2));
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDIPLOMACY_VALUE_REMAINDER());

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDIPLOMACY_VALUE_REMAINDER());
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_civicTrade(CivicTypes eCivic, PlayerTypes ePlayer) const
{
	if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
	{
		if (GET_TEAM(getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
		{
			return NO_DENIAL;
		}
	}
	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	// Would the proposed civic leave us less happy than the one it replaces? The civic's anger is authored as
	// NEGATIVE happiness (curator ruling 12: the legacy percent-anger is a happiness deposit per 100 city
	// population), so the comparison reads the two civics' own wellbeing rather than a player-side accumulator.
	const CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOption());
	if (eCurrentCivic != NO_CIVIC
	&& GC.getCivicInfo(eCurrentCivic).getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_EMPIRE)
	 > GC.getCivicInfo(eCivic).getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_EMPIRE))
	{
		return DENIAL_ANGER_CIVIC;
	}

	CivicTypes eFavoriteCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();
	if (eFavoriteCivic != NO_CIVIC
	&& isCivic(eFavoriteCivic)
	&& GC.getCivicInfo(eCivic).getCivicOption() == GC.getCivicInfo(eFavoriteCivic).getCivicOption())
	{
		return DENIAL_FAVORITE_CIVIC;
	}

	if (GC.getCivilizationInfo(getCivilizationType()).getInitialCivic((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOption()) == eCivic)
	{
		return DENIAL_JOKING;
	}

	if (AI_getAttitude(ePlayer) <= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_ADOPT_CIVIC))
	{
		return DENIAL_ATTITUDE;
	}

	// Don't change civics when at war (Anarchy is bad)
	if (GET_TEAM(getTeam()).isAtWar(false))
	{
		return DENIAL_JOKING;
	}
	return NO_DENIAL;
}


int CvPlayerAI::AI_religionTradeVal(ReligionTypes eReligion, PlayerTypes ePlayer) const
{
	int iValue = (3 * (getTotalPopulation() + GET_PLAYER(ePlayer).getTotalPopulation())); // XXX

	const ReligionTypes eBestReligion = GET_PLAYER(ePlayer).AI_bestReligion();

	if (eBestReligion != NO_RELIGION && eBestReligion != eReligion)
	{
		iValue += std::max(0, (GET_PLAYER(ePlayer).AI_religionValue(eBestReligion) - GET_PLAYER(ePlayer).AI_religionValue(eReligion)));
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDIPLOMACY_VALUE_REMAINDER());

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDIPLOMACY_VALUE_REMAINDER());
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_religionTrade(ReligionTypes eReligion, PlayerTypes ePlayer) const
{
	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	if (getStateReligion() != NO_RELIGION)
	{
		if (getHasReligionCount(eReligion) < std::min((getHasReligionCount(getStateReligion()) - 1), (getNumCities() / 2)))
		{
			return DENIAL_MINORITY_RELIGION;
		}
	}

	if (AI_getAttitude(ePlayer) <= GC.getLeaderHeadInfo(getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_CONVERT_RELIGION))
	{
		return DENIAL_ATTITUDE;
	}

	// Don't change religions when at war (Anarchy is bad)
	if (GET_TEAM(getTeam()).isAtWar(false))
	{
		return DENIAL_NO_GAIN;
	}
	return NO_DENIAL;
}

int CvPlayerAI::AI_unitImpassableCount(UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	const CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);
	// The passable-tech map is keyed by the substrate id; a substrate with no entry has no tech that opens
	// it, which is the same verdict the old sentinel carried.
	const std::map<int, int>& kTerrainPassable = kUnitInfo.getTerrainPassableTechs();
	foreach_(const int iImpassableTerrain, kUnitInfo.getTerrainImpassable())
	{
		const std::map<int, int>::const_iterator itTech = kTerrainPassable.find(iImpassableTerrain);
		const TechTypes eTech = (itTech == kTerrainPassable.end()) ? NO_TECH : (TechTypes)itTech->second;
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
		{
			iCount++;
		}
	}
	const std::map<int, int>& kFeaturePassable = kUnitInfo.getFeaturePassableTechs();
	foreach_(const int iImpassableFeature, kUnitInfo.getFeatureImpassable())
	{
		const std::map<int, int>::const_iterator itTech = kFeaturePassable.find(iImpassableFeature);
		const TechTypes eTech = (itTech == kFeaturePassable.end()) ? NO_TECH : (TechTypes)itTech->second;
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
		{
			iCount++;
		}
	}

	return iCount;
}

int CvPlayerAI::AI_unitHealerValue(UnitTypes eUnit, UnitCombatTypes eUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	const CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);

	// The unit's OWN keyed heal rows -- the handful of combat classes it authored, collected ONCE instead of
	// re-fetching the info per index. The MECHANIC below is unchanged; only the feed moved.
	// ⚠ Both amounts are ×100 and reduce to whole heal points HERE. That is the whole risk in this carve-out:
	// heal must come out neither lost nor ×100 ([roadmap.md] -- the heal acceptance bar is two-sided).
	std::vector<HealByUnitCombat> healRows;
	InfoValuation::collectHealByUnitCombat(kUnitInfo.getModifiers(), healRows);
	const int iNumHealUnitCombatTypes = (int)healRows.size();

	if (eUnitCombat != NO_UNITCOMBAT)
	{
		for (int iRow = 0; iRow < iNumHealUnitCombatTypes; iRow++)
		{
			if (healRows[iRow].iUnitCombat == (int)eUnitCombat)
			{
				iValue += healRows[iRow].iHeal / 100;
				iValue += healRows[iRow].iAdjacentHeal / 100;
			}
		}
	}
	else if (iNumHealUnitCombatTypes > 0)
	{
		int iAverage = 0;
		for (int iRow = 0; iRow < iNumHealUnitCombatTypes; iRow++)
		{
			iAverage += healRows[iRow].iHeal / 100;
			iAverage += healRows[iRow].iAdjacentHeal / 100;
		}
		iAverage /= iNumHealUnitCombatTypes;
		iValue += iAverage;
		iValue += iNumHealUnitCombatTypes;
	}

	iValue *= kUnitInfo.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100;

	return iValue;
}

int CvPlayerAI::AI_unitPropertyValue(UnitTypes eUnit, PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	const CvPropertyManipulators* propertyManipulators = GC.getUnitInfo(eUnit).getPropertyManipulators();

	if (propertyManipulators)
	{
		int iValue = 0;
		foreach_(const CvPropertySource * pSource, propertyManipulators->getSources())
		{
			if (pSource->getType() == PROPERTYSOURCE_CONSTANT && (eProperty == NO_PROPERTY || pSource->getProperty() == eProperty))
			{
				// Value is crudely, just the AIweight of that property times the source size
				iValue += GC.getPropertyInfo(pSource->getProperty()).getAIWeight() * ((const CvPropertySourceConstant*)pSource)->getAmountPerTurn(getGameObject());
			}
		}
		return iValue;
	}
	return 0;
}

int CvPlayerAI::AI_unitValue(UnitTypes eUnit, UnitAITypes eUnitAI, const CvArea* pArea, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAI);

	const CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);

	if (!kUnitInfo.getDomain() == AI_unitAIDomainType(eUnitAI) && eUnitAI != UNITAI_ICBM)
	{
		return 0;
	}

	if (kUnitInfo.hasNotUnitAI(eUnitAI) && (criteria == NULL || !criteria->m_bIgnoreNotUnitAIs))
	{
		return 0;
	}

	// Special settler rule
	if (eUnitAI != UNITAI_SETTLE && kUnitInfo.hasSkill(CLS_SKILL_FOUND))
	{
		return 0;
	}

	int iGeneralPropertyValue = AI_unitPropertyValue(eUnit);
	bool bisNegativePropertyUnit = (iGeneralPropertyValue < 0);
	bool bisPositivePropertyUnit = (iGeneralPropertyValue > 0);
	bool bUndefinedValid = false, bValid = false;

	// The unit's OWN keyed vs-unitcombat entries -- the handful it authored, walked ONCE for the whole function
	// instead of asking every unitcombat id whether this unit deposits onto it (the own-data inversion,
	// [pedia-read-map] finding 2). ⚠ `combat.unit.unitCombat.{UNITCOMBAT_X}.percent` carries no member segment,
	// which compiles to kind 0 -- the scope-wide COMBAT_AMOUNT -- NOT to an unkinded entry; the collect form
	// matches the kind exactly, so anything else here silently reads nothing.
	std::vector<std::pair<int, int> > vsUnitCombat;
	InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT,
		COMBAT_AMOUNT, vsUnitCombat);

	//if (eUnitAI != UNITAI_PROPERTY_CONTROL && eUnitAI != UNITAI_SEE_INVISIBLE && bisPositivePropertyUnit)
	//{
	//	return 0;
	//}

	switch (eUnitAI)
	{
		case UNITAI_UNKNOWN:
		{
			bUndefinedValid = true;
			break;
		}
		case UNITAI_SUBDUED_ANIMAL:
		{
			bValid = true;
			break;
		}
		case UNITAI_HUNTER:
		{
			if (kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				break; // Hard disqualification for hunters.
			}
			// Fall through to next case.
		}
		case UNITAI_HUNTER_ESCORT:
		{
			if (!bisNegativePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) > 0)
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_BARB_CRIMINAL: break;
		case UNITAI_ANIMAL:
		{
			if (isAnimal())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_SETTLE:
		{
			if (kUnitInfo.hasSkill(CLS_SKILL_FOUND))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_WORKER:
		{
			if ((int)kUnitInfo.getBuilds().size() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ESCORT:
		{
			if (!bisNegativePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) > 0)//Note: add a hero filter - a lot of them are being trained for this.
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_ATTACK:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_CITY:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE) && !kUnitInfo.hasSkill(CLS_SKILL_NO_CAPTURE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_COLLATERAL:
		{
			// #410: breakdown chance no longer qualifies -- it is a city-assault death
			// rider, not stack-softening capability; counting it made breakdown-only
			// siege (battering rams etc.) a valid COLLATERAL pick it could never play.
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE)
			&& kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PILLAGE:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_RESERVE:
		{
			if (!bisNegativePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PILLAGE_COUNTER:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_COUNTER:
		{
			if (!bisNegativePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
			{
				if (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0 || (int)kUnitInfo.getTargetUnits().size() > 0)
				{
					bValid = true;
					break;
				}
				std::vector<std::pair<int, int> > vsUnitAttack1;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, COMBAT_ATTACK, vsUnitAttack1);
				foreach_(const STD_PAIR(int, int)& modifier, vsUnitAttack1)
				{
					if (modifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
				// Both halves are the unit's OWN authored data, so the question is asked of what it carries rather
				// than of every unitcombat id.
				foreach_(const STD_PAIR(int, int)& combatModifier, vsUnitCombat)
				{
					if (combatModifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
				if (!bValid && !kUnitInfo.getTargetUnitCombats().empty())
				{
					bValid = true;
				}
				for (int iI = 0; !bValid && iI < GC.getNumUnitInfos(); iI++)
				{
					if (GC.getUnitInfo((UnitTypes)iI).isDefendAgainstUnit(eUnit))
					{
						bValid = true;
						break;
					}

					const int iUnitCombat = kUnitInfo.getCombatClass();
					if (NO_UNITCOMBAT != iUnitCombat && GC.getUnitInfo((UnitTypes)iI).hasDefenderUnitCombat(iUnitCombat))
					{
						bValid = true;
						break;
					}
				}
			}
			break;
		}
		case UNITAI_HEALER:
		case UNITAI_HEALER_SEA:
		{
			if (!bisNegativePropertyUnit && AI_unitHealerValue(eUnit) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PROPERTY_CONTROL:
		case UNITAI_PROPERTY_CONTROL_SEA:
		{
			bValid = bisPositivePropertyUnit;
			break;
		}
		case UNITAI_INVESTIGATOR:
		{
			if ((kUnitInfo.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100) > 0 && !bisNegativePropertyUnit)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_INFILTRATOR:
		{
			if (kUnitInfo.hasSkill(CLS_SKILL_BLEND_INTO_CITY))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_SEE_INVISIBLE:
		case UNITAI_SEE_INVISIBLE_SEA:
		{
			if (bisNegativePropertyUnit)
			{
				break; // Not a valid unit for this role
			}
			const InvisibleTypes eVisibilityRequested = criteria ? criteria->m_eVisibility : NO_INVISIBLE;
			// ADMISSION: a named method must be answered; an unnamed one asks only that it detects SOMETHING.
			// One read either way -- the option used to pick between two tables and there is one surface now.
			if (eVisibilityRequested != NO_INVISIBLE)
			{
				if (kUnitInfo.getHideAndSeek().detectionAgainst(GC.getMethodSkill(eVisibilityRequested)) <= 0)
				{
					break; // Not a valid unit for this role
				}
			}
			else if (kUnitInfo.getHideAndSeek().detection.empty())
			{
				break; // Not a valid unit for this role
			}

			bValid = true;
			break;
		}
		case UNITAI_CITY_DEFENSE:
		{
			if (!bisNegativePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !kUnitInfo.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_CITY_COUNTER:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0 && !bisNegativePropertyUnit && !kUnitInfo.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
			{
				if (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0)
				{
					bValid = true;
					break;
				}

				std::vector<std::pair<int, int> > vsUnitDefense1;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, COMBAT_DEFENSE, vsUnitDefense1);
				foreach_(const STD_PAIR(int, int)& modifier, vsUnitDefense1)
				{
					if (modifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
				foreach_(const STD_PAIR(int, int)& combatModifier, vsUnitCombat)
				{
					if (combatModifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
			}
			break;
		}
		case UNITAI_CITY_SPECIAL:
		{
			if (!bisNegativePropertyUnit)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PARADROP:
		{
			if ((kUnitInfo.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_EXPLORE:
		{
			if (!bisPositivePropertyUnit && (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0)
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_MISSIONARY:
		{
			if (pArea)
			{
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (kUnitInfo.getReligionSpreadStrength((ReligionTypes)iI) > 0)
					{
						const int iNeededMissionaries = AI_neededMissionaries(pArea, (ReligionTypes)iI);

						if (iNeededMissionaries > 0 && iNeededMissionaries > countReligionSpreadUnits(pArea, (ReligionTypes)iI))
						{
							bValid = true;
							break;
						}
					}
				}
				for (int iI = 0; !bValid && iI < GC.getNumCorporationInfos(); iI++)
				{
					if (kUnitInfo.getCorporationSpreadStrength((CorporationTypes)iI) > 0)
					{
						const int iNeededMissionaries = AI_neededExecutives(pArea, (CorporationTypes)iI);

						if (iNeededMissionaries > 0 && iNeededMissionaries > countCorporationSpreadUnits(pArea, (CorporationTypes)iI))
						{
							bValid = true;
							break;
						}
					}
				}
			}
			break;
		}
		case UNITAI_ICBM:
		{
			if ((kUnitInfo.getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) / 100) != -1)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_WORKER_SEA:
		{
			if ((int)kUnitInfo.getBuilds().size() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_SEA:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_RESERVE_SEA:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ESCORT_SEA:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_EXPLORE_SEA:
		{
			if ((kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ASSAULT_SEA:
		case UNITAI_SETTLER_SEA:
		{
			if (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 > 0 && kUnitInfo.getSpecialCargo() == NO_SPECIALUNIT)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_MISSIONARY_SEA:
		case UNITAI_SPY_SEA:
		case UNITAI_CARRIER_SEA:
		case UNITAI_MISSILE_CARRIER_SEA:
		{
			// A special-cargo transport is valid for these carrier roles (mirrors the general-cargo
			// ASSAULT_SEA/SETTLER_SEA case above). This previously gated on CvSpecialUnitInfo's
			// CarrierUnitAITypes, but that data never loaded (loader/XML tag mismatch) and the loop
			// passed eUnitAI instead of its own counter — so it was always false (dead). See #194.
			if (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 > 0 && kUnitInfo.getSpecialCargo() != NO_SPECIALUNIT)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PIRATE_SEA:
		{
			if (kUnitInfo.hasSkill(CLS_SKILL_ALWAYS_HOSTILE) && kUnitInfo.hasSkill(CLS_SKILL_HIDDEN_NATIONALITY))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && !kUnitInfo.hasSkill(CLS_SKILL_SUICIDE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_DEFENSE_AIR:
		{
			if (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_CARRIER_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_MISSILE_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && kUnitInfo.hasSkill(CLS_SKILL_SUICIDE))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_CITY_LEMMING:
		case UNITAI_PROPHET:
		case UNITAI_ARTIST:
		case UNITAI_SCIENTIST:
		case UNITAI_GENERAL:
		case UNITAI_GREAT_HUNTER:
		case UNITAI_GREAT_ADMIRAL:
		case UNITAI_MERCHANT:
		case UNITAI_ENGINEER:
		case UNITAI_SPY:
		{
			break;
		}
		default: FErrorMsg("error");
	}

	if (!bValid || !bUndefinedValid && !kUnitInfo.hasUnitAI(eUnitAI))
	{
		return 0;
	}

	PropertyTypes ePropertyRequested = (criteria == NULL ? NO_PROPERTY : criteria->m_eProperty);
	UnitCombatTypes eHealCombatClassRequested = (criteria == NULL ? NO_UNITCOMBAT : criteria->m_eHealUnitCombat);

	const int iCombatValue = GC.getGame().AI_combatValue(eUnit);
	const int iHealerValue = AI_unitHealerValue(eUnit, eHealCombatClassRequested);
	const int iPropertyValue = AI_unitPropertyValue(eUnit, ePropertyRequested);

	int iValue;

	if (ePropertyRequested != NO_PROPERTY && iPropertyValue <= 0)
	{
		iValue = 0;
	}
	else if (eHealCombatClassRequested != NO_UNITCOMBAT && iHealerValue <= 0)
	{
		iValue = 0;
	}
	else
	{
		iValue = 1 + kUnitInfo.getAIWeight();

		switch (eUnitAI)
		{
			case UNITAI_ANIMAL:
			case UNITAI_SUBDUED_ANIMAL:
			case UNITAI_BARB_CRIMINAL:
			{
				break;
			}
			case UNITAI_SETTLE:
			{
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * 100);
				break;
			}
			case UNITAI_WORKER:
			{
				iValue += (int)kUnitInfo.getBuilds().size();
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100)-1) * iValue / 2;
				//	Scale by how fast a worker works - the extra '4' is a fudge factor
				//	to make worker values (somewhat) comparable to military unit values
				//	now that we have workers that can upgrade to military and we need to
				//	compare (at least very roughly)
				iValue = iValue * kUnitInfo.getWorkRate() / 400;
				break;
			}
			case UNITAI_ATTACK:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}


				iValue += iCombatValue;
				{
					const int iFastMoverMultiplier = AI_isDoStrategy(AI_STRATEGY_FASTMOVERS) ? 3 : 1;
					iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) * iFastMoverMultiplier) / 3);
				}
				iValue += ((iCombatValue * kUnitInfo.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) / 100);

				if (kUnitInfo.getCombatLimit() < 100)
				{
					iValue -= (iCombatValue * (125 - kUnitInfo.getCombatLimit())) / 100;
				}
				//	Also useful if attack stacks can make use of defensive terrain, though
				//	its not a huge factor since we can assume some defensive units will be
				//	along for the ride
				if (kUnitInfo.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
				{
					iValue *= 4;
					iValue /= 5;
				}

				//	Combat modifiers matter for attack units
				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += ((iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first)) / 10000);
				}

				break;
			}
			case UNITAI_ATTACK_CITY:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}

				int iTempValue = ((iCombatValue * iCombatValue) / 75) + (iCombatValue / 2);
				iValue += iTempValue;
				if (kUnitInfo.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
				{
					iValue -= iTempValue / 4;
				}

				if ((kUnitInfo.hasSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || kUnitInfo.hasSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE)))
				{
					iValue += (iTempValue * 8) / 100;
				}

				bool bHasBombardValue = false;
				bool bNoBombardValue = true;
				// #410: breakdown chance is deliberately NOT bombard potency. It only
				// fires while the unit personally assaults the garrison (usually fatal,
				// approximately once) -- valuing it like an aimable, recurring bombard
				// rate drove the AI to overbuild the breakdown-only siege line and march
				// "siege capability" it could never schedule. Breakdown-only units are
				// now valued as the plain attackers they actually are.
				// A presence test, so the ×100 flat needs no reduction here.
				if (kUnitInfo.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) > 0
				|| (kUnitInfo.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) > 0
					&& kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) > 0))
				{
					// Army composition needs to scale with army size, bombard unit potency

					//modified AI_calculateTotalBombard(DOMAIN_LAND) code
					int iTotalBombard = 0;
					int iThisBombard = kUnitInfo.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);
					int iSiegeUnits = 0;
					int iSiegeImmune = 0;
					int iTotalSiegeMaxUnits = 0;
					bNoBombardValue = false;
					bHasBombardValue = true;

					for (int iJ = 0; iJ < GC.getNumUnitInfos(); iJ++)
					{
						UnitTypes eLoopUnit = (UnitTypes)iJ;
						if (GC.getUnitInfo(eLoopUnit).getDomain() == DOMAIN_LAND)
						{
							int iUnitCount = getUnitCount(eLoopUnit);
							int iBombardRate = GC.getUnitInfo(eLoopUnit).getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);

							if (iBombardRate > 0)
							{
								iTotalBombard += ((iBombardRate * iUnitCount * ((GC.getUnitInfo(eLoopUnit).hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE)) ? 3 : 2)) / 2);
							}

							int iBombRate = GC.getUnitInfo(eLoopUnit).getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100;
							if (iBombRate > 0)
							{
								iThisBombard += iBombRate;
								iTotalBombard += iBombRate * iUnitCount;
							}

							int iCollateralDamageMaxUnits = GC.getUnitInfo(eLoopUnit).getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100;
							if (iCollateralDamageMaxUnits > 0 && GC.getUnitInfo(eLoopUnit).getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) > 0)
							{
								iTotalSiegeMaxUnits += iCollateralDamageMaxUnits * iUnitCount;
								iSiegeUnits += iUnitCount;
							}
							else if (GC.getUnitInfo(eLoopUnit).hasSkill(CLS_SKILL_COLLATERAL_IMMUNE))
							{
								iSiegeImmune += iUnitCount;
							}
						}
					}

					if (iThisBombard == 0)
					{
						bNoBombardValue = true;
					}
					else if ((100 * iTotalBombard) / (std::max(1, (iThisBombard * AI_totalUnitAIs(UNITAI_ATTACK_CITY)))) >= GC.getDefineINT("BBAI_BOMBARD_ATTACK_CITY_MAX_STACK_FRACTION"))
					{
						//too many bombard units already
						bNoBombardValue = true;
					}

					int iNumOffensiveUnits = AI_totalUnitAIs(UNITAI_ATTACK_CITY) + AI_totalUnitAIs(UNITAI_ATTACK) + AI_totalUnitAIs(UNITAI_COUNTER) / 2;
					int iNumDefensiveUnits = AI_totalUnitAIs(UNITAI_CITY_DEFENSE) + AI_totalUnitAIs(UNITAI_RESERVE) + AI_totalUnitAIs(UNITAI_CITY_COUNTER) / 2 + AI_totalUnitAIs(UNITAI_COLLATERAL) / 2;
					iSiegeUnits += (iSiegeImmune * iNumOffensiveUnits) / std::max(1, iNumOffensiveUnits + iNumDefensiveUnits);

					int iMAX_HIT_POINTS = GC.getMAX_HIT_POINTS();

					// `limit` and `maxUnits` are FLAT kinds, so they arrive ×100 and are reduced to whole units
					// HERE -- both are mixed with plain counts below ([fixed-point-and-scales]: a value consumed
					// as a whole number reduces at its point of use, leaving the arithmetic untouched).
					const int iCollateralLimit = kUnitInfo.getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100;
					const int iCollateralMaxUnits = kUnitInfo.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100;

					int iCollateralDamageMaxUnitsWeight = (100 * (iNumOffensiveUnits - iSiegeUnits)) / std::max(1, iTotalSiegeMaxUnits);
					iCollateralDamageMaxUnitsWeight = std::min(100, iCollateralDamageMaxUnitsWeight);
					//to decrease value further for units with low damage limits:
					int iCollateralDamageLimitWeight = 100 * iMAX_HIT_POINTS - std::max(0, ((iMAX_HIT_POINTS - iCollateralLimit) * (100 - iCollateralDamageMaxUnitsWeight)));
					iCollateralDamageLimitWeight /= iMAX_HIT_POINTS;

					int iCollateralValue = iCombatValue * kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) * GC.getCOLLATERAL_COMBAT_DAMAGE();
					iCollateralValue /= 100;
					iCollateralValue *= std::max(100, (iCollateralMaxUnits * iCollateralDamageMaxUnitsWeight));
					iCollateralValue /= 100;
					iCollateralValue *= iCollateralDamageLimitWeight;
					iCollateralValue /= 100;
					iCollateralValue /= iMAX_HIT_POINTS;
					iValue += iCollateralValue;

					if (!bNoBombardValue && !AI_isDoStrategy(AI_STRATEGY_AIR_BLITZ))
					{
						int iBombardValue = iThisBombard * ((kUnitInfo.hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE) || kUnitInfo.hasSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL)) ? 3 : 2);
						int iAIDesiredBombardFraction = std::max(5, GC.getDefineINT("BBAI_BOMBARD_ATTACK_STACK_FRACTION")); /*default: 15*/
						int iActualBombardFraction = (100 * 2 * iTotalBombard) / (iBombardValue * std::max(1, iNumOffensiveUnits));
						iActualBombardFraction = std::min(100, iActualBombardFraction);

						// K - Mod note : This goal has no dependency on civ size, map size, era, strategy, or anything else that matters
						// a flat goal of 200... This needs to be fixed. For now, I'll just replace it with something rough.
						// But this is a future "todo".
						// int iGoalTotalBombard = 200;
						int iGoalTotalBombard = (getNumCities() + 3) * (getCurrentEra() + 2) * (AI_isDoStrategy(AI_STRATEGY_CRUSH) ? 10 : 5);
						int iTempBombardValue = 0;
						if (iTotalBombard < iGoalTotalBombard) //still less than 200 bombard points
						{
							iTempBombardValue = iBombardValue * (iGoalTotalBombard + 7 * (iGoalTotalBombard - iTotalBombard));
							iTempBombardValue /= iGoalTotalBombard;
							//iTempBombardValue is at most (8 * iBombardValue)
						}
						else
						{
							iTempBombardValue *= iGoalTotalBombard;
							iTempBombardValue /= std::min(2 * iGoalTotalBombard, 2 * iTotalBombard - iGoalTotalBombard);
						}

						if (iActualBombardFraction < iAIDesiredBombardFraction)
						{
							iBombardValue *= (iAIDesiredBombardFraction + 5 * (iAIDesiredBombardFraction - iActualBombardFraction));
							iBombardValue /= iAIDesiredBombardFraction;
							//new iBombardValue is at most (6 * old iBombardValue)
						}
						else
						{
							iBombardValue *= iAIDesiredBombardFraction;
							iBombardValue /= std::max(1, iActualBombardFraction);
						}

						if (iTempBombardValue > iBombardValue)
						{
							iBombardValue = iTempBombardValue;
						}
						iBombardValue = getModifiedIntValue(iBombardValue, GC.getDefineINT("C2C_ROUGH_BOMBARD_VALUE_MODIFIER"));

						iValue += iBombardValue;
					}
				}
				//TB Adjust: If the unit doesn't have any bombard value, it can still be beneficial to have collateral damage (Rhinos for example)
				if (!bHasBombardValue)
				{
					iValue += ((iCombatValue * kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT)) / 200);
				}
				//TB Adjust: If the unit has bombard value(bHasBombardValue) AND the stack still wants bombard units(!bNoBombardValue) (or the unit doesn't have any bombard value anyhow) then basic modifiers apply.
				//This is intended to keep bombarding siege units from evaluating stronger than normal invading units like swordsman for the basic NON-Bombard stack fill needs.
				//Such siege units often can't seal the deal and actually invade the city despite being very necessary for the stack.
				//Before this change we're getting an overbuild of siege units like rams even after the bombard needs are met for the stack.
				if ((bHasBombardValue && !bNoBombardValue) || !bHasBombardValue)
				{
					// Effect army composition to have more collateral/bombard units
					iValue += ((iCombatValue * kUnitInfo.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT)) / 50);
					{
						const int iFastMoverMultiplier = AI_isDoStrategy(AI_STRATEGY_FASTMOVERS) ? 4 : 1;
						iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) * iFastMoverMultiplier) / 4); // K-Mod put in -1 !
					}
					iValue += ((iCombatValue * kUnitInfo.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) / 100);
				}

				break;
			}
			case UNITAI_COLLATERAL:
			{
				iValue += iCombatValue;
				iValue += ((iCombatValue * kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT)) / 50);
				iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100)-1)) / 4);
				iValue += ((iCombatValue * kUnitInfo.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) / 25);
				iValue += ((iCombatValue * kUnitInfo.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT)) / 100);// was -= ???
				break;
			}
			case UNITAI_PILLAGE:
			{
				iValue -= AI_unitPropertyValue(eUnit) / 30;	//	Bad properties are good for pillagers
				iValue += iCombatValue;
				iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1)) /4); //Calvitix Try to limit impact of moves
				break;
			}
			case UNITAI_RESERVE:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += iCombatValue;
				iValue += ((iCombatValue * kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT)) / 200);
				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += ((iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first)) / 12000);
				}
				iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) ) / 4);  //Calvitix  old value /2
				break;
			}
			case UNITAI_PILLAGE_COUNTER:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += iCombatValue;
				iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1)) / 4);  // Calvitix 2
				break;
			}
			case UNITAI_COUNTER:
			{
				//	For now the AI cannot cope with bad prroperty values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += (iCombatValue / 2);
				std::vector<std::pair<int, int> > vsUnitAttack2;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, COMBAT_ATTACK, vsUnitAttack2);
				foreach_(const STD_PAIR(int, int)& modifier, vsUnitAttack2)
				{
					iValue += ((iCombatValue * modifier.second * AI_getUnitWeight((UnitTypes)modifier.first)) / 7500);
					iValue += ((iCombatValue * (kUnitInfo.hasTargetUnit(modifier.first) ? 50 : 0)) / 100);
				}
				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += ((iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first)) / 10000);
				}
				// The TARGETS set is the unit's own authored list, so it is walked rather than asked per id.
				const std::set<int>& kTargetCombats = kUnitInfo.getTargetUnitCombats();
				for (std::set<int>::const_iterator itTarget = kTargetCombats.begin();
					itTarget != kTargetCombats.end(); ++itTarget)
				{
					iValue += ((iCombatValue * 50) / 100);
				}
				for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
				{
					if (GC.getUnitInfo((UnitTypes)iI).isDefendAgainstUnit(eUnit))
					{
						iValue += (50 * iCombatValue) / 100;
					}

					const int iUnitCombat = kUnitInfo.getCombatClass();
					if (NO_UNITCOMBAT != iUnitCombat && GC.getUnitInfo((UnitTypes)iI).hasDefenderUnitCombat(iUnitCombat))
					{
						iValue += (50 * iCombatValue) / 100;
					}
				}

				iValue += ((iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1)) / 4);  //Calvitix /2
				iValue += ((iCombatValue * kUnitInfo.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT)) / 100);

				if (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0)
				{
					int iTempValue = kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT);

					iTempValue *= 25 + std::min(175, GET_TEAM(getTeam()).AI_getRivalAirPower());
					iTempValue /= 100;

					iValue += iTempValue;
				}

				break;
			}
			case UNITAI_CITY_DEFENSE:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += ((iCombatValue * 3) / 2);
				iValue += ((iCombatValue * kUnitInfo.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT)) / 25);
				//	The '30' scaling is empirical based on what seems reasonable for crime fighting units
				// this is causing the AI to select prop control for defense.
				/*iValue += AI_unitPropertyValue(eUnit)/(ePropertyRequested != NO_PROPERTY ? 30 : 60);*/
				//	Combat modifiers matter for defensive units


				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first) / 12000;
				}

				//  ls612: consider that a unit with OnlyDefensive is less useful

				if (kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
				{
					iValue *= 4;
					iValue /= 5;
				}

				break;

			}
			case UNITAI_CITY_COUNTER:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += ((iCombatValue * 3) / 2);
				iValue += ((iCombatValue * kUnitInfo.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT)) / 75);
				//	The '30' scaling is empirical based on what seems reasonable for crime fighting units
				// this is causing the AI to select prop control for defense.
				/*iValue += AI_unitPropertyValue(eUnit)/(ePropertyRequested != NO_PROPERTY ? 30 : 60);*/
				//	Combat modifiers matter for defensive units

				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first) / 6000;
				}
				//  ls612: consider that a unit with OnlyDefensive is less useful

				if (kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
				{
					iValue *= 4;
					iValue /= 5;
				}

				break;
			}
			case UNITAI_HEALER:
			case UNITAI_HEALER_SEA:
			{
				iValue += iHealerValue;
				// Drop through
			}
			case UNITAI_PROPERTY_CONTROL:
			case UNITAI_PROPERTY_CONTROL_SEA:
			case UNITAI_CITY_SPECIAL:
			{
				if (iPropertyValue > 0)
				{
					iValue += iPropertyValue * 10 + iCombatValue;
				}
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				else iValue = 0;

				break;
			}
			case UNITAI_PARADROP:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += (iCombatValue / 2);
				iValue += ((iCombatValue * kUnitInfo.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT)) / 100);
				iValue /= (kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE) ? 2 : 1);
				std::vector<std::pair<int, int> > vsUnitAttack3;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_UNIT, COMBAT_ATTACK, vsUnitAttack3);
				foreach_(const STD_PAIR(int, int)& modifier, vsUnitAttack3)
				{
					iValue += ((iCombatValue * modifier.second * AI_getUnitWeight((UnitTypes)modifier.first)) / 10000);
					iValue += ((iCombatValue * (kUnitInfo.isDefendAgainstUnit(modifier.first) ? 50 : 0)) / 100);
				}
				for (std::vector<std::pair<int, int> >::const_iterator itCombat = vsUnitCombat.begin();
					itCombat != vsUnitCombat.end(); ++itCombat)
				{
					iValue += ((iCombatValue * itCombat->second * AI_getUnitCombatWeight((UnitCombatTypes)itCombat->first)) / 10000);
				}
				// The DEFENDERS set is likewise the unit's own authored list.
				const std::set<int>& kDefenderCombats = kUnitInfo.getDefenderUnitCombats();
				for (std::set<int>::const_iterator itDefender = kDefenderCombats.begin();
					itDefender != kDefenderCombats.end(); ++itDefender)
				{
					iValue += ((iCombatValue * 50) / 100);
				}

				if (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 0)
				{
					int iTempValue = kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT);

					iTempValue *= 25 + std::min(125, GET_TEAM(getTeam()).AI_getRivalAirPower());
					iTempValue /= 50;

					iValue += iTempValue;
				}
				break;
			}
			case UNITAI_EXPLORE:
			{
				iValue += (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * (100 + iCombatValue) / 4;
				if (kUnitInfo.hasSkill(CLS_SKILL_NO_BAD_GOODIES))
				{
					iValue *= 2;
				}
				//need to add vision and terrain factors here.
				break;
			}
			case UNITAI_HUNTER:
			{
				//Calvitix try to limit impact of moves
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) / 2;
				//iValue += iCombatValue * (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100);
				iValue = (
					getModifiedIntValue(
						iValue,
						  kUnitInfo.getCombatModifier(COMBAT_ANIMAL, CASC_SCOPE_UNIT)
						+ InfoValuation::keyedCombat(kUnitInfo.getModifiers(),
							InfoValuation::COMBAT_TARGET_UNITCOMBAT, GC.getUNITCOMBAT_ANIMAL(), COMBAT_AMOUNT)
					)
				);

				if (kUnitInfo.hasCombatClass(GC.getUNITCOMBAT_HUNTER()))
				{
					iValue = iValue * 3/2; // Unique hunter promotions are essential.
				}
				break;
			}
			case UNITAI_HUNTER_ESCORT:
			{
				//Calvitix try to limit impact of moves
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) / 2; //Only extra moves gives +50% bonus

				break;
			}
			case UNITAI_MISSIONARY:
			{
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * 100);
				if (getStateReligion() != NO_RELIGION)
				{
					if (kUnitInfo.getReligionSpreadStrength(getStateReligion()) > 0)
					{
						iValue += (5 * kUnitInfo.getReligionSpreadStrength(getStateReligion())) / 2;
					}
				}
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (kUnitInfo.getReligionSpreadStrength((ReligionTypes)iI) && hasHolyCity((ReligionTypes)iI))
					{
						iValue += 80;
						break;
					}
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
				{
					int iTempValue = 0;
					for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
					{
						if (kUnitInfo.getReligionSpreadStrength((ReligionTypes)iI))
						{
							iTempValue += (50 * getNumCities()) / (1 + getHasReligionCount((ReligionTypes)iI));
						}
					}
					iValue += iTempValue;
				}
				for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
				{
					if (hasHeadquarters((CorporationTypes)iI) && kUnitInfo.getCorporationSpreadStrength(iI) > 0)
					{
						iValue += kUnitInfo.getCorporationSpreadStrength(iI) * 5/2;

						if (pArea)
						{
							iValue += 300 / std::max(1, pArea->countHasCorporation((CorporationTypes)iI, getID()));
						}
					}
				}
				break;
			}
			case UNITAI_ICBM:
			{
				if ((kUnitInfo.getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) / 100) != -1)
				{
					int iTempValue = 40 + ((kUnitInfo.getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) / 100) * 40);
					if ((kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100) == 0)
					{
						iValue += iTempValue;
					}
					else
					{
						iValue += (iTempValue * std::min(10, (kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100))) / 10;
					}
					iValue += (iTempValue * (60 + kUnitInfo.getAir(AIR_EVASION, CASC_SCOPE_UNIT))) / 100;
				}
				break;
			}
			case UNITAI_WORKER_SEA:
			{
				iValue += 50 * (int)kUnitInfo.getBuilds().size();
				iValue += (kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * 100;
				break;
			}
			case UNITAI_ATTACK_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) / 2; //Calvitix 50% bonus per extra moves
				iValue += kUnitInfo.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) * 4;
				break;
			}
			case UNITAI_RESERVE_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) / 2;
				break;
			}
			case UNITAI_ESCORT_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) / 4;
				iValue += kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) * 3;
				if (!kUnitInfo.getHideAndSeek().detection.empty())
				{
					iValue += 200;
				}
				// Boats which can't be seen don't play defense, don't make good escorts
				if (kUnitInfo.getHideAndSeek().concealment > 0)
				{
					iValue /= 2;
				}
				break;
			}
			case UNITAI_EXPLORE_SEA:
			{
				int iExploreValue = 100;
				if (pArea)
				{
					if (pArea->isWater())
					{
						if (pArea->getUnitsPerPlayer(BARBARIAN_PLAYER) > 0)
						{
							iExploreValue += (2 * iCombatValue);
						}
					}
				}
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * iExploreValue);
				if (kUnitInfo.hasSkill(CLS_SKILL_ALWAYS_HOSTILE))
				{
					iValue /= 2;
				}
				iValue /= (1 + AI_unitImpassableCount(eUnit));
				break;
			}
			case UNITAI_ASSAULT_SEA:
			case UNITAI_SETTLER_SEA:
			case UNITAI_MISSIONARY_SEA:
			case UNITAI_SPY_SEA:
			{
				iValue += (iCombatValue / 2);
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * 200);
				iValue += (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 * 300);
				// Never build galley transports when ocean faring ones exist (issue mainly for Carracks)
				iValue /= (1 + AI_unitImpassableCount(eUnit));
				break;
			}
			case UNITAI_CARRIER_SEA:
			{
				iValue += iCombatValue;
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) * 50);
				iValue += (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100 * 400);
				break;
			}
			case UNITAI_MISSILE_CARRIER_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) /4 ;
				iValue += (25 + iCombatValue) * (3 + (kUnitInfo.getCargo(CARGO_SPACE, CASC_SCOPE_UNIT) / 100));
				break;
			}
			case UNITAI_PIRATE_SEA:
			{
				iValue += iCombatValue;
				iValue += (iCombatValue * ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) /2);
				break;
			}
			case UNITAI_ATTACK_AIR:
			{
				iValue += iCombatValue;
				iValue += (kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) * iCombatValue) / 100;
				iValue += 4 * kUnitInfo.getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100;
				iValue += (iCombatValue * (100 + 2 * kUnitInfo.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT)) * (kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100)) / 100;
				break;
			}
			case UNITAI_DEFENSE_AIR:
			{
				iValue += iCombatValue;
				iValue += (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) * 3);
				iValue += ((kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100) * iCombatValue);
				break;
			}
			case UNITAI_CARRIER_AIR:
			{
				iValue += (iCombatValue);
				iValue += (kUnitInfo.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) * 2);
				iValue += ((kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100) * iCombatValue);
				break;
			}
			case UNITAI_MISSILE_AIR:
			{
				iValue += iCombatValue;
				iValue += 4 * kUnitInfo.getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100;
				iValue += (kUnitInfo.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100) * iCombatValue;
				break;
			}
			case UNITAI_INVESTIGATOR:
			{
				iValue += iCombatValue;
				iValue *= (kUnitInfo.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100);
				break;
			}
			case UNITAI_INFILTRATOR:
			{
				iValue += iCombatValue;
				iValue *= (kUnitInfo.getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100);
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
				{
					// One concealment number now, not a per-type table (see the SEE_INVISIBLE ranking note).
					iValue += kUnitInfo.getHideAndSeek().concealment / 10;
				}
				else
				{
					if (kUnitInfo.getHideAndSeek().concealment > 0)
					{
						iValue *= 10;
					}
					else
					{
						iValue = 0;
					}
				}
				//crime levels and many other things COULD factor in here but most all of those things rank up as these details do so this should be sufficient
				break;
			}
			case UNITAI_SEE_INVISIBLE:
			case UNITAI_SEE_INVISIBLE_SEA:
			{
				const InvisibleTypes eVisibilityRequested = criteria ? criteria->m_eVisibility : NO_INVISIBLE;
				iValue += iCombatValue;
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100) - 1) * 30 / 100;
				// RANKING, deliberately flat: a detector is worth its detection against the method asked for,
				// and nothing is multiplied by it. The old shape scaled the WHOLE unit value by the intensity
				// (x100 per method under hide-and-seek), which is what put detectors into direct competition
				// with ordinary military and civilian units on one scale.
				// ⚖ Owner: invisibility/hide-and-seek units may be UNDERVALUED for now, and the UnitAI rework
				// splits them off this shared track entirely -- so this is a deliberate floor, not a port, and
				// AI tuning is a permanent ongoing process regardless.
				if (eVisibilityRequested != NO_INVISIBLE)
				{
					iValue += kUnitInfo.getHideAndSeek().detectionAgainst(GC.getMethodSkill(eVisibilityRequested)) / 100;
				}
				else
				{
					iValue += 50 * (int)kUnitInfo.getHideAndSeek().detection.size();
				}
				break;
			}
			case UNITAI_ESCORT:
			{
				iValue += iCombatValue;
				//obsolete - for every 10 pts of combat value, make each move pt count for 1 more than a base 1 each.
				//Try 
				iValue += ((kUnitInfo.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100)-1) * iCombatValue / 5;
				//Combat weaknesses are very bad
				// The unit's OWN keyed vs-terrain entries -- the handful it authored, not every id in the registry.
				std::vector<std::pair<int, int> > vsTerrain;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN,
					COMBAT_DEFENSE, vsTerrain);
				foreach_(const STD_PAIR(int, int)& defenseEntry, vsTerrain)
				{
					const int iTerrainModifier = defenseEntry.second;
					if (iTerrainModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iTerrainModifier);
					}
					else if (iTerrainModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iTerrainModifier / 5;
					}
				}
				// The unit's OWN keyed vs-feature entries -- the handful it authored, not every id in the registry.
				std::vector<std::pair<int, int> > vsFeature;
				InfoValuation::collectKeyedCombat(kUnitInfo.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE,
					COMBAT_DEFENSE, vsFeature);
				foreach_(const STD_PAIR(int, int)& defenseEntry, vsFeature)
				{
					const int iFeatureModifier = defenseEntry.second;
					if (iFeatureModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iFeatureModifier);
					}
					else if (iFeatureModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iFeatureModifier / 5;
					}
				}
				foreach_(const STD_PAIR(int, int)& combatEntry, vsUnitCombat)
				{
					const int iCombatModifier = combatEntry.second;
					if (iCombatModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iCombatModifier);
					}
					else if (iCombatModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iCombatModifier / 5;
					}
				}
				//General defense, if the unit has it, is very good. Very bad if penalized.
				iValue = getModifiedIntValue(iValue, kUnitInfo.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT));

				if (kUnitInfo.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
				{
					iValue /= 4;
				}
				if (kUnitInfo.hasSkill(CLS_SKILL_ONLY_DEFENSIVE))
				{
					iValue *= 3;
					iValue /= 2;
					//better because it enables the unit to move through opponent territory with a RoP
				}
				break;

			}
			default: FErrorMsg("error");
		}
	}

	if (iCombatValue > 0 && pArea != NULL && (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY))
	{
		const AreaAITypes eAreaAI = pArea->getAreaAIType(getTeam());
		if (eAreaAI == AREAAI_ASSAULT || eAreaAI == AREAAI_ASSAULT_MASSING)
		{
			// The unit's OWN granted promotions -- the handful it authored, not the whole registry.
			foreach_(const int iGrantedPromotion, kUnitInfo.getGrantedPromotions())
			{
				if (GC.getPromotionInfo((PromotionTypes)iGrantedPromotion).hasSkill(CLS_SKILL_AMPHIB))
				{
					iValue *= 133;
					iValue /= 100;
					break;
				}
			}
		}
	}

	return std::max(0, iValue);
}


int CvPlayerAI::AI_totalUnitAIs(UnitAITypes eUnitAI) const
{
	return (AI_getNumTrainAIUnits(eUnitAI) + AI_getNumAIUnits(eUnitAI));
}


int CvPlayerAI::AI_totalAreaUnitAIs(const CvArea* pArea, UnitAITypes eUnitAI) const
{
	return (pArea->getNumTrainAIUnits(getID(), eUnitAI) + pArea->getNumAIUnits(getID(), eUnitAI));
}


// Strength-weighted totals (#395): deployed units enter at their effective weight; queued
// production enters raw (a unit trains at its type's base rank, weight 100).
int CvPlayerAI::AI_totalEffUnitAIs(UnitAITypes eUnitAI) const
{
	return AI_getNumTrainAIUnits(eUnitAI) + AI_getEffNumAIUnits(eUnitAI);
}


int CvPlayerAI::AI_totalEffAreaUnitAIs(const CvArea* pArea, UnitAITypes eUnitAI) const
{
	return pArea->getNumTrainAIUnits(getID(), eUnitAI) + pArea->getEffNumAIUnits(getID(), eUnitAI);
}


int CvPlayerAI::AI_totalWaterAreaUnitAIs(const CvArea* pArea, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = AI_totalAreaUnitAIs(pArea, eUnitAI);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			foreach_(const CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
			{
				if (pLoopCity->waterArea() == pArea)
				{
					iCount += pLoopCity->plot()->plotCount(PUF_isUnitAIType, eUnitAI, -1, NULL, getID());

					if (pLoopCity->getOwner() == getID())
					{
						iCount += pLoopCity->getNumTrainUnitAI(eUnitAI);
					}
				}
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_countCargoSpace(UnitAITypes eUnitAI) const
{
	return algo::accumulate(units()
		| filtered(CvUnit::fn::AI_getUnitAIType() == eUnitAI)
		| transformed(CvUnit::fn::cargoSpace()), 0);
}


int CvPlayerAI::AI_neededExplorers(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pArea != NULL);
	int iNeeded = (
		std::min(
			(100 + pArea->getNumTiles())
			*
			(1 + pArea->getNumUnrevealedTiles(getTeam()) + pArea->getNumUnownedTiles())
			/
			(100 * pArea->getNumTiles()),
			// Limit the need for very big land based on empire size.
			std::max(5, 3 + getNumCities() / 3)
		)
	);

	if (0 == iNeeded && GC.getGame().countCivTeamsAlive() - 1 > GET_TEAM(getTeam()).getHasMetCivCount(true))
	{
		if (pArea->isWater())
		{
			if (GC.getMap().findBiggestArea(true) == pArea)
			{
				iNeeded++;
			}
		}
		else if (getCapitalCity() != NULL && pArea->getID() == getCapitalCity()->getArea())
		{
			for (int iPlayer = 0; iPlayer < MAX_PC_PLAYERS; iPlayer++)
			{
				const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kPlayer.isAlive() && kPlayer.getTeam() != getTeam() && !GET_TEAM(getTeam()).isHasMet(kPlayer.getTeam()))
				{
					if (pArea->getCitiesPerPlayer(kPlayer.getID()) > 0)
					{
						iNeeded++;
						break;
					}
				}
			}
		}
	}
	return iNeeded;
}

int CvPlayerAI::AI_neededHunters(const CvArea* pArea) const
{
	FAssert(pArea);

	if (pArea->isWater())
	{
		return 0; // Hunter AI currently only operates on land
	}

	if (GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT) && pArea->getNumUnownedTiles() == 0)
	{
		return 1; // A hunter unit might come in handy at some point
	}
	int iHuntersneeded = std::min(
			intSqrt(getNumCities()) + pArea->getNumUnownedTiles() / 16 + 1,
			getNumCities() / 2 + pArea->getNumUnownedTiles() / 64);
	//Calvitix, limit the amount of hunters
	#define NB_MAX_HUNTERS  10
	WorldSizeTypes eWorldSize = GC.getMap().getWorldSize();
	int iWorldSize = (int)eWorldSize;
	int iMaxhunters = 4 + int(NB_MAX_HUNTERS * pow((iWorldSize + 1) / 6.0, 0.8));
	return std::min(iHuntersneeded, iMaxhunters);
	return (iHuntersneeded);
}


int CvPlayerAI::AI_neededWorkers(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	int iNeeded = 0;
	int iCities = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->getArea() == pArea->getID())
		{
			iNeeded += pLoopCity->AI_getWorkersNeeded();
			iCities++;
		}
	}
	if (iCities == 0)
	{
		return 0;
	}
	iNeeded = std::min(iNeeded, 1 + intSqrt(iCities) * 2); // max 1 + 1 worker per city squared in area.  Calvitix Reduce to 1 workers per city suqared in area

	
	return iNeeded;

}


int CvPlayerAI::AI_neededMissionaries(const CvArea* pArea, ReligionTypes eReligion) const
{
	PROFILE_FUNC();

	const bool bCultureVictory = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2);
	const bool bHoly = hasHolyCity(eReligion);
	const bool bState = (getStateReligion() == eReligion);
	const bool bHolyState = ((getStateReligion() != NO_RELIGION) && hasHolyCity(getStateReligion()));

	int iInternalCount = 0;
	int iExternalCount = 0;
	bool bReligiousVictory = false;
	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		bReligiousVictory = true;
	}

	//internal spread.
	if ((bCultureVictory || bState || bHoly)
	&& !(!bState && bReligiousVictory))
	{
		iInternalCount = std::max(iInternalCount, (pArea->getCitiesPerPlayer(getID()) - pArea->countHasReligion(eReligion, getID())));
		if (iInternalCount > 0)
		{
			if (!bCultureVictory && !bReligiousVictory)
			{
				iInternalCount = std::max(1, iInternalCount / (bHoly ? 2 : 4));
			}
		}
	}

	//external spread.
	if (((bHoly && bState) || (bHoly && !bHolyState && (getStateReligion() != NO_RELIGION)))
	&& !(!bState && bReligiousVictory))
	{
		if (bState && bReligiousVictory)
		{
			iExternalCount += (pArea->getNumCities() - pArea->countHasReligion(eReligion));
		}
		else
		{
			iExternalCount += ((pArea->getNumCities() * 2) - (pArea->countHasReligion(eReligion) * 3));
		}

		if (bState && bReligiousVictory)
		{
			if (isConsiderReligiousVictory())
			{
				iExternalCount /= 3;
			}
		}
		else
		{
			iExternalCount /= 8;
		}

		iExternalCount = std::max(0, iExternalCount);

		if (AI_isPrimaryArea(pArea))
		{
			iExternalCount++;
		}
	}
	int iCount = iExternalCount + iInternalCount;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (EnablerKernel::everAvailable(EDGEB_UNITS, iI)
		&& GC.getUnitInfo((UnitTypes)iI).getDefaultUnitAI() == UNITAI_MISSIONARY
		&& GC.getUnitInfo((UnitTypes)iI).getAdvisor() == 1)
		{
			iCount -= getUnitCountPlusMaking((UnitTypes)iI);
		}
	}
	iCount = std::max(0, iCount);
	return iCount;
}


int CvPlayerAI::AI_neededExecutives(const CvArea* pArea, CorporationTypes eCorporation) const
{
	if (!hasHeadquarters(eCorporation))
	{
		return 0;
	}

	int iCount = ((pArea->getCitiesPerPlayer(getID()) - pArea->countHasCorporation(eCorporation, getID())) * 2);
	iCount += (pArea->getNumCities() - pArea->countHasCorporation(eCorporation));

	iCount /= 3;

	if (AI_isPrimaryArea(pArea))
	{
		++iCount;
	}

	return iCount;
}

//Looks like this is an expression of the amount of supporting (same player's) attackers available adjacent to the plot in question
int CvPlayerAI::AI_adjacentPotentialAttackers(const CvPlot* pPlot, bool bTestCanMove) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	foreach_(const CvPlot * pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->getOwner() == getID() && pLoopUnit->getDomainType() == (pPlot->isWater() ? DOMAIN_SEA : DOMAIN_LAND))
			{
				if (pLoopUnit->canAttack() && (!bTestCanMove || pLoopUnit->canMove()) && !pLoopUnit->AI_isCityAIType())
				{
					iCount++;
				}
			}
		}
	}

	return iCount;
}

//TB tooling around - this may at some point be useful...
//int CvPlayerAI::AI_PotentialEnemyAttackers(CvPlot* pPlot, bool bTestCanMove, bool bTestVisible) const
//{
//	int iCount = 0;
//
//	foreach_(CvUnit* pLoopUnit, pPlot->units())
//	{
//		if (GET_TEAM(getTeam()).isAtWar(pLoopUnit->getTeam()))
//		{
//			if (pLoopUnit->getDomainType() == ((pPlot->isWater()) ? DOMAIN_SEA : DOMAIN_LAND) || pLoopUnit->canMoveAllTerrain())
//			{
//				if (pLoopUnit->canAttack())
//				{
//					if (!bTestCanMove || pLoopUnit->canMove())
//					{
//						if (!bTestVisible || (bTestVisible && !(pLoopUnit->isInvisible(getTeam(), true));
//						{
//							if (!(pLoopUnit->AI_isCityAIType()))
//							{
//								iCount++;
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//
//	return iCount;
//}
//
//int CvPlayerAI::AI_PotentialDefenders(CvPlot* pPlot, bool bTestVisible) const
//{
//	int iCount = 0;
//
//	foreach_(const CvUnit* pLoopUnit, pPlot->units())
//	{
//		if (GET_TEAM(getTeam()).isAtWar(pLoopUnit->getTeam()))
//		{
//			if (!bTestVisible || (bTestVisible && !(pLoopUnit->isInvisible(getTeam(), false));
//			{
//				if (pLoopUnit->canDefend())
//				{
//					iCount++;
//				}
//			}
//		}
//	}
//
//	return iCount;
//}


int CvPlayerAI::AI_totalMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			if (pLoopSelectionGroup->AI_getMissionAIType() == eMissionAI)
			{
				iCount += pLoopSelectionGroup->getNumUnits();
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_missionaryValue(const CvArea* pArea, ReligionTypes eReligion, PlayerTypes* peBestPlayer) const
{
	PROFILE_EXTRA_FUNC();
	CvTeam& kTeam = GET_TEAM(getTeam());
	CvGame& kGame = GC.getGame();

	int iSpreadInternalValue = 100;
	int iSpreadExternalValue = 0;

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  03/08/10								jdog5000	  */
	/*																							  */
	/* Victory Strategy AI																		  */
	/************************************************************************************************/
		// Obvious copy & paste bug
	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
	{
		iSpreadInternalValue += 500;
		if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
		{
			iSpreadInternalValue += 1500;
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
			{
				iSpreadInternalValue += 3000;
			}
		}
	}
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/


	/************************************************************************************************/
	/* RevDCM					  Start		 5/1/09												 */
	/*																							  */
	/* Inquisitions																				 */
	/************************************************************************************************/
	bool bStateReligion = (getStateReligion() == eReligion);
	bool bReligiousVictory = false;
	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		bReligiousVictory = true;
	}

	if (!bStateReligion && bReligiousVictory)
	{
		return 0;
	}

	if (bStateReligion && bReligiousVictory)
	{
		if (isPushReligiousVictory())
		{
			iSpreadInternalValue += 2500;
		}
		else
		{
			iSpreadInternalValue += 700;
		}
	}
	/************************************************************************************************/
	/* Inquisitions						 END														*/
	/************************************************************************************************/

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  10/03/09								jdog5000	  */
	/*																							  */
	/* Missionary AI																				*/
	/************************************************************************************************/
		// In free religion, treat all religions like state religions

	if (!isStateReligion())
	{
		// Free religion
		iSpreadInternalValue += 500;
		bStateReligion = true;
	}
	else if (bStateReligion)
	{
		iSpreadInternalValue += 1000;
	}
	else
	{
		iSpreadInternalValue += (500 * getHasReligionCount(eReligion)) / std::max(1, getNumCities());
	}

	int iGoldValue = 0;
	if (kTeam.hasHolyCity(eReligion))
	{
		iSpreadInternalValue += bStateReligion ? 1000 : 300;
		iSpreadExternalValue += bStateReligion ? 1000 : 150;
		if (kTeam.hasShrine(eReligion))
		{
			iSpreadInternalValue += bStateReligion ? 500 : 300;
			iSpreadExternalValue += bStateReligion ? 300 : 200;
			const int iGoldMultiplier = kGame.getHolyCity(eReligion)->getTotalCommerceRateModifier(COMMERCE_GOLD);
			iGoldValue = 6 * iGoldMultiplier;
		}
	}

	int iOurCitiesHave = 0;
	int iOurCitiesCount = 0;

	if (NULL == pArea)
	{
		iOurCitiesHave = kTeam.getHasReligionCount(eReligion);
		iOurCitiesCount = kTeam.getNumCities();
	}
	else
	{
		iOurCitiesHave = pArea->countHasReligion(eReligion, getID()) + countReligionSpreadUnits(pArea, eReligion, true);
		iOurCitiesCount = pArea->getCitiesPerPlayer(getID());
	}

	if (iOurCitiesHave < iOurCitiesCount)
	{
		iSpreadInternalValue *= 30 + ((100 * (iOurCitiesCount - iOurCitiesHave)) / iOurCitiesCount);
		iSpreadInternalValue /= 100;
		iSpreadInternalValue += iGoldValue;
	}
	else
	{
		iSpreadInternalValue = 0;
	}

	if (iSpreadExternalValue > 0)
	{
		int iBestPlayer = NO_PLAYER;
		int iBestValue = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
		{
			if (iPlayer != getID())
			{
				CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() != getTeam() && kLoopPlayer.getNumCities() > 0)
				{
					/************************************************************************************************/
					/* Afforess					  Start		 12/9/09												*/
					/*																							  */
					/*																							  */
					/************************************************************************************************/
					if (GET_TEAM(kLoopPlayer.getTeam()).isOpenBorders(getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isLimitedBorders(getTeam()))
						/************************************************************************************************/
						/* Afforess						 END															*/
						/************************************************************************************************/
					{
						int iCitiesCount = 0;
						int iCitiesHave = 0;
						int iMultiplier = AI_isDoStrategy(AI_STRATEGY_MISSIONARY) ? 60 : 25;
						if (!kLoopPlayer.isNoNonStateReligionSpread() || (kLoopPlayer.getStateReligion() == eReligion))
						{
							if (NULL == pArea)
							{
								iCitiesCount += 1 + (kLoopPlayer.getNumCities() * 75) / 100;
								iCitiesHave += std::min(iCitiesCount, kLoopPlayer.getHasReligionCount(eReligion));
							}
							else
							{
								int iPlayerSpreadPercent = (100 * kLoopPlayer.getHasReligionCount(eReligion)) / kLoopPlayer.getNumCities();
								iCitiesCount += pArea->getCitiesPerPlayer((PlayerTypes)iPlayer);
								iCitiesHave += std::min(iCitiesCount, (iCitiesCount * iPlayerSpreadPercent) / 75);
							}
						}

						if (kLoopPlayer.getStateReligion() == NO_RELIGION)
						{
							// Paganism counts as a state religion civic, that's what's caught below
							if (kLoopPlayer.getStateReligionCount() > 0)
							{
								const int iTotalReligions = kLoopPlayer.countTotalHasReligion();
								iMultiplier += 100 * std::max(0, kLoopPlayer.getNumCities() - iTotalReligions);
								iMultiplier += (iTotalReligions == 0) ? 100 : 0;
							}
						}

						int iValue = (iMultiplier * iSpreadExternalValue * (iCitiesCount - iCitiesHave)) / std::max(1, iCitiesCount);
						iValue /= 100;
						iValue += iGoldValue;

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							iBestPlayer = iPlayer;
						}
					}
				}
			}
		}

		if (iBestValue > iSpreadInternalValue)
		{
			if (NULL != peBestPlayer)
			{
				*peBestPlayer = (PlayerTypes)iBestPlayer;
			}
			return iBestValue;
		}

	}

	if (NULL != peBestPlayer)
	{
		*peBestPlayer = getID();
	}
	return iSpreadInternalValue;
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/
}

int CvPlayerAI::AI_executiveValue(const CvArea* pArea, CorporationTypes eCorporation, PlayerTypes* peBestPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvTeam& kTeam = GET_TEAM(getTeam());
	const CvGame& kGame = GC.getGame();

	int iSpreadInternalValue = 100;
	int iSpreadExternalValue = 0;

	if (kTeam.hasHeadquarters(eCorporation))
	{
		int iGoldMultiplier = kGame.getHeadquarters(eCorporation)->getTotalCommerceRateModifier(COMMERCE_GOLD);
		iSpreadInternalValue += 10 * std::max(0, (iGoldMultiplier - 100));
		iSpreadExternalValue += 15 * std::max(0, (iGoldMultiplier - 150));
	}

	int iOurCitiesHave = 0;
	int iOurCitiesCount = 0;

	if (NULL == pArea)
	{
		iOurCitiesHave = kTeam.getHasCorporationCount(eCorporation);
		iOurCitiesCount = kTeam.getNumCities();
	}
	else
	{
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					  11/14/09								jdog5000	  */
		/*																							  */
		/* City AI																					  */
		/************************************************************************************************/
		iOurCitiesHave = pArea->countHasCorporation(eCorporation, getID()) + countCorporationSpreadUnits(pArea, eCorporation, true);
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/
		iOurCitiesCount = pArea->getCitiesPerPlayer(getID());
	}

	for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); iCorp++)
	{
		if (kGame.isCompetingCorporation(eCorporation, (CorporationTypes)iCorp))
		{
			if (NULL == pArea)
			{
				iOurCitiesHave += kTeam.getHasCorporationCount(eCorporation);
			}
			else
			{
				iOurCitiesHave += pArea->countHasCorporation(eCorporation, getID());
			}
		}
	}

	if (iOurCitiesHave >= iOurCitiesCount)
	{
		iSpreadInternalValue = 0;
		/************************************************************************************************/
		/* UNOFFICIAL_PATCH					   06/23/10								  denev	   */
		/*																							  */
		/* Bugfix																					   */
		/************************************************************************************************/
		/* original bts code
				if (iSpreadExternalValue = 0)
		*/
		if (iSpreadExternalValue == 0)
			/************************************************************************************************/
			/* UNOFFICIAL_PATCH						END												  */
			/************************************************************************************************/
		{
			return 0;
		}
	}

	int iBonusValue = 0;
	CvCity* pCity = getCapitalCity();
	if (pCity != NULL)
	{
		iBonusValue = AI_corporationValue(eCorporation, pCity);
		iBonusValue /= 25;
	}

	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.isAlive() && (kLoopPlayer.getNumCities() > 0))
		{
			if ((kLoopPlayer.getTeam() == getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isVassal(getTeam()))
			{
				if (kLoopPlayer.getHasCorporationCount(eCorporation) == 0)
				{
					iBonusValue += 1000;
				}
			}
		}
	}

	if (iBonusValue == 0)
	{
		return 0;
	}

	iSpreadInternalValue += iBonusValue;

	if (iSpreadExternalValue > 0)
	{
		int iBestPlayer = NO_PLAYER;
		int iBestValue = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
		{
			if (iPlayer != getID())
			{
				CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kLoopPlayer.isAlive() && (kLoopPlayer.getTeam() != getTeam()) && (kLoopPlayer.getNumCities() > 0))
				{
					/************************************************************************************************/
					/* Afforess					  Start		 12/9/09												*/
					/*																							  */
					/*																							  */
					/************************************************************************************************/
					if (GET_TEAM(kLoopPlayer.getTeam()).isOpenBorders(getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isLimitedBorders(getTeam()))
						/************************************************************************************************/
						/* Afforess						 END															*/
						/************************************************************************************************/
					{
						if (!kLoopPlayer.isNoCorporations() && !kLoopPlayer.isNoForeignCorporations())
						{
							int iCitiesCount = 0;
							int iCitiesHave = 0;
							int iMultiplier = AI_getAttitudeWeight((PlayerTypes)iPlayer);
							if (NULL == pArea)
							{
								iCitiesCount += 1 + (kLoopPlayer.getNumCities() * 50) / 100;
								iCitiesHave += std::min(iCitiesCount, kLoopPlayer.getHasCorporationCount(eCorporation));
							}
							else
							{
								int iPlayerSpreadPercent = (100 * kLoopPlayer.getHasCorporationCount(eCorporation)) / kLoopPlayer.getNumCities();
								iCitiesCount += pArea->getCitiesPerPlayer((PlayerTypes)iPlayer);
								iCitiesHave += std::min(iCitiesCount, (iCitiesCount * iPlayerSpreadPercent) / 50);
							}

							if (iCitiesHave < iCitiesCount)
							{
								int iValue = (iMultiplier * iSpreadExternalValue);
								iValue += ((iMultiplier - 55) * iBonusValue) / 4;
								iValue /= 100;
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestPlayer = iPlayer;
								}
							}
						}
					}
				}
			}
		}

		if (iBestValue > iSpreadInternalValue)
		{
			if (NULL != peBestPlayer)
			{
				*peBestPlayer = (PlayerTypes)iBestPlayer;
			}
			return iBestValue;
		}

	}

	if (NULL != peBestPlayer)
	{
		*peBestPlayer = getID();
	}
	return iSpreadInternalValue;
}

//Returns approximately 100 x gpt value of the corporation.
int CvPlayerAI::AI_corporationValue(CorporationTypes eCorporation, const CvCity* pCity) const
{

	PROFILE_EXTRA_FUNC();
	if (pCity == NULL)
	{
		if (getCapitalCity() != NULL)
		{
			pCity = getCapitalCity();
		}
	}
	if (NULL == pCity)
	{
		return 0;
	}
	const CvCorporationInfo& kCorp = GC.getCorporationInfo(eCorporation);
	int iBonusValue = 0;

	for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); iBonus++)
	{
		const BonusTypes eBonus = (BonusTypes)iBonus;
		const int iBonusCount = pCity->getNumBonuses(eBonus);
		if (iBonusCount > 0)
		{
			// The consumed-bonus set is a vector of ENGINE IDS, so the loop variable is an int.
			foreach_(const int iPrereqBonus, kCorp.getConsumedBonuses())
			{
				if ((int)eBonus == iPrereqBonus)
				{
					// ⛔ ONE read, not two planes multiplied by a hand-counted bonus total. The legacy split
					// "produced per bonus" from a flat "change"; the data authors a SINGLE `{channel}.city.flat`
					// whose per-bonus scaling rides the entry's own `per:{anyOf: consumed bonuses}` scaler
					// ([culture-religion-research.md]: the rate and the count are ONE deposit, never two reads).
					// Re-multiplying by iBonusCount here would re-implement the scaler and double-count it.
					// Production is considered worth 4x gold, food 3x.
					iBonusValue += (300 * (kCorp.getFlatYield(YIELD_FOOD, CASC_SCOPE_CITY) / 100));
					iBonusValue += (400 * (kCorp.getFlatYield(YIELD_PRODUCTION, CASC_SCOPE_CITY) / 100));
					iBonusValue += (100 * (kCorp.getFlatYield(YIELD_COMMERCE, CASC_SCOPE_CITY) / 100));

					iBonusValue += (100 * (kCorp.getFlatCommerce(COMMERCE_GOLD, CASC_SCOPE_CITY) / 100));
					iBonusValue += (100 * (kCorp.getFlatCommerce(COMMERCE_RESEARCH, CASC_SCOPE_CITY) / 100));
					iBonusValue += (50 * (kCorp.getFlatCommerce(COMMERCE_CULTURE, CASC_SCOPE_CITY) / 100));
					iBonusValue += (40 * (kCorp.getFlatCommerce(COMMERCE_ESPIONAGE, CASC_SCOPE_CITY) / 100));

					// What the corp SUPPLIES in its city is `provides.bonuses` (json §5a), a list -- a corp may
					// supply more than one, which the single legacy `bonusProduced` could not say.
					const CvProvides* pProvides = kCorp.getProvides();
					if (pProvides != NULL)
					{
						for (std::vector<int>::const_iterator it = pProvides->bonuses.begin();
							it != pProvides->bonuses.end(); ++it)
						{
							const int iOwned = getNumAvailableBonuses((BonusTypes)*it);
							iBonusValue += (AI_baseBonusVal((BonusTypes)*it) * 1000) / (1 + 3 * iOwned * iOwned);
						}
					}
				}
			}
		}
	}
	iBonusValue *= 3;

	// The corp's city-scope wellbeing, experience and military build-rate -- the same compiled flats, reduced
	// at the point of use. ⚠ The legacy "whole numbers, not like the percents above" second plane is gone: it
	// was the same authored deposit read twice.
	iBonusValue += (kCorp.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_CITY) / 100) * 15000;
	iBonusValue += (kCorp.getFlatWellbeing(WELLBEING_HAPPINESS, CASC_SCOPE_CITY) / 100) * 25000;
	iBonusValue += kCorp.getBuildRateModifier(BUILD_RATE_MILITARY, CASC_SCOPE_CITY) * 3500;
	iBonusValue += (kCorp.getExperience(EXPERIENCE_AMOUNT, CASC_SCOPE_CITY) / 100) * 15000;
	/* Afforess						 END															*/
	/************************************************************************************************/

		//	Koshling - this result was 2-orders of magnitude out (relative to what the comment at
		//	the top of the routine claims).  The net result was that pretty much all corporation
		//	headquarters were in a totally different league from oher Wonders and actually triggered
		//	an integer overflow assertion failure in building assessment (for the headquarters)!
		//	Dividing by 100 to bring it back into line with what the header comment claims
	return iBonusValue / 100;
}

int CvPlayerAI::AI_areaMissionAIs(const CvArea* pArea, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			if (pLoopSelectionGroup->AI_getMissionAIType() == eMissionAI)
			{
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

				if (pMissionPlot != NULL)
				{
					if (pMissionPlot->area() == pArea)
					{
						iCount += pLoopSelectionGroup->getNumUnits();
					}
				}
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_plotTargetMissionAIsInternal(const CvPlot* pPlot, MissionAITypes eMissionAI, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

		if (eMissionAI == NO_MISSIONAI || eGroupMissionAI == eMissionAI)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= iRange)
				{
					iCount += pLoopSelectionGroup->getNumUnits();

					if (piClosest != NULL)
					{
						int iGroupDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());

						if (iGroupDistance < *piClosest)
						{
							*piClosest = iGroupDistance;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_plotTargetMissionAIsInternalinCargoVolume(const CvPlot* pPlot, MissionAITypes eMissionAI, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

		if (eMissionAI == NO_MISSIONAI || eGroupMissionAI == eMissionAI)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= iRange)
				{
					iCount += pLoopSelectionGroup->getNumUnitCargoVolumeTotal();

					if (piClosest != NULL)
					{
						const int iGroupDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());

						if (iGroupDistance < *piClosest)
						{
							*piClosest = iGroupDistance;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_plotTargetMissionAIsinCargoVolume(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	//	Only cache 0-range, specific mission AI results
	MissionPlotTargetMap::const_iterator foundMissionPlotMapItr = ((iRange == 0 && eMissionAI != NO_MISSIONAI) ? m_missionTargetCache.find(eMissionAI) : m_missionTargetCache.end());

	if (foundMissionPlotMapItr == m_missionTargetCache.end())
	{
		if (iRange == 0 && eMissionAI != NO_MISSIONAI)
		{
			PlotMissionTargetMap& plotMissionMap = m_missionTargetCache[eMissionAI];

			//	Since we have to walk all the groups now anyway populate the full cache map for this missionAI
			foreach_(const CvSelectionGroup * group, groups())
			{
				const MissionAITypes eGroupMissionAI = group->AI_getMissionAIType();
				if (eGroupMissionAI != eMissionAI)
					continue;

				const CvPlot* pMissionPlot = group->AI_getMissionAIPlot();
				if (pMissionPlot == NULL)
					continue;

				const int iDistance = stepDistance(group->getX(), group->getY(), pMissionPlot->getX(), pMissionPlot->getY());
				{
					// Update cache
					MissionTargetInfo& info = plotMissionMap[pMissionPlot];
					info.iVolume += group->getNumUnitCargoVolumeTotal();
					info.iClosest = std::min(info.iClosest, iDistance);
				}

				if (pMissionPlot == pPlot)
				{
					iCount += group->getNumUnitCargoVolumeTotal();

					if (piClosest != NULL)
					{
						*piClosest = std::min(*piClosest, iDistance);
					}
				}
			}
		}
		else
		{
			iCount = AI_plotTargetMissionAIsInternalinCargoVolume(pPlot, eMissionAI, iRange, piClosest);
		}
	}
	else
	{
		const PlotMissionTargetMap& plotMissionTargetMap = foundMissionPlotMapItr->second;
		PlotMissionTargetMap::const_iterator foundPlotInfoItr = plotMissionTargetMap.find(pPlot);

		if (foundPlotInfoItr != plotMissionTargetMap.end())
		{
			const MissionTargetInfo& info = foundPlotInfoItr->second;

			iCount = info.iVolume;
			if (piClosest != NULL)
			{
				*piClosest = info.iClosest;
			}
		}
		else
		{
			iCount = 0;
		}
	}

	if (pSkipSelectionGroup != NULL &&
		 (pSkipSelectionGroup->AI_getMissionAIType() == eMissionAI || eMissionAI == NO_MISSIONAI) &&
		 pPlot == pSkipSelectionGroup->AI_getMissionAIPlot())
	{
		iCount -= pSkipSelectionGroup->getNumUnitCargoVolumeTotal();
	}

	return std::max(0, iCount);
}

int CvPlayerAI::AI_plotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	//	Only cache 0-range, specific mission AI results
	MissionPlotTargetMap::const_iterator itr = ((iRange == 0 && eMissionAI != NO_MISSIONAI) ? m_missionTargetCache.find(eMissionAI) : m_missionTargetCache.end());

	if (itr == m_missionTargetCache.end())
	{
		if (iRange == 0 && eMissionAI != NO_MISSIONAI)
		{
			PlotMissionTargetMap& plotMissionTargetMap = m_missionTargetCache[eMissionAI];

			//	Since we have to walk all the groups now anyway populate the full cache map for this missionAI
			foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
			{
				const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

				if (eGroupMissionAI != eMissionAI)
					continue;
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();
				if (pMissionPlot == NULL)
					continue;

				const int iDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());
				PlotMissionTargetMap::iterator plotMissionTargetItr = plotMissionTargetMap.find(pMissionPlot);

				if (plotMissionTargetItr != plotMissionTargetMap.end())
				{
					MissionTargetInfo& info = plotMissionTargetItr->second;
					info.iCount += pLoopSelectionGroup->getNumUnits();
					info.iClosest = std::min(info.iClosest, iDistance);
				}
				else
				{
					MissionTargetInfo info;

					info.iCount = 0;
					info.iClosest = iDistance;
					info.iCount = pLoopSelectionGroup->getNumUnits();

					plotMissionTargetMap.insert(std::make_pair(pMissionPlot, info));
				}

				if (pMissionPlot == pPlot)
				{
					iCount += pLoopSelectionGroup->getNumUnits();

					if (piClosest != NULL)
					{
						*piClosest = std::min(*piClosest, iDistance);
					}
				}
			}
		}
		else
		{
			iCount = AI_plotTargetMissionAIsInternal(pPlot, eMissionAI, iRange, piClosest);
		}
	}
	else
	{
		PlotMissionTargetMap::const_iterator plotMissionTargetItr = itr->second.find(pPlot);

		if (plotMissionTargetItr != itr->second.end())
		{
			iCount = plotMissionTargetItr->second.iCount;
			if (piClosest != NULL)
			{
				*piClosest = plotMissionTargetItr->second.iClosest;
			}
		}
		else
		{
			iCount = 0;
		}
	}

	if (pSkipSelectionGroup != NULL &&
		 (pSkipSelectionGroup->AI_getMissionAIType() == eMissionAI || eMissionAI == NO_MISSIONAI) &&
		 pPlot == pSkipSelectionGroup->AI_getMissionAIPlot())
	{
		iCount -= pSkipSelectionGroup->getNumUnits();
	}

	FASSERT_NOT_NEGATIVE(iCount);
	return iCount;
}

void CvPlayerAI::AI_noteMissionAITargetCountChange(MissionAITypes eMissionAI, const CvPlot* pPlot, int iChange, const CvPlot* pUnitPlot, int iVolume)
{
	MissionPlotTargetMap::iterator foundPlotMissionMapItr = m_missionTargetCache.find(eMissionAI);

	if (foundPlotMissionMapItr != m_missionTargetCache.end())
	{
		PlotMissionTargetMap& plotMissionTargetMap = foundPlotMissionMapItr->second;
		MissionTargetInfo& info = plotMissionTargetMap[pPlot];

		const int iDistance = (pUnitPlot == NULL ? MAX_INT : stepDistance(pPlot->getX(), pPlot->getY(), pUnitPlot->getX(), pUnitPlot->getY()));

		info.iCount += iChange;
		info.iVolume += iVolume;

		if (info.iCount == 0)
		{
			info.iClosest = MAX_INT;
		}
		else if (iDistance < info.iClosest)
		{
			info.iClosest = iDistance;
		}
	}
}


int CvPlayerAI::AI_cityTargetUnitsByPath(const CvCity* pCity, const CvSelectionGroup* pSkipSelectionGroup, int iMaxPathTurns) const
{
	PROFILE_FUNC();

	int iCount = 0;

	int iPathTurns;
	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup && pLoopSelectionGroup->plot() != NULL && pLoopSelectionGroup->getNumUnits() > 0)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				const int iDistance = stepDistance(pCity->getX(), pCity->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= 1)
				{
					if (pLoopSelectionGroup->generatePath(pLoopSelectionGroup->plot(), pMissionPlot, 0, true, &iPathTurns))
					{
						if (!(pLoopSelectionGroup->canAllMove()))
						{
							iPathTurns++;
						}

						if (iPathTurns <= iMaxPathTurns)
						{
							iCount += pLoopSelectionGroup->getNumUnits();
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_unitTargetMissionAIs(pUnit, &eMissionAI, 1, pSkipSelectionGroup, -1);
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_unitTargetMissionAIs(pUnit, aeMissionAI, iMissionAICount, pSkipSelectionGroup, -1, false);
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup, int iMaxPathTurns, bool bCargo) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(CvSelectionGroup * group, groups())
	{
		if (group == pSkipSelectionGroup || group->AI_getMissionAIUnit() != pUnit)
		{
			continue;
		}
		int iPathTurns = MAX_INT;

		if (iMaxPathTurns >= 0 && pUnit->plot() != NULL && group->plot() != NULL)
		{
			// Determined the number of turns to reach the target unit
			group->generatePath(group->plot(), pUnit->plot(), 0, false, &iPathTurns);

			if (!group->canAllMove())
			{
				iPathTurns++;
			}
		}

		// If the mission parameters state any amount of movement is ok or the amount of movement allowed is valid
		if (iMaxPathTurns == -1 || iPathTurns <= iMaxPathTurns)
		{
			const MissionAITypes eGroupMissionAI = group->AI_getMissionAIType();

			for (int iMissionAIIndex = 0; iMissionAIIndex < iMissionAICount; iMissionAIIndex++)
			{
				//it looks like we're cycling through a predefined array (in one case we're matching to any one of
				//4 possible missions: MISSIONAI_LOAD_ASSAULT, MISSIONAI_LOAD_SETTLER, MISSIONAI_LOAD_SPECIAL, MISSIONAI_ATTACK_SPY
				if (eGroupMissionAI == aeMissionAI[iMissionAIIndex] || NO_MISSIONAI == aeMissionAI[iMissionAIIndex])
				{
					if (!bCargo)
					{
						iCount += group->getNumUnits();
					}
					else
					{
						iCount += group->getNumUnitCargoVolumeTotal();
					}
				}
			}
		}
	}
	return iCount;
}


int CvPlayerAI::AI_enemyTargetMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_enemyTargetMissionAIs(&eMissionAI, 1, pSkipSelectionGroup);
}

int CvPlayerAI::AI_enemyTargetMissionAIs(MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (NULL != pMissionPlot && pMissionPlot->isOwned())
			{
				const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();
				for (int iMissionAIIndex = 0; iMissionAIIndex < iMissionAICount; iMissionAIIndex++)
				{
					if (eGroupMissionAI == aeMissionAI[iMissionAIIndex] || NO_MISSIONAI == aeMissionAI[iMissionAIIndex])
					{
						if (GET_TEAM(getTeam()).AI_isChosenWar(pMissionPlot->getTeam()))
						{
							iCount += pLoopSelectionGroup->getNumUnits();
							iCount += pLoopSelectionGroup->getCargo();
							//Certainly intended to be a count
						}
					}
				}
			}
		}
	}

	return iCount;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  05/19/10								jdog5000	  */
/*																							  */
/* General AI																				   */
/************************************************************************************************/
int CvPlayerAI::AI_enemyTargetMissions(TeamTypes eTargetTeam, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot == NULL)
			{
				pMissionPlot = pLoopSelectionGroup->plot();
			}

			if (NULL != pMissionPlot)
			{
				if (pMissionPlot->isOwned() && pMissionPlot->getTeam() == eTargetTeam)
				{
					if (atWar(getTeam(), pMissionPlot->getTeam()) || pLoopSelectionGroup->AI_isDeclareWar(pMissionPlot))
					{
						iCount += pLoopSelectionGroup->getNumUnits();
						iCount += pLoopSelectionGroup->getCargo();
						//Certainly intended to be a count
					}
				}
			}
		}
	}

	return iCount;
}
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

int CvPlayerAI::AI_wakePlotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	int iCount = 0;

	foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();
			if (eMissionAI == NO_MISSIONAI || eMissionAI == eGroupMissionAI)
			{
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();
				if (pMissionPlot != NULL && pMissionPlot == pPlot)
				{
					iCount += pLoopSelectionGroup->getNumUnits();
					pLoopSelectionGroup->setActivityType(ACTIVITY_AWAKE);
				}
			}
		}
	}

	return iCount;
}


CivicTypes CvPlayerAI::AI_bestCivic(CivicOptionTypes eCivicOption) const
{
	int iBestValue;
	return AI_bestCivic(eCivicOption, &iBestValue, false);
}

CivicTypes CvPlayerAI::AI_bestCivic(CivicOptionTypes eCivicOption, int* iBestValue, bool bCivicOptionVacuum, CivicTypes* paeSelectedCivics) const
{
	PROFILE_EXTRA_FUNC();
	(*iBestValue) = MIN_INT;
	CivicTypes eBestCivic = NO_CIVIC;

	// #430 F2b (enabler.md par.6): iterate the enabler's LISTED civic frontier (canDoCivics(i) default-args IS
	// m_enabler.civics.listed(i)), REVERSE-iterated so the original descending-id order is preserved -- the
	// strict-'>' best pick tie-breaks to the highest id, so order is load-bearing. The per-option filter stays.
	std::vector<int> vecAdoptable;
	m_enabler.civics.listedIds(vecAdoptable);
	for (std::vector<int>::const_reverse_iterator it = vecAdoptable.rbegin(), itEnd = vecAdoptable.rend(); it != itEnd; ++it)
	{
		const CivicTypes eCivicX = static_cast<CivicTypes>(*it);

		if (GC.getCivicInfo(eCivicX).getCivicOption() == eCivicOption)
		{
			const int iValue = AI_civicValue(eCivicX, bCivicOptionVacuum, paeSelectedCivics);

			if (iValue > (*iBestValue))
			{
				(*iBestValue) = iValue;
				eBestCivic = eCivicX;
			}
		}
	}
	FAssert(!bCivicOptionVacuum || eBestCivic != NO_CIVIC);
	return eBestCivic;
}


//	Provide a measure of overall happyness (weighted appropriately by city)
int CvPlayerAI::AI_getOverallHappyness(int iExtraUnhappy) const
{
	PROFILE_EXTRA_FUNC();
	int iHappyness = 0;

	foreach_(const CvCity * pLoopCity, cities() | filtered(CvCity::fn::isNoUnhappiness()))
	{
		iHappyness += (pLoopCity->netHappiness() - iExtraUnhappy) * 50;
	}

	return iHappyness;
}

//	Helper function to compute a trnuncated quadratic to asign a value to (net) happyness
static int happynessValue(int iNetHappyness)
{
	if (iNetHappyness > 0)
	{
		// Cap useful gains 0n the postive side
		if (iNetHappyness >= 5)
		{
			return 250; // Value from below calculation when iNetHappyness=5
		}
		return 100 * iNetHappyness - 10 * iNetHappyness * iNetHappyness;
	}
	return 100 * iNetHappyness; // Just linear on the negative side since each is one less working pop
}


// What ONE civic contributes to ONE city's wellbeing -- the §2b four channels through the ONE valuation walk
// ([contexts.md] § The read), netted by the shared calc-surface pair. This replaces the legacy
// getAdditional{Happiness,Health}ByCivic composites: the civic's own conditioned deposits evaluated against
// the asking city, which is exactly what those computed by hand.
// ⚠ It answers the civic's OWN contribution, so a caller diffing two civics subtracts two of these.
static void pai_civicWellbeingAt(const CvCivicInfo& kCivic, const CvCity& kCity, const CvPlayer& kOwner,
	int& iHappinessOut, int& iHealthOut, int (&aiChannels)[NUM_WELLBEING_CHANNELS])
{
	kCivic.expectedWellbeing(kCity.getCityContext(), kOwner.getEmpireContext(),
		kCity.plotGroup(kOwner.getID()), aiChannels);
	iHappinessOut = InfoValuation::netHappiness(aiChannels) / 100;
	iHealthOut = InfoValuation::netHealth(aiChannels) / 100;
}

// NULL-safe membership on an info's edge families -- an info with no edges enables nothing.
static bool cvEdgesHas(const CvEdges* pEdges, EnEdgeFamily eFamily, EnEdgeBucket eBucket, int iId)
{
	return pEdges != NULL && pEdges->has(eFamily, eBucket, iId);
}

int CvPlayerAI::AI_civicValue(CivicTypes eCivic, bool bCivicOptionVacuum, CivicTypes* paeSelectedCivics) const
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(-1, GC.getNumCivicInfos(), eCivic);

	if (eCivic == NO_CIVIC || isNPC())
	{
		return 1;
	}

	if (m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())] != MAX_INT)
	{
		return m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())];
	}
	const CvCivicInfo& kCivic = GC.getCivicInfo(eCivic);
	const CvTeamAI& pTeam = GET_TEAM(getTeam());

	bool bWarPlan = pTeam.hasWarPlan(true);
	if (bWarPlan)
	{
		bWarPlan = false;
		int iEnemyWarSuccess = 0;

		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
		{
			const CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
			if (kLoopTeam.isAlive() && !kLoopTeam.isMinorCiv() && pTeam.AI_getWarPlan((TeamTypes)iTeam) != NO_WARPLAN)
			{
				if (pTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_TOTAL || pTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_PREPARING_TOTAL)
				{
					bWarPlan = true;
					break;
				}

				if (pTeam.AI_isLandTarget((TeamTypes)iTeam))
				{
					bWarPlan = true;
					break;
				}
				iEnemyWarSuccess += kLoopTeam.AI_getWarSuccess(getTeam());
			}
		}

		if (!bWarPlan)
		{
			if (iEnemyWarSuccess > std::min(getNumCities(), 4) * GC.getWAR_SUCCESS_CITY_CAPTURING() // Lots of fighting, so war is real
			|| iEnemyWarSuccess > std::min(getNumCities(), 2) * GC.getWAR_SUCCESS_CITY_CAPTURING()
			&& pTeam.AI_getEnemyPowerPercent() > 120)
			{
				bWarPlan = true;
			}
		}
	}

	ReligionTypes eBestReligion = AI_bestReligion();
	if (!kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) && !isStateReligion())
	{
		eBestReligion = NO_RELIGION;
	}
	else if (eBestReligion == NO_RELIGION)
	{
		eBestReligion = getStateReligion();
	}

	//Fuyu Civic AI: restructuring
		//#0: constant values
	int iValue = getNumCities() * 6;

	
	// Koshling - Anarchy length is not part of the civic's value - it's part of the cost of an overall switch and is now evaluated in that process
	//iValue += -(kCivic.getAnarchyLength() * getNumCities());

	int iTempValue = -getSingleCivicUpkeep(eCivic, true) * 80 / 100;
	
	iValue += iTempValue;

	CvCity* pCapital = getCapitalCity();
	iValue += ((kCivic.getScalar(SCALAR_GREAT_PEOPLE_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * getNumCities()) / 10);

	//	Koshling - made the GG calculations non-linear as they were not scaling well with large armies
	int iGGMultiplier = 100 - 1000 / (10 + range(getNumMilitaryUnits(), 1, 100));
	iTempValue = ((kCivic.getScalar(SCALAR_GREAT_GENERAL_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * iGGMultiplier) / 10);
	iTempValue += ((kCivic.getScalar(SCALAR_GREAT_GENERAL_RATE_DOMESTIC, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * iGGMultiplier) / 20);
	//Fuyu: Only if wars ongoing, as suggested by Munch - modified by Koshling to just be an increase then
	
	iValue += iTempValue / (bWarPlan || isMinorCiv() ? 3 : 1);
	iTempValue = -(kCivic.getMaintenanceModifier(MAINTENANCE_DISTANCE, CASC_SCOPE_EMPIRE) * std::max(0, getNumCities() - 3) / 8);

	
	iValue += iTempValue;
	iTempValue = -(kCivic.getMaintenanceModifier(MAINTENANCE_NUM_CITIES, CASC_SCOPE_EMPIRE) * std::max(0, getNumCities() - 3) / 8);

	
	iValue += iTempValue;

	const int iWarmongerPercent = 25000 / std::max(100, 100 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarRand());

	// `experience.empire.flat` -- the civic's own compiled deposit; ×100, so it reduces once here rather than
	// inside the weighting arithmetic below ([DEC-fixedpoint-x100]).
	const int iCivicFreeExperience = kCivic.getExperience(EXPERIENCE_AMOUNT, CASC_SCOPE_EMPIRE) / 100;
	if (iCivicFreeExperience > 0)
	{
		// Free experience increases value of hammers spent on units, population is an okay measure of base hammer production
		iTempValue = (iCivicFreeExperience * getTotalPopulation() * (bWarPlan ? 30 : 12)) / 100;
		iTempValue *= AI_averageYieldMultiplier(YIELD_PRODUCTION);
		iTempValue /= 100;
		iTempValue *= iWarmongerPercent;
		iTempValue /= 100;
		
		iValue += iTempValue;
	}

	iTempValue = ((kCivic.getScalar(SCALAR_WORK_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * AI_getNumAIUnits(UNITAI_WORKER)) / 15);
	
	iValue += iTempValue;
	iTempValue = ((kCivic.getScalar(SCALAR_IMPROVEMENT_UPGRADE_RATE, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) * getNumCities()) / 50);
	
	iValue += iTempValue;
	iTempValue = (kCivic.getBuildRateModifier(BUILD_RATE_MILITARY, CASC_SCOPE_EMPIRE) * getNumCities() * iWarmongerPercent) / (bWarPlan ? 300 : 500);
	
	iValue += iTempValue;
	// FLAT slots, so they reduce here. ⛔ The legacy PopPercent twins are NOT a second read: a pop-scaled source
	// is the SAME deposit carrying `per:{POPULATION}` ([modifier.md] §2), so reading it again and multiplying by
	// population would apply the scaler twice.
	iTempValue = (kCivic.getFlatUpkeep(UPKEEP_FREE_CIVILIAN, CASC_SCOPE_EMPIRE) / 100) / 2;
	
	iValue += iTempValue;
	iTempValue = (kCivic.getFlatUpkeep(UPKEEP_FREE_MILITARY, CASC_SCOPE_EMPIRE) / 100) / 2;
	
	iValue += iTempValue;
	iTempValue = -(kCivic.getUpkeepModifier(UPKEEP_UNIT_CIVILIAN, CASC_SCOPE_EMPIRE) * (getNumUnits() - getNumMilitaryUnits()));
	
	iValue += iTempValue;

	iTempValue = -kCivic.getUpkeepModifier(UPKEEP_UNIT_MILITARY, CASC_SCOPE_EMPIRE) * (bWarPlan ? (getNumMilitaryUnits() * 3 / 2) : getNumMilitaryUnits());

	
	iValue += iTempValue;

	if (kCivic.getScalar(SCALAR_INFLATION, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) != 0)
	{
		//	Koshling - Use 100 turns of first order costs to judge inflation modifiers
		iTempValue = static_cast<int>(-getInflationMod10000() * calculatePreInflatedCosts() * kCivic.getScalar(SCALAR_INFLATION, CASC_SCOPE_EMPIRE, CASC_UNIT_PERCENT) / 10000);
		
		iValue += iTempValue;
	}

	iTempValue = 0;
	if (kCivic.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION))
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			iTempValue += pLoopCity->foodDifference(false, true, true) / 100;   // weighed beside whole AI terms
		}
		//If not at war Food is generally more valuable then hammers
		if (!bWarPlan)
		{
			iTempValue /= -4;
		}
		//If we are at war hammers are more valuable
		else
		{
			iTempValue *= 3;
		}
		
		iValue += iTempValue;
	}

	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		// If there is no civicOption vacuum we need to subtract out the current civic
		CvCivicInfo* kCurrentCivic = NULL;

		if (!bCivicOptionVacuum)
		{
			CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOption());

			if (eCurrentCivic != NO_CIVIC)
			{
				kCurrentCivic = &GC.getCivicInfo(eCurrentCivic);
			}
		}
		if (kCivic.getRevolution(REVOLUTION_LOCAL, CASC_SCOPE_EMPIRE) != 0)
		{
			//	What's our current situation?
			int	localRevIdx = AI_calculateAverageLocalInstability();

			//	Use the more serious of the before and after values if this civic were to be chosen
			if (kCivic.getRevolution(REVOLUTION_LOCAL, CASC_SCOPE_EMPIRE) > 0)
			{
				localRevIdx += kCivic.getRevolution(REVOLUTION_LOCAL, CASC_SCOPE_EMPIRE);
			}

			//	If there is no civoption vacuum we need to subtract out the current civic
			if (kCurrentCivic != NULL)
			{
				localRevIdx -= kCurrentCivic->getRevolution(REVOLUTION_LOCAL, CASC_SCOPE_EMPIRE);
			}

			//	Treat instability seriously as it goes up - not just linear
			const int localRevScaling = localRevIdx < 0 ? 0 : std::min(localRevIdx * localRevIdx / 50 + localRevIdx / 2, 100);

			iTempValue = -(kCivic.getRevolution(REVOLUTION_LOCAL, CASC_SCOPE_EMPIRE) * localRevScaling * getNumCities()) / 4;
			
			iValue += iTempValue;
		}

		if (kCivic.getRevolution(REVOLUTION_NATIONAL, CASC_SCOPE_EMPIRE) != 0)
		{
			iTempValue = -(8 * getNumCities()) * kCivic.getRevolution(REVOLUTION_NATIONAL, CASC_SCOPE_EMPIRE);

			//	If there is no civoption vacuum we need to subtract out the current civic
			if (kCurrentCivic != NULL)
			{
				iTempValue += (8 * getNumCities()) * kCurrentCivic->getRevolution(REVOLUTION_NATIONAL, CASC_SCOPE_EMPIRE);
			}
			
			iValue += iTempValue;
		}

		if (kCivic.getRevolution(REVOLUTION_DISTANCE_MODIFIER, CASC_SCOPE_EMPIRE) != 0)
		{
			int iCapitalDistance = AI_calculateAverageCityDistance();
			int iOldCapitalDistance = iCapitalDistance;
			iCapitalDistance *= 100 + kCivic.getRevolution(REVOLUTION_DISTANCE_MODIFIER, CASC_SCOPE_EMPIRE) - (kCurrentCivic == NULL ? 0 : kCurrentCivic->getRevolution(REVOLUTION_DISTANCE_MODIFIER, CASC_SCOPE_EMPIRE));
			iCapitalDistance /= 100;

			iTempValue = (getNumCities() * (iOldCapitalDistance - iCapitalDistance) * (10 + std::max(0, AI_calculateAverageLocalInstability())));
			
			iValue += iTempValue;
		}
	}

	if (InfoValuation::resolvedCityLimit(kCivic.getCityLimit()) > 0)
	{
		if ((InfoValuation::overThresholdPenalty(kCivic.getModifiers(), MODFAM_HAPPINESS, "CITY_LIMIT") / 100) == 0)
		{
			//	Treat numCities == limit as a want-to-expand case even if we have not actually (yet) decided
			//	to produce the settler
			if (getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) >= InfoValuation::resolvedCityLimit(kCivic.getCityLimit()))
			{
				iValue -= (getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) + 1 - InfoValuation::resolvedCityLimit(kCivic.getCityLimit())) * 100; //if we are planning to expand, city limitations suck
			}
			else
			{
				//	Smaller limits suck more but since we are not trying to expand it can't be
				//	worse than the best case where we ARE trying and can't
				iValue -= 100 / InfoValuation::resolvedCityLimit(kCivic.getCityLimit());
			}
		}
		else
		{
			//	Happiness effect calculation
			int iCost = 0;
			int iCount = 0;
			int iWantToBuild = getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) + 1;

			if (iWantToBuild > InfoValuation::resolvedCityLimit(kCivic.getCityLimit()))
			{
				int iExtraCities = iWantToBuild - InfoValuation::resolvedCityLimit(kCivic.getCityLimit());

				foreach_(const CvCity * pLoopCity, cities() | filtered(!CvCity::fn::isNoUnhappiness()))
				{
					const int iHappy = pLoopCity->netHappiness(3);	//	Allow for pop growth of 3

					iCount++;

					if (iHappy < iExtraCities * (InfoValuation::overThresholdPenalty(kCivic.getModifiers(), MODFAM_HAPPINESS, "CITY_LIMIT") / 100))
					{
						//	Weight by city size as the happiness calculation does
						//	[TBD - is this really right though - unhappy in smaller cities is arguably worse]
						iCost += 50 * (iExtraCities * (InfoValuation::overThresholdPenalty(kCivic.getModifiers(), MODFAM_HAPPINESS, "CITY_LIMIT") / 100) - iHappy);
					}
				}

				//	Same normalization as the happiness calculations later use
				if (iCount != 0)
				{
					int iTempValue = (getNumCities() * 3 * iCost) / (25 * iCount);

					iValue -= iTempValue;

					
				}
			}
		}
	}

	//Upgrade Anywhere
	iTempValue = 0;
	if (kCivic.providesPolicy(CLS_POLICY_UPGRADE_ANYWHERE))
	{
		iTempValue += getNumMilitaryUnits() * iWarmongerPercent / 100;

		if (bWarPlan)
		{
			iTempValue *= 2;
			//the current gold we have plus the gold we will have in 10 turns is a decent
			//estimate of whether we are rich (if we can afford to upgrade units, anyway)
			if (getGold() + 10 * calculateBaseNetGold() > 50 + 100 * getCurrentEra())
			{
				iTempValue *= 2;
			}
		}
		else
		{
			iTempValue /= 2;
		}
		
		iValue += iTempValue;
	}
	bool bValid = true;
	iTempValue = 0;
	int iNonstateReligionCount = 0;
	//Inquisition Civic Values
	if (kCivic.providesPolicy(CLS_POLICY_ALLOW_INQUISITIONS))
	{
		if (getStateReligion() != NO_RELIGION)
		{
			//check that we don't have a civic that already blocks this inquisitions...
			for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
			{
				//we are considering changing this civic, so ignore it
				if (kCivic.getCivicOption() != (CivicOptionTypes)iI)
				{
					if (GC.getCivicInfo(getCivics((CivicOptionTypes)iI)).providesPolicy(CLS_POLICY_DISALLOW_INQUISITIONS))
					{
						bValid = false;
					}
				}
			}
			if (bValid && hasInquisitionTarget())
			{
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if ((ReligionTypes)iI != getStateReligion())
					{
						iNonstateReligionCount += algo::count_if(cities(), CvCity::fn::isHasReligion((ReligionTypes)iI));
					}
				}
			}
		}
		if (isPushReligiousVictory())
		{
			iTempValue += iNonstateReligionCount * 20;
		}
		else if (isConsiderReligiousVictory())
		{
			iTempValue /= 5;
		}
		iTempValue += countCityReligionRevolts() * 5;
		
		iValue += iTempValue;
	}

	iTempValue = 0;
	// the unit-combat-keyed buildRate rows this civic authored -- its own entries, not a registry walk
	std::vector<std::pair<int, int> > kCivicUnitCombatBuildRate;
	InfoValuation::collectKeyedTarget(kCivic.getModifiers(), MODFAM_BUILD_RATE, -1,
		InfoValuation::keyedTargetSegment("unitCombats"), kCivicUnitCombatBuildRate, (int)CASC_SCOPE_EMPIRE);
	for (size_t iKeyed = 0; iKeyed < kCivicUnitCombatBuildRate.size(); ++iKeyed)
	{
		iTempValue += (kCivicUnitCombatBuildRate[iKeyed].second * 2) / 3;
	}
	
	iValue += iTempValue;

	iTempValue = 0;
	// The building-keyed buildRate rows this civic authored, read off its own entries (modifier.md §5).
	std::vector<std::pair<int, int> > kCivicBuildRate;
	InfoValuation::collectKeyedTarget(kCivic.getModifiers(), MODFAM_BUILD_RATE, -1,
		InfoValuation::keyedTargetSegment("buildings"), kCivicBuildRate, (int)CASC_SCOPE_EMPIRE);
	for (size_t iKeyed = 0; iKeyed < kCivicBuildRate.size(); ++iKeyed)
	{
		iTempValue += (kCivicBuildRate[iKeyed].second * 2) / 5;
	}

	iValue += iTempValue;

	iTempValue = 0;
	// the unit-keyed buildRate rows this civic authored
	std::vector<std::pair<int, int> > kCivicUnitBuildRate;
	InfoValuation::collectKeyedTarget(kCivic.getModifiers(), MODFAM_BUILD_RATE, -1,
		InfoValuation::keyedTargetSegment("units"), kCivicUnitBuildRate, (int)CASC_SCOPE_EMPIRE);
	for (size_t iKeyed = 0; iKeyed < kCivicUnitBuildRate.size(); ++iKeyed)
	{
		iTempValue += (kCivicUnitBuildRate[iKeyed].second * 2) / 5;
	}
	
	iValue += iTempValue;

	if (kCivic.getScalar(SCALAR_POPULATION_GROWTH_RATE, CASC_SCOPE_CITY, CASC_UNIT_PERCENT) != 0)
	{
		if (m_iCityGrowthValueBase == -1)
		{
			iTempValue = 0;

			foreach_(const CvCity * pLoopCity, cities())
			{
				int iCityHappy = pLoopCity->netHappiness();
				int iCurrentFoodToGrow = pLoopCity->growthThreshold();
				// reduced at this use: it DIVIDES the whole-unit growth threshold below
				int iFoodPerTurn = pLoopCity->foodDifference(false, true, true) / 100;
				int iCityValue = 0;

				if (iFoodPerTurn > 0 && iCityHappy >= 0)
				{
					iCityValue += (std::min(3, iCityHappy + 1) * iCurrentFoodToGrow) / iFoodPerTurn;
				}

				iTempValue += iCityValue;
			}
		}
		else
		{
			iTempValue = m_iCityGrowthValueBase;
		}

		if (getNumCities() > 0)
		{
			//	iTempValue is now essentially the average number of turns to grow * iCityCount
			//	We want to normalize the value to be somewhere near gold-per-turn units, so that's
			//	roughly the amount of extra 'value' an extra population has multiplied by the
			//	reduction in turns to grow.  The value of an extra pop is more or less the yield
			//	value it produces which can be estimated as a constant that varies with era, reflecting
			//	increased producitivity of farms, mines, specialists, etc.  (era-index * 2 + 3) is a reasonable
			//	estimate.
			//	So the value is:
			int onePopBaseValue = (int)getCurrentEra() * 2 + 3;
			iTempValue = -(kCivic.getScalar(SCALAR_POPULATION_GROWTH_RATE, CASC_SCOPE_CITY, CASC_UNIT_PERCENT) * iTempValue * onePopBaseValue) / (getNumCities() * 100);

			
			iValue += iTempValue;
		}
	}

	if (kCivic.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) != 0)
	{
		int iTempValue = 0;

		// The AI will disfavor bad attitude modifiers more than good ones
		if (kCivic.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) < 0)
		{
			// ATTITUDE_SHARE is the FLAT kind of the diplomacy family (×100), so it reduces at this point of use --
			// the twin read in AI_getAttitudeVal already does ([DEC-fixedpoint-x100]).
			iTempValue += (kCivic.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) / 100 * 4);
		}
		else if (kCivic.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) > 0)
		{
			iTempValue += (kCivic.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) / 100 * 3);
		}
		
		iValue += iTempValue;
	}

	if (bWarPlan)
	{
		int iTempValue = 0;

		//City defense is good, especially during wars
		if (kCivic.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE) > 0)
		{
			iTempValue += (kCivic.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE) * 2);
		}
		//Negative city defense would be really bad in a war, avoid at all costs
		else if (kCivic.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE) < 0)
		{
			iTempValue -= (kCivic.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE) * 4);
		}
		
		iValue += iTempValue;
	}
	else
	{
		iTempValue = kCivic.getDefense(DEFENSE_AMOUNT, CASC_SCOPE_EMPIRE);
		
		iValue += iTempValue;
	}


	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		const int iCivicFreeSpecialists =
			InfoValuation::keyedTarget(kCivic.getModifiers(), MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, -1, iI) / 100;
		if (iCivicFreeSpecialists > 0)
		{
			iTempValue += getNumCities() * iCivicFreeSpecialists * 12;
		}
	}
	
	iValue += iTempValue;

	if (pCapital != NULL)
	{
		const CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOption());

		// THE CANDIDATE SET HAS TWO LEGS, and the second was absent entirely:
		//   GATED   -- buildings whose `requires` NAMES either civic (EDGEF_REQUIRED_BY): the GATE question.
		//   ENABLED -- buildings either civic's `enables` proposes (EDGEF_ENABLES): the MEMBERSHIP question.
		// Both come off each civic's own load-populated edge families ([DEC-one-reverse-view]), so no candidate
		// is found by scanning the building database. ⚑ Most shipped civics author `enables.buildings`, so
		// omitting the ENABLED leg left this valuation blind to every building a civic straightforwardly
		// unlocks -- a missing TERM in the AI's civic choice, not a tidiness gap.
		std::set<int> civicCandidates;
		const CvInfo* pProposedCivic = EnablerKernel::infoFor(EDGEB_CIVICS, (int)eCivic);
		EnablerKernel::addEdge(pProposedCivic, EDGEF_REQUIRED_BY, EDGEB_BUILDINGS, civicCandidates);
		EnablerKernel::addEdge(pProposedCivic, EDGEF_ENABLES, EDGEB_BUILDINGS, civicCandidates);

		const CvInfo* pCurrentCivic = (eCurrentCivic != NO_CIVIC)
			? EnablerKernel::infoFor(EDGEB_CIVICS, (int)eCurrentCivic) : NULL;

		if (pCurrentCivic != NULL)
		{
			EnablerKernel::addEdge(pCurrentCivic, EDGEF_REQUIRED_BY, EDGEB_BUILDINGS, civicCandidates);
			EnablerKernel::addEdge(pCurrentCivic, EDGEF_ENABLES, EDGEB_BUILDINGS, civicCandidates);
		}

		// THE TWO WORLDS, built ONCE for the whole loop rather than re-derived per candidate. Adopting a civic
		// is a SWAP, so each side states both halves -- the civic held AND the one it displaces dropped.
		// `bCivicOptionVacuum` means the slot is currently empty, so there is nothing to displace and the
		// "without" world is simply the proposed civic absent.
		CvCascadeHypothetical kWith;
		kWith.present[EDGEB_CIVICS].insert((int)eCivic);
		CvCascadeHypothetical kWithout;
		kWithout.absent[EDGEB_CIVICS].insert((int)eCivic);

		if (eCurrentCivic != NO_CIVIC && !bCivicOptionVacuum)
		{
			kWith.absent[EDGEB_CIVICS].insert((int)eCurrentCivic);
			kWithout.present[EDGEB_CIVICS].insert((int)eCurrentCivic);
		}

		// The MEMBERSHIP half of each world -- what each civic's own `enables` edges put in the tree.
		// ⛔ Membership and the gate are asked SEPARATELY on purpose: a candidate can be gate-satisfiable under a
		// hypothetical and still not be in the tree, and the reverse ([enabler.md] par.1 -- `requires` NEVER
		// changes membership). Collapsing them into one test would silently answer a different question.
		EnablerOverlay kOverlayWith;
		kOverlayWith.addHave(pProposedCivic, EDGEB_CIVICS, (int)eCivic);
		EnablerOverlay kOverlayWithout;
		kOverlayWithout.addHave(pCurrentCivic, EDGEB_CIVICS, (int)eCurrentCivic);

		const EnablerDomain& kCapitalBuildings = pCapital->m_enabler.buildings;

		for (std::set<int>::const_iterator itCandidate = civicCandidates.begin();
			itCandidate != civicCandidates.end(); ++itCandidate)
		{
			const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(*itCandidate);

			if (GC.getGame().isBuildingMaxedOut(eLoopBuilding) && getBuildingCount(eLoopBuilding) == 0)
			{
				continue;
			}
			// AVAILABLE in a world = in its tree AND its `requires` passes there. Both halves resolve through
			// the ONE membership formula and the ONE evaluator, so nothing here re-derives what a civic
			// prerequisite means -- which is exactly what the hand-rolled pair of civic sweeps used to do.
			const bool bValidWith =
				kOverlayWith.inTree(kCapitalBuildings, EDGEB_BUILDINGS, (int)eLoopBuilding)
				&& EnablerKernel::requiresMetInCity(*pCapital, EDGEB_BUILDINGS, (int)eLoopBuilding, false, &kWith);

			const bool bValidWithout =
				kOverlayWithout.inTree(kCapitalBuildings, EDGEB_BUILDINGS, (int)eLoopBuilding)
				&& EnablerKernel::requiresMetInCity(*pCapital, EDGEB_BUILDINGS, (int)eLoopBuilding, false, &kWithout);

			if (bValidWith == bValidWithout)
			{
				continue;   // the swap changes nothing for this building
			}
			const int iNumInstancesToScore = isLimitedWonder(eLoopBuilding)
				? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4);
			const int iBuildingValue = (pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore) / 6;

			// Gains the ability to construct it, or loses it.
			iValue += bValidWith ? iBuildingValue : -iBuildingValue;
		}

		//TB Note: I hope I did this ok... I broke it down a little since the bools used in the building example were not termed to provide very strong clarity enough to 'get' what they were accomplishing
		//What I fear I may need to give greater consideration to here is the CATEGORY of civics and somehow include some indication of whether a trait is providing this or not.
		//All Religions Active
		if (GC.getGame().isOption(GAMEOPTION_RELIGION_DISABLING))
		{
			iTempValue = 0;
			if (kCivic.providesPolicy(CLS_POLICY_ALL_RELIGIONS_ACTIVE))
			{
				ReligionTypes eCurrentReligion = getStateReligion();
				bool bHasEnablingCivic = (hasAllReligionsActive());
				bool bHasMultipleEnablingCivicCategories = (getAllReligionsActiveCount() > 1);

				// Iterate what the empire HOLDS, not the whole building database. This was a ~5,200-id scan
				// whose only real gate was `count > 0` -- a HAVE question the object now answers directly
				// ([tally.md]: give the OBJECT the accessor). The era filter that stood here went with it as
				// redundant: a building you already hold is by construction one you could reach.
				foreach_(const BuildingTypes eLoopBuilding, getHasBuildings())
				{
					if (GC.getBuildingInfo(eLoopBuilding).getReligion() != eCurrentReligion)
					{
						const int iNumInstancesToScore = isLimitedWonder(eLoopBuilding) ? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4);

						//	If the building is enabled by multiple categories just count it at half value always
						//	This isn't strictly accurate, but because civic evaluation works by linarly combining
						//	evaluations in different categories, cross-category couplings like this have to give
						//	stable result or else civic choices will oscillate
						const int iDivisor = (bHasMultipleEnablingCivicCategories || bHasEnablingCivic) ? 12 : 6;

						//Estimate value from capital city
						iTempValue = pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore / iDivisor;
					}
				}
			}

			iTempValue = 0;
			if (!kCivic.providesPolicy(CLS_POLICY_ALL_RELIGIONS_ACTIVE))
			{
				const ReligionTypes eCurrentReligion = getStateReligion();
				const bool bHasEnablingCivic = hasAllReligionsActive();
				const bool bHasMultipleEnablingCivicCategories = getAllReligionsActiveCount() > 1;

				if (bHasEnablingCivic || bHasMultipleEnablingCivicCategories)
				{
					// The held-set again -- the twin of the sweep above, same HAVE question, same redundant
					// era filter dropped with it.
					foreach_(const BuildingTypes eLoopBuilding, getHasBuildings())
					{
						if (GC.getBuildingInfo(eLoopBuilding).getReligion() != eCurrentReligion)
						{
							//Loses us the ability to construct the building
							const int iValueDivisor = bHasMultipleEnablingCivicCategories ? 12 : 6;
							const int iNumInstancesToScore = (isLimitedWonder(eLoopBuilding) ? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4));

							iTempValue = pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore / iValueDivisor;

							iValue -= iTempValue;
						}
					}
				}
			}
		}
	}

	// ⚠ THREE KEYED CIVIC TERMS ARE GONE -- buildings, improvements, specialists -- and the hole is deliberate.
	// Each was a whole-database scan reading a keyed getter that exists on NO info, because a civic does not
	// carry these containers by design: its `{channel}.empire.buildings.{B}` deposits are LANDED BY THE REVERSE
	// PASS ON THE TARGET BUILDING conditioned on this civic's presence (modifier.md §2a), its specialist
	// percents live on the SPECIALIST as own-output conditioned on the civic ([DEC-deliveryguy]), and the
	// per-improvement happiness authors on no civic at all (it folds into the feature terms, modifier.md §2b).
	// Valuing them needs an AS-IF-ADOPTED evaluation of those landed entries, which `expected*` cannot express:
	// it resolves against the CURRENT EmpireContext, in which this civic is precisely what is not adopted.

	iTempValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
	}
	
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumFlavorTypes(); iI++)
	{
		const int iFlavorContribution = AI_getFlavorValue((FlavorTypes)iI) * kCivic.getFlavorValue((FlavorTypes)iI);
		iTempValue += iFlavorContribution;
		if (iFlavorContribution != 0)
		{
			logDecisionAI(3, "[DAI/civic/cand] player=%d civic=%S flavor=%s contrib=%d",
				getID(), kCivic.getDescription(), GC.getFlavorTypes((FlavorTypes)iI).c_str(),
				iFlavorContribution);
		}
	}
	iValue += iTempValue;

	iTempValue = 0;

	CivicTypes eTargetCivic;
	CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOption());
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		int iOurPower = std::max(1, pTeam.getPower(true));
		int iTheirPower = std::max(1, GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getDefensivePower());

		if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
		{
			int iPlayerValue = 0;
			for (int iJ = 0; iJ < GC.getNumCivicOptionInfos(); iJ++)
			{
				eTargetCivic = GET_PLAYER((PlayerTypes)iI).getCivics((CivicOptionTypes)iJ);
				int iAttitudeChange = (eTargetCivic != NO_CIVIC ? (InfoValuation::keyedTarget(kCivic.getModifiers(), MODFAM_DIPLOMACY, -1, InfoValuation::keyedTargetSegment("civics"), (int)eTargetCivic) - (eCurrentCivic != NO_CIVIC ? InfoValuation::keyedTarget(GC.getCivicInfo(eCurrentCivic).getModifiers(), MODFAM_DIPLOMACY, -1, InfoValuation::keyedTargetSegment("civics"), (int)eTargetCivic) : 0)) : 0);
				//New Civic Attitude minus old civic attitude
				int iCurrentAttitude = AI_getAttitudeVal((PlayerTypes)iI);
				//We are close friends
				if (iCurrentAttitude > 5)
				{//Positive Changes are welcome, negative ones, not so much
					iPlayerValue += iAttitudeChange * 3;
				}
				//we aren't friends
				else
				{//if we aren't gearing up for a war yet...
					if (pTeam.AI_getWarPlan((TeamTypes)iI) != NO_WARPLAN)
					{//Then we would welcome some diplomatic improvements
						iPlayerValue += iAttitudeChange * 3;
						iPlayerValue /= 2;
					}
					else
					{
						//We are going to war, screw diplomacy
					}
				}
			}
			if (pTeam.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
			{//Who cares about vassals?
				iPlayerValue /= 5;
			}
			float fPowerRatio = ((float)iTheirPower) / ((float)iOurPower);
			iTempValue += (int)((float)iPlayerValue * fPowerRatio);
		}
	}
	if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
	{
		iTempValue /= 10;
	}
	
	iValue += iTempValue;

	iTempValue = (AI_RevCalcCivicRelEffect(eCivic));
	
	iValue += iTempValue;

	if (getWorldSizeMaxConscript(eCivic) > 0 && (pCapital != NULL))
	{
		UnitTypes eConscript = pCapital->getConscriptUnit();
		if (eConscript != NO_UNIT)
		{
			// Nationhood
			int iCombatValue = GC.getGame().AI_combatValue(eConscript);
			if (iCombatValue > 33)
			{
				iTempValue = getNumCities() + ((bWarPlan) ? 30 : 10);

				iTempValue *= range(pTeam.AI_getEnemyPowerPercent(), 50, 300);
				iTempValue /= 100;

				iTempValue *= iCombatValue;
				iTempValue /= 75;

				int iWarSuccessRatio = pTeam.AI_getWarSuccessCapitulationRatio();
				if (iWarSuccessRatio < -25)
				{
					iTempValue *= 75 + range(-iWarSuccessRatio, 25, 100);
					iTempValue /= 100;
				}

				
				iValue += iTempValue;
			}
		}
	}
	if (bWarPlan)
	{
		iTempValue = ((kCivic.getExperience(EXPERIENCE_IN_BORDER, CASC_SCOPE_EMPIRE) * getNumMilitaryUnits()) / 200);
		
		iValue += iTempValue;
	}
	iTempValue = -((kCivic.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) * getNumCities()) / ((bWarPlan) ? 10 : 50));
	
	iValue += iTempValue;
	// The civic's untyped slots reach EVERY city of the empire, so they scale by the city count.
	iTempValue = ((kCivic.getFreeSpecialistsAny(CASC_SCOPE_EMPIRE) / 100) * getNumCities() * 12);
	
	iValue += iTempValue;

	int iYieldValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		iTempValue = 0;

		iTempValue += ((kCivic.getYieldModifier((YieldTypes)iI, CASC_SCOPE_EMPIRE) * getNumCities()) / 2);

		// ⚠ The CAPITAL variant is gone as a member: a capital-only deposit is the ordinary empire one gated
		// `enabled: "IS_CAPITAL"` ([DEC-conditions-are-predicates]), so it is a CONDITIONED entry and not part
		// of the unconditioned point sum above. Valuing it needs the conditioned tail against a bound capital;
		// until then this UNDERVALUES a Bureaucracy-style civic, the accepted direction.
		iTempValue += ((kCivic.getTradeRouteYieldModifier((YieldTypes)iI, CASC_SCOPE_EMPIRE) * getNumCities()) / 11);

		// the improvement-keyed rows this civic authored -- its own entries, not a walk of the registry
		std::vector<std::pair<int, int> > kCivicImprovementYield;
		InfoValuation::collectKeyedTarget(kCivic.getModifiers(), infoYieldFamily((YieldTypes)iI), CHANNEL_AMOUNT,
			InfoValuation::keyedTargetSegment("improvements"), kCivicImprovementYield, (int)CASC_SCOPE_EMPIRE);
		for (size_t iKeyed = 0; iKeyed < kCivicImprovementYield.size(); ++iKeyed)
		{
			const int iImprovement = kCivicImprovementYield[iKeyed].first;
			iTempValue += (AI_averageYieldMultiplier((YieldTypes)iI)
				* ((kCivicImprovementYield[iKeyed].second / 100)
					* (getImprovementCount((ImprovementTypes)iImprovement) + getNumCities() / 2))) / 100;
		}

		if (iI == YIELD_FOOD)
		{
			iTempValue *= 3;
		}
		else if (iI == YIELD_PRODUCTION)
		{
			iTempValue *= ((AI_avoidScience()) ? 6 : 2);
		}
		else if (iI == YIELD_COMMERCE)
		{
			iTempValue *= ((AI_avoidScience()) ? 2 : 4);
			iTempValue /= 3;
		}

		iYieldValue += iTempValue;
	}
	
	iValue += iYieldValue;

	const bool bCultureVictory2 = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2);
	const bool bCultureVictory3 = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3);

	int iCommerceValue = 0;
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		iTempValue = 0;

		// Nationhood
		iTempValue += ((kCivic.getCommerceModifier((CommerceTypes)iI, CASC_SCOPE_EMPIRE) * getNumCities()) / 3);
		if (iI == COMMERCE_ESPIONAGE)
		{
			iTempValue *= AI_getEspionageWeight();
			iTempValue /= 500;
		}

		iTempValue *= AI_commerceWeight((CommerceTypes)iI);

		if ((iI == COMMERCE_CULTURE) && bCultureVictory2)
		{
			iTempValue *= 2;
			if (bCultureVictory3)
			{
				iTempValue *= 2;
			}
		}
		iTempValue /= 100;

		iCommerceValue += iTempValue;
	}
	
	iValue += iCommerceValue;

	//Everything hereafter requires at least civic option vacuum

	CivicTypes eCivicOptionCivic = getCivics((CivicOptionTypes)(kCivic.getCivicOption()));


	//#1: Happiness
	if (getNumCities() > 0
	// "Does this civic touch happiness at all" -- ONE read over its own entries. The eight legacy probes
	// (flat, per-military, largest-city, building-keyed, feature-keyed, non-state-religion, ...) were all
	// asking that of one family, and the entry list answers it whatever shape the deposit takes.
	&& (InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1)
		|| InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, -1)
		|| (kCivic.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) != 0
			&& (kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) || isStateReligion()))
		|| (kCivic.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) != 0 && getWarWearinessPercentAnger() != 0)))
	{
		int iExtraPop = 1;
		int iCount = 0;

		int iHappyValue = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityHappy = pLoopCity->netHappiness(iExtraPop);

			// The slider-scaled share of this city's happiness ("+N happiness at 100%% culture" -- an ordinary
			// happiness deposit carrying `per:{CULTURE_RATE, each:100}`) is already INSIDE netHappiness, and no
			// read isolates it, so it is weighed here like any other happiness rather than discounted.

			int iMilitaryHappinessDefenders = 0;
			if (getHappyPerMilitaryUnit() != 0 || InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1))
			{
				//only count happiness from units that are expected to stay inside the city. Maximum 3
				iMilitaryHappinessDefenders = std::max(0, (pLoopCity->plot()->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID(), NO_TEAM, PUF_isCityAIType)
					- pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, NULL, getID()) - ((pLoopCity->getProductionUnitAI() == UNITAI_SETTLE) ? 1 : 0)));
				if (iMilitaryHappinessDefenders >= 4)
					iMilitaryHappinessDefenders = 3;
				//else
				//	iMilitaryHappinessDefenders = std::min(2, iMilitaryHappinessDefenders);
				if (getHappyPerMilitaryUnit() != 0)
				{
					iCityHappy -= pLoopCity->getMilitaryHappiness();
					//iCityHappy += getHappyPerMilitaryUnit() * iMilitaryHappinessDefenders;
				}
			}
			if (!bCivicOptionVacuum)
			{
				//int iCivicOptionHappy;
				int aiOptionWellbeing[NUM_WELLBEING_CHANNELS];
				int iOptionHappy = 0;
				int iOptionHealth = 0;
				pai_civicWellbeingAt(GC.getCivicInfo(eCivicOptionCivic), *pLoopCity, *this,
					iOptionHappy, iOptionHealth, aiOptionWellbeing);
				iCityHappy -= iOptionHappy;
			}

			//Happy calculation
			int aiCandidateWellbeing[NUM_WELLBEING_CHANNELS];
			int iHappy = 0;
			int iCandidateHealth = 0;
			pai_civicWellbeingAt(kCivic, *pLoopCity, *this, iHappy, iCandidateHealth, aiCandidateWellbeing);

			int iHappyNow = iCityHappy;
			int iHappyThen = iCityHappy + iHappy;

			//	Factored original manipulation of the values into a sub-routine, and
			//	modified (in that sub-routine) to better handle negative values
			iTempValue = happynessValue(iHappyThen) - happynessValue(iHappyNow);

			iHappyValue += iTempValue * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);

			//iCount++;
			iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
			//if (iCount > 6)
			//{
			//	break;
			//}
		}

		//return (0 == iCount) ? 50 * iHappy : iHappyValue / iCount;

		if (iCount <= 0)
		{
			//iValue += (getNumCities() * 12 * 50*iHappy) / 100; //always 0 because getNumCities() is 0
		}
		else
		{
			//iValue += (getNumCities() * 12 * iHappyValue) / (100 * iCount);
			// line below is equal to line above
			//  Bracketed this way to prevent possible interger overflow issues with large negative
			//	values that arise when anarchism and similar are evaluated in advanced civilizations
			iTempValue = getNumCities() * ((3 * iHappyValue) / (25 * iCount));
			
			iValue += iTempValue;
		}

	}

	int iHighestReligionCount = ((eBestReligion == NO_RELIGION) ? 0 : getHasReligionCount(eBestReligion));

	//happiness is handled in CvCity::getAdditionalHappinessByCivic
/*
	iValue += (getCivicPercentAnger(eCivic, true) / 10);

	iTempValue = kCivic.getHappyPerMilitaryUnit() * 3;
	if (iTempValue != 0)
	{
		iValue += (getNumCities() * 9 * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
	}

	iTempValue = kCivic.getLargestCityHappiness();
	if (iTempValue != 0)
	{
		iValue += (12 * std::min(getNumCities(), GC.getWorldInfo(GC.getMap().getWorldSize()).getTargetNumCities()) * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
	}

	if (kCivic.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) != 0)
	{
		int iAngerPercent = getWarWearinessPercentAnger();
		int iPopulation = 3 + (getTotalPopulation() / std::max(1, getNumCities()));

		int iTempValue = (-kCivic.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) * iAngerPercent * iPopulation) / (GC.getPERCENT_ANGER_DIVISOR() * 100);
		if (iTempValue != 0)
		{
			iValue += (11 * getNumCities() * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
		}
	}

	if (!kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) && !isStateReligion())
	{
		iHighestReligionCount = 0;
	}

	iValue += (0 * (countTotalHasReligion() - iHighestReligionCount) * 5);
	iValue += (kCivic.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) * iHighestReligionCount * 4);
*/

	// The civic's building-keyed HAPPINESS term is gone with the other three keyed scans above, for the same
	// reason: 49 civics author `happiness.empire.buildings`, but the reverse pass lands it on the TARGET
	// building, so the civic carries nothing to enumerate and every getter this block read is declared on no
	// info. It returns with the as-if-adopted valuation.

	//#2: Health
	if ((getNumCities() > 0) &&
		(kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION) || kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS)
			|| kCivic.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) != 0 || InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HEALTH, +1)))
	{
		//int CvPlayerAI::AI_getHealthWeight(int iHealth, int iExtraPop) const

		int iExtraPop = 1;
		int iCount = 0;

		//if (0 == iHealth)
		//{
		//	iHealth = 1;
		//}

		int iCivicsNoUnhealthyPopulationCountNow = 0;
		int iCivicsNoUnhealthyPopulationCountThen = 0;
		int iCivicsBuildingOnlyHealthyCountNow = 0;
		int iCivicsBuildingOnlyHealthyCountThen = 0;

		if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
		{
			iCivicsNoUnhealthyPopulationCountThen++;
		}
		if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
		{
			iCivicsBuildingOnlyHealthyCountThen++;
		}

		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			const CivicTypes eTempCivic = ((paeSelectedCivics == NULL) ? getCivics((CivicOptionTypes)iI) : paeSelectedCivics[iI]);
			if (eTempCivic != NO_CIVIC)
			{
				const CvCivicInfo& kTempCivic = GC.getCivicInfo(eTempCivic);
				if (kTempCivic.getCivicOption() == iI)
				{
					if (bCivicOptionVacuum)
						continue;
					else
					{
						if (kTempCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
						{
							iCivicsNoUnhealthyPopulationCountNow++;
						}
						if (kTempCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
						{
							iCivicsBuildingOnlyHealthyCountNow++;
						}
					}
				}
				else
				{
					if (kTempCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
					{
						iCivicsNoUnhealthyPopulationCountNow++;
						iCivicsNoUnhealthyPopulationCountThen++;
					}
					if (kTempCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
					{
						iCivicsBuildingOnlyHealthyCountNow++;
						iCivicsBuildingOnlyHealthyCountThen++;
					}
				}
			}
		}

		int iHealthValue = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityHealth = pLoopCity->netHealth(iExtraPop);

			int iGoodHealthFromOtherCivics = 0;
			int iBadHealthFromOtherCivics = 0;
			int iGood; int iBad; int iBadBuilding;
			int iGoodFromNoUnhealthyPopulation = 0;
			int iGoodFromBuildingOnlyHealthy = 0;

			//Health calculation (iGood encludes effects from NoUnhealthyPopulation and BuildingOnlyHealthy)
			iGood = 0; iBad = 0; iBadBuilding = 0;
			// iGood / iBad are the §2b HEALTH and UNHEALTH channels themselves. ⚠ iBadBuilding (the
			// building-sourced share of the bad side) is NOT separable from a per-source read and stays 0 --
			// an accepted undervaluation of the BuildingOnlyHealthy interaction, not a lost term.
			{
				int aiHealthWellbeing[NUM_WELLBEING_CHANNELS];
				int iCivicHappy = 0;
				int iCivicHealth = 0;
				pai_civicWellbeingAt(kCivic, *pLoopCity, *this, iCivicHappy, iCivicHealth, aiHealthWellbeing);
				iGood = aiHealthWellbeing[WELLBEING_HEALTH] / 100;
				iBad = aiHealthWellbeing[WELLBEING_UNHEALTH] / 100;
			}

			if (iGood == 0 && iBad == 0)
				continue;

			int iTempAdditionalHealthByPlayerBuildingOnlyHealthy = 0;

			if (!bCivicOptionVacuum)
			{
				int iTempGood = 0; int iTempBad = 0; int iTempBadBuilding = 0;
				int aiOptionHealthWellbeing[NUM_WELLBEING_CHANNELS];
				int iOptionHappy2 = 0;
				int iOptionHealth2 = 0;
				pai_civicWellbeingAt(GC.getCivicInfo(eCivicOptionCivic), *pLoopCity, *this,
					iOptionHappy2, iOptionHealth2, aiOptionHealthWellbeing);
				iTempGood = aiOptionHealthWellbeing[WELLBEING_HEALTH] / 100;
				iTempBad = aiOptionHealthWellbeing[WELLBEING_UNHEALTH] / 100;
				iCityHealth -= iOptionHealth2;
				iTempAdditionalHealthByPlayerBuildingOnlyHealthy -= iTempBadBuilding;
			}
			for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
			{
				if (kCivic.getCivicOption() == iI)
					continue;
				CivicTypes eOtherCivic = ((paeSelectedCivics == NULL) ? getCivics((CivicOptionTypes)iI) : paeSelectedCivics[iI]);
				if (eOtherCivic != NULL && eOtherCivic != NO_CIVIC)
				{
					//iGood = 0; iBad = 0; iBadBuilding = 0;
					//int iTempHealth = pLoopCity->getAdditionalHealthByCivic(eOtherCivic, iGood, iBad, iBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, iIgnoreNoUnhealthyPopulationCount, iIgnoreBuildingOnlyHealthyCount);
					//if (iGood > 0)
					//{
					//	iGoodHealthFromOtherCivics += iGood;
					//}
					//if (iBad > 0)
					//{
					//	iBadHealthFromOtherCivics -= iBad; //negative values
					//}
					int iTempBadBuilding = 0;
					int aiOtherWellbeing[NUM_WELLBEING_CHANNELS];
					int iOtherHappy = 0;
					int iOtherHealth = 0;
					pai_civicWellbeingAt(GC.getCivicInfo(eOtherCivic), *pLoopCity, *this,
						iOtherHappy, iOtherHealth, aiOtherWellbeing);
					iGoodHealthFromOtherCivics += aiOtherWellbeing[WELLBEING_HEALTH] / 100;
					iBadHealthFromOtherCivics += aiOtherWellbeing[WELLBEING_UNHEALTH] / 100;
				}
				iBadHealthFromOtherCivics = -iBadHealthFromOtherCivics; //negative values
			}

			//free iCityHealth from all current civic health effects
			iCityHealth -= (iGoodHealthFromOtherCivics + iBadHealthFromOtherCivics); //does not include effects from NoUnhealthyPopulation or BuildingOnlyHealthy
			if (iCivicsNoUnhealthyPopulationCountNow > 0)
			{
				iGoodFromNoUnhealthyPopulation = pLoopCity->getAdditionalHealthByPlayerNoUnhealthyPopulation(iExtraPop, iCivicsNoUnhealthyPopulationCountNow);
				iCityHealth -= iGoodFromNoUnhealthyPopulation;
			}
			if (iCivicsBuildingOnlyHealthyCountNow > 0)
			{
				iGoodFromBuildingOnlyHealthy = pLoopCity->getAdditionalHealthByPlayerBuildingOnlyHealthy(iCivicsBuildingOnlyHealthyCountNow);
				iCityHealth -= iGoodFromBuildingOnlyHealthy;
			}

			//Health calculation
			//iGood = 0; iBad = 0; iBadBuilding = 0;
			//int iHealth = pLoopCity->getAdditionalHealthByCivic(eCivic, iGood, iBad, iBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, iIgnoreNoUnhealthyPopulationCount, iIgnoreBuildingOnlyHealthyCount);
			if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
			{
				//iHealth += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;
				iGood += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;
			}

			int iBadTotal = -iBad;
			int iGoodTotal = iGood;

			if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
			{
				iGoodFromNoUnhealthyPopulation = pLoopCity->getAdditionalHealthByPlayerNoUnhealthyPopulation(iExtraPop, iCivicsNoUnhealthyPopulationCountNow);
			}

			if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
			{
				iGoodFromBuildingOnlyHealthy = pLoopCity->getAdditionalHealthByPlayerBuildingOnlyHealthy(iCivicsBuildingOnlyHealthyCountNow);
			}
			iTempAdditionalHealthByPlayerBuildingOnlyHealthy += iBadBuilding;
			iGoodFromBuildingOnlyHealthy += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;

			iBadTotal += iBadHealthFromOtherCivics;
			iGoodTotal += iGoodHealthFromOtherCivics;
			if (iCivicsNoUnhealthyPopulationCountThen > 0 && !kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION))
				iGoodTotal += iGoodFromNoUnhealthyPopulation;
			if (iCivicsBuildingOnlyHealthyCountThen > 0 && !kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS))
				iGoodTotal += iGoodFromBuildingOnlyHealthy;

			if (iGood > 0)
			{
				//add new civic health effects to iHealthNow/Then
				int iHealthNow = iCityHealth + iBadTotal;
				int iHealthThen = iCityHealth + iGoodTotal + iBadTotal;

				//Fuyu: max health 8
				iHealthNow = std::min(8, iHealthNow);
				iHealthThen = std::min(8, iHealthThen);

				//Integration
				iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
				if (iBadTotal > 0)
				{
					iHealthNow -= iBadTotal;
					iHealthThen -= iBadTotal;

					//Fuyu: max health 8
					iHealthNow = std::min(8, iHealthNow);
					iHealthThen = std::min(8, iHealthThen);

					iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
					iTempValue /= 2;
				}

				//iTempValue = (iTempValue * iGood) / (iGood + iGoodHealthFromOtherCivics);
				int iTempValueFromNoUnhealthyPopulation = 0;
				int iTempValueFromBuildingOnlyHealthy = 0;
				int iTempValueFromRest;
				int iGoodFromRest = iGood;
				if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION) && iCivicsNoUnhealthyPopulationCountThen > 1)
				{
					iTempValueFromNoUnhealthyPopulation = (iTempValue * iGoodFromNoUnhealthyPopulation) / (iCivicsNoUnhealthyPopulationCountThen * iGoodTotal);
					iGoodFromRest -= iGoodFromNoUnhealthyPopulation;
				}
				if (kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS) && iCivicsBuildingOnlyHealthyCountThen > 1)
				{
					iTempValueFromBuildingOnlyHealthy = (iTempValue * iGoodFromBuildingOnlyHealthy) / (iCivicsBuildingOnlyHealthyCountThen * iGoodTotal);
					iGoodFromRest -= iGoodFromBuildingOnlyHealthy;
				}
				iTempValueFromRest = (iTempValue * iGood) / iGoodTotal;

				iTempValue = iTempValueFromNoUnhealthyPopulation + iTempValueFromBuildingOnlyHealthy + iTempValueFromRest;

				iHealthValue += std::max(0, iTempValue) * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);
			}
			if (iBad > 0)
			{
				//add new civic health effects to iHealthNow/Then
				int iHealthNow = iCityHealth;
				int iHealthThen = iCityHealth + iBadTotal;

				//Fuyu: max health 8
				iHealthNow = std::min(8, iHealthNow);
				iHealthThen = std::min(8, iHealthThen);

				//Integration
				int iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
				if (iGoodTotal > 0)
				{
					iHealthNow += iGoodTotal;
					iHealthThen += iGoodTotal;

					//Fuyu: max health 8
					iHealthNow = std::min(8, iHealthNow);
					iHealthThen = std::min(8, iHealthThen);

					iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
					iTempValue /= 2;
				}


				iTempValue = (iTempValue * iBad) / std::max(1, (-iBadTotal)); //-iBadTotal == iBad - iBadHealthFromOtherCivics

				iHealthValue += std::min(0, iTempValue) * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);
			}

			//iCount++;
			iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
			//if (iCount > 6)
			//{
			//	break;
			//}
		}
		//return (0 == iCount) ? 50*iHealth : iHealthValue / iCount;

		if (iCount <= 0)
		{
			//iValue += (getNumCities() * 6 * 50*iHealth) / 100; //always 0 because getNumCities() is 0
		}
		else
		{
			//iValue += (getNumCities() * 6 * iHealthValue) / (100 * iCount);
			// line below is equal to line above
			iTempValue = (getNumCities() * 3 * iHealthValue) / (50 * iCount);
			
			iValue += iTempValue;
		}
	}


	//health is handled in CvCity::getAdditionalHealthByCivic
/*
	iValue += ((kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION)) ? (getTotalPopulation() / 3) : 0);
	iValue += ((kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS)) ? (getNumCities() * 3) : 0);

	if (kCivic.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) != 0)
	{
		iValue += (getNumCities() * 6 * ((isCivic(eCivic)) ? -AI_getHealthWeight(-kCivic.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE), 1) : AI_getHealthWeight(kCivic.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE), 1) )) / 100;
	}
*/

	// The building-keyed HEALTH twin of the block above (15 civics author `health.empire.buildings`), gone for
	// the same reason and returning with the same valuation.
	//#2: Health - end

	//#3: Trade
	{
		int iTempNoForeignTradeCount = getNoForeignTradeCount();

		if (!bCivicOptionVacuum)
		{
			iTempNoForeignTradeCount -= (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE));
		}
		const int iConnectedForeignCities = countPotentialForeignTradeCitiesConnected();
		int iTempValue = 0;
		if (iTempNoForeignTradeCount > 0)
		{
			if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE))
			{
				iTempValue -= iConnectedForeignCities * 3 / (1 + iTempNoForeignTradeCount);
				// No additional negative value from NoForeignTrade
				iTempValue += kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) * getNumCities() * 2;
			}
			else
			{
				//kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) should be 0
				//FAssertMsg(kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) == 0, "kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) is supposed to be 0 if kPlayer.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE) is true");
				iTempValue += kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) * (std::max(0, iConnectedForeignCities - getNumCities() * 3) * 6 + getNumCities() * 2);
			}
		}
		else if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE))
		{
			iTempValue -= iConnectedForeignCities * 3;
			iTempValue += kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) * getNumCities() * 2;
		}
		else
		{
			iTempValue += kCivic.getTradeRoute(TRADE_ROUTE_AMOUNT, CASC_SCOPE_EMPIRE) * (std::max(0, iConnectedForeignCities - getNumCities() * 3) * 6 + getNumCities() * 2);
		}
		
		iValue += iTempValue;
	}
	//#3: Trade - end


	//#4: Corporations

	int iCorpMaintenanceMod = kCivic.getMaintenanceModifier(MAINTENANCE_CORPORATION, CASC_SCOPE_EMPIRE);
	iTempValue = 0;
	if (kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) || iCorpMaintenanceMod != 0)
	{
		int iHQCount = 0;
		int iOwnCorpCount = 0;
		int iForeignCorpCount = 0;
		for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
		{
			if (pTeam.hasHeadquarters((CorporationTypes)iCorp))
			{
				iHQCount++;
				iOwnCorpCount += countCorporations((CorporationTypes)iCorp);
			}
			else
			{
				iForeignCorpCount += countCorporations((CorporationTypes)iCorp);
			}
		}

		int iTempNoForeignCorporationsCount = 0;
		iTempNoForeignCorporationsCount += getNoForeignCorporationsCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC ? GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) : false)
			{
				iTempNoForeignCorporationsCount--;
			}
		}
		iTempNoForeignCorporationsCount = std::max(0, iTempNoForeignCorporationsCount);

		int iTempNoCorporationsCount = 0;
		iTempNoCorporationsCount += getNoCorporationsCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC ? GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_NO_CORPORATIONS) : false)
			{
				iTempNoCorporationsCount--;
			}
		}
		iTempNoCorporationsCount = std::max(0, iTempNoCorporationsCount);

		int iTempCorporationValue = 0;
		if (kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS))
		{
			iTempCorporationValue = 0;
			iTempCorporationValue -= iHQCount * (40 + 3 * getNumCities());
			iTempValue += iTempCorporationValue / (1 + iTempNoCorporationsCount);

			if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS)) // ROM Planned denies all corporations
			{
				iTempCorporationValue = iForeignCorpCount * 3;
				iTempValue += iTempCorporationValue / (2 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);
			}
			else if (iTempNoForeignCorporationsCount > 0)
			{
				iTempCorporationValue = -iForeignCorpCount * 3;
				iTempValue += iTempCorporationValue / (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);
			}

			//FAssertMsg((iCorpMaintenanceMod== 0), "NoCorporation civics are not supposed to be have a maintenace modifier");
			//subtracting value from the empire's already-applied corporation-maintenance stack
			if ((maintenancePercentStack((int)MAINTENANCE_CORPORATION) + iCorpMaintenanceMod) != 0)
			{
				iTempCorporationValue = 0;
				iTempCorporationValue -= (-(maintenancePercentStack((int)MAINTENANCE_CORPORATION) + iCorpMaintenanceMod) * (iHQCount * (25 + getNumCities() * 2) + iOwnCorpCount * 7)) / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + iTempNoCorporationsCount));

				iTempCorporationValue = 0;
				iTempCorporationValue -= (-(maintenancePercentStack((int)MAINTENANCE_CORPORATION) + iCorpMaintenanceMod) * (iForeignCorpCount * 7)) / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + ((kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS)) ? 1 : 0) + iTempNoForeignCorporationsCount + iTempNoCorporationsCount));
			}
		}
		else if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS))
		{
			iTempCorporationValue = iForeignCorpCount * 3;
			iTempValue += iTempCorporationValue / (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);

			if ((maintenancePercentStack((int)MAINTENANCE_CORPORATION) + iCorpMaintenanceMod) != 0)
			{
				iTempCorporationValue = 0;
				iTempCorporationValue -= -(maintenancePercentStack((int)MAINTENANCE_CORPORATION) + iCorpMaintenanceMod) * iForeignCorpCount * 7 / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount));
			}
		}

		if (iCorpMaintenanceMod != 0)
		{
			iTempCorporationValue = 0;
			iTempCorporationValue += (-iCorpMaintenanceMod * (iForeignCorpCount * 7)) / 25;
			if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) || kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || iTempNoForeignCorporationsCount > 0 || iTempNoCorporationsCount > 0)
				iTempCorporationValue /= 2;
			iTempValue += iTempCorporationValue;

			iTempCorporationValue = 0;
			iTempCorporationValue += (-iCorpMaintenanceMod * (iHQCount * (25 + getNumCities() * 2) + iOwnCorpCount * 7)) / 25;
			if (kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || iTempNoCorporationsCount > 0)
				iTempCorporationValue /= 2;
			iTempValue += iTempCorporationValue;
		}
	}
	
	iValue += iTempValue;


	/*
		if (kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS))
		{
			iValue -= countHeadquarters() * (40 + 3 * getNumCities());
		}
		if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS))
		{
			for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
			{
				if (!pTeam.hasHeadquarters((CorporationTypes)iCorp))
				{
					iValue += countCorporations((CorporationTypes)iCorp) * 3;
				}
			}
		}
		if (iCorpMaintenanceMod != 0)
		{
			int iCorpCount = 0;
			int iHQCount = 0;
			for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
			{
				if (pTeam.hasHeadquarters((CorporationTypes)iCorp))
				{
					iHQCount++;
				}
				iCorpCount += countCorporations((CorporationTypes)iCorp);
			}
			iValue += (-iCorpMaintenanceMod * (iHQCount * (25 + getNumCities() * 2) + iCorpCount * 7)) / 25;
		}
	*/
	//#4: Corporations - end

	//#5: state religion
	int iStateReligionValue = 0;
	int iTempStateReligionCount = 0;
	iTempStateReligionCount += getStateReligionCount();
	if (!bCivicOptionVacuum)
	{
		iTempStateReligionCount -= ((eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_STATE_RELIGION)) ? 1 : 0);
	}

	if (kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) || iTempStateReligionCount > 0)
	{
		if (iHighestReligionCount > 0)
		{
			//iValue += ((kCivic.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD) && !isNoNonStateReligionSpread()) ? ((getNumCities() - iHighestReligionCount) * 2) : 0);
			if (kCivic.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD))
			{
				int iTempNoNonStateReligionSpreadCount = 0;
				iTempNoNonStateReligionSpreadCount += getNoNonStateReligionSpreadCount();
				if (!bCivicOptionVacuum)
				{
					iTempNoNonStateReligionSpreadCount -= ((eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD)) ? 1 : 0);
				}
				iTempNoNonStateReligionSpreadCount = std::max(0, iTempNoNonStateReligionSpreadCount);

				iStateReligionValue += ((getNumCities() - iHighestReligionCount) * 2) / std::max(1, iTempNoNonStateReligionSpreadCount);
			}
			//iValue += (kCivic.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) * iHighestReligionCount * 4);
			// The whole state-religion cluster reads the civic's own `stateReligion.empire.<kind>` deposits. The
			// three modifiers are PERCENT kinds (not scaled); free experience is the one FLAT kind, so it alone
			// reduces ([DEC-fixedpoint-x100] — the unit verdict lives beside the kind enum, not here).
			const int iGreatPeopleRate = kCivic.getStateReligion(STATE_RELIGION_GREAT_PEOPLE_RATE, CASC_SCOPE_EMPIRE);
			const int iUnitProduction = kCivic.getStateReligion(STATE_RELIGION_UNIT_PRODUCTION, CASC_SCOPE_EMPIRE);
			const int iBuildingProduction = kCivic.getStateReligion(STATE_RELIGION_BUILDING_PRODUCTION, CASC_SCOPE_EMPIRE);
			const int iFreeExperience = kCivic.getStateReligion(STATE_RELIGION_FREE_EXPERIENCE, CASC_SCOPE_EMPIRE) / 100;

			iStateReligionValue += ((iGreatPeopleRate * iHighestReligionCount) / 20);
			iStateReligionValue += (iGreatPeopleRate / 4);
			iStateReligionValue += ((iUnitProduction * iHighestReligionCount) / 4);
			iStateReligionValue += ((iBuildingProduction * iHighestReligionCount) / 3);
			iStateReligionValue += (iFreeExperience * iHighestReligionCount * ((bWarPlan) ? 6 : 2));

			int iTempReligionValue = 0;

			if (kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION))
			{
				iTempReligionValue += iHighestReligionCount;

				// Value civic based on current gains from having a state religion
				for (int iI = 0; iI < GC.getNumVoteSourceInfos(); ++iI)
				{
					if (GC.getGame().isDiploVote((VoteSourceTypes)iI))
					{
						const ReligionTypes eReligion = GC.getGame().getVoteSourceReligion((VoteSourceTypes)iI);

						if (NO_RELIGION != eReligion && eReligion == eBestReligion)
						{
							// Are we leader of AP?
							if (getTeam() == GC.getGame().getSecretaryGeneral((VoteSourceTypes)iI))
							{
								iTempReligionValue += 100;
							}

							// Any benefits we get from AP tied to state religion?
							/*
							for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
							{
								iTempValue = iHighestReligionCount*GC.getVoteSourceInfo((VoteSourceTypes)iI).getReligionYield(iYield);

								iTempValue *= AI_yieldWeight((YieldTypes)iYield);
								iTempValue /= 100;

								iTempReligionValue += iTempValue;
							}

							for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
							{
								iTempValue = (iHighestReligionCount*GC.getVoteSourceInfo((VoteSourceTypes)iI).getReligionCommerce(iCommerce))/2;

								iTempValue *= AI_commerceWeight((CommerceTypes)iCommerce);
								iTempValue = 100;

								iTempReligionValue += iTempValue;
							}
							*/
						}
					}
				}

				// Value civic based on wonders granting state religion boosts
				for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
				{
					iTempValue = (iHighestReligionCount * getStateReligionBuildingCommerce((CommerceTypes)iCommerce)) / 2;

					iTempValue *= AI_commerceWeight((CommerceTypes)iCommerce);
					iTempValue /= 100;

					iTempReligionValue += iTempValue;
				}
			}

			iStateReligionValue += iTempReligionValue / std::max(1, iTempStateReligionCount);
		}
	}
	
	iValue += iStateReligionValue;
	//#5: state religion - end


	//#6: other possibly non-constant factors

	//iValue += ((kCivic.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION)) ? 0 : 0);
	if (kCivic.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION))
	{

		int iTempMilitaryFoodProductionCount = 0;
		iTempMilitaryFoodProductionCount += getMilitaryFoodProductionCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION))
			{
				iTempMilitaryFoodProductionCount--;
			}
		}
		iTempMilitaryFoodProductionCount = std::max(0, iTempMilitaryFoodProductionCount);

		if (AI_isDoStrategy(AI_STRATEGY_LAST_STAND)) //when is it wanted, and how much is it worth in those cases?
		{
			iTempValue = getNumCities() / (1 + iTempMilitaryFoodProductionCount);
		}
		else
		{
			//We want to grow so we don't want this
			iTempValue = -10 * getNumCities() / (1 + std::max(0, getMilitaryFoodProductionCount()));
		}
		
		iValue += iTempValue;
	}

	for (int iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (cvEdgesHas(kCivic.getEdges(), EDGEF_ENABLES, EDGEB_HURRIES, (int)iI))
		{
			iTempValue = 0;

			if (GC.getHurryInfo((HurryTypes)iI).getGoldPerProduction() > 0)
			{
				iTempValue += ((((AI_avoidScience()) ? 50 : 25) * getNumCities()) / GC.getHurryInfo((HurryTypes)iI).getGoldPerProduction());
			}
			iTempValue += (GC.getHurryInfo((HurryTypes)iI).getProductionPerPopulation() * getNumCities() * (bWarPlan ? 2 : 1)) / 5;

			int iTempHurryCount = 0;

			iTempHurryCount += getHurryCount((HurryTypes)iI);
			if (!bCivicOptionVacuum)
			{
				if (eCivicOptionCivic != NO_CIVIC && cvEdgesHas(GC.getCivicInfo(eCivicOptionCivic).getEdges(), EDGEF_ENABLES, EDGEB_HURRIES, (int)iI))
				{
					iTempHurryCount--;
				}
			}
			iTempHurryCount = std::max(0, iTempHurryCount);

			iTempValue = iTempValue / (1 + iTempHurryCount);
			
			iValue += iTempValue;
		}
	}

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (cvEdgesHas(kCivic.getEdges(), EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED, (int)iI))
		{
			int iTempSpecialBuildingNotRequiredCount = 0;
			iTempSpecialBuildingNotRequiredCount += getSpecialBuildingNotRequiredCount((SpecialBuildingTypes)iI);
			if (!bCivicOptionVacuum)
			{
				if (eCivicOptionCivic != NO_CIVIC && cvEdgesHas(GC.getCivicInfo(eCivicOptionCivic).getEdges(), EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED, (int)iI))
				{
					iTempSpecialBuildingNotRequiredCount--;
				}
			}
			iTempSpecialBuildingNotRequiredCount = std::max(0, iTempSpecialBuildingNotRequiredCount);
			iTempValue += ((getNumCities() / 2) + 1) / (1 + iTempSpecialBuildingNotRequiredCount); // XXX
		}

	}
	
	iValue += iTempValue;

	int iTempSpecialistValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		iTempValue = 0;
		if (cvEdgesHas(kCivic.getEdges(), EDGEF_ENABLES, EDGEB_SPECIALISTS, (int)iI))
		{
			iTempValue += ((getNumCities() * (bCultureVictory3 ? 10 : 1)) + 6);
		}
		int iTempSpecialistValidCount = 0;
		iTempSpecialistValidCount += getSpecialistValidCount((SpecialistTypes)iI);
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC && cvEdgesHas(GC.getCivicInfo(eCivicOptionCivic).getEdges(), EDGEF_ENABLES, EDGEB_SPECIALISTS, (int)iI))
			{
				iTempSpecialistValidCount--;
			}
		}
		iTempSpecialistValidCount = std::max(0, iTempSpecialistValidCount);

		iValue += (iTempValue / 2) / (1 + iTempSpecialistValidCount);
	}
	
	iValue += iTempSpecialistValue;

	//#7: final modifiers
	if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() == eCivic)
	{
		iTempValue = iValue;
		if (!kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) || iHighestReligionCount > 0)
		{
			iTempValue *= 5;
			iTempValue /= 4;
			iTempValue += 6 * getNumCities();
			iTempValue += 20;
		}

		iTempValue -= iValue;
		
		iValue += iTempValue;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) && (kCivic.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD)))
	{
		//is this really necessary, even if already running culture3/4 ?
		iValue /= 10;
	}

	m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())] = iValue;

	return iValue;
}


int CvPlayerAI::AI_RevCalcCivicRelEffect(CivicTypes eCivic) const
{
	PROFILE_EXTRA_FUNC();
	if (isNPC())
		return 0;
	if (!isAlive())
		return 0;
	if (getNumCities() == 0)
		return 0;

	int iTotalScore = 0;

	if (GC.getCivicInfo(eCivic).providesPolicy(CLS_POLICY_STATE_RELIGION))
	{
		int iRelScore = 0;

		float fRelGoodMod = GC.getCivicInfo(eCivic).getRevolution(REVOLUTION_GOOD_RELIGION, CASC_SCOPE_EMPIRE);
		float fRelBadMod = GC.getCivicInfo(eCivic).getRevolution(REVOLUTION_BAD_RELIGION, CASC_SCOPE_EMPIRE);
		int iHolyCityGood = GC.getCivicInfo(eCivic).getRevolution(REVOLUTION_HOLY_CITY_GOOD, CASC_SCOPE_EMPIRE);
		int iHolyCityBad = GC.getCivicInfo(eCivic).getRevolution(REVOLUTION_HOLY_CITY_BAD, CASC_SCOPE_EMPIRE);

		ReligionTypes eStateReligion = getStateReligion();

		if (eStateReligion == NO_RELIGION)
		{
			eStateReligion = getLastStateReligion();
		}
		if (eStateReligion == NO_RELIGION)
		{
			eStateReligion = GET_PLAYER(getID()).AI_findHighestHasReligion();
		}
		if (eStateReligion == NO_RELIGION)
		{
			return 0;
		}

		CvCity* pHolyCity = GC.getGame().getHolyCity(eStateReligion);

		foreach_(const CvCity * pLoopCity, cities())
		{
			float fCityStateReligion = 0;
			float fCityNonStateReligion = 0;

			if (pLoopCity->isHasReligion(eStateReligion))
			{
				fCityStateReligion += 4;
			}
			for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
			{
				if ((pLoopCity->isHasReligion((ReligionTypes)iI)) && !(eStateReligion == iI))
				{
					if (fCityNonStateReligion <= 4)
					{
						fCityNonStateReligion += 2.5;
					}
					else
					{
						fCityNonStateReligion += 1;
					}
				}
			}
			if (pLoopCity->isHolyCity())
			{
				if (pLoopCity->isHolyCity(eStateReligion))
				{
					fCityStateReligion += 5;
				}
				else
				{
					fCityNonStateReligion += 4;
				}
			}
			int iLiberalism = GC.getInfoTypeForString("TECH_LIBERALISM");
			int iSciMethod = GC.getInfoTypeForString("TECH_SCIENTIFIC_METHOD");
			bool bHeathens = false;
			if (!(GET_TEAM(getTeam()).isHasTech((TechTypes)iLiberalism)) && (pLoopCity->isHasReligion(eStateReligion)))
			{
				if (pHolyCity != NULL)
				{
					PlayerTypes eHolyCityOwnerID = pHolyCity->getOwner();
					if (getID() == eHolyCityOwnerID)
					{
						fCityStateReligion += iHolyCityGood;
					}
					else
					{
						if (GET_PLAYER(eHolyCityOwnerID).getStateReligion() != eStateReligion)//heathens!
						{
							bHeathens = true;
						}
					}
				}
			}

			int iRelBadEffect = (int)floor((fCityNonStateReligion * (1 + fRelBadMod)) + .5);
			int iRelGoodEffect = (int)floor((fCityStateReligion * (1 + fRelGoodMod)) + .5);

			if (GET_TEAM(getTeam()).isAtWar())
			{
				iRelGoodEffect = (int)floor((iRelGoodEffect * 1.5) + .5);
			}

			int iNetCivicRelEffect = iRelBadEffect - iRelGoodEffect;
			if (bHeathens)
			{
				iNetCivicRelEffect += iHolyCityBad;
			}

			if (GET_TEAM(getTeam()).isHasTech((TechTypes)iSciMethod))
			{
				iNetCivicRelEffect /= 3;
			}
			else if (GET_TEAM(getTeam()).isHasTech((TechTypes)iLiberalism))
			{
				iNetCivicRelEffect /= 2;
			}
			int iRevIdx = pLoopCity->getRevolutionIndex();
			iRevIdx = std::max(iRevIdx - 300, 100);
			float fCityReligionScore = iNetCivicRelEffect * (((float)iRevIdx) / 600);
			iRelScore += (int)(floor(fCityReligionScore));
		}//end of each city loop

		iRelScore *= 3;
		iTotalScore -= iRelScore;
	}//end of if eCivic isStateRel

	if (0 > 0)
	{
		int iCivicScore = 0;

		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityScore = 0 * pLoopCity->getReligionCount();

			int iRevIdx = pLoopCity->getRevolutionIndex();
			iRevIdx = std::max(iRevIdx - 300, 100);

			iCityScore *= iRevIdx;
			iCityScore /= (pLoopCity->angryPopulation() > 0) ? 500 : 700;

			iCivicScore += iCityScore;
		}

		iTotalScore += iCivicScore;
	}

	return iTotalScore;
}


ReligionTypes CvPlayerAI::AI_bestReligion() const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = 0;
	ReligionTypes eBestReligion = NO_RELIGION;
	const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();

	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (canDoReligion((ReligionTypes)iI))
		{
			int iValue = AI_religionValue((ReligionTypes)iI);

			if (getStateReligion() == ((ReligionTypes)iI))
			{
				iValue *= 4;
				iValue /= 3;
			}

			if (eFavorite == iI)
			{
				iValue *= 5;
				iValue /= 4;
			}

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				eBestReligion = ((ReligionTypes)iI);
			}
		}
	}

	if ((NO_RELIGION == eBestReligion) || AI_isDoStrategy(AI_STRATEGY_MISSIONARY))
	{
		return eBestReligion;
	}

	const int iBestCount = getHasReligionCount(eBestReligion);
	const int iPurityPercent = (iBestCount * 100) / std::max(1, countTotalHasReligion());

	if (iPurityPercent < 49)
	{
		const int iSpreadPercent = (iBestCount * 100) / std::max(1, getNumCities());

		if (iSpreadPercent > ((eBestReligion == eFavorite) ? 65 : 75)
		&& iPurityPercent > ((eBestReligion == eFavorite) ? 25 : 32))
		{
			return eBestReligion;
		}
		return NO_RELIGION;
	}
	return eBestReligion;
}


int CvPlayerAI::AI_religionValue(ReligionTypes eReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (getHasReligionCount(eReligion) == 0)
	{
		return 0;
	}
	int iValue = GC.getGame().countReligionLevels(eReligion);

	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isHasReligion(eReligion))
		{
			iValue += pLoopCity->getPopulation();
		}
	}

	CvCity* pHolyCity = GC.getGame().getHolyCity(eReligion);
	if (pHolyCity)
	{
		bool bOurHolyCity = pHolyCity->getOwner() == getID();
		bool bOurTeamHolyCity = pHolyCity->getTeam() == getTeam();

		if (bOurHolyCity || bOurTeamHolyCity)
		{
			int iCommerceCount = 0;

			foreach_(const BuildingTypes eTypeX, pHolyCity->getHasBuildings())
			{
				if (pHolyCity->isDormantBuilding(eTypeX))
				{
					continue;
				}
				for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
				{
					if (GC.getBuildingInfo(eTypeX).getShrineReligion() == eReligion)
					{
						iCommerceCount += GC.getReligionInfo(eReligion).getShrineCommerce((CommerceTypes)iJ);
					}
				}
			}

			if (bOurHolyCity)
			{
				iValue *= (3 + iCommerceCount);
				iValue /= 2;
			}
			else if (bOurTeamHolyCity)
			{
				iValue *= (4 + iCommerceCount);
				iValue /= 3;
			}
		}
	}

	int iTempValueModifier = 100;
	//Consider what kind of delay a change in Religion would mean
	if (getReligionAnarchyLength() != 0 && getStateReligion() != eReligion)
	{
		iTempValueModifier -= (getReligionAnarchyLength() * (10 - (int)GC.getGame().getGameSpeedType()));
	}
	iValue *= iTempValueModifier;
	iValue /= 100;

	return iValue;
}


ReligionTypes CvPlayerAI::AI_findHighestHasReligion()
{
	PROFILE_EXTRA_FUNC();
	int iValue;
	int iBestValue;
	int iI;
	ReligionTypes eMostReligion = NO_RELIGION;

	iBestValue = 0;

	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		iValue = getHasReligionCount((ReligionTypes)iI);

		if (iValue > iBestValue)
		{
			iBestValue = iValue;
			eMostReligion = (ReligionTypes)iI;
		}
	}
	return eMostReligion;
}


EspionageMissionTypes CvPlayerAI::AI_bestPlotEspionage(CvPlot* pSpyPlot, PlayerTypes& eTargetPlayer, CvPlot*& pPlot, int& iData) const
{
	PROFILE_EXTRA_FUNC();
	//ooookay what missions are possible

	FAssert(pSpyPlot != NULL);

	pPlot = NULL;
	iData = -1;

	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	//int iBestValue = 0;
	int iBestValue = 20;

	if (pSpyPlot->isNPC())
	{
		return eBestMission;
	}

	if (pSpyPlot->isOwned())
	{
		if (pSpyPlot->getTeam() != getTeam())
		{
			if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE) && (GET_TEAM(getTeam()).AI_getWarPlan(pSpyPlot->getTeam()) != NO_WARPLAN || AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1)))
			{
				//Destroy Improvement.
				if (pSpyPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.isDestroyImprovement())
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}

				//Bribe
				if (pSpyPlot->plotCount(PUF_isOtherTeam, getID(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID()) >= 1)
				{
					if (pSpyPlot->plotCount(PUF_isUnitAIType, UNITAI_WORKER, -1) >= 1)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.getBuyUnitCostFactor() > 0 && GC.getDefineINT("SS_BRIBE"))
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = -1;
								}
							}
						}
					}
				}
			}

			CvCity* pCity = pSpyPlot->getPlotCity();
			if (pCity != NULL)
			{
				for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
				{
					const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
					if (kMissionInfo.isRevolt() || kMissionInfo.isDisablePower() || kMissionInfo.getWarWearinessCounter() > 0 || kMissionInfo.getBuyCityCostFactor() > 0)
					{
						int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eBestMission = (EspionageMissionTypes)iMission;
							eTargetPlayer = pSpyPlot->getOwner();
							pPlot = pSpyPlot;
							iData = -1;
						}
					}
				}
			}
			if (pCity != NULL)
			{
				if (GET_TEAM(getTeam()).AI_getWarPlan(pCity->getTeam()) != NO_WARPLAN)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.isNuke())
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
			}
			if (pCity != NULL)
			{
				if ((pCity->plot()->countTotalCulture() / std::max<int64_t>(1, pCity->plot()->getCulture(getID()))) > 25)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getCityInsertCultureAmountFactor() > 0)
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
			}
			if (pCity != NULL)
			{
				//Something malicious
				if (AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1))
				{
					//Destroy Building.
					if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							if (GC.getEspionageMissionInfo((EspionageMissionTypes)iMission).getDestroyBuildingCostFactor() < 1)
							{
								continue;
							}
							foreach_(const BuildingTypes eTypeX, pCity->getHasBuildings())
							{
								if (pCity->isDormantBuilding(eTypeX))
								{
									continue;
								}
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, eTypeX);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = eTypeX;
								}
							}
						}
					}

					//Destroy Project
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getDestroyProjectCostFactor() > 0)
						{
							for (int iProject = 0; iProject < GC.getNumProjectInfos(); iProject++)
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, iProject);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = iProject;
								}
							}
						}
					}

					//General dataless city mission.
					if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
							{
								if ((kMissionInfo.getCityPoisonWaterCounter() > 0) || (kMissionInfo.getDestroyProductionCostFactor() > 0)
									|| (kMissionInfo.getStealTreasuryTypes() > 0))
								{
									int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestMission = (EspionageMissionTypes)iMission;
										eTargetPlayer = pSpyPlot->getOwner();
										pPlot = pSpyPlot;
										iData = -1;
									}
								}
							}
						}
					}

					//Disruption suitable for war.
					if (GET_TEAM(getTeam()).isAtWar(pSpyPlot->getTeam()))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
							if ((kMissionInfo.getCityRevoltCounter() > 0) || (kMissionInfo.getPlayerAnarchyCounter() > 0))
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = -1;
								}
							}
						}
					}

					//Assassinate
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.getDestroyUnitCostFactor() > 0 && GC.getDefineINT("SS_ASSASSINATE"))
						{
							SpecialistTypes theGreatSpecialistTarget = (SpecialistTypes)0;

							CvCity* pCity = pSpyPlot->getPlotCity();
							if (NULL != pCity)
							{
								//loop through all great specialist types
								//	⛔ The GROUP read, ONCE -- the per-type count walks the eval ctx, the operating
								//	set and the empire, so asking it per specialist made this scan quadratic.
								std::vector<int64_t> aiFreeSpecialists;
								pCity->getFreeSpecialists(aiFreeSpecialists);
								for (int iSpecialist = 7; iSpecialist < GC.getNumSpecialistInfos(); iSpecialist++)
								{
									SpecialistTypes tempSpecialist = (SpecialistTypes)0;
									//does this city contain this great specialist type?
									if (aiFreeSpecialists[iSpecialist] > 0)
									{
										//sort who is the most significant great specialist in the city
										//prefer any custom specialist	(SpecialistTypes)>13
										//then great spies				(SpecialistTypes)13
										//then great generals			(SpecialistTypes)12
										//then great engineers			(SpecialistTypes)11
										//then great merchants			(SpecialistTypes)10
										//then great scientists			(SpecialistTypes)9
										//then great artists			(SpecialistTypes)8
										//then great priests			(SpecialistTypes)7
										tempSpecialist = (SpecialistTypes)iSpecialist;
										if (tempSpecialist > theGreatSpecialistTarget)
										{
											theGreatSpecialistTarget = tempSpecialist;
										}
									}
								}
							}

							const int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = theGreatSpecialistTarget;
							}
						}
					}
				}

				//TSHEEP - Counter Espionage (Why the heck don't AIs use this in vanilla?) -
				//Requires either annoyance or memory of past Spy transgression
				if ((AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 50 : 0) ||
					AI_getMemoryCount(pSpyPlot->getOwner(), MEMORY_SPY_CAUGHT) > 0) &&
					GET_TEAM(getTeam()).getCounterespionageTurnsLeftAgainstTeam(GET_PLAYER(pSpyPlot->getOwner()).getTeam()) <= 0)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getCounterespionageNumTurns() > 0)
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
				//TSHEEP End of Counter Espionage

				//Steal Technology
				for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
				{
					if (GC.getEspionageMissionInfo((EspionageMissionTypes)iMission).getBuyTechCostFactor() > 0)
					{
						for (int iTech = 0; iTech < GC.getNumTechInfos(); iTech++)
						{
							const TechTypes eTech = (TechTypes)iTech;
							const int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, eTech);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = eTech;
							}
						}
					}
				}
			}
		}
	}

	// [ESP/best] -- the espionage mission a spy commits to this evaluation (or none).
	logEspionageAI(1, "[ESP/best] player=%d spyAt=(%d,%d) mission=%d target=%d value=%d",
		getID(), pSpyPlot ? pSpyPlot->getX() : -1, pSpyPlot ? pSpyPlot->getY() : -1,
		(int)eBestMission, (eBestMission != NO_ESPIONAGEMISSION ? (int)eTargetPlayer : -1), iBestValue);
	eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_ESPIONAGE, ESP_BEST, 1)
		.addI(ESPF_player, (int)getID())
		.addI(ESPF_spyX, pSpyPlot ? pSpyPlot->getX() : -1)
		.addI(ESPF_spyY, pSpyPlot ? pSpyPlot->getY() : -1)
		.addI(ESPF_mission, (int)eBestMission)
		.addI(ESPF_target, eBestMission != NO_ESPIONAGEMISSION ? (int)eTargetPlayer : -1)
		.addI(ESPF_value, iBestValue));

	return eBestMission;
}


/// Assigns value to espionage mission against ePlayer at pPlot, where iData can provide additional information about mission.
int CvPlayerAI::AI_espionageVal(PlayerTypes eTargetPlayer, EspionageMissionTypes eMission, const CvPlot* pPlot, int iData) const
{
	PROFILE_FUNC();
	FAssertMsg(pPlot != NULL, "Plot is not allowed to be NULL");

	if (eTargetPlayer == NO_PLAYER)
	{
		return 0;
	}

	const TeamTypes eTargetTeam = GET_PLAYER(eTargetPlayer).getTeam();

	int iCost = getEspionageMissionCost(eMission, eTargetPlayer, pPlot, iData);

	if (!canDoEspionageMission(eMission, eTargetPlayer, pPlot, iData, NULL))
	{
		return 0;
	}

	const bool bMalicious = (AI_getAttitudeWeight(pPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1) || GET_TEAM(getTeam()).AI_getWarPlan(eTargetTeam) != NO_WARPLAN);

	int64_t iValue = 0;
	if (bMalicious && GC.getEspionageMissionInfo(eMission).isDestroyImprovement())
	{
		if (pPlot->getOwner() == eTargetPlayer)
		{
			ImprovementTypes eImprovement = pPlot->getImprovementType();
			if (eImprovement != NO_IMPROVEMENT)
			{
				BonusTypes eBonus = pPlot->getNonObsoleteBonusType(GET_PLAYER(eTargetPlayer).getTeam());
				if (NO_BONUS != eBonus)
				{
					iValue += GET_PLAYER(eTargetPlayer).AI_bonusVal(eBonus, -1);

					int iTempValue = 0;
					if (NULL != pPlot->getWorkingCity())
					{
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD) * 2);
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_PRODUCTION) * 1);
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_COMMERCE) * 2);
						iTempValue += GC.getImprovementInfo(eImprovement).getUpgradeTime() / 2;
						iValue += iTempValue;
					}
				}
			}
		}
	}

	if (bMalicious
	&& GC.getEspionageMissionInfo(eMission).getDestroyBuildingCostFactor() > 0
	&& canSpyDestroyBuilding(eTargetPlayer, (BuildingTypes)iData))
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (pCity && pCity->isActiveBuilding((BuildingTypes)iData))
		{
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iData);
			if (kBuilding.getCost() > 1 && !isWorldWonder((BuildingTypes)iData))
			{
				iValue += pCity->AI_buildingValue((BuildingTypes)iData);
				iValue *= 60 + kBuilding.getCost();
				iValue /= 100;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyProjectCostFactor() > 0)
	{
		if (canSpyDestroyProject(eTargetPlayer, (ProjectTypes)iData))
		{
			const CvProjectInfo& kProject = GC.getProjectInfo((ProjectTypes)iData);

			iValue += getProductionNeeded((ProjectTypes)iData) * ((kProject.getAllowed()->cap(ALLOWEDCAP_TEAM) == 1) ? 3 : 2);
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyProductionCostFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();
		FAssert(pCity != NULL);
		if (pCity != NULL)
		{
			const int iTempValue = pCity->getProductionProgress();
			if (iTempValue > 0)
			{
				if (pCity->getProductionProject() != NO_PROJECT)
				{
					const CvProjectInfo& kProject = GC.getProjectInfo(pCity->getProductionProject());
					iValue += iTempValue * ((kProject.getAllowed()->cap(ALLOWEDCAP_TEAM) == 1) ? 4 : 2);
				}
				else if (pCity->getProductionBuilding() != NO_BUILDING)
				{
					if (isWorldWonder(pCity->getProductionBuilding()))
					{
						iValue += 3 * iTempValue;
					}
					iValue += iTempValue;
				}
				else
				{
					iValue += iTempValue;
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyUnitCostFactor() > 0)
	{
		/*
		Assassination iValues competes with:
		poisoning (64-768)
		destroy building (2-4439)
		destroy production (8-137)
		revolt (45-150?)
		counter espionage (104-112)
		steal tech (180-17080)
		*/
		SpecialistTypes theGreatSpecialistTarget = (SpecialistTypes)0;

		CvCity* pCity = pPlot->getPlotCity();
		if (NULL != pCity)
		{
			//	⛔ The GROUP read, ONCE (see the sibling scan above).
			std::vector<int64_t> aiFreeSpecialists;
			pCity->getFreeSpecialists(aiFreeSpecialists);
			for (int iSpecialist = 7; iSpecialist < GC.getNumSpecialistInfos(); iSpecialist++)
			{
				SpecialistTypes tempSpecialist = (SpecialistTypes)0;
				if (aiFreeSpecialists[iSpecialist] > 0)
				{
					tempSpecialist = (SpecialistTypes)iSpecialist;
					if (tempSpecialist > theGreatSpecialistTarget)
					{
						theGreatSpecialistTarget = tempSpecialist;
					}
				}
			}
		}
		if (theGreatSpecialistTarget >= 7)
		{
			iValue += 1000;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getBuyUnitCostFactor() > 0)
	{
		/*
		Bribe iValues compete with:
		destroy improvement (1-60)
		*/
		if (pPlot->plotCount(PUF_isOtherTeam, getID(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID()) >= 1)
		{
			if (pPlot->plotCount(PUF_isUnitAIType, UNITAI_WORKER, -1) >= 1)
			{
				iValue += 100;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getStealTreasuryTypes() > 0)
	{
		if (pPlot->getPlotCity() != NULL)
		{
			int64_t iGoldStolen = GET_PLAYER(eTargetPlayer).getGold() * GC.getEspionageMissionInfo(eMission).getStealTreasuryTypes() / 100;
			iGoldStolen *= pPlot->getPlotCity()->getPopulation();
			iGoldStolen /= std::max(1, GET_PLAYER(eTargetPlayer).getTotalPopulation());
			iValue += ((GET_PLAYER(eTargetPlayer).AI_isFinancialTrouble() || AI_isFinancialTrouble()) ? 4 : 2) * (2 * std::max<int64_t>(0, iGoldStolen - iCost));
		}
	}

	if (GC.getEspionageMissionInfo(eMission).getCounterespionageNumTurns() > 0)
	{
		//iValue += 100 * GET_TEAM(getTeam()).AI_getAttitudeVal(GET_PLAYER(eTargetPlayer).getTeam());
		// K-Mod (I didn't comment that line out, btw.)
		const TeamTypes eTeam = GET_PLAYER(eTargetPlayer).getTeam();
		const int iEra = getCurrentEra();
		int iCounterValue = 5;
		iCounterValue *= 50 * iEra * iEra + GET_TEAM(eTeam).getEspionagePointsAgainstTeam(getTeam());
		iCounterValue /= std::max(1, 50 * iEra * iEra + GET_TEAM(getTeam()).getEspionagePointsAgainstTeam(eTeam));
		iCounterValue *= AI_getMemoryCount(eTargetPlayer, MEMORY_SPY_CAUGHT) + 1;
		iValue += iCounterValue;

		//TSHEEP - Make Counterespionage matter
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			iValue += std::max((100 - GET_TEAM(getTeam()).AI_getAttitudeVal(GET_PLAYER(eTargetPlayer).getTeam())) * (1 + std::max(AI_getMemoryCount(eTargetPlayer, MEMORY_SPY_CAUGHT), 0)), 0);
		}
		//TSHEEP End
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getBuyCityCostFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			iValue += AI_cityTradeVal(pCity);

			if (GET_PLAYER(pCity->getOwner()).getNumCities() == 1)
			{
				iValue *= 3;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityInsertCultureAmountFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();
		if (NULL != pCity)
		{
			if (pCity->getOwner() != getID())
			{
				int iPlotCulture = pPlot->getCulture(getID());
				int iScale = 1;

				if (iPlotCulture > MAX_INT / 1000)
				{
					iPlotCulture /= 1000;
					iScale = 1000;
				}

				int iCultureAmount = GC.getEspionageMissionInfo(eMission).getCityInsertCultureAmountFactor() * pPlot->getCulture(getID());
				iCultureAmount /= 100;
				iCultureAmount *= iScale;
				if (pCity->calculateCulturePercent(getID()) > 40)
				{
					iValue += iCultureAmount * 3;
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityPoisonWaterCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityHealth = pCity->netHealth(0);
			int iBaseUnhealth = GC.getEspionageMissionInfo(eMission).getCityPoisonWaterCounter();

			// K-Mod: fixing some "wtf".
			/*
			int iAvgFoodShortage = std::max(0, iBaseUnhealth - iCityHealth) - pCity->foodDifference();
			iAvgFoodShortage += std::max(0, iBaseUnhealth/2 - iCityHealth) - pCity->foodDifference();

			iAvgFoodShortage /= 2;

			if( iAvgFoodShortage > 0 )
			{
			iValue += 8 * iAvgFoodShortage * iAvgFoodShortage;
			}*/
			// reduced at this use: weighed against whole health counters
			int iAvgFoodShortage = std::max(0, iBaseUnhealth - iCityHealth) - pCity->foodDifference() / 100;
			iAvgFoodShortage += std::max(0, -iCityHealth) - pCity->foodDifference() / 100;

			iAvgFoodShortage /= 2;

			if (iAvgFoodShortage > 0)
			{
				iValue += 4 * iAvgFoodShortage * iBaseUnhealth;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityUnhappinessCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityCurAngerLevel = pCity->netHappiness(0);
			int iBaseAnger = GC.getEspionageMissionInfo(eMission).getCityUnhappinessCounter();
			int iAvgUnhappy = iCityCurAngerLevel - iBaseAnger / 2;

			if (iAvgUnhappy < 0)
			{
				iValue += 14 * abs(iAvgUnhappy) * iBaseAnger;
			}
		}
	}

	if (GC.getEspionageMissionInfo(eMission).getBuyTechCostFactor() > 0)
	{
		if (iCost < GET_TEAM(getTeam()).getResearchLeft((TechTypes)iData) * 4 / 3)
		{
			int iTempValue = GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)iData, GET_PLAYER(eTargetPlayer).getTeam());

			if (GET_TEAM(getTeam()).getBestKnownTechScorePercent() < 85)
			{
				iTempValue *= 2;
			}

			iValue += iTempValue;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getSwitchCivicCostFactor() > 0)
	{
		iValue += AI_civicTradeVal((CivicTypes)iData, eTargetPlayer);
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getSwitchReligionCostFactor() > 0)
	{
		iValue += AI_religionTradeVal((ReligionTypes)iData, eTargetPlayer);
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getPlayerAnarchyCounter() > 0)
	{
		iValue += GC.getEspionageMissionInfo(eMission).getPlayerAnarchyCounter() * 40;
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isRevolt())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCurRevStatus = pCity->getRevolutionIndex();
			iValue += std::max(300, 300 + (iCurRevStatus / 5));
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isNuke())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iTempValue = 1;

			iTempValue += GC.getGame().getSorenRandNum((pCity->getPopulation() + 1), "AI Nuke City Value");
			iTempValue += std::max(0, pCity->getPopulation() - 10);

			iTempValue += ((pCity->getPopulation() * (100 + pCity->calculateCulturePercent(pCity->getOwner()))) / 100);

			iTempValue += AI_getAttitudeVal(pCity->getOwner()) / 3;

			foreach_(const CvPlot * pLoopPlot, pCity->plot()->adjacent())
			{
				if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					iTempValue++;
				}
				if (pLoopPlot->getNonObsoleteBonusType(getTeam()) != NO_BONUS)
				{
					iTempValue++;
				}
			}
			if (!(pCity->isEverOwned(getID())))
			{
				iTempValue *= 3;
				iTempValue /= 2;
			}
			if (!GET_TEAM(pCity->getTeam()).isAVassal())
			{
				iTempValue *= 2;
			}
			if (pCity->plot()->isVisible(getTeam(), false))
			{
				iTempValue += 2 * pCity->plot()->getNumVisibleUnits(getID());
			}
			else
			{
				iTempValue += 6;
			}

			iValue += iTempValue * 10;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isDisablePower())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (pCity)
		{
			foreach_(const BuildingTypes eTypeX, pCity->getHasBuildings())
			{
				if (!pCity->isDormantBuilding(eTypeX))
				{
					// ⚠ "needs power" is a requires.operate HAS_POWER clause and the powered yield bonus is a
					// CONDITIONED entry on the same predicate ([DEC-conditions-are-predicates]) -- neither is a
					// member, and both want the conditioned tail evaluated against this city. Left out rather
					// than approximated, so the term UNDERVALUES a powered building instead of inventing one.
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getWarWearinessCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityCurAngerLevel = pCity->netHappiness(0);
			int iBaseAnger = pCity->getWarWearinessPercentAnger();
			iBaseAnger *= (100 + GC.getEspionageMissionInfo(eMission).getWarWearinessCounter());
			iBaseAnger /= 100;
			iBaseAnger -= pCity->getWarWearinessPercentAnger();
			int iAvgUnhappy = iCityCurAngerLevel - iBaseAnger / 2;

			if (iAvgUnhappy < 0)
			{
				iValue += 14 * abs(iAvgUnhappy) * iBaseAnger;
			}
		}
	}

	if (iValue > MAX_INT)
	{
		FErrorMsg("error");
		return MAX_INT;
	}
	return (int)iValue;
}


int CvPlayerAI::AI_getPeaceWeight() const
{
	return m_iPeaceWeight;
}


void CvPlayerAI::AI_setPeaceWeight(int iNewValue)
{
	m_iPeaceWeight = iNewValue;
}

int CvPlayerAI::AI_getEspionageWeight() const
{
	return m_iEspionageWeight;
}

void CvPlayerAI::AI_setEspionageWeight(int iNewValue)
{
	m_iEspionageWeight = iNewValue;
}


int CvPlayerAI::AI_getAttackOddsChange() const
{
	return m_iAttackOddsChange;
}


void CvPlayerAI::AI_setAttackOddsChange(int iNewValue)
{
	m_iAttackOddsChange = iNewValue;
}


int CvPlayerAI::AI_getCivicTimer() const
{
	return m_iCivicTimer;
}


void CvPlayerAI::AI_setCivicTimer(int iNewValue)
{
	m_iCivicTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(AI_getCivicTimer());
}


void CvPlayerAI::AI_changeCivicTimer(int iChange)
{
	AI_setCivicTimer(AI_getCivicTimer() + iChange);
}


int CvPlayerAI::AI_getReligionTimer() const
{
	return m_iReligionTimer;
}


void CvPlayerAI::AI_setReligionTimer(int iNewValue)
{
	m_iReligionTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(AI_getReligionTimer());
}


void CvPlayerAI::AI_changeReligionTimer(int iChange)
{
	AI_setReligionTimer(AI_getReligionTimer() + iChange);
}

int CvPlayerAI::AI_getExtraGoldTarget() const
{
	return m_iExtraGoldTarget;
}

void CvPlayerAI::AI_setExtraGoldTarget(int iNewValue)
{
	m_iExtraGoldTarget = iNewValue;
}

int CvPlayerAI::AI_getNumTrainAIUnits(UnitAITypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	return m_aiNumTrainAIUnits[eIndex];
}


void CvPlayerAI::AI_changeNumTrainAIUnits(UnitAITypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	m_aiNumTrainAIUnits[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getNumTrainAIUnits(eIndex));
}


int CvPlayerAI::AI_getNumAIUnits(UnitAITypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	return m_aiNumAIUnits[eIndex];
}


void CvPlayerAI::AI_changeNumAIUnits(UnitAITypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	m_aiNumAIUnits[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getNumAIUnits(eIndex));
}


// Strength-weighted ledger (#395): a unit counts as its SMeffectiveCount (100 at
// type base group rank, x1.5 per merge rank), so force-sufficiency reads see aggregate
// strength-equivalents rather than raw bodies. Floored on conversion to whole units --
// never round merged force up (owner ruling).
int CvPlayerAI::AI_getEffNumAIUnits(UnitAITypes eIndex) const
{
	return AI_getEffNumAIUnitsTimes100(eIndex) / 100;
}


int CvPlayerAI::AI_getEffNumAIUnitsTimes100(UnitAITypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	return m_aiEffNumAIUnitsTimes100[eIndex];
}


void CvPlayerAI::AI_changeEffNumAIUnitsTimes100(UnitAITypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	m_aiEffNumAIUnitsTimes100[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getEffNumAIUnitsTimes100(eIndex));
}


int CvPlayerAI::AI_getSameReligionCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiSameReligionCounter[eIndex];
}


void CvPlayerAI::AI_changeSameReligionCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiSameReligionCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getSameReligionCounter(eIndex));
}


int CvPlayerAI::AI_getDifferentReligionCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiDifferentReligionCounter[eIndex];
}


void CvPlayerAI::AI_changeDifferentReligionCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiDifferentReligionCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getDifferentReligionCounter(eIndex));
}


int CvPlayerAI::AI_getFavoriteCivicCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiFavoriteCivicCounter[eIndex];
}


void CvPlayerAI::AI_changeFavoriteCivicCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiFavoriteCivicCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getFavoriteCivicCounter(eIndex));
}


int CvPlayerAI::AI_getBonusTradeCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiBonusTradeCounter[eIndex];
}


void CvPlayerAI::AI_changeBonusTradeCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiBonusTradeCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getBonusTradeCounter(eIndex));
}


int CvPlayerAI::AI_getPeacetimeTradeValue(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiPeacetimeTradeValue[eIndex];
}


void CvPlayerAI::AI_changePeacetimeTradeValue(PlayerTypes eIndex, int iChange)
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);

	if (iChange != 0)
	{
		AI_invalidateAttitudeCache(eIndex);
		GET_PLAYER(eIndex).AI_invalidateAttitudeCache(getID());

		m_aiPeacetimeTradeValue[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(AI_getPeacetimeTradeValue(eIndex));

		if (iChange < 0)
		{
			FErrorMsg("iChange is less than zero");
			return;
		}
		const TeamTypes eTeamB = GET_PLAYER(eIndex).getTeam();

		if (eTeamB != getTeam())
		{
			for (int iPlayerC = 0; iPlayerC < MAX_PC_TEAMS; iPlayerC++)
			{
				CvTeamAI& teamC = GET_TEAM((TeamTypes)iPlayerC);
				/*
				If A trades with B and A is C's worst enemy, C is only mad at B if C has met B before
					A = this
					B = eIndex
					C = iPlayerC
				*/
				if (teamC.isAlive() && teamC.isHasMet(eTeamB) && teamC.AI_getWorstEnemy() == getTeam())
				{
					teamC.AI_changeEnemyPeacetimeTradeValue(eTeamB, iChange);
				}
			}
		}
	}
}


int CvPlayerAI::AI_getPeacetimeGrantValue(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiPeacetimeGrantValue[eIndex];
}


void CvPlayerAI::AI_changePeacetimeGrantValue(PlayerTypes eIndex, int iChange)
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);

	if (iChange != 0)
	{
		m_aiPeacetimeGrantValue[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(AI_getPeacetimeGrantValue(eIndex));

		if (iChange < 0)
		{
			FErrorMsg("iChange is less than zero");
			return;
		}
		const TeamTypes eTeamB = GET_PLAYER(eIndex).getTeam();

		if (eTeamB != getTeam())
		{
			for (int iPlayerC = 0; iPlayerC < MAX_PC_TEAMS; iPlayerC++)
			{
				CvTeamAI& teamC = GET_TEAM((TeamTypes)iPlayerC);
				/*
				If A trades with B and A is C's worst enemy, C is only mad at B if C has met B before
					A = this
					B = eIndex
					C = iPlayerC
				*/
				if (teamC.isAlive() && teamC.isHasMet(eTeamB) && teamC.AI_getWorstEnemy() == getTeam())
				{
					teamC.AI_changeEnemyPeacetimeGrantValue(eTeamB, iChange);
				}
			}
		}
	}
}


int CvPlayerAI::AI_getGoldTradedTo(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiGoldTradedTo[eIndex];
}


void CvPlayerAI::AI_changeGoldTradedTo(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiGoldTradedTo[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getGoldTradedTo(eIndex));
}


int CvPlayerAI::AI_getAttitudeExtra(const PlayerTypes ePlayer) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);
	return m_aiAttitudeExtra[ePlayer];
}


void CvPlayerAI::AI_setAttitudeExtra(const PlayerTypes ePlayer, const int iNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);

	if (m_aiAttitudeExtra[ePlayer] != iNewValue)
	{
		AI_changeAttitudeCache(ePlayer, iNewValue - m_aiAttitudeExtra[ePlayer]);
		m_aiAttitudeExtra[ePlayer] = iNewValue;
	}
}


void CvPlayerAI::AI_changeAttitudeExtra(const PlayerTypes ePlayer, const int iChange)
{
	AI_setAttitudeExtra(ePlayer, (AI_getAttitudeExtra(ePlayer) + iChange));
}


bool CvPlayerAI::AI_isFirstContact(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_abFirstContact[eIndex];
}


void CvPlayerAI::AI_setFirstContact(PlayerTypes eIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_abFirstContact[eIndex] = bNewValue;
}


int CvPlayerAI::AI_getContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eIndex2);
	return m_aaiContactTimer[eIndex1][eIndex2];
}


void CvPlayerAI::AI_changeContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eIndex2);

	if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS) && eIndex1 != NO_PLAYER && !GET_PLAYER(eIndex1).isHumanPlayer() && iChange > 0)
	{
		// Afforess - AI's trade with AI's much more often
		m_aaiContactTimer[eIndex1][eIndex2] += (iChange / 3);
	}
	else
	{
		m_aaiContactTimer[eIndex1][eIndex2] += iChange;
	}
	FASSERT_NOT_NEGATIVE(m_aaiContactTimer[eIndex1][eIndex2]);
}


int CvPlayerAI::AI_getMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eIndex2);
	return m_aaiMemoryCount[eIndex1][eIndex2];
}


void CvPlayerAI::AI_changeMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eIndex2);

	m_aaiMemoryCount[eIndex1][eIndex2] += iChange;

	if (eIndex1 == GC.getGame().getActivePlayer())
	{
		// BUG - Update Attitude Icons
		gDLL->getInterfaceIFace()->setDirty((InterfaceDirtyBits)(Score_DIRTY_BIT), true);
	}
	FASSERT_NOT_NEGATIVE(AI_getMemoryCount(eIndex1, eIndex2));
}

int CvPlayerAI::AI_calculateGoldenAgeValue() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		iValue += (
			GC.getYieldInfo((YieldTypes)iI).getGoldenAgeYield() * AI_yieldWeight((YieldTypes)iI)
			/ std::max(1, 1 + GC.getYieldInfo((YieldTypes)iI).getGoldenAgeYieldThreshold())
		);
	}
	iValue *= getTotalPopulation() * getGoldenAgeLength();
	iValue /= 100;

	// Golden Ages Reduce Revolutions
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		int iNationalRevIndex = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			iNationalRevIndex += pLoopCity->getRevolutionIndex();
		}
		iNationalRevIndex /= std::max(1, getNumCities());

		iValue *= std::max(1, iNationalRevIndex / 500);
	}
	return iValue;
}

// Protected Functions...

void CvPlayerAI::AI_doCounter()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam(), false)
		&& GET_TEAM(getTeam()).isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
		{
			if (getStateReligion() != NO_RELIGION
			&&  getStateReligion() == GET_PLAYER((PlayerTypes)iI).getStateReligion())
			{
				AI_changeSameReligionCounter((PlayerTypes)iI, 1);
			}
			else if (AI_getSameReligionCounter((PlayerTypes)iI) > 0)
			{
				AI_changeSameReligionCounter((PlayerTypes)iI, -1);
			}

			if (getStateReligion() != NO_RELIGION
			&&  getStateReligion() != GET_PLAYER((PlayerTypes)iI).getStateReligion()
			&& GET_PLAYER((PlayerTypes)iI).getStateReligion() != NO_RELIGION)
			{
				AI_changeDifferentReligionCounter((PlayerTypes)iI, 1);
			}
			else if (AI_getDifferentReligionCounter((PlayerTypes)iI) > 0)
			{
				AI_changeDifferentReligionCounter((PlayerTypes)iI, -1);
			}

			if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() != NO_CIVIC)
			{
				if (isCivic((CivicTypes) GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic())
				&& GET_PLAYER((PlayerTypes)iI).isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()))
				{
					AI_changeFavoriteCivicCounter(((PlayerTypes)iI), 1);
				}
				else if (AI_getFavoriteCivicCounter((PlayerTypes)iI) > 0)
				{
					AI_changeFavoriteCivicCounter(((PlayerTypes)iI), -1);
				}
			}

			const int iBonusImports = getNumTradeBonusImports((PlayerTypes)iI);

			if (iBonusImports > 0)
			{
				AI_changeBonusTradeCounter(((PlayerTypes)iI), iBonusImports);
			}
			else
			{
				AI_changeBonusTradeCounter(((PlayerTypes)iI), -(std::min(AI_getBonusTradeCounter((PlayerTypes)iI), ((GET_PLAYER((PlayerTypes)iI).getNumCities() / 4) + 1))));
			}
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
			{
				if (AI_getContactTimer(((PlayerTypes)iI), ((ContactTypes)iJ)) > 0)
				{
					AI_changeContactTimer(((PlayerTypes)iI), ((ContactTypes)iJ), -1);
				}
			}
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
			{
				if (AI_getMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ) > 0
				&& GC.getLeaderHeadInfo(getPersonalityType()).getMemoryDecayRand((MemoryTypes)iJ) > 0)
				{
					// Afforess - 04/26/14 - Ruthless AI: Easier for the AI to forget past wrongs
					//	AI attitude is designed to make AI players feel human, but it makes them weak
					//	A Perfect AI treats enemies and friends alike, both are obstacles to victory
					int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getMemoryDecayRand((MemoryTypes)iJ);

					if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_REALISTIC_DIPLOMACY))
					{
						iRand /= 1 + AI_getMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ);
					}

					if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
					{
						iRand /= 3;
					}

					if (GC.getGame().getSorenRandNum(iRand, "Memory Decay") == 0)
					{
						AI_changeMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ, -1);
					}
				}
			}
		}
	}
}

void CvPlayerAI::AI_doMilitary()
{
	PROFILE_FUNC();

	// Afforess - add multiple passes
	if (AI_isFinancialTrouble() && !GET_TEAM(getTeam()).hasWarPlan(true))
	{
		const short iSafePercent = AI_safeFunding();
		int aiOwnCommerces[NUM_COMMERCE_TYPES];
		getCommerces(aiOwnCommerces);
		int64_t iNetIncome = aiOwnCommerces[COMMERCE_GOLD] / 100 + std::max(0, getGoldPerTurn());
		int64_t iNetExpenses;

		for (int iPass = 0; iPass < 4; iPass++)
		{
			short iProfitMargin = getProfitMargin(iNetExpenses);

			while (iNetIncome < iNetExpenses && iProfitMargin < iSafePercent && getUnitUpkeepMilitaryNet() > 0)
			{
				int iExperienceThreshold;
				switch (iPass)
				{
				case 0: iExperienceThreshold = 1; break;
				case 1: iExperienceThreshold = 6; break;
				case 2: iExperienceThreshold = 12; break;
				case 3: iExperienceThreshold = -1; break;
				}
				if (!AI_disbandUnit(iExperienceThreshold))
				{
					break;
				}
				// Recalculate funding
				iProfitMargin = getProfitMargin(iNetExpenses);
				int aiOwnCommerces[NUM_COMMERCE_TYPES];
				getCommerces(aiOwnCommerces);
				iNetIncome = aiOwnCommerces[COMMERCE_GOLD] / 100 + std::max(0, getGoldPerTurn());
			}
		}
	}
	AI_setAttackOddsChange(
		  GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttackOddsChange()
		+ GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getAttackOddsChangeRand(), "AI Attack Odds Change #1")
		+ GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getAttackOddsChangeRand(), "AI Attack Odds Change #2")
	);
}


void CvPlayerAI::AI_doCommerce()
{
	PROFILE_FUNC();

	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	if (isNPC() || getAnarchyTurns() > 0)
	{
		return;
	}
	int64_t iGoldTarget = AI_goldTarget();

	const bool bFlexResearch = isCommerceFlexible(COMMERCE_RESEARCH);
	const bool bFlexCulture = isCommerceFlexible(COMMERCE_CULTURE);
	const bool bFlexEspionage = isCommerceFlexible(COMMERCE_ESPIONAGE);

	const TechTypes eCurrentResearch = getCurrentResearch();
	if (bFlexResearch && eCurrentResearch != NO_TECH && !AI_avoidScience())
	{
		// Set research rate to 100%
		setCommercePercent(COMMERCE_RESEARCH, 100);

		// If we can finish the current research without spending a third of our gold, lower gold target by a third.
		const int iGoldRate = calculateGoldRate();
		if (iGoldRate < 0 && getGold() / 3 >= getResearchTurnsLeft(eCurrentResearch, true) * iGoldRate)
		{
			iGoldTarget *= 2;
			iGoldTarget /= 3;
		}
	}

	bool bReset = false;

	if (bFlexCulture && getCommercePercent(COMMERCE_CULTURE) > 0)
	{
		setCommercePercent(COMMERCE_CULTURE, 0);
		bReset = true;
	}

	if (bFlexEspionage)
	{
		// Reset espionage spending always
		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; ++iTeam)
		{
			setEspionageSpendingWeightAgainstTeam((TeamTypes)iTeam, 0);
		}
		if (getCommercePercent(COMMERCE_ESPIONAGE) > 0)
		{
			setCommercePercent(COMMERCE_ESPIONAGE, 0);
			bReset = true;
		}
	}

	if (bReset)
	{
		AI_assignWorkingPlots();
	}
	const int iIncrement = std::max(1, GC.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"));

	if (bFlexCulture && getNumCities() > 0)
	{
		int iIdealPercent = 0;

		if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		{
			iIdealPercent = 100;
		}
		else
		{
			// The per-city term is "how much culture RATE would clear this city's anger", which needs the
			// happiness contributed PER SLIDER POINT -- the `per:{CULTURE_RATE, each:100}` share of a happiness
			// deposit. That share resolves at gather and no read exposes it, so the ideal rate is unanswerable
			// and stays 0; only the culture-victory branch above moves the slider meanwhile.
			iIdealPercent = 0;
		}
		setCommercePercent(COMMERCE_CULTURE, iIdealPercent);
	}
	const bool bFirstTech = AI_isFirstTech(eCurrentResearch);
	const int iTargetTurns = CvGameSpeedScale::speedPercent() / 10;
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);

	if (bFlexResearch)
	{
		if (!bFirstTech && (isNoResearchAvailable() || AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4)))
		{
			setCommercePercent(COMMERCE_RESEARCH, 0);
		}
		else if (!bFirstTech)
		{
			if (AI_avoidScience())
			{
				changeCommercePercent(COMMERCE_RESEARCH, -10);
			}
			if (team.getChosenWarCount(true) > 0 || team.getWarPlanCount(WARPLAN_ATTACKED_RECENT, true) > 0)
			{
				changeCommercePercent(COMMERCE_RESEARCH, -5);
			}
			const int iOldPercent = getCommercePercent(COMMERCE_RESEARCH);
			int aiOwnCommerces[NUM_COMMERCE_TYPES];
			getCommerces(aiOwnCommerces);
			const int iOldGoldRate = aiOwnCommerces[COMMERCE_GOLD] / 100;
			getCommerces(aiOwnCommerces);   // declared above in this scope
			const int iOldBeakerRate = aiOwnCommerces[COMMERCE_RESEARCH] / 100;

			int iInc = iIncrement;
			int iCount = 0;
			int iGoldRate = calculateGoldRate();
			while (getGold() + iTargetTurns * iGoldRate < iGoldTarget)
			{
				if ((bFirstTech || ++iCount > 3 && getCommercePercent(COMMERCE_RESEARCH) < 50) && iGoldRate > 0)
				{
					break; // Don't sacrifice too much science to reach gold target.
				}
				int aiOwnCommerces[NUM_COMMERCE_TYPES];
				getCommerces(aiOwnCommerces);
				const int iPrevGoldRate = aiOwnCommerces[COMMERCE_GOLD] / 100;
				changeCommercePercent(COMMERCE_RESEARCH, -iInc);
				getCommerces(aiOwnCommerces);
				if (iPrevGoldRate == aiOwnCommerces[COMMERCE_GOLD] / 100)
				{
					changeCommercePercent(COMMERCE_RESEARCH, iInc);
					if (getCommercePercent(COMMERCE_RESEARCH) == iInc)
					{
						break;
					}
					iInc += iIncrement;
				}
				else if (getCommercePercent(COMMERCE_RESEARCH) == 0)
				{
					if (calculateGoldRate() >= 0)
					{
						setCommercePercent(COMMERCE_RESEARCH, iIncrement);
						if (calculateGoldRate() < 1)
						{
							setCommercePercent(COMMERCE_RESEARCH, 0);
						}
					}
					break;
				}
				iGoldRate = calculateGoldRate();
			}
			const int iNewPercent = getCommercePercent(COMMERCE_RESEARCH);
			if (iNewPercent < iOldPercent)
			{
				int aiOwnCommerces[NUM_COMMERCE_TYPES];
				getCommerces(aiOwnCommerces);
				const int iBeakerLoss = iOldBeakerRate - aiOwnCommerces[COMMERCE_RESEARCH] / 100;
				getCommerces(aiOwnCommerces);
				const int iGoldGain = aiOwnCommerces[COMMERCE_GOLD] / 100 - iOldGoldRate;
				if (
					iBeakerLoss > iGoldGain
				&&
					(
						// 5 % more tax doesn't even give 1 gold (typical early prehistoric)
						iOldPercent - iNewPercent >= 5 * iGoldGain
						// or tradeoff is not close to comparably worth it.
						|| iBeakerLoss > 3 * iGoldGain && !AI_isFinancialTrouble()
						)
				) setCommercePercent(COMMERCE_RESEARCH, iOldPercent);
			}
		}
	}

	if (bFlexEspionage && !bFirstTech)
	{
		int iEspionageTargetRate = 0;
		int* piTarget = new int[MAX_PC_TEAMS];
		int* piWeight = new int[MAX_PC_TEAMS];

		for (int iTeamX = 0; iTeamX < MAX_PC_TEAMS; ++iTeamX)
		{
			piTarget[iTeamX] = 0;
			piWeight[iTeamX] = 0;

			const TeamTypes eTeamX = (TeamTypes)iTeamX;
			if (eTeamX == eTeam || !team.isHasMet(eTeamX) || team.isVassal(eTeamX))
				continue;

			const CvTeam& teamX = GET_TEAM(eTeamX);
			if (teamX.isAlive() && !teamX.isVassal(eTeam))
			{
				int iTheirEspPoints = teamX.getEspionagePointsAgainstTeam(eTeam);
				int iDesiredMissionPoints = 0;

				piWeight[iTeamX] = 10;
				int iRateDivisor = 12;

				if (team.AI_getWarPlan(eTeamX) != NO_WARPLAN)
				{
					iTheirEspPoints *= 3;
					iTheirEspPoints /= 2;

					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.isPassive() && (kMissionInfo.isSeeDemographics() || kMissionInfo.isSeeResearch()))
						{
							const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
							if (iDesiredMissionPoints < iMissionCost)
							{
								iDesiredMissionPoints = iMissionCost;
							}
						}
					}

					iRateDivisor = 10;
					piWeight[iTeamX] = 20;

					if (team.AI_hasCitiesInPrimaryArea(eTeamX))
					{
						piWeight[iTeamX] = 30;
						iRateDivisor = 8;
					}
				}
				else
				{
					const int iAttitude = range(team.AI_getAttitudeVal(eTeamX), -12, 12);

					iTheirEspPoints -= iTheirEspPoints * iAttitude / 24;

					if (iAttitude <= -3)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.isPassive() && (kMissionInfo.isSeeDemographics() || kMissionInfo.isSeeResearch()))
							{
								const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
								if (iDesiredMissionPoints < iMissionCost)
								{
									iDesiredMissionPoints = iMissionCost;
								}
							}
						}
					}
					else if (iAttitude < 3)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.isPassive() && kMissionInfo.isSeeDemographics())
							{
								const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
								if (iDesiredMissionPoints < iMissionCost)
								{
									iDesiredMissionPoints = iMissionCost;
								}
							}
						}
					}
					iRateDivisor += iAttitude / 5;
					piWeight[iTeamX] -= iAttitude / 2;
				}

				const int iDesiredEspPoints = std::max(iTheirEspPoints, iDesiredMissionPoints);
				const int iOurEspPoints = team.getEspionagePointsAgainstTeam(eTeamX);

				piTarget[iTeamX] = (iDesiredEspPoints - iOurEspPoints) / std::max(6, iRateDivisor);

				if (piTarget[iTeamX] > 0)
				{
					iEspionageTargetRate += piTarget[iTeamX];
				}
			}
		}

		for (int iI = 0; iI < MAX_PC_TEAMS; ++iI)
		{
			if (piTarget[iI] > 0)
			{
				piWeight[iI] += (150 * piTarget[iI]) / std::max(4, iEspionageTargetRate);
			}
			else if (piTarget[iI] < 0)
			{
				piWeight[iI] += 2 * piTarget[iI];
			}
			setEspionageSpendingWeightAgainstTeam((TeamTypes)iI, std::max(0, piWeight[iI]));
		}
		SAFE_DELETE_ARRAY(piTarget);
		SAFE_DELETE_ARRAY(piWeight);

		// If economy is weak, neglect espionage spending. Invest hammers into espionage via spies/builds instead.
		if (AI_isFinancialTrouble() || AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
		{
			iEspionageTargetRate = 0;
		}
		else
		{
			iEspionageTargetRate *= (110 - getCommercePercent(COMMERCE_GOLD) * 2);
			iEspionageTargetRate /= 110;

			iEspionageTargetRate *= GC.getLeaderHeadInfo(getLeaderType()).getEspionageWeight();
			iEspionageTargetRate /= 100;

			int iMaxEspionage = AI_isFinancialTrouble() ? 0 : 5;

			if (iMaxEspionage > 0 && team.getChosenWarCount(true) == 0 && team.getWarPlanCount(WARPLAN_ATTACKED_RECENT, true) == 0)
			{
				const int iRank = GC.getGame().getPlayerRank(getID());

				if (iRank < 2)
				{
					iMaxEspionage = 15;
				}
				else if (iRank < 4)
				{
					iMaxEspionage = 10;
				}
			}

			int aiOwnCommerces[NUM_COMMERCE_TYPES];
			getCommerces(aiOwnCommerces);
			while (aiOwnCommerces[COMMERCE_ESPIONAGE] / 100 < iEspionageTargetRate && getCommercePercent(COMMERCE_ESPIONAGE) < iMaxEspionage)
			{
				changeCommercePercent(COMMERCE_RESEARCH, -iIncrement);
				changeCommercePercent(COMMERCE_ESPIONAGE, iIncrement);

				if (getGold() + iTargetTurns * calculateGoldRate() < iGoldTarget)
				{
					break;
				}
				if (!AI_avoidScience() && !isNoResearchAvailable()
				// Keep Espionage percent somewhat lower than research percent
				&& getCommercePercent(COMMERCE_RESEARCH) * 2 <= 3 * (getCommercePercent(COMMERCE_ESPIONAGE) + iIncrement))
				{
					break;
				}
			}
		}
	}

	if (!bFirstTech && getGold() < iGoldTarget && getCommercePercent(COMMERCE_RESEARCH) > 40)
	{
		for (int iHurry = 0; iHurry < GC.getNumHurryInfos(); iHurry++)
		{
			if (GC.getHurryInfo((HurryTypes)iHurry).getGoldPerProduction() > 0 && canHurry((HurryTypes)iHurry))
			{
				(
					(getCommercePercent(COMMERCE_ESPIONAGE) > 0)
					?
					changeCommercePercent(COMMERCE_ESPIONAGE, -iIncrement)
					:
					changeCommercePercent(COMMERCE_RESEARCH, -iIncrement)
				);
				break;
			}
		}
	}
	// this is called on doTurn, so make sure our gold is high enough keep us above zero gold.
	verifyGoldCommercePercent();
}


void CvPlayerAI::AI_doCivics()
{
	PROFILE_FUNC();
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");


	m_turnsSinceLastRevolution++;
	m_iCivicSwitchMinDeltaThreshold = (m_iCivicSwitchMinDeltaThreshold * 95) / 100;

	if (isNPC())
	{
		return;
	}

	if (AI_getCivicTimer() > 0)
	{
		AI_changeCivicTimer(-1);
		return;
	}

	if (!canRevolution(NULL))
	{
		return;
	}
	CivicTypes* paeBestCivic = NULL;
	int* paeBestCivicValue = NULL;
	int* paeBestNearFutureCivicValue = NULL;
	int* paeCurCivicValue = NULL;
	int* paiAvailableChoices = NULL;
	int iCurCivicsValue = 0;
	int iBestCivicsValue = 0;

	m_iCityGrowthValueBase = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		int iCityHappy = pLoopCity->netHappiness();
		int iCurrentFoodToGrow = pLoopCity->growthThreshold();
		// reduced at this use: it DIVIDES the whole-unit growth threshold below
		int iFoodPerTurn = pLoopCity->foodDifference(false, true, true) / 100;
		int iCityValue = 0;

		

		iCityValue = iCurrentFoodToGrow;

		int iFoodDiffDivisor = std::abs(iFoodPerTurn - 1) + 5;
		int iHappyDivisor = std::max(1, -iCityHappy + 1) + 4;

//#if 0
//		//	We Always count at least 3 food per turn on any city we evaluate at all, and want to
//		//	evaluate any that are near suplus.  This is to promote civic stability, since small
//		//	health changes are likely in any civic switch and we don't want them to move a city
//		//	from not counting at all to counting a lot
//		if (iFoodPerTurn > -2)
//		{
//			if (iCityHappy >= 0)
//			{
//				//	We look at the food difference without trade yields because otherwise civic switches that change the trade
//				//	yield can wildly distort the value of growth.
//				iCityValue = (std::min(3, iCityHappy + 1) * iCurrentFoodToGrow) / std::max(3, iFoodPerTurn - pLoopCity->getTradeYield(YIELD_FOOD));
//				if (gPlayerLogLevel > 1)
//				{
//					LOG_BBAI_PLAYER(2, ("Player %d (%S) city %S growth value %d",
//							getID(),
//							getCivilizationDescription(0),
//							pLoopCity->getName().c_str(),
//							iCityValue);
//				}
//			}
//		}
//#else
		iCityValue = (iCityValue * 10) / (iFoodDiffDivisor + iHappyDivisor);
		
//#endif

		m_iCityGrowthValueBase += iCityValue;
	}

	FAssertMsg(AI_getCivicTimer() == 0, "AI Civic timer is expected to be 0");

	paeBestCivic = new CivicTypes[GC.getNumCivicOptionInfos()];
	paeBestCivicValue = new int[GC.getNumCivicOptionInfos()];
	paeCurCivicValue = new int[GC.getNumCivicOptionInfos()];
	paeBestNearFutureCivicValue = new int[GC.getNumCivicOptionInfos()];
	paiAvailableChoices = new int[GC.getNumCivicOptionInfos()];

	bool bDoRevolution = false;
	int iCurValue;
	int iBestValue;

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}

	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		paiAvailableChoices[iI] = 0;
	}

	// #430 F2b (enabler.md par.6): count over the enabler's LISTED civic frontier. canDoCivics(i) default-args
	// IS the player's LISTED civic frontier, so this is the identical per-option count without the
	// whole-civic-database scan. Order-irrelevant (a pure count accumulation).
	std::vector<int> vecAdoptable;
	m_enabler.civics.listedIds(vecAdoptable);
	for (std::vector<int>::const_iterator it = vecAdoptable.begin(), itEnd = vecAdoptable.end(); it != itEnd; ++it)
	{
		const int iCivicOption = GC.getCivicInfo((CivicTypes)*it).getCivicOption();

		FASSERT_BOUNDS(0, GC.getNumCivicOptionInfos(), iCivicOption);

		paiAvailableChoices[iCivicOption]++;
	}
	/*
		//Might be good to have if many civics from different cathegories affect each other much and become available at the same time
		// otherwise this is not needed and therefore commented out
		//To use this, simply uncomment and replace any "iI" or "(CivicOptionTypes)iI" in the rest of this this function
		// with "(paeShuffledCivicOptions[iI])"
		CivicOptionTypes* paeShuffledCivicOptions = new Array[GC.getNumCivicOptionInfos()];
		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			paeShuffledCivicOptions[iI] = (CivicOptionTypes)iI;
		}
		int iNumPermutations = 1;
		for (int iI = GC.getNumCivicOptionInfos(); iI > 1; iI--)
		{
			iNumPermutations *= iI;
		}
		int iPermutation = GC.getGame().getSorenRandNum(iNumPermutations, "AI Civic Option Shuffling");
		//mapping each possible iPermutation to one possible permutation
		int iPermutationWidth;
		CivicOptionTypes eTempShuffleCivicOption;
		for (int iI = GC.getNumCivicOptionInfos(); (iI > 0 && iPermutation > 0); iI--)
		{
			iPermutationWidth = iPermutation % iI;
			iNumPermutations /= iI;
			iPermutation %= iNumPermutations;
			if (iPermutationWidth > 0)
			{
				eTempShuffleCivicOption = paeShuffledCivicOptions[iI];
				paeShuffledCivicOptions[iI] = paeShuffledCivicOptions[(iI+iPermutationWidth)];
				paeShuffledCivicOptions[(iI+iPermutationWidth)] = eTempShuffleCivicOption;
			}
		}
		SAFE_DELETE_ARRAY(paeShuffledCivicOptions); //<- not to be forgotten at the end of this function
	*/

	//initializing
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
	}


	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (paiAvailableChoices[iI] > 1)
		{
			//civic option vacuum
			if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
				processCivics(getCivics((CivicOptionTypes)iI), -1, /* bLimited */ true);

			paeBestCivic[iI] = AI_bestCivic((CivicOptionTypes)iI, &iBestValue, /* bCivicOptionVacuum */ true, paeBestCivic);
			paeCurCivicValue[iI] = AI_civicValue(getCivics((CivicOptionTypes)iI), /* bCivicOptionVacuum */ true, paeBestCivic);

			if (paeBestCivic[iI] == NO_CIVIC || iBestValue <= paeCurCivicValue[iI])
			{
				paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
				paeBestCivicValue[iI] = paeCurCivicValue[iI];
			}
			else
			{
				FAssert(paeBestCivic[iI] != 0);
				paeBestCivicValue[iI] = iBestValue;
			}

			if (paeBestCivic[iI] != NO_CIVIC && paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
			{
				
				bDoRevolution = true;
			}

			if (paeBestCivic[iI] != NO_CIVIC)
				processCivics(paeBestCivic[iI], 1, /* bLimited */ true);
		}
		else
		{
			paeBestCivicValue[iI] = -1;	//	Not set
		}
	}

	//repeat? just to be sure we aren't doing anything stupid
	bool bChange = (bDoRevolution);
	int iPass = 0;
	while (bChange && iPass < GC.getNumCivicOptionInfos())
	{
		for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
		{
			m_aiCivicValueCache[iI] = MAX_INT;
		}

		iPass++;
		FAssertMsg(iPass <= 2, "Civic decision takes too long.");
		bChange = false;
		iBestCivicsValue = 0;
		iCurCivicsValue = 0;

		CivicTypes eNewBestCivic;

		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			if (paiAvailableChoices[iI] > 1)
			{
				if (paeBestCivic[iI] != NO_CIVIC)
					processCivics(paeBestCivic[iI], -1, /* bLimited */ true);

				eNewBestCivic = AI_bestCivic((CivicOptionTypes)iI, &iBestValue, /* bCivicOptionVacuum */ true, paeBestCivic);

				iCurValue = AI_civicValue(getCivics((CivicOptionTypes)iI), /* bCivicOptionVacuum */ true, paeBestCivic);

				if (eNewBestCivic == NO_CIVIC || iBestValue < iCurValue)
				{
					if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
					{
						bChange = true;
						paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
						if (eNewBestCivic == NO_CIVIC) //when does this happen?
						{
							paeBestCivicValue[iI] = iCurValue;
							iBestValue = iCurValue;
						}

						
					}
				}
				else
				{
					if (paeBestCivic[iI] != eNewBestCivic)
					{
						bChange = true;
						paeBestCivic[iI] = eNewBestCivic;
						paeBestCivicValue[iI] = iBestValue;

						
					}
				}
				iBestCivicsValue += iBestValue;
				iCurCivicsValue += iCurValue;

				if (paeBestCivic[iI] != NO_CIVIC)
					processCivics(paeBestCivic[iI], 1, /* bLimited */ true);
			}
		}
	}

	//	Put back current civics
	int iCivicChanges = 0;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (paiAvailableChoices[iI] > 1)
		{
			if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
			{
				if (paeBestCivic[iI] != NO_CIVIC)
				{
					iCivicChanges++;
					processCivics(paeBestCivic[iI], -1, /* bLimited */ true);

					
				}

				if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
					processCivics(getCivics((CivicOptionTypes)iI), 1, /* bLimited */ true);
			}
		}
	}

	if (bDoRevolution)
	{
		FAssert(iBestCivicsValue >= iCurCivicsValue);

		// If we have no anarchy or if we're in a golden age that will still be going
		//	next time we get an opportunity to change civs just do it (it's costless)
		if (getMaxAnarchyTurns() != 0
		&& (!isGoldenAge() || GC.getDefineINT("MIN_REVOLUTION_TURNS") >= getGoldenAgeTurns()))
		{
			if (iBestCivicsValue - iCurCivicsValue < m_iCivicSwitchMinDeltaThreshold)
			{
				
				bDoRevolution = false;
			}
			else // Are we close to discovering new civic enablers?  If so should we wait for them?
			{
				bDoRevolution = false;
				while (iCivicChanges > 0 && !bDoRevolution)
				{
					int	iNearFutureCivicsBestValue = iBestCivicsValue;
					int iMaxHorizon = 20 * getCivicAnarchyLength(paeBestCivic);

					for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
					{
						paeBestNearFutureCivicValue[iI] = 0;
					}

					for (int iOptionType = 0; iOptionType < GC.getNumCivicOptionInfos(); iOptionType++)
					{
						bool bTestSwitched = false;

						for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
						{
							const CivicTypes eCivic = static_cast<CivicTypes>(iI);

							if (GC.getCivicInfo(eCivic).getCivicOption() != iOptionType || canDoCivics(eCivic))
							{
								continue;
							}
							const TechTypes eTech = NO_TECH /* civic tech prereq: requires.build, an enabler gate */;

							if (GET_TEAM(getTeam()).isHasTech(eTech) || GC.getTechInfo(eTech).getEra() > getCurrentEra() + 1)
							{
								continue;
							}
							// civic option vacuum
							if (getCivics((CivicOptionTypes)iOptionType) != NO_CIVIC && !bTestSwitched)
							{
								processCivics(getCivics((CivicOptionTypes)iOptionType), -1, /* bLimited */ true);
								bTestSwitched = true;
							}
							const int iNearFutureValue = AI_civicValue(eCivic, /* bCivicOptionVacuum */ true, paeBestCivic);

							if (paeBestCivicValue[iOptionType] == -1)
							{
								// Not calculated yet - do so now
								paeBestCivicValue[iOptionType] = AI_civicValue(paeBestCivic[iOptionType], /* bCivicOptionVacuum */ true, paeBestCivic);
							}

							

							if (iNearFutureValue > paeBestCivicValue[iOptionType])
							{
								const int iTurns = std::max(1, findPathLength(eTech, true) / std::max(1, calculateResearchRate()));

								

								if (iTurns <= iMaxHorizon)
								{
									int weightedDelta = (iNearFutureValue - paeBestCivicValue[iOptionType]) * 20 / (20 + iTurns);

									

									if (weightedDelta > paeBestNearFutureCivicValue[iOptionType])
									{
										iNearFutureCivicsBestValue -= paeBestNearFutureCivicValue[iOptionType];
										iNearFutureCivicsBestValue += weightedDelta;

										paeBestNearFutureCivicValue[iOptionType] = weightedDelta;

										
									}
								}
							}
						}

						if (bTestSwitched)
						{
							processCivics(getCivics((CivicOptionTypes)iOptionType), 1, /* bLimited */ true);
						}
					}

					FAssert(iNearFutureCivicsBestValue >= iBestCivicsValue);

					//	So if the best we can do now is an improvement, and the degree of improvement is greater than
					//	the time-discounted degree of additional improvement we'd get from near future civic just do it now
					bDoRevolution = (iBestCivicsValue > iCurCivicsValue && (iBestCivicsValue - iCurCivicsValue) > (iNearFutureCivicsBestValue - iBestCivicsValue));

					

					if (bDoRevolution)
					{
						//	Factor in lost production/GNP due to anarchy
						int aiOwnCommerces[NUM_COMMERCE_TYPES];
						getCommerces(aiOwnCommerces);
						int iTotalEconomyTurnValue = (aiOwnCommerces[COMMERCE_GOLD] / 100 +
													  aiOwnCommerces[COMMERCE_RESEARCH] / 100 +
													  aiOwnCommerces[COMMERCE_CULTURE] / 100 +
													  aiOwnCommerces[COMMERCE_ESPIONAGE] / 100 +
													  2 * calculateTotalYield(YIELD_PRODUCTION));
						int	iPerTurnEstimatedIncrease = (iBestCivicsValue - iCurCivicsValue);
						int iAnarchyCost = getCivicAnarchyLength(paeBestCivic) * iTotalEconomyTurnValue;
						int iBenfitInTurns = std::min(50, m_turnsSinceLastRevolution) * CvGameSpeedScale::hammerCostPercent() / 100;
						int iBenefit = iPerTurnEstimatedIncrease * iBenfitInTurns;

						

						//	If we won't make it up in (arbitrary number) 50 turns (at standard speed) don't bother
						if (iAnarchyCost > iBenefit)
						{
							bDoRevolution = false;
						}
					}

					if (!bDoRevolution)
					{
						//	Check that a cheaper change is not worthwhile - remove the least-efficient civic option
						//	switch that is in the proposed set and check again
						int					iLowestEfficiency = MAX_INT;
						CivicOptionTypes	eWorstOptionSwitch = NO_CIVICOPTION;

						for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
						{
							if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
							{
								if (paeBestCivic[iI] != NO_CIVIC)
								{
									int anarchyLen = GC.getCivicInfo(paeBestCivic[iI]).getAnarchyLength();

									if (anarchyLen > 0)
									{
										int	efficiency = (paeBestCivicValue[iI] - paeCurCivicValue[iI]) / anarchyLen;

										if (efficiency < iLowestEfficiency)
										{
											iLowestEfficiency = efficiency;
											eWorstOptionSwitch = (CivicOptionTypes)iI;
										}
									}
									else if (iLowestEfficiency == MAX_INT)
									{
										eWorstOptionSwitch = (CivicOptionTypes)iI;
									}
								}
							}
						}

						FAssert(eWorstOptionSwitch != NO_CIVICOPTION);

						paeBestCivic[eWorstOptionSwitch] = getCivics(eWorstOptionSwitch);
						iCivicChanges--;
						iBestCivicsValue -= (paeBestCivicValue[eWorstOptionSwitch] - paeCurCivicValue[eWorstOptionSwitch]);
					}
				}
			}
		}
	}

	if (bDoRevolution && canRevolution(paeBestCivic))
	{
		logDecisionAI(2, "[DAI/civic/best] player=%d (%S) REVOLUTION curValue=%d bestValue=%d",
			getID(), getCivilizationDescription(0), iCurCivicsValue, iBestCivicsValue);
		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
			{
				logDecisionAI(2, "[DAI/civic/best] player=%d option=%d -> %S",
					getID(), iI, GC.getCivicInfo(paeBestCivic[iI]).getDescription());
			}
		}
		m_iCivicSwitchMinDeltaThreshold = (iBestCivicsValue - iCurCivicsValue) * 2;
		revolution(paeBestCivic);
		AI_setCivicTimer((getMaxAnarchyTurns() == 0 || isGoldenAge()) ? GC.getDefineINT("MIN_REVOLUTION_TURNS") : CIVIC_CHANGE_DELAY);
	}
	else
	{
		// AI will re-evaluate whenever it gets a tech anyway, but if it's in a long stagant
		// period have it do so periodically anyway (but not so often as to create a peformance overhead)
		AI_setCivicTimer(CIVIC_CHANGE_DELAY);
	}

	m_iCityGrowthValueBase = -1;

	SAFE_DELETE_ARRAY(paeBestCivic);
	SAFE_DELETE_ARRAY(paeBestCivicValue);
	SAFE_DELETE_ARRAY(paeBestNearFutureCivicValue);
	SAFE_DELETE_ARRAY(paeCurCivicValue);
	SAFE_DELETE_ARRAY(paiAvailableChoices);
}


void CvPlayerAI::AI_doReligion()
{
	PROFILE_FUNC();

	ReligionTypes eBestReligion;

	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	if (isNPC())
	{
		return;
	}

	if (AI_getReligionTimer() > 0)
	{
		AI_changeReligionTimer(-1);
		return;
	}

	if (!canChangeReligion())
	{
		return;
	}

	FAssertMsg(AI_getReligionTimer() == 0, "AI Religion timer is expected to be 0");

	eBestReligion = AI_bestReligion();

	if (eBestReligion == NO_RELIGION)
	{
		eBestReligion = getStateReligion();
	}

	logDecisionAI(1, "[DAI/religion] player=%d (%S) best=%d state=%d willConvert=%d (flRel=%d)",
		getID(), getCivilizationDescription(0), (int)eBestReligion, (int)getStateReligion(),
		canConvert(eBestReligion) ? 1 : 0, AI_getFlavorValue((FlavorTypes)1));

	if (canConvert(eBestReligion))
	{
		convert(eBestReligion);
		AI_setReligionTimer((getMaxAnarchyTurns() == 0) ? (GC.getDefineINT("MIN_CONVERSION_TURNS") * 2) : RELIGION_CHANGE_DELAY);
	}
}


void CvPlayerAI::AI_beginDiplomacy(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	if (isDoNotBotherStatus(ePlayer))
	{
		// Divert AI diplomacy away from the diplomacy screen and induce the appropriate reaction
		// in the AI equivalent to a human rejecting the AI's requests in the interface. There are
		// a number of AI requests that do not need handling and that simply time out. There are
		// also AI requests that occur in CvTeam that induce the diplomacy screen in any case.
		// This diplomacy modification does not alter the AI's characteristics at all and is actually
		// just an interface modification for a player to shut down talks with an AI automatically.
		int ai_request;
		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_RELIGION_PRESSURE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_CONVERT, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_PRESSURE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_REVOLUTION, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_JOIN_WAR, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_STOP_TRADING");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_STOP_TRADING, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_ASK_FOR_HELP");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_REFUSED_HELP, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_REJECTED_DEMAND, ePlayer, -1, -1);
			if (AI_demandRebukedWar(ePlayer))
			{
				this->handleDiploEvent(DIPLOEVENT_DEMAND_WAR, ePlayer, -1, -1);
			}
		}
	}
	else
	{
		gDLL->beginDiplomacy(pDiploParams, (PlayerTypes)ePlayer);
	}
}


void CvPlayerAI::AI_doDiplo()
{
	PROFILE_FUNC();
	PERF_SCOPE("CvPlayerAI::AI_doDiplo", getID());

	FAssert(!isHumanPlayer());
	FAssert(!isMinorCiv());
	FAssert(!isNPC());

	CLLNode<TradeData>* pNode;
	CvDiploParameters* pDiplo;

	CLinkList<TradeData> ourList;
	CLinkList<TradeData> theirList;
	TradeData item;
	BonusTypes eBestReceiveBonus;
	BonusTypes eBestGiveBonus;
	TechTypes eBestReceiveTech;
	TechTypes eBestGiveTech;
	TeamTypes eBestTeam;

	int iBestValue;
	int iOurValue;
	int iLoop;

	bool abContacted[MAX_TEAMS];
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		abContacted[iI] = false;
	}
	const int iRandomTechChoiceSeed = GC.getGame().getSorenRandNum(GC.getNumTechInfos(), "AI trade random tech choice seed");
	stdext::hash_map<int, int> receivableTechs;

	{
		PROFILE("CvPlayerAI::AI_doDiplo.preCalcTechSources");

		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (GET_PLAYER((PlayerTypes)iI).isAlive() && iI != getID()
			&& canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI))
			{
				// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
				// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
				// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
				std::vector<int> tradableTechs;
				getAvailableTechs(tradableTechs);

				for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
				{
					const int iJ = tradableTechs[iAt];
					setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

					if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
					{
						stdext::hash_map<int, int>::const_iterator itr = receivableTechs.find(iJ);

						receivableTechs[iJ] = itr != receivableTechs.end() ? itr->second + 1 : 1;
					}
				}
			}
		}

		for (stdext::hash_map<int, int>::const_iterator itr = receivableTechs.begin(); itr != receivableTechs.end(); ++itr)
		{
			
		}
	}

	for (int iPass = 0; iPass < 2; iPass++)
	{
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (iI == getID() || !GET_PLAYER((PlayerTypes)iI).isAlive())
			{
				continue;
			}
			if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() != (iPass == 1))
			{
				continue;
			}
			

			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				PROFILE("CvPlayerAI::AI_doDiplo.Existing");

				foreach_(CvDeal & kLoopDeal, GC.getGame().deals())
				{
					if (kLoopDeal.isCancelable(getID())
					&& GC.getGame().getGameTurn() - kLoopDeal.getInitialGameTurn() >= getTreatyLength() * 2)
					{
						bool bCancelDeal = false;

						if (kLoopDeal.getFirstPlayer() == getID() && kLoopDeal.getSecondPlayer() == (PlayerTypes)iI)
						{
							if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								for (pNode = kLoopDeal.getFirstTrades()->head(); pNode; pNode = kLoopDeal.getFirstTrades()->next(pNode))
								{
									if (getTradeDenial((PlayerTypes)iI, pNode->m_data) != NO_DENIAL)
									{
										bCancelDeal = true;
										break;
									}
								}
							}
							else if (!AI_considerOffer((PlayerTypes)iI, kLoopDeal.getSecondTrades(), kLoopDeal.getFirstTrades(), -1))
							{
								bCancelDeal = true;
							}
						}
						else if (kLoopDeal.getFirstPlayer() == (PlayerTypes)iI && kLoopDeal.getSecondPlayer() == getID())
						{
							if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								for (pNode = kLoopDeal.getSecondTrades()->head(); pNode; pNode = kLoopDeal.getSecondTrades()->next(pNode))
								{
									if (getTradeDenial(((PlayerTypes)iI), pNode->m_data) != NO_DENIAL)
									{
										bCancelDeal = true;
										break;
									}
								}
							}
							else if (!AI_considerOffer((PlayerTypes)iI, kLoopDeal.getFirstTrades(), kLoopDeal.getSecondTrades(), -1))
							{
								bCancelDeal = true;
							}
						}

						if (bCancelDeal)
						{
							if (canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI) && GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								ourList.clear();
								theirList.clear();

								for (pNode = kLoopDeal.headFirstTradesNode(); (pNode != NULL); pNode = kLoopDeal.nextFirstTradesNode(pNode))
								{
									if (kLoopDeal.getFirstPlayer() == getID())
									{
										ourList.insertAtEnd(pNode->m_data);
									}
									else theirList.insertAtEnd(pNode->m_data);
								}

								for (pNode = kLoopDeal.headSecondTradesNode(); (pNode != NULL); pNode = kLoopDeal.nextSecondTradesNode(pNode))
								{
									if (kLoopDeal.getSecondPlayer() == getID())
									{
										ourList.insertAtEnd(pNode->m_data);
									}
									else theirList.insertAtEnd(pNode->m_data);
								}

								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");

								if (kLoopDeal.isVassalDeal())
								{
									pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_NO_VASSAL"));
									pDiplo->setAIContact(true);

									AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								}
								else
								{
									pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CANCEL_DEAL"));
									pDiplo->setAIContact(true);
									pDiplo->setOurOfferList(theirList);
									pDiplo->setTheirOfferList(ourList);
									AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								}
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}

							if (kLoopDeal.isEmbassy())
							{
								for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
								{
									if (GET_PLAYER((PlayerTypes)iJ).isAlive() && getTeam() == GET_PLAYER((PlayerTypes)iJ).getTeam())
									{
										GET_PLAYER((PlayerTypes)iJ).AI_changeMemoryCount(((PlayerTypes)iI), MEMORY_RECALLED_AMBASSADOR, -AI_getMemoryCount(((PlayerTypes)iI), MEMORY_RECALLED_AMBASSADOR));
									}
								}
							}
							kLoopDeal.kill(); // XXX test this for AI...
						}
					}
				}
			}

			if (canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI))
			{
				PROFILE("CvPlayerAI::AI_doDiplo.CanContact");

				if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam() || GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
				{
					PROFILE("CvPlayerAI::AI_doDiplo.BonusTrade");

					// XXX will it cancel this deal if it loses it's first resource???

					iBestValue = 0;
					eBestGiveBonus = NO_BONUS;

					for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
					{
						if (getNumTradeableBonuses((BonusTypes)iJ) > 1
						&& GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal((BonusTypes)iJ, getID(), 1) > 0
						&& GET_PLAYER((PlayerTypes)iI).AI_bonusVal((BonusTypes)iJ, 1) > AI_bonusVal((BonusTypes)iJ, -1))
						{
							setTradeItem(&item, TRADE_RESOURCES, iJ);

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Bonus Trading #1");

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestGiveBonus = ((BonusTypes)iJ);
								}
							}
						}
					}

					if (eBestGiveBonus != NO_BONUS)
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
						ourList.insertAtEnd(item);

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
								pDiplo->setAIContact(true);
								pDiplo->setTheirOfferList(ourList);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
					}
				}

				if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
				{
					PROFILE("CvPlayerAI::AI_doDiplo.TechTrade");

					iBestValue = 0;
					eBestGiveTech = NO_TECH;

					// Don't give techs for free to advanced vassals ...
					if (GET_PLAYER((PlayerTypes)iI).getTechScore() * 10 < getTechScore() * 9)
					{
						// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
						// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
						// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
						std::vector<int> tradableTechs;
						GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

						for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
						{
							const int iJ = tradableTechs[iAt];
							if (GET_TEAM(getTeam()).AI_techTrade((TechTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam()) == NO_DENIAL)
							{
								setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

								if (canTradeItem(((PlayerTypes)iI), item, true))
								{
									const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Vassal Tech gift");

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestGiveTech = ((TechTypes)iJ);
									}
								}
							}
						}
					}

					if (eBestGiveTech != NO_TECH)
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
						ourList.insertAtEnd(item);

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
								pDiplo->setAIContact(true);
								pDiplo->setTheirOfferList(ourList);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
					}
				}

				if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && !GET_TEAM(getTeam()).isHuman() && (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || !GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHuman()))
				{
					FAssertMsg(!(GET_PLAYER((PlayerTypes)iI).isNPC()), "(GET_PLAYER((PlayerTypes)iI).isNPC()) did not return false as expected");
					FAssertMsg(iI != getID(), "iI is not expected to be equal with getID()");

					if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
					{
						PROFILE("CvPlayerAI::AI_doDiplo.Vasal.BonusTrade");

						iBestValue = 0;
						eBestGiveBonus = NO_BONUS;

						for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
						{
							if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 0 && getNumAvailableBonuses((BonusTypes)iJ) == 0)
							{
								const int iValue = AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestGiveBonus = ((BonusTypes)iJ);
								}
							}
						}

						if (eBestGiveBonus != NO_BONUS)
						{
							theirList.clear();
							ourList.clear();

							setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
							theirList.insertAtEnd(item);

							if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
								{
									CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_VASSAL_GRANT_TRIBUTE, getID(), eBestGiveBonus);
									if (pInfo)
									{
										gDLL->getInterfaceIFace()->addPopup(pInfo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}
							}
							else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
						}
					}

					if (!GET_TEAM(getTeam()).isAtWar(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						if (AI_getAttitude((PlayerTypes)iI) >= ATTITUDE_CAUTIOUS)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Cities");

							foreach_(const CvCity * pLoopCity, cities())
							{
								if (pLoopCity->getPreviousOwner() != (PlayerTypes)iI
								&& (pLoopCity->getGameTurnAcquired() + 4) % 20 == GC.getGame().getGameTurn() % 20)
								{
									int iCount = 0;
									int iPossibleCount = 0;

									for (int iJ = 0; iJ < NUM_CITY_PLOTS; iJ++)
									{
										CvPlot* pLoopPlot = plotCity(pLoopCity->getX(), pLoopCity->getY(), iJ);

										if (pLoopPlot != NULL)
										{
											if (pLoopPlot->getOwner() == iI)
											{
												iCount++;
											}
											iPossibleCount++;
										}
									}

									if (iCount >= iPossibleCount / 2)
									{
										setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

										if (canTradeItem(((PlayerTypes)iI), item, true))
										{
											ourList.clear();
											ourList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_CITY"));
												pDiplo->setAIContact(true);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
											else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, NULL);
										}
									}
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.PermAlliance");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE) == 0)
							{
								bool bOffered = false;
								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PERMANENT_ALLIANCE), "AI Diplo Alliance") == 0)
								{
									setTradeItem(&item, TRADE_PERMANENT_ALLIANCE);

									if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
									{
										ourList.clear();
										theirList.clear();

										ourList.insertAtEnd(item);
										theirList.insertAtEnd(item);

										bOffered = true;

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PERMANENT_ALLIANCE));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											break; // move on to next player since we are on the same team now
										}
									}
								}

								if (!bOffered)
								{
									setTradeItem(&item, TRADE_VASSAL);

									if (canTradeItem((PlayerTypes)iI, item, true))
									{
										ourList.clear();
										theirList.clear();

										ourList.insertAtEnd(item);

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PERMANENT_ALLIANCE));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_VASSAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											const TeamTypes eMasterTeam = GET_PLAYER((PlayerTypes)iI).getTeam();
											bool bAccepted = true;

											for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
											{
												if (GET_TEAM((TeamTypes)iJ).isAlive()
												&& iJ != getTeam() && iJ != eMasterTeam
												&& atWar(getTeam(), (TeamTypes)iJ)
												&& !atWar(eMasterTeam, (TeamTypes)iJ)
												&& GET_TEAM(eMasterTeam).AI_declareWarTrade((TeamTypes)iJ, getTeam(), false) != NO_DENIAL)
												{
													bAccepted = false;
													break;
												}
											}
											if (bAccepted)
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Religion");

							if (getStateReligion() != NO_RELIGION
							&& GET_PLAYER((PlayerTypes)iI).canConvert(getStateReligion())
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_RELIGION_PRESSURE) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_RELIGION_PRESSURE), "AI Diplo Religion Pressure") == 0
							&& !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_RELIGION_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_RELIGION_PRESSURE));
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_RELIGION_PRESSURE"));
								pDiplo->setAIContact(true);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Civic");

							const CivicTypes eFavoriteCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();

							if (eFavoriteCivic != NO_CIVIC && isCivic(eFavoriteCivic)
							&& GET_PLAYER((PlayerTypes)iI).canDoCivics(eFavoriteCivic)
							&& !GET_PLAYER((PlayerTypes)iI).isCivic(eFavoriteCivic)
							&& GET_PLAYER((PlayerTypes)iI).canRevolution(NULL)
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_CIVIC_PRESSURE) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_CIVIC_PRESSURE), "AI Diplo Civic Pressure") == 0
							&& !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_CIVIC_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_CIVIC_PRESSURE));
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_PRESSURE"), GC.getCivicInfo(eFavoriteCivic).getTextKeyWide());
								pDiplo->setAIContact(true);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && GC.getDefineINT("CAN_TRADE_WAR") > 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.WarWith");

							if (AI_getMemoryCount(((PlayerTypes)iI), MEMORY_DECLARED_WAR) == 0 && AI_getMemoryCount(((PlayerTypes)iI), MEMORY_HIRED_WAR_ALLY) == 0)
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_JOIN_WAR) == 0)
								{
									int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_JOIN_WAR);
									AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
									if (eAttitude != ATTITUDE_FRIENDLY)
									{
										iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
									}
									if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Join War") == 0)
									{
										iBestValue = 0;
										eBestTeam = NO_TEAM;

										for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
										{
											if (GET_TEAM((TeamTypes)iJ).isAlive()
											&& atWar((TeamTypes)iJ, getTeam())
											&& !atWar((TeamTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam())
											&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHasMet((TeamTypes)iJ)
											&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).canDeclareWar((TeamTypes)iJ))
											{
												const int iValue = (1 + GC.getGame().getSorenRandNum(10000, "AI Joining War"));

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestTeam = ((TeamTypes)iJ);
												}
											}
										}

										if (eBestTeam != NO_TEAM && !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											m_eDemandWarAgainstTeam = eBestTeam;

											AI_changeContactTimer((PlayerTypes)iI, CONTACT_JOIN_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_JOIN_WAR));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
											pDiplo->setAIContact(true);
											pDiplo->setData(eBestTeam);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.StopTradingWith");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_STOP_TRADING) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_STOP_TRADING);
								const AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
								if (eAttitude != ATTITUDE_FRIENDLY)
								{
									iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
								}
								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Stop Trading") == 0)
								{
									if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
									{
										eBestTeam = AI_bestStopTradeTeam((PlayerTypes)iI);
									}
									else
									{
										eBestTeam = GET_TEAM(getTeam()).AI_getWorstEnemy();
									}

									if (eBestTeam != NO_TEAM && GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHasMet(eBestTeam)
									&& !GET_TEAM(eBestTeam).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())
									&& GET_PLAYER((PlayerTypes)iI).canStopTradingWithTeam(eBestTeam))
									{
										FAssert(!atWar(GET_PLAYER((PlayerTypes)iI).getTeam(), eBestTeam));
										FAssert(GET_PLAYER((PlayerTypes)iI).getTeam() != eBestTeam);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_STOP_TRADING, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_STOP_TRADING));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_STOP_TRADING"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
											pDiplo->setAIContact(true);
											pDiplo->setData(eBestTeam);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Help");

							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getAssets() < GET_TEAM(getTeam()).getAssets() / 2
							&& AI_getAttitude((PlayerTypes)iI) > GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_NO_GIVE_HELP)
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_GIVE_HELP) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_GIVE_HELP), "AI Diplo Give Help") == 0)
							{
								// XXX maybe do gold instead???

								iBestValue = 0;
								eBestGiveTech = NO_TECH;

								// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
								// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
								// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
								std::vector<int> tradableTechs;
								GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

								for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
								{
									const int iJ = tradableTechs[iAt];
									setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

									if (canTradeItem((PlayerTypes)iI, item, true))
									{
										const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Giving Help");

										if (iValue > iBestValue)
										{
											iBestValue = iValue;
											eBestGiveTech = ((TechTypes)iJ);
										}
									}
								}

								if (eBestGiveTech != NO_TECH)
								{
									ourList.clear();

									setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
									ourList.insertAtEnd(item);

									if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
									{
										AI_changeContactTimer(((PlayerTypes)iI), CONTACT_GIVE_HELP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_GIVE_HELP));
										pDiplo = new CvDiploParameters(getID());
										FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
										pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
										pDiplo->setAIContact(true);
										pDiplo->setTheirOfferList(ourList);
										AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AskHelp");

							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getAssets() > GET_TEAM(getTeam()).getAssets() / 2
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_ASK_FOR_HELP) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_ASK_FOR_HELP);
								const int iTechPerc = GET_TEAM(getTeam()).getBestKnownTechScorePercent();
								if (iTechPerc < 90)
								{
									iRand *= std::max(1, iTechPerc - 60);
									iRand /= 30;
								}

								//Afforess make unfriendly AI's less likely to ask for help
								AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
								if (eAttitude != ATTITUDE_FRIENDLY)
								{
									iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
								}

								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Ask For Help") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									// The receivable set is OUR frontier (canTradeItem's own last clause), not the
									// tech database; the random start is kept so the pick is not id-ordered.
									std::vector<int> tradableTechs;
									getAvailableTechs(tradableTechs);

									for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
									{
										const TechTypes eCandidateTech = static_cast<TechTypes>(tradableTechs[(static_cast<size_t>(iRandomTechChoiceSeed) + iAt) % tradableTechs.size()]);
										setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											eBestReceiveTech = eCandidateTech;
											break;
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_ASK_FOR_HELP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_ASK_FOR_HELP));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_ASK_FOR_HELP"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && GET_TEAM(getTeam()).getLeaderID() == getID()
						&& GET_TEAM(getTeam()).canDeclareWar(GET_PLAYER((PlayerTypes)iI).getTeam())
						&& !GET_TEAM(getTeam()).AI_isChosenWar(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Tribute");

							//Afforess changed to check if we are at least 1.5x as powerful
							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true) * 3 < GET_TEAM(getTeam()).getPower(true) * 2
							&& AI_getAttitude((PlayerTypes)iI) <= GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DEMAND_TRIBUTE)
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE) == 0)
							{
								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									int64_t iReceiveGold = std::min<int64_t>(std::max<int64_t>(0, GET_PLAYER((PlayerTypes)iI).getGold() - 50), GET_PLAYER((PlayerTypes)iI).AI_goldTarget());

									iReceiveGold -= (iReceiveGold % GC.getDIPLOMACY_VALUE_REMAINDER());

									if (iReceiveGold > 50)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_GOLD, iReceiveGold > MAX_INT ? MAX_INT : (int)iReceiveGold);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0
								&& GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER((PlayerTypes)iI).getTeam()) > 100)
								{
									theirList.clear();

									setTradeItem(&item, TRADE_MAPS);
									theirList.insertAtEnd(item);

									if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
									{
										AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
										pDiplo = new CvDiploParameters(getID());
										FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
										pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
										pDiplo->setAIContact(true);
										pDiplo->setOurOfferList(theirList);

										AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									// A tech is tradable only if it is LISTED on the RECEIVER's frontier -- canTradeItem's own last
									// clause. So the frontier IS the candidate set; probing all ~943 techs to rediscover it is the
									// whole-database sweep enabler.md §6 deletes, and it leans on legacy gates that are going away.
									std::vector<int> tradableTechs;
									getAvailableTechs(tradableTechs);

									for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
									{
										const int iJ = tradableTechs[iAt];
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true)
										&& GC.getGame().countKnownTechNumTeams((TechTypes)iJ) > 1)
										{
											const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Demanding Tribute (Tech)");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestReceiveTech = ((TechTypes)iJ);
											}
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									iBestValue = 0;
									eBestReceiveBonus = NO_BONUS;

									for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
									{
										if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 1
										&& AI_bonusTradeVal(((BonusTypes)iJ), ((PlayerTypes)iI), 1) > 0)
										{
											setTradeItem(&item, TRADE_RESOURCES, iJ);

											if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Demanding Tribute (Bonus)");

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestReceiveBonus = ((BonusTypes)iJ);
												}
											}
										}
									}

									if (eBestReceiveBonus != NO_BONUS)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_RESOURCES, eBestReceiveBonus);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.OpenBorders");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Open Borders") == 0)
							{
								setTradeItem(&item, TRADE_OPEN_BORDERS);

								if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									ourList.clear();
									theirList.clear();

									ourList.insertAtEnd(item);
									theirList.insertAtEnd(item);

									if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
									{
										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											pDiplo->setTheirOfferList(ourList);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
									else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.DefensivePact");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_DEFENSIVE_PACT) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEFENSIVE_PACT), "AI Diplo Defensive Pact") == 0)
							{
								setTradeItem(&item, TRADE_DEFENSIVE_PACT);

								if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									ourList.clear();
									theirList.clear();

									ourList.insertAtEnd(item);
									theirList.insertAtEnd(item);

									if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
									{
										if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEFENSIVE_PACT, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEFENSIVE_PACT));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											pDiplo->setTheirOfferList(ourList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
									else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
								}
							}
						}

						if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.TradeTechNonHuman");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_TECH) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_TECH);
								const int iTechPerc = GET_TEAM(getTeam()).getBestKnownTechScorePercent();
								if (iTechPerc < 90)
								{
									iRand *= std::max(1, iTechPerc - 60);
									iRand /= 30;
								}
								if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE1))
								{
									iRand /= 2;
								}

								iRand = std::max(1, iRand);
								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Trade Tech") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
									// never rediscovered by probing every tech in the database.
									std::vector<int> tradableTechs;
									getAvailableTechs(tradableTechs);

									for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
									{
										const TechTypes eCandidateTech = static_cast<TechTypes>(tradableTechs[(static_cast<size_t>(iRandomTechChoiceSeed) + iAt) % tradableTechs.size()]);
										setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											stdext::hash_map<int, int>::const_iterator itr = receivableTechs.find(eCandidateTech);

											const int iValue = itr != receivableTechs.end() ? itr->second : 0;

											if (eBestReceiveTech == NO_TECH || iValue > iBestValue)
											{
												eBestReceiveTech = eCandidateTech;
												iBestValue = iValue;
											}
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										iBestValue = 0;
										eBestGiveTech = NO_TECH;

										// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
										// never rediscovered by probing every tech in the database.
										std::vector<int> tradableTechs;
										GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

										for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
										{
											const TechTypes eCandidateTech = static_cast<TechTypes>(tradableTechs[(static_cast<size_t>(iRandomTechChoiceSeed) + iAt) % tradableTechs.size()]);
											setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

											if (canTradeItem(((PlayerTypes)iI), item, true))
											{
												eBestGiveTech = eCandidateTech;
												break;
											}
										}

										iOurValue = GET_TEAM(getTeam()).AI_techTradeVal(eBestReceiveTech, GET_PLAYER((PlayerTypes)iI).getTeam());
										int iTheirValue =
											(
												eBestGiveTech != NO_TECH
												?
												GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
												:
												0
											);

										int iReceiveGold = 0;
										int iGiveGold = 0;

										if (iTheirValue > iOurValue)
										{
											const int iValueDiff = iTheirValue - iOurValue;
											const int iGoldValuePercent = AI_goldTradeValuePercent();
											int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
											if (iGold > 0)
											{
												const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

												if (iGold > iMaxTrade)
												{
													iGold = iMaxTrade;
												}
												else
												{
													// Account for rounding errors
													while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
													{
														iGold++;
													}
												}

												if (iGold > 0)
												{
													const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
													if (iValue > 0)
													{
														setTradeItem(&item, TRADE_GOLD, iGold);

														if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
														{
															iReceiveGold = iGold;
															iOurValue += iValue;
														}
													}
												}
											}
										}
										else if (iOurValue > iTheirValue)
										{
											const int iValueDiff = iOurValue - iTheirValue;
											const int iGoldValuePercent = AI_goldTradeValuePercent();
											int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
											if (iGold > 0)
											{
												const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

												if (iGold > iMaxTrade)
												{
													iGold = iMaxTrade;
												}
												else
												{
													// Account for rounding errors
													while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
													{
														iGold++;
													}
												}

												if (iGold > 0)
												{
													const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
													if (iValue > 0)
													{
														setTradeItem(&item, TRADE_GOLD, iGold);

														if (canTradeItem((PlayerTypes)iI, item, true))
														{
															iGiveGold = iGold;
															iTheirValue += iValue;
														}
													}
												}
											}
										}

										if ((!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
										&& iOurValue > iTheirValue * 2 / 3 && iTheirValue > iOurValue * 2 / 3)
										{
											ourList.clear();
											theirList.clear();

											if (eBestGiveTech != NO_TECH)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
												ourList.insertAtEnd(item);
											}

											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
											theirList.insertAtEnd(item);

											if (iGiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iGiveGold);
												ourList.insertAtEnd(item);
											}

											if (iReceiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iReceiveGold);
												theirList.insertAtEnd(item);
											}

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_TECH, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_TECH));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);

													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}

						if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY) || GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AdvancedDiplomacyOrRuthless");

							//Purchase War Ally
							if (
								(
									GET_PLAYER((PlayerTypes)iI).isHumanPlayer()
									||
									GET_TEAM(getTeam()).getLeaderID() == getID()
									&&
									!GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())
									)
							&& AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR) == 0
							&& AI_getMemoryCount((PlayerTypes)iI, MEMORY_HIRED_WAR_ALLY) == 0
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_TRADE_JOIN_WAR) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_JOIN_WAR), "AI Diplo Trade War") == 0)
							{
								const TeamTypes eBestWarTeam = AI_bestJoinWarTeam((PlayerTypes)iI);

								if (eBestWarTeam != NO_TEAM
								&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_declareWarTrade(eBestWarTeam, getTeam(), true) == NO_DENIAL
								&& (
									GET_TEAM(getTeam()).isGoldTrading()
									||
									GET_TEAM(getTeam()).isTechTrading()
									||
									GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isGoldTrading()
									||
									GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isTechTrading()
									)
								)
								{
									iBestValue = 0;
									eBestGiveTech = NO_TECH;

									// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
									// never rediscovered by probing every tech in the database.
									std::vector<int> tradableTechs;
									GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

									for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
									{
										const int iJ = tradableTechs[iAt];
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Tech Trading #2");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestGiveTech = (TechTypes)iJ;
											}
										}
									}

									iOurValue = GET_TEAM(getTeam()).AI_declareWarTradeVal(eBestWarTeam, GET_PLAYER((PlayerTypes)iI).getTeam());
									int iTheirValue =
										(
											eBestGiveTech != NO_TECH
											?
											GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
											:
											0
										);

									int iReceiveGold = 0;
									int iGiveGold = 0;

									if (iTheirValue > iOurValue)
									{
										const int iValueDiff = iTheirValue - iOurValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
													{
														iReceiveGold = iGold;
														iOurValue += iValue;
													}
												}
											}
										}
									}
									else if (iOurValue > iTheirValue)
									{
										const int iValueDiff = iOurValue - iTheirValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (canTradeItem((PlayerTypes)iI, item, true))
													{
														iGiveGold = iGold;
														iTheirValue += iValue;
													}
												}
											}
										}
									}

									if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
									{
										if ((iOurValue > ((iTheirValue * 2) / 3)) && (iTheirValue > ((iOurValue * 2) / 3)))
										{
											ourList.clear();
											theirList.clear();

											if (eBestGiveTech != NO_TECH)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
												ourList.insertAtEnd(item);
											}

											setTradeItem(&item, TRADE_WAR, eBestWarTeam);
											theirList.insertAtEnd(item);

											if (iGiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iGiveGold);
												ourList.insertAtEnd(item);
											}

											if (iReceiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iReceiveGold);
												theirList.insertAtEnd(item);
											}

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													m_eDemandWarAgainstTeam = eBestWarTeam;
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_JOIN_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_JOIN_WAR));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestWarTeam).getLeaderID()).getCivilizationAdjectiveKey());
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_declareWarTrade(eBestWarTeam, getTeam(), true) == NO_DENIAL)
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
											else m_eDemandWarAgainstTeam = eBestWarTeam;
										}
									}
								}
							}
							//Broker Peace
							if (GET_TEAM(getTeam()).getLeaderID() == getID() && !GC.getGame().isPreviousRequest((PlayerTypes)iI))
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_PEACE_PRESSURE) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PEACE_PRESSURE), "AI Diplo End War") == 0)
									{
										eBestTeam = AI_bestMakePeaceTeam((PlayerTypes)iI);

										if (eBestTeam != NO_TEAM)
										{
											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													GC.getGame().setPreviousRequest((PlayerTypes)iI, true);
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_PRESSURE));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_MAKE_PEACE_WITH"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
													pDiplo->setAIContact(true);
													pDiplo->setData(eBestTeam);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Purchase Trade Embargo
							if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || ((GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())))
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_STOP_TRADING) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_JOIN_WAR), "AI Diplo Trade War") == 0)
									{
										const TeamTypes eBestStopTradeTeam = AI_bestStopTradeTeam((PlayerTypes)iI);

										if (eBestStopTradeTeam != NO_TEAM)
										{

											iBestValue = 0;
											eBestGiveTech = NO_TECH;

											// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
											// never rediscovered by probing every tech in the database.
											std::vector<int> tradableTechs;
											GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

											for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
											{
												const int iJ = tradableTechs[iAt];
												setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

												if (canTradeItem((PlayerTypes)iI, item, true))
												{
													const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Tech Trading #2");

													if (iValue > iBestValue)
													{
														iBestValue = iValue;
														eBestGiveTech = ((TechTypes)iJ);
													}
												}
											}

											iOurValue = AI_stopTradingTradeVal(eBestStopTradeTeam, ((PlayerTypes)iI));
											int iTheirValue =
												(
													eBestGiveTech != NO_TECH
													?
													GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
													:
													0
												);
											int iReceiveGold = 0;
											int iGiveGold = 0;

											if (iTheirValue > iOurValue)
											{
												const int iValueDiff = iTheirValue - iOurValue;
												const int iGoldValuePercent = AI_goldTradeValuePercent();
												int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
												if (iGold > 0)
												{
													const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

													if (iGold > iMaxTrade)
													{
														iGold = iMaxTrade;
													}
													else
													{
														// Account for rounding errors
														while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
														{
															iGold++;
														}
													}

													if (iGold > 0)
													{
														const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
														if (iValue > 0)
														{
															setTradeItem(&item, TRADE_GOLD, iGold);

															if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
															{
																iReceiveGold = iGold;
																iOurValue += iValue;
															}
														}
													}
												}
											}
											else if (iOurValue > iTheirValue)
											{
												const int iValueDiff = iOurValue - iTheirValue;
												const int iGoldValuePercent = AI_goldTradeValuePercent();
												int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
												if (iGold > 0)
												{
													const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

													if (iGold > iMaxTrade)
													{
														iGold = iMaxTrade;
													}
													else
													{
														// Account for rounding errors
														while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
														{
															iGold++;
														}
													}

													if (iGold > 0)
													{
														const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
														if (iValue > 0)
														{
															setTradeItem(&item, TRADE_GOLD, iGold);

															if (canTradeItem((PlayerTypes)iI, item, true))
															{
																iGiveGold = iGold;
																iTheirValue += iValue;
															}
														}
													}
												}
											}

											if ((!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
											&& iOurValue > iTheirValue * 2 / 3 && iTheirValue > iOurValue * 2 / 3)
											{
												ourList.clear();
												theirList.clear();

												if (eBestGiveTech != NO_TECH)
												{
													setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
													ourList.insertAtEnd(item);
												}

												setTradeItem(&item, TRADE_EMBARGO, eBestStopTradeTeam);
												theirList.insertAtEnd(item);

												if (iGiveGold != 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGiveGold);
													ourList.insertAtEnd(item);
												}

												if (iReceiveGold != 0)
												{
													setTradeItem(&item, TRADE_GOLD, iReceiveGold);
													theirList.insertAtEnd(item);
												}

												if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
												{
													GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
												}
												else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_STOP_TRADING, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_STOP_TRADING));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
										}
									}
								}
							}
						}
						if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AdvancedDiplomacy");

							//Establish Embassy
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (!GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam()))
								{
									if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_EMBASSY) == 0)
									{
										if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_EMBASSY), "AI Diplo Open Borders") == 0)
										{
											setTradeItem(&item, TRADE_EMBASSY);

											if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												ourList.clear();
												theirList.clear();

												ourList.insertAtEnd(item);
												theirList.insertAtEnd(item);

												if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
												{
													if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
													{
														AI_changeContactTimer(((PlayerTypes)iI), CONTACT_EMBASSY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_EMBASSY));
														pDiplo = new CvDiploParameters(getID());
														FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
														pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
														pDiplo->setAIContact(true);
														pDiplo->setOurOfferList(theirList);
														pDiplo->setTheirOfferList(ourList);
														AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
														abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
													}
												}
												else
												{
													GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
												}
											}
										}
									}
								}
							}
							//Open Free Trade
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Limited Borders") == 0)
									{
										setTradeItem(&item, TRADE_FREE_TRADE_ZONE);

										if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											ourList.clear();
											theirList.clear();

											ourList.insertAtEnd(item);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													// RevolutionDCM start - new diplomacy option
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// gDLL->beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// RevolutionDCM end
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Open Limited Borders
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Limited Borders") == 0)
									{
										setTradeItem(&item, TRADE_RITE_OF_PASSAGE);

										if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											ourList.clear();
											theirList.clear();

											ourList.insertAtEnd(item);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													// RevolutionDCM start - new diplomacy option
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// gDLL->beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// RevolutionDCM end
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Sell contacts to other teams
							if (GET_TEAM(getTeam()).getLeaderID() == getID()
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_TRADE_CONTACTS) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_CONTACTS) / 2, "AI Diplo Trade Contacts") == 0)
							{
								for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
								{
									const TeamTypes eTeamX = static_cast<TeamTypes>(iJ);
									CvTeam& kTeam = GET_TEAM(eTeamX);
									if (kTeam.isAlive() && !kTeam.isMinorCiv())
									{
										setTradeItem(&item, TRADE_CONTACT, eTeamX);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iGold = AI_getGoldFromValue(GET_TEAM(getTeam()).AI_contactTradeVal(eTeamX, GET_PLAYER((PlayerTypes)iI).getTeam()), AI_goldTradeValuePercent());

											if (iGold > 0 && iGold <= GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()))
											{
												setTradeItem(&item, TRADE_GOLD, iGold);

												if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
												{
													ourList.clear();
													theirList.clear();

													setTradeItem(&item, TRADE_CONTACT, eTeamX);
													ourList.insertAtEnd(item);

													setTradeItem(&item, TRADE_GOLD, iGold);
													theirList.insertAtEnd(item);

													if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
													{
														GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
														CvWString szBuffer;
														switch (AI_getAttitude(GET_TEAM(eTeamX).getLeaderID()))
														{
														case ATTITUDE_FURIOUS:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_FURIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_ANNOYED:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_ANNOYED", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_CAUTIOUS:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_CAUTIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_PLEASED:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_PLEASED", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_FRIENDLY:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_FRIENDLY", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														default:
															FErrorMsg("No Valid Attitude");
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_CAUTIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														}
														for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
														{
															if (GET_PLAYER((PlayerTypes)iJ).getTeam() == eTeamX)
															{
																AddDLLMessage((PlayerTypes)iJ, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_FEAT_ACCOMPLISHED", MESSAGE_TYPE_MAJOR_EVENT, NULL, GC.getCOLOR_WHITE());
															}
														}
													}
													else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
													{
														AI_changeContactTimer((PlayerTypes)iI, CONTACT_TRADE_CONTACTS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_CONTACTS));
														pDiplo = new CvDiploParameters(getID());
														FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
														pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
														pDiplo->setAIContact(true);
														pDiplo->setOurOfferList(theirList);
														pDiplo->setTheirOfferList(ourList);
														AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
														abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
													}
												}
											}
											// Yes, this is the first team we can sell contact with...
											break; // Not like it really matters
										}
									}
								}
							}
							//Purchase Workers
							//Why Does this matter?
							//if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								PROFILE("CvPlayerAI::AI_doDiplo.PurchaseWorker");

								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_WORKERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_WORKERS), "AI Diplo Trade Workers") == 0)
									{
										if (GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam()))
										{
											CvUnit* pWorker = NULL;
											int iNeededWorkers = 0;

											//figure out if we need workers or not
											foreach_(CvArea * pLoopArea, GC.getMap().areas())
											{
												if (pLoopArea->getCitiesPerPlayer(getID()) > 0)
												{
													iNeededWorkers += AI_neededWorkers(pLoopArea);
												}
											}
											//if we need workers
											if (iNeededWorkers > 0)
											{
												foreach_(CvUnit * pLoopUnit, GET_PLAYER((PlayerTypes)iI).units())
												{
													if (pLoopUnit->canTradeUnit(getID()))
													{
														setTradeItem(&item, TRADE_WORKER, pLoopUnit->getID());
														//if they can trade the worker to us
														if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
														{
															pWorker = pLoopUnit;
															break;
														}
													}
												}
											}
											if (pWorker != NULL)
											{
												const int iGold = AI_getGoldFromValue(GET_PLAYER((PlayerTypes)iI).AI_workerTradeVal(pWorker), AI_goldTradeValuePercent());

												if (iGold > 0 && AI_maxGoldTrade((PlayerTypes)iI) >= iGold)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);
													//if we can trade the gold to them
													if (canTradeItem((PlayerTypes)iI, item, true))
													{
														ourList.clear();
														theirList.clear();

														ourList.insertAtEnd(item);

														setTradeItem(&item, TRADE_WORKER, pWorker->getID());
														theirList.insertAtEnd(item);

														if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
														{
															if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
															{
																AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_WORKERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_WORKERS));
																pDiplo = new CvDiploParameters(getID());
																FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
																pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
																pDiplo->setAIContact(true);
																pDiplo->setOurOfferList(theirList);
																pDiplo->setTheirOfferList(ourList);
																AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
																abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
															}
														}
														else
														{
															GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
														}
													}
												}
											}
										}
									}
								}
							}

							// Trade military units
							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MILITARY_UNITS) == 0)
							{
								PROFILE("CvPlayerAI::AI_doDiplo.TardeUnits");

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_MILITARY_UNITS)
									/ std::max(1, GET_TEAM(getTeam()).getAnyWarPlanCount(true)), "AI Diplo Trade Military Units") == 0

								&& GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam())
								&& !AI_isFinancialTrouble())
								{
									int* paiMilitaryUnits;
									paiMilitaryUnits = new int[GET_PLAYER((PlayerTypes)iI).getNumUnits()];
									for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
									{
										paiMilitaryUnits[iJ] = -1;
									}
									int iNumTradableUnits = 0;
									CvUnit* pLoopUnit;
									for (iJ = 0, pLoopUnit = GET_PLAYER((PlayerTypes)iI).firstUnit(&iLoop); pLoopUnit != NULL; iJ++, pLoopUnit = GET_PLAYER((PlayerTypes)iI).nextUnit(&iLoop))
									{
										if (pLoopUnit->canTradeUnit(getID()))
										{
											setTradeItem(&item, TRADE_MILITARY_UNIT, pLoopUnit->getID());
											if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												paiMilitaryUnits[iJ] = pLoopUnit->getID();
												iNumTradableUnits++;
											}
										}
									}
									TechTypes eBestTech = NO_TECH;
									int iBestValue = 0;
									if (iNumTradableUnits > 0)
									{
										// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
										// never rediscovered by probing every tech in the database.
										std::vector<int> tradableTechs;
										GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

										for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
										{
											const int iJ = tradableTechs[iAt];
											setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

											if (canTradeItem((PlayerTypes)iI, item, true))
											{
												const int iValue =
													(
														1 + GC.getGame().getSorenRandNum(10000, "AI Tech For Military")
														/
														std::max(1, GC.getTechInfo((TechTypes)iJ).getFlavorValue(GC.getInfoTypeForString("FLAVOR_MILITARY")))
													);
												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestTech = (TechTypes)iJ;
												}
											}
										}
									}
									if (eBestTech != NO_TECH)
									{
										int iUnitValue = 0;
										int iTechValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestTech, getTeam());
										for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
										{
											if (paiMilitaryUnits[iJ] > 0)
											{
												if (iUnitValue > iTechValue)
												{
													paiMilitaryUnits[iJ] = -1;
												}
												else
												{
													iUnitValue += AI_militaryUnitTradeVal(GET_PLAYER((PlayerTypes)iI).getUnit(paiMilitaryUnits[iJ]));
												}
											}
										}

										ourList.clear();
										theirList.clear();

										//Units are worth more than the tech
										if (iUnitValue > iTechValue)
										{
											const int iValueDiff = iUnitValue - iTechValue;
											const int iGold = AI_getGoldFromValue(iValueDiff, AI_goldTradeValuePercent());

											if (iGold > 0 && AI_maxGoldTrade((PlayerTypes)iI) >= iGold)
											{
												setTradeItem(&item, TRADE_GOLD, iGold);
												if (canTradeItem((PlayerTypes)iI, item, true))
												{
													ourList.insertAtEnd(item);
													iTechValue += iValueDiff;
												}
											}
										}
										//The tech is worth more than the units
										else if (iUnitValue < iTechValue)
										{
											const int iValueDiff = iTechValue - iUnitValue;
											const int iGold = AI_getGoldFromValue(iValueDiff, AI_goldTradeValuePercent());

											if (iGold > 0 && GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()) >= iGold)
											{
												setTradeItem(&item, TRADE_GOLD, iGold);
												if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
												{
													theirList.insertAtEnd(item);
													iUnitValue += iValueDiff;
												}
											}
										}
										if (iUnitValue == iTechValue)
										{
											for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
											{
												if (paiMilitaryUnits[iJ] > 0)
												{
													setTradeItem(&item, TRADE_MILITARY_UNIT, paiMilitaryUnits[iJ]);
													theirList.insertAtEnd(item);
												}
											}
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestTech);
											ourList.insertAtEnd(item);

											if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
											else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer((PlayerTypes)iI, CONTACT_TRADE_MILITARY_UNITS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_MILITARY_UNITS));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
									}
									SAFE_DELETE_ARRAY(paiMilitaryUnits);
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BONUS) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.ContactTradeBonus");

							if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_BONUS), "AI Diplo Trade Bonus") == 0)
							{
								iBestValue = 0;
								eBestReceiveBonus = NO_BONUS;

								for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
								{
									if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 1
									&& GET_PLAYER((PlayerTypes)iI).AI_corporationBonusVal((BonusTypes)iJ) == 0
									&& AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1) > 0)
									{
										setTradeItem(&item, TRADE_RESOURCES, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											const int iValue = AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1) + GC.getGame().getSorenRandNum(200, "AI Bonus Trading #1");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestReceiveBonus = ((BonusTypes)iJ);
											}
										}
									}
								}

								if (eBestReceiveBonus != NO_BONUS)
								{
									iBestValue = 0;
									eBestGiveBonus = NO_BONUS;

									for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
									{
										if (iJ != eBestReceiveBonus && getNumTradeableBonuses((BonusTypes)iJ) > 1
										&& GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal((BonusTypes)iJ, getID(), 1) > 0)
										{
											setTradeItem(&item, TRADE_RESOURCES, iJ);

											if (canTradeItem(((PlayerTypes)iI), item, true))
											{
												const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Bonus Trading #2");

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestGiveBonus = (BonusTypes)iJ;
												}
											}
										}
									}

									if (eBestGiveBonus != NO_BONUS)
									{
										if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (AI_bonusTradeVal(eBestReceiveBonus, ((PlayerTypes)iI), -1) >= GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal(eBestGiveBonus, getID(), 1)))
										{
											ourList.clear();
											theirList.clear();

											setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
											ourList.insertAtEnd(item);

											setTradeItem(&item, TRADE_RESOURCES, eBestReceiveBonus);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BONUS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_BONUS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MAP) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.TradeMaps");

							if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_MAP), "AI Diplo Trade Map") == 0)
							{
								setTradeItem(&item, TRADE_MAPS);

								if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true) && canTradeItem(((PlayerTypes)iI), item, true))
								{
									if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER((PlayerTypes)iI).getTeam()) >= GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_mapTradeVal(getTeam())))
									{
										ourList.clear();
										theirList.clear();

										setTradeItem(&item, TRADE_MAPS);
										ourList.insertAtEnd(item);

										setTradeItem(&item, TRADE_MAPS);
										theirList.insertAtEnd(item);

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MAP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_MAP));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BUY_WAR) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.DeclareWar");

							int iDeclareWarTradeRand = GC.getLeaderHeadInfo(getPersonalityType()).getDeclareWarTradeRand();
							int iMinAtWarCounter = MAX_INT;
							for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
							{
								if (GET_TEAM((TeamTypes)iJ).isAlive())
								{
									if (atWar(((TeamTypes)iJ), getTeam()))
									{
										int iAtWarCounter = GET_TEAM(getTeam()).AI_getAtWarCounter((TeamTypes)iJ);
										if (GET_TEAM(getTeam()).AI_getWarPlan((TeamTypes)iJ) == WARPLAN_DOGPILE)
										{
											iAtWarCounter *= 2;
											iAtWarCounter += 5;
										}
										iMinAtWarCounter = std::min(iAtWarCounter, iMinAtWarCounter);
									}
								}
							}

							eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_CONSIDER, 2)
								.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
								.addI(DIPF_minAtWar, iMinAtWarCounter).addI(DIPF_rand, iDeclareWarTradeRand));

							if (iMinAtWarCounter < 10)
							{
								iDeclareWarTradeRand *= iMinAtWarCounter;
								iDeclareWarTradeRand /= 10;
								iDeclareWarTradeRand++;
							}

							if (iMinAtWarCounter < 4)
							{
								iDeclareWarTradeRand /= 4;
								iDeclareWarTradeRand++;
							}

							eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_RANDADJ, 2)
								.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
								.addI(DIPF_minAtWar, iMinAtWarCounter).addI(DIPF_rand, iDeclareWarTradeRand));

							if (GC.getGame().getSorenRandNum(iDeclareWarTradeRand, "AI Diplo Declare War Trade") == 0)
							{
								iBestValue = 0;
								eBestTeam = NO_TEAM;

								eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_PASSRAND, 2)
									.addI(DIPF_actor, getID()).addI(DIPF_target, iI));

								for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
								{
									if (GET_TEAM((TeamTypes)iJ).isAlive()
									&& atWar((TeamTypes)iJ, getTeam())
									&& !atWar((TeamTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam())
									&& GET_TEAM((TeamTypes)iJ).getAtWarCount(true) < std::max(2, GC.getGame().countCivTeamsAlive() / 2))
									{
										setTradeItem(&item, TRADE_WAR, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											const int iValue =
												(
													(1 + GC.getGame().getSorenRandNum(1000, "AI Declare War Trading"))
													*
													(101 + GET_TEAM((TeamTypes)iJ).AI_getAttitudeWeight(getTeam()))
													/
													100
												);
											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestTeam = ((TeamTypes)iJ);
											}
										}
									}
								}

								if (eBestTeam != NO_TEAM)
									eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_TEAM, 2)
										.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
										.addI(DIPF_value, iBestValue).addI(DIPF_ally, GET_TEAM(eBestTeam).getLeaderID()));
								else
									eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_NOTEAM, 2)
										.addI(DIPF_actor, getID()).addI(DIPF_target, iI));

								if (eBestTeam != NO_TEAM)
								{
									iBestValue = 0;
									eBestGiveTech = NO_TECH;

									// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
									// never rediscovered by probing every tech in the database.
									std::vector<int> tradableTechs;
									GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

									for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
									{
										const int iJ = tradableTechs[iAt];
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iValue =
												(
													(1 + GC.getGame().getSorenRandNum(100, "AI Tech Trading #2"))
													*
													GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getResearchLeft((TechTypes)iJ)
												);
											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestGiveTech = (TechTypes)iJ;
											}
										}
									}

									iOurValue = GET_TEAM(getTeam()).AI_declareWarTradeVal(eBestTeam, GET_PLAYER((PlayerTypes)iI).getTeam());
									int iTheirValue;
									if (eBestGiveTech != NO_TECH)
									{
										iTheirValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam());
										eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_TECH, 2)
											.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
											.addI(DIPF_tech, eBestGiveTech).addI(DIPF_theirValue, iTheirValue));
									}
									else
									{
										iTheirValue = 0;
										eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_NOTECH, 2)
											.addI(DIPF_actor, getID()).addI(DIPF_target, iI));
									}

									int iBestValue2 = 0;
									TechTypes eBestGiveTech2 = NO_TECH;

									if ((iTheirValue < iOurValue) && (eBestGiveTech != NO_TECH))
									{
										// The tradable set is the RECEIVER's frontier -- canTradeItem's own last clause -- so it is read,
										// never rediscovered by probing every tech in the database.
										std::vector<int> tradableTechs;
										GET_PLAYER((PlayerTypes)iI).getAvailableTechs(tradableTechs);

										for (size_t iAt = 0; iAt < tradableTechs.size(); ++iAt)
										{
											const int iJ = tradableTechs[iAt];
											if (iJ != eBestGiveTech)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

												if (canTradeItem(((PlayerTypes)iI), item, true))
												{
													const int iValue =
														(
															(1 + GC.getGame().getSorenRandNum(100, "AI Tech Trading #2"))
															*
															GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getResearchLeft((TechTypes)iJ)
														);
													if (iValue > iBestValue2)
													{
														iBestValue2 = iValue;
														eBestGiveTech2 = (TechTypes)iJ;
													}
												}
											}
										}

										if (eBestGiveTech2 != NO_TECH)
										{
											int iTechValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech2, getTeam());
											iTheirValue += iTechValue;
											eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_TECH2, 2)
												.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
												.addI(DIPF_tech, eBestGiveTech2).addI(DIPF_value, iTechValue).addI(DIPF_total, iTheirValue));
										}
									}

									int iReceiveGold = 0;
									int iGiveGold = 0;

									if (iTheirValue > iOurValue)
									{
										const int iValueDiff = iTheirValue - iOurValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_ASK, 2)
												.addI(DIPF_actor, getID()).addI(DIPF_target, iI).addI(DIPF_gold, iGold));

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
													{
														iReceiveGold = iGold;
														iOurValue += iValue;
													}
												}
											}
										}
									}
									else if (iOurValue > iTheirValue)
									{
										const int iValueDiff = iOurValue - iTheirValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_OFFER, 2)
												.addI(DIPF_actor, getID()).addI(DIPF_target, iI).addI(DIPF_gold, iGold));

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (canTradeItem(((PlayerTypes)iI), item, true))
													{
														iGiveGold = iGold;
														iTheirValue += iValue;
													}
												}
											}
										}
									}

									eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_VALUES, 2)
										.addI(DIPF_actor, getID()).addI(DIPF_target, iI)
										.addI(DIPF_theirValue, iTheirValue).addI(DIPF_ourValue, iOurValue));

									if (iTheirValue > (iOurValue * 3 / 4))
									{
										eventSpine().emit(CvSpineEvent(EVENTKIND_DIAGNOSTIC, SD_DIPLO, DIP_WARALLY_PROCEED, 2)
											.addI(DIPF_actor, getID()).addI(DIPF_target, iI));

										ourList.clear();
										theirList.clear();

										if (eBestGiveTech != NO_TECH)
										{
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
											ourList.insertAtEnd(item);
										}

										if (eBestGiveTech2 != NO_TECH)
										{
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech2);
											ourList.insertAtEnd(item);
										}

										setTradeItem(&item, TRADE_WAR, eBestTeam);
										theirList.insertAtEnd(item);

										if (iGiveGold != 0)
										{
											setTradeItem(&item, TRADE_GOLD, iGiveGold);
											ourList.insertAtEnd(item);
										}

										if (iReceiveGold != 0)
										{
											setTradeItem(&item, TRADE_GOLD, iReceiveGold);
											theirList.insertAtEnd(item);
										}

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
											{
												m_eDemandWarAgainstTeam = eBestTeam;
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BUY_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_BUY_WAR));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											m_eDemandWarAgainstTeam = eBestTeam;
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}


//
// read object from a stream
// used during load
//
void CvPlayerAI::read(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_READ_OBJECT_START(wrapper);

	CvPlayer::read(pStream); // read base class data first

	uint uiFlag = 0;
	WRAPPER_READ(wrapper, "CvPlayerAI", &uiFlag); // flags for expansion

	if ((uiFlag & PLAYERAI_UI_FLAG_OMITTED) == 0)
	{
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iPeaceWeight);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iEspionageWeight);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iAttackOddsChange);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iCivicTimer);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iReligionTimer);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iExtraGoldTarget);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_turnsSinceLastRevolution);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iCivicSwitchMinDeltaThreshold);

		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyHash);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyHashCacheTurn);

		if (uiFlag < 3)
		{
			m_iStrategyHash = 0;
			m_iStrategyHashCacheTurn = -1;
		}

		if (uiFlag > 2)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyRand);
		}
		else
		{
			m_iStrategyRand = 0;
		}

		if (uiFlag > 0)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iVictoryStrategyHash);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iVictoryStrategyHashCacheTurn);
		}
		else
		{
			m_iVictoryStrategyHash = 0;
			m_iVictoryStrategyHashCacheTurn = -1;
		}

		if (uiFlag > 1)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bPushReligiousVictory);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bConsiderReligiousVictory);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bHasInquisitionTarget);
		}
		else
		{
			m_bPushReligiousVictory = false;
			m_bConsiderReligiousVictory = false;
			m_bHasInquisitionTarget = false;
		}
		WRAPPER_READ(wrapper, "CvPlayerAI", (int*)&m_iAveragesCacheTurn);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iAverageGreatPeopleMultiplier);

		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_YIELD_TYPES, m_aiAverageYieldMultiplier);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceMultiplier);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceExchange);

		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCacheTurn);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCachedExpThreshold);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCachedGold);

		WRAPPER_READ_OPTIONAL_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumTrainAIUnits);
		WRAPPER_READ_OPTIONAL_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumAIUnits);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiSameReligionCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiDifferentReligionCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiFavoriteCivicCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiBonusTradeCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeTradeValue);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeGrantValue);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiGoldTradedTo);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiAttitudeExtra);

		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_abFirstContact);

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_CONTACT_TYPES, m_aaiContactTimer[i]);
		}
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_READ_ARRAY_ALLOW_TRUNCATE(wrapper, "CvPlayerAI", NUM_MEMORY_TYPES, m_aaiMemoryCount[i]);
		}
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_bWasFinancialTrouble);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iTurnLastProductionDirty);
		{
			m_aiAICitySites.clear();
			uint iSize;
			WRAPPER_READ(wrapper, "CvPlayerAI", &iSize);
			for (uint i = 0; i < iSize; i++)
			{
				int iCitySite;
				WRAPPER_READ(wrapper, "CvPlayerAI", &iCitySite);
				m_aiAICitySites.push_back(iCitySite);
			}
		}
		WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_aiUnitWeights);
		WRAPPER_READ_CLASS_ARRAY_ALLOW_MISSING(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_aiUnitCombatWeights);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiCloseBordersAttitudeCache);

		m_eBestResearchTarget = NO_TECH;
		WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_TECHS, (int*)&m_eBestResearchTarget);

		AI_updateBonusValue();
	}
	WRAPPER_READ_OBJECT_END(wrapper);

	// #395: rebuild the transient strength-weighted ledgers from the loaded units now --
	// before the NPC cull below, whose kills decrement these ledgers.
	AI_rebuildEffUnitLedgers();

	//	If the total number of barb units is getting dangerously close to the limit we can
	//	cull some animals.  Note - this has to be done in PlayerAI not Player, because kills won't
	//	operate correctly until AI structures are initialized (class counting for example)
	//  TB: Adjusted to cull any NPC units as needed.
	if (isNPC() && getNumUnits() > MAX_BARB_UNITS_FOR_SPAWNING)
	{
		int iCull = getNumUnits() - MAX_BARB_UNITS_FOR_SPAWNING;

		std::vector<CvUnit*> unitsToCull(beginUnits(), endUnits());
		unitsToCull.resize(std::min<size_t>(iCull, unitsToCull.size()));

		foreach_(CvUnit * unit, unitsToCull)
		{
			unit->kill(false);
		}
	}
	AI_invalidateAttitudeCache();
}


//
// save object to a stream
// used during save
//
void CvPlayerAI::write(FDataStreamBase* pStream)
{
	PROFILE_FUNC();

	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_WRITE_OBJECT_START(wrapper);

	CvPlayer::write(pStream); // write base class data first

	// Flag for type of save
	uint uiFlag = 3;
	if (!m_bEverAlive)
	{
		uiFlag |= PLAYERAI_UI_FLAG_OMITTED;
	}

	WRAPPER_WRITE(wrapper, "CvPlayerAI", uiFlag); // flag for expansion

	if (m_bEverAlive)
	{
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iPeaceWeight);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iEspionageWeight);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAttackOddsChange);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iCivicTimer);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iReligionTimer);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iExtraGoldTarget);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_turnsSinceLastRevolution);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iCivicSwitchMinDeltaThreshold);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyHash);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyHashCacheTurn);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyRand);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iVictoryStrategyHash);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iVictoryStrategyHashCacheTurn);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bPushReligiousVictory);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bConsiderReligiousVictory);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bHasInquisitionTarget);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAveragesCacheTurn);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAverageGreatPeopleMultiplier);

		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_YIELD_TYPES, m_aiAverageYieldMultiplier);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceMultiplier);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceExchange);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCacheTurn);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCachedExpThreshold);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCachedGold);

		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumTrainAIUnits);
		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumAIUnits);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiSameReligionCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiDifferentReligionCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiFavoriteCivicCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiBonusTradeCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeTradeValue);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeGrantValue);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiGoldTradedTo);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiAttitudeExtra);

		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_abFirstContact);

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_CONTACT_TYPES, m_aaiContactTimer[i]);
		}
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_MEMORY_TYPES, m_aaiMemoryCount[i]);
		}

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bWasFinancialTrouble);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iTurnLastProductionDirty);

		{
			const uint32_t iSize = m_aiAICitySites.size();
			WRAPPER_WRITE(wrapper, "CvPlayerAI", iSize);
			foreach_(const int iCitySite, m_aiAICitySites)
			{
				WRAPPER_WRITE_DECORATED(wrapper, "CvPlayerAI", iCitySite, "iCitySite");
			}
		}

		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_aiUnitWeights);
		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_aiUnitCombatWeights);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiCloseBordersAttitudeCache);

		WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_TECHS, m_eBestResearchTarget);
	}
	WRAPPER_WRITE_OBJECT_END(wrapper);

	/* Needed if getMaxCivPlayers return MAX_PC_PLAYERS, now it returns MAX_PLAYERS-1.
		if (getID() == MAX_PC_PLAYERS)
		{
			//Write NPC data
			for (int iI = MAX_PC_PLAYERS+1; iI < MAX_PLAYERS; iI++)
			{
				GET_PLAYER((PlayerTypes)iI).write(pStream);
			}
		}
	*/
}


int CvPlayerAI::AI_eventValue(EventTypes eEvent, const EventTriggeredData& kTriggeredData) const
{
	PROFILE_EXTRA_FUNC();
	if (kTriggeredData.m_eTrigger == NO_EVENTTRIGGER)
	{
		return 0;
	}
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	int iNumCities = getNumCities();
	CvCity* pCity = getCity(kTriggeredData.m_iCityId);
	CvPlot* pPlot = GC.getMap().plot(kTriggeredData.m_iPlotX, kTriggeredData.m_iPlotY);
	CvUnit* pUnit = getUnit(kTriggeredData.m_iUnitId);

	int iHappy = 0;
	int iHealth = 0;
	int aiYields[NUM_YIELD_TYPES];
	int aiCommerceYields[NUM_COMMERCE_TYPES];

	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		aiYields[iI] = 0;
	}
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		aiCommerceYields[iI] = 0;
	}

	if (NO_PLAYER != kTriggeredData.m_eOtherPlayer)
	{
		if (kEvent.isDeclareWar())
		{
			switch (AI_getAttitude(kTriggeredData.m_eOtherPlayer))
			{
			case ATTITUDE_FURIOUS:
			case ATTITUDE_ANNOYED:
			case ATTITUDE_CAUTIOUS:
				if (GET_TEAM(getTeam()).getDefensivePower() < GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).getPower(true))
				{
					return -MAX_INT + 1;
				}
				break;
			case ATTITUDE_PLEASED:
			case ATTITUDE_FRIENDLY:
				return -MAX_INT + 1;
				break;
			}
		}
	}

	//Proportional to #turns in the game...
	//(AI evaluation will generally assume proper game speed scaling!)
	const int iGameSpeedPercent = CvGameSpeedScale::speedPercent();

	int iValue = GC.getGame().getSorenRandNum(kEvent.getAIValue(), "AI Event choice");
	iValue += (getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, false) + getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, true)) / 2;

	iValue += kEvent.getEspionagePoints();

	if (kEvent.getTech() != NO_TECH)
	{
		iValue += (GET_TEAM(getTeam()).getResearchCost((TechTypes)kEvent.getTech()) * kEvent.getTechPercent()) / 100;
	}

	if (kEvent.getFreeUnit() != NO_UNIT)
	{
		//Altough AI_unitValue compares well within units, the value is somewhat independent of cost
		int iUnitValue = GC.getUnitInfo((UnitTypes)kEvent.getFreeUnit()).getProductionCost();
		if (iUnitValue > 0)
		{
			iUnitValue *= 2;
		}
		else if (iUnitValue == -1)
		{
			iUnitValue = 200; //Great Person?
		}

		iUnitValue *= CvGameSpeedScale::hammerCostPercent();
		iValue += kEvent.getNumUnits() * iUnitValue;
	}

	if (kEvent.isDisbandUnit())
	{
		CvUnit* pUnit = getUnit(kTriggeredData.m_iUnitId);
		if (NULL != pUnit)
		{
			int iUnitValue = pUnit->getUnitInfo().getProductionCost();
			if (iUnitValue > 0)
			{
				iUnitValue *= 2;
			}
			else if (iUnitValue == -1)
			{
				iUnitValue = 200; //Great Person?
			}

			iUnitValue *= CvGameSpeedScale::hammerCostPercent();
			iValue -= iUnitValue;
		}
	}

	const BuildingTypes eBuilding = static_cast<BuildingTypes>(kEvent.getBuilding());
	if (eBuilding != NO_BUILDING && pCity)
	{
		int iBuildingValue = GC.getBuildingInfo(eBuilding).getCost();
		if (iBuildingValue > 0)
		{
			iBuildingValue *= 2;
		}
		else if (iBuildingValue == -1)
		{
			iBuildingValue = 300; //Shrine?
		}

		iBuildingValue *= CvGameSpeedScale::hammerCostPercent();
		iValue += kEvent.getBuildingChange() * iBuildingValue;
	}

	TechTypes eBestTech = NO_TECH;
	int iBestValue = 0;
	// #430 F2b (enabler.md): iterate the enabler's LISTED tech frontier instead of scanning every tech info and
	// probing each id -- getAvailableTechs fills the player's LISTED tech frontier. The
	// whole loop body sits inside the gate; the best pick uses strict '>', so ascending (forward) order keeps the
	// original lowest-id-wins result.
	std::vector<int> vecResearchable;
	m_enabler.techs.listedIds(vecResearchable);
	for (std::vector<int>::const_iterator it = vecResearchable.begin(), itEnd = vecResearchable.end(); it != itEnd; ++it)
	{
		const TechTypes eTechX = static_cast<TechTypes>(*it);
		if (NO_PLAYER == kTriggeredData.m_eOtherPlayer || GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).isHasTech(eTechX))
		{
			int iValue = 0;
			for (int i = 0; i < GC.getNumFlavorTypes(); ++i)
			{
				iValue += kEvent.getTechFlavorValue(i) * GC.getTechInfo(eTechX).getFlavorValue(i);
			}

			if (iValue > iBestValue)
			{
				eBestTech = eTechX;
				iBestValue = iValue;
			}
		}
	}

	if (eBestTech != NO_TECH)
	{
		iValue += (GET_TEAM(getTeam()).getResearchCost(eBestTech) * kEvent.getTechPercent()) / 100;
	}

	if (kEvent.isGoldenAge())
	{
		iValue += AI_calculateGoldenAgeValue();
	}

	{	//Yield and other changes
		// Iterate the event's OWN authored entries, which is the idiom CvEventInfo.h documents on the
		// container accessors themselves. Each of these summed over EVERY building id to reach a handful of
		// entries -- and the per-id getters LINEAR-SCAN the same vector per call, so the yield one alone cost
		// ~5,200 x NUM_YIELD_TYPES scans per event evaluated. Summing all ids of a keyed change is
		// arithmetically the same as summing the container once, so the totals are identical.
		// (The events system itself is untouched here -- this is only how the AI READS its data.)
		foreach_(const BuildingYieldChange& kChange, kEvent.getBuildingYieldChanges())
		{
			aiYields[kChange.eYield] += kChange.iChange;
		}

		foreach_(const BuildingCommerceChange& kChange, kEvent.getBuildingCommerceChanges())
		{
			aiCommerceYields[kChange.eCommerce] += kChange.iChange;
		}

		foreach_(const BuildingChangeArray::value_type& kChange, kEvent.getBuildingHappyChanges())
		{
			iHappy += kChange.second;
		}

		foreach_(const BuildingChangeArray::value_type& kChange, kEvent.getBuildingHealthChanges())
		{
			iHealth += kChange.second;
		}
	}

	if (kEvent.isCityEffect())
	{
		int iCityPopulation = -1;
		int iCityTurnValue = 0;
		if (NULL != pCity)
		{
			iCityPopulation = pCity->getPopulation();
			for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
			{
				if (kEvent.getFreeSpecialistCount(iSpecialist) > 0)
				{
					iCityTurnValue += (pCity->AI_specialistValue((SpecialistTypes)iSpecialist, false, false) / 300);
				}
			}
		}

		if (-1 == iCityPopulation)
		{
			//What is going on here?
			iCityPopulation = 5;
		}

		iCityTurnValue += ((iHappy + kEvent.getHappy()) * 8);
		iCityTurnValue += ((iHealth + kEvent.getHealth()) * 6);

		iCityTurnValue += aiYields[YIELD_FOOD] * 5;
		iCityTurnValue += aiYields[YIELD_PRODUCTION] * 5;
		iCityTurnValue += aiYields[YIELD_COMMERCE] * 3;

		iCityTurnValue += aiCommerceYields[COMMERCE_RESEARCH] * 3;
		iCityTurnValue += aiCommerceYields[COMMERCE_GOLD] * 3;
		iCityTurnValue += aiCommerceYields[COMMERCE_CULTURE] * 1;
		iCityTurnValue += aiCommerceYields[COMMERCE_ESPIONAGE] * 2;

		iValue += iCityTurnValue * 20 * iGameSpeedPercent / 100;

		iValue += kEvent.getFood();
		iValue += kEvent.getFoodPercent() / 4;
		iValue += kEvent.getPopulationChange() * 30;
		iValue -= kEvent.getRevoltTurns() * (12 + iCityPopulation * 16);
		iValue -= kEvent.getHurryAnger() * 6 * GC.getHURRY_ANGER_DIVISOR() * CvGameSpeedScale::speedPercent() / 100;
		iValue += kEvent.getHappyTurns() * 10;
		iValue += kEvent.getCulture() / 2;
	}
	else if (!kEvent.isOtherPlayerCityEffect())
	{
		const int iPerTurnValue = iNumCities * (iHappy * 4 + kEvent.getHappy() * 8 + iHealth * 3 + kEvent.getHealth() * 6);

		iValue += iPerTurnValue * 20 * iGameSpeedPercent / 100;

		iValue += (kEvent.getFood() * iNumCities);
		iValue += (kEvent.getFoodPercent() * iNumCities) / 4;
		iValue += (kEvent.getPopulationChange() * iNumCities * 40);
		iValue -= iNumCities * kEvent.getHurryAnger() * 6 * GC.getHURRY_ANGER_DIVISOR() * CvGameSpeedScale::speedPercent() / 100;
		iValue += iNumCities * kEvent.getHappyTurns() * 10;
		iValue += iNumCities * kEvent.getCulture() / 2;
	}

	int iBonusValue = 0;
	if (NO_BONUS != kEvent.getBonus())
	{
		iBonusValue = AI_bonusVal((BonusTypes)kEvent.getBonus());
	}

	if (NULL != pPlot)
	{
		if (kEvent.getFeatureChange() != 0)
		{
			int iOldFeatureValue = 0;
			int iNewFeatureValue = 0;
			if (pPlot->getFeatureType() != NO_FEATURE)
			{
				//*grumble* who tied feature production to builds rather than the feature...
				iOldFeatureValue = GC.getFeatureInfo(pPlot->getFeatureType()).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);

				if (kEvent.getFeatureChange() > 0)
				{
					FeatureTypes eFeature = (FeatureTypes)kEvent.getFeature();
					FAssert(eFeature != NO_FEATURE);
					if (eFeature != NO_FEATURE)
					{
						iNewFeatureValue = GC.getFeatureInfo(eFeature).getWellbeingModifier(WELLBEING_HEALTH, CASC_SCOPE_PLOT);
					}
				}

				iValue += ((iNewFeatureValue - iOldFeatureValue) * iGameSpeedPercent) / 100;
			}
		}

		if (kEvent.getImprovementChange() > 0)
		{
			iValue += (30 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getImprovementChange() < 0)
		{
			iValue -= (30 * iGameSpeedPercent) / 100;
		}

		if (kEvent.getRouteChange() > 0)
		{
			iValue += (10 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getRouteChange() < 0)
		{
			iValue -= (10 * iGameSpeedPercent) / 100;
		}

		if (kEvent.getBonusChange() > 0)
		{
			iValue += (iBonusValue * 15 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getBonusChange() < 0)
		{
			iValue -= (iBonusValue * 15 * iGameSpeedPercent) / 100;
		}

		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			if (0 != kEvent.getPlotExtraYield(i))
			{
				if (pPlot->getWorkingCity() != NULL)
				{
					FAssertMsg(pPlot->getWorkingCity()->getOwner() == getID(), "Event creates a boni for another player?");
					aiYields[i] += kEvent.getPlotExtraYield(i);
				}
				else
				{
					iValue += (20 * 8 * kEvent.getPlotExtraYield(i) * iGameSpeedPercent) / 100;
				}
			}
		}
	}

	if (NO_BONUS != kEvent.getBonusRevealed())
	{
		iValue += (iBonusValue * 10 * iGameSpeedPercent) / 100;
	}

	if (NULL != pUnit)
	{
		iValue += (2 * pUnit->baseCombatStrHuman() * kEvent.getUnitExperience() * CvGameSpeedScale::hammerCostPercent()) / 100;

		iValue -= 10 * kEvent.getUnitImmobileTurns();
	}

	{
		int iPromotionValue = 0;

		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (NO_PROMOTION != kEvent.getUnitCombatPromotion(i))
			{
				foreach_(const CvUnit * pLoopUnit, units())
				{
					//if (pLoopUnit->getUnitCombatType() == i)
					//{
					//	if (!pLoopUnit->isHasPromotion((PromotionTypes)kEvent.getUnitCombatPromotion(i)))
					//	{
					//		iPromotionValue += 5 * pLoopUnit->baseCombatStr();
					//	}
					//}
					//TB SubCombat Mod Begin
					if (pLoopUnit->isHasUnitCombat((UnitCombatTypes)i))
					{
						if (!pLoopUnit->isHasPromotion((PromotionTypes)kEvent.getUnitCombatPromotion(i)))
						{
							iPromotionValue += 5 * pLoopUnit->baseCombatStrHuman();
						}
					}
					//TB SubCombat Mod End
				}

				iPromotionValue += iNumCities * 50;
			}
		}

		iValue += (iPromotionValue * iGameSpeedPercent) / 100;
	}

	if (kEvent.getFreeUnitSupport() != 0)
	{
		iValue += (20 * kEvent.getFreeUnitSupport() * iGameSpeedPercent) / 100;
	}

	if (kEvent.getInflationModifier() != 0)
	{
		iValue -= static_cast<int>(20 * kEvent.getInflationModifier() * calculatePreInflatedCosts() * iGameSpeedPercent / 100);
	}

	if (kEvent.getSpaceProductionModifier() != 0)
	{
		iValue += ((20 + iNumCities) * getSpaceProductionModifier() * iGameSpeedPercent) / 100;
	}

	int iOtherPlayerAttitudeWeight = 0;
	if (kTriggeredData.m_eOtherPlayer != NO_PLAYER)
	{
		iOtherPlayerAttitudeWeight = AI_getAttitudeWeight(kTriggeredData.m_eOtherPlayer);
		iOtherPlayerAttitudeWeight += 10 - GC.getGame().getSorenRandNum(20, "AI event value attitude");
	}

	//Religion
	if (kTriggeredData.m_eReligion != NO_RELIGION)
	{
		ReligionTypes eReligion = kTriggeredData.m_eReligion;

		int iReligionValue = 15;

		if (getStateReligion() == eReligion)
		{
			iReligionValue += 15;
		}
		if (hasHolyCity(eReligion))
		{
			iReligionValue += 15;
		}

		iValue += (kEvent.getConvertOwnCities() * iReligionValue * iGameSpeedPercent) / 100;

		if (kEvent.getConvertOtherCities() > 0)
		{
			//Don't like them much = fairly indifferent, hate them = negative.
			iValue += (kEvent.getConvertOtherCities() * (iOtherPlayerAttitudeWeight + 50) * iReligionValue * iGameSpeedPercent) / 15000;
		}
	}

	if (NO_PLAYER != kTriggeredData.m_eOtherPlayer)
	{
		CvPlayerAI& kOtherPlayer = GET_PLAYER(kTriggeredData.m_eOtherPlayer);

		int iDiploValue = 0;
		//if we like this player then value positive attitude, if however we really hate them then
		//actually value negative attitude.
		iDiploValue += ((iOtherPlayerAttitudeWeight + 50) * kEvent.getAttitudeModifier() * GET_PLAYER(kTriggeredData.m_eOtherPlayer).getPower()) / std::max(1, getPower());

		if (kEvent.getTheirEnemyAttitudeModifier() != 0)
		{
			//Oh wow this sure is mildly complicated.
			TeamTypes eWorstEnemy = GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).AI_getWorstEnemy();

			if (NO_TEAM != eWorstEnemy && eWorstEnemy != getTeam())
			{
				int iThirdPartyAttitudeWeight = GET_TEAM(getTeam()).AI_getAttitudeWeight(eWorstEnemy);

				//If we like both teams, we want them to get along.
				//If we like otherPlayer but not enemy (or vice-verca), we don't want them to get along.
				//If we don't like either, we don't want them to get along.
				//Also just value stirring up trouble in general.

				int iThirdPartyDiploValue = 50 * kEvent.getTheirEnemyAttitudeModifier();
				iThirdPartyDiploValue *= (iThirdPartyAttitudeWeight - 10);
				iThirdPartyDiploValue *= (iOtherPlayerAttitudeWeight - 10);
				iThirdPartyDiploValue /= 10000;

				if ((iOtherPlayerAttitudeWeight) < 0 && (iThirdPartyAttitudeWeight < 0))
				{
					iThirdPartyDiploValue *= -1;
				}

				iThirdPartyDiploValue /= 2;

				iDiploValue += iThirdPartyDiploValue;
			}
		}

		iDiploValue *= iGameSpeedPercent;
		iDiploValue /= 100;

		if (NO_BONUS != kEvent.getBonusGift())
		{
			int iBonusValue = -AI_bonusVal((BonusTypes)kEvent.getBonusGift(), -1);
			iBonusValue += (iOtherPlayerAttitudeWeight - 40) * kOtherPlayer.AI_bonusVal((BonusTypes)kEvent.getBonusGift(), +1);
			//Positive for friends, negative for enemies.
			iDiploValue += (iBonusValue * getTreatyLength()) / 60;
		}

		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			//What is this "relationships" thing?
			iDiploValue /= 2;
		}

		if (kEvent.isGoldToPlayer())
		{
			//If the gold goes to another player instead of the void, then this is a positive
			//thing if we like the player, otherwise it's a negative thing.
			int iGiftValue = (getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, false) + getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, true)) / 2;
			iGiftValue *= -iOtherPlayerAttitudeWeight;
			iGiftValue /= 110;

			iValue += iGiftValue;
		}

		if (kEvent.isDeclareWar())
		{
			int iWarValue = (GET_TEAM(getTeam()).getDefensivePower() - GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).getPower(true));// / std::max(1, GET_TEAM(getTeam()).getDefensivePower());
			iWarValue -= 30 * AI_getAttitudeVal(kTriggeredData.m_eOtherPlayer);
			iValue += iWarValue;
		}

		if (kEvent.getMaxPillage() > 0)
		{
			int iPillageValue = (40 * (kEvent.getMinPillage() + kEvent.getMaxPillage())) / 2;
			//If we hate them, this is good to do.
			iPillageValue *= 25 - iOtherPlayerAttitudeWeight;
			iPillageValue *= iGameSpeedPercent;
			iPillageValue /= 12500;
			iValue += iPillageValue;
		}

		iValue += (iDiploValue * iGameSpeedPercent) / 100;
	}

	int iThisEventValue = iValue;
	//XXX THIS IS VULNERABLE TO NON-TRIVIAL RECURSIONS!
	//Event A effects Event B, Event B effects Event A
	for (int iEvent = 0; iEvent < GC.getNumEventInfos(); ++iEvent)
	{
		if (kEvent.getAdditionalEventChance(iEvent) > 0)
		{
			if (iEvent == eEvent)
			{
				//Infinite recursion is not our friend.
				//Fortunately we have the event value for this event - sans values of other events
				//disabled or cleared. Hopefully no events will be that complicated...
				//Double the value since it's recursive.
				iValue += (kEvent.getAdditionalEventChance(iEvent) * iThisEventValue) / 50;
			}
			else
			{
				iValue += (kEvent.getAdditionalEventChance(iEvent) * AI_eventValue((EventTypes)iEvent, kTriggeredData)) / 100;
			}
		}

		if (kEvent.getClearEventChance(iEvent) > 0)
		{
			if (iEvent == eEvent)
			{
				iValue -= (kEvent.getClearEventChance(iEvent) * iThisEventValue) / 50;
			}
			else
			{
				iValue -= (kEvent.getClearEventChance(iEvent) * AI_eventValue((EventTypes)iEvent, kTriggeredData)) / 100;
			}
		}
	}

	iValue *= 100 + GC.getGame().getSorenRandNum(20, "AI Event choice");
	iValue /= 100;

	return iValue;
}

EventTypes CvPlayerAI::AI_chooseEvent(int iTriggeredId, int* pValue) const
{
	PROFILE_EXTRA_FUNC();
	const EventTriggeredData* pTriggeredData = getEventTriggered(iTriggeredId);
	if (NULL == pTriggeredData || pTriggeredData->m_eTrigger == NO_EVENTTRIGGER)
	{
		return NO_EVENT;
	}

	const CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo(pTriggeredData->m_eTrigger);

	int iBestValue = -MAX_INT;
	EventTypes eBestEvent = NO_EVENT;

	for (int i = 0; i < kTrigger.getNumEvents(); i++)
	{
		int iValue = -MAX_INT;
		if (kTrigger.getEvent(i) != NO_EVENT)
		{
			if (canDoEvent((EventTypes)kTrigger.getEvent(i), *pTriggeredData))
			{
				iValue = AI_eventValue((EventTypes)kTrigger.getEvent(i), *pTriggeredData);
			}
		}

		if (iValue > iBestValue)
		{
			iBestValue = iValue;
			eBestEvent = (EventTypes)kTrigger.getEvent(i);
		}
	}

	if (pValue != NULL)
	{
		*pValue = iBestValue;
	}

	return eBestEvent;
}

void CvPlayerAI::AI_doSplit()
{
	PROFILE_FUNC();

	if (!canSplitEmpire())
	{
		return;
	}
	// Toffer - ToDo
	//	Need to stop AI from creating colony on another landmass when it should rather relocate its capitol to the other landmass.
	//	e.g. Its capitol is a lone city on an island, and it has multiple cities on a bigger landmass, it should relocate its capitol
	//	rather than making a vassal of all the cities on the big landmass, need to look into AI evaluation for palace construction too.
	//	hint: City->AI_cityValue(bIgnoreColonyMaintenance), (new argument) could be used to see if the oversea area is a better area for the capitol.
	//		looped over all cities in each area similar to what is done below.
	//		Need a should relocate capitol function called from here to stop the split if true,
	//		but also from wherever it is natural to evaluate whether the capitol should be relocated or not.
	//		Palace should probably be relocated to the other landmass long before the AI considers splitting it up as a colony (this function is called).
	//		not sure if Forbidden Palace and similar government centers removes oversea maintenance cost from landmass it is on, needs to be looked into.

	std::map<int, int> mapAreaValues;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		mapAreaValues[pLoopArea->getID()] = 0;
	}

	foreach_(CvCity * pLoopCity, cities())
	{
		mapAreaValues[pLoopCity->area()->getID()] += pLoopCity->AI_cityValue();
	}

	std::map<int, int>::iterator it;
	for (it = mapAreaValues.begin(); it != mapAreaValues.end(); ++it)
	{
		if (it->second < 0)
		{
			const int iAreaId = it->first;

			if (canSplitArea(iAreaId))
			{
				splitEmpire(iAreaId);

				foreach_(CvUnit * pUnit, units())
				{
					if (pUnit->area()->getID() == iAreaId)
					{
						const TeamTypes ePlotTeam = pUnit->plot()->getTeam();

						if (NO_TEAM != ePlotTeam)
						{
							const CvTeam& kPlotTeam = GET_TEAM(ePlotTeam);
							if (kPlotTeam.isVassal(getTeam()) && GET_TEAM(getTeam()).isParent(ePlotTeam))
							{
								pUnit->gift();
							}
						}
					}
				}
				break;
			}
		}
	}
}

void CvPlayerAI::AI_launch(VictoryTypes eVictory)
{
	PROFILE_EXTRA_FUNC();
	if (GET_TEAM(getTeam()).isHuman())
	{
		return;
	}

	if (!GET_TEAM(getTeam()).canLaunch(eVictory))
	{
		return;
	}

	bool bLaunch = true;

	int iBestArrival = MAX_INT;
	TeamTypes eBestTeam = NO_TEAM;

	for (int iTeam = 0; iTeam < MAX_PC_TEAMS; ++iTeam)
	{
		if (iTeam != getTeam())
		{
			CvTeam& kTeam = GET_TEAM((TeamTypes)iTeam);
			if (kTeam.isAlive())
			{
				const int iCountdown = kTeam.getVictoryCountdown(eVictory);
				if (iCountdown > 0 && iCountdown < iBestArrival)
				{
					iBestArrival = iCountdown;
					eBestTeam = (TeamTypes)iTeam;
				}
			}
		}
	}

	if (bLaunch)
	{
		if (NO_TEAM == eBestTeam || iBestArrival > GET_TEAM(getTeam()).getVictoryDelay(eVictory))
		{
			if (GET_TEAM(getTeam()).getLaunchSuccessRate(eVictory) < 100)
			{
				bLaunch = false;
			}
		}
	}

	if (bLaunch)
	{
		launch(eVictory);
	}
}

void CvPlayerAI::AI_doCheckFinancialTrouble()
{
	PROFILE_FUNC();

	// if we just ran into financial trouble this turn
	bool bFinancialTrouble = AI_isFinancialTrouble();
	if (bFinancialTrouble != m_bWasFinancialTrouble)
	{
		if (bFinancialTrouble)
		{
			int iGameTurn = GC.getGame().getGameTurn();

			// only reset at most every 10 turns
			if (iGameTurn > m_iTurnLastProductionDirty + 10)
			{
				// redeterimine the best things to build in each city
				AI_makeProductionDirty();

				m_iTurnLastProductionDirty = iGameTurn;
			}
		}

		m_bWasFinancialTrouble = bFinancialTrouble;
	}
}

bool CvPlayerAI::AI_disbandUnit(int iExpThreshold)
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = MAX_INT;
	CvUnit* pBestUnit = NULL;

	foreach_(CvUnit * unitX, units())
	{
		if (unitX->getUpkeep() < 1
		|| !unitX->isMilitaryBranch()
		|| unitX->hasCargo()
		|| unitX->isGoldenAge()
		|| unitX->getUnitInfo().getProductionCost() < 1)
		{
			continue;
		}
		if ((iExpThreshold == -1 || unitX->canFight() && unitX->getExperience() <= iExpThreshold)
		&& (!unitX->isMilitaryHappiness() || !unitX->plot()->isCity() || unitX->plot()->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID()) > 2))
		{
			int iValue = (10000 + GC.getGame().getSorenRandNum(1000, "Disband Unit"));

			iValue *= 100 + (unitX->getUnitInfo().getProductionCost() * 3);
			iValue /= 100;

			iValue *= 100 + (unitX->getExperience() * 10);
			iValue /= 100;

			iValue *= 100 + (unitX->getLevel() * 25);
			iValue /= 100;

			if (unitX->plot()->getTeam() == unitX->getTeam())
			{
				iValue *= 3;

				if (unitX->canDefend() && unitX->plot()->isCity())
				{
					iValue *= 2;
				}
			}

			// Multiplying by higher number means unit has higher priority, less likely to be disbanded
			switch (unitX->AI_getUnitAIType())
			{
			case UNITAI_UNKNOWN:
			case UNITAI_ANIMAL:
			case UNITAI_BARB_CRIMINAL:
				break;

			case UNITAI_SUBDUED_ANIMAL:
				//	For now make them less valuable the more you have (strictly this should depend
				//	on what you need in terms of their buildable buildings, but start with an
				//	approximation that is better than nothing
				iValue *= std::max(0, getNumCities() * 2 - AI_getNumAIUnits(UNITAI_SUBDUED_ANIMAL)) / std::max(1, getNumCities());
				break;

			case UNITAI_HUNTER:
			case UNITAI_HUNTER_ESCORT:
				//	Treat hunters like explorers for valuation, but slightly less so
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 10;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_SETTLE:
				iValue *= 20;
				break;

			case UNITAI_WORKER:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) > 10)
				{
					if (unitX->plot()->isCity())
					{
						if (unitX->plot()->getPlotCity()->AI_getWorkersNeeded() == 0)
						{
							iValue *= 10;
						}
					}
				}
				break;

			case UNITAI_ATTACK:
			case UNITAI_ATTACK_CITY:
			case UNITAI_COLLATERAL:
			case UNITAI_PILLAGE:
			case UNITAI_RESERVE:
			case UNITAI_COUNTER:
			case UNITAI_PILLAGE_COUNTER:
			case UNITAI_INVESTIGATOR:
			case UNITAI_SEE_INVISIBLE:
				iValue *= 2;
				break;

			case UNITAI_SEE_INVISIBLE_SEA:
				iValue *= 3;
				break;

			case UNITAI_CITY_DEFENSE:
			case UNITAI_CITY_COUNTER:
			case UNITAI_CITY_SPECIAL:
			case UNITAI_PARADROP:
			case UNITAI_PROPERTY_CONTROL:
			case UNITAI_HEALER:
			case UNITAI_PROPERTY_CONTROL_SEA:
			case UNITAI_HEALER_SEA:
			case UNITAI_ESCORT:
				iValue *= 6;
				break;

			case UNITAI_EXPLORE:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 15;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_MISSIONARY:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 8;
				}
				break;

			case UNITAI_PROPHET:
			case UNITAI_ARTIST:
			case UNITAI_SCIENTIST:
			case UNITAI_GENERAL:
			case UNITAI_GREAT_HUNTER:
			case UNITAI_GREAT_ADMIRAL:
			case UNITAI_MERCHANT:
			case UNITAI_ENGINEER:
				iValue *= 20;
				break;

			case UNITAI_SPY:
			case UNITAI_INFILTRATOR:
				iValue *= 12;
				break;

			case UNITAI_ICBM:
				iValue *= 4;
				break;

			case UNITAI_WORKER_SEA:
				iValue *= 18;
				break;

			case UNITAI_ATTACK_SEA:
			case UNITAI_RESERVE_SEA:
			case UNITAI_ESCORT_SEA:
				break;

			case UNITAI_EXPLORE_SEA:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 12;
				}
				break;

			case UNITAI_SETTLER_SEA:
				iValue *= 6;
				break;

			case UNITAI_MISSIONARY_SEA:
			case UNITAI_SPY_SEA:
				iValue *= 4;
				break;

			case UNITAI_ASSAULT_SEA:
			case UNITAI_CARRIER_SEA:
			case UNITAI_MISSILE_CARRIER_SEA:
				if (GET_TEAM(getTeam()).hasWarPlan(true))
				{
					iValue *= 5;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_PIRATE_SEA:
			case UNITAI_ATTACK_AIR:
				break;

			case UNITAI_DEFENSE_AIR:
			case UNITAI_CARRIER_AIR:
			case UNITAI_MISSILE_AIR:
				if (GET_TEAM(getTeam()).hasWarPlan(true))
				{
					iValue *= 5;
				}
				else
				{
					iValue *= 3;
				}
				break;

			default:
				FErrorMsg("error");
				break;
			}

			iValue /= unitX->getUnitInfo().getUpkeepCost() + 1;

			if (iValue < iBestValue)
			{
				iBestValue = iValue;
				pBestUnit = unitX;
			}
		}
	}

	if (pBestUnit)
	{
		pBestUnit->kill(false);
		return true;
	}
	return false;
}

int CvPlayerAI::AI_cultureVictoryTechValue(TechTypes eTech) const
{

	PROFILE_EXTRA_FUNC();
	if (eTech == NO_TECH)
	{
		return 0;
	}
	int iValue = 0;

	if (GC.getTechInfo(eTech).providesCanTrade(CLS_CANTRADE_DEFENSIVE_PACT))
	{
		iValue += 50;
	}

	if (GC.getTechInfo(eTech).providesCapability(CapabilityContext::commerceRateCapability(COMMERCE_CULTURE)))
	{
		iValue += 100;
	}

	//units
	const bool bAnyWarplan = GET_TEAM(getTeam()).hasWarPlan(true);
	int iBestUnitValue = 0;
	// The tech's own `enables` edge names the units it unlocks -- a list fetch, not a database sweep
	// (patterns.md § THE WHAT-IF DRIVER). iBestUnitValue is a max, so visit order does not affect it.
	std::set<int> unlockedUnits;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_UNITS, unlockedUnits);

	for (std::set<int>::const_iterator itUnlockedUnit = unlockedUnits.begin(); itUnlockedUnit != unlockedUnits.end(); ++itUnlockedUnit)
	{
		int iTempValue = ((GC.getUnitInfo(static_cast<UnitTypes>(*itUnlockedUnit)).getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) * 100) / std::max(1, (GC.getGame().getBestLandUnitCombat()));
		iTempValue *= bAnyWarplan ? 2 : 1;

		iValue += iTempValue / 3;
		iBestUnitValue = std::max(iBestUnitValue, iTempValue);
	}
	iValue += std::max(0, iBestUnitValue - 15);

	//cultural things
	// WHAT DOES THIS TECH ENABLE? -- asked FORWARD off the tech's own load-compiled `enables` edge family, which
	// IS the answer (patterns.md § THE WHAT-IF DRIVER: the fundamental enabler-tree read is a pure list fetch).
	// Membership in the set IS "the tech unlocks it", so no per-candidate re-test remains.
	std::set<int> unlockedBuildings;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_BUILDINGS, unlockedBuildings);

	for (std::set<int>::const_iterator itUnlocked = unlockedBuildings.begin(); itUnlocked != unlockedBuildings.end(); ++itUnlocked)
	{
		const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(static_cast<BuildingTypes>(*itUnlocked));

		iValue += 15 * (kLoopBuilding.getFlatCommerce((CommerceTypes)COMMERCE_CULTURE, CASC_SCOPE_CITY) / 100) / 2;
		iValue += kLoopBuilding.getCommerceModifier((CommerceTypes)COMMERCE_CULTURE, CASC_SCOPE_CITY) * 2;
	}

	//important civics
	std::set<int> unlockedCivics;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_CIVICS, unlockedCivics);

	for (std::set<int>::const_iterator itUnlockedCivic = unlockedCivics.begin(); itUnlockedCivic != unlockedCivics.end(); ++itUnlockedCivic)
	{
		const CivicTypes eLoopCivic = static_cast<CivicTypes>(*itUnlockedCivic);

		iValue += GC.getCivicInfo(eLoopCivic).getCommerceModifier((CommerceTypes)COMMERCE_CULTURE, CASC_SCOPE_EMPIRE) * 2;
	}

	return iValue;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  04/25/10								jdog5000	  */
/*																							  */
/* Victory Strategy AI																		  */
/************************************************************************************************/
int CvPlayerAI::AI_getCultureVictoryStage() const
{

	PROFILE_EXTRA_FUNC();
	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_CULTURE") <= 0 || !GC.getGame().culturalVictoryValid())
	{
		return 0;
	}

	//If AIWeight for cultural is 0, no stage
	if (GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_CULTURE) == 0)
	{
		return 0;
	}

	// Necessary as capital city pointer is used later
	if (getCapitalCity() == NULL)
	{
		return 0;
	}

	int iHighCultureCount = 0;
	int iCloseToLegendaryCount = 0;
	int iLegendaryCount = 0;

	foreach_(const CvCity * cityX, cities())
	{
		if (cityX->getCultureLevel() >= GC.getGame().culturalVictoryCultureLevel() - 1)
		{
			int aiCityCommerces[NUM_COMMERCE_TYPES];
			cityX->getCommerces(aiCityCommerces);
			if (aiCityCommerces[COMMERCE_CULTURE] / 100 > 100)
			{
				iHighCultureCount++;
			}
			const int iVictoryThreshold = GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel());

			// is already there?
			if (cityX->getCulture(getID()) > iVictoryThreshold)
			{
				iLegendaryCount++;
			}
			// is over 1/2 of the way there?
			else if (cityX->getCulture(getID()) > iVictoryThreshold / 2)
			{
				iCloseToLegendaryCount++;
			}
		}
	}

	if (iLegendaryCount >= GC.getGame().culturalVictoryNumCultureCities())
	{
		// Already won, keep playing culture heavy but do some tech to keep pace if human wants to keep playing
		return 3;
	}
	if (iCloseToLegendaryCount >= GC.getGame().culturalVictoryNumCultureCities())
	{
		return 4;
	}

	if (isHumanPlayer())
	{
		if (getCommercePercent(COMMERCE_CULTURE) > 50)
		{
			return 3;
		}
		if (!GC.getGame().isDebugMode())
		{
			return 0;
		}
	}

	if (GC.getGame().getStartEra() > 1)
	{
		return 0;
	}

	bool bHistoricalCalendar = GC.getGame().isModderGameOption(MODDERGAMEOPTION_USE_HISTORICAL_ACCURATE_CALENDAR);
	if (getCapitalCity()->getGameTurnFounded(bHistoricalCalendar) > (10 + GC.getGame().getStartTurn()))
	{
		if (iHighCultureCount < GC.getGame().culturalVictoryNumCultureCities())
		{
			//the loss of the capital indicates it might be a good idea to abort any culture victory
			return 0;
		}
	}

	int iValue = GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_CULTURE);

	iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

	if (iValue > 20 && getNumCities() >= GC.getGame().culturalVictoryNumCultureCities())
	{
		iValue += 10 * countHolyCities();
	}

	iValue += (AI_getStrategyRand(0) % 100);

	if (iValue < 100)
	{
		return 0;
	}

	if (getCurrentEra() >= (GC.getNumEraInfos() - (2 + AI_getStrategyRand(1) % 2))) // K-mod
	{
		bool bAt3 = false;

		// if we have enough high culture cities, go to stage 3
		if (iHighCultureCount >= GC.getGame().culturalVictoryNumCultureCities())
		{
			bAt3 = true;
		}

		// if we have a lot of religion, may be able to catch up quickly
		if (countTotalHasReligion() >= getNumCities() * 3)
		{
			if (getNumCities() >= GC.getGame().culturalVictoryNumCultureCities())
			{
				bAt3 = true;
			}
		}

		if (bAt3)
		{
			if (AI_cultureVictoryTechValue(getCurrentResearch()) < 100)
			{
				return 4;
			}

			return 3;
		}
	}

	if (getCurrentEra() >= ((GC.getNumEraInfos() / 3) + AI_getStrategyRand(2) % 2)) // K-mod
	{
		return 2;
	}

	return 1;
}

int CvPlayerAI::AI_getSpaceVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue;

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_SPACE") <= 0)
	{
		return 0;
	}

	//If AIWeight for Space Vict. is 0, no stage
	if (GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_SPACE) == 0)
	{
		return 0;
	}

	if (getCapitalCity() == NULL)
	{
		return 0;
	}

	// Better way to determine if spaceship victory is valid?
	VictoryTypes eSpace = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).conditionValue(VICTORY_CONDITION_DELAY_TURNS) > 0)
			{
				eSpace = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eSpace == NO_VICTORY)
	{
		return 0;
	}

	// If have built Apollo, then the race is on!
	bool bHasApollo = false;
	bool bNearAllTechs = true;
	for (int iI = 0; iI < GC.getNumProjectInfos(); iI++)
	{
		if (GC.getProjectInfo((ProjectTypes)iI).getLaunchesVictory() == eSpace)
		{
			if (GET_TEAM(getTeam()).getProjectCount((ProjectTypes)iI) > 0)
			{
				bHasApollo = true;
			}
			else
			{
				if (!GET_TEAM(getTeam()).isHasTech(GC.getProjectInfo((ProjectTypes)iI).getTechPrereq()))
				{
					if (!isResearchingTech(GC.getProjectInfo((ProjectTypes)iI).getTechPrereq()))
					{
						bNearAllTechs = false;
					}
				}
			}
		}
	}

	if (bHasApollo)
	{
		if (bNearAllTechs)
		{
			bool bOtherLaunched = false;
			if (GET_TEAM(getTeam()).getVictoryCountdown(eSpace) >= 0)
			{
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (iTeam != getTeam())
					{
						if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) >= 0)
						{
							if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) < GET_TEAM(getTeam()).getVictoryCountdown(eSpace))
							{
								bOtherLaunched = true;
								break;
							}

							if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) == GET_TEAM(getTeam()).getVictoryCountdown(eSpace) && (iTeam < getTeam()))
							{
								bOtherLaunched = true;
								break;
							}
						}
					}
				}
			}
			else
			{
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) >= 0)
					{
						bOtherLaunched = true;
						break;
					}
				}
			}

			if (!bOtherLaunched)
			{
				return 4;
			}

			return 3;
		}

		if (GET_TEAM(getTeam()).getBestKnownTechScorePercent() > (m_iVictoryStrategyHash & AI_VICTORY_SPACE3 ? 80 : 85))
		{
			return 3;
		}
	}

	if (isHumanPlayer() && !(GC.getGame().isDebugMode()))
	{
		return 0;
	}

	// If can't build Apollo yet, then consider making player push for this victory type
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_SPACE);

		if (GET_TEAM(getTeam()).isAVassal())
		{
			iValue += 20;
		}

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

		iValue += (AI_getStrategyRand(3) % 100);

		if (iValue >= 100)
		{
			if (getCurrentEra() >= GC.getNumEraInfos() - 3)
			{
				return 2;
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getConquestVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue;

	if (GET_TEAM(getTeam()).isAVassal())
	{
		return 0;
	}

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_CONQUEST") <= 0)
	{
		return 0;
	}

	//If AIWeight for Conquest Vict. is 0, no stage
	if (GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_CONQUEST) == 0)
	{
		return 0;
	}

	VictoryTypes eConquest = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).conditionFlag(VICTORY_CONDITION_CONQUEST))
			{
				eConquest = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eConquest == NO_VICTORY)
	{
		return 0;
	}

	// Check for whether we are very powerful, looking good for conquest
	int iOurPower = GET_TEAM(getTeam()).getPower(true);
	int iOurPowerRank = 1;
	int iTotalPower = 0;
	int iNumNonVassals = 0;
	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (iI != getTeam())
		{
			CvTeam& kTeam = GET_TEAM((TeamTypes)iI);
			if (kTeam.isAlive() && !(kTeam.isMinorCiv()))
			{
				if (!kTeam.isAVassal())
				{
					iTotalPower += kTeam.getPower(true);
					iNumNonVassals++;

					if (GET_TEAM(getTeam()).isHasMet((TeamTypes)iI))
					{
						if (95 * kTeam.getPower(false) > 100 * iOurPower)
						{
							iOurPowerRank++;
						}
					}
				}
			}
		}
	}
	int iAverageOtherPower = iTotalPower / std::max(1, iNumNonVassals);

	if (3 * iOurPower > 4 * iAverageOtherPower)
	{
		// BBAI TODO: Have we declared total war on anyone?  Need some aggressive action taken, maybe past war success
		int iOthersWarMemoryOfUs = 0;
		for (int iPlayer = 0; iPlayer < MAX_PC_PLAYERS; iPlayer++)
		{
			if (GET_PLAYER((PlayerTypes)iPlayer).getTeam() != getTeam() && GET_PLAYER((PlayerTypes)iPlayer).isEverAlive())
			{
				iOthersWarMemoryOfUs += GET_PLAYER((PlayerTypes)iPlayer).AI_getMemoryCount(getID(), MEMORY_DECLARED_WAR);
			}
		}

		if (GET_TEAM(getTeam()).getHasMetCivCount(false) > GC.getGame().countCivPlayersAlive() / 4)
		{
			if (iOurPowerRank <= 1 + (GET_TEAM(getTeam()).getHasMetCivCount(true) / 10))
			{
				if ((iOurPower > 2 * iAverageOtherPower) && (iOurPower - iAverageOtherPower > 100))
				{
					if (iOthersWarMemoryOfUs > 0)
					{
						return 4;
					}
				}
			}
		}

		if (getCurrentEra() >= ((GC.getNumEraInfos() / 3)))
		{
			if (iOurPowerRank <= 1 + (GET_TEAM(getTeam()).getHasMetCivCount(true) / 7))
			{
				if (iOthersWarMemoryOfUs > 2)
				{
					return 3;
				}
			}
		}
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	// Check for whether we are inclined to pursue a conquest strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_CONQUEST);

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 20 : 0);

		iValue += (AI_getStrategyRand(4) % 100); // K-mod

		if (iValue >= 100)
		{
			if (m_iStrategyHash & AI_STRATEGY_GET_BETTER_UNITS)
			{
				if ((getNumCities() > 3) && (4 * iOurPower > 5 * iAverageOtherPower))
				{
					return 2;
				}
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getDominationVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	if (GET_TEAM(getTeam()).isAVassal())
	{
		return 0;
	}

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_DOMINATION") <= 0)
	{
		return 0;
	}

	//If AIWeight for Conquest Vict. is 0, no stage
	if (GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_DOMINATION) == 0)
	{
		return 0;
	}


	VictoryTypes eDomination = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.conditionValue(VICTORY_CONDITION_LAND_PERCENT) > 0 && kVictoryInfo.conditionValue(VICTORY_CONDITION_POPULATION_PERCENT_LEAD))
			{
				eDomination = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eDomination == NO_VICTORY)
	{
		return 0;
	}

	int iPercentOfDomination = 0;
	int iOurPopPercent = (100 * GET_TEAM(getTeam()).getTotalPopulation()) / std::max(1, GC.getGame().getTotalPopulation());
	int iOurLandPercent = (100 * GET_TEAM(getTeam()).getTotalLand()) / std::max(1, GC.getMap().getLandPlots());

	iPercentOfDomination = (100 * iOurPopPercent) / std::max(1, GC.getGame().getAdjustedPopulationPercent(eDomination));
	iPercentOfDomination = std::min(iPercentOfDomination, (100 * iOurLandPercent) / std::max(1, GC.getGame().getAdjustedLandPercent(eDomination)));


	if (iPercentOfDomination > 80)
	{
		return 4;
	}

	if (iPercentOfDomination > 50)
	{
		return 3;
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	// Check for whether we are inclined to pursue a domination strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_DOMINATION);

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 20 : 0);

		iValue += (AI_getStrategyRand(5) % 100); // K-mod

		if (iValue >= 100)
		{
			if (getNumCities() > 3 && (GC.getGame().getPlayerRank(getID()) < (GC.getGame().countCivPlayersAlive() + 1) / 2))
			{
				return 2;
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getDiplomacyVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_DIPLOMACY") <= 0)
	{
		return 0;
	}

	//If AIWeight for Conquest Vict. is 0, no stage
	if (GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_DIPLOMACY) == 0)
	{
		return 0;
	}

	std::vector<VictoryTypes> veDiplomacy;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).conditionFlag(VICTORY_CONDITION_DIPLO_VOTE))
			{
				veDiplomacy.push_back((VictoryTypes)iI);
			}
		}
	}

	if (veDiplomacy.empty())
	{
		return 0;
	}

	// Check for whether we are elligible for election
	bool bVoteEligible = false;
	for (int iVoteSource = 0; iVoteSource < GC.getNumVoteSourceInfos(); iVoteSource++)
	{
		if (GC.getGame().isDiploVote((VoteSourceTypes)iVoteSource))
		{
			if (GC.getGame().isTeamVoteEligible(getTeam(), (VoteSourceTypes)iVoteSource))
			{
				bVoteEligible = true;
				break;
			}
		}
	}

	bool bDiploInclined = false;

	// Check for whether we are inclined to pursue a diplomacy strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getVictoryWeight(VICTORY_PURSUIT_DIPLOMACY);

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

		iValue += (AI_getStrategyRand(6) % 100); // K-mod

		// BBAI TODO: Level 2?

		if (iValue >= 100)
		{
			bDiploInclined = true;
		}
	}

	if (bVoteEligible && (bDiploInclined || isHumanPlayer()))
	{
		// BBAI TODO: Level 4 - close to enough to win a vote?

		return 3;
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	if (bDiploInclined)
	{
		return 1;
	}

	return 0;
}

/// \brief Returns whether player is pursuing a particular victory strategy.
///
/// Victory strategies are computed on demand once per turn and stored for the rest
/// of the turn.  Each victory strategy type has 4 levels, the first two are
/// determined largely from AI tendencies and randomn dice rolls.  The second
/// two are based on measurables and past actions, so the AI can use them to
/// determine what other players (including the human player) are doing.
bool CvPlayerAI::AI_isDoVictoryStrategy(int iVictoryStrategy) const
{
	if (isNPC() || !isAlive()) //isMinorCiv() ||
	{
		return false;
	}

	return (iVictoryStrategy & AI_getVictoryStrategyHash());
}

bool CvPlayerAI::AI_isDoVictoryStrategyLevel4() const
{
	if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DIPLOMACY4))
	{
		return true;
	}

	return false;
}

bool CvPlayerAI::AI_isDoVictoryStrategyLevel3() const
{
	if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DIPLOMACY3))
	{
		return true;
	}

	return false;
}

void CvPlayerAI::AI_forceUpdateVictoryStrategies()
{
	//this forces a recache.
	m_iVictoryStrategyHashCacheTurn = -1;
}

int CvPlayerAI::AI_getVictoryStrategyHash() const
{
	PROFILE_FUNC();

	if (isNPC() || !isAlive()) //isMinorCiv() ||
	{
		return 0;
	}

	if ((m_iVictoryStrategyHash != 0) && (m_iVictoryStrategyHashCacheTurn == GC.getGame().getGameTurn()))
	{
		return m_iVictoryStrategyHash;
	}

	m_iVictoryStrategyHash = AI_DEFAULT_VICTORY_STRATEGY;
	m_iVictoryStrategyHashCacheTurn = GC.getGame().getGameTurn();

	if (getCapitalCity() == NULL)
	{
		return m_iVictoryStrategyHash;
	}

	bool bStartedOtherLevel3 = false;
	bool bStartedOtherLevel4 = false;

	// Space victory
	int iVictoryStage = AI_getSpaceVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_SPACE1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_SPACE2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_SPACE3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_SPACE4;
				}
			}
		}
	}

	// Conquest victory
	iVictoryStage = AI_getConquestVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST4;
				}
			}
		}
	}

	// Domination victory
	iVictoryStage = AI_getDominationVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION4;
				}
			}
		}
	}

	// Cultural victory
	iVictoryStage = AI_getCultureVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_CULTURE1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_CULTURE2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_CULTURE3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_CULTURE4;
				}
			}
		}
	}

	// Diplomacy victory
	iVictoryStage = AI_getDiplomacyVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY2;
			if (iVictoryStage > 2 && !bStartedOtherLevel3)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY4;
				}
			}
		}
	}

	return m_iVictoryStrategyHash;
}
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  03/18/10								jdog5000	  */
/*																							  */
/* War Strategy AI																			  */
/************************************************************************************************/
// AIAndy: Calculate strategy rand in separate method to control when it is computed for MP
void CvPlayerAI::AI_calculateStrategyRand()
{
	if (m_iStrategyRand <= 0)
	{
		m_iStrategyRand = 1 + GC.getGame().getSorenRandNum(100000, "AI Strategy Rand");
	}
}

// K-Mod, based on BBAI
int CvPlayerAI::AI_getStrategyRand(int iShift) const
{
	PROFILE_EXTRA_FUNC();
	const unsigned iBits = 16;

	iShift += getCurrentEra();
	while (iShift < 0)
		iShift += iBits;
	iShift %= iBits;

	if (m_iStrategyRand <= 0)
	{
		m_iStrategyRand = GC.getGame().getSorenRandNum((1 << (iBits + 1)) - 1, "AI Strategy Rand");
	}

	return (m_iStrategyRand << iShift) + (m_iStrategyRand >> (iBits - iShift));
}

bool CvPlayerAI::AI_isDoStrategy(int iStrategy) const
{
	if (isHumanPlayer() || isNPC() || !isAlive()) //isMinorCiv() |
	{
		return false;
	}
	return (iStrategy & AI_getStrategyHash());
}

void CvPlayerAI::AI_forceUpdateStrategies()
{
	//this forces a recache.
	m_iStrategyHashCacheTurn = -1;
}

int CvPlayerAI::AI_getStrategyHash() const
{
	PROFILE_FUNC();

	if ((m_iStrategyHash != 0) && (m_iStrategyHashCacheTurn == GC.getGame().getGameTurn()))
	{
		return m_iStrategyHash;
	}

	const FlavorTypes AI_FLAVOR_MILITARY = (FlavorTypes)0;
	const FlavorTypes AI_FLAVOR_RELIGION = (FlavorTypes)1;
	const FlavorTypes AI_FLAVOR_PRODUCTION = (FlavorTypes)2;
	const FlavorTypes AI_FLAVOR_GOLD = (FlavorTypes)3;
	const FlavorTypes AI_FLAVOR_SCIENCE = (FlavorTypes)4;
	const FlavorTypes AI_FLAVOR_CULTURE = (FlavorTypes)5;
	const FlavorTypes AI_FLAVOR_GROWTH = (FlavorTypes)6;

	int iLastStrategyHash = m_iStrategyHash;

	m_iStrategyHash = AI_DEFAULT_STRATEGY;
	m_iStrategyHashCacheTurn = GC.getGame().getGameTurn();

	if (AI_getFlavorValue(AI_FLAVOR_PRODUCTION) >= 2) // 0, 2, 5 or 10 in default xml [augustus 5, frederick 10, huayna 2, jc 2, chinese leader 2, qin 5, ramsess 2, roosevelt 5, stalin 2]
	{
		m_iStrategyHash |= AI_STRATEGY_PRODUCTION;
	}

	if (!getCapitalCity())
	{
		return m_iStrategyHash;
	}

	CvTeamAI& team = GET_TEAM(getTeam());
	int iMetCount = team.getHasMetCivCount(true);

	//Unit Analysis
	int iBestSlowUnitCombat = -1;
	int iBestFastUnitCombat = -1;

	bool bHasMobileArtillery = false;
	bool bHasMobileAntiair = false;
	bool bHasBomber = false;

	int iNukeCount = 0;

	int iAttackUnitCount = 0;

	// K-Mod
	int iAverageEnemyUnit = 0;
	int iTypicalAttack = getTypicalUnitValue(UNITAI_ATTACK);
	int iTypicalDefence = getTypicalUnitValue(UNITAI_CITY_DEFENSE);
	// K-Mod end

	CvCity* pCapitalCity = getCapitalCity();
	if (pCapitalCity != NULL)
	{
		// #430 F2b (enabler.md): iterate the capital city's LISTED unit frontier instead of scanning every unit
		// info and calling canTrain per id -- getAvailableUnits IS the city's LISTED unit frontier. Guarded
		// non-NULL; the loop body is wholly inside the gate and its accumulations (count/max/flags) are
		// order-independent, so forward iteration matches.
		std::vector<int> vecTrainable;
		pCapitalCity->getAvailableUnits(vecTrainable);
		for (std::vector<int>::const_iterator it = vecTrainable.begin(), itEnd = vecTrainable.end(); it != itEnd; ++it)
		{
			const UnitTypes eUnitX = static_cast<UnitTypes>(*it);
			const CvUnitInfo& unit = GC.getUnitInfo(eUnitX);
			const int iMoves = (unit.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100);

			if (unit.hasUnitAI(UNITAI_RESERVE)
			|| unit.hasUnitAI(UNITAI_ATTACK_CITY)
			|| unit.hasUnitAI(UNITAI_COUNTER)
			|| unit.hasUnitAI(UNITAI_PILLAGE))
			{
				iAttackUnitCount++;

				if (iMoves == 1)
				{
					iBestSlowUnitCombat = std::max(iBestSlowUnitCombat, (unit.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100));
				}
				else if (iMoves > 1)
				{
					iBestFastUnitCombat = std::max(iBestFastUnitCombat, (unit.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100));
				}
			}
			// Mobile anti-air and artillery flags only meant for land units
			if (unit.getDomain() == DOMAIN_LAND && iMoves > 1)
			{
				if (unit.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) > 25)
				{
					bHasMobileAntiair = true;
				}
				if (unit.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT) > 10)
				{
					bHasMobileArtillery = true;
				}
			}
			if ((unit.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100) > 1 && !unit.hasSkill(CLS_SKILL_SUICIDE)
			&& unit.getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100 > 10 && unit.getAirCombat() > 0)
			{
				bHasBomber = true;
			}
			if ((unit.getAir(AIR_NUKE_RANGE, CASC_SCOPE_UNIT) / 100) > 0)
			{
				iNukeCount++;
			}
		}
	}

	// K-Mod
	{
		int iTotalPower = 0;
		int iTotalWeightedValue = 0;
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kPlayer.getTeam() != getTeam())
			{
				if (kPlayer.isAlive() && team.isHasMet(kPlayer.getTeam()))
				{
					// Attack units are scaled down to roughly reflect their limitations.
					// (eg. Knights (10) vs Macemen (8). Cavalry (15) vs Rifles (14). Tank (28) vs Infantry (20) / Marine (24) )
					int iValue = std::max(100 * kPlayer.getTypicalUnitValue(UNITAI_ATTACK) / 110, kPlayer.getTypicalUnitValue(UNITAI_CITY_DEFENSE));
					iTotalWeightedValue += kPlayer.getPower() * iValue;
					iTotalPower += kPlayer.getPower();
				}
			}
		}
		if (iTotalPower == 0)
			iAverageEnemyUnit = 0;
		else
			iAverageEnemyUnit = iTotalWeightedValue / iTotalPower;

		// A bit of random variation...
		iAverageEnemyUnit *= (91 + AI_getStrategyRand(1) % 20);
		iAverageEnemyUnit /= 100;
	}

	//if (iAttackUnitCount <= 1)
	if (iAttackUnitCount <= 1 || 100 * iAverageEnemyUnit > 140 * iTypicalAttack && 100 * iAverageEnemyUnit > 140 * iTypicalDefence)
	{
		m_iStrategyHash |= AI_STRATEGY_GET_BETTER_UNITS;
	}
	// K-Mod end

	if (iBestFastUnitCombat > iBestSlowUnitCombat)
	{
		m_iStrategyHash |= AI_STRATEGY_FASTMOVERS;
		if (bHasMobileArtillery && bHasMobileAntiair)
		{
			m_iStrategyHash |= AI_STRATEGY_LAND_BLITZ;
		}
	}
	if (iNukeCount > 0)
	{
		if ((GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb() + +AI_getStrategyRand(7) % 15) >= (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 37 : 43))
		{
			m_iStrategyHash |= AI_STRATEGY_OWABWNW;
		}
	}
	if (bHasBomber)
	{
		if (!(m_iStrategyHash & AI_STRATEGY_LAND_BLITZ))
		{
			m_iStrategyHash |= AI_STRATEGY_AIR_BLITZ;
		}
		else
		{
			if ((AI_getStrategyRand(8) % 2) == 0)
			{
				m_iStrategyHash |= AI_STRATEGY_AIR_BLITZ;
				m_iStrategyHash &= ~AI_STRATEGY_LAND_BLITZ;
			}
		}
	}

	

	//missionary
	{
		const ReligionTypes eStateReligion = getStateReligion();
		if (eStateReligion != NO_RELIGION && hasHolyCity(eStateReligion))
		{
			int iMissionary = (
					AI_getFlavorValue(AI_FLAVOR_GROWTH) * 2 // up to 10
				+	AI_getFlavorValue(AI_FLAVOR_CULTURE) * 4 // up to 40
				+	AI_getFlavorValue(AI_FLAVOR_RELIGION) * 6 // up to 60
			);
			{
				const CivicTypes eCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();
				if (eCivic != NO_CIVIC && (GC.getCivicInfo(eCivic).providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD)))
				{
					iMissionary += 20;
				}
			}
			iMissionary += 5*(countHolyCities() - 1) + 7*std::min(iMetCount, 5);

			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				if (!GET_PLAYER((PlayerTypes)iI).isAlive()
				|| iI == getID()
				|| !team.isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam())
				|| !team.isOpenBorders(GET_PLAYER((PlayerTypes)iI).getTeam()))
				{
					continue;
				}
				if (GET_PLAYER((PlayerTypes)iI).getStateReligion() == eStateReligion)
				{
					iMissionary += 10;
				}
				else if (!GET_PLAYER((PlayerTypes)iI).isNoNonStateReligionSpread())
				{
					iMissionary += (GET_PLAYER((PlayerTypes)iI).countHolyCities() == 0) ? 12 : 4;
				}
			}
			{
				bool bHasHolyBuilding = false;
				const CvCity* holyCity = GC.getGame().getHolyCity(getStateReligion());
				if (holyCity && holyCity->getOwner() == getID())
				{
					foreach_(const BuildingTypes eTypeX, holyCity->getHasBuildings())
					{
						if (GC.getBuildingInfo(eTypeX).getShrineReligion() == eStateReligion
						&& !holyCity->isDormantBuilding(eTypeX))
						{
							if (holyCity->isActiveBuilding(eTypeX))
							{
								bHasHolyBuilding = true;
								break;
							}
						}
					}
				}
				if (!bHasHolyBuilding)
				{
					iMissionary -= 10;
				}
				else
				{
					iMissionary += 10;
				}
			}
			iMissionary += 3*(AI_getStrategyRand(9) % 7);

			if (iMissionary > 100)
			{
				m_iStrategyHash |= AI_STRATEGY_MISSIONARY;
			}
		}
	}

	// Espionage
	int iTempValue = 0;
	if (getCommercePercent(COMMERCE_ESPIONAGE) == 0)
	{
		iTempValue += 4;
	}

	if (!team.hasWarPlan(true))
	{
		iTempValue += (team.getBestKnownTechScorePercent() < 85) ? 5 : 3;
	}

	iTempValue += (100 - AI_getEspionageWeight()) / 10;

	iTempValue += AI_getStrategyRand(10) % 12; // K-mod

	if (iTempValue > 10)
	{
		m_iStrategyHash |= AI_STRATEGY_BIG_ESPIONAGE;
	}

	// Turtle strategy
	if (team.isAtWar() && getNumCities() > 0)
	{
		int iMaxWarCounter = 0;
		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
		{
			if (iTeam != getTeam() && GET_TEAM((TeamTypes)iTeam).isAlive() && !GET_TEAM((TeamTypes)iTeam).isMinorCiv())
			{
				iMaxWarCounter = std::max(iMaxWarCounter, team.AI_getAtWarCounter((TeamTypes)iTeam));
			}
		}

		// Are we losing badly or recently attacked?
		if (team.AI_getWarSuccessCapitulationRatio() < -50 || iMaxWarCounter < 10)
		{
			if (team.AI_getEnemyPowerPercent(true) > std::max(150, GC.getDefineINT("BBAI_TURTLE_ENEMY_POWER_RATIO")))
			{
				m_iStrategyHash |= AI_STRATEGY_TURTLE;
			}
		}
	}

	

	int iCurrentEra = getCurrentEra();
	int iParanoia = 0;
	int iCloseTargets = 0;
	int iOurDefensivePower = team.getDefensivePower();

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !GET_PLAYER((PlayerTypes)iI).isMinorCiv())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && team.isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
			{
				if (!GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isAVassal() && !team.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
				{
					if (team.AI_getWarPlan(GET_PLAYER((PlayerTypes)iI).getTeam()) != NO_WARPLAN)
					{
						iCloseTargets++;
					}
					else if (!team.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						// Are they a threat?
						int iTempParanoia = 0;

						const int iTheirPower = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true);

						if (4 * iTheirPower > 3 * iOurDefensivePower)
						{
							if (!GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isAtWar()
							|| GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_getEnemyPowerPercent(false) < 140)
							{
								// Memory of them declaring on us and our friends
								int iWarMemory = AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR);
								iWarMemory += (AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR_ON_FRIEND) + 1) / 2;

								if (iWarMemory > 0)
								{
									//they are a snake
									iTempParanoia += 50 + 50 * iWarMemory;

									
								}
							}
						}

						// Do we think our relations are bad?
						int iCloseness = AI_playerCloseness((PlayerTypes)iI, DEFAULT_PLAYER_CLOSENESS);
						if (iCloseness > 0)
						{
							int iAttitudeWarProb = 100 - GC.getLeaderHeadInfo(getPersonalityType()).getNoWarAttitudeProb(AI_getAttitude((PlayerTypes)iI));
							if (iAttitudeWarProb > 10)
							{
								if (4 * iTheirPower > 3 * iOurDefensivePower)
								{
									iTempParanoia += iAttitudeWarProb / 2;
								}

								iCloseTargets++;
							}

							if (iTheirPower > 2 * iOurDefensivePower)
							{
								if (AI_getAttitude((PlayerTypes)iI) != ATTITUDE_FRIENDLY)
								{
									iTempParanoia += 25;
								}
							}
						}

						if (iTempParanoia > 0)
						{
							iTempParanoia *= iTheirPower;
							iTempParanoia /= std::max(1, iOurDefensivePower);
						}

						// Do they look like they're going for militaristic victory?
						if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST4))
						{
							iTempParanoia += 200;
						}
						else if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
						{
							iTempParanoia += 100;
						}
						else if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3))
						{
							iTempParanoia += 50;
						}

						if (iTempParanoia > 0)
						{
							if (iCloseness == 0)
							{
								iTempParanoia /= 2;
							}

							iParanoia += iTempParanoia;
						}
					}
				}
			}
		}
	}

	if (m_iStrategyHash & AI_STRATEGY_GET_BETTER_UNITS)
	{
		iParanoia *= 3;
		iParanoia /= 2;
	}

	// Scale paranoia in later eras/larger games
	//iParanoia -= (100*(iCurrentEra + 1)) / std::max(1, GC.getNumEraInfos());

	// K-Mod. You call that scaling for "later eras/larger games"? It isn't scaling, and it doesn't use the map size.
	// Lets try something else. Rough and ad hoc, but hopefully a bit better.
	iParanoia *= (3 * GC.getNumEraInfos() - 2 * iCurrentEra);
	iParanoia /= 3 * (std::max(1, GC.getNumEraInfos()));
	// That starts as a factor of 1, and drop to 1/3.  And now for game size...
	iParanoia *= 14;
	iParanoia /= (7 + std::max(team.getHasMetCivCount(true), GC.getWorldInfo(GC.getMap().getWorldSize()).getDefaultPlayers()));

	// Alert strategy
	if (iParanoia >= 200)
	{
		m_iStrategyHash |= AI_STRATEGY_ALERT1;
		if (iParanoia >= 400)
		{
			m_iStrategyHash |= AI_STRATEGY_ALERT2;
		}
	}

	// Economic focus (K-Mod) - Note: this strategy is a gambit. The goal is catch up in tech by avoiding building units.
	if (!team.hasWarPlan(true)
	&& 100 * iAverageEnemyUnit >= 150 * iTypicalAttack
	&& 100 * iAverageEnemyUnit >= 180 * iTypicalDefence)
	{
		m_iStrategyHash |= AI_STRATEGY_ECONOMY_FOCUS;
	}

	

	if (!(AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
		&& !(m_iStrategyHash & AI_STRATEGY_MISSIONARY)
		&& (iCurrentEra <= (2 + (AI_getStrategyRand(11) % 2))) && (iCloseTargets > 0))
	{
		int iDagger = 0;
		iDagger += 12000 / std::max(100, (50 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarRand()));
		iDagger *= (AI_getStrategyRand(12) % 11);
		iDagger /= 10;
		iDagger += 5 * std::min(8, AI_getFlavorValue(AI_FLAVOR_MILITARY));

		if (!GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iDagger += range(100 - GC.getHandicapInfo(GC.getGame().getHandicapType()).getCostsModifier(COSTS_TRAIN, CASC_SCOPE_EMPIRE, true), 0, 15);
		}

		if ((getCapitalCity()->area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || (getCapitalCity()->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
		{
			iDagger += (iAttackUnitCount > 0) ? 40 : 20;
		}

		if (isRebel())
		{
			iDagger += 30;
		}

		if (iDagger >= AI_DAGGER_THRESHOLD)
		{
			m_iStrategyHash |= AI_STRATEGY_DAGGER;
		}
		else
		{
			if (iLastStrategyHash & AI_STRATEGY_DAGGER)
			{
				if (iDagger >= (9 * AI_DAGGER_THRESHOLD) / 10)
				{
					m_iStrategyHash |= AI_STRATEGY_DAGGER;
				}
			}
		}

		
	}

	if (!(m_iStrategyHash & AI_STRATEGY_ALERT2) && !(m_iStrategyHash & AI_STRATEGY_TURTLE))
	{//Crush
		int iWarCount = 0;
		int iCrushValue = 0;

		iCrushValue += AI_getStrategyRand(13) % (4 + AI_getFlavorValue(AI_FLAVOR_MILITARY) / 2);

		if (m_iStrategyHash & AI_STRATEGY_DAGGER)
		{
			iCrushValue += 3;
		}
		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iCrushValue += 3;
		}

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if ((GET_TEAM((TeamTypes)iI).isAlive()) && (iI != getTeam()))
			{
				if (team.AI_getWarPlan((TeamTypes)iI) != NO_WARPLAN)
				{
					if (!GET_TEAM((TeamTypes)iI).isAVassal())
					{
						if (team.AI_teamCloseness((TeamTypes)iI) > 0)
						{
							iWarCount++;
						}

						// K-Mod. (if we attack with our defenders, would they be beat their defenders?)
						if (100 * iTypicalDefence >= 110 * GET_TEAM((TeamTypes)iI).getTypicalUnitValue(UNITAI_CITY_DEFENSE))
						{
							iCrushValue += 2;
						}
					}

					if (team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_PREPARING_TOTAL)
					{
						iCrushValue += 6;
					}
					else if ((team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_TOTAL) && (team.AI_getWarPlanStateCounter((TeamTypes)iI) < 20))
					{
						iCrushValue += 6;
					}

					if ((team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_DOGPILE) && (team.AI_getWarPlanStateCounter((TeamTypes)iI) < 20))
					{
						for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
						{
							if ((iJ != iI) && iJ != getTeam() && GET_TEAM((TeamTypes)iJ).isAlive())
							{
								if ((atWar((TeamTypes)iI, (TeamTypes)iJ)) && !GET_TEAM((TeamTypes)iI).isAVassal())
								{
									iCrushValue += 4;
								}
							}
						}
					}
					if (GET_TEAM((TeamTypes)iI).isRebelAgainst(getTeam()))
					{
						iCrushValue += 4;
					}
				}
			}
		}
		if ((iWarCount <= 1) && (iCrushValue >= ((iLastStrategyHash & AI_STRATEGY_CRUSH) ? 9 : 10)))
		{
			m_iStrategyHash |= AI_STRATEGY_CRUSH;
		}

		
	}

	{
		CvTeamAI& kTeam = team;
		int iOurVictoryCountdown = kTeam.AI_getLowestVictoryCountdown();

		int iTheirVictoryCountdown = MAX_INT;

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if ((GET_TEAM((TeamTypes)iI).isAlive()) && (iI != getTeam()))
			{
				CvTeamAI& kOtherTeam = GET_TEAM((TeamTypes)iI);
				iTheirVictoryCountdown = std::min(iTheirVictoryCountdown, kOtherTeam.AI_getLowestVictoryCountdown());
			}
		}

		if (MAX_INT == iTheirVictoryCountdown)
		{
			iTheirVictoryCountdown = -1;
		}

		if ((iOurVictoryCountdown >= 0) && (iTheirVictoryCountdown < 0 || iOurVictoryCountdown <= iTheirVictoryCountdown))
		{
			m_iStrategyHash |= AI_STRATEGY_LAST_STAND;
		}
		else if ((iTheirVictoryCountdown >= 0))
		{
			if ((iTheirVictoryCountdown < iOurVictoryCountdown))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
			else if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
			else if (AI_isDoVictoryStrategyLevel4() || AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
		}

		if (iOurVictoryCountdown < 0)
		{
			if (isCurrentResearchRepeat())
			{
				int iStronger = 0;
				int iAlive = 1;
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (iTeam != getTeam())
					{
						CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
						if (kLoopTeam.isAlive())
						{
							iAlive++;
							if (kTeam.getPower(true) < kLoopTeam.getPower(true))
							{
								iStronger++;
							}
						}
					}
				}

				if ((iStronger <= 1) || (iStronger <= iAlive / 4))
				{
					m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
				}
			}
		}

	}

	if (isCurrentResearchRepeat())
	{
		int iTotalVictories = 0;
		int iAchieveVictories = 0;
		int iWarVictories = 0;


		int iThreshold = std::max(1, (GC.getGame().countCivTeamsAlive() + 1) / 4);

		CvTeamAI& kTeam = team;
		for (int iVictory = 0; iVictory < GC.getNumVictoryInfos(); iVictory++)
		{
			const CvVictoryInfo& kVictory = GC.getVictoryInfo((VictoryTypes)iVictory);
			if (GC.getGame().isVictoryValid((VictoryTypes)iVictory))
			{
				iTotalVictories++;
				if (kVictory.conditionFlag(VICTORY_CONDITION_DIPLO_VOTE))
				{
					//
				}
				else if (kVictory.conditionFlag(VICTORY_CONDITION_END_SCORE))
				{
					int iHigherCount = 0;
					int IWeakerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (GC.getGame().getTeamScore(getTeam()) < ((GC.getGame().getTeamScore((TeamTypes)iTeam) * 90) / 100))
								{
									iHigherCount++;
									if (kTeam.getPower(true) > kLoopTeam.getPower(true))
									{
										IWeakerCount++;
									}
								}
							}
						}
					}

					if (iHigherCount > 0)
					{
						if (IWeakerCount == iHigherCount)
						{
							iWarVictories++;
						}
					}
				}
				else if (kVictory.getCityCulture() > 0)
				{
					if (m_iStrategyHash & AI_VICTORY_CULTURE1)
					{
						iAchieveVictories++;
					}
				}
				else if (kVictory.conditionValue(VICTORY_CONDITION_MIN_LAND_PERCENT) > 0 || kVictory.conditionValue(VICTORY_CONDITION_LAND_PERCENT) > 0)
				{
					int iLargerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (kTeam.getTotalLand(true) < kLoopTeam.getTotalLand(true))
								{
									iLargerCount++;
								}
							}
						}
					}
					if (iLargerCount <= iThreshold)
					{
						iWarVictories++;
					}
				}
				else if (kVictory.conditionFlag(VICTORY_CONDITION_CONQUEST))
				{
					int iStrongerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (kTeam.getPower(true) < kLoopTeam.getPower(true))
								{
									iStrongerCount++;
								}
							}
						}
					}
					if (iStrongerCount <= iThreshold)
					{
						iWarVictories++;
					}
				}
				else
				{
					if (kTeam.getVictoryCountdown((VictoryTypes)iVictory) > 0)
					{
						iAchieveVictories++;
					}
				}
			}
		}

		if (iAchieveVictories == 0)
		{
			if (iWarVictories > 0)
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
		}
	}

	logDecisionAI(1, "[DAI/strategy] player=%d (%S) hash=0x%08x PRODUCTION=%d MISSIONARY=%d DAGGER=%d CRUSH=%d (flMil=%d flProd=%d flRel=%d flCul=%d flGro=%d)",
		getID(), getCivilizationDescription(0), m_iStrategyHash,
		(m_iStrategyHash & AI_STRATEGY_PRODUCTION) != 0,
		(m_iStrategyHash & AI_STRATEGY_MISSIONARY) != 0,
		(m_iStrategyHash & AI_STRATEGY_DAGGER) != 0,
		(m_iStrategyHash & AI_STRATEGY_CRUSH) != 0,
		AI_getFlavorValue(AI_FLAVOR_MILITARY), AI_getFlavorValue(AI_FLAVOR_PRODUCTION),
		AI_getFlavorValue(AI_FLAVOR_RELIGION), AI_getFlavorValue(AI_FLAVOR_CULTURE),
		AI_getFlavorValue(AI_FLAVOR_GROWTH));

	return m_iStrategyHash;
}


void CvPlayerAI::AI_nowHasTech(TechTypes eTech)
{
	// while its _possible_ to do checks, for financial trouble, and this tech adds financial buildings
	// if in war and this tech adds important war units
	// etc
	// it makes more sense to just redetermine what to produce
	// that is already done every time a civ meets a new civ, it makes sense to do it when a new tech is learned
	// if this is changed, then at a minimum, AI_isFinancialTrouble should be checked
	if (!isHumanPlayer())
	{
		int iGameTurn = GC.getGame().getGameTurn();

		// only reset at most every 10 turns
		if (iGameTurn > m_iTurnLastProductionDirty + 10)
		{
			// redeterimine the best things to build in each city
			AI_makeProductionDirty();

			m_iTurnLastProductionDirty = iGameTurn;
		}
	}
}


int CvPlayerAI::AI_countDeadlockedBonuses(const CvPlot* plot0) const
{
	PROFILE_EXTRA_FUNC();

	const int iMinRange = GC.getGame().getModderGameOption(MODDERGAMEOPTION_MIN_CITY_DISTANCE);
	const int iRange = iMinRange * 2;
	int iCount = 0;

	for (int iDX = -iRange; iDX <= iRange; iDX++)
	{
		for (int iDY = -iRange; iDY <= iRange; iDY++)
		{
			if (plotDistance(iDX, iDY, 0, 0) > CITY_PLOTS_RADIUS)
			{
				const CvPlot* plotX = plotXY(plot0->getX(), plot0->getY(), iDX, iDY);

				if (plotX
				&&  plotX->getBonusType(getTeam()) != NO_BONUS
				&& !plotX->isCityRadius()
				&& (plotX->area() == plot0->area() || plotX->isWater()))
				{
					bool bCanFound = false;
					bool bNeverFound = true;
					// Potentially blockable resource, look for a city site within a city radius
					for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
					{
						const CvPlot* plotY = plotCity(plotX->getX(), plotX->getY(), iI);
						if (plotY)
						{
							//canFound usually returns very quickly
							if (canFound(plotY->getX(), plotY->getY(), false))
							{
								bNeverFound = false;
								if (stepDistance(plot0->getX(), plot0->getY(), plotY->getX(), plotY->getY()) > iMinRange)
								{
									bCanFound = true;
									break;
								}
							}
						}
					}
					if (!bNeverFound && !bCanFound)
					{
						iCount++;
					}
				}
			}
		}
	}
	return iCount;
}

int CvPlayerAI::AI_getOurPlotStrength(const CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const
{
	PROFILE_FUNC();

	int iValue = 0;

	foreach_(const CvPlot * plotX, pPlot->rect(iRange, iRange) | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), plotX->getX(), plotX->getY());

		foreach_(const CvUnit * unitX, plotX->units())
		{
			if (unitX->getOwner() == getID()
			&& (bDefensiveBonuses && unitX->canDefend() || unitX->canAttack())
			&& !unitX->isInvisible(getTeam(), false)
			&& (unitX->atPlot(pPlot) || unitX->canEnterPlot(pPlot) || unitX->canEnterPlot(pPlot, MoveCheck::Attack)))
			{
				if (!bTestMoves)
				{
					iValue += unitX->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
				}
				else if (unitX->baseMoves() >= iDistance)
				{
					iValue += unitX->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
				}
			}
		}
	}
	return iValue;
}

int CvPlayerAI::AI_getEnemyPlotStrength(const CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const
{
	PROFILE_FUNC();

	int iValue = 0;

	foreach_(const CvPlot * plotX, pPlot->rect(iRange, iRange)
	| filtered(CvPlot::fn::area() == pPlot->area()))
	{
		const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), plotX->getX(), plotX->getY());

		foreach_(const CvUnit * unitX, plotX->units())
		{
			if (atWar(unitX->getTeam(), getTeam())
			&& (bDefensiveBonuses && unitX->canDefend() || unitX->canAttack())
			&& !unitX->canCoexistWithTeamOnPlot(getTeam(), *pPlot)
			&& pPlot->isValidDomainForAction(*unitX)
			&& (!bTestMoves || unitX->baseMoves() + plotX->isValidRoute(unitX) >= iDistance))
			{
				iValue += unitX->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
			}
		}
	}
	return iValue;
}

int CvPlayerAI::AI_goldToUpgradeAllUnits(int iExpThreshold) const
{
	PROFILE_FUNC();

	if (m_iUpgradeUnitsCacheTurn == GC.getGame().getGameTurn() && m_iUpgradeUnitsCachedExpThreshold == iExpThreshold)
	{
		return m_iUpgradeUnitsCachedGold;
	}

	int iTotalGold = 0;

	// cache the value for each unit type
	std::vector<int> aiUnitUpgradePrice(GC.getNumUnitInfos(), 0);	// initializes to zeros

	foreach_(const CvUnit * unitX, units())
	{
		// if experience is below threshold, skip this unit
		if (unitX == NULL || unitX->isDelayedDeath() || unitX->getExperience() < iExpThreshold)
		{
			continue;
		}
		const UnitTypes eUnitType = unitX->getUnitType();

		// check cached value for this unit type
		const int iCachedUnitGold = aiUnitUpgradePrice[eUnitType];
		if (iCachedUnitGold != 0)
		{
			// if positive, add it to the sum
			if (iCachedUnitGold > 0)
			{
				iTotalGold += iCachedUnitGold;
			}

			// either way, done with this unit
			continue;
		}

		int iUnitGold = 0;
		int iUnitUpgradePossibilities = 0;

		const UnitAITypes eUnitAIType = unitX->AI_getUnitAIType();
		if (unitX->plot() != NULL)
		{
			const CvArea* pUnitArea = unitX->area();
			const int iUnitValue = AI_unitValue(eUnitType, eUnitAIType, pUnitArea);

			foreach_(int iUnitX, GC.getUnitInfo(eUnitType).getUpgradeChain())
			{
				const UnitTypes eUnitY = (UnitTypes)iUnitX;
				// is it better?
				if (!GC.getUnitInfo(eUnitY).hasNotUnitAI(eUnitAIType)
				&& unitX->canUpgrade(eUnitY)
				&& AI_unitValue(eUnitY, eUnitAIType, pUnitArea) > iUnitValue)
				{
					// can we actually make this upgrade?
					iUnitGold += unitX->upgradePrice(eUnitY);
					iUnitUpgradePossibilities++;
				}
			}
		}

		// if we found any, find average and add to total
		if (iUnitUpgradePossibilities > 0)
		{
			iUnitGold /= iUnitUpgradePossibilities;

			// add to cache
			aiUnitUpgradePrice[eUnitType] = iUnitGold;

			// add to sum
			iTotalGold += iUnitGold;
		}
		else
		{
			// add to cache, dont upgrade to this type
			aiUnitUpgradePrice[eUnitType] = -1;
		}
	}

	m_iUpgradeUnitsCacheTurn = GC.getGame().getGameTurn();
	m_iUpgradeUnitsCachedExpThreshold = iExpThreshold;
	m_iUpgradeUnitsCachedGold = iTotalGold;

	return iTotalGold;
}

int CvPlayerAI::AI_goldTradeValuePercent() const
{
	return AI_isFinancialTrouble() ? 300 : 200;

}

int CvPlayerAI::AI_averageYieldMultiplier(YieldTypes eYield) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	FAssert(m_aiAverageYieldMultiplier[eYield] > 0);
	return m_aiAverageYieldMultiplier[eYield];
}

int CvPlayerAI::AI_averageCommerceMultiplier(CommerceTypes eCommerce) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	return m_aiAverageCommerceMultiplier[eCommerce];
}

int CvPlayerAI::AI_averageGreatPeopleMultiplier() const
{
	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}
	return m_iAverageGreatPeopleMultiplier;
}

//"100 eCommerce is worth (return) raw YIELD_COMMERCE
int CvPlayerAI::AI_averageCommerceExchange(CommerceTypes eCommerce) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	return m_aiAverageCommerceExchange[eCommerce];
}

void CvPlayerAI::AI_calculateAverages() const
{
	PROFILE_EXTRA_FUNC();
	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
		{
			m_aiAverageYieldMultiplier[iI] = 0;
		}
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			m_aiAverageCommerceMultiplier[iI] = 0;
		}
		m_iAverageGreatPeopleMultiplier = 0;

		int64_t sumFinalCommerce[NUM_COMMERCE_TYPES] = {};
		{
			int iTotalPopulation = 0;

			foreach_(const CvCity * cityX, cities())
			{
				const int iPopulation = std::max(cityX->getPopulation(), NUM_CITY_PLOTS);
				iTotalPopulation += iPopulation;

				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] += iPopulation * cityX->AI_yieldMultiplier((YieldTypes)iI);
				}
				int aiCityCommerces[NUM_COMMERCE_TYPES];
				cityX->getCommerces(aiCityCommerces);
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] += iPopulation * cityX->getTotalCommerceRateModifier((CommerceTypes)iI);

					sumFinalCommerce[iI] += aiCityCommerces[(CommerceTypes)iI];
				}
				m_iAverageGreatPeopleMultiplier += iPopulation * cityX->getTotalGreatPeopleRateModifier();
			}

			if (iTotalPopulation > 0)
			{
				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] = std::max(1, m_aiAverageYieldMultiplier[iI] / iTotalPopulation);
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] = std::max(1, m_aiAverageCommerceMultiplier[iI] / iTotalPopulation);
				}
				m_iAverageGreatPeopleMultiplier = std::max(1, m_iAverageGreatPeopleMultiplier / iTotalPopulation);
			}
			else
			{
				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] = 100;
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] = 100;
				}
				m_iAverageGreatPeopleMultiplier = 100;
			}
		}
		// Calculate Exchange Rate -- raw commerce in, realized channel commerce out.
		// ⚠ DANGLING, and deliberately explicit about it: the BASE leg (the per-channel commerce BEFORE the
		// total-modifier stack, legacy getBaseCommerceRateTimes100) has NO read on the new surface -- only the
		// REALIZED getCommerces exists. It read the realized value on BOTH sides, so the ratio was silently
		// always 100 while looking computed. A neutral 100 is the same answer stated honestly, and it stops the
		// no-op from reading as a live heuristic. It resolves when the §2a base-commerce read exists.
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			m_aiAverageCommerceExchange[iI] = 100;
		}
		// Timestamp
		m_iAveragesCacheTurn = GC.getGame().getGameTurn();
	}
}

// K-Mod edition
void CvPlayerAI::AI_convertUnitAITypesForCrush()
{
	PROFILE_EXTRA_FUNC();
	std::map<int, int> spare_units;
	std::multimap<int, CvUnit*> ordered_units;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		// Keep 1/2 of recommended floating defenders.
		if (!pLoopArea || pLoopArea->getAreaAIType(getTeam()) == AREAAI_ASSAULT
			|| pLoopArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
		{
			spare_units[pLoopArea->getID()] = 0;
		}
		else
		{
			spare_units[pLoopArea->getID()] = (2 * AI_getTotalFloatingDefenders(pLoopArea) - AI_getTotalFloatingDefendersNeeded(pLoopArea)) / 2;
		}
	}

	foreach_(CvUnit * pLoopUnit, units())
	{
		bool bValid = false;

		if (pLoopUnit->AI_getUnitAIType() == UNITAI_RESERVE || pLoopUnit->AI_getUnitAIType() == UNITAI_COLLATERAL
		|| pLoopUnit->AI_isCityAIType() && (pLoopUnit->noDefensiveBonus() || pLoopUnit->resolvedValue(URS_CITY_DEFENSE) <= 30))
		{
			bValid = true;
		}

		/*if ((pLoopUnit->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
		|| (pLoopUnit->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
		{
		bValid = false;
		}*/

		if (!pLoopUnit->canAttack() || (pLoopUnit->AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
		{
			bValid = false;
		}

		if (spare_units[pLoopUnit->area()->getID()] <= 0)
			bValid = false;

		if (bValid)
		{
			if (pLoopUnit->plot()->isCity())
			{
				if (pLoopUnit->plot()->getPlotCity()->getOwner() == getID())
				{
					if (pLoopUnit->plot()->getBestDefender(getID()) == pLoopUnit)
					{
						bValid = false;
					}
				}
			}
			// Super Forts begin *AI_defense* - don't convert units guarding a fort
			else if (pLoopUnit->plot()->isCity(true))
			{
				if (pLoopUnit->plot()->getNumDefenders(pLoopUnit->getOwner()) == 1)
				{
					bValid = false;
				}
			}
			// Super Forts end
		}

		if (bValid)
		{
			int iValue = AI_unitValue(pLoopUnit->getUnitType(), UNITAI_ATTACK_CITY, pLoopUnit->area());
			ordered_units.insert(std::make_pair(iValue, pLoopUnit));
		}
	}

	// convert the highest scoring units first.
	std::multimap<int, CvUnit*>::reverse_iterator rit;
	for (rit = ordered_units.rbegin(); rit != ordered_units.rend(); ++rit)
	{
		if (rit->first > 0 && spare_units[rit->second->area()->getID()] > 0)
		{
			rit->second->AI_setUnitAIType(UNITAI_ATTACK_CITY);
			spare_units[rit->second->area()->getID()]--;
		}
	}
}

int CvPlayerAI::AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance) const
{
	PROFILE_FUNC();
	FAssert(GET_PLAYER(eIndex).isAlive());
	FAssert(eIndex != getID());

	int iValue = 0;
	foreach_(CvCity * pLoopCity, cities())
	{
		iValue += pLoopCity->AI_playerCloseness(eIndex, iMaxDistance);
	}

	return iValue;
}


int CvPlayerAI::AI_getTotalProperty(PropertyTypes eProperty) const
{
	int iValue = 0;
	foreach_(CvCity * pLoopCity, cities())
	{
		iValue += pLoopCity->getPropertiesConst()->getValueByProperty(eProperty);
	}
	return iValue;
}

int CvPlayerAI::AI_getTotalAreaCityThreat(const CvArea* pArea, int* piLargestThreat) const
{
	PROFILE_FUNC();

	int iValue = 0;
	int iLargestThreat = 0;

	foreach_(CvCity * pLoopCity, cities() | filtered(CvCity::fn::area() == pArea))
	{
		const int iThreat = pLoopCity->AI_cityThreat();
		iValue += iThreat;

		if (iThreat > iLargestThreat)
		{
			iLargestThreat = iThreat;
		}
	}

	if (piLargestThreat != NULL)
	{
		*piLargestThreat = iLargestThreat;
	}

	return iValue;
}

int CvPlayerAI::AI_countNumAreaHostileUnits(const CvArea* pArea, bool bPlayer, bool bTeam, bool bNeutral, bool bHostile, const CvPlot* pPlot, int iMaxDistance) const
{
	PROFILE_FUNC();
	CvPlot* pLoopPlot;
	int iCount;
	int iI;

	iCount = 0;

	for (iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		pLoopPlot = GC.getMap().plotByIndex(iI);
		if ((pLoopPlot->area() == pArea) && pLoopPlot->isVisible(getTeam(), false) && stepDistance(pLoopPlot->getX(), pLoopPlot->getY(), pPlot->getX(), pPlot->getY()) <= iMaxDistance &&
			((bPlayer && pLoopPlot->getOwner() == getID()) || (bTeam && pLoopPlot->getTeam() == getTeam())
				|| (bNeutral && !pLoopPlot->isOwned()) || (bHostile && pLoopPlot->isOwned() && GET_TEAM(getTeam()).isAtWar(pLoopPlot->getTeam()))))
		{
			iCount += pLoopPlot->plotCount(PUF_isEnemy, getID(), 0, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID());
		}
	}
	return iCount;
}

//this doesn't include the minimal one or two garrison units in each city.
int CvPlayerAI::AI_getTotalFloatingDefendersNeeded(const CvArea* pArea) const
{
	PROFILE_FUNC();

	const int iAreaCities = pArea->getCitiesPerPlayer(getID());
	const int iCurrentEra = std::max(0, getCurrentEra() - GC.getGame().getStartEra() / 2);

	int iDefenders = 1 + iAreaCities * (iCurrentEra + (GC.getGame().getMaxCityElimination() > 0 ? 3 : 2));
	iDefenders /= 3;
	iDefenders += pArea->getPopulationPerPlayer(getID()) / 7;

	if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE || pArea->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE || pArea->getAreaAIType(getTeam()) == AREAAI_MASSING)
	{
		if (!pArea->isHomeArea(getID()) && iAreaCities <= std::min(4, pArea->getNumCities() / 3))
		{
			// Land war here, as floating defenders are based on cities/population need to make sure
			// AI defends its footholds in new continents well.
			iDefenders += GET_TEAM(getTeam()).countEnemyPopulationByArea(pArea) / 14;
		}
	}

	if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
	{
		iDefenders *= 2;
	}
	else if (AI_isDoStrategy(AI_STRATEGY_ALERT2))
	{
		iDefenders *= 2;
	}
	else if (AI_isDoStrategy(AI_STRATEGY_ALERT1))
	{
		iDefenders *= 3;
		iDefenders /= 2;
	}
	else if (pArea->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
	{
		iDefenders *= 2;
		iDefenders /= 3;
	}
	else if (pArea->getAreaAIType(getTeam()) == AREAAI_MASSING)
	{
		if (GET_TEAM(getTeam()).AI_getEnemyPowerPercent(true) < (10 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarNearbyPowerRatio()))
		{
			iDefenders *= 2;
			iDefenders /= 3;
		}
	}

	if (AI_getTotalAreaCityThreat(pArea) == 0)
	{
		iDefenders /= 2;
	}

	if (!GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
	{
		iDefenders *= 2;
		iDefenders /= 3;
	}

	// Afforess - check finances
	if (!GET_TEAM(getTeam()).hasWarPlan(true) && AI_isFinancialTrouble())
	{
		iDefenders = std::max(1, iDefenders / 2);
	}

	// Removed AI_STRATEGY_GET_BETTER_UNITS reduction, it was reducing defenses twice

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		iDefenders += 2 * iAreaCities;
		if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
		{
			iDefenders *= 2; //go crazy
		}
	}

	iDefenders *= 60;
	iDefenders /= std::max(30, (GC.getHandicapInfo(GC.getGame().getHandicapType()).getCostsModifier(COSTS_TRAIN, CASC_SCOPE_EMPIRE, true) - 20));

	if ((iCurrentEra < 3) && (GC.getGame().isOption(GAMEOPTION_BARBARIAN_RAGING)))
	{
		iDefenders += 2;
	}

	if (getCapitalCity() != NULL
	&&  getCapitalCity()->area() != pArea
	// Lessen defensive requirements only if not being attacked locally
	&& pArea->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE)
	{
		// This may be our first city captured on a large enemy continent,
		//	need defenses to scale up based on total number of area cities not just ours.
		iDefenders = std::min(iDefenders, iAreaCities * iAreaCities + pArea->getNumCities() - iAreaCities - 1);
	}

	// Build a few extra floating defenders for occupying forts
	iDefenders += iAreaCities / 2;

	return iDefenders;
}

int CvPlayerAI::AI_getTotalFloatingDefenders(const CvArea* pArea) const
{
	PROFILE_FUNC();
	int iCount = 0;

	// Strength-weighted supply (#395): merged defenders count as their aggregate
	// strength-equivalent (x1.5 per rank), not as their constituent bodies.
	iCount += AI_totalEffAreaUnitAIs(pArea, UNITAI_COLLATERAL);
	iCount += AI_totalEffAreaUnitAIs(pArea, UNITAI_RESERVE);
	iCount += std::max(0, (AI_totalEffAreaUnitAIs(pArea, UNITAI_CITY_DEFENSE) - (pArea->getCitiesPerPlayer(getID()) * 2)));
	iCount += AI_totalEffAreaUnitAIs(pArea, UNITAI_CITY_COUNTER);
	iCount += AI_totalEffAreaUnitAIs(pArea, UNITAI_CITY_SPECIAL);
	// BBAI TODO: Defense air?  Is this outdated?
	iCount += AI_totalEffAreaUnitAIs(pArea, UNITAI_DEFENSE_AIR);
	return iCount;
}

RouteTypes CvPlayerAI::AI_bestAdvancedStartRoute(const CvPlot* pPlot, int* piYieldValue) const
{
	PROFILE_EXTRA_FUNC();
	RouteTypes eBestRoute = NO_ROUTE;
	int iBestValue = -1;
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++)
	{
		RouteTypes eRoute = (RouteTypes)iI;

		int iValue = 0;
		int iCost = getAdvancedStartRouteCost(eRoute, true, pPlot);

		if (iCost >= 0)
		{
			iValue += GC.getRouteInfo(eRoute).getValue();

			if (iValue > 0)
			{
				int iYieldValue = 0;
				if (pPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					iYieldValue += ((GC.getRouteInfo(eRoute).getImprovementYield(pPlot->getImprovementType(), (YieldTypes)(YIELD_FOOD))) * 100);
					iYieldValue += ((GC.getRouteInfo(eRoute).getImprovementYield(pPlot->getImprovementType(), (YieldTypes)(YIELD_PRODUCTION))) * 60);
					iYieldValue += ((GC.getRouteInfo(eRoute).getImprovementYield(pPlot->getImprovementType(), (YieldTypes)(YIELD_COMMERCE))) * 40);
				}
				iValue *= 1000;
				iValue /= (1 + iCost);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestRoute = eRoute;
					if (piYieldValue != NULL)
					{
						*piYieldValue = iYieldValue;
					}
				}
			}
		}
	}
	return eBestRoute;
}

UnitTypes CvPlayerAI::AI_bestAdvancedStartUnitAI(const CvPlot* pPlot, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(eUnitAI != NO_UNITAI, "UnitAI is not assigned a valid value");

	int iValue;
	int iBestValue = 0;
	UnitTypes eBestUnit = NO_UNIT;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

		int iUnitCost = getAdvancedStartUnitCost((UnitTypes)iI, true, pPlot);
		if (iUnitCost >= 0)
		{
			iValue = AI_unitValue((UnitTypes)iI, eUnitAI, pPlot->area());

			if (iValue > 0)
			{
				//free promotions. slow?
				//only 1 promotion per source is counted (ie protective isn't counted twice)
				int iPromotionValue = 0;

				const UnitCombatTypes eUnitCombat = (UnitCombatTypes)kUnit.getCombatClass();

				for (int iJ = 0; iJ < GC.getNumPromotionInfos(); iJ++)
				{
					if (kUnit.grantsPromotion(iJ))
					{
						iPromotionValue += 15;
					}
					else if (isFreePromotion((UnitTypes)iI, (PromotionTypes)iJ))
					{
						iPromotionValue += 15;
					}
					else if (eUnitCombat != NO_UNITCOMBAT)
					{
						if (isFreePromotion(eUnitCombat, (PromotionTypes)iJ))
						{
							iPromotionValue += 15;
							continue;
						}
						for (int iK = 0; iK < GC.getNumTraitInfos(); iK++)
						{
							if (hasTrait((TraitTypes)iK)
							&& false /* trait free-promotion-by-unitcombat: the alive-with-source modifier plane */)
							{
								iPromotionValue += 15;
								break;
							}
						}
					}
				}
				iValue *= (iPromotionValue + 100);
				iValue /= 100;

				iValue *= (GC.getGame().getSorenRandNum(40, "AI Best Advanced Start Unit") + 100);
				iValue /= 100;

				iValue *= (getNumCities() + 2);
				iValue /= (getUnitCountPlusMaking((UnitTypes)iI) + getNumCities() + 2);

				FAssert((MAX_INT / 1000) > iValue);
				iValue *= 1000;

				iValue /= 1 + iUnitCost;

				iValue = std::max(1, iValue);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestUnit = (UnitTypes)iI;
				}
			}
		}
	}
	return eBestUnit;
}

CvPlot* CvPlayerAI::AI_advancedStartFindCapitalPlot() const
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pBestPlot = NULL;
	int iBestValue = -1;

	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kPlayer.isAlive())
		{
			if (kPlayer.getTeam() == getTeam())
			{
				CvPlot* pLoopPlot = kPlayer.getStartingPlot();
				if (pLoopPlot != NULL)
				{
					if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
					{
						int iX = pLoopPlot->getX();
						int iY = pLoopPlot->getY();

						int iValue = 1000;
						if (iPlayer == getID())
						{
							iValue += 1000;
						}
						else
						{
							iValue += GC.getGame().getSorenRandNum(100, "AI Advanced Start Choose Team Start");
						}
						CvCity* pNearestCity = GC.getMap().findCity(iX, iY, NO_PLAYER, getTeam());
						if (NULL != pNearestCity)
						{
							FAssert(pNearestCity->getTeam() == getTeam());
							int iDistance = stepDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
							if (iDistance < 10)
							{
								iValue /= (10 - iDistance);
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
				else
				{
					FErrorMsg("StartingPlot for a live player is NULL!");
				}
			}
		}
	}

	if (pBestPlot != NULL)
	{
		return pBestPlot;
	}

	FErrorMsg("AS: Failed to find a starting plot for a player");

	//Execution should almost never reach here.

	//Update found values just in case - particulary important for simultaneous turns.
	AI_updateFoundValues(true);

	pBestPlot = NULL;
	iBestValue = -1;

	if (NULL != getStartingPlot())
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			if (pLoopPlot->getArea() == getStartingPlot()->getArea())
			{
				int iValue = pLoopPlot->getFoundValue(getID());
				if (iValue > 0)
				{
					if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
					{
						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL)
	{
		return pBestPlot;
	}

	//Commence panic.
	FErrorMsg("Failed to find an advanced start starting plot");
	return NULL;
}


bool CvPlayerAI::AI_advancedStartPlaceExploreUnits(bool bLand)
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pBestExplorePlot = NULL;
	int iBestExploreValue = 0;
	UnitTypes eBestUnitType = NO_UNIT;

	UnitAITypes eUnitAI = NO_UNITAI;
	if (bLand)
	{
		eUnitAI = UNITAI_EXPLORE;
	}
	else
	{
		eUnitAI = UNITAI_EXPLORE_SEA;
	}

	foreach_(const CvCity * pLoopCity, cities())
	{
		CvPlot* pLoopPlot = pLoopCity->plot();
		const CvArea* pLoopArea = bLand ? pLoopCity->area() : pLoopPlot->waterArea();

		if (pLoopArea != NULL)
		{
			int iValue = std::max(0, pLoopArea->getNumUnrevealedTiles(getTeam()) - 10) * 10;
			iValue += std::max(0, pLoopArea->getNumTiles() - 50);

			if (iValue > 0)
			{
				int iOtherPlotCount = 0;
				int iGoodyCount = 0;
				int iExplorerCount = 0;
				const int iAreaId = pLoopArea->getID();

				foreach_(const CvPlot * pLoopPlot2, pLoopPlot->rect(4, 4))
				{
					iExplorerCount += pLoopPlot2->plotCount(PUF_isUnitAIType, eUnitAI, -1, NULL, NO_PLAYER, getTeam());
					if (pLoopPlot2->getArea() == iAreaId)
					{
						if (pLoopPlot2->isGoody())
						{
							iGoodyCount++;
						}
						if (pLoopPlot2->getTeam() != getTeam())
						{
							iOtherPlotCount++;
						}
					}
				}

				iValue -= 300 * iExplorerCount;
				iValue += 200 * iGoodyCount;
				iValue += 10 * iOtherPlotCount;
				if (iValue > iBestExploreValue)
				{
					UnitTypes eUnit = AI_bestAdvancedStartUnitAI(pLoopPlot, eUnitAI);
					if (eUnit != NO_UNIT)
					{
						eBestUnitType = eUnit;
						iBestExploreValue = iValue;
						pBestExplorePlot = pLoopPlot;
					}
				}
			}
		}
	}

	if (pBestExplorePlot != NULL)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_UNIT, pBestExplorePlot->getX(), pBestExplorePlot->getY(), eBestUnitType, true);
		return true;
	}
	return false;
}

void CvPlayerAI::AI_advancedStartRevealRadius(const CvPlot* pPlot, int iRadius)
{
	PROFILE_EXTRA_FUNC();
	for (int iRange = 1; iRange <= iRadius; iRange++)
	{
		for (int iX = -iRange; iX <= iRange; iX++)
		{
			for (int iY = -iRange; iY <= iRange; iY++)
			{
				if (plotDistance(0, 0, iX, iY) <= iRadius)
				{
					CvPlot* pLoopPlot = plotXY(pPlot->getX(), pPlot->getY(), iX, iY);

					if (NULL != pLoopPlot)
					{
						if (getAdvancedStartVisibilityCost(pLoopPlot) > 0)
						{
							doAdvancedStartAction(ADVANCEDSTARTACTION_VISIBILITY, pLoopPlot->getX(), pLoopPlot->getY(), -1, true);
						}
					}
				}
			}
		}
	}
}

bool CvPlayerAI::AI_advancedStartPlaceCity(const CvPlot* pPlot)
{
	PROFILE_EXTRA_FUNC();
	//If there is already a city, then improve it.
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_CITY, pPlot->getX(), pPlot->getY(), -1, true);

		pCity = pPlot->getPlotCity();
		if (pCity == NULL || pCity->getOwner() != getID())
		{
			//this should never happen since the cost for a city should be 0 if
			//the city can't be placed.
			//(It can happen if another player has placed a city in the fog)
			FErrorMsg("ADVANCEDSTARTACTION_CITY failed in unexpected way");
			return false;
		}
	}

	//Only expand culture when we have lots to spare. Never expand for the capital, the palace works fine on it's own
	if (pCity != getCapitalCity()
	&& getAdvancedStartPoints() > getAdvancedStartCultureCost(true, pCity) * 50
	&& pCity->getCultureLevel() <= 1)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_CULTURE, pPlot->getX(), pPlot->getY(), -1, true);
		//to account for culture expansion.
		pCity->AI_updateBestBuild();
	}

	int iPlotsImproved = algo::count_if(pCity->plots(true),
		bind(CvPlot::getWorkingCity, _1) == pCity &&
		bind(CvPlot::getImprovementType, _1) != NO_IMPROVEMENT
	);

	int iDivisor = std::max(1, 2000 / std::max(1, getAdvancedStartPoints()));

	// The HAPPINESS side alone (not the net): the target tracks how much happiness the city can support.
	int aWellbeing[NUM_WELLBEING_CHANNELS];
	pCity->realizedWellbeing(0, aWellbeing);
	int iTargetPopulation = aWellbeing[WELLBEING_HAPPINESS] / 100 + (getCurrentEra() / 2);
	iTargetPopulation /= iDivisor;

	while (iPlotsImproved < iTargetPopulation)
	{
		const CvPlot* pBestPlot;
		ImprovementTypes eBestImprovement = NO_IMPROVEMENT;
		int iBestValue = 0;

		for (int iI = 0; iI < pCity->getNumCityPlots(); iI++)
		{
			const int iValue = pCity->AI_getBestBuildValue(iI);
			if (iValue > iBestValue)
			{
				const BuildTypes eBuild = pCity->AI_getBestBuild(iI);
				if (eBuild != NO_BUILD)
				{
					const ImprovementTypes eImprovement = GC.getBuildInfo(eBuild).getImprovement();
					if (eImprovement != NO_IMPROVEMENT)
					{
						const CvPlot* pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);
						if (pLoopPlot != NULL && pLoopPlot->getImprovementType() != eImprovement)
						{
							eBestImprovement = eImprovement;
							pBestPlot = pLoopPlot;
							iBestValue = iValue;
						}
					}
				}
			}
		}

		if (iBestValue < 1)
		{
			break;
		}
		FAssert(pBestPlot != NULL);
		doAdvancedStartAction(ADVANCEDSTARTACTION_IMPROVEMENT, pBestPlot->getX(), pBestPlot->getY(), eBestImprovement, true);
		iPlotsImproved++;
		if (pCity->getPopulation() < iPlotsImproved)
		{
			doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pBestPlot->getX(), pBestPlot->getY(), -1, true);
		}
	}

	while (iPlotsImproved > pCity->getPopulation())
	{
		const int iPopCost = getAdvancedStartPopCost(true, pCity);
		if (iPopCost <= 0 || iPopCost > getAdvancedStartPoints())
		{
			break;
		}
		if (pCity->healthRate() < 0)
		{
			break;
		}
		doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pPlot->getX(), pPlot->getY(), -1, true);
	}
	pCity->AI_updateAssignWork();

	return true;
}




//Returns false if we have no more points.
bool CvPlayerAI::AI_advancedStartDoRoute(const CvPlot* pFromPlot, const CvPlot* pToPlot)
{
	PROFILE_EXTRA_FUNC();
	FAssert(pFromPlot != NULL);
	FAssert(pToPlot != NULL);

	gDLL->getFAStarIFace()->ForceReset(&GC.getStepFinder());
	if (gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), pFromPlot->getX(), pFromPlot->getY(), pToPlot->getX(), pToPlot->getY(), false, -1, true))
	{
		FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder());
		if (pNode != NULL)
		{
			if (pNode->m_iData1 > (1 + stepDistance(pFromPlot->getX(), pFromPlot->getY(), pToPlot->getX(), pToPlot->getY())))
			{
				//Don't build convulted paths.
				return true;
			}
		}

		while (pNode != NULL)
		{
			CvPlot* pPlot = GC.getMap().plotSorenINLINE(pNode->m_iX, pNode->m_iY);
			RouteTypes eRoute = AI_bestAdvancedStartRoute(pPlot);
			if (eRoute != NO_ROUTE)
			{
				if (getAdvancedStartRouteCost(eRoute, true, pPlot) > getAdvancedStartPoints())
				{
					return false;
				}
				doAdvancedStartAction(ADVANCEDSTARTACTION_ROUTE, pNode->m_iX, pNode->m_iY, eRoute, true);
			}
			pNode = pNode->m_pParent;
		}
	}
	return true;
}
void CvPlayerAI::AI_advancedStartRouteTerritory()
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pLoopPlot;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		pLoopPlot = GC.getMap().plotByIndex(iI);
		if ((pLoopPlot != NULL) && (pLoopPlot->getOwner() == getID()) && (pLoopPlot->getRouteType() == NO_ROUTE))
		{
			if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				BonusTypes eBonus = pLoopPlot->getBonusType(getTeam());
				if (eBonus != NO_BONUS)
				{
					if (GC.getImprovementInfo(pLoopPlot->getImprovementType()).isImprovementBonusTrade(eBonus))
					{
						const int iBonusValue = AI_bonusVal(eBonus, 1);
						if (iBonusValue > 9)
						{
							int iBestValue = 0;
							const CvPlot* pBestPlot = NULL;
							foreach_(const CvPlot * pLoopPlot2, pLoopPlot->rect(2, 2))
							{
								if (pLoopPlot2->getOwner() == getID())
								{
									if (pLoopPlot2->isConnectedToCapital() || pLoopPlot2->isCity())
									{
										int iValue = 1000;
										if (pLoopPlot2->isCity())
										{
											iValue += 100;
											if (pLoopPlot2->getPlotCity()->isCapital())
											{
												iValue += 100;
											}
										}
										if (pLoopPlot2->isRoute())
										{
											iValue += 100;
										}
										const int iDistance = GC.getMap().calculatePathDistance(pLoopPlot, pLoopPlot2);
										if (iDistance > 0)
										{
											iValue /= (1 + iDistance);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = pLoopPlot2;
											}
										}
									}
								}
							}
							if (pBestPlot != NULL)
							{
								if (!AI_advancedStartDoRoute(pLoopPlot, pBestPlot))
								{
									return;
								}
							}
						}
					}
				}
				if (pLoopPlot->getRouteType() == NO_ROUTE)
				{
					int iRouteYieldValue = 0;
					const RouteTypes eRoute = AI_bestAdvancedStartRoute(pLoopPlot, &iRouteYieldValue);
					if (eRoute != NO_ROUTE && iRouteYieldValue > 0)
					{
						doAdvancedStartAction(ADVANCEDSTARTACTION_ROUTE, pLoopPlot->getX(), pLoopPlot->getY(), eRoute, true);
					}
				}
			}
		}
	}

	//Connect Cities
	foreach_(const CvCity * pLoopCity, cities()
	| filtered(!CvCity::fn::isCapital() && !CvCity::fn::isConnectedToCapital()))
	{
		int iBestValue = 0;
		CvPlot* pBestPlot = NULL;
		const int iRange = 5;
		foreach_(CvPlot * pLoopPlot, pLoopCity->plot()->rect(iRange, iRange)
		| filtered(CvPlot::fn::getOwner() == getID() && (CvPlot::fn::isConnectedToCapital(NO_PLAYER) || CvPlot::fn::isCity())))
		{
			int iValue = 1000;
			if (pLoopPlot->isCity())
			{
				iValue += 500;
				if (pLoopPlot->getPlotCity()->isCapital())
				{
					iValue += 500;
				}
			}
			if (pLoopPlot->isRoute())
			{
				iValue += 100;
			}
			const int iDistance = GC.getMap().calculatePathDistance(pLoopCity->plot(), pLoopPlot);
			if (iDistance > 0)
			{
				iValue /= (1 + iDistance);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					pBestPlot = pLoopPlot;
				}
			}
		}
		if (NULL != pBestPlot && !AI_advancedStartDoRoute(pBestPlot, pLoopCity->plot()))
		{
			return;
		}
	}
}


void CvPlayerAI::AI_doAdvancedStart(bool bNoExit)
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(!isNPC(), "Should not be called for NPCs!");

	if (NULL == getStartingPlot())
	{
		FErrorMsg("error");
		return;
	}

	int iStartingPoints = getAdvancedStartPoints();
	int iRevealPoints = (iStartingPoints * 10) / 100;
	int iMilitaryPoints = (iStartingPoints * (isHumanPlayer() ? 17 : (10 + (GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb() / 3)))) / 100;
	int iCityPoints = iStartingPoints - (iMilitaryPoints + iRevealPoints);

	if (getCapitalCity() != NULL)
	{
		AI_advancedStartPlaceCity(getCapitalCity()->plot());
	}
	else
	{
		for (int iPass = 0; iPass < 2 && NULL == getCapitalCity(); ++iPass)
		{
			CvPlot* pBestCapitalPlot = AI_advancedStartFindCapitalPlot();

			if (pBestCapitalPlot != NULL)
			{
				if (!AI_advancedStartPlaceCity(pBestCapitalPlot))
				{
					FErrorMsg("AS AI: Unexpected failure placing capital");
				}
				break;
			}
			//If this point is reached, the advanced start system is broken.
			//Find a new starting plot for this player
			setStartingPlot(findStartingPlot(), true);
			//Redo Starting visibility
			const CvPlot* pStartingPlot = getStartingPlot();
			if (NULL != pStartingPlot)
			{
				for (int iPlotLoop = 0; iPlotLoop < GC.getMap().numPlots(); ++iPlotLoop)
				{
					CvPlot* pPlot = GC.getMap().plotByIndex(iPlotLoop);

					if (plotDistance(pPlot->getX(), pPlot->getY(), pStartingPlot->getX(), pStartingPlot->getY()) <= GC.getADVANCED_START_SIGHT_RANGE())
					{
						pPlot->setRevealed(getTeam(), true, false, NO_TEAM, false);
					}
				}
			}
		}

		if (getCapitalCity() == NULL)
		{
			if (!bNoExit)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_EXIT, -1, -1, -1, true);
			}
			return;
		}
	}

	iCityPoints -= (iStartingPoints - getAdvancedStartPoints());

	int iLastPointsTotal = getAdvancedStartPoints();

	for (int iPass = 0; iPass < 6; iPass++)
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			if (pLoopPlot->isRevealed(getTeam(), false))
			{
				if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
				{
					AI_advancedStartRevealRadius(pLoopPlot, CITY_PLOTS_RADIUS);
				}
				else
				{
					foreach_(const CvPlot * pLoopPlot2, pLoopPlot->cardinalDirectionAdjacent())
					{
						if (getAdvancedStartVisibilityCost(pLoopPlot2) > 0)
						{
							// Mildly maphackery but any smart human can see the terrain type of a tile.
							int iFoodYield = GC.getTerrainInfo(pLoopPlot2->getTerrainType()).getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) / 100;
							if (pLoopPlot2->getFeatureType() != NO_FEATURE)
							{
								iFoodYield += GC.getFeatureInfo(pLoopPlot2->getFeatureType()).getFlatYield(YIELD_FOOD, CASC_SCOPE_PLOT) / 100;
							}
							if (((iFoodYield >= 2) && !pLoopPlot2->isFreshWater()) || pLoopPlot2->isHills() || pLoopPlot2->isRiver())
							{
								doAdvancedStartAction(ADVANCEDSTARTACTION_VISIBILITY, pLoopPlot2->getX(), pLoopPlot2->getY(), -1, true);
							}
						}
					}
				}
			}
			if ((iLastPointsTotal - getAdvancedStartPoints()) > iRevealPoints)
			{
				break;
			}
		}
	}

	iLastPointsTotal = getAdvancedStartPoints();
	iCityPoints = std::min(iCityPoints, iLastPointsTotal);

	//Spend econ points on a tech?
	int iTechRand = 90 + GC.getGame().getSorenRandNum(20, "AI AS Buy Tech 1");
	int iTotalTechSpending = 0;

	if (getCurrentEra() == 0)
	{
		TechTypes eTech = AI_bestTech(1);
		if ((eTech != NO_TECH) && !GC.getTechInfo(eTech).isRepeat())
		{
			int iTechCost = getAdvancedStartTechCost(eTech, true);
			if (iTechCost > 0)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
				iTechRand -= 50;
				iTotalTechSpending += iTechCost;
			}
		}
	}

	bool bDonePlacingCities = false;
	for (int iPass = 0; iPass < 100; ++iPass)
	{
		const int iRand = iTechRand + 10 * getNumCities();
		if ((iRand > 0) && (GC.getGame().getSorenRandNum(100, "AI AS Buy Tech 2") < iRand))
		{
			const TechTypes eTech = AI_bestTech(1);
			if ((eTech != NO_TECH) && !GC.getTechInfo(eTech).isRepeat())
			{
				int iTechCost = getAdvancedStartTechCost(eTech, true);
				if ((iTechCost > 0) && ((iTechCost + iTotalTechSpending) < (iCityPoints / 4)))
				{
					doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
					iTechRand -= 50;
					iTotalTechSpending += iTechCost;

					foreach_(const CvCity * pLoopCity, cities())
					{
						AI_advancedStartPlaceCity(pLoopCity->plot());
					}
				}
			}
		}
		int iBestFoundValue = 0;
		CvPlot* pBestFoundPlot = NULL;
		AI_updateFoundValues(true);
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			//if (pLoopPlot->area() == getStartingPlot()->area())
			{
				if (plotDistance(getStartingPlot()->getX(), getStartingPlot()->getY(), pLoopPlot->getX(), pLoopPlot->getY()) < 9)
				{
					if (pLoopPlot->getFoundValue(getID()) > iBestFoundValue)
					{
						if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
						{
							pBestFoundPlot = pLoopPlot;
							iBestFoundValue = pLoopPlot->getFoundValue(getID());
						}
					}
				}
			}
		}

		if (iBestFoundValue < ((getNumCities() == 0) ? 1 : (500 + 250 * getNumCities())))
		{
			bDonePlacingCities = true;
		}
		if (!bDonePlacingCities)
		{
			const int iCost = getAdvancedStartCityCost(true, pBestFoundPlot);
			if (iCost > getAdvancedStartPoints())
			{
				bDonePlacingCities = true;
			}// at 500pts, we have 200, we spend 100.
			else if (((iLastPointsTotal - getAdvancedStartPoints()) + iCost) > iCityPoints)
			{
				bDonePlacingCities = true;
			}
		}

		if (!bDonePlacingCities)
		{
			if (!AI_advancedStartPlaceCity(pBestFoundPlot))
			{
				FErrorMsg("AS AI: Failed to place city (non-capital)");
				bDonePlacingCities = true;
			}
		}

		if (bDonePlacingCities)
		{
			break;
		}
	}


	bool bDoneWithTechs = false;
	while (!bDoneWithTechs)
	{
		bDoneWithTechs = true;
		const TechTypes eTech = AI_bestTech(1);
		if (eTech != NO_TECH && !GC.getTechInfo(eTech).isRepeat())
		{
			const int iTechCost = getAdvancedStartTechCost(eTech, true);
			if (iTechCost > 0 && iTechCost + iLastPointsTotal - getAdvancedStartPoints() <= iCityPoints)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
				bDoneWithTechs = false;
			}
		}
	}

	//Land
	AI_advancedStartPlaceExploreUnits(true);
	if (getCurrentEra() > 2)
	{
		//Sea
		AI_advancedStartPlaceExploreUnits(false);
		if (GC.getGame().circumnavigationAvailable()
		&& GC.getGame().getSorenRandNum(GC.getGame().countCivPlayersAlive(), "AI AS buy 2nd sea explorer") < 2)
		{
			AI_advancedStartPlaceExploreUnits(false);
		}
	}

	AI_advancedStartRouteTerritory();

	bool bDoneBuildings = (iLastPointsTotal - getAdvancedStartPoints()) > iCityPoints;
	for (int iPass = 0; iPass < 10 && !bDoneBuildings; ++iPass)
	{
		foreach_(CvCity * pLoopCity, cities())
		{
			const BuildingTypes eBuilding = pLoopCity->AI_bestAdvancedStartBuilding(iPass);
			if (eBuilding != NO_BUILDING)
			{
				bDoneBuildings = (iLastPointsTotal - (getAdvancedStartPoints() - getAdvancedStartBuildingCost(eBuilding, true, pLoopCity))) > iCityPoints;
				if (!bDoneBuildings)
				{
					doAdvancedStartAction(ADVANCEDSTARTACTION_BUILDING, pLoopCity->getX(), pLoopCity->getY(), eBuilding, true);
				}
			}
		}
	}

	//Units
	std::vector<UnitAITypes> aeUnitAITypes;
	aeUnitAITypes.push_back(UNITAI_CITY_DEFENSE);
	aeUnitAITypes.push_back(UNITAI_WORKER);
	aeUnitAITypes.push_back(UNITAI_RESERVE);
	aeUnitAITypes.push_back(UNITAI_COUNTER);

	for (int iPass = 0; iPass < 10; ++iPass)
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			if (iPass == 0 || pLoopCity->getArea() == getStartingPlot()->getArea())
			{
				const CvPlot* pUnitPlot = pLoopCity->plot();
				//Token defender
				const UnitTypes eBestUnit = AI_bestAdvancedStartUnitAI(pUnitPlot, aeUnitAITypes[iPass % aeUnitAITypes.size()]);
				if (eBestUnit != NO_UNIT)
				{
					if (getAdvancedStartUnitCost(eBestUnit, true, pUnitPlot) > getAdvancedStartPoints())
					{
						break;
					}
					doAdvancedStartAction(ADVANCEDSTARTACTION_UNIT, pUnitPlot->getX(), pUnitPlot->getY(), eBestUnit, true);
				}
			}
		}
	}

	if (isHumanPlayer())
	{
		// remove unhappy population
		foreach_(const CvCity * pLoopCity, cities())
		{
			while (pLoopCity->angryPopulation() > 0 && getAdvancedStartPopCost(false, pLoopCity) > 0)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pLoopCity->getX(), pLoopCity->getY(), -1, false);
			}
		}
	}

	if (!bNoExit)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_EXIT, -1, -1, -1, true);
	}
}


void CvPlayerAI::AI_recalculateFoundValues(int iX, int iY, int iInnerRadius, int iOuterRadius) const
{
	PROFILE_EXTRA_FUNC();
	for (int iLoopX = -iOuterRadius; iLoopX <= iOuterRadius; iLoopX++)
	{
		for (int iLoopY = -iOuterRadius; iLoopY <= iOuterRadius; iLoopY++)
		{
			CvPlot* pLoopPlot = plotXY(iX, iY, iLoopX, iLoopY);
			if (NULL != pLoopPlot && !AI_isPlotCitySite(pLoopPlot))
			{
				if (stepDistance(0, 0, iLoopX, iLoopY) <= iInnerRadius)
				{
					if (iLoopX != 0 || iLoopY != 0)
					{
						pLoopPlot->setFoundValue(getID(), 0);
					}
				}
				else if (pLoopPlot->isRevealed(getTeam(), false))
				{
					const int iValue = AI_foundValue(pLoopPlot->getX(), pLoopPlot->getY());

					pLoopPlot->setFoundValue(getID(), iValue);

					if (iValue > pLoopPlot->area()->getBestFoundValue(getID()))
					{
						pLoopPlot->area()->setBestFoundValue(getID(), iValue);
					}
				}
			}
		}
	}
}


int CvPlayerAI::AI_getMinFoundValue() const
{
	int iValue = 4000 / (1 + getProfitMargin() * getProfitMargin());

	if (GET_TEAM(getTeam()).hasWarPlan(true))
	{
		iValue *= 2;
	}
	return iValue;
}

// returns value between 1 to 10 based on how much potential land can be gained.
int CvPlayerAI::AI_getCitySitePriorityFactor(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iPriorityCount = 1;
	const int iX = pPlot->getX();
	const int iY = pPlot->getY();

	// Toffer - No reason to check more than pPlot and the 8 surrounding plots.
	for (int iI = 0; iI < NUM_CITY_PLOTS_1; iI++)
	{
		CvPlot* plotX = plotCity(iX, iY, iI);

		if (plotX && (plotX->getTeam() != getTeam() || !plotX->isPlayerCityRadius(getID())))
		{
			iPriorityCount++;
		}
	}
	return iPriorityCount;
}

void CvPlayerAI::AI_updateCitySites(int iMinFoundValueThreshold, int iMaxSites) const
{
	PROFILE_EXTRA_FUNC();
	
	for (int iI = 0; iI < iMaxSites; iI++)
	{
		//Add a city to the list.
		int iBestFoundValue = 0;
		CvPlot* pBestFoundPlot = NULL;

		for (int iPlot = 0; iPlot < GC.getMap().numPlots(); iPlot++)
		{
			CvPlot* plotX = GC.getMap().plotByIndex(iPlot);

			if (plotX->isRevealed(getTeam(), false) && !AI_isPlotCitySite(plotX))
			{
				const int iFoundValue = plotX->getFoundValue(getID());

				if (iFoundValue > iMinFoundValueThreshold)
				{
					const int iValue = iFoundValue * AI_getCitySitePriorityFactor(plotX);

					if (iValue > iBestFoundValue)
					{
						iBestFoundValue = iValue;
						pBestFoundPlot = plotX;
						
					}
				}
			}
		}
		if (pBestFoundPlot == NULL)
		{
			break;
		}
		
		m_aiAICitySites.push_back(GC.getMap().plotNum(pBestFoundPlot->getX(), pBestFoundPlot->getY()));
		AI_recalculateFoundValues(pBestFoundPlot->getX(), pBestFoundPlot->getY(), CITY_PLOTS_RADIUS, 2 * CITY_PLOTS_RADIUS);
	}
	
}

void CvPlayerAI::calculateCitySites() const
{
	static bool isCalulatingCitySites = false;

	//	Re-entrancy protection
	if (!isCalulatingCitySites)
	{
		isCalulatingCitySites = true;

		AI_updateFoundValues(false, NULL);

		m_bCitySitesNotCalculated = false;
		m_aiAICitySites.clear();
		AI_updateCitySites(AI_getMinFoundValue(), 4);

		isCalulatingCitySites = false;
	}
}

int CvPlayerAI::AI_getNumCitySites() const
{
	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	return m_aiAICitySites.size();
}

bool CvPlayerAI::AI_isPlotCitySite(const CvPlot* pPlot) const
{
	const int iPlotIndex = GC.getMap().plotNum(pPlot->getX(), pPlot->getY());

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	return algo::any_of_equal(m_aiAICitySites, iPlotIndex);
}

int CvPlayerAI::AI_getNumAreaCitySites(int iAreaID, int& iBestValue) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	iBestValue = 0;

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	foreach_(const int& i, m_aiAICitySites)
	{
		CvPlot* pCitySitePlot = GC.getMap().plotByIndex(i);
		if (pCitySitePlot->getArea() == iAreaID)
		{
			iCount++;
			const int iValue = pCitySitePlot->getFoundValue(getID());

			

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
			}
		}
	}
	
	return iCount;
}

int CvPlayerAI::AI_getNumAdjacentAreaCitySites(int iWaterAreaID, int iExcludeArea, int& iBestValue) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	iBestValue = 0;

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	foreach_(const int& i, m_aiAICitySites)
	{
		CvPlot* pCitySitePlot = GC.getMap().plotByIndex(i);
		if (pCitySitePlot->getArea() != iExcludeArea)
		{
			if (pCitySitePlot->isAdjacentToArea(iWaterAreaID))
			{
				iCount++;
				iBestValue = std::max(iBestValue, pCitySitePlot->getFoundValue(getID()));
			}
		}
	}
	return iCount;


}

CvPlot* CvPlayerAI::AI_getCitySite(int iIndex) const
{
	FASSERT_BOUNDS(0, (int)m_aiAICitySites.size(), iIndex);
	return GC.getMap().plotByIndex(m_aiAICitySites[iIndex]);
}

int CvPlayerAI::AI_bestAreaUnitAIValue(UnitAITypes eUnitAI, const CvArea* pArea, UnitTypes* peBestUnitType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_EXTRA_FUNC();
	CvCity* pCity = NULL;

	if (pArea != NULL)
	{
		if (getCapitalCity() != NULL)
		{
			if (pArea->isWater())
			{
				if (getCapitalCity()->plot()->isAdjacentToArea(pArea))
				{
					pCity = getCapitalCity();
				}
			}
			else
			{
				if (getCapitalCity()->getArea() == pArea->getID())
				{
					pCity = getCapitalCity();
				}
			}
		}

		if (NULL == pCity)
		{
			foreach_(CvCity * pLoopCity, cities())
			{
				if (pArea->isWater())
				{
					if (pLoopCity->plot()->isAdjacentToArea(pArea))
					{
						pCity = pLoopCity;
						break;
					}
				}
				else
				{
					if (pLoopCity->getArea() == pArea->getID())
					{
						pCity = pLoopCity;
						break;
					}
				}
			}
		}
	}

	return AI_bestCityUnitAIValue(eUnitAI, pCity, peBestUnitType, criteria);

}

int CvPlayerAI::AI_bestCityUnitAIValue(UnitAITypes eUnitAI, const CvCity* pCity, UnitTypes* peBestUnitType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAI);

	int iBestValue = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const UnitTypes eLoopUnit = (UnitTypes)iI;

		if (!isHumanPlayer() || (GC.getUnitInfo(eLoopUnit).getDefaultUnitAI() == eUnitAI))
		{
			const int iValue = AI_unitValue(eLoopUnit, eUnitAI, (pCity == NULL) ? NULL : pCity->area(), criteria);
			if (iValue > iBestValue)
			{
				if (NULL == pCity ? getUnitAvailabilityAnywhere(eLoopUnit) == EnablerDomain::STATE_LISTED : (pCity->getUnitAvailability(eLoopUnit) == EnablerDomain::STATE_LISTED))
				{
					iBestValue = iValue;
					if (peBestUnitType != NULL)
					{
						*peBestUnitType = eLoopUnit;
					}
				}
			}
		}
	}

	return iBestValue;
}

int CvPlayerAI::AI_calculateTotalBombard(DomainTypes eDomain) const
{
	PROFILE_EXTRA_FUNC();
	int iTotalBombard = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

		if (kUnit.getDomain() == eDomain)
		{
			// ⚠ The two bombard kinds carry DIFFERENT units within the one family ([fixed-point-and-scales]:
			// ask the KIND's unit, never the family's). `rate` is a PERCENT and is unscaled; `airBombRate` is a
			// FLAT and is ×100, so it reduces here to join the other on one scale -- which leaves the sum below
			// untouched. Reading `rate` off the flat side instead would answer 0 for every unit that authors it.
			const int iBombRate = kUnit.getFlatBombard(BOMBARD_AIR_BOMB_RATE, CASC_SCOPE_UNIT) / 100;
			// #410: breakdown is not bombard -- war planning must not count phantom
			// siege capability that can only be cashed by dying in an assault.
			int iBombardRate = kUnit.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);

			if (iBombardRate > 0 || iBombRate > 0)
			{
				int iNumUnits = getUnitCount((UnitTypes)iI);
				if (iBombardRate > 0)
				{
					if (kUnit.hasSkill(CLS_SKILL_IGNORE_BUILDING_DEFENSE))
					{
						iBombardRate *= 3;
						iBombardRate /= 2;
					}
					iTotalBombard += iBombardRate * iNumUnits;
				}
				if (iBombRate > 0)
				{
					iTotalBombard += iBombRate * iNumUnits;
				}
			}
		}
	}
	return iTotalBombard;
}

void CvPlayerAI::AI_updateBonusValue(BonusTypes eBonus)
{
	FAssert(m_aiBonusValue != NULL);

	//reset
	m_aiBonusValue[eBonus] = -1;
	m_abNonTradeBonusCalculated[eBonus] = false;
}


void CvPlayerAI::AI_updateBonusValue()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		AI_updateBonusValue((BonusTypes)iI);
	}
}

int CvPlayerAI::AI_getUnitWeight(UnitTypes eUnit) const
{
	return m_aiUnitWeights[eUnit] / 100;
}

int CvPlayerAI::AI_getUnitCombatWeight(UnitCombatTypes eUnitCombat) const
{
	return m_aiUnitCombatWeights[eUnitCombat] / 100;
}

void CvPlayerAI::AI_doEnemyUnitData()
{
	PROFILE_FUNC();

	std::vector<int> aiUnitCounts(GC.getNumUnitInfos(), 0);

	std::vector<int> aiDomainSums(NUM_DOMAIN_TYPES, 0);

	int iI;

	int iNewTotal = 0;

	// Count enemy land and sea units visible to us
	for (iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		const CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
		int iAdjacentAttackers = -1;
		if (pLoopPlot->isVisible(getTeam(), false))
		{
			foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
			{
				if (pLoopUnit->canFight())
				{
					int iUnitValue = 1;

					if (GET_TEAM(getTeam()).AI_getWarPlan(pLoopUnit->getTeam()) != NO_WARPLAN)
					{
						iUnitValue += 10;

						if ((pLoopPlot->getOwner() == getID()))
						{
							iUnitValue += 15;
						}
						else if (atWar(getTeam(), pLoopPlot->getTeam()))
						{
							if (iAdjacentAttackers == -1)
							{
								iAdjacentAttackers = GET_PLAYER(pLoopPlot->getOwner()).AI_adjacentPotentialAttackers(pLoopPlot);
							}
							if (iAdjacentAttackers > 0)
							{
								iUnitValue += 15;
							}
						}
					}

					else if (pLoopUnit->getTeam() != getTeam())
					{
						iUnitValue += pLoopUnit->canAttack() ? 4 : 1;
						if (pLoopPlot->getCulture(getID()) > 0)
						{
							iUnitValue += pLoopUnit->canAttack() ? 4 : 1;
						}
					}

					// If we hadn't seen any of this class before
					if (m_aiUnitWeights[pLoopUnit->getUnitType()] == 0)
					{
						iUnitValue *= 4;
					}

					iUnitValue *= pLoopUnit->baseCombatStrHuman();
					aiUnitCounts[pLoopUnit->getUnitType()] += iUnitValue;
					aiDomainSums[pLoopUnit->getDomainType()] += iUnitValue;
					iNewTotal += iUnitValue;
				}
			}
		}
	}

	if (iNewTotal == 0)
	{
		//This should rarely happen.
		return;
	}

	//Decay
	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		m_aiUnitWeights[iI] -= 100;
		m_aiUnitWeights[iI] *= 3;
		m_aiUnitWeights[iI] /= 4;
		m_aiUnitWeights[iI] = std::max(0, m_aiUnitWeights[iI]);
	}

	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (aiUnitCounts[iI] > 0)
		{
			// The unit's ERA is derived at load from its first prereq-tech atom (CvUnitInfo::deriveAtRegistryComplete),
			// so the era question is asked of the unit directly instead of hopping through a prereq getter to
			// reach the tech's era -- one read where there were two, and the hop's getter no longer exists.
			const int iUnitEra = GC.getUnitInfo((UnitTypes)iI).getEra();
			int iEraDiff = (iUnitEra == NO_ERA) ? 4 : std::min(4, getCurrentEra() - iUnitEra);

			if (iEraDiff > 1)
			{
				iEraDiff -= 1;
				aiUnitCounts[iI] *= 3 - iEraDiff;
				aiUnitCounts[iI] /= 3;
			}
			// The domain as a VALUE, to index the per-domain sums accumulated above. ⚠ Guarded rather than
			// indexed blind: a unit whose tags block has not been filled answers NO_DOMAIN, and subscripting
			// with that would read off the front of the array -- the honest failure is to skip it.
			const DomainTypes eUnitDomain = GC.getUnitInfo((UnitTypes)iI).getDomain();

			if (eUnitDomain != NO_DOMAIN)
			{
				FAssert(aiDomainSums[eUnitDomain] > 0);
				m_aiUnitWeights[iI] += (5000 * aiUnitCounts[iI]) / std::max(1, aiDomainSums[eUnitDomain]);
			}
		}
	}

	for (iI = 0; iI < GC.getNumUnitCombatInfos(); ++iI)
	{
		m_aiUnitCombatWeights[iI] = 0;
	}

	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (m_aiUnitWeights[iI] > 0)
		{
			int ctype = GC.getUnitInfo((UnitTypes)iI).getCombatClass();
			if (ctype >= 0 && ctype < GC.getNumUnitCombatInfos())
			{
				m_aiUnitCombatWeights[ctype] += m_aiUnitWeights[iI];
			}
		}
	}

	for (iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (m_aiUnitCombatWeights[iI] > 2500)
		{
			m_aiUnitCombatWeights[iI] += 2500;
		}
		else if (m_aiUnitCombatWeights[iI] > 0)
		{
			m_aiUnitCombatWeights[iI] += 1000;
		}
	}
}

int CvPlayerAI::AI_calculateUnitAIViability(UnitAITypes eUnitAI, DomainTypes eDomain) const
{
	PROFILE_EXTRA_FUNC();
	int iBestUnitAIStrength = 0;
	int iBestOtherStrength = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnitInfo = GC.getUnitInfo((UnitTypes)iI);

		// The DOMAIN is a tag now ([tags.md]: DOMAIN_* stays the engine enum movement and stacking are wired to,
		// while the classification view answers "what IS this unit"). ⚑
		if (kUnitInfo.getDomain() == eDomain)
		{
			// "Do we hold what unlocks it" is the enabler's membership verdict, not a prereq-tech lookup: a unit
			// is in CAN GET exactly when something held enables it, which is the question the old isHasTech
			// asked one edge at a time.
			if (m_aiUnitWeights[iI] > 0
			|| getUnitAvailabilityAnywhere((UnitTypes)iI) >= EnablerDomain::STATE_GREYED)
			{
				if (kUnitInfo.hasUnitAI(eUnitAI))
				{
					iBestUnitAIStrength = std::max(iBestUnitAIStrength, (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100));
				}
				iBestOtherStrength = std::max(iBestOtherStrength, (kUnitInfo.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100));
			}
		}
	}
	return (100 * iBestUnitAIStrength) / std::max(1, iBestOtherStrength);
}

ReligionTypes CvPlayerAI::AI_chooseReligion()
{
	PROFILE_EXTRA_FUNC();
	const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
	if (NO_RELIGION != eFavorite && !GC.getGame().isReligionFounded(eFavorite))
	{
		return eFavorite;
	}

	std::vector<ReligionTypes> aeReligions;
	for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
	{
		if (!GC.getGame().isReligionFounded((ReligionTypes)iReligion))
		{
			aeReligions.push_back((ReligionTypes)iReligion);
		}
	}

	if (!aeReligions.empty())
	{
		return aeReligions[GC.getGame().getSorenRandNum(aeReligions.size(), "AI pick religion")];
	}

	return NO_RELIGION;
}

int CvPlayerAI::AI_getAttitudeWeight(PlayerTypes ePlayer) const
{
	int iAttitudeWeight = 0;
	switch (AI_getAttitude(ePlayer))
	{
	case ATTITUDE_FURIOUS:
		iAttitudeWeight = -100;
		break;
	case ATTITUDE_ANNOYED:
		iAttitudeWeight = -50;
		break;
	case ATTITUDE_CAUTIOUS:
		iAttitudeWeight = 0;
		break;
	case ATTITUDE_PLEASED:
		iAttitudeWeight = 50;
		break;
	case ATTITUDE_FRIENDLY:
		iAttitudeWeight = 100;
		break;
	}

	return iAttitudeWeight;
}

int CvPlayerAI::AI_getPlotAirbaseValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	if (pPlot->isOwned() && (pPlot->getTeam() != getTeam()))
	{
		return 0;
	}

	if (pPlot->isCityRadius())
	{
		CvCity* pWorkingCity = pPlot->getWorkingCity();
		if (pWorkingCity != NULL)
		{
			if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
			{
				return 0;
			}
			if (pPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				if (!GC.getImprovementInfo(pPlot->getImprovementType()).hasCharacteristic(CLS_CHARACTERISTIC_ACTS_AS_CITY))
				{
					return 0;
				}
			}
		}
	}

	int iMinOtherCityDistance = MAX_INT;
	const CvPlot* iMinOtherCityPlot = NULL;

	// Super Forts begin *choke* *canal* - commenting out unnecessary code
	//	int iMinFriendlyCityDistance = MAX_INT;
	//	CvPlot* iMinFriendlyCityPlot = NULL;

	int iOtherCityCount = 0;

	int iRange = 4;
	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pPlot != pLoopPlot)
		{
			const int iDistance = plotDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());

			if (pLoopPlot->getTeam() == getTeam())
			{
				if (pLoopPlot->isCity(true))
				{
					if (1 == iDistance)
					{
						return 0;
					}
					//					if (iDistance < iMinFriendlyCityDistance)
					//					{
					//						iMinFriendlyCityDistance = iDistance;
					//						iMinFriendlyCityPlot = pLoopPlot;
					//					}
					// Super Forts end
				}
			}
			else if (pLoopPlot->isOwned() && pLoopPlot->isCity(false))
			{
				if (iDistance < iMinOtherCityDistance)
				{
					iMinOtherCityDistance = iDistance;
					iMinOtherCityPlot = pLoopPlot;
					// Super Forts begin  *choke* *canal* - move iOtherCityCount outside the if statement
				}
				iOtherCityCount++;
				// Super Forts end
			}
		}
	}

	if (0 == iOtherCityCount)
	{
		return 0;
	}

	//	if (iMinFriendlyCityPlot != NULL)
	//	{
	//		FAssert(iMinOtherCityPlot != NULL);
	//		if (plotDistance(iMinFriendlyCityPlot->getX(), iMinFriendlyCityPlot->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < (iMinOtherCityDistance - 1))
	//		{
	//			return 0;
	//		}
	//	}

	//	if (iMinOtherCityPlot != NULL)
	//	{
	//		CvCity* pNearestCity = GC.getMap().findCity(iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY(), NO_PLAYER, getTeam(), false);
	//		if (NULL == pNearestCity)
	//		{
	//			return 0;
	//		}
	//		if (plotDistance(pNearestCity->getX(), pNearestCity->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < iRange)
	//		{
	//			return 0;
	//		}
	//	}


		// Super Forts begin *canal* *choke*
	if (iOtherCityCount == 1)
	{
		if (iMinOtherCityPlot)
		{
			CvCity* pNearestCity = GC.getMap().findCity(iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY(), NO_PLAYER, getTeam(), false);
			if (NULL != pNearestCity)
			{
				if (plotDistance(pNearestCity->getX(), pNearestCity->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < iMinOtherCityDistance)
				{
					return 0;
				}
			}
		}
	}
	const int iValue = iOtherCityCount * 50 + pPlot->defenseModifier(getTeam(), false);
	// Super Forts end

	return std::max(0, iValue);
}

int CvPlayerAI::AI_getPlotCanalValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	// Super Forts begin *canal*
	int iCanalValue = pPlot->getCanalValue();

	if (iCanalValue > 0)
	{
		if (pPlot->isOwned())
		{
			if (pPlot->getTeam() != getTeam())
			{
				return 0;
			}
			if (pPlot->isCityRadius())
			{
				CvCity* pWorkingCity = pPlot->getWorkingCity();
				if (pWorkingCity != NULL)
				{
					// Left in this part from the original code. Might be needed to avoid workers from getting stuck in a loop?
					if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
					{
						return 0;
					}
					if (pPlot->getImprovementType() != NO_IMPROVEMENT)
					{
						if (!GC.getImprovementInfo(pPlot->getImprovementType()).hasCharacteristic(CLS_CHARACTERISTIC_ACTS_AS_CITY))
						{
							return 0;
						}
					}
					// Decrease value when within radius of a city
					iCanalValue -= 5;
				}
			}
		}

		foreach_(const CvPlot * pLoopPlot, pPlot->adjacent())
		{
			if (pLoopPlot->isCity(true) && (pLoopPlot->getCanalValue() > 0))
			{
				// Decrease value when adjacent to a city or fort with a canal value
				iCanalValue -= 10;
			}
		}

		iCanalValue *= 10;
		// Favor plots with higher defense
		int iDefenseModifier = pPlot->defenseModifier(getTeam(), false);
		iCanalValue += iDefenseModifier;
	}

	return std::max(0, iCanalValue);
	// Super Forts end
}

// Super Forts begin *choke*
int CvPlayerAI::AI_getPlotChokeValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	int iChokeValue = pPlot->getChokeValue();

	if (iChokeValue > 0)
	{
		if (pPlot->isOwned())
		{
			if (pPlot->getTeam() != getTeam())
			{
				return 0;
			}
			if (pPlot->isCityRadius())
			{
				CvCity* pWorkingCity = pPlot->getWorkingCity();
				if (pWorkingCity != NULL)
				{
					// Left in this part from the original code. Might be needed to avoid workers from getting stuck in a loop?
					if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
					{
						return 0;
					}
					if (pPlot->getImprovementType() != NO_IMPROVEMENT)
					{
						if (!GC.getImprovementInfo(pPlot->getImprovementType()).isMilitaryStructure())
						{
							return 0;
						}
					}
					// Decrease value when within radius of a city
					iChokeValue -= 5;
				}
			}
		}

		foreach_(const CvPlot * pLoopPlot, pPlot->adjacent())
		{
			if (pLoopPlot->isCity(true) && (pLoopPlot->getChokeValue() > 0))
			{
				// Decrease value when adjacent to a city or fort with a choke value
				iChokeValue -= 10;
			}
		}

		iChokeValue *= 10;
		// Favor plots with higher defense
		int iDefenseModifier = pPlot->defenseModifier(getTeam(), false);
		iChokeValue += iDefenseModifier;
	}

	return std::max(0, iChokeValue);
}
// Super Forts end

bool CvPlayerAI::AI_isCivicCanChangeOtherValues(CivicTypes eCivicSelected, ReligionTypes eAssumedReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (eCivicSelected == NO_CIVIC)
	{
		return false;
	}

	const CvCivicInfo& kCivicSelected = GC.getCivicInfo(eCivicSelected);

	//happiness
	if (InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, +1)
	|| InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, -1)
	|| (kCivicSelected.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) != 0
		&& (kCivicSelected.providesPolicy(CLS_POLICY_STATE_RELIGION) || eAssumedReligion != NO_RELIGION))
	|| (kCivicSelected.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) != 0 && getWarWearinessPercentAnger() != 0))
	{
		return true;
	}

	//health
	if (kCivicSelected.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) != 0 || kCivicSelected.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION) || kCivicSelected.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS) || InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HEALTH, +1))
	{
		return true;
	}

	//trade
	if (kCivicSelected.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE))
	{
		return true;
	}

	//corporation
	if (kCivicSelected.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || kCivicSelected.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) || kCivicSelected.getMaintenanceModifier(MAINTENANCE_CORPORATION, CASC_SCOPE_EMPIRE) != 0)
	{
		return true;
	}

	//religion
	if (kCivicSelected.providesPolicy(CLS_POLICY_STATE_RELIGION))
	{
		return true;
	}
	if (kCivicSelected.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD))
	{
		return true;
	}

	//other
	if (kCivicSelected.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION))
	{
		return true;
	}

	int iI;
	for (iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (cvEdgesHas(kCivicSelected.getEdges(), EDGEF_ENABLES, EDGEB_HURRIES, (int)iI))
		{
			return true;
		}
	}
	for (iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (cvEdgesHas(kCivicSelected.getEdges(), EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED, (int)iI))
		{
			return true;
		}
	}

	return false;
}

bool CvPlayerAI::AI_isCivicValueRecalculationRequired(CivicTypes eCivic, CivicTypes eCivicSelected, ReligionTypes eAssumedReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (eCivicSelected == NO_CIVIC || eCivic == NO_CIVIC)
	{
		return false;
	}

	const CvCivicInfo& kCivic = GC.getCivicInfo(eCivic);
	const CvCivicInfo& kCivicSelected = GC.getCivicInfo(eCivicSelected);

	//happiness
	if (InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, -1) || InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1)
	|| InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1) || 0 != 0
	|| InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1) || InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HAPPINESS, +1)
	|| (kCivic.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) != 0 && getWarWearinessPercentAnger() != 0)
	|| (kCivic.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) != 0 && (kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION) || eAssumedReligion != NO_RELIGION)))
	{
		if (InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, -1) || InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, +1)
		|| InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, +1) || 0 != 0
		|| InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, +1) || InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HAPPINESS, +1)
		|| (kCivicSelected.getDiplomacy(DIPLOMACY_WAR_WEARINESS, CASC_SCOPE_EMPIRE) != 0 && getWarWearinessPercentAnger() != 0)
		|| (kCivicSelected.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) != 0 && (kCivicSelected.providesPolicy(CLS_POLICY_STATE_RELIGION) || eAssumedReligion != NO_RELIGION)))
		{
			return true;
		}
	}

	//health
	if (kCivic.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) != 0 || kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION) || kCivic.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS) || InfoValuation::authorsAnySigned(kCivic.getModifiers(), MODFAM_HEALTH, +1))
	{
		if (kCivicSelected.getFlatWellbeing(WELLBEING_HEALTH, CASC_SCOPE_EMPIRE) != 0 || kCivicSelected.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_POPULATION) || kCivicSelected.providesAmenity(CLS_AMENITY_ABOLISHED_UNHEALTH_FROM_BUILDINGS) || InfoValuation::authorsAnySigned(kCivicSelected.getModifiers(), MODFAM_HEALTH, +1))
		{
			return true;
		}
	}

	//trade
	if (kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE))
	{
		if (kCivicSelected.providesPolicy(CLS_POLICY_NO_FOREIGN_TRADE))
		{
			return true;
		}
	}

	//corporation
	if (kCivic.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || kCivic.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) || kCivic.getMaintenanceModifier(MAINTENANCE_CORPORATION, CASC_SCOPE_EMPIRE) != 0)
	{
		if (kCivicSelected.providesPolicy(CLS_POLICY_NO_CORPORATIONS) || kCivicSelected.providesPolicy(CLS_POLICY_NO_FOREIGN_CORPORATIONS) || kCivicSelected.getMaintenanceModifier(MAINTENANCE_CORPORATION, CASC_SCOPE_EMPIRE) != 0)
		{
			return true;
		}
	}

	//religion
	if (kCivic.providesPolicy(CLS_POLICY_STATE_RELIGION))
	{
		if (kCivicSelected.providesPolicy(CLS_POLICY_STATE_RELIGION))
		{
			return true;
		}
		if (kCivic.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD) && kCivicSelected.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD))
		{
			return true;
		}
	}
	else
	{
		if (kCivicSelected.providesPolicy(CLS_POLICY_STATE_RELIGION))
		{
			if (kCivic.providesPolicy(CLS_POLICY_NO_NON_STATE_RELIGION_SPREAD))
			{
				return true;
			}
			if (getStateReligionCount() > 1)
			{
				// "Does this civic treat the STATE religion specially?" -- each leg is one of the civic's own
				// `stateReligion.empire.<kind>` deposits. ⚠ The old happiness leg compared against a NON-state
				// happiness read that no longer exists and that no civic ever authored (5 traits author
				// `nonStateReligion`, zero civics), so it compared against a constant 0; the leg is the plain
				// non-zero test it always effectively was.
				if (kCivic.getStateReligion(STATE_RELIGION_HAPPINESS, CASC_SCOPE_EMPIRE) != 0
					|| kCivic.getStateReligion(STATE_RELIGION_GREAT_PEOPLE_RATE, CASC_SCOPE_EMPIRE) != 0
					|| kCivic.getStateReligion(STATE_RELIGION_UNIT_PRODUCTION, CASC_SCOPE_EMPIRE) != 0
					|| kCivic.getStateReligion(STATE_RELIGION_BUILDING_PRODUCTION, CASC_SCOPE_EMPIRE) != 0
					|| kCivic.getStateReligion(STATE_RELIGION_FREE_EXPERIENCE, CASC_SCOPE_EMPIRE) != 0)
				{
					return true;
				}
			}
		}
	}

	//other kCivicSelected
	if (kCivic.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION) && kCivicSelected.providesPolicy(CLS_POLICY_MILITARY_FOOD_PRODUCTION))
	{
		return true;
	}

	int iI;
	for (iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (cvEdgesHas(kCivic.getEdges(), EDGEF_ENABLES, EDGEB_HURRIES, (int)iI) && cvEdgesHas(kCivicSelected.getEdges(), EDGEF_ENABLES, EDGEB_HURRIES, (int)iI))
		{
			return true;
		}
	}
	for (iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (cvEdgesHas(kCivic.getEdges(), EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED, (int)iI) && cvEdgesHas(kCivicSelected.getEdges(), EDGEF_ENABLES, EDGEB_SPECIAL_BUILDINGS_WAIVED, (int)iI))
		{
			return true;
		}
	}

	return false;
}

//This returns a positive number equal approximately to the sum
//of the percentage values of each unit (there is no need to scale the output by iHappy)
//100 * iHappy means a high value.
int CvPlayerAI::AI_getHappinessWeight(int iHappy, int iExtraPop) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	if (0 == iHappy)
	{
		iHappy = 1;
	}
	int iValue = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		// The slider-scaled share of this city's happiness is already INSIDE netHappiness and no read isolates
		// it, so it is weighed here like any other happiness rather than discounted.
		const int iCityHappy = pLoopCity->netHappiness(iExtraPop);

		//Fuyu: max happy 5
		const int iHappyNow = std::min(5, iCityHappy);
		const int iHappyThen = std::min(5, iCityHappy + iHappy);

		//Integration
		int iTempValue = ((100 * iHappyThen - 10 * iHappyThen * iHappyThen) - (100 * iHappyNow - 10 * iHappyNow * iHappyNow));

		if (pLoopCity->isCapital() && isNoCapitalUnhappiness())
		{
			iTempValue /= 3;
		}

		if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
		{
			iTempValue *= 3;
			iTempValue /= 2;
		}

		if (iHappy > 0)
		{
			iValue += std::max(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}
		else
		{
			iValue += std::min(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}

		iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
	}
	return (0 == iCount) ? 50 * iHappy : iValue / iCount;
}

int CvPlayerAI::AI_getHealthWeight(int iHealth, int iExtraPop) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	if (0 == iHealth)
	{
		iHealth = 1;
	}
	int iValue = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		int iCityHealth = pLoopCity->netHealth(iExtraPop);

		//Fuyu: max health 8
		int iHealthNow = std::min(8, iCityHealth);
		int iHealthThen = std::min(8, iCityHealth + iHealth);

		int iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
		if (iHealth > 0)
		{
			iValue += std::max(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}
		else
		{
			iValue += std::min(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}

		iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
	}

	return (0 == iCount) ? 50 * iHealth : iValue / iCount;
}

void CvPlayerAI::AI_invalidateCloseBordersAttitudeCache()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		m_aiCloseBordersAttitudeCache[i] = MAX_INT;

		AI_invalidateAttitudeCache((PlayerTypes)i);
	}
}

bool CvPlayerAI::AI_isPlotThreatened(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	const CvArea* pPlotArea = pPlot->area();

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange)
	| filtered(CvPlot::fn::area() == pPlotArea))
	{
		CvSelectionGroup* pLoopGroup = NULL;

		do
		{
			CvSelectionGroup* pNextGroup = NULL;

			foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
			{
				if (pLoopUnit->isEnemy(getTeam()) && pLoopUnit->canAttack() && !pLoopUnit->isInvisible(getTeam(), false))
				{
					if (pLoopGroup == NULL ||
						 pLoopUnit->getOwner() > pLoopGroup->getOwner() ||
						 (pLoopUnit->getOwner() == pLoopGroup->getOwner() && pLoopUnit->getGroupID() > pLoopGroup->getID()))
					{
						if (pNextGroup == NULL ||
							 pLoopUnit->getOwner() < pNextGroup->getOwner() ||
							 (pLoopUnit->getOwner() == pNextGroup->getOwner() && pLoopUnit->getGroupID() < pNextGroup->getID()))
						{
							pNextGroup = pLoopUnit->getGroup();
						}
					}
				}
			}

			pLoopGroup = pNextGroup;

			if (pLoopGroup != NULL && pLoopGroup->canEnterOrAttackPlot(pPlot))
			{
				int iPathTurns = 0;
				if (bTestMoves)
				{
					iPathTurns = pLoopGroup->canPathDirectlyTo(pLoopPlot, pPlot) ? 1 : MAX_INT;
					//if (!pLoopUnit->getGroup()->generatePath(pLoopPlot, pPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER, false, &iPathTurns))
					//{
					//	iPathTurns = MAX_INT;
					//}
				}

				if (iPathTurns <= 1)
				{
					return true;
				}
			}
		} while (pLoopGroup != NULL);
	}

	return false;
}

bool CvPlayerAI::AI_isFirstTech(TechTypes eTech) const
{
	PROFILE_EXTRA_FUNC();
	std::set<int> unlockedReligions;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_TECHS, (int)eTech), EDGEF_ENABLES, EDGEB_RELIGIONS, unlockedReligions);

	for (std::set<int>::const_iterator itUnlockedReligion = unlockedReligions.begin(); itUnlockedReligion != unlockedReligions.end(); ++itUnlockedReligion)
	{
		const ReligionTypes eLoopReligion = static_cast<ReligionTypes>(*itUnlockedReligion);

		if (!GC.getGame().isReligionSlotTaken(eLoopReligion) && canFoundReligion())
		{
			return true;
		}
	}
	if (GC.getGame().countKnownTechNumTeams(eTech) == 0
	&& (GC.getTechInfo(eTech).getTriggers() != NULL
		&& GC.getTechInfo(eTech).getTriggers()->consideredGrant() != NULL
		&& (GC.getTechInfo(eTech).getTriggers()->consideredGrant()->firstListId("firstFreeUnit") != -1
			|| GC.getTechInfo(eTech).getTriggers()->consideredGrant()->pulse("freeTechs") > 0)))
	{
		return true;
	}
	return false;
}

void CvPlayerAI::AI_invalidateAttitudeCache(PlayerTypes ePlayer)
{
	m_aiAttitudeCache[ePlayer] = MAX_INT;
}

void CvPlayerAI::AI_invalidateAttitudeCache()
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		AI_invalidateAttitudeCache((PlayerTypes)iI);
	}
}

void CvPlayerAI::AI_changeAttitudeCache(const PlayerTypes ePlayer, const int iChange)
{
	if (m_aiAttitudeCache[ePlayer] == 100 && iChange < 0
	|| m_aiAttitudeCache[ePlayer] == -100 && iChange > 0)
	{
		// Toffer - Why don't we allow the total value to exceed +/- 100 ?
		AI_invalidateAttitudeCache(ePlayer);
	}
	else m_aiAttitudeCache[ePlayer] = range(m_aiAttitudeCache[ePlayer] + iChange, -100, 100);
}

CvCity* CvPlayerAI::getInquisitionRevoltCity(const CvUnit* pUnit, const bool bNoUnit, int iRevIndexThreshold, const int iTrendThreshold) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);
	if (!(hasInquisitionTarget()))
	{
		return NULL;
	}

	CvCity* pBestCity = NULL;
	const CvPlot* pUnitPlot = pUnit->plot();
	int iBestRevoltIndex = 100;

	foreach_(CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isInquisitionConditions())
		{
			if ((pLoopCity->getRevTrend() > iTrendThreshold)
			|| (pLoopCity->getRevolutionIndex() > 1000))
			{
				int iTempCityValue = pLoopCity->getRevolutionIndex() + 7 * (pLoopCity->getRevTrend());
				iTempCityValue -= 10 * (pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopCity->plot()));
				if (iTempCityValue > iBestRevoltIndex)
				{
					if ((bNoUnit) || (pUnit->generatePath(pLoopCity->plot(), 0, false)))
					{
						iBestRevoltIndex = iTempCityValue;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}

	return pBestCity;
}

CvCity* CvPlayerAI::getTeamInquisitionRevoltCity(const CvUnit* pUnit, const bool bNoUnit, int iRevIndexThreshold, const int iTrendThreshold) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);
	if (!(hasInquisitionTarget()))
	{
		return NULL;
	}

	CvCity* pBestCity = NULL;
	const CvPlot* pUnitPlot = pUnit->plot();
	int iBestRevoltIndex = 100;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		if (kLoopPlayer.isAlive())
		{
			if ((kLoopPlayer.getTeam() == getTeam()) ||
			(GET_TEAM(getTeam()).isVassal((TeamTypes)kLoopPlayer.getTeam())))
			{
				foreach_(CvCity * pLoopCity, kLoopPlayer.cities())
				{
					if (pLoopCity->isInquisitionConditions())
					{
						if ((pLoopCity->getRevTrend() > iTrendThreshold)
						|| (pLoopCity->getRevolutionIndex() > 1000))
						{
							int iTempCityValue = pLoopCity->getRevolutionIndex() + 7 * (pLoopCity->getRevTrend());
							iTempCityValue -= 10 * (pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopCity->plot()));
							if (iTempCityValue > iBestRevoltIndex)
							{
								if ((bNoUnit) || (pUnit->generatePath(pLoopCity->plot(), 0, false)))
								{
									iBestRevoltIndex = iTempCityValue;
									pBestCity = pLoopCity;
								}
							}
						}
					}
				}
			}
		}
	}

	return pBestCity;
}

CvCity* CvPlayerAI::getReligiousVictoryTarget(const CvUnit* pUnit, const bool bNoUnit) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);

	if (!hasInquisitionTarget() || (!isPushReligiousVictory() && !isConsiderReligiousVictory()))
	{
		return NULL;
	}

	const CvPlot* pUnitPlot = pUnit->plot();
	const ReligionTypes eStateReligion = getStateReligion();

	CvCity* pBestCity = NULL;
	int iBestCityValue = MAX_INT;
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		const CvTeam& kLoopTeam = GET_TEAM(kLoopPlayer.getTeam());

		if (kLoopPlayer.isAlive()
			&& (TeamTypes(kLoopPlayer.getTeam()) == getTeam() || kLoopTeam.isVassal(getTeam()))
			&& pUnitPlot->isHasPathToPlayerCity(getTeam(), PlayerTypes(iI))
			)
		{
			foreach_(CvCity * pLoopCity, kLoopPlayer.cities())
			{
				CvPlot* pLoopPlot = pLoopCity->plot();
				if (pLoopCity->isInquisitionConditions()
					&& (bNoUnit || pUnit->generatePath(pLoopPlot, 0, false))
					)
				{
					int tempCityValue = pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopPlot);
					if (isNonStateReligionCommerce()
						&& kLoopPlayer.getID() == getID())
					{
						tempCityValue *= 2;
					}
					if (kLoopTeam.isVassal(getTeam()))
					{
						tempCityValue -= 12;
					}
					for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
					{
						const ReligionTypes religionType = static_cast<ReligionTypes>(iJ);
						if (religionType != eStateReligion
							&& hasHolyCity(religionType)
							&& pLoopCity->isHasReligion(religionType))
						{
							tempCityValue += 13;
						}
					}
					if (tempCityValue < iBestCityValue)
					{
						pBestCity = pLoopCity;
						iBestCityValue = tempCityValue;
					}
				}
			}
		}
	}

	return pBestCity;
}


bool CvPlayerAI::isPushReligiousVictory() const
{
	return m_bPushReligiousVictory;
}

void CvPlayerAI::AI_setPushReligiousVictory()
{
	PROFILE_FUNC();

	m_bPushReligiousVictory = false;
	const ReligionTypes eStateReligion = getStateReligion();

	if (eStateReligion == NO_RELIGION
	|| !hasHolyCity(eStateReligion)
	|| AI_getCultureVictoryStage() > 1)
	{
		return;
	}

	// Better way to determine if religious victory is valid?
	int iVictoryTarget;
	bool bValid = false;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.conditionValue(VICTORY_CONDITION_RELIGION_PERCENT) > 0)
			{
				iVictoryTarget = kVictoryInfo.conditionValue(VICTORY_CONDITION_RELIGION_PERCENT);
				bValid = true;
				break;
			}
		}
	}
	if (!bValid)
	{
		m_bPushReligiousVictory = false;
		return;
	}
	const int iStateReligionInfluence = GC.getGame().calculateReligionPercent(eStateReligion);

	if (iStateReligionInfluence > (3 * iVictoryTarget) / 4)
	{
		m_bPushReligiousVictory = true;
		return;
	}

	bool bStateReligionBest = true;
	for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
	{
		if (eStateReligion != (ReligionTypes)iJ)
		{
			if (GC.getGame().calculateReligionPercent((ReligionTypes)iJ) > iStateReligionInfluence)
			{
				bStateReligionBest = false;
				break;
			}
		}
	}

	int iPercentThreshold = iVictoryTarget * 2/3;

	if (eStateReligion != NO_RELIGION)
	{
		const CvCity* holyCity = GC.getGame().getHolyCity(eStateReligion);

		if (holyCity && holyCity->getOwner() == getID())
		{
			foreach_(const BuildingTypes eTypeX, holyCity->getHasBuildings())
			{
				if (GC.getBuildingInfo(eTypeX).getShrineReligion() == eStateReligion
				&& !holyCity->isDormantBuilding(eTypeX))
				{
					iPercentThreshold /= 2;
					break;
				}
			}
		}
	}

	if (bStateReligionBest
	&& (iStateReligionInfluence > iPercentThreshold || GET_TEAM(getTeam()).getTotalLand(true) > 50))
	{
		m_bPushReligiousVictory = true;
	}
}


bool CvPlayerAI::isConsiderReligiousVictory() const
{
	return m_bConsiderReligiousVictory;
}

void CvPlayerAI::AI_setConsiderReligiousVictory()
{
	PROFILE_FUNC();

	if (isPushReligiousVictory())
	{
		m_bConsiderReligiousVictory = true;
		return;
	}

	m_bConsiderReligiousVictory = false;
	if (getStateReligion() == NO_RELIGION)
	{
		return;
	}
	if (AI_getCultureVictoryStage() > 1)
	{
		return;
	}

	ReligionTypes eStateReligion = getStateReligion();
	int iStateReligionInfluence = GC.getGame().calculateReligionPercent(eStateReligion);
	int iI;
	int iVictoryTarget;

	if (!hasHolyCity(eStateReligion))
	{
		return;
	}
	// Better way to determine if religious victory is valid?
	bool bValid = false;
	for (iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.conditionValue(VICTORY_CONDITION_RELIGION_PERCENT) > 0)
			{
				iVictoryTarget = kVictoryInfo.conditionValue(VICTORY_CONDITION_RELIGION_PERCENT);
				bValid = true;
				break;
			}
		}
	}
	if (!bValid)
	{
		return;
	}

	if (iStateReligionInfluence > ((2 * iVictoryTarget) / 3))
	{
		m_bConsiderReligiousVictory = true;
		return;
	}

	bool eStateReligionBest = true;
	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (eStateReligion != (ReligionTypes)iI)
		{
			if (GC.getGame().calculateReligionPercent((ReligionTypes)iI) > iStateReligionInfluence)
			{
				eStateReligionBest = false;
				break;
			}
		}
	}
	if (eStateReligionBest)
	{
		m_bConsiderReligiousVictory = true;
		return;
	}
	else
	{
		if ((iStateReligionInfluence > (iVictoryTarget / 2)))
		{
			m_bConsiderReligiousVictory = true;
			return;
		}
	}

}


bool CvPlayerAI::hasInquisitionTarget() const
{
	return m_bHasInquisitionTarget;
}

void CvPlayerAI::AI_setHasInquisitionTarget()
{
	PROFILE_FUNC();

	m_bHasInquisitionTarget = false;
	if (!isInquisitionConditions())
	{
		return;
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		if (kLoopPlayer.isAlive())
		{
			if ((kLoopPlayer.getTeam() == getTeam()) ||
			(GET_TEAM(getTeam()).isVassal((TeamTypes)kLoopPlayer.getTeam())))
			{
				foreach_(const CvCity * pLoopCity, kLoopPlayer.cities())
				{
					if (pLoopCity->isInquisitionConditions())
					{
						if ((pLoopCity->getRevTrend() > 0) && (pLoopCity->getRevolutionIndex() > 500))
						{
							m_bHasInquisitionTarget = true;
							return;
						}
						else if (pLoopCity->getRevolutionIndex() > 1000)
						{
							m_bHasInquisitionTarget = true;
							return;
						}
					}
				}
			}
		}
	}

	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
			if (kLoopPlayer.isAlive())
			{
				const CvTeam& kLoopTeam = GET_TEAM(kLoopPlayer.getTeam());
				if ((TeamTypes(kLoopPlayer.getTeam()) == getTeam()) || kLoopTeam.isVassal((TeamTypes)kLoopPlayer.getTeam()))
				{
					foreach_(const CvCity * pLoopCity, kLoopPlayer.cities())
					{
						if (pLoopCity->isInquisitionConditions())
						{
							m_bHasInquisitionTarget = true;
							return;
						}
					}
				}
			}
		}
	}
}

int CvPlayerAI::countCityReligionRevolts() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isInquisitionConditions())
		{
			if ((pLoopCity->getRevTrend() > 0) && (pLoopCity->getRevolutionIndex() > 500))
			{
				iCount++;
			}
			else if (pLoopCity->getRevolutionIndex() > 1000)
			{
				iCount++;
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_getEmbassyAttitude(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	const bool bVictim = ((GET_TEAM(getTeam()).AI_getMemoryCount((GET_PLAYER(ePlayer).getTeam()), MEMORY_DECLARED_WAR) == 0) && (GET_TEAM(getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam())));

	int iAttitude = 0;

	if (GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude = 1;
	}
	else if (((GET_TEAM(getTeam())).AI_getMemoryCount(GET_PLAYER(ePlayer).getTeam(), MEMORY_RECALLED_AMBASSADOR) > 0) && !bVictim)
	{
		iAttitude = -2;
	}

	return iAttitude;
}

int CvPlayerAI::AI_workerTradeVal(const CvUnit* pUnit) const
{
	PROFILE_FUNC();


	// `tradable` is ONE skill -- the legacy workerTrade/militaryTrade pair named the DEAL SLOT, not the unit
	// ([skills.md] §1), so WHICH slot a trade goes through is filtered here on the unit's own tag.
	const CvUnitInfo& kTradeUnit = GC.getUnitInfo(pUnit->getUnitType());

	if (!kTradeUnit.hasSkill(CLS_SKILL_TRADABLE) || !kTradeUnit.hasTag(CLS_TAG_WORKER))
	{//It's not a tradable worker, so it's worthless
		return 0;
	}

	//	Also scale by relative merits of this unit compared to the best worker we can produce in our capital
	UnitTypes eBestWorker = NO_UNIT;

	if (getCapitalCity() != NULL)
	{
		int iDummyValue;

		//	This can be called synchronmously orm asynchronously so set to no-rand
		eBestWorker = getCapitalCity()->AI_bestUnitAI(UNITAI_WORKER, iDummyValue, true, true);
	}

	int iValue = 0;
	if (eBestWorker != NO_UNIT)
	{
		iValue = (GC.getUnitInfo(eBestWorker).getProductionCost() > 0) ? GC.getUnitInfo(eBestWorker).getProductionCost() : 500;

		const int iBestUnitAIValue = AI_unitValue(eBestWorker, UNITAI_WORKER, getCapitalCity()->area());
		const int iThisUnitAIValue = AI_unitValue(pUnit->getUnitType(), UNITAI_WORKER, getCapitalCity()->area());

		//	Value as cost of production of the unit we can build scaled by their relative AI value
		iValue = (iThisUnitAIValue * iValue) / std::max(1, iBestUnitAIValue);
	}
	else
	{
		iValue = (GC.getUnitInfo(pUnit->getUnitType()).getProductionCost() > 0) ? GC.getUnitInfo(pUnit->getUnitType()).getProductionCost() : 500;
		iValue *= 3;	//	We can't build workers at all so value this up
	}

	//	Normalise for game speed
	iValue = iValue * CvGameSpeedScale::hammerCostPercent() / 100;
	int iNeededWorkers = 0;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		if (pLoopArea->getCitiesPerPlayer(getID()) > 0)
		{
			iNeededWorkers += AI_neededWorkers(pLoopArea);
		}
	}
	if (iNeededWorkers > 0)
	{
		//	If we could use a large number of workers that dosn't means the first one
		//	is worth a huge multiple - scale non-linearly up to 3 times base cost
		const int iScalingPercent = 300 - 400 / (1 + iNeededWorkers);
		iValue = 2 * (iValue * iScalingPercent) / 100;	//	Double final result as approx hammer->gold conversion
	}
	else
	{
		//	We don't really know what to do with it, so can sell cheap
		iValue /= 2;
	}
	// add a % adjustment global to the final result so that XML can adjust the outcome without having to directly tweak the formula which seems to consider all logical causes but not so much the end result.
	int iPercentAdjustment = (GC.getWORKER_TRADE_VALUE_PERCENT_ADJUSTMENT() * iValue) / 100;
	iValue += iPercentAdjustment;

	return std::max(0, iValue);
}

CvCity* CvPlayerAI::findBestCoastalCity() const
{
	PROFILE_EXTRA_FUNC();
	CvCity* pBestCity = NULL;
	bool	bFoundConnected = false;

	foreach_(CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
		{
			bool bValid = false;
			const bool bConnected = pLoopCity->isConnectedToCapital(getID());

			if (bConnected)
			{
				bValid = true;
			}
			else if (!bFoundConnected)
			{
				bValid = true;
			}

			if (bValid && (pBestCity == NULL || (bConnected && !bFoundConnected) || pLoopCity->getPopulation() > pBestCity->getPopulation()))
			{
				pBestCity = pLoopCity;
			}

			if (bConnected)
			{
				bFoundConnected = true;
			}
		}
	}

	return pBestCity;
}

int CvPlayerAI::strengthOfBestUnitAI(DomainTypes eDomain, UnitAITypes eUnitAIType) const
{
	PROFILE_FUNC();

	CvUnitSelectionCriteria	noGrowthCriteria;

	noGrowthCriteria.m_bIgnoreGrowth = true;
	const UnitTypes eBestUnit = bestBuildableUnitForAIType(eDomain, eUnitAIType, &noGrowthCriteria);

	if (eBestUnit != NO_UNIT)
	{
		return (GC.getUnitInfo(eBestUnit).getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100);
	}
	// We cannot build any! Take the average of any we already have.
	int iTotal = 0;
	int iCount = 0;
	foreach_(const CvUnit * pLoopUnit, units())
	{
		if (eUnitAIType == NO_UNITAI || pLoopUnit->AI_getUnitAIType() == eUnitAIType)
		{
			iCount++;
			iTotal += (pLoopUnit->getUnitInfo().getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100);
		}
	}
	if (iCount > 0)
	{
		iTotal /= iCount;
	}
	return std::max(1, iTotal);
}

UnitTypes CvPlayerAI::bestBuildableUnitForAIType(DomainTypes eDomain, UnitAITypes eUnitAIType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();

	// What is the best unit we can produce of this unitAI type
	CvCity* pCapitalCity = getCapitalCity();

	// Handle capital-less civs (aka barbs) by just using an arbitrary city
	int iDummy;
	if (pCapitalCity == NULL)
	{
		pCapitalCity = firstCity(&iDummy);
	}
	if (pCapitalCity == NULL)
	{
		return NO_UNIT;
	}
	switch (eDomain)
	{
		case NO_DOMAIN:
		{
			const UnitTypes eBestUnit = pCapitalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);

			if (eBestUnit != NO_UNIT || pCapitalCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
			{
				return eBestUnit;
			}
			// Drop through and check coastal
		}
		case DOMAIN_SEA:
		{
			CvCity* pCoastalCity =
			(
				pCapitalCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())
				?
				pCapitalCity
				:
				findBestCoastalCity()
			);
			if (pCoastalCity != NULL)
			{
				return pCoastalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);
			}
			break;
		}
		case DOMAIN_LAND:
		case DOMAIN_AIR:
		{
			return pCapitalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);
		}
	}
	return NO_UNIT;
}

int CvPlayerAI::AI_militaryUnitTradeVal(const CvUnit* pUnit) const
{
	PROFILE_FUNC();

	int iValue;
	const UnitTypes eUnit = pUnit->getUnitType();
	const UnitAITypes eAIType = GC.getUnitInfo(eUnit).getDefaultUnitAI();

	if (eAIType == UNITAI_SUBDUED_ANIMAL)
	{
		int iBestValue = 0;
		CvCity* pEvaluationCity = getCapitalCity();

		if (pEvaluationCity == NULL || (pUnit->getDomainType() == DOMAIN_SEA && !pEvaluationCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())))
		{
			pEvaluationCity = findBestCoastalCity();
		}

		if (pEvaluationCity != NULL)
		{
			const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

			//	Subdued animals are rated primarily on what they can construct
			foreach_(const int iGrantedBuilding, kUnit.getGrantedBuildings())
			{
				const BuildingTypes eBuilding = (BuildingTypes)iGrantedBuilding;

				if (getBuildingAvailabilityAnywhere(eBuilding) == EnablerDomain::STATE_LISTED
				&& AI_getNumBuildingsNeeded(eBuilding, pUnit->getDomainType() == DOMAIN_SEA) > 0)
				{
					foreach_(CvCity * pLoopCity, cities())
					{
						if (pLoopCity->area() == pEvaluationCity->area() && !pLoopCity->hasBuilding(eBuilding))
						{
							const int iValue = pLoopCity->AI_buildingValue(eBuilding);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
							}
						}
					}
				}
			}
			{
				// This section needs more work, the entire function might need work come to think of it.
				int iValue = 0;
				foreach_(const int iHeritage, kUnit.getHeritage())
				{
					if (canAddHeritage((HeritageTypes)iHeritage))
					{
						iValue += 25;
					}
				}
				if (iValue > iBestValue)
				{
					iBestValue = iValue;
				}
			}

			//	Also check their action outcomes (in the capital)
			for (int iI = 0; iI < kUnit.getNumActionOutcomes(); iI++)
			{
				if (kUnit.getActionOutcomeMission(iI) != NO_MISSION)
				{
					const CvOutcomeList* pOutcomeList = kUnit.getActionOutcomeList(iI);
					if (pOutcomeList->isPossibleInPlot(*pUnit, *(pEvaluationCity->plot()), true))
					{
						const int iValue = pOutcomeList->AI_getValueInPlot(*pUnit, *(pEvaluationCity->plot()), true);
						if (iValue > iBestValue)
						{
							iBestValue = iValue;
						}
					}
				}
			}

			for (int iJ = 0; iJ < GC.getNumUnitCombatInfos(); iJ++)
			{
				if (pUnit->isHasUnitCombat((UnitCombatTypes)iJ))
				{
					const CvUnitCombatInfo& kInfo = GC.getUnitCombatInfo((UnitCombatTypes)iJ);
					for (int iI = 0; iI < kInfo.getNumActionOutcomes(); iI++)
					{
						if (kInfo.getActionOutcomeMission(iI) != NO_MISSION)
						{
							const CvOutcomeList* pOutcomeList = kInfo.getActionOutcomeList(iI);
							if (pOutcomeList->isPossibleInPlot(*pUnit, *(pEvaluationCity->plot()), true))
							{
								const int iValue = pOutcomeList->AI_getValueInPlot(*pUnit, *(pEvaluationCity->plot()), true);
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
								}
							}
						}
					}
				}
			}
		}

		iValue = iBestValue;
	}
	else
	{
		const UnitTypes eBestUnit = bestBuildableUnitForAIType(pUnit->getDomainType(), eAIType);
		if (eBestUnit == NO_UNIT)
		{
			iValue = 2 * GC.getUnitInfo(eUnit).getProductionCost();	// We can't build anything like this so double its value
		}
		else
		{
			CvCity* pCapital = getCapitalCity();
			if (pCapital == NULL)
			{
				// Capital-less civ (bestBuildableUnitForAIType can still succeed via firstCity);
				// no area to scale against, so fall back to the unit's production cost.
				iValue = GC.getUnitInfo(eUnit).getProductionCost();
			}
			else
			{
				const int iBestUnitAIValue = AI_unitValue(eBestUnit, GC.getUnitInfo(eUnit).getDefaultUnitAI(), pCapital->area());
				const int iThisUnitAIValue = AI_unitValue(eUnit, GC.getUnitInfo(eUnit).getDefaultUnitAI(), pCapital->area());

				//	Value as cost of production of the unit we can build scaled by their relative AI value
				iValue = (iThisUnitAIValue * GC.getUnitInfo(eBestUnit).getProductionCost()) / std::max(1, iBestUnitAIValue);
			}
		}

		//	Normalise for game speed, and double as approximate hammer->gold conversion
		iValue = iValue * CvGameSpeedScale::hammerCostPercent() / 50;
	}

	return iValue;
}

int CvPlayerAI::AI_pledgeVoteTradeVal(const VoteTriggeredData* kData, PlayerVoteTypes ePlayerVote, PlayerTypes ePlayer) const
{
	return 1;
}

int CvPlayerAI::AI_corporationTradeVal(CorporationTypes eCorporation) const
{
	PROFILE_FUNC();

	int iValue = 100;

	foreach_(const CvCity * pLoopCity, cities())
	{
		int iTempValue = 2 * AI_corporationValue(eCorporation, pLoopCity);
		if (!pLoopCity->isHasCorporation(eCorporation))
		{
			iTempValue /= 2;
		}

		iValue += iTempValue;
	}

	int iCompetitorCount = 0;

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (iI != eCorporation)
		{
			if (hasHeadquarters((CorporationTypes)iI))
			{
				if (GC.getGame().isCompetingCorporation(eCorporation, (CorporationTypes)iI))
				{
					iCompetitorCount++;
				}
			}
		}
	}

	return (iValue * 3) / (3 + iCompetitorCount);

}

int CvPlayerAI::AI_secretaryGeneralTradeVal(VoteSourceTypes eVoteSource, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	typedef bst::array<int, MAX_TEAMS> VotesArray;
	VotesArray aiVotes;
	aiVotes.assign(0);

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		const TeamTypes teamIType = static_cast<TeamTypes>(iI);
		const CvTeamAI& teamI = GET_TEAM(teamIType);

		if (teamI.isAlive() &&
			teamI.isVotingMember(eVoteSource) && !(getTeam() == teamIType))
		{
			int iBestAttitude = -MAX_INT;
			TeamTypes eBestTeam = NO_TEAM;

			for (int iJ = 0; iJ < MAX_TEAMS; iJ++)
			{
				if (iI == iJ)
					continue;

				const TeamTypes teamJType = static_cast<TeamTypes>(iJ);
				if (GET_TEAM(teamJType).isAlive() &&
					GC.getGame().isTeamVoteEligible(teamJType, eVoteSource))
				{
					if (teamI.isVassal(teamJType))
					{
						aiVotes[iJ] += teamI.getVotes(NO_VOTE, eVoteSource);
					}

					const int iAttitude = teamI.AI_getAttitudeVal(teamJType);

					if (iAttitude > iBestAttitude)
					{
						iBestAttitude = iAttitude;
						eBestTeam = teamJType;
					}
				}
			}
			aiVotes[eBestTeam] += teamI.getVotes(NO_VOTE, eVoteSource);
		}
	}

	VotesArray::iterator maxVotesItr = std::max_element(aiVotes.begin(), aiVotes.end());
	int iMostVotes = *maxVotesItr;
	TeamTypes eLikelyWinner = static_cast<TeamTypes>(std::distance(aiVotes.begin(), maxVotesItr));

	bool bKingMaker = false;
	int iOurVotes = 0;
	int iTheirVotes = 0;
	int iValue = 100;

	if (eLikelyWinner != GET_PLAYER(ePlayer).getTeam())
	{
		iOurVotes = GET_TEAM(getTeam()).getVotes(NO_VOTE, eVoteSource);
		iTheirVotes = aiVotes[GET_PLAYER(ePlayer).getTeam()];

		if ((iOurVotes + iTheirVotes) > iMostVotes)
		{
			bKingMaker = true;
		}

		int iOurSharedCivics = 0;
		int iTheirSharedCivics = 0;
		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			if (getCivics((CivicOptionTypes)iI) == GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iI))
			{
				iOurSharedCivics++;
			}

			if (getCivics((CivicOptionTypes)iI) == GET_PLAYER(GET_TEAM(eLikelyWinner).getLeaderID()).getCivics((CivicOptionTypes)iI))
			{
				iTheirSharedCivics++;
			}
		}

		iValue *= std::max(1, iOurSharedCivics);
		iValue /= std::max(1, iTheirSharedCivics);
	}
	else
	{
		bKingMaker = false;
	}

	if (bKingMaker)
	{
		const int iExtraVotes = ((iOurVotes + iTheirVotes) - iMostVotes);

		iValue *= iExtraVotes;
	}

	iValue *= 2;
	return iValue;
}

int CvPlayerAI::AI_getCivicAttitudeChange(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	int iAttitude = 0;

	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
		{
			const CvCivicInfo& kCivicOption = GC.getCivicInfo(getCivics((CivicOptionTypes)iI));

			for (int iJ = 0; iJ < GC.getNumCivicOptionInfos(); iJ++)
			{
				const int eCivic = GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iJ);

				if (eCivic != NO_CIVIC)
				{
					// diplomacy.empire.civics.{CIVIC_X} -- the civic's own keyed entry for the OTHER player's
					// civic, a FLAT reduced at the point of use.
					iAttitude += InfoValuation::keyedTarget(kCivicOption.getModifiers(), MODFAM_DIPLOMACY,
						DIPLOMACY_AMOUNT, InfoValuation::keyedTargetSegment("civics"), eCivic) / 100;
				}
			}
		}
	}

	return iAttitude;
}

int CvPlayerAI::AI_getCivicShareAttitude(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	int iAttitude = 0;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (NO_CIVIC != getCivics((CivicOptionTypes)iI) && getCivics((CivicOptionTypes)iI) == GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iI))
		{
			// diplomacy.empire.attitudeShare -- a FLAT, so it reduces at the point of use.
			iAttitude += GC.getCivicInfo((CivicTypes)getCivics((CivicOptionTypes)iI))
				.getDiplomacy(DIPLOMACY_ATTITUDE_SHARE, CASC_SCOPE_EMPIRE) / 100;
		}
	}
	return iAttitude;
}


TeamTypes CvPlayerAI::AI_bestJoinWarTeam(PlayerTypes eOtherPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& playerOther = GET_PLAYER(eOtherPlayer);
	const TeamTypes eOtherTeam = playerOther.getTeam();
	const CvTeamAI& teamOther = GET_TEAM(eOtherTeam);
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);

	int iBestValue = 0;
	TeamTypes eWarTeam = NO_TEAM;

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		const TeamTypes eTeamX = GET_PLAYER((PlayerTypes)iI).getTeam();

		if (eTeam != eTeamX && team.isHasMet(eTeamX)
		&& !teamOther.isDefensivePact(eTeamX) && teamOther.canDeclareWar(eTeamX)
		&& // If we are at war, or they have backstabbed a friend, or they are a mutual enemy
			(
				atWar(eTeamX, eTeam)
				||
				AI_getMemoryCount((PlayerTypes)iI, MEMORY_BACKSTAB_FRIEND) > 0
				||
				playerOther.AI_getAttitude((PlayerTypes)iI) < ATTITUDE_CAUTIOUS
				&&
				AI_getAttitude((PlayerTypes)iI) < ATTITUDE_CAUTIOUS
				)
		&&
			(
				playerOther.isHumanPlayer()
				||
				playerOther.AI_getAttitude(getID()) > GC.getLeaderHeadInfo(playerOther.getPersonalityType()).getRefuseAttitudeThreshold(REFUSAL_DECLARE_WAR)
				&&
				playerOther.AI_getAttitude((PlayerTypes)iI) < ATTITUDE_PLEASED
				)
		)
		{
			int iValue = team.AI_declareWarTradeVal(eTeamX, eOtherTeam);
			// Favor teams we are already at war with
			if (!atWar(eTeamX, eTeam))
			{
				iValue *= 2;
				iValue /= 3;
			}
			if (iBestValue < iValue)
			{
				iBestValue = iValue;
				eWarTeam = eTeamX;
			}
		}
	}
	return eWarTeam;
}

TeamTypes CvPlayerAI::AI_bestMakePeaceTeam(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	//kWarTeam is the team that is at war with the player we want them to make peace with
	const CvTeamAI& kWarTeam = GET_TEAM(GET_PLAYER(ePlayer).getTeam());
	const int iTheirPower = kWarTeam.getPower(true);
	int iBestValue = 250; //set a small threshold
	TeamTypes eBestTeam = NO_TEAM;
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if ((iI != ePlayer) && (iI != getID()))
		{
			//kPlayer is who we want kWarTeam to make peace with...
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kPlayer.isAlive() && !kPlayer.isMinorCiv())
			{
				if (GET_TEAM(getTeam()).isHasMet(kPlayer.getTeam()))
				{
					if (atWar(kPlayer.getTeam(), GET_PLAYER(ePlayer).getTeam()) && !atWar(getTeam(), kPlayer.getTeam()))
					{
						if (GET_PLAYER(ePlayer).AI_isWillingToTalk((PlayerTypes)iI) && kWarTeam.AI_makePeaceTrade(kPlayer.getTeam(), getTeam()) == NO_DENIAL)
						{
							int iValue = std::max(0, AI_getAttitudeVal((PlayerTypes)iI) * 100);
							if (GET_TEAM(getTeam()).AI_getWarPlan(kPlayer.getTeam()) != NO_WARPLAN)
							{
								//We are planning war, peace is not "that" valuable after all
								iValue /= 10;
							}
							if (iTheirPower > GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true) * 3)
							{
								//They are likely to crush the enemy player, we don't want them to win
								iValue *= 4;
							}
							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestTeam = kPlayer.getTeam();
							}
						}
					}
				}
			}
		}
	}
	return eBestTeam;
}

TeamTypes CvPlayerAI::AI_bestStopTradeTeam(PlayerTypes eOtherPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& playerOther = GET_PLAYER(eOtherPlayer);
	const TeamTypes eOtherTeam = playerOther.getTeam();
	const CvTeamAI& teamOther = GET_TEAM(eOtherTeam);
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);
	const TeamTypes eWorstEnemy = team.AI_getWorstEnemy();

	int iBestValue = MAX_INT;
	TeamTypes eBestTeam = NO_TEAM;

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		const TeamTypes eTeamX = (TeamTypes)iI;

		if (GET_TEAM(eTeamX).isAlive() && eTeam != eTeamX && eOtherTeam != eTeamX
		&& team.isHasMet(eTeamX) && teamOther.isHasMet(eTeamX) && !atWar(eOtherTeam, eTeamX)
		&&
			(
				eWorstEnemy == eTeamX
				|| atWar(eTeamX, eTeam)
				|| team.AI_getAttitude(eTeamX) < ATTITUDE_CAUTIOUS
				|| AI_getMemoryCount(eOtherPlayer, MEMORY_BACKSTAB_FRIEND) > 0
				)
		&& playerOther.canStopTradingWithTeam(eTeamX)
		&& playerOther.AI_stopTradingTrade(eTeamX, getID()) == NO_DENIAL)
		{
			const int iValue = AI_stopTradingTradeVal(eTeamX, eOtherPlayer);
			if (iValue < iBestValue)
			{
				iBestValue = iValue;
				eBestTeam = eTeamX;
			}
		}
	}
	return eBestTeam;
}

int CvPlayerAI::AI_militaryBonusVal(BonusTypes eBonus)
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	// Only the units whose `requires` names this bonus can be carried by it ([DEC-one-reverse-view]); the
	// trainable union below is then the "and I could actually field it" half.
	std::set<int> dependentUnits;
	EnablerKernel::addEdge(EnablerKernel::infoFor(EDGEB_BONUSES, (int)eBonus),
		EDGEF_REQUIRED_BY, EDGEB_UNITS, dependentUnits);

	CvCascadeHypothetical kMilWith;
	kMilWith.present[EDGEB_BONUSES].insert((int)eBonus);
	CvCascadeHypothetical kMilWithout;
	kMilWithout.absent[EDGEB_BONUSES].insert((int)eBonus);
	const CvCity* pCapital = getCapitalCity();

	// the trainable-anywhere UNION, not a per-id fan over the whole database (types x cities). The accumulation
	// below is commutative, so iteration order does not affect the total.
	std::vector<int> vecTrainable;
	getTrainableAnywhere(vecTrainable);
	for (std::vector<int>::const_iterator it = vecTrainable.begin(), itEnd = vecTrainable.end(); it != itEnd; ++it)
	{
		{
			const int iI = *it;

			// Does this bonus carry the unit, or merely appear among its options? The gate answers both at once
			// (the same read the bonus-trade and base-bonus valuations make): trainable with the bonus and not
			// without it means the bonus is LOAD-BEARING here. The old pair of tests reconstructed that from the
			// AND/OR prereq lists and then divided by the alternatives already held -- which is precisely the
			// case where the verdict does NOT change, and now simply scores nothing.
			if (dependentUnits.count(iI) == 0 || pCapital == NULL)
			{
				continue;
			}
			if (EnablerKernel::requiresMetInCity(*pCapital, EDGEB_UNITS, iI, false, &kMilWith)
			&& !EnablerKernel::requiresMetInCity(*pCapital, EDGEB_UNITS, iI, false, &kMilWithout))
			{
				iValue += 1000;
			}
		}
	}
	return iValue;
}


//Slightly altered form of CvUnitAI::AI_promotionValue()
int CvPlayerAI::AI_promotionValue(PromotionTypes ePromotion, UnitTypes eUnit, const CvUnit* pUnit, UnitAITypes eUnitAI, bool bForBuildUp) const
{
	PROFILE_EXTRA_FUNC();
	int iTemp = 0;
	int iExtra;
	int iValue = 0;
	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
	const CvPlot* pPlot = pUnit ? pUnit->plot() : NULL;
	const int iMoves = pUnit ? pUnit->maxMoves() : (kUnit.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100);
	const bool bNoDefensiveBonus = !pUnit && kUnit.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS) || pUnit && pUnit->noDefensiveBonus();

	if (eUnitAI == NO_UNITAI)
	{
		eUnitAI = kUnit.getDefaultUnitAI();
	}

	//#1 Analyze basic promotion effects
	if (pUnit)
	{
		for (int iI = 0; iI < kPromotion.getNumAIWeightsByUnitCombat(); iI++)
		{
			const UnitCombatModifier& kWeight = kPromotion.getAIWeightByUnitCombat(iI);

			if (kWeight.iModifier != 0
			&& (pUnit->isHasUnitCombat(kWeight.eUnitCombat) || kUnit.hasCombatClass(kWeight.eUnitCombat)))
			{
				iValue += kWeight.iModifier;
			}
		}
	}

	//#2 Effects that depend on unitAI role Spy
	if (kUnit.hasTag(CLS_TAG_SPY))
	{
		//Readjust promotion choices favoring security, deception, logistics, escape, improvise,
		//filling in other promotions very lightly because the AI does not yet have situational awareness
		//when using spy promotions at the moment of mission execution.

		//Logistics
		//I & III
		iValue += (kPromotion.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100 * 20);
		//II
		if (kPromotion.hasSkill(CLS_SKILL_ENEMY_ROUTE)) iValue += 20;
		iValue += (kPromotion.getMovement(MOVEMENT_MOVE_DISCOUNT, CASC_SCOPE_UNIT) / 100 * 10);
		//total 20, 30, 20 points

		//Deception
		if (kPromotion.getAir(AIR_EVASION, CASC_SCOPE_UNIT))
		{
			//Lean towards more deception if deception is already present
			iValue += ((kPromotion.getAir(AIR_EVASION, CASC_SCOPE_UNIT) * 2) + (pUnit == NULL ? 0 : pUnit->evasionProbability()));
		}//total 20, 30, 40 points

		//Security
		iValue += kPromotion.getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) * 10 / VISION_OPEN_GROUND_COST;
		//Lean towards more security if security is already present
		iValue += (kPromotion.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) + (pUnit == NULL ? kUnit.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) : pUnit->currInterceptionProbability()));
		//total 20, 30, 40 points

		//Escape
		if (kPromotion.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT))
		{
			iValue += 30;
		}

		//Improvise
		if (kPromotion.getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_UNIT) != 0)
		{
			iValue += 20;
		}

		//Loyalty
		if (kPromotion.hasSkill(CLS_SKILL_ALWAYS_HEAL))
		{
			iValue += 15;
		}

		//Instigator
		//I & II
		if (kPromotion.getFlatHeal(HEAL_ENEMY_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}
		//III
		if (kPromotion.getFlatHeal(HEAL_NEUTRAL_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}

		//Alchemist
		if (kPromotion.getFlatHeal(HEAL_FRIENDLY_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}

		for (int iI = 0; iI < (int)kPromotion.providesUnitCombats().size(); iI++)
		{
			if (pUnit)
			{
				if (!pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.providesUnitCombats()[iI]))
				{
					iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.providesUnitCombats()[iI], eUnit, pUnit, eUnitAI);
				}
			}
			else if (!kUnit.hasCombatClass((UnitCombatTypes)kPromotion.providesUnitCombats()[iI]))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.providesUnitCombats()[iI], eUnit, pUnit, eUnitAI);
			}
		}

		// The combat-class membership the promotion STRIPS -- the info's own typed list ([skills.md] §1:
		// `removesUnitCombats`), not a walk of the unitcombat registry.
		const std::vector<int>& removedCombats = kPromotion.removesUnitCombats();
		for (size_t iAt = 0; iAt < removedCombats.size(); ++iAt)
		{
			const UnitCombatTypes eRemoved = (UnitCombatTypes)removedCombats[iAt];

			if (pUnit ? pUnit->isHasUnitCombat(eRemoved) : kUnit.hasCombatClass(eRemoved))
			{
				iValue -= AI_unitCombatValue(eRemoved, eUnit, pUnit, eUnitAI);
			}
		}

		if (pUnit && iValue > 0 && !bForBuildUp)
		{
			//GC.getGame().logOOSSpecial(1,iValue, pUnit->getID());
			iValue += GC.getGame().getSorenRandNum(25, "AI Spy Promote");
		}

		return iValue;
	}

	//#3 Effects that depend on unitAI role Leader
	// Leaders don't take promotions like regular units, so we don't want to value them here
	if (kPromotion.isLeader())
	{
		// Don't consume the leader as a regular promotion
		return 0;
	}

	//#4 Effects that depend on unitAI role Hunter
	if (eUnitAI == UNITAI_HUNTER ||
		(eUnitAI == UNITAI_HUNTER_ESCORT) ||
		(eUnitAI == UNITAI_GREAT_HUNTER))
	{
		//	Koshling - this is a horrible kludge really to get the AI to realize that promotions
		//	with a subdue animal bonus are VERY good on hunters.  However, it's VERY hard to figure
		//	this out from first principals because its very indirect (outcome has per-promotion value,
		//	but the outcome itself just defines a unit, which then has a build, which than has value!)
		//	As a result I am just giving a bonus to outcome modifiers for hunters generically, which
		//	CURRENTLY works ok since the available outcomes are basically the animal subdues
		for (int iI = 0; iI < GC.getNumOutcomeInfos(); iI++)
		{
			const std::map<int, int>& kPromotionOdds = GC.getOutcomeInfo((OutcomeTypes)iI).getPromotionOdds();
			const std::map<int, int>::const_iterator itOdds = kPromotionOdds.find((int)ePromotion);
			if (itOdds != kPromotionOdds.end())
			{
				iValue += 2 * itOdds->second;
			}
		}
	}

	//#5 Effects for Promotions that have Blitz
	if (kPromotion.hasSkill(CLS_SKILL_BLITZ) && !bForBuildUp)
	{
		//ls612: AI to know that Blitz is only useful on units with more than one move now that the filter is gone
		if (iMoves > 1)
		{
			if ((eUnitAI == UNITAI_RESERVE ||
				eUnitAI == UNITAI_ATTACK ||
				eUnitAI == UNITAI_ATTACK_CITY ||
				eUnitAI == UNITAI_PARADROP))
			{
				iValue += (10 * iMoves);
			}
			else
			{
				iValue += 2;
			}
		}
		else
		{
			iValue += 0;
		}
	}

	//#6 Effects for Promotions that have Amphibious ??
	if (kPromotion.hasSkill(CLS_SKILL_ONE_UP))
	{
		if (eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ESCORT)
		{
			iValue += 20;
		}
		else iValue += 5;
	}

	//#7 Effects for Promotions that have Defensive Victory Move
	if (kPromotion.hasSkill(CLS_SKILL_DEFENSIVE_VICTORY_MOVE))
	{
		if (eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_ATTACK)
		{
			iValue += 12;
		}
		else
		{
			iValue += 8;
		}
	}

	//#8 Effects for Promotions that have Free Drop
	if (kPromotion.hasSkill(CLS_SKILL_FREE_DROP))
	{
		if (eUnitAI == UNITAI_PILLAGE || eUnitAI == UNITAI_ATTACK)
		{
			iValue += 10;
		}
		else iValue += 8;
	}

	//#9 Effects for Promotions that have Offensive Victory Move
	if (kPromotion.hasSkill(CLS_SKILL_OFFENSIVE_VICTORY_MOVE))
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}

		if (pUnit)
		{
			if (pUnit->isBlitz() || pUnit->isFreeDrop())
			{
				iTemp *= 20;
			}
		}
	}
	iValue += iTemp;

	//#10 Effects for Promotions that have Retreat
	if (kPromotion.hasSkill(CLS_SKILL_EXCILE))
	{
		if (pUnit && !pUnit->isExcile())
		{
			iValue -= 25;
		}
	}

	//#11 Effects for Promotions that have Passage
	if (kPromotion.hasSkill(CLS_SKILL_PASSAGE))
	{
		if (pUnit && !pUnit->isPassage())
		{
			if (eUnitAI == UNITAI_ESCORT)
			{
				iValue += 50;
			}
			else
			{
				iValue += 25;
			}
		}
	}

	// The blendIntoCity and noNonOwnedCityEntry skills carry no automatic value here; they may be worth
	// weighting per AI type later.

	//#14 Effects for Promotions that have BarbCoExist
	// ⚠ BEHAVIOUR CHANGE, deliberate: the legacy block was INERT -- both branches accumulated into iTemp and
	// never added it to iValue, so barbarian co-existence scored nothing either way. The authored weights are
	// applied as they plainly read.
	if (kPromotion.hasSkill(CLS_SKILL_BARB_CO_EXIST))
	{
		if (eUnitAI == UNITAI_EXPLORE)
		{
			iValue += 20;
		}
		else if (eUnitAI == UNITAI_ESCORT)
		{
			iValue -= 100;
		}
	}

	//#15 Effects for Promotions with isPillages...
	{
		iTemp = 0;
		if (kPromotion.hasSkill(CLS_SKILL_PILLAGE_ESPIONAGE))
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		iValue += iTemp;

		iTemp = 0;
		if (kPromotion.hasSkill(CLS_SKILL_PILLAGE_MARAUDER))
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		iValue += iTemp;

		iTemp = 0;
		if (kPromotion.hasSkill(CLS_SKILL_PILLAGE_ON_MOVE))
		{
			if (pUnit)
			{
				if (pUnit->isPillageEspionage()
				||  pUnit->isPillageMarauder()
				||  pUnit->isPillageResearch())
				{
					if (eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 16;
					}
					else iTemp += 8;
				}
				else iTemp++;
			}
			else iTemp++;
		}
		iValue += iTemp;

		iTemp = 0;
		if (kPromotion.hasSkill(CLS_SKILL_PILLAGE_ON_VICTORY))
		{
			if (pUnit)
			{
				if (pUnit->isPillageEspionage()
				|| pUnit->isPillageMarauder()
				|| pUnit->isPillageResearch())
				{
					if (eUnitAI == UNITAI_PILLAGE
					|| eUnitAI == UNITAI_ATTACK
					|| eUnitAI == UNITAI_PARADROP
					|| eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 20;
					}
					else iTemp += 12;
				}
				else iTemp += 4;
			}
			else iTemp += 4;
		}
		iValue += iTemp;

		iTemp = 0;
		if (kPromotion.hasSkill(CLS_SKILL_PILLAGE_RESEARCH))
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		if (iMoves > 1)
		{
			iTemp *= 100;
		}
		iValue += iTemp;
	}

	//#16 Effects for AirCombat Units that have extra attacks...

	//#17 Effects for Promotions that have Happyness Bonus...
	iTemp = (kPromotion.hasSkill(CLS_SKILL_CELEBRITY) ? 1 : 0);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_HEALER
		||  eUnitAI == UNITAI_HEALER_SEA
		||  eUnitAI == UNITAI_PROPERTY_CONTROL
		||  eUnitAI == UNITAI_PROPERTY_CONTROL_SEA)
		{
			iValue += iTemp * 10;

			if (pUnit)
			{
				const CvCity* pCity = pPlot->getPlotCity();
				if (pCity && pCity->netHappiness() < 0)
				{
					iValue += iTemp * 40;
				}
			}
		}
	}

	//#18 Effects for Promotions that have extra collateral damage...
	iTemp = kPromotion.getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (15 * iTemp);
		}
	}

	//#19 Effects 
	iTemp = kPromotion.getFlatCombat(COMBAT_LIMIT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				iTemp *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iTemp /= 4;
				}
				iValue += iTemp;
			}
			else
			{
				int iCombatLimitValueProcessed = std::min(iTemp, pUnit->combatLimit() - 100) * 3;
				if (eUnitAI != UNITAI_COLLATERAL)
				{
					iCombatLimitValueProcessed /= 4;
				}

				iValue += iCombatLimitValueProcessed;
			}
		}
		else if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 3);
		}
	}

	//#20 Effects
	iTemp = kPromotion.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 20 * iTemp;
		}
	}


	//#21 Effects for Promotions that have a chance to survive...
	iTemp = kPromotion.getScalar(SCALAR_SURVIVOR, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 5 * iTemp;
		}
		else
		{
			iValue += 2 * iTemp;
		}
	}

	iTemp = 0;
	int iWeight = 0;
	const CvPropertyManipulators* promotionPropertyManipulators = GC.getPromotionInfo(ePromotion).getPropertyManipulators();

	//#22 Effects for Promotions that have Property Manipulators...
	if (promotionPropertyManipulators)
	{
		foreach_(const CvPropertySource * pSource, promotionPropertyManipulators->getSources())
		{
			if (pSource->getType() == PROPERTYSOURCE_CONSTANT)
			{
				const PropertyTypes eProperty = pSource->getProperty();
				int iTemp2 = ((CvPropertySourceConstant*)pSource)->getAmountPerTurn(getGameObject());
				if (pUnit && iTemp2 != 0)
				{
					foreach_(const CvPropertySource * uSource, GC.getUnitInfo(pUnit->getUnitType()).getPropertyManipulators()->getSources())
					{
						if (uSource->getType() == PROPERTYSOURCE_CONSTANT && eProperty == uSource->getProperty())
						{
							iTemp2 += ((CvPropertySourceConstant*)uSource)->getAmountPerTurn(getGameObject());
						}
					}
				}
				iTemp += iTemp2 * GC.getPropertyInfo(eProperty).getAIWeight();
			}
		}

		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_PROPERTY_CONTROL
			||  eUnitAI == UNITAI_PROPERTY_CONTROL_SEA)
			{
				iValue += 10 * iTemp;

				if (pUnit)
				{
					const MissionAITypes eMissionAI = pUnit->getGroup()->AI_getMissionAIType();
					if (eMissionAI == MISSIONAI_PROPERTY_CONTROL_RESPONSE
					||  eMissionAI == MISSIONAI_PROPERTY_CONTROL_MAINTAIN)
					{
						iValue += 10 * iTemp;
					}
				}
			}
			else if (
			    eUnitAI == UNITAI_HEALER
			||  eUnitAI == UNITAI_HEALER_SEA
			||  eUnitAI == UNITAI_CITY_DEFENSE
			||  eUnitAI == UNITAI_CITY_SPECIAL
			||  eUnitAI == UNITAI_INVESTIGATOR
			||  eUnitAI == UNITAI_ESCORT
			||  eUnitAI == UNITAI_SEE_INVISIBLE
			||  eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
			{
				iValue += iTemp / 10;
			}
			else if (
			    eUnitAI == UNITAI_BARB_CRIMINAL
			||  eUnitAI == UNITAI_INFILTRATOR)
			{
				iValue -= 10 * iTemp;
			}
		}
	}

	//#23 Effects for Promotions that get Hidden Nationality...
	if (kPromotion.hasSkill(CLS_SKILL_HIDDEN_NATIONALITY))
	{
		if (pUnit)
		{
			if (!pUnit->isHiddenNationality())
			{
				if (eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += 100;
				}
				else if (eUnitAI == UNITAI_ESCORT)
				{
					iValue += 50;
				}
				iValue += 30;
			}
		}
	}

	//TBHEAL review
	//isNoSelfHeal()
	//#24 Effects for Promotions that don't heal...
	if (kPromotion.hasSkill(CLS_SKILL_NO_SELF_HEAL))
	{
		if (pUnit)
		{
			if (!pUnit->hasNoSelfHeal())
			{
				iValue -= 50;
			}
		}
		else
		{
			iValue -= 25;
		}
	}

	//#25 Effects for Promotions with ExtraMax HP()...
	iTemp = kPromotion.getSizeMatters().maxHP;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getExtraMaxHP();
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY_LEMMING) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 15;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 10;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	//#26 Effects for Promotions with Self Heal...
	iTemp = kPromotion.getHealModifier(HEAL_SELF_MODIFIER, CASC_SCOPE_UNIT);
	if (iTemp > 0)
	{
		if (bForBuildUp)
		{
			if (pUnit)
			{
				iTemp *= pUnit->getDamage();
			}
			iValue += iTemp;
		}
		else
		{
			if (pUnit)
			{
				iTemp += pUnit->getSelfHealModifierTotal();
				if (pUnit->hasNoSelfHeal())
				{
					iTemp *= 2;
				}
			}

			if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_RESERVE) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
			{
				iTemp *= 10;
			}
			else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
					(eUnitAI == UNITAI_COLLATERAL) ||
					(eUnitAI == UNITAI_PILLAGE) ||
					(eUnitAI == UNITAI_CITY_DEFENSE) ||
					(eUnitAI == UNITAI_CITY_COUNTER) ||
					(eUnitAI == UNITAI_ATTACK_SEA) ||
					(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
					(eUnitAI == UNITAI_PIRATE_SEA) ||
					(eUnitAI == UNITAI_ATTACK_AIR) ||
					(eUnitAI == UNITAI_PARADROP) ||
					(eUnitAI == UNITAI_PILLAGE_COUNTER))
			{
				iTemp *= 5;
			}
			else
			{
				iTemp *= 2;
			}

			iValue += iTemp;
		}
	}

	//#27 Effects for Promotions on ennemy Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_ENEMY_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iTemp *= 4;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	//#28 Effects for Promotions on neutral Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_NEUTRAL_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 2;
		}

		iValue += iTemp;
	}

	//#29 Effects for Promotions on Friendly Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_FRIENDLY_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	//#30 Effects for Promotions on Victory Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_VICTORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp = 0;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 5;
		}

		iValue += iTemp;
	}

	//#31 Effects for Promotions on num of Heal supported...
	iTemp = (kPromotion.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100);
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 20;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 5;
		}
		if (bForBuildUp && pPlot != NULL && pUnit != NULL)
		{
			UnitCombatTypes eUnitCombat = pUnit->getBestHealingTypeConst();
			if (eUnitCombat != NO_UNITCOMBAT)
			{
				DomainTypes eDomain = pUnit->getDomainType();
				int iUnsup = pPlot->getInjuredUnitCombatsUnsupportedByHealer(pUnit->getOwner(), eUnitCombat, eDomain);
				iValue += iTemp * (iUnsup * 10);
			}
		}
		else
		{
			iValue += iTemp;
		}
	}

	//#32 Effects for Promotions on num of Same tile Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_SAME_TILE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 6;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 2;
		}

		if (bForBuildUp && pPlot != NULL && pUnit != NULL)
		{
			UnitCombatTypes eUnitCombat = pUnit->getBestHealingTypeConst();
			if (eUnitCombat != NULL)
			{
				DomainTypes eDomain = pUnit->getDomainType();
				int iInjured = pPlot->plotCount(PUF_isInjuredUnitCombatType, eUnitCombat, eDomain, NULL, pUnit->getOwner());
				iValue += iTemp * (iInjured * 5);
			}
		}
		else
		{
			iValue += iTemp;
		}
	}

	//#33 Effects for Promotions on num of Adj tile Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_ADJACENT_TILE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 5;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	//#34 Effects for Promotions on num Combat unit Heal...
	std::vector<HealByUnitCombat> healRows;
	InfoValuation::collectHealByUnitCombat(kPromotion.getModifiers(), healRows);
	if (!healRows.empty())
	{
		for (size_t iRow = 0; iRow < healRows.size(); ++iRow)
		{
			const UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)healRows[iRow].iUnitCombat;
			//	the deposits are ×100 amounts; this weighting consumes whole hit points
			iTemp = healRows[iRow].iHeal / 100;
			iTemp += healRows[iRow].iAdjacentHeal / 100;
			iTemp += (kPromotion.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100);

			if (iTemp > 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
					iTemp += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
					iTemp += ((pUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) * 2);
					iTemp += (((pUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) * 2) / 3);

					iTemp *= (pUnit->getNumHealSupportTotal() + (kPromotion.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100));
				}
				if ((eUnitAI == UNITAI_HEALER) ||
					(eUnitAI == UNITAI_HEALER_SEA))
				{
					iTemp *= 4;
				}
				else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
					(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
					(eUnitAI == UNITAI_ESCORT_SEA) ||
					(eUnitAI == UNITAI_HUNTER_ESCORT) ||
					(eUnitAI == UNITAI_ESCORT))
				{
					iTemp *= 2;
					iTemp /= 3;
				}
				else
				{
					iTemp /= 4;
				}
				if (bForBuildUp && pPlot != NULL && pUnit != NULL)
				{
					DomainTypes eDomain = pUnit->getDomainType();
					int iInjured = pPlot->plotCount(PUF_isInjuredUnitCombatType, eHealUnitCombat, eDomain, NULL, pUnit->getOwner());
					iValue += iTemp * (iInjured * 5);
				}
				else
				{
					iValue += iTemp;
				}
			}
		}
	}


	//#35 Effects for Promotions on Victory Stack Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_VICTORY_STACK, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = ((pUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).hasSkill(CLS_SKILL_HEALS_AS))
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + (kPromotion.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100));
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	//#36 Effects for Promotions on Victory Adj. Heal...
	iTemp = kPromotion.getFlatHeal(HEAL_VICTORY_ADJACENT, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = ((pUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).hasSkill(CLS_SKILL_HEALS_AS))
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + (kPromotion.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100));
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	//#37 Effects for Promotions on Always Heal...
	if (kPromotion.hasSkill(CLS_SKILL_ALWAYS_HEAL))
	{
		if ((eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 35;
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 15;
		}
		else
		{
			iValue += 8;
		}
	}
	
	//#38 Effects for Promotions that give Amphib.
	if (kPromotion.hasSkill(CLS_SKILL_AMPHIB))
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 25;
		}
		else
		{
			iValue++;
		}
	}

	//#39 Effects for Promotions that give Xtra Moves on river...
	if (kPromotion.hasSkill(CLS_SKILL_RIVER))
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 15;
		}
		else
		{
			iValue++;
		}
	}

	//#40 Effects for Promotions that give Xtra Moves on ennemy routes...
	if (kPromotion.hasSkill(CLS_SKILL_ENEMY_ROUTE))
	{
		if (eUnitAI == UNITAI_PILLAGE)
		{
			iValue += (50 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
				   (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (30 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (20 + (4 * iMoves));
		}
		else
		{
			iValue += (4 * iMoves);
		}
	}

	//#41 Effects for Promotions that give Xtra Moves on Hills...
	if (kPromotion.hasSkill(CLS_SKILL_HILLS_DOUBLE_MOVE))
	{
		if (eUnitAI == UNITAI_EXPLORE ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 50;
		}
		else
		{
			iValue += 10;
		}
	}

	//#42 Effects for Promotions that give Moves on Peaks...
	if (kPromotion.getSkills()->has("canPassPeaks") && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 50;
	}
	if (kPromotion.getSkills()->has("canLeadThroughPeaks") && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{// Ability to lead a stack through mountains
		iValue += 75;
	}

	//#43 Effects for Promotions that immunise to 1st Strikes...
	if ((kPromotion.hasSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || kPromotion.hasSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE))
		&& (pUnit == NULL || !pUnit->immuneToFirstStrikes()))
	{
		if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 25;
		}
		else if ((eUnitAI == UNITAI_ATTACK))
		{
			iValue += 15;
		}
		else
		{
			iValue += 10;
		}
	}

	//#44 Effects for Promotions that give Insidiousness...
	iTemp = kPromotion.getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		//TB Must update here as soon as I can improve on AI - not sure about pirates yet.
		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp *= 10;
		}
		else if (/*(eUnitAI == UNITAI_PIRATE_SEA) ||*/
			(eUnitAI == UNITAI_PILLAGE))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp *= -1;
		}
		if (pUnit)
		{
			if (pPlot != NULL)
			{
				CvCity* pCity = pPlot->getPlotCity();
				if (pCity != NULL && pCity->getOwner() != pUnit->getOwner())
				{
					iTemp *= 2;
				}
			}
		}
		iValue += iTemp;
	}

	//#45 Effects for Promotions that give investigation...
	iTemp = kPromotion.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		//TB: Do I need another AI for just this?  hmm...
		//Perhaps work in some temporary situational value at least since buildups will likely apply
		//And only the best at this on the tile will make a difference.
		if (eUnitAI == UNITAI_PROPERTY_CONTROL ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			iTemp *= 2;
			if (pUnit)
			{
				if (pPlot != NULL)
				{
					const CvCity* pCity = pPlot->getPlotCity();
					if (pCity != NULL)
					{
						int iNumCriminals = pPlot->getNumCriminals();
						if (eUnitAI == UNITAI_INVESTIGATOR)
						{
							iNumCriminals += 10;
						}
						iTemp += iTemp * (iNumCriminals * iNumCriminals);
					}
				}
			}
			iValue += iTemp;
		}
	}

	//#46 Effects for Promotions for assassins...
	if (kPromotion.hasSkill(CLS_SKILL_ASSASSIN))
	{
		// Worth more to a unit that does not already have it than to one that does.
		int iTemp2 = pUnit ? (pUnit->isAssassin() ? 10 : 30) : 5;

		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp2 *= 2;
		}
		iValue += iTemp2;
	}

	//#47 Effects for Promotions that affects Visibility...
	iTemp = kPromotion.getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) / VISION_OPEN_GROUND_COST;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_SEE_INVISIBLE) ||
			(eUnitAI == UNITAI_SEE_INVISIBLE_SEA))
		{
			iValue += (iTemp * 100); //50
		}
		else if ((eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iValue += (iTemp * 40);
		}
		else if (eUnitAI == UNITAI_PIRATE_SEA)
		{
			iValue += (iTemp * 20);
		}
	}

	//#48 Effects for Promotions that affects Moves in general...
	iTemp = kPromotion.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ASSAULT_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_HEALER) ||
				(eUnitAI == UNITAI_HEALER_SEA) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 40);
		}
		else
		{
			iValue += (iTemp * 25);
		}
	}

	//#49 Effects for Promotions that Give Moves discount...
	iTemp = kPromotion.getMovement(MOVEMENT_MOVE_DISCOUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HEALER_SEA) ||
			(eUnitAI == UNITAI_ESCORT) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 20);
		}
		else
		{
			iValue += (iTemp * 10);
		}
	}

	//#50 Effects for Promotions that Give extra Air Range...
	iTemp = kPromotion.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR ||
			eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 20);
		}
		else if (eUnitAI == UNITAI_DEFENSE_AIR)
		{
			iValue += (iTemp * 10);
		}
	}

	//#51 Effects for Promotions that Give Air interceptions...
	iTemp = kPromotion.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_DEFENSE_AIR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_ESCORT_SEA)
		{
			iValue += (iTemp * 4);
		}
		else if (eUnitAI == UNITAI_CITY_SPECIAL || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	//#52 Effects for Promotions that Give Air Evasion...
	iTemp = kPromotion.getAir(AIR_EVASION, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	//#53 Effects for Promotions that Give 1st strikes...
	iTemp = kPromotion.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 * 2;
	iTemp += (kPromotion.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 25;
			iExtra = pUnit == NULL ? (kUnit.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) + (kUnit.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) * 2 : (pUnit->resolvedValue(URS_FIRST_STRIKE_CHANCE) / 100) + (pUnit->resolvedValue(URS_FIRST_STRIKES) / 100) * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp * 5);
		}
	}


	//#54 Effects for Promotions that Give Stealth strikes...
	iTemp = (kPromotion.getFlatCombat(COMBAT_STEALTH_STRIKES, CASC_SCOPE_UNIT) / 100) * 2;
	int iInvisFactor = 0;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				// ONE concealment magnitude ([vision.md] §4) -- summing a per-INVISIBLE_* table counted the
				// same concealment once per method.
				iInvisFactor += pUnit->concealment() / 100;
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp *= 25;
			iExtra = pUnit == NULL ? (kUnit.getFlatCombat(COMBAT_STEALTH_STRIKES, CASC_SCOPE_UNIT) / 100) * 2 : pUnit->stealthStrikesTotal() * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;

			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	//#55 Effects for Promotions that Give Withdrawal...
	iTemp = kPromotion.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp != 0)
	{
		iExtra = (kUnit.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) + (pUnit == NULL ? 0 : pUnit->resolvedValue(URS_WITHDRAWAL) * 4));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_EXPLORE)
		{
			iValue += (iTemp * 4) / 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_PILLAGE) ||
			  (eUnitAI == UNITAI_INFILTRATOR) ||
			  (pUnit != NULL && pUnit->getLeaderUnitType() != NO_UNIT))
		{
			iValue += iTemp * 1;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	//TB Combat Mods Begin
	iTemp = kPromotion.getCombatModifier(COMBAT_DAMAGE_MODIFIER, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_DAMAGE_MODIFIER, CASC_SCOPE_UNIT) : pUnit->damageModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	if (!bNoDefensiveBonus)
	{
		{
			iTemp = kPromotion.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT);
			if (iTemp != 0)
			{
				iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_CITY_DEFENSE) : kUnit.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT)));

				if (pPlot && pPlot->isCity(true))
				{
					iTemp *= 2;
				}

				if (eUnitAI == UNITAI_CITY_DEFENSE
				||  eUnitAI == UNITAI_CITY_SPECIAL
				||  eUnitAI == UNITAI_CITY_COUNTER)
				{
					iValue += iTemp / 30;
				}
				else if (
				    eUnitAI == UNITAI_HEALER
				||  eUnitAI == UNITAI_PROPERTY_CONTROL
				||  eUnitAI == UNITAI_INVESTIGATOR)
				{
					iValue += iTemp / 300;
				}
				else iValue += iTemp / 800;
			}

			iTemp = kPromotion.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT);
			if (iTemp != 0)
			{
				iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_HILLS_DEFENSE) : kUnit.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT)));

				if (pPlot && pPlot->isHills())
				{
					iTemp *= 2;
				}

				if (eUnitAI == UNITAI_CITY_DEFENSE
				||  eUnitAI == UNITAI_CITY_SPECIAL
				||  eUnitAI == UNITAI_CITY_COUNTER)
				{
					if (pPlot && pPlot->isCity(true))
					{
						iValue += iTemp / 100;
					}
					else iValue += iTemp / 300;
				}
				else if (
				    eUnitAI == UNITAI_COUNTER
				||  eUnitAI == UNITAI_ESCORT
				||  eUnitAI == UNITAI_HUNTER_ESCORT)
				{
					iValue += iTemp / 500;
				}
				else iValue += iTemp / 1000;
			}
		}
	}

	// The promotion's OWN flanking entries, keyed by UNITCOMBAT ([json.md] §6) -- not a sweep of the whole
	// unit-combat registry asking whether this promotion deposits onto each id.
	{
		const int iFlankingSeg = InfoValuation::keyedTargetSegment("flanking");
		std::vector<std::pair<int, int> > flankingRows;
		InfoValuation::collectKeyedTarget(
			kPromotion.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT, iFlankingSeg, flankingRows);

		for (size_t iRow = 0; iRow < flankingRows.size(); ++iRow)
		{
			const UnitCombatTypes eFlanked = (UnitCombatTypes)flankingRows[iRow].first;
			iTemp = flankingRows[iRow].second;

			if (iTemp != 0)
			{
				iTemp *= 100 + 2 * (InfoValuation::keyedTarget(
						kUnit.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT, iFlankingSeg, eFlanked)
					+ (pUnit ? pUnit->getExtraFlankingStrengthbyUnitCombatType(eFlanked) : 0));

				if (eUnitAI == UNITAI_COUNTER || eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY)
				{
					iValue += iTemp / 125;
				}
				else iValue += iTemp / 1000;
			}
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_UNNERVE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_UNNERVE) : kUnit.getCombatModifier(COMBAT_UNNERVE, CASC_SCOPE_UNIT)));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 200;
		}
		else iValue += iTemp / 800;
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_ENCLOSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_ENCLOSE) : kUnit.getCombatModifier(COMBAT_ENCLOSE, CASC_SCOPE_UNIT)));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 20;
		}
		else iValue += iTemp / 100;
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_LUNGE) : kUnit.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT)));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*((pUnit ? pUnit->resolvedValue(URS_DYNAMIC_DEFENSE) : kUnit.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT)));

		if (eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_HEALER
		||  eUnitAI == UNITAI_PROPERTY_CONTROL
		||  eUnitAI == UNITAI_RESERVE_SEA
		||  eUnitAI == UNITAI_ESCORT_SEA
		||  eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}


	// Skills are GRANT-ONLY ([skills.md] par.4), so a promotion can no longer take stampede AWAY -- only the
	// grant side has a successor to read.
	if (kPromotion.hasSkill(CLS_SKILL_STAMPEDE))
	{
		iValue -= 25;
	}





	//TB Combat Mods End

	iTemp = kPromotion.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->resolvedValue(URS_COLLATERAL) : kUnit.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT));
		iTemp /= 100;

		if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += iTemp * 2;
		}
		else if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += iTemp / 2;
		}
		else iValue += iTemp / 6;
	}

	iTemp = kPromotion.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	//Breakdown
	iTemp = kPromotion.getFlatCombat(COMBAT_BREAKDOWN_CHANCE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canAttack())
		{
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iTemp += (iTemp * 10);
			}
			else
			{
				iTemp += (iTemp / 8);
			}
			iTemp *= pUnit->breakdownDamageTotal();
		}
	}
	iValue += iTemp;

	iTemp = kPromotion.getFlatCombat(COMBAT_BREAKDOWN_DAMAGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canAttack())
		{
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iTemp += (iTemp * 100);
			}
			else
			{
				iTemp += (iTemp / 8);
			}
			iTemp *= pUnit->breakdownChanceTotal();
			iTemp /= 100;
		}
	}
	iValue += iTemp;

	iTemp = kPromotion.getFlatCombat(COMBAT_TAUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->tauntTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += (iTemp * 25);
			}
			else
			{
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = kPromotion.getSizeMatters().sizeModifier;
	if (iTemp != 0)
	{
		iValue += iTemp * 2;
	}

	iTemp = kPromotion.getSizeMatters().quality;
	if (iTemp != 0)
	{
		iValue += iTemp * 200;
	}

	iTemp = kPromotion.getSizeMatters().group;
	if (iTemp != 0)
	{
		iValue += iTemp * 200;
	}

	iTemp = kPromotion.getCommandRange();
	if (iTemp != 0)
	{
		iValue += iTemp * 25;
	}
	//end mod

	//@MOD Commanders: control points promotion AI value
	if (pUnit)
	{
		iTemp = kPromotion.getControlPoints();
		if (iTemp != 0)
		{
			iValue += iTemp * (100 + 25 * pPlot->getNumUnits());
		}
	}
	//end mod

	iTemp = kPromotion.getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		//@MOD Commanders: combat promotion value
		if (eUnitAI == UNITAI_GENERAL)
		{
			iValue += (iTemp * 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else if (eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iInvisFactor = 0;
	iTemp = kPromotion.getCombatModifier(COMBAT_STEALTH, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				// ONE concealment magnitude ([vision.md] §4) -- summing a per-INVISIBLE_* table counted the
				// same concealment once per method.
				iInvisFactor += pUnit->concealment() / 100;
			}
		}
		else
		{
			iInvisFactor = 1;
		}

		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR) ||
				(eUnitAI == UNITAI_PILLAGE))
		{
			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	//TB Combat Mod
	iTemp = kPromotion.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		iValue += (iTemp * 200);
	}

	int iRank = 0;
	iTemp = kPromotion.getSizeMatters().combatModifierPerSizeMore;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->sizeRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getSizeMatters().combatModifierPerSizeLess;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->sizeRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getSizeMatters().combatModifierPerVolumeMore;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->groupRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getSizeMatters().combatModifierPerVolumeLess;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->groupRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	if (pUnit)
	{
		if (pUnit->isAnimal() && kPromotion.hasSkill(CLS_SKILL_ANIMAL_IGNORES_BORDERS) && !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
		{
			iValue += 50;
		}
	}

	if (kPromotion.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
	{
		if (bNoDefensiveBonus)
		{
			iValue -= 5;
		}
		else iValue -= 50;
	}

	if (kPromotion.hasSkill(CLS_SKILL_ONSLAUGHT))
	{
		iValue += 75;
	}

	if (kPromotion.hasSkill(CLS_SKILL_ATTACK_ONLY_CITIES))
	{
		if (pUnit)
		{
			if (pUnit->canAttack())
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue -= 10;
				}
				else
				{
					iValue -= 50;
				}
			}
		}
	}

	if (kPromotion.hasSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL))
	{
		if (pUnit)
		{
			if (pUnit->canAttack() && !pUnit->canAttackOnlyCities())
			{
				if (pUnit->canIgnoreNoEntryLevel())
				{
					iValue += 5;
				}
				else
				{
					iValue += 20;
				}
			}
		}
	}

	if (pUnit != NULL && GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL))
	{
		if (kPromotion.hasSkill(CLS_SKILL_IGNORE_ZONE_OF_CONTROL))
		{
			if (pUnit->canIgnoreZoneofControl())
			{
				iValue += 5;
			}
			else
			{
				iValue += 25;
			}
		}
	}

	if (kPromotion.hasSkill(CLS_SKILL_FLIES_TO_MOVE))
	{
		if (pUnit)
		{
			if (pUnit->canFliesToMove())
			{
				iValue += 5;
			}
			else
			{
				iValue += 100;
			}
		}
	}

	//TB Combat Mods
	//TB Modification note:adjusted City Attack promo value to balance better against withdraw promos for city attack ai units.
	iTemp = kPromotion.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iTemp *= 100 + (pUnit == NULL ? kUnit.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT) : 2 * pUnit->resolvedValue(URS_CITY_ATTACK) - kUnit.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT));
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
			{
				iValue += iTemp * 4;
			}
			else iValue += iTemp;
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_HILLS_ATTACK, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->resolvedValue(URS_HILLS_ATTACK) : kUnit.getCombatModifier(COMBAT_HILLS_ATTACK, CASC_SCOPE_UNIT));
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_COUNTER)
		{
			iValue += iTemp / 4;
		}
		else iValue += iTemp / 16;
	}


	//WorkRateMod
	iTemp = kPromotion.getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);

	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kPromotion.getCapture(CAPTURE_PROBABILITY, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_INVESTIGATOR || pUnit && pUnit->canAttack())
		{
			iValue += iTemp * 3;
		}
		else if (pUnit && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else
		{
			iValue += 2*(iTemp > 0) - 1;
		}
	}

	iTemp = kPromotion.getCapture(CAPTURE_RESISTANCE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canFight() || eUnitAI == UNITAI_INVESTIGATOR || eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp;
		}
		else iValue += 2*(iTemp > 0) - 1;
	}

	iTemp = kPromotion.getScalar(SCALAR_REVOLT_PROTECTION, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_CITY_DEFENSE
		||	eUnitAI == UNITAI_CITY_COUNTER
		||	eUnitAI == UNITAI_CITY_SPECIAL
		||	eUnitAI == UNITAI_PROPERTY_CONTROL
		||	eUnitAI == UNITAI_INVESTIGATOR)
		{
			if (pUnit != NULL && pPlot != NULL && pUnit->getX() != INVALID_PLOT_COORD && pPlot->isCity())
			{
				PlayerTypes eCultureOwner = pPlot->calculateCulturalOwner();
				// High weight for cities being threatened with culture revolution
				if (eCultureOwner != NO_PLAYER && GET_PLAYER(eCultureOwner).getTeam() != getTeam())
				{
					iValue += iTemp * 15;
				}
			}
		}
	}

	iTemp = kPromotion.getCollateralModifier(COLLATERAL_PROTECTION, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL))
		{
			iValue += (iTemp / 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kPromotion.getScalar(SCALAR_PILLAGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_PIRATE_SEA ||
			eUnitAI == UNITAI_INFILTRATOR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	// costs.upgrade is sign-NORMALIZED as a COST modifier, so a discount authors NEGATIVE. The AI's benefit
	// is the reduction, so negate at the read rather than inverting the uses below.
	iTemp = -kPromotion.getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iValue += (iTemp / 4);
	}

	iTemp = kPromotion.getExperienceModifier(EXPERIENCE_AMOUNT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA))
		{
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp / 2);
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_KAMIKAZE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp / 16);
		}
		else
		{
			iValue += (iTemp / 64);
		}
	}

	int iPass = 0;
	int iTempValue = 0;
	for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
	{
		//Team Project (4)
		//WorkRateMod
		iTemp = InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, 0, InfoValuation::keyedTargetSegment("builds"), iI);
		if (iTemp != 0)
		{
			iPass++;
			if (eUnitAI == UNITAI_WORKER)
			{
				int iBuildWeight = 1000;
				//ls612: Weight values by Terrain in area
				if (pUnit)
				{
					//Still more to be done on this.
					//We should be factoring in how many of this build is still needed in the nation
					//only giving consideration to builds currently available to the unit at this time
					//obviously currently needed routes are going to have a lot of weight here if such need exists.
					//@ls612: You've done great work on the ai and I don't want to intrude on this any further than to
					//have simply set up the build rate modifier as a clean example of the unit/promo tag setup chain.
					iBuildWeight = 1000;
				}
				else
				{
					iBuildWeight = 100;
				}
				iTempValue += (iBuildWeight * iTemp) / 1000;
			}
			else
			{
				iTempValue++;
			}
		}
	}
	if (iPass > 0)
	{
		iTempValue /= iPass;
	}
	iValue += iTempValue;

	{
		int iPass = 0;
		int iTempValue = 0;
		for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
		{
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_ATTACK)
			||  InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_DEFENSE)
			||  InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, 0, InfoValuation::keyedTargetSegment("terrain"), iI)
			||  InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_MOVEMENT, 0, InfoValuation::keyedTargetSegment("terrain"), iI))
			{
				iPass++;
				const int iTerrainWeight = (
					pUnit
					?
					1000
					*
					pUnit->area()->getNumRevealedTerrainTiles(getTeam(), (TerrainTypes)iI)
					/
					std::max(1, pUnit->area()->getNumRevealedTiles(getTeam()))
					:
					1000
				);
				const bool bOnTerrain = pPlot && pPlot->getTerrainType() == iI;

				iTemp = InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_ATTACK);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraTerrainAttackPercent((TerrainTypes)iI) : InfoValuation::keyedCombat(kUnit.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_ATTACK));

					if (bOnTerrain)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					if (eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_COUNTER
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_GREAT_HUNTER)
					{
						iTempValue += iTemp * iTerrainWeight / 1000;
					}
					else
					{
						iTempValue += iTemp * iTerrainWeight / 1400;
					}
				}

				if (!bNoDefensiveBonus)
				{
					iTemp = InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_DEFENSE);
					if (iTemp != 0)
					{
						iTemp *= 100 + 2*(pUnit ? pUnit->getExtraTerrainDefensePercent((TerrainTypes)iI) : InfoValuation::keyedCombat(kUnit.getModifiers(), InfoValuation::COMBAT_TARGET_TERRAIN, iI, COMBAT_DEFENSE));

						if (bOnTerrain)
						{
							iTemp /= 50;
						}
						else iTemp /= 100;

						if (eUnitAI == UNITAI_COUNTER
						||	eUnitAI == UNITAI_ESCORT
						||	eUnitAI == UNITAI_HUNTER
						||	eUnitAI == UNITAI_HUNTER_ESCORT
						||	eUnitAI == UNITAI_EXPLORE
						||	eUnitAI == UNITAI_GREAT_HUNTER)
						{
							iTempValue += iTemp * iTerrainWeight / 1000;
						}
						else
						{
							iTempValue += iTemp * iTerrainWeight / 1400;
						}
					}
				}

				if (InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_MOVEMENT, 0, InfoValuation::keyedTargetSegment("terrain"), iI))
				{
					iTemp = iMoves * (bOnTerrain ? 80 : 40);

					if (eUnitAI == UNITAI_EXPLORE
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iTerrainWeight / 200;
					}
					else if (
					    eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_COUNTER)
					{
						iTempValue += iTemp * iTerrainWeight / 500;
					}
					else iTempValue += iTemp * iTerrainWeight / 1000;
				}

				iTemp = InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, 0, InfoValuation::keyedTargetSegment("terrain"), iI);
				if (iTemp != 0)
				{
					if (eUnitAI == UNITAI_WORKER)
					{
						if (bOnTerrain)
						{
							iTemp *= 2;
						}
						iTempValue += iTemp * iTerrainWeight / 100;
					}
					else iTempValue++;
				}

			}
		}
		if (iPass > 0)
		{
			iValue += iTempValue / iPass;
		}
	}

	{
		int iPass = 0;
		int iTempValue = 0;
		for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
		{
			if (InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_ATTACK)
			||  InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_DEFENSE)
			||  InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, 0, InfoValuation::keyedTargetSegment("feature"), iI)
			||  InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_MOVEMENT, 0, InfoValuation::keyedTargetSegment("feature"), iI))
			{
				iPass++;
				const int iFeatureWeight = (
					pUnit
					?
					1000
					*
					pUnit->area()->getNumRevealedFeatureTiles(getTeam(), (FeatureTypes)iI)
					/
					std::max(1, pUnit->area()->getNumRevealedTiles(getTeam()))
					:
					1000
				);
				const bool bOnFeature = pPlot && pPlot->getFeatureType() == iI;

				iTemp = InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_ATTACK);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFeatureAttackPercent((FeatureTypes)iI) : InfoValuation::keyedCombat(kUnit.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_ATTACK));

					if (bOnFeature)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					if (eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_COUNTER
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iFeatureWeight / 1000;
					}
					else iTempValue += iTemp * iFeatureWeight / 1400;
				}

				if (!bNoDefensiveBonus)
				{
					iTemp = InfoValuation::keyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_DEFENSE);
					if (iTemp != 0)
					{
						iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFeatureDefensePercent((FeatureTypes)iI) : InfoValuation::keyedCombat(kUnit.getModifiers(), InfoValuation::COMBAT_TARGET_FEATURE, iI, COMBAT_DEFENSE));

						if (bOnFeature)
						{
							iTemp /= 50;
						}
						else iTemp /= 100;

						if (eUnitAI == UNITAI_COUNTER
						||	eUnitAI == UNITAI_EXPLORE
						||	eUnitAI == UNITAI_HUNTER
						||	eUnitAI == UNITAI_HUNTER_ESCORT
						||	eUnitAI == UNITAI_GREAT_HUNTER
						||	eUnitAI == UNITAI_ESCORT)
						{
							iTempValue += iTemp * iFeatureWeight / 1000;
						}
						else iTempValue += iTemp * iFeatureWeight / 1400;
					}
				}

				if (InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_MOVEMENT, 0, InfoValuation::keyedTargetSegment("feature"), iI))
				{
					iTemp = iMoves * (bOnFeature ? 80 : 40);

					if (eUnitAI == UNITAI_EXPLORE
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iFeatureWeight / 200;
					}
					else if (
					    eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_COUNTER)
					{
						iTempValue += iTemp * iFeatureWeight / 500;
					}
					else iTempValue += iTemp * iFeatureWeight / 1000;
				}

				//ls612: Terrain Work Modifiers //TB Edited for WorkRateMod (THANK you for thinking this out ls!)
				iTemp = InfoValuation::keyedTarget(kPromotion.getModifiers(), MODFAM_WORK_RATE, 0, InfoValuation::keyedTargetSegment("feature"), iI);
				if (iTemp != 0)
				{
					if (eUnitAI == UNITAI_WORKER)
					{
						if (bOnFeature)
						{
							iTemp *= 2;
						}
						iTempValue += iTemp * iFeatureWeight / 100;
					}
					else iTempValue++;
				}
			}
		}
		if (iPass > 0)
		{
			iValue += iTempValue / iPass;
		}
	}


	if ((pUnit && pUnit->canFight() || !pUnit && (kUnit.getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) > 0))
	{
		// The PROMOTION's own authored vs-unitcombat entries drive this term: where it deposits nothing the
		// value is zero on both branches, so the handful it authored replaces the whole unitcombat registry.
		std::vector<std::pair<int, int> > promotionVsCombat;
		InfoValuation::collectKeyedCombat(kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT,
			COMBAT_AMOUNT, promotionVsCombat);

		foreach_(const STD_PAIR(int, int)& combatEntry, promotionVsCombat)
		{
			const int iUnitCombat = combatEntry.first;
			// A unit that is not already WEAK against this class values the promotion double.
			const bool bNotWeakAgainst = pUnit
				? pUnit->unitCombatModifier((UnitCombatTypes)iUnitCombat) >= 0
				: InfoValuation::keyedCombat(kUnit.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT,
					iUnitCombat, COMBAT_AMOUNT) >= 0;

			iValue += bNotWeakAgainst ? combatEntry.second * 2 : combatEntry.second;
		}
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		iTemp = InfoValuation::keyedCombat(
			kPromotion.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, iI, COMBAT_AMOUNT);
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER)
			{
				iValue += (iTemp * 1);
			}
			else if ((eUnitAI == UNITAI_ATTACK) ||
					   (eUnitAI == UNITAI_RESERVE))
			{
				iValue += (iTemp / 2);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	// The concealment-vs-detection CONTEST ([vision.md] par.4) -- its OWN block and its OWN evaluation, never
	// folded into `vision`, which answers only how FAR you see. The collapsed data carries ONE concealment
	// magnitude plus a detection ROW per method answered, so this is TWO terms: the per-INVISIBLE_* intensity
	// tables, their per-substrate terrain/feature/improvement variants and the second RANGE system that ran
	// beside vision's are all retired, and summing a per-type table would have counted one concealment once
	// per method. Values are x100 at the info ([DEC-fixedpoint-x100]), reduced here at the point of use.
	// The UnitAI groupings are carried over as-is; the contest's real evaluation is its own piece of work, so
	// this deliberately UNDER-values rather than guessing a tuned model (owner).
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		const CvHideAndSeekSection& kHideAndSeek = kPromotion.getHideAndSeek();

		iTemp = kHideAndSeek.concealment / 100;
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_ANIMAL ||
				eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_PIRATE_SEA ||
				eUnitAI == UNITAI_ATTACK_SEA ||
				eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
				eUnitAI == UNITAI_HUNTER ||
				eUnitAI == UNITAI_GREAT_HUNTER ||
				eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
				eUnitAI == UNITAI_INFILTRATOR)
			{
				iValue += iTemp * 30;
			}
			else iValue += iTemp * 5;
		}

		int iDetectionTotal = 0;
		for (size_t iRow = 0; iRow < kHideAndSeek.detection.size(); ++iRow)
		{
			iDetectionTotal += kHideAndSeek.detection[iRow].value;
		}
		iTemp = iDetectionTotal / 100;
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
				eUnitAI == UNITAI_PILLAGE_COUNTER)
			{
				iValue += iTemp * 350;
			}
			else if (eUnitAI == UNITAI_COUNTER ||
				eUnitAI == UNITAI_ANIMAL ||
				eUnitAI == UNITAI_ESCORT_SEA ||
				eUnitAI == UNITAI_HUNTER_ESCORT ||
				eUnitAI == UNITAI_PROPERTY_CONTROL ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += iTemp * 15;
			}
			else iValue += iTemp * 10;
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->resolvedValue(URS_COMBAT_ATTACK) : kUnit.getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT));
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ANIMAL))
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->resolvedValue(URS_COMBAT_DEFENSE) : kUnit.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT));
		iTemp /= 100;

		if (bForBuildUp
		|| eUnitAI == UNITAI_RESERVE
		|| eUnitAI == UNITAI_CITY_DEFENSE
		|| eUnitAI == UNITAI_ESCORT_SEA
		|| eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp * (2 + bForBuildUp);
		}
		else iValue += iTemp;
	}

	if (!isNPC())
	{
		iTemp = kPromotion.getCombatModifier(COMBAT_VS_BARBS, CASC_SCOPE_UNIT);
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER
			||  eUnitAI == UNITAI_CITY_DEFENSE
			||  eUnitAI == UNITAI_CITY_COUNTER
			||  eUnitAI == UNITAI_ESCORT_SEA
			||  eUnitAI == UNITAI_EXPLORE_SEA
			||  eUnitAI == UNITAI_EXPLORE
			||  eUnitAI == UNITAI_PILLAGE_COUNTER
			||  eUnitAI == UNITAI_HUNTER
			||  eUnitAI == UNITAI_HUNTER_ESCORT
			||  eUnitAI == UNITAI_GREAT_HUNTER
			||  eUnitAI == UNITAI_ESCORT)
			{
				iTemp *= 10 - getCurrentEra();
				iValue += iTemp / 9;
			}
		}
	}

	iTemp = kPromotion.getCombatModifier(COMBAT_RELIGIOUS, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->resolvedValue(URS_RELIGIOUS_COMBAT) : kUnit.getCombatModifier(COMBAT_RELIGIOUS, CASC_SCOPE_UNIT));
		iTemp /= 100;

		if (eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_COLLATERAL
		||  eUnitAI == UNITAI_PILLAGE
		||  eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_ATTACK_SEA
		||  eUnitAI == UNITAI_RESERVE_SEA
		||  eUnitAI == UNITAI_ASSAULT_SEA
		||  eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iValue += 2 * iTemp;
		}
	}

	// TB Combat Mods Begin
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL) && kPromotion.hasSkill(CLS_SKILL_ZONE_OF_CONTROL))
	{
		iValue += 250;
	}

	for (int iI = 0; iI < (int)kPromotion.providesUnitCombats().size(); iI++)
	{
		if (pUnit == NULL)
		{
			if (!kUnit.hasCombatClass((UnitCombatTypes)kPromotion.providesUnitCombats()[iI]))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.providesUnitCombats()[iI], eUnit, pUnit, eUnitAI);
			}
		}
		else
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.providesUnitCombats()[iI]))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.providesUnitCombats()[iI], eUnit, pUnit, eUnitAI);
			}
		}
	}

	{
		const std::vector<int>& strippedCombats = kPromotion.removesUnitCombats();
		for (size_t iAt = 0; iAt < strippedCombats.size(); ++iAt)
		{
			const UnitCombatTypes eStripped = (UnitCombatTypes)strippedCombats[iAt];

			if (pUnit ? pUnit->isHasUnitCombat(eStripped) : kUnit.hasCombatClass(eStripped))
			{
				iValue -= AI_unitCombatValue(eStripped, eUnit, pUnit, eUnitAI);
			}
		}
	}

	if (pUnit != NULL && pUnit->canAcquirePromotion(ePromotion))
	{
		iValue = std::max(1, iValue);
	}
	int iRandom = 0;
	if (pUnit != NULL && iValue > 0 && !bForBuildUp)
	{
		//GC.getGame().logOOSSpecial(2,iValue, pUnit->getID());
		int iRandomRange = (int(50) + (iValue / 10)); //Calvitix introduce more Random for High Values
		iRandom = GC.getGame().getSorenRandNum(iRandomRange, "AI Unit Promote");
		if (iValue > 75)
		{ //to permit the lower of value with random
			iRandom -= iRandomRange / 2;
		}
	}

	iValue += iRandom;



	return iValue;
}
//TB UNITCOMBAT AI

int CvPlayerAI::AI_unitCombatValue(UnitCombatTypes eUnitCombat, UnitTypes eUnit, const CvUnit* pUnit, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	int iTemp;
	int iExtra;

	int iValue = 0;

	const CvUnitCombatInfo& kUnitCombat = GC.getUnitCombatInfo(eUnitCombat);
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

	const int iMoves = pUnit ? pUnit->maxMoves() : (kUnit.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100);

	if (eUnitAI == NO_UNITAI)
	{
		eUnitAI = kUnit.getDefaultUnitAI();
	}

	if (kUnit.hasTag(CLS_TAG_SPY))
	{

		//Readjust promotion choices favouring security, deception, logistics, escape, improvise,
		//filling in other promotions very lightly because the AI does not yet have situational awareness
		//when using spy promotions at the moment of mission execution.

			//Logistics
			//I & III
		if (kUnitCombat.hasSkill(CLS_SKILL_ENEMY_ROUTE)) iValue += 20;

		//Security
		iValue += kUnitCombat.getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) * 10 / VISION_OPEN_GROUND_COST;
		//Lean towards more security if security is already present
		iValue += (pUnit == NULL ? kUnit.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT) : pUnit->currInterceptionProbability());
		//total 20, 30, 40 points

		//Escape
		if (kUnitCombat.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT))
		{
			iValue += 30;
		}

		//Loyalty
		if (kUnitCombat.hasSkill(CLS_SKILL_ALWAYS_HEAL))
		{
			iValue += 15;
		}

		//Instigator
		//I & II
		if (kUnitCombat.getFlatHeal(HEAL_ENEMY_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}
		//III
		if (kUnitCombat.getFlatHeal(HEAL_NEUTRAL_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}

		//Alchemist
		if (kUnitCombat.getFlatHeal(HEAL_FRIENDLY_TERRITORY, CASC_SCOPE_UNIT) / 100)
		{
			iValue += 15;
		}

		return iValue;
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_BLITZ))
	{
		//ls612: AI to know that Blitz is only useful on units with more than one move now that the filter is gone
		if (iMoves > 1)
		{
			if ((eUnitAI == UNITAI_RESERVE ||
				eUnitAI == UNITAI_ATTACK ||
				eUnitAI == UNITAI_ATTACK_CITY ||
				eUnitAI == UNITAI_PARADROP))
			{
				iValue += (10 * iMoves);
			}
			else
			{
				iValue += 2;
			}
		}
		else
		{
			iValue += 0;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_ONE_UP))
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 20;
		}
		else
		{
			iValue += 5;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_DEFENSIVE_VICTORY_MOVE))
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK))
		{
			iValue += 10;
		}
		else
		{
			iValue += 8;
		}
	}

	if (pUnit && pUnit->noDefensiveBonus())
	{
		iValue *= 100;
	}

	iTemp = 0;
	if (kUnitCombat.hasSkill(CLS_SKILL_FREE_DROP))
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_OFFENSIVE_VICTORY_MOVE))
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}
	}

	if (pUnit)
	{
		if (pUnit->isBlitz() || pUnit->isFreeDrop())
		{
			iTemp *= 20;
		}
	}
	iValue += iTemp;

	iTemp = 0;
	if (kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ESPIONAGE))
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if ((eUnitAI == UNITAI_ATTACK) ||
						(eUnitAI == UNITAI_PARADROP) ||
						(eUnitAI == UNITAI_INFILTRATOR))
				{
					iTemp += 8;
				}
				else
				{
					iTemp += 4;
				}
			}
			else
			{
				iTemp += 2;
			}
		}
		else
		{
			iTemp += 2;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_MARAUDER))
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_PARADROP || eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 8;
				}
				else iTemp += 4;
			}
			else iTemp += 2;
		}
		else iTemp += 2;
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ON_MOVE))
	{
		if (pUnit)
		{
			if (pUnit->isPillageEspionage() || pUnit->isPillageMarauder() || pUnit->isPillageResearch())
			{
				if (eUnitAI == UNITAI_PILLAGE
				|| eUnitAI == UNITAI_ATTACK
				|| eUnitAI == UNITAI_PARADROP
				|| eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 10;
				}
				else iTemp += 8;
			}
			else iTemp++;
		}
		else iTemp++;
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_ON_VICTORY))
	{
		if (pUnit)
		{
			if (pUnit->isPillageEspionage() || pUnit->isPillageMarauder() || pUnit->isPillageResearch())
			{
				if (eUnitAI == UNITAI_PILLAGE
				|| eUnitAI == UNITAI_ATTACK
				|| eUnitAI == UNITAI_PARADROP
				|| eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 20;
				}
				else iTemp += 12;
			}
			else iTemp += 4;
		}
		else iTemp += 4;
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_PILLAGE_RESEARCH))
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if ((eUnitAI == UNITAI_ATTACK) ||
						(eUnitAI == UNITAI_PARADROP) ||
						(eUnitAI == UNITAI_INFILTRATOR))
				{
					iTemp += 8;
				}
				else
				{
					iTemp += 4;
				}
			}
			else
			{
				iTemp += 2;
			}
		}
		else
		{
			iTemp += 2;
		}
	}

	if (pUnit)
	{
		if (pUnit->maxMoves() > 1)
		{
			iTemp *= 100;
		}
	}
	iValue += iTemp;


	iTemp = (kUnitCombat.hasSkill(CLS_SKILL_CELEBRITY) ? 1 : 0);
	int iTempTemp = 0;
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER))
		{
			if (pUnit)
			{
				CvPlot* pUnitPlot = pUnit->plot();
				CvCity* pCity = pUnitPlot->getPlotCity();
				if (pCity != NULL)
				{
					if (pCity->netHappiness() < 0)
					{
						iTempTemp = (iTemp * 5);
					}
					else
					{
						iTempTemp = (iTemp / 100);
					}
				}
			}
			else
			{
				iTempTemp /= 100;
			}
		}
		else
		{
			iTempTemp /= 100;
		}
		iValue += iTempTemp;
	}

	iTemp = kUnitCombat.getFlatCollateral(COLLATERAL_LIMIT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getFlatCollateral(COLLATERAL_MAX_UNITS, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (15 * iTemp);
		}
	}

	iTemp = kUnitCombat.getFlatCombat(COMBAT_LIMIT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				iTemp *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iTemp /= 4;
				}
				iValue += iTemp;
			}
			else
			{
				int iCombatLimit = pUnit->combatLimit();
				int iCombatLimitValue = iCombatLimit - 100;
				int iCombatLimitValueProcessed = std::min(iTemp, iCombatLimitValue);
				iCombatLimitValueProcessed *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iCombatLimitValueProcessed /= 4;
				}

				iValue += iCombatLimitValueProcessed;
			}
		}
		else if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 3);
		}
	}

	iTemp = kUnitCombat.getMovement(MOVEMENT_DROP_RANGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 20 * iTemp;
		}
	}

	iTemp = kUnitCombat.getScalar(SCALAR_SURVIVOR, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 5 * iTemp;
		}
		else
		{
			iValue += 2 * iTemp;
		}
	}

	//TBHEAL review
	//isNoSelfHeal()
	if (kUnitCombat.hasSkill(CLS_SKILL_NO_SELF_HEAL))
	{
		if (pUnit)
		{
			if (!pUnit->hasNoSelfHeal())
			{
				iValue -= 50;
			}
		}
		else
		{
			iValue -= 25;
		}
	}

	iTemp = kUnitCombat.getSizeMatters().maxHP;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getExtraMaxHP();
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY_LEMMING) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER))
		{
			iTemp *= 15;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 10;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getHealModifier(HEAL_SELF_MODIFIER, CASC_SCOPE_UNIT);
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 10;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 5;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_ENEMY_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iTemp *= 4;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_NEUTRAL_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 3;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_FRIENDLY_TERRITORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_VICTORY, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp = 0;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 5;
		}

		iValue += iTemp;
	}


	CvPlot* pPlot = NULL;
	if (pUnit)
	{
		pPlot = pUnit->plot();
	}

	iTemp = (kUnitCombat.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100);
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 20;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 5;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_SAME_TILE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 6;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 2;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_ADJACENT_TILE, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 5;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_VICTORY_STACK, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = ((pUnit->resolvedValue(URS_HEAL_SAME_TILE) / 100) * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).hasSkill(CLS_SKILL_HEALS_AS))
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + (kUnitCombat.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100));
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFlatHeal(HEAL_VICTORY_ADJACENT, CASC_SCOPE_UNIT) / 100;
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = ((pUnit->resolvedValue(URS_HEAL_ADJACENT) / 100) * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).hasSkill(CLS_SKILL_HEALS_AS))
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + (kUnitCombat.getFlatHeal(HEAL_SUPPORT, CASC_SCOPE_UNIT) / 100));
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_ALWAYS_HEAL))
	{
		if ((eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 35;
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 15;
		}
		else
		{
			iValue += 8;
		}
	}
	//

	if (kUnitCombat.hasSkill(CLS_SKILL_AMPHIB))
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 25;
		}
		else
		{
			iValue++;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_RIVER))
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 15;
		}
		else
		{
			iValue++;
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_ENEMY_ROUTE))
	{
		if (eUnitAI == UNITAI_PILLAGE)
		{
			iValue += (50 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
				   (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (30 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (20 + (4 * iMoves));
		}
		else
		{
			iValue += (4 * iMoves);
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_HILLS_DOUBLE_MOVE))
	{
		if (eUnitAI == UNITAI_EXPLORE ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 50;
		}
		else
		{
			iValue += 10;
		}
	}

	if (kUnitCombat.getSkills()->has("canPassPeaks") && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 35;
	}
	if (kUnitCombat.getSkills()->has("canLeadThroughPeaks") && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 75;
	}

	if ((kUnitCombat.hasSkill(CLS_SKILL_IMMUNE_TO_FIRST_STRIKES) || kUnitCombat.hasSkill(CLS_SKILL_FIRST_STRIKE_IMMUNE))
		&& (pUnit == NULL || !pUnit->immuneToFirstStrikes()))
	{
		if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 25;
		}
		else if ((eUnitAI == UNITAI_ATTACK))
		{
			iValue += 15;
		}
		else
		{
			iValue += 10;
		}
	}

	iTemp = kUnitCombat.getUnderworld(UNDERWORLD_INSIDIOUSNESS, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		//TB Must update here as soon as I can improve on AI - not sure about pirates yet.
		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp *= 10;
		}
		else if (/*(eUnitAI == UNITAI_PIRATE_SEA) ||*/
			(eUnitAI == UNITAI_PILLAGE))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp *= -1;
		}
		if (pUnit)
		{
			if (pUnit->plot() != NULL)
			{
				CvCity* pCity = pUnit->plot()->getPlotCity();
				if (pCity != NULL && pCity->getOwner() != pUnit->getOwner())
				{
					iTemp *= 2;
				}
			}
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getUnderworld(UNDERWORLD_INVESTIGATION, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		//TB: Do I need another AI for just this?  hmm...
		//Perhaps work in some temporary situational value at least since buildups will likely apply
		//And only the best at this on the tile will make a difference.
		if (eUnitAI == UNITAI_PROPERTY_CONTROL ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			iTemp *= 2;
			if (pUnit)
			{
				if (pUnit->plot() != NULL)
				{
					CvCity* pCity = pUnit->plot()->getPlotCity();
					if (pCity != NULL)
					{
						int iNumCriminals = pUnit->plot()->getNumCriminals();
						if (eUnitAI == UNITAI_INVESTIGATOR)
						{
							iNumCriminals += 10;
						}
						iTemp += iTemp * (iNumCriminals * iNumCriminals);
					}
				}
			}
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getFlatVision(VISION_STRENGTH, CASC_SCOPE_UNIT) / VISION_OPEN_GROUND_COST;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_SEE_INVISIBLE) ||
			(eUnitAI == UNITAI_SEE_INVISIBLE_SEA))
		{
			iValue += (iTemp * 5);  //Calvitix origin 50
		}
		else if ((eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iValue += (iTemp * 4);  //Calvitix origin 40
		}
		else if (eUnitAI == UNITAI_PIRATE_SEA)
		{
			iValue += (iTemp * 2); //Calvitix origin 20
		}
	}

	iTemp = kUnitCombat.getMovement(MOVEMENT_MOVES, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ASSAULT_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_HEALER) ||
				(eUnitAI == UNITAI_HEALER_SEA) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 20);  //40
		}
		else
		{
			iValue += (iTemp * 8);  //25
		}
	}

	iTemp = kUnitCombat.getMovement(MOVEMENT_MOVE_DISCOUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HEALER_SEA) ||
			(eUnitAI == UNITAI_ESCORT) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 20);
		}
		else
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kUnitCombat.getAir(AIR_RANGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR ||
			eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 20);
		}
		else if (eUnitAI == UNITAI_DEFENSE_AIR)
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kUnitCombat.getAir(AIR_INTERCEPT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_DEFENSE_AIR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_ESCORT_SEA)
		{
			iValue += (iTemp * 4);
		}
		else if (eUnitAI == UNITAI_CITY_SPECIAL || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kUnitCombat.getAir(AIR_EVASION, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kUnitCombat.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100 * 2;
	iTemp += (kUnitCombat.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 25;
			iExtra = pUnit ? (pUnit->resolvedValue(URS_FIRST_STRIKE_CHANCE) / 100) + (pUnit->resolvedValue(URS_FIRST_STRIKES) / 100) * 2 : (kUnit.getScalar(SCALAR_FIRST_STRIKE_CHANCES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) + (kUnit.getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100) * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp * 5);
		}
	}


	iTemp = (kUnitCombat.getFlatCombat(COMBAT_STEALTH_STRIKES, CASC_SCOPE_UNIT) / 100) * 2;
	int iInvisFactor = 0;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				// ONE concealment magnitude ([vision.md] §4) -- summing a per-INVISIBLE_* table counted the
				// same concealment once per method.
				iInvisFactor += pUnit->concealment() / 100;
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp *= 25;
			iExtra = pUnit ? pUnit->stealthStrikesTotal() * 2 : (kUnit.getFlatCombat(COMBAT_STEALTH_STRIKES, CASC_SCOPE_UNIT) / 100) * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;

			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	iTemp = kUnitCombat.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp != 0)
	{
		iExtra = kUnit.getScalar(SCALAR_WITHDRAWAL, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT) + (pUnit ? pUnit->resolvedValue(URS_WITHDRAWAL) * 4 : 0);
		iTemp *= 100 + iExtra;
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_EXPLORE)
		{
			iValue += (iTemp * 4) / 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_PILLAGE) ||
			  (eUnitAI == UNITAI_INFILTRATOR) ||
			  (pUnit != NULL && pUnit->getLeaderUnitType() != NO_UNIT))
		{
			iValue += iTemp * 1;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	//TB Combat Mods Begin

	iTemp = kUnitCombat.getCombatModifier(COMBAT_DAMAGE_MODIFIER, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_DAMAGE_MODIFIER, CASC_SCOPE_UNIT) : pUnit->damageModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	// Flanking is keyed by UNITCOMBAT ([json.md] §6), and each entry carries its own target -- so the unit-side
	// reads key off THAT. ⚠ The legacy loop passed its own list INDEX as a UnitCombatTypes, so it scored one
	// entry against a different class's flanking strength whenever the two disagreed.
	{
		const int iFlankingSeg = InfoValuation::keyedTargetSegment("flanking");
		std::vector<std::pair<int, int> > flankingRows;
		InfoValuation::collectKeyedTarget(
			kUnitCombat.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT, iFlankingSeg, flankingRows);

		for (size_t iRow = 0; iRow < flankingRows.size(); ++iRow)
		{
			const UnitCombatTypes eFlanked = (UnitCombatTypes)flankingRows[iRow].first;
			iTemp = flankingRows[iRow].second;

			if (iTemp != 0)
			{
				if (eUnitAI == UNITAI_COUNTER || eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY)
				{
					iExtra = InfoValuation::keyedTarget(
							kUnit.getModifiers(), MODFAM_COMBAT, COMBAT_AMOUNT, iFlankingSeg, eFlanked)
						+ (pUnit ? 2 * pUnit->getExtraFlankingStrengthbyUnitCombatType(eFlanked) : 0);
					iValue += iTemp * (100 + iExtra) / 125;
				}
				else
				{
					iValue += iTemp / 10;
				}
			}
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_UNNERVE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_PILLAGE))
		{
			iExtra = kUnit.getCombatModifier(COMBAT_UNNERVE, CASC_SCOPE_UNIT) + (pUnit == NULL ? 0 : (pUnit->resolvedValue(URS_UNNERVE) - kUnit.getCombatModifier(COMBAT_UNNERVE, CASC_SCOPE_UNIT)) * 2);
			iValue += ((iTemp / 2) * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_ENCLOSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
			  (eUnitAI == UNITAI_PILLAGE))
		{
			iExtra = kUnit.getCombatModifier(COMBAT_ENCLOSE, CASC_SCOPE_UNIT) + (pUnit == NULL ? 0 : (pUnit->resolvedValue(URS_ENCLOSE) - kUnit.getCombatModifier(COMBAT_ENCLOSE, CASC_SCOPE_UNIT)) * 2);
			iValue += ((iTemp * 5) * (100 + iExtra) / 100);
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY ||
				  (eUnitAI == UNITAI_PILLAGE)))
		{
			iExtra = (pUnit == NULL ? kUnit.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT) : 2 * pUnit->resolvedValue(URS_LUNGE) - kUnit.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT));
			iValue += (iTemp * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_HEALER) ||
			  (eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
			  (eUnitAI == UNITAI_ESCORT))
		{
			iExtra = (pUnit == NULL ? kUnit.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT) : 2 * pUnit->resolvedValue(URS_DYNAMIC_DEFENSE) - kUnit.getCombatModifier(COMBAT_DYNAMIC_DEFENSE, CASC_SCOPE_UNIT));
			iValue += (iTemp * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	if (kUnitCombat.hasSkill(CLS_SKILL_STAMPEDE))
	{
		iValue -= 25;
	}




	//TB Combat Mods End

	iTemp = kUnitCombat.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCollateralModifier(COLLATERAL_DAMAGE, CASC_SCOPE_UNIT) : pUnit->resolvedValue(URS_COLLATERAL); //collateral has no strong synergy (not like retreat)
		iTemp *= (100 + iExtra);
		iTemp /= 100;

		if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 1);
		}
		else if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += ((iTemp * 2) / 3);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getBombardModifier(BOMBARD_RATE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	//Breakdown
	iTemp = kUnitCombat.getFlatCombat(COMBAT_BREAKDOWN_CHANCE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp * 2);
		}
		else if (pUnit != NULL && pUnit->canAttack())
		{
			iTemp *= pUnit->breakdownDamageTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += (iTemp * 10);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	iTemp = kUnitCombat.getFlatCombat(COMBAT_BREAKDOWN_DAMAGE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp * 2);
		}
		else if (pUnit != NULL && pUnit->canAttack())
		{
			iTemp *= pUnit->breakdownChanceTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += (iTemp * 100);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	iTemp = kUnitCombat.getFlatCombat(COMBAT_TAUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->tauntTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += (iTemp * 25);
			}
			else
			{
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = kUnitCombat.getSizeMatters().sizeModifier;
	if (iTemp != 0)
	{
		iValue += iTemp * 2;
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		if (kUnitCombat.getSizeMatters().qualityBase > -10)
		{
			iTemp = kUnitCombat.getSizeMatters().qualityBase - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}

		if (kUnitCombat.getSizeMatters().groupBase > -10)
		{
			iTemp = kUnitCombat.getSizeMatters().groupBase - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}

		if (kUnitCombat.getSizeMatters().sizeBase > -10)
		{
			iTemp = kUnitCombat.getSizeMatters().sizeBase - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		//@MOD Commanders: combat promotion value
		if (eUnitAI == UNITAI_GENERAL)
		{
			iValue += (iTemp * 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else if (eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iInvisFactor = 0;
	iTemp = kUnitCombat.getCombatModifier(COMBAT_STEALTH, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				// ONE concealment magnitude ([vision.md] §4) -- summing a per-INVISIBLE_* table counted the
				// same concealment once per method.
				iInvisFactor += pUnit->concealment() / 100;
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL))
		{
			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	//TB Combat Mod
	iTemp = kUnitCombat.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (pUnit->baseCombatStrHuman() < 10)
			{
				int ievaluation = ((11 - pUnit->baseCombatStrHuman()) * 30);
				iValue += (iTemp * ievaluation);
			}
			else
			{
				iValue += (iTemp * 30);
			}
		}
		else
		{
			iValue += (iTemp * 20);
		}
	}

	int iRank = 0;
	iTemp = kUnitCombat.getSizeMatters().combatModifierPerSizeMore;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->sizeRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getSizeMatters().combatModifierPerSizeLess;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->sizeRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getSizeMatters().combatModifierPerVolumeMore;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->groupRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getSizeMatters().combatModifierPerVolumeLess;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->groupRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	if (pUnit)
	{
		if (kUnitCombat.hasSkill(CLS_SKILL_ANIMAL_IGNORES_BORDERS) && pUnit->isAnimal() && !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
		{
			iValue += 50;
		}

		if (kUnitCombat.hasSkill(CLS_SKILL_NO_DEFENSIVE_BONUS))
		{
			if (!pUnit->noDefensiveBonus())
			{
				iValue -= 50;
			}
			else iValue -= 5;
		}

		if (kUnitCombat.hasSkill(CLS_SKILL_ONSLAUGHT))
		{
			iValue += 75;
		}

		if (pUnit->canAttack())
		{
			if (kUnitCombat.hasSkill(CLS_SKILL_ATTACK_ONLY_CITIES))
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue -= 10;
				}
				else iValue -= 50;
			}

			if (!pUnit->canAttackOnlyCities())
			{
				if (kUnitCombat.hasSkill(CLS_SKILL_IGNORE_NO_ENTRY_LEVEL))
				{
					if (pUnit->canIgnoreNoEntryLevel())
					{
						iValue += 5;
					}
					else iValue += 20;
				}

			}
		}

		if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL))
		{
			if (kUnitCombat.hasSkill(CLS_SKILL_IGNORE_ZONE_OF_CONTROL))
			{
				if (pUnit->canIgnoreZoneofControl())
				{
					iValue += 5;
				}
				else iValue += 25;
			}
		}

		if (kUnitCombat.hasSkill(CLS_SKILL_FLIES_TO_MOVE))
		{
			if (pUnit->canFliesToMove())
			{
				iValue += 5;
			}
			else iValue += 100;
		}

	}


	//TB Combat Mods
	//TB Modification note:adjusted City Attack promo value to balance better against withdraw promos for city attack ai units.
	if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
	{
		const int iCityAttack = kUnitCombat.getCombatModifier(COMBAT_CITY_ATTACK, CASC_SCOPE_UNIT);

		if (iCityAttack != 0)
		{
			if (eUnitAI == UNITAI_ATTACK_CITY
			||  eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
			{
				iValue += iCityAttack * 4;
			}
			else if (eUnitAI == UNITAI_ATTACK)
			{
				iValue += iCityAttack;
			}
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = (pUnit == NULL ? kUnit.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT) : 2 * pUnit->resolvedValue(URS_CITY_DEFENSE) - kUnit.getCombatModifier(COMBAT_CITY_DEFENSE, CASC_SCOPE_UNIT));
		iTemp *= 100 + iExtra;
		iTemp /= 100;
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER))
		{
			iValue += (iTemp * 4);
		}
		else if ((eUnitAI == UNITAI_HEALER) ||
			  (eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			  (eUnitAI == UNITAI_INVESTIGATOR))
		{
			iValue += iTemp / 2;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_HILLS_ATTACK, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_HILLS_ATTACK, CASC_SCOPE_UNIT) : pUnit->resolvedValue(URS_HILLS_ATTACK);
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = ((pUnit == NULL ? kUnit.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT) : 2 * pUnit->resolvedValue(URS_HILLS_DEFENSE) - kUnit.getCombatModifier(COMBAT_HILLS_DEFENSE, CASC_SCOPE_UNIT)));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_CITY_COUNTER ||
			eUnitAI == UNITAI_ESCORT)
		{
			if (pUnit != NULL && pUnit->plot()->isCity() && pUnit->plot()->isHills())
			{
				iValue += (iTemp * 4) / 3;
			}
		}
		else if (eUnitAI == UNITAI_COUNTER)
		{
			if (pUnit != NULL && pUnit->plot()->isHills())
			{
				iValue += (iTemp / 4);
			}
			else
			{
				iValue++;
			}
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	//WorkRateMod
	iTemp = kUnitCombat.getScalar(SCALAR_WORK_RATE_HILLS, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);

	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	//Team Project (3)
	iTemp = kUnitCombat.getCapture(CAPTURE_PROBABILITY, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_INVESTIGATOR) ||
			(pUnit != NULL && pUnit->canAttack()))
		{
			iValue += iTemp * 3;
		}
		else if (pUnit != NULL && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kUnitCombat.getCapture(CAPTURE_RESISTANCE, CASC_SCOPE_UNIT) / 100;
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else if (eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp;
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kUnitCombat.getScalar(SCALAR_REVOLT_PROTECTION, CASC_SCOPE_UNIT, CASC_UNIT_PERCENT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			if (pUnit != NULL && pUnit->plot() != NULL && pUnit->getX() != INVALID_PLOT_COORD && pUnit->plot()->isCity())
			{
				PlayerTypes eCultureOwner = pUnit->plot()->calculateCulturalOwner();
				// High weight for cities being threatened with culture revolution
				if (eCultureOwner != NO_PLAYER && GET_PLAYER(eCultureOwner).getTeam() != getTeam())
				{
					iValue += iTemp * 15;
				}
			}
		}
	}

	iTemp = kUnitCombat.getCollateralModifier(COLLATERAL_PROTECTION, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL))
		{
			iValue += (iTemp / 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getScalar(SCALAR_PILLAGE, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) / 100;
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_PIRATE_SEA ||
			eUnitAI == UNITAI_INFILTRATOR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	// costs.upgrade is sign-NORMALIZED as a COST modifier, so a discount authors NEGATIVE. The AI's benefit
	// is the reduction, so negate at the read rather than inverting the uses below.
	iTemp = -kUnitCombat.getCostsModifier(COSTS_UPGRADE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iValue += (iTemp / 16);
	}

	iTemp = kUnitCombat.getExperienceModifier(EXPERIENCE_AMOUNT, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA))
		{
			iValue += (iTemp * 1);
		}
		else
		{
			iValue += (iTemp / 2);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_KAMIKAZE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp / 16);
		}
		else
		{
			iValue += (iTemp / 64);
		}
	}

	{
		std::vector<std::pair<int, int> > unitCombatRows;
		InfoValuation::collectKeyedCombat(
			kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_UNITCOMBAT, COMBAT_AMOUNT, unitCombatRows);

		for (size_t iRow = 0; iRow < unitCombatRows.size(); ++iRow)
		{
			iValue += unitCombatRows[iRow].second;
		}
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		iTemp = InfoValuation::keyedCombat(
			kUnitCombat.getModifiers(), InfoValuation::COMBAT_TARGET_DOMAIN, iI, COMBAT_AMOUNT);
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER)
			{
				iValue += (iTemp * 1);
			}
			else if ((eUnitAI == UNITAI_ATTACK) ||
					   (eUnitAI == UNITAI_RESERVE))
			{
				iValue += (iTemp / 2);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	// The concealment-vs-detection CONTEST ([vision.md] par.4), the unit-combat twin of the promotion block.
	// ONE concealment magnitude plus a detection ROW per method answered -- the per-INVISIBLE_* intensity tables,
	// their per-substrate variants and the second RANGE system are retired, and a per-type sum would have counted
	// one concealment once per method. x100 at the info, reduced here at the point of use.
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		const CvHideAndSeekSection& kHideAndSeek = kUnitCombat.getHideAndSeek();

		iTemp = kHideAndSeek.concealment / 100;
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_ANIMAL ||
				eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_PIRATE_SEA ||
				eUnitAI == UNITAI_ATTACK_SEA ||
				eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
				eUnitAI == UNITAI_HUNTER ||
				eUnitAI == UNITAI_GREAT_HUNTER ||
				eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
				eUnitAI == UNITAI_INFILTRATOR)
			{
				iValue += iTemp * 30;
			}
			else iValue += iTemp * 5;
		}

		int iDetectionTotal = 0;
		for (size_t iRow = 0; iRow < kHideAndSeek.detection.size(); ++iRow)
		{
			iDetectionTotal += kHideAndSeek.detection[iRow].value;
		}
		iTemp = iDetectionTotal / 100;
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
				eUnitAI == UNITAI_PILLAGE_COUNTER)
			{
				iValue += iTemp * 350;
			}
			else if (eUnitAI == UNITAI_COUNTER ||
				eUnitAI == UNITAI_ANIMAL ||
				eUnitAI == UNITAI_ESCORT_SEA ||
				eUnitAI == UNITAI_HUNTER_ESCORT ||
				eUnitAI == UNITAI_PROPERTY_CONTROL ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += iTemp * 15;
			}
			else iValue += iTemp * 10;
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT) : pUnit->resolvedValue(URS_COMBAT_ATTACK);
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ANIMAL))
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_DEFENSE, CASC_SCOPE_UNIT) : pUnit->resolvedValue(URS_COMBAT_DEFENSE);
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;

		int iMultiplier = 2;

		if (eUnitAI == UNITAI_RESERVE ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_ESCORT)
		{
			iValue += (iTemp * iMultiplier);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_VS_BARBS, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_COUNTER ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_CITY_COUNTER ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_EXPLORE_SEA ||
			eUnitAI == UNITAI_EXPLORE ||
			eUnitAI == UNITAI_PILLAGE_COUNTER ||
			eUnitAI == UNITAI_HUNTER ||
			eUnitAI == UNITAI_HUNTER_ESCORT ||
			eUnitAI == UNITAI_GREAT_HUNTER ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_ESCORT)
		{
			int iEraFactor = 10 - (int)getCurrentEra();
			iTemp *= iEraFactor;
			iTemp /= 9;
			if (pUnit != NULL && !pUnit->isHominid())
			{
				iValue += iTemp;
			}
		}
	}

	iTemp = kUnitCombat.getCombatModifier(COMBAT_RELIGIOUS, CASC_SCOPE_UNIT);
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCombatModifier(COMBAT_RELIGIOUS, CASC_SCOPE_UNIT) : pUnit->resolvedValue(URS_RELIGIOUS_COMBAT);
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;

		if (eUnitAI == UNITAI_ATTACK ||
			eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL ||
			eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_RESERVE ||
			eUnitAI == UNITAI_COUNTER ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_RESERVE_SEA ||
			eUnitAI == UNITAI_ASSAULT_SEA ||
			eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iTemp *= 2;
			iValue += iTemp;
		}
	}

	// TB Combat Mods Begin
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL) && kUnitCombat.hasSkill(CLS_SKILL_ZONE_OF_CONTROL))
	{
		iValue += 250;
	}

	return iValue;
}

TechTypes CvPlayerAI::AI_bestReligiousTech(int iMaxPathLength, TechTypes eIgnoreTech, AdvisorTypes eIgnoreAdvisor)
{
	PROFILE("CvPlayerAI::AI_bestReligiousTech");

	int iBestValue = 0;
	TechTypes eBestTech = NO_TECH;

	// Walked outward from the enabler's researchable frontier via `leadsTo`, to the depth this search accepts --
	// the same shape as AI_bestTech, never a sweep of the tech database.
	std::set<int> candidateTechs;
	AI_walkResearchFrontier(iMaxPathLength, candidateTechs);

	for (std::set<int>::const_iterator itCandidate = candidateTechs.begin(); itCandidate != candidateTechs.end(); ++itCandidate)
	{
		const TechTypes eTechX = static_cast<TechTypes>(*itCandidate);

		if ((eIgnoreTech == NO_TECH || eTechX != eIgnoreTech)
		&& (eIgnoreAdvisor == NO_ADVISOR || GC.getTechInfo(eTechX).getAdvisor() != eIgnoreAdvisor)
		&& (canEverResearch(eTechX))
		&& GC.getTechInfo(eTechX).getEra() <= getCurrentEra())
		{
			const int iPathLength = findPathLength(eTechX, false);

			if (iPathLength <= iMaxPathLength)
			{
				const int iValue = AI_religiousTechValue(eTechX) / std::max(1, iPathLength);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestTech = eTechX;
				}
			}
		}
	}
	return eBestTech;
}

int CvPlayerAI::AI_religiousTechValue(TechTypes eTech) const
{
	PROFILE_FUNC();

	int iReligionValue = 0;

	if (canFoundReligion())
	{
		for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
		{
			const TechTypes eReligionTech = GC.getReligionInfo((ReligionTypes)iJ).getTechPrereq();
			if (eReligionTech == eTech)
			{
				if (!(GC.getGame().isReligionSlotTaken((ReligionTypes)iJ)))
				{
					if (!GC.getGame().isOption(GAMEOPTION_RELIGION_PICK))
					{
						const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
						if (eFavorite != NO_RELIGION)
						{
							if (iJ == eFavorite)
							{
								iReligionValue += 1000;
							}
							else
							{
								iReligionValue += 500;
							}
						}
					}
					iReligionValue += 250;
				}
				if (GC.getGame().isOption(GAMEOPTION_RELIGION_DIVINE_PROPHETS))
				{
					if (GC.getGame().countKnownTechNumTeams(eTech) < 1)
					{
						iReligionValue += 250;
					}
				}
			}
		}

		if (iReligionValue > 0)
		{
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
			{
				iReligionValue += 500;
			}
		}
	}

	return iReligionValue;
}

void CvPlayerAI::AI_doMilitaryProductionCity()
{
	PROFILE_FUNC();

	//invalidate cache
	m_iMilitaryProductionCityCount = -1;
	m_iNavalMilitaryProductionCityCount = -1;

	algo::for_each(cities(), CvCity::fn::AI_setMilitaryProductionCity(false));
	algo::for_each(cities(), CvCity::fn::AI_setNavalMilitaryProductionCity(false));

	if (getNumCities() < 4)
	{
		return;
	}

	int iNumMilitaryProdCitiesNeeded = getNumCities() / 4;

	if (GET_TEAM(getTeam()).isAtWar())
	{
		iNumMilitaryProdCitiesNeeded += getNumCities() / 4;
	}
	else if (GET_TEAM(getTeam()).hasWarPlan(true))
	{
		iNumMilitaryProdCitiesNeeded += getNumCities() / 8;
	}
	if (AI_isFinancialTrouble())
	{
		iNumMilitaryProdCitiesNeeded = std::max(1, iNumMilitaryProdCitiesNeeded / 2);
	}

	for (int iPass = iNumMilitaryProdCitiesNeeded; iPass > 0; iPass--)
	{
		foreach_(CvCity * pLoopCity, cities())
		{
			if (pLoopCity->AI_getMilitaryProductionRateRank() == iPass)
			{
				pLoopCity->AI_setMilitaryProductionCity(true);
			}
			if (pLoopCity->AI_getNavalMilitaryProductionRateRank() == iPass)
			{
				pLoopCity->AI_setNavalMilitaryProductionCity(true);
			}
		}
	}
}

int CvPlayerAI::AI_getMilitaryProductionCityCount() const
{
	if (m_iMilitaryProductionCityCount != -1)
	{
		return m_iMilitaryProductionCityCount;
	}

	const int iCount = algo::count_if(cities(), CvCity::fn::AI_isMilitaryProductionCity());

	m_iMilitaryProductionCityCount = iCount;
	return iCount;
}

int CvPlayerAI::AI_getNavalMilitaryProductionCityCount() const
{
	if (m_iNavalMilitaryProductionCityCount != -1)
	{
		return m_iNavalMilitaryProductionCityCount;
	}

	const int iCount = algo::count_if(cities(), CvCity::fn::AI_isNavalMilitaryProductionCity());

	m_iNavalMilitaryProductionCityCount = iCount;
	return iCount;
}


int	CvPlayerAI::AI_getNumBuildingsNeeded(BuildingTypes eBuilding, bool bCoastal) const
{
	PROFILE_EXTRA_FUNC();
	// Total needed to have one in every city
	BuildingCountMap::const_iterator itr = m_numBuildingsNeeded.find(eBuilding);

	if (itr == m_numBuildingsNeeded.end())
	{
		int	result = 0;

		foreach_(const CvCity * pLoopCity, cities())
		{
			if ((!bCoastal || pLoopCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
			&& !pLoopCity->hasBuilding(eBuilding))
			{
				result++;
			}
		}
		{
			m_numBuildingsNeeded[eBuilding] = result;
		}
		return result;
	}
	return itr->second;
}

void CvPlayerAI::AI_changeNumBuildingsNeeded(BuildingTypes eBuilding, int iChange)
{
	m_numBuildingsNeeded[eBuilding] += iChange;
}

void CvPlayerAI::AI_noteUnitRecalcNeeded()
{
	bUnitRecalcNeeded = true;
}

void CvPlayerAI::AI_recalculateUnitCounts()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
	{
		AI_changeNumAIUnits((UnitAITypes)iI, -AI_getNumAIUnits((UnitAITypes)iI));
		AI_changeEffNumAIUnitsTimes100((UnitAITypes)iI, -AI_getEffNumAIUnitsTimes100((UnitAITypes)iI));

		foreach_(CvArea * pLoopArea, GC.getMap().areas())
		{
			pLoopArea->changeNumAIUnits(m_eID, (UnitAITypes)iI, -pLoopArea->getNumAIUnits(m_eID, (UnitAITypes)iI));
			pLoopArea->changeEffNumAIUnitsTimes100(m_eID, (UnitAITypes)iI, -pLoopArea->getEffNumAIUnitsTimes100(m_eID, (UnitAITypes)iI));
		}
	}

	foreach_(const CvUnit * pLoopUnit, units())
	{
		// The temp unit is explicitly uncounted at creation (getTempUnit: "this one
		// doesn't count") -- recounting must not resurrect it into the ledgers.
		if (pLoopUnit == m_pTempUnit)
		{
			continue;
		}
		const UnitAITypes eAIType = pLoopUnit->AI_getUnitAIType();

		if (NO_UNITAI != eAIType)
		{
			AI_changeNumAIUnits(eAIType, 1);
			AI_changeEffNumAIUnitsTimes100(eAIType, pLoopUnit->SMeffectiveCount());

			pLoopUnit->area()->changeNumAIUnits(m_eID, eAIType, 1);
			pLoopUnit->area()->changeEffNumAIUnitsTimes100(m_eID, eAIType, pLoopUnit->SMeffectiveCount());
		}
	}

	bUnitRecalcNeeded = false;
}


// #395: the strength-weighted ledgers are transient (never serialized) -- rebuild them
// from the loaded units at the end of read(). Area columns for this player were zeroed
// by CvArea::reset during map load; only this player's units contribute to them here.
void CvPlayerAI::AI_rebuildEffUnitLedgers()
{
	PROFILE_EXTRA_FUNC();

	for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
	{
		m_aiEffNumAIUnitsTimes100[iI] = 0;
	}

	foreach_(const CvUnit * pLoopUnit, units())
	{
		if (pLoopUnit == m_pTempUnit)
		{
			continue;
		}
		const UnitAITypes eAIType = pLoopUnit->AI_getUnitAIType();

		if (NO_UNITAI != eAIType)
		{
			AI_changeEffNumAIUnitsTimes100(eAIType, pLoopUnit->SMeffectiveCount());

			if (pLoopUnit->plot() != NULL)
			{
				pLoopUnit->area()->changeEffNumAIUnitsTimes100(m_eID, eAIType, pLoopUnit->SMeffectiveCount());
			}
		}
	}
}

int CvPlayerAI::AI_calculateAverageLocalInstability() const
{
	PROFILE_EXTRA_FUNC();
	int	result = 0;
	int iNum = 0;

	foreach_(const CvCity * pLoopCity, cities())
	{
		//	Count big cities more than small ones to some exent as a revolution there hurts more
		int	iWeight = (pLoopCity->getPopulation() < 6 ? 1 : 2);

		result += iWeight * pLoopCity->getLocalRevIndex();
		iNum += iWeight;
	}

	return (iNum == 0 ? 0 : result / iNum);
}

int CvPlayerAI::AI_calculateAverageCityDistance() const
{
	PROFILE_EXTRA_FUNC();
	int	result = 0;
	int iNum = 0;
	const CvCity* pCapital = getCapitalCity();

	if (pCapital != NULL)
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			if (pLoopCity != pCapital)
			{
				int iCapitalDistance = ::plotDistance(pLoopCity->getX(), pLoopCity->getY(), pCapital->getX(), pCapital->getY());
				//	Count big cities more than small ones to some exent as a revolution there hurts more
				int	iWeight = (pLoopCity->getPopulation() < 6 ? 1 : 2);

				result += iWeight * iCapitalDistance;
				iNum += iWeight;
			}
		}
	}

	return (iNum == 0 ? 0 : result / iNum);
}

void CvPlayerAI::AI_noteWarStatusChange(TeamTypes eTeam, bool bAtWar)
{
	//	Cancel any existing cached beeline tech target if war status changes
	m_eBestResearchTarget = NO_TECH;
}


// Evaluate a building we are considering building here in terms of its effect on properties
int CvPlayerAI::heritagePropertiesValue(const CvHeritageInfo& heritage) const
{
	PROFILE_EXTRA_FUNC();

	const CvCityAI* pCapital = static_cast<CvCityAI*>(getCapitalCity());
	if (!pCapital)
	{
		return 0;
	}
	// Evaluate building properties
	std::map<int, int> effectivePropertyChanges;

	foreach_(const CvPropertySource * pSource, heritage.getPropertyManipulators()->getSources())
	{
		if (pSource->getType() == PROPERTYSOURCE_CONSTANT)
		{
			// Convert to an effective absolute amount by looking at the steady state value given current
			const PropertyTypes eProperty = pSource->getProperty();
			// Only count half the unit source as we want to encourage building sources over unit ones
			const int iCurrentSourceSize = (
				  pCapital->getTotalBuildingSourcedProperty(eProperty)
				+ pCapital->getTotalUnitSourcedProperty(eProperty) / 2
				+ pCapital->getPropertyNonBuildingSource(eProperty)
			);
			const int iNewSourceSize = iCurrentSourceSize + static_cast<const CvPropertySourceConstant*>(pSource)->getAmountPerTurn(getGameObject());
			const int iDecayPercent = pCapital->getPropertyDecay(eProperty);

			// Steady state occurs at a level where the decay removes as much per turn as the sources add
			//	Decay can be 0 if the current level is below the threshold at which decay cuts in, so for the
			//	purposes of calculation just treat this as very slow decay
			const int iCurrentSteadyStateLevel = (100 * iCurrentSourceSize) / std::max(1, iDecayPercent);
			const int iNewSteadyStateLevel = (100 * iNewSourceSize) / std::max(1, iDecayPercent);

			std::map<int, int>::iterator itr = effectivePropertyChanges.find(eProperty);
			if (itr == effectivePropertyChanges.end())
			{
				effectivePropertyChanges[eProperty] = (iNewSteadyStateLevel - iCurrentSteadyStateLevel);
			}
			else
			{
				itr->second += (iNewSteadyStateLevel - iCurrentSteadyStateLevel);
			}
		}
	}

	int iValue = 0;
	for (std::map<int, int>::const_iterator itr = effectivePropertyChanges.begin(); itr != effectivePropertyChanges.end(); ++itr)
	{
		iValue += pCapital->getPropertySourceValue((PropertyTypes)itr->first, itr->second);
	}
	return iValue;
}

int CvPlayerAI::AI_heritageValue(const HeritageTypes eType) const
{
	PROFILE_FUNC();

	const CvHeritageInfo& heritage = GC.getHeritageInfo(eType);

	int iValue = 0;
	//	The era-banded empire commerce. The band table is gone: the curator authors each band as an
	//	ERA-THRESHOLD CONDITIONED entry (`{value, enabled:{type: ERA, min: N}}` -- [json.md §6]), so the bands
	//	ACCUMULATE FOR FREE through ordinary deposit summation -- every entry whose threshold the player's era
	//	has reached applies, which is exactly what the old `eEra >= band` walk was doing by hand.
	//	⚑ A CITY-LESS view evaluates against the CAPITAL ([patterns.md] THE VALUATION PROTOCOL): this is a
	//	player-scope question, and the capital is the ONE ruled stand-in. A player with no capital has no
	//	valuation to give, so the commerce term simply contributes nothing rather than inventing a base.
	const CvCity* pCapital = getCapitalCity();
	if (pCapital != NULL)
	{
		int aiFlatCommerce[NUM_COMMERCE_TYPES];
		heritage.expectedFlatCommerce(pCapital->getCityContext(), getEmpireContext(),
			pCapital->plotGroup(getID()), aiFlatCommerce);
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			iValue += aiFlatCommerce[iI] / 100;   // a FLAT is ×100; the reader reduces at its point of use
		}
	}
	iValue += heritagePropertiesValue(heritage);

	return std::max(0, iValue);
}

#ifdef CVARMY_BREAKSAVE
void CvPlayerAI::AI_formArmies()
{
	const int NB_MAX_ARMIES = 3; // Maximum number of armies to form
	const int NB_GROUP_MINI_FOR_ARMY = 4;
	const int NB_GROUP_MINI_FOR_BIG_ARMY = 7;
	const int NB_GROUP_MINI_FOR_LEADER = 8;

	bool bArmyGrouped = false;
	//Check and kill empty groups
	for (CvPlayer::group_iterator it = groups().begin(); it != groups().end(); ++it)
	{
		CvSelectionGroup* pGroup = *it;

		if (pGroup != NULL && (pGroup->getHeadUnit() == NULL || pGroup->getNumUnits() == 0))
		{
			pGroup->kill(); // supprime proprement le groupe
		}
	}

	if (GET_TEAM(getTeam()).isAtWar() && !isNPC())
	{
		// Keep forming armies until the maximum is reached
		int icount = 0;
		while (m_armies.getCount() < NB_MAX_ARMIES && icount <50)
		{
			// Find the best possible leader
			CvSelectionGroup* pBestLeader = NULL;
				int iBestValue = 0;


			CvPlayer::group_range allGroups = groups();
			for (CvPlayer::group_iterator it = allGroups.begin(); it != allGroups.end(); ++it)
			{
				CvSelectionGroup* pGroup = *it;
				if (pGroup == NULL)
					continue;

				if (pGroup != NULL && pGroup->getHeadUnit() == NULL)
				{
					pGroup->kill(); // supprime proprement le groupe
					continue;
				}


				if (pGroup->getDomainType() != DOMAIN_LAND || pGroup->getArmyID() != -1) // already part of an army
					continue;


				// Select only city attack groups
				if (pGroup->AI_getMissionAIType() == MISSIONAI_ASSAULT ||
					pGroup->getHeadUnit()->AI_getUnitAIType() == UNITAI_ATTACK_CITY)
				{
					int iValue = pGroup->getNumUnits();
					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestLeader = pGroup;
					}
				}
			}

			// Stop if no valid leader is found
			if (pBestLeader == NULL || iBestValue < NB_GROUP_MINI_FOR_LEADER)
				break;

			CvArmy* pArmy = NULL;
			if (m_armies.getCount() == 0 || bArmyGrouped)
			{
				if (m_armies.getCurrentID() == 8192) //Cavltix - if not init
				{
					m_armies.init();
					m_armies.setCurrentID(1);
				}
				// Create the army
				bArmyGrouped = false;
				pArmy = m_armies.add();
				pArmy->init(pArmy->getID(), getID(), ARMY_MISSION_ATTACK_CITY); // init with ID, owner, and mission
				pArmy->setLeader(pBestLeader);                            // assign leader
				//pArmy->addGroup(pBestLeader);                             // add leader group to army
				
			}
			else
			{
				for (FFreeListTrashArray<CvArmy>::iterator it = m_armies.begin(); it != m_armies.end(); ++it)
				{
					pArmy = &(*it);
					if (pArmy != NULL)
					{
						pBestLeader = pArmy->getLeader();
						if (pBestLeader == NULL)
						{
							pBestLeader = pArmy->findNewLeader();
							if (pBestLeader == NULL)
							{
								pArmy->disband();
								m_armies.remove(pArmy);
								return;
							}
						}

					}
				}
			}
			// Assign the target city
			CvPlot* pLeaderPlot = pBestLeader->getHeadUnit()->plot();
			CvArea* pArea = pLeaderPlot->area();
			CvCity* pTargetCity = AI_findTargetCity(pArea);
			if (pTargetCity != NULL)
			{
				pArmy->setTargetPlot(pTargetCity->plot());          // set the army's target plot
			}

			// Add other compatible stacks
			CvPlayer::group_range allGroups2 = groups();
			for (CvPlayer::group_iterator it = allGroups2.begin(); it != allGroups2.end(); ++it)
			{
				CvSelectionGroup* pGroup = *it;
				if (pGroup == NULL || pGroup == pBestLeader || pGroup->getHeadUnit() == NULL)
					continue;

				if (pGroup->getDomainType() != DOMAIN_LAND || pGroup->getArmyID() != -1) // already part of an army
					continue;

				if (((pGroup->AI_getMissionAIType() == MISSIONAI_ASSAULT || pGroup->getHeadUnit()->AI_getUnitAIType() == UNITAI_ATTACK_CITY) &&
					pGroup->getNumUnits() >= NB_GROUP_MINI_FOR_ARMY) || pGroup->getNumUnits() >= NB_GROUP_MINI_FOR_BIG_ARMY)
				{
					int iDist = plotDistance(pGroup->getHeadUnit()->plot()->getX(), pGroup->getHeadUnit()->plot()->getY(),
											 pLeaderPlot->getX(), pLeaderPlot->getY());
					pArmy->addGroup(pGroup);
					
					pGroup->pushMission(MISSION_SKIP);
					pGroup->pushMissionInternal(MISSION_MOVE_TO_UNIT, pBestLeader->getOwner(), pBestLeader->getHeadUnit()->getID(), 0, false, false, MISSIONAI_GROUP, NULL, pBestLeader->getHeadUnit());
					bArmyGrouped = true;
				}
			}
			icount++;
		}
	}
	else
	{
		for (FFreeListTrashArray<CvArmy>::iterator it = m_armies.begin(); it != m_armies.end(); ++it)
		{
			CvArmy * pArmy = &(*it);
			if (pArmy != NULL)
			{
				int iArmyID = pArmy->getID();
				GET_PLAYER(pArmy->getOwner()).deleteArmy(iArmyID);
			}
		}
	}
}

#endif
