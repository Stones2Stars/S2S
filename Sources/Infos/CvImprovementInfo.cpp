//
//	CvImprovementInfo -- the improvement poco's own typed reading on top of the base section dispatch (see the
//	header). The yield/culture/defense families compile into m_modifiers via the base dispatch -- no per-family
//	raw read survives here ([DEC-new-getter-surface]); the conditioned tile-output entries (irrigated /
//	riverside / per-bonus / tech-gated + the reverse-landed cross-entity boosts) live on the compiled
//	conditioned list. mapFrom materializes the identity/mapGeneration census set AND the placement/validity
//	verdicts (ONE walk of the composed requires.build tree) into typed members ([DEC-materialize-at-mapfrom]);
//	idempotent by contract.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvImprovementInfo.h"
#include "UI/CvArtFileMgr.h"      // ARTFILEMGR -- the EXE-shim-merge getArtInfo()
#include "Infos/CvArtInfoImprovement.h"   // complete CvArtInfoImprovement -- getButton() needs the full definition
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonIdInt/jsonIdBool/jsonIdFk/jsonIdStr/jsonWorldArt)
#include "Property/CvPropertyBridge.h" // the shared `triggers` PROPERTY pulse -> manipulator walk

CvImprovementInfo::CvImprovementInfo()
	: m_iPillageGold(0)
	, m_iCultureRange(0)
	, m_iFeatureGrowthProbability(0)
	, m_iUpgradeTime(0)
	, m_iAdvancedStartCost(100)
	, m_iWorldSoundscapeScriptId(-1)
	, m_iUniqueRange(0)
	, m_iGoodyUniqueRange(0)
	, m_iTilesPerGoody(0)
	, m_eImprovementUpgrade(NO_IMPROVEMENT)
	, m_eImprovementPillage(NO_IMPROVEMENT)
	, m_eBonusChange(NO_BONUS)
	, m_bMilitaryStructure(false)
	, m_bCarriesIrrigation(false)
	, m_bOutsideBorders(false)
	, m_bExtraterrestrial(false)
	, m_bUniversalBonusTrade(false)
	, m_bUpgradeRequiresFortify(false)
	, m_bPlacesBonus(false)
	, m_bPlacesFeature(false)
	, m_bPlacesTerrain(false)
	, m_bChangeRemove(false)
	, m_bGoody(false)
	, m_bRequiresRiverSide(false)
	, m_ePrereqTech(NO_TECH)
	, m_bRequiresFeature(false)
	, m_bRequiresFlatlands(false)
	, m_bRequiresIrrigation(false)
	, m_bWaterImprovement(false)
	, m_bCanMoveSeaUnits(false)
	, m_bPeakImprovement(false)
	, m_bNoFreshWater(false)
	, m_bHillsMakesValid(false)
	, m_bFreshWaterMakesValid(false)
	, m_bRiverSideMakesValid(false)
	, m_bPeakMakesValid(false)
{
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		m_aiPrereqNatureYield[iYield] = 0;
	}
}

//
//	The placement/validity tree walk -- curate_improvement.py's requires_improvement() shapes:
//	  - a bare token (IS_WATER/HAS_PEAK/HAS_IRRIGATION/IS_FLATLANDS/HAS_FEATURE/IS_LAND/HAS_COAST/HAS_RIVER/
//	    HAS_HILLS/HAS_FRESHWATER) -> a PREDICATE node (id == -1, unparameterized).
//	  - {terrain|feature:[...]} membership sugar -> a GROUP node whose anyOf holds per-item HAS_TERRAIN/
//	    HAS_FEATURE PREDICATE nodes (id == the resolved TERRAIN_/FEATURE_ engine id).
//	  - {bonus:[...]} membership sugar -> a GROUP node whose anyOf holds per-item PRESENCE nodes (id == the
//	    resolved BONUS_ engine id, `type` retains the "BONUS_..." string).
//	  - the improvement's PrereqTech -> a PRESENCE node directly in `all` (`type` == "TECH_...").
//	A length-1 "MakesValid" OR-alternative COLLAPSES to a bare token sitting DIRECTLY in `all` --
//	indistinguishable BY SHAPE ALONE from the true AND-mandatory token of the same name. Verified against the
//	live CIV4ImprovementInfos.xml (the only two collision-capable pairs in the whole file): every record with
//	bPeakMakesValid=1 ALSO has bPeakImprovement=1 (mountain_mine/radio_tower/machu_picchu); the one
//	bPeakImprovement-only record (early_mountain_mine) produces a single, non-duplicated direct token. So
//	counting DIRECT (an `all`-chain member) vs NESTED (reached via at least one anyOf hop) occurrences recovers
//	both booleans for every real record: isPeakImprovement = "direct count >= 1"; isPeakMakesValid = "nested,
//	OR a SECOND direct occurrence". isRiverSideMakesValid nets out the one guaranteed direct hit
//	m_bRequiresRiverSide contributes when true.
//

