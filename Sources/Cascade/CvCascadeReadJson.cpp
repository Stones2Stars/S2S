//
//	CvCascadeReadJson -- the fresh BoolExpr-routed reader of the curated Assets/Data JSON. See the header for the
//	build plan + scope. INCREMENT 1: the entity-reader skeleton + FK resolution. INCREMENT 2: the condition ->
//	BoolExpr translator + a coverage survey (which predicates map to a GOM/Tag leaf, which need a strategy).
//

#include "CvGameCoreDLL.h"             // PCH umbrella -- picojson, windows.h, gDLL, GC, CvWStringBuffer, GOM/Tag enums
#include "CvCascadeReadJson.h"
#include "AI/BetterBTSAI.h"            // gPlayerLogLevel + streamLogTee
#include "Defines/CvGlobals.h"         // GC.getInfoTypeForString -- the type registry (FK resolution)
#include "Infrastructure/BoolExpr.h"   // the translation target (And/Or/Not/Has/Is) -- the engine's condition evaluator
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>

// --- vocabulary (json.md §1/§2; the cascade-section + intrinsic/aux sets the increment-1 classification needs) ---
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

static bool rj_starts(const std::string& s, const char* p)
{
	const std::string ps(p);
	return s.size() >= ps.size() && s.compare(0, ps.size(), ps) == 0;
}

// A fresh per-entity record (increment 1: type + resolved engine index; richer fields come in later increments).
struct RjEntity { std::string type; int typeId; };

static bool rj_readFile(const std::string& path, std::string& out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open()) return false;
	std::ostringstream ss; ss << f.rdbuf(); out = ss.str();
	return true;
}

// Recursive *.json walk under a dir (the harness's proven find_json; Assets/Data is per-type subdirs). Win32, C++03.
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

// ===================== INCREMENT 2: the condition -> BoolExpr translator + coverage survey =====================
// Maps a JSON condition (json.md §3.4/§3.5) onto the engine's BoolExpr tree (the SAME evaluator legacy
// constructCondition uses): all/any/noneOf -> And/Or/Not (binary, left-folded); a type atom / type-param predicate ->
// BoolExprHas(GOM,id); a relief/water/city predicate -> BoolExprIs(TAG); membership {terrain|feature|bonus:[…]} ->
// Or of Has. The translator is GROUNDED in the GOM/Tag enums -- no guessing.
//
// ⛔ DESIGN FORK SURFACED (not decided here): a chunk of json.md's predicate registry (IS_CAPITAL / HAS_POWER /
// HAS_STATE_RELIGION / IS_GOLDEN_AGE / vicinity-connection / count thresholds via IntExpr) has NO GOM/Tag leaf. Those
// leaves are RECORDED (named + counted) and stand in as a true CONSTANT for the SURVEY only (json.md: an unknown
// predicate is IGNORED, never false) -- a placeholder, NOT the design. The survey data drives the next decision
// (extend TagTypes / add a BoolExpr leaf / a cascade-side predicate evaluator). Count thresholds (min>1/max) are
// likewise recorded as a deferred IntExpr-comparison case.

struct RjCondStats
{
	int leaves, mapped, unmappedPred, countThreshold;
	std::set<std::string> unmappedTokens;
	RjCondStats() : leaves(0), mapped(0), unmappedPred(0), countThreshold(0) {}
	void note(const std::string& tok) { ++unmappedPred; if (unmappedTokens.size() < 64) unmappedTokens.insert(tok); }
};

