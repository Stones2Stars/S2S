//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialBuildingInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvSpecialBuildingInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt


CvSpecialBuildingInfo::CvSpecialBuildingInfo()
	: m_iObsoleteTech(NO_TECH)        // DATA GAP: no tech key in the JSON -- stays NO_TECH (fail-loud)
	, m_iTechPrereq(NO_TECH)          //   ""
	, m_iTechPrereqAnyone(NO_TECH)    //   "" (int-typed tech FK; -1 == NO_TECH)
	, m_iMaxPlayerInstances(-1)       // legacy default (archived .add(iMaxPlayerInstances, -1))
	, m_bValid(true)                  // legacy default TRUE (curator elides valid:true; only explicit valid:false overrides)
{
}


// #430: allowed.empire -> iMaxPlayerInstances; identity.valid overrides the true default only when explicitly present.
// ⛔ DATA GAP (accepted, fail-loud): specialbuildings/*.json carry NO tech data (curator missing), so
// techPrereq / obsoleteTech / techPrereqAnyone are NOT mapped and stay NO_TECH -- tech-gating + the cascade
// EDGEB_SPECIAL_BUILDINGS edges drop until the curator is written. Do NOT invent a mapping.
void CvSpecialBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* a = jsonChildObj(o, "allowed"))
		m_iMaxPlayerInstances = jsonIdInt(*a, "empire", -1);

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		picojson::object::const_iterator it = io->find("valid");
		if (it != io->end() && it->second.is<bool>()) m_bValid = it->second.get<bool>();
	}
}
