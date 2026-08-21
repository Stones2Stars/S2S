//
//	CvSpecialBuildingInfo -- the special-building poco's own typed reading on top of the base section dispatch
//	(see the header). `allowed` is parsed by the BASE dispatch into the composed m_allowed unit
//	(getMaxPlayerInstances reads it) -- never hand-parsed here: a private int once left getAllowed() NULL, and
//	the enabler's group gate (bd_groupCapOk) reads getAllowed(), so the cap silently never applied. Idempotent
//	by contract (unconditional assigns).
//

#include "CvGameCoreDLL.h"
#include "CvInfos.h"       // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvSpecialBuildingInfo.h"
#include "CvJsonParse.h"   // jsonChildObj

CvSpecialBuildingInfo::CvSpecialBuildingInfo()
	: m_iTechPrereq(NO_TECH)      // set at load by the tech-side un-inversion (CvReversePass)
	, m_bValid(true)              // legacy default TRUE (curator elides valid:true; only explicit valid:false overrides)
	, m_iMaxPlayerInstances(-1)   // -1 = uncapped (materialized at mapFrom from the composed allowed)
{
}

void CvSpecialBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + the composed sections

	// idempotency (CvInfo.h): unconditional redefinition (the reverse-pass-fed tech FK is written AFTER the
	// last mapFrom of a load, so it is never clobbered here)
	m_bValid = true;
	// the GROUP cap, off the base-dispatch-parsed composed unit (docs/architecture/patterns.md §Materialize at mapFrom)
	m_iMaxPlayerInstances = m_allowed.cap(ALLOWEDCAP_EMPIRE);

	if (!entity.is<picojson::object>())
	{
		return;
	}
	const picojson::object& entityObj = entity.get<picojson::object>();
	if (const picojson::object* pIdentity = jsonChildObj(entityObj, "identity"))
	{
		picojson::object::const_iterator validIt = pIdentity->find("valid");
		if (validIt != pIdentity->end() && validIt->second.is<bool>())
		{
			m_bValid = validIt->second.get<bool>();
		}
	}
}
