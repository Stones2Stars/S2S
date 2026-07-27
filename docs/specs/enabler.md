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
([superseded-ideas #18](../architecture/superseded-ideas.md)). *(The Palace's FIRST placement is not the
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
> `CvGlobals::doPostLoadCaching`, the route bonus prereqs in the general reverse pass (`Data/CvReversePass.cpp`). **The inversion must keep AND vs OR
> in DISTINCT buckets** or the reverse map loses the distinction — a single AND prereq inverts to its own bucket
> (`enables.routesAnd` / `enables.traitsAnd`), the OR-list to another (`enables.routes` / `enables.traitsOr`), and
> the load pass rebuilds each forward getter separately. *(The tech case reconstructs from the child's retained
> `requires.build.all`/`.any` instead — same goal, the two reconstruction sources.)*

**Empire/team-scope constructables need NO new machinery** (the scope spine already has team/empire): stage-gates
via `enables` (the space line), doctrine bans via `disables` + empire modifiers. An empire-scope building
replaces the `FreeBuilding` autobuild (~345 uses); it authors on the existing scope spine (team/empire), so it
needs no new machinery.

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
(each turn) adds/removes a building as the property value enters/leaves the band. The band models this as uniform
`requires.operate` dormancy: the building is enabled once, and its `requires.operate` `{PROPERTY_*, min/max}` clause
toggles it active/dormant as the value crosses the threshold — no per-turn add/remove churn. A band's own
non-constructibility (it is placed by the property system, not the production queue) authors as `notConstructible`
(an `identity` flag, [json](json.md) §7).
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
   `CvCondition`, the StoneBase `ConditionParser` port) and evaluation through the ONE evaluator
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
emit surfaces as a visibly wrong enabler) — and that diff is taken **OUTSIDE the DLL**, between the two served
documents (`/computed/enabler/operating/{stored,oracle}`, [http-endpoints.md](http-endpoints.md)); the engine
never compares the two sides ([state-repositories.md](../architecture/state-repositories.md)). The `requires` gate re-runs **incrementally over only the affected candidates** (via the reverse
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

**HAVE is NOT a new store — and its READ SURFACE is the per-scope CONTEXTS**
([contexts.md](../architecture/contexts.md), owner). The object-owned has-lists that ALREADY EXIST (the city's
buildings-present / religions / corporations, the player's civics / traits / heritages, the team's techs) stay
where they are — the object owns its presence state, the [tally](tally.md) rule ("let an object care about
itself") applied to presence — and each scope's CONTEXT forwards them (storing only a homeless aggregate, e.g.
`policies`), so every reader — the evaluator's atoms, the gates — asks the context, never reaches into the game
object ad hoc. The DOMAIN event carries the delta that triggers the in-place list update, and the enabler stores
only what it **derives** (the lists + the operating-building set). Predicates/atoms read HAVE through the
contexts; what is event-driven is the **maintenance** (which dependents re-gate, when), never a read-side
recompute.

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

## 8. Build state — where the enabler stands

> **Project-specific build state**, kept on this one enabler surface (no separate plan doc, owner ruling). It
> records only what is BUILT vs NOT, and the forks the §7 model has already resolved — never a narration of how
> the code got here ([DEC-docs-current-truth](../architecture/decisions.md#dec-docs-current-truth)).

### What EXISTS

The enabler machine is built and lives in **`Sources/Enabler/`** — its own tree, carrying no `Cascade` prefix
([DEC-enabler-not-cascade](../architecture/decisions.md#dec-enabler-not-cascade)):

- **`EnablerDomain`** (`CvEnabler.{h,cpp}`) — the §7.1 shape: the tri-state array + the two membership refcount
  planes + the removal-wins formula. One component, instantiated per scope owner.
- **`EnablerKernel`** (`CvEnablerKernel.{h,cpp}`) — the shared edge-apply (`applyEdges`), the `requires` gate
  (`requiresMet` → `cascadeEvalCondition`), the `allowed` cap (`allowedOk`), and the operating-building fixpoint.
- **The eight per-domain enablers** — `CvTechEnabler` / `CvBuildingEnabler` / `CvUnitEnabler` / `CvCivicEnabler` /
  `CvProjectEnabler` / `CvProcessEnabler` / `CvBuildEnabler` / `CvPromotionEnabler`, each its domain's seed +
  event-delta calculator, all routed through the ONE `applyEdges`.
- **`CvEnablerConsumer`** — the enabler's OWN spine consumer, registered by `enablerRegisterConsumer()`. It is
  **LOAD-ACTIVE**: the reseed's in-read emits BUILD the domains through the same appliers play uses
  ([DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed)) — there is no warm-up seed walk. One
  consumer per system; it never routes modifier work.
- **`OperatingBuildings`** (`CvOperatingBuildings.h`) — the §3.2 set type (`active` + `provided` + `obsolete`).
- **`CascadeCapabilities`** (`CvCapabilities.{h,cpp}`) — the per-team derived-on-query capability union
  ([capabilities.md](capabilities.md)).

### The host — GRAFTED

The machine's state now lives on its scope owners, as plain DATA MEMBERS (the guardrail bars adding vtable *bases*
to EXE-bound classes, never members — [state-repositories.md](../architecture/state-repositories.md)):

| owner | member | what it holds |
|---|---|---|
| `CvCity` | `m_enabler` (`CityEnabler`) | the constructible + trainable tri-state domains |
| `CvCity` | `m_operatingBuildings` | the ACTIVE set + provided bonuses at the operate/provides fixpoint (§3.2) |
| `CvPlayer` | `m_enabler` (`PlayerEnabler`) | techs / civics / projects / processes / builds / promotions |
| `CvTeam` | `m_cascadeTeamCaps` | the capability union, on the `CvDerivedCacheSet` mark protocol ([capabilities.md](capabilities.md)) |

All are **public and mutable** by requirement rather than laxity: the domain enablers write through a
`const CvCity&` / `const CvPlayer&` — the owner holds the STORAGE, the enabler owns the delta LOGIC. **None is
serialized**: every one starts empty and un-ready and is filled by the reseed's events through the same appliers
play uses ([DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed)). Each owner's `reset()` clears them,
which is load-bearing because a `CvCity` is RECYCLED out of an `FFreeListTrashArray` — without it a new city
inherits the previous occupant's frontier.

⛔ **REGISTRATION ORDER IS A CONTRACT: contexts → enabler → modifier.** The enabler's load-end gate pass evaluates
through the CityContext / EmpireContext stores, which the contexts' consumer builds on the SAME
`GAME_LOAD_FINISHED` event; gating ahead of it evaluates against empty stores and every verdict is silently wrong,
with no self-heal to re-derive it ([state-repositories.md](../architecture/state-repositories.md)).

### The availability READ surface — BUILT

**⚖ THE NEW SURFACE IS BUILT WITHOUT WAITING FOR THE LEGACY DISCONNECT (owner):** *"assume it is already
disconnected, add the new."* The disconnect is its own sweep; gating the replacement on it is what leaves the
machine unreachable indefinitely. Build the new surface as if the legacy one were already gone.

**ONE READ PAIR PER DOMAIN** — the domain IS the group, and the existing engine enum
(`BuildingTypes`/`TechTypes`/…) is the consumer's vocabulary. The domain set is fixed and small, so the surface
grows by DOMAIN, never by candidate; there is no per-candidate getter and no what-if argument.

| owner | verdict (tri-state) | frontier (caller-owned fill) |
|---|---|---|
| `CvCity` | `getBuildingAvailability` · `getUnitAvailability` | `getAvailableBuildings` · `getAvailableUnits` |
| `CvPlayer` | `getTechAvailability` · `getCivicAvailability` · `getProjectAvailability` · `getProcessAvailability` | `getAvailableTechs` · `getAvailableCivics` · `getAvailableProjects` · `getAvailableProcesses` |
| `CvPlayer` (carve-outs) | `getBuildUnlocked` · `getPromotionUnlocked` | `getUnlockedBuilds` · `getUnlockedPromotions` |

⛔ **Every read is a BARE O(1) FETCH of the maintained tri-state** — no gate runs, no calculator is called, and
`requires` is never evaluated (§7). A missed propagation therefore leaves a visibly wrong verdict instead of
being silently recomputed away ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)).

**The tri-state is returned WHOLE, answering TREE + GATE only.** HIDDEN vs GREYED is the "why not" the build list
needs (§6), so reducing it to a bool would force a second read to recover it. ⛔ The **QUEUED overlay is
deliberately not folded in**: the domain keeps `FLAG_QUEUED` separate from `FLAG_GATE_FAILED` precisely so
"already queued" stays distinguishable from "requires unmet", and collapsing a queued candidate onto GREYED would
destroy that and misreport why it is not offered. The overlay rides only the two reads that care — the FRONTIER
(fresh offer, queued excluded) and `CvCity::isBuildingContinuable` (reads past it, so the production-check sweep
does not cancel every in-progress build).

**⚖ THE "EVER" QUESTION IS ITS OWN READ — the tri-state cannot answer it.** HIDDEN conflates *"nothing enables it
YET"* with *"it can never be offered"*, and a **queue** asks precisely the difference: a research target is chosen
now and researched later, so "not currently offerable" is not a refusal. `CvPlayer::isTechEverReachable` answers
it off the membership planes directly — not held, not statically barred (`identity.disable`), nothing held
removes it. It exposes the EXISTING `FLAG_STATIC_EXCLUDED` plane as a bare read (`EnablerDomain::isStaticExcluded`)
rather than minting a fourth state: the tri-state vocabulary is unchanged.

⚠ The two **carve-out** domains answer the UNLOCKED half only, and a consumer treating either as the whole verdict
over-offers: a BUILD's plot-validity half and a PROMOTION's per-unit applicability are evaluated LIVE at their
decision points (§7.1). The TEAM's capability reads are not here — `CascadeCapabilities` is already that query
surface ([capabilities.md](capabilities.md)); duplicating it onto `CvTeam` would be a second implementation.

⛔ Still do not re-attach the machine ad hoc — a per-site `can*` rewire is the half-migration this rebuild exists
to avoid ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)). Moving CONSUMERS onto
this surface is the remaining sweep.

