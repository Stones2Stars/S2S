# S2S #428 — JSON Data-Model Design Handoff

**Status: model LOCKED 2026-06-14 — the authoritative spec + the live RESUME POINT now live in
`docs/dev/plans/building-cascade-conversion.md`. Read its "⇒ CHECKPOINT & RESUME (2026-06-14)" section
FIRST (it has the current state, what's done, what's next, and the resume checklist), then "THE MODEL
(locked 2026-06-14)" below it.** This handoff is kept for history; where it differs, the locked model wins.
(The prior conversation contained an abandoned mass-migration detour — do not re-load it.)

**Checkpoint snapshot (2026-06-14):** branch `json-data-migration`, ~19 commits off main, tree clean.
**18 entities curated+written:** Tech/Bonus/Handicap/GameSpeed/Era/Process (gameplay) +
BonusClass/CivicOption/Hurry/Victory/PromotionLine/SpecialBuilding (POCO) + **the light batch, now COMPLETE:
Route/Project/Heritage/Religion/Specialist/Corporation** (each its own bespoke C++-verified curator). Next: the
heavy entities (Civic/Trait/Improvement/Terrain/UnitCombat/Promotion/Build/Property/LeaderHead → Building/Unit).
The plan's "⇒ CHECKPOINT & RESUME" + "LIGHT-BATCH DECISIONS" sections hold the authoritative state + the
cross-entity conventions established this batch.

**Resolved since this handoff was written (all in the locked model):** the two top-down cascades PLUS a
third **"sideways static influence" (leakage)** graph for map-adjacency/radius effects (orthogonal,
one-hop — absorbs the dropped `NEAR`); the **hot-path-is-top-down-only** invariant; the reverse index is
**DERIVED / cold-path / display-only, built once on load** (powers pedia + tri-state build list, never
the enablement compute); the **standardized modifier structure** (`modifiers.{scope}.{channel}.{unit}`,
additive `(base+Σflat)×(100+Σpercent)/100`, per-item multipliers); scopes `world→team→empire→area→city`
plus interchangeable sub-city targets (specialist/plot/building); **properties = containment-only** (NEAR
dropped, plot-properties verified inert → no-loss); the **3 `PrereqOrBuildings` techs removed**; and
**parity is NOT a goal** (the new model already exposed old bugs). Remaining opens are in the doc's §5.

---

## CORE PREMISE — the foundation of the whole model (ESTABLISHED; do NOT re-derive)
The data model **is** two cascades, both strictly **top-down**:
- **Cascading enablers** — enablement flows DOWN the entity chain (tech enables buildings, buildings enable units).
  An entity is available iff something above enables it. There are **no prerequisites**; nothing ever queries
  upward. *(If a future session "discovers" that no-prerequisites is the purest top-down form — that IS this
  premise, not a finding. That already happened once and is what poisoned the context.)*
- **Cascading modifiers** — modifiers sum DOWN the scope chain `game → team → empire → area → city`; a modifier at
  a higher scope applies to everything beneath it. (Say **cascading modifiers**, never "cascading bonuses" — a
  *bonus* is a resource entity; conflating the two mixes semantics.)

Everything else here (the skeleton, the one modifier shape, properties-as-base-accumulators, the XML-as-DB load)
is just *how we express these two cascades cleanly*. **Start from this premise; do not re-argue it.**

---

## The pivot (why we're restarting)
We built a **generic mass migration** — one engine transforming all 34 Info types at once
(`Tools/Migration/engine.py`). It worked mechanically but **mixed relation structures inconsistently and did
not adhere to top-down** (e.g. it left units carrying prerequisites). **Abandoned as the approach.** The engine
+ its generic output stay on disk as *scratch/reference only*.

**New approach (decided):**
- Treat the **XML as a real relational database** — each Info type is a table, each `*_TYPE` ref is a foreign key.
- Define + load **one Info at a time**, deliberately (the hand-built `handicaps` prototype is the quality bar).
- Each Info's JSON is a **top-down VIEW** built by *querying* the XML; downward edges via **reverse-lookup**
  (e.g. `tech.enables.buildings` = the buildings whose tech is this one — the building is never asked).
- **Readers adapt to the clean data, not the reverse** — the property engine included. C++ readers come LAST.

---

## Settled conventions (locked)
- **One modifier shape everywhere:** `modifiers.{scope}.{channel}.{unit}: value`.
  - scope ∈ `game / team / empire / area / city` (+ `plot` for properties). The empire-wide scope is **`empire`**
    (matches the handicaps prototype — NOT `player`).
  - unit ∈ `flat / percent / perPopulation / percentPerPopulation / perMilitaryUnit / perTurn / decay / enabler`.
  - **The unit comes from the VALUE element, not the outer tag** — `TechCommerceChanges` actually carries
    `<iCommercePercent>` → `percent`. Never trust the wrapper name.
  - value is a number OR a **formula tree**.
- **Formula trees:** `{op: [operands]}` — e.g. `{div: [ {mult: [ {attribute: "population"}, 7 ]}, -2 ]}`.
  Ops: `Mult`, `Div` (+ `Add/Subtract/Min/Max/Power/Modulo`). Operands: `{attribute: X}`, constants, nested ops.
