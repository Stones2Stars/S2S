//
//	CvCascadeReadJson -- the DLL-side readJson harness (#430). See CvCascadeReadJson.h.
//	Locates an entity's JSON under Assets/Data, parses requires/allowed into a fresh CvEntityAvailability via
//	picojson, and (doTurn slice) shadows cascadeBuildable vs the engine. ⛔ TEMPORARY; purge when full readJson lands.
//
//	rj-prefixed file-local helpers -- FastBuild unity batches concatenate .cpp, so generic anon-namespace names
//	could collide with a sibling in the blob.
//

#include "CvGameCoreDLL.h"   // PCH: engine globals + picojson + windows.h (FindFirstFile)
#include "CvCascadeReadJson.h"
#include "CvCascadeCondition.h"
#include "CvCascadeTally.h"
#include "Defines/CvGlobals.h"
#include "Infrastructure/CvInitCore.h"
#include "AI/CvGameAI.h"
#include "AI/CvPlayerAI.h"
#include "AI/CvTeamAI.h"        // GET_TEAM / isHasTech (obsoletion index)
#include "Engine/CvCity.h"
#include "CvInfos.h"         // GC.getTechInfo (type strings for the obsoletion scan)
#include "CvBuildingInfo.h"
#include "Repos/BuildingsRepo.h"  // autoBuildings() -- the bAutoBuild loop's roster (auto-placement shadow B-i)
#include "Infos/CvPropertyInfo.h" // getPropertyBuildings() -- checkPropertyBuildings' band roster (auto-placement shadow B-i)
#include "Engine/CvProperties.h"         // PropertyBuilding (the band struct)
#include "AI/BetterBTSAI.h"     // gPlayerLogLevel, streamLogTee
#include <fstream>
#include <sstream>
#include <cctype>

namespace
{
	// ---------- small string helpers ----------
	bool rjHasPrefix(const std::string& s, const char* pfx)
	{
		const size_t n = strlen(pfx);
		return s.size() >= n && s.compare(0, n, pfx) == 0;
	}

	std::string rjToLowerStr(const char* s)
	{
		std::string out;
		for (const char* p = s; *p != '\0'; ++p) out += (char)tolower((unsigned char)*p);
		return out;
	}

	void rjAppendNote(std::string& sNotes, const std::string& sReason)
	{
		if (sNotes.size() > 320) return;
		if (!sNotes.empty()) sNotes += "; ";
		sNotes += sReason;
	}

	// ---------- vocabulary maps ----------
	bool rjAtomDomain(const std::string& s, AtomDomain& eOut, bool& bToken)
	{
		bToken = false;
		if (s == "POPULATION")          { eOut = ATOMDOMAIN_POPULATION;  bToken = true; return true; }
		if (s == "CITY")                { eOut = ATOMDOMAIN_CITYCOUNT;   bToken = true; return true; }
		if (s == "AREA_SIZE")           { eOut = ATOMDOMAIN_AREASIZE;    bToken = true; return true; }
		if (rjHasPrefix(s, "BUILDING_"))    { eOut = ATOMDOMAIN_BUILDING;    return true; }
		if (rjHasPrefix(s, "UNIT_"))        { eOut = ATOMDOMAIN_UNIT;        return true; }
		if (rjHasPrefix(s, "TECH_"))        { eOut = ATOMDOMAIN_TECH;        return true; }
		if (rjHasPrefix(s, "BONUS_"))       { eOut = ATOMDOMAIN_BONUS;       return true; }
		if (rjHasPrefix(s, "CIVIC_"))       { eOut = ATOMDOMAIN_CIVIC;       return true; }
		if (rjHasPrefix(s, "RELIGION_"))    { eOut = ATOMDOMAIN_RELIGION;    return true; }
		if (rjHasPrefix(s, "CORPORATION_")) { eOut = ATOMDOMAIN_CORPORATION; return true; }
		if (rjHasPrefix(s, "HERITAGE_"))    { eOut = ATOMDOMAIN_HERITAGE;    return true; }
		if (rjHasPrefix(s, "PROPERTY_"))    { eOut = ATOMDOMAIN_PROPERTY;    return true; }
		return false;
	}

	CountScope rjDefaultScope(AtomDomain d)
	{
		switch (d)
		{
		case ATOMDOMAIN_TECH:      return COUNTSCOPE_TEAM;
		case ATOMDOMAIN_CIVIC:     return COUNTSCOPE_EMPIRE;
		case ATOMDOMAIN_HERITAGE:  return COUNTSCOPE_EMPIRE;
		case ATOMDOMAIN_CITYCOUNT: return COUNTSCOPE_EMPIRE;
		default:                   return COUNTSCOPE_CITY; // building/bonus/religion/corporation/population
		}
	}

	bool rjScope(const std::string& s, CountScope& eOut)
	{
		if (s == "world")  { eOut = COUNTSCOPE_WORLD;  return true; }
		if (s == "team")   { eOut = COUNTSCOPE_TEAM;   return true; }
		if (s == "empire") { eOut = COUNTSCOPE_EMPIRE; return true; }
		if (s == "city")   { eOut = COUNTSCOPE_CITY;   return true; }
		if (s == "plot")   { eOut = COUNTSCOPE_PLOT;   return true; }
		return false;
	}

	void rjConn(const std::string& s, ConnReq& eOut)
	{
		if (s == "trade")    eOut = CONN_TRADE;
		else if (s == "vicinity") eOut = CONN_VICINITY;
		else eOut = CONN_TRADE_OR_VICINITY; // "trade|vicinity" and anything else
	}

	bool rjBarePredicate(const std::string& s, PredicateKind& k)
	{
		if (s == "IS_CAPITAL")           { k = PRED_IS_CAPITAL;         return true; }
		if (s == "HAS_POWER")            { k = PRED_HAS_POWER;          return true; }
		if (s == "HAS_STATE_RELIGION")   { k = PRED_HAS_STATE_RELIGION; return true; }
		if (s == "STATE_RELIGION_IN_CITY") { k = PRED_STATE_RELIGION_IN_CITY; return true; }
		if (s == "IS_COASTAL" || s == "COASTAL_LAND") { k = PRED_IS_COASTAL; return true; }
		if (s == "HAS_RIVER")            { k = PRED_HAS_RIVER;          return true; }
		if (s == "IS_WATER")             { k = PRED_IS_WATER;           return true; }
		if (s == "IS_HILLS")             { k = PRED_IS_HILLS;           return true; }
		if (s == "IS_PEAK")              { k = PRED_IS_PEAK;            return true; }
		if (s == "IS_FLATLANDS")         { k = PRED_IS_FLATLANDS;       return true; }
		if (s == "IS_FRESHWATER")        { k = PRED_IS_FRESHWATER;      return true; }
		if (s == "HAS_IRRIGATION")       { k = PRED_HAS_IRRIGATION;     return true; }
		if (s == "HAS_FEATURE")          { k = PRED_HAS_FEATURE_ANY;    return true; }
		return false;
	}

	bool rjParamPredicateKind(const std::string& key, PredicateKind& k)
	{
		if (key == "HAS_FEATURE")     { k = PRED_HAS_FEATURE;     return true; }
		if (key == "HAS_TERRAIN")     { k = PRED_HAS_TERRAIN;     return true; }
		if (key == "HAS_BONUS")       { k = PRED_HAS_BONUS;       return true; }
		if (key == "HAS_RELIGION")    { k = PRED_HAS_RELIGION;    return true; }
		if (key == "STATE_RELIGION")  { k = PRED_STATE_RELIGION;  return true; }
		if (key == "HOLY_CITY")       { k = PRED_HOLY_CITY;       return true; }
		if (key == "HAS_CORPORATION") { k = PRED_HAS_CORPORATION; return true; }
		return false;
	}

	// ---------- leaf parsing ----------
	bool rjParseAtomObj(const picojson::object& o, CvCountAtom& out, std::string& reason)
	{
		picojson::object::const_iterator it = o.find("type");
		const std::string sType = it->second.get<std::string>();

		AtomDomain eDomain; bool bToken;
		if (!rjAtomDomain(sType, eDomain, bToken)) { reason = "domain-pending(" + sType + ")"; return false; }
		int iType = -1;
		if (!bToken)
		{
			iType = GC.getInfoTypeForString(sType.c_str(), true);
			if (iType < 0) { reason = "type-not-loaded(" + sType + ")"; return false; }
		}

		CountScope eScope = rjDefaultScope(eDomain);
		it = o.find("scope");
		if (it != o.end() && it->second.is<std::string>())
		{
			if (!rjScope(it->second.get<std::string>(), eScope)) { reason = "scope-unknown(" + it->second.get<std::string>() + ")"; return false; }
		}

		ConnReq eConn = CONN_NONE;
		it = o.find("connection");
		if (it != o.end() && it->second.is<std::string>()) rjConn(it->second.get<std::string>(), eConn);

		// iMin default 1 = "presence" for count atoms; a PROPERTY band has no presence semantics, so a max-only band
		// (e.g. disease <= 50) defaults to NO lower bound rather than >=1.
		int iMin = (eDomain == ATOMDOMAIN_PROPERTY) ? -2000000000 : 1;
		int iMax = -1;
		it = o.find("min"); if (it != o.end() && it->second.is<double>()) iMin = (int)it->second.get<double>();
		it = o.find("max"); if (it != o.end() && it->second.is<double>()) iMax = (int)it->second.get<double>();

		out = CvCountAtom(eDomain, iType, eScope, iMin, iMax, eConn);
		return true;
	}

