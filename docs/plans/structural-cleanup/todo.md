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

## Data — blocked on a prerequisite

- **`paralyze` → the `state` block** — blocked on the greenfield `state` model ([state.md](../../specs/state.md)),
  which is work to BUILD. No data is lost meanwhile.
- **The FLAGGED unitcombat remainder** — the taxonomy families (weapon/size/species/quality/group) and the
  ambiguous individual classes; map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).
- **`stronglyRestricted` → a `requires.build` civ-membership gate** — pending NPC civilizations being wired.
- **Property pulses** — a shared property-source cleaner so spatial sources emit as trigger entries carrying
  `on`/`relation`/`distance`, instead of being parked verbatim. Pure DATA and unblocked; the ENGINE spatial
  distribution that later reads those fields is a separate consumer.
- **Corp HQ revenue** (`HeadquarterCommerces`) — rides the corporation rework carve-out.
- **`largestCity` cannot retire** until ranked-target-selection EVALUATION lands, so the civic/trait curators still
  emit the legacy member.

## Data — cross-curator claims to VERIFY

- `curate_bonus` actually inverts the civic bonus-commerce modifiers (the civic curator drops them on that promise).
- The yield resolver reads `identity.movementCost`.
- The property propagator/change-propagator re-homes actually happen at the unit/building passes.
- PropertyBuilding min/max value bands are consumed by the building pass.
- Every bespoke second-pass tag has live emit code, not just set membership.
- Stale tooling docs: `Tools/Migration/README.md` references a non-existent `curate_pocos.py`, and
  `curate_building.py`'s docstring claims second-pass tags show as UNHANDLED when they are mostly implemented.

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
  NOT a coverage oracle — the appendix is enumerable). Read maps: [pedia-map.md](../../reference/pedia-read-map.md) ·
  [python-read-map.md](../../reference/python-read-map.md).

## UnitCombat distillation

> The concept + the target model: [engine.md § UnitCombat](../../reference/engine.md). The MINIMUM that unblocks
> the stuck consumers is the cascade-QUERY surface, not a full re-taxonomy.

- **Emit `tags` from `curate_unitcombat.py`** — it emits families/skills/vision/outcomes/sizeMatters/identity and
  zero tags today, so the tech/equipment classes (`mounted`/`gunpowder`/`mechanized`) are unauthored. Greenfield:
  there is no legacy flag to migrate from, and a reasonable FIRST PASS is fine (a wrong tag is a quick data edit,
  not a perfection-gated blocker). Worklist: [unitcombat-tag-mapping.md](unitcombat-tag-mapping.md).
- **Fold combat-class tags into a unit's effective tag set** (unit-level ∪ primary ∪ subs ∪ promotion-granted).
- **Re-express the keyed "vs unit-combat-class" modifiers onto TAG predicates**, retiring `UNITCOMBAT_*` as a
  modifier target. ⚖ **POST-REWORK, its own dedicated pass (owner)** — until it runs, tags and unitcombats live
  side by side, which is sanctioned and costs nothing (the mapping is additive).
- **Reconcile the double flags** — `bSpy` lives on both the unit and the unit-combat; unify onto the `spy` tag,
  same for `outlaw`/criminal.
- **Purge the vestigial classes** — a majority are referenced by no unit. Opportunistic, never required, and
  bounded by the unreferenced-is-not-dead caveat (module XML holds assignments; a blunt purge over-reached once and
  was reverted). Candidates: [unitcombat-merge-candidates.md](unitcombat-merge-candidates.md).

## Triggers / grants

- **Start packages are DESIGN, not built** ([triggers.md](../../specs/triggers.md) § Game-start provisions): the
  entity type, its folder + prefix + repo row + manifest, and the shipped default packages. Two content decisions
  ride it — which units the defaults name, and NPC/barbarian starts (not authored in a grants block today).
- **Retiring the engine start selection** — the whole-database scan + AI scoring, and the per-role starting counts,
  retire once packages carry the identities. Until then they remain the live path.

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
