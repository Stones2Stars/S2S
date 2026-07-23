//
//	CvUnitInfo::mapFrom -- wires the unit poco's typed members to the curated JSON (Tools/Migration/curate_unit.py).
//	The base CvInfo::mapFrom fills type/identity-text/button + the composed section units (skills/tags/requires/
//	grants/edges/modifier families); this subclass then reads identity.base + the identity scalars/lists + cost + the
//	section 5 unit-scope combat families + vision + succession/replacedBy + ai + the requires-tree prereqs into typed
//	members the getters return. Values are read RAW (human-native) via the shared jsonFam*/jsonId* walkers -- the x100
//	boundary lives only in the CvJsonModifiers family path, which we do not read here. Curator addresses noted inline.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvUnitInfo.h"
#include "CvJsonParse.h"            // jsonResolveId + jsonFamVal/jsonFamMemberVal/jsonChildObj/jsonId*/jsonReadFlavours
#include "UI/CvArtFileMgr.h"        // ARTFILEMGR -- getArtInfo shim (mirrors the CvBonusInfo shim leaf)
#include "Infos/CvArtInfoUnit.h"    // CvArtInfoUnit complete type -- getButton() call needs the full definition
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)
#include "CvUnitCombatInfo.h"   // GC.getUnitCombatInfo(e).getGroupBase() -- the baseGroupRank derivation
#include "CvTechInfo.h"         // GC.getTechInfo(...).getEra() -- the era-comparison reads (unity include exposure)
#include "AI/CvPlayerAI.h"      // GET_PLAYER -- isCivilizationUnit resolves the asking player's civilization
#include "CvPromotionInfo.h"    // GC.getPromotionInfo(i).getUnitCombat() -- the canAcquireExperience derivation
#include "CvJsonModifiers.h"    // getModifiers() walk -> the unit's PROPERTY_* emission sources
#include "CvCascadePropertyBridge.h" // the JSON->BoolExpr translator (property-audit.md increment 4)
#include "Engine/CvOutcomeMission.h" // #430: CvOutcomeMission new/mapFrom/getMission/getOutcomeList (outcomes JSON intake)

CvUnitInfo::CvUnitInfo()
	: spawnOnly(false), unlimitedException(false)
	, m_iCombat(0), m_iMoves(0), m_iWorkRate(0), m_iAirCombat(0), m_iCombatLimit(100), m_iAirCombatLimit(0), m_iAirUnitCap(0)
	, m_iUnitCombatType(-1), m_iDomainType(-1), m_iDefaultUnitAIType(-1), m_iSpecialUnitType(-1), m_iAdvisorType(-1), m_iLeaderPromotion(-1)
	, m_iReligionType(-1), m_iUnitCaptureType(-1), m_iInvisibleType(-1)
	, m_iAssetValue(0), m_iPowerValue(0), m_iXPValueAttack(0), m_iXPValueDefense(0), m_iConscriptionValue(0), m_iAggression(5)
	, m_iAnimalCombatModifier(0), m_iCommandRange(0), m_iControlPoints(0), m_iLeaderExperience(0), m_iMinAreaSize(0), m_iEspionagePoints(0)
	, m_iProductionCost(0), m_iBaseUpkeep(0), m_iHurryCostModifier(0), m_iInstanceCostModifier(0), m_iAdvancedStartCost(100)
	, m_iCargoSpace(0), m_iSpecialCargo(-1), m_iSMNotSpecialCargo(-1), m_iDomainCargo(-1)
	, m_iCityAttackModifier(0), m_iCityDefenseModifier(0), m_iHillsAttackModifier(0), m_iHillsDefenseModifier(0), m_iVSBarbs(0)
	, m_iAttackCombatModifier(0), m_iDefenseCombatModifier(0), m_iCombatModifierPerSizeMore(0), m_iCombatModifierPerSizeLess(0)
	, m_iCombatModifierPerVolumeMore(0), m_iCombatModifierPerVolumeLess(0), m_iLunge(0), m_iEnclose(0), m_iUnnerve(0), m_iDynamicDefense(0)
	, m_iStealthStrikes(0), m_iStealthCombatModifier(0), m_iBreakdownChance(0), m_iBreakdownDamage(0)
	, m_iWithdrawalProbability(0), m_iFirstStrikes(0), m_iChanceFirstStrikes(0)
	, m_iBombardRate(0), m_iRBombardDamage(0), m_iRBombardDamageLimit(0), m_iDCMBombRange(0), m_iDCMBombAccuracy(0), m_iBombRate(0)
	, m_iCollateralDamage(0), m_iCollateralDamageLimit(0), m_iCollateralDamageMaxUnits(0)
	, m_iAirRange(0), m_iInterceptionProbability(0), m_iEvasionProbability(0), m_iNukeRange(-1)
	, m_iCaptureProbabilityModifier(0), m_iCaptureResistanceModifier(0), m_iInsidiousness(0), m_iInvestigation(0)
	, m_iNumHealSupport(0), m_iSelfHealModifier(0), m_iDropRange(0), m_iCultureGarrisonValue(0)
	, m_iBaseDiscover(0), m_iDiscoverMultiplier(0), m_iBaseHurry(0), m_iHurryMultiplier(0), m_iBaseTrade(0), m_iTradeMultiplier(0)
	, m_iGreatWorkCulture(0), m_iBaseFoodChange(0), m_iAIWeight(0), m_iDcmAirBombTier(0)
	, m_iPrereqAndTech(-1), m_iPrereqAndBonus(-1), m_iPrereqVicinityBonus(-1), m_iPrereqReligion(-1), m_iPrereqCorporation(-1)
	, m_iHolyCity(-1), m_iStateReligion(-1), m_iPrereqGameOption(-1), m_iNotGameOption(-1)
	, m_bRequiresStateReligionInCity(false)
	, m_emptyHealUnitCombat(), m_emptyGroupSpawnUnitCombat()
	, m_emptyInvisibleTerrainChanges(), m_emptyInvisibleFeatureChanges(), m_emptyInvisibleImprovementChanges()
	, m_emptyEnabledCivilization()
	, m_iZobristValue(0), m_iCommandType(-1)   // m_iCommandType = NO_COMMAND until SetGlobalActionInfo assigns it
	, m_iBaseGroupRankCache(-1)                // lazy-derived on first getBaseGroupRank() (unitcombats loaded by then)
	, m_bSMBaseDone(false), m_iSMChangeBase(0), m_iSMModifierBase(0), m_iBaseCargoVolumeCache(0)   // lazy SM base (ensureSMBase)
{
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvUnitInfo ctor did (SourceArchive/Infos/CvUnitInfo.cpp:88). CvUnit seeds m_movementCharacteristicsHash from it.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

CvUnitInfo::~CvUnitInfo()
{
	// m_KillOutcomeList is a value member (its dtor frees its CvOutcome*); the action-missions are heap-owned here.
	for (size_t i = 0; i < m_aOutcomeMissions.size(); ++i)
		SAFE_DELETE(m_aOutcomeMissions[i]);
}

// #430: parse the unit's `outcomes` block { kill:[...], actions:[...] } into the CvOutcome engine objects
// (m_KillOutcomeList + m_aOutcomeMissions). Called from mapFrom; idempotent (clears first -- the full-registry re-map).
void CvUnitInfo::mapOutcomes(const picojson::object& o)
{
	m_KillOutcomeList.clear();
	for (size_t i = 0; i < m_aOutcomeMissions.size(); ++i) SAFE_DELETE(m_aOutcomeMissions[i]);
	m_aOutcomeMissions.clear();

	const picojson::object* oc = jsonChildObj(o, "outcomes");
	if (oc == NULL) return;
	picojson::object::const_iterator it = oc->find("kill");
	if (it != oc->end()) m_KillOutcomeList.mapFrom(it->second);
	it = oc->find("actions");
	if (it != oc->end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			CvOutcomeMission* pMission = new CvOutcomeMission();
			pMission->mapFrom(a[i]);
			m_aOutcomeMissions.push_back(pMission);
		}
	}
}

