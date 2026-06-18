# Enabler / Disabler — formal spec (DRAFT v0.3, 2026-06-15)

**Status: BASELINE — owner-accepted 2026-06-15.** This is the **uniform surface** we build #430 on: settled enough
to implement against, with future hole-poking expected but the foundation fixed. Formalizes the *availability* half
of the #428 cascade. Supersedes v0.1 (which mis-modelled the enabler as a modifier-style push-cascade) and v0.2
(which over-loaded `requires`).

- **v0.2's correction (owner, 2026-06-14):** enablers are a TWO-STEP process; modifiers are one-step. We had
  been treating them the same — that was the bug.
- **v0.3's correction (owner, 2026-06-15) — ONE `requires` gate + the BUILD/OPERATE (dormancy) insight.** Two
  refinements landed this session, in order:
  1. *(explored, then folded)* A sticky/fluid analysis suggested splitting the gate into a structural part and a
     fluid part. **Withdrawn:** there is **ONE `requires` object**, a positive-only BoolExpr (`all`/`any` + `min`/`max`
     counts) over the means. The fragmentation we were modelling
     (`enabledWhen` vs `requires`, the sticky/fluid divide) is just an artifact of the legacy prereq zoo —
     PrereqTech / PrereqAndBonus / PrereqVicinityBonus / PrereqCivic / Obsolete* / the disabled-building chain,
     decades of ad-hoc authoring. Collapsing it into one uniform gate is the point.
  2. *(kept — the real contribution)* **`requires` is what it takes to BUILD *and* to keep OPERATING.** It is
     re-evaluated at build time (greying) **and continuously on already-built things** (dormancy): lose a required
     civic / resource / religion and the building goes **dormant — disabled, not demolished** — and wakes again
     when the requirement returns. Civic and religion are *fluid* (revolution swaps a civic; inquisition removes a
     religion), so this dormancy is general, not resource-only.
  - **Why it matters:** the replacement for the ~42 scattered `canBuild`/`canConstruct`/`canTrain` checks is, BY
    DESIGN, applied to a far smaller list — the generated candidate frontier — never "every building/unit in the
    game" at any moment. The small-list property is the whole point, not an optimization bolted on (§2, §4, §14).
  - **Sticky vs fluid is NOT model structure** — it survives only as an optional recompute-*cadence* hint (tech
    rarely changes; resources/civics flip often), which §2 treats as a separate optimization layer.

Synthesis of the owner design sessions 2026-06-14/15 + the enabler rulings in `building-cascade-conversion.md` +
`store.PREREQ_FIELDS`/`OBSOLETE_FIELDS` + the `formalize-enabler-vocabulary` workflow (verb/scope/composition,
with the verify-pass data-shape corrections). Formalize this BEFORE the heavy entities; then retrofit the ~21
already-converted infos.

---

## 0. Scope — model the ACTION, not the OUTCOME

The cascade answers one question: **"can I take this action now?"** — build a unit/building/wonder, lay an
improvement (via a build), research a tech, adopt a civic, found a religion/corporation. It decides what is
**OFFERED**.

It does NOT model what the action *produces*. A build yields either an **independent OUTCOME** — a **unit** (off
to the map) or an **improvement** (on a tile), each with its own lifecycle that **exits this model** — or
persistent **CITY STATE**, a **building** (becomes part of the city; stays in the model). Everything about a
produced instance's fate (a unit surviving, a building demolished, a wonder keeping tourism after its effect
obsoletes, `IMPROVEMENT_COTTAGE → HAMLET` upgrading) is the **outcome/instance-lifecycle system — a deferred
neighbor, explicitly out of scope.**

So the cascade's working state is the **STATE** entities — buildings + techs + civics + religions + bonuses…
(the things you *have* that gate further actions). **Units & improvements are leaf actions**; the cascade only
decides whether to offer them.

---

## 1. THE headline — enablers are 2-step (GENERATE then GATE); modifiers are 1-step

- **A modifier is a LOCAL deposit.** Each source drops its effect on the target; the target sums. Order-free,
  commutative, **one pass**. You never need the whole picture — every deposit is independent.
- **Availability is GENERATE-then-GATE — TWO passes.** It cannot fold-as-you-go: the candidate set must be
  *generated* before any per-candidate gate can run.

Trying to run the enabler like the modifier (one-pass deposit) is what produced all the push-cascade machinery
(deposit-on-event, "disable before enable", per-turn reconciliation, chain-walking). None of it is needed.

**`enables` and `requires` COEXIST — two aspects that both apply, not two homes for one edge (owner, 2026-06-15).**
A target (a building, say) is positioned in the permanent possibility space by its `enables` relationships AND
reversibly gated by its `requires` conditions — *both at once*. **The classifier is the NATURE of the action, not
the atom kind:**

- **`enables` — PERMANENT / irreversible actions** (forward, on the source): **hard-constructive** (research a
  tech, build a prereq → permanently makes things possible) and **destructive** (obsoletion / replacement →
  permanently makes things impossible). This is the GENERATOR: the union of `enables` over HAS, **minus** the
  destroyed, is the candidate frontier CAN GET — bounded by HAS, read **FORWARD** (no reverse lookup). **Tech is
  authored HERE, never in `requires`** — putting it on the target would force a tech→targets reverse index (the
  reverse-lookup clusterfuck).
- **`requires` — POSITIVE reversible MEANS** (forward, on the target): civic adopted / resource connected / religion
  present → buildable & operating; revolted / cut / inquisited → greyed & dormant. A positive-only BoolExpr
  (`all`/`any` + `min`/`max` counts) over the means. Gates the generated candidates AND re-checks built things for
  dormancy (§3). Checked forward (set-membership against HAS). The *negative* side (bans, removal) is NOT in
  `requires` — it is the source-side `disables`/`obsoletes`/`replaces` tracks (§5/§6). `enabledWhen` and the
  sticky/fluid divide were legacy artifacts — collapsed away.

So: **destructive or hard-constructive → `enables`; reversible enable/disable → `requires`** — and the two coexist
on the same entity.

The two passes: **(1) GENERATE candidates via `enables` minus destroyed (bounded by HAS); (2) GATE each by its
reversible `requires`.** `available(X) = X ∈ generated ∧ X.requires ⊆ HAS`. Step 1 bounds the set to what you could
reach, so step 2's per-candidate gate runs over a **small list**, never the whole database — the whole point
(§4, §14). Both reads are FORWARD — `enables` forward from the source, `requires` forward from the target — so the
hot path never inverts; reverse indices are cold-path (pedia) only.

---

## 2. The three terms — HAS, CAN GET, HAS THE MEANS TO (owner framing, 2026-06-15)

The whole model reduces to three sets, computed in order each recompute:

1. **HAS** — what you currently possess: tech, civics, religion, connected resources/bonuses, built buildings.
   Gathered in pass 1.
2. **CAN GET** — the candidate frontier: everything HAS unlocks via `enables`. "Everything we can have." Generated
   in pass 2a — cheap, bounded by HAS (the union over-produces; the gate confirms).
3. **HAS THE MEANS TO** — of the CAN-GET candidates, the ones whose `requires` is satisfied right now. Verified in
   pass 2b against the one `requires` gate.

The build list falls straight out: **CAN GET but `requires` unmet → greyed/hidden** (per the failing clause's
disposition, §4); **CAN GET ∧ `requires` met → buildable**; not in CAN GET → hidden. And the same `requires`
check, re-run on things you already HAVE, is the **operational/dormancy ledger** — a built Forge that loses its
iron, or whose required civic is revolted away, fails `requires` and goes dormant (§3).

### The two passes that produce them

**PASS 1 — gather HAS.** Traverse **right-then-down** (§9) and collect everything you currently possess, at each
scope (§8). Output: the scoped HAS sets.

**PASS 2 — generate (CAN GET), then verify (HAS THE MEANS TO):**

