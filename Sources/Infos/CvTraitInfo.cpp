//
//	CvTraitInfo -- the trait poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the census identity set ONCE into typed members ([DEC-materialize-at-mapfrom]);
//	idempotent by contract (unconditional assigns).
//

#include "CvGameCoreDLL.h"
#include "CvTraitInfo.h"
#include "Property/CvPropertyBridge.h" // the shared PROPERTY_* family -> manipulator walk
#include "CvJsonParse.h"   // jsonChildObj / jsonIdInt / jsonIdBool / jsonIdStr / jsonIdFk / jsonReadIdList

CvTraitInfo::CvTraitInfo()
	: m_bNegativeTrait(false)
	, m_bBarbarianSelectionOnly(false)
	, m_bImpurePropertyManipulators(false)
	, m_bImpurePromotions(false)
	, m_iMinAnarchy(0)
	, m_iMaxAnarchy(0)
	, m_iSuccessionPromotionLine(-1)
	, m_iSuccessionPriority(0)
{
}

void CvTraitInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)

	m_bNegativeTrait = false;
	m_bBarbarianSelectionOnly = false;
	m_bImpurePropertyManipulators = false;
	m_bImpurePromotions = false;
	m_iMinAnarchy = 0;
	m_iMaxAnarchy = 0;
	m_szShortDescriptionKey.clear();
	m_iSuccessionPromotionLine = -1;
	m_iSuccessionPriority = 0;
	m_aiExcludes.clear();

	// PROPERTY_* per-turn SOURCES: a trait's <PROPERTY_X>.city.flat deposits in EVERY owner city while the trait
	// is held -- RELATION_ASSOCIATED, the player-gathered fan the legacy trait manipulators used. The ONE shared
	// walk over the compiled entries; it clears the container first, per the mapFrom idempotency contract.
	CascadePropertyBridge::bridgeFamilies(getModifiers(), m_PropertyManipulators, RELATION_ASSOCIATED);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bNegativeTrait = jsonIdBool(*pIdentity, "negativeTrait");
		m_bBarbarianSelectionOnly = jsonIdBool(*pIdentity, "barbarianSelectionOnly");
		m_bImpurePropertyManipulators = jsonIdBool(*pIdentity, "impurePropertyManipulators");
		m_bImpurePromotions = jsonIdBool(*pIdentity, "impurePromotions");
		m_iMinAnarchy = jsonIdInt(*pIdentity, "minAnarchy");
		m_iMaxAnarchy = jsonIdInt(*pIdentity, "maxAnarchy");
		std::string szTextKey;
		if (jsonIdStr(*pIdentity, "shortDescription", szTextKey))
		{
			m_szShortDescriptionKey = CvWString(szTextKey.c_str());
		}
	}

	// --- par.9 succession.{promotionLine, priority} + excludes ---
	if (const picojson::object* pSuccession = jsonChildObj(entityObj, "succession"))
	{
		m_iSuccessionPromotionLine = jsonIdFk(*pSuccession, "promotionLine");
		m_iSuccessionPriority = jsonIdInt(*pSuccession, "priority");
	}
	jsonReadIdList(entityObj, "excludes", m_aiExcludes);
}
