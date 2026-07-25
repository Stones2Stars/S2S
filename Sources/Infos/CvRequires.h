#pragma once
#ifndef CV_REQUIRES_H
#define CV_REQUIRES_H

//
//	CvRequires -- the `requires` section as ONE composable unit (json.md §4.3): the build/operate condition trees
//	+ the `dormant` trigger ids. Composed BY VALUE on the derived infos that author it (buildings, builds,
//	improvements, techs, units -- the data-grounded table, owner ruling 2026-07-08: units live on the DERIVED infos,
//	never the base). WRITE-ONCE AT LOAD: `parse` is called only from the owning info's mapFrom; everything public is
//	read-only after. Owns its condition trees; noncopyable.
//

#include "CvCondition.h"
#include <vector>

namespace picojson { class value; }

class CvRequires
{
public:
	CvCondition* build;              // requires.build   (greys)             -- NULL if none
	CvCondition* operate;            // requires.operate (greys + dormancy)  -- NULL if none
	CvCondition* spread;             // requires.spread  (CORPORATIONS: needed to SPREAD into a city --
	                                     //   evaluated by the spread system at spread time, never the enabler;
	                                     //   json §4.3) -- NULL if none
	std::vector<int> dormantTriggers;    // requires.{build|operate}.dormant trigger ids (§4.3)

	CvRequires() : build(NULL), operate(NULL), spread(NULL) {}
	~CvRequires();

	// The unit's single load-time writer: parse the section's JSON value (the `requires` object). The condition
	// parser drops the structural `dormant` key, so the trigger ids are extracted separately here (json §4.3).
	void parse(const picojson::value& v);

	void clearParsed();   // frees the trees + resets (the dtor body; the clear-first half of the section re-map)

	bool isEmpty() const { return build == NULL && operate == NULL && spread == NULL && dormantTriggers.empty(); }

private:
	CvRequires(const CvRequires&);            // noncopyable -- owns the condition trees
	CvRequires& operator=(const CvRequires&);
};

#endif // CV_REQUIRES_H
