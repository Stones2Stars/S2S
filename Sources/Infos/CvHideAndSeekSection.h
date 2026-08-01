#pragma once
#ifndef CV_HIDE_AND_SEEK_SECTION_H
#define CV_HIDE_AND_SEEK_SECTION_H

//
//	CvHideAndSeekSection -- the json.md par.9 `hideAndSeek` own-block as ONE composable typed section object
//	(patterns.md par. THE GETTER SETUP category 1: sections are whole typed objects). The concealment-vs-detection
//	CONTEST, gated by GAMEOPTION_COMBAT_HIDE_AND_SEEK, and the own-block sibling of CvSizeMattersSection. Units,
//	promotions and unit-combats share the exact key vocabulary, so ONE unit serves all three.
//
//	⛔ THIS IS NOT `vision`, AND THE SPLIT IS THE POINT (owner). `vision` is an ordinary modifier FAMILY answering
//	how FAR you see (strength / elevation / obstruction); this block answers whether you PERCEIVE what stands
//	inside that reach -- a graduated contest with its OWN evaluation. The legacy engine's two evaluations bled
//	into each other for years, which is precisely what expressing them as one family would let re-form. The
//	contest READS the vision budget for reach and never the reverse.
//
//	⚑ THE METHOD IS NOT IN THIS BLOCK -- it is a SKILL ([skills.md]), because a promotion can grant one (optical
//	camouflage) and a tag cannot be promotion-granted. A seeker's row names the method it answers as the ordinary
//	§3.7 `{unit: HAS_<SKILL>}` qualifier, which is why a row carries a skill id rather than a type of its own.
//
//	⚠ BEING DETECTED IS NOT A STATE (owner): vision is provided by the DETECTING unit, not empire-wide, so the
//	contest resolves live per (seeker, hider) and nothing here is a stored verdict. This block is pure authored
//	magnitudes; no member of it is a runtime flag.
//
//	Values are ×100 at parse ([DEC-fixedpoint-x100]: readJson owns the scale, the reader reduces at its point of
//	use). One open plot is VISION_PLOT authored / VISION_OPEN_GROUND_COST internally -- the SAME scale `vision`
//	speaks in, so a concealment and a sight budget are directly comparable. WRITE-ONCE AT LOAD (parse is the sole
//	writer; clear-first, idempotent under the full-registry re-map).
//

#include <string>
#include <vector>

namespace picojson { class value; }

//	One seeker row: how well this entity detects ONE hiding method. `methodSkill` is the SKILL_* info id the
//	`{unit: HAS_<SKILL>}` qualifier named. It resolves LAZILY (-1 until first asked) because the classification
//	registry mints the SKILL_* infotypes AFTER the entities parse -- the same lazy shape the IS_TAG predicate
//	already uses. A method that never minted stays -1 and simply matches nothing.
struct CvDetectionRow
{
	CvDetectionRow() : methodSkill(-1), value(0) {}

	std::string methodSkillType;   // "SKILL_NAVAL_DISGUISE" -- the minted infotype name, spelled UPPER_SNAKE
	mutable int methodSkill;       // resolved id; -1 = not yet resolved / never minted
	int value;                     // ×100; MAY BE NEGATIVE -- a negative row is counter-detection
};

class CvHideAndSeekSection
{
public:
	CvHideAndSeekSection();

	// The sole load-time writer: locate + parse the entity's `hideAndSeek` block (absent = stays empty).
	void parse(const picojson::value& entity);
	void clearParsed();

	// The block is authored iff either side carries something -- json.md §9's "a module is ON iff its block
	// exists and is non-empty", asked of this entity.
	bool isAuthored() const { return concealment != 0 || !detection.empty(); }

	// How well this entity HIDES (×100). MAY BE NEGATIVE: a negative concealment strips cover (the WANTED line).
	int concealment;

	// How well it SEEKS, per method it answers.
	std::vector<CvDetectionRow> detection;

	// Detection against ONE method (×100), summed over the rows naming it. 0 = answers that method not at all.
	// The rows sum rather than max, so counter-detection is an ordinary negative deposit.
	int detectionAgainst(int iMethodSkillId) const;

private:
	CvHideAndSeekSection(const CvHideAndSeekSection&);              // noncopyable (held by-value on a noncopyable info)
	CvHideAndSeekSection& operator=(const CvHideAndSeekSection&);
};

#endif // CV_HIDE_AND_SEEK_SECTION_H
