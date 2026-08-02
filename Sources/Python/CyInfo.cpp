//
//	CyInfo -- the Python info surface (see the header for the role and the boost rule). Every body is a bare
//	relay through the ONE infotype-prefix -> InfoRepo dispatch, so this file holds no registry knowledge of its
//	own and cannot drift from the load pipeline's table.
//

#include "CvGameCoreDLL.h"
#include "CyInfo.h"
#include "Data/CvReadJson.h"     // rjInfoForType -- the ONE infotype-prefix -> InfoRepo dispatch
#include "Infos/CvInfo.h"

namespace
{
	// Resolve without asserting: a script may legitimately probe an id past the end of a registry, and the
	// honest answer there is "nothing", not a crash -- the same discipline CyEnabler and CyState apply.
	const CvInfo* cyi_info(const std::string& szTypePrefix, int iId)
	{
		if (iId < 0) return NULL;
		return rjInfoForType(szTypePrefix, iId);
	}
}

std::wstring CyInfo::getDescription(const std::string& szTypePrefix, int iId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? std::wstring(pInfo->getDescription()) : std::wstring();
}

std::string CyInfo::getType(const std::string& szTypePrefix, int iId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	return pInfo ? std::string(pInfo->getType()) : std::string();
}

bool CyInfo::exists(const std::string& szTypePrefix, int iId) const
{
	return cyi_info(szTypePrefix, iId) != NULL;
}

void CyInfo::pythonPublish()
{
	python::class_<CyInfo>("CyInfo")
		.def("getDescription", &CyInfo::getDescription)
		.def("getType",        &CyInfo::getType)
		.def("exists",         &CyInfo::exists)
		;
}
