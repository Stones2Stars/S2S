# Superseded ideas — the don't-revive registry

> Dead approaches kept so they aren't reinvented ([DEC-keep-unkilled-ideas](decisions.md)). Condensed — one entry
> per dead idea: what it was, why it's dead, what replaced it.

1. **Derived-data repository pattern** *(mostly obsolete)* — a `TLazy` / version / dirty aggregation layer. Killed:
   the cascade + tally subsume it (tally counts UP, modifier magnitudes DOWN). One residual: AI-heuristic caching
   (plot danger, unit-AI counts) is separate + out of scope. **Don't revive the repository as a
   data/derived-aggregation mechanism — that is the cascade's job.**
2. **Cross-entity inversion** *(dead)* — ~37 inversions that physically moved cross-entity modifiers onto the keyed
   target entity. Killed by [DEC-deliveryguy](decisions.md): the deliverer owns the modifier keyed by target, not
   inverted onto it ([modifier](../specs/modifier.md) §4). **Don't reinstate inversion**,
   even for Terrain/Improvement/Bonus targets.
3. **`loadPrune`** *(dead)* — a curator-era INVENTION: the legacy
   `OnGameOptions`/`NotOnGameOptions`/`PrereqGameOption` validity tags re-encoded as a bespoke "prune at load"
   section, named BACKWARDS (`onGameOptions` meant *keep only when on*) and spec'd wrong, while the spec already
   had the answer (`GAMEOPTION_X` as an ordinary condition). Killed whole: the payload
   authors as the **entity-level `enabled`/`disabled` gate** ([DEC-entity-gate](decisions.md), json.md §2); the
   complex-trait entries dropped outright (they restated the simple/complex FOLDER split, which is the selection
   mechanism). **Don't revive a bespoke game-option section.**
