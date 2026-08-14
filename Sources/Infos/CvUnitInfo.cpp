//
//	CvUnitInfo -- the base dispatch fills the composed units (requires/edges/allowed/grants/triggers/gate; the
//	par.6 modifier families compile into CvModifiers; the par.8 skills/tags bool blocks); this subclass parses
//	ONLY what the type genuinely owns: the identity/cost set, the root combat-class FKs, the par.8 builds
//	repertoire, the par.9 spread/groupSpawn/vision/sizeMatters/replacedBy sections, the grants
//	materialization, the ai metadata, and the outcomes system intake. The par.8 keyed targeting/immunity
//	containers materialize from the COMPILED targeted entries (the family-scoped target tokens). NO
//	family-address read survives here ([DEC-materialize-at-mapfrom]: the modifier values are served by the
//	compiled point reads / entry lists, never re-parsed subclass-side).
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvUnitInfo.h"
#include "CvJsonParse.h"            // jsonResolveId / jsonChildObj / jsonId* / jsonWorldArt / jsonReadIdList / jsonReadFlavours
#include "UI/CvArtFileMgr.h"        // ARTFILEMGR -- getArtInfo shim (mirrors the CvBonusInfo shim leaf)
#include "Infos/CvArtInfoUnit.h"    // CvArtInfoUnit complete type -- getButton() call needs the full definition
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, archive mirror)
#include "CvUnitCombatInfo.h"       // the combat classes' sizeMatters bases + flat-combat sums (derived ranks)
#include "CvModifiers.h"            // getModifiers() walk -> the unit's PROPERTY_* emission sources
#include "Property/CvPropertyBridge.h" // the JSON->manipulator translator (the ONE shared walk)

namespace
{
	// o[section][key] = { TYPE: N } -> out[FK id] = N (spread.religion / spread.corporation). The keyed-map read
	// itself is the SHARED jsonReadFkMap; this only walks in to the section first.
	void un_readKeyedIntMap(const picojson::object& parent, const char* szSection, const char* szKey, std::map<int, int>& out)
	{
		const picojson::object* pSection = jsonChildObj(parent, szSection);
		if (pSection != NULL)
		{
			jsonReadFkMap(*pSection, szKey, out);
		}
	}

	// One compiled par.8 keyed-container membership entry (combat.unit.<token>.{TYPE}: true): the family-scoped
	// target tokens (CvInfoKinds INFO_FAMILY_TARGET_TOKENS) compile each map key as a TARGETED COUNT entry --
	// targetSeg = the container token, targetFk = the resolved TYPE id, unconditioned.
	bool un_isCombatKeyedEntry(const CvModEntry* pEntry, int iToken)
	{
		return pEntry->family == MODFAM_COMBAT
			&& pEntry->scope == CASC_SCOPE_UNIT
			&& pEntry->unit == CASC_UNIT_COUNT
			&& pEntry->targetSeg == iToken
			&& pEntry->targetFk >= 0;
	}

