#pragma once
#ifndef CV_JSON_CONDITION_PARSE_H
#define CV_JSON_CONDITION_PARSE_H

//
//	cascadeParseCondition -- the PORT of StoneBase `Domain/Conditions/ConditionParser.cs`: the ONE human->data boundary
//	for conditions. Reads a curated-JSON value ONCE and emits the fully-typed [CvJsonCondition] tree, normalizing
//	every human convenience (bare type-string, bare predicate, atom object, membership sugar, implied scope) + FK-
//	resolving each type/param to its engine id. After this the cascade sees only typed nodes -- never JSON (owner
//	ruling 2026-06-26). Caller OWNS the returned tree (delete it). Returns NULL only for a JSON null.
//

namespace picojson { class value; }
class CvJsonCondition;

CvJsonCondition* cascadeParseCondition(const picojson::value& v);

#endif // CV_JSON_CONDITION_PARSE_H
