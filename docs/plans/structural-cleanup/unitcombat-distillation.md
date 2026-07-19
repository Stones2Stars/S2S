# Unit-combat distillation — slim the `UnitCombat` god-group into tags / skills / modifier-source

> **Status:** PLAN (owner-approved to plan, 2026-07). **Required before #430 completes** — owner realization
> 2026-07-19: the `UnitCombat` "general unit-group" must be distilled before the migration can finish, because it
> is the common blocker under three otherwise-stuck fronts:
> - the **keyed "vs unit-combat-class" combat modifiers** (F4 step 3 — [f4-unit-plane.md](f4-unit-plane.md) §3);
> - the **upkeep military/civilian bucketing** refinement (rides on a correct `military` tag —
>   [f4-unit-plane.md](f4-unit-plane.md) §5, the accepted-drift split);
> - the **`IS_MILITARY` / `IS_<tag>` predicate + tally surface** that ~a dozen cascade consumers need
>   (code-cut-map §Pass-2 rewire).
>
> This is the grounded plan, NOT implementation. All current-state numbers below are from live code/data
> (2026-07-19), not the stale engine.md figures — see §7.

## 1. The model (owner)

A `UnitCombat` is a **"general unit-group"**: a unit with **no strength / unitdata of its own — just pure modifiers**
applied to a group of member units. That modifier-source concept is legitimate and STAYS. What distills OUT of the
fat ~150-field class is its **classification** (→ `tags`) and its **ability** (→ `skills`); the vestigial majority is
**slimmed**. Three axes:

| axis | what it is | destination |
|---|---|---|
| **modifier-VALUE** | ~90 combat-stat scalars + 11 vs-keyed struct-vectors + the domain array (`CvUnitCombatInfo.h`) | STAYS a modifier SOURCE — the unit-group deposits its stats onto members ([modifier.md §6](../../specs/modifier.md)) |
| **classification** | tech/equipment class (`mounted`/`gunpowder`/`mechanized`) + role/size/species/motility taxonomy | **`tags`** ([tags.md](../../specs/tags.md), [json.md §8](../../specs/json.md)) |
| **ability** | the ~48-boolean ability battery | **`skills`** ([skills.md](../../specs/skills.md)) |

**⚖ The load-bearing distinction (owner 2026-07-19): TAGS are the core "what a unit IS"; UNITCOMBATS carry the "good/bad
AGAINST" column, keyed by tag.**
- A **tag** is a unit's IDENTITY — `mounted`, `gunpowder`, `military`, `armored`, … "what this unit is." Tags are the
  core classification system every consumer queries.
- A **UnitCombat** is a **modifier group** whose "vs" modifiers reference **tags** (via `{unit: IS_<tag>}` evaluated
  against the OPPONENT), never other unit-combat ids. The canonical example: **`anti-mounted` is a UnitCombat** (a
  modifier group carrying the bonuses vs the `mounted` tag); **`mounted` is a Tag** (the identity of the unit it
  fights). So `strength.unit.percent {unit: IS_MOUNTED}` authored ON the anti-mounted UnitCombat — NOT
  `strength.unit.unitCombat.{UNITCOMBAT_MOUNTED}`; the `UnitCombat` id stops being a modifier *target* entirely.

This sharpens the slim (§E): the combat-classes that today encode **identity** (mounted/gunpowder + the size/species/
motility taxonomy — the bulk of the 480 vestigial) distill **INTO tags**; the ones that encode a genuine **vs-tag
modifier group** (anti-mounted, …) stay as lean UnitCombats whose "vs" column now references tags. The whole reason
the tags exist is to be that authoritative classification the cascade queries (upkeep pool, military count, the
`{unit: IS_<tag>}` "vs" modifiers).

## 1a. Scope (owner-ruled 2026-07-19) — MINIMUM to unblock + a welcomed purge

**We care only about the MINIMUM that unblocks the stuck cascade consumers**, plus an opportunistic purge. Concretely:

- **IN scope (the minimum):** the cascade-QUERY surface — the **`IS_<TAG>` predicate** (§3.D) + the **per-tag tally**
  (§3.D) — and the **`IS_MILITARY` consumer rewires** (§4): military happiness (`happiness.empire.cities.{unit:
  IS_MILITARY}`), military count (`getNumMilitaryUnits` + readers), military production, and the related gates. This
  needs **NO new tag mapping** — the `military` tag already exists (`curate_unit.py` from `bMilitarySupport`;
  `isMilitarySupport()` = `tags.has("military")`); the minimum is the QUERY surface reading it + the consumer rewires.
- **WELCOMED (opportunistic):** **purge the superfluous unit-combats** (§3.E — the 480/59% referenced by no unit),
  carefully respecting the "unreferenced ≠ dead" attribute-match caveat (engine.md; the 2026-06-14 blunt purge
  over-reached and was reverted). A conservative purge of the genuinely-dead is a welcomed cleanup, not required.