### The gate stages, by domain

The gate verdict is a per-id flag (`setGateFailed`): a failed gate flips a tree member LISTED → GREYED, membership
untouched. **A domain whose gate stage has not landed never sets the flag, so its members stay LISTED** — the
enable-side over-offer, which is a VISIBLE defect to fix, never a reason to fall back to legacy.

| domain | membership | `requires` gate | `allowed` cap |
|---|---|---|---|
| techs | ✅ | ✅ | ✅ (world-unique founder techs) |
| buildings | ✅ | ✅ | ✅ (world/team/empire self-caps; per-city wonder-CATEGORY caps open) |
| units | ✅ | ✅ | ✅ (world lifetime-created; empire era-scaled national cap) |
| projects | ✅ | ✅ | ✅ |
| civics · processes · builds | ✅ | ✗ — enable-side LISTED | ✗ |
| promotions | ✅ | on demand (§7.1 carve-out — no maintained flag) | n/a |

**Promotions are the exception to the over-offer:** they set no gate flag, but `requires` + the unit-state
applicability leg (unitcombat QUALIFIED/DISQUALIFIED, game options, promotion-line prereq tech, and the runtime
spy/pillage/commander/commodore/blend + intercept/evasion/XP caps) are enforced ON DEMAND at level-up, so the
promotion offer is not over-inclusive.

