#pragma once
#ifndef CV_JSON_INFO_H
#define CV_JSON_INFO_H

//
//	CvJsonInfo -- the data mapped FROM an entity's curated JSON: the JSON counterpart to the engine's XML `CvXInfo`
//	objects. readJson parses each `Assets/Data/<type>/*.json` into one of these; the cascade machines (modifier /
//	enabler) consume it. Shape mirrors json.md + StoneBase's typed model: modifier-family deposits (§6), the requires
//	condition trees (§4.3), the enables-family + provides edges (§4.1/§5a), the allowed caps (§4.4), the grants (§5).
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
//	⚠ PUBLIC fields BY DESIGN (owner 2026-06-29): direct access during the build/shadow phase. Owns its typed condition
//	trees (freed in the dtor) -- NONCOPYABLE.
//

#include "CvCascadeCondition.h"   // CvCascadeCondition -- the StoneBase-ported typed condition tree (was BoolExpr)
#include <string>
#include <vector>
#include <map>
#include <set>

namespace picojson { class value; }   // mapFrom's input -- full definition only in the .cpp (via the PCH umbrella)

// A modifier-family deposit (json.md §6): `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` =
// value (×100 at the leaf). The address segments are kept as strings (the modifier machine resolves them); the
// conditioning (`enabled`/`disabled`) is a typed CvCascadeCondition (the StoneBase port); `per` is flagged (the
// count-scaler is resolved by the machine).
struct CvCascadeDeposit
{
	// The deposit ADDRESS as a dotted path `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>]` — kept
	// generic (the modifier machine classifies the segments, mirroring StoneBase's parser). `unit` is the leaf kind.
	std::string address;    // e.g. "food.city", "production.empire.plots", "food.city.improvements.IMPROVEMENT_FARM"
	std::string unit;       // flat / percent / multiplier / perPopulation / ...
	int value100;           // the single human->×100 leaf value
	CvCascadeCondition* enabled;   // NULL = always-on
	CvCascadeCondition* disabled;  // NULL = never-suppressed
	bool hasPer;            // a per count-scaler is present (resolved by the modifier machine)
	std::vector<int> perAnyOf;   // per:{anyOf:[...]} resolved type ids (e.g. the corp CommercesProduced prereq-bonus scaler)

	// --- COMPILED ints (DepositIndex::compile at readJson push-time; the strings above stay for diagnostics) ---
	// The load-time strings->ints compile (modifier-substrate.md): hot-path matchers compare these, never the
	// strings. The segments also generate the data-derived event->cache routing (state-repositories.md end-state).
	enum { CASC_DEP_SEGS = 4 };
	int addressId;              // whole-address identity (interned) — an exact-address match is ONE int compare
	int unitId;                 // interned unit segment
	int seg[CASC_DEP_SEGS];     // interned dotted segments (family, scope, s3, s4); -1 where absent
	int nSeg;                   // total segment count (seg[] holds the first CASC_DEP_SEGS)
	int targetFk;               // engine info id of the LAST segment when it is a resolvable INFOTYPE key (else -1)

	CvCascadeDeposit() : value100(0), enabled(NULL), disabled(NULL), hasPer(false),
		addressId(-1), unitId(-1), nSeg(0), targetFk(-1)
	{
		for (int i = 0; i < CASC_DEP_SEGS; ++i) seg[i] = -1;
	}
};

// A structured `grants.repeatable` entry (json.md §5): a recurring provision fired each `interval` -- a spawned unit,
// a per-turn heal, or a per-turn PROPERTY_* source (the #429 spatial pulse). Exactly one payload kind is set; the
// generic id-only capture (grantLists["repeatable"]) missed the heal + property entries entirely. Magnitudes ×100.
struct CvCascadeGrantRepeatable
{
	int unitId;          // spawn: the UNIT_ id (-1 = not a spawn)
	int unitCombatId;    // heal: the UNITCOMBAT_ class healed (-1 = not a heal-by-combat)
	int propertyId;      // property-pulse: the PROPERTY_ id (-1 = not a property source)
	int propertyAmount;  // property-pulse: signed per-turn amount (×100)
	int heal;            // heal amount (×100); healFull overrides
	bool healFull;       // heal: "full"
	int count;           // entry count (default 1)
	int intervalTurns;   // interval: "perTurn" -> 1; { perTurn: N } -> N
	int chancePerId;     // chance: { per: <type> } -> the scaler type id (-1 = unconditional)
	int distance;        // #429 spatial: radius (0 = none)
	std::string on;      // #429 spatial: the GameObject target ("plot", ...)
	std::string relation;// #429 spatial: "near", ...
	CvCascadeGrantRepeatable() : unitId(-1), unitCombatId(-1), propertyId(-1), propertyAmount(0), heal(0),
		healFull(false), count(1), intervalTurns(1), chancePerId(-1), distance(0) {}
};

class CvJsonInfo
{
public:
	CvJsonInfo() : requiresBuild(NULL), requiresOperate(NULL) {}
	virtual ~CvJsonInfo();   // virtual: the per-type subclasses below are owned + deleted via this CvJsonInfo* base (InfoRepo)
	void clear();   // free the owned condition trees + reset every container (for re-map safety of a persistent instance)