	// A leaf = a bare-predicate string, a {type,...} count atom (or a TERRAIN_/FEATURE_ plot predicate authored
	// as an atom), or a {PREDICATE: param} object. Anything else (membership sugar, latitude, existedFor, ...)
	// is DROPPED with a reason -- never silently satisfied.
	bool rjParseLeaf(const picojson::value& v, CvCascadeLeaf& out, std::string& reason)
	{
		if (v.is<std::string>())
		{
			PredicateKind k;
			if (rjBarePredicate(v.get<std::string>(), k)) { out.bPredicate = true; out.pred = CvPredicate(k, -1); return true; }
			reason = "bare-predicate-unknown(" + v.get<std::string>() + ")";
			return false;
		}
		if (!v.is<picojson::object>()) { reason = "non-object-leaf"; return false; }
		const picojson::object& o = v.get<picojson::object>();

		picojson::object::const_iterator itType = o.find("type");
		if (itType != o.end() && itType->second.is<std::string>())
		{
			const std::string sType = itType->second.get<std::string>();
			// plot-substrate types authored as atoms are really plot predicates (terrain/feature/improvement = VICINITY)
			if (rjHasPrefix(sType, "TERRAIN_") || rjHasPrefix(sType, "FEATURE_") || rjHasPrefix(sType, "IMPROVEMENT_"))
			{
				const int ip = GC.getInfoTypeForString(sType.c_str(), true);
				if (ip < 0) { reason = "type-not-loaded(" + sType + ")"; return false; }
				out.bPredicate = true;
				const PredicateKind ek = rjHasPrefix(sType, "TERRAIN_") ? PRED_HAS_TERRAIN
				                       : rjHasPrefix(sType, "FEATURE_") ? PRED_HAS_FEATURE : PRED_HAS_IMPROVEMENT;
				out.pred = CvPredicate(ek, ip);
				return true;
			}
			if (rjHasPrefix(sType, "MAPCATEGORY_"))  // map-category placement gate (CENTER-plot, legacy isMapCategory)
			{
				const int ip = GC.getInfoTypeForString(sType.c_str(), true);
				if (ip < 0) { reason = "type-not-loaded(" + sType + ")"; return false; }
				out.bPredicate = true;
				out.pred = CvPredicate(PRED_HAS_MAP_CATEGORY, ip);
				return true;
			}
			CvCountAtom a;
			if (rjParseAtomObj(o, a, reason)) { out.bPredicate = false; out.atom = a; return true; }
			return false;
		}

		// object-parameterized RANGE predicate: {latitude:{min?,max?}} (data-model §2.5). Absent bound = unbounded.
		{
			picojson::object::const_iterator itLat = o.find("latitude");
			if (itLat != o.end() && itLat->second.is<picojson::object>())
			{
				const picojson::object& r = itLat->second.get<picojson::object>();
				out.bPredicate = true;
				out.pred = CvPredicate(PRED_LATITUDE, -1);
				picojson::object::const_iterator m = r.find("min");
				if (m != r.end() && m->second.is<double>()) out.pred.iMin = (int)m->second.get<double>();
				m = r.find("max");
				if (m != r.end() && m->second.is<double>()) out.pred.iMax = (int)m->second.get<double>();
				return true;
			}
		}

		// parameterized predicate: a single recognized key with a string param
		for (picojson::object::const_iterator it = o.begin(); it != o.end(); ++it)
		{
			PredicateKind k;
			if (rjParamPredicateKind(it->first, k) && it->second.is<std::string>())
			{
				const int ip = GC.getInfoTypeForString(it->second.get<std::string>().c_str(), true);
				if (ip < 0) { reason = "pred-param-not-loaded(" + it->second.get<std::string>() + ")"; return false; }
				out.bPredicate = true; out.pred = CvPredicate(k, ip);
				return true;
			}
		}

		reason = "leaf-pending(";
		if (!o.empty()) reason += o.begin()->first;
		reason += ")";
		return false;
	}

	void rjParseConditionObject(const picojson::object& oPart, CvCascadeCondition& kCond,
		int& iSup, int& iSkip, std::string& sNotes)
	{
		picojson::object::const_iterator it = oPart.find("all");
		if (it != oPart.end() && it->second.is<picojson::array>())
		{
			const picojson::array& a = it->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				CvCascadeLeaf l; std::string r;
				if (rjParseLeaf(a[i], l, r)) { kCond.all.push_back(l); ++iSup; }
				else { ++iSkip; rjAppendNote(sNotes, "all:" + r); }
			}
		}
		it = oPart.find("any");
		if (it != oPart.end() && it->second.is<picojson::array>())
		{
			const picojson::array& groups = it->second.get<picojson::array>();
			for (size_t g = 0; g < groups.size(); ++g)
			{
				if (!groups[g].is<picojson::array>()) { ++iSkip; rjAppendNote(sNotes, "any:non-array-group"); continue; }
				const picojson::array& grp = groups[g].get<picojson::array>();
				std::vector<CvCascadeLeaf> vGroup;
				for (size_t i = 0; i < grp.size(); ++i)
				{
					CvCascadeLeaf l; std::string r;
					if (rjParseLeaf(grp[i], l, r)) { vGroup.push_back(l); ++iSup; }
					else { ++iSkip; rjAppendNote(sNotes, "any:" + r); }
				}
				if (!vGroup.empty()) kCond.any.push_back(vGroup);
			}
		}
		it = oPart.find("noneOf");
		if (it != oPart.end() && it->second.is<picojson::array>())
		{
			const picojson::array& a = it->second.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
			{
				CvCascadeLeaf l; std::string r;
				if (rjParseLeaf(a[i], l, r)) { kCond.noneOf.push_back(l); ++iSup; }
				else { ++iSkip; rjAppendNote(sNotes, "noneOf:" + r); }
			}
		}
		// the bare-predicate twins: `disabled` -> noneOf (forbidden while it holds), `enabled` -> all
		it = oPart.find("disabled");
		if (it != oPart.end())
		{
			CvCascadeLeaf l; std::string r;
			if (rjParseLeaf(it->second, l, r)) { kCond.noneOf.push_back(l); ++iSup; }
			else { ++iSkip; rjAppendNote(sNotes, "disabled:" + r); }
		}
		it = oPart.find("enabled");
		if (it != oPart.end())
		{
			CvCascadeLeaf l; std::string r;
			if (rjParseLeaf(it->second, l, r)) { kCond.all.push_back(l); ++iSup; }
			else { ++iSkip; rjAppendNote(sNotes, "enabled:" + r); }
		}
	}

	const char* const RJ_ALLOWED_KEYS[3] = { "world", "team", "empire" };
	const CountScope RJ_ALLOWED_VALS[3]  = { COUNTSCOPE_WORLD, COUNTSCOPE_TEAM, COUNTSCOPE_EMPIRE };

	bool rjParseAllowed(const picojson::object& oAllowed, CountScope& eScopeOut, int& iCapOut)
	{
		for (int i = 0; i < 3; ++i)
		{
			picojson::object::const_iterator it = oAllowed.find(RJ_ALLOWED_KEYS[i]);
			if (it != oAllowed.end() && it->second.is<double>())
			{
				eScopeOut = RJ_ALLOWED_VALS[i];
				iCapOut = (int)it->second.get<double>();
				return true;
			}
		}
		return false;
	}

	// ---------- file location (prefix -> Data/<folder>, search any grouping sub-folders) ----------
	bool rjEntityFolder(const char* szTypeKey, std::string& sFolder)
	{
		const std::string s(szTypeKey);
		if (rjHasPrefix(s, "BUILDING_"))    { sFolder = "buildings";    return true; }
		if (rjHasPrefix(s, "UNIT_"))        { sFolder = "units";        return true; }
		if (rjHasPrefix(s, "TECH_"))        { sFolder = "techs";        return true; }
		if (rjHasPrefix(s, "CIVIC_"))       { sFolder = "civics";       return true; }
		if (rjHasPrefix(s, "PROJECT_"))     { sFolder = "projects";     return true; }
		if (rjHasPrefix(s, "RELIGION_"))    { sFolder = "religions";    return true; }
		if (rjHasPrefix(s, "CORPORATION_")) { sFolder = "corporations"; return true; }
		if (rjHasPrefix(s, "SPECIALBUILDING_")) { sFolder = "specialbuildings"; return true; }
		return false;
	}

	bool rjReadFile(const std::string& sPath, std::string& sOut)
	{
		std::ifstream f(sPath.c_str(), std::ios::in | std::ios::binary);
		if (!f.good()) return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		sOut = ss.str();
		return !sOut.empty();
	}

	bool rjLocateEntityJson(const char* szTypeKey, std::string& sContent)
	{
		std::string sFolder;
		if (!rjEntityFolder(szTypeKey, sFolder)) return false;
		const std::string sFile = rjToLowerStr(szTypeKey) + ".json";
		const std::string sBase = std::string(GC.getInitCore().getDLLPath().c_str()) + "\\Data\\" + sFolder;

		if (rjReadFile(sBase + "\\" + sFile, sContent)) return true; // flat layout

		// foldered: try each immediate sub-directory (era / category / source / ...)
		WIN32_FIND_DATAA fd;
		const std::string sGlob = sBase + "\\*";
		HANDLE h = FindFirstFileA(sGlob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) return false;
		bool bFound = false;
		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
				&& strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
			{
				if (rjReadFile(sBase + "\\" + fd.cFileName + "\\" + sFile, sContent)) { bFound = true; break; }
			}
		} while (FindNextFileA(h, &fd) != 0);
		FindClose(h);
		return bFound;
	}
} // namespace

