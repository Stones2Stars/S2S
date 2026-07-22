#pragma once
#ifndef CV_JSON_PROVIDES_H
#define CV_JSON_PROVIDES_H

//
//	CvJsonProvides -- the `provides` section as ONE composable unit (json.md §5a): the BONUS_* ids this entity
//	supplies in-vicinity WHILE ACTIVE (a herd building supplying its animal bonus). Composed BY VALUE on the derived
//	infos that author it (buildings, corporations). WRITE-ONCE AT LOAD. The enabler's operating-building fixpoint
//	unions active providers' bonuses into the city's vicinity supply.
//

#include <vector>
#include <map>

namespace picojson { class value; }

class CvJsonProvides
{
public:
	std::vector<int> bonuses;      // provides.bonuses -> BONUS_* FK ids (the presence union; count-agnostic consumers read this)
	std::map<int, int> bonusCount; // id -> supply COUNT for entries that carry one (json §5a {BONUS_X:N}); ABSENT = 1 (HOLLYWOOD etc.)

	CvJsonProvides() {}

	// The unit's single load-time writer: parse the `provides` object. `bonuses` is a list whose entry is a bare
	// BONUS_* string (count INFERRED 1) or a single-key object {BONUS_X: N} (an explicit supply count).
	void parse(const picojson::value& v);

	// The supply count for a provided bonus id (1 unless the entry carried an explicit count).
	int countOf(int iBonusId) const
	{
		std::map<int, int>::const_iterator it = bonusCount.find(iBonusId);
		return it != bonusCount.end() ? it->second : 1;
	}

	bool isEmpty() const { return bonuses.empty(); }
	void clearParsed() { bonuses.clear(); bonusCount.clear(); }   // the clear-first half of the full-registry section re-map

private:
	CvJsonProvides(const CvJsonProvides&);            // noncopyable (held by-value on the noncopyable info)
	CvJsonProvides& operator=(const CvJsonProvides&);
};

#endif // CV_JSON_PROVIDES_H
