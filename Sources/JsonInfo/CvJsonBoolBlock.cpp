//
//	CvJsonBoolBlock -- see the header. One shape, five sections (skills/tags/attributes/capabilities/policies).
//

#include "CvGameCoreDLL.h"   // PCH umbrella -- picojson
#include "CvJsonBoolBlock.h"
#include "CvJsonParse.h"     // jsonBoolSet

void CvJsonBoolBlock::parse(const picojson::value& v)
{
	jsonBoolSet(v, m_names);
}