// ===================== exported: parse one entity's availability =====================

bool cascadeReadJsonAvailability(const char* szTypeKey, CvEntityAvailability& kOut, std::string& szNotes)
{
	std::string sContent;
	if (!rjLocateEntityJson(szTypeKey, sContent)) { szNotes = "no JSON file found"; return false; }

	picojson::value root;
	const std::string sErr = picojson::parse(root, sContent);
	if (!sErr.empty() || !root.is<picojson::object>()) { szNotes = "parse-error:" + sErr; return false; }
	const picojson::object& o = root.get<picojson::object>();

	int iSup = 0, iSkip = 0;
	std::string sAtomNotes;

	picojson::object::const_iterator itReq = o.find("requires");
	if (itReq != o.end() && itReq->second.is<picojson::object>())
	{
		const picojson::object& oReq = itReq->second.get<picojson::object>();
		picojson::object::const_iterator itB = oReq.find("build");
		if (itB != oReq.end() && itB->second.is<picojson::object>())
			rjParseConditionObject(itB->second.get<picojson::object>(), kOut.requiresBuild, iSup, iSkip, sAtomNotes);
		picojson::object::const_iterator itO = oReq.find("operate");
		if (itO != oReq.end() && itO->second.is<picojson::object>())
			rjParseConditionObject(itO->second.get<picojson::object>(), kOut.requiresOperate, iSup, iSkip, sAtomNotes);
	}

	picojson::object::const_iterator itAllowed = o.find("allowed");
	if (itAllowed != o.end() && itAllowed->second.is<picojson::object>())
	{
		CountScope eScope; int iCap;
		if (rjParseAllowed(itAllowed->second.get<picojson::object>(), eScope, iCap))
		{
			kOut.allowedScope = eScope;
			kOut.allowedCap = iCap;
		}
	}

	// identity.spawnOnly / identity.notConstructible -- the clean flags (migrated from the legacy iCost==-1 sentinel)
	// marking an entity as NOT player-producible. spawnOnly = a unit (wildlife/spawned); notConstructible = the building
	// twin (autobuilt / property-spawned / outcome-granted / GP-or-event placed / doctrine toggle). Settlers (no iCost
	// tag, population cost) are NOT spawnOnly -> buildable. The gate keys on the flag, never on a raw -1 cost.
	picojson::object::const_iterator itId = o.find("identity");
	if (itId != o.end() && itId->second.is<picojson::object>())
	{
		const picojson::object& oId = itId->second.get<picojson::object>();
		picojson::object::const_iterator itSp = oId.find("spawnOnly");
		if (itSp != oId.end() && itSp->second.is<bool>()) kOut.spawnOnly = itSp->second.get<bool>();
		picojson::object::const_iterator itNc = oId.find("notConstructible");
		if (itNc != oId.end() && itNc->second.is<bool>()) kOut.notConstructible = itNc->second.get<bool>();
		picojson::object::const_iterator itAb = oId.find("autoBuild");
		if (itAb != oId.end() && itAb->second.is<bool>()) kOut.autoBuild = itAb->second.get<bool>();
	}

	std::ostringstream ss;
	ss << iSup << " wired, " << iSkip << " pending";
	if (!sAtomNotes.empty()) ss << " [" << sAtomNotes << "]";
	szNotes = ss.str();
	return true;
}

// ===================== exported: parse one entity's MODIFIER deposits (the magnitude layer) =====================

namespace
{
	// ALL modifier families (lowercase JSON key -> ModifierFamily id; the first three == YieldTypes by value, so the
	// yield-pilot deposit ids are unchanged). The shadow now covers every channel -- readJson tags each family's deposits
	// with its id, the cascade folds them generically, and /diagnostic/modifierSweep diffs them via the family registry
	// (CvCascadeModifier.h). PROPERTY_* / unit-plane stats widen the same way as their data lands.
	struct RjModFamily { const char* sz; int iFamily; };
	const RjModFamily RJ_MOD_FAMILIES[] = {
		{ "food", MODFAM_FOOD }, { "production", MODFAM_PRODUCTION }, { "commerce", MODFAM_COMMERCE },
		{ "gold", MODFAM_GOLD }, { "research", MODFAM_RESEARCH }, { "culture", MODFAM_CULTURE }, { "espionage", MODFAM_ESPIONAGE },
		{ "health", MODFAM_HEALTH }, { "happiness", MODFAM_HAPPINESS }, { "defense", MODFAM_DEFENSE },
		{ "maintenance", MODFAM_MAINTENANCE }, { "greatPeopleRate", MODFAM_GREATPEOPLE }
	};
	const int RJ_NUM_MOD_FAMILIES = (int)(sizeof(RJ_MOD_FAMILIES) / sizeof(RJ_MOD_FAMILIES[0]));

	// Scope keys (lowercase JSON -> ModifierScope) -- the full containment spine. PILOT consumes city + plot; the rest are
	// parsed so coverage gaps are visible. Loop bound is sizeof-derived so adding a scope can't drift the bound.
	struct RjModScope { const char* sz; int iScope; };
	const RjModScope RJ_MOD_SCOPES[] = {
		{ "world", MODSCOPE_WORLD }, { "team", MODSCOPE_TEAM }, { "empire", MODSCOPE_EMPIRE }, { "area", MODSCOPE_AREA },
		{ "city", MODSCOPE_CITY }, { "plot", MODSCOPE_PLOT }, { "self", MODSCOPE_SELF },
		{ "specialist", MODSCOPE_SPECIALIST }, { "unit", MODSCOPE_UNIT }
	};
	const int RJ_NUM_MOD_SCOPES = (int)(sizeof(RJ_MOD_SCOPES) / sizeof(RJ_MOD_SCOPES[0]));

	// Emit 0+ deposits from a flat/percent value: scalar | { value, enabled?, disabled? } | array-of-those.
	void rjEmitModDeposits(const picojson::value& v, int iFamily, int iScope, ModifierUnit eUnit,
		CvEntityModifiers& kOut, int& iSup, int& iSkip, std::string& sNotes)
	{
		if (v.is<double>())
		{
			CvCascadeModifierDeposit d;
			d.iFamily = iFamily; d.iScope = iScope; d.eUnit = eUnit; d.iValue = (int)v.get<double>();
			kOut.deposits.push_back(d); ++iSup;
		}
		else if (v.is<picojson::object>())
		{
			const picojson::object& o = v.get<picojson::object>();
			picojson::object::const_iterator itVal = o.find("value");
			if (itVal == o.end() || !itVal->second.is<double>()) { ++iSkip; rjAppendNote(sNotes, "mod:noValue"); return; }
			CvCascadeModifierDeposit d;
			d.iFamily = iFamily; d.iScope = iScope; d.eUnit = eUnit; d.iValue = (int)itVal->second.get<double>();
			picojson::object::const_iterator itEn = o.find("enabled");
			if (itEn != o.end())
			{
				if (itEn->second.is<picojson::object>())
					rjParseConditionObject(itEn->second.get<picojson::object>(), d.enabled, iSup, iSkip, sNotes);
				else
				{
					CvCascadeLeaf l; std::string r;
					if (rjParseLeaf(itEn->second, l, r)) d.enabled.all.push_back(l);
					else { ++iSkip; rjAppendNote(sNotes, "mod.enabled:" + r); }
				}
			}
			picojson::object::const_iterator itDis = o.find("disabled");
			if (itDis != o.end() && itDis->second.is<picojson::object>())
				rjParseConditionObject(itDis->second.get<picojson::object>(), d.disabled, iSup, iSkip, sNotes);
			kOut.deposits.push_back(d); ++iSup;
		}
		else if (v.is<picojson::array>())
		{
			const picojson::array& a = v.get<picojson::array>();
			for (size_t i = 0; i < a.size(); ++i)
				rjEmitModDeposits(a[i], iFamily, iScope, eUnit, kOut, iSup, iSkip, sNotes);
		}
		else { ++iSkip; rjAppendNote(sNotes, "mod:badValue"); }
	}

