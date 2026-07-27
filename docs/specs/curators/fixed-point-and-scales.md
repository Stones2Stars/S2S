# Fixed-point & the scale registry — the ONE place scales live

> **Status:** reference (canonical scale registry) · **Verified against:** `Sources/Engine/CvCity.cpp`,
> `Sources/Infos/*.h`.
> **Grounding:** every scale below was figured from the math in the cited accessor, not from the field
> name. Line numbers drift — confirm the named function, not the integer.
>
> This is the **single source of truth for value scales** in S2S. If you need to know whether a quantity
> is human-readable, ×100 fixed-point, a percent, or a multiplier — it is here. Do not re-derive a scale
> in another doc; link this one. Ruling: [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100),
> [DEC-curator-owns-descale](../../architecture/decisions.md#dec-curator-owns-descale).

---

## 1. The model — integer ×100 EVERYWHERE, human only at the IN and OUT boundaries

×100 fixed-point with 2 decimals is the engine's **native representation everywhere** — the cascade, the realized
getters, and the consumers all carry ×100. No floats anywhere — this is OOS-load-bearing (Civ4 MP is deterministic
lockstep; CPU-dependent float math desyncs). `V100 = round(human × 100)`, so `1.00 → 100`, `7 → 700`, `0.5 → 50`;
`FIXED_ONE = 100`.

Human numbers exist at exactly **two boundaries** — nobody in between guesses scales, and **no getter has a ×100
"variant"** (there is no `getX()` + `getX100()` pair; the getter simply IS ×100):

| Layer | Job | Sees ×100? |
|---|---|---|
| **XML** (legacy, frozen) | the inherited data — MIXED scales (some fields `*Changes100`, some normal) | the mess we're leaving |
| **CURATOR** (`Tools/Migration/`) | resolve the XML per-100-vs-normal ambiguity → emit **uniform human-readable** numbers to JSON | reads ×100 XML, writes human |
| **JSON** (`Assets/Data/**`) | human numbers only (`7`, `25`, `1.5`) — **no ×100, no scale markers** | NO |
| **readJson** (the IN boundary) | the **entire** human→×100 conversion + percent semantics, once at load | converts → ×100 |
| **CASCADE + getters + consumers** (the engine) | pure integer ×100 math; the realized getters return ×100 and every consumer carries it | ×100 throughout |
| **READERS** (the OUT boundary) | ×100 → human, once: any READER (UI / the `/computed` HTTP fields / the `Cy*` Python wrappers) does its own trivial `÷100`. A value that is physically a **whole game count** (angry citizens, a food modifier) reduces at the **point of use** that consumes it as a whole number — that use is itself a reader | converts ← ×100 |

> **⛔ NO getter reduces, and there are NO discrete carve-outs — every channel works identically (owner ruling).**
> This uniformity IS the rework: *"then we never have to care about what format inside the structure."* A getter that
> reduces internally hands every consumer a pre-rounded number whether or not it wants one, and a consumer needing
> precision cannot get it back — which is the same shoehorn as a `getX`+`getX100` pair, just spelled differently.
> Discreteness is a property of a USE (the game unassigns whole citizens), not of a getter.
>
> **⛔ And the NAME never carries the scale (owner ruling): no `100` suffix on any internal getter / function /
> member.** Every value is ×100 by the universal rule above, so a `100` in the name is redundant noise —
> `getScalar` / `sum` / `expectedModifier`, never `getScalar100`. The one algebra rule universal ×100 brings:
> **never multiply two ×100 values together without rescaling** — the product is ×10000, so a `÷100` belongs at
> the multiply. No site is believed to do this today; any found is a defect to flag, never a silent rescale.

**Why ×100 out to the consumers, not reduced at the getter** ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)):
reducing at the getter forces a human-variant getter (a `getX`+`getX100` split) the moment anything internal needs
the ×100 form, and it lets the cascade be shoehorned into legacy-shaped getters instead of the consumers being
rewired — the exact reflex that produced the half-migration. Carrying ×100 out makes format-tracking unnecessary
(the answer is always "×100"), and the visible-100×-if-mis-wired forcing function makes every consumer wire
correctly or be discarded. **Blast radius is never a reason to limit the conversion.** A consumer that only tests
SIGN or ranks is scale-invariant (no change); one that mixes with a whole count reduces at that use; an aggregate
stays ×100 and its own reader reduces. Migration status + the pilot pattern:
[fixed-point-conformance.md](../../plans/structural-cleanup/fixed-point-conformance.md).

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

