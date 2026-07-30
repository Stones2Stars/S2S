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

## 1. The model — integer ×100 for AMOUNTS, human only at the IN and OUT boundaries

×100 fixed-point is the engine's **native representation for every AMOUNT** — a yield, a combat value, any
magnitude — carried that way through the cascade, the realized getters and the consumers. No floats anywhere —
this is OOS-load-bearing (Civ4 MP is deterministic lockstep; CPU-dependent float math desyncs).
`V100 = round(human × 100)`, so `1.00 → 100`, `7 → 700`, `0.5 → 50`; `FIXED_ONE = 100`.

> **⛔ THE ×100 EXISTS FOR ONE REASON — TWO DECIMALS ON AN AMOUNT (owner).** *"The reason for multiplying int
> values by 100 is so we can have 2 decimals… so we can express anything with 2 decimals at edge."* That is the
> whole of it, and it is what decides where the scale applies.
>
> **⛔ A PERCENTAGE IS THEREFORE NOT SCALED (owner): *"percentages should not have decimals."*** A percent is a
> whole number, so it has no decimals to carry and the ×100 buys nothing. Scaling it costs a **second identity
> constant**: `100 + Σpercent` becomes `10000 + Σpercent` at every site a percent is combined, and that magic
> `10000` then has to be threaded through every consumer — which is exactly the fudge-factor class §4c-bis exists
> to reject. `flat` and `multiplier` DO convert (a flat is an amount; a multiplier is authored on the same
> two-decimal footing, identity 100 → `×1.5` = `150`).
>
> ⚑ **This is NOT a per-channel carve-out** ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)'s
> uniformity still binds): the rule is per **UNIT** and applies identically to every family. No channel gets a
> special case; `percent` simply is not a two-decimal quantity in any of them.

Human numbers exist at exactly **two boundaries** — nobody in between guesses scales, and **no getter has a ×100
"variant"** (there is no `getX()` + `getX100()` pair; the getter simply IS ×100):

| Layer | Job | Sees ×100? |
|---|---|---|
| **XML** (legacy, frozen) | the inherited data — MIXED scales (some fields `*Changes100`, some normal) | the mess we're leaving |
| **CURATOR** (`Tools/Migration/`) | resolve the XML per-100-vs-normal ambiguity → emit **uniform human-readable** numbers to JSON | reads ×100 XML, writes human |
| **JSON** (`Assets/Data/**`) | human numbers only (`7`, `25`, `1.5`) — **no ×100, no scale markers** | NO |
| **readJson** (the IN boundary) | the **entire** human→×100 conversion, once at load, keyed on the authored UNIT (amounts convert; percents do not) | converts amounts → ×100 |
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
stays ×100 and its own reader reduces. The conversion METHOD is §4c-bis below; what remains to convert is the
[todo](../../plans/structural-cleanup/todo.md).

**Consequence:** a ×100 value in a JSON file is a **curator bug** — it leaked an integer-math
representation onto the human surface. Because the curator absorbs all scale mixing once, readJson has
ZERO per-FIELD scale knowledge — it reads only the authored **UNIT** (`flat`/`percent`/`multiplier`, which the
data states outright), never which field it belongs to. The per-field registry below is therefore a
**curator-only, used-once** checklist — it must not leak into readJson or the cascade.

## 1b. ⛔ WIDTH IS PER UNIT TOO — amounts are 64-bit, percents are 32

Scale is decided per UNIT (§1); so is **width**, for the same reason and along the same line.

- **An AMOUNT accumulates** — across sources, across scopes, at ×100 — so it carries **`int64_t`**: the package's
  flat dictionary and its receiver sums, and every accumulator, parameter and return on the calc surface.
- **A PERCENT does not accumulate into anything** — it is a small whole number by ruling (no decimals, hence no
  ×100), so it stays **`int`**. Widening it would buy nothing and cost storage on every scope object.

⛔ **`long` IS NOT A WIDER TYPE HERE.** On the frozen VC7.1/x86 toolchain `long` is 32 bits — the tree typedefs
`int64_t` as `long long` for exactly that reason. A `long` accumulator therefore *reads* as deliberate headroom
and provides none, which is precisely how the ceiling stayed invisible: the calc surface was written in `long`
throughout and was int32 the whole time.

