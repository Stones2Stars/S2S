//------------------------------------------------------------------------------------------------
//  FILE:    CvLeaderHeadInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, CvString/CvWString, gDLL, SAFE_DELETE_ARRAY, algo
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvLeaderHeadInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonResolveId
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- getLeaderheadArtInfo / CvArtInfoLeaderhead getters
#include "Defines/CvGlobals.h"    // GC (getNumFlavorTypes / getNumImprovementInfos / getNumEraInfos)


//======================================================================================================
//					CvLeaderHeadInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//  FUNCTION:   CvLeaderHeadInfo()  -- default constructor (all members at their legacy load defaults)
//------------------------------------------------------------------------------------------------------
CvLeaderHeadInfo::CvLeaderHeadInfo()
	// The four "other" refuse thresholds carry NON-(-1) string defaults (the archived read()'s
	// GetOptionalChildXmlValByName third argument): no leaderhead JSON authors them, so they always
	// keep these seeds. ATTITUDE_ANNOYED / ATTITUDE_CAUTIOUS are the AttitudeTypes enum ids.
	: m_iMilitaryUnitRefuseAttitudeThreshold(ATTITUDE_ANNOYED)
	, m_iWorkerRefuseAttitudeThreshold(ATTITUDE_ANNOYED)
	, m_iCorporationRefuseAttitudeThreshold(ATTITUDE_CAUTIOUS)
	, m_iSecretaryGeneralVoteRefuseAttitudeThreshold(ATTITUDE_ANNOYED)
	, m_bNPC(false)
	, m_iWonderConstructRand(0)
	, m_iBaseAttitude(0)
	, m_iBasePeaceWeight(0)
	, m_iPeaceWeightRand(0)
	, m_iWarmongerRespect(0)
	, m_iEspionageWeight(0)
	, m_iRefuseToTalkWarThreshold(0)
	, m_iNoTechTradeThreshold(0)
	, m_iTechTradeKnownPercent(0)
	, m_iMaxGoldTradePercent(0)
	, m_iMaxGoldPerTurnTradePercent(0)
	, m_iCultureVictoryWeight(0)
	, m_iSpaceVictoryWeight(0)
	, m_iConquestVictoryWeight(0)
	, m_iDominationVictoryWeight(0)
	, m_iDiplomacyVictoryWeight(0)
	, m_iMaxWarRand(0)
	, m_iMaxWarNearbyPowerRatio(0)
	, m_iMaxWarDistantPowerRatio(0)
	, m_iMaxWarMinAdjacentLandPercent(0)
	, m_iLimitedWarRand(0)
	, m_iLimitedWarPowerRatio(0)
	, m_iDogpileWarRand(0)
	, m_iMakePeaceRand(0)
	, m_iDeclareWarTradeRand(0)
	, m_iDemandRebukedSneakProb(0)
	, m_iDemandRebukedWarProb(0)
	, m_iRazeCityProb(0)
	, m_iBuildUnitProb(0)
	, m_iBaseAttackOddsChange(0)
	, m_iAttackOddsChangeRand(0)
	, m_iWorseRankDifferenceAttitudeChange(0)
	, m_iBetterRankDifferenceAttitudeChange(0)
	, m_iCloseBordersAttitudeChange(0)
	, m_iLostWarAttitudeChange(0)
	, m_iAtWarAttitudeDivisor(0)
	, m_iAtWarAttitudeChangeLimit(0)
	, m_iAtPeaceAttitudeDivisor(0)
	, m_iAtPeaceAttitudeChangeLimit(0)
	, m_iSameReligionAttitudeChange(0)
	, m_iSameReligionAttitudeDivisor(0)
	, m_iSameReligionAttitudeChangeLimit(0)
	, m_iDifferentReligionAttitudeChange(0)
	, m_iDifferentReligionAttitudeDivisor(0)
	, m_iDifferentReligionAttitudeChangeLimit(0)
	, m_iBonusTradeAttitudeDivisor(0)
	, m_iBonusTradeAttitudeChangeLimit(0)
	, m_iOpenBordersAttitudeDivisor(0)
	, m_iOpenBordersAttitudeChangeLimit(0)
	, m_iDefensivePactAttitudeDivisor(0)
	, m_iDefensivePactAttitudeChangeLimit(0)
	, m_iShareWarAttitudeChange(0)
	, m_iShareWarAttitudeDivisor(0)
	, m_iShareWarAttitudeChangeLimit(0)
	, m_iFavoriteCivicAttitudeChange(0)
	, m_iFavoriteCivicAttitudeDivisor(0)
	, m_iFavoriteCivicAttitudeChangeLimit(0)
	// The 17 *RefuseAttitudeThreshold FKs, FavoriteCivic and FavoriteReligion are enum-as-int type ids
	// that default to -1 (NO_ATTITUDE / NO_CIVIC / NO_RELIGION -- the archived addEnumAsInt init).
	, m_iDemandTributeAttitudeThreshold(-1)
	, m_iNoGiveHelpAttitudeThreshold(-1)
	, m_iTechRefuseAttitudeThreshold(-1)
	, m_iStrategicBonusRefuseAttitudeThreshold(-1)
	, m_iHappinessBonusRefuseAttitudeThreshold(-1)
	, m_iHealthBonusRefuseAttitudeThreshold(-1)
	, m_iMapRefuseAttitudeThreshold(-1)
	, m_iDeclareWarRefuseAttitudeThreshold(-1)
	, m_iDeclareWarThemRefuseAttitudeThreshold(-1)
	, m_iStopTradingRefuseAttitudeThreshold(-1)
	, m_iStopTradingThemRefuseAttitudeThreshold(-1)
	, m_iAdoptCivicRefuseAttitudeThreshold(-1)
	, m_iConvertReligionRefuseAttitudeThreshold(-1)
	, m_iOpenBordersRefuseAttitudeThreshold(-1)
	, m_iDefensivePactRefuseAttitudeThreshold(-1)
	, m_iPermanentAllianceRefuseAttitudeThreshold(-1)
	, m_iVassalRefuseAttitudeThreshold(-1)
	, m_iVassalPowerModifier(0)
	, m_iFreedomAppreciation(0)
	, m_iFavoriteCivic(-1)
	, m_iFavoriteReligion(-1)
	, m_piFlavorValue(NULL)
	, m_piContactRand(NULL)
	, m_piContactDelay(NULL)
	, m_piMemoryDecayRand(NULL)
	, m_piMemoryAttitudePercent(NULL)
	, m_piNoWarAttitudeProb(NULL)
	, m_piUnitAIWeightModifier(NULL)
	, m_piImprovementWeightModifier(NULL)
	, m_piDiploPeaceIntroMusicScriptIds(NULL)
	, m_piDiploPeaceMusicScriptIds(NULL)
	, m_piDiploWarIntroMusicScriptIds(NULL)
	, m_piDiploWarMusicScriptIds(NULL)
{
}


