//
//	CvTagReads -- see the header. The tag sibling of CvSkillReads; one memoized generated-id per key, and a
//	NULL block (an info type that authors no tags) answers false.
//

#include "CvGameCoreDLL.h"   // PCH umbrella
#include "CvTagReads.h"
#include "CvClassificationBlock.h"

#define TAG_READ(method, key)                                              \
	bool CvTagReads::method(const CvClassificationBlock* tags)             \
	{                                                                      \
		static int s_id = -1;                                              \
		return tags != NULL && tags->hasKey(s_id, CLSD_TAG, key);          \
	}

TAG_READ(military, "military")
TAG_READ(civilian, "civilian")
TAG_READ(spy,      "spy")
TAG_READ(wild,     "wild")
// The DOMAIN tags (tags.md): DOMAIN_* stays the engine enum for movement/stacking, and these are the
// classification VIEW of it -- what a consumer asks when the question is "what IS this unit", not how the
// pathfinder should move it. A CvUnitInfo has no domain getter of its own by design.
TAG_READ(seaUnit,  "seaUnit")
TAG_READ(landUnit, "landUnit")
TAG_READ(airUnit,  "airUnit")

#undef TAG_READ