	// Walk a building's root yield keys -> scope blocks -> flat/percent peer keys, emitting deposits. Sub-scopes
	// (improvements/buildings/specialists) + perPopulation are tagged pending (a later sub-pass, not the pilot).
	void rjParseModifiers(const picojson::object& root, CvEntityModifiers& kOut, int& iSup, int& iSkip, std::string& sNotes)
	{
		for (int f = 0; f < RJ_NUM_MOD_FAMILIES; ++f)
		{
			picojson::object::const_iterator itFam = root.find(RJ_MOD_FAMILIES[f].sz);
			if (itFam == root.end() || !itFam->second.is<picojson::object>()) continue;
			const picojson::object& oFam = itFam->second.get<picojson::object>();
			for (int s = 0; s < RJ_NUM_MOD_SCOPES; ++s)
			{
				picojson::object::const_iterator itSc = oFam.find(RJ_MOD_SCOPES[s].sz);
				if (itSc == oFam.end() || !itSc->second.is<picojson::object>()) continue;
				const picojson::object& oSc = itSc->second.get<picojson::object>();
				picojson::object::const_iterator itFlat = oSc.find("flat");
				if (itFlat != oSc.end())
					rjEmitModDeposits(itFlat->second, RJ_MOD_FAMILIES[f].iFamily, RJ_MOD_SCOPES[s].iScope, MODUNIT_FLAT, kOut, iSup, iSkip, sNotes);
				picojson::object::const_iterator itPct = oSc.find("percent");
				if (itPct != oSc.end())
					rjEmitModDeposits(itPct->second, RJ_MOD_FAMILIES[f].iFamily, RJ_MOD_SCOPES[s].iScope, MODUNIT_PERCENT, kOut, iSup, iSkip, sNotes);
				// GROUPED families (`<family>.<scope>.<member>.<unit>` -- defense.amount, maintenance.distance, the
				// espionage insidiousness/investigation sub-stats, ...) are NOT folded into the family slot: a member maps
				// to its OWN realized value (defense.amount -> getDefenseModifier, but bombardDefense/adjacentDamage are
				// SEPARATE stats; insidiousness != espionage-commerce), so folding them all into one slot and diffing vs
				// one legacy getter is semantically wrong (it over-counts -- e.g. it polluted the espionage shadow). The
				// correct structure is a PER-MEMBER shadow (each member diffed vs its own getter); that needs the member->
				// getter map identified per family (not invented). Until then grouped families show honest missingDeposit
				// (the parity work the owner verdicts). Tagged pending so the gap is visible, not silently dropped.
				for (picojson::object::const_iterator itM = oSc.begin(); itM != oSc.end(); ++itM)
				{
					const std::string& sMember = itM->first;
					if (sMember == "flat" || sMember == "percent") continue;
					if (itM->second.is<picojson::object>()) { ++iSkip; rjAppendNote(sNotes, "mod:member:" + sMember); }
				}
				if (oSc.find("perPopulation") != oSc.end()) { ++iSkip; rjAppendNote(sNotes, "mod:perPopulation"); }
				if (oSc.find("improvements") != oSc.end() || oSc.find("buildings") != oSc.end() || oSc.find("specialists") != oSc.end())
					{ ++iSkip; rjAppendNote(sNotes, "mod:subScope"); }
			}
		}
	}
} // namespace

bool cascadeReadJsonModifiers(const char* szTypeKey, CvEntityModifiers& kOut, std::string& szNotes)
{
	std::string sContent;
	if (!rjLocateEntityJson(szTypeKey, sContent)) { szNotes = "no JSON file found"; return false; }
	picojson::value root;
	const std::string sErr = picojson::parse(root, sContent);
	if (!sErr.empty() || !root.is<picojson::object>()) { szNotes = "parse-error:" + sErr; return false; }

	int iSup = 0, iSkip = 0; std::string sNotes;
	rjParseModifiers(root.get<picojson::object>(), kOut, iSup, iSkip, sNotes);
	kOut.iParsed = iSup; kOut.iSkipped = iSkip;

	std::ostringstream ss;
	ss << iSup << " deposits, " << iSkip << " pending";
	if (!sNotes.empty()) ss << " [" << sNotes << "]";
	szNotes = ss.str();
	return true;
}

// ===================== GENERATION (partial): the obsoletion reverse index =====================
// A target is obsolete for a team if the team has researched a tech whose JSON `obsoletes` edge names it. The
// model authors obsoletion FORWARD on the tech (tech.obsoletes.{buildings,units}); we invert it ONCE into a
// reverse index (entity -> obsoleting techs) by scanning the tech JSONs, then check team-HAS per query. Built
// fresh from the JSON (NOT the legacy getObsoleteTech), game-thread only (no locking).

namespace
{
	std::map<int, std::vector<int> > g_obsBuildingTechs;    // buildingIdx -> [techIdx that obsolete it]
	std::map<int, std::vector<int> > g_obsUnitTechs;        // unitIdx     -> [techIdx]
	std::map<int, std::vector<int> > g_enableBuildingTechs; // buildingIdx -> [techIdx whose enables.buildings names it]
	std::map<int, std::vector<int> > g_enableUnitTechs;     // unitIdx     -> [techIdx whose enables.units names it]
	bool g_techBuilt = false;

	void rjIndexTechEdge(std::map<int, std::vector<int> >& m, const picojson::object& oSection, const char* szKey, int iTech)
	{
		picojson::object::const_iterator it = oSection.find(szKey);
		if (it == oSection.end() || !it->second.is<picojson::array>()) return;
		const picojson::array& a = it->second.get<picojson::array>();
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (!a[i].is<std::string>()) continue;
			const int idx = GC.getInfoTypeForString(a[i].get<std::string>().c_str(), true);
			if (idx >= 0) m[idx].push_back(iTech);
		}
	}

	// ONE scan of the tech JSONs builds BOTH reverse indices: enables (entity -> enabling techs) and obsoletes
	// (entity -> obsoleting techs). enables gives the forward tech-gate; obsoletes the obsolescence gate.
	void rjBuildTechIndex()
	{
		if (g_techBuilt) return;
		g_techBuilt = true; // set first -- a parse miss must not retry-loop the whole scan
		const int iNumTechs = GC.getNumTechInfos();
		for (int i = 0; i < iNumTechs; ++i)
		{
			const char* szType = GC.getTechInfo((TechTypes)i).getType(); // type registry only (EXE-bound, allowed)
			std::string sContent;
			if (!rjLocateEntityJson(szType, sContent)) continue;
			picojson::value root;
			if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator itE = o.find("enables");
			if (itE != o.end() && itE->second.is<picojson::object>())
			{
				const picojson::object& oEn = itE->second.get<picojson::object>();
				rjIndexTechEdge(g_enableBuildingTechs, oEn, "buildings", i);
				rjIndexTechEdge(g_enableUnitTechs, oEn, "units", i);
			}
			picojson::object::const_iterator itO = o.find("obsoletes");
			if (itO != o.end() && itO->second.is<picojson::object>())
			{
				const picojson::object& oObs = itO->second.get<picojson::object>();
				rjIndexTechEdge(g_obsBuildingTechs, oObs, "buildings", i);
				rjIndexTechEdge(g_obsUnitTechs, oObs, "units", i);
			}
		}
	}
} // namespace

