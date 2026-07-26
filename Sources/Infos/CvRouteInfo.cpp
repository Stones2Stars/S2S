//
//	CvRouteInfo -- the route poco's own typed reading on top of the base section dispatch (see the header).
//	The yield and movement families compile into m_modifiers via the base dispatch -- no per-family raw read
//	survives here ([DEC-new-getter-surface]); the improvement-keyed rows and the tech-conditioned move deltas
//	are compiled keyed/conditioned entries. mapFrom materializes the identity census set ONCE into typed
//	members ([DEC-materialize-at-mapfrom]); idempotent by contract. The bonus-prereq forward FKs reset here
//	and are re-landed by CvReversePass after every re-map.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvRouteInfo.h"
#include "CvJsonParse.h"          // the shared walkers (jsonChildObj/jsonIdInt/jsonIdBool)
#include "AI/CvGameAI.h"          // complete CvGameAI -- GC.getGame().getSorenRand() (zobrist draw, mirrors the archive)

CvRouteInfo::CvRouteInfo()
	: m_iValue(0)
	, m_iAdvancedStartCost(100)
	, m_iMovementCost(0)
	, m_iFlatMovementCost(0)
	, m_iZobristValue(0)
	, m_bSeaTunnel(false)
	, m_ePrereqBonus(NO_BONUS)
{
	// Non-XML runtime map-hash value, drawn from the synced RNG at info construction EXACTLY as the archived
	// CvRouteInfo ctor did. CvPlot XORs it into m_movementCharacteristicsHash.
	m_iZobristValue = GC.getGame().getSorenRand().getInt();
}

void CvRouteInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the full-registry pass re-runs mapFrom. The bonus-prereq forward FKs
	// reset here because CvReversePass re-lands them AFTER every re-map (the OR-list would double otherwise).
	m_ePrereqBonus = NO_BONUS;
	m_aePrereqOrBonuses.clear();

	CvInfo::mapFrom(entity);   // core reading + the section dispatch (compiles m_modifiers)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	// identity: the intrinsic route stats
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_iValue = jsonIdInt(*pIdentity, "value");
		m_iMovementCost = jsonIdInt(*pIdentity, "movementCost");
		m_iFlatMovementCost = jsonIdInt(*pIdentity, "flatMovementCost");
		m_bSeaTunnel = jsonIdBool(*pIdentity, "seaTunnel");
		if (const picojson::object* pAdvancedStart = jsonChildObj(*pIdentity, "advancedStart"))
		{
			m_iAdvancedStartCost = jsonIdInt(*pAdvancedStart, "cost", 100);   // legacy load default 100
		}
	}
	// (m_iZobristValue is drawn in the ctor -- non-XML runtime value, see there.)
}