- **TAIL / OUT of the minimum:** the greenfield `mounted`/`gunpowder`/`mechanized` mapping table (§8.1 — NOT needed
  for the minimum, which is `military`-only), the keyed "vs unit-combat-class" modifier re-expression (§3.C / F4
  step 3 — stays parked), and the deep three-axis distillation. These land later, not to complete #430.

So the gating "author the mapping table" decision (§8.1) is **deferred** — the minimum path does not touch it.

## 2. Current state — what exists vs what's missing (grounded)

| axis / piece | state |
|---|---|
| **modifier-VALUE (poco)** | mapped: `CvUnitCombatInfo.h` carries all ~90 scalars + 11 vs-keyed vectors (`VS_KEYED`, `curate_unitcombat.py:53-65`) + domain array. The scalar SOURCE gather is LANDED for the F4 scalar/combat/upkeep planes. |
| **modifier-VALUE (keyed consumer)** | **UNBUILT** — the keyed `changeExtra{UnitCombat,Domain,Flanking}Modifier` accumulators have no cascade fold (code-cut-map §34/§group 11, `@SAVEBREAK`). Distilling tags does NOT by itself build these; they also need the self-accumulator + the predicate re-expression (§3.C). |
| **classification → `tags` (unitcombat)** | **NOT emitted.** `curate_unitcombat.py` emits families/skills/vision/outcomes/sizeMatters/identity — **zero tags** (verified, `curate():240-259`). `mounted`/`gunpowder`/`mechanized` deferred here (`curate_unit.py:117-122`). |
| **classification → `tags` (unit-level)** | PARTLY DONE: `curate_unit.py:664-678` emits role tags (`worker`/`settler`/`missionary`/`merchant`/`spy`/`civilian`) from `DefaultUnitAI`, `military` from `bMilitarySupport`, `outlaw` from the criminal combat class. 1464/2074 unit files carry a `tags` block. |
| **ability → `skills`** | PARTLY DONE: `curate_unitcombat.py` emits `skills` from the CAP_* tables (71/815 files); ~48 skill getters in the poco. Runtime feed-rewires (processUnitCombat → source `skills.X`) mostly unbuilt (code-cut-map §consumer-read). |
| **tag STORAGE + getter** | DONE: `ClassificationRegistry` mints `TAG_*` infos; readJson loads unit `tags` into a bool-block bitset (`CvUnitInfo.h:509/519/537`); `isMilitarySupport()`/`isSpy()` already `return getTags()->has(...)` (`CvUnitInfo.h:472-474/224`). |
| **`IS_<TAG>` predicate** | **MISSING** — `CvCascadeConditionEval.cpp` (~:302-363) has only plot/city/player game-state predicates; **no** `IS_MILITARY`/`IS_MOUNTED`/any tag-membership predicate. |
| **per-tag TALLY** | **MISSING** — `CvCascadeTally.h` counts only by entity-type id (`buildingCount`/`unitCount`); no count-by-tag. |
| **slimming** | **NOT started** — 480/815 (59%) unit-combats are referenced by no unit; the size/quality/group ranks still flow whole to `identity`/`sizeMatters`. |

**The central gap is the cascade-QUERY surface (predicate + tally).** The tag *storage/getter* half is built; the
half that lets the cascade ASK "is this unit military/mounted?" and "how many military units?" does not exist, and
that is what blocks every downstream consumer.

## 3. The work, by axis

### A. Classification → `tags` (the core new data pass)
1. **`curate_unitcombat.py` emits `tags`.** Add a `tags` block to its output. Two sub-cases:
   - **Tech/equipment class — `mounted`/`gunpowder`/`mechanized`/… (GREENFIELD, no legacy boolean).** There is NO
     legacy flag to migrate from ([tags.md](../../specs/tags.md) §Tech/equipment — PERMANENT carve-out); the
     unit-combat → tag mapping is authored, not derived. **⚖ It does NOT need to be perfect (owner 2026-07-19):** a
     reasonable FIRST-PASS is fine — the tags are curator-emitted data, so a wrong/missing tag is a quick edit after
     the fact ([tags.md](../../specs/tags.md): "a glossary needn't be complete to begin; many units untagged, fixed
     in validation"). So this is a first-pass authoring, NOT a perfection-gated blocker. (And it is TAIL anyway — the
     minimum path (§1a) is `military`-only and never touches it.)
   - **The size/species/motility taxonomy** currently crammed into the combat-role enum (the `*Base` ranks →
     `identity`/`sizeMatters` today): decide per-family whether each becomes a `tag` (accounting membership) or stays
     `sizeMatters` data. Most are SizeMatters ranks (stay), a minority are genuine type-membership (→ tags).
