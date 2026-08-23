//
//	CvProcessInfo -- the process poco (see the header). The §9 `conversion` block (item 18: hammers->commerce
//	conversion, hurry's bespoke-block home) materializes ONCE at mapFrom into the typed per-channel plane
//	(docs/architecture/patterns.md §Materialize at mapFrom); the getter is a bare member read.
//

#include "CvGameCoreDLL.h"
#include "CvProcessInfo.h"
#include "CvJsonParse.h"   // jsonChildObj + jsonIdInt -- the shared parse primitives (never re-hand-rolled)

CvProcessInfo::CvProcessInfo()
	: m_eTechPrereq(NO_TECH)
{
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiProductionToCommerce[iCommerce] = 0;
	}
}

void CvProcessInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + the section dispatch (fills edges: obsoletedBy.techs)

	// the §9 `conversion` block -- {gold|research|culture|espionage: humanPercent}, CommerceTypes order below.
	// Idempotent: the plane is fully redefined on every (re-)map (zero-fill first).
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiProductionToCommerce[iCommerce] = 0;
	}
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object* pConversion = jsonChildObj(entity.get<picojson::object>(), "conversion");
	if (pConversion == NULL)
	{
		return;
	}
	const char* aszChannels[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };
	for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
	{
		m_aiProductionToCommerce[iCommerce] = jsonIdInt(*pConversion, aszChannels[iCommerce]);
	}
}
