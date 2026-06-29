#pragma once
#ifndef CV_CASCADE_READJSON_H
#define CV_CASCADE_READJSON_H

//
//	CvCascadeReadJson -- the #430 cascade's FRESH, BoolExpr-routed picojson reader of the curated Assets/Data JSON
//	(build plan: docs/plans/structural-cleanup/readjson.md). It is built FROM SCRATCH, interface-bounded; it NEVER
//	threads through CvInfoUtil / the XML read() path (cascade-engine-430.md §2b/§3). It mirrors json.md + StoneBase's
//	parser (the live, spec-current model) -- NOT the frozen Tools/ReadJson C++ harness (kept only as a traversal
//	skeleton reference).
//
//	INCREMENT 1 (this file): the entity-reader SKELETON. Load every Assets/Data/<type>/*.json via picojson into a
//	fresh per-entity record, walk its top-level keys (classify reserved-section / intrinsic / modifier-family / flag,
//	json.md §1), and resolve each entity's `type` against the engine's type registry (GC.getInfoTypeForString) to
//	PROVE the curated ids map to the same indices the engine binds (the FK foundation). A one-shot [READJSON] shadow
//	summary reports it. Later increments add: the BoolExpr conditional translator (the all/any/noneOf tree), the
//	modifier-family deposit tree + the readable->x100 conversion, enables/requires/allowed, and grants.
//

//	Run the increment-1 probe ONCE (self-guarding; gated by gPlayerLogLevel so it costs nothing in normal play) --
//	parse + walk + FK-resolve the whole Assets/Data set and emit the [READJSON] summary. Hooked at CvGame::doTurn.
void cascadeReadJsonProbe();

#endif // CV_CASCADE_READJSON_H