⚑ **The tell that the ceiling was already being hit is a PARALLEL 64-BIT TWIN.** `getModifiedIntValue64`,
`intSqrt64`, `intPow64`, `applySMRank64` exist beside their 32-bit originals, and the money plane
(`calculatePreInflatedCosts` / `getFinalExpense` / `getInflationCost` / `calcCorporateMaintenance` /
`getHurryGold` / `getRealPopulation`) is already `int64_t`. That is the same signature as a surviving fudge
factor (§4c-bis), one level up: **a duplicated surface exists because two WIDTHS meet**, exactly as a magic
constant exists because two SCALES meet. Widening the amount plane is what lets those twins be deleted rather
than extended.

⚖ **Storage is per-unit; the CALC SURFACE is 64-bit throughout.** Inside a calculation every intermediate is
`int64_t` regardless of which side it came from, so no intermediate can overflow and no mixed-width promotion
has to be reasoned about. The per-unit rule governs what is STORED (where the memory cost is), never the local
arithmetic.

⚠ **Widening costs no save work at all.** The cascade plane serializes nothing
([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)), and a **serialized**
field widens SOFTLY because the save READER absorbs the narrower stored form — keep the member, the name and the
tag ([save.md §8](../save.md)). So width is decided on the merits of the value, never traded against migration
cost.

## 2. The unit table (what readJson does)

| JSON (human) | meaning | internal | combine |
|---|---|---|---|
| `flat: 7` (or `7.5`) | additive +7.00 / +7.50 | `700` / `750` (×100 — an AMOUNT) | summed: `Σflat100` |
| `percent: 25` | +25% | **`25` — NOT scaled** (a percent has no decimals) | summed: `Σpercent`, applied as `(100 + Σpercent)/100` |
| `multiplier: 2` (or `1.5`) | ×2.00 / ×1.50 | `200` / `150` (×100 — identity 100) | product: `Π(mult100/100)` |

⛔ **readJson is the ONE place that knows this, and a CALCULATION never scales (owner):** *"you should not need
to scale any value inside any actual calculation — it is literally readJson's job to ensure it's scaled."* The
unit-aware conversion lives at the parse edge (`CvModifiers.cpp`, the entry-value sites); every consumer then
receives what it can use directly. A `/100` or a `×100` appearing inside a calculation to make two operands
agree is the defect, never the fix.

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

## 4c-bis. ⛔ CONVERT BY ARITHMETIC CLUSTER, NEVER BY GETTER

**A getter cannot be converted alone.** Its co-operands are on the same scale *by arithmetic necessity*: convert one
side and every mixing site needs a compensating `÷100` — manufacturing the very fudge factor that signals a
misplaced reduce. Convert the whole cluster and the mixing sites need **no change at all**, because the units
already cancel. This is why such a sweep keeps getting re-shoehorned: each getter looks independently convertible,
and none is.