- **GENERATE — CAN GET (cheap, from HAS):** for each thing you HAVE, look up what it unlocks via `enables` → union
  = the candidate set. This is the **derived `enables`/reverse index**'s hot-path job. **Bounded by HAS** — you
  never scan all ~3869 buildings, only what HAS could reach. The union *over-produces* (it proposes X from one of
  X's prereqs even if X needs others) — a candidate net, not a gate.
- **VERIFY — HAS THE MEANS TO (per candidate, the gate):** each candidate checks its own **`requires ⊆ HAS`** — the
  positive-only BoolExpr of means: AND (`all`) + OR (`any`) over civic/religion/resource + `min`/`max` counts. (No
  negatives here: bans are the source-side `disables` track and permanent removal is `obsoletes`/`replaces`, both
  subtracted during generation, §5/§6.) Candidates that pass are buildable; the rest grey or hide (§4).

Why both, why neither alone: **generation** keeps the work bounded by HAS (cheap union); **`requires`** is the
authoritative gate (the AND/OR/NOT and the negatives the union can't express). Neither alone suffices — the union
over-produces, and the gate alone would have to run over the whole DB.

**Cost model — this IS the optimization, not overhead.** Both steps are bounded by HAS, never the ~5000-entity
database. Generation considers only candidates reachable from HAS; the `requires` check runs only on that frontier.
So total cost is proportional to *what you could plausibly get*, not *everything that exists* — strictly cheaper
than re-checking every building/unit/tech each recompute (the old O(all-entities) PreLoop). This is *why* the
`canBuild`/`canConstruct`/`canTrain` replacement works on a far smaller list by design (§14).

**Sticky vs fluid → an optional CADENCE hint only (not model structure).** Atoms differ in how often they change:
tech is effectively monotonic (rarely recompute CAN GET), while resources/civics/religion flip often (re-run
`requires`). If ever measured worthwhile, a cache could regenerate CAN GET only on tech/building change and re-gate
on resource/civic change — but that is a wrapper layer (cache-agnostic, below), **never** a split in the data model.
The model has one `requires`.

**On-the-fly recompute is affordable — no caching needed (owner).** Today's engine already recomputes
availability over the *full* entity set constantly and has tolerated it since time immemorial; the bounded
two-pass is strictly *less* work. So recompute `have`/`canget` on demand — no aggressive caching, no incremental
deposits, no clever invalidation. Even a slightly-too-eager recompute beats the status quo. (This is why the §2
cadence can be "recompute on `have`-change" without engineering a cache around it.)

**The model is CACHE-AGNOSTIC (owner) — keep the decoupling.** The have/frontier computation is a **pure function
`have → frontier`**. If a cache + delta/incremental updates ever prove warranted, that is a **separate
optimization LAYER wrapped around** the pure function — it must never leak into the model. The model defines
*what* the frontier is; caching is only an orthogonal *how-fast*, added if measured and removed without touching
the model (the same stance the doc takes on the derived-data repository: "an optional, measured optimization added
after the engine, not foundational"). So the spec models *only* the pure two-pass; deltaing is out of its scope.

`have` (pass 1) and the frontier (pass 2) are the two lists. **Most of `have` flips turn-over-turn** (resources/
bonuses, civics on revolution, religion on spread/inquisition, buildings) — only **tech** is effectively
monotonic. But `have` is a cheap list to recompile, so this is a cadence note, not a model split (§2).

**Recompute cadence — the frontier is `f(have)`; recompute on `have`-CHANGE (commonly once/turn).** `have` usually
changes only at the turn boundary, so the DOMINANT cadence is once per turn, held for the turn — a single-item or
queued city reads that frontier, and a queue is a plan *over* it. **BUT same-turn `have`-changes do occur and MUST
trigger a mid-turn recompute** (owner — hole in the earlier "frozen per turn" claim): the **AI** processes
production mid-turn, so it can finish building A and then, the SAME turn, build B that depended on A (A's
completion lands in `have` immediately for the AI's next decision); a **human** can hit the same via the
**tamed-animal mission builds** edge case. So the rule is **recompute when `have` changes**, not "once per turn".
This stays cheap: it's a RECOMPUTE of the bounded two-pass over the *affected scope* (one city's `have`), not an
incremental deposit, and it fires only when `have` *actually* changes (rare mid-turn). Per-turn is the common
case, never an invariant. (The exact set of mid-turn `have`-change triggers should be enumerated — see §13.)

---

## 3. `requires` — "do I have the MEANS"; distinguishes one-time BUILD vs continuous OPERATE

**`requires` checks one thing — do I have the MEANS — and it must DISTINGUISH two timings (owner, 2026-06-15),
grounded in today's two building checkpoints:**

- **`requires.build`** — the **one-time** construction requirement (today's `canBuild` resources). Must hold to
  build; if missing the candidate is **GREYED** (with the resource named, §4). **NOT re-checked after** — once
  built, losing it does nothing.
- **`requires.operate`** — the **continuous** prerequisite (today's `PrereqBonuses`; `CvCity::isActiveBuilding`,
  CvCity.cpp:14364). Must hold to build AND to keep running; re-checked every recompute on a built thing — failing
  → **DORMANT** (disabled, not demolished; wakes when it returns, §11).

**Applicability — `requires.operate` is for PERSISTENT STATE; units use `requires.build` today (owner,
2026-06-15).** Per §0, units are *leaf actions* that exit the model once built, so a unit carries `requires.build`
now; `requires.operate` (dormancy) applies to persistent city state, i.e. buildings. Three DISTINCT unit behaviors
fall out cleanly, and the split is exactly why retiring `disables` (for `obsoletes` + `requires`) matters:

- **`obsoletes` a unit** → removed from CAN GET (no NEW builds), but **existing units STAY ON THE MAP** — obsoletion
  touches buildability only, never the instance. (Instance destruction is the deferred outcome system, §0/§11; the
  design choice is keep-on-map.)
- **`requires.build` unmet** → can't build now (greyed); existing units unaffected.
- **`requires.operate` on a unit (FUTURE — e.g. tanks need fuel)** → would **disable an existing unit going
  forward** (dormant/inactive when the input is missing) while it **stays on the map** — a reversible turn-off, the
  return-trip `disables` could never express. The structure already supports it (just a `requires.operate` on the
  unit); not modelled now.

Each part is a positive BoolExpr (`all` + `any` OR-groups) over the means kinds — **civic, religion,
resource/bonus**. Authored on the target, scope-tagged per clause:

```jsonc
"requires": {
  "build":   { "all": [ {"bonus":"BONUS_STONE","scope":"city","connection":"trade|vicinity"} ] },   // canBuild: greyed if missing, not re-checked
  "operate": { "all": [ {"civic":"CIVIC_GUILDS","scope":"empire"},
                        {"bonus":"BONUS_IRON","scope":"city","connection":"trade|vicinity"} ],       // PrereqBonuses: greyed at build + dormancy
               "any": [ [ {"bonus":"BONUS_COAL"}, {"bonus":"BONUS_OIL"} ] ] }
}
```

- **Build-time gate (greying) = `build` ∧ `operate`** — you need both present to construct; either missing → greyed
  with that resource named. **Operate-time gate (dormancy) = `operate` only** — a built thing re-checks just the
  continuous part. That is the whole distinction the object carries.
- **TECH drives GENERATION via `enables`, never as a `requires` GENERATION driver — but CAN appear in `requires` for
  the per-candidate CONFIRM (owner, 2026-06-15).** Tech is a permanent CONSTRUCTIVE action → `enables`, read forward.
  Relying on a `requires`-tech to *generate* would force a tech→targets **reverse index** (the reverse-lookup
  clusterfuck) — so generation is always `enables`-driven, and `canBuild`'s structural unlock (tech, prereq
  buildings, obsolete/replace) feeds the `enables` side. **BUT** once a candidate is in the frontier, a tech MAY
  appear in its `requires.build.all`/`requires.build.any` as a **confirm** — the same `requires.build` every entity
  uses, no tech special-casing. The tech-tree multi-parent case (§13.8): the child is proposed by `enables` from one
  parent, and `requires.build.all: [TECH_1, TECH_2]` confirms it's actually researchable — a forward check on the
  already-generated candidate's own small parent-list, **no reverse index**. Rule: never let `requires`-tech *drive*
  generation (always have the `enables` edge); it only confirms.
- **`requires` is MOSTLY positive, but admits a NEGATIVE for reversible DORMANCY (owner, 2026-06-15) — don't
  exclude it.** The common case is positive means (`all`/`any` presence + `min`/`max` counts). But a **negative
  clause is allowed**: `disableIfAny` / `disableIfAll` — "go DORMANT while a forbidder/condition is present" —
  **reversible and non-destructive** (the **pseudobuilding** case: education / crime / tourism thresholds switching
  on and off; condition met → dormant, clears → reactivates). **Authoring container = `noneOf`** (the locked memory's
  name; `noneOf:[A,B]` ≡ `disableIfAny:[A,B]` = "requires NONE of A,B present"), parallel to the positive `all`/`any`.
  - **⚠ SUPERSEDED (owner 2026-06-17) — the one-time RACE/uniqueness gate is now the `allowed` CAP (§5a), NOT a
    `requires.build` SELF-negative.** This originally modelled global-uniqueness as
    `requires.build.noneOf:[{type:SELF,scope:world}]` (Tech `bGlobal`, the 29 religion-founding techs; world wonders;
    unique units). That spelling forced "cap 1" to be written `max:0` and conflated *needed* with *allowed* — withdrawn.
    `bGlobal` = `allowed:{world:1}` now (the religion-founding-once race; `CvPlayer::canEverResearch` bars it once
    `countKnownTechNumTeams>0`, the engine enforcing it by reading the tally). **The `requires`-negative survives only
    for reversible DORMANCY** (operate-time, pseudobuildings — crime/education/tourism bands); it no longer carries the
    build-time uniqueness race. `SELF` is gone from `requires` entirely — it lives only in `per` count-scalers.
  - **This is DISTINCT from the source-side `disables` ban**, which is **DESTRUCTIVE** (destroys instances, §5/§6).
    The split is by **FATE**: a `requires`-negative → **DORMANT (kept, reverses)**; a `disables` → **DESTROYED**. And
    by **author**: `requires`-negative is on the TARGET ("I go dormant while X"); `disables` is on the SOURCE/law
    ("I destroy X").
  - Permanent removal (obsoletion / replacement) is also source-side (`obsoletes`/`replaces`), never a `requires`
    clause. So `requires` covers the REVERSIBLE space (positive means + dormancy negatives); the `enables` family
    covers the PERMANENT space (construct / destroy / supersede).
  - **Pseudobuildings = the in-city representation of PROPERTY EFFECTS (owner, 2026-06-15).** Primarily (only?) used
    to show the effect of a city's **properties** (`PROPERTY_CRIME`/`EDUCATION`/`DISEASE`/tourism — the `CvProperties`
    system) at a threshold band. So their real home ties to the **property system (#429)** — a property-effect /
    threshold-band entity, not a fake building. Behaviourally they're just **cascade participants** (carry `requires`
    incl. the dormancy negative + `enables`/etc., same requirement management — no behavioural difference from a
    building). Formalizing them as their own class is a **UX/presentation question**, a separate easily-solvable issue.
    **Name = `PropertyEffect` (owner, 2026-06-15)** — parallels the existing `BuildingEffect` convention. **GROUNDED:
    today's pseudobuildings ARE the `BUILDING_EFFECT_*` family** (e.g. `BUILDING_EFFECT_HERITAGE_BIG_CATS`) —
    effect-markers hacked in as real buildings; `PropertyEffect` formalizes them OUT of the building roster into their
    own class (much of `BuildingEffect` gets repurposed this way). NB: bare `effect` was rejected — `CvEffectInfo` is
    EXE-bound (`DllExport getPath()` imported by the engine for the `.nif`), so it can't be renamed/repurposed.
  - **`BaseEffect` hierarchy — a SEPARATE, LATER issue; do NOT touch during the migration (owner, 2026-06-15).** The
    `BUILDING_EFFECT_*` pseudobuildings **work fine as-is for now** — the cascade just treats them as buildings, so
    leave them alone. The future improvement is ORGANIZATIONAL: `PropertyEffect`, `BuildingEffect`, and the other
    effect kinds each become their OWN Info deriving from a shared **`BaseEffect`** base class. A sibling refactor,
    tracked apart from the enabler cascade (#428/#430), not blocking — noted here only because `PropertyEffect` is one
    of its members.
- **Surface rule (owner):** the modder-facing authoring surface = exactly what the engine implements; the exotic
  remainder (mixed-polarity OR, OR-of-negatives) stays OFF the surface — documented as possible future, never as
  existing.
- **A clean per-case LEVER: grey (`requires`) or hide (`enables`) — and it is a UI question, not operational
  (owner, 2026-06-15).** Both placements work identically in the engine; the only difference is what the player
  sees, so authoring the resource on one side or the other is a **clean, per-case display choice**:
  - **`requires`** → candidate **GREYED** with the resource named (player can craft/trade for it — an active
    decision).
  - **`enables`** → candidate **HIDDEN** until the resource is present.
  The uniform structure gives this lever for free — go either way depending on the case. **General lean: grey on
  resources (→ `requires`)**, because surfacing "what to acquire" is usually what we want; flip a specific resource
  to `enables` when hiding is the better UX for that case. (§4.)
- **Positive only, checked FORWARD** — does HAS contain this atom? (set membership; no reverse lookup). Atoms carry
  `connection` (trade/vicinity) and a scope (§8).
- **Scope resolution → `requires` reads TALLY (owner, 2026-06-15; folds in §7/§8).** A **city**-scope means
  checks the local city HAS; an **empire/team/world**-scope means — presence (`∈ HAS` = `count ≥ 1`) AND count
  thresholds (`≥ N of X`, e.g. the old `PrereqNumOfBuildings`) — resolves against the **tally module**, the
  additive roll-up of per-city "X is had" reports. So verify never crosses city isolation: it reads the local HAS
  for city clauses and tally for higher scopes, uniformly. This is the same module wanted anyway for
  demographics/AI/score (§8).
- **Leaf atom** = `{<kind>: "TYPE", scope, connection?, disposition}` where `<kind>` ∈
  `civic | religion | bonus | …` (never `tech`). The `all`/`any` nodes nest to form the BoolExpr.
- **OBJECT-EVALUATED PREDICATES (owner, 2026-06-15).** Some conditions need runtime object state the static Info
  cannot hold (is this city the holy city? the capital? is this the player's state religion?). The **object
  itself (city/player/plot) answers it at evaluation** — "the city does not have the info in the data, it says itself
  whether it is." The engine's predicate owns any compound logic (e.g. `STATE_RELIGION` encapsulates the C++
  relaxation "present AND (is-state-religion OR no-state-religion OR non-state-commerce)"). Same shape inside
  `requires` and the modifier `enabled`/`disabled`.
  - **AUTHORING SHORTHAND (owner 2026-06-16): a parameter-free / unambiguously-scoped predicate is a BARE STRING;**
    a parameterized one stays the object `{PREDICATE: parameter}`. The discriminator is whether anything must be
    *named* for the predicate to be unambiguous (consistent with the "atoms are explicit, never infer from context"
    rule — bare is allowed only when there is genuinely nothing to infer).
    - **PARSING CONTRACT (owner 2026-06-16): a bare predicate DESUGARS deterministically to `{PREDICATE: true}`** in
      one fixed translation step (`HAS_RIVER` ≡ `{HAS_RIVER: true}`, `IS_CAPITAL` ≡ `{IS_CAPITAL: true}`) — pure
      authoring sugar over the canonical object, **never a separate shape the engine handles specially.** So a
      predicate is **bare-able *iff* its canonical form is `{PREDICATE: true}`** (exactly the parameter-free case);
      `HOLY_CITY` cannot be bare because its canonical object is `{HOLY_CITY: RELIGION_X}`, not `{HOLY_CITY: true}` —
      there is nothing to translate it to.
    - **bare** (nothing to name): **`enabled: HAS_RIVER`** (the river is on *the plot* the terrain/feature occupies),
      **`enabled: IS_CAPITAL`** (a city is or isn't the capital).
    - **object** (a parameter is load-bearing): **`{HOLY_CITY: RELIGION_X}`** (holy city *of which religion?*),
      `{HAS_RELIGION: RELIGION_X}`, `{STATE_RELIGION: RELIGION_X}`, `{HAS_CORPORATION: CORPORATION_X}`.
    - **negation = the `disabled` twin**, not a `false` value: **`disabled: HAS_RIVER`** ("suppress where there is a
      river") is the clean negative; `enabled: {HAS_RIVER: false}` remains a valid (verbose) equivalent.
    - No ambiguity with data Types: a bare string in `enabled`/`disabled` is a predicate catch-all token; a resource
      condition is ALWAYS the full tally atom `{type, scope, min}`, never a bare `BONUS_X`.
  - Vocabulary so far (extensible, engine-resolved like the § catch-all tokens — not info special-cases):
  - **`HAS_RELIGION: RELIGION_X`** — the city has religion X present (`isHasReligion`).
  - **`STATE_RELIGION: RELIGION_X`** — X is the player's (effective) state religion. *(Religion `StateReligionCommerces`.)*
  - **`HOLY_CITY: RELIGION_X`** — this city is X's holy city (`isHolyCity`). *(Religion `HolyCityCommerces`.)* Parameterized → object.
  - **`IS_CAPITAL`** (bare; parameter-free) — this city is the capital. *(Many capital-specific bonuses in Trait + Civic; the
    committed civic `capital` member retrofits to this later.)*
  - **`HAS_CORPORATION: CORPORATION_X`** — this city has corporation X active (spreads like a religion,
    `isHasCorporation`). *(Corporation per-city effects — the Religion-parallel model.)*
  - **`HAS_RIVER`** (bare; parameter-free) — this **PLOT** has a river (edge-attribute, `getRiverCrossingCount() > 0` /
    `CvPlot::isRiver`; the PLOT self-reports, extending the predicate's object set beyond city/player to the plot).
    A river is **independent of any feature** — it exists on a plot with or without one. River is "just added on"
    — NOT its own feature/terrain entity — so each river-driven yield is a **CONDITIONAL modifier gated by this
    predicate**, owned by its **deliveryguy** (owner 2026-06-16, modifier-spec §6.1). The homes, from
    `CvPlot::calculateYield` (CvPlot.cpp:8060-8092):
    - **the river's OWN BASE yield** (the +1 commerce any river plot gets, feature or not; `YieldInfo.iRiverChange`,
      line 8090) → **the TERRAIN, as a `HAS_RIVER`-conditional deposit** (owner 2026-06-16): every river plot has a
      terrain (the always-present plot owner), so the base river bonus lands on each river-capable LAND terrain as
      `commerce.plot.flat[].{value:1, enabled:"HAS_RIVER"}` — NOT kept on the abstract `YieldInfo`. Done in the
      Terrain #20 re-curation (renames §Terrain). The **hills/peak** base yields (`getHillsChange`/`getPeakChange`)
      likewise move onto `TERRAIN_HILL`/`TERRAIN_PEAK` (those ARE their own terrains); `YieldInfo`'s plot-type yields
      retire, its remaining fields a later small curation.
    - **a feature's EXTRA river-side yield** (forest-on-river) → **`FeatureInfo.RiverYieldChange`** (line 8085), applied
      only when a feature AND a river coincide → feature-owned, gated by `HAS_RIVER` (Feature #21).
    - **`ImprovementInfo.RiverSideYieldChange`**, **`BuildingInfo.RiverPlotYieldChanges`** → improvement-/building-owned,
      authored at their passes. There is no river field on `CvTerrainInfo`.
  - **`HAS_FEATURE: FEATURE_X` / `HAS_TERRAIN: TERRAIN_X` (parameterized) + `IS_COASTAL` (bare) — the BoolExpr-converter
    predicates (owner 2026-06-16, the BoolExpr/settler follow-up).** The shared `BoolExpr → enabler-condition` converter
    (`Tools/Migration/boolexpr.py`) translates the XML `BoolExpr` machinery (`And`/`Or`/`Not`/`Has[GOMType,ID]`/`Is[TAG]`
    - integer-compare; `Sources/BoolExpr.{h,cpp}`) into this `requires` vocabulary — used to retrofit the building
    `ConstructCondition`, the building `NewCityFree`, and the unit `TrainCondition` (all parked, now parsed). The GOM/TAG
    map: `GOM_TECH`→`{type:TECH,scope:team}`, `GOM_BONUS`→`{type:BONUS,scope:city,connection:"trade|vicinity"}`,
    `GOM_BUILDING`→`{type:BUILDING,scope:city}`, **`GOM_FEATURE`→`{HAS_FEATURE:FEATURE_X}`**, **`GOM_TERRAIN`→
    `{HAS_TERRAIN:TERRAIN_X}`** (parameterized predicates, uniform with `HAS_BONUS`/`HAS_CORPORATION`), **`Is
    TAG_COASTAL`→`IS_COASTAL`** (a bare city-is-coastal predicate, `CvCity::isCoastal`), `And`→`all`, `Or`→`any`,
    `GreaterEqual(ATTRIBUTE_POPULATION,N)`→`{type:POPULATION,scope:city,min:N}`. The converter RAISES on any node/GOM/tag
    outside this set (so a future module addition is caught, never silently mis-converted — owner: "if parsing is too
    cumbersome we hand-recreate with grants by hand"). ✅ **MEMBERSHIP RECONCILE — RESOLVED 2026-06-16 (owner, hole #1):**
    `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_BONUS` are the **canonical** single-valued predicates (uniform w/ `HAS_RELIGION`/
    `HAS_CORPORATION`); Improvement #22's `{terrain|feature|bonus:[…]}` is the compact **membership SUGAR** desugaring to
    `any`-of-the-predicate (no data churn — the lists stay). `COASTAL_LAND` is UNUSED in real data (0) → moot; `IS_COASTAL`
    (`CvCity::isCoastal`, city-coastal) stays, distinct from any plot predicate. Bare `HAS_FEATURE` ("has ANY feature")
    coexists with the list ("one of THESE"). Cleared 229 conformance flags. (data-model-spec §2.5.)
- **PREDICATES ARE ORGANIZED BY SYSTEM — each is a system's ISOLATED QUERY-SURFACE (owner, 2026-06-16).** A system
  exposes its runtime state through its own predicate(s), which any data references to gate on that system *without
  coupling to its internals*: `HAS_RIVER` (river), `HAS_RELIGION`/`STATE_RELIGION`/`HOLY_CITY` (religion),
  `HAS_CORPORATION` (corporation), `IS_CAPITAL` (capital). This is the conditioning counterpart of the dedicated-block
  rule (modifier-spec §0.8): system *data* clusters in its block; a system's *state* is queried only through its
  predicate surface — together they keep systems modular with isolated surfaces. Two rules keep that surface robust:
  - **Each SYSTEM documents its OWN predicates** — the predicate is part of that system's surface and responsibility,
    not a central monolithic registry. Adding a system adds *and documents* its predicate(s); the vocabulary grows
    per-system, owned where the system lives.
  - **A missing / unknown predicate is IGNORED — DROPPED from the evaluation, NOT substituted with `false` (owner,
    2026-06-16). `inactive == ignored, NOT false`.** Substituting `false` is wrong in two ways: (1) a deposit
    `enabled` by a now-missing predicate would evaluate `enabled=false` and be spuriously turned **OFF** — "the
    disable would kick in" — even though nothing meant to disable it; (2) a `false` would corrupt `all`/`any`
    compounds. Instead the dangling predicate atom is **removed from the boolean expression** and the deposit is
    evaluated on its remaining (known) conditions. So retiring a system (and its predicate object) never forces
    unrelated data off and never errors — the reference simply goes quiet. This graceful "ignore the unknown" is what
    makes a whole system safely *removable* (e.g. Global Warming) without hunting down every reference — the
    isolated-surface guarantee in practice. *(Exact compound-logic semantics for a dropped atom — incl. a deposit
    whose ONLY condition went missing — pin at #430.)*
  - **PLANNED ALIGNMENT PASS (owner, 2026-06-16) — after data migration, BEFORE #430 parsing.** Once every info is
    curated, a dedicated pass brings *today's* predicates into line with this modularity rule: treat **Religion,
    Corporations, and Traits (both the simple and the complex/Thunderbrd versions)** as **isolated systems**, and edit
    their infos so each system's predicate surface is self-contained, self-documented, and gracefully-ignorable. Done
    *before* the engine parser exists, so #430 implements against already-aligned data (no retrofit churn mid-parse).
    This is the predicate-side companion to the §12 retrofit. **This list is the first batch, NOT exhaustive — ANY
    concept we define as a system gets the same treatment (owner, 2026-06-16):** dedicated data block, its own
    predicate surface, gracefully ignorable. (Religion / Corporation / Trait / vision / global-warming / each
    `PROPERTY_*` are instances of the one universal rule, modifier-spec §0.8.)

---

## 4. Greying — falls out of the gate (per-clause disposition), for free

The build-list tri-state is a byproduct of the same generate-then-gate pass — no separate "why greyed" computation.
The gate is per-clause, not pass/fail; each clause carries a **disposition** (`greyable` or `hiding`):

- **not in CAN GET** (generation didn't reach it) → **HIDDEN**.
- **in CAN GET ∧ all `requires` clauses met → LISTED** (buildable).
- **in CAN GET ∧ only `greyable` clauses unmet** (a missing connectable resource, an unadopted civic) → **GREYED**
  ("needs `BONUS_COPPER`" / "needs `CIVIC_GUILDS`").
- **in CAN GET ∧ a `hiding` clause unmet** (wrong era) → **HIDDEN**. (Permanent removal — `obsoletes`/`disables`/
  `replaces` — drops it from CAN GET entirely, so that's the "not in CAN GET" case above, not a greying clause.)

Disposition is a property of the clause kind, set once: resource/bonus/civic that the player can go acquire →
`greyable` ("you could get this"); era/obsolete/structural-impossible → `hiding`. So the same `requires` evaluation
that gates buildability also yields *why* it's greyed/hidden — the tri-state the migration doc wanted, emitted by
the gate itself.

**The frontier is a SHARED choice set — UI *and* AI.** Computed once per recompute, read by both: the UI greys
from it (above), and the **AI production decision** (`doProduction` / `AI_bestCityBuild`) iterates **only the known
frontier** — what the city can actually build now — instead of filtering and scoring the whole ~5000-entity
database every decision. The cascade has already answered "can I build this"; the AI only does the "should I"
scoring, over a small set. That makes `doProduction` (a documented turn-time hotspot) dramatically cheaper —
arguably the biggest systemic payoff of the two-pass model, beyond UI greying.

**ONE trigger, not 42 (owner).** Today availability is recomputed ad-hoc at ~42 discrete call sites
(`canConstruct`/`canTrain`/prereq checks scattered across CvCity/CvPlayer/CvUnit/CvGameTextMgr/AI), each with its
own slightly-divergent logic — a correctness *and* maintenance hazard. The cascade triggers the recompute in **one
place**; everyone **reads the shared frontier**. So: one source of truth (the 42 can no longer silently disagree),
one place to change the rule, compute-once-read-many instead of 42 redundant scans. This is the consolidation
win — and it's why "recompute on the fly" is fine: the *single* recompute replaces dozens of scattered ones.

---

## 5. Base relationship objects — the `enables` family (PERMANENT) + `requires` (REVERSIBLE)

**Authored schema (owner): any item can carry these base relationship sections — `enables`, `obsoletes`,
`replaces`, `requires` (and more to come).** They sort by the **nature of the action** (§1), into two cascades that
COEXIST on the same entity:

- **The `enables` family — FOUR source-side base objects, forward on the source, drives GENERATION
  (owner, 2026-06-15):**
  - **`enables`** — **constructive**: what this entity unlocks (`enables.{buildings,units,builds,techs,…}`). Tech
    lives here. Read forward over HAS → ADDED to the candidate union.
  - **`disables`** — **destructive, ACTIVE removal (a ban)**: what this entity FORBIDS-and-DESTROYS. Ban the death
    penalty → the electric chair is *removed* and its space *repurposed*; ban prostitution → the brothels are torn
    down. Removes the target from CAN GET while the disabler is in HAS **and destroys existing instances**. **NOT
    dormancy** — repeal later and you must **REBUILD** (pay the cost); nothing mothballed silently resumes.
  - **`obsoletes`** — **destructive, PASSIVE supersession**: a tech/era makes the target outdated. Removes it from
    CAN GET, but **existing instances PERSIST** (obsolete units linger on the map). *Could technically be a
    `disables`, but kept SEPARATE for modder-clear semantics* — progress-supersedes vs policy-forbids (owner).
  - **`replaces`** — **destructive, SUCCESSION**: a successor takes the predecessor's slot (predecessor gone once any
    successor is present; transitive chain, §6).
  - So **CAN GET = union(`enables` over HAS) − (`disables` ∪ `obsoletes` ∪ `replaces` over HAS)** — generation is the
    whole source-side family, read forward, bounded by HAS. No reverse lookup.
- **`requires` — the TARGET-side gate: "do I have the MEANS" (§3).** Positive means (civic/religion/resource via
  `all`/`any`) + `min`/`max` counts of OTHER types; checked forward per candidate against HAS/tally.
- **`allowed` — the declarative CAP: "how many may EXIST" (owner 2026-06-17; §5a below).** The ceiling, distinct from the
  `requires` "needed" means. Authored with the REAL number; the gate permits a build while `count(X) < allowed`.

**§5a — `allowed` (the instance / category CAP).** "Allowed" names the ceiling unambiguously, fixing the off-by-one the old
SELF-`requires`-atom forced (cap 1 had to be written `max:0`). Two shapes, discriminated by key namespace:

- **Self-cap** `allowed:{<scope>:N}` (scope key `world`/`team`/`empire`) — "at most N of ME at scope." For a building the cap
  scope IS its wonder-category marker (`isWorldWonder == getMaxGlobalInstances()!=-1`, CvGameCoreUtils.cpp:340-369): world→
  worldWonder, team→teamWonder, empire→nationalWonder. A unique unit → `allowed:{empire:1}`. (Today's `iMaxGlobalInstances`/
  `iMaxTeamInstances`/`iMaxPlayerInstances` + the SpecialBuilding group cap + tech `bGlobal` all fold to this single idiom;
  `bGlobal` = `allowed:{world:1}`, the religion-founding-once race, replacing the old `requires.build.noneOf:[SELF@world]`.)
- **Category count-cap** `allowed:{<wonderCategory>:N}` (`worldWonders`/`teamWonders`/`nationalWonders`, + reserved
  `totalWonders`) — a **per-city** cap on how many of a category a city may hold, authored on **CultureLevel** (it grants the
  city its allowance; city scope implicit). Replaces CultureLevel's old `identity.maxWorldWonders…`.
- **Enforcement is ENGINE, reading the TALLY (§7):** build permitted while `tally.count(SELF|category, scope) < allowed`.
  Absent ⇒ uncapped. The engine OWNS the dynamic parts — ignoring the cap under `NO_WONDER_LIMIT`/`NO_NATIONAL_UNIT_LIMIT`/
  `CHALLENGE_ONE_CITY`, era-scaling the base, `+extra` bumps — none of which touch the parser (§0.6 boundary, and the
  rebuild-out-to-the-canDoStuff-gates ruling). **OCC carries no separate limits** (it just forces wonder limits off); any
  future game-option-specific override is a *game-option-specific JSON* the engine loads on option-enable (the
  override-by-design mechanism, generalizing the `replacedBy`/CvInfoReplacements swap).

**`disables` is a DESTRUCTIVE ban — it tears the thing down (owner, 2026-06-15).** A ban does not park a building
dormant — it **destroys** it. Ban the death penalty → the electric chair is removed and its space repurposed; ban
prostitution → the brothels are demolished. **Repeal later and you must REBUILD** (pay the cost) — nothing mothballed
silently resumes. That destruction (not a pause) is exactly why `disables` is its OWN source-side track. **Reversible
DORMANCY still exists — but it is a different mechanism: the `requires`-negative** (`disableIfAny`/`disableIfAll`,
target-side, §3), used for pseudobuildings (education/crime/tourism) that switch off/on as a condition holds. So:
**ban = `disables` = destroyed; condition-dormancy = `requires`-negative = parked-and-resumes.**

**INSTANCE FATE — how a target loses availability decides what happens to existing instances (owner, 2026-06-15).
TABLE OWNER-ACCEPTED / LOCKED 2026-06-15 (final yes given — the destroyed-vs-dormant split below is settled):**

| trigger | new builds | existing instances |
|---|---|---|
| **`disables`** (ban — death-penalty / prostitution) | forbidden while the disabler is in HAS | **DESTROYED** — removed, space repurposed; repeal ⇒ **REBUILD** from scratch, no mothball-resume |
| **`requires`-negative met** (`disableIfAny`/`disableIfAll` — pseudobuilding: crime/education/tourism) | greyed | **DORMANT** — parked, reactivates when the condition clears (reversible, non-destructive) |
| **`obsoletes`** (tech supersedes) | forbidden | **TARGET DECIDES** — see below (defunct/destroyed, or lose-bonus-keep-as-attraction) |
| **`replaces`** (successor) | forbidden | replaced by the successor |
| **positive `requires` fails** (lose iron / revolt away a required civic) | greyed | **DORMANT** — building persists, output gated, resumes if the means returns |

So **disabling is destructive; means-loss is dormancy** — different triggers, different fate.

**The CHARACTER of the two, crisply (owner, 2026-06-15):**

- **`disables` = a hard "be gone, spawn of darkness".** The source COMMANDS destruction; the target gets no say —
  it is removed, full stop (the ban tears it down).
- **`obsoletes` = a soft "meh, we don't need you anymore, do whatever".** Just a SIGNAL; then it's **up to the
  obsoleted entity to REACT.**

**So OBSOLETION FATE is a TARGET-side decision, NOT in the chain-down.** The `obsoletes` edge on the source only
says "X is now obsolete." **WHAT happens to X's instances is authored ON X** — a `whenObsolete` property on the
target: some buildings go **defunct/destroyed**, while **wonders and walls especially LOSE their specific bonus but
REMAIN** (a tourist/culture attraction). Obsoletion fate is per-target, declared by the target, and lives in the
deferred outcome/instance-lifecycle system (§11) — the obsoleting tech never dictates it. (`disables`, by contrast,
dictates: destroyed.)

**Precedence — `replaces` overrides the obsolete-stay choice (owner, 2026-06-15).** If a target is BOTH obsoleted
AND replaced, `replaces` wins: the successor has taken its slot, so the predecessor is **genuinely gone even if its
`whenObsolete` wanted it to stay** (a replaced thing cannot linger as an attraction — the successor is standing
where it was). That case is already covered by the `replaces` track; `whenObsolete` only governs when nothing
replaced the target.

**Authoring + scope.** A ban is authored as `disables` on the LAW/doctrine (one law lists what it forbids), is
**EMPIRE/TEAM-scope and standing** (applies player-wide, flips only on adopt/repeal), and is processed in generation
(subtract from CAN GET + destroy instances) — NOT a per-city per-turn condition, NOT a `requires` gate. Bans act on
something **otherwise available** — default-available (prostitution/alcohol/drugs, never enabled) or
explicitly-enabled-then-bannable (slavery/cannibalism); the enable is orthogonal (net = available AND not-disabled).

**PEDIA — why keeping `obsoletes` ≠ `disables` pays off (owner, 2026-06-15).** It keeps the useful target-side
**"Obsoleted by [tech]"** line (when will this lapse), while we **need NO "potentially disabled by [law]" line** — a
ban is a player policy choice listed on the LAW's side ("this doctrine bans X, Y"), not a defensive worry-line on
every bannable building.

**WHY THIS MATTERS — a correct home for traditionally-SHOEHORNED concepts.** Empire-wide moral/legal bans (slavery,
cannibalism, prostitution, death penalty) never had a principled seat — hacked in wherever. They are now just
**`disables` from an empire-scope source**. A direct validation of the rework; a lens for the audit (watch for other
shoehorned concepts that now have a home).

**THE EMPIRE/TEAM-WIDE PROJECTS/BUILDINGS tier (owner, 2026-06-15) — a long-latent concept this exploration finally
DEFINED.** The general idea: *constructables that live at empire/team scope instead of city scope* — "buildings"
and "projects" one tier up. They fit the existing enabler model with **NO new machinery**: the only requirement is
that buildings/projects can exist above city scope (the scope spine §8 already has team/empire). The tier carries
the **FULL enabler-model edge set** at empire scope — it is NOT just one flavor:

- **Constructive STAGE-GATES (the `enables` edge):** an empire/team project that, once built, unlocks a major new
  capability — **milestone/progression gates.** The prime class is the **space line**: you cannot settle the moon
  until the empire-level **moon-base project** exists; you cannot reach Mars until the **mission-to-mars project**
  is built; and onward (each milestone project in team-HAS → `enables` the next capability/region). This is the
  #421 *Lunar Landing → Lunar Base → unlock region* stage-gate (retiring the dedicated Cislunar map for a gated
  multimap region; for when space is nailed down). The same shape serves any "you can't do X until you've built
  the empire-wide Y" gate.
- **BANS (the `disables` edge) — the "doctrine" flavor:** ban-list + modifiers, below.
- **Empire-wide MODIFIERS:** the **#421 team-buildings** (effects via a shared pool).

**This REPLACES today's autobuild clunk (owner, 2026-06-15).** Currently an empire-wide effect is faked by building
*one* building that **autobuilds itself into every city** (the `FreeBuilding` / per-city autobuild mechanism, ~345
uses). The tier replaces that with **ONE building at empire/team scope** — no N per-city copies, no autobuild fan-out
— exactly the #421 "retire per-city autobuilds" win. The empire effect lives where it conceptually is (the empire),
not smeared across every city.

So **"doctrine" is just the ban flavor, not the tier name** (and not load-bearing — see the naming note above; the
*tier* name is TBD). A **ban-flavored** team/player "building" is shaped like this:

- It is **persistent HAS state**, enacted (`requires.build`) and removable/repealable — like building then
  demolishing a building, one tier up.
- It authors its **`disables` list** (the `buildings`/`units`/`builds` it forbids) — concretely "a list of banned
  buildings and actions". While the doctrine is in HAS, generation subtracts those targets from CAN GET **and
  destroys their instances** (§5/§6, destructive). Repeal removes the doctrine from HAS → those targets are
  buildable again, but destroyed instances must be REBUILT (no mothball-resume).
- It carries **`modifiers`** for its empire-wide effects (the "some modifiers attached").
- It does NOT carry the *allow* — banned things are default-available (prostitution/alcohol/drugs) or enabled by a
  separate source (slavery/cannibalism); the doctrine is the ban-list + modifiers only.

So "new entity or civic?" resolves to **neither — a higher-scope building.** The building entity + the existing
scope model (§8, team/empire) already express it; we only need buildings to exist above city scope. (Dovetails with
the **#421 team-buildings** concept — empire-wide "buildings" with team-level effects. Name TBC: doctrine / edict /
institution / team-building.)

**INTERIM vs END-STATE — keep the existing machinery; the tier is its OWN later issue (owner, 2026-06-15).** The
empire/team `disables`-building above is the END-STATE. **For now, bans/repeals are implemented like effects** — a
**pseudobuilding/autobuild with a disable/enable** (the existing per-city machinery) — and **promoted UP the
hierarchy later**. Crucially, **the whole team/player-level "buildings" TIER is its own ISSUE** (related to the
effects / `BaseEffect` issue, §3), and **the existing machinery can be made to work with the new cascade WITHOUT
refactor — so we KEEP it.** The migration does NOT block on building the tier; bans/doctrines + the tier sit in the
same "works now, organize-and-promote later" bucket as `PropertyEffect`/`BaseEffect`. (Owner likes the interim
less, but it's the pragmatic path.)

(The lone *converted* `disables` — a per-civ research ban — STAYS a `disables` (owner 2026-06-15): it is modeled
as a REVERSIBLE ban. There is no current in-game logic to reverse a tech-disable — nor any general logic to apply
one: it is ONE hardcoded case today, the BARBARIAN/Neanderthal civ (`TECH_SEDENTARY_LIFESTYLE`). But nothing
stops us adding reversal if we want, so the model treats it as the reversible `disables` (data leads, engine
catches up). The earlier "PERMANENT, re-home it to `obsoletes`/`loadPrune`/`policies`" framing was WRONG. Scope is
just a parameter — the empire-scope player-law ban (destructive, repeal⇒rebuild) and this per-civ/barb tech ban
are the same uniform `disables`, differing only in scope.)

**Each EDGE is authored ONCE, on its natural end** — `enables`/`obsoletes`/`replaces` on the source (the actor),
`requires` on the target (the thing needing means). **The reverse view of each is derived at load and KEPT, but is
COLD-PATH only** — it powers the pedia / web-Civilopedia ("Unlocks", "Obsoletes", "Required by"), and it is **never
read on the hot path**: generation reads `enables`/`obsoletes`/`replaces` forward; the gate reads `requires` forward.
This is the answer to the reverse-lookup concern (owner, 2026-06-15) — inversion happens once at load for the pedia,
never per-recompute.

**No single "`requires` ⇄ `enables`" mirror.** They are *different relationships* — `enables` = a permanent unlock
(tech→building), `requires` = a reversible means (building→resource) — not two ends of one edge. (An interim draft
that paired them, and an earlier one that put tech/obsoletion in `requires`, are both withdrawn; §1.) An item
commonly carries several of these objects at once: a building `enables` the units it unlocks, may `obsoletes`/
`replaces` a predecessor, and `requires` the resources/civics it needs to operate.

---

## 6. Source-side base objects (`obsoletes`, `replaces`) + auxiliary sections

`obsoletes` and `replaces` are two of the three **DESTRUCTIVE members of the `enables` family** (§5 — `disables`, the
reversible BAN, is the third) — permanent actions, forward on the source, **processed during generation** (all three
SUBTRACT from CAN GET). They are **not** `requires` clauses (`requires` is positive means only, §3). `disables` (the
destructive ban — destroys instances, repeal ⇒ rebuild) is detailed in §5, not here; do NOT confuse it with the
reversible NON-destructive turn-off, which is a `requires`-negative going true (dormancy, §3). The rest below are
**auxiliary** (provisioning / config / identity — not availability edges). Shapes verified on disk:

- **`obsoletes` (base object; the old `Obsolete*`)** — the ANTI-ENABLE, per-type sections mirroring `enables`:
  `obsoletes:{buildings:[],units:[],builds:[],bonuses:[],techs:[],…}` (330 files already use this shape, e.g.
  `tech.obsoletes.units` = the old `ObsoleteTech`). **NB: obsoleting an INSTANCE-bearing target (a unit) removes it
  from CAN GET only — existing instances stay on the map** (the instance lifecycle is the deferred outcome system,
  §0/§11; design choice = keep-on-map). The per-civ research ban (the lone converted `disables`) STAYS a
  `disables` — a reversible ban, NOT re-homed (owner 2026-06-15; see §5). It is ONE hardcoded case today: the
  **Neanderthal barbarians** (`CIVILIZATION_NPC_NEANDERTHAL`) can't research `TECH_SEDENTARY_LIFESTYLE`.
  Source-authored; read forward in generation to remove the named target from CAN GET while active.
- **`replaces` (base object; self-framing)** — per-type successor sections mirroring `enables`/`obsoletes`:
  `replaces:{buildings:[BUILDING_X],…}` ("I take X's slot"; conditional form carries `onGameOption`, load-stable) —
  destructive (it destroys the predecessor), same reason as `obsoletes`. Source-authored; the **transitive successor
  chain** (`A→B→C` ⇒ A removed once any successor is present) is precomputed once at load and applied in generation.
  **Unit `upgradesTo` is NOT `replaces`** — it is constructive/MANUAL (an *available action*; the old unit isn't
  auto-removed), so it lives under `succession` (below).
- **`grants`** *(auxiliary)* — one-shot event provision (game start, wonder completion); does not sum/propagate.
  `grants:{buildings:[],civics:[],techs:[],units:[],specialists:[],freePromotions:{PROMOTION_X:[UNITCOMBAT_Y]}}`
  (`freePromotions` keyed by PROMOTION → list of UnitCombats). Sources: Civilization, Trait, Project, Religion,
  SpecialBuilding, Heritage.
  - **`grants` also carries one-time NUMERIC PULSES, not only entity lists (owner, 2026-06-15: "grants feels like
    the natural place to put those pulses").** A non-entity one-shot effect fired on an event is a grant too — the
    first instance is the Civic `iRevIdxSwitchTo` → `grants.revolution` (a signed revolution-index burst applied on
    *switching to* that civic). This is the same home the **outcomes** system will use for one-time yields (e.g. a
    merchant's trade-mission granting a one-time gold/yield burst). So `grants` is the general one-shot home:
    entity provisions (the lists above) AND numeric pulses (`grants.<channel>: value`), both fired by the engine
    event-hook system (§ modifier-spec §7), never summed/propagated.
- **`loadPrune`** *(auxiliary)* — load-stable game-option gate (§10). `loadPrune:{onGameOptions:[],notOnGameOptions:[]}`
  (241 files). If false at load, the entity/channel is **not materialized** — never enters `have` or the candidate
  set. Sibling of `obsoletes` (load-time data removal vs live query).
- **`policies`** *(auxiliary)* — civ meta-capability (non-cascade structural).
  `policies:{playable,aiPlayable,stronglyRestricted}` (128 civ files). Who may play / AI build-lockdown.
- **`succession` / `excludes`** *(auxiliary, structural)* — `succession:{upgradesTo:["UNIT_X"],promotionLine:"…"}`
  (manual unit upgrade + promotion-line link); `excludes:["TRAIT_X"]` (bare-array same-tier mutual exclusion,
  Trait `DisallowedTraitTypes`).
- **EXPLORATORY (not in data):** civ-capability `allows`/`unique` — `allows` is a forward enabler edge growing the
  same generator. Grow into; not data now.

### 6.1 ENABLE → check-for-immediate-GRANT → AUTO-BUILD → `requires` active (owner, 2026-06-15; formalized for reuse)

A recurring edge case — the **property effect-buildings** are the first real instance, but the owner ruled it
worth FORMALIZING for other potential edge cases. Four coupled rules:

1. **A grant cannot fire until its target is ENABLED.** Grants are gated by enablement (you cannot grant a thing
   that is not yet available).
2. **ENABLING triggers a grant-CHECK (the trigger).** When something becomes enabled (enters CAN GET), the engine
   must CHECK whether it should be **immediately GRANTED somewhere** — i.e. whether a `grants` edge applies to the
   now-enabled target. If so, fire the grant immediately.
3. **AUTO-BUILD:** a thing granted-on-enable is AUTOMATICALLY BUILT / placed, never player-constructed (the
   existing FreeBuilding / autobuild idea, made an explicit trigger). The flag rides the TARGET (the building).
4. **`requires` decides ACTIVE:** the auto-built thing then runs its normal `requires` gate for active/dormant.
**Worked example — property effect-buildings:** TECH `enables` `BUILDING_CRIME_LYING` → on enable, the engine
checks for a grant → `PROPERTY_CRIME` `grants` it (`grants.buildings` = a PURE LIST of granted buildings) → it is
AUTO-BUILT → the BUILDING's OWN `requires` section `{type:PROPERTY_CRIME, scope:city, min:1}` decides
active/dormant (the §3 pseudobuilding / `PropertyEffect` dormancy). **`grants` and `requires` are SEPARATE
reserved sections** (owner 2026-06-15): `grants` LISTS the granted buildings; the `requires` (WHEN it is active)
lives on the BUILDING — the thing it gates — NEVER mixed into the property's grants. The parser reads
`building.requires`; the property json carries only the grant list.

**EVERY `requires` atom is FULL + EXPLICIT + self-describing (owner 2026-06-15).** A `requires` carries its own
`type` + `scope` (+ `min`/`max`) ALWAYS — `{type:PROPERTY_CRIME, scope:city, min:1}`, never a bare `{min:1}`. The
parser must NEVER infer context (it must not have to know "this atom sits in the crime property, so add the crime
parts"). Explicitness is what makes `requires` uniform across sources and keeps the parser free of special cases.
The enable→grant-check + auto-build WIRING is #430 engine machinery (§0.6), not info data.

**⛔ THE UNIFORMITY LAW (owner 2026-06-15) — applies to EVERY info, no exceptions.** *"The moment we start doing
special things with enabling, granting, or requirement for any particular info, is the moment the clowns with
rollerskates come flying in and ruin our whole day."* `enables` / `grants` / `requires` use the **exact same
uniform structure for every info** — a property is not special, a building is not special, nothing is. No
per-info shapes, no parser branch that knows "this is the crime property," no bespoke gate. If a case seems to
need special handling (as the property effect-buildings first appeared to), the answer is to express it with the
UNIFORM primitives (grant a list + a building-side explicit `requires` atom), never to special-case the info.
This is the same law as §0.6 (no machinery in data) and the "no parser special-case" rule — stated as the bright
line it is: special-casing one info is how the whole model rots.

---

## 7. Under-modeled: COUNT / THRESHOLD / WAIVE (flagged)

`specialBuildingsWaived` (lift a SpecialBuilding per-player group CAP), `PrereqNumOfBuildings`/`PrereqAmountBuildings`
(≥N of X), `ProjectInfo PrereqProjects/iNeeded` (N of Y). These are **count thresholds**, not presence atoms — a
clause like `{"countOf":"BUILDING_X", "min":N}` or `{"waivesCap":"SPECIALBUILDING_Y"}`. A set-based `have` can't
answer counts directly. The `have` COUNT of buildings lives in the isolated CITY, so an empire/team-wide "≥N of X
across my cities" can't be answered from a single city's `have`. **RESOLVED (owner): a separate TALLY
module.** *(Named `tally` — owner 2026-06-15; over earlier "statistics" and over "census", which collides with the
existing benchmark-census project.)* It tallies how many of each thing the empire holds (counts + presence), and
lives OUTSIDE the have-builder. As the have-builder gathers each city's `have`, it **reports to
tally that "X is had, somewhere"**; tally aggregates the empire/team-wide counts. A count-threshold
clause (`{countOf:"BUILDING_X","min":N,"scope":"empire"}`) reads the **tally** count, not a city's `have`.
The have-builder stays per-city and isolated — it only emits a report side-channel; the cross-city aggregate lives
in the separate module, which **we want anyway** (demographics, AI strategy, score). Same decoupling as caching
(§2): a sibling consumer of the have-builder's output, never a complication of the model.

---

## 8. Scopes — `have` is SCOPED sets, matched per clause

`world → team → empire → area → city`. `have` is not one global set; each clause names the scope it consults.
**The scope also tells you which step the lookup feeds** — the `enables`-family lookups (tech presence/
obsolescence, building progression) feed **generation** (what's in CAN GET); the `requires` lookups (civic,
religion, connectable bonuses) feed the **gate**.

- **team** — tech presence + tech-obsolescence → generation; **trade-connected bonus presence** → `requires`.
- **empire** — civic / religion / corporation → `requires` (reversible means); per-civ research ban
  (`obsoletes`-shaped) → generation.
- **area** — CONTINENT / landmass scope (the far-coarse end of the spectrum). **NOT vicinity** (owner,
  2026-06-15: "area is continents, far on the other side of the spectrum").
- **city** — in-city buildings → generation/`requires`; **VICINITY bonus presence** (`hasVicinityBonus` — the
  city can WORK the resource, i.e. it sits on a plot in the city's work radius, which is what lets a special
  building be built there) → `requires`, at **CITY scope** (owner, 2026-06-15: "vicinity is a city scope, NOT
  area"). The city is also where the per-candidate `requires` gate is finally evaluated.
  - **What VICINITY *is*, precisely (owner, 2026-06-16): the collection of plots a city can CURRENTLY REACH —
    its workable radius, which GROWS with culture: 1 ring → 2 rings → 3 rings at high culture / metropolitan
    administration.** It is not a fixed shape; the predicate evaluates against the city's *current* reach. **A
    single plot can be in the vicinity of TWO cities** (overlapping work radii) — so a vicinity-bonus on a shared
    plot is present for *both* cities (vicinity is per-city, computed from that city's reach, not a partition of
    the map). This is the live-state the `hasVicinityBonus` / vicinity-`connection` clause queries; the static
    Info data only names the bonus + `connection:"vicinity"`, the city answers "can I currently reach it?" at
    evaluation (a city-scope predicate, akin to the §3 object-evaluated predicates). The growing-radius rule is
    the **city work-radius system**'s own surface (modifier-spec §0.8 / §3 — a system whose state the vicinity
    predicate reads).
  - **Why this matters + an ACCEPTED consequence (owner, 2026-06-16): vicinity has always been slightly fuzzy and
    is a suspected source of bugs** — pinning it to "the city's currently-reachable plots, per-city, overlapping"
    removes that fuzz. One deliberate side-effect, owner-accepted ("I can live with it"): it **slightly buffs
    natural wonders.** A natural wonder grants its effect via an autobuild gated on a vicinity bonus; under the
    precise overlapping model, if **two cities overlap on the natural-wonder plot, BOTH** now satisfy the vicinity
    condition and BOTH get the wonder's autobuild (previously fuzzy/single). **The same holds for ANY
    vicinity-bonus-gated build, incl. PLOT RESOURCES:** if two cities overlap on a bonus plot (e.g. oil), BOTH can
    build the bonus-gated improvement/building (the oil rig), **even though only ONE city can WORK the plot at a
    time** — both *qualify to build*, one *works* it. **The point is NOT whether this is perfectly logical gameplay
    (owner, 2026-06-16) — it's that the precise definition creates a CLEAN, well-defined BARRIER that can be
    ITERATED ON later.** A clean rule first, gameplay nuance tuned afterward; the small buff/oddity is the accepted
    cost of that clarity. Flag if a vicinity-bonus / natural-wonder balance question arises later.
  - **Concrete iteration path (owner, 2026-06-16): a `workedBy: SELF` predicate.** If the overlap buff is later
    judged wrong, gate the vicinity-bonus build on the plot being **worked by THIS city** (`workedBy: SELF`),
    restricting "qualify to build" to the city actually working the plot. The point: it's a **one-predicate add,
    no restructuring** — slotted into a post-migration review pass. That this refinement costs a single new
    predicate (which then self-documents + gracefully-degrades like any other) IS the modular-predicate payoff
    demonstrated: nail the clean barrier now, iterate behaviour cheaply later.
    - **NB — `workedBy: SELF` is NEEDED at the BUILDING pass anyway (owner, 2026-06-16):** there are buildings that
      **scale bonuses by WORKED TILES** (e.g. +X per worked tile of a type), which read the same city work-radius
      worked-plot state. So it lands as a real Building requirement, not only a vicinity-overlap tweak. Two forms,
      one underlying system: the **presence** form is the predicate `workedBy: SELF` ("is this plot worked by this
      city?"); the **count** form is a `per`-count over worked tiles (modifier-spec §4 — "+X per worked tile of
      type T", a count read from the work-radius system). ⚑ Flag for the Building pass (#32).
- **world** — game-wide presence + game-option gates (mostly load-prune, §10).

**Trade-connected bonus — RESOLVED (owner): TEAM scope, gathered EARLY.** A trade-connected bonus comes in via the
**team** and is added to `have` *earlier* than your local/vicinity bonuses (it's high on the spine; pass 1 gathers
it before the area/city bonuses, §9). The per-city `CvCity::hasBonus = getNumBonuses()>0` is just the **local
materialization** — each city's `have` = the team's trade-connected bonuses (added early) + its own vicinity
bonuses (added later). So there is no empire-vs-city confusion: trade-connected = a **team** clause; vicinity = a
**city** clause (the city can work the resource). Two scopes, gathered at two points in the right-then-down pass.
(`area` is a THIRD, unrelated scope — continents/landmasses — never vicinity.)

**Tally IS the higher-scope `have` (owner).** The scoped `have` splits cleanly: **city**-scope `have` is the
per-city isolated gather; the **empire / team / world** scopes' `have` is the **tally module** (§7),
aggregated from the per-city "X is had, somewhere" reports the have-builder emits. So a `requires` clause at
empire/team scope consults **tally**, and that uniformly covers BOTH presence (`∈ have` = `count ≥ 1`) and
count-thresholds (`count ≥ N`) — no special case. The have-builder feeds tally; verify reads the right scope
per clause (local gather for city, tally for empire/team/world). Tally thus *becomes* the empire state
`requires` wants — counting and presence in one place, and a module we want anyway (demographics / AI / score).

**Tally REUSES the modifier machinery — multi-level + ADDITIVE (owner).** It is not a new mechanism: it
lives at every scope level and **aggregates UP the spine additively, exactly as modifiers sum down it.** A "had X"
report at a city is an additive deposit that rolls up to empire/team/world counts. So **one additive-scope machine
serves both cascades** — modifiers sum *effect-magnitudes*, tally sum *presence-counts*. Consequently
**`requires` only ever reads the team/empire tally** (plus the local city `have`): the verify gate consults
the aggregated count/presence at the clause's scope, and never has to itself cross city isolation.

**Tally bottom out at the CITY (like modifiers) — a clean fun-fact boundary (owner).** Because the leaf is
the city, the **city-level tally are a well-defined place to record and query per-city facts** — not only the
current-state counts `requires` reads, but **lifetime/historical** ones ("how many knights has this city produced
in its lifetime?", buildings ever built, …). So the one structure serves THREE readers: the **enabler** (current
counts/presence), **demographics / UI / AI** (lifetime + aggregated fun-facts), and the additive roll-up yields
empire/team/world totals for free. One module, many readers — and it's wanted regardless of the cascade.

---

## 9. `have` gather order — right-then-down (sideways then down)

`have` isn't pure enumeration: the lower part is **derived** (you "have" a bonus only if connected/vicinity; a
building may reveal/produce one). So pass 1 gathers in dependency order — each tier left-to-right (the tech tree,
build chain, …), then **down** to the next tier when it can't go further right. Sticky top (techs/civics) first,
volatile bottom (resources/bonuses/buildings) after, so derived `have`-entries resolve against what's already
gathered. This is the enabler's *only* use of the two-axis topology — as a **gather order**, not a propagation.

---

## 10. Gates — prune-at-load vs match-at-runtime

- **LOAD-STABLE → PRUNE** (`loadPrune`, the `CvInfoReplacements` conditional swap, WorldBuilder/BUG toggles,
  per-civ research ban): rare, deliberate-setup changes → resolve at load, don't materialize if false →
  smaller `have`/candidate sets, zero runtime checks. Rebuild-on-change amortizes to ~never.
- **DYNAMIC → MATCH** (tech/civic/bonus presence; handicap under flexible difficulty): flips during play → the
  two-pass match each recompute.

---

## 11. Out of scope / deferred neighbors

Outcome/instance lifecycle (unit survival, building demolition, wonder residual, `ImprovementUpgrade`); the
**modifier cascade** (`building-cascade-conversion.md`); the **CREST** conditioner case (#430); the
**resource/bonus tier order** pressure-test; the **culture-bonus intermediary** `building→bonus→building`
(migrate faithfully now, collapse in the purge).

---

## 12. Retrofit (what changes in the ~21 converted infos)

**NOTHING IS FLIPPED (owner, 2026-06-15).** The converted `enables.{buildings,units,…}` on the sources (techs etc.)
is **correct as authored** — it is the constructive generator (§5), read forward. v0.2's "flip `enables`→`requires`"
is withdrawn. The retrofit is formalization + adding the `requires` (means) side where it's missing:

- **Keep `enables` on sources** (the constructive generator), and keep `obsoletes`/`replaces` on sources (the
  destructive members, §6). Their reverse views are derived at load for the pedia (cold path) — no hand
  re-authoring, no hot-path inversion.
- **Author `requires` (the MEANS) on targets** — positive civic/religion/resource conditions only. The ~21
  converted infos are mostly sources (tech/civic/religion/era/…), so few carry a `requires`; the bulk lands at the
  **Building / Unit** pass (old `PrereqAndBonus`/`PrereqVicinityBonus` → `requires`). Confirm the means gaps for
  `hurries`, `specialists`, `specialBuildingsWaived` at their passes.
- **No NEGATIVES on the target** — obsoletion/replacement/bans are all SOURCE-side `obsoletes`/`replaces`/`disables`
  (§5/§6), removing the candidate from CAN GET in generation; `requires` is positive means only. (Withdraws v0.2's
  "fold removal into a target `none` clause" and this session's interim `requires.disableIfAny` quantifier.)
- **Name `loadPrune` + `policies`** as their own sections (already on disk, unnamed).
- Tag each `requires` clause with scope + greyable/hiding disposition (§4).

---

## 13. Open decisions (for hole-poking)

1. **Where each edge is authored — RESOLVED (owner, 2026-06-15), by the NATURE of the action.** Permanent actions
   (constructive `enables`, destructive `obsoletes`/`replaces`, incl. all tech) are authored **forward on the source**
   and drive generation; reversible **means** (`requires`: civic/religion/resource) are authored **on the target**
   and drive the gate. They COEXIST on an entity. Hot path is forward both ways — reverse indices are cold-path
   (pedia) only, so there is **no reverse-lookup clusterfuck** and `enabledWhen` is not needed (the interim
   sticky/fluid two-object draft is withdrawn). *(Perf — RESOLVED: both steps bounded by HAS, never all ~5000
   entities; the frontier is also the AI `doProduction` choice set, strictly cheaper than the old PreLoop.)*
   - **Build-vs-operate — RESOLVED (owner, 2026-06-15), grounded in today's data.** Today's two checkpoints —
     `canBuild` (one-time) and `PrereqBonuses` (continuous, `isActiveBuilding`) — map as: `canBuild`'s STRUCTURAL
     part → `enables` (generation, hides if unmet); the RESOURCE/means part → ONE `requires` set that greys at build
     and dorms at operate (PrereqBonuses are a presence check, never consumed, so no separate build-only set). No
     `persist` flag needed (§3).
2. **Count/threshold/waive — RESOLVED (owner, §7/§8):** a separate **tally** module (wanted anyway for
   demographics/AI/score) *is* the empire/team-scope `have`. The have-builder reports "X is had, somewhere" as it
   gathers; tally aggregates; empire/team `requires` clauses (presence AND count) read tally. The
   have-builder stays per-city isolated. *(GROUNDED — sweep 2026-06-14: SIX count-threshold types, ALL cross-city,
   ZERO per-city — `PrereqNumOfBuildings`/`getNumCitiesPrereq`/`getUnitLevelPrereq`/`getNumTeamsPrereq`/
   `PrereqProjects iNeeded`/the CIVIC city-limit (CvPlayer.cpp:6707/6754/6763/6685/6872/8466) + the SpecialBuilding
   group-cap waive — empirically VALIDATING the separate tally module: a city's set-`have` answers none.)*
3. **Trade-connected bonus scope — RESOLVED (owner):** TEAM scope, gathered early into `have`; the per-city
   `hasBonus` count is the local materialization, not an empire-vs-city confusion (§8).
4. **Runtime `have`-change triggers + state-retraction — GROUNDED (sweep 2026-06-14).** Cadence CONFIRMED: many
   mid-turn triggers — AI building completion (`doProduction`→`changeHasBuilding`, CvCity.cpp:16519/14380),
   tamed-animal/heritage missions (CvUnit.cpp:8852/8909), `doAutobuild` add/remove, bonus connect/disconnect via
   worker missions (`updatePlotGroupBonus`), city conquest, religion spread, inquisition, nuke. **"Buildings-only"
   state-retraction is FALSE:** **inquisition** retracts a RELIGION (CvUnit.cpp:24036/24083) and cascades
   prereqReligion building removal (24021); bonuses are lost via connectivity-cut/depletion (civics are slot-swap,
   techs add-only). **No model change** — each retraction is just a `have`-change that recomputes the frontier, and
   a `requires` clause on the retracted religion/bonus correctly goes false; religion & bonus clauses are
   first-class, not building-only. (The instance demolition itself stays the deferred OUTCOME system.)
5. **`policies` — RESOLVED (sweep):** AUXILIARY, non-cascade. `isPlayable` (human civ selection,
   CvInitCore.cpp:1570), `isAIPlayable` (AI assignment, CvPlayer.cpp:23171), `isStronglyRestricted` (NPC
   build-whitelist meta-gate, CvCity.cpp:2205/2547) — ZERO participation in the `requires` BoolExpr. Keep §6's
   dedicated `policies` section.
6. **OR-group authoring shape — RESOLVED (owner):** handled by the `requires` full BoolExpr — OR-groups are `any`
   nodes inside it (§3); no separate shaping. *(Pedia rendering of the BoolExpr is a deferred UI detail.)*
7. **Logic-gate coverage — MAPPED, the surface is COMPLETE (owner, 2026-06-15).** Availability has a REVERSIBLE
   space (`requires`, target) and a PERMANENT space (the `enables` family, source):

   | space | where | forms |
   |---|---|---|
   | **REVERSIBLE — `requires` (target gate)** | per candidate | POSITIVE `all`/`any` + `min`/`max` counts; NEGATIVE `disableIfAny`/`disableIfAll` (→ dormancy) |
   | **PERMANENT — `enables` family (source)** | subtracted in generation | `enables` (construct); `disables` (ban→destroy), `obsoletes` (supersede), `replaces` (succeed) |

   So `requires` carries **BOTH** a positive side (the common case — must-have means) **and** a negative side
   (`disableIfAny`/`disableIfAll` — "go DORMANT while a forbidder/condition is present"; the pseudobuilding case:
   education/crime/tourism). The `requires`-negative is **REVERSIBLE/non-destructive** (dormancy); the source-side
   **`disables`** is **DESTRUCTIVE** — same "negative" intuition, different FATE (§3). (An interim pass wrongly
   withdrew the `requires`-negative; it is RESTORED — positive is just the common case, negatives are not excluded.)

   **`requires` evaluates in TWO STAGES — predictable, hence CACHEABLE: COMBINATOR then CONDITIONS (owner,
   2026-06-15).** Every `requires` resolves the SAME way (deterministic, no special cases):
   - **Stage 1 — the combinator: `all`/`any` × positive/negative.** Pure boolean structure — how the clauses combine
     (AND=`all`, OR=`any`) and polarity (must-have, or DORMANT-if-present via `disableIfAny`/`disableIfAll`); fixed by
     the clause shape before any state is read. Nesting gives OR-of-ANDs, so top-level alternative requirement-sets
     need no special form.
   - **Stage 2 — the conditions under that gate.** Each atom's check against state: a **presence** test (`∈ HAS`,
     read directly at scope — no tally), or a **count** test **`min(TYPE, N)`** / **`max(TYPE, N)`** (read at scope:
     city = local count, empire/team = **TALLY**, the only tally consumer). `min(BUILDING_BARRACKS,12)` = West Point
     prereq (a genuine "needed" count of ANOTHER type). exact-N = `min(X,N) ∧ max(X,N)` — a combination, no separate primitive.
     - **⚠ INSTANCE CAPS ARE NOT A `requires` ATOM (owner 2026-06-17) — they are the declarative `allowed` cap (§5a).**
       The earlier framing (`max(UNIT_COMMANDER,5)` / SELF-cap as a `requires` count atom) is **WITHDRAWN**: a SELF-atom forced
       an off-by-one (cap 1 → `max:0`) and conflated "needed" with "allowed." A cap is a property OF the entity, authored as
       `allowed:{scope:N}` with the real number; `requires` keeps only genuine *needed* counts of OTHER types. `SELF` no longer
       appears in `requires` (it left entirely).
   - **Predictable ⇒ CACHEABLE.** The verdict is a pure function of (combinator, conditions, state) → cache it *if
     measured worthwhile*, with predictable invalidation. Caching stays the optional wrapper of §2, never the model.
     - **Routing by Type PREFIX:** the id prefix — `BUILDING_` / `UNIT_` / `UNITGROUP_` / `BONUS_` / … — selects which
       tally bucket to count. No separate `kind` field; the namespace prefix self-routes. (Relies on every Type id
       carrying its category prefix — generally true, worth enforcing; the same prefix discriminates atoms throughout.)
     - **VOLUMETRIC-READY — author resource presence as `min(BONUS_COAL, 1)`.** Presence is the N=1 degenerate case
       of the count form; doing this consistently leaves **immediate room to go volumetric with ZERO model rework**
       (resources gain amounts → change the `1` to an `N`). Unifies all count-capable kinds under one `min`/`max`.
   - **Spec-implementation parity — build a NAMED form even if rarely used (owner).** #430 implements every named
     form as a real, unit-tested branch from day one — `disableIfAll` and `max` get built+tested alongside the common
     `disableIfAny`/`min`, even where data leans the other way — because a spec-named form the engine no-ops is latent
     breakage that bites the first modder to use it.
   - **UNIVERSAL EVALUATION ORDER.** Every process: **generate CAN GET** = `enables` − (`disables` ∪ `obsoletes` ∪
     `replaces`) over HAS → **`requires` gate** (positive means present AND negative forbidders absent). PERMANENT
     removal happens in generation (source tracks → destroyed/superseded); REVERSIBLE dormancy is in the gate (a
     `requires`-negative met → dormant, a positive unmet → greyed/dormant). Clean inverse of the old "disable before
     enable" tangle (§1).
   - **OFF the surface** (documented as possible future, never as existing): mixed-polarity disjunction ("have A OR
     not B"), OR-of-negatives. None grounded; build only if a real case appears.
8. **Tech-tree multi-parent AND/OR — GAP found + RESOLVED (owner, 2026-06-15).** **Gap:** `store.py` inverts BOTH
   `AndPreReqs/PrereqTech` and `OrPreReqs/PrereqTech` into the *same* flat `enables.techs` (store.py:68-69), so
   `enables` cannot distinguish AND from OR, and the converted tech JSON dropped each child's prereq grouping
   (`requires` is in 0 files) — "this tech needs BOTH parents" is currently unrepresentable, and flat-`enables`
   generation over-produces (one parent proposes the child). **This is essentially TECH-ONLY:** techs are the kind
   whose multi-parent AND is purely generation-side and structurally common (the tech tree); buildings/units carry a
   single tech unlock + put their real AND/OR over resources/civics in `requires` (which already captures it). (The
   rare `TechTypes` multi-tech building/unit rides the same fix.) **Resolution (owner): use the SAME `requires.build` mechanism
   everything else uses — no tech special-casing.** A child tech carries **`requires.build.all: [TECH_1, TECH_2]`**
   (or `requires.build.any` for OR-prereqs) — tech atoms, exactly like a building's `requires.build`. `enables`
   proposes the child into the frontier from one parent; `requires.build.all` confirms it actually has all parents →
   researchable. **Not the reverse-lookup problem** — the candidate is already generated, so `requires` checks its
   own small parent-list forward against HAS-techs (§3). No new mechanism, no `enabledWhen`. It's `requires.**build**`
   (researching is a one-time action like `canBuild`); techs are monotonic so there's no `operate`/dormancy side —
   confirm only. **Retrofit action:** the tech curator must RETAIN the child's `AndPreReqs`/`OrPreReqs` as
   `requires.build.all`/`requires.build.any` tech clauses (currently dropped on inversion); keep the flat `enables`
   for generation. **Efficiency (the point):** several parent techs propose the child onto a SMALL frontier; the
   `requires.build.all` confirm runs only over **that frontier subset**, NOT by re-scanning every tech's prereqs
   every turn (the old O(all-techs) check) — the bounded-by-HAS win, applied to the tech tree.

---

## 14. DEMOLITION LIST — what the cascade REPLACES and we delete (grounded 2026-06-14)

The grounding sweep's key output (owner: most of the cascade is new code; the value is knowing what gets ripped
out). The cascade (gather HAS → generate CAN GET via the `enables` family → gate by `requires` means, ONE trigger)
subsumes all of this — and because each step is bounded by HAS, the replacement runs on a far smaller list than the
gates below, which re-scan the whole database:

**A. The ~7 scattered gate functions + internals (~2100 lines)** — each → HAS (pass 1) + `enables`-family
generation + per-candidate `requires` gate (pass 2):

- `CvCity::canConstruct`/`canConstructInternal` (CvCity.cpp:2470-2528+) + `CvPlayer::canConstruct`/internal
  (CvPlayer.cpp:6509-6566+, incl. the count loops 6685/6707/6754/6763) — building gate.
- `CvCity::canTrain` (2333) + `CvPlayer::canTrain` (6370-6506) — unit gate.
- `CvPlayer::canEverResearch` (8258-8286) + `CvGame::canEverResearch` — tech gate.
- `CvPlayer::canDoCivics` (8447-8478, incl. city-limit count 8466) — civic gate.
- `CvPlayer::canFoundReligion` (10103); `CvCity::canCreate` (3008) + `CvPlayer::canCreate` (6800-6882, PrereqProjects
  loop) — project gate; `canMaintain` (3024/6885) — process; `CvPlayer::canFound` (6195) — settle.

**B. The ~37 scattered CALL SITES** across 12 files (CvCity/CvCityAI/CvDLLButtonPopup/CvDLLWidgetData/CvGame/
CvGameTextMgr/CvPlayer/CvPlayerAI/CvTeam/CvUnit/CvUnitAI/CyGame), each invoking the gates with divergent
`bContinue`/`bTestVisible`/`bIgnoreCost`/`bExtra` flags → collapse to ONE shared frontier read (the "42 → 1" win).

**C. The 5 caching structures — DELETED** (the frontier is the single recompute-on-change source; §2
cache-agnostic): `CvPlayer` FOUR arrays (`m_bCanConstruct`/`Cached` + the `DefaultParam` pair, 6520-6558);
`CvCity` lazy `std::map<int,bool>*` + the `VALIDATE_CAN_CONSTRUCT_CACHE` shadow-check (2483-2522).

**D. On-demand candidate regeneration — REPLACED by the persistent frontier:** `CvCityAI::CalculateAllBuildingValues`
PreLoop (CvCityAI.cpp:12811-12835) + the `AI_bestCityBuild`-era usages — the per-city-per-decision regeneration the
frontier kills (= the AI `doProduction` choice-set win).

**E. Precursor models — REPLACED by `requires`:** the `ConstructRequirement` GOM struct
(`Sources/ConstructRequirement.h` + `getConstructRequirements`) + its fidelity audit
(`logConstructRequirementFidelity`, CvGlobals.cpp:3465-3542) — the simpler precursor the richer `requires` BoolExpr
supersedes. The **#195 enabler reverse-index** (`buildConstructibilityEnablerIndex`, `getBuildingsEnabledBy`/
`getUnitsEnabledBy`, CvGlobals.cpp:3294/3235/3241) — **PARTLY KEPT** (it *is* the candidate-generation `enables`
index) / partly replaced.

**F. Imperative replace/disable machinery — REPLACED by DERIVATION:** `CvCity::setHasBuilding` removal block —
`getExtendsBuilding` extension cascade (14432-14445) + `setDisabledBuilding` replace/re-enable chain-walk
(14448-14477) — become two derived behaviours, NOT imperative walks: the **destructive** `obsoletes`/`replaces` chain
drops the candidate from CAN GET in generation (§6), and the **operational/dormancy** check ("disabled ≠ absent" —
a built thing whose `requires` means fail goes dormant but persists, §3) is just the per-candidate gate re-run on
things you HAVE.

**G. UI prereq rendering — MIGRATED (re-point, not delete):** `CvGameTextMgr` `buildBuildingRequiresString` /
`appendRequirementHelp` / `appendVicinityRequirementHelp` / `appendCivicRequirementHelp` / `buildRequirementItemLink`
→ re-point from `ConstructRequirement` to the cascade's `requires` + reverse index.

---

*Status: model GROUNDED and design-complete (v0.3 — HAS → CAN GET → HAS THE MEANS TO; `enables` family =
PERMANENT actions [constructive `enables` + destructive `obsoletes`/`replaces`, incl. tech] forward on the source
→ generation; `requires` = REVERSIBLE means [civic/religion/resource] on the target → gate + dormancy; the two
COEXIST; hot path forward-only, reverse indices cold-path/pedia only). Next: (a) retrofit per §12 — NO flip; keep
the `enables` family on sources, author `requires` means on targets, name `loadPrune`/`policies`; (b) prove
HAS → CAN GET → `requires` round-trips on one converted entity; (c) hand the engine (#430) the §2 generate-then-gate
model + §3 `requires` means + §5/§6 `enables` family + §4 greying + this §14 demolition list.*

*Consequence (owner, 2026-06-15): forward-only tech generation RE-ENABLES purging redundant tech-side
prerequisites (e.g. obsidian-weapon / waterproof-concrete / lead-glass) — a tech's predecessors carry the edge
forward via `enables`, so the prereq fields on the tech itself are redundant and can be shed (the model no longer
needs them for any reverse lookup). This is safe **because the whole thing — constructive `enables`, destructive
`obsoletes`/`replaces`, and the `requires` means gate — lives in ONE UNIFORM CHAIN** (the single cascade, one
trigger, §4): there is no scattered per-prereq mechanism left that a shed field could silently break. Capture in
the tech retrofit / dead-structure pass.*
