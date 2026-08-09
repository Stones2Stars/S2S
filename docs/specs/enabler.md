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
> A unit's superseders ARE genuine removal-on-succession (the legacy engine dropped the predecessor once a
> superseder was buildable) → modeled as the unit's `replacedBy.units` replace edge (§ units, below).
> The legacy *building* `ReplacementBuildings` (A lists the buildings that supersede it) *looks* like removal, but the legacy engine only
> DISABLED A while the successor was present and re-enabled it when the successor was gone — reversible **dormancy**, never removed. So it is mirrored as the **target's
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
> the inverted `enables`** (never stubbed, never re-authored on the child): the tech `leadsTo` and the route
> bonus prereqs in the ONE general reverse pass (`Data/CvReversePass.cpp`) — a cross-entity reconstruction has
> exactly ONE home, and it is never `mapFrom` (which runs while the view is still being built) nor a second
> load-time pass beside the reader. ⚠ **The TRAIT prereqs are the deliberate exception and their forward GETTER is
> NOT reconstructed:** the rebuilt `CvTraitInfo` carries no prereq getter at all, because re-adding
> `getPrereqTrait`/`getPrereqOrTrait1/2` would be a legacy getter name returning
> ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)). Their consumers
> (`CvPlayer`, `CvGameTextMgr`) read the trait's own edge families instead, as stage-4 consumer work.
> ⛔ **"Not reconstructed" is about the GETTER ONLY — the prereqs THEMSELVES are live and load-bearing. Reading
> this line as "trait prereqs are inert" is the misreading this callout exists to stop.** A trait's `TraitPrereq`
> and `PrereqTech` INVERT at the store onto the SOURCE's `enables`: trait→trait becomes the developing-ladder
> edge, and tech→trait becomes `tech.enables.traits` — §2's rule that a tech is authored in `enables`, never as a
> generation driver in `requires`. ⚑ The TECH leg is not a curiosity: every rank ±2/±3 rung carries one, so it is
> the gate on advancing a developing line at all, and a tech JSON missing those edges leaves every upper rung
> permanently unreachable — silently, since nothing reads a gate that was never emitted.
> **The inversion must keep AND vs OR
> in DISTINCT buckets** or the reverse map loses the distinction — a single AND prereq inverts to its own bucket
> (`enables.routesAnd` / `enables.traitsAnd`), the OR-list to another (`enables.routes` / `enables.traitsOr`), and
> the load pass rebuilds each forward getter separately. *(The tech case reconstructs from the child's retained
> `requires.build.all`/`.any` instead — same goal, the two reconstruction sources.)*
>
> ⚖ **WHAT DECIDES RECONSTRUCT-vs-EDGE-FAMILIES: CAN THE THING PHYSICALLY MOVE? (owner)** A **ROUTE** is pinned to
> its plot, so a static forward list on the info describes something that cannot change place — reconstruction is
> coherent, and the route side KEEPS it. A **UNIT** physically moves, so its consumers read the unit's own **edge
> families** rather than a reconstructed forward list, and no `unitsAnd` bucket is minted for one.
> ⛔ Apply this test to any future forward-view question; it is not a per-case preference.
> ⚠ **The cost to price in: an edge family is ONE MERGED BUCKET.** `EDGEF_RELATED` lands every authored family's
> references together, so a unit's `EDGEB_BONUSES` does NOT preserve the mandatory-vs-one-of split, and its
> `EDGEB_TECHS` mixes ENABLING techs with OBSOLETING ones. A consumer with **ANY** semantics is safe (a superset
> only loosens); a consumer with **ALL** semantics is NOT — reading a merged bucket as "every one of these is
> required" silently demands a unit's own obsoleting tech before it may be trained. Keep the exact predicate over
> the family ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)); where ALL semantics are
> genuinely needed the answer is the owning info's own `requires` section, never the merged family.

**Empire/team-scope constructables need NO new machinery** (the scope spine already has team/empire): stage-gates
via `enables` (the space line), doctrine bans via `disables` + empire modifiers.

> **⚖ MOVING THE ALL-ENCOMPASSING BUILDINGS TO EMPIRE SCOPE IS WANTED, AND IS OUTSIDE THIS REWORK (owner).**
> It is forward intent, not a #430 instruction — do not read it as one, and do not build a player-held building
> instance for it.
> ⛔ **In particular it is NOT how the free building is served.** That is a GRANT: the source names the target
> in `grants.buildings` and the receiving city genuinely HAS the building, which is load-bearing because the
> authored data gates on holding those targets in over a thousand `requires` atoms — an empire-scope effect-only
> shape would satisfy none of them ([legacy-grant-apply-sites.md](../reference/legacy-grant-apply-sites.md) §4
> carries the ruling and the two-leg apply).

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

**Pseudobuilding bands.** Legacy `CvPropertyInfo` `iMinValue`/`iMaxValue`/`BuildingType` added/removed a building
every turn as the property value entered/left the band. The band models this as uniform
`requires.operate` dormancy: the building is enabled once, and its `requires.operate` `{PROPERTY_*, min/max}` clause
toggles it active/dormant as the value crosses the threshold — no per-turn add/remove churn. A band's own
non-constructibility (it is placed by the property system, not the production queue) authors as `notConstructible`
(an `identity` flag, [json](json.md) §7).

**⛔ `notConstructible` MEANS ONE THING: IT NEVER GOES THROUGH THE `canConstruct` GATE, EVER (owner).** It is a
statement about the PRODUCTION QUEUE and nothing else — the entity is not offered, not greyed, not evaluated as a
build candidate. ⛔ **It does NOT mean "build it in every city"** (owner), and reading it that way is what this
callout exists to stop.

⇒ **WHO places it, and WHERE, belongs to the PLACING SYSTEM — never to this flag.** The property solver places its
bands; `CvGame::setHeadquarters` places a corporate HQ in the ONE city that holds it; the achievement system awards
one per player. Those systems already know their own answer, and the flag's job is only to keep the production
queue out of it.

