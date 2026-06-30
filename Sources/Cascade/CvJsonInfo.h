#pragma once
#ifndef CV_JSON_INFO_H
#define CV_JSON_INFO_H

//
//	CvJsonInfo -- the data mapped FROM an entity's curated JSON: the JSON counterpart to the engine's XML `CvXInfo`
//	objects. readJson parses each `Assets/Data/<type>/*.json` into one of these; the cascade machines (modifier /
//	enabler) consume it. Shape mirrors json.md + StoneBase's typed model: modifier-family deposits (§6), the requires
//	BoolExpr trees (§4.3), the enables-family + provides edges (§4.1/§5a), the allowed caps (§4.4), the grants (§5).
//
//	⛔ HOME (owner ruling 2026-06-30) = the per-info-type **InfoRepo** (`Repos/InfoRepo.h`): a `get()` singleton per
//	info type holding a `std::vector<CvJsonInfo*>` PARALLEL to the engine's `GC.m_pa<X>Info`, indexed by the same id.
//	The JSON info data is therefore a SEPARATE, uniform layer -- NOT a member on `CvInfoBase` and NOT the old side-table.
//	Why a separate layer (not on the info object): it keeps the migration boundary clean (the engine's XML info stays
//	pure; the XML-vs-JSON shadow is two structures, swapped cleanly at cutover), it is immune to the `CvInfoReplacements`
//	info-pointer swap (a parallel array indexed by id stays put), and access is standardized + O(1). This object holds
//	ONLY the genuinely-new JSON data; the standard fields (`type`, description, …) stay on the engine `CvXInfo`, reached
//	via its `DllExport` getters at the same id -- never duplicated here.
//
//	⚠ PUBLIC fields BY DESIGN (owner 2026-06-29): direct access during the build/shadow phase. Owns its BoolExpr trees
//	(freed in the dtor) -- NONCOPYABLE.
//

#include "Infrastructure/BoolExpr.h"
#include <string>
#include <vector>
#include <map>
#include <set>

// A modifier-family deposit (json.md §6): `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` =
// value (×100 at the leaf). The address segments are kept as strings (the modifier machine resolves them); the
// conditioning (`enabled`/`disabled`) is a BoolExpr; `per` is flagged (the count-scaler is resolved by the machine).
struct CvCascadeDeposit
{
	// The deposit ADDRESS as a dotted path `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>]` — kept
	// generic (the modifier machine classifies the segments, mirroring StoneBase's parser). `unit` is the leaf kind.
	std::string address;    // e.g. "food.city", "production.empire.plots", "food.city.improvements.IMPROVEMENT_FARM"
	std::string unit;       // flat / percent / multiplier / perPopulation / ...
	int value100;           // the single human->×100 leaf value
	const BoolExpr* enabled;   // NULL = always-on
	const BoolExpr* disabled;  // NULL = never-suppressed
	bool hasPer;            // a per count-scaler is present (resolved by the modifier machine)
	CvCascadeDeposit() : value100(0), enabled(NULL), disabled(NULL), hasPer(false) {}
};

class CvJsonInfo
{
public:
	CvJsonInfo() : requiresBuild(NULL), requiresOperate(NULL) {}
	~CvJsonInfo();
	void clear();   // free the owned BoolExpr trees + reset every container (for re-map safety of a persistent instance)

	// Holds ONLY the genuinely-NEW JSON data. The standard EXE-required fields (`type`/getType(), description, button,
	// the DllExport getters) are NOT duplicated -- they stay on the engine `CvXInfo` at the same id; a consumer that has
	// this CvJsonInfo (by domain+id, via InfoRepo) also has the engine info at that id, and reads the standard fields there.

	// --- Effects (modifier families, §6) ---
	std::vector<CvCascadeDeposit> deposits;

	// --- Availability (§4) ---
	const BoolExpr* requiresBuild;                         // requires.build tree (NULL if none)  -- greys
	const BoolExpr* requiresOperate;                       // requires.operate tree (NULL if none) -- greys + dormancy
	std::map<std::string, std::vector<int> > edges;        // "<edge>.<bucket>" -> [resolved ids]: enables/obsoletes/
	                                                       // replaces/disables/obsoletedBy/provides (§4.1/§4.2/§5a)
	std::map<std::string, int> allowed;                    // cap kind (world/team/empire/worldWonders/...) -> N (§4.4)

	// --- Provisions (grants, §5) ---
	std::map<std::string, std::vector<int> > grantLists;   // "<bucket>" -> [resolved ids] (techs/units/foundBuildings/...)
	std::map<std::string, int> grantPulses;                // numeric pulse channel -> value (×100 at leaf)

	// --- Classification: empire capabilities (json.md §8) ---
	// The team/empire abilities this entity GRANTS when held (tech-unlocked; e.g. techTrading, foundOnPeaks). The
	// empire's ACTIVE capability set is the union over the team's held grantors -- derived (live) where consumed
	// (canFound/canBuild + the team-ability systems), not stored here.
	std::set<std::string> capabilities;                    // granted capability names (the `capabilities:{name:true}` block)

private:
	CvJsonInfo(const CvJsonInfo&);                         // noncopyable -- owns the BoolExpr trees
	CvJsonInfo& operator=(const CvJsonInfo&);
};

// The synthetic `TECH_GAME_START` root (json/naming: deliberately NOT in the engine XML -- the unified way to define
// what is available at start with NO tech prereq, avoiding special-case reverse-lookup `requires`). It has no engine
// `TechTypes` id (a readJson non-resolver), so it lives HERE, off the InfoRepo: readJson maps its `enables` into this
// single instance, and the enabler seeds GENERATE from it for EVERY player (every civ grants it via `grants.techs`).
CvJsonInfo& cascadeStartNode();

#endif // CV_JSON_INFO_H
