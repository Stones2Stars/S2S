//
//	CvLeaderHeadInfo -- the leaderhead poco's exemplar reads (see the header). Every grouped table fills
//	from the authored key spellings through the shared parse surface (CvJsonParse), and every getter is a
//	bare member read over the materialized forms ([DEC-materialize-at-mapfrom] -- no string-keyed read
//	survives past mapFrom).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, CvString, gDLL, FASSERT_BOUNDS
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvLeaderHeadInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonWorldArt / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonReadFkMap / jsonReadFlavours / jsonResolveId
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- getLeaderheadArtInfo / CvArtInfoLeaderhead getters


namespace
{
	// The authored-key tables, each in its enum's declaration order (the header axes; the census spellings).
	// lh_-prefixed: these translation units unity-batch together, so even anonymous-namespace names must be
	// unique across sibling infos.
	const char* LH_DIPLO_RELATION_KEYS[NUM_LEADER_DIPLO_RELATIONS] =
	{
		"worseRankDifference",
		"betterRankDifference",
		"closeBorders",
		"lostWar",
		"atWar",
		"atPeace",
		"sameReligion",
		"differentReligion",
		"bonusTrade",
		"openBorders",
		"defensivePact",
		"shareWar",
		"favoriteCivic"
	};

	const char* LH_REFUSAL_KEYS[NUM_LEADER_REFUSALS] =
	{
		"demandTribute",
		"noGiveHelp",
		"tech",
		"strategicBonus",
		"happinessBonus",
		"healthBonus",
		"map",
		"declareWar",
		"declareWarThem",
		"stopTrading",
		"stopTradingThem",
		"adoptCivic",
		"convertReligion",
		"openBorders",
		"defensivePact",
		"permanentAlliance",
		"vassal"
	};

	const char* LH_VICTORY_PURSUIT_KEYS[NUM_LEADER_VICTORY_PURSUITS] =
	{
		"culture",
		"space",
		"conquest",
		"domination",
		"diplomacy"
	};

	const char* LH_DIPLO_MUSIC_KEYS[NUM_LEADER_DIPLO_MUSIC] =
	{
		"diploIntroMusicPeace",
		"diploMusicPeace",
		"diploIntroMusicWar",
		"diploMusicWar"
	};

	// An engine-enum-keyed authored table (parent[key] = { "ENUMTYPE_X": n }) scattered into a fixed
	// array, bounded (an unresolved id surfaces via jsonResolveId's diagnostics; an out-of-range id is
	// skipped -- the archived SetVariableListTagPair guard).
	void lh_fillEnumSlots(const picojson::object& parentObj, const char* szKey, int* aiSlots, int iSlotCount)
	{
		std::map<int, int> keyedValues;
		jsonReadFkMap(parentObj, szKey, keyedValues);
		for (std::map<int, int>::const_iterator slotIt = keyedValues.begin(); slotIt != keyedValues.end(); ++slotIt)
		{
			if (slotIt->first >= 0 && slotIt->first < iSlotCount)
			{
				aiSlots[slotIt->first] = slotIt->second;
			}
		}
	}

	// sound.diplo* -- an era-keyed { C2C_ERA_*: "AS2D_*" } table resolved to (era id -> RUNTIME audio-tag
	// index). An intro table may instead be a bare [era, ...] LIST carrying no scripts; a null/empty slot
	// carries no script either -- both leave the map sparse, and an absent era reads the -1 default.
	void lh_fillDiploMusic(const picojson::object& soundObj, const char* szKey, std::map<int, int>& eraToScriptId)
	{
		picojson::object::const_iterator tableIt = soundObj.find(szKey);
		if (tableIt == soundObj.end() || !tableIt->second.is<picojson::object>())
		{
			return;
		}
		const picojson::object& eraTable = tableIt->second.get<picojson::object>();
		for (picojson::object::const_iterator eraIt = eraTable.begin(); eraIt != eraTable.end(); ++eraIt)
		{
			if (!eraIt->second.is<std::string>() || eraIt->second.get<std::string>().empty())
			{
				continue;
			}
			const int iEra = jsonResolveId(eraIt->first);
			if (iEra >= 0)
			{
				eraToScriptId[iEra] = gDLL->getAudioTagIndex(eraIt->second.get<std::string>().c_str());
			}
		}
	}

}


