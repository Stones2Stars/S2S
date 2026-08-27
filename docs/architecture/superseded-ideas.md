# Superseded ideas — the don't-revive registry

> Dead approaches kept so they aren't reinvented ([the keep-unkilled-ideas policy](../plans/parked/README.md#parked--out-of-active-scope-plans-kept-for-intent)). Condensed — one entry
> per dead idea: what it was, why it's dead, what replaced it.

1. **Derived-data repository pattern** *(mostly obsolete)* — a `TLazy` / version / dirty aggregation layer. Killed:
   the cascade + tally subsume it (tally counts UP, modifier magnitudes DOWN). One residual: AI-heuristic caching
   (plot danger, unit-AI counts) is separate + out of scope. **Don't revive the repository as a
   data/derived-aggregation mechanism — that is the cascade's job.**
2. **Cross-entity inversion** *(dead)* — ~37 inversions that physically moved cross-entity modifiers onto the keyed
   target entity. Killed by [the deliveryguy ownership rule](../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule): the deliverer owns the modifier keyed by target, not
   inverted onto it ([modifier](../cascade.md) §4). **Don't reinstate inversion**,
   even for Terrain/Improvement/Bonus targets.
3. **`loadPrune`** *(dead)* — a curator-era INVENTION: the legacy
   `OnGameOptions`/`NotOnGameOptions`/`PrereqGameOption` validity tags re-encoded as a bespoke "prune at load"
   section, named BACKWARDS (`onGameOptions` meant *keep only when on*) and spec'd wrong, while the spec already
   had the answer (`GAMEOPTION_X` as an ordinary condition). Killed whole: the payload
   authors as the **entity-level `enabled`/`disabled` gate** ([the whole-entity applicability gate](../specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity), json.md §2); the
   complex-trait entries dropped outright (they restated the simple/complex FOLDER split, which is the selection
   mechanism). **Don't revive a bespoke game-option section.**
4. **The offline DRY CALCULATOR — all four attempts** *(dead as an approach)* — an out-of-process reimplementation
   of the cascade/enabler calculation, used to judge the engine. Four were built: (1) full legacy-calc-pipeline
   offline emulation, (2) the Python per-scope/combine calculators, (3) the first-version .NET validator, and
   (4) **StoneBase**, whose original purpose was exactly this. All are retired — it proved easier to dump
   individual calcs from the game itself, and a dry calculator that judges the spec while drifting from it
   corrupts the loop it is meant to close. Verification is LIVE — done-is-observable endpoint polls
   ([done = observable in the running game](../specs/validation.md)) and turn time
   ([turn time is king](../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)), and for the cascade the THREE-LEG check — the
   LOGS, the JSON INFO, and what STATE expects, all three agreeing
   ([http-endpoints.md](../specs/http-endpoints.md)); the zero-ride-in principle still
   holds ([the pollution guardrail](../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)). **Never build a fifth dry calculator** —
   StoneBase was the fourth, and the approach is what died, not any one implementation. *(StoneBase itself lives
   on in other roles — it is the dry-calculator/verification job that is over.)*
5. **The `any:[[…]]` "AND-of-ORs" condition shape** *(dead)* — `any` holding lists-of-lists to mean "OR-groups
   AND-ed together". Killed: a condition is a plain **recursive boolean tree** (`all`/`any`/`noneOf`, nestable —
   json.md §3.4); "(A or B) AND (C or D)" nests two `any` under an `all`. The temporary hand-rolled
   `vector<vector<leaf>>` parser was the same mistake — route through `BoolExpr`. **`any` never means AND.**
6. **The `byEra.{C2C_ERA_*}` value-table key** *(dead)* — an agent-invented bespoke era band-table. Killed: era is
   the plain `ERA` counter; era-dependent values are ordinary conditioned deposits on an `ERA` threshold
   (json.md §6). **No bespoke era key.**
7. **Condition-carrying sub-scope members** (`empire.capital`, `perMilitaryUnit`, …) *(dead as a class)* — encoding
   a deposit's condition as a bespoke member instead of a predicate/`unit:` qualifier. Killed by
   [conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query) (the golden-age yield-effect member-mirror is the one PERMANENT exception).
   `perMilitaryUnit` specifically authors as the `cities.{unit: IS_MILITARY}` entry (json.md §3.7).
8. **The "deliberately more permissive" vicinity model** *(dead)* — vicinity with no ownership filter. Killed:
   vicinity mirrors the engine's ownership tiers (owned ⊂ owned+neutral ⊂ crossBorder; json.md §3.4, enabler.md §3).
