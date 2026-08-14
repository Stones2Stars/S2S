//
//	CvHandicapInfo -- the handicap poco's exemplar reads on top of the base section dispatch (see the header).
//	The legacy modifier-family scalar MIRRORS are DEAD (wave D): every magnitude read is a game-start-base
//	point read over the COMPILED forms ([DEC-materialize-at-mapfrom] -- no raw-JSON family walker survives).
//	mapFrom materializes ONCE, idempotently: the per-ERA-at-one tail from the compiled entry list (the one
//	sanctioned load-time scan source), the §7 ai config, the §5 grants views, and the identity roster.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvHandicapInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonResolveId
#include "CvModEntry.h"           // the compiled entries -- the mapFrom materialization scan
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk


namespace
{
	// The packed tail-map key: (audience, family, kind, scope, unit) -- the same axes as the compiled slot
	// key, plus the dual-leaf audience bit.
	int hc_tailKey(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bAiAudience)
	{
		const int iAudienceBit = bAiAudience ? 1 : 0;
		return (iAudienceBit << 24) | (((int)eFamily & 0x7F) << 17) | ((iKind & 0xFF) << 9) | (((int)eScope & 0x1F) << 4) | ((int)eUnit & 0xF);
	}
}


CvHandicapInfo::CvHandicapInfo()
	: m_iUnitUpkeepEraModifier(0)
	, m_iAdvancedStartPointsMod(0)
	, m_iAdvancedStartAiPercent(0)
	, m_iStartingGold(0)
	, m_iStartingDefenseUnits(0)
	, m_iStartingWorkerUnits(0)
	, m_iStartingExploreUnits(0)
	, m_iAIStartingDefenseUnits(0)
	, m_iAIStartingWorkerUnits(0)
	, m_iAIStartingExploreUnits(0)
{
}


// ======================= the game-start-base point reads (header doc: the hc_leafBase semantics) =========

int CvHandicapInfo::gameStartBase(ModifierFamily eFamily, int iKind, CvCascScope eScope, CvCascUnit eUnit, bool bAiAudience) const
{
	// the dual-leaf audience maps straight onto the spelled-out CvModAudience read: the AI-ONLY sibling leaf
	// is directly askable (MOD_AUDIENCE_AI_ONLY -- never derived as inclusive minus human)
	const CvModAudience eAudience = bAiAudience ? MOD_AUDIENCE_AI_ONLY : MOD_AUDIENCE_HUMAN;
	int iBase = m_modifiers.sum(eFamily, iKind, eScope, eUnit, eAudience);
	const std::map<int, int>::const_iterator tailIt = m_perEraStart.find(hc_tailKey(eFamily, iKind, eScope, eUnit, bAiAudience));
	if (tailIt != m_perEraStart.end())
	{
		iBase += tailIt->second;
	}
	return iBase;
}

int CvHandicapInfo::getMaintenanceModifier(MaintenanceKind eKind, CvCascScope eScope) const
{
	return gameStartBase(MODFAM_MAINTENANCE, (int)eKind, eScope, CASC_UNIT_PERCENT, false);
}

int CvHandicapInfo::getUpkeepModifier(UpkeepKind eKind, CvCascScope eScope, bool bAiAudience) const
{
	return gameStartBase(MODFAM_UPKEEP, (int)eKind, eScope, CASC_UNIT_PERCENT, bAiAudience);
}

int CvHandicapInfo::getCostsModifier(CostsKind eKind, CvCascScope eScope, bool bAiAudience) const
{
	return gameStartBase(MODFAM_COSTS, (int)eKind, eScope, CASC_UNIT_PERCENT, bAiAudience);
}

int CvHandicapInfo::getCombat(CombatKind eKind, CvCascScope eScope, bool bAiAudience) const
{
	return gameStartBase(MODFAM_COMBAT, (int)eKind, eScope, infoKindUnit(MODFAM_COMBAT, (int)eKind, eScope), bAiAudience);
}

int CvHandicapInfo::getDiplomacy(DiplomacyKind eKind, CvCascScope eScope, bool bAiAudience) const
{
	return gameStartBase(MODFAM_DIPLOMACY, (int)eKind, eScope, infoKindUnit(MODFAM_DIPLOMACY, (int)eKind, eScope), bAiAudience);
}

int CvHandicapInfo::getBarbarians(BarbariansKind eKind, CvCascScope eScope) const
{
	return gameStartBase(MODFAM_BARBARIANS, (int)eKind, eScope, infoKindUnit(MODFAM_BARBARIANS, (int)eKind, eScope), false);
}

int CvHandicapInfo::getRevolutionIndexModifier(CvCascScope eScope) const
{
	return gameStartBase(MODFAM_REVOLUTION, (int)REVOLUTION_AMOUNT, eScope, CASC_UNIT_PERCENT, false);
}

int CvHandicapInfo::getFlatWellbeing(WellbeingChannel eChannel, CvCascScope eScope) const
{
	if (eChannel == WELLBEING_ANGER || eChannel == WELLBEING_UNHEALTH)
	{
		return 0;
	}
	return gameStartBase(infoWellbeingFamily(eChannel), (int)CHANNEL_AMOUNT, eScope, CASC_UNIT_FLAT, false);
}

