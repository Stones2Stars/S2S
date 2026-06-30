#pragma once
#ifndef CV_CASCADE_DATA_H
#define CV_CASCADE_DATA_H

//
//	CvCascadeData -- the #430 cascade's data mapped FROM an entity's curated JSON (the owner mapping/cutover ruling
//	2026-06-29: readJson maps each entity's JSON to new cascade data; the new calc reads it; the OLD XML-populated Info
//	members are cut only after shadow parity). Shape mirrors json.md + StoneBase's typed model: modifier-family
//	deposits (§6), the requires BoolExpr trees (§4.3), the enables-family + provides edges (§4.1/§5a), the allowed caps
//	(§4.4), and the grants provisions (§5).
//
//	⛔ HOME (target, owner ruling 2026-06-30) = a NEW APPENDED MEMBER on each CORRECT, SPECIFIC info class
//	(`CvBuildingInfo`/`CvUnitInfo`/…), NOT a side-table and NOT a `CvInfoBase` member. The standard EXE-required fields
//	(`type`/description/the DllExport getters) are REUSED from the info, never duplicated here; this struct holds ONLY
//	the genuinely-new data. Why: (a) widening the **base** `CvInfoBase` crashes the EXE (it binds the base layout — a
//	memcpy AV in `std::string::assign` off a shifted member), but **appending to a DERIVED class is ABI-safe** (the EXE
//	reads each info's existing members at their original offsets; a trailing member is untouched — the standard C2C way);
//	(b) a direct member is **faster** than a side-table map lookup in the modifier's hot per-source loops; (c) it makes
//	**provenance obvious** (`info->…` vs a separate map). Access is TYPED (readJson + the machines dispatch by type via
//	`GC.get*Info(id)`), so no base virtual getter is needed.
//
//	⏳ CURRENT (interim, to be replaced): readJson maps into this struct held in a SIDE-TABLE keyed by `CvInfoBase*`
//	(`cascadeForInfo`/`cascadeAttach`) — the over-correction taken after the base-widening crash ("no `CvInfo` member"
//	instead of "no *base* member"). It works + is verified, but it is the slower/less-discoverable shim; the redesign
//	moves this onto the per-derived appended members above.
//
//	⚠ PUBLIC fields BY DESIGN (owner 2026-06-29): direct access during the build/shadow phase. Owns its BoolExpr trees
//	(freed in the dtor) -- NONCOPYABLE.
//

#include "Infrastructure/BoolExpr.h"
#include <string>
#include <vector>
#include <map>

class CvInfoBase;

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

class CvCascadeData
{
public:
	CvCascadeData() : requiresBuild(NULL), requiresOperate(NULL) {}
	~CvCascadeData();

	// ⛔ This holds ONLY the genuinely-NEW cascade data (no legacy home). It does NOT duplicate the standard fields the
	// game object already carries (owner ruling 2026-06-30): the EXE-required `CvInfo` fields — `type` (getType()),
	// description, button, the DllExport-backed getters — are REUSED from the keyed `CvInfo`, never copied here. The
	// side-table is keyed by `CvInfoBase*`, so every consumer already has the info (and thus its `type`/standard fields).

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

private:
	CvCascadeData(const CvCascadeData&);                   // noncopyable -- owns the BoolExpr trees
	CvCascadeData& operator=(const CvCascadeData&);
};

// The side-table: the cascade data for a game object, keyed by its `CvInfoBase*` (the ABI-safe home; see top). readJson
// populates via cascadeAttach; the machines read via cascadeForInfo. cascadeAttach takes ownership and replaces+frees
// any existing entry for that info. cascadeForInfo returns NULL when nothing is mapped.
CvCascadeData* cascadeForInfo(const CvInfoBase* pInfo);
void cascadeAttach(const CvInfoBase* pInfo, CvCascadeData* pData);

#endif // CV_CASCADE_DATA_H
