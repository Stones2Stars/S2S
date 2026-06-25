# Superseded ideas — the don't-revive registry

> Dead approaches kept so they aren't reinvented ([DEC-keep-unkilled-ideas](decisions.md)). Condensed.

1. **Derived-data repository pattern** *(mostly obsolete)* — a `TLazy` / version / dirty aggregation layer. Killed:
   the cascade + tally subsume it (tally counts UP, modifier magnitudes DOWN). One residual: AI-heuristic caching
   (plot danger, unit-AI counts) is separate + out of scope. **Don't revive the repository as a
   data/derived-aggregation mechanism — that is the cascade's job.**
2. **Cross-entity inversion** *(dead)* — ~37 inversions that physically moved cross-entity modifiers onto the keyed
   target entity. Killed by [DEC-deliveryguy](decisions.md): the deliverer owns the modifier keyed by target, not
   inverted onto it ([modifier](../specs/modifier.md) §6 resolves the pending cases). **Don't reinstate inversion**,
   even for Terrain/Improvement/Bonus targets.
3. **Full legacy-calc-pipeline offline emulation** *(dead)* — emulating the whole gather+combine pipeline offline.
   Retired: it proved easier to dump individual calcs from the game itself. The Python per-scope/combine calculators
   that briefly carried it forward, and the first-version .NET validator, are **likewise dead** — **[StoneBase](../specs/validation.md)
   is now the sole validation tool** (the zero-ride-in principle still holds: [DEC-calc-zero-ride-in]).