int CvHandicapInfo::getScalarModifier(InfoScalar eScalar, CvCascScope eScope, bool bAiAudience) const
{
	ModifierFamily eFamily = MODFAM_NONE;
	int iKind = -1;
	infoScalarSlot(eScalar, eFamily, iKind);
	return gameStartBase(eFamily, iKind, eScope, CASC_UNIT_PERCENT, bAiAudience);
}


// ======================= mapFrom -- the ONE load hook (idempotent by contract) ============================

void CvHandicapInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + the section dispatch (compiles m_modifiers, folds grants into m_triggers)

	// PROPERTY_* per-turn SOURCES: player-gathered (the handicap slot), fanned to every owner city -- the ONE
	// shared walk.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED, 0, NULL, getType());

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_aiGoodies.clear();
	m_perEraStart.clear();
	m_iUnitUpkeepEraModifier = 0;
	m_iAdvancedStartPointsMod = 0;
	m_iAdvancedStartAiPercent = 0;
	m_iStartingGold = 0;
	m_iStartingDefenseUnits = 0;
	m_iStartingWorkerUnits = 0;
	m_iStartingExploreUnits = 0;
	m_iAIStartingDefenseUnits = 0;
	m_iAIStartingWorkerUnits = 0;
	m_iAIStartingExploreUnits = 0;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- the load-time scan of the COMPILED entry list (the one sanctioned scan source, patterns.md
	//     § Materialize at mapFrom): the per-ERA game-start tail (header doc). ---
	const std::vector<CvModEntry*>& compiledEntries = m_modifiers.entries();
	for (size_t iEntry = 0; iEntry < compiledEntries.size(); ++iEntry)
	{
		const CvModEntry* pEntry = compiledEntries[iEntry];
		if (!pEntry->hasPer)
		{
			continue;
		}
		if (pEntry->kind < 0)
		{
			continue;
		}
		if (pEntry->perType != "ERA")
		{
			continue;   // a non-ERA per has no game-start scalar meaning (none authored today)
		}
		if (pEntry->perEach != 1)
		{
			continue;   // count(ERA) = 1 at game start; each > 1 floors to 0
		}
		const int iTailKey = hc_tailKey(pEntry->family, pEntry->kind, pEntry->scope, pEntry->unit, pEntry->aiOnly);
		m_perEraStart[iTailKey] += pEntry->value;
	}

	// --- the §7 ai metadata plane (ruling 24: the AI unit-upkeep era stage is a PLAIN CONFIG VALUE) ---
	if (const picojson::object* pAiMeta = jsonChildObj(entityObj, "ai"))
	{
		m_iUnitUpkeepEraModifier = jsonIdInt(*pAiMeta, "unitUpkeepEraModifier");
	}

	// --- grants views: one-shot GAME-START provisioning, read off the COMPOSED unit. The base keys are §5
	//     numeric PULSES (stored ×100 by the section parse); the AI override rides `grants.ai.<key>`, which
	//     the same parse captures as a SCOPED pulse under channel "ai". ONE representation, so the grants
	//     machine reads the same parsed data these views serve. ---
	const CvGrants* pGrants = m_triggers.consideredGrant();
	m_iStartingGold         = (pGrants != NULL) ? pGrants->pulse("startingGold") / 100 : 0;
	m_iStartingDefenseUnits = (pGrants != NULL) ? pGrants->pulse("startingDefenseUnits") / 100 : 0;
	m_iStartingWorkerUnits  = (pGrants != NULL) ? pGrants->pulse("startingWorkerUnits") / 100 : 0;
	m_iStartingExploreUnits = (pGrants != NULL) ? pGrants->pulse("startingExploreUnits") / 100 : 0;
	m_iAIStartingDefenseUnits = (pGrants != NULL) ? pGrants->scopedPulse("ai", "startingDefenseUnits") / 100 : 0;
	m_iAIStartingWorkerUnits  = (pGrants != NULL) ? pGrants->scopedPulse("ai", "startingWorkerUnits")  / 100 : 0;
	m_iAIStartingExploreUnits = (pGrants != NULL) ? pGrants->scopedPulse("ai", "startingExploreUnits") / 100 : 0;

	// --- identity: the advanced-start POINTS BUDGET config + the goody-hut roster (GOODY_* FKs -> ids) ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		if (const picojson::object* pAdvancedStart = jsonChildObj(*pIdentity, "advancedStart"))
		{
			m_iAdvancedStartPointsMod = jsonIdInt(*pAdvancedStart, "pointsMod");
			m_iAdvancedStartAiPercent = jsonIdInt(*pAdvancedStart, "aiPercent");
		}
		picojson::object::const_iterator goodiesIt = pIdentity->find("goodies");
		if (goodiesIt != pIdentity->end() && goodiesIt->second.is<picojson::array>())
		{
			const picojson::array& goodyList = goodiesIt->second.get<picojson::array>();
			for (size_t iGoody = 0; iGoody < goodyList.size(); ++iGoody)
			{
				if (!goodyList[iGoody].is<std::string>())
				{
					continue;
				}
				const int iGoodyId = jsonResolveId(goodyList[iGoody].get<std::string>());
				if (iGoodyId >= 0)
				{
					m_aiGoodies.push_back(iGoodyId);   // unresolved GOODY_* surface via jsonResolveId (Orwell)
				}
			}
		}
	}

}
