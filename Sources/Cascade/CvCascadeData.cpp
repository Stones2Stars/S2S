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
