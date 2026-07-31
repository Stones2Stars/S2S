#pragma once
#ifndef CV_CAPABILITY_READS_H
#define CV_CAPABILITY_READS_H

//
//	CvCapabilityReads -- the ONE shared surface for the json.md §8 empire-CAPABILITY reads a consumer makes
//	on a grantor info (a TECH, a CIVIC or a BUILDING: capabilities are empire-HELD, grantor-PROVIDED).
//
//	A purely-organizational static-methods class: no data members, never instantiated. A static class rather
//	than a namespace, because a namespace risks name-mangling under the frozen VC7.1 / Boost / closed-EXE ABI
//	(DEC-single-implementation).
//
//	The sibling of CvSkillReads (unit skills) and CvTagReads (unit tags), and it exists for the same reason
//	patterns.md gives for those: a per-key memoized id resolved in a file-local helper gets reimplemented once
//	per translation unit that wants it. ONE home per domain is the rule; this is the CAPABILITY domain's.
//
//	An info exposes only the parameterized group read getCapabilities(); a consumer asks for a key HERE, and
//	this surface holds the memoized generated id. The ClassificationRegistry mints the CAPABILITY_* ids at LOAD
//	from the union of authored keys, so there is no compile-time id a caller could pass -- which is the whole
//	reason the reads are per-key rather than parameterized.
//
//	⚑ The per-key form is TRANSITIONAL by ruling (patterns.md § THE GETTER SETUP): the parameterized read is
//	the destination, blocked on an id vocabulary the open registry does not hand out at compile time. Having
//	exactly ONE home is what makes that collapse a single edit instead of a sweep.
//
//	⚠ These answer what a GRANTOR PROVIDES, never what an empire HOLDS. The empire's active set is the
//	derived-on-query union over its live sources and has its own surface (CascadeCapabilities, the sole union
//	-- capabilities.md); asking a tech whether it provides a key is a different question from asking a team
//	whether it has one, and the two must not be confused at a call site.
//
//	⚠ A block-less info answers FALSE. CvInfo::getCapabilities() returns NULL for any type that authors no
//	capabilities block, so the read guards it: providing no capabilities is providing no capability.
//

class CvClassificationBlock;

class CvCapabilityReads
{
public:
	static bool canBuildBridges(const CvClassificationBlock* capabilities);
	static bool canFarmDesert(const CvClassificationBlock* capabilities);
	static bool canFoundOnPeaks(const CvClassificationBlock* capabilities);
	static bool canIgnoreIrrigation(const CvClassificationBlock* capabilities);
	static bool canMoveFastOnPeaks(const CvClassificationBlock* capabilities);
	static bool canPassPeaks(const CvClassificationBlock* capabilities);
	static bool canRebaseAnywhere(const CvClassificationBlock* capabilities);
	static bool canSeeFurtherFromWater(const CvClassificationBlock* capabilities);
	static bool canSpreadIrrigation(const CvClassificationBlock* capabilities);
	static bool hasCenteredMap(const CvClassificationBlock* capabilities);
	static bool hasLanguage(const CvClassificationBlock* capabilities);
	static bool hasRiverTrade(const CvClassificationBlock* capabilities);
	static bool hasWholeMapRevealed(const CvClassificationBlock* capabilities);

	// The commerce sliders are three discrete keys, one per flexible channel (capabilities.md): after the
	// split each is a genuine bare-bool ability, so a caller asks for the channel it means.
	static bool canSetScienceRate(const CvClassificationBlock* capabilities);
	static bool canSetCultureRate(const CvClassificationBlock* capabilities);
	static bool canSetEspionageRate(const CvClassificationBlock* capabilities);
	// The same three, selected by CommerceTypes, for a consumer walking the channels.
	static bool canSetCommerceRate(const CvClassificationBlock* capabilities, int iCommerce);

private:
	CvCapabilityReads();                                     // organization only -- never instantiated
	CvCapabilityReads(const CvCapabilityReads&);
	CvCapabilityReads& operator=(const CvCapabilityReads&);
};

#endif // CV_CAPABILITY_READS_H
