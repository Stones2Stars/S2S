# Handover 2026-06-19 — modifier cascade: the GROUNDED modifier map + the cumulative-band placement task

> Transient relay. Durable facts cited to their committed homes. Read the durable docs first (AGENTS.md cascade
> section incl. the **NO-GUESSING rule**, `modifier-cascade-known-discrepancies.md`, `calc-emulator-spec.md`,
> `enabler-cascade-spec.md` §3 PropertyEffect, `cascade-fixed-point.md`).

## ⛔ THE governing rule established this session (owner, in AGENTS.md now)
**No guessing. Map everything, always — that is what the Orwellian surveillance is FOR.** When a cascade value
diverges from legacy: EMIT the full legacy decomposition via the dump and map the cascade by the SAME components;
attribute every divergence to a NAMED source with numbers. If the attributing data isn't emitted, emit it FIRST.
Piecemeal human-style guess-and-recheck does NOT work in this codebase — cover large areas at once (use minions).

## What is DONE + COMMITTED (json-data-migration)
- **`c762cb635`** education property-bands: pseudobuildings never `replace` (48 stripped); 4 ladders → INCREMENTAL
  deltas; `BLACKENED_SKIES` replace→reversible `disables`. **DESPAIR #9** (`e3b083057`).
- **`4fa075bdc`** `BUILDING_STORAGE_EMPIRE` nuked (broken resource-limiter; AI-estimator-only cap, uncapped live).
  XML yields stripped + `bAutoBuild=0`; engine `if`-block now a dead no-op (flagged for deletion). Idea → **issue #443**.
- AGENTS.md: the no-guessing rule + the Release-vs-FinalRelease testing rule.

## The GROUNDED modifier map (4-agent audit, verified on real data — NOT guesses)
Legacy yield % = `getBaseYieldRateModifier` = `100 + modBuilding + modBonus + modPlayer + modPower + modArea + modCapital`
(CvCity.cpp:11217). Every source mapped end-to-end (legacy writer → XML → curator → JSON → cascade_sim):

| source | legacy | curator emits | cascade_sim sums | status |
|---|---|---|---|---|
| **YieldModifiers** | `m_buildingYieldMod` (CvCity 4681) | `YIELD_FAMILIES`→`<y>.city.percent` | yes | ✅ (but EDUCATION — see below) |
| **BonusYieldModifiers** | `m_aiBonusYieldRateModifier` (4929) | `COND_KEYED`→bonus-gated `<y>.city.percent` | yes (`connection`-aware) | ✅ wired |
| **PowerYieldModifiers** | `m_aiPowerYieldRateModifier` (4683), `isPower()`-gated | **FIXED this session** — was mis-classed in `COND_KEYED` (direct per-yield array, `_keyed` dropped it); now `pass2` emits `<y>.city.percent enabled:HAS_POWER` | yes (`HAS_POWER`) | ✅ wired (16 files) |
| **GlobalYieldModifiers** | `CvPlayer::m_aiYieldRateModifier` (7457) | `YIELD_FAMILIES`→`<y>.empire.percent` | yes (empire scope) | ✅ |
| **AreaYieldModifiers** | area (7456) | `YIELD_FAMILIES`→`<y>.area.percent` | yes (area scope) | ✅ (0 buildings use it) |
| **TechYieldModifiers** | tech-gated (4961) | `COND_KEYED` tech-gated | yes | ✅ (1 building, rare) |
| **Civics** YieldModifier/CapitalYieldModifier | `CvPlayer` 18063/18064 | `curate_civic.py` `<y>.empire.percent` + `empire.capital.percent` | yes | ✅ FULLY wired (verified) |
| **Traits** YieldModifier/CapitalYieldModifier | `CvPlayer` 28600/28601 | `curate_trait.py` same | yes (**trait loop added this session**) | ✅ FULLY wired |
| Events | transient | not migrated | not summed | n/a (correct) |

**Conclusion: every modifier source is wired EXCEPT the EDUCATION placement issue below.** The player-side "gap" I
measured was a grouping artifact (legPlr includes building-`GlobalYieldModifier`, counted on the building side).

## ✅ Validated cascade_sim / curator fixes this session (UNCOMMITTED — correct, grounded)
1. **ACTIVE-only modifiers** (`cascade_sim.simulate_yields`: `present = buildings` only, dropped dormant). VERIFIED:
   legacy removes a DORMANT building's modifier AND flat via `processBuilding(-1)` (resource-disabled +
   religiously-limited). The earlier all-present was wrong; it over-counted dormant, MASKING the un-wired sources.
