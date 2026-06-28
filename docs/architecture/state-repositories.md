# State repositories — recompute-only caches with a dirty trigger

**Status:** landed for the plot-yield cache (2026-06-27) and extended to the **specialist** commerce/yield getters and
the **building** commerce + yield (squirrelBanana) caches (2026-06-28, all verified live-parity-clean). The pattern is
the cure for a whole class of "stale cache" bugs; the plot cache is the proof, and the model the rest of the engine's
derived state should follow. The reusable [`CvDerivedCache`](#the-standardized-cvderivedcache-component-formalized-2026-06-28-built-at-shadow--final-migration-time)
component (below) formalizes the hand-rolled instances; building it + migrating them onto it is deferred to shadow/final-migration time.

`CvPlot` and `CvCity` are **domain objects** — the in-game data entities — and they **stay**. This is not about
dissolving them (that's `CvCityAI`'s eventual job, the AI/behaviour tangle riding on top). It is about how their
**derived data** (yields, and later commerce/health/…) is computed and kept coherent.

## The problem: no unified `dataChanged` trigger

Every derived value in the engine is a **hand-maintained cache with ad-hoc, gappy invalidation**. There is no single
"the source changed, refresh me" primitive, so caches drift out of sync with the data they derive from. The
2026-06-25→27 parity work hit this repeatedly — every instance was the *same* disease, not independent bugs:

- `getImprovementYieldChange` (a city cache) kept a **dormant building's** improvement-yield because the dormancy path
  never decremented it, and "recalculate modifiers" never rebuilds that per-improvement cache.
- A building value change (`MODERN_GRANARY` 1→2) left the cache on the old value.
- `doVicinityBonus` only stamps on a bonus **transition**, so a building built *after* the bonus connected never
  refreshed; the tech yield only enters on **research**, so build-after-tech never refreshed.
- `/state/all` and `/computed` could report **different** worked-plot yields for the same city at the same time,
  because they refresh on different schedules.

The tell is **squirrelBanana** (`CvCity::getBuildingExtraYield100`) and the reverted `getImprovementYieldChange`
bypass: both **recompute every read**. That is not a design choice — it is a *workaround for a cache nobody can trust*
because there is no reliable refresh. Correct, but it pays the full cost on the hot path.

## The model

> **domain object mutates → flips a dirty flag → the derived value recomputes lazily on next read → consumers up the
> chain read that one value.** One trigger, one refresh path, one source of truth.

A derived cache in this model is:

1. **Lazy + dirty-flagged.** A trigger flips `dirty`; the value recomputes on the next read and clears the flag. The
   expensive recompute runs **once per change-then-read**, never per change and never per read.
2. **Recompute-only, NOT serialized** — the [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is dirty by default, so the first read
   recomputes from current state — **never stale-from-save**, the bug class above killed at the root. Drop serialization
   via `WRAPPER_SKIP_ELEMENT` (the soft-remove of [DEC-save-remove-is-soft](decisions.md#dec-save-remove-is-soft)): it
   consumes a removed field's bytes from an *old* save by name so nothing after it shifts, and is a no-op on a new save
   that never wrote it. *Just deleting the read/write breaks the save layout.*
3. **The single source — PULL, not push.** Things up the chain (the city, the diagnostics, the cascade oracle) **read**
   the value; the source does not **push** deltas into them. Push + a parallel cache double-count and drift; pull from
   one authoritative value cannot.

## First instance: the plot-yield cache (`CvPlot`)

- `m_aiYield` (the cached per-yield value) + `mutable bool m_bYieldDirty` — **neither serialized** (`m_aiYield` via
  `WRAPPER_SKIP_ELEMENT(... SAVE_VALUE_TYPE_SHORT_ARRAY)`, no write; `m_bYieldDirty` true on construct).
- `getYield()` — `if (m_bYieldDirty) recomputeYield(); return m_aiYield[i];`. O(1) when clean.
- `recomputeYield() const` — sums the plot's yield, with the **building→improvement keyed buff summed FRESH** over the
  working city's *active* (non-disabled) buildings rather than read from the stale `getImprovementYieldChange` cache
  (the "buildings buff a yield on an improvement" feature the legacy cache doesn't carry). Clears the flag.
- `updateYield()` — now the **trigger only**: flips `m_bYieldDirty`, fires `AI_setAssignWorkDirty` + `onYieldChange`
  (the downstream commerce/UI dirty the old push carried). No eager recompute, **no push**.
- `CvCity::getPlotYield()` — **pulls** `Σ` over worked plots of `getYield`, replacing the push-maintained
  `m_aiBaseYieldRate` (`changePlotYield` and the `processWorkingPlot` push are gone; `m_aiBaseYieldRate` is dead).

## Why this is the right fix, not a patch

The engine's *actual* base yield (`getPlotYield`) now equals the build-order-independent value the cascade has always
computed, because both sum the live data the same way — the stale-cache divergences are resolved **at the source**, not
papered over in the cascade. This is mirror-phase, behaviour-preserving ([DEC-parity](decisions.md#dec-parity),
[DEC-mirror-then-redesign](decisions.md#dec-mirror-then-redesign)). It also dissolves the perf objection to the per-read
bypass — the fresh sum runs only on recompute (change-then-read), never per read.

(Per [DEC-no-parity-results-in-docs](decisions.md#dec-no-parity-results-in-docs), the pass numbers themselves stay out of here.)

## The direction

- **Build out the trigger.** Today `updateYield`'s many call sites are the de-facto trigger; the destination is a
  unified `dataChanged`/dirty propagation (plot dirty → city dirty → commerce dirty) so every derived layer is a thin
  recompute-only cache over the one below.
- **City layer pulls plots.** `getPlotYield` is the first pull; the city's other derived caches (commerce, health) get
  the same treatment, pulling through the plot/city repository contract.
- **Retire the workarounds.** squirrelBanana and any remaining recompute-every-read getters collapse to a trustworthy
  cache + read once the trigger is real.
- This is the Clean-Architecture north-star applied to engine state: the repository **is** the contract, and it is the
  lever for thinning the `Cv*` god-classes without touching the closed-EXE-bound `CvPlot`/`CvCity` layout. See
  [north-star](north-star.md).

## The standardized `CvDerivedCache` component (formalized 2026-06-28; BUILT at shadow / final-migration time)

> **Decision (owner 2026-06-28):** the pattern above gets **formalized into ONE reusable C++03 component** rather than
> hand-rolled per cache (the plot-yield cache's `m_aiYield`+`mutable m_bYieldDirty`+`recomputeYield`, and the interim
> recompute-on-read getters for specialist commerce/yield, are each the same shape written by hand). **It is NOT built
> now** — it is captured here so it is ready when we **build the shadow and do the final migration**; the current
> recompute-on-read getters stand in until then (correct, just the per-read-cost workaround §"Why this is the right fix").

**Chosen mechanism — a templated value-holder with the recompute injected as a member-function-pointer** (the one part
that genuinely needs owner state stays owner-side; everything else — storage, dirty flag, pull-on-read, trigger — is the
reusable contract). Poor-man's-DI-adjacent ([patterns](patterns.md)): the recompute is the injected dependency.

```cpp
template <class TOwner, class T, int N>
class CvDerivedCache {                       // recompute-only, dirty-flagged, NEVER serialized; the single PULL source
    mutable T    m_data[N];
    mutable bool m_dirty;
    TOwner*      m_owner;
    void (TOwner::*m_recompute)(T*) const;   // fills m_data from CURRENT state (needs owner; stays owner-side)
public:
    void bind(TOwner* o, void (TOwner::*fn)(T*) const) { m_owner = o; m_recompute = fn; m_dirty = true; }
    void markDirty() { m_dirty = true; }     // the trigger — call at every input-change site (no eager recompute, no push)
    T    get(int i) const { if (m_dirty) { (m_owner->*m_recompute)(m_data); m_dirty = false; } return m_data[i]; }
};
// e.g. int CvCity::getSpecialistCommerce(CommerceTypes e) const { return m_specCommerceCache.get(e) / 100; }
```

- **It is a DATA MEMBER** on `CvCity`/`CvPlot` — fine: data members are added routinely (`m_bYieldDirty` was just added
  to `CvPlot`); the [patterns](patterns.md) guardrail bars adding vtable *bases* to EXE-bound classes, **not** data members.
- **Never serialized.** The owner's `read()` does `WRAPPER_SKIP_ELEMENT` for the legacy field (the
  [DEC-save-remove-is-soft](decisions.md#dec-save-remove-is-soft) soft-remove); the cache is dirty-on-construct, so a
  loaded game recomputes on first read — never stale-from-save.
- **Apply to** (at build time): migrate the hand-rolled **plot-yield** cache onto it; convert the **specialist-commerce**
  and **specialist-yield** recompute-on-read getters onto it; then the future **commerce / health** city caches — one
  pattern everywhere. This is the concrete form of the unified `dataChanged` trigger named in "The direction" above.
