# #430 — the TODO

> **What is NOT done. Nothing else** ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)).
> The DESIGN lives in the specs; this list measures what is LEFT. A finished item is **DELETED** from here — never
> ticked, never annotated — and anything durable it established moves into its owning spec first. Git history is
> the record of work done.
>
> ⛔ **Verify before you act.** Every line here is a claim about the tree; confirm it against the code before
> building on it ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)). Sequencing and the
> governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- **`defense.counterDamage`** (13 authorings) — its `chance` is an on-attack roll, so it belongs on the trigger
  plane, but re-homing needs an `onAttacked` happening AND a `damage` action verb, neither of which exists. Until
  ruled, the membership-list shape compiles no entries.
- **The `savemigration.txt` parser is PREFIX-FREE** — it skips only `|`/`=`/`#` lines and registers the first
  whitespace-delimited token containing `::`, so the file's own documented `CUT:`/`RENAME:` prefixes are IGNORED
  and a wrapped prose line beginning with a live `Class::m_member` token would silently drain that field on every
  load. The fix (require the documented prefix) changes save-load behaviour, so it is an owner call.

## Data — curator batches

- `culture.unit.garrison` · `costs.empire.perInstance` — flagged in-code, awaiting their batch.
- The ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`, `combat.nukeInterception`) —
  each attaches to its trigger's `chance`; authoring shapes finalize with the trigger system's build-out.

## Stage 4 — the consumer cut (sequenced LAST; see the roadmap's ORDER ruling)

- **The `CvCity`/`CvPlayer` getter consolidation** — known work, not the primary focus, and a fair few collapse on
  their own as the rebuilt infos wire through. Measure what survives that before planning a sweep.
- **The AI call sites** — the largest consumer of the info surface, deliberately last. A dangling AI call site is
  intended output, not a defect to fix on sight.
- **`CvGameTextMgr` composers onto rendered entry lines** — the per-entry renderer exists (`Sources/UI/CvEntryText`);
  the ~18 info-help composer families still hand-assemble from getters.
- **Re-point the unit consumer getters onto `resolvedValue()`** (`Sources/Cascade/CvUnitResolved`).
- **The unit power-value plane** — its readers are ordinary consumer debt on a deliberately red tree.
- **The Python data-fetching library** — built COMPLETE, then the `Cy*` surface disconnected whole. Contract:
  [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Build it for the pedia (a SHAPE oracle,
  NOT a coverage oracle — the appendix is enumerable). Read maps: [pedia-map.md](pedia-map.md) ·
  [python-read-map.md](python-read-map.md).

## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · `CvOutcomeMission::mapFrom` · the property-manipulator helpers ·
  `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine, sequenced after the
  stages complete; the renderer's spell-back fallback is the accepted output until then.

## Enabler

- Converge the private reverse buckets (`s_bc*` / `s_uc*` / `s_op*`) onto `EDGEF_REQUIRED_BY`
  ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
- The remaining §8 open items: residency/counting, plot-group membership not trusted from a save, the load-end
  dormancy fixpoint, the dynamic operate axes ([enabler.md §8](../../specs/enabler.md)).

## Spec gaps to close

- **The mod-data design invariants have no spec home.** `TestCode.py` encoded ~50 consistency checks — a
  requirement may not unlock after the thing requiring it; replacements are explicit, never implicit; a replacing
  entity must be better — and the JSON spec does not currently state them. The checks are gone; the invariants
  belong in the spec.

## Known gaps carried deliberately

- **Game-option flips carry no DOMAIN event** — a mid-game toggle would not re-mark. An emit endpoint is the fix
  if/when WorldBuilder option toggling is in scope.
- **Ranked-target-selection EVALUATION** is parked ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md));
  a ranked entry applies unranked until it lands.
- **The `expected*` endpoints have no callers yet**, so the two pass-in scenarios hold vacuously — the seam is
  built ahead of the package graft by design.
