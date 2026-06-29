//
//	CvCascadeReadJson -- INCREMENT 1: the entity-reader skeleton. See CvCascadeReadJson.h for the build plan + scope.
//

#include "CvGameCoreDLL.h"          // the PCH umbrella -- brings picojson (CvGameCoreDLL.h:310), windows.h, gDLL, GC
#include "CvCascadeReadJson.h"
#include "AI/BetterBTSAI.h"         // gPlayerLogLevel + streamLogTee (the gate + the /events tee)
#include "Defines/CvGlobals.h"      // GC.getInfoTypeForString -- the type registry (FK resolution)
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>

// --- vocabulary (json.md §1/§2; the cascade-section + intrinsic/aux sets the increment-1 classification needs) ---
// Ported from the proven (now frozen) Tools/ReadJson harness -- the authoritative grammar SURFACE; deeper grammar
// (conditions, families, deposits) is parsed in later increments, not classified here.
static const char* RJ_CASCADE_SECTIONS[] = { "requires", "allowed", "enables", "obsoletes", "replaces", "disables", "grants", 0 };
static const char* RJ_INTRINSIC[] = {
	"type", "text", "description", "help", "civilopedia", "message", "quote", "strategy", "adjective", "shortDescription",
	"cost", "ui", "world", "sound", "identity", "ai",
	"loadPrune", "policies", "succession", "excludes", "produces", "condition", "effect",
	"vision", "outcomes", "mapGeneration", "replacedBy", "capabilities", "skills",
	"promotionLine", "buildUp", "shrine", "properties", "voteSource", "threshold", "role", "victory",
	"targetLevel", "conversion", "cityFounding", "unitCapability", 0
};

static bool rj_in(const char** a, const std::string& k)
{
	for (int i = 0; a[i]; ++i) if (k == a[i]) return true;
	return false;
}

// A fresh per-entity record. INCREMENT 1: type + its resolved engine index; the rich fields (families / conditions /
// deposits) land in later increments. This is the start of the cascade's own runtime store (NOT a CvInfoUtil reuse).
struct RjEntity { std::string type; int typeId; };

static bool rj_readFile(const std::string& path, std::string& out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open()) return false;
	std::ostringstream ss; ss << f.rdbuf(); out = ss.str();
	return true;
}

// Recursive *.json walk under a dir (the harness's proven find_json -- Assets/Data is per-type subdirs, so the engine's
// single-dir enumerateFiles won't reach them; a recursive Win32 walk does). Win32 only, C++03.
static void rj_find(const std::string& dir, std::vector<std::string>& out)
{
	const std::string pattern = dir + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do
	{
		const std::string name = fd.cFileName;
		if (name == "." || name == "..") continue;
		const std::string full = dir + "\\" + name;
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) rj_find(full, out);
		else if (name.size() > 5 && name.substr(name.size() - 5) == ".json") out.push_back(full);
	} while (FindNextFileA(h, &fd) != 0);
	FindClose(h);
}

void cascadeReadJsonProbe()
{
	static bool s_done = false;
	if (s_done || gPlayerLogLevel < 1)
	{
		return; // one-shot, and only while logging is on (shadow testing) -- zero cost in normal play
	}
	s_done = true;

	// Locate Assets\Data under the running mod -- the same getModName base the XML loader uses (CvXMLLoadUtility.cpp:272).
	std::string base = gDLL->getModName(true);
	if (!base.empty() && base[base.size() - 1] != '\\' && base[base.size() - 1] != '/') base += "\\";
	const std::string dataDir = base + "Assets\\Data";

	std::vector<std::string> files;
	rj_find(dataDir, files);

	int iFailed = 0, iEntities = 0, iResolved = 0, iUnresolved = 0, iShownUnres = 0;
	std::set<std::string> families, flags;
	std::vector<RjEntity> store;   // the fresh entity store (increment 1)
	char szBuf[1024];

	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string text;
		if (!rj_readFile(files[i], text)) { ++iFailed; continue; }
		picojson::value v;
		const std::string err = picojson::parse(v, text);
		if (!err.empty() || !v.is<picojson::object>()) { ++iFailed; continue; }
		const picojson::object& o = v.get<picojson::object>();

		// `type` + FK resolution against the engine registry (the increment-1 proof).
		picojson::object::const_iterator t = o.find("type");
		if (t == o.end() || !t->second.is<std::string>()) continue; // not a typed entity -- skip silently
		++iEntities;
		const std::string type = t->second.get<std::string>();
		RjEntity rec;
		rec.type = type;
		rec.typeId = GC.getInfoTypeForString(type.c_str(), true /*hideAssert*/);
		store.push_back(rec);
		if (rec.typeId >= 0) ++iResolved;
		else
		{
			++iUnresolved;
			if (iShownUnres < 16)
			{
				sprintf(szBuf, "[READJSON/unresolved] type=%s", type.c_str());
				gDLL->logMsg("Cascade.log", szBuf);
				streamLogTee(1, szBuf);
				++iShownUnres;
			}
		}

		// Top-level key classification (json.md §1): cascade section / intrinsic / object-valued => modifier family /
		// scalar => flag-or-text. Counted here; the deeper per-section parse is later increments.
		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			const std::string& k = it->first;
			if (rj_in(RJ_CASCADE_SECTIONS, k) || rj_in(RJ_INTRINSIC, k)) continue;
			if (it->second.is<picojson::object>()) families.insert(k);
			else flags.insert(k);
		}
	}

	sprintf(szBuf, "[READJSON/dir] %s", dataDir.c_str());
	gDLL->logMsg("Cascade.log", szBuf);
	streamLogTee(1, szBuf);
	sprintf(szBuf, "[READJSON/probe] files=%d parsed=%d failed=%d entities=%d resolved=%d unresolved=%d familyKinds=%d flagKinds=%d",
		(int)files.size(), (int)files.size() - iFailed, iFailed, iEntities, iResolved, iUnresolved,
		(int)families.size(), (int)flags.size());
	gDLL->logMsg("Cascade.log", szBuf);
	streamLogTee(1, szBuf);
}
