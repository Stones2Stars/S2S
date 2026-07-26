#pragma once

#ifndef CV_LEADER_HEAD_INFO_H
#define CV_LEADER_HEAD_INFO_H

//
//	CvLeaderHeadInfo -- the LEADERHEAD poco on the exemplar surface (patterns.md § THE GETTER SETUP). A
//	leader personality is pure json.md §7 `ai` METADATA (flavours, weights, personality -- drives AI
//	behaviour, never rules) plus identity text, the portrait art tag, and the era-keyed diplomacy music:
//	the authored census (Assets/Data/leaderheads/*.json, 119 entities) carries NO modifier family and NO
//	enabler section, so the type composes no section unit. JSON-fed via mapFrom; no XML read
//	(DEC-no-xml-into-game).
//
//	The grouped tables each hold ONE typed member read by ONE getter parameterized over the group's
//	natural index (never a per-key hand getter). Engine-enumerated axes reuse the engine enum
//	(ContactTypes / MemoryTypes / AttitudeTypes / UnitAITypes; info-keyed axes are sparse id-maps --
//	FlavorTypes / ImprovementTypes / EraTypes). The four axes the engine does not enumerate are declared
//	here, in authored-key order: the per-relation attitude adjustments, the per-interaction refuse
//	thresholds, the victory pursuit weights, and the diplomacy music tables.
//
//	TRAITLESS BY DESIGN: the curator strips every leader<->trait assignment (owner ruling; assignment
//	returns as authored data serving a fresh read) -- no trait member or getter exists here.
//
//	Values are raw human config (the §7 ai plane carries no magnitude entering cascade math).
//

#include "CvInfo.h"   // JSON-info base (mapFrom); on /I -> bare include
#include <map>

namespace picojson { class value; }

// ai.attitude.<relation> -- the per-relation diplomacy adjustment axis. Each relation authors up to
// three components (change / divisor / changeLimit), read by the three relation-parameterized getters.
enum LeaderDiploRelation
{
	DIPLO_RELATION_WORSE_RANK_DIFFERENCE,
	DIPLO_RELATION_BETTER_RANK_DIFFERENCE,
	DIPLO_RELATION_CLOSE_BORDERS,
	DIPLO_RELATION_LOST_WAR,
	DIPLO_RELATION_AT_WAR,
	DIPLO_RELATION_AT_PEACE,
	DIPLO_RELATION_SAME_RELIGION,
	DIPLO_RELATION_DIFFERENT_RELIGION,
	DIPLO_RELATION_BONUS_TRADE,
	DIPLO_RELATION_OPEN_BORDERS,
	DIPLO_RELATION_DEFENSIVE_PACT,
	DIPLO_RELATION_SHARE_WAR,
	DIPLO_RELATION_FAVORITE_CIVIC,
	NUM_LEADER_DIPLO_RELATIONS
};

// ai.refuse.<interaction> -- the diplomatic-interaction refusal axis. Each slot holds the minimum
// AttitudeTypes id at which the leader stops refusing that interaction (-1 = no threshold authored).
enum LeaderRefusal
{
	REFUSAL_DEMAND_TRIBUTE,
	REFUSAL_NO_GIVE_HELP,
	REFUSAL_TECH,
	REFUSAL_STRATEGIC_BONUS,
	REFUSAL_HAPPINESS_BONUS,
	REFUSAL_HEALTH_BONUS,
	REFUSAL_MAP,
	REFUSAL_DECLARE_WAR,
	REFUSAL_DECLARE_WAR_THEM,
	REFUSAL_STOP_TRADING,
	REFUSAL_STOP_TRADING_THEM,
	REFUSAL_ADOPT_CIVIC,
	REFUSAL_CONVERT_RELIGION,
	REFUSAL_OPEN_BORDERS,
	REFUSAL_DEFENSIVE_PACT,
	REFUSAL_PERMANENT_ALLIANCE,
	REFUSAL_VASSAL,
	NUM_LEADER_REFUSALS
};

// ai.victory.<pursuit> -- the victory-pursuit weight axis. The data keys five fixed pursuit concepts
// (AI strategy planes), not VICTORY_* info FKs, so the axis is this closed enum.
enum LeaderVictoryPursuit
{
	VICTORY_PURSUIT_CULTURE,
	VICTORY_PURSUIT_SPACE,
	VICTORY_PURSUIT_CONQUEST,
	VICTORY_PURSUIT_DOMINATION,
	VICTORY_PURSUIT_DIPLOMACY,
	NUM_LEADER_VICTORY_PURSUITS
};

