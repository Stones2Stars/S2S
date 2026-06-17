// readjson.cpp — isolated #428 JSON CONFORMANCE harness (step 1, v2).
//
// PURPOSE: build the JSON parsing in ISOLATION (no game launch), exhaustively against the
// data-model-spec, so it ports into the mod's readJson later with high confidence every case
// is covered. (1) proves the vendored picojson ingests the WHOLE Assets/Data set; (2) walks the
// cascade sections (requires / enables-family / grants / modifier families) against the grammar
// and FLAGS anything it does not recognize — drive the flags + unknown buckets to a reviewed set.
//
// CONSTRAINTS (despair #13 — the toolchain is genuinely 2003): STRICT C++03 / VC7.1. No auto /
// range-for / nullptr / lambdas / std::filesystem / C++11 library. Win32 FindFirstFile + the
// vendored Sources/include/picojson.h.
//
//   build: Sources/ReadJson/build.ps1   ->   Sources/ReadJson/readjson.exe
//   run:   readjson.exe [Assets/Data]   (default "Assets/Data", run from repo root)

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>
// picojson's SNPRINTF guard is `_MSC_VER > 1310 ? _snprintf_s : snprintf`; VC7.1 is EXACTLY 1310,
// so it falls to bare `snprintf` (no C99 in 2003). The DLL gets it from its Boost/Python chain;
// this isolated app pulls none of that in, so shim it to the MS CRT `_snprintf`.
#if defined(_MSC_VER) && _MSC_VER <= 1310
#define snprintf _snprintf
#endif
#include "picojson.h"

// ===================== known vocabulary (data-model-spec §1/§2) =====================
static const char* RESERVED_SECTIONS[] = {
    "type", "text", "description", "help", "civilopedia", "message",
    "enables", "obsoletes", "replaces", "disables", "requires", "allowed", "grants",
    "cost", "ui", "world", "sound", "identity", "ai",
    "loadPrune", "policies", "succession", "excludes", "produces", "condition", "effect",
    "vision", "outcomes", "capabilities", "mapGeneration", "replacedBy", 0
};
// intrinsic / auxiliary sections — NOT read by the 3 machines; light-touch (no grammar check). Includes the
// settled TEXT fields (→ identity, taxonomy decision A) and the enumerated BESPOKE entity sections (decision C).
static const char* INTRINSIC[] = {
    "type", "text", "description", "help", "civilopedia", "message", "quote", "strategy", "adjective", "shortDescription",
    "cost", "ui", "world", "sound", "identity", "ai",
    "loadPrune", "policies", "succession", "excludes", "produces", "condition", "effect",
    "vision", "outcomes", "mapGeneration", "replacedBy",
    // capabilities (TEAM, tech-unlocked) vs skills (UNIT, innate) — scope encoded by the section NAME (decision B):
    "capabilities", "skills",
    // bespoke entity sections (object-valued but NOT scope-keyed modifier families):
    "promotionLine", "buildUp", "shrine", "properties", "voteSource", "threshold", "role", "victory",
    "targetLevel", "conversion", "cityFounding", "unitCapability", 0
};
static const char* SCOPES[] = {
    "world", "team", "empire", "area", "city", "plot",
    "improvement", "feature", "terrain", "route", "building", "specialist", "unit",
    "self", 0  // "self" = the entity's OWN production/construction (buildRate.self — build THIS faster); owner 2026-06-16
};
static const char* UNITS[] = { "flat", "percent", "multiplier", "postMultiplier", "rawPercent", 0 };
static const char* ATOM_KEYS[] = { "type", "scope", "min", "max", "connection", "role", "each", 0 };
static const char* PER_KEYS[] = { "type", "anyOf", "each", "scope", 0 };
static const char* ENABLES_BUCKETS[] = {
    "buildings", "units", "builds", "techs", "civics", "religions", "corporations", "projects",
    "processes", "promotions", "promotionLines", "heritages", "specialBuildings", "specialBuildingsWaived",
    "improvements", "bonuses", "routes", "votes", "hurries", "traits", "specialists", "outcomes", 0
};
static const char* PRED_BARE[] = {
    "IS_WATER", "IS_FRESHWATER", "IS_FLATLANDS", "IS_HILLS", "IS_PEAK", "HAS_RIVER", "HAS_IRRIGATION",
    "COASTAL_LAND", "IS_COASTAL", "IS_CAPITAL", "HAS_POWER", "HAS_STATE_RELIGION", "STATE_RELIGION_IN_CITY",
    "IS_CITY",
    "HAS_FEATURE", 0  // dual-mode: bare = "has ANY feature"; {HAS_FEATURE:X} (in PRED_PARAM) = "has this one" (hole #2)
};
static const char* PRED_PARAM[] = {
    "HAS_FEATURE", "HAS_TERRAIN", "HAS_BONUS", "HAS_RELIGION", "STATE_RELIGION", "HOLY_CITY",
    "HAS_CORPORATION", "latitude", "natureYield", "workedBy", "existedFor", 0
};
// {terrain|feature|bonus:[Type,...]} = membership SUGAR; desugars to any-of HAS_<KEY> (owner 2026-06-16, hole #1).
// HAS_TERRAIN/HAS_FEATURE/HAS_BONUS (in PRED_PARAM) are the canonical single-valued predicates; the list is the
// compact authoring form (improvement placement make-valid sets).
static const char* MEMBERSHIP[] = { "terrain", "feature", "bonus", 0 };
// `allowed` cap keys (owner 2026-06-17): a SCOPE (world/team/empire — self-cap "at most N of me") OR a wonder-CATEGORY
// discriminator below (per-city count cap "at most N of this category"). totalWonders = the all-encompassing aggregate
// (reserved; no source field today). Self-cap vs category-cap is told by which namespace the key is in.
static const char* ALLOWED_CATEGORIES[] = { "worldWonders", "teamWonders", "nationalWonders", "totalWonders", 0 };

