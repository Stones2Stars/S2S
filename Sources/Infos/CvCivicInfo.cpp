//
//	CvCivicInfo -- the civic poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the census identity set ONCE into typed members ([DEC-materialize-at-mapfrom]) and
//	SOURCE-resolves the CITY_LIMIT per.above token onto its own compiled entries (ruling 26, the SELF-collapse
//	precedent, json §3.1). Idempotent: unconditional assignment + the resolver only fills entries the fresh
//	parse left unresolved.
//

#include "CvGameCoreDLL.h"
#include "CvCivicInfo.h"
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdFk / jsonIdStr
#include "CvModEntry.h"    // the compiled entries -- the over-limit-anger presence derivation

CvCivicInfo::CvCivicInfo()
	: m_iCivicOption(-1)
	, m_iAnarchyLength(0)
	, m_iUpkeepLevel(-1)
	, m_iCityLimit(0)
	, m_bCityOverLimitAnger(false)
{
}

void CvCivicInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)

	m_iCivicOption = -1;
	m_iAnarchyLength = 0;
	m_iUpkeepLevel = -1;
	m_iCityLimit = 0;
	m_bCityOverLimitAnger = false;
	m_szWeLoveTheKingKey.clear();

	// PROPERTY_* per-turn SOURCES: an adopted civic's <PROPERTY_X>.city.flat deposits in EVERY owner city --
	// RELATION_ASSOCIATED, the player-gathered fan the legacy civic manipulators used. The ONE shared walk over
	// the compiled entries; it clears the container first, per the mapFrom idempotency contract.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iCivicOption = jsonIdFk(*pIdentity, "civicOption");
		m_iAnarchyLength = jsonIdInt(*pIdentity, "anarchyLength");
		m_iUpkeepLevel = jsonIdFk(*pIdentity, "upkeepLevel");
		m_iCityLimit = jsonIdInt(*pIdentity, "cityLimit");
		std::string szTextKey;
		if (jsonIdStr(*pIdentity, "weLoveTheKing", szTextKey))
		{
			m_szWeLoveTheKingKey = CvWString(szTextKey.c_str());
		}
	}

	// The CITY_LIMIT per.above token (ruling 26: `{-V, per:{CITY, above:"CITY_LIMIT"}}` on the government
	// civics) is SOURCE-resolved here -- the depositing civic stamps its own base limit onto its compiled
	// entries; the world-size scaling leg stays at eval (MMKernel::perApply).
	if (m_iCityLimit > 0)
	{
		m_modifiers.resolveAboveToken("CITY_LIMIT", m_iCityLimit);
	}
	// a CITY_LIMIT token on a civic with NO cityLimit config stays unresolved (perAbove -1): perScale skips
	// the scaling -- surfaced by the data being wrong, never silently zeroed (the curator warns on that case)

	// Ruling 26, option (a): the over-limit-ANGER PRESENCE derives from the compiled CITY_LIMIT `per.above`
	// entries -- a load-time scan of the compiled entry list (the ONE sanctioned load-time scan source,
	// patterns.md § Materialize at mapFrom), materialized to a bool the engine's hard-cap sites read bare.
	const std::vector<CvModEntry*>& entries = m_modifiers.entries();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const CvModEntry* pEntry = entries[i];
		if (pEntry->hasAbove && pEntry->perAboveToken == "CITY_LIMIT")
		{
			m_bCityOverLimitAnger = true;
			break;
		}
	}
}
