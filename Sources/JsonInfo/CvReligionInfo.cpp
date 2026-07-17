//
//	CvReligionInfo::mapFrom -- base (text + availability: enables.buildings/units + grants.freeUnit/numFreeUnits)
//	+ the shrine block, the STATE/HOLY per-commerce split (demuxed by the enabled predicate), spread factor, art/
//	sound/adjective, and AI flavours. HUMAN-native (no ×100). See the header.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvReligionInfo.h"
#include "CvJsonParse.h"            // jsonCommerceMap (shrine) + jsonReadFlavours + the shared walkers

static const char* COMMERCE_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

CvReligionInfo::CvReligionInfo()
	: m_iSpreadFactor(0), m_iTGAIndex(-1), m_iFreeUnit(-1), m_iNumFreeUnits(0),   // TGAIndex -1 = the TGA-filler sentinel (RemoveTGAFiller erases fillers whose index stayed -1; real religions override from JSON)
	  m_iMissionType(-1), m_iChar(-1), m_iHolyCityChar(-1),
	  m_eTechPrereq(NO_TECH)
{
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) { m_aiStateReligionCommerce[i] = 0; m_aiHolyCityCommerce[i] = 0; m_aiGlobalReligionCommerce[i] = 0; }
}

int CvReligionInfo::getGlobalReligionCommerce(int i) const
{ return (i >= 0 && i < NUM_COMMERCE_TYPES) ? m_aiGlobalReligionCommerce[i] : 0; }

const char* CvReligionInfo::getButtonDisabled() const
{
	// Mirror the legacy derivation (SourceArchive/Infos/CvReligionInfo.cpp): the base button (ui.art.icon) with its
	// ".dds" extension replaced by the "_D.dds" disabled variant. Empty base button -> empty disabled path.
	static char szDisabled[512];
	szDisabled[0] = '\0';
	const char* szButton = getButton();
	const size_t iLen = szButton ? strlen(szButton) : 0;
	if (iLen > 4 && iLen + 3 <= sizeof(szDisabled))   // result is (iLen-4)+"_D.dds"+NUL = iLen+3 bytes
	{
		strncpy(szDisabled, szButton, iLen - 4);
		szDisabled[iLen - 4] = '\0';
		strcat(szDisabled, "_D.dds");
	}
	return szDisabled;
}
int CvReligionInfo::getFlavorValue(int i) const
{ std::map<int, int>::const_iterator it = m_flavours.find(i); return it != m_flavours.end() ? it->second : 0; }
int CvReligionInfo::getFreeUnit() const { return m_iFreeUnit; }          // grants.freeUnit (materialized at mapFrom)
int CvReligionInfo::getNumFreeUnits() const { return m_iNumFreeUnits; }  // grants.numFreeUnits pulse (materialized at mapFrom)

void CvReligionInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): the state-religion/holy-city demux below ACCUMULATES (+=) -- start from zero.
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) { m_aiStateReligionCommerce[c] = 0; m_aiHolyCityCommerce[c] = 0; }
	shrineCommerce.clear();
	CvInfo::mapFrom(entity);   // base: text + availability (enables.buildings/units, grants.freeUnit/numFreeUnits)
	// materialized grants reads (bucket-string reads are load-time only; pulses store ×100, /100 = the human count)
	m_iFreeUnit = m_grants.firstListId("freeUnit");
	m_iNumFreeUnits = m_grants.pulse100("numFreeUnits") / 100;
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	picojson::object::const_iterator sh = o.find("shrine");
	if (sh != o.end()) jsonCommerceMap(sh->second, shrineCommerce);
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)   // materialize the int-indexed shrine array from the by-name map
	{
		std::map<std::string, int>::const_iterator gi = shrineCommerce.find(COMMERCE_NAME[c]);
		m_aiGlobalReligionCommerce[c] = (gi != shrineCommerce.end()) ? gi->second : 0;
	}

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
			m_iTGAIndex = jsonIdInt(*art, "tgaIndex");
			if (const picojson::object* mov = jsonChildObj(*art, "movie"))
			{
				jsonIdStr(*mov, "file", m_szMovieFile);
				jsonIdStr(*mov, "sound", m_szMovieSound);
			}
		}

	if (const picojson::object* so2 = jsonChildObj(o, "sound")) jsonIdStr(*so2, "sound", m_szSound);

	// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map).
	if (const picojson::object* ai = jsonChildObj(o, "ai"))
		jsonReadFlavours(*ai, m_flavours);
}