2. **Traits** summed (empire scope) + **`empire.capital` sub-scope** (civics+traits, gated `isCapital`) — `_entity_deposits`.
3. **`connection:vicinity`** bonus atoms check the vicinity set (plot bonuses), not combined `resources` (fixed the
   commerce vicinity over-count).
4. **Curator: PowerYieldModifiers** fix (above).

## ⭐ THE OPEN TASK — cumulative property-band placement (owner design, 2026-06-19)
**Owner ruling: property-effect buildings are CUMULATIVE — ALL bands active (a building is active above/below its
target level), each carrying its INCREMENT, summing to the full total.** This is the model; the increment data is
correct. THE PROBLEM: the live engine still uses `ReplacementBuildings` → only ONE band (highest, e.g.
`BUILDING_EDUCATION_ENLIGHTENED`) is in the dump's active `buildings` set, at legacy's FULL value. So `cascade_sim`
(reading the legacy dump's one band) sees one increment (+5) vs legacy full (+35) → the **−30 production/commerce
gap, every city** (the dominant remaining divergence). FIX = make placement cumulative:
- **HOW (owner ruling 2026-06-19): band them with `requires`, NOT `replaces`.** Concretely:
  1. **Curator** — author on EACH property-effect band a `requires.operate` property-in-band atom from its
     `CIV4PropertyInfos.xml` `PropertyBuilding` `iMinValue`/`iMaxValue`:
     `requires.operate.all:[{type:PROPERTY_X, scope:city, min:iMinValue, max:iMaxValue}]` (data-model §2.1
     `ATOMDOMAIN_PROPERTY`, already parsed by readJson). A band is active iff the city's property value is in its band.
     Cumulative falls out: all in-band (positive: value≥threshold; negative: value≤threshold) bands are active.
  2. **Dump** — emit the city's CURRENT property VALUES (`pCity->getProperties()->getValueByProperty` per `PROPERTY_*`)
     so the offline sim can evaluate the band atom. (Per-building property DEPOSITS are already emitted; the city's
     accumulated VALUE is the missing input — verify/extend `cityInput`.)
  3. **`cascade_sim`** — evaluate the `PROPERTY_X` band atom in `eval_condition`/`_eval_atom` (value in [min,max]
     from the dumped property values), and place property-effect bands by their `requires.operate`, NOT by the
     dump's legacy one-band `buildings` set. Then the cumulative increments sum to the full total = legacy.
  This is the **PropertyEffect** direction (enabler-spec §3): retire `checkPropertyBuildings`/`ReplacementBuildings`,
  band membership = `requires.operate` property-in-band. Do NOT use `replaces` (pseudobuildings never replace).
- Until placement is cumulative, the education ladders' sweep stays under by the increment-vs-full delta. Do NOT
  "revert to full values" (owner rejected — full breaks the cumulative/clearer-UX design); make placement cumulative.

## The comprehensive DUMP (live, Release-deployed; restart to refresh)
`/diagnostic/cityInput?player=N` now emits, per ACTIVE building: yields flat100+pct, **the full yield-modifier
breakdown** (`modBonus/modBuilding/modPlayer/modPower/modArea/modCapital` per yield), commerce-split flat+pct,
health/happiness/freeXp, and `properties`. Plus city-level commerce-split/defense/maintenance/growth/health (already
there) + `traits`. `CvHttpServer.cpp` changes UNCOMMITTED (intertwined with prior-session DLL work). Fixtures:
`Tools/ModifierCalc/samples/m_p{0,1,2,4,5,6}.json` (London/Keleia/Qart-hadast/Tenochtitlan/Sy Ara/Moscow).

## Sweep state (cascade_sim, 6 cities) — under because education placement not yet cumulative
food 4/6 (10.2%) · production 1/6 (15.9%) · commerce 1/6 (15.3%). The under-count is dominated by the education
one-band-vs-cumulative issue + (smaller) residuals. Commerce had been 5/6 before active-only; it drops because
active-only correctly removed the dormant over-count that was masking education + sources.

## Next session
1. Implement cumulative property-band placement (the ⭐ task) — the dominant gap.
2. Commit the validated cascade_sim/curator fixes (+ the comprehensive dump, with the prior DLL work).
3. Then the remaining channels (commerce-split, health/happiness, properties) — the dump already emits the legacy
   data per-building; extend cascade_sim to compute each + compare (the grounded pass, no guessing).
