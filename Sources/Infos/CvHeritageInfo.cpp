#include "CvGameCoreDLL.h"
#include "Tools/FProfiler.h"

#include "Defines/CvDefines.h"
#include "CvHeritageInfo.h"
#include "CvInfoUtil.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"

CvHeritageInfo::CvHeritageInfo()
	// Only the non-XML runtime field needs explicit init (assigned later via setMissionType);
	// every declared field is defaulted by initDataMembers() below.
	: m_iMissionType(NO_MISSION)
{
	CvInfoUtil(this).initDataMembers();
}

CvHeritageInfo::~CvHeritageInfo()
{
	CvInfoUtil(this).uninitDataMembers();
	GC.removeDelayedResolutionVector(m_prereqOrHeritage);
}


void CvHeritageInfo::getDataMembers(CvInfoUtil& util)
{
	// Declared in the legacy getCheckSum order. m_prereqOrHeritage (delayed-resolution vector,
	// no wrapper yet) stays hand-written in read/copyNonDefaults/dtor. m_bNeedLanguage is read
	// but was never part of the legacy checksum, so getCheckSum below stays explicit and this
	// field is parked last.
	util
		.add(m_PropertyManipulators)
		.addEnumAsInt(m_iPrereqTech, L"PrereqTech")
		.add(m_eraCommerceChanges, L"EraCommerceChanges", L"EraType", L"CentiCommerce")
		.add(m_bNeedLanguage, L"bNeedLanguage")
	;
}


void CvHeritageInfo::getCheckSum(uint32_t& iSum) const
{
	// Explicit (not delegated) to reproduce the legacy field set and order byte-for-byte:
	// m_bNeedLanguage is read but was never checksummed, and the hand-written m_prereqOrHeritage
	// sits mid-order.
	m_PropertyManipulators.getCheckSum(iSum);

	CheckSum(iSum, m_iPrereqTech);

	CheckSumC(iSum, m_prereqOrHeritage);
	CheckSumC(iSum, m_eraCommerceChanges);
}

bool CvHeritageInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	pXML->SetOptionalVectorWithDelayedResolution(m_prereqOrHeritage, L"PrereqOrHeritage");

	return true;
}

void CvHeritageInfo::copyNonDefaults(const CvHeritageInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);

	GC.copyNonDefaultDelayedResolutionVector(m_prereqOrHeritage, pClassInfo->getPrereqOrHeritage());
}

void CvHeritageInfo::doPostLoadCaching(uint32_t iThis)
{
}