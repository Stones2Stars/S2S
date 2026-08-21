#pragma once
#ifndef CV_CASCADE_COMMERCE_CALC_H
#define CV_CASCADE_COMMERCE_CALC_H

//
//	CommerceCalc -- StoneBase CommerceSplit.cs + CommercePackages.cs: the §2 commerce stage. Commerce is the §1
//	commerce-YIELD split: each commerce TYPE = its slider share of the modified commerce yield (HALF 1) + the
//	base-extra free-additions ×100 (HALF 2), × the commerce percent stack + Process(=0), clamped; civil disorder
//	forces the realized rate to 0. The LIVE path is CvCascadeAccumulator's standing scope packages combined at
//	read time through combineSplit below. See patterns.md (single-source law) +
//	docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//	All §2 packages are IMPLEMENTED: the reused §1 packages (specialist/religion/golden-age/building-flat/player-extra)
//	AND shrine / corp-HQ / double-time / state-religion / corporation / building-keyed (the .cpp fully computes each from
//	the cascade identity structs + engine counts). A few still read Info CONFIG interim (shrine/corpHQ/double/corp -- the
//	`shrine`/`identity` intrinsic blocks are a readJson-mapping cleanliness follow-up, NOT a correctness gap; flagged per-fn).
//

#include "Conditions/CvConditionEval.h"   // CvCascadeEvalCtx -- the eval target for deposit conditions
#include <string>
#include <map>

class CvCity;
class CvPlayer;

class CommerceCalc
{
public:
	// The commerce-type string indexed by the CommerceTypes enum ({ "gold", "research", "culture", "espionage" }).
	static const char* channel(int eC);

	// §2 BASE: religion commerce -- Σ the city's PRESENT religions' {ch}.city.flat (StateReligion/HolyCity tables, gated). x1.
	static int religion(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: player-extra commerce ×100 -- trait CommerceChanges + heritage EraCommerceChanges, both {ch}.empire.flat (the
	// heritage era-counter gate `enabled:{ERA,min}` is evaluated by MMKernel::applies). (Traits: option-gated active set +
	// PURE_TRAITS via sumTrait100/traitData.)
	static long playerExtra(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// §2 BASE: shrine commerce ×100 -- Σ active SHRINE buildings (getGlobalReligionCommerce FK) of religion.shrine.{c} ×
	// world religion-levels (ShrinePackage; engine CvCity:12278). ⏳ INTERIM config read.
	static long shrine(const std::string& channel, const CvCity* pCity);

	// §2 BASE: corp-HQ commerce ×100 -- Σ active corp-HQ buildings (getGlobalCorporationCommerce FK) of corp.headquarters.{c}
	// × world corp-levels (CorpHQPackage; engine CvCity:12286). ⏳ INTERIM config read.
	static long corpHQ(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: state-religion commerce ×100 -- 100 × POOL × matchCount (StateReligionPackage; engine CvCity:12266-73). POOL =
	// Σ player building TYPES of count × building.getStateReligionCommerce(c); matchCount = the city's active buildings whose
	// religion == the owner's state religion. COMPUTED from building counts + config (NOT the engine pool accumulator).

	// §2 BASE: CommerceChangeDoubleTime whole-building doubling ×100 -- for each active building older than its double-time
	// threshold (game-years), ANOTHER copy of its WHOLE per-building commerce (own un-conditioned city.flat + shrine + corpHQ)
	// (DoubleExtraPackage; engine CvCity:12290). ⏳ INTERIM config read; own-flat via the un-conditioned deposit (×100, integer).
	static long doubleExtra(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: corporation commerce -- Σ active corps' getCorporationCommerceByCorporation (engine CvCity:12752): per corp,
	// iC = getCommerceChange(c)×100 + Σ prereq-bonus(getCommerceProduced(c) × city bonus-count × worldCorpMaintPct/100),
	// team-revenue-modified, ceil(÷100). Returns human (the §2 bucket ×100s it). ⏳ INTERIM config read.
	static int corporation(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// ===== the SCOPED HALVES (the scope-package fills ride these) =====
	// The CITY-ONLY base terms ×100: religion + corporation + building-own + shrine + corpHQ + doubleTime
	// (the HALF-2 base-extra sum MINUS the player-scope goldenAge/playerExtra and the keyed/SR realizations,
	// which are separate scope packages).
	static long baseOwn100(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// The state-religion POOL (player-scope: Σ owned building TYPES' count × config) -- × the city match at read.
	static long stateReligionPool(const std::string& channel, const CvPlayer& player);
	// The city's SR MATCH count (active buildings whose religion == the state religion).
	static int stateReligionMatch(const CvCity* pCity, const CvCascadeEvalCtx& ec);
	// The buildingKeyed GRANTOR LEDGER (player-scope): targetFk -> Σ count(G) × G's {ch}.empire.buildings.{B}.flat (×100).
	static void buildingKeyedLedger(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec,
		std::map<int, long>& out);

	// The CombineSplit KERNEL (CvCity:11969-11996, bit-exact): slider split of the commerce yield + the capped
	// base-extra, × the commerce percent stack, + Process (0, TODO), clamps/sentinels; disorder -> 0. The slider
	// + disorder are read LIVE here -- they need no invalidation anywhere. Single-sourced: the accumulator's
	// read-time combine calls THIS.
	static long combineSplit(CommerceTypes eC, const CvCity* pCity, long yieldCommerce100, long prodRate,
		long lBaseExtra100, int iTotalModifier);
};

#endif // CV_CASCADE_COMMERCE_CALC_H