// sound.diplo* -- the diplomacy-screen music tables: (peace|war) x (intro|loop), each era-keyed.
enum LeaderDiploMusic
{
	DIPLO_MUSIC_PEACE_INTRO,
	DIPLO_MUSIC_PEACE,
	DIPLO_MUSIC_WAR_INTRO,
	DIPLO_MUSIC_WAR,
	NUM_LEADER_DIPLO_MUSIC
};

class CvArtInfoLeaderhead;

class CvLeaderHeadInfo : public CvInfo
{
public:

	CvLeaderHeadInfo();

	virtual void mapFrom(const picojson::value& entity);

	// ================= 3. GROUPED TABLES -- one parameterized getter per table ==================
	// ai.attitude.<relation>.{change,divisor,changeLimit} -- three quantities over the relation axis.
	int getAttitudeChange(LeaderDiploRelation eRelation) const;
	int getAttitudeDivisor(LeaderDiploRelation eRelation) const;
	int getAttitudeChangeLimit(LeaderDiploRelation eRelation) const;
	// ai.refuse.<interaction> -- the minimum-attitude refusal thresholds (AttitudeTypes id, -1 = none).
	int getRefuseAttitudeThreshold(LeaderRefusal eRefusal) const;
	// ai.victory.<pursuit> -- the victory-pursuit weights.
	int getVictoryWeight(LeaderVictoryPursuit ePursuit) const;
	// ai.contact.{rand,delay} keyed by CONTACT_*.
	int getContactRand(ContactTypes eContact) const;
	int getContactDelay(ContactTypes eContact) const;
	// ai.memory.{decay,attitudePercent} keyed by MEMORY_*.
	int getMemoryDecayRand(MemoryTypes eMemory) const;
	int getMemoryAttitudePercent(MemoryTypes eMemory) const;
	// ai.noWarProb keyed by ATTITUDE_*.
	int getNoWarAttitudeProb(AttitudeTypes eAttitude) const;
	// ai.unitWeights keyed by UNITAI_*.
	int getUnitAIWeightModifier(UnitAITypes eUnitAI) const;
	// ai.flavours -- FLAVOR_* id -> weight (sparse; absent = 0).
	int getFlavorValue(FlavorTypes eFlavor) const;
	// ai.improvementWeights -- IMPROVEMENT_* id -> weight (sparse; absent = 0).
	int getImprovementWeightModifier(ImprovementTypes eImprovement) const;
	// sound.diplo* -- the RUNTIME audio-tag index for a music table at an era (-1 = engine default).
	int getDiploMusicScriptId(LeaderDiploMusic eMusic, EraTypes eEra) const;

	// ================= 4. INTRINSIC -- bare typed reads (raw human config) ======================
	// ai.npc -- a non-playable leader (barbarian / neanderthal / wildlife).
	bool isNPC() const { return m_bNPC; }
	// ai.personality.* -- the disposition knobs.
	int getBaseAttitude() const { return m_iBaseAttitude; }
	int getBasePeaceWeight() const { return m_iBasePeaceWeight; }
	int getPeaceWeightRand() const { return m_iPeaceWeightRand; }
	int getWarmongerRespect() const { return m_iWarmongerRespect; }
	int getEspionageWeight() const { return m_iEspionageWeight; }
	int getWonderConstructRand() const { return m_iWonderConstructRand; }
	int getBuildUnitProb() const { return m_iBuildUnitProb; }
	int getFreedomAppreciation() const { return m_iFreedomAppreciation; }
	int getVassalPowerModifier() const { return m_iVassalPowerModifier; }
	// ai.war.* -- the war-planning knobs (distinct quantities authored as bare scalars, not a keyed set).
	int getMaxWarRand() const { return m_iMaxWarRand; }
	int getMaxWarNearbyPowerRatio() const { return m_iMaxWarNearbyPowerRatio; }
	int getMaxWarDistantPowerRatio() const { return m_iMaxWarDistantPowerRatio; }
	int getMaxWarMinAdjacentLandPercent() const { return m_iMaxWarMinAdjacentLandPercent; }
	int getLimitedWarRand() const { return m_iLimitedWarRand; }
	int getLimitedWarPowerRatio() const { return m_iLimitedWarPowerRatio; }
	int getDogpileWarRand() const { return m_iDogpileWarRand; }
	int getMakePeaceRand() const { return m_iMakePeaceRand; }
	int getDeclareWarTradeRand() const { return m_iDeclareWarTradeRand; }
	int getDemandRebukedSneakProb() const { return m_iDemandRebukedSneakProb; }
	int getDemandRebukedWarProb() const { return m_iDemandRebukedWarProb; }
	int getRefuseToTalkWarThreshold() const { return m_iRefuseToTalkWarThreshold; }
	int getBaseAttackOddsChange() const { return m_iBaseAttackOddsChange; }
	int getAttackOddsChangeRand() const { return m_iAttackOddsChangeRand; }
	int getRazeCityProb() const { return m_iRazeCityProb; }
	// ai.trade.* -- the trade-willingness knobs.
	int getMaxGoldTradePercent() const { return m_iMaxGoldTradePercent; }
	int getMaxGoldPerTurnTradePercent() const { return m_iMaxGoldPerTurnTradePercent; }
	int getNoTechTradeThreshold() const { return m_iNoTechTradeThreshold; }
	int getTechTradeKnownPercent() const { return m_iTechTradeKnownPercent; }
	// ai.favorites.* -- FK ids (-1 = none).
	int getFavoriteCivic() const { return m_iFavoriteCivic; }
	int getFavoriteReligion() const { return m_iFavoriteReligion; }

