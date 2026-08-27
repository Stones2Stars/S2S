# The read — the per-GROUP valuation, and the cascade provides while the game object sums

> Part of the **[cascade](../cascade.md)** spec.

> This is where the misunderstanding that has cost repeated rebuilds lives: agents treat the cascade as the
> thing that COMPUTES a yield and leave the game objects as passengers. It is the opposite.

An info's ACTUAL contextual output is read **one endpoint per GROUP of channels**, never per single channel:
`expectedFlatYields` / `expectedYieldModifiers` / `expectedPlotYields` / `expectedFlatCommerce` / `expectedWellbeing`.
Each takes the three live contexts and fills that group's ×100 array — **you pass the contexts in, you get the group's
expected values out**: `(CityContext, EmpireContext, CvPlotGroup)` → the group's values.

- **CityContext** — vicinity + local state AND the river/water/… plot-attr COUNTS (`plotAttrs`). A building reads the
  CITY context for "how many river tiles", **never a PlotContext directly** — the plot-count sums live in the
  city context. It also answers the city's **traded** bonuses (through the city's own plot-group-backed reads).
- **EmpireContext** — the empire-scope state (civics/traits/policies/state religion).
- **CvPlotGroup** — the trade-network object; the reserved explicit **traded**-bonus source (`connection:"trade"` vs
  `"vicinity"`, [json.md §3.4](../specs/json.md)). Traded state is **NEVER mirrored into `CityContext`**. The
  valuation seam fills it into the eval ctx (`CvCascadeEvalCtx::plotGroup`): a `connection:"trade"` atom reads the
  city's own plot-group-backed RELAY when a city is bound (`CityContext::tradedBonusCount` forwards to
  `CvCity::getNumBonuses` — the tech-gate/minted/corp layer over the group's count), and the passed group directly
  for the city-less what-if.

Each endpoint returns the UNCONDITIONED ×100 base PLUS every conditioned `m_cond` deposit whose condition holds — summed
via the **one** evaluator (`MMKernel::applies`) over a `CvCascadeEvalCtx` the contexts fill (`CityContext::fillEvalCtx`
= city/plot, `EmpireContext::fillEvalCtx` = player/team) — so the contexts ARE the eval state, not a raw-pointer ctx
built beside them. `expectedPlotYields` scales each plots-target deposit by `cityContext.plotAttrs.count(predicate)`.

> **Everything an info holds is ×100** ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) — readJson converts
> human→×100 once at load; the info never de-scales; a reader `÷100`s at the point of use. So these endpoints add
> `value100` directly, and the materialized base members are `value100`.

> **Naming — no abbreviated parameters.** Parameters are spelled in full (`cityContext`, `empireContext`,
> `plotGroup`), never `cx`/`pg`: short names are only defensible inside a tightly-scoped lambda, which C++03 lacks.
> Index parameters likewise name the enum they key (`YieldTypes eYield`, `DefenseKind eKind`), reusing the
> existing engine + family enums — a new family mints one typed enum, and the group's entries + its `expected*`
> array both key off it ([patterns.md § THE GETTER SETUP](../architecture/patterns.md)).

### ⚖ THE READ PATH — THE CASCADE PROVIDES, THE GAME OBJECT SUMS (owner, LOCKED)

- **The cascade is the PACKAGE STORE, nothing more.** Per `(scope × channel × combine-position)` it holds one
  standing sum — how a yield is influenced, and by how much, from every source. It answers *what influences
  this*, and it **never computes a final number**.
- **The GAME OBJECT sums.** The consuming object fetches the packages it sits under and applies its channel's
  combine formula. That arithmetic is the object's, not the store's.
