# Fixed-point & the scale registry — the ONE place scales live

> **Grounding:** every scale below was figured from the math in the cited accessor, not from the field name —
> never eyeball a name. Most citations below (§3, §4c) trace the **pre-migration engine** the curator was built
> against; that source has since been fully superseded and no longer exists anywhere in the tree (not even in
> `SourceArchive/`), so a `CvCity.cpp:NNNN` citation is historical grounding for how the scale was FIGURED, not a
> live pointer to re-open — confirm a disputed scale against the `/computed` decomposition parity (§5), never
> against the old line number. §4b's citations are the exception: those six `…100()` accessors are curator
> input still present in `SourceArchive/Infos/*.h` (see §4b).
>
> This is the **single source of truth for value scales** in S2S. If you need to know whether a quantity
> is human-readable, ×100 fixed-point, a percent, or a multiplier — it is here. Do not re-derive a scale
> in another doc; link this one.

---

## 1. The model — integer ×100 for AMOUNTS, human only at the IN and OUT boundaries

×100 fixed-point is the engine's **native representation for every AMOUNT** — a yield, a combat value, any
magnitude — carried that way through the cascade, the realized getters and the consumers. No floats anywhere —
this is OOS-load-bearing (Civ4 MP is deterministic lockstep; CPU-dependent float math desyncs).
`V100 = round(human × 100)`, so `1.00 → 100`, `7 → 700`, `0.5 → 50`; `FIXED_ONE = 100`.

> **⛔ THE ×100 EXISTS FOR ONE REASON — TWO DECIMALS ON AN AMOUNT.** Integer values are multiplied by 100 so two decimals are available, so anything That is the
> whole of it, and it is what decides where the scale applies.
>
> **⛔ A PERCENTAGE IS THEREFORE NOT SCALED: *"percentages should not have decimals."*** A percent is a
> whole number, so it has no decimals to carry and the ×100 buys nothing. Scaling it costs a **second identity
> constant**: `100 + Σpercent` becomes `10000 + Σpercent` at every site a percent is combined, and that magic
> `10000` then has to be threaded through every consumer — which is exactly the fudge-factor class §4c-bis exists
> to reject. `flat` and `multiplier` DO convert (a flat is an amount; a multiplier is authored on the same
> two-decimal footing, identity 100 → `×1.5` = `150`).
>
> ⚑ **This is NOT a per-channel carve-out** ([the ×100 fixed-point model](#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)'s
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

> **⛔ NO getter reduces, and there are NO discrete carve-outs — every channel works identically.**
> This uniformity IS the rework: *"then we never have to care about what format inside the structure."* A getter that
> reduces internally hands every consumer a pre-rounded number whether or not it wants one, and a consumer needing
> precision cannot get it back — which is the same shoehorn as a `getX`+`getX100` pair, just spelled differently.
> Discreteness is a property of a USE (the game unassigns whole citizens), not of a getter.
>
> **⛔ And the NAME never carries the scale: no `100` suffix on any internal getter / function /
> member.** Every value is ×100 by the universal rule above, so a `100` in the name is redundant noise —
> `getScalar` / `sum` / `expectedModifier`, never `getScalar100`. The one algebra rule universal ×100 brings:
> **never multiply two ×100 values together without rescaling** — the product is ×10000, so a `÷100` belongs at
> the multiply. No site is believed to do this today; any found is a defect to flag, never a silent rescale.

**Why ×100 out to the consumers, not reduced at the getter** ([the ×100 fixed-point model](#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)):
reducing at the getter forces a human-variant getter (a `getX`+`getX100` split) the moment anything internal needs
the ×100 form, and it lets the cascade be shoehorned into legacy-shaped getters instead of the consumers being
rewired — the exact reflex that produced the half-migration. Carrying ×100 out makes format-tracking unnecessary
(the answer is always "×100"), and the visible-100×-if-mis-wired forcing function makes every consumer wire
correctly or be discarded. **Blast radius is never a reason to limit the conversion.** A consumer that only tests
SIGN or ranks is scale-invariant (no change); one that mixes with a whole count reduces at that use; an aggregate
stays ×100 and its own reader reduces. The conversion METHOD is §4c-bis below.

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
([derived data is never trusted from a save](../save.md#5-derived-data-serializes-nothing-)), and a **serialized**
field widens SOFTLY because the save READER absorbs the narrower stored form — keep the member, the name and the
tag ([save.md §8](../save.md)). So width is decided on the merits of the value, never traded against migration
cost.

## 2. The unit table (what readJson does)

| JSON (human) | meaning | internal | combine |
|---|---|---|---|
| `flat: 7` (or `7.5`) | additive +7.00 / +7.50 | `700` / `750` (×100 — an AMOUNT) | summed: `Σflat100` |
| `percent: 25` | +25% | **`25` — NOT scaled** (a percent has no decimals) | summed: `Σpercent`, applied as `(100 + Σpercent)/100` |
| `multiplier: 2` (or `1.5`) | ×2.00 / ×1.50 | `200` / `150` (×100 — identity 100) | product: `Π(mult100/100)` |

⛔ **readJson is the ONE place that knows this, and a CALCULATION never scales:** no value is scaled inside an actual calculation - it is literally readJson's job to The
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

⛔ **THIS PAGE CARRIES RULINGS OF ITS OWN, AND THE PAGES BELOW CARRY THE REST — read both.** It is not a
map you may skip; the parts your work touches are read END TO END on top of it, and the count that applies is
something you FIND, not something you decide ([AGENTS.md](../../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[already human 1 emit as is](fixed-point-and-scales/01-already-human-1-emit-as-is.md)** | 4a. Already-human (×1) — emit as-is |
| **[the closed per 100 set 100 to](fixed-point-and-scales/02-the-closed-per-100-set-100-to.md)** | 4b. The CLOSED per-100 set — ÷100 to humanize |
| **[the 100 space addends that lack a](fixed-point-and-scales/03-the-100-space-addends-that-lack-a.md)** | 4c. The ×100-space ADDENDS that LACK a `…100()` getter — the heuristic's blind spot |
| **[rev two scales in one tag the](fixed-point-and-scales/04-rev-two-scales-in-one-tag-the.md)** | 4c-rev. ⛔ TWO SCALES IN ONE TAG — the RevolutionDCM mods |
| **[unit ask the kinds unit never the](fixed-point-and-scales/05-unit-ask-the-kinds-unit-never-the.md)** | 4c-unit. ⛔ ASK THE KIND'S UNIT, NEVER THE FAMILY'S |
| **[zero no exceptions an indivisible](fixed-point-and-scales/06-zero-no-exceptions-an-indivisible.md)** | 4c-zero. ⛔ NO EXCEPTIONS — AN INDIVISIBLE QUANTITY IS STILL ×100 INTERNALLY |
| **[bis 100 everywhere internally](fixed-point-and-scales/07-bis-100-everywhere-internally.md)** | 4c-bis. ⛔ ×100 EVERYWHERE INTERNALLY — TRUNCATE ONCE, AT THE EDGE |
| **[ter the combat strength cluster](fixed-point-and-scales/08-ter-the-combat-strength-cluster.md)** | 4c-ter. The COMBAT-STRENGTH cluster — the target shape |

