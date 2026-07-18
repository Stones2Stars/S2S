# Fixed-point conformance — the cascade must be ×100 THROUGHOUT, reduce ONCE at the boundary

> **Owner ruling 2026-07-18 (fundamental spec divergence — fix NOW, do NOT defer).** The
> [fixed-point model](../../specs/curators/fixed-point-and-scales.md) / [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100):
> **JSON human → C++ cascade ×100 EVERYWHERE → reduce to human ONCE at the frontend (the Python layer is our
> definition of "frontend").** There is **no ×100 "variant" of any variable** — every cascade value is simply ×100.
> Agents shoehorned the cascade to match old per-item-truncating data instead of fixing the chain; that is the
> divergence this fixes.

## The violation

Deposits are stored ×100 (`dep.value100`, `readJson` converts human→×100). But many gather sites reduce **early,
per-deposit**, inside the accumulation:

```cpp
t.spec.iGood += (int)(v100 / 100);                                   // CvCascadeWellbeing.cpp — per specialist type
t.featSubstrate.fold(it->second / 100);                             // per feature
t.extraB.fold((int)(MMKernel::perScale(dep, ec, dep.value100) / 100));  // per deposit
```

Consequences:
- **Truncation loss on fractional deposits** — a specialist worth `0.75` health is `75` (×100); `75/100 = 0`
  truncates it to nothing. The realized level silently loses every sub-1.00 contribution. (Integer deposits like
  a building's `+2` happy are `200`; `200/100 = 2` is lossless — which is why the bug hid: only fractional
  channels visibly break.)
- **Truncation-ORDER divergence vs legacy** — legacy summed the ×100 values and divided `/100` **once** at the
  verdict (`getSpecialistGoodHealth()/100`, `CvCity.cpp` `5914` etc.); the cascade divides per-item then sums, so
  even the integer case can drift by a unit.
- **Wrong shape everywhere** — a future fractional value in any channel breaks silently.

## The correct model

**Accumulate ×100 through the whole chain; apply the single `/100` reduction at the ONE boundary where the value
leaves the cascade** — the realized getter / verdict read (the C++→gameplay boundary) or the Python frontend
(the C++→display boundary). The yield/commerce/maintenance channels already do this: `getYieldRate100` /
`…Times100()` carry ×100 and the human getter divides once (`getMaintenance = …Times100()/100`). Every channel
matches that shape.

## The sites (mapped 2026-07-18 — audit adversarially before cutting; grep `/ *100` per file)

| file | ~sites | boundary (where the single `/100` belongs) |
|---|---|---|
| `CvCascadeWellbeing.cpp` | 4 gather + the `assemble` reads | terms accumulate ×100; `assemble` divides each term `/100` once when folding into the (human-integer) verdict; the `getBonus*/getBuilding*/getFeature*/getSpecialist*` getters return ×100 (their consumers already `/100`, e.g. `goodHealthLegacy` 5914) |
| `CvCascadePropertyBridge.cpp` | 4 | the property value boundary |
| `CvCascadeProperty.cpp` | 3 | ditto |
| `CvCascadeMMKernel.cpp` | 3 | verify: `perScale` must RETURN ×100; the caller keeps ×100 — the `/100` is the caller's early-reduction bug, not the kernel's |
| `CvCascadeScalarChannels.cpp` | 2 | the scalar getter boundary |
| `CvCascadeGrants.cpp` | 2 | the grant apply boundary |
| `CvCascadeAccumulator.cpp` | 2 | the realized-rate read |
| `CvCascadeReadJson.cpp` | 1 | verify — readJson is the human→×100 IN-boundary, so a `/100` here is likely wrong |

⚠ NOT every `/100` is a bug: a `× slider% / 100` percent application, a `× perCount / each` scaler, and the
genuine OUT-boundary getters (`…100()/100`) are correct. The target is the **per-item reduction folded into an
accumulator** — that value must stay ×100 until the accumulator is read.

## Execution (per channel, verified LIVE — never a blanket sweep)

1. **Wellbeing first** (the confirmed bug, and where the getters are mid-flip): terms → accumulate ×100; `assemble`
   → `/100` per term at the verdict fold; getters → return ×100. Verify: `/computed/cities/wellbeing` served
   verdict unchanged for integer-dominated cities, and specialist-heavy cities gain their previously-truncated
   fractional health/happiness. `cascHappy`/`cascGoodHealth` are the oracle to reconcile against — but per
   [DEC-oracle-tautology](../../architecture/decisions.md#dec-oracle-tautology) the real check is served-value
   sanity, not oracle parity.
2. Then property, scalar, grants, accumulator — each channel's gather → ×100, boundary → single `/100`, verified
   on its `/computed` surface.
3. `MMKernel` / `readJson` — verify these are kernel-correct (the reduction is the caller's, not theirs) before
   changing.

**Bar:** the served value is SANE and gains no truncation loss; the `(scope,channel)` calc-count gate stays flat.
Do NOT shoehorn back to old per-item-truncated numbers — matching the old truncation is the divergence, not the goal.
