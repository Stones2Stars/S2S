# The enabler — "can I?"

> **⛔ NAMING ([DEC-enabler-not-cascade](../architecture/decisions.md#dec-enabler-not-cascade)).** This is **the
> enabler** — a system SEPARATE from the modifier **cascade**. The two are routinely conflated; do not. "cascade"
> names the modifier ("how much?") system ONLY. The enabler's classes carry no `Cascade` prefix
> (`EnablerKernel`/`BuildingEnabler`/`UnitEnabler`/`TechEnabler`), and its availability getters read the enabler's
> OWN cached sets directly.

The enabler is the machine that decides **what an entity is allowed to do or build right now** — research a tech,
train a unit, construct a building, adopt a civic, lay an improvement. It answers one question per candidate: *"can I
take this action this turn?"* — and as a byproduct, *why not* (greyed / hidden).

It **reads** the availability data authored on entities — `enables`, `obsoletes`, `replaces`, `disables`,
`requires`, `allowed` (the [json spec](json.md) §4 owns their shape). This doc is the **machine** that consumes
them; it does not restate the JSON syntax.

---

## 1. The one idea: GENERATE, then GATE

Availability is **two passes that cannot fold into one** — you must *build the candidate list* before you can
*check each candidate*:

1. **GENERATE** the candidates — from everything you HAVE, what does it unlock? (the `enables` family).
2. **GATE** each candidate — are its `requires` satisfied, and is it under its `allowed` cap?

> `available(X)  =  X was generated  ∧  X.requires met  ∧  X under its allowed cap`

The two passes narrow through three sets:

| set | what it is |
|---|---|
| **HAVE** | what you actually possess — built / researched / adopted |
| **CAN GET** | the candidate frontier — everything HAVE unlocks, minus what's been removed |
| **HAS THE MEANS** | the candidates whose `requires` are met (the buildable set) |

**The two passes are not peers — pass 1 is the authority, pass 2 is the follow-up.** The `enables` family
(`enables` / `disables` / `replaces` / `obsoletes`) is the **sole authority on what is in the tree** (CAN GET) —
what you can *actually do*; it alone adds and removes candidates, and it runs **to completion first**, producing
the final tree. **`requires` runs afterward and CANNOT change tree membership** — it never adds or removes a
candidate, it only decides whether a tree member is **attainable now** (buildable) or **unattainable** (greyed,
or dormant once built). A failed `requires` leaves the thing in the tree, just out of reach.

> **⛔ THE GENERATE TREE IS CONDITIONAL-FREE — every `all`/`any`/`noneOf` lives EXCLUSIVELY in `requires`.** Pass 1 is
> **pure set algebra** — `union(enables) − (disables ∪ obsoletes ∪ replaces)` over HAVE (§2) — with **zero condition
> evaluation**: no combinators, no predicates, no "if". A candidate that *needs multiple things* is **never** a
> conditional edge in the tree — the tree unconditionally proposes it from *any* enabling source, and the AND
> ("actually need T1 **and** T2") is enforced by **`requires.build.all` on the gate** (§2 multi-parent tech; §3). So
> when the parse-time reverse-mapping inverts prereqs into `enables` it must **not** drag AND/OR into the tree — the
> tree stays unconditional; the AND/OR distinction is preserved only for the `requires`-side reconstruction. This is
> the load-bearing split: **generation is a cheap top-down sweep with no calculation; the ONLY calculation is the
> `requires` gate**, and it runs over **just the frontier** — the CAN GET candidates not yet built (the "can I have?"
> set, §6) for `requires.build`, and the built instances for `requires.operate` (§3.2) — never the whole database.

Both passes read **forward** — `enables` forward from the source, `requires` forward from the target — so the
hot path never does a reverse lookup. What is **recomputed on demand is the FRONTIER** — the pure-`f(HAVE)`
CAN GET set (§7) — **never the entire enabler**: the enabler's runtime outputs (the stored availability
vectors, the operating-building set §3.2) are maintained in place by targeted propagation, not recomputed.

---

## 2. Pass 1 — GENERATE the frontier (the `enables` family)

Four source-side edges drive generation. All read forward over HAVE: `enables` **adds** to the candidate set,
the other three **remove** from it.

| edge | nature | new builds | existing instances |
|---|---|---|---|
| **`enables`** | a permanent unlock | **added** to CAN GET | — (this *is* the unlock) |
| **`disables`** | a **law / ban** (policy forbids) | removed while the disabler is held | **destroyed** — torn down; rebuilt on repeal. *(Dormancy is NOT a `disables` — it's the target's `requires.operate.dormant`, §3.)* |
| **`obsoletes`** | passive supersession | removed | **persist** (an obsolete unit stays on the map); the target decides its own fate |
| **`replaces`** | succession **removal** — a superseder removes the predecessor (`replacedBy`; e.g. a unit's `SupersedingUnits`) | removed | dropped from buildable once the superseder is itself buildable; the *building* `ReplacementBuildings` is instead *dormancy* (`requires.operate.dormant`, §3) |

So **`CAN GET = union(enables) − (disables ∪ obsoletes ∪ replaces)`**, all over HAVE (`replaces`' one live use
is unit succession — §3).

**The tree is fully connected — `TECH_GAME_START` is the universal root.** Every entity enters CAN GET via an
inbound `enables` edge; there is **no** implicit "no-edge ⇒ always available" engine rule. Entities available
from the start of the game — the Palace's ongoing constructibility, the starter units (`UNIT_BRUTE`), the base
promotions, `PROCESS_IDLE`, the base civics — are authored onto **`TECH_GAME_START`'s `enables`** (the synthetic
start node every player holds), derived by the curator (no prereq in legacy ⇒ enabled from game start). A missing
edge therefore fails **closed** (the entity is unreachable — loud in validation), never silently-available.
The dead workarounds for this — whole-domain frontiers, hardcoded always-available whitelists — are tombstoned
([superseded-ideas #14](../architecture/superseded-ideas.md)). *(The Palace's FIRST placement is not the
enabler's doing: the settler's `grants.foundBuildings` places it at founding — the grants machine,
[json](json.md) §5.)*

**`TECH_GAME_START` is guaranteed into HAVE at load.** It is a newly-added concept — a tech that will **never be
in the tech tree** — so: **new games hold it by default; an existing save does not** and gets it **added on save
load if absent** (the backfill that keeps an old save's generation rooted). Without it, generation on a
pre-concept save produces an empty tree. **This backfill is the ONLY engine special case the root model needs**
— everything else is pure data + the generic machine.

> **`replaces` is the UNIT succession edge; building "replacement" is dormancy (engine-verified).**
> A unit's `SupersedingUnits` ARE genuine removal-on-succession (the engine's `isSupersedingUnitAvailable` drops the
> predecessor once a superseder is buildable) → modeled as the unit's `replacedBy.units` replace edge (§ units, below).
> The legacy *building* `ReplacementBuildings` (A lists the buildings that supersede it) *looks* like removal, but the engine only
> **disables** A while the successor is present (`setDisabledBuilding`, CvCity.cpp:14413) and re-enables it when the
> successor is gone — reversible **dormancy**, never removed. So it is mirrored as the **target's
> `requires.operate.dormant: [successor]`** (§3) and leaves CAN-GET membership untouched — *not* a `replaces` edge.
> This unifies the education ladders (a lower band dorms while a higher is present = only-highest-active) with the
> pollution effects (blackened-skies dorms the observatory). `replaces` stays a defined family member for a future
> genuine-removal source; there is none today.

**`obsoletes` vs `disables` kept SEPARATE for clear semantics** (progress-supersedes vs policy-forbids) + the
pedia line ("Obsoleted by [tech]"). `disables` = a hard "be gone" (the source commands; the target gets no say);
`obsoletes` = a soft signal — the instance's fate is authored on the TARGET via `whenObsolete`, a **separate full
modifier tree** applied while obsolete ([json](json.md) §4.2): empty ⇒ the building is fully gone; non-empty ⇒ its
normal families stop and this tree applies instead (wonders/walls keep culture/tourism, most buildings vanish).

**The fixed run order — `replaces` → `disables` → `enables` → `obsoletes`.** The *membership* of CAN GET is
order-independent (set-difference is commutative); the fixed order matters only for two **propagation** effects:
a possession change (if a law `disables` a building, anything that building `enables` must also drop) and
instance-fate precedence (`replaces` wins over `obsoletes`). So: collapse succession chains, drop
banned/destroyed things from HAVE, *then* generate from the corrected HAVE, *then* prune obsoleted candidates.
**Tech is authored in `enables`** (a tech `enables` what it unlocks) — never as a generation driver in `requires`.

**`disables` — the worked case.** The lone law-disable today is the per-civ Neanderthal research ban
(`TECH_SEDENTARY_LIFESTYLE`, reversible — bars the tech while active). **NB the blackened-skies → observatory case
is NOT a `disables`:** blackened skies don't nuke the observatory from orbit — it goes **dormant** and wakes when the
skies clear, so the *observatory* carries `requires.operate.dormant: BLACKENED_SKIES` (§3). (`BLACKENED_SKIES` is
itself a tech-created pseudobuilding, dormant via its own air-pollution band until pollution gets *really* bad — only
then does it dorm the observatory.) Dormancy is always the **target's `requires.operate.dormant`**, never a source
`disables` (the disease-band / rat-catcher case is the same shape).

**Multi-parent tech.** A child tech carries `requires.build.all:[T1,T2]` (AND) or `.any:[T1,T2]` (OR — a plain `||`
over its members, [json](json.md) §3.4; NOT a list-of-groups). `enables` proposes
the child from one parent, `requires.build` confirms all parents forward from HAVE. The curator must RETAIN
`AndPreReqs`/`OrPreReqs` as `requires.build.all`/`.any` **because the store's prereq-inversion flattens them
into other techs' `enables` for generation and does not keep them on the child** — so the curator re-reads them
off the child. `requires.build` only —
techs are monotonic, no `operate`.

> **Reverse-mapping the forward compat views (owner standing direction — "reverse-map everything on load").** The
> store's prereq-inversion is *for the GENERATE pass* (it flattens each entity's prereqs into the prereq entity's
> `enables`). But many legacy consumers still read the **forward** view off the child — a route's `getPrereqBonus`
> (the `CvPlot` build gate), a trait's `getPrereqTrait`, a tech's `leadsTo`. These are **reconstructed AT LOAD from
> the inverted `enables`** (never stubbed, never re-authored on the child): the tech `leadsTo` + trait prereqs in
> `CvGlobals::doPostLoadCaching`, the route bonus prereqs in `CvCascadeReadJson`. **The inversion must keep AND vs OR
> in DISTINCT buckets** or the reverse map loses the distinction — a single AND prereq inverts to its own bucket
> (`enables.routesAnd` / `enables.traitsAnd`), the OR-list to another (`enables.routes` / `enables.traitsOr`), and
> the load pass rebuilds each forward getter separately. *(The tech case reconstructs from the child's retained
> `requires.build.all`/`.any` instead — same goal, the two reconstruction sources.)*

**Empire/team-scope constructables need NO new machinery** (the scope spine already has team/empire): stage-gates
via `enables` (the space line), doctrine bans via `disables` + empire modifiers — replacing the `FreeBuilding`
autobuild clunk (~345 uses) with one empire-scope building. (INTERIM — a later issue; the per-city machinery
still works.)

> **The two fates are two mechanisms — nothing to declare.** `disables` = **destroy** (a law/ban
> removes it; rebuilt on repeal); the target's `requires.operate.dormant` = **dormant** (it stays put,
> inactive while the condition holds — §3). There is no flag on `disables`: the choice of *mechanism* IS the fate.