2. **A unit's effective tags = unit-level `tags` ∪ its combat classes' `tags`** (primary + subs + promotion-granted
   `skills.unitCombats`, the [skills.md §3b](../../specs/skills.md) membership rule). The tag-derivation must fold the
   combat-class tags in — this is why `curate_unit.py` defers mounted/gunpowder to "THEN" (`:117-122`).
3. **Reconcile the double flags:** `bSpy` lives on BOTH `CvUnitInfo` and `CvUnitCombatInfo` (code-cut-map rows 13/19)
   — unify onto the `spy` tag together. Same discipline for `outlaw`/criminal.

### B. Ability → `skills` (finish + wire)
- The curator emit is mostly there (CAP_* → `skills`); finish any gaps. The runtime CONSUMER rewires (every engine
  read of a legacy unitcombat ability flag → the composite `skills` getter folding unit + promotion + combat-class,
  [skills.md §3b](../../specs/skills.md)) are the Gate-3 consumption sweep — largely unbuilt, tracked in code-cut-map.

### C. Modifier-source — the keyed "vs" re-expression (couples with the unit self-accumulator)
- **A "vs unit-combat-class" modifier becomes `strength.unit.percent {unit: IS_<tag>}`** (evaluated against the
  opponent), NOT `strength.unit.unitCombat.{X}`. The curator re-expresses the `VS_KEYED` `unitCombat`/`domain`
  entries onto the `{unit: IS_<tag>}` / `{unit: IS_<domain>}` predicate form.
- ⚠ This ALSO needs the keyed self-accumulator consumer (the `maxCombatStr` opponent-set fold, code-cut-map §34,
  unbuilt) AND the predicate surface (§D). So it is downstream of BOTH this distillation AND the F4 keyed build — it
  does not land with the tag emit alone. (F4 step 3 stays blocked until §A+§D land; then the keyed consumer can build
  against the predicate form.)

### D. The cascade-QUERY machinery (net-new — the load-bearing unblock)
1. **`IS_<TAG>` predicates in `cascadeEvalCondition`** — a `CASC_PRED_*` per queryable tag (or a generic
   tag-membership predicate parameterized by the tag id) that reads the target unit's tag bitset
   (`ec.unit->getTags()->has(tagId)`). This is what makes `{unit: IS_MILITARY}` / `{unit: IS_MOUNTED}` evaluate.
   Predicates are an extensible registry ([json.md §3.5](../../specs/json.md)) — add them, don't invent a member.
2. **A per-tag TALLY** — "how many `IS_<tag>` units at scope" ([tally.md](../../specs/tally.md)). The tally is
   read-not-store: a tag count iterates the owner's units testing the tag bitset (or a maintained per-tag count on the
   object if a hot consumer needs O(1) — decide per the tally's read-not-store rule). This replaces
   `getNumMilitaryUnits` + the ~12 readers, and feeds `per:{unit: IS_MILITARY}` scalers (military happiness).

### E. Slimming the vestigial classes  — a DIRECT MEMORY WIN (owner 2026-07-19)
- **Motivation is memory, not tidiness:** every `CvUnitCombatInfo` (all ~150 fields) is loaded resident **whether any
  unit references it or not**, so the 480 vestigial classes are pure wasted memory. Under the 32-bit ~3.2 GB
  address-space ceiling (the roadmap's `bad_alloc`-near-the-ceiling pressure), purging them is a direct reduction —
  this is why the owner welcomes it even though it is outside the minimum-to-unblock.
- **The working method (owner 2026-07-19): map the OBVIOUS, FLAG the unsure/not-mapped — don't force completeness.**
  Both the tag mapping (§3.A) and the purge run this way: classify/purge the clear cases, and leave anything
  ambiguous FLAGGED (kept, tagged "unsure") for a later editable pass. This is what makes it swingable relatively
  easily — no exhaustive up-front analysis.
- Re-derive the vestigial set from LIVE data (480/815 unreferenced by any unit primary/sub; ADD promotion-granted
  `skills.unitCombats` membership before finalizing — §7 risk 1). For each vestigial class decide: **delete**
  (obviously dead — Categories, DCM, traps already dropped), or **fold** its taxonomy into a tag/`sizeMatters` and
  drop the class; **flag** the unsure and leave it.
- **Also purge DUPLICATES (owner 2026-07-19):** among 815 fat classes there are near-certainly functionally-identical
  redundant unit-combats — detect classes whose field payload is identical (or trivially so), collapse each duplicate
  set to one, and re-point the referencing units/subs to the survivor. Same memory win; same obvious-only + flag-unsure
  discipline (only collapse where the payloads are genuinely identical; flag near-duplicates for review). ⚠ engine.md's "unreferenced ≠ dead" caveat (attribute-matched
  classes selected by era/religion/culture, never XML-named) is exactly WHY the unsure ones get flagged not deleted —
  do not blunt-purge (the 2026-06-14 blunt purge over-reached and was reverted).