### Resolved forks (part of the §7 model — not open questions)

- **HAVE model:** the enabler owns NO HAVE store — it ties into the object-owned has-lists that already exist
  (city buildings/religions/corps, player civics/traits/heritages, team techs). Presence stays on the objects; the
  [tally](tally.md) stays the count accessor.
- **Evaluator depth:** `cascadeEvalCondition` reads raw object-owned state (legitimate live reads). What is
  event-driven is the MAINTENANCE — which dependents re-gate, when — never the read source.
- **Component model:** one unified component, instantiated per §7.1 owner; the delta-apply SIBLING of
  `CvDerivedCache`, which cannot operate the same way (§7 — no dirty→recompute path exists at all).
- **The root rule:** no implicit "no-edge ⇒ available" engine rule. Start-available entities are authored onto
  `TECH_GAME_START`'s `enables` (§2, curator-derived), the tree is fully connected, a missing edge fails closed.
  The load backfill of `TECH_GAME_START` itself is the ONLY engine special case the model needs.
- **The BONUS axis is GATE-ONLY** (owner ruling): a plot-group-carried bonus NEVER drives tree membership. The
  curator keeps authoring bonus `enables` edges (the reverse-mapped view of the target's retained `requires`
  atom), but the runtime consumes bonus events as pure stateless gate re-checks over the bonus's
  `EDGEF_REQUIRED_BY` dependents. Membership rides tech/building/civic edges + the root; an entity whose only
  inbound edges are bonuses ROOTS, sitting visible-GREYED on its bonus requirement. The one carve-out — a bonus ON
  a plot enabling an improvement's placement (`enables.builds`) — is a live per-plot gate, no domain involvement.

