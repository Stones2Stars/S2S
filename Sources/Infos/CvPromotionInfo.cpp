//
//	CvPromotionInfo -- the base dispatch fills the composed units (the par.6 modifier families compile into
//	CvModifiers; the par.8 `skills` bool block; the entity-level gate; the edges/requires chain halves); this
//	subclass parses ONLY what the type genuinely owns: the identity set, the par.9 vision + sizeMatters +
//	promotionLine sections, the par.8 keyed-skill FK lists, the ai weight rows, and the sound asset. NO
//	family-address read survives here ([DEC-materialize-at-mapfrom]).
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson + GC
#include "CvPromotionInfo.h"
#include "CvPromotionLineInfo.h"    // GC.getPromotionLineInfo(...) -> the line's unitcombat lists (qualified-set build)
#include "CvJsonParse.h"            // jsonResolveId / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonChildObj / jsonReadIdList / jsonReadKeyedBoolIdList
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "AI/CvGameAI.h"            // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, archive mirror)

namespace
{
	// identity numeric-or-FK scalar: a plain number stays as-is; a string FK-resolves; absent -> iAbsent (the
	// legacy "unset" sentinel -- an absent era BAND is NO_ERA, never era 0).
	int pr_numOrFk(const picojson::object& identity, const char* szKey, int iAbsent)
	{
		picojson::object::const_iterator iter = identity.find(szKey);
		if (iter == identity.end())
		{
			return iAbsent;
		}
		if (iter->second.is<double>())
		{
			return (int)iter->second.get<double>();
		}
		if (iter->second.is<std::string>())
		{
			return jsonResolveId(iter->second.get<std::string>());
		}
		return iAbsent;
	}
}

