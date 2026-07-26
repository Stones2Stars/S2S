#pragma once
#ifndef CV_JSON_CONDITION_PARSE_H
#define CV_JSON_CONDITION_PARSE_H

//
//	cascadeParseCondition -- the PORT of StoneBase `Domain/Conditions/ConditionParser.cs`: the ONE human->data boundary
//	for conditions. Reads a curated-JSON value ONCE and emits the fully-typed [CvCondition] tree, normalizing
//	every human convenience (bare type-string, bare predicate, atom object, membership sugar, implied scope) + FK-
//	resolving each type/param to its engine id. After this the cascade sees only typed nodes -- never JSON (owner
//	ruling 2026-06-26). Caller OWNS the returned tree (delete it). Returns NULL only for a JSON null.
//

#include "CvCondition.h"   // CvCascPredKind -- the predicate vocabulary the spell-back mirrors

namespace picojson { class value; }

CvCondition* cascadeParseCondition(const picojson::value& v);

// The REVERSE of the predicate recognizer (kept HERE so parse and spell-back stay in lockstep in the one
// vocabulary home): the authored spelling of a predicate kind ("IS_WATER", ...). "" for UNKNOWN. Cold-path
// consumers only (the per-entry text renderer, diagnostics) -- never a runtime comparison.
const char* cascadeSpellPredKind(CvCascPredKind ePredKind);

#endif // CV_JSON_CONDITION_PARSE_H
