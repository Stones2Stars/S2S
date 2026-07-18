# Fixed-point conformance — ×100 is the engine's NATIVE representation, out to the consumers

> **Owner ruling (fundamental spec divergence — fix NOW, do NOT defer).** The
> [fixed-point model](../../specs/curators/fixed-point-and-scales.md) / [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100):
> **every value inside the engine is ×100 fixed-point, EVERYWHERE** — through the cascade, through the realized
> getters, and **out to the consumers**. A value is human only at two boundaries: **the reader** (UI / the HTTP API /
> Python — "having any reader do ÷100 is trivial") and **the point it becomes a genuinely discrete game quantity**
> (whole angry citizens, a whole food modifier). **There is no ×100 "variant" of any getter** — `happyLevel()` simply
> IS ×100; we never make a `happyLevel()`+`happyLevel100()` pair.

## Why ×100-out-to-consumers (owner)

- **"then we never have to care about what format inside the structure"** — ×100 is the invariant, so no code ever
  asks "is this scaled?". The answer is always yes.
- **The forcing function.** Making the getter ×100 and pushing it out FORCES every consumer to be examined and
  properly wired (a mis-wired consumer is visibly 100× wrong), while dead consumers are discarded. It surfaces
  remaining legacy the cutover missed and collapses redundant call sites.
- **⛔ Blast radius is NEVER a reason to limit scope.** Reducing at the getter to spare the consumers is the exact
  reflex that produced the half-migration — the cascade gets shoehorned into legacy-shaped getters instead of the
  consumers being rewired through. The mapped consumer surface is the worklist, not a warning.

## The violation it fixes

The cascade reduced ×100 → human **early, per-item, mid-chain** — every gather fold did `sumUnit`/`perScale(...)/100`,
truncating each deposit before the sum. Lossless for integers (`200/100=2`), truncating for fractionals (a `-0.4`
happiness deposit — SPECIALIST_SETTLED_SLAVE_HEALTH authors exactly this — became `0`), and the wrong SHAPE
everywhere (a future fractional value in any channel breaks silently). The realized getter then also returned human,
so the cascade's ×100 world and the engine's human world met at a seam *mid-engine*.

## The model (what the code must be)

**Accumulate and carry ×100 through the ENTIRE chain; convert to human ONLY at the two boundaries.**

- **IN boundary** (human → ×100): `readJson`, once, at load.
- **The engine** (cascade gather + combine + the realized getters + every consumer): ×100, no mid-chain ÷100.
- **OUT boundaries** (×100 → human): (a) **readers** — UI display, the `/computed` HTTP fields, the `Cy*` Python
  wrappers — each does its own trivial `÷100`; (b) **discrete realized quantities** — where the value physically
  becomes a whole game count (the game unassigns WHOLE citizens), the quantity reduces `÷100` and is itself human
  onward.

A consumer that only compares SIGN (`happy − unhappy < 0`) or ranks is scale-invariant — it needs no change, ×100
flows through. A consumer that MIXES the value with a whole count (population, an era index, a config threshold)
reduces `÷100` at that use. An aggregate (Σ over cities) stays ×100; its own reader reduces.

## Wellbeing — the PILOT (landed; under live verification)

Wellbeing is the first channel converted, establishing the pattern the other channels follow.

- **Cascade core ×100** (`CvCascadeWellbeing.cpp`): every gather term accumulates at native ×100 (`sumUnit100`, no
  `perScale(...)/100`, `value100` raw; the `getInitialHappiness` commerce-pool seed ×100; the event ledgers ×100;
  `wb_extraParts` ×100). `assemble` computes each of the four verdicts fully in ×100 — every raw-state input (anger
  percents, vassal, handicap, espionage, tax, foreign, landmark, city-over-limit, freshwater, TEMP_HAPPY) ×100; the
  anger math is `anger% × pop × 100 / DIVISOR` (the ×100 keeps sub-unit anger). The `WbSplit`/`CascadeWbTerms`
  structs hold ×100.