const CvOutcomeList* CvUnitInfo::getActionOutcomeList(int index) const { return m_aOutcomeMissions[index]->getOutcomeList(); }
MissionTypes CvUnitInfo::getActionOutcomeMission(int index) const { return m_aOutcomeMissions[index]->getMission(); }

const CvOutcomeList* CvUnitInfo::getActionOutcomeListByMission(MissionTypes eMission) const
{
	for (size_t i = 0; i < m_aOutcomeMissions.size(); ++i)
		if (m_aOutcomeMissions[i]->getMission() == eMission)
			return m_aOutcomeMissions[i]->getOutcomeList();
	return NULL;
}

const CvOutcomeMission* CvUnitInfo::getOutcomeMissionByMission(MissionTypes eMission) const
{
	for (size_t i = 0; i < m_aOutcomeMissions.size(); ++i)
		if (m_aOutcomeMissions[i]->getMission() == eMission)
			return m_aOutcomeMissions[i];
	return NULL;
}

const CvArtInfoUnit* CvUnitInfo::getArtInfo(int /*i*/, EraTypes /*eEra*/, UnitArtStyleTypes /*eStyle*/) const
{
	// REAL DATA: single art-define tag (world.art.define) -- the archived per-(era,style) grid collapses to one tag.
	return ARTFILEMGR.getUnitArtInfo(m_szArtDefineTag.c_str());
}

// The unit button lives in the art define (CIV4ArtDefines_Unit.xml), not the unit JSON -- reproduce legacy
// (archived CvUnitInfo::updateArtDefineButton = getArtInfo(0,NO_ERA,NO_UNIT_ARTSTYLE)->getButton()) so the UI icon
// resolves; CvInfoBase::getButton would return the empty m_szButton (units carry no ui.art.icon).
const char* CvUnitInfo::getButton() const
{
	const CvArtInfoUnit* p = getArtInfo(0, NO_ERA, NO_UNIT_ARTSTYLE);
	return p != NULL ? p->getButton() : "";
}

// Size-Matters group rank -- DERIVED, never stored (json.md §9 sizeMatters). Reproduces the archived post-load pass
// (SourceArchive/Infos/CvUnitInfo.cpp:4347-4381): Σ over the unit's combat classes (primary + subs) of each
// UnitCombat's getGroupBase() where > -10 (the "unset" sentinel; a combat class with no SM group data is skipped).
// It feeds getUnitCountSM (count / 3^(rank-1)); the save serializes m_unitCountSM, so a stub 0 -> intPow(3,-1)=0 ->
// integer div-by-zero on load. Lazy: unitcombats are all registered by the time a unit is queried at runtime.
int CvUnitInfo::getBaseGroupRank() const
{
	if (m_iBaseGroupRankCache < 0)
	{
		int iRank = 0;
		for (int i = -1; i < getNumSubCombatTypes(); ++i)
		{
			const UnitCombatTypes e = (i < 0) ? (UnitCombatTypes)m_iUnitCombatType : getSubCombatType(i);
			if (e == NO_UNITCOMBAT) continue;
			const int iGroupBase = GC.getUnitCombatInfo(e).getGroupBase();
			if (iGroupBase > -10) iRank += iGroupBase;
		}
		m_iBaseGroupRankCache = iRank;
	}
	return m_iBaseGroupRankCache;
}

// DERIVED (not curated) -- the unit's era = its prereq tech's era, reproducing archive CvUnitInfo::getEraInfo: the
// single AND-tech wins if present; else the HIGHEST-era tech among the AND-tech list; else NO_ERA. Consumed by
// CvPlayer::getBaseUnitCost100 (the era train-% cost modifier), which was silently using the game's start era.
int CvUnitInfo::getEraInfo() const
{
	if (m_iPrereqAndTech != NO_TECH)
		return GC.getTechInfo((TechTypes)m_iPrereqAndTech).getEra();
	int iHighest = NO_TECH;
	for (size_t i = 0; i < m_prereqAndTechs.size(); ++i)
	{
		const int t = m_prereqAndTechs[i];
		if (t == NO_TECH) continue;
		if (iHighest == NO_TECH || GC.getTechInfo((TechTypes)t).getEra() > GC.getTechInfo((TechTypes)iHighest).getEra())
			iHighest = t;
	}
	return (iHighest != NO_TECH) ? GC.getTechInfo((TechTypes)iHighest).getEra() : NO_ERA;
}

// DERIVED: a unit can gain XP iff SOME promotion targets its (primary) combat class -- archive CvUnitInfo::canAcquireExperience.
// Consumed by CvCity free-XP gating + the /state observability endpoint (both silently wrong while this stubbed true).
bool CvUnitInfo::canAcquireExperience() const
{
	if (m_iUnitCombatType == NO_UNITCOMBAT) return false;
	const int n = GC.getNumPromotionInfos();
	for (int i = 0; i < n; ++i)
		if (GC.getPromotionInfo((PromotionTypes)i).getUnitCombat(m_iUnitCombatType)) return true;
	return false;
}

