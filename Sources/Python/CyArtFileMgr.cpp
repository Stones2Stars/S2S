//
// Python wrapper class for CvArtFileMgr
//
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CyArtFileMgr.h"
// boost needs COMPLETE art types for reference_existing_object -- the SPECIFIC headers, never the
// CvInfos.h umbrella the old interface leaned on (AGENTS.md Conventions).
#include "Infos/CvArtInfoInterface.h"
#include "Infos/CvArtInfoMovie.h"
#include "Infos/CvArtInfoMisc.h"
#include "Infos/CvArtInfoUnit.h"
#include "Infos/CvArtInfoBuilding.h"
#include "Infos/CvArtInfoCivilization.h"
#include "Infos/CvArtInfoLeaderhead.h"
#include "Infos/CvArtInfoBonus.h"
#include "Infos/CvArtInfoImprovement.h"
#include "Infos/CvArtInfoTerrain.h"
#include "Infos/CvArtInfoFeature.h"


CyArtFileMgr::CyArtFileMgr() : m_pArtFileMgr(ARTFILEMGR) {}

CyArtFileMgr::CyArtFileMgr(const CvArtFileMgr& pArtFileMgr) : m_pArtFileMgr(pArtFileMgr) {}


CvArtInfoInterface* CyArtFileMgr::getInterfaceArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getInterfaceArtInfo(szArtDefineTag);
}

CvArtInfoMovie* CyArtFileMgr::getMovieArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getMovieArtInfo(szArtDefineTag);
}

CvArtInfoMisc* CyArtFileMgr::getMiscArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getMiscArtInfo(szArtDefineTag);
}

CvArtInfoUnit* CyArtFileMgr::getUnitArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getUnitArtInfo(szArtDefineTag);
}

CvArtInfoBuilding* CyArtFileMgr::getBuildingArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getBuildingArtInfo(szArtDefineTag);
}

CvArtInfoCivilization* CyArtFileMgr::getCivilizationArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getCivilizationArtInfo(szArtDefineTag);
}

CvArtInfoBonus* CyArtFileMgr::getBonusArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getBonusArtInfo(szArtDefineTag);
}

CvArtInfoImprovement* CyArtFileMgr::getImprovementArtInfo(const char* szArtDefineTag) const
{
	return m_pArtFileMgr.getImprovementArtInfo(szArtDefineTag);
}

//
//	THE ART BOUNDARY, republished. ART IS EXPLICITLY OUT OF SCOPE for this rework (roadmap Scope decisions:
//	the art defines stay in the ART XML and ARTFILEMGR keeps resolving them), so this is a KEPT boundary like
//	TXT -- not the library, and not the banned read surface. It was collateral in the Cy BINDING purge.
//
void CyArtFileMgr::pythonPublish()
{
	python::class_<CyArtFileMgr>("CyArtFileMgr")
		.def("getInterfaceArtInfo", &CyArtFileMgr::getInterfaceArtInfo,  python::return_value_policy<python::reference_existing_object>())
		.def("getMovieArtInfo", &CyArtFileMgr::getMovieArtInfo,  python::return_value_policy<python::reference_existing_object>())
		.def("getMiscArtInfo", &CyArtFileMgr::getMiscArtInfo, python::return_value_policy<python::reference_existing_object>())
		.def("getUnitArtInfo", &CyArtFileMgr::getUnitArtInfo, python::return_value_policy<python::reference_existing_object>())
		.def("getBuildingArtInfo", &CyArtFileMgr::getBuildingArtInfo, python::return_value_policy<python::reference_existing_object>())
		.def("getCivilizationArtInfo", &CyArtFileMgr::getCivilizationArtInfo, python::return_value_policy<python::reference_existing_object>())
		.def("getBonusArtInfo", &CyArtFileMgr::getBonusArtInfo, python::return_value_policy<python::reference_existing_object>())
		.def("getImprovementArtInfo", &CyArtFileMgr::getImprovementArtInfo, python::return_value_policy<python::reference_existing_object>())
	;
}