## 4. What it unblocks (the payoff)

- **F4 step 3** — the keyed "vs class"/"vs domain" combat modifiers can finally build (as `{unit: IS_<tag>}`
  predicate deposits, once §A+§D land + the keyed self-accumulator).
- **Upkeep bucketing** — the `military` tag becomes the authoritative, correctly-derived split (refining the
  accepted-drift bucketing of [f4-unit-plane.md](f4-unit-plane.md) §5).
- **Military happiness/anger** — `happiness.empire.cities.{unit: IS_MILITARY}` gets a real predicate+tally
  (currently NONE FOUND wired).
- **Military count/cap, military production, spy/outlaw/missionary/worker gates** — all rewire onto the tag surface.

## 5. Sequencing (proposed)

1. **Author the unit-combat → tag mapping table** (the greenfield decision, §8) — the gate for everything else.
2. **Build the cascade-QUERY machinery** (§D: the `IS_<TAG>` predicate + the per-tag tally) — net-new, and the
   single unblock the most consumers wait on. Verifiable in isolation (a `{unit: IS_MILITARY}` count on the endpoints).
3. **`curate_unitcombat.py` emits tags** (§A) + fold combat-class tags into the unit tag derivation; regen.
4. **Rewire the IS_MILITARY consumers** onto the predicate/tally (upkeep pool, military count, happiness, …) —
   verify live, expecting the deliberate 1007/1325/1276→`military` unification divergence (skills.md §3).
5. **Finish `skills`** emit + the ability consumer rewires (§B).
6. **Re-express the keyed "vs" modifiers** to the predicate form + build the keyed self-accumulator consumer (§C /
   F4 step 3).
7. **Slim** the vestigial classes (§E) — last, once nothing references the taxonomy it carried.

## 6. Acceptance / verification

Per [validation.md](../../specs/validation.md): each step live-verified via the endpoints. Key checks: a
`{unit: IS_MILITARY}` count on `/computed/tally` (or a new tag-count field) matches the real military unit set; the
upkeep military/civilian split is now tag-derived and stable; the military-happiness deposit manifests; a "vs mounted"
combat modifier evaluates against a mounted opponent. The military-flag unification is a KNOWN, attributed divergence.

## 7. Current-state numbers (live, superseding stale figures)

- **815** unit-combat JSON files; **335** referenced by some unit (primary `combatClass` ∪ `combatClasses`); **480
  (59%) unreferenced** by any unit — vestigial. (engine.md's "~981 / ~77%" is STALE — re-derive; and add
  promotion-granted `skills.unitCombats` membership before finalizing the vestigial set.)
- **63** distinct classes appear as a PRIMARY combat class; heavy long-tail (HERO 406 units, ANIMAL 234, SUBDUED 194,
  MOUNTED 146, MELEE 143, GUN 75, …).
- **71/815** unit-combats carry a `skills` block; **814/815** carry an `identity` block (the size/quality/group ranks
  — the taxonomy). **1464/2074** units carry a `tags` block.
- `CvUnitCombatInfo`: ~90 modifier scalars + 11 vs-keyed vectors + domain array; ~48 ability bools.

## 8. Open decisions (owner)

1. **The unit-combat → tag mapping** (`mounted`/`gunpowder`/`mechanized`/… and which taxonomy classes become tags):
   greenfield, hand-authored, no legacy source. Who authors it and against what taxonomy? — the gating decision.
2. **The per-tag tally shape:** pure iterate-on-read (read-not-store) vs a maintained per-tag count on the object for
   hot consumers (military count is read ~a dozen places). Decide per [tally.md](../../specs/tally.md).
3. **Slimming aggressiveness:** delete the 480 unreferenced classes, or fold-then-delete, respecting the
   "unreferenced ≠ dead" attribute-match caveat (engine.md).
4. **Scope fence for #430:** is the FULL distillation (all three axes + slim) required to *complete* #430, or the
   minimum that unblocks the stuck consumers (tags + predicate + tally + the IS_MILITARY rewires), with the deep slim
   + the keyed-vs re-expression as a tail? The owner's "required before completion" ruling needs this line drawn.

## See also
- [f4-unit-plane.md](f4-unit-plane.md) — the unit modifier plane; §3 (keyed, blocked on this) + §4 (the scope
  boundary) + §5 (upkeep, whose bucketing this refines).
- [tags.md](../../specs/tags.md) · [skills.md](../../specs/skills.md) · [json.md §8](../../specs/json.md) — the
  classification model. [modifier.md §6](../../specs/modifier.md) — the unit self-accumulator (modifier-source axis).
- [code-cut-map.md](code-cut-map.md) — the cut inventory (§group 11/18/34, §51, §Pass-2 rewire 1963-2017).