CvPromotionInfo::CvPromotionInfo()
	: m_bLeader(false)
	, m_bStatus(false)
	, m_bQuick(false)
	, m_bStarsign(false)
	, m_bZeroesXP(false)
	, m_bForOffset(false)
	, m_bCargoPrereq(false)
	, m_bSetOnInvestigated(false)
	, m_bPrereqNormInvisible(false)
	, m_bRemoveAfterSet(false)
	, m_iStateReligionPrereq(-1)
	, m_iControlPoints(0)
	, m_iCommandRange(0)
	, m_iLevelPrereq(0)
	, m_iMinEra(NO_ERA)
	, m_iReplacesUnitCombat(-1)
	, m_iDomainCargoChange(-1)
	, m_iSpecialCargoChange(-1)
	, m_iSMNotSpecialCargoChange(-1)
	, m_ePromotionLine(NO_PROMOTIONLINE)
	, m_iLinePriority(0)
	, m_bChangesMoveThroughPlots(false)
	, m_bNegativeEffects(false)
	, m_iZobristValue(0)
	, m_iCommandType(NO_COMMAND)
	, m_eTechPrereq(NO_TECH)
	, m_eObsoleteTech(NO_TECH)
{
	// Non-XML runtime state-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// ctor did (never in mapFrom: a full-registry re-map must not redraw the synced RNG).
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

void CvPromotionInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every appending vector.
	m_aiUnitCombats.clear();
	m_aiNotOnUnitCombats.clear();
	m_aiNotOnDomains.clear();
	m_aiProvidesUnitCombats.clear();
	m_aiRemovesUnitCombats.clear();
	m_aiTerrainDoubleMove.clear();
	m_aiFeatureDoubleMove.clear();
	m_aAIWeights.clear();
	m_szSound.clear();
	m_ePromotionLine = NO_PROMOTIONLINE;
	m_iLinePriority = 0;

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers; fills skills/gate/edges/requires)

	// par.9 typed sections (clear-first inside their own parse). NB the 4 identity.negatesInvisibility
	// authorings duplicate vision.negates value-for-value -- the vision section is the served home (the
	// double-authoring is a reported curator seam, not parsed twice).
	m_vision.parse(entity);
	m_sizeMatters.parse(entity);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// PROPERTY_* per-turn SOURCES: a promotion's <PROPERTY_X>.{city|plot}.flat (the crime-fighting /
	// law-enforcement lines) emits into the holding unit's same-plot city / plot -- the ONE shared walk.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_SAME_PLOT);

	// --- par.8 keyed-skill extras (the flat bools live on the composed m_skills block via the base dispatch) ---
	if (const picojson::object* pSkills = jsonChildObj(entityObj, "skills"))
	{
		jsonReadKeyedBoolIdList(*pSkills, "terrainDoubleMove", m_aiTerrainDoubleMove);
		jsonReadKeyedBoolIdList(*pSkills, "featureDoubleMove", m_aiFeatureDoubleMove);
		jsonReadIdList(*pSkills, "unitCombats", m_aiProvidesUnitCombats);
		jsonReadIdList(*pSkills, "removesUnitCombats", m_aiRemovesUnitCombats);
	}

	// --- identity: the census-authored set, each key one typed member ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bLeader = jsonIdBool(*pIdentity, "leader");
		m_bStatus = jsonIdBool(*pIdentity, "status");
		m_bQuick = jsonIdBool(*pIdentity, "quick");
		m_bStarsign = jsonIdBool(*pIdentity, "starsign");
		m_bZeroesXP = jsonIdBool(*pIdentity, "zeroesXP");
		m_bForOffset = jsonIdBool(*pIdentity, "forOffset");
		m_bCargoPrereq = jsonIdBool(*pIdentity, "cargoPrereq");
		m_bSetOnInvestigated = jsonIdBool(*pIdentity, "setOnInvestigated");
		m_bPrereqNormInvisible = jsonIdBool(*pIdentity, "prereqNormInvisible");
		m_bRemoveAfterSet = jsonIdBool(*pIdentity, "removeAfterSet");
		m_iStateReligionPrereq = jsonIdFk(*pIdentity, "stateReligionPrereq");
		m_iControlPoints = jsonIdInt(*pIdentity, "controlPoints");
		m_iCommandRange = jsonIdInt(*pIdentity, "commandRange");
		m_iLevelPrereq = jsonIdInt(*pIdentity, "levelPrereq");
		m_iMinEra = pr_numOrFk(*pIdentity, "minEra", NO_ERA);
		m_iReplacesUnitCombat = jsonIdFk(*pIdentity, "replacesUnitCombat");
		m_iDomainCargoChange = jsonIdFk(*pIdentity, "domainCargoChange");
		m_iSpecialCargoChange = jsonIdFk(*pIdentity, "specialCargoChange");
		m_iSMNotSpecialCargoChange = jsonIdFk(*pIdentity, "smNotSpecialCargoChange");
		jsonReadIdList(*pIdentity, "unitCombats", m_aiUnitCombats);
		jsonReadIdList(*pIdentity, "notOnUnitCombats", m_aiNotOnUnitCombats);
		jsonReadIdList(*pIdentity, "notOnDomains", m_aiNotOnDomains);
	}

	// --- par.9 promotionLine link: the top-level {LINE: rank} object ---
	if (const picojson::object* pLine = jsonChildObj(entityObj, "promotionLine"))
	{
		if (!pLine->empty())
		{
			const int iLine = jsonResolveId(pLine->begin()->first);
			if (iLine >= 0)
			{
				m_ePromotionLine = static_cast<PromotionLineTypes>(iLine);
			}
			if (pLine->begin()->second.is<double>())
			{
				m_iLinePriority = (int)pLine->begin()->second.get<double>();
			}
		}
	}

	// --- ai.unitCombatWeights {UC:int} -> UnitCombatModifier rows ---
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		if (const picojson::object* pWeights = jsonChildObj(*pAi, "unitCombatWeights"))
		{
			for (picojson::object::const_iterator iter = pWeights->begin(); iter != pWeights->end(); ++iter)
			{
				if (!iter->second.is<double>())
				{
					continue;
				}
				const int iUnitCombat = jsonResolveId(iter->first);
				if (iUnitCombat < 0)
				{
					continue;
				}
				UnitCombatModifier row;
				row.eUnitCombat = static_cast<UnitCombatTypes>(iUnitCombat);
				row.iModifier = (int)iter->second.get<double>();
				m_aAIWeights.push_back(row);
			}
		}
	}

	// --- sound.sound ---
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		jsonIdStr(*pSound, "sound", m_szSound);
	}

	// --- the derived verdicts, materialized over the promotion's own data (string-plane skill reads are
	// load-time only; the getters are bare member reads; move-through-plots is the ONE shared derivation) ---
	m_bChangesMoveThroughPlots = deriveChangesMoveThroughPlots(m_skills, m_aiTerrainDoubleMove, m_aiFeatureDoubleMove);
	// The archived negative-effects test mapped onto the surviving planes (the endurance leg is dead --
	// unauthored; the chance-first-strikes leg waits on the firstStrike.chance vocabulary row -- reported).
	m_bNegativeEffects = getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT) < 0
		|| getScalar(SCALAR_FIRST_STRIKES, CASC_SCOPE_UNIT, CASC_UNIT_FLAT) < 0
		|| getCombatModifier(COMBAT_VS_BARBS, CASC_SCOPE_UNIT) < 0
		|| getFlatCombat(COMBAT_AMOUNT, CASC_SCOPE_UNIT) < 0
		|| getCombatModifier(COMBAT_ATTACK, CASC_SCOPE_UNIT) < 0
		|| getCombatModifier(COMBAT_AMOUNT, CASC_SCOPE_UNIT) < 0
		|| m_skills.has("defenseOnly")
		|| m_skills.has("noInvisibility")
		|| m_skills.has("hiddenNationality")
		|| m_skills.hasFalse("hiddenNationality");
}