static std::set<std::string> mk(const char** a) {
    std::set<std::string> s;
    for (int i = 0; a[i]; ++i) s.insert(a[i]);
    return s;
}
static std::set<std::string> S_RESERVED, S_INTRINSIC, S_SCOPES, S_UNITS, S_ATOMK, S_PERK, S_BUCKETS, S_PBARE, S_PPARAM, S_MEMBERSHIP, S_ALLOWEDCAT;
static bool has(const std::set<std::string>& s, const std::string& k) { return s.find(k) != s.end(); }

// ===================== report =====================
struct Report {
    std::map<std::string, int> folders, topKeys, families, requiresKeys, scopes, units,
        atomKeys, members, enablesBuckets, grantsKeys, predicates, connections, flagsOrText;
    std::map<std::string, int> flagKinds;     // flag message -> count (collapsed)
    std::vector<std::string>   flagSamples;    // first N concrete "file :: path :: msg"
    int parsed, failed;
    std::vector<std::string> parseErrors;
    std::string curFile;
    Report() : parsed(0), failed(0) {}
    void flag(const std::string& path, const std::string& msg) {
        flagKinds[msg] += 1;
        if (flagSamples.size() < 60) flagSamples.push_back(curFile + " :: " + path + " :: " + msg);
    }
};

static std::string sval(const picojson::value& v) { return v.is<std::string>() ? v.get<std::string>() : std::string("?"); }

// ----- conditions (requires.build/operate members, enabled, disabled) -----
static void check_condition(const picojson::value& v, const std::string& path, Report& r);

static void check_atom(const picojson::object& o, const std::string& path, Report& r) {
    if (o.find("type") == o.end()) { r.flag(path, "atom has no 'type'"); return; }
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        r.atomKeys[it->first] += 1;
        if (!has(S_ATOMK, it->first)) r.flag(path, "atom: unknown key '" + it->first + "'");
        if (it->first == "scope") { r.scopes[sval(it->second)] += 1; if (!has(S_SCOPES, sval(it->second))) r.flag(path, "atom: bad scope '" + sval(it->second) + "'"); }
        if (it->first == "connection") r.connections[sval(it->second)] += 1;
    }
}

