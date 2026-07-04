//
//	MMKernel -- the shared LEAF helpers of the #430 modifier machine (see the header). Ported VERBATIM from
//	CvCascadeModifierMath.cpp's file-static mm_* helpers -- StoneBase ModifierMath.cs over the flat-deposit model.
//	Promoted to a declared static-methods surface so every Calc package reaches the ONE implementation (the
//	single-source law, patterns.md). LOGIC unchanged: only the signatures + internal call sites were rewritten.
//

#include "CvGameCoreDLL.h"
#include "CvCascadeMMKernel.h"
#include "CvJsonInfo.h"                // CvJsonInfo + CvCascadeDeposit
#include "Repos/InfoRepo.h"            // InfoRepo<CvXInfo>::get().get(id) -- the JSON info home
#include "Defines/CvGlobals.h"
#include "Engine/CvPlot.h"
#include "Engine/CvPlayer.h"
#include "Engine/CvCity.h"             // building-keyed percent iterates the city's active buildings
#include "Infos/CvBuildingInfo.h"      // GC.getBuildingInfo(b).getType() -- the address key for building-keyed deposits
#include "Engine/CvGame.h"             // GC.getGame().isOption (the trait-set option gate)
#include "Infos/CvTerrainInfo.h"
#include "Infos/CvFeatureInfo.h"
#include "Infos/CvBonusInfo.h"
#include "Infos/CvImprovementInfo.h"
#include "Infos/CvTraitInfo.h"
#include "Infos/CvCivicInfo.h"
#include "AI/CvPlayerAI.h"             // GET_PLAYER
#include "CvCascadeConditionEval.h"    // cascadeEvalCondition
#include "CvCascadeDepositIndex.h"     // DepositIndex -- the compiled deposit index (hot paths match ints)
#include <map>

// Query-side cached segment ids. A hit (>=0) is cached forever; a miss RE-LOOKS-UP each call -- the interner is
// append-only, so a re-map can turn a miss into a hit but never invalidates a cached id.
static int mmk_seg(const char* s, int& cache)
{
	if (cache < 0) cache = DepositIndex::lookupSegment(std::string(s));
	return cache;
}
static int s_segFlat = -1, s_segPercent = -1, s_segEmpire = -1, s_segBuildings = -1,
           s_segImprovements = -1, s_segTerrains = -1, s_segFeatures = -1, s_segBonusKey = -1,
           s_segPlots = -1, s_segPlot = -1;

// A deposit applies iff enabled holds (or is absent) AND disabled does NOT hold (json.md §3.9), evaluated through the
// typed-condition evaluator against the live engine ctx. MODIFIER context = the lenient flags (default): a
// {STATE_RELIGION:X} compound matches loosely (the strict-match form is the enabler's requires.build only).
bool MMKernel::applies(const CvCascadeCondition* enabled, const CvCascadeCondition* disabled, const CvCascadeEvalCtx& ec)
{
	static const CvCascadeEvalFlags kFlags;   // default: strictStateReligionForBuild=false (the modifier reading)
	if (enabled != NULL && !cascadeEvalCondition(enabled, ec, kFlags)) return false;
	if (disabled != NULL && cascadeEvalCondition(disabled, ec, kFlags)) return false;
	return true;
}

// Sum a channel's SCOPE-WIDE percent deposits (address == "<family>.<scope>", unit "percent"), gated, as HUMAN percent.
int MMKernel::sumPercent(const CvJsonInfo* d, const std::string& wantAddress, const CvCascadeEvalCtx& ec)
{
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;   // never authored anywhere
	const int unitId = mmk_seg("percent", s_segPercent);
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (!applies(dep.enabled, dep.disabled, ec)) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}

// ===================== StoneBase Calc PORT -- leaf helpers (ModifierMath.cs over the flat-deposit model) =====================
// The C++ flat-deposit model: each CvCascadeDeposit's `address` IS the dotted "<channel>.<scope>[.<member>[.<KEY>]]" path
// (readJson built it), `unit` the leaf kind, `value100` the ×100 magnitude. So a StoneBase tree-walk
// (fam.Root.Children[scope]...Magnitudes) is an address-string match here. These mirror ModifierMath.cs.
// The active trait set IS option-gated (traitData() picks the SIMPLE vs COMPLEX repo by GAMEOPTION_LEADER_COMPLEX_TRAITS,
// StoneBase ActiveTraitSet) and PURE_TRAITS IS applied (sumTrait/sumTrait100 drop off-alignment values, StoneBase PureFilter).

