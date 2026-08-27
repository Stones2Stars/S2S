# DRY — one implementation per calculation / evaluation (the single-source law)

> Part of the **[patterns](../patterns.md)** spec.

> The law that keeps the cascade from becoming C2C again. C2C's decades-old disease is **N evaluators computing the
> same thing slightly differently**; this rule forbids it. Grounded in the reference impl: **StoneBase already has this
> separation** (one exposed unit per `Calc/*` package, one `ConditionEvaluator`) — the C++ port must carry it over, not
> flatten it. Binding: [the DRY single-implementation law](#dry--one-implementation-per-calculation--evaluation-the-single-source-law).

**The law.** Every calculation and every evaluation exists **exactly once**, as a **pure static function fed its
inputs** (data + context → value), reachable by every consumer. No machine reimplements another's logic; a machine that
needs a fact FEEDS it to the one function, it never re-derives it.

1. **One evaluator for conditions/predicates.** `cascadeEvalCondition` is the **sole** place a condition/predicate is
   evaluated. The enabler and the modifier **delegate** to it (`EnablerKernel::requiresMet`, `MMKernel::applies` are
   thin wrappers) — they
   never re-read a predicate. A machine that needs a fact the evaluator uses (`hasVicinityBonus`/`isGovernmentCenter`/
   active-building) **supplies it through the eval context** (the precomputed operating-building set), never evaluates it itself.
   ⚠ `BoolExpr` still exists and still serves the KEEP-legacy property engine — it is not the cascade evaluator,
   and translating a `CvCondition` back into one so another solver can evaluate it is a SECOND evaluation surface,
   whatever it is named.
2. **One function per calculation**, mirroring StoneBase's `src/Application/Features/Calc/*` packages **1:1**:
   `PercentStack` · `YieldBasePackages` · `YieldRate` · `YieldSplit` · `CommerceSplit` · `CommercePackages` ·
   `BuildingPackage` · `CalcContributions`. No parallel or near-duplicate calc anywhere.
3. **Pure static functions, no hidden state.** A calculator/evaluator takes everything it needs as parameters and
   returns a value, holding **no data members** — data lives in the `InfoRepo`, counts in the tally; that purity
   *is* the DRY guarantee. Grouping is fine as a **static-methods class** (à la StoneBase's `static class
   PercentStack`), never a namespace (funky name-mangling risk under VC7.1/Boost/`boost::python`). Forbidden: an
   instance, any member field, a namespace grouping, or a file-`static` function no other unit can reach.
4. **Exposed, never file-`static`-hidden.** Each calculator/evaluator is a declared surface (a header) reachable by
   every consumer — a file-`static` calculator is a DRY hazard the next consumer can't see, so it reimplements it,
   the exact mechanism of the C2C rot. *(Realized: the deposit-read side — `MMKernel` (the per-deposit leaf
   primitives), `Data/CvDepositRead.h`; `InfoValuation`, `Data/CvInfoValuation.h`, carrying StoneBase's `Calc/*` packages (the per-group walk, `YieldRate`
   §2a's `cityRate` combine, `CommerceSplit`'s `commerceSplit`, the plot-as-base package, the cross-scope roll-up)
   — and the enabler (`EnablerKernel` + `TechEnabler`/`BuildingEnabler`/`UnitEnabler`/`CivicEnabler`/
   `ProcessEnabler`/`ProjectEnabler`/`PromotionEnabler`/`BuildEnabler`, `Sources/Enabler/Cv<X>Enabler.{h,cpp}`,
   mirroring StoneBase `CascadingEnabler/*`) are both split this way.)*
5. **Harness ≠ calc.** The observability surface and the spine logging are
   **separate consumers** of the calc surface, never folded into the calc functions.
6. **Single source of "active".** "Is X active / available / connected / non-dormant" is computed **once, by the
   enabler**; the modifier reads it, never recomputing from the live engine or the engine's *dormancy verdict*
   (the camouflaged ride-in, [the pollution guardrail](../../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)). *(Realized: for
   buildings, `EnablerKernel::recomputeOperatingBuildingsInto` derives active/dormant from `requires.operate` +
   dormant triggers into `CvCascadeEvalCtx::activeBuildings` (twin of `waivedPrereqBuildings`), read via
   `cascadeIsBuildingActive` (never `isActiveBuilding`); the same pass fills `vicinityProvidedBonuses` for
   in-vicinity `provides` (json §5a). Two
   states stay ENGINE-OWNED inputs instead, because the cascade does not model their driver: route/trade
   `CONNECTED` (the network), and CORPORATION active/dormant (per-turn spread state, like religion —
   `isActiveCorporation`; [culture-religion-research.md](../../reference/culture-religion-research.md)).)*
7. **No duplication is sanctioned.** During the migration the legacy shadow was the one sanctioned duplication (the
   cascade running *alongside* legacy, diffed, with a defined death — [the map-before-delete discipline](../../../AGENTS.md#cascade-observability--the-total-observability-orwell-bar));
   **the shadow phase has ended** ([validation](../../specs/validation.md)), so no duplication is sanctioned at all.
8. **Composition root names concretes** ([the interface-contracts pattern](01-the-interface-shape-composability.md#the-interface-shape-composability)) — the
   active-set / game-option swaps are picked there; a leaked concrete `#include` into a consumer breaks the single wiring point.
9. **⛔ ONE PATH PER MUTATION — EVENTS AND WORLDBUILDER GO THROUGH THE SAME ONE.** The law above governs
   reads and evaluations; it governs WRITES identically. A given state change has ONE published path, and every
   caller uses it: a random event granting a tech, a WorldBuilder screen setting it, and any other mutator are the
   SAME call, never a per-caller variant.
   ⚑ **The reason is that a second path is how the two drift into disagreeing about what the mutation MEANS** —
   one remembers to announce the crossing, refresh the dependents or refcount the grantor and the other does not,
   so the editor produces a state the game can never reach and a bug reproducible only through one of them. That
   is the C2C disease in write form, and it is worse than the read form: a divergent read is wrong, a divergent
   write is CORRUPTING.
   ⚠ **A WorldBuilder caller is not licence to bypass the path** because "it is only an editor". If a mutation is
   safe to perform, it is safe through the shared path; if the shared path refuses it, the editor must not be
   doing it either. ⇒ Where an editor genuinely needs a capability gameplay lacks, that is a MISSING VERB on the
   shared surface to add deliberately, never a private setter beside it.

**Enforcement (how to keep certainty).** The data-machine trees (`Sources/Data/`, `Sources/Conditions/`,
`Sources/Enabler/`) should read like `StoneBase/src` — one unit per `Calc` package, one evaluator. To verify: grep for a second implementation of any calc/predicate; confirm every
machine's condition gate routes through `cascadeEvalCondition`; confirm no calculator holds state. **A new
"does-the-same-thing" function is the failure** — reuse the existing one, or lift it to the shared surface. This is the
anti-rollerskate check an agent runs before adding cascade calc/eval code.

