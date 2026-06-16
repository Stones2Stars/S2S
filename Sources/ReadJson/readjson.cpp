// readjson.cpp — isolated #428 JSON-parsing harness (step 1).
//
// PURPOSE: build the JSON parsing in ISOLATION (no game launch), exhaustively against the
// data-model-spec structure, so it can be ported into the mod's readJson later with high
// confidence every case is covered. It (1) proves the vendored picojson ingests the WHOLE
// Assets/Data set, and (2) censuses the structure so any shape the parser does not yet
// recognize is visible (drive the "unknown" buckets to zero, then port).
//
// CONSTRAINTS (despair #13 — the toolchain is genuinely 2003): STRICT C++03 / VC7.1. No
// auto / range-for / nullptr / lambdas / std::filesystem / C++11 library. Win32 FindFirstFile
// for the directory walk; the vendored Sources/include/picojson.h for parsing.
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
// picojson's SNPRINTF guard is `_MSC_VER > 1310 ? _snprintf_s : snprintf`; VC7.1 is EXACTLY 1310, so it
// falls to bare `snprintf` (no C99 in 2003). The DLL gets `snprintf` from its Boost/Python include chain;
// this isolated app pulls none of that in, so shim it to the MS CRT `_snprintf` directly.
#if defined(_MSC_VER) && _MSC_VER <= 1310
#define snprintf _snprintf
#endif
#include "picojson.h"

// ---- the data-model-spec vocabulary the harness knows about (data-model-spec.md §1/§2) ----
// Reserved top-level sections (§1). NB the curated data carries text fields (description/help/
// civilopedia/message) at TOP LEVEL today, not nested under `text` — both are accepted.
static const char* RESERVED_SECTIONS[] = {
    "type", "text", "description", "help", "civilopedia", "message",
    "enables", "obsoletes", "replaces", "disables", "requires", "grants",
    "cost", "ui", "world", "sound", "identity", "ai",
    "loadPrune", "policies", "succession", "excludes", "produces", "condition", "effect",
    "vision", "outcomes", "capabilities", "mapGeneration", "replacedBy",
    "revolution", "stateReligion", "shrine", "spawnRate",
    0
};
// Scopes (§2.2) — the containment spine + the plot sub-leaves.
static const char* SCOPES[] = {
    "world", "team", "empire", "area", "city", "plot",
    "improvement", "feature", "terrain", "route", "building", "specialist", "unit",
    0
};
// Modifier-magnitude units (§2.3).
static const char* UNITS[] = { "flat", "percent", "multiplier", "postMultiplier", "rawPercent", 0 };

static std::set<std::string> make_set(const char** arr) {
    std::set<std::string> s;
    for (int i = 0; arr[i] != 0; ++i) s.insert(arr[i]);
    return s;
}

// ---- file/dir helpers (Win32, C++03) ----
static bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static void find_json(const std::string& dir, std::vector<std::string>& out) {
    std::string pattern = dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = dir + "\\" + name;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            find_json(full, out);
        } else if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
            out.push_back(full);
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}

// top-level folder under the data root (for per-entity-kind counts)
static std::string top_folder(const std::string& root, const std::string& path) {
    std::string rel = path.substr(root.size());
    std::string::size_type a = rel.find_first_not_of("\\/");
    if (a == std::string::npos) return "(root)";
    std::string::size_type b = rel.find_first_of("\\/", a);
    if (b == std::string::npos) return "(root)";
    return rel.substr(a, b - a);
}

// ---- census ----
struct Census {
    std::map<std::string, int> topKeys;   // top-level keys across all entities (sections + families)
    std::map<std::string, int> allKeys;   // EVERY object key at every depth (so nothing hides)
    std::map<std::string, int> folders;   // entity count per top folder
    std::map<std::string, int> requiresKeys;  // keys directly under `requires` (expect build/operate)
    int parsed;
    int failed;
    std::vector<std::string> parseErrors;
    Census() : parsed(0), failed(0) {}
};

// recurse every object key at every depth
static void walk_keys(const picojson::value& v, Census& c) {
    if (v.is<picojson::object>()) {
        const picojson::object& o = v.get<picojson::object>();
        for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
            c.allKeys[it->first] += 1;
            walk_keys(it->second, c);
        }
    } else if (v.is<picojson::array>()) {
        const picojson::array& a = v.get<picojson::array>();
        for (size_t i = 0; i < a.size(); ++i) walk_keys(a[i], c);
    }
}