//------------------------------------------------------------------------------------------------------
//  FUNCTION:   ~CvLeaderHeadInfo()  -- default destructor
//------------------------------------------------------------------------------------------------------
CvLeaderHeadInfo::~CvLeaderHeadInfo()
{
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piContactRand);
	SAFE_DELETE_ARRAY(m_piContactDelay);
	SAFE_DELETE_ARRAY(m_piMemoryDecayRand);
	SAFE_DELETE_ARRAY(m_piMemoryAttitudePercent);
	SAFE_DELETE_ARRAY(m_piNoWarAttitudeProb);
	SAFE_DELETE_ARRAY(m_piUnitAIWeightModifier);
	SAFE_DELETE_ARRAY(m_piImprovementWeightModifier);
	SAFE_DELETE_ARRAY(m_piDiploPeaceIntroMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploPeaceMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploWarIntroMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploWarMusicScriptIds);
}


const char* CvLeaderHeadInfo::getButton() const
{
	const CvArtInfoLeaderhead* pLeaderheadArtInfo = getArtInfo();
	return pLeaderheadArtInfo ? pLeaderheadArtInfo->getButton() : NULL;
}


bool CvLeaderHeadInfo::isNPC() const
{
	return m_bNPC;
}


int CvLeaderHeadInfo::getWonderConstructRand() const
{
	return m_iWonderConstructRand;
}


int CvLeaderHeadInfo::getBaseAttitude() const
{
	return m_iBaseAttitude;
}


int CvLeaderHeadInfo::getBasePeaceWeight() const
{
	return m_iBasePeaceWeight;
}


int CvLeaderHeadInfo::getPeaceWeightRand() const
{
	return m_iPeaceWeightRand;
}


int CvLeaderHeadInfo::getWarmongerRespect() const
{
	return m_iWarmongerRespect;
}


int CvLeaderHeadInfo::getEspionageWeight() const
{
	return m_iEspionageWeight;
}


int CvLeaderHeadInfo::getRefuseToTalkWarThreshold() const
{
	return m_iRefuseToTalkWarThreshold;
}


int CvLeaderHeadInfo::getNoTechTradeThreshold() const
{
	return m_iNoTechTradeThreshold;
}


int CvLeaderHeadInfo::getTechTradeKnownPercent() const
{
	return m_iTechTradeKnownPercent;
}


int CvLeaderHeadInfo::getMaxGoldTradePercent() const
{
	return m_iMaxGoldTradePercent;
}


int CvLeaderHeadInfo::getMaxGoldPerTurnTradePercent() const
{
	return m_iMaxGoldPerTurnTradePercent;
}


int CvLeaderHeadInfo::getCultureVictoryWeight() const
{
	return m_iCultureVictoryWeight;
}


int CvLeaderHeadInfo::getSpaceVictoryWeight() const
{
	return m_iSpaceVictoryWeight;
}


int CvLeaderHeadInfo::getConquestVictoryWeight() const
{
	return m_iConquestVictoryWeight;
}


int CvLeaderHeadInfo::getDominationVictoryWeight() const
{
	return m_iDominationVictoryWeight;
}


int CvLeaderHeadInfo::getDiplomacyVictoryWeight() const
{
	return m_iDiplomacyVictoryWeight;
}


int CvLeaderHeadInfo::getMaxWarRand() const
{
	return m_iMaxWarRand;
}


int CvLeaderHeadInfo::getMaxWarNearbyPowerRatio() const
{
	return m_iMaxWarNearbyPowerRatio;
}


int CvLeaderHeadInfo::getMaxWarDistantPowerRatio() const
{
	return m_iMaxWarDistantPowerRatio;
}


int CvLeaderHeadInfo::getMaxWarMinAdjacentLandPercent() const
{
	return m_iMaxWarMinAdjacentLandPercent;
}


int CvLeaderHeadInfo::getLimitedWarRand() const
{
	return m_iLimitedWarRand;
}


int CvLeaderHeadInfo::getLimitedWarPowerRatio() const
{
	return m_iLimitedWarPowerRatio;
}


int CvLeaderHeadInfo::getDogpileWarRand() const
{
	return m_iDogpileWarRand;
}


int CvLeaderHeadInfo::getMakePeaceRand() const
{
	return m_iMakePeaceRand;
}


