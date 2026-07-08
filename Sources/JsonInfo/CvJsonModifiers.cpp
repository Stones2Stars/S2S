//
//	CvJsonModifiers -- see the header. Walks every §6 modifier-family key of an entity (a key jsonClassifyKey
//	classes CJK_FAMILY) down the deposit address `<family>.<scope>[.<target>|.{KEY}][.<member>].<unit>`: a unit
//	keyword ends the address (a leaf -- its entries parse via CvJsonModFamily::parseLeaf, ×100 + conditions); any
//	other key recurses one segment deeper. The address is stored MINUS the unit (each entry carries its own unit);
//	the scope is read from the address's second segment (json §3.2, default city).
//
//	Implemented directly against json.md §6 + the header's contract.
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvJsonModifiers.h"
#include "CvJsonParse.h"     // jsonClassifyKey + jsonParseScope -- the ONE reserved-key/scope-token vocabulary (never re-hand-rolled here)

CvJsonModifiers::~CvJsonModifiers()
{
	for (std::map<std::string, CvJsonModFamily*>::iterator it = m_families.begin(); it != m_families.end(); ++it)
		delete it->second;
}

const CvJsonModFamily* CvJsonModifiers::find(const std::string& address) const
{
	std::map<std::string, CvJsonModFamily*>::const_iterator it = m_families.find(address);
	return (it != m_families.end()) ? it->second : NULL;
}

// The scope segment of a deposit address (json §3.2 -- the segment after the family; default city). The token
// vocabulary is the shared jsonParseScope; only the address slicing lives here.
static CvCascScope mod_scopeOf(const std::string& addr)
{
	const size_t dot = addr.find('.');
	if (dot == std::string::npos) return CASC_SCOPE_CITY;
	const size_t end = addr.find('.', dot + 1);
	const std::string s = addr.substr(dot + 1, (end == std::string::npos ? addr.size() : end) - dot - 1);
	return jsonParseScope(s, CASC_SCOPE_CITY);
}

void CvJsonModifiers::walk(const std::string& addr, const picojson::value& node)
{
	if (!node.is<picojson::object>()) return;   // a family/segment node is always object-valued (json §1/§6)
	const picojson::object& o = node.get<picojson::object>();
	// the NODE-form `unit:` predicate qualifier (json §3.7 -- a sibling of the magnitude leaves: cargo.space.
	// {unit: IS_AIR, flat: N}); consumed here, applied to this node's leaves, never an address segment.
	picojson::object::const_iterator uq = o.find("unit");
	const picojson::value* nodeQual = (uq != o.end() && uq->second.is<std::string>()) ? &uq->second : NULL;
	for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
	{
		if (nodeQual != NULL && it == uq) continue;   // the qualifier key itself
		const CvCascUnit unit = cascadeUnitFromString(it->first);
		if (unit != CASC_UNIT_UNKNOWN)   // a unit keyword ends the address -- this is a magnitude LEAF
		{
			CvJsonModFamily*& fam = m_families[addr];
			if (fam == NULL) fam = new CvJsonModFamily();
			fam->parseLeaf(it->second, unit, mod_scopeOf(addr), nodeQual);
		}
		else if (it->second.is<double>() || it->second.is<picojson::array>())
		{
			// the modifier.md §6 COUNT-BY-TYPE leaf (freeSpecialists/allowedSpecialists: the key IS the type/`any`
			// and the value the count) -- the one sanctioned non-unit leaf; synthesized unit COUNT, key in the address.
			CvJsonModFamily*& fam = m_families[addr + "." + it->first];
			if (fam == NULL) fam = new CvJsonModFamily();
			fam->parseLeaf(it->second, CASC_UNIT_COUNT, mod_scopeOf(addr), nodeQual);
		}
		else
			walk(addr + "." + it->first, it->second);   // a scope/target/member segment -- one level deeper
	}
}

void CvJsonModifiers::parseEntity(const picojson::object& entity)
{
	for (picojson::object::const_iterator it = entity.begin(); it != entity.end(); ++it)
		if (jsonClassifyKey(it->first, it->second.is<picojson::object>()) == CJK_FAMILY)
			walk(it->first, it->second);
}