// SM strength/cargo base -- one combat-class pass mirroring the archived post-load derivation (CvUnitInfo.cpp:4345-4400):
// change base = Σ getStrengthChange(); modifier base = Σ (quality/size/group Base - 5) where > -10 (the "unset" sentinel);
// cargo volume applies the volumetric multiplier once per unit of the (size+group) offset, then /100, floor 1.
void CvUnitInfo::ensureSMBase() const
{
	if (m_bSMBaseDone) return;
	int iOffset = 0, iChange = 0, iMod = 0;
	for (int i = -1; i < getNumSubCombatTypes(); ++i)
	{
		const UnitCombatTypes e = (i < 0) ? (UnitCombatTypes)m_iUnitCombatType : getSubCombatType(i);
		if (e == NO_UNITCOMBAT) continue;
		iChange += GC.getUnitCombatInfo(e).getStrengthChange();
		const int q = GC.getUnitCombatInfo(e).getQualityBase();
		const int s = GC.getUnitCombatInfo(e).getSizeBase();
		const int g = GC.getUnitCombatInfo(e).getGroupBase();
		if (q > -10) iMod += q - 5;
		if (s > -10) { iMod += s - 5; iOffset += s; }
		if (g > -10) { iMod += g - 5; iOffset += g; }
	}
	const int iMul = GC.getSIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER();
	const int iSMMult = (iMul < 1) ? 1 : iMul;   // defensive: the volumetric divide below must not div-by-zero
	int iBase = 10000;
	if (iOffset > 0) { for (int k = 0; k <  iOffset; ++k) iBase = iBase * iSMMult / 100; }
	else             { for (int k = 0; k < -iOffset; ++k) iBase = iBase * 100 / iSMMult; }
	iBase /= 100;
	m_iBaseCargoVolumeCache = (iBase < 1) ? 1 : iBase;
	m_iSMChangeBase = iChange;
	m_iSMModifierBase = iMod;
	m_bSMBaseDone = true;
}

int CvUnitInfo::getBaseCargoVolume() const { ensureSMBase(); return m_iBaseCargoVolumeCache; }

// archive CvUnitInfo::getTotalModifiedCombatStrength100: 100 × (combat[or air] + SM change base), SM-scaled by the
// modifier base. Was stubbed 0 -> every unit's combat-strength UI/help read zero.
int CvUnitInfo::getTotalModifiedCombatStrength100(bool bSizeMatters) const
{
	ensureSMBase();
	const int iStr = 100 * ((getDomainType() == DOMAIN_AIR ? m_iAirCombat : m_iCombat) + m_iSMChangeBase);
	if (iStr < 1) return 0;
	if (!bSizeMatters || m_iSMModifierBase == 0) return iStr;
	const int iSM = applySMRank(iStr, m_iSMModifierBase, GC.getSIZE_MATTERS_MOST_MULTIPLIER());
	return (iSM < 1) ? 1 : iSM;
}

