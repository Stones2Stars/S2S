//
//	CvJsonSpecialistInfo::mapFrom -- base core reading + availability, then the specialist's CITY-scope output +
//	identity tags. ⏳-flagged shapes to confirm. FK resolution via the kept type registry. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonSpecialistInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...) + jsonX100

CvJsonSpecialistInfo::CvJsonSpecialistInfo()
	: m_iGreatPeopleRateChange(0), m_iGreatPeopleUnitType(-1), m_iExperience(0), m_iHealthPercent(0),
	  m_iHappinessPercent(0), m_iInsidiousness(0), m_iInvestigation(0), m_bSlave(false), m_bVisible(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiCommerceChange[i] = 0;
}

void CvJsonSpecialistInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// city-scope yields + commerce (the specialist's own output)
	m_aiYieldChange[YIELD_FOOD]       = jsonFamVal(o, "food", "city", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = jsonFamVal(o, "production", "city", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = jsonFamVal(o, "commerce", "city", "flat");
	m_aiCommerceChange[COMMERCE_GOLD]      = jsonFamVal(o, "gold", "city", "flat");
	m_aiCommerceChange[COMMERCE_RESEARCH]  = jsonFamVal(o, "research", "city", "flat");
	m_aiCommerceChange[COMMERCE_CULTURE]   = jsonFamVal(o, "culture", "city", "flat");
	m_aiCommerceChange[COMMERCE_ESPIONAGE] = jsonFamVal(o, "espionage", "city", "flat");

	m_iGreatPeopleRateChange = jsonFamVal(o, "greatPeopleRate", "city", "flat");
	// investigation/insidiousness/experience are FAMILIES at city.flat (×1) -- NOT identity (curate_specialist.py:26-28).
	m_iInvestigation = jsonFamVal(o, "investigation", "city", "flat");
	m_iInsidiousness = jsonFamVal(o, "insidiousness", "city", "flat");
	m_iExperience    = jsonFamVal(o, "experience", "city", "flat");
	// health/happiness: the JSON carries the ÷100 human value at .city.flat; the engine's latent-/100 consumers
	// (goodHealth/happyLevel read /100) expect the ×100 value -- re-apply ×100 (curate_specialist.py scale note §4c).
	m_iHealthPercent    = jsonX100(jsonFamDbl(o, "health", "city", "flat"));
	m_iHappinessPercent = jsonX100(jsonFamDbl(o, "happiness", "city", "flat"));

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bSlave   = jsonIdBool(*io, "slave");
		m_bVisible = jsonIdBool(*io, "visible");
		picojson::object::const_iterator gp = io->find("greatPeopleUnit");   // curate_specialist.py IDENTITY map
		if (gp != io->end() && gp->second.is<std::string>())
			m_iGreatPeopleUnitType = jsonResolveId(gp->second.get<std::string>());
	}
}
