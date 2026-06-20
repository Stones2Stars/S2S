# The enabler — "can I?", in full (the deep mechanism)

> **Status:** reference   ·   **Verified against:** old `docs/dev/plans/enabler-cascade-spec.md` (BASELINE v0.3, owner-accepted 2026-06-15, refined through 2026-06-19) reconciled with the live engine cites below; re-confirm a `file:line` against the named function before relying on it.
> **Grounding:** the design is the owner-accepted enabler spec; engine cites carry `file:line` to the **live source** (paths re-grounded to the reorganized tree — `Cv*` engine classes now under `Sources/Engine/`, the cascade machine under `Sources/Cascade/`, Infos under `Sources/Infos/`). **Line numbers DRIFT** — every cite means "the named function, around this line"; confirm the function, not the integer.
>
> **What this is.** The DEEP reference for the *availability* machine — the exact `enables`-family semantics, the two-pass GENERATE-then-GATE mechanics, the per-clause greying/hiding dispositions, the build-vs-operate `requires` split, the tech-as-generator rule, and recompute-on-demand. The **overview** (the synthesis: three sets, why bidirectional, greying-for-free) is [`cascade-architecture.md` §2](../../explanation/cascade-architecture.md) — read it first; this doc is the atom-level detail it summarizes, and does **not** restate it.

