#pragma once

#ifndef CyWorldInfo_h__
#define CyWorldInfo_h__

//
//	CyWorldInfo -- the WORLD-SIZE accessor, one of the PER-INFO accessors the Python read boundary is built from
//	([patterns.md] THE PYTHON READ BOUNDARY).
//
//	⛔ EXPLICIT IMPORTS, ALWAYS -- A MODULE'S BINDINGS MUST SHOW WHAT IT USES (owner). A script binds this type by
//	name at module scope, so the bindings list IS the module's dependency list. That is what the `Cy*` cut was for:
//	`GC.getWorldInfo(i).getFoo()` declared nothing, so a reader could not tell which registries a module touched
//	and the god object handed out everything.
//
//	⛔ AND THE CALL SITE NAMES THE THING IT FETCHES. A generic slot read
//	(`getIntrinsic("WORLD_", id, PYINT_...)`) is decoupled from the global context and STILL fails the test --
//	the caller names a slot rather than the value, so a reader again does not know what is being fetched. The
//	generic prefix-addressed plane ([CyInfo]) is reserved for what is genuinely UNIFORM across every registry:
//	identity text, the classification tests, the edge families. A value belonging to ONE type is named HERE.
//
//	⚠ NOT the legacy per-FIELD getter contract (docs/architecture/patterns.md §THE TWO READ ROLES (new getter surface, never widen legacy)) -- that ban is on the ~300 hand-named
//	getters mirroring the old `CvXInfo` surface, which is a different axis from a named accessor per info TYPE.
//	This carries the reads Python actually makes, and grows only as a real consumer asks for one.
//
//	The id is the WORLD SIZE, passed per call: an accessor holds no bound entity, so it is a plain read surface
//	rather than a handle with a lifetime.
//
class CyWorldInfo
{
public:
	CyWorldInfo() {}

	// The staging screen's player count for this map size.
	int getDefaultPlayers(int iWorldSize) const;
	// The city count this size is balanced around -- the `TARGET_NUM_CITIES` token's value ([json.md] 3.3).
	int getTargetNumCities(int iWorldSize) const;
	// Map-generation shaping: the smallest water body that counts as ocean, and the terrain/feature grain deltas.
	int getOceanMinAreaSize(int iWorldSize) const;
	//	The world size's map DIMENSIONS -- the grid a map script lays its plots out on.
	int getGridWidth(int iWorldSize) const;
	int getGridHeight(int iWorldSize) const;
	int getTerrainGrainChange(int iWorldSize) const;
	int getFeatureGrainChange(int iWorldSize) const;
	// Corporate maintenance scales with map size ([economy.md] -- a per-size CONFIG percent, HUMAN, never x100).
	int getCorporationMaintenancePercent(int iWorldSize) const;

	static void pythonPublish();
};

#endif // CyWorldInfo_h__
