//------------------------------------------------------------------------------------------------
//  FILE:    CvSpecialBuildingInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvInfos.h"              // umbrella: keeps the unity batch's info-type defs whole (leakage guard)
#include "AI/CvGameAI.h"
#include "CvSpecialBuildingInfo.h"
#include "CvJsonParse.h"          // jsonChildObj / jsonIdInt


CvSpecialBuildingInfo::CvSpecialBuildingInfo()
	: m_iTechPrereq(NO_TECH)          // set at load by the tech-side un-inversion (loadJson)
	, m_iTechPrereqAnyone(NO_TECH)    // no authoring exists in the XML (int-typed tech FK; -1 == NO_TECH)
	, m_bValid(true)                  // legacy default TRUE (curator elides valid:true; only explicit valid:false overrides)
{
}


// #430: `allowed` is parsed by the BASE section dispatch into the composed m_allowed unit (getMaxPlayerInstances
// reads it) -- never hand-parsed here: a private int left getAllowed() NULL, and the enabler's group gate
// (bd_groupCapOk) reads getAllowed(), so the cap silently never applied.
// techPrereq is RECONSTRUCTED at load from the tech-side inversion (tech.enables.specialBuildings -> setTechPrereq,
// loadJson); obsoleteTech reads the `obsoletedBy.techs` edge off the base dispatch; techPrereqAnyone stays
// NO_TECH (the XML carries no authoring for it).
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
