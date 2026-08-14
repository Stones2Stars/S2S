#pragma once
#ifndef CV_BUILDING_ENABLER_H
#define CV_BUILDING_ENABLER_H

//
//	BuildingEnabler -- the BUILDINGS domain's PURE CALCULATORS on the standardized enabler component
//	(enabler.md par.7/7.1; CvEnabler.h): onCityCreated (the lifecycle init + cross-scope fold) + the onCity*
//	event-delta appliers that maintain CvCity::m_enabler.buildings. The content is built PURELY from DOMAIN
//	events -- the load reseed's in-read emits and the play-time emits are one mechanism (DEC-spine-reseed);
//	only the cross-scope HAVE that predates the city (team techs + player civics) folds at city creation. A
//	static is a calculator ONLY -- reads are the owner's bare member lookups (canConstruct reads
//	m_enabler.buildings.listed directly). Also home of the shared AugmentState prereq-WAIVER set
//	(augmentWaived), which the unit enabler reuses.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//

#include <set>

class CvInfo;
class CvPlayer;
class CvTeam;
class CvCity;

class BuildingEnabler
{
public:
	// AugmentState's prereq-WAIVER set (ObsoleteBuildings ∪ PrereqWaivedBuildings). Shared by the building + unit
	// enablers (both gate requires.build through the SAME evaluator).
	static void augmentWaived(const CvPlayer& kPlayer, const CvTeam& kTeam, std::set<int>& waived);

	// ==== the STANDARDIZED per-city BUILDING domain (enabler.md par.7/7.1; CvEnabler.h -- CvCity::m_enabler) ====
	// The domain arrays are the ONLY mutable state: every HAVE-event applies its source's building edges as a
	// DIRECT +/-1 delta (flip-guarded emits / live-derivable crossings; a thin event is an emit-surface gap to
	// fix, never a license for side state). At load the SAME appliers consume the reseed's in-read emits.

	// The HAVE-source axes feeding the buildings bucket (the store's enables.buildings inversion sources).
	enum CityHaveAxis
	{
		AX_TECH = 0,       // team techs (incl. the TECH_GAME_START root via the cascadeStartNode redirect)
		AX_CIVIC = 1,      // the player's adopted civics
		AX_BUILDING = 2,   // this city's present buildings (contributing only while NOT tech-obsolete)
		AX_RELIGION = 3,   // this city's present religions
		AX_CORP = 4,       // this city's present corporations
		AX_BONUS = 5,      // this city's bonus access (trade network or vicinity -- enable-side generous)
		AX_CULTURE = 6     // this city's culture level
	};

	// The city-created applier: init (size + static exclusions) + fold the cross-scope HAVE that predates the
	// city (team techs + player civics), then (outside the load window) the one-city gate pass. Called at
	// founding (CvCity::init) and at the start of the city's save read, BEFORE its own in-read emits stream.
	// The city's own facts arrive as events.
	static void onCityCreated(const CvCity& kCity);