int CvLeaderHeadInfo::getDeclareWarTradeRand() const
{
	return m_iDeclareWarTradeRand;
}


int CvLeaderHeadInfo::getDemandRebukedSneakProb() const
{
	return m_iDemandRebukedSneakProb;
}


int CvLeaderHeadInfo::getDemandRebukedWarProb() const
{
	return m_iDemandRebukedWarProb;
}


int CvLeaderHeadInfo::getRazeCityProb() const
{
	return m_iRazeCityProb;
}


int CvLeaderHeadInfo::getBuildUnitProb() const
{
	return m_iBuildUnitProb;
}


int CvLeaderHeadInfo::getBaseAttackOddsChange() const
{
	return m_iBaseAttackOddsChange;
}


int CvLeaderHeadInfo::getAttackOddsChangeRand() const
{
	return m_iAttackOddsChangeRand;
}


int CvLeaderHeadInfo::getWorseRankDifferenceAttitudeChange() const
{
	return m_iWorseRankDifferenceAttitudeChange;
}


int CvLeaderHeadInfo::getBetterRankDifferenceAttitudeChange() const
{
	return m_iBetterRankDifferenceAttitudeChange;
}


int CvLeaderHeadInfo::getCloseBordersAttitudeChange() const
{
	return m_iCloseBordersAttitudeChange;
}


int CvLeaderHeadInfo::getLostWarAttitudeChange() const
{
	return m_iLostWarAttitudeChange;
}


int CvLeaderHeadInfo::getAtWarAttitudeDivisor() const
{
	return m_iAtWarAttitudeDivisor;
}


