//
//	CvCascadeModifier -- the magnitude combine core (see CvCascadeModifier.h).
//

#include "CvGameCoreDLL.h"
#include "CvCascadeModifier.h"
#include "CvCascadeReadJson.h" // cascadeReadJsonModifiers (the parse the deposit-flow consumes)
#include <map>
#include <set>

void CvModifierSlot::deposit(ModifierUnit eUnit, int iValue)
{
	switch (eUnit)
	{
	case MODUNIT_FLAT:       iFlat += iValue;                              break;
	case MODUNIT_PERCENT:    iPercent += iValue;                          break;
	case MODUNIT_MULTIPLIER: iMultiplierX100 = iMultiplierX100 * iValue / 100; break; // compose by product
	}
}

int CvModifierSlot::effective(int iBase) const
{
	// (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100). Int math, matching the legacy accumulators;
	// the precise ordering / overflow / combine-mode handling is the #430 arithmetic pass.
	int iValue = iBase + iFlat;
	iValue = iValue * (100 + iPercent) / 100;
	iValue = iValue * iMultiplierX100 / 100;
	return iValue;
}

// ===================== the DEPOSIT-FLOW + effective read =====================

// Parity-first scaffold (R-M1): additive-only until the deposit-flow is proven against legacy.
const bool cascadeModifierParityMode = true;

// The active calculation flow (owner 2026-06-19): legacy-flat-outside now (parity can reach zero; data balanced for it).
// SWAP the model later by changing this one const (and adding a case below) -- the engine doesn't change elsewhere.
const ModifierCalcFlow cascadeModifierCalcFlow = CALCFLOW_LEGACY_FLAT_OUTSIDE;

int cascadeModifierApply(const CvModifierSlot& slot, int iBase)
{
	switch (cascadeModifierCalcFlow)
	{
	case CALCFLOW_LEGACY_FLAT_OUTSIDE:
		// building flat added OUTSIDE the percent -- matches legacy `(base)×modifier + extraYield`. Multiplier is identity
		// here (parity is additive-only); it composes only in the unified flow.
		return iBase * (100 + slot.iPercent) / 100 + slot.iFlat;
	case CALCFLOW_UNIFIED_FLAT_INSIDE:
	default:
		return slot.effective(iBase); // (base+Σflat)×(100+Σpercent)/100×Π(mult/100) -- the spec's unified model
	}
}

namespace
{
	// Lazy per-TYPE modifier caches (buildings + civics): parse each entity's JSON ONCE, reuse. Game-thread only (no
	// locking). Parse is static (the JSON files don't change in-session); the per-deposit enabled/disabled is re-eval'd live.
	std::map<int, CvEntityModifiers> g_buildingMods;
	std::set<int>                    g_buildingModsNoJson; // negative cache (no JSON / parse fail) -- don't retry
	std::map<int, CvEntityModifiers> g_civicMods;
	std::set<int>                    g_civicModsNoJson;

	const CvEntityModifiers* cachedBuildingMods(int iBuilding)
	{
		std::map<int, CvEntityModifiers>::const_iterator it = g_buildingMods.find(iBuilding);
		if (it != g_buildingMods.end()) return &it->second;
		if (g_buildingModsNoJson.find(iBuilding) != g_buildingModsNoJson.end()) return NULL;
		CvEntityModifiers kMods; std::string sNotes;
		if (!cascadeReadJsonModifiers(GC.getBuildingInfo((BuildingTypes)iBuilding).getType(), kMods, sNotes))
		{
			g_buildingModsNoJson.insert(iBuilding);
			return NULL;
		}
		g_buildingMods[iBuilding] = kMods;
		return &g_buildingMods[iBuilding];
	}

	const CvEntityModifiers* cachedCivicMods(int iCivic)
	{
		std::map<int, CvEntityModifiers>::const_iterator it = g_civicMods.find(iCivic);
		if (it != g_civicMods.end()) return &it->second;
		if (g_civicModsNoJson.find(iCivic) != g_civicModsNoJson.end()) return NULL;
		CvEntityModifiers kMods; std::string sNotes;
		if (!cascadeReadJsonModifiers(GC.getCivicInfo((CivicTypes)iCivic).getType(), kMods, sNotes))
		{
			g_civicModsNoJson.insert(iCivic);
			return NULL;
		}
		g_civicMods[iCivic] = kMods;
		return &g_civicMods[iCivic];
	}

	// A deposit applies when its `enabled` holds AND its `disabled` doesn't (empty = unconditional). Re-eval per call.
	bool depositActive(const CvCascadeModifierDeposit& d, const CvCascadeContext& kCtx)
	{
		if (!d.enabled.isEmpty() && !cascadeEvalCondition(d.enabled, kCtx)) return false;
		if (!d.disabled.isEmpty() && cascadeEvalCondition(d.disabled, kCtx)) return false;
		return true;
	}

	// Fold every deposit in pMods matching (iFamily, iScope) whose enabled/disabled hold into slotOut. Shared by the
	// building (city-scope) and civic (empire-scope) loops so the filter logic lives in ONE place.
	void foldDeposits(const CvEntityModifiers* pMods, int iFamily, int iScope, const CvCascadeContext& kCtx, CvModifierSlot& slotOut)
	{
		if (pMods == NULL) return;
		for (size_t i = 0; i < pMods->deposits.size(); ++i)
		{
			const CvCascadeModifierDeposit& d = pMods->deposits[i];
			if (d.iFamily != iFamily || d.iScope != iScope) continue;
			if (cascadeModifierParityMode && d.eUnit == MODUNIT_MULTIPLIER) continue; // additive-only in parity mode
			if (!depositActive(d, kCtx)) continue;
			slotOut.deposit(d.eUnit, d.iValue);
		}
	}
}