⛔ **SO A BLANKET "PUT EVERY QUEUE-EXCLUDED MEMBER IN EVERY CITY" PASS IS A DEFECT, NOT THE MODEL.** It hands every
city a copy of entities whose own data says one may exist — a `{world: 1}` corporate headquarters or relic, an
`{empire: 1}` achievement — and `allowed` cannot refuse it, because `allowed` gates a BUILD (§4) and a
queue-excluded entity is never a build candidate. ⚑ **The damage is not confined to over-offering:** an entity
that is ACTIVE in N cities deposits N times, so a scope-wide deposit it carries is multiplied by the city count —
silently, on a plausible-looking number ([modifier.md §5](modifier.md)).
⚠ **A per-city population is the BAND's property, not the class's.** A band genuinely belongs in every city and is
toggled by its `requires.operate` threshold, which is what deletes the legacy per-turn add/remove churn; that is
the property system placing its own entities, and it stays exactly as it is. What does not follow is the
generalization from it.

⛔ **A band bound is a SIGNED threshold, so "absent" can never be encoded as a negative.** A property value is
legitimately negative (the low-education ladder is authored entirely in negative bands), so a `min`/`max` absent-test
that asks `< 0` silently drops a real bound and the clause collapses to always-true. The absent marker has to live
outside the value domain.

