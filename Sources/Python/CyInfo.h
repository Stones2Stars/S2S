#pragma once

#ifndef CyInfo_h__
#define CyInfo_h__

#include <string>
#include "Infrastructure/IDValueMap.h"   // the bulk id->value index handed to Python in one crossing

//
//	CyInfo -- the Python INFO surface: the "what do I CARRY?" read role (patterns.md § THE TWO READ ROLES),
//	exposed to script. Sibling of CyEnabler ("can I?"), CyState ("what do I HAVE?") and CyEnums (the vocabulary).
//
//	⛔ THIS IS WHERE INFOS LIVE NOW, AND THE ONLY PLACE. The global context deliberately hands out none
//	([DEC-cy-not-fixed]): its `get<X>Info(i)` accessors returned an object carrying the whole legacy getter set,
//	which is the escape hatch the rebuild closes. A script wanting a SETTING asks the global context; a script
//	wanting ENTITY DATA asks here.
//
//	⚑ ADDRESSED BY INFOTYPE PREFIX, not by a getter per registry. `getDescription("UNIT_", id)` routes through the
//	ONE infotype-prefix -> InfoRepo dispatch the load pipeline already owns
//	([DEC-single-implementation]), so a new category is served the moment it is registered there -- no method is
//	added here, and the two cannot drift. That is the same "extensible by DATA" rule the rest of the surface obeys.
//
//	⚠ DELIBERATELY SMALL. This serves the per-type INDEX shape (pedia-read-map § shape 2: id -> name/type), which
//	is what every enumeration in script actually asks for. The per-entity PAYLOAD, the edge lists and the rendered
//	entry lines are the rest of the shape and are NOT here yet; a consumer needing one gets a loud AttributeError
//	rather than a quiet wrong answer.
//
//	BOOST: only the `python::` alias, never a bare `boost::` -- two Boosts coexist (engine.md).
//
class CyInfo
{
public:
	CyInfo() {}

	// The entity's display NAME, already localized (the info's own resolved text).
	std::wstring getDescription(const std::string& szTypePrefix, int iId) const;
	// The entity's stable TYPE KEY ("UNIT_AXEMAN") -- what a scenario serializer and a config string need.
	std::string getType(const std::string& szTypePrefix, int iId) const;
	// Whether the registry actually holds an entity at this id, so a caller can skip a hole without
	// inferring it from an empty name.
	bool exists(const std::string& szTypePrefix, int iId) const;

	// ⚑ THE BULK INDEX SHAPE -- one boundary crossing for a WHOLE id->value column, not one per entity.
	// A boost::python call costs far more than the lookup inside it, so the read that scales is the one that
	// crosses ONCE and is cached on the Python side (CivicData does exactly that). Typed end to end: no string
	// prefix, no string member name -- the id type IS the addressing.
	// The map is engine-owned and built once at first use; Python receives a reference and may only iterate it,
	// test membership, and getValue() -- it can mutate nothing.
	const IDValueMap<CivicTypes, int>& civicOptions() const;

	static void pythonPublish();
};

#endif // CyInfo_h__