CvLeaderHeadInfo::CvLeaderHeadInfo()
{
	resetMapped();
}


// ======================= the grouped-table reads (bounds-guarded fixed arrays / sparse maps) ============

int CvLeaderHeadInfo::getAttitudeChange(LeaderDiploRelation eRelation) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_DIPLO_RELATIONS, eRelation);
	return m_aiAttitudeChange[eRelation];
}


int CvLeaderHeadInfo::getAttitudeDivisor(LeaderDiploRelation eRelation) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_DIPLO_RELATIONS, eRelation);
	return m_aiAttitudeDivisor[eRelation];
}


int CvLeaderHeadInfo::getAttitudeChangeLimit(LeaderDiploRelation eRelation) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_DIPLO_RELATIONS, eRelation);
	return m_aiAttitudeChangeLimit[eRelation];
}


int CvLeaderHeadInfo::getRefuseAttitudeThreshold(LeaderRefusal eRefusal) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_REFUSALS, eRefusal);
	return m_aiRefuseAttitudeThreshold[eRefusal];
}


int CvLeaderHeadInfo::getVictoryWeight(LeaderVictoryPursuit ePursuit) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_VICTORY_PURSUITS, ePursuit);
	return m_aiVictoryWeight[ePursuit];
}


int CvLeaderHeadInfo::getContactRand(ContactTypes eContact) const
{
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eContact);
	return m_aiContactRand[eContact];
}


int CvLeaderHeadInfo::getContactDelay(ContactTypes eContact) const
{
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eContact);
	return m_aiContactDelay[eContact];
}


int CvLeaderHeadInfo::getMemoryDecayRand(MemoryTypes eMemory) const
{
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eMemory);
	return m_aiMemoryDecayRand[eMemory];
}


int CvLeaderHeadInfo::getMemoryAttitudePercent(MemoryTypes eMemory) const
{
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eMemory);
	return m_aiMemoryAttitudePercent[eMemory];
}


int CvLeaderHeadInfo::getNoWarAttitudeProb(AttitudeTypes eAttitude) const
{
	FASSERT_BOUNDS(0, NUM_ATTITUDE_TYPES, eAttitude);
	return m_aiNoWarAttitudeProb[eAttitude];
}


int CvLeaderHeadInfo::getUnitAIWeightModifier(UnitAITypes eUnitAI) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAI);
	return m_aiUnitAIWeightModifier[eUnitAI];
}


int CvLeaderHeadInfo::getFlavorValue(FlavorTypes eFlavor) const
{
	return mapValueOrDefault(m_flavours, (int)eFlavor);
}


int CvLeaderHeadInfo::getImprovementWeightModifier(ImprovementTypes eImprovement) const
{
	return mapValueOrDefault(m_improvementWeights, (int)eImprovement);
}


int CvLeaderHeadInfo::getDiploMusicScriptId(LeaderDiploMusic eMusic, EraTypes eEra) const
{
	FASSERT_BOUNDS(0, NUM_LEADER_DIPLO_MUSIC, eMusic);
	return mapValueOrDefault(m_diploMusicScriptIds[eMusic], (int)eEra, -1);
}


// ======================= the art plane (EXE-bound portrait surface) =====================================

const CvArtInfoLeaderhead* CvLeaderHeadInfo::getArtInfo() const
{
	return ARTFILEMGR.getLeaderheadArtInfo(getArtDefineTag());
}


const char* CvLeaderHeadInfo::getLeaderHead() const
{
	return getArtInfo() ? getArtInfo()->getNIF() : NULL;
}


const char* CvLeaderHeadInfo::getButton() const
{
	const CvArtInfoLeaderhead* pLeaderheadArtInfo = getArtInfo();
	return pLeaderheadArtInfo ? pLeaderheadArtInfo->getButton() : NULL;
}


// ======================= mapFrom -- the ONE load hook (idempotent by contract) ==========================

void CvLeaderHeadInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / identity text) + the base section dispatch/census

	resetMapped();   // idempotency (CvInfo.h): the full-registry re-run fully redefines every mapped member

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- the authored trait ASSIGNMENTS (root `traits` / `complexTraits`, TRAIT_* FKs) ---
	// Both slots are always present in the data and normally EMPTY: the assignment is community-owned
	// content a modder authors, not something the curator reconstructs. Which set is ACTIVE is decided by
	// the CONSUMER (CvTraitSelection::leaderTraits) -- an info never reads a game option.
	jsonReadIdList(entityObj, "traits", m_aiTraits);
	jsonReadIdList(entityObj, "complexTraits", m_aiComplexTraits);

	// --- world.art.define -> the ART_DEF_* tag ARTFILEMGR resolves the leaderhead's art from ---
	// ⛔ The EXE reads this one through getArtInfo(), which is DllExport and is NOT null-checked on its side:
	// an unresolved tag makes ARTFILEMGR answer NULL and the diplomacy screen dereferences it while reading the
	// art's path strings. So an empty tag here is not a missing portrait, it is a crash on first contact.
	if (const picojson::object* pWorldArt = jsonWorldArt(entityObj))
	{
		std::string szArtDefineTag;
		if (jsonIdStr(*pWorldArt, "define", szArtDefineTag))
		{
			m_szArtDefineTag = szArtDefineTag.c_str();
		}
	}

	// --- ai.* -- the §7 AI-metadata plane (raw human config; the curator emits plain ints, no x100) ---
	if (const picojson::object* pAiMeta = jsonChildObj(entityObj, "ai"))
	{
		m_bNPC = jsonIdBool(*pAiMeta, "npc");

		jsonReadFlavours(*pAiMeta, m_flavours);

		if (const picojson::object* pPersonality = jsonChildObj(*pAiMeta, "personality"))
		{
			m_iBaseAttitude        = jsonIdInt(*pPersonality, "baseAttitude");
			m_iBasePeaceWeight     = jsonIdInt(*pPersonality, "basePeaceWeight");
			m_iPeaceWeightRand     = jsonIdInt(*pPersonality, "peaceWeightRand");
			m_iWarmongerRespect    = jsonIdInt(*pPersonality, "warmongerRespect");
			m_iEspionageWeight     = jsonIdInt(*pPersonality, "espionageWeight");
			m_iWonderConstructRand = jsonIdInt(*pPersonality, "wonderConstructRand");
			m_iBuildUnitProb       = jsonIdInt(*pPersonality, "buildUnitProb");
			m_iResearchSearchDepth = jsonIdInt(*pPersonality, "researchSearchDepth", DEFAULT_RESEARCH_SEARCH_DEPTH);
			m_iFreedomAppreciation = jsonIdInt(*pPersonality, "freedomAppreciation");
			m_iVassalPowerModifier = jsonIdInt(*pPersonality, "vassalPowerModifier");
		}

		if (const picojson::object* pWar = jsonChildObj(*pAiMeta, "war"))
		{
			m_iMaxWarRand                   = jsonIdInt(*pWar, "maxWarRand");
			m_iMaxWarNearbyPowerRatio       = jsonIdInt(*pWar, "maxWarNearbyPowerRatio");
			m_iMaxWarDistantPowerRatio      = jsonIdInt(*pWar, "maxWarDistantPowerRatio");
			m_iMaxWarMinAdjacentLandPercent = jsonIdInt(*pWar, "maxWarMinAdjacentLandPercent");
			m_iLimitedWarRand               = jsonIdInt(*pWar, "limitedWarRand");
			m_iLimitedWarPowerRatio         = jsonIdInt(*pWar, "limitedWarPowerRatio");
			m_iDogpileWarRand               = jsonIdInt(*pWar, "dogpileWarRand");
			m_iMakePeaceRand                = jsonIdInt(*pWar, "makePeaceRand");
			m_iDeclareWarTradeRand          = jsonIdInt(*pWar, "declareWarTradeRand");
			m_iDemandRebukedSneakProb       = jsonIdInt(*pWar, "demandRebukedSneakProb");
			m_iDemandRebukedWarProb         = jsonIdInt(*pWar, "demandRebukedWarProb");
			m_iRefuseToTalkWarThreshold     = jsonIdInt(*pWar, "refuseToTalkWarThreshold");
			m_iBaseAttackOddsChange         = jsonIdInt(*pWar, "baseAttackOddsChange");
			m_iAttackOddsChangeRand         = jsonIdInt(*pWar, "attackOddsChangeRand");
			m_iRazeCityProb                 = jsonIdInt(*pWar, "razeCityProb");
		}

		if (const picojson::object* pVictory = jsonChildObj(*pAiMeta, "victory"))
		{
			for (int iPursuit = 0; iPursuit < NUM_LEADER_VICTORY_PURSUITS; ++iPursuit)
			{
				m_aiVictoryWeight[iPursuit] = jsonIdInt(*pVictory, LH_VICTORY_PURSUIT_KEYS[iPursuit]);
			}
		}

		if (const picojson::object* pTrade = jsonChildObj(*pAiMeta, "trade"))
		{
			m_iMaxGoldTradePercent        = jsonIdInt(*pTrade, "maxGoldPercent");
			m_iMaxGoldPerTurnTradePercent = jsonIdInt(*pTrade, "maxGoldPerTurnPercent");
			m_iNoTechTradeThreshold       = jsonIdInt(*pTrade, "noTechTradeThreshold");
			m_iTechTradeKnownPercent      = jsonIdInt(*pTrade, "techTradeKnownPercent");
		}

		// ai.attitude.<relation>.{change,divisor,changeLimit} -- an absent component reads 0.
		if (const picojson::object* pAttitude = jsonChildObj(*pAiMeta, "attitude"))
		{
			for (int iRelation = 0; iRelation < NUM_LEADER_DIPLO_RELATIONS; ++iRelation)
			{
				const picojson::object* pRelation = jsonChildObj(*pAttitude, LH_DIPLO_RELATION_KEYS[iRelation]);
				if (pRelation == NULL)
				{
					continue;
				}
				m_aiAttitudeChange[iRelation]      = jsonIdInt(*pRelation, "change");
				m_aiAttitudeDivisor[iRelation]     = jsonIdInt(*pRelation, "divisor");
				m_aiAttitudeChangeLimit[iRelation] = jsonIdInt(*pRelation, "changeLimit");
			}
		}

		// ai.refuse.<interaction> = "ATTITUDE_*" FK string (absent -> -1).
		if (const picojson::object* pRefuse = jsonChildObj(*pAiMeta, "refuse"))
		{
			for (int iRefusal = 0; iRefusal < NUM_LEADER_REFUSALS; ++iRefusal)
			{
				m_aiRefuseAttitudeThreshold[iRefusal] = jsonIdFk(*pRefuse, LH_REFUSAL_KEYS[iRefusal]);
			}
		}

		// ai.memory.{decay,attitudePercent} keyed by MEMORY_*; ai.contact.{rand,delay} keyed by CONTACT_*.
		if (const picojson::object* pMemory = jsonChildObj(*pAiMeta, "memory"))
		{
			lh_fillEnumSlots(*pMemory, "decay", m_aiMemoryDecayRand, NUM_MEMORY_TYPES);
			lh_fillEnumSlots(*pMemory, "attitudePercent", m_aiMemoryAttitudePercent, NUM_MEMORY_TYPES);
		}
		if (const picojson::object* pContact = jsonChildObj(*pAiMeta, "contact"))
		{
			lh_fillEnumSlots(*pContact, "rand", m_aiContactRand, NUM_CONTACT_TYPES);
			lh_fillEnumSlots(*pContact, "delay", m_aiContactDelay, NUM_CONTACT_TYPES);
		}

		// flat keyed maps directly under ai.
		lh_fillEnumSlots(*pAiMeta, "noWarProb", m_aiNoWarAttitudeProb, NUM_ATTITUDE_TYPES);
		lh_fillEnumSlots(*pAiMeta, "unitWeights", m_aiUnitAIWeightModifier, NUM_UNITAI_TYPES);
		jsonReadFkMap(*pAiMeta, "improvementWeights", m_improvementWeights);

		// ai.favorites.{civic,religion} -- FK strings (absent -> -1).
		if (const picojson::object* pFavorites = jsonChildObj(*pAiMeta, "favorites"))
		{
			m_iFavoriteCivic    = jsonIdFk(*pFavorites, "civic");
			m_iFavoriteReligion = jsonIdFk(*pFavorites, "religion");
		}
	}

	// --- sound.diplo* -> the four era-keyed music tables (RUNTIME audio-tag indices, NOT info ids) ---
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		for (int iMusic = 0; iMusic < NUM_LEADER_DIPLO_MUSIC; ++iMusic)
		{
			lh_fillDiploMusic(*pSound, LH_DIPLO_MUSIC_KEYS[iMusic], m_diploMusicScriptIds[iMusic]);
		}
	}

	// --- the non-zero engine default tables (header doc): fill only slots the JSON left at 0 ---
	overlayDefaultMemoryValues();
	overlayDefaultContactValues();
}


