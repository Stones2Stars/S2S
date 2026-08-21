#pragma once

#ifndef CyBuildingInfo_h__
#define CyBuildingInfo_h__

//
//	CyBuildingInfo -- the BUILDING accessor, one of the PER-INFO accessors the Python read boundary is built from
//	([patterns.md] THE PYTHON READ BOUNDARY).
//
//	⛔ EXPLICIT IMPORTS, ALWAYS -- A MODULE'S BINDINGS MUST SHOW WHAT IT USES (owner). A script binds this type by
//	name at module scope, so the bindings list IS the module's dependency list.
//
//	⛔ AND THE CALL SITE NAMES THE THING IT FETCHES. A generic slot read
//	(`getIntrinsic("BUILDING_", id, PYINT_...)`) is decoupled from the global context and STILL fails the test --
//	the caller names a slot rather than the value. The generic prefix-addressed plane ([CyInfo]) is reserved for
//	what is genuinely UNIFORM across every registry: identity text, the classification tests, the edge families.
//	A value belonging to ONE type is named HERE.
//
//	⚑ The six reads below came OFF that plane rather than being copied from it -- there is no building case left
//	in `CyInfo::getIntrinsic`, so the two surfaces cannot drift and a caller has one place to look.
//
//	⚠ NOT the legacy per-FIELD getter contract (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)) -- that ban is on the ~300 hand-named
//	getters mirroring the old `CvXInfo` surface, which is a different axis from a named accessor per info TYPE.
//	This carries the reads Python actually makes, and grows only as a real consumer asks for one.
//
//	The id is the BUILDING, passed per call: an accessor holds no bound entity, so it is a plain read surface
//	rather than a handle with a lifetime.
//
class CyBuildingInfo
{
public:
	CyBuildingInfo() {}

	//	The SPECIALBUILDING_* group this building shares a cap with, NO_SPECIALBUILDING when it is ungrouped
	//	(json §4.4: the member authors the group, the GROUP entity holds `allowed`).
	int getSpecialBuilding(int iBuilding) const;

	//	WHICH scope the self-cap sits at -- an ALLOWEDCAP_* value, or -1 when uncapped. That scope IS the wonder
	//	category: WORLD -> world wonder, TEAM -> team wonder, EMPIRE -> national ([json.md] §4.4, never an
	//	isWorldWonder mirror).
	int getWonderScope(int iBuilding) const;

	//	Is this building capped at all -- by its OWN self-cap or by its specialbuilding GROUP's. Both count, and
	//	reading only the first calls every grouped wonder unlimited.
	bool isLimitedWonder(int iBuilding) const;

	//	RELOCATABLE: the building waives the EMPIRE (national-wonder) cap, so it can be rebuilt elsewhere. The cap
	//	itself stays; only its empire enforcement is waived, which is why this is its own fact.
	bool isRelocatable(int iBuilding) const;

	//	The VOTESOURCE_* diplomatic body this building CONVENES, NO_VOTESOURCE when it convenes none.
	int getVoteSource(int iBuilding) const;

	//	The authored PRODUCTION cost (`cost.production`). ⛔ It carries NO -1 sentinel: the legacy "-1 means not
	//	player-constructible" overload is the `notConstructible` flag's job ([json.md] §7), so a consumer asking
	//	what something COSTS never has to decode a buildability verdict out of the answer.
	int getCost(int iBuilding) const;

	//	The RELIGION_* this building is associated with, NO_RELIGION when none — the religious-building link.
	//	⛔ NOT the SHRINE relationship (json §9 `shrine`, a separate FK) and NOT a holy-city gate, which moved to
	//	`requires.build` ([json.md] §5) and is therefore an availability question, never an info read.
	int getReligion(int iBuilding) const;

	//	The CORPORATION_* this building is the HEADQUARTERS of (json §9 `headquarters`), NO_CORPORATION when none.
	//	⛔ NOT the inverse question -- "which building heads corporation X" is a reverse lookup and belongs to the
	//	edge families (docs/cascade.md §1 (reverse lookups are populated once, at load)), never a scan of every building asking this.
	int getHeadquartersCorporation(int iBuilding) const;

	static void pythonPublish();
};

#endif // CyBuildingInfo_h__
