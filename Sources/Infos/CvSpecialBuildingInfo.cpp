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
	, m_bValid(true)                  // legacy default TRUE (curator elides valid:true; only explicit valid:false overrides)
{
}


// #430: `allowed` is parsed by the BASE section dispatch into the composed m_allowed unit (getMaxPlayerInstances
// reads it) -- never hand-parsed here: a private int left getAllowed() NULL, and the enabler's group gate
// (bd_groupCapOk) reads getAllowed(), so the cap silently never applied.
// techPrereq is RECONSTRUCTED at load from the tech-side inversion (tech.enables.specialBuildings -> setTechPrereq,
// cascadeLoadJson); obsoleteTech / techPrereqAnyone stay NO_TECH by design (verified unused across all groups).
void CvSpecialBuildingInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading (type / text keys / button) + availability + the composed sections
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		picojson::object::const_iterator it = io->find("valid");
		if (it != io->end() && it->second.is<bool>()) m_bValid = it->second.get<bool>();
	}
}