	// world.art.icon -- the EXE-bound leaderhead portrait plane (ART_DEF_* tag resolved via ARTFILEMGR).
	const char* getArtDefineTag() const { return m_szArtDefineTag; }
	DllExport const CvArtInfoLeaderhead* getArtInfo() const;
	const char* getLeaderHead() const;
	const char* getButton() const;

private:
	// Full redefinition of every mapped member to its load default (mapFrom idempotency, CvInfo.h).
	void resetMapped();
	// The archived non-zero engine default tables: the curator emits only XML-authored values, so
	// memory/contact slots still 0 after the JSON overlay receive these values.
	void overlayDefaultMemoryValues();
	void overlayDefaultContactValues();

	// --- intrinsic config ---
	bool m_bNPC;
	int m_iBaseAttitude;
	int m_iBasePeaceWeight;
	int m_iPeaceWeightRand;
	int m_iWarmongerRespect;
	int m_iEspionageWeight;
	int m_iWonderConstructRand;
	int m_iBuildUnitProb;
	int m_iFreedomAppreciation;
	int m_iVassalPowerModifier;
	int m_iMaxWarRand;
	int m_iMaxWarNearbyPowerRatio;
	int m_iMaxWarDistantPowerRatio;
	int m_iMaxWarMinAdjacentLandPercent;
	int m_iLimitedWarRand;
	int m_iLimitedWarPowerRatio;
	int m_iDogpileWarRand;
	int m_iMakePeaceRand;
	int m_iDeclareWarTradeRand;
	int m_iDemandRebukedSneakProb;
	int m_iDemandRebukedWarProb;
	int m_iRefuseToTalkWarThreshold;
	int m_iBaseAttackOddsChange;
	int m_iAttackOddsChangeRand;
	int m_iRazeCityProb;
	int m_iMaxGoldTradePercent;
	int m_iMaxGoldPerTurnTradePercent;
	int m_iNoTechTradeThreshold;
	int m_iTechTradeKnownPercent;
	int m_iFavoriteCivic;
	int m_iFavoriteReligion;
	CvString m_szArtDefineTag;

	// --- the grouped tables (fixed arrays where the axis is a compile-time enum) ---
	int m_aiAttitudeChange[NUM_LEADER_DIPLO_RELATIONS];
	int m_aiAttitudeDivisor[NUM_LEADER_DIPLO_RELATIONS];
	int m_aiAttitudeChangeLimit[NUM_LEADER_DIPLO_RELATIONS];
	int m_aiRefuseAttitudeThreshold[NUM_LEADER_REFUSALS];
	int m_aiVictoryWeight[NUM_LEADER_VICTORY_PURSUITS];
	int m_aiContactRand[NUM_CONTACT_TYPES];
	int m_aiContactDelay[NUM_CONTACT_TYPES];
	int m_aiMemoryDecayRand[NUM_MEMORY_TYPES];
	int m_aiMemoryAttitudePercent[NUM_MEMORY_TYPES];
	int m_aiNoWarAttitudeProb[NUM_ATTITUDE_TYPES];
	int m_aiUnitAIWeightModifier[NUM_UNITAI_TYPES];

	// --- the grouped tables (sparse id-maps where the axis is info-keyed / runtime-sized) ---
	std::map<int, int> m_flavours;                                       // FLAVOR_* id -> weight
	std::map<int, int> m_improvementWeights;                             // IMPROVEMENT_* id -> weight
	std::map<int, int> m_diploMusicScriptIds[NUM_LEADER_DIPLO_MUSIC];    // era id -> audio-tag index
};

#endif // CV_LEADER_HEAD_INFO_H
