//
//	Published Python interface for CyEnabler -- the NEW availability surface (enabler.md par.8).
//
//	⛔ Registered as its OWN class rather than as extra .defs on CyCity / CyPlayer ([DEC-cy-not-fixed]): the
//	replacement surface must be able to stand while the legacy per-type wrappers are CUT AWAY, which it cannot
//	do if it lives inside them.
//	BOOST: `python::` only (= boost::python, the 1.32 compiled bridge). Never a bare `boost::`, never a
//	using-directive -- the tree carries two Boosts and the PCH will silently resolve the wrong one (engine.md).
//

#include "CvGameCoreDLL.h"
#include "CyEnabler.h"

void CyEnablerPythonInterface()
{
	OutputDebugString("Python Extension Module - CyEnablerPythonInterface\n");

	python::class_<CyEnabler>("CyEnabler")
		// CITY domains -- construction and training are one plane, both gated on city-local supply
		.def("getBuildingAvailability", &CyEnabler::getBuildingAvailability, "int (int iPlayer, int iCity, int eBuilding) - EnablerState tri-state; == ENABLER_LISTED is offerable now")
		.def("getUnitAvailability", &CyEnabler::getUnitAvailability, "int (int iPlayer, int iCity, int eUnit) - EnablerState tri-state")
		.def("isBuildingContinuable", &CyEnabler::isBuildingContinuable, "bool (int iPlayer, int iCity, int eBuilding) - may an in-progress build carry on (reads past the queued overlay)")
		.def("getAvailableBuildings", &CyEnabler::getAvailableBuildings, "list (int iPlayer, int iCity) - the city's LISTED building frontier; iterate this, never the whole database")
		.def("getAvailableUnits", &CyEnabler::getAvailableUnits, "list (int iPlayer, int iCity) - the city's LISTED unit frontier")
		// PLAYER domains
		.def("getTechAvailability", &CyEnabler::getTechAvailability, "int (int iPlayer, int eTech) - EnablerState tri-state")
		.def("getCivicAvailability", &CyEnabler::getCivicAvailability, "int (int iPlayer, int eCivic) - EnablerState tri-state")
		.def("getProjectAvailability", &CyEnabler::getProjectAvailability, "int (int iPlayer, int eProject) - EnablerState tri-state")
		.def("getProcessAvailability", &CyEnabler::getProcessAvailability, "int (int iPlayer, int eProcess) - EnablerState tri-state")
		.def("getAvailableTechs", &CyEnabler::getAvailableTechs, "list (int iPlayer) - the LISTED tech frontier")
		.def("getAvailableCivics", &CyEnabler::getAvailableCivics, "list (int iPlayer) - the LISTED civic frontier")
		.def("getAvailableProjects", &CyEnabler::getAvailableProjects, "list (int iPlayer) - the LISTED project frontier")
		.def("getAvailableProcesses", &CyEnabler::getAvailableProcesses, "list (int iPlayer) - the LISTED process frontier")
		// the two carve-outs -- UNLOCKED half only; the plot/unit half is evaluated live at the decision point
		.def("getBuildUnlocked", &CyEnabler::getBuildUnlocked, "int (int iPlayer, int eBuild) - unlocked half ONLY; plot validity is a live per-plot gate")
		.def("getPromotionUnlocked", &CyEnabler::getPromotionUnlocked, "int (int iPlayer, int ePromotion) - unlocked half ONLY; unit applicability is evaluated at level-up")
		// the empire-wide fan -- walks the cities; there is no player-level construct/train verdict
		.def("canAnyCityTrain", &CyEnabler::canAnyCityTrain, "bool (int iPlayer, int eUnit) - fans over the player's cities")
		.def("canAnyCityConstruct", &CyEnabler::canAnyCityConstruct, "bool (int iPlayer, int eBuilding) - fans over the player's cities")
		;
}
