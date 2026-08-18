#include "CvGameCoreDLL.h"
#include "Defines/CvGlobals.h"
#include "UI/CvMessageControl.h"
#include "CyMessageControl.h"

void CyMessageControl::sendPushOrder(int iCityID, int eOrder, int iData, bool bAlt, bool bShift, bool bCtrl)
{
	CvMessageControl::getInstance().sendPushOrder(iCityID, (OrderTypes) eOrder, iData, bAlt, bShift, bCtrl);
}

void CyMessageControl::sendDoTask(int iCity, int eTask, int iData1, int iData2, bool bOption, bool bAlt, bool bShift, bool bCtrl)
{
	CvMessageControl::getInstance().sendDoTask(iCity, (TaskTypes) eTask, iData1, iData2, bOption, bAlt, bShift, bCtrl);
}

void CyMessageControl::sendUpdateCivics(const python::list& lCivics)
{
	std::vector<CivicTypes> v;
	python::container_utils::extend_container(v, lCivics);
	CvMessageControl::getInstance().sendUpdateCivics(v);
}

void CyMessageControl::sendEmpireSplit(int /*PlayerTypes*/ ePlayer, int iAreaId)
{
	CvMessageControl::getInstance().sendEmpireSplit((PlayerTypes) ePlayer, iAreaId);
}

void CyMessageControl::sendResearch(int eTech, bool bShift)
{
	CvMessageControl::getInstance().sendResearch((TechTypes)eTech, -1, bShift);
}

void CyMessageControl::sendPlayerOption(int /*PlayerOptionTypes*/ eOption, bool bValue)
{
	gDLL->sendPlayerOption((PlayerOptionTypes) eOption, bValue);
}

void CyMessageControl::sendEspionageSpendingWeightChange(int /*TeamTypes*/ eTargetTeam, int iChange)
{
	CvMessageControl::getInstance().sendEspionageSpendingWeightChange((TeamTypes) eTargetTeam, iChange);
}

void CyMessageControl::sendAdvancedStartAction(int /*AdvancedStartActionTypes*/ eAction, int /*PlayerTypes*/ ePlayer, int iX, int iY, int iData, bool bAdd)
{
	CvMessageControl::getInstance().sendAdvancedStartAction((AdvancedStartActionTypes) eAction, (PlayerTypes) ePlayer, iX, iY, iData, bAdd);
}

void CyMessageControl::sendModNetMessage(int iData1, int iData2, int iData3, int iData4, int iData5)
{
	CvMessageControl::getInstance().sendModNetMessage(iData1, iData2, iData3, iData4, iData5);
}

//
// return true if succeeded
//
int CyMessageControl::GetFirstBadConnection() const
{
	return gDLL->getFirstBadConnection();
}

int CyMessageControl::GetConnState(int iPlayer) const
{
	return gDLL->getConnState((PlayerTypes)iPlayer);
}

//
//	THE COMMAND boundary, republished. ⛔ This is NOT the banned read surface: the Cy* cut is DIRECTIONAL and
//	only the info/state GETTER contract dies (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract)). Every method here SENDS a net message --
//	the MP-safe way Python-authoritative UI asks the engine to act -- so it answers no question about game
//	state and constitutes no getter contract. It was collateral in the binding purge, like TXT and ART.
//
//	⚑ Routing these through the net layer rather than mutating directly is what keeps multiplayer in lockstep
//	(engine.md § THE SYNCHRONIZED RNG: a divergent mutation order desyncs), so the alternative to publishing it
//	is not "Python stops mutating" -- it is Python losing the only synchronized path it has.
//
void CyMessageControl::pythonPublish()
{
	python::class_<CyMessageControl>("CyMessageControl")
		.def("sendPushOrder", &CyMessageControl::sendPushOrder)
		.def("sendDoTask", &CyMessageControl::sendDoTask)
		.def("sendUpdateCivics", &CyMessageControl::sendUpdateCivics)
		.def("sendResearch", &CyMessageControl::sendResearch)
		.def("sendPlayerOption", &CyMessageControl::sendPlayerOption)
		.def("sendEspionageSpendingWeightChange", &CyMessageControl::sendEspionageSpendingWeightChange)
		.def("sendAdvancedStartAction", &CyMessageControl::sendAdvancedStartAction)
		.def("sendModNetMessage", &CyMessageControl::sendModNetMessage)
		.def("sendEmpireSplit", &CyMessageControl::sendEmpireSplit)
		.def("GetFirstBadConnection", &CyMessageControl::GetFirstBadConnection)
		.def("GetConnState", &CyMessageControl::GetConnState)
	;
}
