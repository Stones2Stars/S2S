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

namespace picojson { class value; }

class CvJsonProvides
{
public:
	std::vector<int> bonuses;   // provides.bonuses -> BONUS_* FK ids

	CvJsonProvides() {}

	// The unit's single load-time writer: parse the `provides` object ({bonuses:[BONUS_ids]}).
	void parse(const picojson::value& v);

	bool isEmpty() const { return bonuses.empty(); }
	void clearParsed() { bonuses.clear(); }   // the clear-first half of the full-registry section re-map

private:
	CvJsonProvides(const CvJsonProvides&);            // noncopyable (held by-value on the noncopyable info)
	CvJsonProvides& operator=(const CvJsonProvides&);
};

#endif // CV_JSON_PROVIDES_H