bool cascadeIsObsoleteForTeam(int eDomain, int iEntity, int iTeam)
{
	if (iTeam < 0 || iTeam >= MAX_TEAMS) return false;
	rjBuildTechIndex();
	const std::map<int, std::vector<int> >& m = (eDomain == COUNTDOMAIN_UNIT) ? g_obsUnitTechs : g_obsBuildingTechs;
	std::map<int, std::vector<int> >::const_iterator it = m.find(iEntity);
	if (it == m.end()) return false;
	const CvTeam& kTeam = GET_TEAM((TeamTypes)iTeam);
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (kTeam.isHasTech((TechTypes)it->second[i])) return true;
	}
	return false;
}

// ===================== SpecialBuilding GROUP CAP (owner 2026-06-17) =====================
// A SpecialBuilding is a building GROUP with a shared cap (e.g. SPECIALBUILDING_GROUP_ELITE_UNIVERSITIES -- pick ONE
// of 15 elite universities). MEMBER->group is authored FORWARD on the building (identity.specialBuildingType, the
// migrated XML FK); GROUP->members is the derived reverse index we build here ONCE by scanning the building JSONs.
// The cap (allowed:{scope:N}) lives on the GROUP entity's JSON. Enforcement: a member is buildable only while
// count(its whole group at the cap's scope) < N -- "at most N of my OWN GROUP". Coexists with the member's own
// self-cap (allowed:{world:1}); this only adds the group dimension. (Built from JSON, game-thread, no locking.)
namespace
{
	std::map<int, int>               g_buildingGroup;   // buildingIdx -> specialBuildingIdx (its group), or absent
	std::map<int, std::vector<int> > g_groupMembers;    // specialBuildingIdx -> [member buildingIdx]
	std::map<int, int>               g_groupCapN;        // specialBuildingIdx -> cap N (absent/-1 == uncapped group)
	std::map<int, int>               g_groupCapScope;    // specialBuildingIdx -> CountScope of the cap
	bool g_groupBuilt = false;

	void rjBuildGroupIndex()
	{
		if (g_groupBuilt) return;
		g_groupBuilt = true; // set first -- a parse miss must not retry-loop the scan
		// PASS 1: building -> group, from each building JSON's identity.specialBuildingType (the migrated FK).
		const int iNumBuildings = GC.getNumBuildingInfos();
		for (int i = 0; i < iNumBuildings; ++i)
		{
			const char* szType = GC.getBuildingInfo((BuildingTypes)i).getType();
			std::string sContent;
			if (!rjLocateEntityJson(szType, sContent)) continue;
			picojson::value root;
			if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator itId = o.find("identity");
			if (itId == o.end() || !itId->second.is<picojson::object>()) continue;
			const picojson::object& oId = itId->second.get<picojson::object>();
			picojson::object::const_iterator itSB = oId.find("specialBuildingType");
			if (itSB == oId.end() || !itSB->second.is<std::string>()) continue;
			const int sb = GC.getInfoTypeForString(itSB->second.get<std::string>().c_str(), true);
			if (sb < 0) continue;
			g_buildingGroup[i] = sb;
			g_groupMembers[sb].push_back(i);
		}
		// PASS 2: group -> cap, from each grouped SpecialBuilding's JSON `allowed` (only the ~dozens with members).
		for (std::map<int, std::vector<int> >::const_iterator it = g_groupMembers.begin(); it != g_groupMembers.end(); ++it)
		{
			const char* szSB = GC.getSpecialBuildingInfo((SpecialBuildingTypes)it->first).getType();
			CvEntityAvailability kA;
			std::string sNotes;
			if (cascadeReadJsonAvailability(szSB, kA, sNotes))
			{
				g_groupCapN[it->first]     = kA.allowedCap;   // -1 if the group is uncapped
				g_groupCapScope[it->first] = (int)kA.allowedScope;
			}
		}
	}
} // namespace

bool cascadeBuildingGroupAllows(int iBuilding, const CvCascadeContext& kCtx)
{
	rjBuildGroupIndex();
	std::map<int, int>::const_iterator itG = g_buildingGroup.find(iBuilding);
	if (itG == g_buildingGroup.end()) return true; // not in any group
	const int sb = itG->second;
	std::map<int, int>::const_iterator itN = g_groupCapN.find(sb);
	if (itN == g_groupCapN.end() || itN->second < 0) return true; // group is uncapped
	const CountScope eScope = (CountScope)g_groupCapScope[sb];
	std::map<int, std::vector<int> >::const_iterator itM = g_groupMembers.find(sb);
	int iCount = 0;
	if (itM != g_groupMembers.end())
	{
		for (size_t k = 0; k < itM->second.size(); ++k)
		{
			iCount += cascadeTally().count(COUNTDOMAIN_BUILDING, itM->second[k], eScope, kCtx.contextFor(eScope));
		}
	}
	return iCount < itN->second; // at most N of the whole group at the cap's scope
}

// REPLACES (destructive succession, enabler-spec §6): a building is REPLACED -- blocked -- when a SUCCESSOR (a
// building whose `replaces.buildings` names it) is active in the context city. Legacy CvCity.cpp:2917. The successor
// carries `replaces`; invert ONCE into predecessor -> [successors] by scanning building JSONs, then check
// isActiveBuilding in the city. The verdict's missing destructive `replaces` subtraction -- the obsolete index's sibling.
namespace
{
	std::map<int, std::vector<int> > g_replacedBy;   // predecessor buildingIdx -> [successor buildingIdx]
	bool g_replBuilt = false;

	void rjBuildReplaceIndex()
	{
		if (g_replBuilt) return;
		g_replBuilt = true;
		const int iNum = GC.getNumBuildingInfos();
		for (int i = 0; i < iNum; ++i) // i = the SUCCESSOR
		{
			std::string sContent;
			if (!rjLocateEntityJson(GC.getBuildingInfo((BuildingTypes)i).getType(), sContent)) continue;
			picojson::value root;
			if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator itR = o.find("replaces");
			if (itR == o.end() || !itR->second.is<picojson::object>()) continue;
			const picojson::object& oR = itR->second.get<picojson::object>();
			picojson::object::const_iterator itB = oR.find("buildings");
			if (itB == oR.end() || !itB->second.is<picojson::array>()) continue;
			const picojson::array& a = itB->second.get<picojson::array>();
			for (size_t k = 0; k < a.size(); ++k)
			{
				if (!a[k].is<std::string>()) continue;
				const int pred = GC.getInfoTypeForString(a[k].get<std::string>().c_str(), true);
				if (pred >= 0) g_replacedBy[pred].push_back(i);
			}
		}
	}
}

bool cascadeIsReplacedInCity(int iBuilding, const CvCascadeContext& kCtx)
{
	rjBuildReplaceIndex();
	std::map<int, std::vector<int> >::const_iterator it = g_replacedBy.find(iBuilding);
	if (it == g_replacedBy.end()) return false;
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS || kCtx.iCity < 0) return false;
	const CvCity* c = GET_PLAYER((PlayerTypes)kCtx.iPlayer).getCity(kCtx.iCity);
	if (c == NULL) return false;
	for (size_t k = 0; k < it->second.size(); ++k)
		if (c->isActiveBuilding((BuildingTypes)it->second[k])) return true;
	return false;
}

// GENERATION (forward enables tech-gate): is this entity reachable -- i.e. has the team researched a tech whose
// JSON `enables` names it? If the entity has NO tech enabler (enabled by a building/civic/always), this does not
// gate (returns true) -- a non-tech enable is the deeper enables-generation pass. eDomain: COUNTDOMAIN_* values.
bool cascadeTechReachable(int eDomain, int iEntity, int iTeam)
{
	if (iTeam < 0 || iTeam >= MAX_TEAMS) return true; // no team context -> don't gate
	rjBuildTechIndex();
	const std::map<int, std::vector<int> >& m = (eDomain == COUNTDOMAIN_UNIT) ? g_enableUnitTechs : g_enableBuildingTechs;
	std::map<int, std::vector<int> >::const_iterator it = m.find(iEntity);
	if (it == m.end() || it->second.empty()) return true; // not tech-enabled -> not gated on tech
	const CvTeam& kTeam = GET_TEAM((TeamTypes)iTeam);
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (kTeam.isHasTech((TechTypes)it->second[i])) return true; // an enabling tech is researched
	}
	return false; // tech-enabled but none of the enablers researched -> not reachable
}

// ===================== GENERATION (units): the all-branches-alive upgrade resolver =====================
// Our clean model of CvCity::allUpgradesAvailable's intent. buildable() is the cascade base (requires+cap+
// obsolete, NOT the upgrade rule). A unit hides only when EVERY upgrade branch is alive (fully superseded).

namespace
{
	std::map<int, std::vector<int> > g_upgradesTo; // unitIdx -> direct upgrade targets (succession.upgradesTo)
	bool g_upgradeBuilt = false;

