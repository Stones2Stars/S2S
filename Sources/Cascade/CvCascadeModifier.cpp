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
	// Lazy per-building-type modifier cache: parse each building's JSON ONCE, reuse. Game-thread only (no locking).
	// Parse is static (the JSON files don't change in-session); the per-deposit enabled/disabled is re-evaluated live.
	std::map<int, CvEntityModifiers> g_buildingMods;
	std::set<int>                    g_buildingModsNoJson; // negative cache (no JSON / parse fail) -- don't retry

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

	// A deposit applies when its `enabled` holds AND its `disabled` doesn't (empty = unconditional). Re-eval per call.
	bool depositActive(const CvCascadeModifierDeposit& d, const CvCascadeContext& kCtx)
	{
		if (!d.enabled.isEmpty() && !cascadeEvalCondition(d.enabled, kCtx)) return false;
		if (!d.disabled.isEmpty() && cascadeEvalCondition(d.disabled, kCtx)) return false;
		return true;
	}
}

void cascadeModifierCitySlot(int iFamily, const CvCascadeContext& kCtx, CvModifierSlot& slotOut)
{
	slotOut.clear();
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS) return;
	const CvCity* pCity = GET_PLAYER((PlayerTypes)kCtx.iPlayer).getCity(kCtx.iCity);
	if (pCity == NULL) return;

	const int iNumBuildings = GC.getNumBuildingInfos();
	for (int b = 0; b < iNumBuildings; ++b)
	{
		if (!pCity->hasBuilding((BuildingTypes)b)) continue; // present buildings only
		const CvEntityModifiers* pMods = cachedBuildingMods(b);
		if (pMods == NULL) continue;
		for (size_t i = 0; i < pMods->deposits.size(); ++i)
		{
			const CvCascadeModifierDeposit& d = pMods->deposits[i];
			if (d.iFamily != iFamily || d.iScope != MODSCOPE_CITY) continue;
			if (cascadeModifierParityMode && d.eUnit == MODUNIT_MULTIPLIER) continue; // additive-only in parity mode
			if (!depositActive(d, kCtx)) continue;
			slotOut.deposit(d.eUnit, d.iValue);
		}
	}
}

int cascadeModifierEffective(int iFamily, int iScope, const CvCascadeContext& kCtx)
{
	if (iScope != MODSCOPE_CITY) return 0; // PILOT: city scope only (plot/other scopes are later sub-passes)
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS) return 0;
	const CvCity* pCity = GET_PLAYER((PlayerTypes)kCtx.iPlayer).getCity(kCtx.iCity);
	if (pCity == NULL) return 0;

	CvModifierSlot slot;
	cascadeModifierCitySlot(iFamily, kCtx, slot);
	return cascadeModifierApply(slot, pCity->getBaseYieldRate((YieldTypes)iFamily));
}
