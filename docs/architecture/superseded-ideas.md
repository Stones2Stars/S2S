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
10. **`grants.specialists`** *(dead)* — free specialists as a grant. Killed: free specialists are the
    `freeSpecialists` MODIFIER family, alive-with-source ([modifier.md §6](../specs/modifier.md)); zero authorings
    ever existed.
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
