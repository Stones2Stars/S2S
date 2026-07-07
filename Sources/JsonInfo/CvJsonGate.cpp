//
//	CvJsonGate -- see the header. The entity-level enabled/disabled pair, parsed through the ONE condition boundary.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvJsonGate.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition

CvJsonGate::~CvJsonGate()
{
	delete enabled;
	delete disabled;
}

void CvJsonGate::parseEnabled(const picojson::value& v)
{
	delete enabled;
	enabled = cascadeParseCondition(v);
}

void CvJsonGate::parseDisabled(const picojson::value& v)
{
	delete disabled;
	disabled = cascadeParseCondition(v);
}