---

## 3. Pass 2 — GATE each candidate (`requires`)

`requires` answers *"do I have the means?"* — checked **forward** (is this atom in HAVE?). It is authored on the
**target**, in two timings ([json](json.md) §4.3):

- **`build`** — needed to construct; **greys** the candidate if missing. Checked once, at build.
- **`operate`** — needed to construct **and** to keep running; re-checked every recompute. Lose it after
  building and the thing goes **dormant** — inactive, not destroyed — and wakes when the condition returns.
  (Units carry `build` only; they're leaf actions that exit the model once built.)

So the build-time gate = `build ∧ operate`; the ongoing dormancy gate = `operate` only. A `noneOf` clause is the
**dormancy trigger** `requires.operate.dormant: X` ("dormant *while* X is present") — distinct from a source-side `disables` ban by fate
(dormant-and-reversible vs destroyed-and-rebuilt) and author (the target vs the law).

**Pseudobuilding bands.** Legacy `CvPropertyInfo` `iMinValue`/`iMaxValue`/`BuildingType` + `checkPropertyBuildings`
(each turn) adds/removes a building as the property value enters/leaves the band. End-state: retire the per-turn
churn — model the band as uniform `requires.operate` dormancy (enabled once; toggles active/dormant); `notConstructible`
(an `identity` flag, [json](json.md) §7) is the **interim** until the `requires.operate` dormancy end-state lands.
Where the bands form a succession chain (the **Education ladder**) a higher band dorms the lower via
`requires.operate.dormant` (only-highest-active, no stacking) — the **same uniform `ReplacementBuildings → dormant`
mirror as §2, not a special case** (there is no separate "education" ruling); chainless bands (crime/disease/
pollution) compound, every in-band band active. Bands are **bidirectional** — effect-buildings can spawn on the
**negative** side, not just the positive ladder; a negative band is being considered for **every property**.

**`requires.operate` on a UNIT** (FUTURE — e.g. tanks need fuel) would reversibly disable an existing unit while
it stays on the map; the structure supports it, but it is not modelled now — **units carry `build` only** (a trained
unit never goes dormant on resource loss, and on-map behaviour is out of the cascade's `canTrain` scope).

**Units reuse this whole machine — only the inputs differ (verified to full `canTrain`
parity).** `canTrain` is the same generate-then-gate over unit inputs: frontier (every unit) → prune
`obsoletedBy.techs` (the target-side obsoleting tech, mirroring buildings; an obsolete unit leaves the buildable set
but persists on the map, upgradeable) → exclude `identity.spawnOnly` (never-trainable; building/farm-improvement/
vassalage-granted only) → the `allowed` instance cap (`world` = lifetime-created, `empire` = live count *era-scaled
for a base of 5*; units have no `team` cap) → `requires.build` via the **same** condition evaluator. The two upgrade
relationships are **distinct gates, mirroring the engine** (`build`/`operate` share the conditional vocabulary):
- **`UnitUpgrades` → `requires.build.dormant.all`** = the unit's *direct* upgrades **minus** any that are also
  superseders. The cascade recurses these engine-side (mirrors `allUpgradesAvailable`): hide the unit only when
  **every** such upgrade resolves to a reachable-trainable unit (one dead branch keeps it buildable). The named
  `dormant` clause is fail-safe (default *not*-dormant). *(This recursion — `uc_reachable`, the StoneBase
  `UnitCascade.Reachable` closure — is what resolves the whole upgrade TREE: chains, obsolete intermediates, cycles.
  It is the spec'd resolver; do NOT replace it with a one-level or hand-rolled scheme.)*
- **`SupersedingUnits` → the `replaces` edge (`replacedBy.units`, §2)** = genuine **removal-on-succession**: the unit
  drops from buildable the moment any superseder is itself buildable (mirrors `isSupersedingUnitAvailable`). The engine
  SKIPS superseders in `allUpgradesAvailable`, so they live here, not in the dormancy gate. This is the first real use
  of the long-reserved `replaces` family. **The enabler reads the curated TARGET-side `replacedBy.units`** (each unit's
  own superseders), never the source-side `replaces.units` (which nothing authors).

Other gates fold into `requires.build` as **declarative conditions** (no engine special-case, modder-extensible):
**game options** → the **ENTITY-LEVEL `enabled`/`disabled` gate** ([DEC-entity-gate] — e.g. the inquisitor's
`"enabled": "GAMEOPTION_RELIGION_INQUISITIONS"`),
evaluated live against the active options; `requires` holds only genuine needs; a **unit** corp prereq →
`{HAS_CORPORATION: X}` = **active** (`isActiveCorporation`), distinct from a building's bare `CORPORATION_` = present.
No `canTrain` gate logic is re-mirrored from the engine — every divergence is a missing input mapped to its named source.

**VICINITY** (enabler-specific) = the city's current workable radius, which **grows with culture** (1→2→3 rings),
NOT fixed; a plot can lie in two overlapping cities' vicinity (counts for both). The plot scan carries a
**city-relative semantic** (`VICINITY ⊇ WORKABLE ⊇ IS_WORKED`, [json](json.md) §3.5): `VICINITY` = in the radius;
`WORKABLE` = in radius **and owned/eligible-to-work**; `IS_WORKED` = a citizen works it. The engine's gates pick the
level — `isValidTerrainForBuildings` requires an **owned** plot for terrain/improvement/peak/hill (= `WORKABLE`;
a FEATURE prereq also accepts a neutral plot unless `EXP_STRICT_VICINITY`), and `hasVicinityBonus` requires the
bonus **owned + valid + connected** (the obtained semantic) or supplied by an active building.

### 3.1 The cache-friendly two-stage evaluation

Every `requires` resolves the same way, so it's cacheable as a pure function of clause-shape + state:

1. **combinator** — the `all`/`any`/`noneOf` structure ([json](json.md) §3.4): **`all` = AND** (`&&`), **`any` = OR**
   (`||`), **`noneOf` = NONE**, each over its **direct children** (a leaf, or a nested `all`/`any`/`noneOf` node — a
   recursive boolean tree). Parsing routes through the ONE typed-condition parser (`cascadeParseCondition` →
   `CvJsonCondition`, the StoneBase `ConditionParser` port) and evaluation through the ONE evaluator
   (`cascadeEvalCondition`) — never reinvent and/or ([superseded-ideas](../architecture/superseded-ideas.md) #5:
   the AND-of-ORs `any:[[…]]` shape and hand-rolled `vector<vector<leaf>>` were exactly that mistake).
2. **conditions** — each leaf: a presence/count **atom** (`min`/`max` at a scope) or a **predicate**. A count at
   `city`/`plot` reads the live object; at `empire`/`team`/`world` it reads the [tally](tally.md). A missing
   predicate is **ignored**, never false (json §3.5) — so retiring a system never spuriously disables data.
   **Tally-bucket routing is by TYPE PREFIX** (`BUILDING_`/`UNIT_`/`BONUS_`/…), no separate `kind` field; author
   resource presence as `min(BONUS_X,1)` (the N=1 case) — volumetric-ready.

### 3.2 The operating-building set — what the modifier reads

As a byproduct of the dormancy gate, the enabler maintains, per city, the **operating-building set**: the
buildings that are present **and operating** (`requires.operate` holds ∧ no dormant-trigger successor present),
plus the **bonuses those operating buildings supply in-vicinity** (`provides.bonuses`, [json](json.md) §5a). The
two form one **least-fixpoint** — an operating building's `operate` can consume a bonus another operating building
provides, so an operating/dormant flip ripples.

This set is the enabler's output the **[modifier](modifier.md) reads to decide which buildings deposit**: an
operating building contributes its modifiers, a dormant one contributes nothing. It is the built-instance
counterpart of the frontier (§2 — the frontier is "what can I build"; this is "of what I've built, what is
operating right now").

It is **maintained by targeted propagation, never a blanket recompute**: computed once at load, then each
HAVE-change ripples only the affected buildings into the authoritative set in place (via an operate reverse-index)
— see [state-repositories](../architecture/state-repositories.md) and
[enabler-frontier-perf](../plans/structural-cleanup/enabler-frontier-perf.md) Stage 2. In code it is
`CvCity::m_operatingBuildings` (type **`OperatingBuildings`** — its `active` + `provided` + `obsolete` sets), read via
`EnablerKernel::operatingBuildings` / `wireOperatingBuildings`; the full recompute
`recomputeOperatingBuildingsInto` is the load seed and the validation oracle. *(Historical note: this was the
undefined internal name "facts"; it is the operating-building set.)*

**Obsolescence is the THIRD outcome of this same pass.** A present building whose `obsoletedBy` tech is held is
neither active nor dormant — it goes into the `obsolete` set (excluded from `active`, provides nothing), and the
[modifier](modifier.md) reads its **`whenObsolete`** tree (§2 / [json](json.md) §4.2) in place of its normal
families. It is maintained by the same targeted propagation (an `obsoletedBy.techs` reverse-index re-checked on a
tech change), read via `cascadeIsBuildingObsolete`. Inert while legacy still swaps an obsolete building out of the
city; live at the swap cut, when an obsolete building STAYS present.

---

## 4. The `allowed` cap

`allowed` ([json](json.md) §4.4) is a separate gate from `requires` — "how many of **me** may exist," not "what
I need." A build is permitted while **`count(me, scope) < allowed`**; the count comes from the [tally](tally.md).
The engine owns ignoring caps under game options / era-scaling — the machine just compares.

---

## 5. The load-bearing asymmetry — bidirectional, not down-only

The cascade is **bidirectional**: generation flows down from sources, but the `requires` gate resolves by a
**callback UP the scope chain** — a city-scope candidate asks its empire/team/world about civics, counts, state
religion. This is **how the model expresses AND** (every clause must hold, possibly at different scopes), and it
is **not optional**:

A pure down-only design (sources push everything onto targets) was tried and abandoned — it can model **OR**
(many sources enable one thing) but **cannot reliably model AND**, and it forces a modder to maintain every
requirement at the top of the chain. The upward `require` callback is load-bearing. Do not "simplify" it back to
down-only.

---

## 6. Greying — the build-list tri-state falls out for free

The same gate that decides buildability yields **why** a thing isn't buildable — no separate "why greyed" pass.
Each clause carries a disposition (set once by its kind):

| state | condition |
|---|---|
| **HIDDEN** | not in CAN GET — generation never reached it (or it was obsoleted/replaced/banned away) |
| **LISTED** (buildable) | in CAN GET ∧ all `requires` met ∧ under `allowed` |
| **GREYED** | in CAN GET ∧ only *greyable* clauses unmet — a connectable resource, an unadopted civic (named to the player) |

Grey vs hide is a **UI choice per clause**, not engine behaviour: author a resource on `requires` to **grey**
(surfacing "go get copper"), or on `enables` to **hide** until present. General lean: grey on resources.

**The frontier is one shared choice set — UI *and* AI.** It is computed once per recompute; the UI greys from
it, and the AI's production decision iterates **only this small frontier** instead of scoring the whole entity
database. That consolidation — one recompute replacing dozens of scattered ad-hoc `canBuild` checks — is the
biggest systemic win.

---

## 7. Recompute cadence + the runtime realization — event-maintained vectors over `f(HAVE)`

**What is recomputed on demand is the FRONTIER — never the entire enabler.** The frontier is a pure function of
HAVE — conditional-free set algebra (§1) — so it is **recomputed when HAVE changes**, not cached with deltas. The
dominant cadence is once per turn, but same-turn HAVE-changes must trigger a mid-turn recompute (the AI finishes
building A then builds B the same turn; religion spreads; a bonus connects; a city is conquered). This stays
cheap: the bounded two-pass over the affected scope is *less* work than the scattered legacy checks it replaces,
which already re-scan the whole database constantly. Any caching is a separate optimization layer wrapped around
the pure `HAVE → frontier` function, never leaking into the model.

**The runtime realization (LOCKED) — a CONSTANTLY-UPDATED VECTOR, not recompute-on-read.** The
`canConstruct` / `canResearch` / `canTrain` / … lists are **stored vectors the ENABLER OWNS**, built **once** at
load by the **reseed events** (the in-read emits stream through the same appliers as play,
[DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed) — never a warm-up walk beside the event
stream) and **updated in place on events** (a tech researched adds its `enables` / removes its
obsoletes; a building built leaves `buildable`; …). Every read is a **pure O(1) lookup that NEVER calls a
calculator.** The static calculators (`TechEnabler::available` / `BuildingEnabler::verifyCity`'s fresh-build +
`EnablerKernel::gateSet`) are the **validation oracle ONLY — never the read path and never a load build**; the
oracle-vs-maintained diff is the missed-emit tripwire (the enabler consumes ONLY events precisely so a missed
emit surfaces as a visibly wrong enabler). The `requires` gate re-runs **incrementally over only the affected candidates** (via the reverse
index), and the operating-building set (§3.2) is maintained the same way — this is
[state-repositories](../architecture/state-repositories.md)' targeted propagation applied to the availability
machine. The representation is deliberately primitive: **the HAS list, and the enabler list built from HAS, are
literally TWO SETS OF INTS (enum ids)** — set algebra over int sets, nothing richer.

**Per-scope instantiation — EACH CITY owns its OWN enabler object (buildings + units).** The
buildable/trainable lists are per-city derived state, so every `CvCity` carries its own enabler object — exactly
as it carries its package set — and the player carries the player-domain lists (researchable / adoptable /
hurries / …). It is **ONE unified enabler component**, instantiated per scope owner and fed by the eventspine
consumer — a **SIBLING of `CvDerivedCache`, which CANNOT operate the same way**
([state-repositories](../architecture/state-repositories.md): the two distinct kinds of derived cache). A value
cache recomputes on dirty; the enabler **fundamentally behaves differently: the CAN-HAVE set is built PURELY on
the events of ALREADY-HAS** — each HAVE-event applies its `enables`/removal edges in place, the load reseed's
events are the one full build, and no dirty→recompute path exists at all. A component's `requires` gate resolves
cross-scope atoms by reading its parent scope's state up the chain (§5's upward callback, realized).

**HAVE is NOT a new store — the enabler ties directly into the object-owned has-lists that ALREADY EXIST** (the
city's buildings-present / religions / corporations, the player's civics / traits / heritages, the team's
techs). The object owns its presence state — the [tally](tally.md) rule ("let an object care about itself")
applied to presence — the DOMAIN event carries the delta that triggers the in-place list update, and the enabler
stores only what it **derives** (the lists + the operating-building set). Predicates/atoms keep reading that raw
object-owned state; what is event-driven is the **maintenance** (which dependents re-gate, when), never a
read-side recompute.

**Event-fed, the end-state:** the enabler's derived sets — the **domain lists**, the **operating-building set**
— are built by the **load reseed** (the in-read DOMAIN events populate them,
[DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed)) and **maintained incrementally by play-time
events** (building built → the city's lists re-gate its dependents; tech researched → its `enables`/`obsoletes`
edges apply; bonus network shift → operate re-check) — never re-reading live game objects wholesale and never a
per-turn blanket re-check. Exactly the modifier caches' model, applied to the "can I?" machine.

**Mid-turn HAVE-change triggers** also include **inquisition** (which retracts a RELIGION, not just a building —
disproving "buildings-only" state-retraction), nuke, and `doAutobuild` add/remove.

**Gather order — "right-then-down".** Pass 1 gathers in dependency order: sticky top (techs/civics) first, then
volatile bottom (resources/bonuses/buildings), so derived have-entries resolve against what's already gathered.

**Game-option gates are the ENTITY-LEVEL `enabled`/`disabled` pair, evaluated LIVE ([DEC-entity-gate]).**
The legacy engine checks the option tags at USE time, and the gate mirrors that: an entity whose `enabled` fails (or
`disabled` holds) is simply never offered/valid while the option state says so. LOAD-STABLE machinery that genuinely
resolves at load (the `CvInfoReplacements` swap, WorldBuilder/BUG, a per-civ research ban) is engine-side, not
entity data.

### 7.1 The concrete structure + the delta algorithm

**Storage — one per-domain TRI-STATE ARRAY per owner** (semantically the two int-sets of §7; physically flatter):
`state[id] ∈ {HIDDEN, GREYED, LISTED}`, a byte-array indexed by enum id, one per domain on its owner (city:
buildings, units; player: techs, civics, projects, processes). **Hurries are NOT an enabler domain** — whether
a hurry type is usable is a civic-enacted ability (the capabilities/policies side, [capabilities.md](capabilities.md));
the city `canHurry` gold/population/progress arithmetic is a live stats check. Neither half is this machine's.
**The owner is where the domain's HAVE
axes live, NOT where the gate is asked:** projects/processes are chosen and built on the CITY's production list
(`canCreate`/`canMaintain` — a project builds exactly like a unit/building/wonder, one city queue with a
team-wide effect; the engine's apparent multi-city project production does not actually work), but their axes
are team-scope, so the domain is PLAYER-held — per-city copies would be byte-identical duplicated state that
must never drift — and the city gate reads through its owner (a dynamic `getOwner()` lookup, conquest-safe;
never a stored pointer). The one city-local project fact (the plot map-category gate) stays a live check at
the gate, the same split as worker builds below. CAN GET = `state ≥ GREYED`; the
gate-passed set = `LISTED`; §6's tri-state IS the array. Chosen over two `std::set<int>`s deliberately: O(1)
reads on the AI's hottest gates (vs O(log n) + ~20 B/entry tree-node overhead), O(delta) writes, ~8.5 KB per city
for both big domains, and frontier iteration is a linear byte scan. The **only mutable state is these arrays**
(plus the operating-building set §3.2); the reverse indices are static load-compiled data; **nothing serializes**
— the load reseed is the one full build.

**The delta algorithm — per HAVE-event H, everything O(delta):**

1. **Generation — membership is the FORMULA, never the operation sequence.** A candidate is in CAN GET iff
   `(≥1 held source enables it) ∧ (0 held sources remove it)`, maintained as **two per-candidate refcounts**:
   H's `enables.<bucket>` entries increment their candidates' enable-count; H's
   `obsoletes`/`disables`/`replaces` edges increment the remove-count; a **lost** source decrements (civic
   swaps, bonus disconnects). Membership = `enableCount > 0 && removeCount == 0` — **REMOVAL WINS regardless of
   arrival order**. ⛔ The naive sequenced add/erase delta ("insert on enables, erase on removes") is BANNED: an
   enables-add arriving after an obsoletes-remove re-inserts the candidate (the `TECH_GAME_START`-arrives-last /
   obsoleted-`UNIT_BRUTE` edge case — the remove was a no-op on the absent element, then the late add resurrects
   it). Same refcount shape as the operating-building set's provided-bonus counts. Entering CAN GET gates
   **once** (→ GREYED or LISTED); leaving → HIDDEN, with §2's instance-fate side effects.
2. **Re-gate:** the requires-reverse-index (HAVE-atom id → dependent candidate ids) names the in-tree
   candidates whose `requires` references H; **only those** re-evaluate, flipping GREYED↔LISTED. Its canonical
   home is **`EDGEF_REQUIRED_BY` on the referenced info**, populated by the readJson reverse pass
   ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)) — never a bespoke side index
   inside an enabler.
3. **Caps / queue / built:** a count event re-checks `allowed` for that one type; queueing/completion is the
   targeted single-id erase. The leave-rules differ per domain: a **building** leaves the frontier when built; a
   **unit** stays trainable (it leaves only on a cap or supersession).
4. **Operate ripple:** operate-atoms referencing H drive the operating-building work-list fixpoint (§3.2).

**⛔ ORDER-INDEPENDENCE is a HARD INVARIANT of the delta algorithm.** Events are facts, not causal steps
([event-spine](event-spine.md)) — the sets must converge to the same content whatever order the events arrive
in (`TECH_GAME_START` last, first, or anywhere). The algorithm guarantees it because every piece is commutative:
generation is the **refcounted membership formula** (step 1 — removal wins; sequenced add/erase is banned);
gating is gate-on-entry *against current state* + re-gate via the reverse index when a referenced atom later
changes. Three implementation failure modes are therefore BANNED: (a) any ordering assumption in the delta
("parents before children" — prerequisite logic belongs only in `requires`, which re-gates); (b) the sequenced
add/erase membership delta (step 1's edge case); (c) a load reseed that gates-on-entry against half-built state
while SKIPPING re-gates during the load window — during the reseed either every event's re-gates apply as they
arrive, or gating runs once after the stream ends; both are correct, the mix is the bug.

**Two deliberate maintained-set EXCEPTIONS (efficiency — maintain only where reads are hot and the owner-space
is small):** **promotions** keep no per-unit maintained sets (thousands of units × hundreds of promotions,
churned on every tech, for a decision that only happens at level-up) — the player maintains one
unlocked-promotions set and `canAcquirePromotion` evaluates on demand at level-up; **worker builds** — the player
maintains the unlocked-builds set, and the plot-validity half stays a live per-plot gate (a maintained set over
~10k plots is waste; worker decisions already iterate plots).

---

## 8. Build state — the event-fed rework (transient: gaps + open forks)

> **Project-specific build state** — where the code stands against §7, kept on this one enabler surface (no
> separate plan doc, owner ruling). Companions: the modifier-side event work
> ([f0-eventspine-invalidation](../plans/structural-cleanup/f0-eventspine-invalidation.md),
> [scope-packages](../plans/structural-cleanup/scope-packages.md)), the reverse-index/perf build
> ([enabler-frontier-perf](../plans/structural-cleanup/enabler-frontier-perf.md)).

### The current code violates §7 (it reads live state; events only TRIGGER re-reads)

- **Recompute-on-read.** `CascadeAccumulator::enConstruct`/`enResearch` do `ensure(CPK_FRONT_B)`/`ensure(PSC_FRONT_P)`,
  which on a dirty bit calls the **static calculator** to rebuild the WHOLE set. Events keep marking it dirty, so it
  recomputes-from-scratch constantly — a memo in front of a full recompute, not a maintained vector.
- **Only half event-driven.** Targeted propagation (`recheckHave`/`onBuildingChanged`) covers only
  bonus/religion/corp/pop/power; **tech/civic/golden-age** fall back to the broad `markPlayerScopeAndCities` blanket
  → full recompute on next read.
- **Wrong home.** The frontier data + read accessors live on the **modifier cascade** (`m_cascadeCityPackages` /
  `m_cascadePlayerScope`, `CascadeAccumulator::en*`) — the enabler must own them on its OWN surface.
- **How it sources data today:** `EnablerKernel::generate` pulls HAVE straight off `GET_TEAM`/`GET_PLAYER`/`CvCity`;
  the gate's predicates (`hasTech`/`hasBonus`/`isCapital`/…) are live game-state queries via the shared
  `cascadeEvalCondition`; `recomputeOperatingBuildingsInto` re-reads `city.getHasBuildings` each recompute; counts
  read the [tally](tally.md), which reads the object-owned counts (by design).

### The fix (the real enabler-event work) — ✅ REALIZED FOR ALL EIGHT DOMAINS (city: buildings, units; player: techs, civics, projects, processes, builds, promotions)

> **⛔ THE PHASE'S ONLY BAR (owner): every availability surface on the ONE enabler structure** (`EnablerDomain`
> on its scope owner). Parity/diff checks have NO meaning in this phase (validation.md: parity is closed; the
> enable-side is deliberately over-inclusive, so there is nothing to mismatch against) — do not build or cite
> them. The shadow-pass whole-set machinery (`buildable`/`trainable`, `bc_`/`uc_isBuildable`, the
> `s_bc*`/`s_uc*` buckets, the `CPK_FRONT_*` box fills) was SCAFFOLDING from the closed parity era — never a
> pattern to build on (it was the vehicle of the live-compute rollerskating). The whole CITY box is DELETED
> (the `CPK_FRONT_B/U/PP` fills, accessors, members and bits); `CvBuildingEnabler`/`CvUnitEnabler` are now only
> their domains' seed + event-delta calculators (+ the shared `augmentWaived`).

**The BUILDINGS domain runs per-city on the component** (`CvCity::m_enabler`, a `CityEnabler`): the §7.1 shape
exactly — the domain arrays are the ONLY mutable state, and every HAVE-event applies its source's building
edges (`enables` → the enable plane; `obsoletes`/`replaces`/`disables` → the remove plane) as a DIRECT ±1
delta. The delta comes from the EVENT (flip-guarded emits carry `has`; the bonus emit carries the count delta,
applied on the 0-crossing; the civic and culture-level emits carry the swapped-out value — the swap facts were
added to the emit surface, never worked around with side state); the one broad emit (tech) is flip-guarded by
the PLAYER tech domain's held flag, so the building tech-delta runs BEFORE `TechEnabler::onTechChanged` in the
spine route (the ordering contract), and carries the obsolete-present ripple (a present building this tech
obsoletes stops/resumes enabling, guarded by the other-held-obsoleting-techs counterfactual). At load the SAME
appliers consume the reseed's in-read emits — one mechanism, no seed walk. Building obsoletion is authored
target-side, so the tech-side forward view (`tech.obsoletes.buildings`) is reconstructed by the readJson
reverse pass. **The standing verification is `/computed/enabler/buildings?player=N&city=M`** — the maintained
vector diffed against a fresh seed from current state (0 mismatches = the event maintenance is exact). **The read is FLIPPED (owner ruling
2026-07-14): nothing that needs the enabler reads legacy — that is the premise; otherwise we do not see what
is broken.** `canConstruct`'s gate verdict is the owner's BARE member read (`m_enabler.buildings.listed`) —
per §7 a static is a PURE CALCULATOR (seed / delta / oracle) and NEVER appears on a live read path (no read
accessor, no lazy-seed guard; seeding is a lifecycle act — player/city init + the load warm-up). The set is
deliberately OVER-INCLUSIVE until the requires gate + `allowed` caps land on the component — a wrong offer is
a VISIBLE enabler defect to fix, never a reason to fall back (the rollerskating this cures was the opposite
mechanism: a read path that live-calculated EVERYTHING per call with no cache). **The gates are PURE enabler
reads — NO legacy fallback, NO pre-init guard, NO what-if legacy path; the `*Legacy` gate bodies are DELETED.**
Legacy cannot even RUN post-ship (its XML is being removed — [DEC-red-ratchet](../architecture/decisions.md#dec-red-ratchet)),
so a fallback is not a safety net but **BAIT**: it substitutes an answer that will not exist and MASKS the very
cascade hole we need exposed. The what-if callers (`bContinue`/`bIgnore*`/`bTestVisible`/probability) now receive
the plain frontier verdict — the enabler models no hypothetical, so a what-if that needs one is itself a hole to
fix on the component, never a legacy read ([DEC-no-legacy-masking](../architecture/decisions.md#dec-no-legacy-masking)).

The standardized component: **`Sources/Cascade/CvEnabler.{h,cpp}`** (`EnablerDomain` — the §7.1 tri-state
array + the two membership refcount planes + the removal-wins formula), instantiated per §7.1's owners:
`PlayerEnabler` (`CvPlayer::m_enabler`: techs, civics, projects, processes — the §7.1 owner-is-where-the-axes-
live rule; the city `canCreate`/`canMaintain` gates read through `getOwner()`) and `CityEnabler`
(`CvCity::m_enabler`: buildings, units). Every domain runs
the same end-to-end shape: a lifecycle `initDomain`/`onCityCreated` (sizing + static exclusions, NO content —
at the owner's init AND at the start of its save read, before the in-read reseed emits stream; the city-created
applier additionally folds the one non-event input, the cross-scope HAVE that predates the city — team techs +
player civics); content built PURELY from DOMAIN events, the load reseed's in-read emits and the play-time
emits through the SAME O(delta) appliers ([DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed) —
never a warm-up seed walk beside the event stream), routed by the spine consumer (`CvCascadeInvalidation.cpp`
— the per-domain
enablers `TechEnabler`/`CivicEnabler`/`BuildingEnabler`/`UnitEnabler`/`ProjectEnabler`/`ProcessEnabler`, all
through the ONE `EnablerKernel::applyEdges`); and a bare O(1) member read behind the gate
(`canResearch`/`canDoCivics`/`canConstruct`/`canTrain`/`canCreate`/`canMaintain`). The project emit is
PER-MEMBER (one per alive team member — the tech-emit precedent; the applier scopes to the emitting player, so
load and play are exactly-once per player). Per-domain HAVE axes: techs
+ civics ← team techs (incl. the root); buildings ← techs/buildings/civics/religions/corporations/bonuses/
culture-level (+ `notConstructible` → staticExcluded, present-building held flags, the obsolete-present
ripple); units ← techs/buildings/civics/religions/bonuses (+ `spawnOnly` → staticExcluded; trained units stay
listed); projects ← team techs + team completed projects (the Apollo chain, the count 0-crossing); processes ←
team techs only; builds ← team techs only (`enables.builds` → the enable plane, `obsoletes.builds` → the
remove plane — the §7.1 player unlocked-builds set; `canBuild`'s plot-validity half + the `isDisabled` runtime
toggle stay live checks at the gate); promotions ← team techs only (the §7.1 player unlocked-promotions set —
the per-unit level-up gate `enPromotionValid` OVERLAYS the unit's held-promo/unitcombat planes on the domain's
planes via the raw plane reads, membership = Σenable > 0 ∧ Σremove == 0; the old "no tech edge ⇒
always-unlocked" whitelist is dead, superseded-ideas #14 — start promotions ride the root's
`enables.promotions`). The old recompute paths are deleted (`enResearch`/`enResearchable`,
`enConstruct`/`enTrain`/`enCreate`/`enMaintain` + the whole `CPK_FRONT_B/U/PP` city-box slice,
`enBuildUnlocked`/`enBuildUnlockedFast` + the `enBuildRem` rem-set, the `PSC_FRONT_PROMO` tech-halves fill,
`unitCountChanged`, `canResearchLegacy`).

1. ✅ The enabler OWNS its frontier vectors (all six domains).
2. ✅ **Built once at load by the RESEED EVENTS** — the in-read emits consumed through the play-time appliers
   (no warm-up seed walk exists).
3. ✅ **Iterative update on EVERY HAVE-event** (the `markPlayerScopeAndCities` blanket no longer feeds any
   availability list — it remains only as the modifier-package conditioner mark).
4. ✅ Reads are **bare lookups** (all six gates).

**The enable side is COMPLETE across every enabler surface** — no availability list remains off the component.
(Hurries are deliberately absent — not an enabler concern, §7.1; the box's `enHurryOk` slice serves `canHurry`
until the civic-ability model consumes it.)

> **The consumer ITERATION sweep (F2b) — the CLEAN subset is done.** The hot AI decision loops whose gate is the
> plain-verdict `can*` (default args) now iterate the enabler's LISTED frontier via `EnablerDomain::listedIds`
> instead of scanning the whole entity database (`AI_bestBuildingsThreshold`, `CalculateAllBuildingValues`,
> `AI_bestUnitAI`/`AI_bestProject`/`AI_bestProcess`, the spread/inquisitor/promotion-value loops, the civic
> counts, the capital unit-analysis + event-value loops, …). What remains is NOT a frontier swap: a loop that
> calls a gate with WHAT-IF args cannot iterate the frontier (it answers only the current verdict — and with the
> `*Legacy` bodies now deleted, that caller receives the plain verdict, exposed), and the `AI_chooseProduction`
> focus-ladder collapse (§F2b of the [roadmap](../plans/structural-cleanup/roadmap.md)) is an AI-architecture
> change, not a per-loop rewrite.

**The BONUS axis is GATE-ONLY (owner ruling 2026-07-15): a plot-group-carried bonus NEVER drives tree
membership.** A bonus you would rely on the trade network for (traded / manufactured / vicinity-supplied) only
ever gates: the curator keeps authoring the bonus `enables` edges (the reverse-mapped view of the target's
retained `requires` atom — both views exist in the data), but the runtime consumes bonus events as pure
stateless gate re-checks over the bonus's `EDGEF_REQUIRED_BY` dependents — no membership edge-applies, and the
oracle's fresh build folds no bonus axis. Membership rides the tech/building/civic edges + the root; an entity
whose only inbound edges are bonuses ROOTS (the curator's root derivation ignores `BONUS_` sources), sitting
visible-GREYED on its bonus requirement — json.md §6's grey-on-resources lean. The one carve-out — a bonus ON a
plot enabling an improvement's placement (`enables.builds`) — is the live per-plot gate, no domain involvement.

**RESIDENCY + COUNTING.** The `CvPlotGroup` object holds the NETWORK's bonus content
in two derived arrays — produced/extracted (vicinity-improved tiles + city `provides`) and deal-TRADED
(import/export, anchored at the capital's group; the capital-plot fold re-homes it through every merge/split) —
read as one SUM, presence crossings tested on the sum. **The plot group is the ONLY authoritative list for trade
resources, own or partner-traded; the CITY holds NO authoritative mirror of it.** The city READ, however, is a
**maintained number — added and subtracted on spine events, never calculated per read** (the standing
[state-repositories capstone](../architecture/state-repositories.md) — reads are bare fetches — applied here;
re-affirmed by the owner 2026-07-16 after the implementation shipped a per-read calculation anyway):
`CvCity::getNumBonuses` must be a bare fetch of a derived, never-serialized per-city count kept
current by the crossing fan-out (`processNumBonusChange`) + the tech/minted/corp events that move its gates.
The per-read calculation it replaces (TechCityTrade gate → two-hop plot-group resolution → group sum → minted
gate → corp add-on, re-executed on EVERY call) was measured by the EIP sampler as the turn wall's hottest
cluster under the governor's read volume. Derived-cache discipline applies: the group stays the authority, the
city count rebuilds from it at load/reconcile, and drift is a defect to surface.
VICINITY belongs to the CITY and is a plain local-presence fact ("we have it
here"): it satisfies `connection:"vicinity"` atoms and NOTHING else — it never adds a second owned count (one
pasture is ONE horse, not vicinity+network=2; count atoms always read the network side). A vicinity-improved
source — an improved tile in the radius OR an active building providing the bonus (a settled horse herd ≡ a
horse plot) — is ONE class with ONE group injection each; two counts mean two genuine sources.

**NEITHER the counts NOR the plot-group MEMBERSHIP are trusted from a save** ([state-repositories](../architecture/state-repositories.md)
no-serialized-caches — membership is derived state: routes + terrain-trade capabilities + ownership). The
deserialized groups are stream-drained and discarded; the load-end rebuild (`CvGame::onFinalInitialized`)
re-colors membership from current state and folds the counts through the live entry points as each plot joins,
announcing every bonus fact as a genuine crossing emit before the `GAME_LOAD_FINISHED` gate pass.

**The DORMANCY VERDICT is the operating-building fixpoint** (§3.2; [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)):
`CvCity::checkBuildings` applies the enabler's classification (active / operate-dormant / dormant-trigger /
obsolete) through `setDisabledBuilding` — never a hand re-derivation from legacy prereq getters — plus the two
runtime-state legs the authored data does not carry (the employed-population composition; the
banned-non-state-religion policy), which stay engine-side compositions at the consumer. The serialized disabled
flags are read only as the PROCESSED-STATE baseline (they key the still-serialized legacy effect accumulators);
the load-end cross-city fixpoint — iterate {re-fixpoint each city's operating set → apply flips → the provides
injections adjust the network} until stable — reconciles them to the computed verdict inside the load bracket
(a manufactured chain lights tier by tier: ore → wares → firearms). The iteration is WORK-LIST driven (pass 1
sweeps every city — the one legitimate full build; later passes re-check only the cities the previous pass's
flips can have touched: self, same-owner group members on a provides flip, the whole player on a free-trait
building), each flip keeps the FULL per-flip processBuilding semantics (its side-effect surface — power,
freshwater, employed population, traits, provides — is never mirrored in an overlay), and convergence is
declared ONLY by a quiet FULL verify pass — an incomplete affected-derivation surfaces as a loud verify catch,
never a silently wrong end state. The `loadPipeline` spine line carries the stage timings + flip/convergence
diagnostics. ⛔ BAKED-CONSUMER RE-RUNS: an engine consumer that BAKES state on modifier changes (the
trade-route ASSIGNMENT — `updateTradeRoutes` fires eagerly off processBuilding) runs during this fixpoint
against not-yet-warmed packages and its baked result self-heals never; every such consumer is re-run ONCE
after the load-end package warm (`CvGame::updateTradeRoutes` is the known case). That flag read retires
together with the legacy accumulator cluster at the modifier cut.

**The REQUIRES-GATE stage (GREYED) — ✅ LIVE FOR TECHS.** The component carries the gate verdict as a per-id
flag (`setGateFailed` — gate failed flips a tree member LISTED → GREYED, membership untouched; a domain whose
gate stage has not landed never sets the flag, so its members stay LISTED — the enable-side over-offer).
`TechEnabler` gates per §7.1 step 2: **gate-on-entry + re-gate over ONLY the touched candidates** of each tech
event (its `enables`/removal targets + its `EDGEF_REQUIRED_BY` tech dependents — the readJson-populated
requires-reverse-index, [DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)), evaluated
through the ONE evaluator (`EnablerKernel::requiresMet` → `cascadeEvalCondition`) against the object-owned
team techs. During the load reseed the re-gates apply per event as it arrives (the pure per-event option of
the §7.1 order rule; the gate's atoms — team techs — are final before any player reads). `canResearch` still
reads `listed`, so the gate narrows the offer with no read change. The **`allowed` cap** is part of the same
gate verdict: `EnablerKernel::allowedOk`'s tech branch reads the **engine-owned counts** (the tally
read-not-store philosophy, the PROJECT-branch precedent — world = `countKnownTechNumTeams`, ever-alive teams
holding it; techs are monotonic so held == ever-held), covering the 29 world-unique founder techs
(`allowed:{world:1}`). The cap CROSSING re-gates per §7.1 step 3: a tech event re-gates that one capped tech
on ALL seeded players' domains (the founder techs vanish from every rival's list the moment one team takes
them). **The UNITS gate is LIVE** — the parity-proven canTrain legs restored onto the component as the gate
verdict: the instance caps (world = lifetime-created + in-production `making`; empire = live tally count +
`making` vs the ERA-SCALED base-5 national cap, waived under `GAMEOPTION_NO_NATIONAL_UNIT_LIMIT` unless
`unlimitedException`), the entity-level `enabled`/`disabled` gate, `requires.build` (STRICT, the shared
AugmentState waiver + operating-buildings wiring — vicinity `provides` supply included), the SUPERSEDER
removal (`replacedBy.units`, read from the poco's superseding list — hidden the moment any superseder is
available), and the §3 upgrade-tree dormancy (`requires.build.dormant.all` via the cycle-guarded
`uc_reachable`-class closure — a unit hides only when EVERY direct upgrade is reachable-trainable).
`SEVT_UNIT_COUNT` drives the step-3 cap/relation crossing (skip-guarded for the uncapped, unreferenced,
non-upgrade common case, so combat births/deaths stay free); the no-FK classes + the bounded per-turn dynamic
re-check mirror the buildings shape. **The BUILDINGS gate is LIVE the same way**: the gate verdict = `requiresMet` (build ∧ operate, the
full city context — waived-prereq set + the standing operating-buildings wiring) ∧ `allowedOk` (the
world/team/empire self-caps through the tally's buildings domain; the per-city wonder-CATEGORY caps stay the
marked allowedOk TODO). LOAD takes the §7.1 order rule's **"gate once after the stream ends"** option — no
gate evaluations inside the load bracket (a mid-read evaluation would ensure the operating-buildings cache
against half-read state); `GAME_LOAD_FINISHED` runs one full gate pass per city. Play-time takes the pure
per-event option: gate-on-entry + touched re-gates in every applier (the FK axes via `EDGEF_REQUIRED_BY`), the
**cap crossing** re-gates a completed capped building on every seeded city (the world wonder vanishing
everywhere), the **queue leg** (§7.1 step 3) rides `SEVT_CITY_ORDER_CHANGED` (`pushOrder`/`popOrder`) into a
one-id update whose verdict reads the live queue — a QUEUED building leaves the **fresh offer**
(`listed`/`listedIds`, the `!bContinue` `getFirstBuildingOrder` exclusion) via a **separate `FLAG_QUEUED`
read-time overlay, NOT a gate-failure reason**, restored on dequeue. The overlay is split OUT of the gate so
the **continue verdict** (`canConstruct` `bContinue=true` → `EnablerDomain::listedForContinue`) reads PAST it —
an in-progress build stays valid to `canContinueProduction`; folding the queue INTO the gate (GREYED) instead
cancels every in-progress build each turn (`doCheckProduction`'s `!canContinueProduction` → `popOrder` purge, progress lost); the built case is the membership
leave — the no-FK event classes (population / power / golden-age / state-religion) re-gate their
load-compiled class lists (`EnablerKernel::scanCondDeps`), and the live non-HAVE clauses (latitude /
existedFor / IS_CAPITAL / vicinity connection / count tokens) ride the **bounded per-turn dynamic re-check**
([enabler-frontier-perf](../plans/structural-cleanup/enabler-frontier-perf.md) Stage 2 step 5) — a targeted
sweep of that small list in `CvCity::doTurn`, never a blanket. `verifyCity`'s oracle diff compares MEMBERSHIP
(the fresh build is enable-side only), keeping it the event-maintenance tripwire. The remaining domains gate
the same way, each with its own axes.

### Resolved forks (owner-ruled — now part of the §7 model)

- **HAVE model:** the enabler owns NO HAVE store — it ties directly into the object-owned has-lists that already
  exist (city buildings/religions/corps, player civics/traits/heritages, team techs). The tally stays the count
  accessor; presence stays on the objects.
- **Evaluator depth:** `cascadeEvalCondition` keeps reading raw object-owned state (legitimate live reads — the
  tally precedent). Event-driven is the MAINTENANCE (which dependents re-gate, when), never the read source.
- **Component model:** one unified enabler component, per-city instances for buildings+units, the
  delta-apply sibling of `CvDerivedCache` (§7).
- **The root rule:** no implicit engine "no-edge ⇒ available" rule — start-available entities are authored onto
  `TECH_GAME_START`'s `enables` (§2), the tree is fully connected, a missing edge fails closed. The load
  backfill of `TECH_GAME_START` itself is the ONLY engine special case.
- **Structure + algorithm:** the §7.1 tri-state arrays + O(delta) event algorithm (delegated to the
  implementation; inefficiencies get flagged, not silently absorbed).

### Build items

0. ✅ **The root node is CURATOR-DERIVED** (`curate_tech.synthesize_game_start`, the whole-set `synthesize`
   hook): every `enables` bucket (techs/civics/units/processes/buildings/builds/improvements/promotions) is
   computed mechanically from the store's no-inbound-edge rule — an entity roots iff NO inbound enables-family
   ADD edge exists anywhere, minus each kind's never-generated class filtered by the SAME sentinel that kind's
   own curator translates (unit `spawnOnly`, building `notConstructible`, tech `bDisable`, improvements no
   build produces, and a ZERO instance cap at any scope — a 0-`allowed` entity is never created by build/train,
   only granted: UNIT_BAND, the start-only settler). Nothing is hand-listed; the per-kind semantics live in the curator's derivation comment
   (the old→new map lives in the curators). Verified live via `/state/info?type=TECH_GAME_START` ≡ the
   authored JSON.
1. ✅ **The `TECH_GAME_START` load backfill** (§2) — two homes by semantic: an old save is **upgraded ON READ**
   (`CvTeam::read`, a raw flag write right after the tech array deserializes — all teams, dead ones too; every
   derived structure rebuilds from that state at load, and the reseed's per-held-tech emits announce it like any
   saved tech); a NEW game receives it as a **genuine grant** (`CvGame::onFinalInitialized`, `bNewGame`-gated,
   the full `setHasTech` — the stand-in for the unbuilt grants apply-loop's `grants.techs` row, moving there when
   grants apply). A missing `TECH_GAME_START` id fails LOUD (assert) at both sites.
2. **The dynamic operate axes ride their (now-wired) events**: connectivity via
   `SEVT_PLOTGROUP_BONUS_CHANGED`/`SEVT_CITY_NETWORK_CHANGED`, vicinity (radius growth) via
   `SEVT_CITY_CULTURE_LEVEL_CHANGED` — routed into the operate re-check of dependents. (The legacy
   `onSliceRebuildActive` bounded poll died with `playerSliceRebuild` and is NOT resurrected.)
3. Event-maintain the operating-building set incrementally (add/remove on building/have events, no re-read) —
   the frontier-perf targeted-propagation completion.
3b. **Converge the enabler's PRIVATE reverse buckets onto `EDGEF_REQUIRED_BY`**
   ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)): the `s_bc*` (lazily built,
   HAVE-atom buckets mostly built-but-dead), `s_uc*`, and operate `s_op*` static indexes inside
   `CvBuildingEnabler`/`CvUnitEnabler`/`CvEnablerKernel` are the pre-existing bespoke-reverse-view remnants —
   each retires onto the info-homed REQUIRED_BY axis as the standardized component's maintenance consumes it.
4. ✅ **The enabler sets build from the in-read reseed events**
   ([DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed)): the domains init (sizing + static
   exclusions) at each owner's read-start/init, the reseed emits stream through the play-time appliers, and the
   one emit gap it exposed (no in-read `projectChanged`) was fixed by EMITTING it (per-member, from
   `CvPlayer::read`) — the missed-emit tripwire working as designed, never a seed-walk workaround.
5. ✅ The breaking step (`canConstruct`/`canResearch`/… read ONLY the enabler) stands on the event-built enabler.

---

## See also
- [json.md](json.md) — the data this machine reads: `enables`/`obsoletes`/`replaces`/`disables` (§4.1–4.2),
  `requires` build/operate (§4.3), `allowed` (§4.4), and the `all`/`any`/`noneOf` + atom/predicate vocabulary (§3).
- [tally.md](tally.md) — the count machine the `requires` count-atoms and the `allowed` cap read at cross-city scopes.
- [modifier.md](modifier.md) — the sibling "how much?" machine. A dormant/unavailable entity (per this doc)
  simply deposits no modifiers.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids that fill the `enables` buckets and `requires` atoms.
