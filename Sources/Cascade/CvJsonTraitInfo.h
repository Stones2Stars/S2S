#pragma once
#ifndef CV_JSON_TRAIT_INFO_H
#define CV_JSON_TRAIT_INFO_H

//
//	CvJsonTraitInfo -- the per-type cascade info for TRAITS. Base: the trait's modifier families. Extensions: the
//	`negativeTrait` alignment flag (the PURE_TRAITS gate) + the `policies` this trait GRANTS (json.md §9) -- the SAME
//	pure-empire-STATE set a civic enacts (`policies` is one meaning, two grantors; a trait grants them permanently while
//	held). The two DISTINCT trait sets are CvJsonSimpleTraitInfo / CvJsonComplexTraitInfo (their ids collide; the active
//	set is chosen by GAMEOPTION_LEADER_COMPLEX_TRAITS). The cascade NEVER reads the engine CvTraitInfo for trait values
//	(its runtime CvInfoReplacements swap can't represent this clean split).
//
//	⏳ Note (owner 2026-07-01): the legacy `freeSpecialistPer{World,National,Team}Wonder` keys under a trait `policies`
//	block are EFFECTS (free specialists scaled per wonder, CvCity:5764), not pure states -> they reclassify to a
//	`freeSpecialists` modifier family via the curator; until then they ride here in `policies` harmlessly (no consumer
//	this pass). (`nonStateReligionCommerce` is VERIFIED a pure STATE -- a Free-Church permission -- so it correctly stays.)
//

#include "CvJsonInfo.h"
#include <set>

class CvJsonTraitInfo : public CvJsonInfo
{
public:
	CvJsonTraitInfo() : negativeTrait(false) {}
	bool negativeTrait;                 // StoneBase NegativeTrait -- PURE_TRAITS drops a negative trait's positive values / a positive trait's negative
	std::set<std::string> policies;     // §9 pure empire states this trait grants (permanent while held)
	virtual void mapFrom(const picojson::value& entity);
};

#endif // CV_JSON_TRAIT_INFO_H