// Σ a unit at a scope-wide address as a HUMAN int (value100/100; StoneBase SumUnitAtScope = Σ (int)m.Value), gated.
int MMKernel::sumUnit(const CvJsonInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec)
{
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (!applies(dep.enabled, dep.disabled, ec)) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}

// Σ a unit at a scope-wide address in ×100 FIXED-POINT (value100 direct; StoneBase SumUnit100 = Σ round(human×100)),
// gated -- the OOS-correct sum for FRACTIONAL flats (a commerce −0.6 stays −60, not truncated to 0). modifier.md §2.
long MMKernel::sumUnit100(const CvJsonInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec)
{
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	long sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (!applies(dep.enabled, dep.disabled, ec)) continue;
		sum += dep.value100;
	}
	return sum;
}

// Σ a unit's UNCONDITIONED magnitudes at a scope-wide address (no enabled/disabled) -- the entity's INTRINSIC base
// (a specialist's own getYield/CommerceChange). StoneBase SumUnitUnconditioned. Human int (value100/100).
int MMKernel::sumUnconditioned(const CvJsonInfo* d, const std::string& wantAddress, const char* unit)
{
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (dep.enabled != NULL || dep.disabled != NULL) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}

// ---- the active-trait-set + PURE_TRAITS helpers (StoneBase ActiveTraitSet + PureFilter; NEVER the engine CvTraitInfo) ----

// The active trait set's CvJsonTraitInfo for trait t -- COMPLEX if GAMEOPTION_LEADER_COMPLEX_TRAITS, else SIMPLE
// (StoneBase ActiveTraitSet). The two sets collide on the engine id, so they live in separate repos; this picks by the
// live option (asserted from /state in StoneBase). NEVER the engine CvTraitInfo (its CvInfoReplacements swap is the catastrophe).
const CvJsonTraitInfo* MMKernel::traitData(int t)
{
	if (GC.getGame().isOption(GAMEOPTION_LEADER_COMPLEX_TRAITS))
	{
		const CvJsonInfo* d = InfoRepo<CvComplexTraitTag>::get().get(t);
		if (d != NULL) return static_cast<const CvJsonTraitInfo*>(d);
	}
	const CvJsonInfo* d = InfoRepo<CvTraitInfo>::get().get(t);   // the SIMPLE set (engine CvTraitInfo tag = the simple repo)
	return d != NULL ? static_cast<const CvJsonTraitInfo*>(d) : NULL;
}

// Σ a TRAIT's deposits (addr, unit) with the PURE_TRAITS sign filter (StoneBase PureFilter: under GAMEOPTION_LEADER_PURE_TRAITS
// a negative trait keeps only v<=0, a positive keeps only v>=0). sumTrait = human (value100/100); sumTrait100 = ×100.
int MMKernel::sumTrait(const CvJsonTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec)
{
	if (d == NULL) return 0;
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (!applies(dep.enabled, dep.disabled, ec)) continue;
		if (bPure && (d->negativeTrait ? dep.value100 > 0 : dep.value100 < 0)) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}
long MMKernel::sumTrait100(const CvJsonTraitInfo* d, const std::string& wantAddress, const char* unit, const CvCascadeEvalCtx& ec)
{
	if (d == NULL) return 0;
	const int wantId = DepositIndex::lookupAddress(wantAddress);
	if (wantId < 0) return 0;
	const int unitId = DepositIndex::lookupSegment(std::string(unit));
	if (unitId < 0) return 0;
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
	long sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitId || dep.addressId != wantId) continue;
		if (!applies(dep.enabled, dep.disabled, ec)) continue;
		if (bPure && (d->negativeTrait ? dep.value100 > 0 : dep.value100 < 0)) continue;
		sum += dep.value100;
	}
	return sum;
}

// ---- plot-context leaf helpers (the substrate + keyed-plot reads need ec.plot set + the PlotEval bonusFromPlot flag) ----

