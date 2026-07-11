//
//	CvSpecialistInfo::mapFrom -- base core reading + availability, then the specialist's CITY-scope output +
//	identity tags. Every field maps a real curator address (see header). FK resolution via the kept type registry.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvSpecialistInfo.h"
#include "CvJsonParse.h"          // jsonResolveId + the shared walkers (jsonChildObj/jsonFamVal/...) + jsonX100

CvSpecialistInfo::CvSpecialistInfo()
	: m_iGreatPeopleRateChange(0), m_iGreatPeopleUnitType(-1), m_iExperience(0), m_iHealthPercent(0),
	  m_iHappinessPercent(0), m_iInsidiousness(0), m_iInvestigation(0), m_bSlave(false), m_bVisible(false),
	  m_iMissionType(NO_MISSION)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) m_aiYieldChange[i] = 0;
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) m_aiCommerceChange[i] = 0;
}

const UnitCombatModifier& CvSpecialistInfo::getUnitCombatExperienceType(int iIndex) const
{
	static const UnitCombatModifier s_default = { NO_UNITCOMBAT, 0 };
	return (iIndex >= 0 && iIndex < (int)m_aUnitCombatExperienceTypes.size()) ? m_aUnitCombatExperienceTypes[iIndex] : s_default;
}

// <family>.city.flat may be a scalar (base only) OR an array mixing the base scalar with tech KEEP-ON-SELF entries
// {value, enabled:{type:TECH_X, scope:team}} (curate_specialist.py _inject_cond). The base is ÷100 human -> re-apply
// ×100 for the latent-/100 consumer (happyLevel/goodHealth); the tech values are ×1 RAW (CvCity::getExtraTechSpecialist*
// reads them un-scaled) -> stored keyed by the resolved tech id. Returns the ×100 base; fills techOut with the raw adds.
static int readWellbeing(const picojson::object& o, const char* family, std::map<int, int>& techOut)
{
	const picojson::object* fo = jsonChildObj(o, family);   if (!fo) return 0;
	const picojson::object* so = jsonChildObj(*fo, "city");  if (!so) return 0;
	picojson::object::const_iterator it = so->find("flat");
	if (it == so->end()) return 0;
	const picojson::value& flat = it->second;
	if (flat.is<double>()) return jsonX100(flat.get<double>());   // scalar base only
	if (!flat.is<picojson::array>()) return 0;
	int base = 0;
	const picojson::array& a = flat.get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].is<double>()) { base += jsonX100(a[i].get<double>()); continue; }   // base scalar element (÷100 human)
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		picojson::object::const_iterator ve = e.find("value");
		if (ve == e.end() || !ve->second.is<double>()) continue;
		const picojson::object* en = jsonChildObj(e, "enabled");
		if (!en) { base += jsonX100(ve->second.get<double>()); continue; }           // unconditioned {value} -> base
		picojson::object::const_iterator ty = en->find("type");                      // tech keep-on-self (enabled.type = TECH)
		if (ty != en->end() && ty->second.is<std::string>())
		{
			const int tid = jsonResolveId(ty->second.get<std::string>());
			if (tid >= 0) techOut[tid] += (int)ve->second.get<double>();             // ×1 RAW (NOT ×100)
		}
	}
	return base;
}

void CvSpecialistInfo::mapFrom(const picojson::value& entity)
{
	CvInfo::mapFrom(entity);   // core reading + availability
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
	// health/happiness: the base at .city.flat carries the ÷100 human value (re-apply ×100 for the latent-/100
	// consumers goodHealth/happyLevel, §4c). The SAME leaf may also carry tech keep-on-self entries (slaves), split
	// out into m_techHappiness/m_techHealth (×1 RAW) by readWellbeing -- a plain jsonFamDbl would drop the array base.
	m_iHealthPercent    = readWellbeing(o, "health", m_techHealth);
	m_iHappinessPercent = readWellbeing(o, "happiness", m_techHappiness);

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_bSlave   = jsonIdBool(*io, "slave");
		m_bVisible = jsonIdBool(*io, "visible");
		picojson::object::const_iterator gp = io->find("greatPeopleUnit");   // curate_specialist.py IDENTITY map
		if (gp != io->end() && gp->second.is<std::string>())
			m_iGreatPeopleUnitType = jsonResolveId(gp->second.get<std::string>());
	}

	// ui.art.texture -- the specialist's city-screen glyph (curate_specialist.py ART: Texture -> ui.art.texture)
	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
			jsonIdStr(*art, "texture", m_szTexture);

	// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (curate_specialist.py:196 ai["flavours"] = v)
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		jsonReadFlavours(*ai, m_flavours);

	// experience.city.unitCombats.{UNITCOMBAT}.flat -- target-keyed per-unit-combat XP modifiers (curate_specialist.py _unit_combat_xp)
	if (const picojson::object* eo = jsonChildObj(o, "experience"))
		if (const picojson::object* co = jsonChildObj(*eo, "city"))
			if (const picojson::object* uco = jsonChildObj(*co, "unitCombats"))
				for (picojson::object::const_iterator uc = uco->begin(); uc != uco->end(); ++uc)
					if (uc->second.is<picojson::object>())
					{
						const int id = jsonResolveId(uc->first);
						if (id >= 0)
						{
							UnitCombatModifier mod;
							mod.eUnitCombat = (UnitCombatTypes)id;
							mod.iModifier = jsonIdInt(uc->second.get<picojson::object>(), "flat");
							m_aUnitCombatExperienceTypes.push_back(mod);
						}
					}
}
