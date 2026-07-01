//
//	CvCascadeCondition -- see the header. Only the dtor needs a body (it owns + frees its child nodes).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeCondition.h"

CvCascadeCondition::~CvCascadeCondition()
{
	for (size_t i = 0; i < all.size(); ++i)    delete all[i];
	for (size_t i = 0; i < anyOf.size(); ++i)  delete anyOf[i];
	for (size_t i = 0; i < noneOf.size(); ++i) delete noneOf[i];
	delete enabled;
	delete disabled;
}
