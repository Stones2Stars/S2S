//
//	CvBuildingInfo -- the building poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity set ONCE into typed members ([DEC-materialize-at-mapfrom]);
//	idempotent by contract (unconditional scalar assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvBuildingInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonResolveId / jsonCommerceMap
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "UI/CvArtFileMgr.h"    // ARTFILEMGR -- the getArtInfo shim (mirrors CvUnitInfo)

CvBuildingInfo::CvBuildingInfo()
	: m_iCost(0)
	, m_iCostSizeModifier(0)
	, m_iCostMaterialsModifier(0)
	, m_iCostComplexityModifier(0)
	, m_iCostCountModifier(0)
	, m_iCostHurryModifier(0)
	, m_iWorth(0)
	, m_iPopulationChange(0)
	, m_iGlobalPopulationChange(0)
	, m_iFreeTechs(0)
	, m_iFreeSpecialistsAny(0)
	, m_iMilitaryWorth(0)
	, m_iConquestProbability(0)
	, m_iVisibilityPriority(0)
	, m_iAirlift(0)
	, m_iAirUnitCapacity(0)
	, m_iWorkableRadius(0)
	, m_iMaxPlayerInstancesExtra(0)
	, m_bCenterInCity(false)
	, m_bNotConstructible(false)
	, m_bAutoBuild(false)
	, m_bNoInstanceLimit(false)
	, m_bAllowsNukes(false)
	, m_bForceNoPrereqScaling(false)
	, m_iGreatPeopleUnitType(-1)
	, m_iAdvisor(-1)
	, m_iSpecialBuildingType(-1)
	, m_iFreeStartEra(-1)
	, m_iDiploVoteType(-1)
	, m_iReligion(-1)
	, m_iShrineReligion(-1)
	, m_iHeadquartersCorporation(-1)
{
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiStateReligionCommerce[iCommerce] = 0;
		m_aiCommerceDoubleTime[iCommerce] = 0;
	}
}

namespace
{
	// A {<channel word>: N} commerce-keyed identity object -> a CommerceTypes-positional fill. The channel word
	// resolves through the ONE vocabulary (infoFamilyFromKey -> infoFamilyCommerce), never a local table.
	void bi_fillCommerceKeyed(const picojson::object& parent, const char* szKey, int (&aiOut)[NUM_COMMERCE_TYPES], bool bX100)
	{
		for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
		{
			aiOut[iCommerce] = 0;
		}
		const picojson::object* pChild = jsonChildObj(parent, szKey);
		if (pChild == NULL)
		{
			return;
		}
		for (picojson::object::const_iterator it = pChild->begin(); it != pChild->end(); ++it)
		{
			if (!it->second.is<double>())
			{
				continue;
			}
			const int iCommerce = infoFamilyCommerce(infoFamilyFromKey(it->first));
			if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES)
			{
				continue;
			}
			const double dValue = it->second.get<double>();
			aiOut[iCommerce] = bX100 ? jsonX100(dValue) : (int)dValue;
		}
	}
}

int CvBuildingInfo::getFlavorValue(FlavorTypes eFlavor) const
{
	return mapValueOrDefault(m_flavours, (int)eFlavor);
}

const CvArtInfoBuilding* CvBuildingInfo::getArtInfo() const
{
	return ARTFILEMGR.getBuildingArtInfo(m_szArtDefineTag.c_str());
}

void CvBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + the section dispatch (compiles m_modifiers)

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_aiMapCategories.clear();
	m_aiEnabledCivilizations.clear();
	m_victoryThresholds.clear();
	m_flavours.clear();
	m_iPopulationChange = 0;
	m_iGlobalPopulationChange = 0;
	m_iFreeTechs = 0;
	m_iFreeSpecialistsAny = 0;
	m_szArtDefineTag.clear();

	// PROPERTY_* per-turn SOURCES: a building's <PROPERTY_X>.city.flat (the crime/disease/pollution cuts and
	// adders that make the solver's numbers move) deposits in ITS OWN city -- NO_RELATION, the legacy building
	// shape -- while <PROPERTY_X>.empire.flat rides the all-cities container the city gather walks for the
	// owning player (property-audit.md's converted one-shots). The ONE shared walk over the compiled entries;
	// it clears both containers first, per the mapFrom idempotency contract.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, NO_RELATION, 0,
		&m_PropertyManipulatorsAllCities);

	// world.art.define -- the ART_DEF_* tag getArtInfo resolves through ArtFileMgr (every building authors one).
	if (const picojson::object* pArt = jsonWorldArt(entity))
	{
		jsonIdStr(*pArt, "define", m_szArtDefineTag);
	}

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// --- the §9 FK sections: shrine (religion) + headquarters (corporation) -- the relationship IS the data ---
	std::string szFk;
	m_iShrineReligion = -1;
	m_iHeadquartersCorporation = -1;
	if (jsonIdStr(entityObj, "shrine", szFk))
	{
		m_iShrineReligion = jsonResolveId(szFk);
	}
	if (jsonIdStr(entityObj, "headquarters", szFk))
	{
		m_iHeadquartersCorporation = jsonResolveId(szFk);
	}

	// --- the identity block: the census set, each key one typed member ---
	// ⛔ An entity with NO `identity` block must still FULLY REDEFINE every member below -- the mapFrom
	// idempotency contract (CvInfo.h), which the full-registry re-run depends on. So the block is read against
	// an EMPTY object when absent and each jsonId* call takes its own default; an early return here instead left
	// the previous pass's scalars standing.
	static const picojson::object kEmptyIdentity;
	const picojson::object* pIdentity = jsonChildObj(entityObj, "identity");
	const picojson::object& identity = (pIdentity != NULL) ? *pIdentity : kEmptyIdentity;
	m_iWorth = jsonIdInt(identity, "worth");
	// The `cost` section (json §7) -- fully redefined every (re-)map like every other materialized member, so
	// an entity that LOSES its cost block on a re-curate reads 0 rather than keeping the previous pass's value.
	{
		static const picojson::object kEmptyCost;
		const picojson::object* pCost = jsonChildObj(entityObj, "cost");
		const picojson::object& cost = (pCost != NULL) ? *pCost : kEmptyCost;
		m_iCost                   = jsonIdInt(cost, "production");
		m_iCostSizeModifier       = jsonIdInt(cost, "sizeModifier");
		m_iCostMaterialsModifier  = jsonIdInt(cost, "materialsModifier");
		m_iCostComplexityModifier = jsonIdInt(cost, "complexityModifier");
		m_iCostCountModifier      = jsonIdInt(cost, "countModifier");
		m_iCostHurryModifier      = jsonIdInt(cost, "hurryModifier");
	}
	// json §7 `ai` METADATA (flavours) -- AI weighting only, never a rule. Shared reader, same as the
	// leaderhead's; absent block leaves the map empty, so an absent flavour reads 0.
	{
		const picojson::object* pAiMeta = jsonChildObj(entityObj, "ai");
		if (pAiMeta != NULL) { jsonReadFlavours(*pAiMeta, m_flavours); }
	}

	// The considered-action PULSES, materialized as HUMAN counts: `grants.population` per scope and
	// `grants.freeTechs`. Interned key handles, resolved once ([DEC-materialize-at-mapfrom]); the pulses are
	// ×100 at parse, so the /100 here is the single human boundary and no reader repeats it.
	{
		static const int iKeyPopulation = CvGrants::key("population");
		static const int iKeyFreeTechs  = CvGrants::key("freeTechs");
		static const int iKeyScopeCity  = CvGrants::key("city");
		static const int iKeyScopeEmpire = CvGrants::key("empire");
		m_iPopulationChange       = grantScopedPulse(iKeyPopulation, iKeyScopeCity) / 100;
		m_iGlobalPopulationChange = grantScopedPulse(iKeyPopulation, iKeyScopeEmpire) / 100;
		m_iFreeTechs              = grantPulse(iKeyFreeTechs) / 100;
	}

	// freeSpecialists.city.`any` -- the generic slot count, now an ORDINARY compiled point read: `any` is the
	// untyped bucket the engine assigns a type to at placement, so it decodes as the memberless scope-wide
	// amount rather than a named target, and the compiled sum already excludes the conditioned tail.
	m_iFreeSpecialistsAny =
		m_modifiers.sum(MODFAM_FREE_SPECIALISTS, CHANNEL_AMOUNT, CASC_SCOPE_CITY, CASC_UNIT_COUNT) / 100;
	m_iMilitaryWorth = jsonIdInt(identity, "militaryWorth");
	m_iConquestProbability = jsonIdInt(identity, "conquestProbability");
	m_iVisibilityPriority = jsonIdInt(identity, "visibilityPriority");
	m_iAirlift = jsonIdInt(identity, "airlift");
	m_iAirUnitCapacity = jsonIdInt(identity, "airUnitCapacity");
	m_iWorkableRadius = jsonIdInt(identity, "workableRadius");
	m_iMaxPlayerInstancesExtra = jsonIdInt(identity, "maxPlayerInstancesExtra");
	m_bCenterInCity = jsonIdBool(identity, "centerInCity");
	m_bNotConstructible = jsonIdBool(identity, "notConstructible");
	m_bAutoBuild = jsonIdBool(identity, "autoBuild");
	m_bNoInstanceLimit = jsonIdBool(identity, "noInstanceLimit");
	m_bAllowsNukes = jsonIdBool(identity, "allowsNukes");
	m_bForceNoPrereqScaling = jsonIdBool(identity, "forceNoPrereqScaling");
	m_iGreatPeopleUnitType = jsonIdFk(identity, "greatPeopleUnitType");
	m_iAdvisor = jsonIdFk(identity, "advisor");
	m_iSpecialBuildingType = jsonIdFk(identity, "specialBuildingType");
	m_iFreeStartEra = jsonIdFk(identity, "freeStartEra");
	m_iDiploVoteType = jsonIdFk(identity, "diploVoteType");
	m_iReligion = jsonIdFk(identity, "religion");

	// FK lists (MAPCATEGORY_* / CIVILIZATION_*): unresolved ids surface via jsonResolveId's diagnostic (Orwell)
	picojson::object::const_iterator listIt = identity.find("mapCategories");
	if (listIt != identity.end() && listIt->second.is<picojson::array>())
	{
		const picojson::array& categories = listIt->second.get<picojson::array>();
		for (size_t i = 0; i < categories.size(); ++i)
		{
			if (categories[i].is<std::string>())
			{
				const int iCategory = jsonResolveId(categories[i].get<std::string>());
				if (iCategory >= 0)
				{
					m_aiMapCategories.push_back(iCategory);
				}
			}
		}
	}
	listIt = identity.find("enabledCivilizations");
	if (listIt != identity.end() && listIt->second.is<picojson::array>())
	{
		const picojson::array& civilizations = listIt->second.get<picojson::array>();
		for (size_t i = 0; i < civilizations.size(); ++i)
		{
			if (civilizations[i].is<std::string>())
			{
				const int iCivilization = jsonResolveId(civilizations[i].get<std::string>());
				if (iCivilization >= 0)
				{
					m_aiEnabledCivilizations.push_back(iCivilization);
				}
			}
		}
	}
	// victoryThresholds: [ { "VICTORY_X": N }, ... ] -- VICTORY_* FK -> threshold count
	listIt = identity.find("victoryThresholds");
	if (listIt != identity.end() && listIt->second.is<picojson::array>())
	{
		const picojson::array& thresholds = listIt->second.get<picojson::array>();
		for (size_t i = 0; i < thresholds.size(); ++i)
		{
			if (!thresholds[i].is<picojson::object>())
			{
				continue;
			}
			const picojson::object& row = thresholds[i].get<picojson::object>();
			for (picojson::object::const_iterator rowIt = row.begin(); rowIt != row.end(); ++rowIt)
			{
				if (!rowIt->second.is<double>())
				{
					continue;
				}
				const int iVictory = jsonResolveId(rowIt->first);
				if (iVictory >= 0)
				{
					m_victoryThresholds[iVictory] = (int)rowIt->second.get<double>();
				}
			}
		}
	}
	// the two commerce-keyed identity configs: stateReligionCommerce is a MAGNITUDE (×100);
	// commerceDoubleTime is TURNS (a plain count)
	bi_fillCommerceKeyed(identity, "stateReligionCommerce", m_aiStateReligionCommerce, true);
	bi_fillCommerceKeyed(identity, "commerceDoubleTime", m_aiCommerceDoubleTime, false);
}