// Σ a source's flat deposits at an address with EXPLICIT plot-eval flags (bonusFromPlot: a bare {HAS_BONUS:X} reads THIS
// plot -- the engine's per-plot improvement bonus-yield, ModifierMath.PlotEval). The caller sets ec.plot per worked plot.
// pureSign (StoneBase PureFilter): 0 = no filter; +1 = a POSITIVE trait under PURE_TRAITS (keep only non-negative values);
// -1 = a NEGATIVE trait (keep only non-positive). Threaded through the keyed helpers (default 0 = non-trait sources unchanged).
// The shared flat-sum core over COMPILED SEGMENTS: match by segment count + each segment id, with the plot-eval
// flag (bonusFromPlot: a bare {HAS_BONUS:X} reads THIS plot -- ModifierMath.PlotEval; the caller sets ec.plot)
// and the PureFilter sign gate (+1 positive trait keeps non-negative values; -1 negative keeps non-positive).
static int mmk_sumFlatSegs(const CvJsonInfo* d, int nSeg, int s0, int s1, int s2, int s3,
	const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign, int unitFlat)
{
	CvCascadeEvalFlags f;
	f.bonusFromPlot = bonusFromPlot;
	int sum = 0;
	for (size_t i = 0; i < d->deposits.size(); ++i)
	{
		const CvCascadeDeposit& dep = d->deposits[i];
		if (dep.unitId != unitFlat || dep.nSeg != nSeg) continue;
		if (dep.seg[0] != s0 || dep.seg[1] != s1) continue;
		if (nSeg >= 3 && dep.seg[2] != s2) continue;
		if (nSeg >= 4 && dep.seg[3] != s3) continue;
		if (dep.enabled != NULL && !cascadeEvalCondition(dep.enabled, ec, f)) continue;
		if (dep.disabled != NULL && cascadeEvalCondition(dep.disabled, ec, f)) continue;
		if (pureSign > 0 && dep.value100 < 0) continue;
		if (pureSign < 0 && dep.value100 > 0) continue;
		sum += dep.value100 / 100;
	}
	return sum;
}

// KeyedMember by compiled segments at an EXPLICIT unit: Σ a source's <unit> at "<chan>.<scope>.<member>.<KEY>"
// (ModifierMath.KeyedMember). The keyed percent channels (buildRate) pass the percent segment; the flat form below.
int MMKernel::sumKeyed4U(const CvJsonInfo* d, int chanId, int scopeId, int memberId, int keyId, int unitId,
	const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign)
{
	if (d == NULL || chanId < 0 || scopeId < 0 || memberId < 0 || keyId < 0 || unitId < 0) return 0;   // never authored => 0
	return mmk_sumFlatSegs(d, 4, chanId, scopeId, memberId, keyId, ec, bonusFromPlot, pureSign, unitId);
}

// The FLAT parameterization of the above (the plot/keyed-yield walks).
int MMKernel::sumKeyed4F(const CvJsonInfo* d, int chanId, int scopeId, int memberId, int keyId,
	const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign)
{
	return sumKeyed4U(d, chanId, scopeId, memberId, keyId, mmk_seg("flat", s_segFlat), ec, bonusFromPlot, pureSign);
}

// KeyedPlotYield: Σ a source's flat keyed by THIS plot's improvement(s)/terrain/feature/bonus at a scope (the engine's
// improvementYieldChange / terrainYieldChange / per-plot-bonus addends). impKeyIds = the plot's improvement + its
// upgrade-ancestors (a building source) or just the direct improvement (civic/trait/substrate), as interned segment ids.
int MMKernel::keyedPlotYield(int chanId, const CvJsonInfo* d, int scopeId, const CvPlot* p,
	TeamTypes eTeam, const std::vector<int>& impKeyIds, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign)
{
	if (d == NULL || chanId < 0 || scopeId < 0) return 0;
	int sum = 0;
	const int mImp = mmk_seg("improvements", s_segImprovements);
	for (size_t k = 0; k < impKeyIds.size(); ++k)
		sum += sumKeyed4F(d, chanId, scopeId, mImp, impKeyIds[k], ec, bonusFromPlot, pureSign);
	if (p->getTerrainType() != NO_TERRAIN)
		sum += sumKeyed4F(d, chanId, scopeId, mmk_seg("terrains", s_segTerrains),
			DepositIndex::segIdForTerrain(p->getTerrainType()), ec, bonusFromPlot, pureSign);
	if (p->getFeatureType() != NO_FEATURE)
		sum += sumKeyed4F(d, chanId, scopeId, mmk_seg("features", s_segFeatures),
			DepositIndex::segIdForFeature(p->getFeatureType()), ec, bonusFromPlot, pureSign);
	if (p->getBonusType(eTeam) != NO_BONUS)
		sum += sumKeyed4F(d, chanId, scopeId, mmk_seg("bonus", s_segBonusKey),
			DepositIndex::segIdForBonus(p->getBonusType(eTeam)), ec, bonusFromPlot, pureSign);
	return sum;
}