void CvLeaderHeadInfo::resetMapped()
{
	m_bNPC = false;
	m_iBaseAttitude = 0;
	m_iBasePeaceWeight = 0;
	m_iPeaceWeightRand = 0;
	m_iWarmongerRespect = 0;
	m_iEspionageWeight = 0;
	m_iWonderConstructRand = 0;
	m_iBuildUnitProb = 0;
	// NOT 0 -- a zero depth would search nothing at all. An entity with no `ai.personality` block never reaches
	// the mapFrom read above, so the unauthored default has to be established here too.
	m_iResearchSearchDepth = DEFAULT_RESEARCH_SEARCH_DEPTH;
	m_iFreedomAppreciation = 0;
	m_iVassalPowerModifier = 0;
	m_iMaxWarRand = 0;
	m_iMaxWarNearbyPowerRatio = 0;
	m_iMaxWarDistantPowerRatio = 0;
	m_iMaxWarMinAdjacentLandPercent = 0;
	m_iLimitedWarRand = 0;
	m_iLimitedWarPowerRatio = 0;
	m_iDogpileWarRand = 0;
	m_iMakePeaceRand = 0;
	m_iDeclareWarTradeRand = 0;
	m_iDemandRebukedSneakProb = 0;
	m_iDemandRebukedWarProb = 0;
	m_iRefuseToTalkWarThreshold = 0;
	m_iBaseAttackOddsChange = 0;
	m_iAttackOddsChangeRand = 0;
	m_iRazeCityProb = 0;
	m_iMaxGoldTradePercent = 0;
	m_iMaxGoldPerTurnTradePercent = 0;
	m_iNoTechTradeThreshold = 0;
	m_iTechTradeKnownPercent = 0;
	m_iFavoriteCivic = -1;
	m_iFavoriteReligion = -1;
	m_szArtDefineTag = "";

	for (int iRelation = 0; iRelation < NUM_LEADER_DIPLO_RELATIONS; ++iRelation)
	{
		m_aiAttitudeChange[iRelation] = 0;
		m_aiAttitudeDivisor[iRelation] = 0;
		m_aiAttitudeChangeLimit[iRelation] = 0;
	}
	for (int iRefusal = 0; iRefusal < NUM_LEADER_REFUSALS; ++iRefusal)
	{
		m_aiRefuseAttitudeThreshold[iRefusal] = -1;
	}
	for (int iPursuit = 0; iPursuit < NUM_LEADER_VICTORY_PURSUITS; ++iPursuit)
	{
		m_aiVictoryWeight[iPursuit] = 0;
	}
	for (int iContact = 0; iContact < NUM_CONTACT_TYPES; ++iContact)
	{
		m_aiContactRand[iContact] = 0;
		m_aiContactDelay[iContact] = 0;
	}
	for (int iMemory = 0; iMemory < NUM_MEMORY_TYPES; ++iMemory)
	{
		m_aiMemoryDecayRand[iMemory] = 0;
		m_aiMemoryAttitudePercent[iMemory] = 0;
	}
	for (int iAttitude = 0; iAttitude < NUM_ATTITUDE_TYPES; ++iAttitude)
	{
		m_aiNoWarAttitudeProb[iAttitude] = 0;
	}
	for (int iUnitAI = 0; iUnitAI < NUM_UNITAI_TYPES; ++iUnitAI)
	{
		m_aiUnitAIWeightModifier[iUnitAI] = 0;
	}

	m_flavours.clear();
	m_improvementWeights.clear();
	m_aiTraits.clear();
	m_aiComplexTraits.clear();
	for (int iMusic = 0; iMusic < NUM_LEADER_DIPLO_MUSIC; ++iMusic)
	{
		m_diploMusicScriptIds[iMusic].clear();
	}
}


