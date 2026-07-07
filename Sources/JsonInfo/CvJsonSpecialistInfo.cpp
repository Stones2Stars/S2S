//
//	CvJsonSpecialistInfo::mapFrom -- base core reading + availability, then the specialist's CITY-scope output +
//	identity tags. ⏳-flagged shapes to confirm. FK resolution via the kept type registry. See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson, GC
#include "CvJsonSpecialistInfo.h"
#include "Defines/CvGlobals.h"    // GC.getInfoTypeForString

CvJsonSpecialistInfo::CvJsonSpecialistInfo()
	: m_iGreatPeopleRateChange(0), m_iGreatPeopleUnitType(-1), m_iExperience(0), m_iHealthPercent(0),
	  m_iHappinessPercent(0), m_iInsidiousness(0), m_iInvestigation(0), m_bSlave(false), m_bVisible(false)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiCommerceChange[i] = 0;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static double fam_dbl(const picojson::object& o, const char* family, const char* scope, const char* unit)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return 0.0;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return 0.0;
	picojson::object::const_iterator u = so->find(unit);
	return (u != so->end() && u->second.is<double>()) ? u->second.get<double>() : 0.0;
}
static int fam_val(const picojson::object& o, const char* family, const char* scope, const char* unit)
{ return (int)fam_dbl(o, family, scope, unit); }
// re-apply the latent ×100 (round half away from zero): the JSON carries the ÷100 human value, the engine wants ×100.
static int x100(double v) { return (int)(v >= 0 ? v * 100.0 + 0.5 : v * 100.0 - 0.5); }
static int  id_int (const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }

void CvJsonSpecialistInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core reading + availability
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// city-scope yields + commerce (the specialist's own output)
	m_aiYieldChange[YIELD_FOOD]       = fam_val(o, "food", "city", "flat");
	m_aiYieldChange[YIELD_PRODUCTION] = fam_val(o, "production", "city", "flat");
	m_aiYieldChange[YIELD_COMMERCE]   = fam_val(o, "commerce", "city", "flat");
	m_aiCommerceChange[COMMERCE_GOLD]      = fam_val(o, "gold", "city", "flat");
	m_aiCommerceChange[COMMERCE_RESEARCH]  = fam_val(o, "research", "city", "flat");
	m_aiCommerceChange[COMMERCE_CULTURE]   = fam_val(o, "culture", "city", "flat");
	m_aiCommerceChange[COMMERCE_ESPIONAGE] = fam_val(o, "espionage", "city", "flat");

	m_iGreatPeopleRateChange = fam_val(o, "greatPeopleRate", "city", "flat");
	// investigation/insidiousness/experience are FAMILIES at city.flat (×1) -- NOT identity (curate_specialist.py:26-28).
	m_iInvestigation = fam_val(o, "investigation", "city", "flat");
	m_iInsidiousness = fam_val(o, "insidiousness", "city", "flat");
	m_iExperience    = fam_val(o, "experience", "city", "flat");
	// health/happiness: the JSON carries the ÷100 human value at .city.flat; the engine's latent-/100 consumers
	// (goodHealth/happyLevel read /100) expect the ×100 value -- re-apply ×100 (curate_specialist.py scale note §4c).
	m_iHealthPercent    = x100(fam_dbl(o, "health", "city", "flat"));
	m_iHappinessPercent = x100(fam_dbl(o, "happiness", "city", "flat"));

	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_bSlave   = id_bool(*io, "slave");
		m_bVisible = id_bool(*io, "visible");
		picojson::object::const_iterator gp = io->find("greatPeopleUnit");   // curate_specialist.py IDENTITY map
		if (gp != io->end() && gp->second.is<std::string>())
			m_iGreatPeopleUnitType = GC.getInfoTypeForString(gp->second.get<std::string>().c_str(), true);
	}
}
