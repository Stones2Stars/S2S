#pragma once

#ifndef CyUnitInfo_h__
#define CyUnitInfo_h__

#include <boost/python/list.hpp>

//
//	CyUnitInfo -- the UNIT accessor, one of the PER-INFO accessors the Python read boundary is built from
//	([patterns.md] THE PYTHON READ BOUNDARY).
//
//	⛔ HOMING IS THE REQUIREMENT, AND "NAMED" ALONE DOES NOT SATISFY IT (owner): an endpoint belongs on the
//	accessor for the type it addresses, because a flat class accumulating UNIT, BUILDING and IMPROVEMENT reads
//	side by side is the spaghetti wearing named endpoints. So these live here rather than on [CyInfo], whose job
//	is what is genuinely UNIFORM across every registry -- identity text, the classification tests, the edge
//	families, the group reads.
//
//	⚑ WHAT THESE PUBLISH IS ALREADY CARRIED. Every read below is a bare fetch of the unit's own authored member:
//	the combat classes are the root `combatClass`/`combatClasses` FKs, the builds are its authored repertoire,
//	the granted promotions are its `grants.promotions`. The boundary was missing a read, never the data -- so
//	nothing here derives, and nothing here is a legacy per-FIELD getter revived ([DEC-new-getter-surface] bans
//	mirroring the old `CvXInfo` contract; a named accessor per info TYPE is a different axis).
//
//	⛔ THERE IS DELIBERATELY NO "PROMOTIONS THIS UNIT QUALIFIES FOR" READ. The legacy page swept the whole
//	promotion registry asking each id whether it applied -- the own-data inversion [DEC-one-reverse-view] bans --
//	and the rebuilt info carries no qualified-promotion member to answer it from. A promotion's own
//	`PYLIST_QUALIFIED_UNITCOMBATS` is the authored direction; the inverse is not served, so it is left UNSERVED
//	rather than approximated ([DEC-no-legacy-masking]: a gap shows, it is never papered over).
//
//	The id is the UNIT, passed per call: an accessor holds no bound entity, so it is a plain read surface rather
//	than a handle with a lifetime.
//
class CyUnitInfo
{
public:
	CyUnitInfo() {}

	// The unit's COMBAT CLASSES -- the primary `combatClass` followed by the `combatClasses` subs, in one list.
	// ⚑ The primary is FIRST and is never omitted: reading only the subs was the historic sniper-immunity miss
	// ([engine.md] UnitCombat), so the two are handed out together or the caller silently loses one.
	python::list getCombatClasses(int iUnit) const;

	// The BUILD_* repertoire this unit can perform (json par.9 `builds` -- which builds THIS unit can do, never
	// "which are unlocked", which is the tech's `enables.builds`).
	python::list getBuilds(int iUnit) const;

	// The promotions this unit is CREATED with -- its `grants.promotions`.
	python::list getGrantedPromotions(int iUnit) const;

	// The SKILL tests this page names. A skill is a pure boolean enabler ([skills.md]); the parameterized
	// `CyInfo::hasSkill` is not the consumer surface, so each is a named endpoint added for the call site that
	// wants it ([patterns.md]: "you can easily make a Cy wrapper for a specific skill ... minimal amount of
	// endpoints is not the target here, properly organized is").
	bool isFound(int iUnit) const;
	bool isIgnoreBuildingDefense(int iUnit) const;

	// The unit's own intrinsic self-description.
	int getConscription(int iUnit) const;   // identity.conscription -- 0 when it cannot be drafted
	int getCaptureUnit(int iUnit) const;    // identity.captures -- the UNIT_* capturing this one yields, else -1

	// WHERE the unit operates -- the `DomainTypes` value off identity.domain.
	// ⛔ It is deliberately NOT answered from the tag set ([tags.md]): a tag says what a unit IS, a domain says
	// where it OPERATES, so reading it off the tags means filtering every tag for what one field already holds.
	// A domain is EXCLUSIVE (no unit carries two) and crossing one is a SKILL, so this is a single value.
	int getDomain(int iUnit) const;
	// The authored PRODUCTION cost (`cost.production`). ⚠ The unit keeps the `getProductionCost` spelling on its
	// info while the BUILDING's became `getCost` -- the rename does not generalize, so confirm the member rather
	// than pattern-matching the sibling accessor.
	int getCost(int iUnit) const;
	// The `UnitAITypes` role the unit is created with (identity.defaultUnitAI).
	int getDefaultUnitAI(int iUnit) const;

	static void pythonPublish();
};

#endif // CyUnitInfo_h__