void CvLeaderHeadInfo::overlayDefaultMemoryValues()
{
	for (int iMemory = 0; iMemory < NUM_MEMORY_TYPES; iMemory++)
	{
		if (m_aiMemoryDecayRand[iMemory] == 0)
		{
			switch (iMemory)
			{
				case MEMORY_WARMONGER:
				case MEMORY_MADE_PEACE:
				{
					m_aiMemoryDecayRand[iMemory] = 1;
					break;
				}
				case MEMORY_RECALLED_AMBASSADOR:
				{
					m_aiMemoryDecayRand[iMemory] = 25;
					break;
				}
				case MEMORY_INQUISITION:
				{
					m_aiMemoryDecayRand[iMemory] = 75;
					break;
				}
				case MEMORY_ENSLAVED_CITIZENS:
				{
					m_aiMemoryDecayRand[iMemory] = 100;
					break;
				}
				case MEMORY_SACKED_CITY:
				{
					m_aiMemoryDecayRand[iMemory] = 125;
					break;
				}
				case MEMORY_SACKED_HOLY_CITY:
				{
					m_aiMemoryDecayRand[iMemory] = 200;
					break;
				}
				case MEMORY_BACKSTAB:
				case MEMORY_BACKSTAB_FRIEND:
				{
					m_aiMemoryDecayRand[iMemory] = 250;
				}
			}
		}
		if (m_aiMemoryAttitudePercent[iMemory] == 0)
		{
			switch (iMemory)
			{
				case MEMORY_INQUISITION:
				{
					m_aiMemoryAttitudePercent[iMemory] = -100;
					break;
				}
				case MEMORY_BACKSTAB_FRIEND:
				{
					m_aiMemoryAttitudePercent[iMemory] = -150;
					break;
				}
				case MEMORY_SACKED_CITY:
				case MEMORY_ENSLAVED_CITIZENS:
				{
					m_aiMemoryAttitudePercent[iMemory] = -200;
					break;
				}
				case MEMORY_SACKED_HOLY_CITY:
				case MEMORY_BACKSTAB:
				{
					m_aiMemoryAttitudePercent[iMemory] = -400;
				}
			}
		}
	}
}


