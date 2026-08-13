#pragma once
#ifndef CV_JSON_PARSE_H
#define CV_JSON_PARSE_H

//
//	CvJsonParse -- the SHARED, composable JSON parse primitives the JsonInfo layer reuses (relocated out of the
//	retired Cascade-side parse-helper home, owner ruling 2026-07-08: parsing the info data is INFO-side, not cascade --
//	[DEC-json-not-cascade]). The tiny reused primitives live here so the base CvInfo::mapFrom, every section
//	UNIT (CvRequires/Edges/Allowed/Grants/...), AND each per-type subclass's mapFrom draw from ONE place --
//	no walker is re-hand-rolled per type. Behaviour is a faithful relocation of the spec/StoneBase-proven logic
//	(json.md; the ×100 rule is fixed-point-and-scales.md §1).
//
//	FK-resolution is CENTRALIZED here (jsonResolveId) with a load-time diagnostics accumulator, so the Orwell
//	observability bar survives: every unresolved INFOTYPE id -- and every AUTHORED-BUT-UNCONSUMED section (a
//	section present in an entity's JSON that its info type does not compose a unit for) -- surfaces in the
//	[READJSON] survey instead of vanishing silently.
//

#include "picojson.h"          // picojson::value/object -- the walker signatures below take objects
#include "CvCondition.h"   // CvCascScope -- the shared scope-token vocabulary (jsonParseScope)
#include <string>
#include <set>
#include <map>
#include <vector>

// The single human -> ×100 fixed-point conversion (round half away from zero). 7 -> 700, 1.5 -> 150, -10 -> -1000.
// The ONE place the human->int×100 conversion happens (determinism; DEC-fixedpoint-x100).
// ⛔ IT IS NOT A BLANKET, AND READING IT AS ONE IS THE COSTLIEST MISTAKE ON THIS SURFACE. It converts AMOUNTS
// only. A PERCENT IS NEVER SCALED -- mod_valueForUnit (CvModifiers.cpp) picks per LEAF from the authored key and
// routes a percent leaf straight past this function, plain. "Zero per-FIELD scale knowledge" (the curator owns
// that, DEC-curator-owns-descale) does NOT mean zero per-UNIT distinction: the unit IS the decision.
// ⚠ Consequence for every consumer: a value read with CASC_UNIT_PERCENT is already the human percent, so a
// `/100` applied to it destroys it. Believing this was a blanket is what produced a family of such divides --
// zeroed AI war declarations, culture thresholds and property decay among them. Ask the KIND's unit
// (infoKindUnit), never the family's. (fixed-point-and-scales.md §1/§3.2)
int jsonX100(double h);

// FK-resolve an INFOTYPE id string via the kept type registry (GC.getInfoTypeForString). Returns the engine id, or -1
// if unresolved -- and records the unresolved string in the load-time diagnostics set (below) so it still surfaces.
int jsonResolveId(const std::string& id);

// A `{name:true}` boolean-flag object -> the set of true-valued names (the SHARED shape of skills / tags / attributes /
// capabilities / civic policies, json.md §8/§9). Only true entries are inserted; false/non-bool are ignored.
void jsonBoolSet(const picojson::value& v, std::set<std::string>& out);

// A `{channel:value}` commerce map (the building's stateReligionCommerce marker). Values are
// read as-is (already-authored ints; NOT ×100 -- these are engine-native counts, matching the former reader).
void jsonCommerceMap(const picojson::value& v, std::map<std::string, int>& out);

// Is `key` in the NULL-terminated `list`? (the shared "in this vocabulary?" test; external so callers pass a LOCAL table.)
bool jsonInList(const char** list, const std::string& key);

// Is `iNeedle` in `haystack`? The ONE id-vector membership scan the info getters' has*/is* reads share
// (runtime-callable, allocation-free -- a per-poco private copy is the file-hidden DRY hazard patterns.md bans).
inline bool vectorHas(const std::vector<int>& haystack, int iNeedle)
{
	for (size_t i = 0; i < haystack.size(); ++i)
	{
		if (haystack[i] == iNeedle)
		{
			return true;
		}
	}
	return false;
}

