# Superseded & dead-end ideas — DON'T rollerskate back in

> **Status:** living (the inverse of [`patterns/`](patterns/README.md)) · **Grounding:** the cited old
> specs + the landed cascade.
>
> Agents keep *"suddenly wanting to continue"* an early idea that has since been superseded — because the
> old docs still describe it as if it were live. This registry is the antidote: a catalogue of ideas
> **tried and abandoned / mostly-obsoleted**, what each *was*, *why it's dead*, and *what replaced it*.
> **Before reviving any older approach, grep here first.** This is the design-side complement to the
> README §0 *build/toolchain* traps (the dead `.vcxproj`, etc.) — those are "never trust"; these are "don't
> revive."

This area is **living**: when an idea is superseded or killed during the rework, record it here in the same
change ([DEC-keep-unkilled-ideas](decisions.md#dec-keep-unkilled-ideas) — an *un-killed* idea is kept; a
*killed/superseded* one is recorded here so it isn't accidentally revived).

---

## The derived-data REPOSITORY pattern — MOSTLY OBSOLETE (subsumed by the cascade + tally)

- **What it was:** a generic derived-data **repository** — a `TLazy` / version / dirty caching-and-
  aggregation layer holding derived values per city / player / team, replacing hand-rolled caches scattered
  across `CvCity`/`CvCityAI`/`CvPlayer` with repository getters.
- **Why it's mostly dead:** the **#428/#430 cascade + tally subsumes its intended purpose** (authoritative
  additive aggregation of derived state). Its own plan doc says so up front, and that **the cascade
  accumulator is deliberately NOT a repository tenant** — no `TLazy`/version/dirty machinery. Derived
  aggregation now flows through the **tally** (counts roll UP) and the **modifier** (magnitudes deposit
  DOWN), not a repository. → [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md),
  [DEC-tally-serializes-nothing](decisions.md#dec-tally-serializes-nothing).
- **⛔ Do NOT** revive the repository as the data / derived-aggregation mechanism. That is the cascade's
  job now; building a parallel repository layer re-introduces exactly the scattered-cache tangle the
  cascade exists to remove.
- **The one residual sliver (out of *active* scope):** caching of **advisory AI heuristics** (plot danger,
  unit-AI counts, mission targets) and the file-size win of extracting those out of `CvCityAI`/`CvUnitAI` —
  this **touches AI behaviour** (the AI side, out of active scope; `../plans/ai/`), and is **not** the
  data-aggregation use above. If AI-side caching is ever revisited, it is a separate, narrow thing — never a
  general derived-data repository competing with the cascade.
- **Grounding:** old `derived-data-repository.md` (header: "the cascade … subsumes this repository's
  intended purposes"); `tally-cascade-spec.md` §9.

## The cross-entity INVERSION approach — DEAD (superseded by the deliveryguy rule)

- **What it was:** a model where a cross-entity modifier declared on source A (e.g.
  `BuildingInfo.TerrainYieldChanges`) was **inverted** — physically moved onto the keyed target entity
  (`CvTerrainInfo`) — so the target owned and aggregated the inbound modifier. A whole blueprint of ~37 such
  inversions across Bonus/Tech/Improvement/Terrain/Religion/Unit targets was planned.
- **Why it's dead:** the **deliveryguy rule** ([DEC-deliveryguy](decisions.md#dec-deliveryguy)) replaced the
  conditioner-vs-target discriminator with *"who DELIVERS this modifier?"* — and the deliverer (the building,
  route, civic, tech) **owns it, keyed by the target**, *not* inverted onto the target. Inverting would put
  the modifier on an entity that doesn't semantically own it.
- **What replaced it:** keep-on-source / fold-onto-the-deliveryguy →
  [`../reference/cascade/modifier.md` §6](../reference/cascade/modifier.md#6-ownership--the-deliveryguy-rule).
  ⛔ Do NOT reinstate inversion even for Terrain/Improvement/Bonus targets — the old "owner decisions pending"
  are resolved by §6 (keep building-side, keyed by target).
- **Grounding:** `cross-entity-inversion-blueprint.md` (the source for "what it was" + the full ~37-inversion
  catalog) — lives in `old-docs/` and leaves with the pre-rebuild set when the owner archives it out; this
  entry is self-contained so nothing is lost.

## Full LEGACY-CALC-pipeline emulation — DEAD (replaced by dumping calcs from the game)

- **What it was:** the plan to **emulate the entire legacy value-calc pipeline offline** (gather + combine)
  so the cascade could be diffed against a reconstructed legacy result — the `calc-emulator-spec` ambition.
- **Why it's semi-retired (owner 2026-06-20):** it proved **easier to dump the individual calculations from
  the game itself** (the `/diagnostic/cityInput` etc. surface — [`../reference/cascade/legacy-value-calc-map.md`](../reference/cascade/legacy-value-calc-map.md)
  §12) than to re-implement the legacy gatherers offline. So the *full-pipeline* emulator is dead.
- **What survives:** the **cascade calc emulator** — `Tools/ModifierCalc/cascade_sim.py`, the *combine-only*
  comparator (legacy combine vs cascade combine on the same input vector) — **is still used** and is the live
  doc'd tool in [`../reference/cascade/shadow.md`](../reference/cascade/shadow.md) §7. Don't confuse the two:
  the legacy-pipeline emulation is dead; the combine comparator lives.
- **Grounding:** `json-migration/calc-emulator-spec.md` (the retired ambition, archived with the migration);
  `shadow.md` §7 (the surviving tool).

## See also
- [`patterns/`](patterns/README.md) — the *landed* patterns (what TO build with).
- [`../README.md`](../README.md) §0 — the build/toolchain traps (the dead `.vcxproj`, etc.).
- [`README.md`](README.md) — this area (ideas + designs + rulings + dead-ends).