⚑ **The consequence is that such an entity carries NO `requires.build`, and this is structural rather than a
convention to remember.** `build` only ever greys a QUEUE candidate and is checked ONCE (§3 above); the ongoing
dormancy gate reads `operate` alone. A queue-excluded entity is never a queue candidate, so its `build` clause has
no consumer at all, and anything left there would silently never be
evaluated again (a cliff dwelling placed in a flat city would come up ACTIVE, its `TERRAIN_PEAK` clause sitting in
the half nothing reads). The curator therefore folds `build` into `operate` for the whole class
([DEC-recurate-on-decision](../architecture/decisions.md#dec-recurate-on-decision)).
⚑ The folded position is strictly MORE correct than the one it leaves: `operate` is re-checked every recompute, so
the entity correctly dorms if the ground it needed stops existing (terrain levelled to sea level — the WMD case),
which a checked-once `build` clause could never do.

⚠ **Cost, for the population a placing system genuinely does put in every city (the bands):** it allocates nothing
new — the per-city building arrays are already dimensioned by `NUM_BUILDING_TYPES`
([memory-footprint.md §2](../reference/memory-footprint.md)) — and it is not a per-turn cost, because the operate
fixpoint is targeted-propagation maintained (§3.2) and re-walks only what an event touched — each building
resolving its own dormancy as it arrives, once. ⛔ That is a cost argument for the BAND population, and it was never a licence to widen the population it
is paid for.
Where the bands form a succession chain (the **Education ladder**) a higher band dorms the lower via
`requires.operate.dormant` (only-highest-active, no stacking) — the **same uniform `ReplacementBuildings → dormant`
mirror as §2, not a special case** (there is no separate "education" ruling); chainless bands (crime/disease/
pollution/tourism) compound, every in-band band active.

> **⛔ A DORMANT TRIGGER TESTS WHETHER THE SUCCESSOR IS *ACTIVE*, NEVER WHETHER IT IS *PRESENT* — and under the
> band model nothing else is even expressible.** A band is PLACED ONCE and never removed, so every rung of every
> ladder is present in every city from turn one. A presence test therefore reads TRUE forever: each rung sees the
> rung above it standing there and dorms, the top rung dorms on its own `operate` clause, and **only-highest-active
> collapses to NOTHING-active** — in every city, on every ladder, for every property.
> ⚑ **The blackened-skies case is the proof, not an analogy:** §2 promises the observatory *"goes dormant and
> wakes when the skies clear"*. `BLACKENED_SKIES` is itself a band and is therefore permanently present, so only
> its ACTIVE state ever clears — under a presence test the skies never clear at all.
> ⚠ **Legacy tested presence and was right to**, which is what makes this easy to reintroduce: legacy added and
> removed band buildings every turn, so present and active were THE SAME FACT. The band model is precisely what
> separates them ([engine.md](../reference/engine.md): the per-turn add/remove maintainer is CUT), so the test has
> to follow the half that still carries the meaning.
> ⚑ **Two consequences for the fixpoint, both load-bearing.** (1) The operate/provides fixpoint now has TWO
> coupled unknowns — the supply AND the active set — so it terminates only when BOTH are stable; stopping on the
> supply alone freezes a ladder with every rung active, the mirror image of the same bug and equally silent.
> (2) An ACTIVE flip must re-check whoever dorms on that building, via the dormant-triggered-by reverse index —
> a route presence never needed, because presence only moved when something was built or destroyed, while an
> active state moves whenever a property value crosses a band. Without it a ladder settles once and never
> re-settles, so a rising property leaves two rungs depositing side by side.
> ⚠ The ripple's queued-mark is therefore a de-duplicator for what is CURRENTLY QUEUED, never a processed-once
> ledger: a rung genuinely must be re-classified after its successor settles. Bands are **bidirectional** — effect-buildings can spawn on the
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
  superseders. The cascade recurses these engine-side: hide the unit only when
  **every** such upgrade resolves to a reachable-trainable unit (one dead branch keeps it buildable). The named
  `dormant` clause is fail-safe (default *not*-dormant). *(This recursion — `uc_reachable`, the StoneBase
  `UnitCascade.Reachable` closure — is what resolves the whole upgrade TREE: chains, obsolete intermediates, cycles.
  It is the spec'd resolver; do NOT replace it with a one-level or hand-rolled scheme.)*

- **`SupersedingUnits` → the `replaces` edge (`replacedBy.units`, §2)** = genuine **removal-on-succession**: the unit
  drops from buildable the moment any superseder is itself buildable. Superseders are excluded from the upgrade
  closure, so they live here, not in the dormancy gate. This is the first real use
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
level — the workable-plot predicates (`evp_terrain`/`evp_improvement`/`evp_route`/`evp_peak`/`evp_hill`,
`Conditions/CvConditionEval.cpp`) require an **owned** plot (= `WORKABLE`), while `evp_feature` also accepts a
neutral plot unless `EXP_STRICT_VICINITY` is on. **A `vicinity:"onSite"` atom asks the strongest of these: the
resource is AVAILABLE here — an OWNED radius tile whose IMPROVEMENT trades it, or an active building supplying it
([json.md §5a](json.md)).** ⛔ It does NOT ask the network: onSite and `connection:"trade"` are ORTHOGONAL, so a
resource can be either without the other ([json.md §3.4](json.md)).

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

It is **maintained by targeted propagation, never a blanket recompute**: each HAVE-change ripples only the
affected buildings into the authoritative set in place (via an operate reverse-index)
— see [state-repositories](../architecture/state-repositories.md). In code it is
`CvCity::m_operatingBuildings` (type **`OperatingBuildings`** — its `active` + `provided` + `obsolete` sets), read via
`EnablerKernel::operatingBuildings` / `wireOperatingBuildings`.

> **⛔ THERE IS NO LOAD SEED — THE SET IS BUILT BY THE FACTS, LIKE EVERYTHING ELSE (owner).** A full per-city
> recompute ran at `GAME_LOAD_FINISHED` and is DELETED. The game objects and their contexts exist before the
> facts flow — the save could not load otherwise — so a building announces its presence as it deserializes,
> resolves its own dormancy there, and every HAVE axis (bonus / vicinity bonus / religion / corporation /
> population / power / building) re-checks the consumers of what it supplies. A manufactured chain therefore
> lights tier by tier AS THE STREAM RUNS; there is nothing left for a rebuild to discover.
> ⚑ **What the recompute actually cost, measured:** it forced the in-read ANNOUNCE to be suppressed (otherwise
> the load-end re-announce double-applied every deposit), so the ENABLER was event-built while the CASCADE saw
> no operating verdict at all until after the bracket — 102k activations and every deposit landing in one burst,
> off a set the facts had already converged 55 seconds earlier. **The cascade and the enabler must build on the
> SAME SEEDS (owner)**, and two compensating hacks were what stopped them.
> ⛔ Do not reintroduce either half. A guard must never suppress an emit
> ([event-spine.md](event-spine.md) § THE RECEIVED LINE), and a recompute beside an event-built set is banned
> outright ([DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed): it "may never survive beside the
> setters"). ⚠ Order is not what makes this safe — a package is additive, so arrival sequence is irrelevant
> (owner); what matters is each fact arriving EXACTLY ONCE, which is precisely what a second builder breaks.

> **⛔ THERE IS NO PER-TURN RE-CHECK OF ANY KIND, AND A "BOUNDED" ONE IS NOT AN EXCEPTION (owner).** A sweep that
> re-gates a set once a turn — however small the set — **jumps over the core system**: the fact is what moves a
> verdict, and a periodic pass is a second maintenance surface running beside it. It is
> [DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal) (no blanket per-turn rebuild) and
> [DEC-flag-is-fossil](../architecture/decisions.md#dec-flag-is-fossil) (a periodic re-check ASSERTS that we
> cannot know what changed, which a saturated emit surface falsifies by construction).
> ⚑ **Its real cost is not the cycles, it is the CONCEALMENT:** a sweep silently repairs the verdict a missing
> route left wrong, so the gap stops being observable and the enable-side over-offer that would have named it
> never appears. ⇒ **Over-offer is always the same diagnosis — a fact that is not being read** — so the fix is
> the ROUTE, every time.
> ⛔ So a candidate whose `requires` reads live state does not earn a sweep: either its axis has a fact and is
> routed on it, or the axis is STATIC for the city's life (a plot's latitude, a victory condition) and is gated
> once at creation. Nothing in the authored data falls outside those two, and a future atom that appears to must
> get its fact ([DEC-close-event-gaps-now](../architecture/decisions.md#dec-close-event-gaps-now)), never a
> re-check.

**Obsolescence is the THIRD outcome of this same pass.** A present building whose `obsoletedBy` tech is held is
neither active nor dormant — it goes into the `obsolete` set (excluded from `active`, provides nothing), and the
[modifier](modifier.md) reads its **`whenObsolete`** tree (§2 / [json](json.md) §4.2) in place of its normal
families. It is maintained by the same targeted propagation (an `obsoletedBy.techs` reverse-index re-checked on a
tech change), read via `cascadeIsBuildingObsolete`.

⛔ **THE INSTANCE'S FATE IS DECIDED BY `whenObsolete`, AND THERE ARE EXACTLY TWO (owner):** an **absent/empty**
tree means the building is **HARD REMOVED**; a tree **carrying any modifier** means the building **STAYS** and
that tree **TAKES OVER** from its normal families ([json.md §4.2](json.md)). So this `obsolete` set is the
**tree-carrying population** — present, non-active, depositing `whenObsolete` — never the removed ones, which
are not in the city to hold.

⚖ **A TECH IS THE ONLY THING THAT CAN OBSOLETE (owner), which is what makes the whole fate purely EVENT-DRIVEN
and needs no fact to DRIVE it.** When a tech lands, the buildings it obsoletes are checked and each does what it
needs to do — so the apply lives on the TECH fact, in the enabler's `onTechChanged`, beside the edge application
that already runs there.

⚖ **AN "I HAVE BEEN OBSOLETED" FACT IS WELCOME — but it is PURELY for LOGGING and the NOTIFICATION (owner),
never the mechanism.** That is the [event-spine.md](event-spine.md) player-alert shape exactly: the alert is a
CONSUMER of a fact, never re-inlined at the mutation site, and the legacy "your building was obsoleted" message
died with the legacy mutator this cut removes — so it is on the owed-alerts list. ⛔ What must NOT happen is the
APPLY being moved onto that fact: the removal is not waiting on an announcement, and routing it through one
would make a UI concern a condition of the state change.

⛔ **So the legacy shape was wrong in three separate ways, and all three are cut.** `CvTeam::processTech` swept
the WHOLE building registry asking each id whether this tech obsoleted it, tore the instance out
unconditionally, then walked a `getObsoletesToBuilding` chain to place a successor. But the tech's own
`EDGEF_OBSOLETES`/`EDGEB_BUILDINGS` edge already names the handful (the own-data inversion — never scan the
registry), the fate is the `whenObsolete` branch above rather than an unconditional removal, and the successor
that chain placed is exactly what the curator now reads to emit the tree. A hand-wired per-site reaction inside
a mutator is retired in favour of the one surface.

---

## 4. The `allowed` cap

`allowed` ([json](json.md) §4.4) is a separate gate from `requires` — "how many of **me** may exist," not "what
I need." A build is permitted while **`count(me, scope) < allowed`**; the count comes from the [tally](tally.md).
The engine owns ignoring caps under game options / era-scaling — the machine just compares.

**The two cap shapes gate in DIFFERENT places, because they have different scopes.** A **self-cap**
(`world`/`team`/`empire`) is player-scoped and gates in `allowedOk`. A **category count-cap** — how many
world/team/national wonders one CITY may hold, set by its `CultureLevel` — is per-CITY, which `allowedOk` cannot
see, so it gates in the building domain's own gate beside the SpecialBuilding group cap. A building's CATEGORY is
derived from **WHICH self-cap it authors** ([json.md §4.4](json.md): the cap's scope is what makes it a world /
team / national wonder), never from an `isWorldWonder` mirror, and the comparison uses the city's RAW category
counts — never the engine's `isWorldWondersMaxed()` verdict, which is a computed output a gate must not ride in on
([DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)).

⚠ **Its two gate INPUTS name the candidate NOWHERE, so neither is reachable through the candidate's own
`EDGEF_REQUIRED_BY` set** — the city's CULTURE LEVEL (which sets the max) and another wonder of the same category
ARRIVING here (which moves the count). Both therefore re-gate the whole capped set: on the culture-level fact, and
in this city on the building-changed fact beside the existing cap-scope fan. An unrouted gate input is a
permanently stale verdict ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)).

⚖ **TWO GAME OPTIONS REMOVE THE CATEGORY CAP OUTRIGHT, and the gate must honour BOTH.**
**`GAMEOPTION_NO_WONDER_LIMIT`** is the player asking for no limit — removing it is the whole point of the option —
and **`GAMEOPTION_CHALLENGE_ONE_CITY`** = NO wonder limits (owner); OCC remains an UNSUPPORTED mode, but it is an
ordinary game option like any other and needs no special machinery. While either is on, the category cap simply
does not apply.
⛔ There is deliberately **no curated cap variant** for either — neither RESCALES the limit, they REMOVE it, so the
legacy per-culture-level OCC cap field is not migrated. The gate reads the options at the CONSUMING system (here,
the enabler) while the info keeps serving ungated data ([json.md §9](json.md)).
⚠ **The enabler computes this verdict itself and must therefore carry the carve-outs itself.** It may not read
`CvCity::isWorldWondersMaxed()` — that is a computed output a gate must not ride in on
([DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)) — so the option checks that answer
holds there do NOT come along with the count, and an omitted one silently enforces a limit the player switched
off. Re-deriving a verdict means re-deriving every carve-out on it, not just its arithmetic.

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

⚑ **The DATA proves it, so this is not a stylistic preference:** across the curated set, **~75% of building
`requires` and the large majority of unit `requires` are AND** — multi-condition, often at different scopes, with
live predicates (connected / `IS_CAPITAL` / count thresholds). A top-down single-enable inversion cannot flatten
that, so the up-walk STAYS. What makes it cheap is that it re-runs **INCREMENTALLY over only the affected
candidates** via the `EDGEF_REQUIRED_BY` reverse index (§7.1), never over the whole frontier.

⛔ **CORRECTNESS *IS* THE TARGETED INVALIDATION — there is no self-heal net behind it**
([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)). The reverse index plus targeted propagation is
the WHOLE correctness mechanism: every HAVE-change re-gates exactly its dependents, and nothing blanket-rebuilds
behind it absorbing misses. ⚑ The asymmetry to hold onto: **over-inclusion in the reverse index is SAFE** (a few
harmless extra re-checks), while a **MISS is a bug to close, never an accepted one-slice lag** — it must surface as
a live divergence (a wrong `can*` verdict, or a stored-vs-oracle diff on the operating-set routes), and that
divergence is the signal to fix the reverse-index hole.

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
enabler consumes ONLY events precisely so a missed emit surfaces as a visibly wrong enabler. ⛔ The
oracle-vs-maintained ENDPOINT DIFF that once claimed to catch that is DEAD
([superseded-ideas #33](../architecture/superseded-ideas.md)): an endpoint cannot replay the event chain, so its
recompute side was never comparable. `/computed/enabler/operating` serves the MAINTAINED set alone, and a wrong
verdict is caught by the three-leg check ([http-endpoints.md](http-endpoints.md)). The `requires` gate re-runs **incrementally over only the affected candidates** (via the reverse
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
cache recomputes on its mark; the enabler **fundamentally behaves differently: the CAN-HAVE set is built PURELY on
the events of ALREADY-HAS** — each HAVE-event applies its `enables`/removal edges in place, the load reseed's
events are the one full build, and no mark-then-recompute path exists at all. A component's `requires` gate resolves
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
resolves at load (the legacy whole-Info replacement swap — dissolved into the curated trait sets, see
[modifier.md](modifier.md) — WorldBuilder/BUG, a per-civ research ban) is engine-side, not entity data.

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

## 8. The machine's shape — components, host, and the read surface

> The structural half of the design: what the machine decomposes into, where its state lives, and the contract its
> readers get. ⛔ It carries no build status and no worklist — what is NOT done is
> [todo.md](../plans/structural-cleanup/todo.md)
> ([DEC-spec-plus-todo](../architecture/decisions.md#dec-spec-plus-todo)).

### The components

The enabler lives in **`Sources/Enabler/`** — its own tree, carrying no `Cascade` prefix
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

⛔ **The empire-capability union is NOT one of these** — it is a keyed store the PLAYER holds, fed by the tech /
civic / building facts ([capabilities.md](capabilities.md),
[DEC-scope-contexts](../architecture/decisions.md#dec-scope-contexts)). The enabler is a SOURCE of those facts,
never the home of that answer.

### RESIDENCY — the network count lives on the PLOT GROUP, and only there

> **⛔ A PLOT GROUP IS A PURE OWNERSHIP QUESTION, AND IT IS ALWAYS FUNNELED THROUGH THE CITIES / FORTS THAT
> PARTICIPATE IN IT — NEVER THROUGH THE PLOT (owner).** It answers *"does this city HAVE this bonus"* — feeding
> `requires` gates, the `connection:"trade"` atom and any deposit conditioned on `HAS_BONUS`. It never
> contributes a MAGNITUDE to anything, and it never answers for a tile: the city is the asker
> (`CvCity::getNumBonuses` relays through the city's plot-group pointer), a fort participates as a city-like
> member via the `actsAsCity` characteristic ([json.md §8](json.md)), and the plot is merely where the resource
> sits.
> ⛔ **THE ROLLERSKATE THIS EXISTS TO STOP — CONFLATING THE PLOT GROUP WITH THE LOCAL PLOT SCOPE.** Both say
> "plot", and they are unrelated: a plot GROUP is a connectivity object spanning the map answering possession;
> plot SCOPE is one tile's own output. ⚑ **The measured consequence when they were conflated:** the connection /
> vicinity / network facts were routed into the PLOT package plane, where — carrying no plot — they fanned a mark
> over every plot of every city of the owner, dominating the entire load bracket. A connection fact moves no
> tile's output at all: the resource was already on its tile producing it.
> ⚑ **And a bonus's own yield reaches ONE tile — its own.** A resource changing a NEIGHBOURING tile's output is
> the deliveryguy's ([DEC-deliveryguy](../architecture/decisions.md#dec-deliveryguy)) and is authored on that
> tile's IMPROVEMENT, conditioned on the bonus — never on the bonus. ⇒ A plot-scope deposit is authored only by a
> PLOT-RESIDENT source, so a plot-scope route with no named plot has no target by construction, and declining to
> fan drops nothing.

**⛔ The `CvPlotGroup` is the ONLY authoritative list for trade resources, and NOTHING mirrors it.** Its content
is placed by the member CITIES (and `actsAsCity` forts) — never by a plot, which only holds the resource — so the
group is where the number is formed; every reader below it RELAYS. A `connection:"trade"` gate reads that list
and nothing else.

- **`CvCity::getNumBonuses` is a relay**, not a stored count: it reads the group through the city's plot-group
  pointer and applies the three things that are genuinely per-asker — the bonus's `TechCityTrade` gate, the
  player's minted-percent suppression, and the city's own corporation add-on. **The city declares no
  bonus-count member.**
- **`CityContext::tradedBonusCount` FORWARDS to that read** — it is the object's own O(1) data, so the
  STORES-vs-FORWARDS rule ([contexts.md](../architecture/contexts.md)) puts it on the forward side. A stored
  copy re-swept every bonus on every fact that could move one, for a number a pointer hop already answers.
- **What the crossing fan-out is FOR.** `CvPlotGroup::changeNumBonuses` still fans into its member cities, and
  the city's plot-group moves still announce — but only to fire the **presence CROSSING** (`processBonus` + the
  corporation re-check), never to maintain a value. A count moving between two non-zero values announces
  nothing, by ruling ([event-spine.md](event-spine.md)).

⚑ **Why a per-city mirror is the wrong answer even though the read is hot.** Three copies of one number
(group → city → context) is duplicated authoritative state with only drift to gain — the read-not-store rule
([tally.md](tally.md): *"creating something new when we already have it is pointless"*). And the cost that
argued for it is gone: the group maintains its holdings as a sparse `id → count` map, so the relay is a pointer
hop and a lookup, not the group SUM the mirror was built to avoid.

⚖ **VICINITY belongs to the CITY and is a plain local-presence fact:** it satisfies `connection:"vicinity"`
atoms and NOTHING else — it never adds a second owned count (one pasture is ONE horse, not vicinity+network=2).

### The host — where the state lives

The machine's state lives on its scope owners, as plain DATA MEMBERS (the guardrail bars adding vtable *bases*
to EXE-bound classes, never members — [state-repositories.md](../architecture/state-repositories.md)):

| owner | member | what it holds |
|---|---|---|
| `CvCity` | `m_enabler` (`CityEnabler`) | the constructible + trainable tri-state domains |
| `CvCity` | `m_operatingBuildings` | the ACTIVE set + provided bonuses at the operate/provides fixpoint (§3.2) |
| `CvPlayer` | `m_enabler` (`PlayerEnabler`) | techs / civics / projects / processes / builds / promotions |

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

### The availability READ surface

**⚖ THE NEW SURFACE IS BUILT WITHOUT WAITING FOR THE LEGACY DISCONNECT (owner):** *"assume it is already
disconnected, add the new."* The disconnect is its own sweep; gating the replacement on it is what leaves the
machine unreachable indefinitely. Build the new surface as if the legacy one were already gone.

**⚖ BUILDING CONSTRUCTION AND UNIT TRAINING ARE THE SAME PLANE (owner)** — one design, two domains, never two
designs. Both are **CITY** concerns for the same concrete reason: the gate needs *"what resources are in VICINITY,
and in the PLOT GROUP"* — city-local supply that no other scope can answer. ⛔ **There is therefore no
player-level construct/train verdict**, and a player-scope `canTrain`/`canConstruct` is not merely redundant, it
is asking at a scope that cannot know. A caller with a city in hand asks that city; a caller genuinely meaning
"anywhere" fans over the player's cities. ⛔ Do NOT mint a maintained player-level union to make the old shape
work: it is duplicated state that must never drift — the same argument that keeps projects/processes player-held
rather than copied per city (§7.1), running the other way.

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

### ⛔ WHAT THE ENABLER IS NOT — tech-tree PATHING AND QUEUING BELONG TO THE TECH-PICKING LOGIC (owner)

The enabler answers **"can I, right now?"** and stops there. Two research features are **NOT its concern**:

- **QUEUING FURTHER THAN THE TREE** — a player may queue a tech that is not in CAN GET yet (several steps away).
- **THE EASIEST PATH** to a chosen tech — the cheapest prerequisite chain from what is currently held.

*"That is NOT the enabler's concern; that is the concern of the actual tech-picking logic."* Both are
**research-only and only needed inside the TECH-TREE BROWSER** (owner). They are structurally impossible for the
enabler anyway: its maintained frontier holds only what is unlocked NOW, so it cannot see a candidate three steps
out — that answer comes from the **static compiled `enables`/prereq edges** the infos carry
([patterns.md § THE WHAT-IF DRIVER](../architecture/patterns.md)), walked COLD by the picking logic. A path search
is a genuine graph walk, which is acceptable on a browser path and would be unacceptable on the frontier.

⛔ So do NOT grow path-finding, queue projection, or a reachability closure inside the enabler. The enabler
supplies the FACTS (held / statically barred / removed / the gate verdict); the picking logic composes the route.
This is the [north-star](../architecture/north-star.md) test applied — ask *whose job is this?* and the answer
names the picking logic, not availability.

**⚑ AND IT NEEDS NO NEW MACHINERY EITHER — the picking logic just HYPOTHETICALLY FINISHES a tech (owner).** It
takes the maintained planes, overlays "as if this tech were held" (which contributes that tech's `enables` edges),
re-applies the §7.1 membership formula, and repeats — walking outward until it reaches the target. That is the
whole of both features: queuing beyond the tree is one such step, the easiest path is the cheapest chain of them.
The raw membership reads (`enableCount` / `removeCount`) are public precisely so a composite gate can OVERLAY
per-instance planes on the maintained ones before applying the formula; `EnablerOverlay` is the ONE
implementation of that shape and every hypothetical asker is a consumer of it, never a second overlay.
⛔ The overlay is the CALLER's, held in the caller's own scratch: it never writes the maintained planes. A
hypothetical that mutated the domain would leave the real frontier describing a game state that never happened.
⛔ **The formula itself is NOT re-implemented alongside it** — the overlay and the maintained refresh resolve
membership through the same `EnablerDomain::isMember` ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)).
A second copy would diverge the first time the formula gained a term, and a hypothetical that disagrees with the
frontier it is overlaid on is worse than no hypothetical at all.

⚖ **A WHAT-IF ASKS BOTH HALVES, AND THEY ARE ASKED SEPARATELY.** *"Would I be able to build X if I adopted this
civic"* resolves as **membership** (`EnablerOverlay` over the enable/remove planes) **AND** the **gate**
(`requiresMetInCity` with the hypothetical). A candidate can be gate-satisfiable under a hypothetical and still
not be in the tree, and the reverse — so collapsing the two into one test silently answers a different question.
⚑ Adopting a civic is a **SWAP**, so each side states both halves: the civic held and the one it displaces
dropped. An empty option slot displaces nothing.

⛔ **A BONUS IS NOT AN OVERLAY SOURCE, and the overlay refuses one.** The curator authors bonus `enables` edges
(the reverse-mapped view of the target's retained `requires` atom) but the runtime never counts them — the bonus
axis is GATE-ONLY (§8, the settled model rulings). Folding them would hand the hypothetical an edge class the maintained
planes have never had, so every HIDDEN candidate whose inbound edge is that bonus would read as newly unlocked
when acquiring it changes no membership whatsoever. *"Would this bonus let me build X"* is a **`requires`-GATE**
question — re-evaluate the candidate's `requires` with the bonus injected into the eval ctx — and it is a
separate mechanism from this one, never a widening of it.

**⚖ THE RESEARCH SEARCH DEPTH IS A LEADER VARIABLE (owner).** It bounds both the candidate walk and every
path-length test in the tech pick, so it is the ONE knob that tunes how far ahead an AI commits — and it is
therefore PERSONALITY, never a constant. It is authored as `ai.personality.researchSearchDepth` on the
LEADERHEAD; an unauthored leader takes the default, so per-leader values are pure data.
⚑ **This is the dial that governs BEELINING**, which is why it is worth having at all: the depth is exactly how
many hops past the researchable frontier a single distant unlock can pull an AI, so it is the lever on the
over-valued-enablement problem ([AGENTS.md](../../AGENTS.md) § AI valuation of ENABLEMENT — relaxing enablement
pull is only ever an improvement).
⚠ **The picker's other depth arguments are OVERRIDES, not depths** — a human's picker and a committed
culture-victory AI both ask for the immediate best (depth 1) rather than a plan, and neither becomes
personality-driven.
It belongs to the picking logic, like everything else in this section — never to the enabler.

**⚖ THE "EVER" QUESTION IS THE PICKING LOGIC'S, AND IT ALREADY OWNS IT.** HIDDEN conflates *"nothing enables it
YET"* with *"it can never be offered"*, and a research QUEUE asks precisely that difference — a target is chosen
now and researched later, so "not currently offerable" is not a refusal. ⛔ That is **not a gap in the tri-state
to fill**: per the boundary above it is a picking concern, and `CvPlayer::canEverResearch` is its existing, single
implementation, carrying the PERMANENT bars the enabler does not model as membership — the game-option bars
(`NO_FUTURE`, a tech's `PrereqGameOption`), the world-unique rule (*"religion techs are global and can only be
invented once by one player in a game"*) and the limited-religion hoarding guard.
⚠ **Do not re-derive it on the availability surface.** A second "ever" predicate reading only the membership
planes silently drops those bars — it would call a religion tech already invented elsewhere a legitimate queue
target ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)).
The split, stated once: **the enabler answers CAN-I-NOW (the tri-state); the picking logic answers CAN-I-EVER and
BY WHAT PATH.** The two membership bars that ARE the enabler's — `identity.disable` and a civilization's own
never-researchable list — are static for a player's life and sit on the static-exclusion plane at `initDomain`.

⚖ **BUT WHERE THE BAR *IS* AN ENTITY GATE, THE EVER QUESTION IS THE ENABLER'S — AND SO IS THE OPTION READ
(owner).** *"For all unit/promotions that rely on game options, and anything else the enabler deals with, it is
the enabler's job to call `hasGameOption`."* A whole-entity game-option bar authors as the entity-level
`enabled`/`disabled` pair ([DEC-entity-gate](../architecture/decisions.md#dec-entity-gate)), so answering "is this
barred for the whole game" is just evaluating that gate — availability data, read by the availability machine.
`EnablerKernel::everAvailable(bucket, id)` is that ONE implementation, parameterized over the domain axis rather
than split per domain, and it is where the option read lives for every entity-gated domain.

- **It is TOTAL by construction.** `CvInfo::getGate()` is declared on the BASE returning `NULL` and
  `cascadeGateOk(NULL, …)` is true, so a domain whose data authors no gate answers "never barred" and a
  newly-authored gate lights up as pure DATA — no engine change, no per-domain variant.
- **Evaluated against a bare ctx, deliberately.** Every authored entity gate in the tree is a `GAMEOPTION_` leaf,
  which reads the live options and consults no scope context — which is precisely what makes the verdict the same
  for every player and city, i.e. what "ever" means.
- ⚑ **The verdict is STABLE for the game, and that is load-bearing (owner): nothing the enabler gates rides a
  BUG/live option.** A game option is fixed at setup, whereas a live option (`setDefineINT`) is changeable
  mid-game and its flip carries **no DOMAIN event** — so a maintained verdict gating on one would go permanently
  stale with nothing to re-derive it ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)). The last
  enabler-facing live options went with the ranged-bombard removal
  ([superseded-ideas #24](../architecture/superseded-ideas.md)), so the hazard is absent from this surface rather
  than merely avoided. ⛔ Do not gate an enabler entity on a live option; if one is ever wanted, it needs its emit
  first.

⛔ **TECHS stay the picking logic's, and the reason is the distinction to apply elsewhere: their bar is a
COMPOSITION, not a gate.** `CvGame::canEverResearch` composes `NO_FUTURE` against the tech's own era and `isRepeat`
data — a consuming-system calc ([engine.md](../reference/engine.md)), which no entity gate carries and which an
info structurally cannot answer. Run that test on any future "ever" bar: a plain entity gate is the enabler's; a
composition over game state plus authored data belongs at the consuming system.

⚠ The two **carve-out** domains answer the UNLOCKED half only, and a consumer treating either as the whole verdict
over-offers: a BUILD's plot-validity half and a PROMOTION's per-unit applicability are evaluated LIVE at their
decision points (§7.1). EMPIRE-capability reads are not here either: they are asked of the PLAYER's own keyed
union ([capabilities.md](capabilities.md)), which no availability read duplicates.

⛔ Still do not re-attach the machine ad hoc — a per-site `can*` rewire is the half-migration this rebuild exists
to avoid ([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)). Moving CONSUMERS onto
this surface is the remaining sweep.

### The gate stages, by domain

The gate verdict is a per-id flag (`setGateFailed`): a failed gate flips a tree member LISTED → GREYED, membership
untouched. **A domain whose gate stage has not landed never sets the flag, so its members stay LISTED** — the
enable-side over-offer, which is a VISIBLE defect to fix, never a reason to fall back to legacy.

Every domain carries all three stages — membership, the `requires` gate, and an `allowed` cap — with the cap
taking its domain's own shape:

| domain | what its `allowed` cap bounds |
|---|---|
| techs | world-unique founder techs |
| buildings | world/team/empire self-caps + the per-city wonder-CATEGORY cap (§4) |
| units | world lifetime-created; empire era-scaled national cap |
| projects · civics · processes · builds | the plain per-scope cap |
| promotions | none — and the gate is on demand, not a maintained flag (§7.1 carve-out) |

**Promotions are the exception to the over-offer:** they set no gate flag, but `requires` + the unit-state
applicability leg (unitcombat QUALIFIED/DISQUALIFIED, game options, promotion-line prereq tech, and the runtime
spy/pillage/commander/commodore/blend + intercept/evasion/XP caps) are enforced ON DEMAND at level-up, so the
promotion offer is not over-inclusive.

### The settled model rulings

- **HAVE model:** the enabler owns NO HAVE store — it ties into the object-owned has-lists that already exist
  (city buildings/religions/corps, player civics/traits/heritages, team techs). Presence stays on the objects; the
  [tally](tally.md) stays the count accessor.
- **Evaluator depth:** `cascadeEvalCondition` reads raw object-owned state (legitimate live reads). What is
  event-driven is the MAINTENANCE — which dependents re-gate, when — never the read source.
- **Component model:** one unified component, instantiated per §7.1 owner; the delta-apply SIBLING of
  `CvDerivedCache`, which cannot operate the same way (§7 — no mark-then-recompute path exists at all).
- **The root rule:** no implicit "no-edge ⇒ available" engine rule. Start-available entities are authored onto
  `TECH_GAME_START`'s `enables` (§2, curator-derived), the tree is fully connected, a missing edge fails closed.
  The load backfill of `TECH_GAME_START` itself is the ONLY engine special case the model needs.
- **The BONUS axis is GATE-ONLY** (owner ruling): a plot-group-carried bonus NEVER drives tree membership. The
  curator keeps authoring bonus `enables` edges (the reverse-mapped view of the target's retained `requires`
  atom), but the runtime consumes bonus events as pure stateless gate re-checks over the bonus's
  `EDGEF_REQUIRED_BY` dependents. Membership rides tech/building/civic edges + the root; an entity whose only
  inbound edges are bonuses ROOTS, sitting visible-GREYED on its bonus requirement. The one carve-out — a bonus ON
  a plot enabling an improvement's placement (`enables.builds`) — is a live per-plot gate, no domain involvement.

### The reverse index, and what is deliberately NOT one

**The canonical reverse axis is `EDGEF_REQUIRED_BY`** ([DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view)),
and a per-id bucket that duplicates it is a defect. ⛔ But the axis-flag lists (power / golden age / state
religion / the coarse religion-civic-tech lists) and the PROPERTY band index are **NOT** convergence targets and
must not be swept into one: the reverse pass deliberately excludes engine tokens, the plot substrate and
`PROPERTY_` bands, and **a coarse list matches a coarse event**. Reading the two populations as one uniform
"operate index" is exactly the mistake the spelled-out naming rule exists to prevent
([Sources/AGENTS.md](../../Sources/AGENTS.md) § Code Style).

⛔ **THE PLOT PLANE CARRIES NO `EDGEF_REQUIRED_BY` AT ALL, AND ITS COARSE LIST IS THE `(kind, id)` PLOT-ATOM
INDEX.** `CvReversePass::rp_requiredByRefInfo` routes nine infotype prefixes and returns NULL for every other,
so **no terrain / feature / improvement / route / mapcategory info ever gains a REQUIRED_BY edge.** The coarse
list this section prescribes is therefore built by the enabler itself: `scanCondDeps` records each substrate id
the `requires` names, and each domain compiles `(PlotAtomKind, id) → candidates` — read by
`onPlotAtomChanged`, fanned over the plot's own `workableByCities()`.
⚑ **A TERRAIN fact also seeds the MAPCATEGORY atoms**, because a plot's categories are derived from its terrain
(`CvPlot::getMapCategories` forwards to the terrain info) and have no fact of their own; `plotAtomSeeds` is the
one place that hop lives.
⚑ **The bare plot BITS ride the verdict fact, not a substrate id.** `HAS_RIVER` / `HAS_COAST` / `IS_WATER` and
their kin name no entity, so they index by their `CASC_PRED_*` id and re-gate off
`SEVT_PLOT_PREDICATE_ADDED / _REMOVED` — which is exactly why that fact exists beside the substrate ones
([event-spine.md](event-spine.md): one says what the tile CARRIES, the other what it MEANS).
⚠ **Reading the empty reverse edge instead FAILS SILENTLY, which is why this is spelled out**: the walk
succeeds, finds nothing, and re-gates nobody — indistinguishable from "no candidate needed re-gating" at every
observation point, including a stored-vs-oracle diff taken when nothing has changed since load. The index
therefore reports its own size at load (`[ENABLER/plotatoms] atomKeys=… atomEntries=…`), so an index that
compiled EMPTY says so.

⚑ **And this is what keeps `GATE_DYNAMIC` meaning what §7.1 says it means.** `scanCondDeps` marks `dynamic` for
any atom it does not NAME, so every axis that later gained a precise route must also gain a case there — or it
keeps marking the catch-all, and the "small load-compiled set" becomes the whole registry (the plot substrate
alone put every building in it, and every fact routed through the class then re-gated everything). ⛔ So when
you wire a new route, remove its axis from the catch-all in the same change; the residue is the genuinely live
state — `existedFor`, `IS_CAPITAL`, the count tokens, connection.

> **⛔ AN AXIS HAS TWO SPELLINGS AND THEY MUST NOT DISAGREE — this is the failure mode, not a tidiness point.**
> `scanCondDeps` meets most axes twice: as a PRESENCE atom (`BONUS_IRON`) and as a PREDICATE
> (`{HAS_BONUS: BONUS_IRON}`). Narrowing one and leaving the other keeps the whole axis in the catch-all while
> the code reads as though it were routed — and the note justifying the surviving half is typically the one
> already retired beside it. ⚑ **Measured: the bonus axis had exactly that split, and closing it took the class
> from 2,674 of 5,180 buildings to 423.** ⇒ When you route an axis, grep BOTH branches.
>
> **⚖ THE THIRD DISPOSITION IS *STATIC*, and forgetting it is what puts a never-moving axis in a live class.**
> §3.2's rule is that an axis either has a fact and is routed on it, or is STATIC for the city's life and gated
> once at creation. A static axis therefore marks **nothing at all** — a plot's LATITUDE cannot change and a city
> cannot move, and a VICTORY condition is fixed at setup, so neither has a crossing to wait for and marking them
> dynamic bought a re-gate that could never change a verdict.
> ⚠ **`existedFor` is the neighbour that is NOT static and must stay in the residue:** the game YEAR advances, so
> an age-gated candidate genuinely crosses a threshold with no fact naming it.
>
> ⚑ **THE CLASS SIZE IS INSTRUMENTED, so a widening is observable rather than suspected** —
> `[ENABLER/gateclass] domain=… class=… members=… of=…` at load, beside `[ENABLER/plotatoms]`. Read `members`
> against `of`: a class approaching the registry size is not a bounded re-gate set, and every fact routed through
> it re-gates nearly everything. ⛔ Do not narrow this class by reasoning alone — the number is one line in
> `Cascade.log`, and the last two attempts to estimate it from the authored JSON were both wrong.

### Load-end reconciliation

- **Neither the counts NOR plot-group MEMBERSHIP are trusted from a save** (membership is derived state: routes +
  terrain-trade capabilities + ownership). The deserialized groups are drained and discarded; a load-end rebuild
  re-colors membership from current state and folds the counts through the live entry points as each plot joins,
  announcing every bonus fact as a genuine crossing emit before the `GAME_LOAD_FINISHED` gate pass.
- **The DORMANCY VERDICT is the operating-building fixpoint** (§3.2,
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
- **The dynamic operate axes ride their events** — connectivity via the plot-group/network bonus events,
  vicinity (radius growth) via the culture-level event — routed into the operate re-check of dependents.

⚠ **A WHAT-IF asker can never iterate the frontier.** The frontier answers the CURRENT verdict only, so a gate
called with hypothetical arguments is served by `EnablerOverlay` (§8, "WHAT THE ENABLER IS NOT") — not by a swap
to `listedIds`.

---

## See also
- [json.md](json.md) — the data this machine reads: `enables`/`obsoletes`/`replaces`/`disables` (§4.1–4.2),
  `requires` build/operate (§4.3), `allowed` (§4.4), and the `all`/`any`/`noneOf` + atom/predicate vocabulary (§3).
- [tally.md](tally.md) — the count machine the `requires` count-atoms and the `allowed` cap read at cross-city scopes.
- [modifier.md](modifier.md) — the sibling "how much?" machine. A dormant/unavailable entity (per this doc)
  simply deposits no modifiers.
- [naming.md](naming.md) — the `INFOTYPE_NAME` ids that fill the `enables` buckets and `requires` atoms.
