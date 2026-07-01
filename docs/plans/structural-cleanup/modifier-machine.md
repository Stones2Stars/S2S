# The modifier machine — in-DLL build plan ("how much?")

> The next cascade machine after the tally + readJson mapping. It reads the **mapped `CvJsonInfo`** (the deposits
> readJson mapped into the per-type `InfoRepo<CvXInfo>`, `Repos/InfoRepo.h`) + the **live engine state**, computes each city's
> per-channel yield/commerce **rate**, and **shadows** it against the legacy `CvCity` accumulators until `diverging=0`.
> Design authority: [`modifier.md`](../../specs/modifier.md) (§2 combine, §2a the realized two-tier rate). Reference
> implementation to PORT: **StoneBase** `src/Application/Features/Calc/*` over `src/CascadingModifier/ModifierMath.cs`
> (parity-proven for yields + commerce). Oracle for the shadow: [`legacy-value-calc-map.md`](legacy-value-calc-map.md)
> (the `getYieldRate100`/`getCommerceRateTimes100` decomposition).

---

## 0. Strategy — PORT IT ALL IN, then compare vs StoneBase (owner ruling 2026-06-30)

**Do NOT chase parity per-increment.** StoneBase has already **mapped every source** and is **parity-proven** (bit-exact
yields + commerce vs `/computed`). So the job here is to **port the WHOLE calc in** (all the §1 packages + the assembler +
the §2 commerce stage — faithfully reproducing StoneBase's `Calc`/`ModifierMath`), and **THEN check whether the in-engine
shadow produces the SAME results StoneBase did.** The verification question is **port fidelity** — "does the C++ shadow
reproduce StoneBase's numbers?" — not an independent per-increment drive to legacy parity. A divergence the in-DLL shadow
shows that StoneBase did NOT is a **port bug** (the C++ doesn't match the C#); a divergence BOTH show is a known StoneBase
residual. So: get it all in, run the holistic shadow, diff the in-DLL result against the StoneBase result. (Per-source
parity-chasing — the percent-stack residuals in §5.1b — is therefore **deferred**: get the full calc in first.)

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

## 2. The input — mapped data + the ENABLER's active state (NOT the live engine)

- **Per-source effect data:** the entity's `CvJsonInfo` via `InfoRepo<CvXInfo>::get().get(id)` — its `deposits`
  (`address` = dotted `family.scope[.target][.member]`, `unit`, `value100`, `enabled`/`disabled` BoolExpr, `hasPer`).
- **⛔ The ACTIVE-SOURCE state comes from the cascade ENABLER, NOT the live engine (owner ruling 2026-06-30).** What
  is *active* — which buildings are non-dormant, which bonuses are connected/available, which civics/traits hold — and
  the `enabled`/`disabled`/`connection` condition evaluation must read the **cascade enabler's** HAVE/active model, so
  the modifier (and the whole cascade) is **self-contained** — it must keep working after the legacy state is cut. The
  enabler's active state is **independently SHADOWED vs the live engine** to prove they are equal (StoneBase proved this
  is achievable). This is *why* the enabler is a co-requisite, built alongside the modifier (cascade-engine-430 §7.4).
  - **⛔ NEVER read legacy COMPUTED/active outputs as a cascade INPUT (owner ruling 2026-06-30, load-bearing).** That
    is the **pollution anti-pattern** — the cascade depending on the very state it replaces — and it is *the* mistake
    that had to be corrected repeatedly before StoneBase got it right (validation.md's pollution guardrail: engine-
    calculated data enters ONLY at the comparison boundary, never as an input). Legacy reads are allowed ONLY when
    **explicitly designated**: (a) the comparison boundary (the shadow diff vs the oracle), and (b) named raw INPUTs
    (the saved/base state — buildings built, bonuses present, civics adopted, the trade-route yield input). A legacy
    **computed** output (`isActiveBuilding`/dormancy, connected-bonus resolution, `getBaseYieldRateModifier`) is OFF
    LIMITS as input — the cascade computes active state itself (the **enabler** from raw), and the modifier reads that.
  - **✅ DONE (2026-07-01) — building active/dormant no longer rides in.** The old debt (the percent stack read the
    engine's `isActiveBuilding` = present ∧ ¬`isDisabledBuilding`, a *computed dormancy verdict*) is removed: dormancy is
    now DERIVED by the cascade from `requires.operate` + dormant triggers (`EnablerKernel::computeActiveBuildings` →
    `CvCascadeEvalCtx::activeBuildings`, read via `cascadeIsBuildingActive`). Only raw presence (`hasBuilding`) is read.
    Governed by [DEC-calc-zero-ride-in](../../architecture/decisions.md#dec-calc-zero-ride-in)'s camouflaged-case clause.
  - **VICINITY (owner ruling 2026-07-01) — split by kind:** vicinity **geometry is already CALCULATED** from plot state
    (`ev_vicinityHas` scans the workable radius: `getCityIndexPlot`/`getBonusType`/`getOwner`/`isBeingWorked` for the
    owned/neutral/worked/crossBorder discriminators — no engine read); **trade-connection is accepted STATE** (`hasBonus`
    — we don't model roads/connected cities, so "connected" is a raw input, not derived). Both correct as-is.
  - **⏳ REMAINING (parked — trivial, but needs the fact-wiring done once):** two `hasVicinityBonus` reads survive in
    `ev_vicinityHas`. (a) The **building-`provides` supply** fallback (a herd/tamed-animal building supplies e.g. horse
    in-vicinity, json §5a — horse units gate on it): this is trivially DERIVED from JSON — the union of *active*
    buildings' `provides.bonuses` (edge `provides.bonuses`), so it becomes a precomputed `vicinityProvidedBonuses` ctx
    fact (twin of `activeBuildings`), not `hasVicinityBonus`. (b) The **`CONNECTED` discriminator** — the route/trade
    "obtained" case — stays STATE (per the vicinity ruling). Do (a) as ONE "eval-context facts" pass that computes
    `activeBuildings` + `vicinityProvidedBonuses` per city and feeds BOTH machines (closing the gap that `activeBuildings`
    is currently wired only into the modifier's eval path, not the enabler's `requires`-eval) — never a modifier-only bolt-on.
- **Plot/specialist/trade inputs:** the worked plots + assigned specialists; trade-route yield is the one live-yield
  INPUT (folded in, not derived).

## 3. The port — StoneBase `Calc` → C++ (no god-class; a kernel + per-term functions)

| StoneBase | C++ port — per-package static-methods class (2026-07-01) |
|---|---|
| `ModifierMath.cs` (the leaf kernel: `SumUnitAtScope`, `Families`, `ActiveTraitSet`, `PureFilter`, the constants) | **`MMKernel`** (`CvCascadeMMKernel.{h,cpp}`) — static methods over `CvJsonInfo.deposits` + a `CvCity*`/`CvPlot*` context; `enabled`/`disabled` evaluated via `cascadeEvalCondition` (the typed evaluator, not `BoolExpr`) |
| `PercentStack.cs` (modifier = max(0,100+Σ%)) | **`PercentStack::percentStack(channel, city, MMBreak&)`** — city active buildings + empire buildings + civics + traits, Σ `<channel>.<scope>.percent` |
| `YieldBasePackages.cs` / `PlotPackage` (basePlotYield) | **`YieldBasePackages::basePlot(...)`** (+ `tradeRoute` / `freeCity` / `goldenAge` / `specialist`) |
| `BuildingPackage` (AFTER ×100) | **`BuildingPackage::buildingFlat(...)`** |
| `YieldRate.cs` (the assembler) | **`YieldRate::yieldRate100(channel, city)`** — the §1 formula |
| `CommercePackages.cs` / `CommerceSplit.cs` / `YieldSplit.cs` | **`CommerceCalc`** — the §2 commerce second stage (`commerceRate100` + the per-source terms) |

Home: `Sources/Cascade/`. Interface-bounded; wired at the composition root. **Each `Calc` package is a single EXPOSED
surface — a purely-organizational static-methods class (NOT a namespace: mangling risk with VC7.1/Boost/python/EXE-ABI;
NOT a file-static monolith). ✅ DONE 2026-07-01: `MMKernel`/`PercentStack`/`YieldBasePackages`/`BuildingPackage`/
`YieldRate`/`CommerceCalc` (`CvCascade<X>.{h,cpp}`); `CvCascadeModifierMath.cpp` is now just the shadow harness** — the
binding DRY law ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation) /
[patterns.md § DRY](../../architecture/patterns.md)); the current `CvCascadeModifierMath.cpp` monolith is the gap to
close (split per StoneBase `Calc/*`). Multiplier deposits are **identity** (no source authors one — verified in the
readJson survey `mult=0`), so the stack is additive, matching legacy exactly.

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
   (sum a channel's scope-wide `percent` deposits off the mapped `CvJsonInfo`, gated by each deposit's `enabled`/
   `disabled` BoolExpr evaluated against the city's `getGameObject()`; ×100→human via `/100`) + `mm_percentStack`
   (`max(0,100+Σ)` over active city buildings city+area, empire buildings, adopted civics, active traits). Hooked at
   `CvGame::doTurn` after the readJson map as `cvCascadeModifierShadow` — a gated one-shot diff vs legacy
   `getBaseYieldRateModifier`. **Verified live: the stack computes close to legacy with small systematic divergences**
   (the deferred sources below + not-yet-modelled events + always-true deferred predicates), surfaced per city/channel.
   - **1b ✅ DONE (attribution) — divergences localized.** The `[MODIFIER/diff]` line now emits the cascade buckets
     (`bC/bA/bE/civ/tr`) vs the legacy sub-terms (`bld/bon/pow/evt/ply/cap`, the `getBaseYieldRateModifier` parts).
     Verified live: **the BUILDING tier is bit-exact** — `bC` == `bld + bon + pow` in every diverging city (the
     `city.percent` + the bonus- and power-conditioned percents all evaluate correctly; the dominant term is the bulk
     of the value). The residual is **two small, named issues in the player tier** (cascade `bE+civ+tr` vs legacy
     `ply+cap`): **(i)** the **capital term** (`getCapitalYieldRateModifier`) is unmodelled — diverges only in capitals
     — needs the `IS_CAPITAL` predicate on a civic/trait `empire.percent` (a currently-deferred predicate); **(ii)** a
     **player-tier production↔commerce ~5 swap** (cascade slightly high on commerce / low on production, every city) —
     one mis-channeled source, to pin with PER-SOURCE attribution.
   - **1c (next) — drive to parity:** add per-civic/per-trait attribution to pin issue (ii); add the `IS_CAPITAL`
     predicate for (i); fold in the still-deferred sources (PURE_TRAITS filter, civic building-keyed percent, projects).
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
