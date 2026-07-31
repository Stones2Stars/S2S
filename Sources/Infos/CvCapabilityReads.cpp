//
//	CvCapabilityReads -- see the header. One memoized generated-id per key, resolved once and bit-tested
//	thereafter; a NULL block (an info type that authors no capabilities) answers false.
//

#include "CvGameCoreDLL.h"   // PCH umbrella
#include "CvCapabilityReads.h"
#include "CvClassificationBlock.h"

// Each read owns the id it memoizes. The id is minted at LOAD by the ClassificationRegistry, so the first
// call resolves it and every later call is an O(1) bitset test (the pre-resolve load window falls back to
// the string set inside hasKey, so an early consumer still reads correctly).
#define CAPABILITY_READ(method, key)                                                  \
	bool CvCapabilityReads::method(const CvClassificationBlock* capabilities)          \
	{                                                                                 \
		static int s_id = -1;                                                         \
		return capabilities != NULL && capabilities->hasKey(s_id, CLSD_CAPABILITY, key); \
	}

CAPABILITY_READ(canBuildBridges,        "canBuildBridges")
CAPABILITY_READ(canFarmDesert,          "canFarmDesert")
CAPABILITY_READ(canFoundOnPeaks,        "canFoundOnPeaks")
CAPABILITY_READ(canIgnoreIrrigation,    "canIgnoreIrrigation")
CAPABILITY_READ(canMoveFastOnPeaks,     "canMoveFastOnPeaks")
CAPABILITY_READ(canPassPeaks,           "canPassPeaks")
CAPABILITY_READ(canRebaseAnywhere,      "canRebaseAnywhere")
CAPABILITY_READ(canSeeFurtherFromWater, "canSeeFurtherFromWater")
CAPABILITY_READ(canSpreadIrrigation,    "canSpreadIrrigation")
CAPABILITY_READ(hasCenteredMap,         "hasCenteredMap")
CAPABILITY_READ(hasLanguage,            "hasLanguage")
CAPABILITY_READ(hasRiverTrade,          "hasRiverTrade")
CAPABILITY_READ(hasWholeMapRevealed,    "hasWholeMapRevealed")

CAPABILITY_READ(canSetScienceRate,      "canSetScienceRate")
CAPABILITY_READ(canSetCultureRate,      "canSetCultureRate")
CAPABILITY_READ(canSetEspionageRate,    "canSetEspionageRate")

// GOLD has no slider -- it is the RESIDUAL of the other three (capabilities.md), so no key exists for it and
// a caller walking the channels gets a straight false rather than a missing-key lookup.
bool CvCapabilityReads::canSetCommerceRate(const CvClassificationBlock* capabilities, int iCommerce)
{
	switch (iCommerce)
	{
	case COMMERCE_RESEARCH:   return canSetScienceRate(capabilities);
	case COMMERCE_CULTURE:    return canSetCultureRate(capabilities);
	case COMMERCE_ESPIONAGE:  return canSetEspionageRate(capabilities);
	default:                  return false;
	}
}