	// Materialize one keyed targeting/immunity container from the COMPILED entries into a SET
	// ([DEC-materialize-at-mapfrom] -- the raw-JSON subtree read is gone).
	void un_collectCombatKeyedSet(const CvModifiers& modifiers, const char* szToken, std::set<int>& out)
	{
		const int iToken = modSegmentLookup(szToken);
		if (iToken < 0)
		{
			return;   // the token was never authored anywhere
		}
		const std::vector<CvModEntry*>& entries = modifiers.entries();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (un_isCombatKeyedEntry(entries[i], iToken))
			{
				out.insert(entries[i]->targetFk);
			}
		}
	}

	// Same read into a vector -- the compile walk iterates the authored object in sorted key order (picojson
	// map), so the entry order reproduces the deterministic order the raw read produced (combat.unit.unitTargets).
	void un_collectCombatKeyedList(const CvModifiers& modifiers, const char* szToken, std::vector<int>& out)
	{
		const int iToken = modSegmentLookup(szToken);
		if (iToken < 0)
		{
			return;
		}
		const std::vector<CvModEntry*>& entries = modifiers.entries();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (un_isCombatKeyedEntry(entries[i], iToken))
			{
				out.push_back(entries[i]->targetFk);
			}
		}
	}

	// identity.{feature|terrain}PassableTechs = [ { TYPE: TECH_or_NONE }, ... ] -> out[typeId] = techId (-1 = NONE).
	void un_readPassableTechs(const picojson::object& identity, const char* szKey, std::map<int, int>& out)
	{
		picojson::object::const_iterator iter = identity.find(szKey);
		if (iter == identity.end() || !iter->second.is<picojson::array>())
		{
			return;
		}
		const picojson::array& entries = iter->second.get<picojson::array>();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (!entries[i].is<picojson::object>())
			{
				continue;
			}
			const picojson::object& row = entries[i].get<picojson::object>();
			for (picojson::object::const_iterator rowIter = row.begin(); rowIter != row.end(); ++rowIter)
			{
				const int iSubstrate = jsonResolveId(rowIter->first);
				int iTech = -1;
				if (rowIter->second.is<std::string>())
				{
					iTech = jsonResolveId(rowIter->second.get<std::string>());
				}
				if (iSubstrate >= 0)
				{
					out[iSubstrate] = iTech;
				}
			}
		}
	}

}

