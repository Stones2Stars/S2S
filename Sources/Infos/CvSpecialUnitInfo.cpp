//
//	CvSpecialUnitInfo -- mapFrom materializes the identity flags ONCE ([DEC-materialize-at-mapfrom]); the
//	combat/withdrawal families ride the base section dispatch into the composed CvModifiers (no family-address
//	read survives here). Idempotent by contract (unconditional scalar assigns).
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvSpecialUnitInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdBool

CvSpecialUnitInfo::CvSpecialUnitInfo()
	: m_bValid(true)          // legacy default TRUE (curator elides valid:true; only an explicit valid:false overrides)
	, m_bCityLoad(false)
	, m_bSMLoadSame(false)
{
}

void CvSpecialUnitInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): fully redefine every materialized member before the guarded parse
	m_bValid = true;          // legacy default TRUE (curator elides valid:true; only an explicit valid:false overrides)
	m_bCityLoad = false;
	m_bSMLoadSame = false;

	CvInfo::mapFrom(entity);   // core reading (type / text keys) + the section dispatch (compiles m_modifiers)
	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();

	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		m_bCityLoad = jsonIdBool(*pIdentity, "cityLoad");
		m_bSMLoadSame = jsonIdBool(*pIdentity, "smLoadSame");
		picojson::object::const_iterator validIter = pIdentity->find("valid");
		if (validIter != pIdentity->end() && validIter->second.is<bool>())
		{
			m_bValid = validIter->second.get<bool>();
		}
	}
}
