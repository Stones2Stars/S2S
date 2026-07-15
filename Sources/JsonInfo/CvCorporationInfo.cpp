//
//	CvCorporationInfo::mapFrom -- base (text + availability: enables.buildings, tech/bonus enables.corporations,
//	provides.bonuses) + the corp's per-city families. Yield/commerce split by `per`-presence into change (×1) vs
//	produced (×100 re-applied, the ÷100-descaled fractional); the shared per.anyOf prereq-bonus set collected once;
//	maintenance summed AS-IS (curator left it raw ×100). See the header for the corp scale caveats.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvCorporationInfo.h"
#include "CvJsonParse.h"            // jsonResolveId + jsonX100 + the shared walkers (jsonChildObj/jsonIdInt/...)
#include <set>

static const char* YIELD_NAME[NUM_YIELD_TYPES]  = { "food", "production", "commerce" };
static const char* COMM_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

CvCorporationInfo::CvCorporationInfo()
	: m_iMaintenance(0), m_iHealth(0), m_iHappiness(0), m_iFreeXP(0), m_iMilitaryProductionModifier(0),
	  m_iSpreadCost(0), m_iSpread(0), m_iCompetingSpreadCostPercent(0), m_iTGAIndex(-1), m_iHeadquarterChar(-1), m_iMissionType(-1), m_iChar(0),   // TGAIndex -1 = TGA-filler sentinel (RemoveTGAFiller)
	  m_eTechPrereq(NO_TECH), m_eObsoleteTech(NO_TECH)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) { m_aiYieldChange[i] = 0; m_aiYieldProduced[i] = 0; }
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) { m_aiCommerceChange[i] = 0; m_aiCommerceProduced[i] = 0; m_aiHeadquarterCommerce[i] = 0; }
}

// GameFont glyph: DERIVED from the TGA index, offset PAST the religion block (corps follow religions in GameFont) --
// reproduce the archived CvCorporationInfo::setChar exactly (SourceArchive/Infos/CvCorporationInfo.cpp:83), else the
// corp icon lands on the wrong/empty slot. The symbol pass's sequential id arg is ignored (as legacy did).
void CvCorporationInfo::setChar(int /*i*/)
{
	m_iChar = 8550 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}

void CvCorporationInfo::setHeadquarterChar(int /*i*/)
{
	m_iHeadquarterChar = 8551 + (GC.getGAMEFONT_TGA_RELIGIONS() + m_iTGAIndex) * 2;
}

int CvCorporationInfo::getBonusProduced() const
{ return !m_provides.bonuses.empty() ? m_provides.bonuses[0] : -1; }

// navigate o -> family -> scope [-> member] -> "flat" ; returns the flat value (array or number), or NULL.
static const picojson::value* flat_leaf(const picojson::object& o, const char* family, const char* scope, const char* member)
{
	const picojson::object* fo = jsonChildObj(o, family);  if (!fo) return NULL;
	const picojson::object* so = jsonChildObj(*fo, scope); if (!so) return NULL;
	const picojson::object* leaf = so;
	if (member) { leaf = jsonChildObj(*so, member); if (!leaf) return NULL; }
	picojson::object::const_iterator it = leaf->find("flat");
	return (it != leaf->end()) ? &it->second : NULL;
}

// sum a scalar OR an array of bare-numbers / {value:N} objects (condition-blind: a static getter cannot evaluate an
// `enabled` clause, so every entry's magnitude is taken -- the archived static getter had no conditions to honour).
static int sumValueLeaf(const picojson::value& v)
{
	if (v.is<double>()) return (int)v.get<double>();
	if (!v.is<picojson::array>()) return 0;
	const picojson::array& a = v.get<picojson::array>();
	int iSum = 0;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].is<double>()) { iSum += (int)a[i].get<double>(); continue; }
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		picojson::object::const_iterator ve = e.find("value");
		if (ve != e.end() && ve->second.is<double>()) iSum += (int)ve->second.get<double>();
	}
	return iSum;
}

// demux a conditioned flat leaf: entries WITH `per` -> produced (×100), collect per.anyOf into prereq; else change (×1).
static void demux(const picojson::value* flat, int& change, int& produced, std::vector<int>& prereq, std::set<int>& seen)
{
	if (!flat) return;
	if (flat->is<double>()) { change += (int)flat->get<double>(); return; }
	if (!flat->is<picojson::array>()) return;
	const picojson::array& a = flat->get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].is<double>()) { change += (int)a[i].get<double>(); continue; }
		if (!a[i].is<picojson::object>()) continue;
		const picojson::object& e = a[i].get<picojson::object>();
		picojson::object::const_iterator ve = e.find("value");
		if (ve == e.end() || !ve->second.is<double>()) continue;
		const double v = ve->second.get<double>();
		const picojson::object* per = jsonChildObj(e, "per");
		if (per)
		{
			produced += jsonX100(v);   // per-scaled -> the ÷100-descaled fractional base, ×100 re-applied
			picojson::object::const_iterator any = per->find("anyOf");
			if (any != per->end() && any->second.is<picojson::array>())
			{
				const picojson::array& ba = any->second.get<picojson::array>();
				for (size_t b = 0; b < ba.size(); ++b)
					if (ba[b].is<std::string>()) { const int id = jsonResolveId(ba[b].get<std::string>()); if (id >= 0 && seen.insert(id).second) prereq.push_back(id); }
			}
		}
		else change += (int)v;
	}
}