CvUnitInfo::CvUnitInfo()
	: m_iCombatClass(-1)
	, m_bSpawnOnly(false)
	, m_iWorth(0)
	, m_iMilitaryWorth(0)
	, m_iXpValueAttack(0)
	, m_iXpValueDefense(0)
	, m_iConscription(0)
	, m_iAggression(5)
	, m_iAnimalCombat(0)
	, m_iCommandRange(0)
	, m_iControlPoints(0)
	, m_iLeaderExperience(0)
	, m_iMinAreaSize(0)
	, m_iEspionagePoints(0)
	, m_iDomain(-1)
	, m_iDefaultUnitAI(-1)
	, m_iSpecialUnitType(-1)
	, m_iAdvisor(-1)
	, m_iLeaderPromotion(-1)
	, m_iReligion(-1)
	, m_iCaptures(-1)
	, m_iWorkRate(0)
	, m_iAirCombat(0)
	, m_iCombatLimit(100)
	, m_iAirCombatLimit(0)
	, m_iAirUnitCap(0)
	, m_iProductionCost(0)
	, m_iUpkeepCost(0)
	, m_iHurryCostModifier(0)
	, m_iAdvancedStartCost(100)
	, m_iSpecialCargo(-1)
	, m_iSMNotSpecialCargo(-1)
	, m_iDiscoverBase(0)
	, m_iDiscoverMultiplier(0)
	, m_iHurryBase(0)
	, m_iHurryMultiplier(0)
	, m_iTradeBase(0)
	, m_iTradeMultiplier(0)
	, m_iGreatWorkBase(0)
	, m_iFoodBase(0)
	, m_iAIWeight(0)
	, m_iZobristValue(0)
	, m_iCommandType(-1)   // NO_COMMAND until SetGlobalActionInfo assigns it
	, m_iBaseQualityRank(0)
	, m_iBaseGroupRank(0)
	, m_iBaseSizeRank(0)
	, m_iSMChangeBase(0)
	, m_iSMModifierBase(0)
	, m_iBaseCargoVolume(1)   // the derivation's floor -- the getUnitCountSM divisor must never see < 1 geometry
	, m_iMeshGroupSize(0)
	, m_iMeleeWaveSize(0)
	, m_iRangedWaveSize(0)
	, m_fAnimationMaxSpeed(0.0f)
	, m_fAnimationPadTime(0.0f)
	, m_iEra(NO_ERA)
	, m_bCanAcquireExperience(false)
{
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// ctor did. CvUnit seeds m_movementCharacteristicsHash from it. Never redrawn in mapFrom (synced RNG).
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

//	The era INDEX each art band applies FROM, against the ordered era list (Assets/Data/eras/_order.json:
//	0 prehistoric, 1 ancient, 2 classical, 3 medieval, 4 renaissance, 5 industrial, 6 atomic, 7 information,
//	8 nanotech, 9 transhuman, 10 galactic, 11 cosmic, 12 transcendent, 13 future). The band names are the ART's
//	vocabulary, not the era list's -- `late` opens at ATOMIC and `future` at TRANSHUMAN -- so the mapping is a
//	table rather than a name lookup, and it lives here once.
static const int s_aiUnitArtEraBandFrom[NUM_UNIT_ART_ERA_BANDS] =
{
	0,   // early
	2,   // classical
	3,   // middle
	4,   // renaissance
	5,   // industrial
	6,   // late
	9,   // future
};

int CvUnitInfo::getUnitGroupRequired(int iGroup) const
{
	if (iGroup < 0 || iGroup >= (int)m_meshGroups.size())
	{
		return -1;
	}
	return m_meshGroups[iGroup].iRequired;
}

const CvArtInfoUnit* CvUnitInfo::getArtInfo(int iIndex, EraTypes eEra, UnitArtStyleTypes /*eStyle*/) const
{
	//	Walk DOWN from the highest band the era has reached to the first one this group actually authors, then
	//	fall back to the unit's own top-level tag. A band the unit does not author is fallen THROUGH, which is
	//	what lets a unit that only ever authors `early` keep its art for the whole game.
	//	NO_ERA reads as the FIRST era, so a caller with no era in hand (the button) still gets the group's own
	//	early art rather than the unit's top-level tag -- which differ the moment a unit has several groups.
	const int iEra = ((int)eEra < 0) ? 0 : (int)eEra;
	if (iIndex >= 0 && iIndex < (int)m_meshGroups.size())
	{
		const CvUnitMeshGroup& kGroup = m_meshGroups[iIndex];
		for (int iBand = NUM_UNIT_ART_ERA_BANDS - 1; iBand >= 0; --iBand)
		{
			if (iEra >= s_aiUnitArtEraBandFrom[iBand] && !kGroup.aszEraDefine[iBand].empty())
			{
				return ARTFILEMGR.getUnitArtInfo(kGroup.aszEraDefine[iBand].c_str());
			}
		}
	}
	return ARTFILEMGR.getUnitArtInfo(m_szArtDefineTag.c_str());
}

// The unit button lives in the art define (CIV4ArtDefines_Unit.xml), not the unit JSON -- resolve through the
// art info so the UI icon works; CvInfoBase::getButton would return the empty m_szButton.
const char* CvUnitInfo::getButton() const
{
	const CvArtInfoUnit* pArt = getArtInfo(0, NO_ERA, NO_UNIT_ARTSTYLE);
	return pArt != NULL ? pArt->getButton() : "";
}

// The post-map LOAD-WINDOW derivation (json.md par.9 sizeMatters: the ranks are DERIVED at load, never stored
// on the data). Called by reversePassRun for every unit AFTER the full registry is mapped -- the combat classes'
// sizeMatters blocks, the tech eras, and the promotion applicability sets are all complete by then. Idempotent:
// every derived member is recomputed and assigned each run; the upgrade chain is cleared first.
void CvUnitInfo::deriveAtRegistryComplete(int iFirstPrereqTechEra, const std::set<int>& combatClassesWithPromotions)
{
	// --- the SM base ranks + strength/cargo bases + the IDENTITY TAGS: ONE combat-class pass mirroring the
	// archived post-load derivation. Ranks = Sigma each class's *Base where > -10 (the "unset" sentinel); change
	// base (x100) = Sigma classes' flat-combat sum; modifier base = Sigma (quality/size/group base - 5) where
	// > -10; cargo volume applies the volumetric multiplier once per unit of the (size+group) offset, /100,
	// floor 1. The group rank feeds getUnitCountSM (count / 3^(rank-1)), so the derived value must be real,
	// never a stub.
	//
	// TAGS ride the same walk because they answer the same question over the same set: a unit's effective tags
	// are its OWN union its combat classes' ([engine.md] UnitCombat). The tag is authored once, on the CLASS, so
	// a unit holds no baked copy that could go stale when a class is re-tagged. The walk is primary +
	// combatClasses, which is exactly the set tags are defined over -- a tag is creation/upgrade-set and NOT
	// promotion-grantable ([tags.md]), so a class a PROMOTION grants contributes no tag and none is missed here.
	int iQualityRank = 0;
	int iGroupRank = 0;
	int iSizeRank = 0;
	int iOffset = 0;
	int iChange = 0;
	int iModifier = 0;
	for (int iIndex = -1; iIndex < (int)m_aiCombatClasses.size(); ++iIndex)
	{
		const int iClass = (iIndex < 0) ? m_iCombatClass : m_aiCombatClasses[iIndex];
		if (iClass < 0)
		{
			continue;
		}
		const CvUnitCombatInfo& classInfo = GC.getUnitCombatInfo((UnitCombatTypes)iClass);
		m_tags.mergeGrantedIds(*classInfo.getTags());
		iChange += classInfo.getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT);
		const int iQuality = classInfo.getSizeMatters().qualityBase;
		const int iSize = classInfo.getSizeMatters().sizeBase;
		const int iGroup = classInfo.getSizeMatters().groupBase;
		if (iQuality > -10)
		{
			iQualityRank += iQuality;
			iModifier += iQuality - 5;
		}
		if (iSize > -10)
		{
			iSizeRank += iSize;
			iModifier += iSize - 5;
			iOffset += iSize;
		}
		if (iGroup > -10)
		{
			iGroupRank += iGroup;
			iModifier += iGroup - 5;
			iOffset += iGroup;
		}
	}
	const int iMultiplierRaw = GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER();
	const int iMultiplier = (iMultiplierRaw < 1) ? 1 : iMultiplierRaw;   // defensive: the divide below must not hit zero
	int iBase = 10000;
	if (iOffset > 0)
	{
		for (int iStep = 0; iStep < iOffset; ++iStep)
		{
			iBase = iBase * iMultiplier / 100;
		}
	}
	else
	{
		for (int iStep = 0; iStep < -iOffset; ++iStep)
		{
			iBase = iBase * 100 / iMultiplier;
		}
	}
	iBase /= 100;
	m_iBaseQualityRank = iQualityRank;
	m_iBaseGroupRank = iGroupRank;
	m_iBaseSizeRank = iSizeRank;
	m_iSMChangeBase = iChange;
	m_iSMModifierBase = iModifier;
	m_iBaseCargoVolume = (iBase < 1) ? 1 : iBase;

	// --- the unit's era = its first prereq TECH atom's era (the pass resolves the atom's kind through the ONE
	// load-pipeline type dispatch; NO_ERA when tech-free) ---
	m_iEra = iFirstPrereqTechEra;

	// --- a unit can gain XP iff SOME promotion applies to its primary combat class (archive mirror; the union
	// set is the pass's one cross-registry precompute) ---
	m_bCanAcquireExperience = m_iCombatClass >= 0 && combatClassesWithPromotions.count(m_iCombatClass) != 0;

	// --- the upgrade CHAIN: every unit transitively reachable over the DIRECT upgrades (the whole upgrade
	// tree, SM upkeep/count consumer). Breadth-first over the direct edges; the visited test both dedups and
	// terminates a cyclic authoring. ---
	m_aiUpgradeChain.clear();
	std::vector<int> pendingUnits(getUpgradesTo());
	while (!pendingUnits.empty())
	{
		const int iReached = pendingUnits.front();
		pendingUnits.erase(pendingUnits.begin());
		if (iReached < 0 || vectorHas(m_aiUpgradeChain, iReached))
		{
			continue;
		}
		m_aiUpgradeChain.push_back(iReached);
		const std::vector<int>& nextUpgrades = GC.getUnitInfo((UnitTypes)iReached).getUpgradesTo();
		for (size_t iNext = 0; iNext < nextUpgrades.size(); ++iNext)
		{
			pendingUnits.push_back(nextUpgrades[iNext]);
		}
	}
}