9. **The tally as a store** *(dead)* — a tally-owned accumulator / load-time event-replay rebuild duplicating the
   object-owned counts. Killed: the tally is a read-only accessor + roll-up over the objects' own counts
   ([tally.md](../specs/tally.md), [the tally serializes nothing](../specs/tally.md#4-it-serializes-nothing--and-maintains-nothing-it-reads)). **Never re-add a tally-side store,
   seed, or shadow.**
10. **`grants.specialists` for an ALIVE-WITH-SOURCE specialist** *(dead)* — the ordinary building/civic free
    specialist authored as a grant. Killed: those are the `freeSpecialists` MODIFIER family, which dies with its
    source ([modifier.md §6](../cascade.md)).
    ⚠ **The key itself is NOT dead — the LIFETIME is what was killed.** The spec reserved a carve-out for
    anything that "genuinely GRANTS permanent free specialists, surviving the destruction of its source", and
    that case exists: the trait's ERA-ADVANCE specialist is a persisted PULSE landing in the city's
    UNATTRIBUTED typed-free ledger, so it outlives the trait. It authors on the TRIGGER plane
    (`onEraChanged` → `action.grant.specialists`), never as an entity-level grant
    ([json.md §5](../specs/json.md)). ⛔ Do not read this entry as banning that shape, and do not "restore"
    the ban over it — the discriminator is whether removing the source removes the specialist.
11. **Graded parity tolerance (the six-rung "care scale")** *(dead)* — grading a divergence's acceptability. Killed
    by [the completeness+attribution bar](../specs/validation.md#the-observation-surface): exact match, no tolerance band, no agent grading; a divergence is a
    data-collection gap to close.
12. **The `/shadow/*` endpoint surface** *(dead)* — in-DLL cascade-vs-legacy sweep endpoints. Killed: the two
    verification legs never mix surfaces ([validation.md](../specs/validation.md)); the shadow rode the gated
    logging, and the shadow phase itself has since ended.
13. **The load reseed as a fabricated full-state replay (`spineEmitGameState`)** *(dead)* — a separate pass, run
    after deserialization, that walked already-populated game objects and emitted a synthetic DOMAIN event for every
    present fact ("for each building the city has, emit built"). Killed: it FABRICATES events from populated state
    rather than the events coming from the genuine save read — a pseudo-emit that feeds the cascade reconstructed
    values and invites the next agent to reconstruct more state the same way. The reseed must be **event-sourced from
    inside the read** ([spine.md](../spine.md) § The load reseed, [the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)):
    reading a fact off the stream is what fires its event. **Never re-add a post-deserialization state-walking emit
    pass.**
14. **The bespoke per-scope modifier SUBSTRATE** (`CascadeAccumulator` + `CascadeCityPackages` /
    `CascadePlayerScope` / `CascadeAreaPackages` / `CascadeTeamCaps` / `CascadeUnitPackages`, the `CPK_*`/`PSC_*`
    box slices, `CascadeRateSlots` + epochs, `playerSliceRebuild`) *(dead)* — five hand-shaped structs with
    hand-named per-channel scalar members, each carrying its own bespoke invalidation path, reached through a
    read-side `ensure()` protocol. Killed by [every derived cache is one shape](../cascade/04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta): every
    derived cache is the SAME object type on every owner (one channel-indexed `CvDerivedCacheSet<TOwner>`, one mark
    derivation), so a hand-named scalar field is a DEFECT and a new scope/channel is DATA rather than a new struct.
    The whole tree is archived (`SourceArchive/Cascade/`). **Never re-add a per-scope package struct, an
    `ensure`-on-read protocol, or a `*Rebuild` blanket** — the replacement is the uniform channel-indexed cache on
    each scope owner ([cascade.md](../cascade.md)).
15. **Re-bodying the legacy getters to read the cascade (the "computed-getter flip")** *(dead)* — keeping each
    legacy getter's signature and swapping its body to a cascade read, so no call site changed. Killed by
    [build a new getter surface, never widen a legacy one](patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers): a legacy getter's contract encodes legacy
    scale/granularity/combine, so pointing the cascade at it makes the CASCADE bend to the legacy shape — the
    mechanism that produces the half-migrated state. **A change that leaves every consumer untouched is the tell,
    not the win.** The replacement is a NEW uniform parameterized getter set over the channel index, with the old
    surface disconnected.
16. **One shared spine consumer routing BOTH machines** (`CvCacheInvalidationConsumer` — enabler deltas and
    modifier marks in one `onEvent`) *(dead)* — it welded the two systems the docs work hardest to keep apart, and
    forced one load-suppression policy onto two that genuinely differ (the enabler is load-ACTIVE, the modifier
    build is not). Killed by [the enabler and the modifier cascade are two separate systems](../specs/enabler.md): **one consumer per
    system**. `enablerRegisterConsumer` is the enabler's own; the modifier gets its own when it is rebuilt.
17. **The `*Legacy` / `*Recomputed` / `*Leg` ORACLE-TWIN surface** *(dead — and one of the reasons the hard rebuild
    was forced, owner)* — per-channel comparison getters + `/computed` twin fields, kept so a cascade value could be
    diffed against a "legacy" one. It rotted twice over: agents **cheated the comparison by sneaking legacy-computed
    data into the cascade calc** so it could not fail (the abuse [the pollution guardrail](../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)
    now bans outright), and once the legacy accumulators were deleted both sides read the same derivation, so the
    check could never turn red at all. The rebuild removed the whole surface — **zero `*Legacy`/`*Recomputed`
    symbols remain in `Sources/`** — so this is solved STRUCTURALLY, not by a standing rule; the ledger entry that
    policed it (`DEC-oracle-tautology`) is retired with it. **Never re-add a comparison getter or a `/computed` twin
    field.** ⚠ It does NOT follow that comparison is banned: a check whose two sides are genuinely different
    derivations — **event-built state vs a fresh recompute-from-source**, served on two endpoints and diffed
    OUTSIDE the DLL — is the missed-emit tripwire and is the sanctioned shape
    ([cascade.md](../cascade.md)). What is dead is the same-derivation twin, not verification.
18. **The whole-domain enabler frontier + implicit "no-enabler ⇒ always-available" rules** *(dead as a class)* —
    workarounds for entities with no inbound `enables` edge (PALACE, PROCESS_IDLE, the COMBAT1-5 promotions):
    making the frontier ALL entities of the domain gated by `requires`, or hardcoded always-unlocked whitelists
    (the promotion "PALACE-whitelist"). Killed: the tree is **fully connected** — start-available entities are
    authored onto the `TECH_GAME_START` root's `enables` (curator-derived, fails closed;
    [enabler.md §2](../specs/enabler.md)), the long-specced root model these workarounds skated around.
    **Never re-add a whole-domain frontier or an implicit availability rule.**
19. **The GATED IN-DLL CACHE VERIFIER** (a read-side `verifyIfGated` behind a log-level gate, recomputing over the
    stored slot, comparing, emitting a `SEVT_CACHE_DIVERGED` spine event, then restoring) *(dead)* — the read-side
    `ensure()` reincarnated as a diagnostic. Killed on both halves: it put a gate test back on a read that must be
    a bare fetch, and it made a divergence an in-DLL HAPPENING — an event is an invitation to a consumer, and the
    next agent's consumer "handles" a value known to be wrong by CORRECTING it, so the shape itself licenses
    self-heal ([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). ⚠ What this entry once named as its
    replacement — a recompute-from-source served on a second route and diffed outside the DLL — is itself dead
    (#33), so nothing here nominates one: a divergence is found by the THREE-LEG check
    ([http-endpoints.md](../specs/http-endpoints.md)). **A divergence has NO in-DLL representation — never re-add a
    diff, a log line, an event, or a field for one, and never snapshot-and-restore a stored slot.**
20. **The per-turn `(scope,channel)` CALC-COUNT GATE** (every calculation counting itself by scope and channel; the
    per-turn total a standing acceptance gate + regression tripwire, ~50k the breach line, the histogram naming the
    culprit) *(dead)* — it existed to catch a blanket recompute or a per-read walk creeping back, which was a real
    risk while a READ could trigger a recompute: the `ensure()`-on-read protocol (#14) coupled reads to
    calculations, so millions of reads could mean millions of calcs and only a count could tell you. The rebuild
    removed that coupling — a read is an unconditional BARE FETCH and the only path to a rebuild is a mark — so the
    count collapses onto mark volume, which is event volume and is already visible on the spine. It measures
    nothing it did not already say, and the failure it policed is no longer representable: solved STRUCTURALLY, not
    by a standing measurement. **Never re-add a calculation counter, a per-turn calc budget, or a ratio derived
    from one** — the live acceptance signals are done-is-observable endpoint polls
    ([done = observable in the running game](../specs/validation.md)) and turn time
    ([turn time is king](../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)), and no successor metric replaces the gate.
21. **The BLANKET MODIFIER RECALCULATION** (a whole-world wipe-and-reapply pass: zero every accumulated total on
    game/team/player/city/area/plot, re-run every tech, civic, trait, building, religion, corporation and event,
    fronted by a "should the modifiers be recalculated?" popup on an asset-checksum mismatch, plus a hotkey and a
    net message to carry it) *(dead)* — the archetypal self-heal
    ([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). It existed to purge derived data that had drifted **in a
    save**, which no longer happens: no cache is serialized, so LOAD rebuilds everything from source and there is
    nothing to purge. Worse than a generic blanket, it fired precisely on the saves most likely to have drifted,
    silently papering over the missed invalidations the event spine is built to EXPOSE. The asset checksum gates
    nothing — not OOS, not loading ([engine.md](../reference/engine.md)) — so a mismatch has no action to take.
    **Never re-add a recalculate-everything entry point, a wipe-the-totals helper, an "are you sure you want to
    recalculate" prompt, or an in-recalc suppression flag that makes ordinary mutators skip their work.**
22. **MIRROR-THEN-REDESIGN** — *"the migration reproduces the engine's existing behaviour exactly; behavioural
    redesign is deferred to post-migration"* *(dead — retired as `DEC-mirror-then-redesign`)*. It was **dead by its
    own construction**: it presupposed (a) a legacy implementation worth faithfully mirroring and (b) a LATER
    phase in which redesign unlocks. Neither exists — the legacy surface is being **NUKED, not mirrored** (the ~622
    channel-shaped getters are a DELETION list, [build a new getter surface, never widen a legacy one](patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)),
    parity and shadow are closed, and there is no post-migration phase to hand work to
    (["deferred" is banned](../../AGENTS.md#design)). **The SPEC leads, now:** where code and spec disagree the
    spec is right and the code is the defect. ⛔ Never re-argue that a shape must be preserved because it is what
    the engine does today — "this is how it works" carries no weight without a live named reason (a spec
    requirement, the EXE calling in, save state, a real ordering dependency). A behaviour change is a fact to state
    and weigh, never a thing to defer.
23. **The ROUTE flat-movement cost** (`iFlatMovement` → `CvRouteInfo::getFlatMovementCost`, consumed in
    `CvPlot::movementCost` as `min(routeCost, flatCost × unit->baseMoves())`) *(dead — owner: "kill the
    mechanic")*. It guaranteed every unit a FIXED tile count along a route regardless of its own moves
    (`MOVE_DENOMINATOR / flatCost`), by scaling the per-step cost with `baseMoves`. Sitting inside a `min()` it
    was a FLOOR, never a cap — a fast unit was not held back, it simply gained nothing. **13 of the 21 authored
    routes had `flatCost == moveCost`, which makes it mathematically inert** (it binds only when
    `baseMoves < moveCost / flatCost`, i.e. below 1), so it did anything at all only on the rail-and-tunnel
    class. Engine path, route getter, Cy binding, both help composers and the data are all removed.
    ⚠ **NOT the same thing as the UNIT skill of the same name** (`CvUnitInfo::isFlatMovementCost`, "every tile
    costs 1 movement", [skills.md](../specs/skills.md)) — a different mechanic, which STAYS.
    **Never reinstate the route-side one.**
24. **RANGED BOMBARD and OPPORTUNITY FIRE** (`MISSION_RBOMBARD` + `INTERFACEMODE_BOMBARD`, `canRBombard`/
    `canBombardAtRanged`/`bombardRanged`/`rBombardCombat`, the `rBombardDamage`/`…Limit`/`…MaxUnits` +
    `DCMBombRange`/`DCMBombAccuracy` stat quartets, `doOpportunityFire`, and the `DCM_RANGE_BOMBARD` /
    `DCM_OPP_FIRE` / `DCM_RB_*` / `DCM_AIR_BOMBING` globals with their BUG options)
    *(dead — owner: "dcm is stone dead, and we need to redesign ranged bombard from the ground up, so drop
    it")*. It **broke the AI** rather than merely underperforming: the bombard step was a turn-satisfying
    TERMINAL, so a stack that could plink did, reported progress, and never reached the commit-or-withdraw
    decision — armies camped outside cities for eras feeding near-zero-damage strikes (the #410 pseudo-progress
    class, [AGENTS.md](../../AGENTS.md)). The data had already stopped arriving: legacy records still author the
    damage values and the curator emits NO key, NO kind and NO entry for any of them, so every consumer read a
    member that could not exist. ⛔ **Ranged bombard RETURNS as a ground-up redesign, so nothing here is a
    starting point** — do not revive the members, the AI terminals or the DCM globals to "build on", and do not
    mint kinds for the old shape ([build the proper structure once](../../AGENTS.md#design)).
    ⚠ NOT the same thing as the ordinary `bombard` FAMILY (`bombard.unit.rate` / `airBombRate`), which is live,
    authored and STAYS.
    ⚖ **THE RULE THAT DECIDES THE BOUNDARY: *"if it uses the ranged attack, and is not an airplane, it
    goes — vanilla airplanes have ranged attack."*** That is the whole test, and it is what makes the split
    re-derivable instead of memorized: **AIRPLANE ranged attack is vanilla and STAYS** (fighter engage — a
    first-class `MISSION_FENGAGE` with its own interface mode and pedia concept; and ACTIVE DEFENSE, which runs
    on `airStrikeTarget`/`airCombatDamage`/`MISSION_AIRSTRIKE`). **Non-airplane ranged attack GOES.**
    ⚖ **KEPT vs DROPPED — the cut is by MECHANIC, never by name.** The DEFENCE-GRINDING bombard stays
    exactly as it is: a unit adjacent to a city wearing its defences down (`MISSION_BOMBARD` → `bombardRate` →
    `getDefenseDamage`), and **NAVAL units keep the bombards they have**. ⛔ **Three naming traps sit on
    this boundary, and each has already misled a sweep:** `AI_bombardCity` (defence grinder, STAYS) is ONE LETTER
    from `AI_RbombardCity` (ranged, gone) and the naval path called BOTH in sequence; **`INTERFACEMODE_BOMBARD`
    was the RANGED targeting mode despite its name**, while `INTERFACEMODE_AIRBOMB` is the vanilla one that
    stays; and the `dcm` prefix marks mod PROVENANCE, not membership — `dcmFighterEngage` is a live vanilla
    mechanic wearing it. Decide every one of these by what the code DOES, never by what it is called.
    ⚑ Opportunity fire went with ranged bombard because it *gated on the same `getDCMBombRange()` stat*, and its
    own author's comment records why it deserved to: *"absolutely zero resistability to this damage and no
    potential for failure to strike, making it far more powerful than any player determined action."*
    ⚖ **WHAT THE REDESIGN OWES:** *"we basically want vanilla civ bombard back"* as the baseline, and
    **ranged attack has to DO SOMETHING to be worthwhile** — the retired failure is not that ranged existed, it
    is that it dealt ~nothing while still satisfying the turn, so a redesign that reintroduces a near-zero-damage
    ranged action has reproduced the bug. ⚑ Naval shore bombardment is a DELIBERATE divergence from vanilla,
    which did not allow it — without it these ships are near worthless.
25. **The PER-INSTANCE unit build-cost ramp** (`iInstanceCostModifier` → `costs.empire.perInstance` with
    `per:{SELF}`, consumed in `CvPlayer::getProductionNeeded(UnitTypes)` as
    `productionNeeded × unitCount(eUnit) × modifier`) *(dead — owner: the concept "is dumb in the first place,
    it was made as a balancing mechanic, but all it did was make units unreasonably expensive")*. Each unit of a
    type already owned raised the hammer cost of the next one. ⚑ **The data shows it was never balanced at all:
    a flat 5% on ALL 582 authoring units** — no per-unit tuning existed to preserve, so the drop loses no design
    intent. Curator emit, the 582 authorings, and the engine consumer are removed; the legacy XML tag stays
    listed in the curator's HANDLED set as knowingly dropped. ⚠ NOT the same thing as unit UPKEEP scaling
    (`cost.upkeep` → `upkeep.unit.extra`), which is a different family and STAYS. **Never re-add a
    count-scaled build-cost ramp**; if unit proliferation needs a brake, it is a fresh design decision, not this.
26. **The per-unit upkeep PERCENTAGE stacked on top of Size Matters** — the promotion/unit-combat
    `upkeep.unit.modifier` (`iUpkeepModifier`, 119 promotions + 10 unit-combats, mostly +10% but up to +50%)
    multiplying the same upkeep the SM rank multiplier (`m_iUpkeepMultiplierSM`) already scaled *(dead —
    owner)*. Both stages are removed; unit upkeep is FLAT.
    ⛔ **THE SM MULTIPLIER WAS NOT THE FAULT, and blaming it is the wrong lesson to take.** Size Matters
    FUSES 3 equal units into 1 bigger one, so a bigger unit costing more upkeep *"makes sense"*. The arithmetic
    agrees: the multiplier is ×1.5 per rank while a rank represents 3 fused units, i.e. a fused unit paid 1.5×
    the upkeep of one unit while BEING three — a discount against fielding them separately, not a punishment.
    ⚑ **The failure was COMPOSITION:** adding the per-unit scaling into the mix on top is what got it out of hand. A defensible per-size cost and an unbounded per-unit percentage
    multiplied each other, and the product is what made armies unaffordable.
    ⚖ **FLAT is an INTERIM, not the destination.** Unit maintenance is owed a coherent redesign; this removal
    clears the incoherent version rather than settling the model. A standing example of what that redesign must
    address: **FREE UNITS did not take Size Matters into account** — the free-unit allowance counted units
    while SM changed what a unit IS.
    ⚠ NOT the empire-scope upkeep modifiers — the TRAIT/civic one (`upkeep.empire.civic`,
    `CvPlayer::m_iUpkeepModifier`) and the HANDICAP scaling are different mechanics at a different scope and are
    untouched. The `UPKEEP_MODIFIER` kind is retired with the mechanic; both members were serialized, so both are
    named in `Assets/savemigration.txt` with NO replacement recorded, deliberately.
    ⛔ So: **do not re-add a percentage multiplier on per-unit upkeep**, and equally **do not "restore" the SM
    multiplier on the belief it was the problem** — it goes back, if at all, as part of the maintenance redesign.
27. **The CARRIED-CARGO stat contribution** (`CvUnit::processLoadedSpecialUnit` — a loaded `SPECIALUNIT_*`
    applying its own combat percent and withdrawal change to the TRANSPORT, refreshed on every load/unload)
    *(dead — owner: *"it creates complexity for no real gain"*)*. Only `SPECIALUNIT_CAPTIVE` ever authored it
    (`combat.unit.percent −5`, `withdrawal.unit.percent −10` — a hauling-prisoners malus), and it had ALREADY
    stopped working: both of its `change*` calls wrote to members the accumulator cut had deleted, so the
    penalty applied to nothing.
    ⚑ **Why it does not come back as a live fold, which is the tempting move:** cargo is neither a promotion nor
    a combat-class change, so the unit RESOLVED plane cannot gather it — nothing would ever dirty the slot — and
    the correct shape would therefore be a per-read walk of the transport's cargo
    ([unit-carried modifiers apply on top, live, never cached](../cascade/09-wellbeing-channels.md#2b-the-wellbeing-channels--health--happiness-signed-split-the-2a-sibling): a modifier that TRAVELS is folded live
    on top). That is real per-read work on the combat path for one authored entity.
    ⚖ **It is also on the way out wholesale: land units carrying other land units "and all those
    shenanigans" go post-rework**, so the mechanic this served is itself scheduled for removal.
    ⚠ **The authored data STAYS in `specialunit_captive.json` and is now read by nothing** — do not read its
    presence as a wiring gap to close. ⛔ Never re-add `processLoadedSpecialUnit`, and do not re-home its two
    stats onto the resolved plane.
28. **The `PROMOTIONLINE_FERAL` TERRITORY LADDER** — three per-unit tiers of animal border-ignoring
    (`canAnimalIgnoresBorders` / `…Improvements` / `…Cities`, tested as a stored count `> 0` / `> 1` / `> 2`,
    fed by `PROMOTION_FERAL2` (+1) and `PROMOTION_FERAL3` (+2) *(dead — owner: where animals may go is decided
    by the GAME OPTIONS entirely)*. `GAMEOPTION_ANIMAL_STAY_OUT` bars them from national borders,
    `GAMEOPTION_ANIMAL_DANGEROUS` admits them to borders and improved tiles; **FERAL2 and FERAL3 no longer
    differ on territory.**
    ⚑ **The ladder was already unrecoverable from the data, which is what forced the question:** a skill is a
    pure boolean ENABLER carrying no value ([skills.md](../specs/skills.md)), so the curator collapsed the
    legacy `+1` and `+2` alike to a plain grant — the rung distinction does not survive into the JSON at all.
    ⚠ The FERAL promotions and `PROMOTIONLINE_FERAL` **stay** and still author the skill; only the per-unit
    TERRITORY tiering is gone. ⛔ So do not read those authorings as a wiring gap, and do not reconstruct the
    tier from the promotion rung — the rung is available (the line models it, and the accrual sums down it),
    which is exactly why it looks like a fix and is not one.
    ⚠ Consequence to know rather than rediscover: nothing now grants an animal CITY entry except
    `ANIMAL_DANGEROUS`, whose own help text stops at improved tiles.
29. **The HIDDEN-NATIONALITY CAPTURE MARK** (`bSetOnHNCapture` → `CvUnit::doHNCapture` /
    `removeHNCapturePromotion` / the serialized `m_bHasHNCapturePromotion`, plus the `CyUnit` wrapper method)
    *(dead)* — a unit captured BY a hidden-nationality unit was to be given a promotion flagged for it, stripped
    again once that unit stood in its owner's own territory.
    ⚑ **It never had data, and the shape of the absence is the point:** the tag exists in the unit SCHEMA and
    nowhere else — **no promotion record has ever carried it** — so both engine loops scanned the whole promotion
    registry every capture to find nothing, and the serialized bool was never once set.
    ⛔ It is **not** re-homed and the member is **not** kept alive meanwhile: it is TRIGGER-SHAPED (a happening,
    then promote), which is the building-counter-damage case exactly
    ([triggers.md](../specs/triggers.md)) — a verb is not minted speculatively for one mechanic, and the data goes
    out rather than the old shape being preserved. If the mechanic is wanted it is authored fresh on the trigger
    plane (an `onCaptured` happening + the `promote` action), never by restoring a promotion-side "apply me when
    X" flag, which is the condition-as-member shape
    ([conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query)) inverted onto the target.
    ⚠ **The revival risk is the surviving SCHEMA tag**: it reads like an unmigrated field. The curator now DROPs
    it explicitly so the mapping cannot quietly re-emit a key nothing reads.
30. **DIRTY-AND-RECOMPUTE FOR THE CASCADE PACKAGES** — the mark protocol (`markDirty(mask)` → `rebuildMarked` →
    a gather that re-walks the scope's sources), the banked-marks load drain, the derived dirty MASK per event,
    and the planned "flags all turn, ONE batched rebuild at turn end in dependency order" end-state *(dead — the error was assuming yield packages had to be dirtied and recalculated all the
     time, when it is in essence just a compiled sum that is always updated, based on incoming spine events"**)*.
    A package is a MAINTAINED SUM: the fact names the source, the compiled index names that source's deposits,
    and applying them IS the maintenance — so there is nothing to mark, nothing to defer, and nothing to batch
    ([cascade.md](../cascade.md) § THE MAINTAINED SUM).
    ⛔ **THIS IS A SUPERSEDED DESIGN, NOT A ROLLERSKATE — do not read it as one, and do not treat the code
    around it as suspect.** The protocol was among the FIRST things designed for this rework and was implemented
    faithfully; what changed is the premise, *"the moment we landed on eventspine for everything"*. ⚠
    Contrast entry #14: the ensure-ON-READ protocol genuinely was a rollerskate. Two adjacent entries, two
    different populations — the registry holds both, and conflating them sends an agent hunting a culprit that
    does not exist.
    ⚑ **The general form, because it outlives this instance:** a dirty flag is a CLAIM THAT WE DO NOT KNOW WHAT
    CHANGED, so a complete emit surface falsifies it by construction — *"a dirty flag is the fossil of an
    incomplete emit surface"*, the [self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) fossil rule one level up.
    ⚠ It dissolved SILENTLY: a design whose premise goes away keeps returning correct numbers and merely does
    unnecessary work, so there is no symptom to notice — which is exactly why it survived.
    ⚑ **Three independent reasons it died, and the third is the deciding one:** a rebuild's cost scales with
    what a city HAS rather than with what CHANGED (so the walks do not get faster, they cease); a missed mark
    leaves a stale-but-plausible value that reads fine forever, where a missed emit leaves a loud compounding
    one ([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) prefers the failure that announces itself); and the
    mark derivation is a SECOND completeness census that — unlike the emit census — is **not answerable at any
    one site, moves with the authored data, and cannot be made safe by over-inclusion** (it is far easier to ensure every event exists than to ensure every package is correctly dirtied).
    ⛔ **Never re-add a dirty flag, a derived dirty mask, a mark-then-rebuild protocol, or a batched rebuild
    phase to the package plane.** ⚠ The CONDITIONED tail is NOT this: a deposit gated on state or scaled by a
    count is genuinely re-resolved when its DEPENDENCY moves, routed by the condition-atom reverse index — that
    is the one evaluation moment the model keeps, and reading its survival as licence to restore the mask is the
    misreading this entry exists to prevent.
    ⚠ Also NOT this: `CvDerivedCache`'s use for a genuine leaf recompute elsewhere.
31. **THE GOLDEN-AGE FOOD-FOR-GROWTH DISCOUNT** (`GOLDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH`, applied in
    `CvPlayer::getGrowthThreshold` to the completed threshold) *(dead — owner: "if growth reduction for golden
    age has never worked, we won't introduce it now, game has been balanced around not having it")*.
    ⚑ **It never ran, and that is the whole argument.** The legacy engine looked the define up as
    **`GOlDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH`** — a lowercase `l` in the first word — and no such key exists,
    so `getDefineINT` answered 0, `getModifiedIntValue(v, 0)` returned `v`, and the mechanic was inert for the
    entire life of the mod while reading as implemented at the call site. Every balance decision the mod has ever
    made was made against a threshold a golden age does not move.
    ⛔ **So correcting the spelling is a BALANCE CHANGE, not a bug fix** — at the authored `-25` it cut every
    city's food requirement by 20% for the duration of a golden age, and on the standing save 16 of 26 cities
    loaded already at or above their new threshold, having banked that food against the real one. The branch, the
    define and its XML entry are all removed. **Never re-add a golden-age term to `getGrowthThreshold`.**
    ⚑ **The general lesson it is kept for, because it is not about golden ages:** a `getDefineINT` miss is SILENT
    and composes as the identity, so a mistyped define never warns, never crashes, and leaves a plausible number
    at every observation point. ⚠ So when one is found dead the question is never *"fix the spelling"* — it is
    **what has been balanced around its silence**, and the answer is often that the silent version is the real one.
    ⚠ NOT the same thing as the golden age's YIELD effects (the per-plot threshold bonus, the player golden-age
    yield, the golden-age commerce), which are live, authored and STAY
    ([golden-age.md](../reference/golden-age.md)).
32. **THE `onTurnEnd` FREE-PROMOTION HAPPENING** — the authored token that carried every free-promotion entry
    (buildings and traits alike), asserting that promotions are granted to the units present at END OF TURN
    *(dead — owner: it "was a thing because repeatable grants could not do what triggers can")*.
    ⚑ **It never described the engine.** The applier has always been TARGETED PROPAGATION off two crossing
    facts — a unit ENTERING (`SEVT_UNIT_ENTERED_CITY`) and a source going ACTIVE
    (`SEVT_CITY_BUILDING_ACTIVATED`) — which together maintain the relation *(active source × unit present)*.
    The per-turn rescan it replaced measured **42,336 assign calls in ONE turn**, nearly all re-checking
    promotions the units already held: the blanket-recompute shape
    [self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) rejects.
    ⚑ The token survived because the trigger plane could not name the real happening when it was written; once
    it could, the data said one thing while the engine did another. The happening is now
    `onUnitEnteredCity`, spelled once (`TRIGGER_UNIT_ENTERED_CITY`) because three sites string-match it.
    ⛔ **Never re-add an end-turn sweep, or a per-turn rescan, for free promotions.**
    ⚠ **NOT the same thing as the `onTurn` trigger, which is LIVE and needed** — a genuine recurring
    roll (the property-scaled criminal spawn, [json.md §5](../specs/json.md)). Retiring one fossilised token is
    not a trim of the cadence vocabulary, and reading it that way would take a working mechanic with it.
33. **The stored-vs-oracle ENDPOINT TRIPWIRE — all six routes** *(dead as a doorknob, and the hardest one to keep
    dead: **agent after agent refuses to let it go**, this session included)* — two routes per plane (cascade
    packages, enabler operating set, team capabilities), one serving what the events built and one claiming to
    recompute the same values FROM SOURCE, diffed by an external consumer as the missed-emit tripwire.
    **Killed because the oracle side CANNOT WORK the way things are set up: reproducing event-built
    state means replaying the full event chain, and an endpoint cannot build that chain.** So the oracle does
    not answer a second derivation of the same quantity — it answers a number that was never comparable, and
    diffing it produces confident nonsense at scale.
    ⚑ **The TELL, if a future agent runs it anyway before reading this:** the docs promised the oracle would be
    *"SLOW BY DESIGN … orders of magnitude longer than its `stored` twin"*, and a whole-empire fetch returns in
    **half a second** — the same as stored. It is not slow because it is not recomputing. A measured run then
    reported ~1500 divergent city slots with the oracle 17-29x higher, which reads as a catastrophic cascade bug
    and is an artefact of the instrument.
    ⛔ **Why it kept coming back, and the thing to actually fix:** the other tombstones and specs POINTED AT IT as
    the live replacement for what they killed (the `*Legacy` twins, the mark-and-recompute cache), so an agent
    following any of those trails arrives here and finds a working-looking endpoint. A dead idea that other docs
    nominate as the answer is not dead.
    **⚖ WHAT REPLACES IT: READ THE LOGS, CHECK THE DATA AGAINST THE JSON INFOS, AND AGAINST WHAT STATE
    EXPECTS.** THREE legs, and the third is not optional — a deposit is conditioned and scaled, so the authored
    number alone predicts nothing:
    - the **LOGS** say what actually happened — every deposit with its source, channel, scope, unit, driving
      fact and apply COUNT;
    - the **JSON INFO** says what that source is authored to deposit;
    - the **STATE** says how many times, and whether at all — who holds the source, which gates hold, what the
      counts are.
    Correctness is all three agreeing, attributed to a named source with numbers — the
    [no-guessing rule](../../AGENTS.md)'s own prescription, and the only check that does not need an event chain
    rebuilt to be meaningful.
    ⚠ **Two legs is not a check.** Log-vs-JSON alone cannot tell a correct multi-owner apply from an
    over-application, and JSON-vs-state alone never observes what landed.
    ⚑ Worked, same session: a trait's `maintenance` deposit read `-70` against an authored `-10%` — which looks
    like a 7x over-application until the third leg answers it, the spine showing exactly 7 owners holding that
    trait and the log's own apply count saying 7. Fully attributed, without one.
    **⛔ AND THE WORD GOES WITH THE MECHANISM — "ORACLE" IS NOT A TERM WE USE.** The machinery cannot be rebuilt from an endpoint call — that is a call
    that would take more than a minute to complete."* ⚑ That is a SHARPER disqualification than the one above and
    worth holding separately: even granting an endpoint could replay the chain, the replay is minutes of work, so
    it could never be an endpoint's answer. The two reasons compose — it cannot be done, and if it could it would
    not fit.
    ⛔ **This is ["dirty" is not a term we use](../cascade/03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) one plane over, and for
    the identical reason**: a term that survives its mechanism is the evidence-of-the-abandoned-path that teaches
    the next agent to reach for it. "Dirty" was removed WITH the thing it named; so is this.
    ⚠ **It had spread into the load-bearing docs, which is HOW it kept teaching** — the decisions ledger,
    `validation.md` and the scale registry each described the four LIVE `/computed` routes as "oracle endpoints",
    directly contradicting [http-endpoints.md](../specs/http-endpoints.md) and
    [spine.md](../spine.md), which say there is no oracle side at all. Those four are
    **STORED-side DECOMPOSITION CENSUSES** and are named that.
    ⇒ **Keep the word ONLY where it names this dead idea AS dead** (this entry, and the http-endpoints section
    that explains the ban). Anywhere it describes a live surface it is wrong on the facts, not merely stale —
    delete it as you pass one ([docs state current truth only](../../AGENTS.md#docs)).
34. **The PER-CITY TRADED-BONUS MIRROR** (`CityContext::m_traded` — an `id → count` store answering
    `tradedBonusCount`, fed by the has-verdict crossing AND the vicinity supply count) *(dead)* — it replaced the
    RELAY that [enabler.md §8](../specs/enabler.md) RESIDENCY specifies (*"the `CvPlotGroup` is the ONLY
    authoritative list for trade resources, and NOTHING mirrors it"*), on the argument that a relay *"cannot be
    verified, cannot be diffed against an oracle, and cannot fail loud when an emit goes missing"*.
    ⛔ **Every leg of that argument was wrong, and the shape is what made it persuasive.** The ORACLE it wanted to
    be diffed against is itself dead (#33). A relay depends on NO emit, so none can go missing — the fact surface it
    was worried about maintains the plot group's count, which the relay reads. And it consumed TWO of the three
    facts that describe one resource reaching one city, which [spine.md](../spine.md) names as the
    double-count outright: a city SUPPLYING a resource booked its own holding twice.
    ⚑ **Decisively, it answered a DIFFERENT NUMBER than the engine.** `CvCity::getNumBonuses` applies three
    genuinely per-asker adjustments — the bonus's `TechCityTrade` gate, the player's minted-percent suppression and
    the city's corporation add-on — and the store carried none of them, so a resource whose trade tech was
    unresearched read as HELD by every deposit gate asking the context.
    ⚠ **The tell that it had drifted was visible in one screen and read past for months:** the doc comment directly
    above the getter still said *"FORWARDED, never stored"* while the line below it returned the store.
    ⛔ **Never re-add a per-city or per-context copy of the network count.** What the store reached for — a fact-fed,
    inspectable answer — is already there: the plot group's `m_bonusCounts` IS the maintained store, the crossings
    are announced by `changeNumBonuses`, and the enumerating read (`CvCity::collectHeldBonuses`) shares the
    single-bonus read's gates by construction.
35. **The legacy per-invisible-type TABLE PAIR** (`invisibilityIntensity{X}` / `visibilityIntensity{X}` /
    `seeInvisible` / `negates`, 13 per-type tables across 14 invisible types, 477 authorings) *(dead, collapsed
    deliberately — owner: "it's a TB mod after all, it's on drugs")* — killed because only **270 of 355** authoring
    entities ever named more than one type: the 14×13 cross-product served a quarter of its own data. Replaced by
    [vision.md §4](../specs/vision.md)'s `hideAndSeek` block: the method becomes a promotion-grantable
    [skill](../specs/skills.md) (`camouflage`, `disguised`, …), the hider's `concealment` magnitude and the
    seeker's `detection` list (qualified `{unit: HAS_<SKILL>}`) replace the per-type tables, and
    `visibilityIntensityRange` + its substrate variants are gone outright — the contest rides vision's own reach.
    **Don't reinstate a per-invisible-type table or a second range system for detection.**

36. **THE PROPERTY→PROMOTION BAND** (`PropertyPromotions` → `CvPropertyInfo::getPropertyPromotions` →
    `CvGameObjectUnit::eventPropertyChanged`, which placed and removed a promotion as a property value entered
    and left a `{iMinValue, iMaxValue}` band) *(dead)* — the UNIT-side twin of the building band.
    ⚑ **It never had data, and the shape of the absence is what settles it:** `<PropertyPromotions>` appears in
    the SCHEMA and in **no record anywhere in the repo** — not in `Assets/XML`, not in `Assets/Modules`, not in
    `SourceArchive/Assets` — so the curator emits nothing, the vector was never filled, and the override looped
    an empty container on every property change of every unit. The BUILDING half beside it
    (`PropertyBuildings`, 188 authored entries) is genuinely live and simply lives elsewhere: the curator splits
    the bands onto the building's own `requires.operate` clause ([enabler.md §3](../specs/enabler.md)).
    ⛔ The whole hook went with it — base, override and the three `CvProperties` call sites — because the
    override was its only tenant. ⚑ That also retires a documented FOOTGUN: `CvGameObjectUnit` overrode the hook
    WITHOUT chaining to the base, so three separate spine comments had to warn that an emit placed there is
    silently skipped for every unit. With no hook there is nothing to warn about.
    ⛔ **If the mechanic is ever wanted it authors on the TRIGGER plane**, never by restoring a property-side
    member: the happening already exists (`SEVT_CITY_PROPERTY_BAND_ADDED / _REMOVED`) and so does the verb
    (`action.promote`, [json.md §5](../specs/json.md)) — which is what makes this cheaper to author fresh than
    to revive. ⚠ **The revival risk is the surviving SCHEMA tag**, exactly as in #29: it reads like an
    unmigrated field, and it is not one.
37. **NOMADIC START** — a founding restriction behind `#define NOMADIC_START`: a unit could not `MISSION_FOUND`
    until its team held `TECH_SEDENTARY_LIFESTYLE` *(dead, owner: "it does not work, and never has … I have
    serious doubts we can actually make it a compelling game mechanic")*. Two guarded blocks in
    `CvSelectionGroup`, plus the commented-out `#define`; deleted whole.
    ⛔ **So do not "re-enable nomadic start."** There is no working implementation to switch on and the concept
    is not wanted. If a start-condition of this kind is ever built, it is a `GAMEOPTION_*` evaluated live
    ([the whole-entity applicability gate](../specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)), never a compile switch.

    ⚠ **WHY it never worked is NOT established, and the investigation is a trap worth naming.** The obvious
    reading — that the `TECH_SEDENTARY_LIFESTYLE` global-define binding is unbound, so the gate resolved to
    `NO_TECH`, which `CvTeam::isHasTech` answers TRUE for — does not survive its own control: `TECH_TRIBALISM`,
    the other `TechTypes` entry in the same `DO_FOR_EACH_ENUM_GLOBAL_DEFINE` table, is equally absent from the
    XML defines. ⇒ Reasoning about a tech binding from `GlobalDefines.xml` says nothing here; the tech itself is
    real and is the first era-gate tech. The cause is unknown and does not need to be known — the mechanic is
    gone.

38. **THE CIVILIZATION WHITELIST + NPC BUILD-LOCKDOWN** (`EnabledCivilizationTypes` on buildings/units +
    `bStronglyRestricted` on civilizations) *(killed, owner: "creating explicit whitelist for some edgecase npc
    civ like this means it's poorly designed in the first place — whitelists in things like this create poorly
    visible gamedesign")*. Legacy applied the whitelist ONLY inside `isStronglyRestricted() && isNPC()` (a
    strongly-restricted NPC could build only what listed it; inert for every normal civ) — and a rebuild misread
    it as a universal whitelist, statically barring 21 empire-level constructibles for every human player, which
    is exactly the poor visibility the ruling names. Techs (and `requires`) decide what any civ can build; a
    deliberate bar authors as `disables`, the mechanic that *"suits any purpose better than whitelist
    override."* Cut whole: engine members/getters, enabler bars, barbarian-spawn and start-unit picker filters,
    curator emits and the data keys. ⛔ Do not re-introduce a civ whitelist for NPC shaping — the NPC culture
    plane already carries the limitation tools.
39. **`CvCascadingModifierBundle`** *(dead)* — the unified effect-scope cascade bundle/repository (`#421`/`#423`)
    designed as the implementation vehicle for empire/team-scope constructables (team buildings). Killed: it
    never got past a working-tree prototype, and the #428/#430 top-down model needs no new machinery — an
    empire/team-scope constructable authors `enables`/`disables`/`requires`/modifier families like any other
    entity ([enabler.md §2](../specs/enabler.md)). **Don't revive the bundle/repository plumbing** — the
    autobuild-replacement concept (shared hammer pool, empire-scope buildings) is still wanted and now lives in
    enabler.md §2's empire-level-building model.
40. **PER-MECHANIC PARITY COMPARISON** (`DEC-per-mechanic-parity` — verifying a value by diffing it,
    mechanic-by-mechanic, against the engine's own legacy computation) *(dead)* — parity/shadow as an ACTIVE
    validation phase is CLOSED ([parity and shadow are closed](../specs/validation.md)):
    there is no legacy oracle left to diff a mechanic against on the cut surfaces
    ([the red ratchet](../../AGENTS.md#build-and-test)). The completeness bar that survives
    ([the completeness+attribution bar](../specs/validation.md#the-observation-surface)) is verified by the THREE-LEG check — the logs, the JSON info, and
    what state expects, all three agreeing — never by comparing a mechanic's cascade value against its legacy
    counterpart (entry #33 above). **Never re-frame a check as "compare this mechanic against its legacy
    value" or re-invoke a per-mechanic comparison sweep.**
41. **THE GOLD-PAID BUILDING UPGRADE** *(dead — ruled out, never built)* — giving buildings the unit-upgrade
    treatment: a player-chosen, priced "upgrade this Forge to a Foundry for N gold", mirroring
    `CvUnit::upgrade` / `upgradePrice`. Killed outright: *"we won't have a gold-paid version for
    buildings."* A building upgrade is a **consequence of becoming obsolete, applied automatically** — authored
    as `whenObsolete.becomes` ([json.md §4.2](../specs/json/04-availability.md#42-obsoletes--replaces--disables--removal-permanent-source-side)),
    which declares the fate in isolation and never names what obsoleted the building.
    **⛔ THE REASON IS TWOFOLD, AND IT IS RECORDED BECAUSE IT IS WHAT PROTECTS THE RULE:**
    (1) **It is astronomically exploitable.** Buildings sit in a priced LADDER, so a player hoards gold, builds
    the *lowest production-cost* rung in every city, and upgrades the lot in a single turn — converting gold
    straight into top-tier buildings while paying the production of the cheapest one. Units do not break this way
    because a unit upgrade buys ONE unit; a building upgrade buys a whole empire's worth of tiers at once, and
    the many-to-one convergence in the data (83 buildings feed one receiver) makes the arbitrage worse, not
    better. (2) **The UX would be a nightmare to build** — choosing which buildings, in which cities, at which
    prices, is a whole interface for a mechanic nobody asked for.
    ⚑ **The revival risk is the SYMMETRY, which is why this is a tombstone and not a footnote:** units genuinely
    do have a chosen, priced upgrade, so "buildings should too" reads as an obvious consistency fix rather than a
    new mechanic — and the two upgrade paths otherwise share a shape (a succession edge, an availability
    consequence, a transformation verb that is not the creation path,
    [parked/upgrade-chains.md](../plans/parked/upgrade-chains.md)). **Don't add a cost, a price function, a
    prompt or a player action to the building-obsolescence fate.**

42. **DLL-SIDE GRAPHICS ASSET SHARING — "make every plot using the same forest model hold ONE copy"**
    *(dead — investigated with a live-process measurement, impossible from our side of the boundary)* — the
    intent was to stop N plots each carrying their own copy of an asset, so that full-map residency became
    affordable and paging/viewports were unnecessary. **It cannot be done from the DLL, and the reason is
    structural rather than a matter of effort:** plot graphics are requested by COORDINATE — `RebuildPlot(x,y)`,
    `RebuildTileArt(x,y)`, `RebuildRiverPlotTile`, `ForceTreeOffsets` — and **an asset handle never crosses the
    boundary at all.** A census of the entire EXE-side surface (`CvDLLEngineIFaceBase` 71 virtuals,
    `CvDLLEntityIFaceBase` 26) finds **no instancing, sharing, clone or reuse primitive anywhere**; there is no
    call that expresses "reuse the node you already built". Everything between `(x,y)` and the finished scene
    node is EXE-internal, unsymbolized, and unreachable.
    ⇒ **Only two quantities remain controllable: HOW MANY objects have graphics (residency) and WHAT ONE COPY
    COSTS (art payload).** Since per-plot cost tracks mesh size (~70–190 KB/plot measured, against a 68 KB mean
    NIF), decimating the most-frequently-placed feature/improvement meshes is the only lever that attacks the
    copying itself; everything else is residency.
    ⚑ **The revival risk is that the idea is correct and obvious** — sharing IS the right design, a forest tile
    plainly *should* reference one mesh, and the conclusion "so let's make it do that" follows naturally from
    looking at the art defines. The blocker is invisible until someone enumerates the interface headers, which
    is why this is a tombstone. ⛔ **Do not re-derive it; and do not read `CvArtInfo`/`ARTFILEMGR` sharing as
    evidence the engine shares** — the DLL side is already shared (one `CvArtInfo*` per art define, tag strings
    only, §4), which says nothing about what the EXE does downstream.
    ⚖ **The same closed EXE is the root of the 32-bit ceiling, the frozen VC7.1/Python 2.4 toolchain
    ([engine.md](../reference/engine.md)) and this copying** — so the only thing that would genuinely dissolve
    it is not owning that constraint, which is a far larger question than an optimisation.
