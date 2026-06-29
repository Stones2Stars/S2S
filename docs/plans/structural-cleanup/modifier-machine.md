# The modifier machine — in-DLL build plan ("how much?")

> The next cascade machine after the tally + readJson mapping. It reads the **mapped `CvCascadeData`** (the deposits
> readJson attached per game object, side-table `cascadeForInfo`) + the **live engine state**, computes each city's
> per-channel yield/commerce **rate**, and **shadows** it against the legacy `CvCity` accumulators until `diverging=0`.
> Design authority: [`modifier.md`](../../specs/modifier.md) (§2 combine, §2a the realized two-tier rate). Reference
> implementation to PORT: **StoneBase** `src/Application/Features/Calc/*` over `src/CascadingModifier/ModifierMath.cs`
> (parity-proven for yields + commerce). Oracle for the shadow: [`legacy-value-calc-map.md`](legacy-value-calc-map.md)
> (the `getYieldRate100`/`getCommerceRateTimes100` decomposition).

---

## 1. The realized rate (the target — modifier.md §2a)

A city's per-channel rate is a **two-tier** combine (`StoneBase Calc/YieldRate.cs`, the assembler):

```
rate100 = min(CAP, max(100, (Σ BASE + specialist) × max(0, modifier) + 100·⌊AFTER100 / 100⌋))
```