// Just the IMPROVEMENT-keyed part of the above (the engine's impPlayer/impTeam accumulator inside the clamped improvement addend).
int MMKernel::keyedImprovementOnly(int chanId, const CvJsonInfo* d, int scopeId,
	const std::vector<int>& impKeyIds, const CvCascadeEvalCtx& ec, bool bonusFromPlot, int pureSign)
{
	if (d == NULL || chanId < 0 || scopeId < 0) return 0;
	int sum = 0;
	const int mImp = mmk_seg("improvements", s_segImprovements);
	for (size_t k = 0; k < impKeyIds.size(); ++k)
		sum += sumKeyed4F(d, chanId, scopeId, mImp, impKeyIds[k], ec, bonusFromPlot, pureSign);
	return sum;
}

// PlotsTargetYield: Σ a source's plots-TARGET flat that applies to THIS plot (the predicate evaluated against the plot).
int MMKernel::plotsTargetYield(int chanId, const CvJsonInfo* d, int scopeId, const CvCascadeEvalCtx& ec, int pureSign)
{
	if (d == NULL || chanId < 0 || scopeId < 0) return 0;
	const int mPlots = mmk_seg("plots", s_segPlots);
	if (mPlots < 0) return 0;
	return mmk_sumFlatSegs(d, 3, chanId, scopeId, mPlots, -1, ec, false, pureSign, mmk_seg("flat", s_segFlat));
}

// One plot-substrate entity's own plot.flat + its plot-keyed yield (a route folds the improvement's RouteYieldChanges),
// PlotEval (bonusFromPlot). StoneBase SubstratePlotYield. NULL info -> 0.
int MMKernel::substratePlotYield(int chanId, const CvJsonInfo* d, const CvPlot* p, TeamTypes eTeam,
	const std::vector<int>& directImpKeyIds, const CvCascadeEvalCtx& ec)
{
	if (d == NULL || chanId < 0) return 0;
	const int sPlot = mmk_seg("plot", s_segPlot);
	int sum = keyedPlotYield(chanId, d, sPlot, p, eTeam, directImpKeyIds, ec, true);
	if (sPlot >= 0)
		sum += mmk_sumFlatSegs(d, 2, chanId, sPlot, -1, -1, ec, true, 0, mmk_seg("flat", s_segFlat));
	return sum;
}

