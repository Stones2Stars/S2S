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
	// The DOMAIN view (tags.md): `DOMAIN_*` remains the engine enum movement and stacking are wired to; these
	// answer the classification question "what IS this unit" that a rebuilt CvUnitInfo carries no getter for.
	static bool seaUnit(const CvClassificationBlock* tags);
	static bool landUnit(const CvClassificationBlock* tags);
	static bool airUnit(const CvClassificationBlock* tags);

	// The COMPOSITION every `kUnitInfo.getDomainType() == eDomain` site becomes. It is a composition of the
	// three reads above rather than a parameterized id read -- the ids are minted at LOAD, so there is no
	// compile-time domain id to key on ([DEC-classification-infos]).
	// ⛔ It lives HERE rather than at each call site for the reason the class exists at all: a per-site copy of
	// the mapping is the duplicate that made four translation units reimplement the same memoized-id family
	// ([patterns.md] -- "the next consumer can't see it, so it reimplements it").
	// ⚠ A domain with no matching tag answers FALSE, which is the honest answer for a unit whose tags block
	// has not been filled -- never a silent "matches everything".
	static bool isDomain(const CvClassificationBlock* tags, DomainTypes eDomain);

	// The INVERSE, for a consumer that needs the domain as a VALUE rather than a test -- indexing a per-domain
	// array, say. Well-defined because the three domain tags are derived from the unit's single `DOMAIN_*`
	// (tags.md), so exactly one can hold. ⚠ Answers NO_DOMAIN for a unit whose tags block has not been filled;
	// a caller indexing an array MUST guard that rather than let it subscript with -1.
	static DomainTypes domainOf(const CvClassificationBlock* tags);

private:
	CvTagReads();                                    // organization only -- never instantiated
	CvTagReads(const CvTagReads&);
	CvTagReads& operator=(const CvTagReads&);
};

#endif // CV_TAG_READS_H
