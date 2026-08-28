#pragma once

#ifndef CyEnums_h__
#define CyEnums_h__

#include <string>

//
//	CyEnums -- the Python ENUM VOCABULARY (the engine enums a script indexes results with), plus name->id
//	resolution. Sibling of CyEnabler ("can I?") and each object's own accessor ("what do I HAVE?").
//
//	⛔ NOT the banned binding surface (docs/architecture/patterns.md §THE PYTHON READ BOUNDARY (Cy* is not a fixed contract) bans the .def GETTER contract). This publishes
//	CONSTANTS -- no reads, no getters. It is a PREREQUISITE of the new surface rather than a survival of the old
//	one: patterns.md § THE TWO READ ROLES specifies that the existing ENGINE ENUM indexes the RESULT of a group
//	read, so `CyCity::getYields()[YieldTypes.YIELD_FOOD]` is unconsumable until these types exist.
//
//	⚑ Enum operations are FIRST CLASS, covering RESOLUTION and EXTENSION (patterns.md § THE PYTHON READ
//	BOUNDARY). Resolution is `getInfoType`. EXTENSION needs no API of its own: a published boost enum is a real
//	Python type, so BUG's existing construct-from-int + setattr mechanism mints new members at runtime once the
//	type is published. ⛔ Do not add a mint verb with no caller.
//
class CyEnums
{
public:
	CyEnums() {}

	// An INFOTYPE_NAME to its engine id, -1 when unknown (the generalization of getInfoTypeForString).
	int getInfoType(const std::string& szType) const;

	// Publishes the engine enum vocabulary AND this class. Called from DLLPublishToPython.
	static void pythonPublish();
};

#endif // CyEnums_h__