static GOMTypes rj_gomForType(const std::string& t)
{
	if (rj_starts(t, "BUILDING_"))     return GOM_BUILDING;
	if (rj_starts(t, "UNITCOMBAT_"))   return GOM_UNITCOMBAT;   // before UNIT_
	if (rj_starts(t, "UNIT_"))         return GOM_UNITTYPE;
	if (rj_starts(t, "TECH_"))         return GOM_TECH;
	if (rj_starts(t, "BONUS_"))        return GOM_BONUS;
	if (rj_starts(t, "CIVIC_"))        return GOM_CIVIC;
	if (rj_starts(t, "RELIGION_"))     return GOM_RELIGION;
	if (rj_starts(t, "CORPORATION_"))  return GOM_CORPORATION;
	if (rj_starts(t, "PROMOTION_"))    return GOM_PROMOTION;
	if (rj_starts(t, "TRAIT_"))        return GOM_TRAIT;
	if (rj_starts(t, "FEATURE_"))      return GOM_FEATURE;
	if (rj_starts(t, "TERRAIN_"))      return GOM_TERRAIN;
	if (rj_starts(t, "ROUTE_"))        return GOM_ROUTE;
	if (rj_starts(t, "IMPROVEMENT_"))  return GOM_IMPROVEMENT;
	if (rj_starts(t, "HERITAGE_"))     return GOM_HERITAGE;
	if (rj_starts(t, "GAMEOPTION_"))   return GOM_OPTION;
	return NO_GOM;
}

static TagTypes rj_tagForPredicate(const std::string& p)
{
	if (p == "IS_WATER")        return TAG_WATER;
	if (p == "HAS_FRESHWATER")  return TAG_FRESH_WATER;
	if (p == "HAS_PEAK")        return TAG_PEAK;
	if (p == "HAS_HILLS")       return TAG_HILL;
	if (p == "IS_FLATLANDS")    return TAG_FLATLAND;
	if (p == "HAS_COAST")       return TAG_COASTAL;
	if (p == "IS_CITY")         return TAG_CITY;
	return NO_TAG;
}

static GOMTypes rj_gomForParamPred(const std::string& k)
{
	if (k == "HAS_BONUS")        return GOM_BONUS;
	if (k == "HAS_TERRAIN")      return GOM_TERRAIN;
	if (k == "HAS_FEATURE")      return GOM_FEATURE;
	if (k == "HAS_IMPROVEMENT")  return GOM_IMPROVEMENT;
	if (k == "HAS_RELIGION")     return GOM_RELIGION;
	if (k == "HAS_CORPORATION")  return GOM_CORPORATION;
	return NO_GOM;
}

// A type-string presence leaf -> Has(GOM,id) if it resolves; records + true-constant otherwise.
static const BoolExpr* rj_leafType(const std::string& type, RjCondStats& st)
{
	++st.leaves;
	const GOMTypes gom = rj_gomForType(type);
	const int id = GC.getInfoTypeForString(type.c_str(), true);
	if (gom != NO_GOM && id >= 0) { ++st.mapped; return new BoolExprHas(gom, id); }
	st.note(type);
	return new BoolExprConstant(true);
}

static const BoolExpr* rj_translate(const picojson::value& v, RjCondStats& st);

// Left-fold a child list with binary And/Or. Empty all->true, empty any->false (identity elements).
static const BoolExpr* rj_fold(const picojson::array& a, bool bAnd, RjCondStats& st)
{
	const BoolExpr* acc = NULL;
	for (size_t i = 0; i < a.size(); ++i)
	{
		const BoolExpr* e = rj_translate(a[i], st);
		if (e == NULL) continue;
		if (acc == NULL) acc = e;
		else acc = bAnd ? (const BoolExpr*)new BoolExprAnd(acc, e) : (const BoolExpr*)new BoolExprOr(acc, e);
	}
	return acc != NULL ? acc : (const BoolExpr*)new BoolExprConstant(bAnd);
}