namespace
{
	bool ii_condHasPredicate(const CvCondition* pCondition, CvCascPredKind ePredicate, int iId)
	{
		if (pCondition == NULL)
		{
			return false;
		}
		if (pCondition->kind == CASC_COND_PREDICATE && pCondition->predKind == ePredicate && pCondition->id == iId)
		{
			return true;
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				if (ii_condHasPredicate(pCondition->all[iChild], ePredicate, iId))
				{
					return true;
				}
			}
			for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
			{
				if (ii_condHasPredicate(pCondition->anyOf[iChild], ePredicate, iId))
				{
					return true;
				}
			}
		}
		return false;
	}

	// Collect every parameterized occurrence of a predicate kind into an id set (the {terrain|feature:[...]}
	// membership sugar), and every BONUS_ presence atom (the {bonus:[...]} sugar -- PRESENCE nodes, filtered on
	// the retained `type` string so a numerically-coincident id from another FK space cannot collide).
	void ii_collectPredicateIds(const CvCondition* pCondition, CvCascPredKind ePredicate, std::set<int>& idsOut)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PREDICATE && pCondition->predKind == ePredicate && pCondition->id >= 0)
		{
			idsOut.insert(pCondition->id);
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				ii_collectPredicateIds(pCondition->all[iChild], ePredicate, idsOut);
			}
			for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
			{
				ii_collectPredicateIds(pCondition->anyOf[iChild], ePredicate, idsOut);
			}
		}
	}

	void ii_collectBonusPresenceIds(const CvCondition* pCondition, std::set<int>& idsOut)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PRESENCE && pCondition->id >= 0
			&& pCondition->type.compare(0, 6, "BONUS_") == 0)
		{
			idsOut.insert(pCondition->id);
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				ii_collectBonusPresenceIds(pCondition->all[iChild], idsOut);
			}
			for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
			{
				ii_collectBonusPresenceIds(pCondition->anyOf[iChild], idsOut);
			}
		}
	}

	// The {natureYield:{<channel>:N}} placement thresholds -- CASC_PRED_NATURE_YIELD predicate nodes on the
	// MANDATORY chain (`all`, incl. a multi-channel object's own all-group; the channel rides `id`, the
	// threshold `min`). anyOf alternatives are never walked: a threshold is an AND gate (the engine's
	// CvPlot::canHaveImprovement calculateNatureYield >= prereq test), and the curator authors it only in
	// requires.build.all.
	void ii_collectNatureYieldMins(const CvCondition* pCondition, int* aiMinsOut)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PREDICATE && pCondition->predKind == CASC_PRED_NATURE_YIELD
			&& pCondition->id >= 0 && pCondition->id < NUM_YIELD_TYPES && pCondition->min >= 0)
		{
			aiMinsOut[pCondition->id] = pCondition->min;
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				ii_collectNatureYieldMins(pCondition->all[iChild], aiMinsOut);
			}
		}
	}

	// direct = occurrences of the bare predicate reached WITHOUT crossing an anyOf hop; nested = occurrences
	// through at least one anyOf hop (the "MakesValid" OR-alternative position). See the block comment above.
	void ii_countPredicate(const CvCondition* pCondition, CvCascPredKind ePredicate, bool bViaAny,
		int& iDirect, int& iNested)
	{
		if (pCondition == NULL)
		{
			return;
		}
		if (pCondition->kind == CASC_COND_PREDICATE && pCondition->predKind == ePredicate)
		{
			if (bViaAny)
			{
				++iNested;
			}
			else
			{
				++iDirect;
			}
			return;
		}
		if (pCondition->kind == CASC_COND_GROUP)
		{
			for (size_t iChild = 0; iChild < pCondition->all.size(); ++iChild)
			{
				ii_countPredicate(pCondition->all[iChild], ePredicate, bViaAny, iDirect, iNested);
			}
			for (size_t iChild = 0; iChild < pCondition->anyOf.size(); ++iChild)
			{
				ii_countPredicate(pCondition->anyOf[iChild], ePredicate, true, iDirect, iNested);
			}
		}
	}
}

