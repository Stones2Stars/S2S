//------------------------------------------------------------------------------------------------
//  FILE:    CvSpaceShipInfo.cpp
//------------------------------------------------------------------------------------------------
#include "CvGameCoreDLL.h"
#include "UI/CvArtFileMgr.h"
#include "CvBuildingInfo.h"
#include "CvHeritageInfo.h"
#include "AI/CvGameAI.h"
#include "UI/CvGameTextMgr.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "CvInfoUtil.h"
#include "AI/CvPlayerAI.h"
#include "Infrastructure/CvPython.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Infrastructure/CvXMLLoadUtilityModTools.h"
#include "Tools/CheckSum.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvSpaceShipInfo.h"


//======================================================================================================
//					CvSpaceShipInfo
//======================================================================================================

//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   CvSpaceShipInfo()
//
//  PURPOSE :   Default constructor
//
//------------------------------------------------------------------------------------------------------
CvSpaceShipInfo::CvSpaceShipInfo() :
// Hand-written fields: hand-mapped graphics enums (not info-type FKs) and the
// ProjectName/ProjectType pair resolved through setProjectName().
m_eSpaceShipInfoType(SPACE_SHIP_INFO_TYPE_NONE),
m_eProjectType(NO_PROJECT),
m_eCameraUpAxis(AXIS_X)
{
	// NOTE: the legacy ctor initialized m_iPartNumber/m_iArtType/m_iEventCode to -1, but the
	// legacy read() always stomped absent tags with 0 (GetOptionalChildXmlValByName default)
	// and copyNonDefaults compared against 0, so the wrapper default of 0 reproduces all
	// post-read behaviour; only the never-observed pre-read value changes.
	CvInfoUtil(this).initDataMembers();
}


// Declared in legacy read() order (this class has no getCheckSum). ProjectName (paired with
// m_eProjectType via setProjectName), CameraUpAxis and InfoType (hand-mapped engine enums,
// not registered info types) stay hand-written in read()/copyNonDefaults().
void CvSpaceShipInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_szNodeName, L"NodeName")
		.add(m_iPartNumber, L"PartNumber")
		.add(m_iArtType, L"ArtType")
		.add(m_iEventCode, L"EventCode")
	;
}


//------------------------------------------------------------------------------------------------------
//
//  FUNCTION:   ~CvSpaceShipInfo()
//
//  PURPOSE :   Default destructor
//
//------------------------------------------------------------------------------------------------------
CvSpaceShipInfo::~CvSpaceShipInfo()
{
}


const char* CvSpaceShipInfo::getNodeName()
{
	return m_szNodeName;
}


const char* CvSpaceShipInfo::getProjectName()
{
	return m_szProjectName;
}


void CvSpaceShipInfo::setProjectName(const char* szVal)
{
	m_szProjectName = szVal;
	m_eProjectType = (ProjectTypes) GC.getInfoTypeForString(m_szProjectName, true);
}


ProjectTypes CvSpaceShipInfo::getProjectType()
{
	return m_eProjectType;
}


AxisTypes CvSpaceShipInfo::getCameraUpAxis()
{
	return m_eCameraUpAxis;
}


SpaceShipInfoTypes CvSpaceShipInfo::getSpaceShipInfoType()
{
	return m_eSpaceShipInfoType;
}


int CvSpaceShipInfo::getPartNumber()
{
	return m_iPartNumber;
}


int CvSpaceShipInfo::getArtType()
{
	return m_iArtType;
}


int CvSpaceShipInfo::getEventCode()
{
	return m_iEventCode;
}


bool CvSpaceShipInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	CvString szTextVal;
	pXML->GetChildXmlValByName(szTextVal, L"ProjectName");
	setProjectName(szTextVal);

	//up axis
	pXML->GetChildXmlValByName(szTextVal, L"CameraUpAxis");
	if(szTextVal.CompareNoCase("AXIS_X") == 0)
		m_eCameraUpAxis = AXIS_X;
	else if(szTextVal.CompareNoCase("AXIS_Y") == 0)
		m_eCameraUpAxis = AXIS_Y;
	else if(szTextVal.CompareNoCase("AXIS_Z") == 0)
		m_eCameraUpAxis = AXIS_Z;
	else
	{
		FErrorMsg("[Jason] Unknown Axis Type.");
	}

	//type
	pXML->GetChildXmlValByName(szTextVal, L"InfoType");
	if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_FILENAME") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_FILENAME;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_ALPHA_CENTAURI") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_ALPHA_CENTAURI;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_LAUNCH") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_LAUNCH;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_LAUNCHED") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_LAUNCHED;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_ZOOM_IN") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_ZOOM_IN;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_ZOOM_MOVE") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_ZOOM_MOVE;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_COMPONENT_OFF") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_COMPONENT_OFF;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_COMPONENT_APPEAR") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_COMPONENT_APPEAR;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_COMPONENT_PREVIEW") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_COMPONENT_PREVIEW;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_COMPONENT_ON") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_COMPONENT_ON;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_LIGHT_OFF") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_LIGHT_OFF;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_GANTRY_SMOKE_ON") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_GANTRY_SMOKE_ON;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_IN_SPACE_SMOKE_ON") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_IN_SPACE_SMOKE_ON;
	else if(szTextVal.CompareNoCase("SPACE_SHIP_INFO_TYPE_IN_GAME_SMOKE_ON") == 0)
		m_eSpaceShipInfoType = SPACE_SHIP_INFO_TYPE_IN_GAME_SMOKE_ON;
	else
	{
		FErrorMsg("[Jason] Unknown SpaceShipInfoType.");
	}

	return true;
}


void CvSpaceShipInfo::copyNonDefaults(CvSpaceShipInfo* pClassInfo)
{
	const CvString cDefault = CvString::format("").GetCString();

	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	if (getProjectName() == cDefault) setProjectName(pClassInfo->getProjectName());

//	if (getCameraUpAxis() == cDefault) m_eCameraUpAxis = pClassInfo->getCameraUpAxis();
//	if (getSpaceShipInfoType() == cDefault) m_eSpaceShipInfoType = pClassInfo->getSpaceShipInfoType();
}