	// Parsed-availability cache (static JSON, game-state-independent) -- so a full-roster sweep + the chain
	// recursion don't re-parse the same unit JSON thousands of times.
	std::map<int, CvEntityAvailability> g_unitAvailCache;
	std::map<int, bool> g_unitAvailKnown; // unitIdx -> has-JSON

	const CvEntityAvailability* rjUnitAvail(int iUnit)
	{
		std::map<int, bool>::iterator k = g_unitAvailKnown.find(iUnit);
		if (k != g_unitAvailKnown.end()) return k->second ? &g_unitAvailCache[iUnit] : NULL;
		CvEntityAvailability kAvail;
		std::string sNotes;
		const char* szType = GC.getUnitInfo((UnitTypes)iUnit).getType();
		const bool bOk = cascadeReadJsonAvailability(szType, kAvail, sNotes);
		g_unitAvailKnown[iUnit] = bOk;
		if (bOk) g_unitAvailCache[iUnit] = kAvail;
		return bOk ? &g_unitAvailCache[iUnit] : NULL;
	}

	void rjBuildUpgradeIndex()
	{
		if (g_upgradeBuilt) return;
		g_upgradeBuilt = true;
		const int iNumUnits = GC.getNumUnitInfos();
		for (int i = 0; i < iNumUnits; ++i)
		{
			const char* szType = GC.getUnitInfo((UnitTypes)i).getType();
			std::string sContent;
			if (!rjLocateEntityJson(szType, sContent)) continue;
			picojson::value root;
			if (!picojson::parse(root, sContent).empty() || !root.is<picojson::object>()) continue;
			const picojson::object& o = root.get<picojson::object>();
			picojson::object::const_iterator it = o.find("succession");
			if (it == o.end() || !it->second.is<picojson::object>()) continue;
			const picojson::object& succ = it->second.get<picojson::object>();
			picojson::object::const_iterator itU = succ.find("upgradesTo");
			if (itU == succ.end() || !itU->second.is<picojson::array>()) continue;
			const picojson::array& a = itU->second.get<picojson::array>();
			for (size_t k = 0; k < a.size(); ++k)
			{
				if (!a[k].is<std::string>()) continue;
				const int idx = GC.getInfoTypeForString(a[k].get<std::string>().c_str(), true);
				if (idx >= 0) g_upgradesTo[i].push_back(idx);
			}
		}
	}

	// The cascade BASE buildability (requires + cap + not-obsolete) -- the recursion's base case, NOT the upgrade rule.
	bool rjUnitBuildable(int iUnit, const CvCascadeContext& kCtx)
	{
		if (iUnit < 0 || iUnit >= GC.getNumUnitInfos()) return false;
		const CvEntityAvailability* pAvail = rjUnitAvail(iUnit);
		if (pAvail == NULL) return false; // no JSON -> can't assess -> not buildable
		if (pAvail->spawnOnly) return false; // wildlife/spawned -> not player-buildable (clean flag, not the iCost==-1 hack)
		if (!cascadeBuildable(*pAvail, COUNTDOMAIN_UNIT, iUnit, kCtx)) return false; // requires.build now carries the AND techs (isHasTech)
		const int iTeam = (kCtx.iPlayer >= 0) ? (int)GET_PLAYER((PlayerTypes)kCtx.iPlayer).getTeam() : -1;
		// NOTE: tech is enforced via requires.build TECH atoms above (the multi-tech AND confirm). The old
		// cascadeTechReachable() was an OR-over-enables stopgap that over-offered any unit sharing one of several
		// enabling techs (the MODERN_ARMOR/tank-line bug) -- enables cannot encode AND, so it is gone from the gate.
		if (cascadeIsObsoleteForTeam(COUNTDOMAIN_UNIT, iUnit, iTeam)) return false; // obsoletion
		return true;
	}

	// A branch from v is ALIVE if v is buildable OR some upgrade of v has an alive branch. Memoized per query;
	// the in-progress 0 doubles as a cycle-guard (a back-edge contributes no aliveness).
	bool rjBranchAlive(int v, const CvCascadeContext& kCtx, std::map<int, int>& memo)
	{
		std::map<int, int>::iterator m = memo.find(v);
		if (m != memo.end()) return m->second == 1;
		memo[v] = 0;
		bool bAlive = rjUnitBuildable(v, kCtx);
		if (!bAlive)
		{
			std::map<int, std::vector<int> >::const_iterator it = g_upgradesTo.find(v);
			if (it != g_upgradesTo.end())
			{
				for (size_t i = 0; i < it->second.size(); ++i)
				{
					if (rjBranchAlive(it->second[i], kCtx, memo)) { bAlive = true; break; }
				}
			}
		}
		memo[v] = bAlive ? 1 : 0;
		return bAlive;
	}
} // namespace

bool cascadeUnitTrainable(int iUnit, const CvCascadeContext& kCtx)
{
	if (!rjUnitBuildable(iUnit, kCtx)) return false; // base: must be buildable at all
	rjBuildUpgradeIndex();

	std::map<int, std::vector<int> >::const_iterator it = g_upgradesTo.find(iUnit);
	if (it == g_upgradesTo.end() || it->second.empty()) return true; // no upgrades -> top of chain -> kept

	// hidden iff EVERY direct upgrade branch is alive; kept if any branch is dead (the fall-back band)
	std::map<int, int> memo;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (!rjBranchAlive(it->second[i], kCtx, memo)) return true; // a dead branch keeps this unit on the list
	}
	return false; // all branches alive -> fully superseded -> hidden
}

// ===================== the doTurn shadow harness (buildings, vs the capital city) =====================

namespace
{
	void rjLogLine(const CvString& s)
	{
		gDLL->logMsg("Cascade.log", s.c_str());
		streamLogTee(1, s.c_str());
	}

	int rjEngineCount(int iBuilding, CountScope eScope, int iCtxPlayer)
	{
		int iSum = 0;
		for (int p = 0; p < MAX_PLAYERS; ++p)
		{
			const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
			if (!kP.isAlive()) continue;
			if (eScope == COUNTSCOPE_EMPIRE && p != iCtxPlayer) continue;
			if (eScope == COUNTSCOPE_TEAM && iCtxPlayer >= 0 &&
				kP.getTeam() != GET_PLAYER((PlayerTypes)iCtxPlayer).getTeam()) continue;
			iSum += kP.getBuildingCount((BuildingTypes)iBuilding);
		}
		return iSum;
	}

	void rjRunOne(const char* szTypeKey, int iPlayer)
	{
		const int iBuilding = GC.getInfoTypeForString(szTypeKey, true);
		if (iBuilding < 0) { rjLogLine(CvString::format("[READJSON] %s -- type not loaded this game", szTypeKey)); return; }

		CvEntityAvailability kAvail;
		std::string sNotes;
		if (!cascadeReadJsonAvailability(szTypeKey, kAvail, sNotes))
		{
			rjLogLine(CvString::format("[READJSON] %s -- %s", szTypeKey, sNotes.c_str()));
			return;
		}

		CvCity* pCap = (iPlayer >= 0) ? GET_PLAYER((PlayerTypes)iPlayer).getCapitalCity() : NULL;
		const int iCity = (pCap != NULL) ? pCap->getID() : -1;
		const int iTeam = (iPlayer >= 0) ? (int)GET_PLAYER((PlayerTypes)iPlayer).getTeam() : -1;
		CvCascadeContext kCtx(iPlayer, iCity);
		bool bCascade = cascadeBuildable(kAvail, COUNTDOMAIN_BUILDING, iBuilding, kCtx);
		if (pCap != NULL && pCap->hasBuilding((BuildingTypes)iBuilding)) bCascade = false;        // generation: already built here
		if (cascadeIsObsoleteForTeam(COUNTDOMAIN_BUILDING, iBuilding, iTeam)) bCascade = false;    // generation: obsolete

		CvString szCap = "cap=none";
		if (kAvail.allowedCap >= 0)
		{
			const CvBuildingInfo& kInfo = GC.getBuildingInfo((BuildingTypes)iBuilding);
			int iLegacyCap = -1; const char* szScope = "?";
			switch (kAvail.allowedScope)
			{
			case COUNTSCOPE_WORLD:  iLegacyCap = kInfo.getMaxGlobalInstances(); szScope = "world";  break;
			case COUNTSCOPE_TEAM:   iLegacyCap = kInfo.getMaxTeamInstances();   szScope = "team";   break;
			case COUNTSCOPE_EMPIRE: iLegacyCap = kInfo.getMaxPlayerInstances(); szScope = "empire"; break;
			default: break;
			}
			const int iTally = cascadeTally().count(COUNTDOMAIN_BUILDING, iBuilding, kAvail.allowedScope, kCtx.contextFor(kAvail.allowedScope));
			const int iEngine = rjEngineCount(iBuilding, kAvail.allowedScope, iPlayer);
			szCap = CvString::format("cap=%s json=%d legacy=%d %s | count tally=%d engine=%d %s",
				szScope, kAvail.allowedCap, iLegacyCap, (iLegacyCap == kAvail.allowedCap) ? "MATCH" : "DIVERGE",
				iTally, iEngine, (iTally == iEngine) ? "MATCH" : "DIVERGE");
		}

		bool bLegacy = false;
		if (pCap != NULL) bLegacy = pCap->canConstruct((BuildingTypes)iBuilding); // plain "buildable now" -- apples-to-apples
		const bool bAgree = (bCascade == bLegacy);

		rjLogLine(CvString::format("[READJSON] %s p=%d city=%d | %s | %s | cascade=%d legacy=%d %s",
			szTypeKey, iPlayer, iCity, sNotes.c_str(), szCap.c_str(), bCascade ? 1 : 0, bLegacy ? 1 : 0,
			bAgree ? "AGREE" : "DIVERGE"));
	}
} // namespace

