//
//	CvCascadeData -- see the header. Only the dtor needs an out-of-line body (it frees the owned BoolExpr trees).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeData.h"

CvCascadeData::~CvCascadeData()
{
	for (size_t i = 0; i < deposits.size(); ++i)
	{
		delete deposits[i].enabled;
		delete deposits[i].disabled;
	}
	delete requiresBuild;
	delete requiresOperate;
}

// --- the side-table (keyed by the game object's CvInfoBase*; the ABI-safe home -- see the header) ---
// Function-local static avoids static-init-order issues; lives for the process (the infos do too).
static std::map<const CvInfoBase*, CvCascadeData*>& rjCascadeTable()
{
	static std::map<const CvInfoBase*, CvCascadeData*> s_table;
	return s_table;
}

CvCascadeData* cascadeForInfo(const CvInfoBase* pInfo)
{
	std::map<const CvInfoBase*, CvCascadeData*>::const_iterator it = rjCascadeTable().find(pInfo);
	return it != rjCascadeTable().end() ? it->second : NULL;
}

void cascadeAttach(const CvInfoBase* pInfo, CvCascadeData* pData)
{
	CvCascadeData*& slot = rjCascadeTable()[pInfo];
	if (slot != pData) delete slot;   // replace + free any existing mapping for this info (re-attach)
	slot = pData;
}