int CvLeaderHeadInfo::getAtWarAttitudeChangeLimit() const
{
	return m_iAtWarAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getAtPeaceAttitudeDivisor() const
{
	return m_iAtPeaceAttitudeDivisor;
}


int CvLeaderHeadInfo::getAtPeaceAttitudeChangeLimit() const
{
	return m_iAtPeaceAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getSameReligionAttitudeChange() const
{
	return m_iSameReligionAttitudeChange;
}


int CvLeaderHeadInfo::getSameReligionAttitudeDivisor() const
{
	return m_iSameReligionAttitudeDivisor;
}


int CvLeaderHeadInfo::getSameReligionAttitudeChangeLimit() const
{
	return m_iSameReligionAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getDifferentReligionAttitudeChange() const
{
	return m_iDifferentReligionAttitudeChange;
}


int CvLeaderHeadInfo::getDifferentReligionAttitudeDivisor() const
{
	return m_iDifferentReligionAttitudeDivisor;
}


int CvLeaderHeadInfo::getDifferentReligionAttitudeChangeLimit() const
{
	return m_iDifferentReligionAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getBonusTradeAttitudeDivisor() const
{
	return m_iBonusTradeAttitudeDivisor;
}


int CvLeaderHeadInfo::getBonusTradeAttitudeChangeLimit() const
{
	return m_iBonusTradeAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getOpenBordersAttitudeDivisor() const
{
	return m_iOpenBordersAttitudeDivisor;
}


int CvLeaderHeadInfo::getOpenBordersAttitudeChangeLimit() const
{
	return m_iOpenBordersAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getDefensivePactAttitudeDivisor() const
{
	return m_iDefensivePactAttitudeDivisor;
}


int CvLeaderHeadInfo::getDefensivePactAttitudeChangeLimit() const
{
	return m_iDefensivePactAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getShareWarAttitudeChange() const
{
	return m_iShareWarAttitudeChange;
}


int CvLeaderHeadInfo::getShareWarAttitudeDivisor() const
{
	return m_iShareWarAttitudeDivisor;
}


int CvLeaderHeadInfo::getShareWarAttitudeChangeLimit() const
{
	return m_iShareWarAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getFavoriteCivicAttitudeChange() const
{
	return m_iFavoriteCivicAttitudeChange;
}


int CvLeaderHeadInfo::getFavoriteCivicAttitudeDivisor() const
{
	return m_iFavoriteCivicAttitudeDivisor;
}


int CvLeaderHeadInfo::getFavoriteCivicAttitudeChangeLimit() const
{
	return m_iFavoriteCivicAttitudeChangeLimit;
}


int CvLeaderHeadInfo::getDemandTributeAttitudeThreshold() const
{
	return m_iDemandTributeAttitudeThreshold;
}


int CvLeaderHeadInfo::getNoGiveHelpAttitudeThreshold() const
{
	return m_iNoGiveHelpAttitudeThreshold;
}


int CvLeaderHeadInfo::getTechRefuseAttitudeThreshold() const
{
	return m_iTechRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getStrategicBonusRefuseAttitudeThreshold() const
{
	return m_iStrategicBonusRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getHappinessBonusRefuseAttitudeThreshold() const
{
	return m_iHappinessBonusRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getHealthBonusRefuseAttitudeThreshold() const
{
	return m_iHealthBonusRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getMapRefuseAttitudeThreshold() const
{
	return m_iMapRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getDeclareWarRefuseAttitudeThreshold() const
{
	return m_iDeclareWarRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getDeclareWarThemRefuseAttitudeThreshold() const
{
	return m_iDeclareWarThemRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getStopTradingRefuseAttitudeThreshold() const
{
	return m_iStopTradingRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getStopTradingThemRefuseAttitudeThreshold() const
{
	return m_iStopTradingThemRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getAdoptCivicRefuseAttitudeThreshold() const
{
	return m_iAdoptCivicRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getConvertReligionRefuseAttitudeThreshold() const
{
	return m_iConvertReligionRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getOpenBordersRefuseAttitudeThreshold() const
{
	return m_iOpenBordersRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getDefensivePactRefuseAttitudeThreshold() const
{
	return m_iDefensivePactRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getPermanentAllianceRefuseAttitudeThreshold() const
{
	return m_iPermanentAllianceRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getVassalRefuseAttitudeThreshold() const
{
	return m_iVassalRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getVassalPowerModifier() const
{
	return m_iVassalPowerModifier;
}


int CvLeaderHeadInfo::getFavoriteCivic() const
{
	return m_iFavoriteCivic;
}


int CvLeaderHeadInfo::getFavoriteReligion() const
{
	return m_iFavoriteReligion;
}


int CvLeaderHeadInfo::getFreedomAppreciation() const
{
	return m_iFreedomAppreciation;
}


const char* CvLeaderHeadInfo::getArtDefineTag() const
{
	return m_szArtDefineTag;
}


// Arrays
//DEFTRAITORIG
bool CvLeaderHeadInfo::hasTrait(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumTraitInfos(), i);
	return algo::any_of_equal(m_aeTraits, static_cast<TraitTypes>(i));
}


int CvLeaderHeadInfo::getFlavorValue(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumFlavorTypes(), i);
	return m_piFlavorValue ? m_piFlavorValue[i] : 0;
}


int CvLeaderHeadInfo::getContactRand(int i) const
{
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, i);
	return m_piContactRand[i];
}


int CvLeaderHeadInfo::getContactDelay(int i) const
{
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, i);
	return m_piContactDelay[i];
}


int CvLeaderHeadInfo::getMemoryDecayRand(int i) const
{
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, i);
	return m_piMemoryDecayRand[i];
}


int CvLeaderHeadInfo::getMemoryAttitudePercent(int i) const
{
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, i);
	return m_piMemoryAttitudePercent[i];
}


int CvLeaderHeadInfo::getNoWarAttitudeProb(int i) const
{
	FASSERT_BOUNDS(0, NUM_ATTITUDE_TYPES, i);
	return m_piNoWarAttitudeProb ? m_piNoWarAttitudeProb[i] : 0;
}


int CvLeaderHeadInfo::getUnitAIWeightModifier(int i) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, i);
	return m_piUnitAIWeightModifier ? m_piUnitAIWeightModifier[i] : 0;
}


int CvLeaderHeadInfo::getImprovementWeightModifier(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumImprovementInfos(), i);
	return m_piImprovementWeightModifier ? m_piImprovementWeightModifier[i] : 0;
}


int CvLeaderHeadInfo::getDiploPeaceIntroMusicScriptIds(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), i);
	return m_piDiploPeaceIntroMusicScriptIds ? m_piDiploPeaceIntroMusicScriptIds[i] : -1;
}


int CvLeaderHeadInfo::getDiploPeaceMusicScriptIds(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), i);
	return m_piDiploPeaceMusicScriptIds ? m_piDiploPeaceMusicScriptIds[i] : -1;
}


int CvLeaderHeadInfo::getDiploWarIntroMusicScriptIds(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), i);
	return m_piDiploWarIntroMusicScriptIds ? m_piDiploWarIntroMusicScriptIds[i] : -1;
}


int CvLeaderHeadInfo::getDiploWarMusicScriptIds(int i) const
{
	FASSERT_BOUNDS(0, GC.getNumEraInfos(), i);
	return m_piDiploWarMusicScriptIds ? m_piDiploWarMusicScriptIds[i] : -1;
}


const char* CvLeaderHeadInfo::getLeaderHead() const
{
	return getArtInfo() ? getArtInfo()->getNIF() : NULL;
}


int CvLeaderHeadInfo::getMilitaryUnitRefuseAttitudeThreshold() const
{
	return m_iMilitaryUnitRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getWorkerRefuseAttitudeThreshold() const
{
	return m_iWorkerRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getCorporationRefuseAttitudeThreshold() const
{
	return m_iCorporationRefuseAttitudeThreshold;
}


int CvLeaderHeadInfo::getSecretaryGeneralVoteRefuseAttitudeThreshold() const
{
	return m_iSecretaryGeneralVoteRefuseAttitudeThreshold;
}


void CvLeaderHeadInfo::setCultureVictoryWeight(int i)
{
	m_iCultureVictoryWeight = i;
}


void CvLeaderHeadInfo::setSpaceVictoryWeight(int i)
{
	m_iSpaceVictoryWeight = i;
}


void CvLeaderHeadInfo::setConquestVictoryWeight(int i)
{
	m_iConquestVictoryWeight = i;
}


void CvLeaderHeadInfo::setDominationVictoryWeight(int i)
{
	m_iDominationVictoryWeight = i;
}


void CvLeaderHeadInfo::setDiplomacyVictoryWeight(int i)
{
	m_iDiplomacyVictoryWeight = i;
}


//Int list Vector without delayed resolution
int CvLeaderHeadInfo::getDefaultTrait(int i) const
{
	return m_aiDefaultTraits[i];
}


int CvLeaderHeadInfo::getNumDefaultTraits() const
{
	return (int)m_aiDefaultTraits.size();
}


bool CvLeaderHeadInfo::isDefaultTrait(int i) const
{
	return algo::any_of_equal(m_aiDefaultTraits, i);
}


int CvLeaderHeadInfo::getDefaultComplexTrait(int i) const
{
	return m_aiDefaultComplexTraits[i];
}


int CvLeaderHeadInfo::getNumDefaultComplexTraits() const
{
	return (int)m_aiDefaultComplexTraits.size();
}


bool CvLeaderHeadInfo::isDefaultComplexTrait(int i) const
{
	return algo::any_of_equal(m_aiDefaultComplexTraits, i);
}


const CvArtInfoLeaderhead* CvLeaderHeadInfo::getArtInfo() const
{
	return ARTFILEMGR.getLeaderheadArtInfo( getArtDefineTag());
}


// ------------------------------------------------------------------------------------------------------
// JSON mapping helpers (file-local, uniquely prefixed -- these translation units unity-batch together,
// so a bare jsonChildArr/etc. would collide with a sibling info's file-local of the same signature).
// ------------------------------------------------------------------------------------------------------

// o[key] as a JSON array child, or NULL.
static const picojson::array* lhChildArr(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::array>()) ? &it->second.get<picojson::array>() : NULL;
}

// Allocate an int[iLen] filled with iDefault (>=0 lengths only; new int[0] is valid). The mid-registry
// first mapFrom may see a 0 count for GC-sized arrays -- the full-registry re-run reallocates correctly.
static int* lhAllocList(int iLen, int iDefault)
{
	if (iLen < 0) iLen = 0;
	int* p = new int[iLen];
	for (int i = 0; i < iLen; ++i) p[i] = iDefault;
	return p;
}

// parent[key] = { "INFOTYPE_X": n } -> arr[jsonResolveId("INFOTYPE_X")] = n (bounded; unresolved keys skipped,
// mirroring the archived SetVariableListTagPair GetInfoClass()!=-1 guard).
static void lhFillKeyed(const picojson::object& parent, const char* key, int* arr, int iLen)
{
	const picojson::object* m = jsonChildObj(parent, key);
	if (m == NULL) return;
	for (picojson::object::const_iterator it = m->begin(); it != m->end(); ++it)
	{
		if (!it->second.is<double>()) continue;
		const int iId = jsonResolveId(it->first);
		if (iId >= 0 && iId < iLen) arr[iId] = (int)it->second.get<double>();
	}
}

// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects -> arr[flavorId] = n.
static void lhFillFlavours(const picojson::object& ai, int* arr, int iLen)
{
	const picojson::array* a = lhChildArr(ai, "flavours");
	if (a == NULL) return;
	for (size_t i = 0; i < a->size(); ++i)
	{
		if (!(*a)[i].is<picojson::object>()) continue;
		const picojson::object& e = (*a)[i].get<picojson::object>();
		for (picojson::object::const_iterator it = e.begin(); it != e.end(); ++it)
		{
			if (!it->second.is<double>()) continue;
			const int iId = jsonResolveId(it->first);
			if (iId >= 0 && iId < iLen) arr[iId] = (int)it->second.get<double>();
		}
	}
}

// sound.diplo* -> the era-indexed audio-script array, EXACTLY the archived SetVariableListTagPairForAudioScripts:
// full music is a { era: AS2D_* } map (resolve era, store gDLL->getAudioTagIndex(script)); intro music is an
// [era,...] LIST carrying no script (each slot resolves to -1 == the default, so the list form is a no-op).
static void lhFillDiploMusic(const picojson::object& snd, const char* key, int* arr, int iLen)
{
	picojson::object::const_iterator it = snd.find(key);
	if (it == snd.end() || !it->second.is<picojson::object>()) return;   // list form -> nothing to set (all -1)
	const picojson::object& m = it->second.get<picojson::object>();
	for (picojson::object::const_iterator e = m.begin(); e != m.end(); ++e)
	{
		const int iEra = jsonResolveId(e->first);
		if (iEra < 0 || iEra >= iLen) continue;
		if (e->second.is<std::string>() && !e->second.get<std::string>().empty())
			arr[iEra] = gDLL->getAudioTagIndex(e->second.get<std::string>().c_str());
	}
}


// #430: the AI leader's personality/diplomacy/strategy surface from Assets/Data/leaderheads/*.json. Base reads
// type + identity text keys + the section census (the whole `ai` tree lands there as an unconsumed modifier-family
// diagnostic -- this class reads it as typed members below, not composed section units). TRAITLESS BY DESIGN: the
// curator strips every leader<->trait assignment (owner 2026-07-01), so m_aeTraits / m_aiDefault*Traits stay empty
// and are never read here. IDEMPOTENT (CvInfo.h): every owned array is deleted + reallocated each call, so the
// full-registry re-run cannot leak or double.
void CvLeaderHeadInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / identity text) + the base section dispatch/census
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	std::string s;

	// --- world.art.icon -> the EXE-bound leaderhead portrait ArtDefineTag ---
	if (const picojson::object* world = jsonChildObj(o, "world"))
		if (const picojson::object* art = jsonChildObj(*world, "art"))
			if (jsonIdStr(*art, "icon", s)) m_szArtDefineTag = s.c_str();

	// --- reset + (re)allocate the owned arrays (idempotency). The four contact/memory arrays MUST be non-NULL:
	//     their getters deref without a guard, and the default tables below index them directly. ---
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piContactRand);
	SAFE_DELETE_ARRAY(m_piContactDelay);
	SAFE_DELETE_ARRAY(m_piMemoryDecayRand);
	SAFE_DELETE_ARRAY(m_piMemoryAttitudePercent);
	SAFE_DELETE_ARRAY(m_piNoWarAttitudeProb);
	SAFE_DELETE_ARRAY(m_piUnitAIWeightModifier);
	SAFE_DELETE_ARRAY(m_piImprovementWeightModifier);
	SAFE_DELETE_ARRAY(m_piDiploPeaceIntroMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploPeaceMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploWarIntroMusicScriptIds);
	SAFE_DELETE_ARRAY(m_piDiploWarMusicScriptIds);

	m_piContactRand           = lhAllocList(NUM_CONTACT_TYPES, 0);
	m_piContactDelay          = lhAllocList(NUM_CONTACT_TYPES, 0);
	m_piMemoryDecayRand       = lhAllocList(NUM_MEMORY_TYPES, 0);
	m_piMemoryAttitudePercent = lhAllocList(NUM_MEMORY_TYPES, 0);
	m_piNoWarAttitudeProb     = lhAllocList(NUM_ATTITUDE_TYPES, 0);
	m_piUnitAIWeightModifier  = lhAllocList(NUM_UNITAI_TYPES, 0);
	m_piFlavorValue           = lhAllocList(GC.getNumFlavorTypes(), 0);
	m_piImprovementWeightModifier = lhAllocList(GC.getNumImprovementInfos(), 0);
	m_piDiploPeaceIntroMusicScriptIds = lhAllocList(GC.getNumEraInfos(), -1);
	m_piDiploPeaceMusicScriptIds      = lhAllocList(GC.getNumEraInfos(), -1);
	m_piDiploWarIntroMusicScriptIds   = lhAllocList(GC.getNumEraInfos(), -1);
	m_piDiploWarMusicScriptIds        = lhAllocList(GC.getNumEraInfos(), -1);

	// --- ai.* : the whole personality/diplomacy tree (all raw ints; curator emitted int() with NO x100) ---
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		m_bNPC = jsonIdBool(*ai, "npc");

		lhFillFlavours(*ai, m_piFlavorValue, GC.getNumFlavorTypes());

		if (const picojson::object* p = jsonChildObj(*ai, "personality"))
		{
			m_iBaseAttitude        = jsonIdInt(*p, "baseAttitude");
			m_iBasePeaceWeight     = jsonIdInt(*p, "basePeaceWeight");
			m_iPeaceWeightRand     = jsonIdInt(*p, "peaceWeightRand");
			m_iWarmongerRespect    = jsonIdInt(*p, "warmongerRespect");
			m_iEspionageWeight     = jsonIdInt(*p, "espionageWeight");
			m_iWonderConstructRand = jsonIdInt(*p, "wonderConstructRand");
			m_iBuildUnitProb       = jsonIdInt(*p, "buildUnitProb");
			m_iFreedomAppreciation = jsonIdInt(*p, "freedomAppreciation");
			m_iVassalPowerModifier = jsonIdInt(*p, "vassalPowerModifier");
		}

		if (const picojson::object* w = jsonChildObj(*ai, "war"))
		{
			m_iMaxWarRand                  = jsonIdInt(*w, "maxWarRand");
			m_iMaxWarNearbyPowerRatio      = jsonIdInt(*w, "maxWarNearbyPowerRatio");
			m_iMaxWarDistantPowerRatio     = jsonIdInt(*w, "maxWarDistantPowerRatio");
			m_iMaxWarMinAdjacentLandPercent = jsonIdInt(*w, "maxWarMinAdjacentLandPercent");
			m_iLimitedWarRand              = jsonIdInt(*w, "limitedWarRand");
			m_iLimitedWarPowerRatio        = jsonIdInt(*w, "limitedWarPowerRatio");
			m_iDogpileWarRand              = jsonIdInt(*w, "dogpileWarRand");
			m_iMakePeaceRand               = jsonIdInt(*w, "makePeaceRand");
			m_iDeclareWarTradeRand         = jsonIdInt(*w, "declareWarTradeRand");
			m_iDemandRebukedSneakProb      = jsonIdInt(*w, "demandRebukedSneakProb");
			m_iDemandRebukedWarProb        = jsonIdInt(*w, "demandRebukedWarProb");
			m_iRefuseToTalkWarThreshold    = jsonIdInt(*w, "refuseToTalkWarThreshold");
			m_iBaseAttackOddsChange        = jsonIdInt(*w, "baseAttackOddsChange");
			m_iAttackOddsChangeRand        = jsonIdInt(*w, "attackOddsChangeRand");
			m_iRazeCityProb                = jsonIdInt(*w, "razeCityProb");
		}

		if (const picojson::object* v = jsonChildObj(*ai, "victory"))
		{
			m_iCultureVictoryWeight    = jsonIdInt(*v, "culture");
			m_iSpaceVictoryWeight      = jsonIdInt(*v, "space");
			m_iConquestVictoryWeight   = jsonIdInt(*v, "conquest");
			m_iDominationVictoryWeight = jsonIdInt(*v, "domination");
			m_iDiplomacyVictoryWeight  = jsonIdInt(*v, "diplomacy");
		}

		if (const picojson::object* t = jsonChildObj(*ai, "trade"))
		{
			m_iMaxGoldTradePercent       = jsonIdInt(*t, "maxGoldPercent");
			m_iMaxGoldPerTurnTradePercent = jsonIdInt(*t, "maxGoldPerTurnPercent");
			m_iNoTechTradeThreshold      = jsonIdInt(*t, "noTechTradeThreshold");
			m_iTechTradeKnownPercent     = jsonIdInt(*t, "techTradeKnownPercent");
		}

		// ai.attitude.<relation>.{change,divisor,changeLimit} -- each relation is its own object.
		if (const picojson::object* at = jsonChildObj(*ai, "attitude"))
		{
			if (const picojson::object* r = jsonChildObj(*at, "worseRankDifference")) m_iWorseRankDifferenceAttitudeChange = jsonIdInt(*r, "change");
			if (const picojson::object* r = jsonChildObj(*at, "betterRankDifference")) m_iBetterRankDifferenceAttitudeChange = jsonIdInt(*r, "change");
			if (const picojson::object* r = jsonChildObj(*at, "closeBorders")) m_iCloseBordersAttitudeChange = jsonIdInt(*r, "change");
			if (const picojson::object* r = jsonChildObj(*at, "lostWar")) m_iLostWarAttitudeChange = jsonIdInt(*r, "change");
			if (const picojson::object* r = jsonChildObj(*at, "atWar"))
			{
				m_iAtWarAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iAtWarAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "atPeace"))
			{
				m_iAtPeaceAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iAtPeaceAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "sameReligion"))
			{
				m_iSameReligionAttitudeChange     = jsonIdInt(*r, "change");
				m_iSameReligionAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iSameReligionAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "differentReligion"))
			{
				m_iDifferentReligionAttitudeChange     = jsonIdInt(*r, "change");
				m_iDifferentReligionAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iDifferentReligionAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "bonusTrade"))
			{
				m_iBonusTradeAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iBonusTradeAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "openBorders"))
			{
				m_iOpenBordersAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iOpenBordersAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "defensivePact"))
			{
				m_iDefensivePactAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iDefensivePactAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "shareWar"))
			{
				m_iShareWarAttitudeChange     = jsonIdInt(*r, "change");
				m_iShareWarAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iShareWarAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
			if (const picojson::object* r = jsonChildObj(*at, "favoriteCivic"))
			{
				m_iFavoriteCivicAttitudeChange     = jsonIdInt(*r, "change");
				m_iFavoriteCivicAttitudeDivisor    = jsonIdInt(*r, "divisor");
				m_iFavoriteCivicAttitudeChangeLimit = jsonIdInt(*r, "changeLimit");
			}
		}

		// ai.refuse.<key> = "ATTITUDE_*" FK string -> the min-attitude thresholds (absent -> -1 == NO_ATTITUDE).
		if (const picojson::object* rf = jsonChildObj(*ai, "refuse"))
		{
			if (jsonIdStr(*rf, "demandTribute", s))    m_iDemandTributeAttitudeThreshold        = jsonResolveId(s);
			if (jsonIdStr(*rf, "noGiveHelp", s))       m_iNoGiveHelpAttitudeThreshold           = jsonResolveId(s);
			if (jsonIdStr(*rf, "tech", s))             m_iTechRefuseAttitudeThreshold           = jsonResolveId(s);
			if (jsonIdStr(*rf, "strategicBonus", s))   m_iStrategicBonusRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "happinessBonus", s))   m_iHappinessBonusRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "healthBonus", s))      m_iHealthBonusRefuseAttitudeThreshold    = jsonResolveId(s);
			if (jsonIdStr(*rf, "map", s))              m_iMapRefuseAttitudeThreshold            = jsonResolveId(s);
			if (jsonIdStr(*rf, "declareWar", s))       m_iDeclareWarRefuseAttitudeThreshold     = jsonResolveId(s);
			if (jsonIdStr(*rf, "declareWarThem", s))   m_iDeclareWarThemRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "stopTrading", s))      m_iStopTradingRefuseAttitudeThreshold    = jsonResolveId(s);
			if (jsonIdStr(*rf, "stopTradingThem", s))  m_iStopTradingThemRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "adoptCivic", s))       m_iAdoptCivicRefuseAttitudeThreshold     = jsonResolveId(s);
			if (jsonIdStr(*rf, "convertReligion", s))  m_iConvertReligionRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "openBorders", s))      m_iOpenBordersRefuseAttitudeThreshold    = jsonResolveId(s);
			if (jsonIdStr(*rf, "defensivePact", s))    m_iDefensivePactRefuseAttitudeThreshold  = jsonResolveId(s);
			if (jsonIdStr(*rf, "permanentAlliance", s)) m_iPermanentAllianceRefuseAttitudeThreshold = jsonResolveId(s);
			if (jsonIdStr(*rf, "vassal", s))           m_iVassalRefuseAttitudeThreshold         = jsonResolveId(s);
		}

		// ai.memory.{decay,attitudePercent} keyed by MEMORY_*; ai.contact.{rand,delay} keyed by CONTACT_*.
		if (const picojson::object* mem = jsonChildObj(*ai, "memory"))
		{
			lhFillKeyed(*mem, "decay", m_piMemoryDecayRand, NUM_MEMORY_TYPES);
			lhFillKeyed(*mem, "attitudePercent", m_piMemoryAttitudePercent, NUM_MEMORY_TYPES);
		}
		if (const picojson::object* ct = jsonChildObj(*ai, "contact"))
		{
			lhFillKeyed(*ct, "rand", m_piContactRand, NUM_CONTACT_TYPES);
			lhFillKeyed(*ct, "delay", m_piContactDelay, NUM_CONTACT_TYPES);
		}

		// flat keyed maps directly under ai: noWarProb (ATTITUDE_*), unitWeights (UNITAI_*), improvementWeights (IMPROVEMENT_*).
		lhFillKeyed(*ai, "noWarProb", m_piNoWarAttitudeProb, NUM_ATTITUDE_TYPES);
		lhFillKeyed(*ai, "unitWeights", m_piUnitAIWeightModifier, NUM_UNITAI_TYPES);
		lhFillKeyed(*ai, "improvementWeights", m_piImprovementWeightModifier, GC.getNumImprovementInfos());

		// ai.favorites.{civic,religion} -- FK strings (CIVIC_* / RELIGION_*); absent -> -1.
		if (const picojson::object* fav = jsonChildObj(*ai, "favorites"))
		{
			if (jsonIdStr(*fav, "civic", s))    m_iFavoriteCivic    = jsonResolveId(s);
			if (jsonIdStr(*fav, "religion", s)) m_iFavoriteReligion = jsonResolveId(s);
		}
	}

	// --- sound.diplo* -> era-indexed audio-script indices (RUNTIME audio-tag indices, NOT info ids) ---
	if (const picojson::object* snd = jsonChildObj(o, "sound"))
	{
		lhFillDiploMusic(*snd, "diploIntroMusicPeace", m_piDiploPeaceIntroMusicScriptIds, GC.getNumEraInfos());
		lhFillDiploMusic(*snd, "diploMusicPeace",      m_piDiploPeaceMusicScriptIds,      GC.getNumEraInfos());
		lhFillDiploMusic(*snd, "diploIntroMusicWar",   m_piDiploWarIntroMusicScriptIds,   GC.getNumEraInfos());
		lhFillDiploMusic(*snd, "diploMusicWar",        m_piDiploWarMusicScriptIds,        GC.getNumEraInfos());
	}

	// --- overlay the legacy NON-ZERO memory/contact default tables onto slots still 0 (the archived read()
	//     post-pass; the curator emits only the values authored in XML, so omitted slots need these) ---
	setDefaultMemoryInfo();
	setDefaultContactInfo();
}



