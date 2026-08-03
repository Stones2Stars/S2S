//
//	CyInfo -- the Python info surface (see the header for the role and the boost rule). Every body is a bare
//	relay through the ONE infotype-prefix -> InfoRepo dispatch, so this file holds no registry knowledge of its
//	own and cannot drift from the load pipeline's table.
//

#include "CvGameCoreDLL.h"
#include "CyInfo.h"
#include "Data/CvReadJson.h"     // rjInfoForType -- the ONE infotype-prefix -> InfoRepo dispatch
#include "Infos/CvInfo.h"
#include "Infos/CvEdges.h"           // the load-derived edge families ([DEC-one-reverse-view])
#include "Infos/CvCivicInfo.h"       // the civic column the bulk index reads
#include "Defines/CvGlobals.h"        // GC.getNumCivicInfos / getCivicInfo

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

std::string CyInfo::getButton(const std::string& szTypePrefix, int iId) const
{
	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL) return std::string();
	const char* szButton = pInfo->getButton();
	return szButton ? std::string(szButton) : std::string();
}

bool CyInfo::exists(const std::string& szTypePrefix, int iId) const
{
	return cyi_info(szTypePrefix, iId) != NULL;
}

python::list CyInfo::getEdgeIds(const std::string& szTypePrefix, int iId, int iFamily, int iBucket) const
{
	python::list lIds;

	const CvInfo* pInfo = cyi_info(szTypePrefix, iId);
	if (pInfo == NULL) return lIds;

	const CvEdges* pEdges = pInfo->getEdges();
	if (pEdges == NULL) return lIds;   // an entity that authors no edge answers EMPTY, never an error

	if (iFamily < 0 || iFamily >= NUM_EDGEF) return lIds;
	if (iBucket < 0 || iBucket >= NUM_EDGEB) return lIds;

	const std::vector<int>* pList = pEdges->find((EnEdgeFamily)iFamily, (EnEdgeBucket)iBucket);
	if (pList == NULL) return lIds;

	for (std::vector<int>::const_iterator it = pList->begin(); it != pList->end(); ++it)
	{
		lIds.append(*it);
	}
	return lIds;
}

// The civic -> CIVICOPTION_ column, compiled ONCE on first use and handed out by reference thereafter. Static
// rather than rebuilt per call because IDValueMap is noncopyable and Python holds a reference: the object has to
// outlive the call. Info data is immutable after load, so one build is correct for the process.
const IDValueMap<CivicTypes, int>& CyInfo::civicOptions() const
{
	static IDValueMap<CivicTypes, int> s_civicOptions;
	static bool s_built = false;
	if (!s_built)
	{
		s_built = true;
		for (int iCivic = 0; iCivic < GC.getNumCivicInfos(); ++iCivic)
		{
			s_civicOptions.insert((CivicTypes)iCivic, GC.getCivicInfo((CivicTypes)iCivic).getCivicOption());
		}
	}
	return s_civicOptions;
}

void CyInfo::pythonPublish()
{
	publishIDValueMapPythonInterface<IDValueMap<CivicTypes, int> >();

	python::class_<CyInfo>("CyInfo")
		.def("getDescription", &CyInfo::getDescription)
		.def("getType",        &CyInfo::getType)
		.def("getButton",      &CyInfo::getButton)
		.def("exists",         &CyInfo::exists)
		.def("getEdgeIds",     &CyInfo::getEdgeIds)
		.def("civicOptions",   &CyInfo::civicOptions, python::return_value_policy<python::reference_existing_object>())
		;
}
