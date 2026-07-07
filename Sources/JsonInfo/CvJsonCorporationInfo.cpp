//
//	CvJsonCorporationInfo::mapFrom -- base (text + availability: enables.buildings, tech/bonus enables.corporations,
//	provides.bonuses) + the corp's per-city families. Yield/commerce split by `per`-presence into change (×1) vs
//	produced (×100 re-applied, the ÷100-descaled fractional); the shared per.anyOf prereq-bonus set collected once;
//	maintenance summed AS-IS (curator left it raw ×100). See the header for the corp scale caveats.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonCorporationInfo.h"
#include "Defines/CvGlobals.h"      // GC.getInfoTypeForString
#include <set>

static const char* YIELD_NAME[NUM_YIELD_TYPES]  = { "food", "production", "commerce" };
static const char* COMM_NAME[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };

CvJsonCorporationInfo::CvJsonCorporationInfo()
	: m_iMaintenance(0), m_iHealth(0), m_iHappiness(0), m_iFreeXP(0), m_iMilitaryProductionModifier(0),
	  m_iSpreadCost(0), m_iSpread(0), m_iCompetingSpreadCostPercent(0), m_iHeadquarterChar(-1), m_iMissionType(-1)
{
	for (int i = 0; i < NUM_YIELD_TYPES; ++i) { m_aiYieldChange[i] = 0; m_aiYieldProduced[i] = 0; }
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i) { m_aiCommerceChange[i] = 0; m_aiCommerceProduced[i] = 0; m_aiHeadquarterCommerce[i] = 0; }
}

int CvJsonCorporationInfo::getBonusProduced() const
{ return !m_provides.bonuses.empty() ? m_provides.bonuses[0] : -1; }

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{ picojson::object::const_iterator it = o.find(key); return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL; }
static int id_int(const picojson::object& io, const char* k)
{ picojson::object::const_iterator it = io.find(k); return (it != io.end() && it->second.is<double>()) ? (int)it->second.get<double>() : 0; }
static void id_str(const picojson::object& io, const char* k, std::string& out)
{ picojson::object::const_iterator it = io.find(k); if (it != io.end() && it->second.is<std::string>()) out = it->second.get<std::string>(); }
static int x100(double v) { return (int)(v >= 0 ? v * 100.0 + 0.5 : v * 100.0 - 0.5); }

// navigate o -> family -> scope [-> member] -> "flat" ; returns the flat value (array or number), or NULL.
static const picojson::value* flat_leaf(const picojson::object& o, const char* family, const char* scope, const char* member)
{
	const picojson::object* fo = child_obj(o, family);  if (!fo) return NULL;
	const picojson::object* so = child_obj(*fo, scope); if (!so) return NULL;
	const picojson::object* leaf = so;
	if (member) { leaf = child_obj(*so, member); if (!leaf) return NULL; }
	picojson::object::const_iterator it = leaf->find("flat");
	return (it != leaf->end()) ? &it->second : NULL;
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
		const picojson::object* per = child_obj(e, "per");
		if (per)
		{
			produced += x100(v);   // per-scaled -> the ÷100-descaled fractional base, ×100 re-applied
			picojson::object::const_iterator any = per->find("anyOf");
			if (any != per->end() && any->second.is<picojson::array>())
			{
				const picojson::array& ba = any->second.get<picojson::array>();
				for (size_t b = 0; b < ba.size(); ++b)
					if (ba[b].is<std::string>()) { const int id = GC.getInfoTypeForString(ba[b].get<std::string>().c_str(), true); if (id >= 0 && seen.insert(id).second) prereq.push_back(id); }
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

void CvJsonCorporationInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // base: text + availability (enables.buildings, provides.bonuses, tech/bonus enables.corporations)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	std::set<int> seen;
	for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		demux(flat_leaf(o, YIELD_NAME[y], "city", NULL), m_aiYieldChange[y], m_aiYieldProduced[y], m_aePrereqBonuses, seen);
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		demux(flat_leaf(o, COMM_NAME[c], "city", NULL), m_aiCommerceChange[c], m_aiCommerceProduced[c], m_aePrereqBonuses, seen);

	// maintenance.city.corporation.flat -- summed AS-IS (raw ×100, curator not descaled; see header)
	m_iMaintenance = sumFlatAsIs(flat_leaf(o, "maintenance", "city", "corporation"));
	// ungated scalar families (×1). ⏳ if any corp ever per-scales these, demux instead (none in current data).
	m_iHealth  = sumFlatAsIs(flat_leaf(o, "health", "city", NULL));
	m_iHappiness = sumFlatAsIs(flat_leaf(o, "happiness", "city", NULL));
	m_iFreeXP  = sumFlatAsIs(flat_leaf(o, "experience", "city", NULL));

	// buildRate.city.military.percent
	if (const picojson::object* br = child_obj(o, "buildRate"))
		if (const picojson::object* bc = child_obj(*br, "city"))
			if (const picojson::object* mil = child_obj(*bc, "military"))
				m_iMilitaryProductionModifier = id_int(*mil, "percent");

	// {c}.empire.headquarters.perCorporationLevel
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
		if (const picojson::object* fo = child_obj(o, COMM_NAME[c]))
			if (const picojson::object* emp = child_obj(*fo, "empire"))
				if (const picojson::object* hq = child_obj(*emp, "headquarters"))
					m_aiHeadquarterCommerce[c] = id_int(*hq, "perCorporationLevel");

	if (const picojson::object* co = child_obj(o, "cost")) m_iSpreadCost = id_int(*co, "spread");
	if (const picojson::object* io = child_obj(o, "identity"))
	{
		m_iSpread                     = id_int(*io, "spreadFactor");
		m_iCompetingSpreadCostPercent = id_int(*io, "competingSpreadCostPercent");
	}
	if (const picojson::object* so = child_obj(o, "sound")) id_str(*so, "sound", m_szSound);
}
