#pragma once
#ifndef CV_CASCADE_JSON_PARSE_H
#define CV_CASCADE_JSON_PARSE_H

//
//	CvCascadeJsonParse -- the SHARED, composable JSON parse helpers the CvJson*Info loaders reuse (audit 2026-07-01:
//	dissolving the one-function god-reader into per-type "infos load themselves"). The tiny reused primitives live here
//	so the base CvJsonInfo::mapFrom AND each per-type subclass's mapFrom draw from ONE place -- no walker is re-hand-rolled
//	per type (the CvHttpServer bolt-on anti-pattern, structural-cleanup.md §4). Behaviour is a faithful relocation of the
//	former reader's spec/StoneBase-proven logic -- NOT a rewrite (json.md; the ×100 rule is fixed-point-and-scales.md §1).
//
//	FK-resolution is CENTRALIZED here (cascadeJsonResolveId) with a load-time diagnostics accumulator, so the Orwell
//	observability bar survives the refactor: every unresolved INFOTYPE id still surfaces in the [READJSON] survey.
//

#include <string>
#include <set>
#include <map>

namespace picojson { class value; }

// The single human -> ×100 fixed-point conversion (round half away from zero). 7 -> 700, 1.5 -> 150, -10 -> -1000.
// The ONE place the human->int×100 conversion happens (determinism; DEC-fixedpoint-x100). readJson has ZERO per-field
// scale knowledge -- a BLANKET ×100 at every magnitude leaf (fixed-point-and-scales.md §1/§3.2).
int cascadeJsonX100(double h);

// FK-resolve an INFOTYPE id string via the kept type registry (GC.getInfoTypeForString). Returns the engine id, or -1
// if unresolved -- and records the unresolved string in the load-time diagnostics set (below) so it still surfaces.
int cascadeJsonResolveId(const std::string& id);

// A `{name:true}` boolean-flag object -> the set of true-valued names (the SHARED shape of skills / tags / capabilities /
// civic policies, json.md §8/§9). Only true entries are inserted; false/non-bool are ignored.
void cascadeJsonBoolSet(const picojson::value& v, std::set<std::string>& out);

// A `{channel:value}` commerce map (building stateReligionCommerce / commerceDoubleTime; religion shrine). Values are
// read as-is (already-authored ints; NOT ×100 -- these are engine-native counts, matching the former reader).
void cascadeJsonCommerceMap(const picojson::value& v, std::map<std::string, int>& out);

// Is `key` in the NULL-terminated `list`? (the shared "in this vocabulary?" test; external so callers pass a LOCAL table.)
bool cascadeJsonInList(const char** list, const std::string& key);

// --- top-level key classification (json.md §1) -- the ONE home for the reserved/intrinsic vocabulary ---
// Shared by the base CvJsonInfo::mapFrom (dispatch the cascade sections / skip the rest) and the reader's completeness
// CENSUS (prove 0 UNCLASSIFIED). An unknown OBJECT key is a modifier family; an unknown SCALAR is a flag/text -- so
// EVERY key gets a class, never "unclassified". (The intrinsic/auxiliary/classification blocks are all CJK_INTRINSIC:
// the base skips them; the owning subclass / another system parses them.)
enum CascJsonKeyClass { CJK_EDGE, CJK_PROVIDES, CJK_ALLOWED, CJK_GRANTS, CJK_REQUIRES, CJK_INTRINSIC, CJK_FAMILY, CJK_FLAG };
CascJsonKeyClass cascadeJsonClassifyKey(const std::string& key, bool valueIsObject);
const char* cascadeJsonKeyClassName(CascJsonKeyClass c);   // the census label ("edge"/"family"/"flag"/…)

// --- load-time FK diagnostics (Orwell bar) -- reset before a map, read after to surface every unresolved id ---
void cascadeJsonResetDiag();
const std::set<std::string>& cascadeJsonUnresolvedIds();

#endif // CV_CASCADE_JSON_PARSE_H
