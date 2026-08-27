# 3. Pass 2 — GATE each candidate (`requires`)

> Part of the **[enabler](../enabler.md)** spec.

`requires` answers *"do I have the means?"* — checked **forward** (is this atom in HAVE?). It is authored on the
**target**, in two timings ([json](../json.md) §4.3):

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
(an `identity` flag, [json](../json.md) §7).

**⛔ `notConstructible` MEANS ONE THING: IT NEVER GOES THROUGH THE `canConstruct` GATE, EVER.** It is a
statement about the PRODUCTION QUEUE and nothing else — the entity is not offered, not greyed, not evaluated as a
build candidate. ⛔ **It does NOT mean "build it in every city"**, and reading it that way is what this
callout exists to stop.

⚖ **THE INFO SELF-SERVES ITS OFFERABILITY, AS ONE GETTER: *"the info should know itself if it is
offerable to canConstruct — that is literally the getter needed, and then that should be folded in the
enabler."*** `CvBuildingInfo::isOfferable()` / `CvUnitInfo::isOfferable()` are that verdict, and the enabler's
static-exclusion plane folds THE GETTER — never per-flag logic re-assembled at the consumer. ⛔ An
asker-DEPENDENT bar can never live in it (an info does not know who is asking) — and the one legacy
asker-dependent bar, the civ whitelist + NPC lockdown, is a KILLED mechanic
([superseded-ideas #38](../../architecture/superseded-ideas.md)): techs decide what any civ can build, and a
deliberate bar authors as `disables`.

⇒ **WHO places it, and WHERE, belongs to the PLACING SYSTEM — never to this flag.** The property solver places its
bands; `CvGame::setHeadquarters` places a corporate HQ in the ONE city that holds it; the achievement system awards
one per player. Those systems already know their own answer, and the flag's job is only to keep the production
queue out of it.

⛔ **SO A BLANKET "PUT EVERY QUEUE-EXCLUDED MEMBER IN EVERY CITY" PASS IS A DEFECT, NOT THE MODEL.** It hands every
city a copy of entities whose own data says one may exist — a `{world: 1}` corporate headquarters or relic, an
`{empire: 1}` achievement — and `allowed` cannot refuse it, because `allowed` gates a BUILD (§4) and a
queue-excluded entity is never a build candidate. ⚑ **The damage is not confined to over-offering:** an entity
that is ACTIVE in N cities deposits N times, so a scope-wide deposit it carries is multiplied by the city count —
silently, on a plausible-looking number ([modifier.md §5](../../cascade.md)).
⚠ **The place-everywhere population is TWO data-identified classes, never the whole queue-excluded set:
the PROPERTY BANDS (a `requires.operate` PROPERTY band) and the `identity.autoBuild` set** — the legacy per-turn
`doAutobuild` population: the housing ladder, the pests, the resource and presence markers, the civic markers,
the education knowledge bases, the space colonies, the `C_AD_*` culture-adoption markers. Both are placed ONCE
(`CvCity::placeSystemBuildings`, at founding + the load backfill) and their `requires.operate` decides active vs
dormant forever — the band model, which is what deletes both legacy per-turn passes. Every gate axis they name is
already fact-maintained, so no per-turn re-check exists for either.
- ⛔ **A WORLD/TEAM-capped autoBuild member is EXCLUDED from placement** (the enabler's census excludes it): its
  cap is a cross-player RACE — two empires satisfying the gate would both activate a `{world: 1}` entity, and
  `allowed` gates BUILDS, never activations. Such a member is instead **AWARDED FIRST-TO-EARN** by the trigger
  engine: on the facts that can move its gate (a building added, a tech acquired, a population step), the gate is
  evaluated through the ONE evaluator and the cap through the ONE cap comparison (`allowedOk`), and the first
  city to satisfy both receives it as a **genuine first acquisition** (`bFirst = true`, so its one-shot pulses
  fire exactly once through the ordinary ADDED path); thereafter it stands and dormancy toggles its standing
  effects like anything else. `BUILDING_VALLEY_OF_THE_KINGS` is the whole shipped population — its gate is the
  Pyramid AND the Sphinx standing IN THE SAME CITY, which makes the qualifying city world-unique by
  construction. An EMPIRE cap stays in placement: it is per-player, and the shipped members' own gates pin the
  one active city (the C_AD palace atom).
- ⚖ **A system-placed building's CONSIDERED ACTION is its ACTIVATION, never its placement.** It is
  placed with `bFirst = false`, and the trigger engine fires its considered BUILDING-GRANT leg on the
  `SEVT_CITY_BUILDING_ACTIVATED` crossing instead — the live case is `C_AD_*` granting its `C_AC_*` access
  marker on adoption. Re-activation re-fires the leg, and that is safe by construction: the place path skips a
  held target and the empire-level choke point folds to held-once, so the grant is idempotent — and the grant
  PERSISTS when the marker later dorms (losing the adoption keeps the earned access; grants are never
  refcounted, [legacy-grant-apply-sites.md §4](../../reference/legacy-grant-apply-sites.md)).
  ⛔ The one-shot PULSE legs (population / goldenAge / freeTechs) deliberately do NOT fire on activation — a
  building that can wake repeatedly gives them no defined moment, which is the second reason the world-capped
  member above is excluded rather than band-placed.

⚠ **A pseudobuilding representing a CHOICE (an ordinance ENACTED, a culture HELD, a folklore requirement) was
the second, separate defect of the per-city placement: present everywhere AND active everywhere, its
`requires.operate` naming only a tech and a map category — never the choice itself.** The empire-level move
(§2, [empire-level buildings](02-pass-1-generate-the-frontier-the.md#2-pass-1--generate-the-frontier-the-enables-family)) resolves it
structurally for that class: the player HOLDS the marker iff the choice was actually made, so holding IS the
choice and no per-city active-everywhere state exists to get wrong.

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
([recurate on every decision](../../../AGENTS.md#git--delivery)).
⚑ The folded position is strictly MORE correct than the one it leaves: `operate` is re-checked every recompute, so
the entity correctly dorms if the ground it needed stops existing (terrain levelled to sea level — the WMD case),
which a checked-once `build` clause could never do.

⚠ **Cost, for the population a placing system genuinely does put in every city (the bands + the autoBuild
set):** it allocates nothing new — the per-city building arrays are already dimensioned by `NUM_BUILDING_TYPES`
([memory-footprint.md §2](../../reference/memory-footprint.md)) — and it is not a per-turn cost, because the operate
fixpoint is targeted-propagation maintained (§3.2) and re-walks only what an event touched — each building
resolving its own dormancy as it arrives, once. ⛔ That is a cost argument for the two DATA-IDENTIFIED
populations, and it was never a licence to widen placement to the whole queue-excluded class.
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
> separates them ([engine.md](../../reference/engine.md): the per-turn add/remove maintainer is CUT), so the test has
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
**game options** → the **ENTITY-LEVEL `enabled`/`disabled` gate** ([the whole-entity applicability gate](../json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity) — e.g. the inquisitor's
`"enabled": "GAMEOPTION_RELIGION_INQUISITIONS"`),
evaluated live against the active options; `requires` holds only genuine needs; a **unit** corp prereq →
`{HAS_CORPORATION: X}` = **active** (`isActiveCorporation`), distinct from a building's bare `CORPORATION_` = present.
No `canTrain` gate logic is re-mirrored from the engine — every divergence is a missing input mapped to its named source.

**VICINITY** (enabler-specific) = the city's current workable radius, which **grows with culture** (1→2→3 rings),
NOT fixed; a plot can lie in two overlapping cities' vicinity (counts for both). The plot scan carries a
**city-relative semantic** (`VICINITY ⊇ WORKABLE ⊇ IS_WORKED`, [json](../json.md) §3.5): `VICINITY` = in the radius;
`WORKABLE` = in radius **and owned/eligible-to-work**; `IS_WORKED` = a citizen works it. The engine's gates pick the
level — the workable-plot predicates (`evp_terrain`/`evp_improvement`/`evp_route`/`evp_peak`/`evp_hill`,
`Conditions/CvConditionEval.cpp`) require an **owned** plot (= `WORKABLE`), while `evp_feature` also accepts a
neutral plot unless `EXP_STRICT_VICINITY` is on. **A `vicinity:"onSite"` atom asks the strongest of these: the
resource is AVAILABLE here — an OWNED radius tile whose IMPROVEMENT trades it, or an active building supplying it
([json.md §5a](../json.md)).** ⛔ It does NOT ask the network: onSite and `connection:"trade"` are ORTHOGONAL, so a
resource can be either without the other ([json.md §3.4](../json.md)).

### 3.1 The cache-friendly two-stage evaluation

Every `requires` resolves the same way, so it's cacheable as a pure function of clause-shape + state:

1. **combinator** — the `all`/`any`/`noneOf` structure ([json](../json.md) §3.4): **`all` = AND** (`&&`), **`any` = OR**
   (`||`), **`noneOf` = NONE**, each over its **direct children** (a leaf, or a nested `all`/`any`/`noneOf` node — a
   recursive boolean tree). Parsing routes through the ONE typed-condition parser (`cascadeParseCondition` →
   `CvCondition`, the StoneBase `ConditionParser` port) and evaluation through the ONE evaluator
   (`cascadeEvalCondition`) — never reinvent and/or ([superseded-ideas](../../architecture/superseded-ideas.md) #5:
   the AND-of-ORs `any:[[…]]` shape and hand-rolled `vector<vector<leaf>>` were exactly that mistake).
2. **conditions** — each leaf: a presence/count **atom** (`min`/`max` at a scope) or a **predicate**. A count at
   `city`/`plot` reads the live object; at `empire`/`team`/`world` it reads the [tally](../tally.md). A missing
   predicate is **ignored**, never false (json §3.5) — so retiring a system never spuriously disables data.
   **Tally-bucket routing is by TYPE PREFIX** (`BUILDING_`/`UNIT_`/`BONUS_`/…), no separate `kind` field; author
   resource presence as `min(BONUS_X,1)` (the N=1 case) — volumetric-ready.

### 3.2 The operating-building set — what the modifier reads

As a byproduct of the dormancy gate, the enabler maintains, per city, the **operating-building set**: the
buildings that are present **and operating** (`requires.operate` holds ∧ no dormant-trigger successor present),
plus the **bonuses those operating buildings supply in-vicinity** (`provides.bonuses`, [json](../json.md) §5a). The
two form one **least-fixpoint** — an operating building's `operate` can consume a bonus another operating building
provides, so an operating/dormant flip ripples.

This set is the enabler's output the **[modifier](../../cascade.md) reads to decide which buildings deposit**: an
operating building contributes its modifiers, a dormant one contributes nothing. It is the built-instance
counterpart of the frontier (§2 — the frontier is "what can I build"; this is "of what I've built, what is
operating right now").

It is **maintained by targeted propagation, never a blanket recompute**: each HAVE-change ripples only the
affected buildings into the authoritative set in place (via an operate reverse-index)
— see [cascade.md](../../cascade.md). In code it is
`CvCity::m_operatingBuildings` (type **`OperatingBuildings`** — its `active` + `provided` + `obsolete` sets), read via
`EnablerKernel::operatingBuildings` / `wireOperatingBuildings`.

> **⛔ THERE IS NO LOAD SEED — THE SET IS BUILT BY THE FACTS, LIKE EVERYTHING ELSE.** A full per-city
> recompute ran at `GAME_LOAD_FINISHED` and is DELETED. The game objects and their contexts exist before the
> facts flow — the save could not load otherwise — so a building announces its presence as it deserializes,
> resolves its own dormancy there, and every HAVE axis (bonus / vicinity bonus / religion / corporation /
> population / power / building) re-checks the consumers of what it supplies. A manufactured chain therefore
> lights tier by tier AS THE STREAM RUNS; there is nothing left for a rebuild to discover.
> ⚑ **What the recompute actually cost, measured:** it forced the in-read ANNOUNCE to be suppressed (otherwise
> the load-end re-announce double-applied every deposit), so the ENABLER was event-built while the CASCADE saw
> no operating verdict at all until after the bracket — 102k activations and every deposit landing in one burst,
> off a set the facts had already converged 55 seconds earlier. **The cascade and the enabler must build on the
> SAME SEEDS**, and two compensating hacks were what stopped them.
> ⛔ Do not reintroduce either half. A guard must never suppress an emit
> ([spine.md](../../spine.md) § THE RECEIVED LINE), and a recompute beside an event-built set is banned
> outright ([the load reseed](../../spine/05-the-load-reseed.md#5-the-load-reseed): it "may never survive beside the
> setters"). ⚠ Order is not what makes this safe — a package is additive, so arrival sequence is irrelevant
>; what matters is each fact arriving EXACTLY ONCE, which is precisely what a second builder breaks.

> **⛔ THERE IS NO PER-TURN RE-CHECK OF ANY KIND, AND A "BOUNDED" ONE IS NOT AN EXCEPTION.** A sweep that
> re-gates a set once a turn — however small the set — **jumps over the core system**: the fact is what moves a
> verdict, and a periodic pass is a second maintenance surface running beside it. It is
> [self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) (no blanket per-turn rebuild) and
> [a staleness flag is the fossil of a missing emit](../../cascade/03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) (a periodic re-check ASSERTS that we
> cannot know what changed, which a saturated emit surface falsifies by construction).
> ⚑ **Its real cost is not the cycles, it is the CONCEALMENT:** a sweep silently repairs the verdict a missing
> route left wrong, so the gap stops being observable and the enable-side over-offer that would have named it
> never appears. ⇒ **Over-offer is always the same diagnosis — a fact that is not being read** — so the fix is
> the ROUTE, every time.
> ⛔ So a candidate whose `requires` reads live state does not earn a sweep: either its axis has a fact and is
> routed on it, or the axis is STATIC for the city's life (a plot's latitude, a victory condition) and is gated
> once at creation. Nothing in the authored data falls outside those two, and a future atom that appears to must
> get its fact ([an event gap is closed the moment it is found](../../spine/03-the-domain-emit-surface-every-fact.md#-a-fact-names-the-happening--something-changed-is-not-a-fact)), never a
> re-check.

**Obsolescence is the THIRD outcome of this same pass.** A present building whose `obsoletedBy` tech is held is
neither active nor dormant — it goes into the `obsolete` set (excluded from `active`, provides nothing), and the
[modifier](../../cascade.md) reads its **`whenObsolete`** tree (§2 / [json](../json.md) §4.2) in place of its normal
families. It is maintained by the same targeted propagation (an `obsoletedBy.techs` reverse-index re-checked on a
tech change), read via `cascadeIsBuildingObsolete`.

⛔ **THE INSTANCE'S FATE IS DECIDED BY `whenObsolete`, AND THERE ARE EXACTLY TWO:** an **absent/empty**
tree means the building is **HARD REMOVED**; a tree **carrying any modifier** means the building **STAYS** and
that tree **TAKES OVER** from its normal families ([json.md §4.2](../json.md)). So this `obsolete` set is the
**tree-carrying population** — present, non-active, depositing `whenObsolete` — never the removed ones, which
are not in the city to hold.

⚖ **A TECH IS THE ONLY THING THAT CAN OBSOLETE, which is what makes the whole fate purely EVENT-DRIVEN
and needs no fact to DRIVE it.** When a tech lands, the buildings it obsoletes are checked and each does what it
needs to do — so the apply lives on the TECH fact, in the enabler's `onTechChanged`, beside the edge application
that already runs there.

⚖ **AN "I HAVE BEEN OBSOLETED" FACT IS WELCOME — but it is PURELY for LOGGING and the NOTIFICATION,
never the mechanism.** That is the [spine.md](../../spine.md) player-alert shape exactly: the alert is a
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