// The ONE requires.build walk -> the typed placement/validity members (getters are bare reads). Runs after
// the base section dispatch each mapFrom, so the full-registry re-map re-materializes against fully-resolved
// FK ids. Clear-first/assign-all -- idempotent.
void CvImprovementInfo::materializeValidity()
{
	m_ePrereqTech = NO_TECH;
	m_bRequiresFeature = false;
	m_bRequiresFlatlands = false;
	m_bRequiresIrrigation = false;
	m_bWaterImprovement = false;
	m_bCanMoveSeaUnits = false;
	m_bPeakImprovement = false;
	m_bNoFreshWater = false;
	m_bHillsMakesValid = false;
	m_bFreshWaterMakesValid = false;
	m_bRiverSideMakesValid = false;
	m_bPeakMakesValid = false;
	m_terrainMakesValid.clear();
	m_featureMakesValid.clear();
	m_bonusMakesValid.clear();
	for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
	{
		m_aiPrereqNatureYield[iYield] = 0;
	}

	const CvCondition* pBuild = m_requires.build;
	if (pBuild == NULL)
	{
		return;
	}
	// the improvement's PrereqTech: a TECH_ PRESENCE node directly in `all`
	for (size_t iChild = 0; iChild < pBuild->all.size(); ++iChild)
	{
		const CvCondition* pChild = pBuild->all[iChild];
		if (pChild != NULL && pChild->kind == CASC_COND_PRESENCE && pChild->type.compare(0, 5, "TECH_") == 0)
		{
			m_ePrereqTech = (TechTypes)pChild->id;
			break;
		}
	}
	m_bRequiresFeature = ii_condHasPredicate(pBuild, CASC_PRED_HAS_FEATURE, -1);
	m_bRequiresFlatlands = ii_condHasPredicate(pBuild, CASC_PRED_IS_FLATLANDS, -1);
	m_bRequiresIrrigation = ii_condHasPredicate(pBuild, CASC_PRED_HAS_IRRIGATION, -1);
	m_bWaterImprovement = ii_condHasPredicate(pBuild, CASC_PRED_IS_WATER, -1);
	m_bCanMoveSeaUnits = ii_condHasPredicate(pBuild, CASC_PRED_IS_LAND, -1)
		&& ii_condHasPredicate(pBuild, CASC_PRED_HAS_COAST, -1);
	m_bHillsMakesValid = ii_condHasPredicate(pBuild, CASC_PRED_HAS_HILLS, -1);
	m_bFreshWaterMakesValid = ii_condHasPredicate(pBuild, CASC_PRED_HAS_FRESHWATER, -1);
	// the direct-vs-nested disambiguation pair (peak; riverside nets out the mapGeneration flag's direct hit)
	int iDirect = 0;
	int iNested = 0;
	ii_countPredicate(pBuild, CASC_PRED_HAS_PEAK, false, iDirect, iNested);
	m_bPeakImprovement = iDirect >= 1;
	m_bPeakMakesValid = iNested > 0 || iDirect >= 2;
	iDirect = 0;
	iNested = 0;
	ii_countPredicate(pBuild, CASC_PRED_HAS_RIVER, false, iDirect, iNested);
	m_bRiverSideMakesValid = iNested > 0 || iDirect > (m_bRequiresRiverSide ? 1 : 0);
	// requires.build.noneOf HAS_FRESHWATER -> the no-fresh-water exclusion
	for (size_t iChild = 0; iChild < pBuild->noneOf.size(); ++iChild)
	{
		const CvCondition* pChild = pBuild->noneOf[iChild];
		if (pChild != NULL && pChild->kind == CASC_COND_PREDICATE && pChild->predKind == CASC_PRED_HAS_FRESHWATER)
		{
			m_bNoFreshWater = true;
			break;
		}
	}
	// the keyed membership sets
	ii_collectPredicateIds(pBuild, CASC_PRED_HAS_TERRAIN, m_terrainMakesValid);
	ii_collectPredicateIds(pBuild, CASC_PRED_HAS_FEATURE, m_featureMakesValid);
	ii_collectBonusPresenceIds(pBuild, m_bonusMakesValid);
	// the {natureYield} placement thresholds (CASC_PRED_NATURE_YIELD atoms on the mandatory chain)
	ii_collectNatureYieldMins(pBuild, m_aiPrereqNatureYield);
}

void CvImprovementInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h contract): fully define every accumulating member.
	// NB m_aeBuildTypes is NOT cleared here -- the reverse pass owns it (clear-first), not this parse.
	m_aeMapCategories.clear();
	m_aiAlternativeImprovementUpgradeTypes.clear();
	m_aiFeatureChangeTypes.clear();
	m_bonusTradeIds.clear();
	m_bonusDiscoverRand.clear();
	m_bonusDepletionRand.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (requires tree + the modifier compile)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: the improvement's `triggers` property pulses feed the KEEP-legacy solver
	// via the plot gather. The ONE shared pulse walk (clear-and-refill inside).
	CascadePropertyBridge::bridgePulses(getTriggers(), m_PropertyManipulators);

	// mapGeneration
	if (const picojson::object* pMapGen = jsonChildObj(entityObj, "mapGeneration"))
	{
		m_iUniqueRange = jsonIdInt(*pMapGen, "uniqueRange");
		m_iGoodyUniqueRange = jsonIdInt(*pMapGen, "goodyRange");
		m_iTilesPerGoody = jsonIdInt(*pMapGen, "tilesPerGoody");
		m_bGoody = jsonIdBool(*pMapGen, "goody");
		m_bRequiresRiverSide = jsonIdBool(*pMapGen, "requiresRiverSide");
	}

	// identity: scalars, FKs, held flags (the store-inverted placement prereqs are NOT here -- they ride
	// requires.build and materialize below)
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iPillageGold = jsonIdInt(*pIdentity, "pillageGold");
		m_iCultureRange = jsonIdInt(*pIdentity, "cultureRange");
		m_iFeatureGrowthProbability = jsonIdInt(*pIdentity, "featureGrowth");
		m_iUpgradeTime = jsonIdInt(*pIdentity, "upgradeTime");
		// upgradesTo/pillageTo are improvement->improvement self-FKs -- the full-registry re-run re-resolves
		// against the complete id space regardless of first-pass ordering.
		m_eImprovementUpgrade = (ImprovementTypes)jsonIdFk(*pIdentity, "upgradesTo");
		m_eImprovementPillage = (ImprovementTypes)jsonIdFk(*pIdentity, "pillageTo");
		m_eBonusChange = (BonusTypes)jsonIdFk(*pIdentity, "bonusChange");

		m_bMilitaryStructure = jsonIdBool(*pIdentity, "militaryStructure");
		m_bCarriesIrrigation = jsonIdBool(*pIdentity, "carriesIrrigation");
		m_bOutsideBorders = jsonIdBool(*pIdentity, "outsideBorders");
		m_bExtraterrestrial = jsonIdBool(*pIdentity, "extraterrestrial");
		m_bUniversalBonusTrade = jsonIdBool(*pIdentity, "universalBonusTrade");
		m_bUpgradeRequiresFortify = jsonIdBool(*pIdentity, "upgradeRequiresFortify");
		m_bPlacesBonus = jsonIdBool(*pIdentity, "placesBonus");
		m_bPlacesFeature = jsonIdBool(*pIdentity, "placesFeature");
		m_bPlacesTerrain = jsonIdBool(*pIdentity, "placesTerrain");
		m_bChangeRemove = jsonIdBool(*pIdentity, "changeRemove");
		if (const picojson::object* pAdvancedStart = jsonChildObj(*pIdentity, "advancedStart"))
		{
			m_iAdvancedStartCost = jsonIdInt(*pAdvancedStart, "cost", 100);   // legacy load default 100
		}

		picojson::object::const_iterator categoriesIter = pIdentity->find("mapCategories");
		if (categoriesIter != pIdentity->end() && categoriesIter->second.is<picojson::array>())
		{
			const picojson::array& categoryList = categoriesIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < categoryList.size(); ++iEntry)
			{
				if (!categoryList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(categoryList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aeMapCategories.push_back((MapCategoryTypes)iResolved);
				}
			}
		}

		picojson::object::const_iterator alternativesIter = pIdentity->find("alternativeUpgrades");
		if (alternativesIter != pIdentity->end() && alternativesIter->second.is<picojson::array>())
		{
			const picojson::array& alternativeList = alternativesIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < alternativeList.size(); ++iEntry)
			{
				if (!alternativeList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(alternativeList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aiAlternativeImprovementUpgradeTypes.push_back(iResolved);
				}
			}
		}

		picojson::object::const_iterator featureChangesIter = pIdentity->find("featureChanges");
		if (featureChangesIter != pIdentity->end() && featureChangesIter->second.is<picojson::array>())
		{
			const picojson::array& featureChangeList = featureChangesIter->second.get<picojson::array>();
			for (size_t iEntry = 0; iEntry < featureChangeList.size(); ++iEntry)
			{
				if (!featureChangeList[iEntry].is<std::string>())
				{
					continue;
				}
				const int iResolved = jsonResolveId(featureChangeList[iEntry].get<std::string>());
				if (iResolved >= 0)
				{
					m_aiFeatureChangeTypes.push_back(iResolved);
				}
			}
		}

		// identity.bonuses.{BONUS}: {trade, discoverRand, depletionRand}
		if (const picojson::object* pBonuses = jsonChildObj(*pIdentity, "bonuses"))
		{
			for (picojson::object::const_iterator bonusIter = pBonuses->begin(); bonusIter != pBonuses->end(); ++bonusIter)
			{
				if (!bonusIter->second.is<picojson::object>())
				{
					continue;
				}
				const int iBonusId = jsonResolveId(bonusIter->first);
				if (iBonusId < 0)
				{
					continue;
				}
				const picojson::object& bonusObj = bonusIter->second.get<picojson::object>();
				picojson::object::const_iterator tradeIter = bonusObj.find("trade");
				if (tradeIter != bonusObj.end() && tradeIter->second.is<bool>() && tradeIter->second.get<bool>())
				{
					m_bonusTradeIds.insert(iBonusId);
				}
				picojson::object::const_iterator discoverIter = bonusObj.find("discoverRand");
				if (discoverIter != bonusObj.end() && discoverIter->second.is<double>())
				{
					m_bonusDiscoverRand[iBonusId] = (int)discoverIter->second.get<double>();
				}
				picojson::object::const_iterator depletionIter = bonusObj.find("depletionRand");
				if (depletionIter != bonusObj.end() && depletionIter->second.is<double>())
				{
					m_bonusDepletionRand[iBonusId] = (int)depletionIter->second.get<double>();
				}
			}
		}
	}

	// world.art.icon -- the ART_DEF_* tag (EXE map-gen art lookup). Cleared first: jsonIdStr only assigns when
	// the key is present (mapFrom idempotency, CvInfo.h).
	m_szArtDefineTag.clear();
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);
	}

	// sound.soundscape -> runtime audio-manager index, resolved at info-load exactly as the archived read did
	// (gDLL->getAudioTagIndex); absent/empty tag leaves the legacy -1 default.
	m_iWorldSoundscapeScriptId = -1;
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		picojson::object::const_iterator soundscapeIter = pSound->find("soundscape");
		if (soundscapeIter != pSound->end() && soundscapeIter->second.is<std::string>()
			&& soundscapeIter->second.get<std::string>().length() > 0)
		{
			m_iWorldSoundscapeScriptId = gDLL->getAudioTagIndex(soundscapeIter->second.get<std::string>().c_str(), AUDIOTAG_SOUNDSCAPE);
		}
	}

	// the placement/validity verdicts -- the ONE walk of the composed requires.build tree the base just parsed
	materializeValidity();
}

bool CvImprovementInfo::isAlternativeImprovementUpgradeType(int iImprovement) const
{
	for (size_t iEntry = 0; iEntry < m_aiAlternativeImprovementUpgradeTypes.size(); ++iEntry)
	{
		if (m_aiAlternativeImprovementUpgradeTypes[iEntry] == iImprovement)
		{
			return true;
		}
	}
	return false;
}

bool CvImprovementInfo::isFeatureChangeType(int iFeature) const
{
	for (size_t iEntry = 0; iEntry < m_aiFeatureChangeTypes.size(); ++iEntry)
	{
		if (m_aiFeatureChangeTypes[iEntry] == iFeature)
		{
			return true;
		}
	}
	return false;
}

const CvArtInfoImprovement* CvImprovementInfo::getArtInfo() const
{
	return ARTFILEMGR.getImprovementArtInfo(getArtDefineTag());
}

const char* CvImprovementInfo::getButton() const
{
	const CvArtInfoImprovement* pArtInfo = getArtInfo();
	return pArtInfo != NULL ? pArtInfo->getButton() : "";
}