	// Load THIS info from its curated JSON entity object -- "the info loads itself" (audit 2026-07-01, owner). The BASE
	// parses the COMMON cascade sections shared by every entity: the modifier-family deposits (§6), the enables-family +
	// provides edges (§4.1/§4.2/§5a), the allowed caps (§4.4), the grants (§5), and requires.build/operate + dormant
	// (§4.3). A per-type SUBCLASS overrides this to call CvJsonInfo::mapFrom(entity) FIRST, then parse its ONE extension
	// block (unit skills/tags, tech capabilities, civic policies, building identity, ...). So the reader stops being a
	// god-function that knows every type -- each type owns its own parsing, drawing shared primitives from CvCascadeJsonParse.
	virtual void mapFrom(const picojson::value& entity);

	// Holds ONLY the genuinely-NEW JSON data. The standard EXE-required fields (`type`/getType(), description, button,
	// the DllExport getters) are NOT duplicated -- they stay on the engine `CvXInfo` at the same id; a consumer that has
	// this CvJsonInfo (by domain+id, via InfoRepo) also has the engine info at that id, and reads the standard fields there.

	// --- Effects (modifier families, §6) ---
	std::vector<CvCascadeDeposit> deposits;

	// --- Availability (§4) ---
	CvCascadeCondition* requiresBuild;                     // requires.build tree (NULL if none)  -- greys
	CvCascadeCondition* requiresOperate;                   // requires.operate tree (NULL if none) -- greys + dormancy
	std::map<std::string, std::vector<int> > edges;        // "<edge>.<bucket>" -> [resolved ids]: enables/obsoletes/
	                                                       // replaces/disables/obsoletedBy/provides (§4.1/§4.2/§5a)
	std::map<std::string, int> allowed;                    // cap kind (world/team/empire/worldWonders/...) -> N (§4.4)
	std::vector<int> dormantTriggers;                      // the requires.{operate|build}.dormant trigger ids (StoneBase
	                                                       // DormantTriggers): buildings = the successor buildings whose
	                                                       // presence dorms this; units = the direct upgrades (§4.3).
	                                                       // Extracted SEPARATELY -- the condition parser drops `dormant`.

	// --- Provisions (grants, §5) ---
	std::map<std::string, std::vector<int> > grantLists;   // "<bucket>" -> [resolved ids] (techs/units/foundBuildings/...)
	std::map<std::string, int> grantPulses;                // numeric pulse channel -> value (×100 at leaf)
	std::set<std::string> grantFlags;                      // bool grants ("goldenAge": true) -- a flag handed out on the trigger
	std::map<std::string, std::map<std::string, int> > grantScopedPulses;  // "<channel>" -> "<scope>" -> value (×100), e.g.
	                                                       // population {city|empire:N}. Object-VALUED dicts (deferred
	                                                       // mission-keys like greatPersonAction) are NOT captured here.
	std::vector<CvCascadeGrantRepeatable> grantRepeatables;   // structured `repeatable` entries (spawn / heal / property-pulse)

	// NB the CLASSIFICATION blocks are NOT on the base -- each lives on the ONE type that owns it (group-unambiguity,
	// owner 2026-07-01): empire `capabilities` -> CvJsonTechInfo (§8); unit `skills`/`tags` -> CvJsonUnitInfo (§8);
	// `skills` -> CvJson{Promotion,UnitCombat}Info; `policies` (pure empire STATE, §9) -> CvJsonCivicInfo / CvJsonTraitInfo.

private:
	CvJsonInfo(const CvJsonInfo&);                         // noncopyable -- owns the condition trees
	CvJsonInfo& operator=(const CvJsonInfo&);
};

// Per-type cascade info SUBCLASSES (owner ruling 2026-06-30; ONE class per file, mirroring StoneBase's Domain/Infos): the
// type-specific JSON data lives on the type's OWN class -- the cascade NEVER reads the engine CvXInfo for it (ESPECIALLY
// CvTraitInfo: its runtime CvInfoReplacements swap can't represent the clean simple/complex split). Each in its own header:
//   CvJsonTraitInfo.h (+ CvJsonSimpleTraitInfo.h / CvJsonComplexTraitInfo.h), CvJsonBuildingInfo.h, CvJsonReligionInfo.h,
//   CvJsonCorporationInfo.h, CvJsonUnitInfo.h.
// InfoRepo creates the right subclass per type (Repos/InfoRepo.h JsonPayload).

// The synthetic `TECH_GAME_START` root (json/naming: deliberately NOT in the engine XML -- the unified way to define
// what is available at start with NO tech prereq, avoiding special-case reverse-lookup `requires`). It has no engine
// `TechTypes` id (a readJson non-resolver), so it lives HERE, off the InfoRepo: readJson maps its `enables` into this
// single instance, and the enabler seeds GENERATE from it for EVERY player (every civ grants it via `grants.techs`).
CvJsonInfo& cascadeStartNode();

#endif // CV_JSON_INFO_H