void cascadeReadJsonSlice()
{
	if (gPlayerLogLevel < 1) return;

	int iCtx = (int)GC.getGame().getActivePlayer();
	if (iCtx < 0 || iCtx >= MAX_PLAYERS || !GET_PLAYER((PlayerTypes)iCtx).isAlive())
	{
		iCtx = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p)
		{
			if (GET_PLAYER((PlayerTypes)p).isAlive()) { iCtx = p; break; }
		}
	}

	rjRunOne("BUILDING_ABU_SIMBEL", iCtx);          // allowed:{world:1} + disabled:IS_CAPITAL + any:[HAS_TERRAIN(PEAK)]
	rjRunOne("BUILDING_ACUPUNCTURISTS_SHOP", iCtx); // requires.build.all: BUILDING_C_L_ASIAN @ city

	// increment-1 verify (modifier parse): a yield-rich sample (food/production/commerce flat+percent, some conditional).
	{
		CvEntityModifiers kMods; std::string sNotes;
		if (cascadeReadJsonModifiers("BUILDING_ANCIENT_CUSTOMS", kMods, sNotes))
			rjLogLine(CvString::format("[MODPARSE] BUILDING_ANCIENT_CUSTOMS %s", sNotes.c_str()));
	}
}

// ===================== §14 H AUTO-PLACEMENT SHADOW (B-i) =====================

void cascadeAutoPlacedRoster(std::vector<int>& outBuildings, std::vector<int>& outKind)
{
	outBuildings.clear();
	outKind.clear();
	const int iNum = GC.getNumBuildingInfos();
	std::vector<int> aKind(iNum, 0); // per-building bitmask: bit0 (1) = bAutoBuild loop, bit1 (2) = property-band

	// (1) the per-turn bAutoBuild loop (CvCity::doAutobuild, BuildingsRepo::autoBuildings)
	const std::vector<BuildingTypes>& aAuto = BuildingsRepo::get().autoBuildings();
	for (size_t i = 0; i < aAuto.size(); ++i)
	{
		const int b = (int)aAuto[i];
		if (b >= 0 && b < iNum) aKind[b] |= 1;
	}
	// (2) the per-turn property-band system (CvCity::checkPropertyBuildings; each property's PropertyBuildings)
	for (int iP = 0; iP < GC.getNumPropertyInfos(); ++iP)
	{
		const std::vector<PropertyBuilding>& aPB = GC.getPropertyInfo((PropertyTypes)iP).getPropertyBuildings();
		for (size_t i = 0; i < aPB.size(); ++i)
		{
			const int b = (int)aPB[i].eBuilding;
			if (b >= 0 && b < iNum) aKind[b] |= 2;
		}
	}
	for (int b = 0; b < iNum; ++b)
	{
		if (aKind[b] != 0) { outBuildings.push_back(b); outKind.push_back(aKind[b]); }
	}
}

const char* cascadePlacementReason(int iBuilding, const CvEntityAvailability& kA,
	const CvCascadeContext& kCtx, int iTeam, bool& bCascadeWouldPlace)
{
	bCascadeWouldPlace = false;
	if (!kA.autoBuild) return "noMarker"; // no cascade auto-placement marker (un-migrated, e.g. property-band building)
	if (cascadeIsObsoleteForTeam(COUNTDOMAIN_BUILDING, iBuilding, iTeam)) return "obsolete";
	if (cascadeIsReplacedInCity(iBuilding, kCtx)) return "replaced";
	if (!cascadeBuildingGroupAllows(iBuilding, kCtx)) return "groupCap";
	if (!cascadeEvalCondition(kA.requiresBuild, kCtx)) return "requiresBuild";
	if (!cascadeEvalCondition(kA.requiresOperate, kCtx)) return "requiresOperate";
	if (!cascadeWithinAllowed(COUNTDOMAIN_BUILDING, iBuilding, kA.allowedScope, kA.allowedCap, kCtx)) return "allowedCap";
	bCascadeWouldPlace = true;
	return "place";
}

void cascadePlacementShadow()
{
	if (gPlayerLogLevel < 1) return;

	int iCtx = (int)GC.getGame().getActivePlayer();
	if (iCtx < 0 || iCtx >= MAX_PLAYERS || !GET_PLAYER((PlayerTypes)iCtx).isAlive())
	{
		iCtx = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p)
		{
			if (GET_PLAYER((PlayerTypes)p).isAlive()) { iCtx = p; break; }
		}
	}
	if (iCtx < 0) return;

	CvPlayer& kP = GET_PLAYER((PlayerTypes)iCtx);
	const int iTeam = (int)kP.getTeam();

	std::vector<int> aRoster, aKind;
	cascadeAutoPlacedRoster(aRoster, aKind);

	// Parse each roster building's availability ONCE this turn (file IO), reuse across all the player's cities.
	std::vector<CvEntityAvailability> aAvail(aRoster.size());
	std::vector<char> aParsed(aRoster.size(), 0);
	for (size_t i = 0; i < aRoster.size(); ++i)
	{
		std::string sN;
		aParsed[i] = cascadeReadJsonAvailability(GC.getBuildingInfo((BuildingTypes)aRoster[i]).getType(), aAvail[i], sN) ? 1 : 0;
	}

	int iCities = 0, iCells = 0, iDiv = 0;
	int iIter = 0;
	for (CvCity* pCity = kP.firstCity(&iIter); pCity != NULL; pCity = kP.nextCity(&iIter))
	{
		++iCities;
		CvCascadeContext kCtx(iCtx, pCity->getID());
		for (size_t i = 0; i < aRoster.size(); ++i)
		{
			if (!aParsed[i]) continue;
			++iCells;
			const int b = aRoster[i];
			bool bCascade = false;
			const char* szReason = cascadePlacementReason(b, aAvail[i], kCtx, iTeam, bCascade);
			const bool bLegacy = pCity->hasBuilding((BuildingTypes)b);
			if (bCascade != bLegacy)
			{
				++iDiv;
				if (gPlayerLogLevel >= 2)
				{
					rjLogLine(CvString::format("[PLACEMENT] DIVERGE p=%d city=%d %s kind=%d cascade=%d legacy=%d reason=%s",
						iCtx, pCity->getID(), GC.getBuildingInfo((BuildingTypes)b).getType(),
						aKind[i], bCascade ? 1 : 0, bLegacy ? 1 : 0, szReason));
				}
			}
		}
	}
	rjLogLine(CvString::format("[PLACEMENT] p=%d roster=%d cities=%d cells=%d diverge=%d",
		iCtx, (int)aRoster.size(), iCities, iCells, iDiv));
}

// ===================== §14 H DORMANCY SHADOW (B-ii) =====================

const char* cascadeDormancyReason(const CvEntityAvailability& kAvail, const CvCascadeContext& kCtx, bool& bCascadeActive)
{
	// A built thing stays active only while requires.operate holds (cascadeOperational); empty operate == always active.
	bCascadeActive = cascadeOperational(kAvail, kCtx);
	return bCascadeActive ? "active" : "requiresOperate";
}

const char* cascadeDormancyLegacyReason(const CvCity* pCity, int iBuilding)
{
	if (pCity == NULL) return "noCity";
	if (pCity->isReligiouslyLimitedBuilding((BuildingTypes)iBuilding)) return "religiousLimit";
	if (pCity->isDisabledBuilding((short)iBuilding)) return "disabled"; // resource / replacement-suppression
	return "active";
}

