//
//	CyAct -- the Python action surface (see the header for the role, the id rule and the line it does not cross).
//

#include "CvGameCoreDLL.h"
#include "CyAct.h"
#include "Defines/CvGlobals.h"
#include "Engine/CvCity.h"
#include "Engine/CvPlayer.h"
#include "AI/CvPlayerAI.h"                           // GET_PLAYER
#include "Infrastructure/CvDLLInterfaceIFaceBase.h"  // selectCity -- the engine action this relays

bool CyAct::selectCity(int iPlayer, int iCity, bool bTestProduction) const
{
	if (iPlayer < 0 || iPlayer >= MAX_PLAYERS)
	{
		return false;
	}
	CvCity* pCity = GET_PLAYER((PlayerTypes)iPlayer).getCity(iCity);
	if (pCity == NULL)
	{
		return false;
	}
	gDLL->getInterfaceIFace()->selectCity(pCity, bTestProduction);
	return true;
}

void CyAct::pythonPublish()
{
	OutputDebugString("Python Extension Module - CyAct\n");

	python::class_<CyAct>("CyAct")
		.def("selectCity", &CyAct::selectCity)
		;
}
