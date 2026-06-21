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
		// ACTIVE buildings only -- legacy's per-turn value (getYieldRate100, goodHealth, ...) counts only
		// hasFullyActiveBuilding (present AND not resource/replacement-disabled AND not religiously-limited), so a
		// present-but-DORMANT building must NOT deposit, or the cascade over-counts (the extraDeposit divergence the
		// shadow surfaced). This matches the cityInput deposit-source rule (apples-to-apples with legacy).
		if (!pCity->hasFullyActiveBuilding((BuildingTypes)b)) continue;
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

// Per-source twin of cascadeModifierCitySlot -- captures each active building's (city-scope) and civic's (empire-scope)
// OWN contribution to the family slot, so the diagnostic can attribute the cascade total source-by-source against the
// legacy per-source decomposition (owner ruling 2026-06-20: know what we mirror). Same filter as the aggregate loop.
void cascadeModifierCitySources(int iFamily, const CvCascadeContext& kCtx, std::vector<CvModifierSourceContribution>& out)
{
	out.clear();
	if (kCtx.iPlayer < 0 || kCtx.iPlayer >= MAX_PLAYERS) return;
	CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)kCtx.iPlayer);
	const CvCity* pCity = kPlayer.getCity(kCtx.iCity);
	if (pCity == NULL) return;

	const int iNumBuildings = GC.getNumBuildingInfos();
	for (int b = 0; b < iNumBuildings; ++b)
	{
		if (!pCity->hasFullyActiveBuilding((BuildingTypes)b)) continue;
		CvModifierSourceContribution c;
		c.iEntity = b; c.bCivic = false; c.slot.clear();
		foldDeposits(cachedBuildingMods(b), iFamily, MODSCOPE_CITY, kCtx, c.slot);
		if (!c.slot.isIdentity()) out.push_back(c);
	}

	const int iNumCivicOptions = GC.getNumCivicOptionInfos();
	for (int co = 0; co < iNumCivicOptions; ++co)
	{
		const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)co);
		if (eCivic == NO_CIVIC) continue;
		CvModifierSourceContribution c;
		c.iEntity = (int)eCivic; c.bCivic = true; c.slot.clear();
		foldDeposits(cachedCivicMods((int)eCivic), iFamily, MODSCOPE_EMPIRE, kCtx, c.slot);
		if (!c.slot.isIdentity()) out.push_back(c);
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

// ===================== the MODIFIER-FAMILY registry (ALL channels) =====================

namespace
{
	// The family registry, indexed by ModifierFamily. JSON key + combine mode (modifier-spec §7). The combine modes
	// are the documented ones (legacy-value-calc-map): yields are base×%+flat; the commerce-split base + health/
	// happiness/defense are additive ledgers; great-people is base×modifier/100; maintenance is cost-asymmetric.
	const ModifierFamilyInfo g_modFamilies[NUM_MODIFIER_FAMILIES] =
	{
		{ "food", MODCOMBINE_YIELD }, { "production", MODCOMBINE_YIELD }, { "commerce", MODCOMBINE_YIELD },
		{ "gold", MODCOMBINE_ADDITIVE }, { "research", MODCOMBINE_ADDITIVE }, { "culture", MODCOMBINE_ADDITIVE }, { "espionage", MODCOMBINE_ADDITIVE },
		{ "health", MODCOMBINE_ADDITIVE }, { "happiness", MODCOMBINE_ADDITIVE }, { "defense", MODCOMBINE_ADDITIVE },
		{ "maintenance", MODCOMBINE_COST }, { "greatPeopleRate", MODCOMBINE_BASExMOD }
	};
	const ModifierFamilyInfo g_modFamilySentinel = { "?", MODCOMBINE_ADDITIVE };
}

const ModifierFamilyInfo& cascadeModifierFamilyInfo(int iFamily)
{
	if (iFamily < 0 || iFamily >= NUM_MODIFIER_FAMILIES) return g_modFamilySentinel;
	return g_modFamilies[iFamily];
}

