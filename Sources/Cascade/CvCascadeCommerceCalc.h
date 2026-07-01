#pragma once
#ifndef CV_CASCADE_COMMERCE_CALC_H
#define CV_CASCADE_COMMERCE_CALC_H

//
//	CommerceCalc -- StoneBase CommerceSplit.cs + CommercePackages.cs: the §2 commerce stage. Commerce is the §1
//	commerce-YIELD split (CommerceSplit RIDES YieldRate("commerce")): each commerce TYPE = its slider share of the modified
//	commerce yield (HALF 1) + the baseExtra free-additions ×100 (HALF 2), × the commerce percent stack + Process(=0),
//	clamped; civil disorder forces the realized rate to 0. See patterns.md (single-source law) +
//	docs/plans/structural-cleanup/modifier-machine.md.
//
//	Purely-organizational static-methods class: NO data members, never instantiated, no per-instance state.
//	⏳ PARTIAL: the BULK packages are ported (specialist/religion/golden-age/building-flat/player-extra, reusing §1); the
//	shrine / corp-HQ / double-time / state-religion / corporation / building-keyed packages are STUBBED 0 -- they need
//	readJson to map the `shrine`/`identity` intrinsic blocks + verified corp/heritage/built-year engine state (the next
//	sub-tasks). So a commerce-rate diff is EXPECTED until they land (owner: port it all, then compare). Flagged, not dropped.
//

#include "CvCascadeConditionEval.h"   // CvCascadeEvalCtx -- the eval target for deposit conditions
#include <string>

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
	// heritage era-counter gate `enabled:{ERA,min}` is evaluated by MMKernel::applies). (⏳ interim trait read -- §6.)
	static long playerExtra(const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec);

	// §2 BASE: building-keyed commerce ×100 (GlobalBuildingExtraCommerces, BuildingKeyedCommercePackage) -- a building G
	// grants commerce to OTHER building TYPES B empire-wide: Σ over the city's ACTIVE buildings B of (Σ over granting
	// buildings G of count(G) × G's {ch}.empire.buildings.{B}.flat). Pure deposits (no readJson gap). ×100.
	static long buildingKeyed(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: shrine commerce ×100 -- Σ active SHRINE buildings (getGlobalReligionCommerce FK) of religion.shrine.{c} ×
	// world religion-levels (ShrinePackage; engine CvCity:12278). ⏳ INTERIM config read.
	static long shrine(const std::string& channel, const CvCity* pCity);

	// §2 BASE: corp-HQ commerce ×100 -- Σ active corp-HQ buildings (getGlobalCorporationCommerce FK) of corp.headquarters.{c}
	// × world corp-levels (CorpHQPackage; engine CvCity:12286). ⏳ INTERIM config read.
	static long corpHQ(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: state-religion commerce ×100 -- 100 × POOL × matchCount (StateReligionPackage; engine CvCity:12266-73). POOL =
	// Σ player building TYPES of count × building.getStateReligionCommerce(c); matchCount = the city's active buildings whose
	// religion == the owner's state religion. COMPUTED from building counts + config (NOT the engine pool accumulator).
	static long stateReligion(const std::string& channel, const CvCity* pCity);

	// §2 BASE: CommerceChangeDoubleTime whole-building doubling ×100 -- for each active building older than its double-time
	// threshold (game-years), ANOTHER copy of its WHOLE per-building commerce (own un-conditioned city.flat + shrine + corpHQ)
	// (DoubleExtraPackage; engine CvCity:12290). ⏳ INTERIM config read; own-flat via the un-conditioned deposit (×100, integer).
	static long doubleExtra(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// §2 BASE: corporation commerce -- Σ active corps' getCorporationCommerceByCorporation (engine CvCity:12752): per corp,
	// iC = getCommerceChange(c)×100 + Σ prereq-bonus(getCommerceProduced(c) × city bonus-count × worldCorpMaintPct/100),
	// team-revenue-modified, ceil(÷100). Returns human (the §2 bucket ×100s it). ⏳ INTERIM config read.
	static int corporation(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec);

	// The §2 COMMERCE-SPLIT ASSEMBLER + CombineSplit kernel (CommerceSplit.cs). channel = the commerce-type string (eC index).
	static long commerceRate100(const std::string& channel, CommerceTypes eC, const CvCity* pCity, const CvCascadeEvalCtx& ec,
		long yieldCommerce100, long prodRate);
};

#endif // CV_CASCADE_COMMERCE_CALC_H
