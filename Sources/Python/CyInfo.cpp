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
// The straggler dispatch reaches these concrete registries -- specific headers, never the CvInfos.h umbrella.
#include "Infos/CvBuildingInfo.h"
#include "Infos/CvAllowed.h"
#include "Infos/CvSpecialBuildingInfo.h"   // the GROUP that may hold the cap for its members
#include "Infos/CvBonusInfo.h"
#include "Infos/CvSpecialistInfo.h"
#include "Infos/CvYieldInfo.h"
#include "Infos/CvControlInfo.h"
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

	// A SELF-cap at any ownable scope. The CATEGORY count-caps are a different axis and are not one.
	bool cyi_hasSelfCap(const CvAllowed* pAllowed)
	{
		if (pAllowed == NULL) return false;
		return pAllowed->cap(ALLOWEDCAP_WORLD)  >= 0
		    || pAllowed->cap(ALLOWEDCAP_TEAM)   >= 0
		    || pAllowed->cap(ALLOWEDCAP_EMPIRE) >= 0;
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

int CyInfo::getIntrinsic(const std::string& szTypePrefix, int iId, int iSlot) const
{
	if (iId < 0) return -1;

	// ⚑ Two populations, deliberately in ONE place. The JSON-backed types resolve through the shared
	// infotype-prefix dispatch; the XML-only registries (yields, controls) have no InfoRepo entry and are
	// reached through their own accessor. Keeping both here means a consumer never has to know which is which,
	// and the split cannot leak into script.
	switch (iSlot)
	{
	case PYINT_COST:
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
			return GC.getBuildingInfo((BuildingTypes)iId).getCost();
		break;

	case PYINT_BONUS_CLASS:
		if (szTypePrefix == "BONUS_" && iId < GC.getNumBonusInfos())
			return GC.getBonusInfo((BonusTypes)iId).getBonusClassType();
		break;

	case PYINT_IS_MAP_BONUS:
		if (szTypePrefix == "BONUS_" && iId < GC.getNumBonusInfos())
			return GC.getBonusInfo((BonusTypes)iId).isMapBonus() ? 1 : 0;
		break;

	case PYINT_IS_VISIBLE:
		if (szTypePrefix == "SPECIALIST_" && iId < GC.getNumSpecialistInfos())
			return GC.getSpecialistInfo((SpecialistTypes)iId).isVisible() ? 1 : 0;
		break;

	case PYINT_COLOR_TYPE:
		if (szTypePrefix == "YIELD_" && iId < NUM_YIELD_TYPES)
			return GC.getYieldInfo((YieldTypes)iId).getColorType();
		break;

	case PYINT_ACTION_INFO_INDEX:
		// CvControlInfo derives from CvHotkeyInfo, which owns the index -- the control registry is XML-only,
		// so there is no InfoRepo route to it.
		// Controls are a FIXED enum rather than a counted registry, so the bound is the enum terminator.
		if (szTypePrefix == "CONTROL_" && iId < NUM_CONTROL_TYPES)
			return GC.getControlInfo((ControlTypes)iId).getActionInfoIndex();
		break;

	case PYINT_IS_LIMITED_WONDER:
		// ⚑ A building's wonder CATEGORY is derived from WHICH self-cap it authors ([json.md §4.4]: the cap's
		// scope is what makes it a world / team / national wonder), never from an isWorldWonder mirror.
		//
		// ⛔ TWO WAYS TO BE CAPPED, and reading only the first is wrong. A building may carry its own self-cap,
		// OR belong to a SPECIALBUILDING GROUP that holds the cap for all its members (json.md §4.4: the member
		// authors identity.specialBuildingType, the GROUP entity holds `allowed`). A grouped wonder authors no
		// cap of its own, so a self-cap-only test calls it unlimited -- silently, and only for the grouped ones.
		// This mirrors the enabler's own gate (CvBuildingEnabler bd_groupCapOk / the group-capped re-gate), so
		// there is ONE meaning of "capped" rather than two that can drift.
		//
		// ⚠ The CATEGORY count-caps (worldWonders/teamWonders/nationalWonders) are a DIFFERENT axis -- a per-city
		// bound set by CultureLevel -- and deliberately do not make a building "limited".
		if (szTypePrefix == "BUILDING_" && iId < GC.getNumBuildingInfos())
		{
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iId);
			if (cyi_hasSelfCap(kBuilding.getAllowed())) return 1;

			const int iSpecialBuilding = kBuilding.getSpecialBuildingType();
			if (iSpecialBuilding != NO_SPECIALBUILDING
			&&  cyi_hasSelfCap(GC.getSpecialBuildingInfo((SpecialBuildingTypes)iSpecialBuilding).getAllowed()))
			{
				return 1;
			}
			return 0;
		}
		break;

	default:
		break;
	}
	return -1;
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
		.def("getIntrinsic",   &CyInfo::getIntrinsic)
		.def("civicOptions",   &CyInfo::civicOptions, python::return_value_policy<python::reference_existing_object>())
		;
}
