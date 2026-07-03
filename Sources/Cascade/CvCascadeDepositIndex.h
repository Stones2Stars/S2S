#pragma once
#ifndef CV_CASCADE_DEPOSIT_INDEX_H
#define CV_CASCADE_DEPOSIT_INDEX_H

//
//	DepositIndex -- the #430 COMPILED DEPOSIT INDEX: the load-time strings->ints compile over the CvJsonInfo
//	deposits (modifier-substrate.md "the compiled deposit index"; cutover.md flip lesson: "the JSON stays
//	HUMAN-shaped ... and the LOAD step programmatically compiles it into the top-down routing"). Every deposit's
//	address + unit is interned ONCE when readJson pushes it; the hot-path matchers (MMKernel et al.) then compare
//	INTS, and a query address that was never authored anywhere answers 0 without touching a single deposit.
//
//	The compiled SEGMENTS (family / scope / member / target + the FK-resolved target id) are ALSO the generator of
//	the data-derived event->cache routing (state-repositories.md end-state): a DOMAIN event's source names the
//	channels x scopes x targets it touches straight off its compiled deposits -- today's hand-wired hook masks are
//	the interim shape of that derivation.
//
//	Purely-organizational static-methods class: NO data members, never instantiated (patterns.md static-class law).
//	Game-thread only. The interner is APPEND-ONLY -- ids stay valid across a readJson re-map (rj_clearAllRepos +
//	re-map re-interns the same strings to the SAME ids). Query-side caches therefore may cache a hit forever but
//	must RE-LOOKUP while negative (a miss can turn into a hit after a re-map introduces new data).
//

#include <string>

struct CvCascadeDeposit;

class DepositIndex
{
public:
	// Intern (assign-on-first-sight) -- the load-time compile side; append-only.
	static int internSegment(const std::string& s);
	static int internAddress(const std::string& s);
	// Lookup WITHOUT interning -- the query side. -1 = never authored anywhere => any gated sum over it is 0.
	static int lookupSegment(const std::string& s);
	static int lookupAddress(const std::string& s);

	// Fill a deposit's compiled fields from its address/unit strings (readJson push-time; the strings stay for
	// rendering/diagnostics). Splits the dotted address, interns each segment (the first CASC_DEP_SEGS kept),
	// interns the whole address, and FK-resolves the LAST segment to an engine info id when it is an INFOTYPE key.
	static void compile(CvCascadeDeposit& d);

	// Lazily-cached SEGMENT ids for per-info-type key strings (TYPE string -> segment id) -- kills all string
	// handling in the per-plot keyed walks. -1 = that TYPE was never authored as a deposit key (nothing matches).
	// Hits cache forever; misses re-lookup (append-only interner, see the header note).
	static int segIdForTerrain(int i);
	static int segIdForFeature(int i);
	static int segIdForBonus(int i);
	static int segIdForImprovement(int i);
	static int segIdForBuilding(int i);
};

#endif // CV_CASCADE_DEPOSIT_INDEX_H
