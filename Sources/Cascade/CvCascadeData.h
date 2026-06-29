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
//	⛔ HOME = a SIDE-TABLE keyed by the game object (`cascadeForInfo`), NOT a member on `CvInfoBase` (owner ruling
//	2026-06-29, ABI-forced): the closed Firaxis EXE binds `CvInfoBase`'s layout (it reads the type-string member by a
//	hardcoded offset), so widening `CvInfoBase` -- even appending -- shifts member offsets and crashes the EXE on load
//	(proven: a memcpy AV in `std::string::assign` off a shifted `CvInfoBase` member). The side-table keeps the cascade
//	data fully ISOLATED from the EXE-bound `CvInfo` (cleaner anyway). Physical on-object placement, if ever wanted, is
//	a per-derived-class append at cutover -- not a base member.
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
