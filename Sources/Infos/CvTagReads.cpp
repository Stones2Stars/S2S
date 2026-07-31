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
// The DOMAIN tags (tags.md). ⚠ A domain read goes to the unit's OWN entry (CvUnitInfo::getDomain), never
// through these: a tag says what a unit IS, a domain says WHERE IT OPERATES, and answering the second from
// the first means filtering the tag set for something one field already holds. They exist because the data
// carries them and a surplus tag is inert (tags.md), not because anything should ask them this question.
TAG_READ(seaUnit,  "seaUnit")
TAG_READ(landUnit, "landUnit")
TAG_READ(airUnit,  "airUnit")

#undef TAG_READ