static void check_condition(const picojson::value& v, const std::string& path, Report& r) {
    if (v.is<std::string>()) { std::string p = v.get<std::string>(); r.predicates[p] += 1; if (!has(S_PBARE, p)) r.flag(path, "unknown bare predicate '" + p + "'"); return; }
    if (v.is<bool>()) return;  // enabled:true/false
    if (!v.is<picojson::object>()) { r.flag(path, "condition: unexpected value type"); return; }
    const picojson::object& o = v.get<picojson::object>();
    bool comb = false;
    if (o.find("all") != o.end())    { comb = true; const picojson::value& a = o.find("all")->second; if (a.is<picojson::array>()) { const picojson::array& ar = a.get<picojson::array>(); for (size_t i = 0; i < ar.size(); ++i) check_condition(ar[i], path + ".all", r); } else r.flag(path, "'all' not array"); }
    if (o.find("noneOf") != o.end()) { comb = true; const picojson::value& a = o.find("noneOf")->second; if (a.is<picojson::array>()) { const picojson::array& ar = a.get<picojson::array>(); for (size_t i = 0; i < ar.size(); ++i) check_condition(ar[i], path + ".noneOf", r); } else r.flag(path, "'noneOf' not array"); }
    if (o.find("any") != o.end())    { comb = true; const picojson::value& a = o.find("any")->second;
        if (a.is<picojson::array>()) { const picojson::array& groups = a.get<picojson::array>();
            for (size_t g = 0; g < groups.size(); ++g) {
                if (groups[g].is<picojson::array>()) { const picojson::array& grp = groups[g].get<picojson::array>(); for (size_t i = 0; i < grp.size(); ++i) check_condition(grp[i], path + ".any", r); }
                else check_condition(groups[g], path + ".any", r);  // tolerate a flat any
            }
        } else r.flag(path, "'any' not array"); }
    if (o.find("enabled") != o.end())  { comb = true; check_condition(o.find("enabled")->second, path + ".enabled", r); }
    if (o.find("disabled") != o.end()) { comb = true; check_condition(o.find("disabled")->second, path + ".disabled", r); }
    if (comb) return;
    if (o.find("type") != o.end()) { check_atom(o, path, r); return; }
    // else: a parameterized predicate {PRED: param}
    if (o.size() == 1) {
        std::string k = o.begin()->first;
        if (has(S_MEMBERSHIP, k)) {  // {terrain|feature|bonus:[...]} membership sugar = any-of HAS_<KEY> (#1, owner 2026-06-16)
            if (!o.begin()->second.is<picojson::array>()) r.flag(path, "membership '" + k + "' not array");
            return;
        }
        r.predicates[k] += 1; if (!has(S_PPARAM, k)) r.flag(path, "unknown predicate object '" + k + "'"); return;
    }
    r.flag(path, "unrecognized condition object");
}

static void check_requires(const picojson::value& v, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "requires not object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        r.requiresKeys[it->first] += 1;
        if (it->first != "build" && it->first != "operate") r.flag(path, "requires: unknown sub '" + it->first + "'");
        check_condition(it->second, path + "." + it->first, r);
    }
}

static void check_enables(const picojson::value& v, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "enables-family not object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        r.enablesBuckets[it->first] += 1;
        if (!has(S_BUCKETS, it->first)) r.flag(path, "enables: unknown bucket '" + it->first + "'");
        if (!it->second.is<picojson::array>()) r.flag(path + "." + it->first, "enables bucket not array");
    }
}

// ----- allowed: the declarative instance/category CAP (owner 2026-06-17) -----
// `allowed:{<scope>:N}` self-cap (scope key) OR `allowed:{<category>:N}` per-city count cap (wonder-category key).
// Each value is a plain count (number). enabler-spec §5/§13.7.
static void check_allowed(const picojson::value& v, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "allowed not object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        bool isScope = has(S_SCOPES, it->first), isCat = has(S_ALLOWEDCAT, it->first);
        if (isScope) r.scopes[it->first] += 1;
        if (!isScope && !isCat) r.flag(path, "allowed: key not a scope or wonder-category '" + it->first + "'");
        if (!it->second.is<double>()) r.flag(path, "allowed: value not a count for '" + it->first + "'");
    }
}