//I'm lazy, so sue me. The XML still overrides this, so no worries.
void CvLeaderHeadInfo::setDefaultMemoryInfo()
{
	PROFILE_EXTRA_FUNC();

	for (int i = 0; i < NUM_MEMORY_TYPES; i++)
	{
		if (m_piMemoryDecayRand[i] == 0)
		{
			switch (i)
			{
				case MEMORY_WARMONGER:
				case MEMORY_MADE_PEACE:
				{
					m_piMemoryDecayRand[i] = 1;
					break;
				}
				case MEMORY_RECALLED_AMBASSADOR:
				{
					m_piMemoryDecayRand[i] = 25;
					break;
				}
				case MEMORY_INQUISITION:
				{
					m_piMemoryDecayRand[i] = 75;
					break;
				}
				case MEMORY_ENSLAVED_CITIZENS:
				{
					m_piMemoryDecayRand[i] = 100;
					break;
				}
				case MEMORY_SACKED_CITY:
				{
					m_piMemoryDecayRand[i] = 125;
					break;
				}
				case MEMORY_SACKED_HOLY_CITY:
				{
					m_piMemoryDecayRand[i] = 200;
					break;
				}
				case MEMORY_BACKSTAB:
				case MEMORY_BACKSTAB_FRIEND:
				{
					m_piMemoryDecayRand[i] = 250;
				}
			}
		}
		if (m_piMemoryAttitudePercent[i] == 0)
		{
			switch (i)
			{
				case MEMORY_INQUISITION:
				{
					m_piMemoryAttitudePercent[i] = -100;
					break;
				}
				case MEMORY_BACKSTAB_FRIEND:
				{
					m_piMemoryAttitudePercent[i] = -150;
					break;
				}
				case MEMORY_SACKED_CITY:
				case MEMORY_ENSLAVED_CITIZENS:
				{
					m_piMemoryAttitudePercent[i] = -200;
					break;
				}
				case MEMORY_SACKED_HOLY_CITY:
				case MEMORY_BACKSTAB:
				{
					m_piMemoryAttitudePercent[i] = -400;
				}
			}
		}
	}
}


