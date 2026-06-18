#pragma once
#ifndef CV_CASCADE_SELF_TEST_H
#define CV_CASCADE_SELF_TEST_H

//
//	⛔ TEMPORARY -- PURGE THIS (file + its call in CvGame::doTurn + the vcxproj entry) BEFORE wiring the new
//	readJson data. Owner 2026-06-17: "create a few test events and a few test tallies that use the OLD data; we
//	will purge those tests before we start wiring the new jsonread."
//
//	It validates the #430 cascade MACHINERY against the OLD/live engine data, with no dependence on the new JSON:
//	  - a few TEST EVENTS emitted through CvEventSpine (observable in Cascade.log + the live /events stream);
//	  - a few TEST TALLIES that recompute counts from live state via CvScopedAccumulator and SHADOW-compare them
//	    against the engine's maintained counts (getNumCities / getUnitCount) -- a match proves the substrate +
//	    the empire-scope roll-up against ground truth.
//	Gated by gPlayerLogLevel (free when logging is off).
//
void cascadeSelfTest();

#endif // CV_CASCADE_SELF_TEST_H
