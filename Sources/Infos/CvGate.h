#pragma once
#ifndef CV_GATE_H
#define CV_GATE_H

//
//	CvGate -- the ENTITY-LEVEL `enabled`/`disabled` applicability pair as ONE composable unit (json.md §3.9
//	applied at entity level; owner ruling 2026-07-08: `enabled: GAMEOPTION_SIZE_MATTERS` "is literally what it is
//	supposed to be" -- the loadPrune invention's replacement). The whole entity applies only while `enabled` holds
//	and `disabled` does not (enabled read first, disabled overrides -- §3.9). Same condition vocabulary as
//	everything else; the evaluator resolves it wherever the entity's applicability is checked (a promotion's
//	acquire gate, a culture level's validity, …). Composed BY VALUE on the derived infos that author it
//	(promotions, unitcombats, promotionlines, culturelevels, units). WRITE-ONCE AT LOAD. Owns its trees.
//

#include "CvCondition.h"

namespace picojson { class value; }

class CvGate
{
public:
	CvCondition* enabled;    // applies only while this holds  -- NULL = always-on
	CvCondition* disabled;   // suppressed while this holds    -- NULL = never-suppressed

	CvGate() : enabled(NULL), disabled(NULL) {}
	~CvGate();

	// The unit's load-time writers: parse the entity's top-level "enabled" / "disabled" value (a condition --
	// bare GAMEOPTION_X string, atom, or all/any/noneOf tree) through the ONE condition boundary.
	void parseEnabled(const picojson::value& v);
	void parseDisabled(const picojson::value& v);

	bool isEmpty() const { return enabled == NULL && disabled == NULL; }
	void clearParsed();   // frees the trees + resets (the dtor body; the clear-first half of the section re-map)

private:
	CvGate(const CvGate&);            // noncopyable -- owns the condition trees
	CvGate& operator=(const CvGate&);
};

#endif // CV_GATE_H