### Open work

1. **Converge the enabler's PRIVATE reverse buckets onto `EDGEF_REQUIRED_BY`**
   ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)) — the `s_bc*` / `s_uc*` / operate
   `s_op*` static indexes inside `CvBuildingEnabler` / `CvUnitEnabler` / `CvEnablerKernel` are pre-existing
   bespoke-reverse-view remnants; each retires onto the info-homed REQUIRED_BY axis.
2. **The gate stage for civics, processes, and builds** — the three flagless domains above.
3. **Per-city wonder-CATEGORY caps** in `allowedOk` (the `CultureLevel` `worldWonders`/`teamWonders`/
   `nationalWonders` counts, [json.md §4.4](json.md)).
4. **RESIDENCY + COUNTING.** The `CvPlotGroup` holds the network's bonus content and **is the ONLY authoritative
   list for trade resources** — the city holds no authoritative mirror. But the CITY read must be a **maintained
   number, added and subtracted on spine events, never calculated per read** (the state-repositories capstone):
   `CvCity::getNumBonuses` is a bare fetch of a derived, never-serialized per-city count kept current by the
   crossing fan-out plus the tech/minted/corp events that move its gates. The per-read calculation it replaces
   (TechCityTrade gate → two-hop plot-group resolution → group sum → minted gate → corp add-on, re-executed on
   EVERY call) was the turn wall's hottest cluster under the governor's read volume. VICINITY belongs to the CITY
   and is a plain local-presence fact: it satisfies `connection:"vicinity"` atoms and NOTHING else — it never adds
   a second owned count (one pasture is ONE horse, not vicinity+network=2).
5. **Neither the counts NOR plot-group MEMBERSHIP are trusted from a save** (membership is derived state: routes +
   terrain-trade capabilities + ownership). The deserialized groups are drained and discarded; a load-end rebuild
   re-colors membership from current state and folds the counts through the live entry points as each plot joins,
   announcing every bonus fact as a genuine crossing emit before the `GAME_LOAD_FINISHED` gate pass.
6. **The DORMANCY VERDICT is the operating-building fixpoint** (§3.2,
   [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)) — applied through the engine's
   disabled-building flag, never a hand re-derivation from legacy prereq getters, plus the two runtime-state legs
   the authored data does not carry (employed-population composition; the banned-non-state-religion policy). The
   load-end cross-city fixpoint — iterate {re-fixpoint each city's operating set → apply flips → the provides
   injections adjust the network} until stable — reconciles the serialized flags to the computed verdict inside
   the load bracket (a manufactured chain lights tier by tier: ore → wares → firearms). The iteration is
   WORK-LIST driven, each flip keeps the FULL per-flip side-effect surface (power, freshwater, employed
   population, traits, provides), and convergence is declared ONLY by a quiet FULL verify pass.
   ⛔ **BAKED-CONSUMER RE-RUNS:** an engine consumer that BAKES state on modifier changes (the trade-route
   ASSIGNMENT) runs during this fixpoint against not-yet-warmed packages and its baked result self-heals never;
   every such consumer is re-run ONCE after the load-end package warm.
7. **The dynamic operate axes ride their events** — connectivity via the plot-group/network bonus events,
   vicinity (radius growth) via the culture-level event — routed into the operate re-check of dependents.

### The consumer ITERATION sweep (F2b)

Spec authority is §6: the AI's decisions iterate ONLY the frontier, never the entity database. The hot AI decision
loops whose gate is the plain-verdict `can*` read the enabler's LISTED frontier via `EnablerDomain::listedIds`
instead of scanning the whole entity space. Two things are NOT a frontier swap and stay open: a loop calling a
gate with WHAT-IF args cannot iterate the frontier (it answers only the current verdict), and the
`AI_chooseProduction` focus-ladder collapse is an AI-architecture change, not a per-loop rewrite.

---

## See also
- [json.md](json.md) — the data this machine reads: `enables`/`obsoletes`/`replaces`/`disables` (§4.1–4.2),
  `requires` build/operate (§4.3), `allowed` (§4.4), and the `all`/`any`/`noneOf` + atom/predicate vocabulary (§3).
- [tally.md](tally.md) — the count machine the `requires` count-atoms and the `allowed` cap read at cross-city scopes.
- [modifier.md](modifier.md) — the sibling "how much?" machine. A dormant/unavailable entity (per this doc)
  simply deposits no modifiers.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids that fill the `enables` buckets and `requires` atoms.