- **Properties are BASE ACCUMULATORS — the same family as base yields & base commerces** (NOT a special section,
  not a special anything). A property (crime, education, disease…) is a passive integer that accumulates and that
  *other checks read* (thresholds, events) — it does not actively produce output, exactly like a base
  yield/commerce stockpile. So it's a modifier channel like any other: `modifiers.{scope}.{property}.{unit}: value`.
  Map: `GameObjectType`→scope, `PropertyType`→channel, `PropertySourceType`→unit (`CONSTANT`→`perTurn`,
  `DECAY`→`decay`), `iAmountPerTurn`→value (const or formula). The property engine reads these channels like any
  base accumulator (property names are a known set from `PropertyInfo`). Treat yields, commerces, and properties
  as one structural family of base-integer channels — only their semantics differ.
- **No `prerequisites` anywhere.** Pure top-down enablement: enablers carry `enables`; enabled entities are
  passive targets (no prereq, no enabledBy, no upward query). Game-option gates → **load-time prune** (a separate
  mechanism: static config resolved at load; rare manual changes via WorldBuilder/BUG just rebuild the data;
  frequent/automatic ones like flexible-difficulty handicap stay runtime — prune is size-proportional).
- **File layout:** 1 JSON per Info, **loose**, in **plural** per-entity folders (`buildings/`, `techs/`,
  `bonuses/`, `units/`…); buildings additionally sub-foldered by era (from the enabling tech's era).
  `Assets/Data/handicaps/` is the hand-built **reference prototype** — keep it.
- **Top-level section order:** `type` → hoisted text (`description`/`help`/`civilopedia`/`strategy`) →
  `modifiers` → entity-specific (`enables`, handicap's `aiDifficulty`, …) → `cost` → `flavors` (AI-targeting
  metadata — its OWN section, not identity) → `art` (good as-is, leave it) → `identity` (shrinking catch-all).

---

## The base Info skeleton (near-locked, prereq-free)
```jsonc
{
  "type": "BUILDING_FORGE",

  "description": "TXT_KEY_…",
  "help":        "TXT_KEY_…",
  "civilopedia": "TXT_KEY_…",
  "strategy":    "TXT_KEY_…",

  "modifiers": { "<scope>": { "<channel>": { "<unit>": <value> } } },   // properties live here too

  "cost":     { … },
  "flavors":  [ { "FLAVOR_X": n }, … ],
  "art":      { … },

  "identity": { … }
}
```
An enabler Info additionally carries (NO prereqs):
```jsonc
  "enables": { "buildings": [ "BUILDING_…" ], "units": [ "UNIT_…" ] }
```

---

## Open questions (resolve next session)
1. **Lock the skeleton** — keep `identity` as a named catch-all, or force every field into a real section?
2. **Multi-enabler semantics** — "needs tech AND bonus AND civic": each enables it; presence is all-of or any-of?
3. **`enables` structure** — grouped (`enables: {buildings:[], units:[]}`) vs flat (`enabledBuildings: []`).
4. **Property relation/scope** — emissions target plots too (312 city / 192 plot) with relations
   `SAME_PLOT` (307) / `ASSOCIATED` (124) / `NEAR` (58) / a few `TRADE`/`WORKING_*`. How does the relation fold
   into the modifier scope (scope+relation, or a qualifier)?
5. **First Info to define** — proposed **Tech** (top of the chain; exercises both `enables` and conditioner boosts).
6. **Property engine adaptation** — reader-side, later.
7. **Update `handicaps` prototype** `cascadeModifiers`→`modifiers` (it predates the rename; `empire` scope is fine).

---

## Artifacts on disk (what to reuse vs ignore)
- `Tools/Migration/extract_tags.py` → per-entity distinct-tag lists with samples (writes `tags/`, gitignored).
  **Reuse** — authoritative field inventory per Info.
- `Tools/Migration/mapping/*.json` → per-entity field classifications from the 34-agent workflow (committed).
  **Useful raw input** per Info, but treat as a first pass, not gospel.
- `Tools/Migration/engine.py` → the generic mass-migration engine. **Approach superseded**, but its field-handling
  (unit-from-value-element, formula trees, property-source cleaning, flavor collapse, plural folders, hoisting)
  is correct and reusable as functions.
- `Tools/Migration/map_workflow.js` → the classification workflow (chunked to dodge rate limits). Reference.
- `Assets/Data/*/` → generic database DRAFT (loose plural, **uncommitted scratch** — fine to delete/regen).
- `Assets/Data/handicaps/` → hand-built **prototype** = the format reference (committed).
- `docs/dev/plans/building-cascade-conversion.md` → design spec (committed; conventions, prune principle).
- `docs/dev/plans/cross-entity-inversion-blueprint.md` → cross-entity reference inventory (committed).

## Git state (branch `buildings-json`)
- **Committed:** mapping files, design docs, consolidated DB (`186c2ed5`), unit-fix (`bc873817`).
- **Uncommitted scratch:** latest `engine.py` refinements + loose plural DB + handicap-skip. No need to commit.

## First moves next session
1. Fresh context; read this doc only.
2. Lock the base skeleton (Q1).
3. Settle `modifiers` end-to-end — including the properties fold and the relation qualifier (Q4).
4. Stand up the queryable XML store (XML-as-DB: tables + FK/reverse-lookup indexes).
5. Define ONE Info top-down (Tech), fully, as the new template — then the next.
6. Once the model is locked, fold these rulings into `building-cascade-conversion.md` (permanent home);
   this handoff is temporary scaffolding.