static const BoolExpr* rj_translate(const picojson::value& v, RjCondStats& st)
{
	if (v.is<bool>()) return new BoolExprConstant(v.get<bool>());
	if (v.is<std::string>())
	{
		const std::string s = v.get<std::string>();
		if (!s.empty() && s[0] == '!') return new BoolExprNot(rj_translate(picojson::value(s.substr(1)), st));
		const TagTypes tag = rj_tagForPredicate(s);
		if (tag != NO_TAG) { ++st.leaves; ++st.mapped; return new BoolExprIs(tag); }
		if (rj_gomForType(s) != NO_GOM) return rj_leafType(s, st);
		++st.leaves; st.note(s);                       // an unmapped bare predicate (IS_CAPITAL/HAS_POWER/…)
		return new BoolExprConstant(true);
	}
	if (!v.is<picojson::object>()) return new BoolExprConstant(true);
	const picojson::object& o = v.get<picojson::object>();
	picojson::object::const_iterator it;
	if ((it = o.find("all")) != o.end() && it->second.is<picojson::array>())    return rj_fold(it->second.get<picojson::array>(), true, st);
	if ((it = o.find("any")) != o.end() && it->second.is<picojson::array>())    return rj_fold(it->second.get<picojson::array>(), false, st);
	if ((it = o.find("noneOf")) != o.end() && it->second.is<picojson::array>()) return new BoolExprNot(rj_fold(it->second.get<picojson::array>(), false, st));
	if (o.find("enabled") != o.end() || o.find("disabled") != o.end())          // a gate: enabled AND NOT disabled
	{
		const BoolExpr* acc = NULL;
		if ((it = o.find("enabled")) != o.end()) acc = rj_translate(it->second, st);
		if ((it = o.find("disabled")) != o.end())
		{
			const BoolExpr* dis = new BoolExprNot(rj_translate(it->second, st));
			acc = acc ? (const BoolExpr*)new BoolExprAnd(acc, dis) : dis;
		}
		return acc ? acc : (const BoolExpr*)new BoolExprConstant(true);
	}
	if ((it = o.find("type")) != o.end() && it->second.is<std::string>())        // atom {type, scope, min, max, connection}
	{
		picojson::object::const_iterator mn = o.find("min"), mx = o.find("max");
		if ((mn != o.end() && mn->second.is<double>() && mn->second.get<double>() > 1) || mx != o.end())
			++st.countThreshold;                       // a count threshold -> deferred IntExpr comparison (recorded)
		return rj_leafType(it->second.get<std::string>(), st);   // presence (min:1) -> Has; threshold approximated as presence
	}
	if (o.size() == 1)                                                          // membership {t|f|b:[…]} OR {PRED:param}
	{
		const std::string k = o.begin()->first;
		const picojson::value& p = o.begin()->second;
		if ((k == "terrain" || k == "feature" || k == "bonus") && p.is<picojson::array>())
		{
			const GOMTypes gom = (k == "terrain") ? GOM_TERRAIN : (k == "feature") ? GOM_FEATURE : GOM_BONUS;
			const picojson::array& a = p.get<picojson::array>();
			const BoolExpr* acc = NULL;
			for (size_t i = 0; i < a.size(); ++i) if (a[i].is<std::string>())
			{
				++st.leaves;
				const int id = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true);
				const BoolExpr* e;
				if (id >= 0) { ++st.mapped; e = new BoolExprHas(gom, id); }
				else { st.note(a[i].get<std::string>()); e = new BoolExprConstant(true); }
				acc = acc ? (const BoolExpr*)new BoolExprOr(acc, e) : e;
			}
			return acc ? acc : (const BoolExpr*)new BoolExprConstant(false);
		}
		const GOMTypes gom = rj_gomForParamPred(k);
		if (gom != NO_GOM && p.is<std::string>())
		{
			++st.leaves;
			const int id = GC.getInfoTypeForString(p.get<std::string>().c_str(), true);
			if (id >= 0) { ++st.mapped; return new BoolExprHas(gom, id); }
			st.note(k);
			return new BoolExprConstant(true);
		}
		++st.leaves; st.note(k);                       // unmapped param predicate ({HAS_COAST:{minArea}}, latitude, …)
		return new BoolExprConstant(true);
	}
	return new BoolExprConstant(true);
}

