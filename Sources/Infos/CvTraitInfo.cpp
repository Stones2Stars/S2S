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
	m_aiExcludes.clear();
	m_flavours.clear();
	m_bCoastalAIInfluence = false;

	if (entity.is<picojson::object>())
	{
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

	// --- par.9 `excludes` -- the same-tier traits this one is mutually exclusive with ---
	jsonReadIdList(entityObj, "excludes", m_aiExcludes);

	// --- par.7 `ai` metadata -- the flavour weights the level-up valuation multiplies the leader's own against ---
	if (const picojson::object* pAi = jsonChildObj(entityObj, "ai"))
	{
		jsonReadFlavours(*pAi, m_flavours);
		if (const picojson::object* pBehaviour = jsonChildObj(*pAi, "behaviour"))
		{
			m_bCoastalAIInfluence = jsonIdBool(*pBehaviour, "coastalAIInfluence");
		}
	}
	}

	// ⚖ THE PURE_TRAITS ALIGNMENT GATE (modifier.md §4) -- a PARSE TRANSFORM, run here because this is the point
	// between the trait having been READ (its alignment flag is set above) and its entries LANDING in the
	// compiled forms. It gates rather than drops, and gating moves an entry out of the point sums into the
	// conditioned list, so both planes come out right with nothing threaded to any consumer (CvModifiers.cpp).
	// ⛔ IT RUNS BEFORE THE BRIDGE BELOW, and the order is load-bearing rather than cosmetic: the bridge walks
	// these same entries to build the property manipulators, so a downside property source on a positive trait
	// would otherwise be bridged ungated and deposit for the whole game.
	m_modifiers.applyPureTraitGate(m_bNegativeTrait);

}