void cascadeModifierCitySlot(int iFamily, const CvCascadeContext& kCtx, CvModifierSlot& slotOut)
{
	slotOut.clear();
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS) return;
	CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kCtx.iPlayer);
	const CvCity* pCity = kPlayer.getCity(kCtx.iCity);
	if (pCity == NULL) return;

	// (1) the city's own PRESENT BUILDINGS -- city-scope deposits.
	const int iNumBuildings = GC.getNumBuildingInfos();
	for (int b = 0; b < iNumBuildings; ++b)
	{
		if (!pCity->hasBuilding((BuildingTypes)b)) continue;
		foldDeposits(cachedBuildingMods(b), iFamily, MODSCOPE_CITY, kCtx, slotOut);
	}

	// (2) the player's ACTIVE CIVICS -- empire-scope deposits roll DOWN to every city (modifier-spec scopes: empire =
	// the player = all cities). The first non-building yield source wired (the biggest single yield-% contributor);
	// TRAIT / TECH / building-empire sources fold the same way -- follow-ups (the incremental "add a source" method, R-M1).
	const int iNumCivicOptions = GC.getNumCivicOptionInfos();
	for (int co = 0; co < iNumCivicOptions; ++co)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)co);
		if (eCivic == NO_CIVIC) continue;
		foldDeposits(cachedCivicMods((int)eCivic), iFamily, MODSCOPE_EMPIRE, kCtx, slotOut);
	}
}

int cascadeModifierCityBase(const CvCity* pCity, int iFamily)
{
	if (pCity == NULL) return 0;
	// Legacy getYieldRate100 applies the modifier to (getBaseYieldRate + getSpecialistYieldTotal) (CvCity.cpp:11253) --
	// specialist yield gets the city yield modifier exactly like worked tiles (#317). The cascade's pre-modifier base
	// matches that pair. (Shadow stand-in: the eventual model computes specialist output from MODSCOPE_SPECIALIST deposits.)
	return pCity->getBaseYieldRate((YieldTypes)iFamily) + pCity->getSpecialistYieldTotal((YieldTypes)iFamily);
}

int cascadeModifierEffective(int iFamily, int iScope, const CvCascadeContext& kCtx)
{
	if (iScope != MODSCOPE_CITY) return 0; // PILOT: city scope only (plot/other scopes are later sub-passes)
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS) return 0;
	const CvCity* pCity = GET_PLAYER((PlayerTypes)kCtx.iPlayer).getCity(kCtx.iCity);
	if (pCity == NULL) return 0;

	CvModifierSlot slot;
	cascadeModifierCitySlot(iFamily, kCtx, slot);
	return cascadeModifierApply(slot, cascadeModifierCityBase(pCity, iFamily));
}

// ===================== the SHADOW classifier (cause-tag + care level) =====================

const char* cascadeModifierCareName(int iCare)
{
	switch (iCare)
	{
	case CARE_FINE:     return "Fine";
	case CARE_ROUNDING: return "Rounding";
	case CARE_BETTER:   return "Better";
	case CARE_WEIRD:    return "Weird";
	case CARE_BUG:      return "Bug";
	case CARE_MELTDOWN: return "Meltdown";
	default:            return "?";
	}
}

const char* cascadeModifierClassify(int iCascade, int iLegacy, const CvModifierSlot& slot, int& iCareOut)
{
	// Systemic garbage first -- a NEGATIVE realized yield means the combine produced nonsense (overflow / bad data).
	// (legacy getYieldRate100 floors at >=1, so a negative legacy is itself broken.)
	if (iCascade < 0 || iLegacy < 0) { iCareOut = CARE_MELTDOWN; return "channelGarbage"; }

	const int iDelta = iCascade - iLegacy;
	if (iDelta == 0) { iCareOut = CARE_FINE; return "match"; }

	const int iAbs = (iDelta < 0) ? -iDelta : iDelta;
	// §6 LOCKED tolerance: EXACT-ZERO in parity mode (any diff is a wiring bug -- Mode A is the plumbing proof);
	// |delta| <= 1 in capability mode (int-rounding / sum-order noise).
	const int iTol = cascadeModifierParityMode ? 0 : 1;
	if (iAbs <= iTol) { iCareOut = CARE_ROUNDING; return "rounding"; }

	// A true MULTIPLIER (composes only in capability mode -- parity skips them) explains a product-vs-additive gap:
	// the cascade's deliberate correction over fragmented-legacy additive math (R-M2) -- a WIN, not a bug.
	if (!cascadeModifierParityMode && slot.iMultiplierX100 != 100) { iCareOut = CARE_BETTER; return "multiplierComposition"; }

	// Cascade under/over-shoots: a deposit is missing / extra / mis-scoped. In Mode A (the plumbing proof) this is a
	// wiring-or-coverage gap -> Bug (must-fix before that channel's cutover, §4 cutover rule). NB pre-completion this
	// DOMINATES the histogram: only BUILDING deposits are wired so far, so any city whose legacy yield draws on
	// bonus/civic/tech/specialist/event/area/capital/player sources undershoots -- the EXPECTED parity work (§3.1a),
	// not a day-one alarm. The owner reads it as "wire these sources", then re-verdicts the rung (R-M3).
	iCareOut = CARE_BUG;
	return (iDelta < 0) ? "missingDeposit" : "extraDeposit";
}