// ----- modifier families: scope -> [member/target ...] -> unit -> value/entry -----
static void check_per(const picojson::value& v, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "per not object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        if (!has(S_PERK, it->first)) r.flag(path, "per: unknown key '" + it->first + "'");
        if (it->first == "scope") r.scopes[sval(it->second)] += 1;
    }
}
static void check_value(const picojson::value& v, const std::string& path, Report& r);
static void check_deposit(const picojson::value& v, const std::string& path, Report& r) {
    if (v.is<double>() || v.is<bool>()) return;
    if (v.is<picojson::array>()) { const picojson::array& a = v.get<picojson::array>(); for (size_t i = 0; i < a.size(); ++i) check_deposit(a[i], path, r); return; }
    if (!v.is<picojson::object>()) { r.flag(path, "deposit: unexpected value type"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        const std::string& k = it->first;
        if (has(S_UNITS, k))               { r.units[k] += 1; check_value(it->second, path + "." + k, r); }
        else if (k == "ai")                check_deposit(it->second, path + ".ai", r);
        else if (k == "per")               check_per(it->second, path, r);
        else if (k == "enabled" || k == "disabled") check_condition(it->second, path + "." + k, r);
        else if (k == "value")             { /* entry magnitude */ }
        else                               { r.members[k] += 1; check_deposit(it->second, path + "." + k, r); }  // member / targetType / TYPE
    }
}
static void check_value(const picojson::value& v, const std::string& path, Report& r) {
    if (v.is<double>() || v.is<bool>()) return;
    if (v.is<picojson::array>()) { const picojson::array& a = v.get<picojson::array>(); for (size_t i = 0; i < a.size(); ++i) check_value(a[i], path, r); return; }
    if (!v.is<picojson::object>()) { r.flag(path, "unit value: unexpected type"); return; }
    const picojson::object& o = v.get<picojson::object>();   // an entry: {value, per, enabled, disabled, ai}
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        const std::string& k = it->first;
        if (k == "value")                  { /* number */ }
        else if (k == "per")               check_per(it->second, path, r);
        else if (k == "enabled" || k == "disabled") check_condition(it->second, path + "." + k, r);
        else if (k == "ai")                check_deposit(it->second, path + ".ai", r);
        else if (has(S_UNITS, k))          { r.units[k] += 1; check_value(it->second, path + "." + k, r); }
        else                               r.flag(path, "entry: unknown key '" + k + "'");
    }
}
static void check_family(const picojson::value& v, const std::string& fam, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "family '" + fam + "' value not an object (looks like a mis-homed flag?)"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        if (has(S_SCOPES, it->first)) { r.scopes[it->first] += 1; check_deposit(it->second, path + "." + it->first, r); }
        else r.flag(path, "family '" + fam + "' first key not a scope: '" + it->first + "'");
    }
}

