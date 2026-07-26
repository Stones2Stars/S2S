//
//	CvBuildingInfo -- the building poco's own typed reading on top of the base section dispatch (see the
//	header). mapFrom materializes the census identity set ONCE into typed members ([DEC-materialize-at-mapfrom]);
//	idempotent by contract (unconditional scalar assigns, clear-first containers).
//

#include "CvGameCoreDLL.h"
#include "CvBuildingInfo.h"
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdFk / jsonIdStr / jsonResolveId / jsonCommerceMap

CvBuildingInfo::CvBuildingInfo()
	: m_iWorth(0)
	, m_iMilitaryWorth(0)
	, m_iConquestProbability(0)
	, m_iVisibilityPriority(0)
	, m_iSightRange(0)
	, m_iAirlift(0)
	, m_iAirUnitCapacity(0)
	, m_iWorkableRadius(0)
	, m_iMaxPlayerInstancesExtra(0)
	, m_iDcmAirbombMission(0)
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

void CvBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys) + the section dispatch (compiles m_modifiers)

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_aiMapCategories.clear();
	m_aiEnabledCivilizations.clear();
	m_victoryThresholds.clear();

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
	const picojson::object* pIdentity = jsonChildObj(entityObj, "identity");
	if (pIdentity == NULL)
	{
		return;
	}
	const picojson::object& identity = *pIdentity;
	m_iWorth = jsonIdInt(identity, "worth");
	m_iMilitaryWorth = jsonIdInt(identity, "militaryWorth");
	m_iConquestProbability = jsonIdInt(identity, "conquestProbability");
	m_iVisibilityPriority = jsonIdInt(identity, "visibilityPriority");
	m_iSightRange = jsonIdInt(identity, "sightRange");
	m_iAirlift = jsonIdInt(identity, "airlift");
	m_iAirUnitCapacity = jsonIdInt(identity, "airUnitCapacity");
	m_iWorkableRadius = jsonIdInt(identity, "workableRadius");
	m_iMaxPlayerInstancesExtra = jsonIdInt(identity, "maxPlayerInstancesExtra");
	m_iDcmAirbombMission = jsonIdInt(identity, "dcmAirbombMission");
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
