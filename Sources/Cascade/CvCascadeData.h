#pragma once
#ifndef CV_CASCADE_DATA_H
#define CV_CASCADE_DATA_H

//
//	CvCascadeData -- the #430 cascade's data mapped FROM an entity's curated JSON onto the game object (the owner
//	mapping/cutover ruling 2026-06-29: readJson maps each entity's JSON onto NEW member variables on the game object;
//	the new calc reads them; the OLD XML-populated Info members are cut only after shadow parity). One instance hangs
//	off every `CvInfoBase` (`m_pCascade`, allocated by readJson, NULL until mapped). Shape mirrors json.md +
//	StoneBase's typed model: modifier-family deposits (§6), the requires BoolExpr trees (§4.3), the enables-family +
//	provides edges (§4.1/§5a), the allowed caps (§4.4), and the grants provisions (§5).
//
//	⚠ PUBLIC fields BY DESIGN (owner 2026-06-29): direct access during the build/shadow phase; encapsulated at the
//	cutover. Owns its BoolExpr trees (freed in the dtor) -- so it is NONCOPYABLE (it is attached post-load, after the
//	XML copyNonDefaults phase, so it is never copied).
//

#include "Infrastructure/BoolExpr.h"
#include <string>
#include <vector>
#include <map>

// A modifier-family deposit (json.md §6): `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` =
// value (×100 at the leaf). The address segments are kept as strings (the modifier machine resolves them); the
// conditioning (`enabled`/`disabled`) is a BoolExpr; `per` is flagged (the count-scaler is resolved by the machine).
struct CvCascadeDeposit
{
	std::string family;     // food / production / commerce / happiness / strength / PROPERTY_* / ...
	std::string scope;      // world / team / empire / area / city / plot / building / specialist / unit / self
	std::string target;     // a plural target (plots/units/...) or a named-entity key (improvements/...), "" if scope-wide
	std::string member;     // a grouped-family member (maintenance.distance, defense.amount), "" if none
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

#endif // CV_CASCADE_DATA_H
