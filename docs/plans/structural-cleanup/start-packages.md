# Start packages — the game-start provisions as authored data

> **Design, not yet built.** The game-start sequence (what a player begins the game holding) becomes a set of
> JSON-authored **start packages** the grants machine applies, replacing the hardcoded engine selection. Owner
> intent: *"this gives modders a chance to set up how they want."*
>
> Companion: [grants-machine.md](grants-machine.md) (the machine that applies them) ·
> [grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md) §2 Game start (the legacy sites being replaced).

## Why

Today the game start is split between data and hardcoded engine logic, and the engine half is the problem:

- The **counts** are curated (`grants` on eras/handicaps): `startingUnitMultiplier`, `startingExploreUnits`,
  `startingDefenseUnits`, `startingWorkerUnits` (+ an `ai:` scoped variant), `startingGold`.
- The **unit identity** is not authored anywhere. `CvPlayer::addStartUnitAI` picks it at runtime by scanning the
  whole unit database, filtering on `canTrain`, and scoring with `AI_unitValue`.
- The **settler** is not in data at all — it is hardcoded (*"can't start a game without one"*), so there is no
  `startingSettlers` count to author.

A modder therefore cannot say what a start looks like; they can only nudge counts and hope the AI scorer picks the
unit they meant. The selection is also where a stubbed getter silently made every new game unplayable
(`isCivilizationUnit`), because nothing exercised that path until a real new game ran.

## The shape

A start package is an ordinary entity — **one JSON object per file**, same as every other type. It is essentially a
named `grants` block plus the condition that decides when it applies:

```jsonc
{
  "type": "STARTPACKAGE_ANCIENT_DEFAULT",
  "enabled": { "type": "ERA", "max": 1 },          // §3.9's ordinary entity-level gate
  "grants": {
    "units": [ { "unit": "UNIT_SETTLER", "count": 1 },
               { "unit": "UNIT_BRUTE",   "count": 2 } ],
    "startingGold": 40
  }
}
```

- **Reuses `grants` wholesale** — no parallel vocabulary. `units` / `techs` / `civics` / `buildings` / numeric
  pulses already mean the right things ([json.md §5](../../specs/json.md)), and the machine already applies them.

> **⚖ THE POINT: author the shared start ONCE, not per civilization (owner).** `grants` can already express a
> start — but putting it on the CIVILIZATION means repeating the same block, with the same conditionals, across
> ~50 civ files that all start identically. *"Setting up full conditionals in every civilization is kinda dumb,
> when it's the same package for all of them."* A package inverts that: **the condition lives ON THE PACKAGE,
> evaluated once**, and every civ it applies to gets it without authoring anything. A civilization only authors
> something when it **deviates** — a unique starting unit, an extra tech — and that deviation is its own package
> (or its own civ-gated one), stacking on top of the shared default rather than restating it.
> This is also why packages STACK rather than replace: the shared default + a civ's delta is the common case, and
> single-selection would force every deviating civ to re-author the whole start.
- **`startingSettlers` stops being hardcoded** — the settler is just a `units` entry.
- The per-role counts (`startingExploreUnits`/`DefenseUnits`/`WorkerUnits`) are superseded by explicit unit
  entries; the era/handicap grants that carry them retire as packages take over.

### ⛔ "Conditionally loaded" means the ENTITY GATE, never a load-time prune

The applicability condition is the **entity-level `enabled`/`disabled` pair, evaluated LIVE**
([DEC-entity-gate](../../architecture/decisions.md#dec-entity-gate), [json.md §2](../../specs/json.md)) — the same
gate every other entity uses for game options and state.

**Do NOT build a load-time prune / "load these files, skip those".** That is `loadPrune`, a curator-era invention
that was **killed whole** ([superseded-ideas](../../architecture/superseded-ideas.md) #3): it re-encoded validity
tags as a bespoke prune section, was named backwards, and the answer already existed as the ordinary condition
vocabulary. Every package loads; the gate decides which APPLY.

### Packages STACK (owner)

Applicable packages **sum**, exactly like any other grants deposit — they are not mutually exclusive alternatives.
This is the flexibility ruling: stacking **subsumes** single-selection, so the modder chooses the granularity.

- Want one coherent start? Author a single package holding everything.
- Want era + handicap + civilization to compose? Author three, each gated.

Single-selection could not express the second, and it is also what the engine does today (era + handicap counts add
up), so stacking is the behaviour-preserving choice as well as the flexible one.

## What a new entity type requires

Nothing bespoke — it follows the standing pattern, and skipping any of these is how an entity ends up half-wired:

1. **Folder** `Assets/Data/startpackages/`, one object per file.
2. **Infotype prefix** `STARTPACKAGE_` registered in [naming.md](../../specs/naming.md) — the prefix is how
   `readJson` routes a reference, never inferred from context.
3. **A row in `RJ_REPO_TYPES`** (`CvCascadeReadJson.cpp`) — the ONE per-type `InfoRepo` dispatch, which is what
   earns the entity its full-registry re-map (cross-category FK resolve), its DepositIndex push, and a
   `/state/info?type=…` home for the standing loaded≡authored check.
4. **An `_order.json` manifest** via `curate_order.py` (id order is manifest-driven).
5. **Authored through `_additions`** — entity curation is complete
   ([curators/README.md](../../specs/curators/README.md)), so packages are post-curation data, not curator output
   from legacy XML. There is no legacy XML to convert: the unit identities never existed as data.

## Open

- **Which units the shipped default packages name.** The counts exist; the identities must be chosen. Deriving
  them (unit whose default UNITAI matches the role and which is start-available) reproduces roughly what the AI
  scorer picks today and keeps the initial data non-arbitrary — but it is a content decision, not a mechanical one.
- **Retiring the engine selection.** `addStartUnitAI` (whole-database scan + `AI_unitValue` scoring) and the
  era/handicap per-role counts retire once packages carry the identities. Until then they remain the live path.
- **NPC / barbarian starts** — `barbarianInitialDefenders` is in scope per the §0 scope rule but is not authored in
  a `grants` block ([grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md) §5.4 curation gap); packages are its natural home.