void cascadeModifierFamilyShadow(const CvCity* pCity, const CvCascadeContext& kCtx, int iFamily,
	CvModifierSlot& slotOut, int& iBaseOut, int& iCascadeOut, int& iLegacyOut)
{
	slotOut.clear(); iBaseOut = 0; iCascadeOut = 0; iLegacyOut = 0;
	if (pCity == NULL) return;

	// Fold the family's migrated deposits (the city's buildings at city scope + the player's civics at empire scope).
	// cascadeModifierCitySlot is family-generic -- it filters by (iFamily, scope), so it works for every family once
	// readJson tags the deposits with the family id; no per-family wiring needed here.
	cascadeModifierCitySlot(iFamily, kCtx, slotOut);

	// the pre-modifier BASE + the legacy REALIZED value (the same x1 scale the cascade produces), per family.
	switch (iFamily)
	{
	case MODFAM_FOOD: case MODFAM_PRODUCTION: case MODFAM_COMMERCE:
		iBaseOut   = pCity->getBaseYieldRate((YieldTypes)iFamily) + pCity->getSpecialistYieldTotal((YieldTypes)iFamily);
		iLegacyOut = pCity->getYieldRate100((YieldTypes)iFamily) / 100;
		break;
	// commerce split: the additive base commerce the deposits build (pre-slider, pre-totalModifier). getBaseCommerceRate
	// already sums specialist+religion+corp+building+player -- the apples-to-apples additive target for the deposits.
	case MODFAM_GOLD:       iLegacyOut = pCity->getBaseCommerceRate(COMMERCE_GOLD);      break;
	case MODFAM_RESEARCH:   iLegacyOut = pCity->getBaseCommerceRate(COMMERCE_RESEARCH);  break;
	case MODFAM_CULTURE:    iLegacyOut = pCity->getBaseCommerceRate(COMMERCE_CULTURE);   break;
	case MODFAM_ESPIONAGE:  iLegacyOut = pCity->getBaseCommerceRate(COMMERCE_ESPIONAGE); break;
	// signed-split / standing ledgers: the cascade sums the family's flats; legacy is the realized net (good-bad) or modifier.
	case MODFAM_HEALTH:      iLegacyOut = pCity->goodHealth() - pCity->badHealth();          break;
	case MODFAM_HAPPINESS:   iLegacyOut = pCity->happyLevel() - pCity->unhappyLevel();       break;
	case MODFAM_DEFENSE:     iLegacyOut = pCity->getDefenseModifier(false);                  break;
	case MODFAM_MAINTENANCE: iLegacyOut = pCity->getMaintenanceTimes100() / 100;             break;
	case MODFAM_GREATPEOPLE: iBaseOut = pCity->getBaseGreatPeopleRate(); iLegacyOut = pCity->getGreatPeopleRate(); break;
	default: break;
	}

	// apply the family's combine mode to (base, slot).
	switch (cascadeModifierFamilyInfo(iFamily).eCombine)
	{
	case MODCOMBINE_YIELD:    iCascadeOut = cascadeModifierApply(slotOut, iBaseOut);          break; // base×%+flat (active flow)
	case MODCOMBINE_BASExMOD: iCascadeOut = iBaseOut * (100 + slotOut.iPercent) / 100;        break; // base × modifier/100
	case MODCOMBINE_COST:                                                                            // cost-asymmetric: pin at #430;
	case MODCOMBINE_ADDITIVE:                                                                        // additive ledger (base 0 for most)
	default:                  iCascadeOut = iBaseOut + slotOut.iFlat;                          break;
	}
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
	// ⛔ The CARE scale is the OWNER'S acceptance vocabulary, assigned at the validation stage -- NOT the agent's
	// "this is OK / this is a Bug" classifier (owner ruling 2026-06-20). So this classifier only auto-assigns the two
	// SAFE-FACTUAL low rungs (exact match / within-rounding) plus the documented capability win, and routes EVERY real
	// divergence to CARE_WEIRD -- the "surface it, the OWNER verdicts" bucket. It deliberately NEVER self-asserts
	// CARE_BUG or CARE_MELTDOWN: those are verdicts, not facts. The FACTUAL attribution is the cause-tag returned + the
	// magnitudes in the row; the owner reads the distribution and assigns the real rung. (shadow.md §4 -- realigned
	// after the scale had drifted into an agent auto-verdict, incl. flagging legitimate negatives as "garbage".)
	const int iDelta = iCascade - iLegacy;
	if (iDelta == 0) { iCareOut = CARE_FINE; return "match"; }

	const int iAbs = (iDelta < 0) ? -iDelta : iDelta;
	// §6 tolerance: EXACT-ZERO in parity mode (the plumbing proof); |delta| <= 1 in capability mode (int-rounding noise).
	const int iTol = cascadeModifierParityMode ? 0 : 1;
	if (iAbs <= iTol) { iCareOut = CARE_ROUNDING; return "rounding"; }

	// A true MULTIPLIER (composes only in capability mode -- parity skips them) explains a product-vs-additive gap:
	// the cascade's deliberate correction over fragmented-legacy additive math -- the documented capability win.
	if (!cascadeModifierParityMode && slot.iMultiplierX100 != 100) { iCareOut = CARE_BETTER; return "multiplierComposition"; }

	// A real divergence. Report the FACTUAL cause by sign (cascade under- vs over-shoots) and route to the owner's
	// verdict (Weird). Pre-completion this is dominated by un-wired sources (bonus/tech/trait/area/power/capital/
	// specialist/the grouped-family members) -- the expected parity work, NOT a defect the agent should pre-judge.
	iCareOut = CARE_WEIRD;
	return (iDelta < 0) ? "missingDeposit" : "extraDeposit";
}