static void census_entity(const picojson::value& v, Census& c) {
    if (!v.is<picojson::object>()) return;
    const picojson::object& o = v.get<picojson::object>();
    for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it) {
        c.topKeys[it->first] += 1;
        if (it->first == "requires" && it->second.is<picojson::object>()) {
            const picojson::object& r = it->second.get<picojson::object>();
            for (picojson::object::const_iterator rit = r.begin(); rit != r.end(); ++rit)
                c.requiresKeys[rit->first] += 1;
        }
    }
    walk_keys(v, c);
}

// ---- output ----
static void print_map(const char* title, const std::map<std::string, int>& m) {
    std::printf("\n=== %s (%u distinct) ===\n", title, (unsigned)m.size());
    for (std::map<std::string, int>::const_iterator it = m.begin(); it != m.end(); ++it)
        std::printf("  %6d  %s\n", it->second, it->first.c_str());
}

// intersect a census map with a known set; report the known ones + the UNKNOWN remainder
static void classify(const char* title, const std::map<std::string, int>& m,
                     const std::set<std::string>& known) {
    std::map<std::string, int> in, out;
    for (std::map<std::string, int>::const_iterator it = m.begin(); it != m.end(); ++it) {
        if (known.count(it->first)) in[it->first] = it->second; else out[it->first] = it->second;
    }
    std::printf("\n=== %s: KNOWN (%u) ===\n", title, (unsigned)in.size());
    for (std::map<std::string, int>::const_iterator it = in.begin(); it != in.end(); ++it)
        std::printf("  %6d  %s\n", it->second, it->first.c_str());
    std::printf("--- %s: UNKNOWN / FAMILIES + flags (%u) ---\n", title, (unsigned)out.size());
    for (std::map<std::string, int>::const_iterator it = out.begin(); it != out.end(); ++it)
        std::printf("  %6d  %s\n", it->second, it->first.c_str());
}

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : "Assets/Data";
    std::set<std::string> reserved = make_set(RESERVED_SECTIONS);
    std::set<std::string> scopes = make_set(SCOPES);
    std::set<std::string> units = make_set(UNITS);

    std::vector<std::string> files;
    find_json(root, files);
    std::printf("readjson harness (#428 step 1) — root=\"%s\"\n", root.c_str());
    std::printf("found %u JSON files\n", (unsigned)files.size());
    if (files.empty()) {
        std::printf("NO FILES FOUND — run from the repo root, or pass the data dir as argv[1].\n");
        return 2;
    }

    Census c;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string text;
        if (!read_file(files[i], text)) {
            c.failed += 1;
            c.parseErrors.push_back(files[i] + "  (could not read file)");
            continue;
        }
        picojson::value v;
        std::string err = picojson::parse(v, text);
        if (!err.empty()) {
            c.failed += 1;
            c.parseErrors.push_back(files[i] + "  (" + err + ")");
            continue;
        }
        c.parsed += 1;
        c.folders[top_folder(root, files[i])] += 1;
        census_entity(v, c);
    }

    std::printf("\n========== PARSE ==========\n");
    std::printf("parsed OK : %d\n", c.parsed);
    std::printf("FAILED    : %d\n", c.failed);
    for (size_t i = 0; i < c.parseErrors.size() && i < 40; ++i)
        std::printf("  FAIL  %s\n", c.parseErrors[i].c_str());

    print_map("entities per folder", c.folders);
    classify("TOP-LEVEL keys (sections vs families)", c.topKeys, reserved);
    classify("REQUIRES sub-keys (expect only build/operate)", c.requiresKeys, scopes /*placeholder*/);
    classify("SCOPES seen anywhere", c.allKeys, scopes);
    classify("UNITS seen anywhere", c.allKeys, units);

    std::printf("\n========== SUMMARY ==========\n");
    std::printf("files=%u parsed=%d failed=%d | distinct top-level keys=%u | distinct keys (all depths)=%u\n",
                (unsigned)files.size(), c.parsed, c.failed,
                (unsigned)c.topKeys.size(), (unsigned)c.allKeys.size());
    std::printf("NEXT: drive the UNKNOWN buckets to a reviewed set (every family/predicate accounted for),\n"
                "      then this parser ports into the mod's readJson with the spec cases covered.\n");
    return (c.failed == 0) ? 0 : 1;
}