// sum a flat leaf AS-IS (no ×100) -- for maintenance (curator-not-descaled) + the ungated scalar families.
static int sumFlatAsIs(const picojson::value* flat)
{
	if (!flat) return 0;
	if (flat->is<double>()) return (int)flat->get<double>();
	if (!flat->is<picojson::array>()) return 0;
	int sum = 0; const picojson::array& a = flat->get<picojson::array>();
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].is<double>()) sum += (int)a[i].get<double>();
		else if (a[i].is<picojson::object>()) { picojson::object::const_iterator v = a[i].get<picojson::object>().find("value"); if (v != a[i].get<picojson::object>().end() && v->second.is<double>()) sum += (int)v->second.get<double>(); }
	}
	return sum;
}

void CvCorporationInfo::mapFrom(const picojson::value& entity)
{
	// remap-idempotency (CvInfo.h): demux ACCUMULATES (+=) into the yield/commerce arrays and appends the prereqs.
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)    { m_aiYieldChange[y] = 0;    m_aiYieldProduced[y] = 0; }
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c) { m_aiCommerceChange[c] = 0; m_aiCommerceProduced[c] = 0; }
	m_aePrereqBonuses.clear(); m_aeExcludes.clear();

	CvInfo::mapFrom(entity);   // base: text + availability (enables.buildings, provides.bonuses, tech/bonus enables.corporations)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	std::set<int> seen;
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		demux(flat_leaf(o, YIELD_NAME[y], "city", NULL), m_aiYieldChange[y], m_aiYieldProduced[y], m_aePrereqBonuses, seen);
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		demux(flat_leaf(o, COMM_NAME[c], "city", NULL), m_aiCommerceChange[c], m_aiCommerceProduced[c], m_aePrereqBonuses, seen);

	// maintenance.city.corporation.flat -- summed AS-IS (raw ×100, curator not descaled; see header)
	m_iMaintenance = sumFlatAsIs(flat_leaf(o, "maintenance", "city", "corporation"));
	// ungated scalar families (×1). STUB if any corp ever per-scales these, demux instead (none in current data).
	m_iHealth  = sumFlatAsIs(flat_leaf(o, "health", "city", NULL));
	m_iHappiness = sumFlatAsIs(flat_leaf(o, "happiness", "city", NULL));
	m_iFreeXP  = sumFlatAsIs(flat_leaf(o, "experience", "city", NULL));

	// buildRate.city.military.percent -- authored as an ARRAY ([{value:15,enabled:…}]) on the shipped corp, so
	// jsonIdInt (scalar-only) silently read 0. Sum the array's value(s) condition-blind (static getter).
	if (const picojson::object* br = jsonChildObj(o, "buildRate"))
		if (const picojson::object* bc = jsonChildObj(*br, "city"))
			if (const picojson::object* mil = jsonChildObj(*bc, "military"))
			{
				picojson::object::const_iterator pit = mil->find("percent");
				if (pit != mil->end()) m_iMilitaryProductionModifier = sumValueLeaf(pit->second);
			}

	// {c}.empire.headquarters.perCorporationLevel
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		if (const picojson::object* fo = jsonChildObj(o, COMM_NAME[c]))
			if (const picojson::object* emp = jsonChildObj(*fo, "empire"))
				if (const picojson::object* hq = jsonChildObj(*emp, "headquarters"))
					m_aiHeadquarterCommerce[c] = jsonIdInt(*hq, "perCorporationLevel");

	if (const picojson::object* co = jsonChildObj(o, "cost")) m_iSpreadCost = jsonIdInt(*co, "spread");
	if (const picojson::object* io = jsonChildObj(o, "identity"))
	{
		m_iSpread                     = jsonIdInt(*io, "spreadFactor");
		m_iCompetingSpreadCostPercent = jsonIdInt(*io, "competingSpreadCostPercent");
	}
	if (const picojson::object* so = jsonChildObj(o, "sound")) jsonIdStr(*so, "sound", m_szSound);

	// ui.art: tgaIndex + movie.file/movie.sound (mirrors CvReligionInfo; grants.freeUnit rides the composed m_grants)
	if (const picojson::object* ui = jsonChildObj(o, "ui"))
		if (const picojson::object* art = jsonChildObj(*ui, "art"))
		{
			m_iTGAIndex = jsonIdInt(*art, "tgaIndex");
			if (const picojson::object* mov = jsonChildObj(*art, "movie"))
			{
				jsonIdStr(*mov, "file", m_szMovieFile);
				jsonIdStr(*mov, "sound", m_szMovieSound);
			}
		}

	// top-level `excludes` -- CompetingCorporations (curate_corporation.py EXCLUDES; json sec9 same-tier corp<->corp
	// exclusion). Empty in all shipped base XML, so this stays empty until data lands. `excludes` is CJK_INTRINSIC
	// (base skips it), so the subclass owns the parse; resolve each CORPORATION_ FK inline (as the Promotion FK lists).
	picojson::object::const_iterator ex = o.find("excludes");
	if (ex != o.end() && ex->second.is<picojson::array>())
	{
		const picojson::array& a = ex->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
			if (a[i].is<std::string>()) { const int id = jsonResolveId(a[i].get<std::string>()); if (id >= 0) m_aeExcludes.push_back(id); }
	}
}