void CvLeaderHeadInfo::overlayDefaultContactValues()
{
	for (int iContact = 0; iContact < NUM_CONTACT_TYPES; iContact++)
	{
		if (m_aiContactRand[iContact] == 0)
		{
			switch (iContact)
			{
				case CONTACT_TRADE_JOIN_WAR:
				case CONTACT_TRADE_BUY_WAR:
				{
					m_aiContactRand[iContact] = 10;
					break;
				}
				case CONTACT_TRADE_CONTACTS:
				{
					m_aiContactRand[iContact] = 15;
					break;
				}
				case CONTACT_TRADE_STOP_TRADING:
				case CONTACT_TRADE_MILITARY_UNITS:
				{
					m_aiContactRand[iContact] = 20;
					break;
				}
				case CONTACT_EMBASSY:
				case CONTACT_SECRETARY_GENERAL_VOTE:
				case CONTACT_TRADE_WORKERS:
				{
					m_aiContactRand[iContact] = 25;
					break;
				}
				case CONTACT_TRADE_CORPORATION:
				{
					m_aiContactRand[iContact] = 35;
					break;
				}
				case CONTACT_PEACE_PRESSURE:
				{
					m_aiContactRand[iContact] = 50;
				}
			}
		}
		if (m_aiContactDelay[iContact] == 0)
		{
			switch (iContact)
			{
				case CONTACT_TRADE_BUY_WAR:
				{
					m_aiContactDelay[iContact] = 10;
					break;
				}
				case CONTACT_EMBASSY:
				case CONTACT_TRADE_JOIN_WAR:
				case CONTACT_TRADE_CONTACTS:
				case CONTACT_TRADE_STOP_TRADING:
				{
					m_aiContactDelay[iContact] = 20;
					break;
				}
				case CONTACT_SECRETARY_GENERAL_VOTE:
				case CONTACT_TRADE_MILITARY_UNITS:
				{
					m_aiContactDelay[iContact] = 25;
					break;
				}
				case CONTACT_PEACE_PRESSURE:
				case CONTACT_TRADE_WORKERS:
				{
					m_aiContactDelay[iContact] = 30;
					break;
				}
				case CONTACT_TRADE_CORPORATION:
				{
					m_aiContactDelay[iContact] = 50;
				}
			}
		}
	}
}