// ------------------------------------------------------------------------------------------------------------------
//  local JSON read helpers (all read RAW human values; FK strings resolve via jsonResolveId)
// ------------------------------------------------------------------------------------------------------------------
namespace
{
	// o[key] as an int, else fallback (distinguishes present-vs-absent for sentinel defaults like nukeRange).
	int childInt(const picojson::object& o, const char* key, int fallback)
	{
		picojson::object::const_iterator it = o.find(key);
		if (it != o.end() && it->second.is<double>()) return (int)it->second.get<double>();
		return fallback;
	}
	// parent[key] = [ "TYPE_A", "TYPE_B", ... ] -> resolved ids appended to out.
	void readIdArray(const picojson::object& parent, const char* key, std::vector<int>& out)
	{
		picojson::object::const_iterator it = parent.find(key);
		if (it == parent.end() || !it->second.is<picojson::array>()) return;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) out.push_back(id); }
	}
	// o[family][scope][keyword] = { TYPE: { [member:] { leaf: N } } } -> out[id] = N (member NULL = leaf directly under TYPE).
	void readKeyedIntMap(const picojson::object& o, const char* family, const char* scope, const char* keyword,
	                     const char* member, const char* leaf, std::map<int, int>& out)
	{
		const picojson::object* f  = jsonChildObj(o, family);    if (!f)  return;
		const picojson::object* sc = jsonChildObj(*f, scope);    if (!sc) return;
		const picojson::object* kw = jsonChildObj(*sc, keyword); if (!kw) return;
		for (picojson::object::const_iterator it = kw->begin(); it != kw->end(); ++it)
		{
			const int id = jsonResolveId(it->first);
			if (id < 0 || !it->second.is<picojson::object>()) continue;
			const picojson::object& tobj = it->second.get<picojson::object>();
			const picojson::object* leafParent = &tobj;
			if (member) { const picojson::object* m = jsonChildObj(tobj, member); if (!m) continue; leafParent = m; }
			picojson::object::const_iterator lv = leafParent->find(leaf);
			if (lv != leafParent->end() && lv->second.is<double>()) out[id] = (int)lv->second.get<double>();
		}
	}
	// o[family][scope][keyword] = { TYPE: true } -> resolved ids inserted into out. A 3-level (family/scope/keyword)
	// keyed-set reader -- the curator moved the targets/defenders classification blocks under strength.unit.*.
	void readFamKeyedSet(const picojson::object& o, const char* family, const char* scope, const char* keyword, std::set<int>& out)
	{
		const picojson::object* f  = jsonChildObj(o, family);    if (!f)  return;
		const picojson::object* sc = jsonChildObj(*f, scope);    if (!sc) return;
		const picojson::object* kw = jsonChildObj(*sc, keyword); if (!kw) return;
		for (picojson::object::const_iterator it = kw->begin(); it != kw->end(); ++it)
		{ const int id = jsonResolveId(it->first); if (id >= 0) out.insert(id); }
	}
	// same 3-level shape, into a vector (deterministic sorted order) for indexed access (strength.unit.unitTargets).
	void readFamKeyedList(const picojson::object& o, const char* family, const char* scope, const char* keyword, std::vector<int>& out)
	{
		const picojson::object* f  = jsonChildObj(o, family);    if (!f)  return;
		const picojson::object* sc = jsonChildObj(*f, scope);    if (!sc) return;
		const picojson::object* kw = jsonChildObj(*sc, keyword); if (!kw) return;
		for (picojson::object::const_iterator it = kw->begin(); it != kw->end(); ++it)
		{ const int id = jsonResolveId(it->first); if (id >= 0) out.push_back(id); }
	}
	// o[section][key] = { TYPE: N } -> out[id] = N (a FLAT keyed-int map -- e.g. spread.religion / spread.corporation).
	void readKeyedIntSimple(const picojson::object& o, const char* section, const char* key, std::map<int, int>& out)
	{
		const picojson::object* s = jsonChildObj(o, section); if (!s) return;
		const picojson::object* kv = jsonChildObj(*s, key);   if (!kv) return;
		for (picojson::object::const_iterator it = kv->begin(); it != kv->end(); ++it)
		{
			const int id = jsonResolveId(it->first);
			if (id >= 0 && it->second.is<double>()) out[id] = (int)it->second.get<double>();
		}
	}
	// parent[key] = [ "str", ... ] -> raw strings (NOT infotypes: unit names / text keys) appended to out.
	void readStrArray(const picojson::object& parent, const char* key, std::vector<std::string>& out)
	{
		picojson::object::const_iterator it = parent.find(key);
		if (it == parent.end() || !it->second.is<picojson::array>()) return;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i) if (a[i].is<std::string>()) out.push_back(a[i].get<std::string>());
	}
	// vision[key] = { INVISIBLE_X: N } -> (InvisibleTypes,int) pairs appended.
	void readIntensity(const picojson::object& vis, const char* key, std::vector<std::pair<InvisibleTypes, int> >& out)
	{
		const picojson::object* kv = jsonChildObj(vis, key); if (!kv) return;
		for (picojson::object::const_iterator it = kv->begin(); it != kv->end(); ++it)
		{
			const int id = jsonResolveId(it->first);
			if (id >= 0 && it->second.is<double>())
				out.push_back(std::make_pair((InvisibleTypes)id, (int)it->second.get<double>()));
		}
	}
	bool prefix(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }
	// o[key] as an INFOTYPE string FK-resolved, else -1.
	int strFk(const picojson::object& o, const char* key)
	{
		picojson::object::const_iterator it = o.find(key);
		return (it != o.end() && it->second.is<std::string>()) ? jsonResolveId(it->second.get<std::string>()) : -1;
	}
	// identity.{feature|terrain}PassableTechs = [ { TYPE: TECH_or_NONE }, ... ] -> out[typeId] = techId (-1 for NONE).
	void readPassableTechs(const picojson::object& id, const char* key, std::map<int, int>& out)
	{
		picojson::object::const_iterator it = id.find(key);
		if (it == id.end() || !it->second.is<picojson::array>()) return;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			for (picojson::object::const_iterator k = e.begin(); k != e.end(); ++k)
			{
				const int fid = jsonResolveId(k->first);
				const int tid = k->second.is<std::string>() ? jsonResolveId(k->second.get<std::string>()) : -1;
				if (fid >= 0) out[fid] = tid;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------
//  requires-tree prereq reconstruction: the curated `requires.build` condition tree (requires_unit()) re-derived into
//  the legacy typed prereq fields. build.all holds AND atoms (PRESENCE) + PREDICATE gates + nested {any} OR groups.
// ------------------------------------------------------------------------------------------------------------------
void CvUnitInfo::reconstructPrereqs()
{
	const CvJsonRequires* r = getRequires();
	if (r == NULL || r->build == NULL) return;
	const CvJsonCondition* b = r->build;
	for (size_t i = 0; i < b->all.size(); ++i)
	{
		const CvJsonCondition* c = b->all[i];
		if (c == NULL) continue;
		if (c->kind == CASC_COND_PRESENCE)
		{
			const std::string& t = c->type;
			if (prefix(t, "TECH_"))            { m_prereqAndTechs.push_back((TechTypes)c->id); if (m_iPrereqAndTech < 0) m_iPrereqAndTech = c->id; }
			else if (prefix(t, "BONUS_"))
			{
				if (c->connection == CASC_CONN_VICINITY && c->vicinity == CASC_VIC_CONNECTED) m_iPrereqVicinityBonus = c->id;
				else m_iPrereqAndBonus = c->id;   // trade|vicinity single AND bonus
			}
			else if (prefix(t, "BUILDING_"))   m_prereqAndBuildings.push_back(c->id);
			else if (prefix(t, "RELIGION_"))   m_iPrereqReligion = c->id;
			else if (prefix(t, "HERITAGE_"))   m_prereqAndHeritage.push_back((HeritageTypes)c->id);
		}
		else if (c->kind == CASC_COND_PREDICATE)
		{
			if      (c->predKind == CASC_PRED_HAS_CORPORATION)        m_iPrereqCorporation = c->id;
			else if (c->predKind == CASC_PRED_STATE_RELIGION)         m_iStateReligion = c->id;
			else if (c->predKind == CASC_PRED_IS_HOLY_CITY)           m_iHolyCity = c->id;
			else if (c->predKind == CASC_PRED_STATE_RELIGION_IN_CITY) m_bRequiresStateReligionInCity = true;
		}
		else if (c->kind == CASC_COND_GROUP && !c->anyOf.empty())
		{
			// OR group: classify by the first member's kind/type; append every member's id to the matching bucket.
			const CvJsonCondition* first = c->anyOf[0];
			if (first == NULL || first->kind != CASC_COND_PRESENCE) continue;
			const std::string& ft = first->type;
			for (size_t j = 0; j < c->anyOf.size(); ++j)
			{
				const CvJsonCondition* m = c->anyOf[j];
				if (m == NULL || m->kind != CASC_COND_PRESENCE) continue;
				if (prefix(ft, "BONUS_"))
				{
					if (first->connection == CASC_CONN_VICINITY && first->vicinity == CASC_VIC_CONNECTED)
						m_prereqOrVicinityBonuses.push_back((BonusTypes)m->id);
					else m_prereqOrBonuses.push_back((BonusTypes)m->id);
				}
				else if (prefix(ft, "BUILDING_"))  m_prereqOrBuildings.push_back(m->id);
				else if (prefix(ft, "HERITAGE_"))  m_prereqOrHeritage.push_back((HeritageTypes)m->id);
				else if (prefix(ft, "CIVIC_"))     m_prereqOrCivics.push_back(m->id);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------
void CvUnitInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending vector
	// (keyed std::maps assign per key and the std::sets re-insert identical elements: no clear needed).
	// NB m_upgradeChain is NOT cleared -- runtime-built via addUnitToUpgradeChain by a separate pass, not this parse.
	m_unitAIs.clear(); m_notUnitAIs.clear(); m_mapCategories.clear(); m_heritage.clear(); m_subCombatTypes.clear();
	m_targetUnits.clear(); m_freePromotions.clear(); m_buildings.clear(); m_greatPeoples.clear();
	m_seeInvisibleTypes.clear(); m_impassableTerrains.clear(); m_impassableFeatures.clear();
	m_defendAgainstUnits.clear(); m_upgrades.clear(); m_superseding.clear(); m_enabledCivs.clear();
	m_groupSpawn.clear(); m_healUnitCombat.clear(); m_invisibilityIntensity.clear();
	m_invisibleFeatureChanges.clear(); m_invisibleTerrainChanges.clear();
	m_prereqAndTechs.clear(); m_prereqOrBonuses.clear(); m_prereqOrVicinityBonuses.clear();
	m_prereqAndBuildings.clear(); m_prereqOrBuildings.clear();
	m_prereqAndHeritage.clear(); m_prereqOrHeritage.clear(); m_prereqOrCivics.clear();

	CvInfo::mapFrom(entity);   // core fields + composed units (skills/tags/requires/grants/edges/modifier families)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();
	picojson::object::const_iterator it;

	// PROPERTY_* per-turn SOURCES (property-audit.md increments B+4): a unit's <PROPERTY_X>.{city|plot}.flat
	// emits into the city / plot it stands on (RELATION_SAME_PLOT) each turn -- the ONE shared walk
	// (clear-and-refill + conditioned-entry translation inside).
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_SAME_PLOT);

	// world.art.icon -- the ART_DEF_* tag getArtInfo resolves through ArtFileMgr (mirrors CvBonusInfo).
	if (const picojson::object* art = jsonWorldArt(o)) jsonIdStr(*art, "define", m_szArtDefineTag);

	// --- top-level `builds` repertoire (BUILD_* ids) ---
	if ((it = o.find("builds")) != o.end() && it->second.is<picojson::array>())
	{
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int bid = jsonResolveId(a[i].get<std::string>()); if (bid >= 0) builds.push_back(bid); }
	}

	// --- combat classes (ROOT-level after the curator restructure) ---
	m_iUnitCombatType = jsonIdFk(o, "combatClass");        // the PRIMARY combat class (root `combatClass`)
	readIdArray(o, "combatClasses", m_subCombatTypes);     // the SubCombatTypes list (root `combatClasses`)

	// --- identity (scalars + base + lists) ---
	if (const picojson::object* id = jsonChildObj(o, "identity"))
	{
		{ picojson::object::const_iterator s = id->find("spawnOnly"); if (s != id->end() && s->second.is<bool>()) spawnOnly = s->second.get<bool>(); }
		// identity scalars (curate_unit.py ID_SCALAR)
		m_iAssetValue        = jsonIdInt(*id, "worth");
		m_iPowerValue        = jsonIdInt(*id, "militaryWorth");
		m_iXPValueAttack     = jsonIdInt(*id, "xpValueAttack");
		m_iXPValueDefense    = jsonIdInt(*id, "xpValueDefense");
		m_iConscriptionValue = jsonIdInt(*id, "conscription");
		m_iAggression        = childInt(*id, "aggression", 5);        // legacy load default 5 (archive .add:2780) -- curator elides it
		m_iAnimalCombatModifier = jsonIdInt(*id, "animalCombat");
		m_iCommandRange      = jsonIdInt(*id, "commandRange");
		m_iControlPoints     = jsonIdInt(*id, "controlPoints");
		m_iLeaderExperience  = jsonIdInt(*id, "leaderExperience");
		m_iMinAreaSize       = jsonIdInt(*id, "minAreaSize");
		m_iEspionagePoints   = jsonIdInt(*id, "espionagePoints");
		m_iDomainType        = jsonIdFk(*id, "domain");
		m_iDefaultUnitAIType = jsonIdFk(*id, "defaultUnitAI");
		if (m_iDefaultUnitAIType < 0) m_iDefaultUnitAIType = GC.getInfoTypeForString("UNITAI_UNKNOWN");  // legacy load default (archive :3258); -1 negative-indexes AI count arrays
		m_iSpecialUnitType   = jsonIdFk(*id, "special");
		m_iAdvisorType       = jsonIdFk(*id, "advisor");
		m_iLeaderPromotion   = jsonIdFk(*id, "leaderPromotion");
		m_iReligionType      = jsonIdFk(*id, "religion");
		m_iUnitCaptureType   = jsonIdFk(*id, "captures");
		jsonIdStr(*id, "formationType", m_szFormationType);
		// identity.base (the create-unit foundation) -- combat/moves/combatClass RELOCATED to root (read below);
		// workRate/airCombat/combatLimit/airCombatLimit/airUnitCap still live here.
		if (const picojson::object* base = jsonChildObj(*id, "base"))
		{
			m_iWorkRate       = jsonIdInt(*base, "workRate");
			m_iAirCombat      = jsonIdInt(*base, "airCombat");
			m_iCombatLimit    = childInt(*base, "combatLimit", 100);  // legacy load default 100 (archive .add:2691) -- 0 reads
			                                                          // every undamaged defender as combat-limit-reached (no attacks)
			m_iAirCombatLimit = jsonIdInt(*base, "airCombatLimit");
			m_iAirUnitCap     = jsonIdInt(*base, "airUnitCap");
		}
		// identity lists
		readIdArray(*id, "unitAIs",           m_unitAIs);
		readIdArray(*id, "notUnitAIs",        m_notUnitAIs);
		readIdArray(*id, "mapCategories",     m_mapCategories);
		readIdArray(*id, "featureImpassable", m_impassableFeatures);
		readIdArray(*id, "terrainImpassable", m_impassableTerrains);
		readIdArray(*id, "defendAgainstUnit", m_defendAgainstUnits);
		readIdArray(*id, "heritage",          m_heritage);
		readStrArray(*id, "uniqueNames",      m_unitNames);   // name/text-key strings (NOT infotypes)
		{
			std::vector<int> civs;
			readIdArray(*id, "enabledCivilizations", civs);   // NPC whitelist -> EnabledCivilizations structs
			for (size_t i = 0; i < civs.size(); ++i)
			{ EnabledCivilizations e; e.eCivilization = (CivilizationTypes)civs[i]; m_enabledCivs.push_back(e); }
		}
		// identity.advancedStart.cost
		if (const picojson::object* as = jsonChildObj(*id, "advancedStart")) m_iAdvancedStartCost = childInt(*as, "cost", 100);  // legacy load default 100 (archive .add:2674)
		// identity.cargo.special / smNotSpecial (SpecialUnit-cargo restrictions)
		if (const picojson::object* cargo = jsonChildObj(*id, "cargo"))
		{ m_iSpecialCargo = jsonIdFk(*cargo, "special"); m_iSMNotSpecialCargo = jsonIdFk(*cargo, "smNotSpecial"); }
		// identity.{feature|terrain}PassableTechs -> id->tech maps
		readPassableTechs(*id, "featurePassableTechs", m_featurePassableTech);
		readPassableTechs(*id, "terrainPassableTechs", m_terrainPassableTech);
	}

	// --- cost ---
	if (const picojson::object* cost = jsonChildObj(o, "cost"))
	{
		m_iProductionCost    = jsonIdInt(*cost, "production");
		m_iBaseUpkeep        = jsonIdInt(*cost, "upkeep");
		m_iHurryCostModifier = jsonIdInt(*cost, "hurryCostModifier");
	}
	// costs.empire.perInstance.percent (iInstanceCostModifier -- the count-scaled cost)
	if (const picojson::object* costs = jsonChildObj(o, "costs"))
		if (const picojson::object* emp = jsonChildObj(*costs, "empire"))
			if (const picojson::object* pi = jsonChildObj(*emp, "perInstance"))
				m_iInstanceCostModifier = childInt(*pi, "percent", 0);

	// --- base combat strength + base moves: the FLAT scalar of the strength/movement families (curator restructure).
	// strength.unit ALSO carries cityAttack/targets/etc; read the base scalar specifically at .flat (0 when the whole
	// block is absent -- a non-combatant with no strength block reads combat 0, never a crash).
	m_iCombat                      = jsonFamVal(o, "strength", "unit", "flat");
	m_iMoves                       = jsonFamVal(o, "movement", "unit", "flat");

	// --- section 5 unit-scope combat-trait families (raw human values; curate_unit.py UNIT_FAMILIES) ---
	m_iCityAttackModifier          = jsonFamMemberVal(o, "strength", "unit", "cityAttack", "percent");
	m_iCityDefenseModifier         = jsonFamMemberVal(o, "strength", "unit", "cityDefense", "percent");
	m_iHillsAttackModifier         = jsonFamMemberVal(o, "strength", "unit", "hillsAttack", "percent");
	m_iHillsDefenseModifier        = jsonFamMemberVal(o, "strength", "unit", "hillsDefense", "percent");
	m_iVSBarbs                     = jsonFamMemberVal(o, "strength", "unit", "vsBarbs", "percent");
	m_iAttackCombatModifier        = jsonFamMemberVal(o, "strength", "unit", "attack", "percent");
	m_iDefenseCombatModifier       = jsonFamMemberVal(o, "strength", "unit", "defense", "percent");
	// SM per-rank combat mods -> sizeMatters.combatModifier (json.md §9), moved off the strength family
	if (const picojson::object* sm = jsonChildObj(o, "sizeMatters"))
		if (const picojson::object* cm = jsonChildObj(*sm, "combatModifier"))
		{
			m_iCombatModifierPerSizeMore   = jsonIdInt(*cm, "perSizeMore");
			m_iCombatModifierPerSizeLess   = jsonIdInt(*cm, "perSizeLess");
			m_iCombatModifierPerVolumeMore = jsonIdInt(*cm, "perVolumeMore");
			m_iCombatModifierPerVolumeLess = jsonIdInt(*cm, "perVolumeLess");
		}
	m_iLunge                       = jsonFamMemberVal(o, "strength", "unit", "lunge", "percent");
	m_iEnclose                     = jsonFamMemberVal(o, "strength", "unit", "enclose", "percent");
	m_iUnnerve                     = jsonFamMemberVal(o, "strength", "unit", "unnerve", "percent");
	m_iDynamicDefense              = jsonFamMemberVal(o, "strength", "unit", "dynamicDefense", "percent");
	m_iStealthStrikes              = jsonFamMemberVal(o, "strength", "unit", "stealthStrikes", "flat");
	m_iStealthCombatModifier       = jsonFamMemberVal(o, "strength", "unit", "stealth", "percent");
	m_iBreakdownChance             = jsonFamMemberVal(o, "strength", "unit", "breakdownChance", "flat");
	m_iBreakdownDamage             = jsonFamMemberVal(o, "strength", "unit", "breakdownDamage", "flat");
	m_iWithdrawalProbability       = jsonFamVal(o, "withdrawal", "unit", "percent");
	m_iFirstStrikes                = jsonFamMemberVal(o, "firstStrike", "unit", "strikes", "flat");
	m_iChanceFirstStrikes          = jsonFamMemberVal(o, "firstStrike", "unit", "chance", "flat");
	m_iBombardRate                 = jsonFamMemberVal(o, "bombard", "unit", "rate", "percent");
	m_iRBombardDamage              = jsonFamMemberVal(o, "bombard", "unit", "rangedDamage", "flat");
	m_iRBombardDamageLimit         = jsonFamMemberVal(o, "bombard", "unit", "rangedDamageLimit", "flat");
	m_iDCMBombRange                = jsonFamMemberVal(o, "bombard", "unit", "dcmRange", "flat");
	m_iDCMBombAccuracy             = jsonFamMemberVal(o, "bombard", "unit", "dcmAccuracy", "flat");
	m_iBombRate                    = jsonFamMemberVal(o, "bombard", "unit", "airBombRate", "flat");
	m_iCollateralDamage            = jsonFamMemberVal(o, "collateral", "unit", "damage", "percent");
	m_iCollateralDamageLimit       = jsonFamMemberVal(o, "collateral", "unit", "limit", "flat");
	m_iCollateralDamageMaxUnits    = jsonFamMemberVal(o, "collateral", "unit", "maxUnits", "flat");
	m_iAirRange                    = jsonFamVal(o, "range", "unit", "flat");
	m_iInterceptionProbability     = jsonFamMemberVal(o, "air", "unit", "intercept", "percent");
	m_iEvasionProbability          = jsonFamMemberVal(o, "air", "unit", "evasion", "percent");
	{ const int nr = jsonFamMemberVal(o, "air", "unit", "nukeRange", "flat"); if (nr != 0) m_iNukeRange = nr; }  // -1 sentinel kept when absent
	m_iCaptureProbabilityModifier  = jsonFamMemberVal(o, "capture", "unit", "probability", "flat");
	m_iCaptureResistanceModifier   = jsonFamMemberVal(o, "capture", "unit", "resistance", "flat");
	m_iInsidiousness               = jsonFamMemberVal(o, "espionage", "unit", "insidiousness", "flat");
	m_iInvestigation               = jsonFamMemberVal(o, "espionage", "unit", "investigation", "flat");
	m_iNumHealSupport              = jsonFamMemberVal(o, "heal", "unit", "support", "flat");
	m_iSelfHealModifier            = jsonFamMemberVal(o, "heal", "unit", "selfModifier", "percent");
	m_iDropRange                   = jsonFamMemberVal(o, "movement", "unit", "dropRange", "flat");
	m_iCultureGarrisonValue        = jsonFamMemberVal(o, "culture", "unit", "garrison", "flat");
	m_iCargoSpace                  = jsonFamMemberVal(o, "cargo", "unit", "space", "flat");   // iCargo capacity
	// getDomainCargo: cargo.unit.space.unit qualifier "IS_<DOMAIN>" -> DOMAIN_<X> FK (curate_unit.py cargo transform)
	if (const picojson::object* cf = jsonChildObj(o, "cargo"))
		if (const picojson::object* cu = jsonChildObj(*cf, "unit"))
			if (const picojson::object* sp = jsonChildObj(*cu, "space"))
			{
				picojson::object::const_iterator qi = sp->find("unit");
				if (qi != sp->end() && qi->second.is<std::string>())
				{
					const std::string& q = qi->second.get<std::string>();
					if (q.compare(0, 3, "IS_") == 0) m_iDomainCargo = jsonResolveId("DOMAIN_" + q.substr(3));
				}
			}

	// --- keyed combat families (strength.unit.<keyword>.{TYPE}...) -> id->value maps ---
	readKeyedIntMap(o, "strength", "unit", "terrain",    "attack",  "percent", m_terrainAttack);
	readKeyedIntMap(o, "strength", "unit", "terrain",    "defense", "percent", m_terrainDefense);
	readKeyedIntMap(o, "strength", "unit", "feature",    "attack",  "percent", m_featureAttack);
	readKeyedIntMap(o, "strength", "unit", "feature",    "defense", "percent", m_featureDefense);
	readKeyedIntMap(o, "strength", "unit", "unitCombat", NULL,      "percent", m_unitCombatModifier);
	readKeyedIntMap(o, "strength", "unit", "domain",     NULL,      "percent", m_domainModifier);
	// spread.religion / spread.corporation -- the unit's per-religion / per-corp SPREAD STRENGTH (own block, owner 2026-07-11)
	readKeyedIntSimple(o, "spread", "religion",    m_religionSpreads);
	readKeyedIntSimple(o, "spread", "corporation", m_corporationSpreads);
	// groupSpawn: struct rows {unitCombat, chance, title} -> m_groupSpawn (the pack-spawn config; own block, owner 2026-07-11)
	{
		picojson::object::const_iterator git = o.find("groupSpawn");
		if (git != o.end() && git->second.is<picojson::array>())
		{
			const picojson::array& ga = git->second.get<picojson::array>();
			for (size_t i = 0; i < ga.size(); ++i)
			{
				if (!ga[i].is<picojson::object>()) continue;
				const picojson::object& e = ga[i].get<picojson::object>();
				picojson::object::const_iterator uc = e.find("unitCombat");
				if (uc == e.end() || !uc->second.is<std::string>()) continue;
				const int id = jsonResolveId(uc->second.get<std::string>());
				if (id < 0) continue;
				GroupSpawnUnitCombat row;
				row.eUnitCombat = (UnitCombatTypes)id;
				picojson::object::const_iterator ch = e.find("chance");
				row.iChance = (ch != e.end() && ch->second.is<double>()) ? (int)ch->second.get<double>() : 0;
				picojson::object::const_iterator ti = e.find("title");
				if (ti != e.end() && ti->second.is<std::string>())
				{
					const std::string& s = ti->second.get<std::string>();
					row.m_szTitle = CvWString(s.c_str());   // narrow TXT_KEY -> wide (CvPropertyInfo pattern)
				}
				m_groupSpawn.push_back(row);
			}
		}
	}
	readKeyedIntMap(o, "strength", "unit", "flanking",   NULL,      "percent", m_flankingByUnitCombat);
	// by-UNIT flanking strength -> m_flankingStrikeUnits (an IDValueMap): read via a temp std::map, then setValue in.
	{
		std::map<int, int> tmp;
		readKeyedIntMap(o, "strength", "unit", "flankingUnit", NULL, "percent", tmp);
		for (std::map<int, int>::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
			m_flankingStrikeUnits.setValue((UnitTypes)it->first, it->second);
	}
	// strength.unit.vsUnit.{U}.attack/defense.percent -> IDValueMap via setValue (skip 0; UnitAttackMods/UnitDefenseMods)
	{
		std::map<int, int> atk, def;
		readKeyedIntMap(o, "strength", "unit", "vsUnit", "attack",  "percent", atk);
		readKeyedIntMap(o, "strength", "unit", "vsUnit", "defense", "percent", def);
		for (std::map<int, int>::const_iterator i = atk.begin(); i != atk.end(); ++i) if (i->second != 0) m_unitAttackModifiers.setValue((UnitTypes)i->first, i->second);
		for (std::map<int, int>::const_iterator i = def.begin(); i != def.end(); ++i) if (i->second != 0) m_unitDefenseModifiers.setValue((UnitTypes)i->first, i->second);
	}

	// --- keyed skill-extras: the curator moved targets/defenders/unitTargets under strength.unit.<keyword>.{TYPE:true}
	// (3-level path). collateralImmune became a blanket `skills` bit (read in getUnitCombatCollateralImmune), not a
	// per-unitCombat keyed set, so it is no longer parsed here.
	readFamKeyedSet(o, "strength", "unit", "targets",     m_targetUnitCombat);
	readFamKeyedSet(o, "strength", "unit", "defenders",   m_defenderUnitCombat);
	readFamKeyedList(o, "strength", "unit", "unitTargets", m_targetUnits);
	// skills.dcmAirBomb was an INT tier read from the (former) skills OBJECT; `skills` is now a pure string array and
	// NO current unit authors a dcmAirBomb tier, so this stays 0 (jsonChildObj on the array yields NULL). See report.
	if (const picojson::object* sk = jsonChildObj(o, "skills")) m_iDcmAirBombTier = childInt(*sk, "dcmAirBomb", 0);
	unlimitedException = skill("unlimitedException");   // skills.unlimitedException (the enabler's per-unit exception)

	// --- buildRate.self.percent (BonusProductionModifiers -- per-bonus SELF build-rate, gated by bonus presence) ---
	if (const picojson::object* br = jsonChildObj(o, "buildRate"))
		if (const picojson::object* self = jsonChildObj(*br, "self"))
		{
			picojson::object::const_iterator pit = self->find("percent");
			if (pit != self->end() && pit->second.is<picojson::array>())
			{
				const picojson::array& a = pit->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					const int val = childInt(e, "value", 0);
					if (const picojson::object* en = jsonChildObj(e, "enabled"))
					{
						picojson::object::const_iterator ty = en->find("type");
						if (ty != en->end() && ty->second.is<std::string>())
						{ const int bid = jsonResolveId(ty->second.get<std::string>()); if (bid >= 0) m_bonusProductionModifier[bid] = val; }
					}
				}
			}
		}

	// --- heal.unit.unitCombat.{UC} = {heal, adjacentHeal} -> HealUnitCombat structs ---
	if (const picojson::object* healF = jsonChildObj(o, "heal"))
		if (const picojson::object* hu = jsonChildObj(*healF, "unit"))
			if (const picojson::object* huc = jsonChildObj(*hu, "unitCombat"))
				for (picojson::object::const_iterator hit = huc->begin(); hit != huc->end(); ++hit)
				{
					const int id = jsonResolveId(hit->first);
					if (id < 0 || !hit->second.is<picojson::object>()) continue;
					const picojson::object& e = hit->second.get<picojson::object>();
					HealUnitCombat h; h.eUnitCombat = (UnitCombatTypes)id;
					h.iHeal = childInt(e, "heal", 0); h.iAdjacentHeal = childInt(e, "adjacentHeal", 0);
					m_healUnitCombat.push_back(h);
				}

	// --- grants (lists + GP-action magnitudes) ---
	if (const picojson::object* grants = jsonChildObj(o, "grants"))
	{
		readIdArray(*grants, "promotions",  m_freePromotions);   // FreePromotions
		readIdArray(*grants, "greatPeople", m_greatPeoples);     // GreatPeoples
		readIdArray(*grants, "buildings",   m_buildings);        // Buildings
		// grants.greatPersonAction.<act>.{base,multiplier} (GP_ACTIONS)
		if (const picojson::object* gpa = jsonChildObj(*grants, "greatPersonAction"))
		{
			if (const picojson::object* d = jsonChildObj(*gpa, "discover"))  { m_iBaseDiscover = childInt(*d, "base", 0); m_iDiscoverMultiplier = childInt(*d, "multiplier", 0); }
			if (const picojson::object* h = jsonChildObj(*gpa, "hurry"))     { m_iBaseHurry    = childInt(*h, "base", 0); m_iHurryMultiplier    = childInt(*h, "multiplier", 0); }
			if (const picojson::object* t = jsonChildObj(*gpa, "trade"))     { m_iBaseTrade    = childInt(*t, "base", 0); m_iTradeMultiplier    = childInt(*t, "multiplier", 0); }
			if (const picojson::object* g = jsonChildObj(*gpa, "greatWork")) { m_iGreatWorkCulture = childInt(*g, "base", 0); }
			if (const picojson::object* f = jsonChildObj(*gpa, "food"))      { m_iBaseFoodChange   = childInt(*f, "base", 0); }
		}
	}

	// --- succession.upgradesTo + replacedBy.units ---
	if (const picojson::object* succ = jsonChildObj(o, "succession")) readIdArray(*succ, "upgradesTo", m_upgrades);
	if (const picojson::object* rep = jsonChildObj(o, "replacedBy"))  readIdArray(*rep, "units", m_superseding);

	// --- ai.flavours + ai.behaviour.weight ---
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
	{
		jsonReadFlavours(*ai, m_flavours);
		if (const picojson::object* beh = jsonChildObj(*ai, "behaviour")) m_iAIWeight = jsonIdInt(*beh, "weight");
	}

	// --- vision (invisible / seeInvisible / intensity arrays + invisible-change struct rows) ---
	if (const picojson::object* vis = jsonChildObj(o, "vision"))
	{
		m_iInvisibleType = jsonIdFk(*vis, "invisible");
		readIdArray(*vis, "seeInvisible", m_seeInvisibleTypes);
		readIntensity(*vis, "visibilityIntensity",   m_visibilityIntensity);
		readIntensity(*vis, "invisibilityIntensity", m_invisibilityIntensity);
		// vision.invisibleTerrain rows -> InvisibleTerrainChanges { invisible, terrain, intensity }
		{
			picojson::object::const_iterator vi = vis->find("invisibleTerrain");
			if (vi != vis->end() && vi->second.is<picojson::array>())
			{
				const picojson::array& a = vi->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					InvisibleTerrainChanges r;
					r.eInvisible = (InvisibleTypes)strFk(e, "invisible");
					r.eTerrain = (TerrainTypes)strFk(e, "terrain");
					r.iIntensity = childInt(e, "intensity", 0);
					m_invisibleTerrainChanges.push_back(r);
				}
			}
		}
		// vision.invisibleFeature rows -> InvisibleFeatureChanges { invisible, feature, intensity }
		{
			picojson::object::const_iterator vi = vis->find("invisibleFeature");
			if (vi != vis->end() && vi->second.is<picojson::array>())
			{
				const picojson::array& a = vi->second.get<picojson::array>();
				for (size_t i = 0; i < a.size(); ++i)
				{
					if (!a[i].is<picojson::object>()) continue;
					const picojson::object& e = a[i].get<picojson::object>();
					InvisibleFeatureChanges r;
					r.eInvisible = (InvisibleTypes)strFk(e, "invisible");
					r.eFeature = (FeatureTypes)strFk(e, "feature");
					r.iIntensity = childInt(e, "intensity", 0);
					m_invisibleFeatureChanges.push_back(r);
				}
			}
		}
	}

	// --- entity gate game-options (top-level enabled/disabled -> the composed CvJsonGate the base parsed) ---
	if (const CvJsonGate* g = getGate())
	{
		if (g->enabled  && g->enabled->kind  == CASC_COND_PRESENCE && prefix(g->enabled->type,  "GAMEOPTION_")) m_iPrereqGameOption = g->enabled->id;
		if (g->disabled && g->disabled->kind == CASC_COND_PRESENCE && prefix(g->disabled->type, "GAMEOPTION_")) m_iNotGameOption = g->disabled->id;
	}

	// --- requires-tree prereqs (walk the composed requires.build the base just parsed) ---
	reconstructPrereqs();

	// --- CvOutcome kill/action-mission intake (outcomes.kill[] / actions[]) ---
	mapOutcomes(o);
}

// ===================== game-option-gated getters (archive mirror -- SourceArchive/Infos/CvUnitInfo.cpp
// :1642-:1950; owner ruling: IS_GAME_OPTION covers the combat-mod fields). Value = real curated data; the OPTION
// decides whether the consuming system is on, exactly as legacy gated it at the getter. (getMaxHP stays the
// header stub: NO unit authors sizeMatters.maxHP, and the archive returns 100 for unset under every option state.) =====
int CvUnitInfo::getUnnerve() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iUnnerve : 0; }
int CvUnitInfo::getEnclose() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iEnclose : 0; }
int CvUnitInfo::getLunge() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iLunge : 0; }
int CvUnitInfo::getDynamicDefense() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SURROUND_DESTROY) ? m_iDynamicDefense : 0; }
int CvUnitInfo::getCombatModifierPerSizeMore() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerSizeMore : 0; }
int CvUnitInfo::getCombatModifierPerSizeLess() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerSizeLess : 0; }
int CvUnitInfo::getCombatModifierPerVolumeMore() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerVolumeMore : 0; }
int CvUnitInfo::getCombatModifierPerVolumeLess() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS) ? m_iCombatModifierPerVolumeLess : 0; }
int CvUnitInfo::getStealthStrikes() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING) ? m_iStealthStrikes : 0; }
int CvUnitInfo::getStealthCombatModifier() const
{ return GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING) ? m_iStealthCombatModifier : 0; }
bool CvUnitInfo::isStealthDefense() const
{
	if (!GC.getGame().isOption(GAMEOPTION_COMBAT_WITHOUT_WARNING)) return false;
	static int s_clsId = -1;
	return m_skills.hasKey(s_clsId, CLSD_SKILL, "stealthDefense");
}

// identity.enabledCivilizations is a WHITELIST -- empty means the unit is available to every civilization (the
// overwhelming majority), non-empty restricts it to the listed ones (the Neanderthal / NPC case, the same data
// CvCity::canTrain gates `stronglyRestricted` NPCs on). See the header for why the two semantics share an overload.
bool CvUnitInfo::isCivilizationUnit(const PlayerTypes ePlayer) const
{
	const int iNum = (int)m_enabledCivs.size();
	if (ePlayer == NO_PLAYER) return iNum > 0;   // "is this unit civilization-RESTRICTED at all?"
	if (iNum == 0) return true;                  // unrestricted -> available to every civilization
	const CivilizationTypes eCiv = GET_PLAYER(ePlayer).getCivilizationType();
	for (int i = 0; i < iNum; ++i)
	{
		if (m_enabledCivs[i].eCivilization == eCiv) return true;
	}
	return false;
}
