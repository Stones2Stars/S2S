//
//	CvJsonHeritageInfo::mapFrom -- base core reading + availability (tech enables.heritages + this heritage's
//	enables.heritages succession ride the base), then the era-threshold-gated empire commerce + the language gate.
//	Commerce is HUMAN (÷100-descaled by the curator). PropertyManipulators are deferred to the property subsystem.
//	See header.
//

#include "CvGameCoreDLL.h"        // PCH umbrella -- picojson
#include "CvJsonHeritageInfo.h"

int CvJsonHeritageInfo::getEraCommerceChange(int iCommerce, int iEra) const
{
	if (iCommerce < 0 || iCommerce >= NUM_COMMERCE_TYPES) return 0;
	int iTotal = 0;
	const std::vector<EraBand>& bands = m_aEraCommerce[iCommerce];
	for (size_t i = 0; i < bands.size(); ++i)
		if (bands[i].eraMin <= iEra) iTotal += bands[i].value;
	return iTotal;
}

static const picojson::object* child_obj(const picojson::object& o, const char* key)
{
	picojson::object::const_iterator it = o.find(key);
	return (it != o.end() && it->second.is<picojson::object>()) ? &it->second.get<picojson::object>() : NULL;
}
static bool id_bool(const picojson::object& io, const char* key)
{ picojson::object::const_iterator it = io.find(key); return (it != io.end() && it->second.is<bool>()) ? it->second.get<bool>() : false; }

void CvJsonHeritageInfo::mapFrom(const picojson::value& entity)
{
	CvJsonInfo::mapFrom(entity);   // core + availability (tech enables.heritages, this heritage's enables.heritages)
	if (!entity.is<picojson::object>()) return;
	const picojson::object& o = entity.get<picojson::object>();

	// era-gated empire commerce -- {gold/research/culture/espionage}.empire.flat, each a bare number (one always-on
	// band, eraMin 0) OR a list of { value, enabled:{type:"ERA", min:N} } era-threshold bands.
	static const char* fam[NUM_COMMERCE_TYPES] = { "gold", "research", "culture", "espionage" };   // COMMERCE_* order
	for (int c = 0; c < NUM_COMMERCE_TYPES; ++c)
	{
		const picojson::object* fo = child_obj(o, fam[c]);          if (!fo) continue;
		const picojson::object* so = child_obj(*fo, "empire");      if (!so) continue;
		picojson::object::const_iterator fl = so->find("flat");     if (fl == so->end()) continue;
		if (fl->second.is<double>())
		{
			EraBand b; b.eraMin = 0; b.value = (int)fl->second.get<double>();
			m_aEraCommerce[c].push_back(b);
		}
		else if (fl->second.is<picojson::array>())
		{
			const picojson::array& a = fl->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				EraBand b; b.eraMin = 0; b.value = 0;
				if (a[i].is<double>()) { b.value = (int)a[i].get<double>(); m_aEraCommerce[c].push_back(b); continue; }
				if (!a[i].is<picojson::object>()) continue;
				const picojson::object& e = a[i].get<picojson::object>();
				picojson::object::const_iterator ve = e.find("value");
				if (ve == e.end() || !ve->second.is<double>()) continue;
				b.value = (int)ve->second.get<double>();
				const picojson::object* en = child_obj(e, "enabled");
				if (en) { picojson::object::const_iterator mn = en->find("min"); if (mn != en->end() && mn->second.is<double>()) b.eraMin = (int)mn->second.get<double>(); }
				m_aEraCommerce[c].push_back(b);
			}
		}
	}

	if (const picojson::object* io = child_obj(o, "identity"))
		m_bNeedsLanguage = id_bool(*io, "needsLanguage");
}