**The acceptance gate per cluster: ZERO new fudge factors at the mixing sites.** If a conversion forces compensating
constants, the cluster boundary was drawn WRONG — stop and redraw it, never push through. (This is the second of
[AGENTS.md](../../../AGENTS.md)'s drift detectors, stated as a conversion method.)

⚑ **A cluster is defined by what MIXES, not by what looks similar.** Worked groupings: the yield/food/wellbeing
chain is one unit because food consumption subtracts angry population and health rate; commerce joins it at the
production→commerce term; gold/maintenance/upkeep joins commerce because gold IS a yield
([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)); unit experience is genuinely
self-contained and so is the one safely parallelizable cluster.
⚠ **Same SHAPE is not same NATURE:** a `…Times100` on AI unit counts or plot strength carries *fractional
SizeMatters counts*, not a modifier channel — it is not a scale violation and must not be swept in with the yields.

**Sequencing within a cluster (owner): set the mechanic up to spec FIRST, then wire the consumers.** Do not open
with a hundred consumer edits; build the value chain so it is internally ×100-consistent, then reduce at the readers.

## 4d. ⛔ THE EDGE — where a scale error can occur at all, and therefore what an audit checks

**A scale error cannot happen inside the cascade.** ×100 is native EVERYWHERE within it (§1), so every magnitude
there is ×100 by construction and any two operands already agree. **A scale error is only possible where a value
CROSSES A BOUNDARY** — which makes the audit an ENUMERATION OF BOUNDARIES, never a sweep over every multiply:

1. the **IN** boundary — readJson's single human→×100 conversion;
2. the **OUT** boundary — a reader's `÷100` at the point of use;
3. a **sanctioned engine INPUT** — a value the cascade folds in rather than computes.

⚑ **The trade-route fold is THE EDGE (owner)** — the exemplar of class 3 and the reason class 3 exists.
`tradeYield` is the ONE sanctioned live-yield input ([modifier.md §2a](../modifier.md)): the cascade cannot
re-derive the trade NETWORK, so that calculation stays engine-owned ([north-star.md](../../architecture/north-star.md)
KEEP — it is none of the four systems' job) and its value is FOLDED IN. That is precisely why the scales differ
there, and why **the conversion belongs THERE: an edge converts**, exactly as the IN and OUT boundaries do.

**How to audit, since the naming ruling removed the marker.** Every value is ×100 and NO name says so, so a
grep for a `100` token returns nothing and proves nothing. Instead, at each boundary site check the two operands
against the **DECLARED scale of the surface each came from** (the calc functions' documented inputs/outputs, the
package slot reads, the compiled sums) — never against a name. ⛔ Where a boundary function mixes a plain engine
percent with a ×100 sum, fix it STRUCTURALLY — have the function lift and take both down together — so a caller
passes what it holds and cannot get the scale wrong; a comment warning the caller is not a fix.
⚠ **And never multiply two ×100 values without rescaling** — the product is ×10000, so the `÷100` belongs at the
multiply.

> **⛔ THE OUT BOUNDARY IS DECIDED PER UNIT, AND A FAMILY-WIDE `÷100` RULE IS ITSELF A DEFECT.** Within one
> family the kinds differ: `infoKindUnit` makes some PERCENT (unscaled — a re-point is 1:1) and some FLAT
> (×100 — the reader reduces). So "this getter set is ×100, reduce at the reader" is never a safe blanket; it
> zeroes every percent it touches. **Ask the KIND's unit, never the family's or the getter's name.**
>
> **⛔ MOVEMENT IS ALREADY A PER-100 VALUE — `MOVE_DENOMINATOR` is its fixed point, and always was (owner).**
> That is why routes author 5–100: they are denominator units expressing PART STEPS. So the cascade's ×100
> sits on top of a denominator the mechanic already had, and the family slot holds **two scales, each ×100'd**
> — terrain/feature as whole moves (1–6), routes as denominator units (5–100).
> ⛔ Do NOT "finish" this by carrying ×100 deeper into the resolver: that compounds the double-scaling instead
> of resolving it. What has to be decided FIRST is which single denominator movement speaks in, and that is a
> CURATOR question (does terrain author denominator units too?), never a consumer sweep.
>
> ⚑ **The worked case, both ways round, on ONE family (handicap).** `DIPLOMACY_DECLARE_WAR` is a percent, so a
> blanket `÷100` would have turned a 90% AI war probability into **0** — the difficulty setting silently
> switched off. `BARBARIANS_DEFENDERS` is a flat, and reading it raw returned the authored **8 as 800**: a loop
> bound spawning 800 initial defenders, and a `getNumUnits() >= 800` test that could never fire, leaving an
> entire AI branch dead.
> ⚠ **Neither failure crashes, and that is the point** — a mis-scaled CONFIG scaler produces a game that runs
> perfectly while playing by different numbers, so it survives every smoke test. This class is found by
> checking the unit at the boundary, never by observing that the build is green.

## 5. Verification — the math proves the scales, not manual JSON review

The owner cannot eyeball thousands of JSONs, so a mis-scaled field is found by the MATH: the effective value the
authored JSON produces is observed live on the `/computed` oracle endpoints, on a real save
([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)). **Residual divergence localises
the next mis-scaled field** → fix the curator → regenerate → re-check. Exact parity is the bar — 0 in-scope mismatches; a residual divergence is a data-collection gap (a still-mis-scaled field), never a formula difference ([DEC-parity](../../architecture/decisions.md#dec-parity)).

## See also
- [decisions ledger](../../architecture/decisions.md) — `DEC-fixedpoint-x100`, `DEC-curator-owns-descale` index
  this doc as their home.
- [legacy-value-calc-map.md](../../reference/legacy-value-calc-map.md) — the full per-calc
  DESTROY-pass map this scale work feeds.
- [modifier.md](../modifier.md) — the §2 arithmetic that consumes ×100 values.