- **BASE (×1, the percent stack multiplies it):** worked-plot yields (`basePlotYield`) + trade-route yield + free-city
  yield (active traits' `YieldChanges`) + golden-age yield + specialist yields. (modifier.md §2a Tier-1.)
- **modifier = max(0, 100 + Σpercent)** — the single additive **percent stack** over every active source.
- **AFTER (×100, added flat OUTSIDE the stack):** building flat yields, truncated to whole units first
  (`100·⌊after100/100⌋` — the engine's `getExtraYield100` integer-truncation gotcha).
- **CAP** = the `CITY_MAX_YIELD_RATE` constant (never an oracle field). All integer ×100.
- **Commerce (§2)** is the same shape a second time on the COMMERCE-yield × the channel slider + its own baseExtra
  sub-terms; disorder forces the whole rate to 0 first.

## 2. The input — mapped data + live state (NOT a re-parse)

- **Per-source effect data:** the entity's `CvCascadeData` via `cascadeForInfo(&GC.get*Info(id))` — its `deposits`
  (`address` = dotted `family.scope[.target][.member]`, `unit`, `value100`, `enabled`/`disabled` BoolExpr, `hasPer`).
- **The active-source SETS come from the LIVE engine** (not the static data): a city's active (non-dormant) buildings,
  the player's empire-wide buildings, adopted civics, the player's active trait set (option-selected — modifier.md §4
  `ActiveTraitSet`, pure-filtered), projects. The calc iterates these, looks up each one's `CvCascadeData`, and sums
  the channel's deposits at the relevant scope.
- **Plot/specialist/trade inputs:** the worked plots (`/state` equivalent — the live `CvPlot`s the city works) +
  assigned specialists; trade-route yield is the one live-yield INPUT (folded in, not derived).

## 3. The port — StoneBase `Calc` → C++ (no god-class; a kernel + per-term functions)

| StoneBase | C++ port |
|---|---|
| `ModifierMath.cs` (the leaf kernel: `SumUnitAtScope`, `Families`, `ActiveTraitSet`, `PureFilter`, the constants) | `CvCascadeModifierMath.{h,cpp}` — free functions over `CvCascadeData.deposits` + a `CvCity*`/`CvPlot*` context; `enabled`/`disabled` evaluated via the deposit's `BoolExpr::evaluate(CvGameObject*)` |
| `PercentStack.cs` (modifier = max(0,100+Σ%)) | `cvModifierPercentStack(channel, city)` — iterate the city's active buildings + empire buildings + civics + traits + projects, sum `<channel>.<scope>.percent` |
| `YieldBasePackages.cs` / `PlotPackage` (basePlotYield) | `cvModifierBasePlot(channel, city)` — Σ worked plots' isolated base package (calc-map §10.1) |
| `SpecialistPackage` / `BuildingPackage` (AFTER ×100) | `cvModifierSpecialist(...)` / `cvModifierBuildingFlat(...)` |
| `YieldRate.cs` (the assembler) | `cvModifierYieldRate100(channel, city)` — the §1 formula |
| `CommercePackages.cs` / `CommerceSplit.cs` / `YieldSplit.cs` | the §2 commerce second-stage |

Home: `Sources/Cascade/`. Interface-bounded; wired at the composition root. Multiplier deposits are **identity**
(no source authors one — verified in the readJson survey `mult=0`), so the stack is additive, matching legacy exactly.

## 4. Validation — the in-engine SHADOW (per channel, per city)

Per [`validation.md`](../../specs/validation.md): the cascade rate runs **alongside** legacy, emitting a per-turn
`[MODIFIER/shadow]` diff via the spine (gated logging) — cascade vs the live `CvCity::getYieldRate100(ch)` /
`getCommerceRateTimes100(ch)`, **decomposed to the named sub-terms** (BASE/modifier/AFTER) so a divergence localises to
a term (total-observability; the §2a decomposition is exactly why `YieldRateResult` carries the sub-terms). Bar =
PARITY, `diverging=0`; a divergence is a data-collection gap mapped to a named source, never a tolerance. The legacy
accumulators (`m_aiBaseYieldRate`/`m_aiYieldRateModifier`/`m_aiExtraYield`/… — cascade-engine-430 §4) stay
authoritative until the shadow is clean, then are cut (atomic, with the cutover).

## 5. Build increments (each compiles + shadows before the next)

1. **The percent stack** ✅ **DONE (shadow established)** — `Sources/Cascade/CvCascadeModifierMath.{h,cpp}`: `mm_sumPercent`
   (sum a channel's scope-wide `percent` deposits off the mapped `CvCascadeData`, gated by each deposit's `enabled`/
   `disabled` BoolExpr evaluated against the city's `getGameObject()`; ×100→human via `/100`) + `mm_percentStack`
   (`max(0,100+Σ)` over active city buildings city+area, empire buildings, adopted civics, active traits). Hooked at
   `CvGame::doTurn` after the readJson map as `cvCascadeModifierShadow` — a gated one-shot diff vs legacy
   `getBaseYieldRateModifier`. **Verified live: the stack computes close to legacy with small systematic divergences**
   (the deferred sources below + not-yet-modelled events + always-true deferred predicates), surfaced per city/channel.
   - **1b (next) — drive to parity:** add the **sub-term attribution** (building/empire/civic/trait/project buckets, à
     la StoneBase `PercentStack.BySource`) to localize each divergence, then map it to its named legacy source. Fold in
     the deferred sources: the **PURE_TRAITS** filter, the **civic building-keyed** percent, and **projects**.
2. **The AFTER tier** — building flat yields (×100, the truncation gotcha). Shadow vs `getExtraYield100`.
3. **The BASE tier** — plot package (worked-plot isolated base, calc-map §10.1) + specialists + trade + free-city +
   golden-age. The largest piece (the plot calc).
4. **The assembler** — `cvModifierYieldRate100` (the §1 formula) shadowed vs `getYieldRate100`; then the §2 **commerce**
   second stage shadowed vs `getCommerceRateTimes100` (+ disorder → 0).
5. **Drive to parity, then cut** the legacy yield/commerce accumulators + `process*` apply-loops (§4 demolition map).

Then the **enabler** machine (generate→gate) on the same mapped data, and the atomic cutover.

## See also
- [`modifier.md`](../../specs/modifier.md) — the model. [`cascade-engine-430.md`](cascade-engine-430.md) — the parent plan + the §4 demolition map.
- [`legacy-value-calc-map.md`](legacy-value-calc-map.md) — the legacy `getYieldRate100`/`getCommerceRateTimes100` oracle decomposition.
- StoneBase `Calc/*` + `ModifierMath.cs` — the parity-proven reference to port. [`tally.md`](../../specs/tally.md) — the `per`-scaler count source.
