# Superseded ideas — the don't-revive registry

> Dead approaches kept so they aren't reinvented ([DEC-keep-unkilled-ideas](decisions.md)). Condensed — one entry
> per dead idea: what it was, why it's dead, what replaced it.

1. **Derived-data repository pattern** *(mostly obsolete)* — a `TLazy` / version / dirty aggregation layer. Killed:
   the cascade + tally subsume it (tally counts UP, modifier magnitudes DOWN). One residual: AI-heuristic caching
   (plot danger, unit-AI counts) is separate + out of scope. **Don't revive the repository as a
   data/derived-aggregation mechanism — that is the cascade's job.**
2. **Cross-entity inversion** *(dead)* — ~37 inversions that physically moved cross-entity modifiers onto the keyed
   target entity. Killed by [DEC-deliveryguy](decisions.md): the deliverer owns the modifier keyed by target, not
   inverted onto it ([modifier](../specs/modifier.md) §4). **Don't reinstate inversion**,
   even for Terrain/Improvement/Bonus targets.
3. **`loadPrune`** *(dead)* — a curator-era INVENTION: the legacy
   `OnGameOptions`/`NotOnGameOptions`/`PrereqGameOption` validity tags re-encoded as a bespoke "prune at load"
   section, named BACKWARDS (`onGameOptions` meant *keep only when on*) and spec'd wrong, while the spec already
   had the answer (`GAMEOPTION_X` as an ordinary condition). Killed whole: the payload
   authors as the **entity-level `enabled`/`disabled` gate** ([DEC-entity-gate](decisions.md), json.md §2); the
   complex-trait entries dropped outright (they restated the simple/complex FOLDER split, which is the selection
   mechanism). **Don't revive a bespoke game-option section.**
4. **Full legacy-calc-pipeline offline emulation** *(dead)* — emulating the whole gather+combine pipeline offline.
   Retired: it proved easier to dump individual calcs from the game itself. The Python per-scope/combine calculators
   that briefly carried it forward, and the first-version .NET validator, are **likewise dead** — **[StoneBase](../specs/validation.md)
   is now the sole validation tool** (the zero-ride-in principle still holds: [DEC-calc-zero-ride-in]).
5. **The `any:[[…]]` "AND-of-ORs" condition shape** *(dead)* — `any` holding lists-of-lists to mean "OR-groups
   AND-ed together". Killed: a condition is a plain **recursive boolean tree** (`all`/`any`/`noneOf`, nestable —
   json.md §3.4); "(A or B) AND (C or D)" nests two `any` under an `all`. The temporary hand-rolled
   `vector<vector<leaf>>` parser was the same mistake — route through `BoolExpr`. **`any` never means AND.**
6. **The `byEra.{C2C_ERA_*}` value-table key** *(dead)* — an agent-invented bespoke era band-table. Killed: era is
   the plain `ERA` counter; era-dependent values are ordinary conditioned deposits on an `ERA` threshold
   (json.md §6). **No bespoke era key.**
7. **Condition-carrying sub-scope members** (`empire.capital`, `perMilitaryUnit`, …) *(dead as a class)* — encoding
   a deposit's condition as a bespoke member instead of a predicate/`unit:` qualifier. Killed by
   [DEC-conditions-are-predicates](decisions.md) (the golden-age member-mirror is the one deferred exception).
   `perMilitaryUnit` specifically authors as the `cities.{unit: IS_MILITARY}` entry (json.md §3.7).
8. **The "deliberately more permissive" vicinity model** *(dead)* — vicinity with no ownership filter. Killed:
   vicinity mirrors the engine's ownership tiers (owned ⊂ owned+neutral ⊂ crossBorder; json.md §3.4, enabler.md §3).
9. **The tally as a store** *(dead)* — a tally-owned accumulator / load-time event-replay rebuild duplicating the
   object-owned counts. Killed: the tally is a read-only accessor + roll-up over the objects' own counts
   ([tally.md](../specs/tally.md), [DEC-tally-serializes-nothing](decisions.md)). **Never re-add a tally-side store,
   seed, or shadow.**
10. **`grants.specialists`** *(dead)* — free specialists as a grant. Killed: free specialists are the
    `freeSpecialists` MODIFIER family, alive-with-source ([modifier.md §6](../specs/modifier.md)); zero authorings
    ever existed.
11. **Graded parity tolerance (the six-rung "care scale")** *(dead)* — grading a divergence's acceptability. Killed
    by [DEC-parity](decisions.md): exact match, no tolerance band, no agent grading; a divergence is a
    data-collection gap to close.
12. **The `/shadow/*` endpoint surface** *(dead)* — in-DLL cascade-vs-legacy sweep endpoints. Killed: the two
    verification legs never mix surfaces ([validation.md](../specs/validation.md)); the shadow rode the gated
    logging, and the shadow phase itself has since ended.