- `getYieldRate100` (`CvCity.cpp`) — the tell lives wherever the rate is composed. While the getter computes the
  legacy two-tier shape in place, read it there; once the channel is on the cascade the tell moves into the package
  computation. Either way, confirm the SCALE at the site that composes the value, never at the getter's name.
- `getExtraYield100` (`CvCity.cpp:10408`) just returns `getBuildingExtraYield100` — building-extra only, no
  other term. The tell lives in `getBuildingExtraYield100`
  (`CvCity.cpp:10360`): `100 * kBuilding.getYieldChange(eYield) + kTeam.getBuildingYieldTechChange(eYield, eB)`
  — the `× 100` on `getYieldChange` proves that field is human-scale going in (§4a); `getBuildingYieldTechChange`
  is already ×100 (§4b).

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
`CvCity.cpp`:

| field | scale | evidence | curator action |
|---|---|---|---|
| `BonusCommercePercentChanges` (Building) | **×100, and FLAT** | added raw beside `100 * getBuildingCommerce` inside `getBuildingCommerce100` (`CvCity.cpp:12132`); the *rate* modifier is the separate `m_aiBonusCommerceRateModifier` | ÷100 de-scale **+ relabel `percent`→`flat`** (the name's "Percent" is a misnomer) |
| `YieldPerPopChange` / `CommercePerPopChange` (per-pop) | **×1 human, NOT ×100** | added raw into the ×100-space `getExtraYield100` / `getBuildingCommerce100` (`CvCity.cpp:11323` / `:12132`) — the legacy "latent /100 weakening" | **emit as-is; do NOT de-scale** (÷100 here corrupts `1/pop` → `0.01/pop`) |
| `YieldsProduced` / `CommercesProduced` (Corporation) | **×100** | `getCorporationYieldByCorporation` (`CvCity.cpp:12594-12602`): `produced × Σ getNumBonuses(prereqBonus) × worldCorpMaintPct / 100`, then the corp result `/100` — so `produced=75` ⇒ 0.75/bonus. NOT the genuinely-×1 `*Changes` twin (`getYieldChange × 100` in-formula) | ÷100 de-scale → human (`curate_corporation`). The dedicated corp pass also verifies + de-scales `iMaintenance` (`calculateCorporationMaintenanceTimes100`, ×100) |
| `iHealthPercent` / `iHappinessPercent` (Specialist) | **×100, and FLAT** | `processSpecialist` STORES them raw (`CvCity.cpp:5184/5192`, `change*Health/*Happiness(field × count)`) — the misleading part — but the REALIZED `goodHealth()`/`badHealth()`/`happyLevel()`/`unhappyLevel()` read them `/100` (`CvCity.cpp:5848/5876/5714/5654`). The `/100` is NOT AI-only weighting; it is the actual realized level. | ÷100 de-scale → human (FLAT; the "Percent" is a misnomer). `curate_specialist`. ⚠ Map at the CONSUMER, not the store — the raw `change*` store site is the trap that produces a wrong "it's FLAT ×1" correction |

> The per-pop row is the [DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing) case in miniature:
> the scale was *mapped* at the consumption site, never guessed from the field name.

## 5. Verification — the math proves the scales, not manual JSON review

The owner cannot eyeball thousands of JSONs, so a mis-scaled field is found by the MATH: the effective value the
authored JSON produces is observed live on the `/computed` oracle endpoints, on a real save
([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)). **Residual divergence localises
the next mis-scaled field** → fix the curator → regenerate → re-check. Exact parity is the bar — 0 in-scope mismatches; a residual divergence is a data-collection gap (a still-mis-scaled field), never a formula difference ([DEC-parity](../../architecture/decisions.md#dec-parity)).

## See also
- [decisions ledger](../../architecture/decisions.md) — `DEC-fixedpoint-x100`, `DEC-curator-owns-descale` index
  this doc as their home.
- [legacy-value-calc-map.md](../../plans/structural-cleanup/legacy-value-calc-map.md) — the full per-calc
  DESTROY-pass map this scale work feeds.
- [modifier.md](../modifier.md) — the §2 arithmetic that consumes ×100 values.