// 100 x (strength base [air units: identity.base.airCombat] + the combat-class flat-combat change base),
// SM-scaled by the derived modifier base when bSizeMatters. The strength base is the compiled
// strength.unit.flat sum (ruling 5: strength = the BASE value only), already x100.
int CvUnitInfo::getTotalModifiedCombatStrength(bool bSizeMatters) const
{
	int iStrength = 0;
	if (getDomain() == DOMAIN_AIR)
	{
		iStrength = 100 * m_iAirCombat;
	}
	else
	{
		iStrength = getScalar(SCALAR_STRENGTH, CASC_SCOPE_UNIT, CASC_UNIT_FLAT);
	}
	iStrength += m_iSMChangeBase;
	if (iStrength < 1)
	{
		return 0;
	}
	if (!bSizeMatters || m_iSMModifierBase == 0)
	{
		return iStrength;
	}
	const int iScaled = applySMRank(iStrength, m_iSMModifierBase, GC.getSIZE_MATTERS_MOST_MULTIPLIER());
	return (iScaled < 1) ? 1 : iScaled;
}

void CvUnitInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending
	// container. NB m_aiUpgradeChain is NOT cleared here: deriveAtRegistryComplete owns it (clear-first) and
	// runs after every mapFrom. The guarded jsonIdStr string reads leave their target untouched on an absent
	// key, so the string members reset here too.
	m_szArtDefineTag.clear();
	m_meshGroups.clear();
	m_iMeshGroupSize = 0;
	m_iMeleeWaveSize = 0;
	m_iRangedWaveSize = 0;
	m_fAnimationMaxSpeed = 0.0f;
	m_fAnimationPadTime = 0.0f;
	m_szFormationType.clear();
	m_aiCombatClasses.clear();
	m_aiUnitAIs.clear();
	m_aiNotUnitAIs.clear();
	m_aeMapCategories.clear();
	m_aiTerrainImpassable.clear();
	m_aiFeatureImpassable.clear();
	m_aiDefendAgainstUnits.clear();
	m_aiHeritage.clear();
	m_aszUniqueNames.clear();
	m_aiEnabledCivilizations.clear();
	m_featurePassableTechs.clear();
	m_terrainPassableTechs.clear();
	m_targetUnitCombats.clear();
	m_defenderUnitCombats.clear();
	m_aiTargetUnits.clear();
	m_aiBuilds.clear();
	m_religionSpread.clear();
	m_corporationSpread.clear();
	m_groupSpawn.clear();
	m_aiReplacedByUnits.clear();
	m_aiGrantedPromotions.clear();
	m_aiGrantedGreatPeople.clear();
	m_aiGrantedBuildings.clear();
	m_flavours.clear();

	CvInfo::mapFrom(entity);   // core fields + the section dispatch (compiles m_modifiers; fills the composed units)

	// par.8/par.9 typed sections (clear-first inside their own parse)
	m_hideAndSeek.parse(entity);
	m_sizeMatters.parse(entity);
	m_outcomes.parse(entity);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: a unit's <PROPERTY_X>.{city|plot}.flat emits into the city / plot it stands
	// on (RELATION_SAME_PLOT) each turn -- the ONE shared walk.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_SAME_PLOT, 0, NULL, getType());

	// world.art -- the ART_DEF_* tag getArtInfo resolves through ArtFileMgr, and the MESH GROUPS beside it: art
	// is art, and the formation's numbers were authored in the same block as the tags that name their models
	// (json.md par.7). The EXE lays the unit out and animates it through these, so an absent value is not
	// "no art" -- it is a formation with no offsets and a walk cycle that never translates.
	if (const picojson::object* pArt = jsonWorldArt(entityObj))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);

		if (const picojson::object* pMesh = jsonChildObj(*pArt, "meshGroups"))
		{
			m_iMeshGroupSize = jsonIdInt(*pMesh, "groupSize");
			m_iMeleeWaveSize = jsonIdInt(*pMesh, "meleeWaveSize");
			m_iRangedWaveSize = jsonIdInt(*pMesh, "rangedWaveSize");
			m_fAnimationMaxSpeed = jsonIdFloat(*pMesh, "maxSpeed");
			m_fAnimationPadTime = jsonIdFloat(*pMesh, "padTime");

			// The BAND KEYS, in UnitArtEraBand order -- the band a tag is authored under IS its index.
			static const char* aszBandKey[NUM_UNIT_ART_ERA_BANDS] =
			{
				"early", "classical", "middle", "renaissance", "industrial", "late", "future"
			};
			picojson::object::const_iterator itGroups = pMesh->find("groups");
			if (itGroups != pMesh->end() && itGroups->second.is<picojson::array>())
			{
				const picojson::array& groups = itGroups->second.get<picojson::array>();
				for (size_t iGroup = 0; iGroup < groups.size(); ++iGroup)
				{
					if (!groups[iGroup].is<picojson::object>())
					{
						continue;
					}
					const picojson::object& groupObj = groups[iGroup].get<picojson::object>();
					CvUnitMeshGroup group;
					group.iRequired = jsonIdInt(groupObj, "required");
					if (const picojson::object* pDefine = jsonChildObj(groupObj, "define"))
					{
						for (int iBand = 0; iBand < NUM_UNIT_ART_ERA_BANDS; ++iBand)
						{
							jsonIdStr(*pDefine, aszBandKey[iBand], group.aszEraDefine[iBand]);
						}
					}
					m_meshGroups.push_back(group);
				}
			}
		}
	}

	// --- par.8 `builds` repertoire (BUILD_* FKs, top-level) ---
	jsonReadIdList(entityObj, "builds", m_aiBuilds);

	// --- the root combat-class FKs (json par.8) ---
	m_iCombatClass = jsonIdFk(entityObj, "combatClass");
	jsonReadIdList(entityObj, "combatClasses", m_aiCombatClasses);

	// --- identity (scalars + base + lists) ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bSpawnOnly = jsonIdBool(*pIdentity, "spawnOnly");
		m_iWorth = jsonIdInt(*pIdentity, "worth");
		m_iMilitaryWorth = jsonIdInt(*pIdentity, "militaryWorth");
		m_iXpValueAttack = jsonIdInt(*pIdentity, "xpValueAttack");
		m_iXpValueDefense = jsonIdInt(*pIdentity, "xpValueDefense");
		m_iConscription = jsonIdInt(*pIdentity, "conscription");
		m_iAggression = jsonIdInt(*pIdentity, "aggression", 5);   // legacy load default 5 -- curator elides it
		m_iAnimalCombat = jsonIdInt(*pIdentity, "animalCombat");
		m_iCommandRange = jsonIdInt(*pIdentity, "commandRange");
		m_iControlPoints = jsonIdInt(*pIdentity, "controlPoints");
		m_iLeaderExperience = jsonIdInt(*pIdentity, "leaderExperience");
		m_iMinAreaSize = jsonIdInt(*pIdentity, "minAreaSize");
		m_iEspionagePoints = jsonIdInt(*pIdentity, "espionagePoints");
		m_iDomain = jsonIdFk(*pIdentity, "domain");
		m_iDefaultUnitAI = jsonIdFk(*pIdentity, "defaultUnitAI");
		if (m_iDefaultUnitAI < 0)
		{
			// legacy load default: -1 negative-indexes the AI count arrays
			m_iDefaultUnitAI = GC.getInfoTypeForString("UNITAI_UNKNOWN");
		}
		m_iSpecialUnitType = jsonIdFk(*pIdentity, "special");
		m_iAdvisor = jsonIdFk(*pIdentity, "advisor");
		m_iLeaderPromotion = jsonIdFk(*pIdentity, "leaderPromotion");
		m_iReligion = jsonIdFk(*pIdentity, "religion");
		m_iCaptures = jsonIdFk(*pIdentity, "captures");
		jsonIdStr(*pIdentity, "formationType", m_szFormationType);
		// identity.base -- the create-unit foundation (strength/moves RELOCATED to their families, ruling 5)
		if (const picojson::object* pBase = jsonChildObj(*pIdentity, "base"))
		{
			m_iWorkRate = jsonIdInt(*pBase, "workRate");
			m_iAirCombat = jsonIdInt(*pBase, "airCombat");
			m_iCombatLimit = jsonIdInt(*pBase, "combatLimit", 100);   // legacy load default 100 -- a 0 read
			                                                          // makes every undamaged defender combat-limit-reached
			m_iAirCombatLimit = jsonIdInt(*pBase, "airCombatLimit");
			m_iAirUnitCap = jsonIdInt(*pBase, "airUnitCap");
		}
		// identity lists
		jsonReadIdList(*pIdentity, "unitAIs", m_aiUnitAIs);
		jsonReadIdList(*pIdentity, "notUnitAIs", m_aiNotUnitAIs);
		// mapCategories is held TYPED so every carrier of the concept -- plot, terrain, feature, improvement,
		// bonus, project, goody, building, unit -- serves ONE shape; the shared isMapCategory walk binds both
		// sides by reference and cannot straddle two element types.
		{
			std::vector<int> mapCategoryIds;
			jsonReadIdList(*pIdentity, "mapCategories", mapCategoryIds);
			for (size_t iCategory = 0; iCategory < mapCategoryIds.size(); ++iCategory)
			{
				m_aeMapCategories.push_back((MapCategoryTypes)mapCategoryIds[iCategory]);
			}
		}
		jsonReadIdList(*pIdentity, "terrainImpassable", m_aiTerrainImpassable);
		jsonReadIdList(*pIdentity, "featureImpassable", m_aiFeatureImpassable);
		jsonReadIdList(*pIdentity, "defendAgainstUnit", m_aiDefendAgainstUnits);
		jsonReadIdList(*pIdentity, "heritage", m_aiHeritage);
		jsonReadStrList(*pIdentity, "uniqueNames", m_aszUniqueNames);
		jsonReadIdList(*pIdentity, "enabledCivilizations", m_aiEnabledCivilizations);
		// identity.advancedStart.cost
		if (const picojson::object* pAdvancedStart = jsonChildObj(*pIdentity, "advancedStart"))
		{
			m_iAdvancedStartCost = jsonIdInt(*pAdvancedStart, "cost", 100);   // legacy load default 100
		}
		// identity.cargo.special / smNotSpecial (the SpecialUnit cargo restrictions)
		if (const picojson::object* pCargo = jsonChildObj(*pIdentity, "cargo"))
		{
			m_iSpecialCargo = jsonIdFk(*pCargo, "special");
			m_iSMNotSpecialCargo = jsonIdFk(*pCargo, "smNotSpecial");
		}
		un_readPassableTechs(*pIdentity, "featurePassableTechs", m_featurePassableTechs);
		un_readPassableTechs(*pIdentity, "terrainPassableTechs", m_terrainPassableTechs);
	}

	// --- cost (json par.7) ---
	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iProductionCost = jsonIdInt(*pCost, "production");
		m_iUpkeepCost = jsonIdInt(*pCost, "upkeep");
		m_iHurryCostModifier = jsonIdInt(*pCost, "hurryCostModifier");
	}

	// --- the par.8 keyed targeting/immunity containers (combat.unit.{targets|defenders|unitTargets}.{TYPE}:
	// true): the family-scoped target tokens compile them as targeted COUNT entries; the typed keyed members
	// materialize here from the compiled entry list -- no raw-JSON subtree read survives. ---
	un_collectCombatKeyedSet(m_modifiers, "targets", m_targetUnitCombats);
	un_collectCombatKeyedSet(m_modifiers, "defenders", m_defenderUnitCombats);
	un_collectCombatKeyedList(m_modifiers, "unitTargets", m_aiTargetUnits);

	// --- par.9 spread (the unit's standing per-religion / per-corp spread strength) ---
	un_readKeyedIntMap(entityObj, "spread", "religion", m_religionSpread);
	un_readKeyedIntMap(entityObj, "spread", "corporation", m_corporationSpread);

	// --- par.9 groupSpawn rows {unitCombat, chance, title} (the pack-spawn config) ---
	{
		picojson::object::const_iterator groupIter = entityObj.find("groupSpawn");
		if (groupIter != entityObj.end() && groupIter->second.is<picojson::array>())
		{
			const picojson::array& rows = groupIter->second.get<picojson::array>();
			for (size_t i = 0; i < rows.size(); ++i)
			{
				if (!rows[i].is<picojson::object>())
				{
					continue;
				}
				const picojson::object& rowObj = rows[i].get<picojson::object>();
				picojson::object::const_iterator classIter = rowObj.find("unitCombat");
				if (classIter == rowObj.end() || !classIter->second.is<std::string>())
				{
					continue;
				}
				const int iClass = jsonResolveId(classIter->second.get<std::string>());
				if (iClass < 0)
				{
					continue;
				}
				GroupSpawnUnitCombat row;
				row.eUnitCombat = (UnitCombatTypes)iClass;
				row.iChance = jsonIdInt(rowObj, "chance");
				picojson::object::const_iterator titleIter = rowObj.find("title");
				if (titleIter != rowObj.end() && titleIter->second.is<std::string>())
				{
					const std::string& titleKey = titleIter->second.get<std::string>();
					row.m_szTitle = CvWString(titleKey.c_str());   // narrow TXT_KEY -> wide
				}
				m_groupSpawn.push_back(row);
			}
		}
	}

	// --- grants (par.5 payload): the lists materialized once + the GP-action magnitudes ---
	if (const picojson::object* pGrants = jsonChildObj(entityObj, "grants"))
	{
		jsonReadIdList(*pGrants, "promotions", m_aiGrantedPromotions);
		jsonReadIdList(*pGrants, "greatPeople", m_aiGrantedGreatPeople);
		jsonReadIdList(*pGrants, "buildings", m_aiGrantedBuildings);
		if (const picojson::object* pAction = jsonChildObj(*pGrants, "greatPersonAction"))
		{
			if (const picojson::object* pDiscover = jsonChildObj(*pAction, "discover"))
			{
				m_iDiscoverBase = jsonIdInt(*pDiscover, "base");
				m_iDiscoverMultiplier = jsonIdInt(*pDiscover, "multiplier");
			}
			if (const picojson::object* pHurry = jsonChildObj(*pAction, "hurry"))
			{
				m_iHurryBase = jsonIdInt(*pHurry, "base");
				m_iHurryMultiplier = jsonIdInt(*pHurry, "multiplier");
			}
			if (const picojson::object* pTrade = jsonChildObj(*pAction, "trade"))
			{
				m_iTradeBase = jsonIdInt(*pTrade, "base");
				m_iTradeMultiplier = jsonIdInt(*pTrade, "multiplier");
			}
			if (const picojson::object* pGreatWork = jsonChildObj(*pAction, "greatWork"))
			{
				m_iGreatWorkBase = jsonIdInt(*pGreatWork, "base");
			}
			if (const picojson::object* pFood = jsonChildObj(*pAction, "food"))
			{
				m_iFoodBase = jsonIdInt(*pFood, "base");
			}
		}
	}

	// --- par.4.2 replacedBy.units (the superseders; the DIRECT upgrades ride requires.build.dormant.all,
	// materialized by CvRequires and read straight off it) ---
	if (const picojson::object* pReplacedBy = jsonChildObj(entityObj, "replacedBy"))
	{
		jsonReadIdList(*pReplacedBy, "units", m_aiReplacedByUnits);
	}

	// --- ai.flavours + ai.behaviour.weight ---
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		jsonReadFlavours(*pAi, m_flavours);
		if (const picojson::object* pBehaviour = jsonChildObj(*pAi, "behaviour"))
		{
			m_iAIWeight = jsonIdInt(*pBehaviour, "weight");
		}
	}
}
