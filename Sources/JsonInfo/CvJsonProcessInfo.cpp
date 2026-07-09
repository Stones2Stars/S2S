//
//	CvJsonProcessInfo::mapFrom -- base core reading + availability (the tech prereq rides the base as tech.enables.
//	processes; the only-latest supersession as this process's obsoletedBy.techs), then the per-commerce production->
//	commerce conversion percents. Natural human %, NO ×100. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonProcessInfo.h"
#include "CvJsonParse.h"          // the shared walkers (jsonFamVal)

CvJsonProcessInfo::CvJsonProcessInfo()
	: m_eTechPrereq(NO_TECH)
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiProductionToCommerce[i] = 0;
}

void CvJsonProcessInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability (tech enables.processes / obsoletedBy.techs)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// split-commerce production->commerce conversion (the process's whole point) -- natural %, NOT ×100
	m_aiProductionToCommerce[COMMERCE_GOLD]      = jsonFamVal(o, "gold", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_RESEARCH]  = jsonFamVal(o, "research", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_CULTURE]   = jsonFamVal(o, "culture", "city", "percent");
	m_aiProductionToCommerce[COMMERCE_ESPIONAGE] = jsonFamVal(o, "espionage", "city", "percent");
}
