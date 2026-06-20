# Fixed-point & the scale registry — the ONE place scales live

> **Status:** reference (canonical scale registry) · **Verified against:** `Sources/Engine/CvCity.cpp`,
> `Sources/Infos/*.h` — 2026-06-19.
> **Grounding:** every scale below was figured from the math in the cited accessor, not from the field
> name. Line numbers drift — confirm the named function, not the integer.
>
> This is the **single source of truth for value scales** in S2S. If you need to know whether a quantity
> is human-readable, ×100 fixed-point, a percent, or a multiplier — it is here. Do not re-derive a scale
> in another doc; link this one. Ruling: [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100),
> [DEC-per100-closed-set](../../architecture/decisions.md#dec-per100-closed-set),
> [DEC-curator-owns-descale](../../architecture/decisions.md#dec-curator-owns-descale).

---

## 1. The model — integer ×100, conversion in exactly ONE layer

The cascade does **all** value math in integer fixed-point with 2 decimals (×100). No floats anywhere —
this is OOS-load-bearing (Civ4 MP is deterministic lockstep; CPU-dependent float math desyncs). `V100 =
round(human × 100)`, so `1.00 → 100`, `7 → 700`, `0.5 → 50`; `FIXED_ONE = 100`.

The conversion human↔×100 lives in **exactly one layer**, so nobody in between guesses scales:

| Layer | Job | Sees ×100? |
|---|---|---|
| **XML** (legacy, frozen) | the inherited data — MIXED scales (some fields `*Changes100`, some normal) | the mess we're leaving |
| **CURATOR** (`Tools/Migration/`) | resolve the XML per-100-vs-normal ambiguity → emit **uniform human-readable** numbers to JSON | reads ×100 XML, writes human |
| **JSON** (`Assets/Data/**`) | human numbers only (`7`, `25`, `1.5`) — **no ×100, no scale markers** | NO |
| **readJson** (the import) | the **entire** human→×100 conversion + percent semantics, once at load | converts → ×100 |
| **CASCADE** (the math) | pure integer math on prepared data — never checks or knows about ×100 | NO (just integers) |

**Consequence:** a ×100 value in a JSON file is a **curator bug** — it leaked an integer-math
representation onto the human surface. Because the curator absorbs all scale mixing once, readJson has
ZERO per-field scale knowledge (a blanket ×100). The per-field registry below is therefore a
**curator-only, used-once** checklist — it must not leak into readJson or the cascade.

## 2. The unit table (what readJson does)

| JSON (human) | meaning | internal (×100) | combine |
|---|---|---|---|
| `flat: 7` (or `7.5`) | additive +7.00 / +7.50 | `700` / `750` | summed: `Σflat100` |
| `percent: 25` | +25.00% | `2500` | summed: `Σpct100` |
| `multiplier: 2` (or `1.5`) | ×2.00 / ×1.50 | `200` / `150` | product: `Π(mult100/100)` |

## 3. How to figure a field's scale (the method — do NOT eyeball the name)

A legacy field is **per-100 (÷100 to humanize)** iff its value flows **into a ×100 accumulator with no
`× 100` on the way in** — i.e. the engine treats the stored integer as already-scaled. It is **normal
(×1, human)** iff the engine multiplies it by 100 when depositing. The tell is at the consumption site:

- `getYieldRate100` (`CvCity.cpp:11246`) = `(getBaseYieldRate + getSpecialistYieldTotal) *
  getBaseYieldRateModifier + 100 * getExtraYield` — the `× 100` on `getExtraYield` proves the extra bucket
  is human-scale going in.
- `getExtraYield100` (`CvCity.cpp:11323`) = `m_aiExtraYield * 100 + getBuildingExtraYield100 +
  getBaseYieldPerPopRate * getPopulation()` — `m_aiExtraYield` is scaled **up** by 100 to enter ×100
  space; a term added **raw** beside it is therefore either already-×100 or a ×1 value being applied at
  1/100 strength.

## 4. The per-field scale REGISTRY

### 4a. Already-human (×1) — emit as-is
| field | accessor | why ×1 |
|---|---|---|
| `YieldChange` / `CommerceChange` | `getYieldChange` / `getCommerceChange` | deposited `× 100` by the engine |
| `YieldModifier` / `CommerceModifier` | `getYieldModifier` … | an integer **percent** (emit `percent`) |

### 4b. The CLOSED per-100 set — ÷100 to humanize
Verified exhaustive: `grep -rE "get[A-Za-z_]+100 *\(" Sources/Infos/*.h` returns **exactly six** `…100()`
accessors across all Info headers. That set IS the de-scale list:

| field | accessor | scale | curator action |
|---|---|---|---|
| `TechYieldChanges` (Building) | `getTechYieldChanges100` | ×100 | ÷100 → human (FLAT) |
| `TechCommerceChanges` (Building) | `getTechCommerceChanges100` | ×100 | ÷100 → human; it is **FLAT** (`changeBuildingCommerceTechChange`→`getBaseCommerceRate100`, `CvCity.cpp:12136`); the XML "CommercePercents" sub-tag is a misnomer |
| `EraCommerceChanges` (Heritage) | `getEraCommerceChanges100` | ×100 | ÷100 → human |
| `iExtraUpkeep100` (Promotion / UnitCombat) | `getExtraUpkeep100` | ×100 | ÷100 → human |
| `getTotalModifiedCombatStrength100` (CvUnit) | — | ×100 | **computed**, not an XML field — nothing to de-scale |

### 4c. The ×100-space ADDENDS that LACK a `…100()` getter — the heuristic's blind spot
The "`*100` getters mark the scaled fields" rule is INCOMPLETE: some fields are added in ×100 space
*without* a `…100()` getter. These must be mapped at the consumption site, not by name. Verified against
`CvCity.cpp` 2026-06-19:

| field | scale | evidence | curator action |
|---|---|---|---|
| `BonusCommercePercentChanges` (Building) | **×100, and FLAT** | added raw beside `100 * getBuildingCommerce` inside `getBuildingCommerce100` (`CvCity.cpp:12132`); the *rate* modifier is the separate `m_aiBonusCommerceRateModifier` | ÷100 de-scale **+ relabel `percent`→`flat`** (the name's "Percent" is a misnomer) |
| `YieldPerPopChange` / `CommercePerPopChange` (per-pop) | **×1 human, NOT ×100** | added raw into the ×100-space `getExtraYield100` / `getBuildingCommerce100` (`CvCity.cpp:11323` / `:12132`) — the legacy "latent /100 weakening" | **emit as-is; do NOT de-scale** (÷100 here corrupts `1/pop` → `0.01/pop`) |

> The per-pop finding retires an earlier tentative "de-scale perPopulation" plan — it was wrong. This is
> exactly the [DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing) case: the scale was *mapped* at the
> consumption site, not guessed from the field name.

## 5. Verification — the math proves the scales, not manual JSON review

The owner cannot eyeball thousands of JSONs; the offline tester (`Tools/ModifierCalc/cascade_sim.py`)
imports the human JSON (human→×100 per §2), computes the effective value, and compares against the live
legacy `getYieldRate100`. **Residual divergence localises the next mis-scaled field** → fix the curator →
regenerate → re-run. Parity-adjacent is the bar, not byte-parity ([DEC-parity-not-goal](../../architecture/decisions.md#dec-parity-not-goal)).

## See also
- [decisions ledger](../../architecture/decisions.md) — `DEC-fixedpoint-x100`, `DEC-per100-closed-set`,
  `DEC-curator-owns-descale` index this doc as their home.
- `legacy-value-calc-map.md` *(pending rebuild)* — the full per-calc DESTROY-pass map this scale work feeds.
- the modifier cascade spec *(pending rebuild)* — §2 arithmetic that consumes ×100 values.
