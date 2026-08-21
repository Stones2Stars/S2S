//
//	CvTechInfo -- the tech poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the capability-plane sibling blocks + the census identity set ONCE into typed members
//	(docs/architecture/patterns.md §Materialize at mapFrom) and walks the composed requires.build tree into the forward prereq views
//	(enabler.md §2). Idempotent by contract (unconditional scalar assigns, clear-first containers). Every
//	modifier family the tech authors compiles into m_modifiers via the base dispatch -- no per-family raw read
//	survives here (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)).
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvTechInfo.h"
#include "CvJsonParse.h"            // jsonBoolSet / jsonResolveId / jsonChildObj / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonX100 / jsonReadFlavours

CvTechInfo::CvTechInfo()
	: m_iResearchCost(0)
	, m_iEra(-1)
	, m_iAdvisor(-1)
	, m_iGridX(0)
	, m_iGridY(0)
	, m_iWorth(0)
	, m_iMilitaryWorth(0)
	, m_bRepeat(false)
	, m_bTradeable(false)
	, m_bDisable(false)
	, m_bGoodyTech(false)
	, m_iAIWeight(0)
	, m_iAITradeModifier(0)
{
}

// identity.quote -- RESOLVE the loaded TXT_KEY to display text (see the header).
std::wstring CvTechInfo::getQuote() const
{
	return gDLL->getText(m_szQuoteKey);
}

namespace
{
	// identity double (the ×100 worth fields author fractional human values).
	double ti_identityDouble(const picojson::object& identity, const char* szKey)
	{
		picojson::object::const_iterator valueIter = identity.find(szKey);
		if (valueIter == identity.end())
		{
			return 0.0;
		}
		if (!valueIter->second.is<double>())
		{
			return 0.0;
		}
		return valueIter->second.get<double>();
	}
}

void CvTechInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom -- fully define every accumulating
	// member (the ability sets clear-first; the prereq vectors would double).
	// NB m_leadsTo is NOT cleared here -- the reverse pass's rp_deriveTechLeadsTo owns it (clear-first), not this parse
	// (addLeadsToTech is a std::set insert, so a re-populate cannot double).
	m_canTradeOnTerrains.clear();
	m_flavours.clear();
	m_aePrereqAndTechs.clear();
	m_aePrereqOrTechs.clear();
	m_aPrereqOrBuildings.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers, parses the sections)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- the capability-plane sibling blocks (json §8 / capabilities.md): `capabilities`, `canTrade` and
	// `canWorkOn` are flat-bool blocks and ride the ONE section dispatch into their composed units; only
	// `canTradeOn` is bespoke, carrying TERRAIN_ FKs rather than authored keys, so it parses here. ---
	picojson::object::const_iterator blockIter = entityObj.find("canTradeOn");
	if (blockIter != entityObj.end() && blockIter->second.is<picojson::object>())
	{
		const picojson::object& canTradeOnObj = blockIter->second.get<picojson::object>();
		picojson::object::const_iterator terrainsIter = canTradeOnObj.find("terrains");
		if (terrainsIter != canTradeOnObj.end() && terrainsIter->second.is<picojson::array>())
		{
			const picojson::array& terrainList = terrainsIter->second.get<picojson::array>();
			for (size_t iTerrain = 0; iTerrain < terrainList.size(); ++iTerrain)
			{
				if (!terrainList[iTerrain].is<std::string>())
				{
					continue;
				}
				const int iTerrainId = jsonResolveId(terrainList[iTerrain].get<std::string>());
				if (iTerrainId >= 0)
				{
					m_canTradeOnTerrains.insert(iTerrainId);
				}
			}
		}
	}

	// --- cost.research (plane-1 actual cost, ruling 18) ---
	if (const picojson::object* pCost = jsonChildObj(entityObj, "cost"))
	{
		m_iResearchCost = jsonIdInt(*pCost, "research");
	}

	// --- identity: scalars / flags / FK / the ×100 worth magnitudes / the quote key ---
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iEra = jsonIdFk(*pIdentity, "era");
		m_iGridX = jsonIdInt(*pIdentity, "gridX");
		m_iGridY = jsonIdInt(*pIdentity, "gridY");
		m_iWorth = jsonX100(ti_identityDouble(*pIdentity, "worth"));
		m_iMilitaryWorth = jsonX100(ti_identityDouble(*pIdentity, "militaryWorth"));
		m_bRepeat = jsonIdBool(*pIdentity, "repeat");
		m_bTradeable = jsonIdBool(*pIdentity, "tradeable");
		m_bDisable = jsonIdBool(*pIdentity, "disable");
		m_bGoodyTech = jsonIdBool(*pIdentity, "goodyTech");
		std::string szQuote;
		jsonIdStr(*pIdentity, "quote", szQuote);
		if (!szQuote.empty())
		{
			m_szQuoteKey = CvWString(szQuote.c_str());
		}
	}

	// --- ui.art.advisor (FK) -- NB under ui.art, not identity ---
	if (const picojson::object* pUi = jsonChildObj(entityObj, "ui"))
	{
		if (const picojson::object* pArt = jsonChildObj(*pUi, "art"))
		{
			m_iAdvisor = jsonIdFk(*pArt, "advisor");
		}
	}

	// --- sound.{sound,soundMP} -- the tech-completed jingle + the MP variant (distinct keys) ---
	// Cleared first: jsonIdStr only assigns when the key is present (mapFrom idempotency, CvInfo.h).
	m_szSound.clear();
	m_szSoundMP.clear();
	if (const picojson::object* pSound = jsonChildObj(entityObj, "sound"))
	{
		jsonIdStr(*pSound, "sound", m_szSound);
		jsonIdStr(*pSound, "soundMP", m_szSoundMP);
	}

	// --- ai.behaviour.{weight,tradeModifier} + ai.flavours ---
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		if (const picojson::object* pBehaviour = jsonChildObj(*pAi, "behaviour"))
		{
			m_iAIWeight = jsonIdInt(*pBehaviour, "weight");
			m_iAITradeModifier = jsonIdInt(*pBehaviour, "tradeModifier");
		}
		jsonReadFlavours(*pAi, m_flavours);
	}

	// --- the forward prereq views: walk the composed requires.build the base just parsed (enabler.md §2 --
	// the tech case reconstructs from the child's RETAINED requires.build.all/.any). curate_tech.requires_fn
	// builds ONE `all` list holding: team-scope TECH_ presence atoms (AND prereqs, incl. folded 1-member ORs),
	// and nested {any} OR-groups (each homogeneous: a multi-member tech OR-group, or the building OR-group
	// with min counts). ---
	const CvRequires* pRequires = getRequires();
	if (pRequires == NULL || pRequires->build == NULL)
	{
		return;
	}
	const CvCondition* pBuild = pRequires->build;
	for (size_t iChild = 0; iChild < pBuild->all.size(); ++iChild)
	{
		const CvCondition* pChild = pBuild->all[iChild];
		if (pChild == NULL)
		{
			continue;
		}
		if (pChild->kind == CASC_COND_PRESENCE)
		{
			if (pChild->id >= 0 && pChild->type.compare(0, 5, "TECH_") == 0)
			{
				m_aePrereqAndTechs.push_back((TechTypes)pChild->id);   // AND tech prereq (incl. folded 1-member OR)
			}
			continue;
		}
		if (pChild->kind != CASC_COND_GROUP || pChild->anyOf.empty())
		{
			continue;
		}
		const CvCondition* pFirst = pChild->anyOf[0];
		if (pFirst == NULL || pFirst->kind != CASC_COND_PRESENCE)
		{
			continue;
		}
		const bool bTech = pFirst->type.compare(0, 5, "TECH_") == 0;
		const bool bBuilding = pFirst->type.compare(0, 9, "BUILDING_") == 0;
		if (pChild->anyOf.size() == 1)   // a single-member OR is logically a hard AND (defensive: curator pre-folds)
		{
			if (bTech && pFirst->id >= 0)
			{
				m_aePrereqAndTechs.push_back((TechTypes)pFirst->id);
			}
			continue;
		}
		if (bTech)   // FIRST multi-member TECH OR-group (mirrors the legacy single Or-list)
		{
			if (!m_aePrereqOrTechs.empty())
			{
				// the single-Or-list forward view holds ONE TECH OR-group; a further group stays gated by
				// requires.build itself but cannot appear in this view -- surfaced, never silent
				FErrorMsg(CvString::format("CvTechInfo::mapFrom(%s) -- a second multi-member TECH_ OR-group in "
					"requires.build.all exceeds the single-Or-list forward view", getType()).c_str());
				gDLL->logMsg("Loading.log", CvString::format(
					"[READJSON] ERROR or-group-skipped tech=%s class=TECH_ members=%d",
					getType(), (int)pChild->anyOf.size()).c_str(), true, false);
				continue;
			}
			for (size_t iMember = 0; iMember < pChild->anyOf.size(); ++iMember)
			{
				const CvCondition* pMember = pChild->anyOf[iMember];
				if (pMember != NULL && pMember->kind == CASC_COND_PRESENCE && pMember->id >= 0
					&& pMember->type.compare(0, 5, "TECH_") == 0)
				{
					m_aePrereqOrTechs.push_back((TechTypes)pMember->id);
				}
			}
			continue;
		}
		if (bBuilding)   // the BUILDING_ OR-group ((building, min) count atoms)
		{
			if (!m_aPrereqOrBuildings.empty())
			{
				// the single-Or-list forward view holds ONE BUILDING OR-group; a further group stays gated by
				// requires.build itself but cannot appear in this view -- surfaced, never silent
				FErrorMsg(CvString::format("CvTechInfo::mapFrom(%s) -- a second multi-member BUILDING_ OR-group in "
					"requires.build.all exceeds the single-Or-list forward view", getType()).c_str());
				gDLL->logMsg("Loading.log", CvString::format(
					"[READJSON] ERROR or-group-skipped tech=%s class=BUILDING_ members=%d",
					getType(), (int)pChild->anyOf.size()).c_str(), true, false);
				continue;
			}
			for (size_t iMember = 0; iMember < pChild->anyOf.size(); ++iMember)
			{
				const CvCondition* pMember = pChild->anyOf[iMember];
				if (pMember != NULL && pMember->kind == CASC_COND_PRESENCE && pMember->id >= 0
					&& pMember->type.compare(0, 9, "BUILDING_") == 0)
				{
					PrereqBuilding prereqBuilding;
					prereqBuilding.eBuilding = (BuildingTypes)pMember->id;
					prereqBuilding.iMinimumRequired = (pMember->min > 0) ? pMember->min : 1;
					m_aPrereqOrBuildings.push_back(prereqBuilding);
				}
			}
		}
	}
}

// --- the synthetic TECH_GAME_START root (readjson.md §5.1) ---
// No engine id -> lives OFF the InfoRepo as this owned singleton. Reset-recreate (never re-parse into a stale
// object) keeps the write-once-at-load discipline across a re-map.
static CvTechInfo* s_pStartNode = NULL;

CvTechInfo& cascadeStartNode()
{
	if (s_pStartNode == NULL)
	{
		s_pStartNode = new CvTechInfo();
	}
	return *s_pStartNode;
}

void cascadeStartNodeReset()
{
	delete s_pStartNode;
	s_pStartNode = NULL;
}
