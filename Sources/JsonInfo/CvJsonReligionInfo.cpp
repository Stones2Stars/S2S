//
//	CvJsonReligionInfo::mapFrom -- base (text + availability: enables.buildings/units + grants.freeUnit/numFreeUnits)
//	+ the shrine block, the STATE/HOLY per-commerce split (demuxed by the enabled predicate), spread factor, art/
//	sound/adjective, and AI flavours. HUMAN-native (no ×100). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonReligionInfo.h"
#include "CvJsonParse.h"            // jsonCommerceMap (shrine) + jsonReadFlavours + the shared walkers

static const char* COMMERCE_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

CvJsonReligionInfo::CvJsonReligionInfo()
	: m_iSpreadFactor(0), m_iMissionType(-1), m_iChar(-1), m_iHolyCityChar(-1)
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) { m_aiStateReligionCommerce[i] = 0; m_aiHolyCityCommerce[i] = 0; }
}

int CvJsonReligionInfo::getGlobalReligionCommerce(int i) const
{
	if (i < 0 || i >= NUM_COMMERCE_TYPES) return 0;
	std::map<std::string, int>::const_iterator it = shrineCommerce.find(COMMERCE_NAME[i]);
	return it != shrineCommerce.end() ? it->second : 0;
}
int CvJsonReligionInfo::getFlavorValue(int i) const
{ std::map<int, int>::const_iterator it = m_flavours.find(i); return it != m_flavours.end() ? it->second : 0; }
int CvJsonReligionInfo::getFreeUnit() const
{ return m_grants.firstListId("freeUnit"); }
int CvJsonReligionInfo::getNumFreeUnits() const
{ return m_grants.pulse100("numFreeUnits") / 100; }   // pulses store ×100 (restored); /100 = the human count this getter always served

void CvJsonReligionInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // base: text + availability (enables.buildings/units, grants.freeUnit/numFreeUnits)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	picojson::object::const_iterator sh = o.find("shrine");
	if (sh != o.end()) jsonCommerceMap(sh->second, shrineCommerce);

	// STATE-religion / HOLY-city commerce: {commerce}.city.flat is a LIST of { value, enabled:{PRED:self} } -- demux by PRED.
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const picojson::object* fo = jsonChildObj(o, COMMERCE_NAME[c]);   if (!fo) continue;
		const picojson::object* so = jsonChildObj(*fo, "city");           if (!so) continue;
		picojson::object::const_iterator fl = so->find("flat");           if (fl == so->end() || !fl->second.is<picojson::array>()) continue;
		const picojson::array& a = fl->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<picojson::object>()) continue;
			const picojson::object& e = a[i].get<picojson::object>();
			picojson::object::const_iterator ve = e.find("value");
			const picojson::object* en = jsonChildObj(e, "enabled");
			if (ve == e.end() || !ve->second.is<double>() || !en) continue;
			const int val = (int)ve->second.get<double>();
			if (en->find("STATE_RELIGION") != en->end())      m_aiStateReligionCommerce[c] += val;
			else if (en->find("IS_HOLY_CITY") != en->end())   m_aiHolyCityCommerce[c] += val;
		}
	}

	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iSpreadFactor = jsonIdInt(*io, "spreadFactor");
		std::string adj; jsonIdStr(*io, "adjective", adj); if (!adj.empty()) m_szAdjectiveKey = CvWString(adj.c_str());
	}

	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
		{
			jsonIdStr(*art, "techButton", m_szTechButton);
			jsonIdStr(*art, "genericTechButton", m_szGenericTechButton);
		}

	if (const picojson::object* so2 = jsonChildObj(o, "sound")) jsonIdStr(*so2, "sound", m_szSound);

	// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map).
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		jsonReadFlavours(*ai, m_flavours);
}