// The player's effective extra/less-yield threshold for a channel: the engine takes the MIN over the POSITIVE per-DEPOSIT
// magnitudes of {thresholdFamily}.empire.{channel}.flat across the player's active traits + civics (StoneBase
// MinPositiveThreshold: `if (v>0 && (best==0 || v<best)) best=v` over INDIVIDUAL magnitudes, NOT a per-source sum). 0 ->
// no threshold. PURE_TRAITS gate (StoneBase): a source-level filter by threshold FAMILY -- a lessYieldThreshold is a
// DOWNSIDE (dropped from a non-negative trait), an extraYieldThreshold is an UPSIDE (dropped from a negative trait);
// civics carry no alignment (never filtered).
int MMKernel::minPosThreshold(const char* thresholdFamily, const std::string& channel, const CvPlayer& player, const CvCascadeEvalCtx& ec)
{
	const int wantId = DepositIndex::lookupAddress(std::string(thresholdFamily) + ".empire." + channel);
	if (wantId < 0) return 0;   // never authored anywhere
	const int unitFlat = mmk_seg("flat", s_segFlat);
	const bool bPure = GC.getGame().isOption(GAMEOPTION_LEADER_PURE_TRAITS);
	const bool bLess = (std::string(thresholdFamily) == "lessYieldThreshold");
	const bool bExtra = (std::string(thresholdFamily) == "extraYieldThreshold");
	int best = 0;
	for (int t = 0; t < GC.getNumTraitInfos(); ++t)
	{
		if (!player.hasTrait((TraitTypes)t)) continue;
		const CvJsonTraitInfo* d = traitData(t);   // the option-gated active set (simple/complex), StoneBase ActiveTraitSet
		if (d == NULL) continue;
		if (bPure)   // StoneBase threshold-family pure gate (source-level, by family), NOT the value-sign sumTrait filter
		{
			if (bLess && !d->negativeTrait) continue;   // a downside threshold is removed from a non-negative trait
			if (bExtra && d->negativeTrait) continue;    // an upside threshold is removed from a negative trait
		}
		for (size_t i = 0; i < d->deposits.size(); ++i)   // per-DEPOSIT MIN over the family's magnitudes (StoneBase)
		{
			const CvCascadeDeposit& dep = d->deposits[i];
			if (dep.unitId != unitFlat || dep.addressId != wantId) continue;
			if (!applies(dep.enabled, dep.disabled, ec)) continue;
			const int v = dep.value100 / 100;
			if (v > 0 && (best == 0 || v < best)) best = v;
		}
	}
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (d == NULL) continue;
		for (size_t i = 0; i < d->deposits.size(); ++i)   // civics: no alignment, no pure filter
		{
			const CvCascadeDeposit& dep = d->deposits[i];
			if (dep.unitId != unitFlat || dep.addressId != wantId) continue;
			if (!applies(dep.enabled, dep.disabled, ec)) continue;
			const int v = dep.value100 / 100;
			if (v > 0 && (best == 0 || v < best)) best = v;
		}
	}
	return best;
}

// Σ CIVIC BUILDING-KEYED percent for the city's ACTIVE buildings -- the engine's owner.getBuildingCommerceModifier
// (civic-fed) folded into modBuilding per built building (StoneBase BuildingKeyedSourcePercent). INVERTED on the
// compiled index (was O(allBuildings × civics × deposits) with a string build per building): each adopted civic's
// <channel>.empire.buildings.<B>.percent deposits are ledgered ONCE by their compiled targetFk, then summed over
// the target types ACTIVE (non-dormant constructed) in this city -- identical (b,c,dep) triples, no string touch.
int MMKernel::buildingKeyedSourcePercent(const std::string& channel, const CvCity* pCity, const CvCascadeEvalCtx& ec)
{
	const int chanId = DepositIndex::lookupSegment(channel);
	if (chanId < 0) return 0;
	const int unitPct = mmk_seg("percent", s_segPercent);
	const int sEmpire = mmk_seg("empire", s_segEmpire);
	const int mBuildings = mmk_seg("buildings", s_segBuildings);
	if (sEmpire < 0 || mBuildings < 0) return 0;
	const CvPlayer& player = GET_PLAYER(pCity->getOwner());
	std::map<int, int> ledger;   // target building id -> Σ human percent (gated)
	for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
	{
		const CivicTypes c = player.getCivics((CivicOptionTypes)co);
		if (c == NO_CIVIC) continue;
		const CvJsonInfo* d = InfoRepo<CvCivicInfo>::get().get((int)c);
		if (d == NULL) continue;
		for (size_t i = 0; i < d->deposits.size(); ++i)
		{
			const CvCascadeDeposit& dep = d->deposits[i];
			if (dep.unitId != unitPct || dep.nSeg != 4) continue;
			if (dep.seg[0] != chanId || dep.seg[1] != sEmpire || dep.seg[2] != mBuildings) continue;
			if (dep.targetFk < 0) continue;
			if (!applies(dep.enabled, dep.disabled, ec)) continue;
			ledger[dep.targetFk] += dep.value100 / 100;
		}
	}
	int sum = 0;
	for (std::map<int, int>::const_iterator it = ledger.begin(); it != ledger.end(); ++it)
		if (cascadeIsBuildingActive(it->first, ec)) sum += it->second;   // non-dormant constructed (StoneBase ConstructedBuildings \ Dormant)
	return sum;
}

// getModifiedIntValue port (CvGameCoreDLL.cpp:691): mod>0 -> v×(100+mod)/100; mod<0 -> v×100/(100-mod); else v.
int MMKernel::modifiedInt(int v, int mod) { return mod > 0 ? v * (100 + mod) / 100 : (mod < 0 ? v * 100 / (100 - mod) : v); }
