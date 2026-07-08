#pragma once
#ifndef CV_JSON_MODIFIERS_H
#define CV_JSON_MODIFIERS_H

//
//	CvJsonModifiers -- an entity's MODIFIER FAMILIES per json.md §6, as JsonInfo-owned DATA. The full deposit address
//	`<family>.<scope>[.<target>|.<targetType>.{KEY}][.<member>].<unit>` is walked from the JSON; each unit-leaf's
//	entries become CvJsonModEntry (§3.9) collected under the address (MINUS the unit -- each entry carries its own
//	unit). This is the cascade's READ SURFACE for a complex modifier-bearing type (building/civic/trait/unit) whose
//	family tree is too keyed/conditioned to reduce to named scalar getters (json.md §6 IS the model; the JSON already
//	models it -- owner 2026-07-07). Pure data, ZERO cascade runtime ([DEC-json-not-cascade]); the cascade reads it to
//	build its DepositIndex. Faithfully mirrors the JSON structure -- there are no per-value getters.
//

#include "CvJsonModEntry.h"
#include "picojson.h"   // picojson::value + object -- parseEntity takes a picojson::object (object is a TYPEDEF, not
                        // a class: forward-declaring it as `class object;` collided with the real typedef and ICE'd VC7.1)
#include <map>
#include <string>

class CvJsonModifiers
{
public:
	CvJsonModifiers() {}
	~CvJsonModifiers();

	// Walk every top-level MODIFIER-FAMILY key of an entity (a non-reserved, object-valued key -- json §1) into the
	// address-keyed family map. Reserved sections (identity/cost/ui/enables/requires/grants/attributes/… -- the base
	// + the subclass own those) are skipped by an internal recognizer kept in sync with json.md §11.
	void parseEntity(const picojson::object& entity);

	const CvJsonModFamily* find(const std::string& address) const;   // e.g. "food.city" / "happiness.city.buildings.BUILDING_FORGE"
	const std::map<std::string, CvJsonModFamily*>& all() const { return m_families; }
	bool empty() const { return m_families.empty(); }

private:
	void walk(const std::string& addr, const picojson::value& node);   // recurse; a unit key -> a leaf, else deeper

	std::map<std::string, CvJsonModFamily*> m_families;   // dotted address (minus unit) -> owned family entries
	CvJsonModifiers(const CvJsonModifiers&);              // noncopyable -- owns the families
	CvJsonModifiers& operator=(const CvJsonModifiers&);
};

#endif // CV_JSON_MODIFIERS_H