	// ==== THE REQUIRES GATE (enabler.md par.7.1 steps 2+3): requiresMet (build ∧ operate) + the allowed
	// self-caps, through the ONE evaluator/cap check; a failed gate flips a tree member LISTED -> GREYED. ====
	// LOAD follows the par.7.1 order rule's "gate once after the stream ends" option: NO gate evaluations
	// inside the load bracket (a mid-read evaluation would ensure the operating-buildings cache against
	// half-read state); GAME_LOAD_FINISHED runs one full gate pass per city. Play-time follows the pure
	// per-event option: gate-on-entry + touched re-gates in each applier, the FK axes via EDGEF_REQUIRED_BY.
	static void gateCity(const CvCity& kCity);   // the one-city full gate pass (load-end + city creation)
	// EVERY city's full gate pass. Two callers, both WHOLESALE facts with no finer route to derive: the load end,
	// and a GAME OPTION flip (an option is the entity gate's one axis, so a flip can move ANY entity's verdict).
	static void gateAllCities();
	static void onLoadFinished();                // SEVT_GAME_LOAD_FINISHED: gate every city once
	// The CLASS re-gates: event classes with no FK reverse edge (no info to home a REQUIRED_BY on) re-gate the
	// load-compiled list of candidates whose requires reference that class (EnablerKernel::scanCondDeps).
	enum GateClass { GATE_POP = 0, GATE_POWER = 1, GATE_GOLDEN_AGE = 2, GATE_STATE_RELIGION = 3, GATE_DYNAMIC = 4, NUM_GATE_CLASSES = 5 };
	static void onCityGateClass(const CvCity& kCity, int eClass);      // pop / power (city-scope events)
	static void onPlayerGateClass(PlayerTypes ePlayer, int eClass);    // golden-age / state-religion (player-scope)
	// MUST run BEFORE TechEnabler::onTechChanged (the player tech domain's held flag is the broad-emit flip guard)
	static void onCityTechChanged(TeamTypes eTeam, TechTypes eTech, bool bHas);
	static void onCityBuildingChanged(const CvCity& kCity, int iBuilding, bool bPresent);
	static void onCityOrderChanged(const CvCity& kCity, int iBuilding);   // queue push/pop of THIS building: the one-id re-gate (par.7.1 step 3)
	// par.7.1 step 2 for the PLOT plane: re-gate exactly the buildings whose requires names this atom.
	// ⛔ It reads the enabler's own compiled (kind, id) index, NOT EDGEF_REQUIRED_BY -- the reverse pass lands no
	// edge for any plot-substrate prefix, so that walk finds nothing and silently re-gates nobody (enabler.md
	// par.8: a coarse list matches a coarse event). eKind is a PlotAtomKind.
	static void onPlotAtomChanged(const CvCity& kCity, int eKind, int iId);
	// The index's own census (distinct atom keys, total candidate entries) -- reported at load, because an index
	// that compiled EMPTY re-gates nobody and looks exactly like one with nothing to do.
	static void plotAtomCensus(int& iKeysOut, int& iEntriesOut);
	// Per-class member counts + the registry size -- the instrument that makes a class WIDENING observable.
	static void gateClassCensus(int (&aiCountsOut)[NUM_GATE_CLASSES], int& iTotalOut);
	// Re-map safety (rj_clearAllRepos): the gate-class / plot-atom / group-member / capped lists compile from
	// the infos, so the postmenu re-map that frees them resets them here; the next consumer rebuilds.
	static void clearCompiledIndexes();
	static void onBuildingCountChanged(PlayerTypes ePlayer, int eBuilding);   // the empire per-type COUNT moved: re-check its `allowed` self-cap across the cap's own scope
	static void onCityReligionChanged(const CvCity& kCity, int iReligion, bool bHas);
	static void onCityCorporationChanged(const CvCity& kCity, int iCorporation, bool bHas);
	static void onCityBonusChanged(const CvCity& kCity, int iBonus, int iChange);     // network count delta; re-gates on a 0-crossing
	static void onCityVicinityBonusChanged(const CvCity& kCity, int iBonus);          // LOCAL presence flip; re-gates vicinity dependents
	static void onCityCultureLevelChanged(const CvCity& kCity, int iLevel, int iCrossing);
	static void onPlayerCivicsChanged(PlayerTypes ePlayer, int iOldCivic, int iNewCivic);

	// NB there is NO read accessor here (enabler.md par.7: a static is a PURE CALCULATOR -- seed + delta only;
	// a read is the owner's BARE member lookup, canConstruct reads m_enabler.buildings.listed directly).
	// ⛔ And no fresh-seed-and-diff either: a recompute served beside the maintained set answers a number that
	// was never comparable ([superseded-ideas #33](../../docs/architecture/superseded-ideas.md)). A wrong verdict
	// is caught by the THREE-LEG check, and DECOMPOSED for a reader by /computed/enabler/buildings + /verdict.
};

#endif // CV_BUILDING_ENABLER_H