// ----- grants (light: lists / pulses / repeatable / foundBuildings / outcomes) -----
static void check_grants(const picojson::value& v, const std::string& path, Report& r) {
    if (!v.is<picojson::object>()) { r.flag(path, "grants not object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        r.grantsKeys[it->first] += 1;
        if (it->first == "repeatable" && it->second.is<picojson::array>()) {
            const picojson::array& a = it->second.get<picojson::array>();
            for (size_t i = 0; i < a.size(); ++i) if (a[i].is<picojson::object>()) {
                const picojson::object& e = a[i].get<picojson::object>();
                if (e.find("enabled") != e.end()) check_condition(e.find("enabled")->second, path + ".repeatable.enabled", r);
            }
        }
    }
}

// ===================== clear-text RENDER (the litmus test) =====================
// Prove the parse is complete + translatable: take an entity and emit readable English for what it
// enables / requires (the conditionals) / modifies / grants. The runtime readJson instead translates
// the SAME conditional structure into a BoolExpr tree (the engine-side evaluator we reuse); this is
// the offline text equivalent.
static std::string num(const picojson::value& v) {
    if (v.is<double>()) { char b[64]; double d = v.get<double>(); double t; if (std::modf(d, &t) == 0.0) std::sprintf(b, "%.0f", d); else std::sprintf(b, "%g", d); return b; }
    if (v.is<bool>()) return v.get<bool>() ? "true" : "false";
    if (v.is<std::string>()) return v.get<std::string>();
    return "?";
}
static std::string join(const std::vector<std::string>& p, const char* sep) { std::string s; for (size_t i = 0; i < p.size(); ++i) { if (i) s += sep; s += p[i]; } return s; }
static const picojson::value* mget(const picojson::object& o, const char* k) { picojson::object::const_iterator it = o.find(k); return it == o.end() ? 0 : &it->second; }

static std::string render_atom(const picojson::object& o) {
    std::string out = mget(o, "type") ? sval(*mget(o, "type")) : "?";
    if (mget(o, "scope")) out += " @" + sval(*mget(o, "scope"));
    if (mget(o, "min")) out += " >=" + num(*mget(o, "min"));
    if (mget(o, "max")) out += " <=" + num(*mget(o, "max"));
    if (mget(o, "connection")) out += " via " + sval(*mget(o, "connection"));
    return out;
}
static std::string render_cond(const picojson::value& v) {
    if (v.is<std::string>()) return v.get<std::string>();                  // bare predicate
    if (v.is<bool>()) return v.get<bool>() ? "always" : "never";
    if (!v.is<picojson::object>()) return "?";
    const picojson::object& o = v.get<picojson::object>();
    std::vector<std::string> conj;
    if (mget(o, "all") && mget(o, "all")->is<picojson::array>()) { const picojson::array& a = mget(o, "all")->get<picojson::array>(); std::vector<std::string> p; for (size_t i = 0; i < a.size(); ++i) p.push_back(render_cond(a[i])); conj.push_back(p.size() <= 1 ? join(p, "") : "(" + join(p, " AND ") + ")"); }
    if (mget(o, "any") && mget(o, "any")->is<picojson::array>()) { const picojson::array& gs = mget(o, "any")->get<picojson::array>(); for (size_t g = 0; g < gs.size(); ++g) { std::vector<std::string> p; if (gs[g].is<picojson::array>()) { const picojson::array& grp = gs[g].get<picojson::array>(); for (size_t i = 0; i < grp.size(); ++i) p.push_back(render_cond(grp[i])); } else p.push_back(render_cond(gs[g])); conj.push_back("(one of: " + join(p, ", ") + ")"); } }
    if (mget(o, "noneOf") && mget(o, "noneOf")->is<picojson::array>()) { const picojson::array& a = mget(o, "noneOf")->get<picojson::array>(); std::vector<std::string> p; for (size_t i = 0; i < a.size(); ++i) p.push_back(render_cond(a[i])); conj.push_back("NONE of (" + join(p, ", ") + ")"); }
    if (mget(o, "disabled")) conj.push_back("disabled while " + render_cond(*mget(o, "disabled")));
    if (mget(o, "enabled")) conj.push_back("only while " + render_cond(*mget(o, "enabled")));
    if (!conj.empty()) return join(conj, " AND ");
    if (mget(o, "type")) return render_atom(o);
    if (o.size() == 1) {   // {PRED: param} — param may be a value, an object {min:N}, or a membership list [Type,...]
        const std::string& k = o.begin()->first;
        const picojson::value& p = o.begin()->second;
        if (p.is<picojson::array>()) {   // {terrain|feature|bonus:[...]} membership sugar
            const picojson::array& a = p.get<picojson::array>();
            std::vector<std::string> items; for (size_t i = 0; i < a.size(); ++i) items.push_back(sval(a[i]));
            return k + " one of [" + join(items, ", ") + "]";
        }
        if (p.is<picojson::object>()) {
            const picojson::object& po = p.get<picojson::object>();
            std::string s = k;
            if (mget(po, "min")) s += " >=" + num(*mget(po, "min"));
            if (mget(po, "max")) s += " <=" + num(*mget(po, "max"));
            return s;
        }
        return k + " " + num(p);
    }
    return "?";
}
// flatten a modifier family to readable "scope.member.unit=value [while cond]" leaves
static void flatten(const picojson::value& v, const std::string& pre, const std::string& cond, std::vector<std::string>& out) {
    if (v.is<double>() || v.is<bool>()) { out.push_back(pre + "=" + num(v) + (cond.empty() ? "" : " [while " + cond + "]")); return; }
    if (v.is<picojson::array>()) { const picojson::array& a = v.get<picojson::array>(); for (size_t i = 0; i < a.size(); ++i) flatten(a[i], pre, cond, out); return; }
    if (!v.is<picojson::object>()) return;
    const picojson::object& o = v.get<picojson::object>();
    std::string c = cond;
    if (mget(o, "enabled")) c = render_cond(*mget(o, "enabled"));
    std::string per;
    if (mget(o, "per") && mget(o, "per")->is<picojson::object>()) { const picojson::object& po = mget(o, "per")->get<picojson::object>(); per = " per " + (mget(po, "type") ? sval(*mget(po, "type")) : std::string("set")); }
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        if (it->first == "enabled" || it->first == "disabled" || it->first == "per") continue;
        if (it->first == "value") { out.push_back(pre + "=" + num(it->second) + per + (c.empty() ? "" : " [while " + c + "]")); continue; }
        flatten(it->second, pre.empty() ? it->first : pre + "." + it->first, c, out);
    }
}
static void render_entity(const picojson::value& v) {
    if (!v.is<picojson::object>()) { std::printf("(not an object)\n"); return; }
    const picojson::object& o = v.get<picojson::object>();
    std::string type = mget(o, "type") ? sval(*mget(o, "type")) : "?";
    std::printf("\n================ %s ================\n", type.c_str());
    const char* famEdges[] = { "enables", "obsoletes", "replaces", "disables", 0 };
    for (int fi = 0; famEdges[fi]; ++fi) {
        const picojson::value* e = mget(o, famEdges[fi]);
        if (!e || !e->is<picojson::object>()) continue;
        const picojson::object& b = e->get<picojson::object>();
        for (picojson::object::const_iterator it = b.begin(); it != b.end(); ++it) {
            std::vector<std::string> list;
            if (it->second.is<picojson::array>()) { const picojson::array& a = it->second.get<picojson::array>(); for (size_t i = 0; i < a.size(); ++i) list.push_back(sval(a[i])); }
            std::printf("  %s %s: %s\n", famEdges[fi], it->first.c_str(), join(list, ", ").c_str());
        }
    }
    const picojson::value* rq = mget(o, "requires");
    if (rq && rq->is<picojson::object>()) { const picojson::object& ro = rq->get<picojson::object>(); for (picojson::object::const_iterator it = ro.begin(); it != ro.end(); ++it) std::printf("  requires to %s: %s\n", it->first.c_str(), render_cond(it->second).c_str()); }
    const picojson::value* al = mget(o, "allowed");
    if (al && al->is<picojson::object>()) { const picojson::object& ao = al->get<picojson::object>(); for (picojson::object::const_iterator it = ao.begin(); it != ao.end(); ++it) std::printf("  allowed %s %s\n", num(it->second).c_str(), it->first.c_str()); }
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        if (has(S_INTRINSIC, it->first) || it->first == "enables" || it->first == "obsoletes" || it->first == "replaces" || it->first == "disables" || it->first == "requires" || it->first == "allowed" || it->first == "grants") continue;
        std::vector<std::string> leaves; flatten(it->second, "", "", leaves);
        std::printf("  modifier %s: %s\n", it->first.c_str(), join(leaves, "; ").c_str());
    }
    const picojson::value* g = mget(o, "grants");
    if (g && g->is<picojson::object>()) { const picojson::object& go = g->get<picojson::object>(); std::vector<std::string> gk; for (picojson::object::const_iterator it = go.begin(); it != go.end(); ++it) gk.push_back(it->first); std::printf("  grants: %s\n", join(gk, ", ").c_str()); }
}
// complexity proxy: total keys at all depths (richest entity)
static int complexity(const picojson::value& v) {
    if (v.is<picojson::object>()) { const picojson::object& o = v.get<picojson::object>(); int n = (int)o.size(); for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) n += complexity(it->second); return n; }
    if (v.is<picojson::array>()) { const picojson::array& a = v.get<picojson::array>(); int n = 0; for (size_t i = 0; i < a.size(); ++i) n += complexity(a[i]); return n; }
    return 0;
}