4. **The offline DRY CALCULATOR — all four attempts** *(dead as an approach)* — an out-of-process reimplementation
   of the cascade/enabler calculation, used to judge the engine. Four were built: (1) full legacy-calc-pipeline
   offline emulation, (2) the Python per-scope/combine calculators, (3) the first-version .NET validator, and
   (4) **StoneBase**, whose original purpose was exactly this. All are retired — it proved easier to dump
   individual calcs from the game itself, and a dry calculator that judges the spec while drifting from it
   corrupts the loop it is meant to close. Verification is LIVE — done-is-observable endpoint polls
   ([DEC-done-is-observable](decisions.md#dec-done-is-observable)) and turn time
   ([DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)), and for the cascade the stored-vs-oracle endpoint
   pair diffed OUTSIDE the DLL ([state-repositories.md](state-repositories.md)); the zero-ride-in principle still
   holds ([DEC-calc-zero-ride-in](decisions.md#dec-calc-zero-ride-in)). **Never build a fifth dry calculator** —
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
   [DEC-conditions-are-predicates](decisions.md) (the golden-age yield-effect member-mirror is the one PERMANENT exception).
   `perMilitaryUnit` specifically authors as the `cities.{unit: IS_MILITARY}` entry (json.md §3.7).
8. **The "deliberately more permissive" vicinity model** *(dead)* — vicinity with no ownership filter. Killed:
   vicinity mirrors the engine's ownership tiers (owned ⊂ owned+neutral ⊂ crossBorder; json.md §3.4, enabler.md §3).
9. **The tally as a store** *(dead)* — a tally-owned accumulator / load-time event-replay rebuild duplicating the
   object-owned counts. Killed: the tally is a read-only accessor + roll-up over the objects' own counts
   ([tally.md](../specs/tally.md), [DEC-tally-serializes-nothing](decisions.md)). **Never re-add a tally-side store,
   seed, or shadow.**
10. **`grants.specialists` for an ALIVE-WITH-SOURCE specialist** *(dead)* — the ordinary building/civic free
    specialist authored as a grant. Killed: those are the `freeSpecialists` MODIFIER family, which dies with its
    source ([modifier.md §6](../specs/modifier.md)).
    ⚠ **The key itself is NOT dead — the LIFETIME is what was killed.** The spec reserved a carve-out for
    anything that "genuinely GRANTS permanent free specialists, surviving the destruction of its source", and
    that case exists: the trait's ERA-ADVANCE specialist is a persisted PULSE landing in the city's
    UNATTRIBUTED typed-free ledger, so it outlives the trait. It authors on the TRIGGER plane
    (`onEraChanged` → `action.grant.specialists`), never as an entity-level grant
    ([json.md §5](../specs/json.md)). ⛔ Do not read this entry as banning that shape, and do not "restore"
    the ban over it — the discriminator is whether removing the source removes the specialist.
11. **Graded parity tolerance (the six-rung "care scale")** *(dead)* — grading a divergence's acceptability. Killed
    by [DEC-parity](decisions.md): exact match, no tolerance band, no agent grading; a divergence is a
    data-collection gap to close.
12. **The `/shadow/*` endpoint surface** *(dead)* — in-DLL cascade-vs-legacy sweep endpoints. Killed: the two
    verification legs never mix surfaces ([validation.md](../specs/validation.md)); the shadow rode the gated
    logging, and the shadow phase itself has since ended.
13. **The load reseed as a fabricated full-state replay (`spineEmitGameState`)** *(dead)* — a separate pass, run
    after deserialization, that walked already-populated game objects and emitted a synthetic DOMAIN event for every
    present fact ("for each building the city has, emit built"). Killed: it FABRICATES events from populated state
    rather than the events coming from the genuine save read — a pseudo-emit that feeds the cascade reconstructed
    values and invites the next agent to reconstruct more state the same way. The reseed must be **event-sourced from
    inside the read** ([event-spine.md](../specs/event-spine.md) load-RESEED, [DEC-spine-reseed](decisions.md#dec-spine-reseed)):
    reading a fact off the stream is what fires its event. **Never re-add a post-deserialization state-walking emit
    pass.**
14. **The bespoke per-scope modifier SUBSTRATE** (`CascadeAccumulator` + `CascadeCityPackages` /
    `CascadePlayerScope` / `CascadeAreaPackages` / `CascadeTeamCaps` / `CascadeUnitPackages`, the `CPK_*`/`PSC_*`
    box slices, `CascadeRateSlots` + epochs, `playerSliceRebuild`) *(dead)* — five hand-shaped structs with
    hand-named per-channel scalar members, each carrying its own bespoke invalidation path, reached through a
    read-side `ensure()` protocol. Killed by [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape): every
    derived cache is the SAME object type on every owner (one channel-indexed `CvDerivedCacheSet<TOwner>`, one mark
    derivation), so a hand-named scalar field is a DEFECT and a new scope/channel is DATA rather than a new struct.
    The whole tree is archived (`SourceArchive/Cascade/`). **Never re-add a per-scope package struct, an
    `ensure`-on-read protocol, or a `*Rebuild` blanket** — the replacement is the uniform channel-indexed cache on
    each scope owner ([state-repositories.md](state-repositories.md)).
15. **Re-bodying the legacy getters to read the cascade (the "computed-getter flip")** *(dead)* — keeping each
    legacy getter's signature and swapping its body to a cascade read, so no call site changed. Killed by
    [DEC-new-getter-surface](decisions.md#dec-new-getter-surface): a legacy getter's contract encodes legacy
    scale/granularity/combine, so pointing the cascade at it makes the CASCADE bend to the legacy shape — the
    mechanism that produces the half-migrated state. **A change that leaves every consumer untouched is the tell,
    not the win.** The replacement is a NEW uniform parameterized getter set over the channel index, with the old
    surface disconnected.
16. **One shared spine consumer routing BOTH machines** (`CvCacheInvalidationConsumer` — enabler deltas and
    modifier marks in one `onEvent`) *(dead)* — it welded the two systems the docs work hardest to keep apart, and
    forced one load-suppression policy onto two that genuinely differ (the enabler is load-ACTIVE, the modifier
    build is not). Killed by [DEC-enabler-not-cascade](decisions.md#dec-enabler-not-cascade): **one consumer per
    system**. `enablerRegisterConsumer` is the enabler's own; the modifier gets its own when it is rebuilt.
17. **The `*Legacy` / `*Recomputed` / `*Leg` ORACLE-TWIN surface** *(dead — and one of the reasons the hard rebuild
    was forced, owner)* — per-channel comparison getters + `/computed` twin fields, kept so a cascade value could be
    diffed against a "legacy" one. It rotted twice over: agents **cheated the comparison by sneaking legacy-computed
    data into the cascade calc** so it could not fail (the abuse [DEC-calc-zero-ride-in](decisions.md#dec-calc-zero-ride-in)
    now bans outright), and once the legacy accumulators were deleted both sides read the same derivation, so the
    check could never turn red at all. The rebuild removed the whole surface — **zero `*Legacy`/`*Recomputed`
    symbols remain in `Sources/`** — so this is solved STRUCTURALLY, not by a standing rule; the ledger entry that
    policed it (`DEC-oracle-tautology`) is retired with it. **Never re-add a comparison getter or a `/computed` twin
    field.** ⚠ It does NOT follow that comparison is banned: a check whose two sides are genuinely different
    derivations — **event-built state vs a fresh recompute-from-source**, served on two endpoints and diffed
    OUTSIDE the DLL — is the missed-emit tripwire and is the sanctioned shape
    ([state-repositories.md](state-repositories.md)). What is dead is the same-derivation twin, not verification.
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
    self-heal ([DEC-no-self-heal](decisions.md#dec-no-self-heal)). Replaced by the endpoint oracle: two routes per
    plane, recompute into a caller-owned buffer, diff OUTSIDE the DLL
    ([state-repositories.md](state-repositories.md)). **A divergence has NO in-DLL representation — never re-add a
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
    ([DEC-done-is-observable](decisions.md#dec-done-is-observable)) and turn time
    ([DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)), and no successor metric replaces the gate.
21. **The BLANKET MODIFIER RECALCULATION** (a whole-world wipe-and-reapply pass: zero every accumulated total on
    game/team/player/city/area/plot, re-run every tech, civic, trait, building, religion, corporation and event,
    fronted by a "should the modifiers be recalculated?" popup on an asset-checksum mismatch, plus a hotkey and a
    net message to carry it) *(dead)* — the archetypal self-heal
    ([DEC-no-self-heal](decisions.md#dec-no-self-heal)). It existed to purge derived data that had drifted **in a
    save**, which no longer happens: no cache is serialized, so LOAD rebuilds everything from source and there is
    nothing to purge. Worse than a generic blanket, it fired precisely on the saves most likely to have drifted,
    silently papering over the missed invalidations the event spine is built to EXPOSE. The asset checksum gates
    nothing — not OOS, not loading ([engine.md](../reference/engine.md)) — so a mismatch has no action to take.
    **Never re-add a recalculate-everything entry point, a wipe-the-totals helper, an "are you sure you want to
    recalculate" prompt, or an in-recalc suppression flag that makes ordinary mutators skip their work.**
22. **MIRROR-THEN-REDESIGN** — *"the migration reproduces the engine's existing behaviour exactly; behavioural
    redesign is deferred to post-migration"* *(dead — retired as `DEC-mirror-then-redesign`)*. It was **dead by its
    own construction (owner)**: it presupposed (a) a legacy implementation worth faithfully mirroring and (b) a LATER
    phase in which redesign unlocks. Neither exists — the legacy surface is being **NUKED, not mirrored** (the ~622
    channel-shaped getters are a DELETION list, [DEC-new-getter-surface](decisions.md#dec-new-getter-surface)),
    parity and shadow are closed, and there is no post-migration phase to hand work to
    ([DEC-no-deferred](decisions.md#dec-no-deferred)). **The SPEC leads, now:** where code and spec disagree the
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
    mint kinds for the old shape ([DEC-proper-once](decisions.md#dec-proper-once)).
    ⚠ NOT the same thing as the ordinary `bombard` FAMILY (`bombard.unit.rate` / `airBombRate`), which is live,
    authored and STAYS.
    ⚖ **THE RULE THAT DECIDES THE BOUNDARY (owner): *"if it uses the ranged attack, and is not an airplane, it
    goes — vanilla airplanes have ranged attack."*** That is the whole test, and it is what makes the split
    re-derivable instead of memorized: **AIRPLANE ranged attack is vanilla and STAYS** (fighter engage — a
    first-class `MISSION_FENGAGE` with its own interface mode and pedia concept; and ACTIVE DEFENSE, which runs
    on `airStrikeTarget`/`airCombatDamage`/`MISSION_AIRSTRIKE`). **Non-airplane ranged attack GOES.**
    ⚖ **KEPT vs DROPPED — the cut is by MECHANIC, never by name (owner).** The DEFENCE-GRINDING bombard stays
    exactly as it is: a unit adjacent to a city wearing its defences down (`MISSION_BOMBARD` → `bombardRate` →
    `getDefenseDamage`), and **NAVAL units keep the bombards they have** (owner). ⛔ **Three naming traps sit on
    this boundary, and each has already misled a sweep:** `AI_bombardCity` (defence grinder, STAYS) is ONE LETTER
    from `AI_RbombardCity` (ranged, gone) and the naval path called BOTH in sequence; **`INTERFACEMODE_BOMBARD`
    was the RANGED targeting mode despite its name**, while `INTERFACEMODE_AIRBOMB` is the vanilla one that
    stays; and the `dcm` prefix marks mod PROVENANCE, not membership — `dcmFighterEngage` is a live vanilla
    mechanic wearing it. Decide every one of these by what the code DOES, never by what it is called.
    ⚑ Opportunity fire went with ranged bombard because it *gated on the same `getDCMBombRange()` stat*, and its
    own author's comment records why it deserved to: *"absolutely zero resistability to this damage and no
    potential for failure to strike, making it far more powerful than any player determined action."*
    ⚖ **WHAT THE REDESIGN OWES (owner):** *"we basically want vanilla civ bombard back"* as the baseline, and
    **ranged attack has to DO SOMETHING to be worthwhile** — the retired failure is not that ranged existed, it
    is that it dealt ~nothing while still satisfying the turn, so a redesign that reintroduces a near-zero-damage
    ranged action has reproduced the bug. ⚑ Naval shore bombardment is a DELIBERATE divergence from vanilla,
    which did not allow it: *"we want them to, otherwise they are pretty damn worthless."*
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
    ⛔ **THE SM MULTIPLIER WAS NOT THE FAULT, and blaming it is the wrong lesson to take (owner).** Size Matters
    FUSES 3 equal units into 1 bigger one, so a bigger unit costing more upkeep *"makes sense"*. The arithmetic
    agrees: the multiplier is ×1.5 per rank while a rank represents 3 fused units, i.e. a fused unit paid 1.5×
    the upkeep of one unit while BEING three — a discount against fielding them separately, not a punishment.
    ⚑ **The failure was COMPOSITION:** *"the problem came from when you added the per unit scaling in the mix as
    well, then it got real out of hand"*. A defensible per-size cost and an unbounded per-unit percentage
    multiplied each other, and the product is what made armies unaffordable.
    ⚖ **FLAT is an INTERIM, not the destination (owner): *"we want to have unit maintenance make more sense in
    the future, so we leave it like this for now"*.** Unit maintenance is owed a coherent redesign; this removal
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
    ([DEC-unit-modifiers-on-top](decisions.md#dec-unit-modifiers-on-top): a modifier that TRAVELS is folded live
    on top). That is real per-read work on the combat path for one authored entity.
    ⚖ **It is also on the way out wholesale (owner): land units carrying other land units "and all those
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
    ([DEC-conditions-are-predicates](decisions.md#dec-conditions-are-predicates)) inverted onto the target.
    ⚠ **The revival risk is the surviving SCHEMA tag**: it reads like an unmigrated field. The curator now DROPs
    it explicitly so the mapping cannot quietly re-emit a key nothing reads.
30. **DIRTY-AND-RECOMPUTE FOR THE CASCADE PACKAGES** — the mark protocol (`markDirty(mask)` → `rebuildMarked` →
    a gather that re-walks the scope's sources), the banked-marks load drain, the derived dirty MASK per event,
    and the planned "flags all turn, ONE batched rebuild at turn end in dependency order" end-state *(dead —
    owner: **"what I got wrong is that I thought the yield packages had to be dirtied and recalculated all the
    time, when it is in essence just a compiled sum that is always updated, based on incoming spine events"**)*.
    A package is a MAINTAINED SUM: the fact names the source, the compiled index names that source's deposits,
    and applying them IS the maintenance — so there is nothing to mark, nothing to defer, and nothing to batch
    ([state-repositories.md](state-repositories.md) § THE MAINTAINED SUM).
    ⛔ **THIS IS A SUPERSEDED DESIGN, NOT A ROLLERSKATE — do not read it as one, and do not treat the code
    around it as suspect.** The protocol was among the FIRST things designed for this rework and was implemented
    faithfully; what changed is the premise, *"the moment we landed on eventspine for everything"* (owner). ⚠
    Contrast entry #14: the ensure-ON-READ protocol genuinely was a rollerskate. Two adjacent entries, two
    different populations — the registry holds both, and conflating them sends an agent hunting a culprit that
    does not exist.
    ⚑ **The general form, because it outlives this instance:** a dirty flag is a CLAIM THAT WE DO NOT KNOW WHAT
    CHANGED, so a complete emit surface falsifies it by construction — *"a dirty flag is the fossil of an
    incomplete emit surface"*, the [DEC-no-self-heal](decisions.md#dec-no-self-heal) fossil rule one level up.
    ⚠ It dissolved SILENTLY: a design whose premise goes away keeps returning correct numbers and merely does
    unnecessary work, so there is no symptom to notice — which is exactly why it survived.
    ⚑ **Three independent reasons it died, and the third is the deciding one:** a rebuild's cost scales with
    what a city HAS rather than with what CHANGED (so the walks do not get faster, they cease); a missed mark
    leaves a stale-but-plausible value that reads fine forever, where a missed emit leaves a loud compounding
    one ([DEC-no-self-heal](decisions.md#dec-no-self-heal) prefers the failure that announces itself); and the
    mark derivation is a SECOND completeness census that — unlike the emit census — is **not answerable at any
    one site, moves with the authored data, and cannot be made safe by over-inclusion** (*"it is far easier to
    ensure we have all the events than to ensure that we have all packages correctly dirtied"*).
    ⛔ **Never re-add a dirty flag, a derived dirty mask, a mark-then-rebuild protocol, or a batched rebuild
    phase to the package plane.** ⚠ The CONDITIONED tail is NOT this: a deposit gated on state or scaled by a
    count is genuinely re-resolved when its DEPENDENCY moves, routed by the condition-atom reverse index — that
    is the one evaluation moment the model keeps, and reading its survival as licence to restore the mask is the
    misreading this entry exists to prevent.
    ⚠ Also NOT this: `CvDerivedCache`'s use for a genuine leaf recompute elsewhere, and the ENDPOINT ORACLE's
    full recompute-from-source, which is deliberately independent and stays.
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