- **The verdicts return ×100** (`CvCity::happyLevel/unhappyLevel/goodHealth/badHealth` → the ×100 `aWbVerdict`; the
  pre-init/what-if legacy siblings stay human internally and are ×100'd at the getter dispatch).
- **The two discrete boundaries** (÷100, the ONLY in-engine reductions): `angryPopulation` (whole angry citizens)
  and `healthRate` (whole food modifier). Inside `assemble`, `iAngry` is computed whole (÷100) and `unhealthyPop`
  re-scaled ×100 — citizens are physically discrete.
- **Readers ÷100**: `Cy*City` (keeps all Python backward-compatible, untouched), the `/computed/cities/wellbeing`
  realized fields, the CityBar + the happiness/health help screens.
- **Every consumer wired**: the AI happiness/health valuations (`CvCityAI`/`CvPlayerAI` — ÷100 each verdict read,
  reproducing legacy's whole-value behavior), the event-trigger thresholds, the score sum, the player aggregates,
  the `getAdditional*` what-ifs, the property `ATTRIBUTE_HAPPINESS`, and the flipped legacy DECOMPOSITION getters
  (`getFeature/Bonus/Building{Good,Bad}{Health,Happiness}`, CvArea/CvPlayer building rollups) — these last read the
  cascade term via the `CascadeWellbeing::*Wellbeing` accessors and ÷100 to keep their human contract (so
  `happyLevelLegacy` stays consistent human).

**Acceptance:** served-value SANITY on `/computed/cities/wellbeing` (per [DEC-oracle-tautology] the check is a sane
number, NOT oracle parity) — specialist-heavy cities gain their previously-truncated fractional health/happiness;
`angryPopulation` stays a sane whole count; no 100× display. NOT bit-parity with the old per-item-truncated numbers
— matching the old truncation IS the divergence being removed.

## Wellbeing legacy-accumulator CUT + the cascade-sourced breakdown (the combat-tooltip pattern)

> **Owner ruling.** The legacy wellbeing ACCUMULATORS go (same as the `*Legacy` verdict bodies already deleted),
> but the **per-scope/per-source breakdown for the Python tooltip STAYS** — re-sourced from the cascade, given
> "the same treatment as the combat-tooltip rework."

**The pattern (from `CvCombatModel.h` `computeCombatPreview`):** ONE cascade-sourced producer returns the realized
numbers **plus** a `detailLines` vector of ready-to-render `{label, signed value, category}` rows; `CvGameTextMgr`
is a **pure renderer** (zero math of its own), printing them generically, coloured by category. The itemised
breakdown is an extension seam (Shift-gated in combat; the wellbeing panel shows it inline).

**Apply to wellbeing:**
- **Producer** — `CascadeWellbeing::computeBreakdown(city)` returns the four verdicts + `happyLines`/`healthLines`/
  `angerLines` (per-source rows: building good/bad, bonus, religion, specialist, civic, feature, extraBuilding,
  area/empire, per-pop, the anger-percent sources, …), every value from the cascade terms already computed in
  `gather`/`assemble` (×100; the renderer ÷100). One producer, single-source (patterns.md).
- **Pure renderers** — `setHappyHelp`/`setBadHealthHelp`/`setGoodHealthHelp`/`setAngerHelp` render the lines; the
  `/computed/cities/wellbeing` decomposition + Python read the SAME producer. No hand-summed decomposition anywhere.
- **DELETE the legacy accumulator cluster** (still present — building/bonus/feature already cut): `extraBuilding`
  (`m_iExtraBuildingGood/BadHappiness`, `…Good/BadHealth`, `…HappinessFromTech`, `…HealthFromTech`), `religion`
  (`m_iReligionGood/BadHappiness` + `updateReligionHappiness`), `specialist` (`m_iSpecialistGood/BadHealth`,
  `m_iSpecialistHappiness/Unhappiness`) — the members, their `change*`/`update*`/`process*` maintainers, and the
  getters that read them (the getters re-point to the cascade breakdown, like the flipped building/bonus/feature
  getters already do). ~106 maintainer/consumer sites across `CvCity`/`CvPlayer` (+ `CvArea`).

**Scope:** a focused increment on the scale of the fixed-point cut above (producer + 4 UI panels + endpoint +
the ~106-site accumulator deletion) — executed in one pass, verified live, so no half-cut state.

## Remaining channels (same pattern, per-channel, verified live)

Yields / commerce / the scalar channels / properties. The yield channel already carries ×100 internally but keeps a
`getYieldRate100` + `getYield` (÷100) SPLIT — the exact "×100 variant" this ruling dissolves: the single getter
returns ×100 and every consumer reduces at its reader/discrete boundary. Each channel's `MMKernel`/`Calc` gather
moves off the truncating `sumUnit` onto `sumUnit100`; the enforcement point is the single-source calculators
(`sumUnit` — the truncating variant — ultimately retires). Map the consumer surface exhaustively per channel
([DEC-all-means-all]); convert; verify live.
