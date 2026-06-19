#include "CvGameCoreDLL.h"
#include "CvInfoUtil.h"
#include "CvPlayerOptionInfo.h"

CvPlayerOptionInfo::CvPlayerOptionInfo()
{
	CvInfoUtil(this).initDataMembers();
}

CvPlayerOptionInfo::~CvPlayerOptionInfo()
{
}

bool CvPlayerOptionInfo::getDefault() const
{
	return m_bDefault;
}

void CvPlayerOptionInfo::getDataMembers(CvInfoUtil& util)
{
	util
		.add(m_bDefault, L"bDefault")
	;
}

bool CvPlayerOptionInfo::read(CvXMLLoadUtility* pXML)
{
	if (!CvInfoBase::read(pXML))
	{
		return false;
	}

	CvInfoUtil(this).readXml(pXML);

	return true;
}

void CvPlayerOptionInfo::copyNonDefaults(const CvPlayerOptionInfo* pClassInfo)
{
	CvInfoBase::copyNonDefaults(pClassInfo);

	CvInfoUtil(this).copyNonDefaults(pClassInfo);
}

void CvPlayerOptionInfo::getCheckSum(uint32_t& iSum) const
{
	CvInfoUtil(this).checkSum(iSum);
}