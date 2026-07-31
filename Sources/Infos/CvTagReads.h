#pragma once
#ifndef CV_TAG_READS_H
#define CV_TAG_READS_H

//
//	CvTagReads -- the ONE shared surface for the json.md §8 unit-TAG reads (the `IS_<TAG>` question a consumer
//	asks of a UNIT / UNITCOMBAT info). The tag sibling of CvSkillReads: same shape, CLSD_TAG instead of
//	CLSD_SKILL.
//
//	A purely-organizational static-methods class: no data members, never instantiated. A static class rather
//	than a namespace, because a namespace risks name-mangling under the frozen VC7.1 / Boost / closed-EXE ABI
//	(DEC-single-implementation).
//
//	A tag is IMMUTABLE, accounting-only membership derived from the unit's TYPE (tags.md) -- so a read here is
//	always "what IS this", never "what can it do" (that is a skill). The id is minted at LOAD by the
//	ClassificationRegistry, which is why the reads are per-key rather than parameterized; see CvSkillReads.h
//	for the ruling that makes that form transitional.
//
//	⚠ A block-less info answers FALSE: CvInfo::getTags() returns NULL for a type that authors no tags block.
//

#include "Defines/CvEnums.h"   // DomainTypes -- the engine enum the domain composition maps onto

class CvClassificationBlock;

class CvTagReads
{
public:
	static bool military(const CvClassificationBlock* tags);
	static bool civilian(const CvClassificationBlock* tags);
	static bool spy(const CvClassificationBlock* tags);
	static bool wild(const CvClassificationBlock* tags);
	// The DOMAIN tags (tags.md). ⛔ A domain read goes to `CvUnitInfo::getDomain()`, never through these:
	// a tag says what a unit IS, a domain says WHERE IT OPERATES. Kept because the data carries them.
	static bool seaUnit(const CvClassificationBlock* tags);
	static bool landUnit(const CvClassificationBlock* tags);
	static bool airUnit(const CvClassificationBlock* tags);

private:
	CvTagReads();                                    // organization only -- never instantiated
	CvTagReads(const CvTagReads&);
	CvTagReads& operator=(const CvTagReads&);
};

#endif // CV_TAG_READS_H