// ===================== walk an entity =====================
static void walk_entity(const picojson::value& v, Report& r) {
    if (!v.is<picojson::object>()) { r.flag("(root)", "entity not an object"); return; }
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        const std::string& k = it->first;
        r.topKeys[k] += 1;
        if (k == "requires")                                              check_requires(it->second, k, r);
        else if (k == "allowed")                                          check_allowed(it->second, k, r);
        else if (k == "enables" || k == "obsoletes" || k == "replaces" || k == "disables") check_enables(it->second, k, r);
        else if (k == "grants")                                           check_grants(it->second, k, r);
        else if (has(S_INTRINSIC, k))                                     { /* intrinsic — light touch */ }
        else if (it->second.is<picojson::object>())                       { r.families[k] += 1; check_family(it->second, k, k, r); }  // object value -> a modifier family
        else                                                              { r.flagsOrText[k] += 1; }  // bare flag/string/number -> capability (→capabilities/skills) or text (→identity), NOT a family
    }
}

// ===================== file / dir =====================
static bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return false;
    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}
static void find_json(const std::string& dir, std::vector<std::string>& out) {
    std::string pattern = dir + "\\*";
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = dir + "\\" + name;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) find_json(full, out);
        else if (name.size() > 5 && name.substr(name.size() - 5) == ".json") out.push_back(full);
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}
static std::string top_folder(const std::string& root, const std::string& path) {
    std::string rel = path.substr(root.size());
    std::string::size_type a = rel.find_first_not_of("\\/");
    if (a == std::string::npos) return "(root)";
    std::string::size_type b = rel.find_first_of("\\/", a);
    if (b == std::string::npos) return "(root)";
    return rel.substr(a, b - a);
}

