//
//	CvHurryInfo -- the hurry poco's own typed reading on top of the base section dispatch (see the header).
//	mapFrom materializes the bespoke §9 `conversion` unit + the causesAnger flag ONCE
//	(docs/architecture/patterns.md §Materialize at mapFrom). Idempotent by contract (reset-first unit, unconditional assigns).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvHurryInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt / jsonIdBool


CvHurryInfo::Conversion::Conversion()
{
	reset();
}


// The unit's full redefinition (the mapFrom idempotency contract, CvInfo.h).
void CvHurryInfo::Conversion::reset()
{
	goldPerProduction = 0;
	productionPerPopulation = 0;
}


CvHurryInfo::CvHurryInfo()
	: m_bCausesAnger(false)
{
}


// conversion.{goldPerProduction, productionPerPopulation} -> the typed unit (mutually exclusive rush rates,
// raw ints, each absent -> 0); the top-level causesAnger flag -> the anger intrinsic.
void CvHurryInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the section dispatch

	// idempotency (CvInfo.h): the full-registry re-run fully redefines every materialized member
	m_conversion.reset();
	m_bCausesAnger = false;

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pConversion = jsonChildObj(entityObj, "conversion"))
	{
		m_conversion.goldPerProduction = jsonIdInt(*pConversion, "goldPerProduction");
		m_conversion.productionPerPopulation = jsonIdInt(*pConversion, "productionPerPopulation");
	}
	m_bCausesAnger = jsonIdBool(entityObj, "causesAnger");
}