void cascadeDormancyShadow()
{
	if (gPlayerLogLevel < 1) return;

	int iCtx = (int)GC.getGame().getActivePlayer();
	if (iCtx < 0 || iCtx >= MAX_PLAYERS || !GET_PLAYER((PlayerTypes)iCtx).isAlive())
	{
		iCtx = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p)
		{
			if (GET_PLAYER((PlayerTypes)p).isAlive()) { iCtx = p; break; }
		}
	}
	if (iCtx < 0) return;

	CvPlayer& kP = GET_PLAYER((PlayerTypes)iCtx);
	const int iNum = GC.getNumBuildingInfos();

	// Lazily parse each building's availability ONCE (a building present in many cities is parsed once), reused across cities.
	std::vector<CvEntityAvailability> aAvail(iNum);
	std::vector<char> aState(iNum, 0); // 0 = not tried, 1 = parsed OK, 2 = no JSON

	int iCities = 0, iCells = 0, iDiv = 0;
	int iIter = 0;
	for (CvCity* pCity = kP.firstCity(&iIter); pCity != NULL; pCity = kP.nextCity(&iIter))
	{
		++iCities;
		CvCascadeContext kCtx(iCtx, pCity->getID());
		for (int b = 0; b < iNum; ++b)
		{
			if (!pCity->hasBuilding((BuildingTypes)b)) continue; // dormancy is about BUILT things only
			if (aState[b] == 0)
			{
				std::string sN;
				aState[b] = cascadeReadJsonAvailability(GC.getBuildingInfo((BuildingTypes)b).getType(), aAvail[b], sN) ? 1 : 2;
			}
			if (aState[b] != 1) continue;
			++iCells;
			bool bCascadeActive = false;
			const char* szCascade = cascadeDormancyReason(aAvail[b], kCtx, bCascadeActive);
			const bool bLegacyActive = pCity->hasFullyActiveBuilding((BuildingTypes)b);
			if (bCascadeActive != bLegacyActive)
			{
				++iDiv;
				if (gPlayerLogLevel >= 2)
				{
					rjLogLine(CvString::format("[DORMANCY] DIVERGE p=%d city=%d %s cascadeActive=%d legacyActive=%d cascade=%s legacy=%s",
						iCtx, pCity->getID(), GC.getBuildingInfo((BuildingTypes)b).getType(),
						bCascadeActive ? 1 : 0, bLegacyActive ? 1 : 0, szCascade, cascadeDormancyLegacyReason(pCity, b)));
				}
			}
		}
	}
	rjLogLine(CvString::format("[DORMANCY] p=%d cities=%d builtCells=%d diverge=%d", iCtx, iCities, iCells, iDiv));
}

// ===================== §430 MODIFIER SHADOW (the magnitude twin of placement/dormancy) =====================

void cascadeModifierShadow()
{
	if (gPlayerLogLevel < 1) return;

	const int aFam[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
	const char* aFamName[3] = { "food", "production", "commerce" };

	// Per-player, every alive player (human AND AI -- §3.5; same per-alive-player cadence as [STATE/fin]).
	for (int p = 0; p < MAX_PLAYERS; ++p)
	{
		CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;

		int iCities = 0, iCells = 0, iDiv = 0, iWorstCare = 0;
		int aChDiv[3] = { 0, 0, 0 };
		int iIter = 0;
		for (CvCity* pCity = kP.firstCity(&iIter); pCity != NULL; pCity = kP.nextCity(&iIter))
		{
			++iCities;
			CvCascadeContext kCtx(p, pCity->getID());
			for (int f = 0; f < 3; ++f)
			{
				++iCells;
				CvModifierSlot slot;
				cascadeModifierCitySlot(aFam[f], kCtx, slot);
				const int iBase = cascadeModifierCityBase(pCity, aFam[f]);            // base + specialist (legacy parity)
				const int iCascade = cascadeModifierApply(slot, iBase);              // x1 realized (active calc-flow)
				const int iLegacy = pCity->getYieldRate100((YieldTypes)aFam[f]) / 100; // x1 realized (legacy display)
				int iCare = 0;
				const char* szCause = cascadeModifierClassify(iCascade, iLegacy, slot, iCare);
				if (iCare > iWorstCare) iWorstCare = iCare;
				if (iCascade != iLegacy)
				{
					++iDiv;
					++aChDiv[f];
					if (gPlayerLogLevel >= 2)
					{
						rjLogLine(CvString::format(
							"[MODSHADOW] DIVERGE p=%d city=%d ch=%s cascade=%d legacy=%d delta=%d flat=%d pct=%d mult=%d cause=%s care=%d/%s",
							p, pCity->getID(), aFamName[f], iCascade, iLegacy, iCascade - iLegacy,
							slot.iFlat, slot.iPercent, slot.iMultiplierX100, szCause, iCare, cascadeModifierCareName(iCare)));
					}
				}
			}
		}
		rjLogLine(CvString::format(
			"[MODSHADOW] p=%d cities=%d cells=%d diverge=%d food=%d prod=%d comm=%d worstCare=%d/%s parity=%d",
			p, iCities, iCells, iDiv, aChDiv[0], aChDiv[1], aChDiv[2],
			iWorstCare, cascadeModifierCareName(iWorstCare), cascadeModifierParityMode ? 1 : 0));
	}
}

// ===================== LIVE STATE EVENT FEED (the "cameras") =====================

void cascadeStateLog()
{
	if (gPlayerLogLevel < 1) return;

	// [STATE/game] -- the terminal/era signal so an autoplay run is narratable + end-detectable from the wire.
	// (Namespaced [STATE/*] to avoid the legacy BBAI [GAME] (GameInfo.log session header) + [DIP] (DiploAI.log) tags.)
	{
		CvGame& kG = GC.getGame();
		rjLogLine(CvString::format("[STATE/game] turn=%d state=%d era=%d winnerTeam=%d victory=%d maxTurns=%d",
			kG.getGameTurn(), (int)kG.getGameState(), (int)kG.getCurrentEra(),
			(int)kG.getWinner(), (int)kG.getVictory(), kG.getMaxTurns()));
	}

	// [STATE/fin] -- the expense side of every economy (the AI_isFinancialTrouble gate especially), per alive player.
	for (int p = 0; p < MAX_PLAYERS; ++p)
	{
		const CvPlayer& kP = GET_PLAYER((PlayerTypes)p);
		if (!kP.isAlive()) continue;
		rjLogLine(CvString::format("[STATE/fin] p=%d gold=%.0f rate=%d maint=%d civic=%d units=%.0f strike=%d finTrouble=%d",
			p, (double)kP.getGold(), kP.calculateGoldRate(), kP.getTotalMaintenance(), kP.getCivicUpkeep(),
			(double)kP.getFinalUnitUpkeep(), kP.isStrike() ? 1 : 0, kP.AI_isFinancialTrouble() ? 1 : 0));
	}

	if (gPlayerLogLevel < 2) return;

	// [STATE/dip] attitude matrix + [STATE/city] per-city accumulation layer -- the detailed feed (opt-in at level 2; can be large).
	for (int p = 0; p < MAX_PLAYERS; ++p)
	{
		const CvPlayerAI& kP = GET_PLAYER((PlayerTypes)p); // CvPlayerAI for AI_getAttitudeVal
		if (!kP.isAlive()) continue;

		CvString sAtt = CvString::format("[STATE/dip] p=%d att=", p);
		for (int q = 0; q < MAX_PLAYERS; ++q)
		{
			if (q == p) continue;
			if (!GET_PLAYER((PlayerTypes)q).isAlive()) continue;
			sAtt += CvString::format("%d:%d ", q, kP.AI_getAttitudeVal((PlayerTypes)q));
		}
		rjLogLine(sAtt);

		int iIter = 0;
		for (CvCity* c = kP.firstCity(&iIter); c != NULL; c = kP.nextCity(&iIter))
		{
			rjLogLine(CvString::format(
				"[STATE/city] p=%d id=%d pop=%d happy=%d unhappy=%d angry=%d disorder=%d occ=%d occT=%d hurryT=%d conscT=%d defyT=%d happyT=%d wltkd=%d good=%d bad=%d food=%d foodDiff=%d grow=%d gpp=%d cultRate=%d rels=%d",
				p, c->getID(), c->getPopulation(),
				c->happyLevel(), c->unhappyLevel(), c->angryPopulation(), c->isDisorder() ? 1 : 0, c->isOccupation() ? 1 : 0,
				c->getOccupationTimer(), c->getHurryAngerTimer(), c->getConscriptAngerTimer(), c->getDefyResolutionAngerTimer(), c->getHappinessTimer(),
				c->isWeLoveTheKingDay() ? 1 : 0, c->goodHealth(), c->badHealth(), c->getFood(), c->foodDifference(), c->growthThreshold(),
				c->getGreatPeopleProgress(), c->getCommerceRate(COMMERCE_CULTURE), c->getReligionCount()));
		}
	}
}
