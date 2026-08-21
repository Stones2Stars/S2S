//
//	CvGate -- see the header. The entity-level enabled/disabled pair, parsed through the ONE condition boundary.
//

#include "CvGameCoreDLL.h"          // PCH umbrella -- picojson
#include "CvGate.h"
#include "CvJsonConditionParse.h"   // cascadeParseCondition

CvGate::~CvGate()
{
	clearParsed();
}

void CvGate::clearParsed()
{
	delete enabled;  enabled = NULL;
	delete disabled; disabled = NULL;
}

void CvGate::parseEnabled(const picojson::value& v)
{
	delete enabled;
	enabled = cascadeParseCondition(v);
}

void CvGate::parseDisabled(const picojson::value& v)
{
	delete disabled;
	disabled = cascadeParseCondition(v);
}
