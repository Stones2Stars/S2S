#pragma once
#ifndef CV_ENABLER_OVERLAY_H
#define CV_ENABLER_OVERLAY_H

//
//	EnablerOverlay -- the AS-IF-HELD overlay (enabler.md par.8). A CALLER-OWNED scratch that answers "what would
//	be in the tree if I ALSO held these things", by folding hypothetical HAVE sources' edges over the maintained
//	membership planes and re-applying the par.7.1 formula.
//
//	⛔ IT NEVER WRITES A DOMAIN. The overlay lives in the caller's own scratch and every read takes the domain by
//	const reference. A hypothetical that mutated the maintained planes would leave the real frontier describing a
//	game state that never happened -- and with no self-heal anywhere (docs/cascade.md §A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT) nothing would put it
//	back. That is the whole reason the raw membership reads (enableCount/removeCount) are public.
//
//	WHO ASKS. Any hypothetical asker (enabler.md par.8 -- one overlay implementation, never a second); the live
//	consumer is the civic what-if's MEMBERSHIP half (CvPlayerAI's civic-value building leg). A future asker adds
//	the read it needs in the same change as its consumer -- "what does this tech ENABLE" is deliberately not one
//	of them: that is a forward edge fetch off the tech's own compiled `enables`, never an overlay question
//	(AGENTS.md par. AI valuation of ENABLEMENT).
//
//	⛔ WHAT IT DOES NOT ANSWER -- THE BONUS AXIS. A bonus NEVER drives tree membership (enabler.md par.8, resolved
//	forks: the BONUS axis is GATE-ONLY): its dependents ROOT in the tree and sit visible-GREYED on the bonus
//	requirement, so "would this bonus let me build X" is a REQUIRES-GATE question, not a membership one, and this
//	class will answer it `false` for every candidate -- correctly, because the bonus adds no enable edge. The gate
//	twin re-evaluates the candidate's `requires` with the bonus injected into the eval ctx; it is a different
//	mechanism and is NOT to be bolted on here by making bonuses contribute enable counts, which would put a
//	membership meaning on an axis the ruling says has none.
//
//	COST. One addHave() per hypothetical source (an edge-family walk, the same one the maintained appliers do);
//	every read is then a set probe + the formula. It is a COLD path by construction -- a caller asking a
//	what-if -- and never sits on a turn path.
//

#include "Enabler/CvEnablerKernel.h"   // EnBucketSets + accumHave -- the ONE source-side edge walk
#include "CvEdges.h"                   // EnEdgeBucket -- the interned bucket vocabulary

class CvInfo;
class EnablerDomain;

class EnablerOverlay
{
public:
	EnablerOverlay() {}

	// Fold ONE hypothetical HAVE source's edges into the scratch: its `enables` targets become hypothetical
	// enable contributions, its obsoletes/replaces/disables targets hypothetical removals. Routed through the
	// kernel's accumHave so the overlay and the maintained appliers read the SAME edges
	// (docs/architecture/patterns.md §DRY (single implementation)) -- an overlay walking its own edge set would answer for a tree the real
	// appliers do not build. The source's own bucket + id are required so it also registers as hypothetically
	// HELD (below); pass NO_EDGEB/-1 for a source that sits in no enabler domain.
	//
	// ⛔ A BONUS SOURCE IS REFUSED, and the refusal is the point rather than a caller convenience. The curator
	// DOES author bonus `enables` edges (enabler.md par.8: the reverse-mapped view of the target's retained
	// `requires` atom), so accumHave would fold them happily -- but the RUNTIME never counts them, because the
	// bonus axis is GATE-ONLY. Folding them would give the "with" side an edge class the maintained "without"
	// side has never had, and every HIDDEN candidate whose inbound edge is that bonus would report as newly
	// unlocked when acquiring the bonus in fact changes no membership at all. Refusing here makes that
	// unrepresentable instead of leaving it to a comment nobody reads at the call site.
	void addHave(const CvInfo* pSource, EnEdgeBucket eSourceBucket, int iSourceId);

	// Mark a candidate as hypothetically HELD -- what a source that is ITSELF in the asked-about domain does to
	// its own slot (finishing a tech takes that tech out of the researchable frontier). Membership excludes a
	// held candidate, so without this a hypothetically-finished tech would still read as offerable. addHave
	// registers it for its own source automatically; this is for a caller composing a HAVE it has no info for.
	void setHeld(EnEdgeBucket eBucket, int iId);

	void clear();
	bool isEmpty() const;

	// ---- the read: the par.7.1 formula over (maintained + overlay) ----

	// In the tree GIVEN the hypothetical.
	bool inTree(const EnablerDomain& kDomain, EnEdgeBucket eBucket, int iId) const;

private:
	// A SET per bucket, not a count -- and that is exact rather than a simplification: the formula only ever
	// tests `enable > 0` and `remove == 0`, so a hypothetical contributing "at least one" is all the arithmetic
	// can observe. Two hypothetical sources enabling the same target cannot change either test, which is why
	// reusing the kernel's EnBucketSets (what accumHave already fills) loses nothing.
	EnBucketSets m_enables;
	EnBucketSets m_removes;
	EnBucketSets m_held;
};

#endif // CV_ENABLER_OVERLAY_H