// --- the shared JSON walkers -- the ONE canonical copy every mapFrom draws (never re-hand-rolled per type) ---
// o[key] as an object child, or NULL.
const picojson::object* jsonChildObj(const picojson::object& o, const char* key);
// the o[world][art] sub-object, or NULL -- the EXE-bound map-gen art block (define = ART_DEF_* tag; entityEvent).
const picojson::object* jsonWorldArt(const picojson::object& o);
// (the former jsonFamDbl/jsonFamVal/jsonFamMemberVal raw family-address probes are DELETED -- the compiled
// CvModifiers entries/sums are the ONE post-parse read surface, and the waves left the probes caller-less)
// identity-block scalar reads: int (0 if absent), bool (false if absent), FK (-1 if absent; via jsonResolveId),
// string (out untouched if absent; returns whether the key was present as a string).
// iDefault = the value an ABSENT key restores. The curator elides values equal to the LEGACY LOAD DEFAULT
// (the archived .add(member, tag, default) third argument), so a non-zero legacy default MUST be passed here
// by the poco read -- absent-reads-0 was the no-attacks combatLimit bug class.
int jsonIdInt(const picojson::object& io, const char* key, int iDefault = 0);
// An ART value, taken as authored. Art is not a cascade amount, so it carries no fixed-point scale
// ([DEC-fixedpoint-x100] governs AMOUNTS); the animation numbers are handed to the EXE in its own units.
float jsonIdFloat(const picojson::object& io, const char* key, float fDefault = 0.0f);
bool jsonIdBool(const picojson::object& io, const char* key);
int jsonIdFk(const picojson::object& io, const char* key);
bool jsonIdStr(const picojson::object& io, const char* key, std::string& out);
// FK-keyed int map: parent[key] = {"SPECIALIST_X"/"VICTORY_X": n} -> out[id] = n (unresolved keys surface via jsonResolveId).
void jsonReadFkMap(const picojson::object& parent, const char* key, std::map<int, int>& out);
// FK-id ARRAY: parent[key] = ["TYPE_A", ...] -> resolved ids APPENDED to out in authored order (non-strings
// skipped; unresolved ids surface via jsonResolveId and are not appended).
void jsonReadIdList(const picojson::object& parent, const char* key, std::vector<int>& out);
// Keyed-bool FK object: parent[key] = {"TYPE_A": true, ...} -> resolved ids of the TRUE entries APPENDED to out
// (the par.8 keyed-skill FK shape -- skills.terrainDoubleMove / featureDoubleMove; false/non-bool entries skipped).
void jsonReadKeyedBoolIdList(const picojson::object& parent, const char* key, std::vector<int>& out);
// Raw STRING array: parent[key] = ["str", ...] -> the strings APPENDED to out in authored order (non-strings
// skipped). NOT FK-resolved -- the TEXT-KEY / unique-name pools (civilization cityNames, unit uniqueNames).
// Templated over the element type so std::string and CvString containers share the ONE walker.
template <class StringType>
inline void jsonReadStrList(const picojson::object& parent, const char* szKey, std::vector<StringType>& out)
{
	picojson::object::const_iterator listIter = parent.find(szKey);
	if (listIter == parent.end() || !listIter->second.is<picojson::array>())
	{
		return;
	}
	const picojson::array& entries = listIter->second.get<picojson::array>();
	for (size_t iEntry = 0; iEntry < entries.size(); ++iEntry)
	{
		if (entries[iEntry].is<std::string>())
		{
			out.push_back(StringType(entries[iEntry].get<std::string>().c_str()));
		}
	}
}
// ai.flavours -- an ARRAY of single-key { FLAVOR_X: n } objects (NOT a map) -> out[flavorId] = n.
void jsonReadFlavours(const picojson::object& aiObj, std::map<int, int>& out);
// The sparse id-map POINT read (the read-twin of the map fills above: flavours / keyed weights / era tables).
// Runtime-callable and allocation-free -- the vectorHas precedent: a per-poco private find is the file-hidden
// DRY hazard patterns.md bans. iAbsent = the value an absent key reads (0 for the flavour/weight getters).
inline int mapValueOrDefault(const std::map<int, int>& valueMap, int iKey, int iAbsent = 0)
{
	std::map<int, int>::const_iterator valueIter = valueMap.find(iKey);
	return valueIter != valueMap.end() ? valueIter->second : iAbsent;
}
// The §3.2 scope-token vocabulary -> CvCascScope; an unknown token falls back to the CALLER's default.
CvCascScope jsonParseScope(const std::string& s, CvCascScope defaultScope);
// Is `s` one of the §3.2 scope tokens? (the address-decode discriminator: a second segment that is NOT a scope
// token stays part of the deposit's tail -- e.g. a count-by-type key directly under the family).
bool jsonIsScopeToken(const std::string& s);

// --- top-level key classification (json.md §1/§11) -- the ONE home for the reserved/intrinsic vocabulary ---
// Shared by the base CvInfo::mapFrom (dispatch the section units / skip the rest) and the reader's completeness
// CENSUS. The family vocabulary is CLOSED: a non-reserved OBJECT key classifies CJK_FAMILY only when it is in
// the known-family table (or the open PROPERTY_* plane); otherwise it is CJK_UNKNOWN -- a LOUD load error the
// reader prints unconditionally, never a silently-minted family. A non-reserved SCALAR key stays CJK_FLAG (the
// §8 classification registries are open by design). CJK_INTRINSIC is the intrinsic/auxiliary set the base
// SKIPS (the owning subclass / another system parses it); CJK_CLASSBLOCK is the §8/§9 classification set the
// base DISPATCHES itself, and its keys come from CvInfo's ONE table so the two halves cannot drift. CJK_GATE is the
// entity-level `enabled`/`disabled` applicability pair (json §3.9 at entity level). CJK_TRIGGERS is the §5
// trigger->chance->action section (array-valued; dispatched to the composing type's CvTriggers unit).
// CJK_RETIRED is the tombstone class for purged vocabulary ("loadPrune") -- dispatched to the unconsumed census,
// NEVER parsed (superseded-ideas.md).
enum JsonKeyClass { CJK_EDGE, CJK_PROVIDES, CJK_ALLOWED, CJK_GRANTS, CJK_TRIGGERS, CJK_REQUIRES, CJK_WHEN_OBSOLETE,
                    CJK_GATE, CJK_CLASSBLOCK, CJK_INTRINSIC, CJK_FAMILY, CJK_FLAG, CJK_RETIRED, CJK_UNKNOWN };
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
// A CJK_UNKNOWN top-level key (a non-reserved object key outside the closed family vocabulary): recorded as
// "<typeId>:<key>" -- the reader prints each as an unconditional [READJSON] ERROR unknown-key line.
void jsonNoteUnknownKey(const std::string& szType, const std::string& szKey);
const std::set<std::string>& jsonUnknownKeys();

#endif // CV_JSON_PARSE_H
