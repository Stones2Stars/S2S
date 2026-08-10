#pragma once

#ifndef CV_TRAIT_SELECTION_H
#define CV_TRAIT_SELECTION_H

//
//	CvTraitSelection -- the ONE consuming-system calc for "may this trait be held right now?".
//
//	CvTraitInfo serves the authored alignment (isNegativeTrait / isBarbarianSelectionOnly) and NOTHING
//	else: an info never reads game state, so a verdict composing
//	GAMEOPTION_LEADER_* cannot live there (json.md §9 -- a game option gates AT THE CONSUMING SYSTEM).
//	This class IS that consuming system, held in one place rather than re-derived per call site
//	([DEC-single-implementation]) -- the same shape as CvGameSpeedScale, and for the same reason.
//
//	It replaces the archived CvTraitInfo::isValidTrait, which was exactly that boundary violation, plus the
//	hand-inlined copies of its option composition that had accumulated across CvPlayer and CvGameTextMgr.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state
//	(patterns.md static-class law; a namespace risks VC7.1/Boost name-mangling).
//

class CvTraitInfo;
class CvLeaderHeadInfo;

class CvTraitSelection
{
public:
	// WHICH of a leader's two authored trait lists is ACTIVE, composing GAMEOPTION_LEADER_COMPLEX_TRAITS.
	//
	// The leaderhead serves both raw lists and picks neither -- an info never reads a game option -- so the
	// choice lives here, once, rather than being re-derived at each assignment site (it was inlined twice in
	// CvPlayer, which is how these drift).
	//
	// ⛔ THE ACTIVE SET ANSWERS ALONE -- no fall-through in EITHER direction (owner). A complex game does not fill
	// a simple trait because the complex one is missing, and a simple game does not reach into the complex set.
	// The two sets share no id, so either fall-through produces the mixed holding the prefix exists to make
	// impossible ([modifier.md] §4).
	// ⚠ An empty active list means the leader misses its traits. That is the state, not a case to accommodate:
	// a leader authoring only half a pair is unfinished data, and reading the other half for it would hide that.
	static const std::vector<int>& leaderTraits(const CvLeaderHeadInfo& kLeader);
	// Whether the trait may be held under the LIVE game options.
	//
	// `bGameStart` distinguishes the two asks: INITIAL assignment (a leader's starting traits, the
	// advanced-start pick) from an in-play acquisition (learning/unlearning down a developing line). It is
	// what gates the two start-only rules -- the barbarian-selection carve-out and START_NO_POSITIVE_TRAITS.
	//
	// The two live legs, in order:
	//  1. a barbarian-selection-only trait is always selectable AT START (and only there);
	//  2. ALIGNMENT vs the option pair -- NO_NEGATIVE_TRAITS drops negative traits; START_NO_POSITIVE_TRAITS
	//     drops positive ones at start (and negative ones too while LEADER_DEVELOPING is live, since the
	//     developing line is then the only intended route in).
	// ⛔ There is no third, rung-shaped leg: base-vs-developed is the ENABLER's completed tree, never a rank
	// this calc reads (a base roots on TECH_GAME_START; a rung above one needs its predecessor held).
	static bool isSelectable(const CvTraitInfo& kTrait, bool bGameStart);

private:
	CvTraitSelection();                                  // never instantiated
	CvTraitSelection(const CvTraitSelection&);
	CvTraitSelection& operator=(const CvTraitSelection&);
};

#endif // CV_TRAIT_SELECTION_H