void CvLeaderHeadInfo::setDefaultContactInfo()
{
	PROFILE_EXTRA_FUNC();

	for (int i = 0; i < NUM_CONTACT_TYPES; i++)
	{
		if (m_piContactRand[i] == 0)
		{
			switch (i)
			{
				case CONTACT_TRADE_JOIN_WAR:
				case CONTACT_TRADE_BUY_WAR:
				{
					m_piContactRand[i] = 10;
					break;
				}
				case CONTACT_TRADE_CONTACTS:
				{
					m_piContactRand[i] = 15;
					break;
				}
				case CONTACT_TRADE_STOP_TRADING:
				case CONTACT_TRADE_MILITARY_UNITS:
				{
					m_piContactRand[i] = 20;
					break;
				}
				case CONTACT_EMBASSY:
				case CONTACT_SECRETARY_GENERAL_VOTE:
				case CONTACT_TRADE_WORKERS:
				{
					m_piContactRand[i] = 25;
					break;
				}
				case CONTACT_TRADE_CORPORATION:
				{
					m_piContactRand[i] = 35;
					break;
				}
				case CONTACT_PEACE_PRESSURE:
				{
					m_piContactRand[i] = 50;
				}
			}
		}
		if (m_piContactDelay[i] == 0)
		{
			switch (i)
			{
				case CONTACT_TRADE_BUY_WAR:
				{
					m_piContactDelay[i] = 10;
					break;
				}
				case CONTACT_EMBASSY:
				case CONTACT_TRADE_JOIN_WAR:
				case CONTACT_TRADE_CONTACTS:
				case CONTACT_TRADE_STOP_TRADING:
				{
					m_piContactDelay[i] = 20;
					break;
				}
				case CONTACT_SECRETARY_GENERAL_VOTE:
				case CONTACT_TRADE_MILITARY_UNITS:
				{
					m_piContactDelay[i] = 25;
					break;
				}
				case CONTACT_PEACE_PRESSURE:
				case CONTACT_TRADE_WORKERS:
				{
					m_piContactDelay[i] = 30;
					break;
				}
				case CONTACT_TRADE_CORPORATION:
				{
					m_piContactDelay[i] = 50;
				}
			}
		}
	}
}