**BLUF.** Availability answers one question — *"can I take this action NOW?"* — in **two passes that cannot fold into one**: (1) GENERATE the candidate frontier from the **`enables` family** (forward, source-side, permanent), (2) GATE each candidate by its **`requires`** (forward, target-side, reversible). `available(X) = X ∈ generated ∧ X.requires ⊆ HAS`. The two families COEXIST on an entity and are split by the **nature of the action**: permanent/irreversible → `enables`/`obsoletes`/`replaces`/`disables`; reversible means → `requires`. Tech is authored in `enables` (never as a generation driver in `requires`). The build-list tri-state (listed / greyed / hidden) and the dormancy ledger both fall out of the same gate. **Design state: landed (owner-accepted v0.3); engine implementation (#430) in progress** — the legacy gate functions this replaces are catalogued in [`constructibility.md`](constructibility.md) and still run.

---

## 1. Why TWO passes (and not one)

A **modifier** is a local deposit — each source drops its effect, the target sums, order-free, **one pass**; you never need the whole picture. **Availability is different: GENERATE-then-GATE, two passes**, because the candidate set must be *generated* before any per-candidate gate can run — it cannot fold-as-you-go.

Trying to run the enabler like the modifier (one-pass deposit) is what produced all the historic push-cascade machinery (deposit-on-event, "disable before enable", per-turn reconciliation, chain-walking). None of it is needed. The cascade being **bidirectional** rather than down-only — the `requires` gate resolving by a callback UP the scope chain — is the load-bearing asymmetry: down-only models OR but **cannot reliably model AND** and forces every requirement to the top of the chain. That ruling and its rationale live in [DEC-cascade-bidirectional](../../architecture/decisions.md#dec-cascade-bidirectional); not restated here.

The three sets the two passes narrow through (**HAS → CAN GET → HAS THE MEANS TO**) are the overview's job — [`cascade-architecture.md` §2](../../explanation/cascade-architecture.md). This doc details the *mechanics* under them.

### The universal evaluation order

Every recompute runs the same shape, no special cases:

```
generate CAN GET = union(enables over HAS) − (disables ∪ obsoletes ∪ replaces over HAS)
gate            = for each candidate: requires met?  (positive means present AND dormancy-negatives absent)
```

Both reads are **FORWARD** — `enables` forward from the source, `requires` forward from the target — so the hot path never inverts. Reverse indices (derived at load, kept) are **cold-path / pedia only** (§7). Permanent removal happens in generation (source tracks subtract → destroyed/superseded); reversible dormancy happens in the gate (a `requires`-negative met → dormant, a positive unmet → greyed/dormant). This is the clean inverse of the old "disable before enable" tangle.

---

## 2. The `enables` family — the GENERATOR (permanent, source-side, forward)

Four source-side base objects drive generation. All are read **forward over HAS**; `enables` ADDS to the candidate union, the other three SUBTRACT. The classifier is the **nature of the action**, not the atom kind.

| object | nature | new builds | existing instances |
|---|---|---|---|
| **`enables`** | constructive, permanent | ADDED to CAN GET | — (this is the unlock) |
| **`disables`** | destructive ban (a law) | removed while disabler ∈ HAS | **DESTROYED** — repeal ⇒ REBUILD from scratch (see §2.1 for the effect-disable refinement) |
| **`obsoletes`** | passive supersession (tech) | removed | **PERSIST** — target decides its fate (§2.2) |
| **`replaces`** | succession | removed | replaced by the successor (transitive chain) |

So **CAN GET = union(`enables` over HAS) − (`disables` ∪ `obsoletes` ∪ `replaces` over HAS)** — the whole source-side family, read forward, bounded by HAS, no reverse lookup. The authored per-target-kind shapes (`enables:{buildings,units,builds,techs,…}`, the parallel `obsoletes`/`replaces` sections) are the [`data-model.md` §3.1](data-model.md) concern, not restated here.

- **`obsoletes` vs `disables` is kept SEPARATE for modder-clear semantics** even though obsoletion *could* technically be a `disables`: progress-supersedes (`obsoletes`) vs policy-forbids (`disables`). This also keeps the useful pedia line **"Obsoleted by [tech]"** while needing **no** "potentially disabled by [law]" worry-line on every bannable building — a ban is a policy listed on the *law's* side.
- **`replaces` overrides the obsolete-stay choice (precedence).** If a target is BOTH obsoleted AND replaced, `replaces` wins — the successor has taken its slot, so the predecessor is genuinely gone even if its `whenObsolete` (§2.2) wanted it to linger.
- **The `enables` index is partly the kept #195 enabler reverse-index** (`buildConstructibilityEnablerIndex`, `getBuildingsEnabledBy`/`getUnitsEnabledBy`, `Sources/Engine/CvGlobals.cpp` ~3294/3235/3241) — it *is* the candidate-generation `enables` index, partly kept and partly replaced (the demolition map, §8).

### 2.1 `disables` carries TWO fates by source intent

The default framing is **destruction**: a ban does not park a building dormant — it tears it down. Ban the death penalty → the electric chair is removed and its space repurposed; ban prostitution → the brothels are demolished. **Repeal later and you must REBUILD** (pay the cost) — nothing mothballed silently resumes. This destruction (not a pause) is exactly why `disables` is its own source-side track. This is the **law/doctrine** flavor.

**⚠ REFINEMENT (owner ruling 2026-06-19) — the reversible EFFECT-disable.** An EFFECT source (a property-effect pseudobuilding, §3.3) may `disables` a target so the target goes **DORMANT** (parked, reactivates when the disabler clears), **never destroyed/rebuilt**. Worked case: `BUILDING_POLLUTION_BLACKENED_SKIES` `disables` the telescope/observatory buildings — they go dormant while the skies are blackened and reactivate when the air clears. A normal building may likewise `disables` a band (a rat-catcher disabling the disease pest band). So `disables` carries two fates: **law-ban → destroyed (rebuild on repeal)**; **effect-disable → dormant (auto-resumes)** — the source-side counterpart of the target-side `requires`-negative dormancy (§3.2). *(Exact mechanism — whether an effect-`disables` routes through the same dormancy path as the `requires`-negative, and how a source declares dormant-vs-destructive intent — pins at #430; `disables` is unparsed today. The lone converted `disables` is the per-civ research ban on the Neanderthal barbarians — `TECH_SEDENTARY_LIFESTYLE` — modelled as a reversible ban; scope is just a parameter.)*

### 2.2 Obsoletion fate is a TARGET-side decision

The `obsoletes` edge on the source only signals *"X is now obsolete."* **WHAT happens to X's instances is authored ON X** — a `whenObsolete` property on the target: some buildings go defunct/destroyed, while **wonders and walls especially LOSE their specific bonus but REMAIN** as a culture/tourism attraction. The obsoleting tech never dictates this. Crisply: `disables` = a hard *"be gone"* (source commands destruction, target gets no say); `obsoletes` = a soft *"we don't need you anymore, do whatever"* (a signal; the obsoleted entity reacts). The instance's actual fate lives in the deferred **outcome / instance-lifecycle system** (out of scope, §6).

### 2.3 The empire/team-wide tier — bans/projects ONE scope up

A long-latent concept the spec finally defined: **constructables that live at empire/team scope instead of city scope** — "buildings" and "projects" one tier up. They fit the model with **NO new machinery** — the scope spine (§4) already has team/empire — and carry the FULL edge set at that scope:

- **Constructive stage-gates** (the `enables` edge): an empire/team project that unlocks a major capability — the **space line** is the prime class (moon-base project `enables` lunar settlement → mission-to-mars `enables` Mars, the #421 stage-gate retiring the dedicated Cislunar map).
- **Bans** (the `disables` edge): the "doctrine" flavor — a `disables` list + empire-wide `modifiers`, enacted via `requires.build` and repealable like demolishing a building one tier up.
- **Empire-wide modifiers**: the #421 team-buildings.

This gives traditionally-**shoehorned** moral/legal bans (slavery, cannibalism, prostitution, death penalty) a principled home: just `disables` from an empire-scope source. It also **replaces today's autobuild clunk** — an empire-wide effect faked by ONE building that autobuilds into every city (the `FreeBuilding` mechanism, ~345 uses) becomes ONE building at empire scope, no per-city fan-out. *(INTERIM: bans/doctrines + the whole tier are their own LATER issue — the existing per-city machinery is kept and works with the new cascade without refactor; the migration does not block on building the tier. Same "works now, organize-and-promote later" bucket as `PropertyEffect`/`BaseEffect`.)*

---

## 3. `requires` — the reversible MEANS gate (target-side, forward)

`requires` answers one thing — *do I have the MEANS* — checked **forward** (set-membership: does HAS contain this atom?). It is authored on the **target**, is a positive-only BoolExpr over the means (civic / religion / resource/bonus, via `all`/`any` + `min`/`max` counts), plus a narrow negative for dormancy (§3.2). It gates the generated candidates AND re-checks built things for dormancy. The authored JSON shape (`requires.build` / `requires.operate`, the `{all,any,noneOf}` tree, the full+explicit atoms) is [`data-model.md` §3.2 + §2.4](data-model.md); this section is the *semantics*.

### 3.1 The BUILD vs OPERATE split (the core distinction the object carries)

`requires` distinguishes two timings, grounded in today's two building checkpoints:

- **`requires.build`** — the **one-time** construction requirement (today's `canBuild` resources). Must hold to build; if missing → **GREYED** (with the resource named, §5). **NOT re-checked after** — once built, losing it does nothing.
- **`requires.operate`** — the **continuous** prerequisite (today's `PrereqBonuses`; `CvCity::isActiveBuilding`, `Sources/Engine/CvCity.cpp` ~14364). Must hold to build AND to keep running; re-checked every recompute on a built thing — failing → **DORMANT** (disabled, not demolished; wakes when it returns).

So: **build-time gate (greying) = `build` ∧ `operate`** (need both present to construct); **operate-time gate (dormancy) = `operate` only** (a built thing re-checks just the continuous part). `PrereqBonuses` are a presence check, never consumed, so there is no separate build-only resource set and no `persist` flag is needed.

**Applicability — `requires.operate` is for PERSISTENT STATE; units use `requires.build` today.** Per §6, units are *leaf actions* that exit the model once built, so dormancy (`operate`) applies to persistent city state (buildings). Three distinct unit behaviors fall out cleanly:

- **`obsoletes` a unit** → removed from CAN GET (no NEW builds), but **existing units STAY ON THE MAP** (design choice = keep-on-map; instance destruction is the deferred outcome system).
- **`requires.build` unmet** → can't build now (greyed); existing units unaffected.
- **`requires.operate` on a unit (FUTURE — e.g. tanks need fuel)** → would disable an existing unit going forward (dormant/inactive while the input is missing) while it stays on the map — a reversible turn-off the old return-trip `disables` could never express. The structure already supports it (just a `requires.operate` on the unit); not modelled now.

### 3.2 The dormancy NEGATIVE (`noneOf`) — distinct from a `disables` ban

`requires` is mostly positive, but admits a **negative for reversible DORMANCY**: a `noneOf` clause (`noneOf:[A,B]` = "requires NONE of A,B present"), parallel to the positive `all`/`any`. Condition met → dormant; clears → reactivates. This is the **pseudobuilding** case (education / crime / tourism thresholds switching on/off). It is split from the source-side `disables` ban by **FATE** and **AUTHOR**:

| | `requires`-negative (`noneOf`) | source-side `disables` ban |
|---|---|---|
| fate | **DORMANT** — kept, reverses | **DESTROYED** — repeal ⇒ rebuild |
| authored on | the TARGET ("I go dormant while X") | the SOURCE/law ("I destroy X") |
| processed in | the gate (per-candidate) | generation (subtract + destroy) |

Permanent removal (obsoletion/replacement) is also source-side, never a `requires` clause. So `requires` covers the REVERSIBLE space (positive means + dormancy negatives); the `enables` family covers the PERMANENT space.

> **⚠ SUPERSEDED (owner 2026-06-17) — the one-time RACE/uniqueness gate is the `allowed` CAP, NOT a `requires` SELF-negative.** Global-uniqueness was originally modelled as `requires.build.noneOf:[{type:SELF,scope:world}]` (Tech `bGlobal`, world wonders, unique units) — withdrawn because it forced "cap 1" to be written `max:0` and conflated *needed* with *allowed*. `bGlobal` = `allowed:{world:1}` now (the religion-founding-once race; `CvPlayer::canEverResearch` bars it once `countKnownTechNumTeams>0`). **`SELF` is gone from `requires` entirely.** The declarative cap is [`data-model.md` §3.4](data-model.md#34-allowed--the-declarative-instance-cap).

### 3.3 Pseudobuildings = property-effect cascade participants (`PropertyEffect`)

The dormancy negative's primary user is **pseudobuildings** — the in-city representation of a city's **property effects** (`PROPERTY_CRIME`/`EDUCATION`/`DISEASE`/tourism — the `CvProperties` system, #429) at a threshold band. Behaviourally they are just cascade participants (carry `requires` incl. the dormancy negative + `enables`/etc., no behavioural difference from a building). Today they ARE the `BUILDING_EFFECT_*` family (e.g. `BUILDING_EFFECT_HERITAGE_BIG_CATS`); the future `PropertyEffect` class formalizes them OUT of the building roster (a UX/presentation question, deferred — do NOT build now; the `notConstructible` flag is the correct interim).

- **Placement = CONTINUOUS band-membership, not one-time (VERIFIED 2026-06-18).** The mechanism is the **`PropertyBuilding` band system** (`CvPropertyInfo` `iMinValue`/`iMaxValue`/`BuildingType`), distinct from `bAutoBuild`. `CvCity::checkPropertyBuildings` (`Sources/Engine/CvCity.cpp` ~1490, *"checked each turn"*) ADDS the building when the property value enters `[iMinValue,iMaxValue]` and REMOVES it (`changeHasBuilding(false)`) when it leaves. So the cascade condition is a continuous property-in-band atom (`{type:PROPERTY_CRIME, scope:city, min, max}`) on the **`requires.operate`** side — not a one-time enables-side placement.
- **END-STATE (owner 2026-06-18): retire the bespoke per-turn `checkPropertyBuildings` ENTIRELY** — model the band as a uniform `requires` dormancy condition. The building is enabled once; `requires` toggles it active/dormant as the value enters/leaves the band — no per-turn `changeHasBuilding` churn, no "property special case." Dormant property-effect buildings are delisted in the Python UI's separate property-effect list, so the normal building listing is untouched.
- **⛔ Pseudobuildings MUST NOT `replace` each other — bands stack CUMULATIVELY (owner ruling 2026-06-19).** Every band whose threshold is met stays ACTIVE; a higher band never supersedes a lower one. A pseudobuilding's lifecycle is enable / obsolete / active / dormant — **`replace` is the wrong mechanism between bands.** Crime/disease/tourism/pollution already follow this. **Education was the lone outlier** (a parallel succession of 4 ladders × 13 bands chained by `ReplacementBuildings`, each carrying the FULL per-band value) — pulled in line by the curator (`apply_property_bands`, `Tools/Migration/curate_building.py`): pseudo→pseudo `replaces` stripped, each ladder band re-authored as its INCREMENTAL delta (`full[rank] − full[rank−1]`) so the cumulative active bands reproduce the top band's total. **A pseudo→REAL `replace` becomes a reversible `disables`** (§2.1 effect-disable). Full record: `../../plans/` (modifier-cascade discrepancies §A.1).

### 3.4 Object-evaluated PREDICATES — each a system's isolated query-surface

Some conditions need runtime object state the static Info cannot hold (is this city the holy city? the capital? the player's state religion?). The **object itself (city/player/plot) answers it at evaluation** — the engine's predicate owns any compound logic. The vocabulary, bare-vs-parameterized authoring sugar, and the desugar rule (`HAS_RIVER` ≡ `{HAS_RIVER:true}`) are [`data-model.md` §2.5](data-model.md#25-predicates--each-a-systems-isolated-query-surface). The semantic rules that matter here:

- **A missing/unknown predicate is IGNORED — DROPPED from the evaluation, NOT substituted with `false`.** `inactive == ignored, NOT false`. Substituting `false` would (1) spuriously turn OFF a deposit `enabled` by a now-missing predicate ("the disable would kick in") and (2) corrupt `all`/`any` compounds. So a dangling atom is removed from the boolean expression and the clause is evaluated on its remaining known conditions — this is what makes a whole system (e.g. Global Warming) safely *removable* without hunting every reference. *(Exact compound-logic semantics for a dropped atom — incl. a deposit whose ONLY condition went missing — pin at #430.)*
- **Each SYSTEM documents its OWN predicates** — the predicate is part of that system's surface, not a central monolithic registry. Adding a system adds *and documents* its predicate(s). A **planned alignment pass** (after data migration, before #430 parsing) brings Religion, Corporations, and Traits (simple + complex/Thunderbrd) into line as isolated systems with self-contained, gracefully-ignorable predicate surfaces.

### 3.5 Two-stage evaluation — combinator then conditions (hence cacheable)

Every `requires` resolves the same way:

- **Stage 1 — the combinator:** `all`/`any` × positive/negative. Pure boolean structure (AND=`all`, OR=`any`) + polarity (must-have, or DORMANT-if-present via `noneOf`), fixed by clause shape before any state is read. Nesting gives OR-of-ANDs, so top-level alternative requirement-sets need no special form.
- **Stage 2 — the conditions:** each atom's check against state — a **presence** test (`∈ HAS`, read directly at scope), or a **count** test `min(TYPE,N)` / `max(TYPE,N)` (city = local count, empire/team = TALLY). `min(BUILDING_BARRACKS,12)` = West Point prereq; exact-N = `min(X,N) ∧ max(X,N)`. **Routing by Type PREFIX** (`BUILDING_`/`UNIT_`/`BONUS_`/…) selects the tally bucket — no separate `kind` field. **Author resource presence as `min(BONUS_COAL,1)`** (presence = the N=1 case) — volumetric-ready with zero model rework.

Predictable ⇒ **CACHEABLE** (pure function of combinator + conditions + state). **Spec-implementation parity:** #430 builds every named form as a real unit-tested branch from day one — `noneOf`/`max` get built+tested alongside the common positive/`min` even where data leans the other way; a spec-named form the engine no-ops is latent breakage. **OFF the surface** (documented as possible future, never as existing): mixed-polarity disjunction ("have A OR not B"), OR-of-negatives — none grounded, build only if a real case appears.

### 3.6 Tech as GENERATOR — never a `requires` generation driver

**Tech is a permanent constructive action → authored in `enables`, read forward.** Relying on a `requires`-tech to *generate* would force a tech→targets **reverse index** (the reverse-lookup clusterfuck), so generation is always `enables`-driven. **BUT** once a candidate is in the frontier, a tech MAY appear in its `requires.build.all`/`requires.build.any` as a **CONFIRM** — the same `requires.build` every entity uses, no special-casing.

This is the **tech-tree multi-parent** resolution: `store.py` inverts both `AndPreReqs` and `OrPreReqs` into the same flat `enables.techs`, so flat-`enables` over-produces (one parent proposes the child). The fix: a child tech carries `requires.build.all:[TECH_1,TECH_2]` (or `.any` for OR-prereqs); `enables` proposes the child from one parent, `requires.build.all` confirms it has all parents → researchable. **Not the reverse-lookup problem** — the candidate is already generated, so `requires` checks its own small parent-list forward against HAS-techs. It is `requires.build` (researching is one-time, like `canBuild`); techs are monotonic so there is no `operate`/dormancy side. **Retrofit action:** the tech curator must RETAIN the child's `AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any` (currently dropped on inversion); keep the flat `enables` for generation.

---

## 4. Scopes & the gather order

`have` is **SCOPED sets**, not one global set — `world → team → empire → area → city`; each clause names the scope it consults, and the scope also tells you which step the lookup feeds (`enables`-family lookups feed generation; `requires` lookups feed the gate). The full spine + per-machine directional flow lives in [`data-model.md` §2.2](data-model.md#22-scopes--the-containment-spine); the enabler-specific facts:

- **team** — tech presence + tech-obsolescence → generation; **trade-connected bonus presence** → `requires` (TEAM scope, gathered EARLY into `have`).
- **empire** — civic / religion / corporation → `requires`; per-civ research ban (`disables`-shaped) → generation.
- **area** — CONTINENT / landmass (the far-coarse end). **NOT vicinity.**
- **city** — in-city buildings → generation/`requires`; the per-candidate gate is finally evaluated here. **VICINITY** bonus/terrain/feature/improvement presence (`hasVicinityBonus`) → `requires` at CITY scope.
- **world** — game-wide presence + game-option gates (mostly load-prune, §5).

**VICINITY, precisely (owner 2026-06-16):** the collection of plots a city can CURRENTLY REACH — its workable radius, which GROWS with culture (1 → 2 → 3 rings). Not a fixed shape; the predicate evaluates against the city's *current* reach (`getCityIndexPlot(0..getNumCityPlots())`). A single plot can be in the vicinity of TWO cities (overlapping radii) — a vicinity-bonus on a shared plot is present for *both*. The static Info only names the bonus + `connection:"vicinity"`; the city answers "can I currently reach it?" at evaluation. The terrain/feature/improvement plot prereqs (`PrereqOrTerrain`/`PrereqOrFeature`/`PrereqOrImprovement` → `{HAS_TERRAIN/FEATURE/IMPROVEMENT}`) are the SAME vicinity query, evaluated as a radius scan with **no ownership/worked filter** — deliberately MORE PERMISSIVE than BOTH legacy semantics it subsumes (`isValidTerrainForBuildings`, `Sources/Engine/CvCity.cpp` ~20402, owned plots; and `ConstructCondition` `GOM_TERRAIN`, `Sources/Engine/CvGameObject.cpp` ~1241, worked plots). One deliberate owner-accepted consequence: two cities overlapping a natural-wonder/bonus plot BOTH qualify to build the gated improvement (only one *works* it). The clean barrier first; the `workedBy:SELF` predicate is the one-predicate tightening path if the overlap buff is later judged wrong.

**Gather order — right-then-down (§9 of the spec).** Pass 1 gathers in dependency order: each tier left-to-right (tech tree, build chain), then DOWN to the next tier. Sticky top (techs/civics) first, volatile bottom (resources/bonuses/buildings) after, so derived `have`-entries (you "have" a bonus only if connected/vicinity; a building may produce one) resolve against what's already gathered. This is the enabler's *only* use of the two-axis topology — as a gather ORDER, not a propagation.

**Higher-scope `have` IS the tally.** City-scope `have` is the per-city isolated gather; empire/team/world `have` is the **tally module** (the additive roll-up of per-city "X is had, somewhere" reports). A `requires` clause at empire/team scope consults the tally — uniformly covering presence (`∈ have` = `count ≥ 1`) and count-thresholds (`count ≥ N`). The tally's mechanics, count-threshold cases, and serialize-nothing rule are its own doc; cross-cutting ruling [DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing). Six count-threshold types were swept (`PrereqNumOfBuildings`, `getNumCitiesPrereq`, `getUnitLevelPrereq`, `getNumTeamsPrereq`, `PrereqProjects iNeeded`, the CIVIC city-limit) — **ALL cross-city, ZERO per-city**, empirically validating the separate tally module: a city's set-`have` answers none.

---

## 5. Greying — the build-list tri-state falls out of the gate

The tri-state is a **byproduct** of the same generate-then-gate pass — no separate "why greyed" computation. The gate is per-clause; each clause carries a **disposition** (`greyable` or `hiding`), a property of the clause KIND set once:

| state | condition | disposition |
|---|---|---|
| **HIDDEN** | not in CAN GET (generation didn't reach it) | — |
| **LISTED** (buildable) | in CAN GET ∧ all `requires` clauses met | — |
| **GREYED** | in CAN GET ∧ only `greyable` clauses unmet (a connectable resource, an unadopted civic) | `greyable` ("needs `BONUS_COPPER`" / "needs `CIVIC_GUILDS`") |
| **HIDDEN** | in CAN GET ∧ a `hiding` clause unmet (wrong era) | `hiding` |

Permanent removal (`obsoletes`/`disables`/`replaces`) drops a target from CAN GET entirely → the "not in CAN GET" HIDDEN case, not a greying clause. The same `requires` evaluation that gates buildability also yields *why* it's greyed/hidden.

**A clean per-case LEVER — grey (`requires`) or hide (`enables`) — is a UI question, not operational.** Both placements work identically in the engine; authoring a resource on one side or the other is a per-case display choice: `requires` → GREYED with the resource named (player can craft/trade for it); `enables` → HIDDEN until present. **General lean: grey on resources** (surfacing "what to acquire" is usually wanted); flip to `enables` when hiding is the better UX.

**The frontier is a SHARED choice set — UI *and* AI.** Computed once per recompute: the UI greys from it, and the **AI production decision** (`doProduction` / `AI_bestCityBuild`) iterates **only the known frontier** instead of filtering/scoring the whole ~5000-entity database every decision. The cascade has answered "can I build this"; the AI only does the "should I" scoring over a small set — arguably the biggest systemic payoff. **ONE trigger, not 42:** today availability is recomputed ad-hoc at ~42 scattered call sites each with slightly-divergent logic; the cascade triggers ONE recompute, everyone reads the shared frontier (the consolidation win — and why recompute-on-the-fly is fine: the *single* recompute replaces dozens of scattered ones).

---

## 6. Scope — model the ACTION, not the OUTCOME (and recompute cadence)

The cascade decides what is **OFFERED** — build a unit/building/wonder, lay an improvement, research a tech, adopt a civic, found a religion/corporation. It does **NOT** model what the action *produces*: a build yields either an independent OUTCOME (a unit off to the map, an improvement on a tile — each exits this model) or persistent CITY STATE (a building — stays in the model). Everything about a produced instance's fate (a unit surviving, a building demolished, a wonder keeping tourism after obsoletion, `IMPROVEMENT_COTTAGE → HAMLET` upgrading) is the **outcome / instance-lifecycle system — a deferred neighbor, explicitly out of scope.** The cascade's working state is the **STATE** entities (buildings + techs + civics + religions + bonuses…); units & improvements are **leaf actions** the cascade only decides whether to offer.

**Recompute cadence — the frontier is `f(have)`; recompute on `have`-CHANGE.** The DOMINANT cadence is once per turn (held for the turn), but same-turn `have`-changes DO occur and MUST trigger a mid-turn recompute: the **AI** processes production mid-turn (finish A, then build B that depended on A the same turn — A lands in `have` immediately); a **human** hits the same via tamed-animal mission builds. So the rule is **recompute when `have` changes**, not "once per turn." This stays cheap — a recompute of the bounded two-pass over the *affected scope* (one city's `have`), not an incremental deposit, firing only when `have` actually changes. Confirmed mid-turn triggers (sweep 2026-06-14): AI building completion (`doProduction`→`changeHasBuilding`, `Sources/Engine/CvCity.cpp` ~16519/14380), tamed-animal/heritage missions (`Sources/Engine/CvUnit.cpp` ~8852/8909), `doAutobuild` add/remove, bonus connect/disconnect, city conquest, religion spread, **inquisition** (retracts a RELIGION — `Sources/Engine/CvUnit.cpp` ~24036/24083 — disproving "buildings-only" state-retraction), nuke.

**No caching needed; the model is CACHE-AGNOSTIC.** Today's engine already recomputes availability over the *full* entity set constantly and has tolerated it forever; the bounded two-pass is strictly *less* work. The have/frontier computation is a **pure function `have → frontier`** — any cache + delta/incremental updates are a separate optimization LAYER wrapped around it, never leaking into the model. **Sticky vs fluid is NOT model structure** — it survives only as an optional recompute-*cadence* hint (tech rarely changes; resources/civics flip often). The model has ONE `requires`.

---

## 7. Hot path forward-only; reverse views are cold-path/pedia

**Each EDGE is authored ONCE, on its natural end** — `enables`/`obsoletes`/`replaces` on the source (the actor), `requires` on the target (the thing needing means). **The reverse view of each is derived at load and KEPT, but COLD-PATH only** — it powers the pedia / web-Civilopedia ("Unlocks", "Obsoletes", "Required by") and is **never read on the hot path**: generation reads `enables`/`obsoletes`/`replaces` forward, the gate reads `requires` forward. Inversion happens once at load, never per-recompute — the answer to the reverse-lookup concern.

There is **no single "`requires` ⇄ `enables`" mirror.** They are *different relationships* — `enables` = a permanent unlock (tech→building), `requires` = a reversible means (building→resource) — not two ends of one edge. An item commonly carries several at once: a building `enables` the units it unlocks, may `obsoletes`/`replaces` a predecessor, and `requires` the resources/civics it needs to operate.

**Load-time gates (`loadPrune`).** A separate axis from the runtime two-pass: **LOAD-STABLE → PRUNE** (`loadPrune`, the `CvInfoReplacements` conditional swap, WorldBuilder/BUG toggles, per-civ research ban) resolve at load and don't materialize if false → smaller `have`/candidate sets, zero runtime checks. **DYNAMIC → MATCH** (tech/civic/bonus presence) flips during play → the two-pass matches each recompute.

---

## 8. What this REPLACES (the demolition target)

The two-pass cascade (gather HAS → generate CAN GET via the `enables` family → gate by `requires`, ONE trigger) subsumes the scattered legacy buildability machinery — and because each step is bounded by HAS, it runs on a far smaller list than the gates below (which re-scan the whole database). The as-shipped mechanism being shadowed and cut is catalogued in [`constructibility.md`](constructibility.md) — `CvCity::canConstruct`/`canConstructInternal`, `CvPlayer::canConstruct`, `canTrain`, `canEverResearch`, `canDoCivics`, `canFoundReligion`, `canCreate`/`canMaintain`/`canFound` (~7 gate functions, ~36 call sites across 12 files incl. the `/diagnostic` endpoints), the 5 caching structures, the `ConstructRequirement` precursor model, and the imperative `setHasBuilding` extends/replace/disable chain-walks. That doc holds the current `file:line` inventory; this doc is the *design* that replaces it.

**The STATE-MAINTAINER demolition (owner 2026-06-18) is the deep part.** A building's *active / present / dormant* state is today decided by a zoo of bespoke engine mechanisms ("we rely on quirks"). Each must fold into the ONE cascade model — governed by **our own defined facts, not engine quirks**:

- `CvCity::isReligiouslyLimitedBuilding` (religion dormancy) → `requires.operate` (`STATE_RELIGION` + a `hasAllReligionsActive` waiver clause).
- `CvCity::checkPropertyBuildings` (per-turn property-band add/remove) → `requires.operate` property-in-band dormancy (§3.3).
- `bAutoBuild` + the per-turn autobuild loop → `enables` + the `autoBuild` placement marker.
- `isSpecialBuildingNotRequired` + the building-GROUP cap → uniform group-gate inheritance.
- `isActiveBuilding` + `PrereqBonuses` resource dormancy (`Sources/Engine/CvCity.cpp` ~14364) → `requires.operate` resource dormancy.

**⚠ The hard switch must wrangle ALL of these, NOT shadow-coexist.** The failure mode is stopping at shadow-parity (the cascade computes the right answer but the quirks still RUN → two systems). Each maintainer must be DELETED at the switch, its behaviour re-derived from the cascade. **Bias = OVER-REACH:** *"I rather go further than needed, than not far enough"* — when unsure whether a quirk should fold in, FOLD IT. **Map-before-delete** ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)) is the gate: each maintainer needs its OWN runtime behaviour shadow (the buildability `/diagnostic/sweep` maps buildability ONLY — it excludes already-built things, so zero buildability-divergence ≠ fully mapped). Built so far: the auto-placement shadow (`/diagnostic/placementSweep`) and the dormancy shadow (`/diagnostic/dormancySweep`). *(Live migration status — what's built vs pending, the shadow specifics — is roadmap; it lives in `../../plans/`, not this reference.)*

---

## See also

- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) §2 — the enabler OVERVIEW (the synthesis: HAS → CAN GET → HAS THE MEANS TO, the three machines, greying-for-free). Read it first; this doc is the deep mechanism under it, and links rather than restates it.
- [`data-model.md`](data-model.md) — the authored JSON shape the enabler consumes (`enables`/`requires`/`allowed` section syntax §3, the shared atom/condition/predicate vocabulary §2). This doc is the *semantics*; that doc is the *shape*.
- [`fixed-point-and-scales.md`](fixed-point-and-scales.md) — the scale registry (the `requires` count atoms and any value math are human-readable; the human↔×100 convention lives there, not here).
- [`constructibility.md`](constructibility.md) — the **legacy** `canConstruct`/`canTrain` + `ConstructRequirement` + reverse-index machinery this design replaces (the as-shipped mechanism being shadowed and cut); holds the current `file:line` demolition inventory.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — the rulings ledger: [DEC-cascade-bidirectional](../../architecture/decisions.md#dec-cascade-bidirectional) (`requires` = the AND mechanism, resolved up-chain; why not down-only), [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete) (shadow before cutting a maintainer), [DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing) (the higher-scope `have`).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.
