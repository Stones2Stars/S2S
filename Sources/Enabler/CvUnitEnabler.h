#pragma once
#ifndef CV_UNIT_ENABLER_H
#define CV_UNIT_ENABLER_H

//
//	UnitEnabler -- the UNITS domain's PURE CALCULATORS on the standardized enabler component (enabler.md
//	par.7/7.1; CvEnabler.h): onCityCreated (the lifecycle init + cross-scope fold) + the onCity* event-delta
//	appliers that maintain CvCity::m_enabler.units. The content is built PURELY from DOMAIN events -- the load
//	reseed's in-read emits and the play-time emits are one mechanism (docs/spine.md §5 (the load reseed)); only the cross-scope
//	HAVE that predates the city (team techs + player civics) folds at city creation. A static is a calculator
//	ONLY -- reads are the owner's bare member lookups (canTrain reads m_enabler.units.listed directly).
//	Enable-side stage: no requires gate, no allowed caps, no superseder/dormant-upgrade gates (those are the
//	requires stage); a unit never leaves the frontier on being trained (no held flag) -- spawnOnly is the
//	static never-trainable exclusion.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

class CvCity;

class UnitEnabler
{
public:
	// The city-created applier: init (size + static exclusions) + fold the cross-scope HAVE that predates the
	// city (team techs + player civics), then (outside the load window) the one-city gate pass. Called at
	// founding (CvCity::init) and at the start of the city's save read, BEFORE its own in-read emits stream.
	static void onCityCreated(const CvCity& kCity);
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
	// MUST run BEFORE BuildingEnabler::onCityBuildingChanged (the buildings domain's held flag is the flip guard)
	static void onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent);
	static void onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas);
	static void onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange);   // network count delta; re-gates on a 0-crossing
	static void onCityVicinityBonusChanged(const CvCity& kCity, int iBonus);        // LOCAL presence flip; re-gates vicinity dependents
	// par.7.1 step 2 for the PLOT plane -- the twin of BuildingEnabler::onPlotAtomChanged, reading the enabler's
	// own compiled (kind, id) index rather than EDGEF_REQUIRED_BY, which carries no plot substrate. eKind is a
	// PlotAtomKind.
	static void onPlotAtomChanged(const CvCity& kCity, int eKind, int iId);
	static void plotAtomCensus(int& iKeysOut, int& iEntriesOut);   // the twin of BuildingEnabler::plotAtomCensus
	static void onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic);

	// ==== THE REQUIRES GATE (enabler.md par.7.1 steps 2+3; the par.3 unit machine -- see the .cpp header):
	// the parity-proven canTrain legs (era-scaled instance caps, entity gate, requires.build, the superseder
	// removal, the uc_reachable upgrade-tree dormancy) as the domain's gate verdict. LOAD gates once at
	// GAME_LOAD_FINISHED (the par.7.1 "gate once after the stream ends" option). ====
	static void gateCity(const CvCity& kCity);
	// EVERY city's full gate pass -- the load end, and a GAME OPTION flip (the entity gate's one axis; units are
	// the domain that actually authors option gates today, so a flip genuinely moves trainability).
	static void gateAllCities();
	static void onLoadFinished();
	enum GateClass { GATE_POP = 0, GATE_POWER = 1, GATE_GOLDEN_AGE = 2, GATE_STATE_RELIGION = 3, GATE_DYNAMIC = 4, NUM_GATE_CLASSES = 5 };
	// Per-class member counts + the registry size -- the twin of BuildingEnabler::gateClassCensus. Declared
	// BELOW the enum it is dimensioned by.
	static void gateClassCensus(int (&aiCountsOut)[NUM_GATE_CLASSES], int& iTotalOut);
	// Re-map safety (rj_clearAllRepos): the class / unit-relation / plot-atom lists compile from the infos, so
	// the postmenu re-map that frees them resets them here; the next consumer rebuilds.
	static void clearCompiledIndexes();
	static void onCityGateClass(const CvCity& kCity, int eClass);
	static void onPlayerGateClass(PlayerTypes ePlayer, int eClass);
	// SEVT_UNIT_COUNT (par.7.1 step 3): the changed unit's cap/relations re-gate (skip-guarded for the
	// uncapped, unreferenced, non-upgrade common case -- combat births/deaths stay free).
	static void onUnitCountChanged(PlayerTypes ePlayer, int eUnit);

	// The DECOMPOSITION (the /computed/enabler/units no-guessing surface): one unit's verdict split into the
	// NAMED gate legs, so a wrong offer attributes to a source instead of a hypothesis.
	struct Explain
	{
		bool bInTree, bListed, bSpawnOnly, bObsoleteTech, bCapped, bEntityGateFail, bRequiresFail;
		bool bUpgradeDormant, bSuperseded;
		int iSupersededBy;   // the available superseder that removes it (-1 none)
		Explain() : bInTree(false), bListed(false), bSpawnOnly(false), bObsoleteTech(false), bCapped(false),
			bEntityGateFail(false), bRequiresFail(false), bUpgradeDormant(false), bSuperseded(false), iSupersededBy(-1) {}
	};
	static void explain(const CvCity& kCity, int iUnit, Explain& out);
};

#endif // CV_UNIT_ENABLER_H
