#pragma once
#ifndef CV_JSON_PARSE_H
#define CV_JSON_PARSE_H

//
//	CvJsonParse -- the SHARED, composable JSON parse primitives the JsonInfo layer reuses (relocated out of the
//	retired Cascade-side parse-helper home, owner ruling 2026-07-08: parsing the info data is INFO-side, not cascade --
//	[DEC-json-not-cascade]). The tiny reused primitives live here so the base CvJsonInfo::mapFrom, every section
//	UNIT (CvJsonRequires/Edges/Allowed/Grants/...), AND each per-type subclass's mapFrom draw from ONE place --
//	no walker is re-hand-rolled per type. Behaviour is a faithful relocation of the spec/StoneBase-proven logic
//	(json.md; the ×100 rule is fixed-point-and-scales.md §1).
//
//	FK-resolution is CENTRALIZED here (jsonResolveId) with a load-time diagnostics accumulator, so the Orwell
//	observability bar survives: every unresolved INFOTYPE id -- and every AUTHORED-BUT-UNCONSUMED section (a
//	section present in an entity's JSON that its info type does not compose a unit for) -- surfaces in the
//	[READJSON] survey instead of vanishing silently.
//

#include "picojson.h"          // picojson::value/object -- the walker signatures below take objects
#include "CvJsonCondition.h"   // CvCascScope -- the shared scope-token vocabulary (jsonParseScope)
#include <string>
#include <set>
#include <map>

// The single human -> ×100 fixed-point conversion (round half away from zero). 7 -> 700, 1.5 -> 150, -10 -> -1000.
// The ONE place the human->int×100 conversion happens (determinism; DEC-fixedpoint-x100). The reader has ZERO
// per-field scale knowledge -- a BLANKET ×100 at every magnitude leaf (fixed-point-and-scales.md §1/§3.2).
int jsonX100(double h);

// FK-resolve an INFOTYPE id string via the kept type registry (GC.getInfoTypeForString). Returns the engine id, or -1
// if unresolved -- and records the unresolved string in the load-time diagnostics set (below) so it still surfaces.
int jsonResolveId(const std::string& id);

// A `{name:true}` boolean-flag object -> the set of true-valued names (the SHARED shape of skills / tags / attributes /
// capabilities / civic policies, json.md §8/§9). Only true entries are inserted; false/non-bool are ignored.
void jsonBoolSet(const picojson::value& v, std::set<std::string>& out);

// A `{channel:value}` commerce map (building stateReligionCommerce / commerceDoubleTime; religion shrine). Values are
// read as-is (already-authored ints; NOT ×100 -- these are engine-native counts, matching the former reader).
void jsonCommerceMap(const picojson::value& v, std::map<std::string, int>& out);

// Is `key` in the NULL-terminated `list`? (the shared "in this vocabulary?" test; external so callers pass a LOCAL table.)
bool jsonInList(const char** list, const std::string& key);

// --- the shared JSON walkers -- the ONE canonical copy every mapFrom draws (never re-hand-rolled per type) ---
// o[key] as an object child, or NULL.
const picojson::object* jsonChildObj(const picojson::object& o, const char* key);
// the o[world][art] sub-object, or NULL -- the EXE-bound map-gen art block (icon = ART_DEF_* tag; entityEvent).
const picojson::object* jsonWorldArt(const picojson::object& o);
// entity[family][scope][unit] as a human value (0 if any hop is missing); the int form truncates the double form.
double jsonFamDbl(const picojson::object& o, const char* family, const char* scope, const char* unit);
int jsonFamVal(const picojson::object& o, const char* family, const char* scope, const char* unit);
// entity[family][scope][member][unit] (the grouped-family case, e.g. defense.plot.amount.percent).
int jsonFamMemberVal(const picojson::object& o, const char* family, const char* scope, const char* member, const char* unit);
// identity-block scalar reads: int (0 if absent), bool (false if absent), FK (-1 if absent; via jsonResolveId),
// string (out untouched if absent; returns whether the key was present as a string).
int jsonIdInt(const picojson::object& io, const char* key);
bool jsonIdBool(const picojson::object& io, const char* key);
int jsonIdFk(const picojson::object& io, const char* key);
bool jsonIdStr(const picojson::object& io, const char* key, std::string& out);
// FK-keyed int map: parent[key] = {"SPECIALIST_X"/"VICTORY_X": n} -> out[id] = n (unresolved keys surface via jsonResolveId).
void jsonReadFkMap(const picojson::object& parent, const char* key, std::map<int, int>& out);
// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map) -> out[flavorId] = n.
void jsonReadFlavours(const picojson::object& aiObj, std::map<int, int>& out);
// The §3.2 scope-token vocabulary -> CvCascScope; an unknown token falls back to the CALLER's default.
CvCascScope jsonParseScope(const std::string& s, CvCascScope defaultScope);

// --- top-level key classification (json.md §1) -- the ONE home for the reserved/intrinsic vocabulary ---
// Shared by the base CvJsonInfo::mapFrom (dispatch the section units / skip the rest) and the reader's completeness
// CENSUS (prove 0 UNCLASSIFIED). An unknown OBJECT key is a modifier family; an unknown SCALAR is a flag/text -- so
// EVERY key gets a class, never "unclassified". (The intrinsic/auxiliary/classification blocks are CJK_INTRINSIC:
// the base skips them; the owning subclass / another system parses them.) CJK_GATE is the entity-level
// `enabled`/`disabled` applicability pair (json §3.9 at entity level -- the loadPrune replacement, owner 2026-07-08).
// CJK_RETIRED is the tombstone class for purged vocabulary ("loadPrune") -- dispatched to the unconsumed census,
// NEVER parsed (superseded-ideas.md).
enum JsonKeyClass { CJK_EDGE, CJK_PROVIDES, CJK_ALLOWED, CJK_GRANTS, CJK_REQUIRES, CJK_WHEN_OBSOLETE, CJK_GATE,
                    CJK_INTRINSIC, CJK_FAMILY, CJK_FLAG, CJK_RETIRED };
JsonKeyClass jsonClassifyKey(const std::string& key, bool valueIsObject);
const char* jsonKeyClassName(JsonKeyClass c);   // the census label ("edge"/"family"/"flag"/…)

// --- load-time diagnostics (Orwell bar) -- reset before a map, read after to surface every miss ---
void jsonResetDiag();
const std::set<std::string>& jsonUnresolvedIds();
// A section authored on an entity whose info type composes NO unit for it (or a CJK_RETIRED key still in the data):
// recorded as "<typeId>:<section>" -- a representation gap that must surface, never silently drop (the exact
// "didn't pan out" gap class that bred the old-Info fallbacks).
void jsonNoteUnconsumed(const std::string& szType, const std::string& szSection);
const std::set<std::string>& jsonUnconsumedSections();

#endif // CV_JSON_PARSE_H