void cascadeReadJsonProbe()
{
	static bool s_done = false;
	if (s_done || gPlayerLogLevel < 1)
	{
		return; // one-shot, and only while logging is on (shadow testing) -- zero cost in normal play
	}
	s_done = true;

	std::string base = gDLL->getModName(true);
	if (!base.empty() && base[base.size() - 1] != '\\' && base[base.size() - 1] != '/') base += "\\";
	const std::string dataDir = base + "Assets\\Data";

	std::vector<std::string> files;
	rj_find(dataDir, files);

	int iFailed = 0, iEntities = 0, iResolved = 0, iUnresolved = 0, iShownUnres = 0;
	int iConds = 0, iCondsFull = 0, iCondSample = 0;
	std::set<std::string> families, flags;
	std::vector<RjEntity> store;
	RjCondStats cond;
	char szBuf[1024];

	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string text;
		if (!rj_readFile(files[i], text)) { ++iFailed; continue; }
		picojson::value v;
		const std::string err = picojson::parse(v, text);
		if (!err.empty() || !v.is<picojson::object>()) { ++iFailed; continue; }
		const picojson::object& o = v.get<picojson::object>();

		picojson::object::const_iterator t = o.find("type");
		if (t == o.end() || !t->second.is<std::string>()) continue;
		++iEntities;
		const std::string type = t->second.get<std::string>();
		RjEntity rec;
		rec.type = type;
		rec.typeId = GC.getInfoTypeForString(type.c_str(), true);
		store.push_back(rec);
		if (rec.typeId >= 0) ++iResolved;
		else { ++iUnresolved; if (iShownUnres < 16) { sprintf(szBuf, "[READJSON/unresolved] type=%s", type.c_str()); gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf); ++iShownUnres; } }

		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			const std::string& k = it->first;
			if (rj_in(RJ_CASCADE_SECTIONS, k) || rj_in(RJ_INTRINSIC, k)) continue;
			if (it->second.is<picojson::object>()) families.insert(k);
			else flags.insert(k);
		}

		// INCREMENT 2: translate requires.build / requires.operate -> BoolExpr (the condition translator + survey).
		picojson::object::const_iterator rq = o.find("requires");
		if (rq != o.end() && rq->second.is<picojson::object>())
		{
			const picojson::object& ro = rq->second.get<picojson::object>();
			for (picojson::object::const_iterator sub = ro.begin(); sub != ro.end(); ++sub)
			{
				++iConds;
				const int beforeGaps = cond.unmappedPred + cond.countThreshold;
				const BoolExpr* e = rj_translate(sub->second, cond);
				if (cond.unmappedPred + cond.countThreshold == beforeGaps) ++iCondsFull; // every leaf mapped, no deferred
				if (iCondSample < 6 && e != NULL)
				{
					CvWStringBuffer buf;
					e->buildDisplayString(buf);
					sprintf(szBuf, "[READJSON/cond] %s.%s = %S", type.c_str(), sub->first.c_str(), buf.getCString());
					gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf); ++iCondSample;
				}
				delete e; // free the translated tree (probe-only; the cutover keeps it on the entity record)
			}
		}
	}

	sprintf(szBuf, "[READJSON/dir] %s", dataDir.c_str());
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
	sprintf(szBuf, "[READJSON/probe] files=%d parsed=%d failed=%d entities=%d resolved=%d unresolved=%d familyKinds=%d flagKinds=%d",
		(int)files.size(), (int)files.size() - iFailed, iFailed, iEntities, iResolved, iUnresolved, (int)families.size(), (int)flags.size());
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
	// INCREMENT 2 survey: how much of the requires-condition surface translates to a BoolExpr leaf today.
	sprintf(szBuf, "[READJSON/cond-survey] conditions=%d fullyMapped=%d leaves=%d mapped=%d unmappedLeaves=%d countThresholds=%d unmappedKinds=%d",
		iConds, iCondsFull, cond.leaves, cond.mapped, cond.unmappedPred, cond.countThreshold, (int)cond.unmappedTokens.size());
	gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
	for (std::set<std::string>::const_iterator it = cond.unmappedTokens.begin(); it != cond.unmappedTokens.end(); ++it)
	{
		sprintf(szBuf, "[READJSON/cond-gap] %s", it->c_str());
		gDLL->logMsg("Cascade.log", szBuf); streamLogTee(1, szBuf);
	}
}