// ===================== output =====================
static void print_map(const char* title, const std::map<std::string, int>& m) {
    std::printf("\n=== %s (%u) ===\n", title, (unsigned)m.size());
    for (std::map<std::string, int>::const_iterator it = m.begin(); it != m.end(); ++it)
        std::printf("  %7d  %s\n", it->second, it->first.c_str());
}

int main(int argc, char** argv) {
    S_RESERVED = mk(RESERVED_SECTIONS); S_INTRINSIC = mk(INTRINSIC); S_SCOPES = mk(SCOPES);
    S_UNITS = mk(UNITS); S_ATOMK = mk(ATOM_KEYS); S_PERK = mk(PER_KEYS); S_BUCKETS = mk(ENABLES_BUCKETS);
    S_PBARE = mk(PRED_BARE); S_PPARAM = mk(PRED_PARAM); S_MEMBERSHIP = mk(MEMBERSHIP); S_ALLOWEDCAT = mk(ALLOWED_CATEGORIES);

    std::string root = "Assets/Data", renderType; bool doComplex = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--render" && i + 1 < argc) renderType = argv[++i];
        else if (a == "--complex") doComplex = true;
        else if (!a.empty() && a[0] != '-') root = a;
    }
    std::vector<std::string> files; find_json(root, files);
    if (files.empty()) { std::printf("NO FILES — run from repo root, or pass the data dir.\n"); return 2; }

    // ---- RENDER mode (the litmus test): clear-text for one entity ----
    if (!renderType.empty() || doComplex) {
        std::string bestFile; int bestScore = -1;
        for (size_t i = 0; i < files.size(); ++i) {
            std::string text; if (!read_file(files[i], text)) continue;
            picojson::value v; if (!picojson::parse(v, text).empty() || !v.is<picojson::object>()) continue;
            std::string type = mget(v.get<picojson::object>(), "type") ? sval(*mget(v.get<picojson::object>(), "type")) : "";
            if (!renderType.empty()) { if (type == renderType) { render_entity(v); return 0; } }
            else { int s = complexity(v); if (s > bestScore) { bestScore = s; bestFile = files[i]; } }
        }
        if (!renderType.empty()) { std::printf("type '%s' not found under %s\n", renderType.c_str(), root.c_str()); return 2; }
        std::string text; read_file(bestFile, text); picojson::value v; picojson::parse(v, text);
        std::printf("MOST COMPLEX entity (score=%d): %s", bestScore, bestFile.c_str());
        render_entity(v);
        return 0;
    }

    std::printf("readjson conformance harness (#428 step 1, v2) — root=\"%s\"\n", root.c_str());
    std::printf("found %u JSON files\n", (unsigned)files.size());

    Report r;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string text;
        if (!read_file(files[i], text)) { r.failed += 1; r.parseErrors.push_back(files[i] + " (read error)"); continue; }
        picojson::value v; std::string err = picojson::parse(v, text);
        if (!err.empty()) { r.failed += 1; r.parseErrors.push_back(files[i] + " (" + err + ")"); continue; }
        r.parsed += 1; r.folders[top_folder(root, files[i])] += 1;
        r.curFile = top_folder(root, files[i]) + "/" + (v.is<picojson::object>() && v.get<picojson::object>().find("type") != v.get<picojson::object>().end() ? sval(v.get<picojson::object>().find("type")->second) : std::string("?"));
        walk_entity(v, r);
    }

    std::printf("\n========== PARSE ==========\nparsed OK : %d\nFAILED    : %d\n", r.parsed, r.failed);
    for (size_t i = 0; i < r.parseErrors.size() && i < 40; ++i) std::printf("  FAIL  %s\n", r.parseErrors[i].c_str());

    print_map("entities per folder", r.folders);
    print_map("FAMILIES (top-level, non-reserved, object-valued)", r.families);
    print_map("BARE flags/text (top-level non-object -> capabilities/skills or identity)", r.flagsOrText);
    print_map("requires sub-keys", r.requiresKeys);
    print_map("scopes (in context)", r.scopes);
    print_map("units (at leaves)", r.units);
    print_map("predicates (condition context)", r.predicates);
    print_map("atom keys", r.atomKeys);
    print_map("connection values", r.connections);
    print_map("enables-family buckets", r.enablesBuckets);
    print_map("grants keys", r.grantsKeys);
    print_map("family members / targetTypes / Types", r.members);

    std::printf("\n========== CONFORMANCE FLAGS (kinds=%u) ==========\n", (unsigned)r.flagKinds.size());
    for (std::map<std::string, int>::const_iterator it = r.flagKinds.begin(); it != r.flagKinds.end(); ++it)
        std::printf("  %7d  %s\n", it->second, it->first.c_str());
    std::printf("--- samples (first %u) ---\n", (unsigned)r.flagSamples.size());
    for (size_t i = 0; i < r.flagSamples.size(); ++i) std::printf("  %s\n", r.flagSamples[i].c_str());

    int totalFlags = 0; for (std::map<std::string, int>::const_iterator it = r.flagKinds.begin(); it != r.flagKinds.end(); ++it) totalFlags += it->second;
    std::printf("\n========== SUMMARY ==========\n");
    std::printf("files=%u parsed=%d failed=%d | families=%u | flag-kinds=%u total-flags=%d\n",
                (unsigned)files.size(), r.parsed, r.failed, (unsigned)r.families.size(),
                (unsigned)r.flagKinds.size(), totalFlags);
    std::printf("GOAL: review FAMILIES + drive FLAGS to a reviewed set; then the parser ports into the mod's readJson.\n");
    return (r.failed == 0 && totalFlags == 0) ? 0 : 1;
}