- **ONE reporting surface, read identically by both consumers** — the game object summing, and the endpoints
  decomposing — so the number a city computes and the breakdown an endpoint renders are the SAME bytes. Two
  surfaces would be two derivations, and they would drift.

  Plot and the upper scopes are therefore mirror images (yield-only vs percent-only), and **CITY is the single
  scope carrying both**. That is why "whether a scope's packages are empty is irrelevant" is not hand-waving: the
  shape is uniform, and the origin rule says which half any given scope ever fills.
  ⚖ **The rule governs the YIELD/RATE plane; for every other family the sides are the DATA's and the minted
  channel sets enforce them** (wellbeing authors empire flats; health/defense/property author plot percents)
  — §1. ⛔ Consequence for any read-side roll-up: **the channel set is the
  gate, never a hand-written per-scope filter.**

  **⛔ THE CONSOLIDATION REQUIREMENT: every modifier/yield cache is ONE shape** — one flats dictionary
  per YIELD ORIGIN the scope carries plus one percents dictionary, each an int keyed by channel. Every scope but
  CITY carries a single flats dictionary, because only the city has more than one yield origin (below). The
  requirement is SAMENESS OF SHAPE, never a count of dictionaries: what is banned is a bespoke struct or a
  hand-named field, not a second dictionary of the same uniform type distinguished by its origin. The drift it replaces is the ~33
  hand-named scalar fields (`scGpBaseBld`, `scDefense`, `scDefBombard`, `scMaintModCity`, `scTradeCity`,
  `brCityMilitary`, …): a hand-named field cannot be addressed uniformly, so it forces a bespoke invalidation
  path per field, which is how that many accumulated. A new scope or channel must be DATA, not a new struct.

  **⛔ KEYS ONLY WHERE THEY ARE NEEDED — the storage is NOT a global dense index.** The channel set is
  DATA-DEFINED (`PROPERTY_*` is one channel per property info) and no object uses more than a fraction of it, so
  a dense array over every channel on every object is mostly zeros — on 9,600 plots that is ~7 MB of nothing.
  Each scope carries ONLY the channels authored AT that scope, both the channel ids and the per-scope sets
  derived from the data at load (the `ClassificationRegistry` minting precedent), never hand-listed. The
  layout is OPEN-ENDED: slot indices are append-only ints with no fixed bit budget, so the per-scope counts
  grow with the authored data — read them off the load's `[MODIFIER]` channel-census line
  (`Cascade.log`, one line per scope: authored / slots / receivers), never from a remembered figure.

  **⛔ A SCOPE MUST BE UNAMBIGUOUSLY OWNABLE — WHICH IS WHY A LANDMASS IS NOT ONE.** This is the test a
  candidate scope has to pass, and it explains the whole spine at once:
  - **WORLD passes by being UNIVERSAL** — *"game scope works, because it affects everyone, always"*, so the
    question of who owns the value never arises.
  - **team / empire / city / plot pass by being OWNED BY EXACTLY ONE PLAYER** up the chain, which is what lets a
    deposit roll DOWN and a target read one combined total.
  - **A LANDMASS passes NEITHER.** *"It knows no borders"* — one landmass spans several empires at once, so an
    effect on it *"affects individual players"* and is inherently a per-(landmass × player) **CROSS-PRODUCT**
    rather than a scope. Modelling it as one forces a bespoke slot into the MIDDLE of the containment spine, and
    that bespoke slot is the TELL, not the solution.

  So **there is no area scope**: `"area"` is not a scope token, no object carries an area package, and the
  containment spine is `world › team › empire › city › plot` ([json.md §3.2](../specs/json.md)). The legacy
  `iArea*` authorings were modders reading "area" as "player" — they author at **EMPIRE** — and the ONE genuine
  area concept is a PHYSICAL CONTIGUITY constraint (you cannot run power lines across an ocean), which is the
  engine-side clean-power counter and never a cascade channel.
  ⚑ **The area ID SURVIVES as a plain FACT**, and that is the whole of what an area is to the cascade: a bare id
  plus its tile count, forwarded by `CityContext` for the `AREA_SIZE` token and the coastal water-body read
  (§ THE CONTEXTS, above). ⛔ The city carries that ID, never a `CvArea*` or a per-read `area()` chase — a
  per-read `area()->getNumTiles()` dereferences a whole object to answer a counter an int already holds.
  **⚑ Areas are VIRTUALLY NEVER recalculated** — `CvMap::recalculateAreas` exists for the extreme case
  of terrain levelled to sea level (the WMD mechanic), plus map generation; a landmass does not otherwise split
  or merge in play. Treat a rebuild as RARE-but-real: it does `m_areas.removeAll()` and reassigns every id.
  **So the rebuild announces itself as a DOMAIN fact: emit "areas recalculated" and force the recheck** —
  every holder of an area id re-reads, rather than each cache inventing its own staleness test. Being rare, the
  blanket costs nothing; and it is not the banned self-heal: a wholesale identity reassignment is not
  addressable per-source, so no finer route exists to derive ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)
  bans papering over a
  MISSED invalidation, not announcing a genuine wholesale one).

  **⛔ TWO SCOPES ARE DELIBERATELY NOT PACKAGES:**
  - **WORLD is CONFIG** — cost multipliers and the like, carried by eras / gamespeeds / handicaps. It changes
    essentially never and is read from its sources, not cached behind a staleness protocol. A project granting
    something to every player is NOT world-scope state: it authors the plural TARGET `world.empires`
    ([json.md §3.3](../specs/json.md)) and lands in each PLAYER's package. The handful of `health.world` /
    `happiness.world` / `tradeRoutes.world` project authorings are mis-scoped data, a curator fix
    ([recurate on every decision](../../AGENTS.md#git--delivery)).
  - **UNIT is RESOLVED VALUES, not a package** — "when the number is put on the unit, no more percentages or
    whatever is involved, the data just IS". The exact set of numbers a unit carries is known, so they are summed
    and stored individually, and they move on a different trigger from everything else: ONLY when a promotion or
    combat class changes — plus ONE seeding gather at BIRTH (`SEVT_UNIT_CREATED` at play; the END of the unit's
    own `read()` at load, the one point its full held set has streamed in — the consumer's mark cannot serve a
    loaded unit, since its getUnit lookup runs while the player's unit list is still mid-stream), because the
    non-delta slots (vision above all) carry the unit's own BASE: a unit holding no promotion and no extra
    combat class would otherwise never gather and read 0. It is the most static plane in the engine.
    ⛔ **THE SUM WALKS WHAT THE UNIT HOLDS, NEVER THE REGISTRY.** The contributors are the unit's own type plus
    its held promotions and held combat classes, enumerated from the containers the unit already keys them in —
    not discovered by sweeping every promotion and every class asking "do I have this?". That sweep costs the
    DATABASE per gather to rediscover a handful, which is the O(registry) shape the event-built state exists to
    delete (§ THE CONTEXTS, above: a read that walks per call is the efficiency defect to reject in review)
    and the own-data inversion [reverse lookups are populated once, at load](01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1) bans one plane over.
    The unit's storage is therefore NOT a
    bespoke struct awaiting consolidation — it is correctly its own shape, and the 12 unit-only families
    (`strength`, `movement`, `withdrawal`, `firstStrike`, `capture`, `collateral`, `heal`, `bombard`, `air`,
    `cargo`, `range`, `pillage`, …) never enter a scope's channel set.
    ⚖ **STRENGTH'S BASE IS PER-UNIT STATE AND IS DELIBERATELY SERIALIZED.** Every other resolved
    slot takes the unit's own TYPE from the gather, because it is a pure function of that type. Base strength is
    not: **WorldBuilder edits an individual unit's strength**, and the WBS scenario format persists the result
    (`CombatStr=`, written only when it differs from the type). *"You want people to be able to do things in
    WorldBuilder."* So the base lives on `CvUnit` as the serialized `m_iBaseCombat`, the resolved plane carries
    the promotion / unit-combat **DELTA ONLY**, and the consumer adds the two. ⛔ This is the ONE carve-out in an
    otherwise uniform gather, and it is load-bearing: letting the type contribute to the strength slot as well
    silently DOUBLE-COUNTS every unit's authored base. ⚠ It is therefore NOT a
    [derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-) violation — the value is genuine
    per-unit state that no amount of re-derivation can reconstruct, which is exactly why it is stored.
    ⛔ **AND A SECOND ONE IS NOT ADMITTED: AN INVISIBLE ADDITION TO A BASE STAT IS BAD DESIGN WHICHEVER WAY
    YOU PUT IT.** A per-unit stat change is expressed as a CARRIER — a promotion or a status — *"so you
    actually see what is going on with the unit and why"*, which is also why the gather walks the carriers it
    does: each is visible on the unit. ⇒ A mechanic that would force a second carve-out is the MECHANIC that
    goes — an event handing one unit a stat outright is source-less one-shot state, and *"I would be inclined
    to nuke such an event."* ⚠ It has no claim anyway: its only delivery is a promotion
    ([state.md](../specs/state.md)), which is already what serializes.
    ⚖ **A NEW SPECIAL CASE SHIPS WITH THE MEANS TO SHOW IT, OR IT IS NOT ADDED: *"if we want to
    support special casing, we also need to support the ability to show it — so if we want to add that in the
    WorldBuilder, then we need to create tooling for it."***
    ⛔ **STRENGTH ITSELF STAYS FOR NOW AND IS NOT TO BE TOUCHED — it works**: illegible in exactly the
    way this dislikes, and knowingly kept, so an agent "fixing" it is undoing a decision rather than closing a
    gap. ⚖ It is NOT permanent — **when a real pass at WorldBuilder special-case additions is taken, strength
    folds into it**: sequencing with a named destination, so
    ["deferred" is banned](../../AGENTS.md#design) does not reach it. ⛔ Do not start that fold early or take
    it opportunistically.
    ⚖ **A SECTION FOLDS BESIDE THE SLOT TABLE, ON THE SAME MARK — it does not become a slot, and it does not
    become a hand-named pair either.** The slot table addresses modifier-FAMILY entries by
    `(family, kind, scope, unit)`; a `hideAndSeek`-shaped SECTION ([json.md §9](../specs/json.md)) has no such
    address, so it cannot ride the table — and a scalar pair beside it would be the shape
    [every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta) calls a defect. It gets its own cached block
    on the SAME two facts, so ONE route maintains one unit's whole resolved state.
    ⚑ **What earns a section that block is the CARRIER SET, and it is the test to apply to the next one:** it
    folds over exactly the unit's own info ∪ held promotions ∪ held unit-combat classes — the same three the slot
    table folds over — so the two facts that move the table are precisely the two that move it, with no third
    trigger to find. ⛔ A per-read fold over those carriers is the O(registry) walk this plane exists to delete,
    and converting it to walk the HELD containers instead is NOT the fix — that is the same walk with a better
    receiver (§ THE CONTEXTS, above). The read becomes a bare fetch or nothing has been done.
  ⚠ Hand-maintained duplicates DRIFT — that is not theoretical: the maintenance decomposition and its cached fill
  duplicated five terms, and the L8 home/otherArea overlay landed in one and not the other, so `/computed`
  under-reported by 39 against the served value until the duplicate was replaced by a delegation.
  Full rebuild of everything = LOAD ONLY.
  **⛔ THE FIX IS NEVER "ADD ANOTHER STRUCT" — that is the failure mode this ruling exists to close.** The previous
  substrate grew ONE BESPOKE STRUCT PER SCOPE, each with hand-named per-channel members instead of channel-indexed
  Σflat/Σpercent; it is archived and must not be reconstructed ([superseded-ideas](../architecture/superseded-ideas.md) #14).
  **A missing scope is a SYMPTOM of that, not the disease:** with one uniform package, giving a scope its packages
  is a single member; with bespoke structs every scope is its own project — which is exactly why a small scope
  (team, at three channels) never got one, and why its sums leaked into whichever neighbour already had
  a struct. So the package TYPE is unified FIRST (one owner-templated, channel-indexed package on
  `CvDerivedCacheSet<TOwner>`), after which every scope falls out of the same member. Adding a further per-scope
  struct deepens the divergence this closes.

### ⚖ THE CAPSTONE RULE: the cascade is built and kept current ENTIRELY from events — no blanket rebuild, ever, and no per-slot rebuild either

On LOAD the cascade is stood up by the **event reseed** — the save read fires
the DOMAIN events for every fact as it deserializes and each fact applies its source's deposits
([spine.md](../spine.md) / [the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)); the old
recompute-on-load / warm-up recalc (`playerSliceRebuild` + `worldRebuild`) was a stabilize-the-drift STOPGAP
and is REMOVED. Post-load, a fact reaches exactly the slots its deposits feed and **nothing else runs at all** —
no full per-player rebuild on `doTurn`, no mark-all, no per-slice blanket, no turn-roll self-heal
([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). A missed emit surfaces as a live divergence, never a
silently self-healed cost — which is precisely why the event spine must be COMPLETE (every mutation emits) and
is built proper and FIRST.
⚑ **Under the maintained sum that sentence hardens from a design preference into a PRECONDITION:** an
unsaturated spine does not merely leave a value stale, it leaves the sum wrong with nothing that could ever
correct it ([the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)).
Reads are BARE NUMBER FETCHES during the turn (an ensure-per-read protocol on AI-hot paths measurably ground
unit automation). ⚑ *"It's the percentage recalcs that hurt"* is answered at the root rather than mitigated:
the compiled deposit carries its channel AND its unit, so a flat fact touches a flat slot and no percent stack
is ever walked — there is no mask to split, because there is no recalc to narrow.