// remove the FIRST occurrence of iValue from the vector (the archived find+erase, without <algorithm>).
static void pr_eraseValue(std::vector<int>& values, int iValue)
{
	for (std::vector<int>::iterator iter = values.begin(); iter != values.end(); ++iter)
	{
		if (*iter == iValue)
		{
			values.erase(iter);
			return;
		}
	}
}

// Applicability sets, derived post-map by the general reverse pass: the qualified set = the
// unitcombats this promotion applies to (identity.unitCombats) UNION the promotion line's unitcombat list;
// the disqualified set = this promotion's notOnUnitCombats UNION the line's, each removed from the
// qualified set.
void CvPromotionInfo::setQualifiedUnitCombatTypes()
{
	m_aiQualifiedUnitCombats.clear();
	for (int iUnitCombat = 0; iUnitCombat < GC.getNumUnitCombatInfos(); iUnitCombat++)
	{
		if (appliesToUnitCombat(iUnitCombat))
		{
			m_aiQualifiedUnitCombats.push_back(iUnitCombat);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();
	if (ePromotionLine > -1)
	{
		const CvPromotionLineInfo& lineInfo = GC.getPromotionLineInfo(ePromotionLine);
		const std::vector<int>& lineCombats = lineInfo.getUnitCombats();
		for (size_t i = 0; i < lineCombats.size(); ++i)
		{
			if (!isQualifiedUnitCombat(lineCombats[i]))
			{
				m_aiQualifiedUnitCombats.push_back(lineCombats[i]);
			}
		}
	}
}

void CvPromotionInfo::setDisqualifiedUnitCombatTypes()
{
	m_aiDisqualifiedUnitCombats.clear();
	for (int iUnitCombat = 0; iUnitCombat < GC.getNumUnitCombatInfos(); iUnitCombat++)
	{
		if (isNotOnUnitCombat(iUnitCombat))
		{
			if (isQualifiedUnitCombat(iUnitCombat))
			{
				pr_eraseValue(m_aiQualifiedUnitCombats, iUnitCombat);
			}
			m_aiDisqualifiedUnitCombats.push_back(iUnitCombat);
		}
	}
	const PromotionLineTypes ePromotionLine = getPromotionLine();
	if (ePromotionLine > -1)
	{
		const CvPromotionLineInfo& lineInfo = GC.getPromotionLineInfo(ePromotionLine);
		const std::vector<int>& lineNotOn = lineInfo.getNotOnUnitCombats();
		for (size_t i = 0; i < lineNotOn.size(); ++i)
		{
			const int iUnitCombat = lineNotOn[i];
			if (!isNotOnUnitCombat(iUnitCombat))
			{
				if (isQualifiedUnitCombat(iUnitCombat))
				{
					pr_eraseValue(m_aiQualifiedUnitCombats, iUnitCombat);
				}
				m_aiDisqualifiedUnitCombats.push_back(iUnitCombat);
			}
		}
	}
}

// The LINE ACCRUAL -- see the header. The reverse pass hands over the finished ordered list (it groups every
// line ONCE, so no promotion re-scans the registry for its siblings); this only takes ownership of it.
//
// ⚑ Materializing it here is what kills a WHOLE-DATABASE SCAN PER TOOLTIP: the display used to rebuild a
// promotion's line by walking every promotion in the game on every hover. That walk is load-time work over
// static data -- a promotion's line and rank never move -- so it belongs exactly here
// ([DEC-materialize-at-mapfrom]: the getter is a bare member read).
//
// clear-first, like every other idempotent load-time derivation: the postmenu full-registry pass re-runs it.
void CvPromotionInfo::deriveAtRegistryComplete(const std::vector<int>& aiLineAccrual)
{
	m_aiLineAccrual = aiLineAccrual;
}
