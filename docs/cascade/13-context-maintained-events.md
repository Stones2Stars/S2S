# Maintained EVENT-DRIVEN — never a per-turn recompute

> Part of the **[cascade](../cascade.md)** spec.

> **⛔ WE DO NOT DIRTY CONTEXTS — THAT IS THE BOTTOM LINE.** A context store carries **no staleness
> mechanism of any kind**: no flag, no stamp, no epoch, no rebuild entry point, and no `refresh*`. **The FACT
> SETS the bit it names and MOVES the count it names**, and that is the ENTIRE maintenance path
> ([a context is never marked or refreshed](#maintained-event-driven--never-a-per-turn-recompute),
> ["dirty" is not a term we use](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up)) — the same rule § A
> STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE states above, one plane over: what varies is only
> whether the slot holds a magnitude or a gate, never how it is kept current.
> ⛔ Re-deriving a whole BLOCK because something in its vicinity happened is the legacy read path RESCHEDULED
> from read-time to event-time, not deleted — the same single error the packages express per CHANNEL and the
> contexts express per BLOCK.
>
> **⚖ A PLOT'S PREDICATES FOLLOW MEMBERSHIP, AND OWNERSHIP IS A MEMBERSHIP FACT.** *"When a city gains or
> loses ownership, the `HAS_RIVER`, `HAS_COAST` and whatever other predicates associated with that plot need to
> be added to / removed from the city in question — that is how it has to work."* So the ONE applier
> (`CvCity::onCityPlotChanged(plot, ±1)`, which folds the plot's STORED bitset) fires on **every membership
> change**, not on the worked-radius relation alone:
>
> | membership fact | what moves |
> |---|---|
> | the plot gains / loses this city's OWNERSHIP | the whole of that plot's bits, `±1` each |
> | the plot enters / leaves the worked radius | the same fold, same applier |
> | a MEMBER plot's own bits move | **the PLOT announces the bit** -- `add(bit, ±1)`, nothing re-derived |
>
> ⚑ **THE FIRST TWO ROWS ARE ONE FACT, not two routes — `CvPlot::setOwner` CALLS `updateWorkingCity`.** So an
> ownership change re-assigns the working city and announces
> `SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED`, which is the membership fold; a city cannot work a plot it does not
> own, so the two triggers cannot come apart. ⛔ **Adding a second route on `SEVT_PLOT_OWNER_*` into the same
> applier would therefore DOUBLE-COUNT** — the wrong-wiring class
> ([neither playability nor compiling gates removing legacy](../specs/validation.md#playability-not-a-gate)), not a gap to close.
> ⚠ The ORDER composes exactly, which is worth knowing rather than re-deriving: `setOwner` writes `m_eOwner`
> first, so the `IS_OWNED` bit crosses and is withdrawn by its own predicate fact BEFORE `updateWorkingCity`
> folds the remaining bits out — no bit is subtracted twice.
>
> ⚑ **One applier, several facts** — never one fact per relation with its own derivation, and never a re-scan of
> the city's plots to find out what it now has.

> **⚖ THE SANCTIONED EXCEPTION — AN EVENT-TRIGGERED RECALC, WHERE THE FACT CANNOT NAME WHAT MOVES.**
> *"It is the best example of event triggered recalc we need."* The rule above assumes the fact NAMES the thing
> that moved, so the applier can set it. Where that assumption fails the recalc is CORRECT, and banning it on the
> word `refresh` mistakes the name for the mechanism.
> ⚑ **The exemplar is `DISTANCE_TO_GOVERNMENT_CENTER`** (`CityContext::refreshGovernmentCenterDistance`, driven by
> `SEVT_CITY_AMENITY_ADDED / _REMOVED` filtered to the `governmentCenter` key): the value is a
> **MIN over the player's government centres**, so
> a centre appearing in ONE city can shorten the distance for EVERY city, and one disappearing forces a re-derive
> against the remaining set. The fact names the city that gained or lost the designation; the values that move
> belong to all the others.
> ⇒ **THE TEST, and it is narrow:** a recalc is sanctioned when (1) a genuine DOMAIN fact triggers it, (2) the
> consequence is NON-LOCAL — the fact cannot name the values that move — and (3) no finer route exists to derive,
> because the quantity is an aggregate over a set (a min, a nearest, a wholesale identity reassignment) rather
> than a sum a delta could carry. `SEVT_AREAS_RECALCULATED` is the other instance, for the same reason.
> ⛔ **What stays banned is unchanged, and none of it is this:** a recalc with NO naming fact (per-turn, blanket,
> or on-read); one that papers over a MISSED invalidation
> ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)); and one that RE-DERIVES A WHOLE BLOCK because something in
> its vicinity happened when the fact could simply have set what it named — the legacy read path rescheduled to
> event time, which is what the callouts above retire.
> ⚠ So *"a context carries no `refresh*`"* is about the MECHANISM, never the spelling: the question to ask of one
> is **what triggers it, and could the fact have named the value instead** — not what it is called.

The stored aggregate rides events, exactly like the rest of the spine; a missed event drifts it, but that is the
event spine's **baseline invariant** (plot-groups and vicinity drift the same way if events are incomplete), not a
context-specific weakness. There is **no blanket per-turn rebuild** and no recompute-on-read.

⛔ **AND NOTHING HEALS A MISS — that is what makes incomplete wiring safe to grow.** No periodic or per-turn
context refresh, no "rebuild if it looks stale", no lazy recompute-on-read when a store looks empty, no staleness-timer
sweep, no validity/epoch stamp that triggers re-derivation, and no "recompute once per turn to be safe" backstop —
not as a safety net, not transitionally, not "just for load". If a store ever seems to need a "make sure it's
current" call, that is a **missing fact to report**, never a recompute to add — the full reasoning (why a missing
emit is the failure that should survive) is § A SELF-HEAL IS THE
FOSSIL OF A MISSING EMIT, above ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban); CAPSTONE — LOAD is the only full build).

- **`PlotContext`'s verdict bitset** ← the plot-substrate DOMAIN facts — terrain / feature / improvement / route /
  bonus / owner / **plot type / river / irrigation / landmark / worked** — consumed by `PlotContext` ITSELF
  ([a context dictionary is a spine consumer](11-context-stores-vs-forwards.md#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade)), which sets the bits the announcing fact FEEDS
  and nothing else.
  > **⚖ THE ROUTING IS DERIVED FROM A PER-BIT TABLE, never hand-written per event.** Each bit declares its own
  > derivation AND the substrate AXES it reads, side by side; a fact re-derives exactly the rows whose axes it
  > moved. That is what answers the hazard the retired whole-block derivation was right about — a hand-written
  > per-event bit mask drifting from what the bits actually read — without recomputing everything to avoid it. A
  > new bit is one row; a new fact is one axis.
  > **⚖ HAS_COAST IS SYMMETRIC: LAND WITH ADJACENT WATER, *OR WATER WITH ADJACENT LAND*.** Off the stored
  > bits that is ONE statement — **a neighbour whose `IS_WATER` differs from mine**, i.e. the plot sits on the
  > land/water boundary — so the verdict reads entirely off blocks the stores already hold.
  > ⚠ It also fixes a live defect: the derivation this replaced called `isCoastalLand()`, which returns false for
  > a water plot outright, so **every water tile read `HAS_COAST` false**.
  > ⚑ **And it is what deletes the deferred-drain machinery.** The only reason the old derivation touched `CvArea`
  > at all was `isCoastal`'s `>= iMinWaterSize` test, and the bare predicate passes `-1` — a comparison no existing
  > area can fail. With no area dereference there is no unsettled-map window to defer against, so the mark/drain
  > pass, its per-plot byte vector and the `isFinalInitialized` gate all go. *(The city-scope
  > `{HAS_COAST:{minArea:N}}` form is the one that genuinely needs the water-body SIZE, and it stays
  > `CityContext`'s.)*
  > ⚖ **`HAS_FRESHWATER` keeps calling `CvPlot::isFreshWater`, deliberately** — a seven-leg verdict the ENGINE
  > still consults for irrigation and farm gates, so re-expressing it over stored bits would fork a live predicate
  > into two implementations that drift ([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
  > Deriving a bit by calling the ONE accessor is what that rule asks for; what is banned is re-deriving the WHOLE
  > BLOCK, which per-bit routing is precisely what stops. Its neighbour leg is the one walk left, and it is now
  > paid only when a fact that actually feeds it arrives.
  > **⛔ THE PRICE OF CALLING A LIVE ENGINE PREDICATE FROM A LOAD-STREAM FACT: IT MUST BE TOTAL AGAINST THE
  > NOT-YET-READ SENTINEL.** `CvMap::read` fills the map ONE PLOT AT A TIME and each plot announces as it lands,
  > so a derivation that reaches a NEIGHBOUR reaches one that may still hold `NO_TERRAIN` / `NO_FEATURE` / no
  > city. Every such leg tests its sentinel and answers false; an unguarded `getTerrainInfo(NO_TERRAIN)` is a
  > fail-loud info-plane read ([the info plane is write-once-at-load](../architecture/patterns/04-the-info-data-out-contract-what-an/01-write-once-at-load-a-read-never.md#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud)) and kills the
  > load outright. ⚠ **The self-correcting load order does NOT cover this** — it guarantees the VALUE converges
  > (the neighbour's own fact fans back and both sides re-derive), which is worth nothing if the first pass
  > raised. Convergence is about the answer; totality is about surviving to give one.
  > ⚑ **The distinction that decides which bits are exposed: what the leg READS.** `HAS_COAST` reads the
  > neighbour's STORED `IS_WATER` bit, so an unread neighbour reads false and the row is safe by construction;
  > `HAS_FRESHWATER` reaches through a live `CvPlot` accessor into the info plane, so it is not. That is the cost
  > of the [the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law) carve-out above, and it is worth
  > paying — but it is paid HERE, by making the engine leg total, never by re-introducing a deferred drain pass.
  > **⛔ THE FACT SETS THE BIT — it does not trigger a callback that goes and asks.** *"Those 'refresh'
  > functions are legacy-inspired rollerskating."* Re-deriving the WHOLE block through the same `CvPlot`
  > accessors a read used to call is the legacy read path RESCHEDULED from read-time to event-time, not
  > deleted — and this document bans that exact computation two sections up (§ a forwarded read that COMPUTES,
  > whose worked example is `isCoastalLand()`'s 8-neighbour scan). Running it once per EVENT instead of once per
  > READ is the same defect on a different clock.
  > ⚑ **It is ONE error on two planes, not two errors:** recalculate-instead-of-delta-derive, which the
  > packages expressed per CHANNEL and the contexts express per BLOCK. ⚠ Their ORIGINS differ and that is worth
  > keeping straight — the package protocol was designed that way and faithfully built
  > ([superseded-ideas](../architecture/superseded-ideas.md) #30: a superseded design, not a rollerskate), while these imported
  > the legacy read path. Same shape, different provenance, one fix.
  > ⚑ **It is [a staleness flag is the fossil of a missing emit](03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) wearing a second costume: both
  > throw away the fact's identity.** A staleness flag reduces the fact to *"something moved"*; a whole-block
  > re-derivation ignores WHICH bit the fact names. The spine already carries the answer: the fact NAMES the new
  > terrain, so a terrain fact SETS `IS_WATER` and never calls back to ask what the terrain is.
  > ⚠ **What the retired justification was right about, so the fix does not re-introduce it:** *"one uniform
  > derivation, never a bespoke per-event bit mask"* guarded against a hand-written per-event mask drifting from
  > what the bits actually read — the same hazard [every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)
  > names. The answer is the packages' answer: **DERIVE the routing, never hand-write it.** What each bit reads is
  > declared beside that bit's own derivation — a small, checkable, per-BIT statement (eleven of them), never a
  > per-EVENT judgement call.
  > ⚑ **The ADJACENCY half cannot be set from one plot's payload and does not need to be rescanned either:** a
  > neighbour's coast / fresh-water verdict reads the announcing plot's **STORED block**, never a fresh walk back
  > through `CvPlot`. Same move, one hop out.
  > **⚖ THE FAN-OUT RIDES THE AXIS, NEVER A BIT'S OWN CROSSING — that is what BOUNDS it to one hop.** Only
  > `TYPE` and `TERRAIN` are neighbour-visible (a neighbour's `HAS_COAST` reads my `IS_WATER`; its
  > `HAS_FRESHWATER` reads my water + fresh-terrain state), so those two axes re-derive the 8 neighbours'
  > adjacency rows and nothing else does. An adjacency verdict is read by nobody's adjacency verdict, so a
  > cascade is structurally impossible rather than merely avoided — and `IS_WORKED` is excluded by construction
  > rather than by an exclusion anyone has to remember.
  > ⚑ **It also makes the LOAD ORDER self-correcting, which is what retires the drain pass:** whichever plot of a
  > boundary pair is read second re-derives BOTH sides, so a stream that fills the map in any order converges with
  > no deferral, no marks and no final sweep.
  **The store's own consumer is the ONLY maintenance entry** — every plot mutation that moves a stored verdict
  emits its own DOMAIN fact, so no choke point calls a derivation directly
  ([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
  > **⛔ THE CHOKE POINT ANNOUNCES; IT DOES NOT APPLY.** `CvPlot::updateWorkingCity` used to fold DIRECTLY beside
  > its emit, with the consumer skipping the fact at play to compensate. That is a SECOND surface maintaining one
  > fact, and it double-counts the instant the consumer grows the route — the exact failure
  > [a context dictionary is a spine consumer](11-context-stores-vs-forwards.md#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade) exists to prevent. The mutation site owns the
  > SOURCE, never the store. ⚑ Nothing is lost by moving it: `emit()` dispatches SYNCHRONOUSLY, so the fold still
  > lands at that instant, and each side resolves ITS city from the fact's own payload rather than from
  > `m_workingCity`, which has already moved by then.
  > ⚠ **The consumer's play-time SKIP was the tell.** It ignored the membership fact "because the choke point
  > already applied it", which is what a second maintenance surface always looks like from the store's side.
  > A wrong wiring like this is removed ON SIGHT and an interim double-count is accepted rather than weighed
  > ([neither playability nor compiling gates removing legacy](../specs/validation.md#playability-not-a-gate)).

  A MEMBER plot's bits moving reaches the counts through the **PLOT's own announcement**
  (`SEVT_PLOT_PREDICATE_ADDED / _REMOVED`, carrying the `CASC_PRED_*` id): when a member plot's verdict bit moves,
  the PLOT says so and the dictionary applies `add(bit, ±1)`.
  > **⛔ THE PLOT SENDS IT UP THE CHAIN; THE CITY NEVER REACHES DOWN FOR IT.** A city-side maintainer
  > that "unfolds the old bits and refolds the new ones" cannot work and must not be built: by the time any
  > consumer runs, the plot's bitset already holds the NEW value, so the old bits are gone and recovering them
  > means re-deriving the block -- the legacy read path rescheduled from read-time to event-time, which this
  > document bans two sections up. Let the object care about itself
  > ([tally.md](../specs/tally.md)) and the dictionary consume the fact
  > ([a context dictionary is a spine consumer](11-context-stores-vs-forwards.md#what-a-context-stores-vs-forwards---a-context-is-an-event-built-store-not-a-forwarding-facade)).
  > ⚠ **THE FAILURE IF IT IS MISSING IS NOT A STALE GATE -- IT IS A COMPOUNDING MAGNITUDE.** `plotAttrs` is
  > plane B's COUNT (§ THE MAINTAINED SUM, above), so a bit that is
  > never withdrawn leaves every deposit scaled on it (`+1 food per flatland plot`) inflated permanently, and
  > inflated further on every subsequent substrate change.
  > ⚑ The MEMBERSHIP case is different and needs no announcement of its own: a plot joining or leaving folds
  > that plot's CURRENT bits, which are readable where they are.
- **`CityContext`'s other blocks** ← each maintained by the fact that names what moved, routed through the same
  consumer. ⚠ These are on the same re-derive-whole shape the callout above retires, and they convert the same
  way — the target is the fact SETTING what it names, never a re-run of the block's whole derivation because
  something in its vicinity happened:
  - the **VICINITY store** ← the radius tiles' bonus / owner / worked facts, each applying `±1` through the ONE
    write point.
    > **⚖ THE CITY DEFINES ITS OWN POTENTIAL WORK AREA, AND THAT IS UNAVOIDABLE — because the cross
    > GROWS** (culture level, `adds3rdRing` — the two sources `CvCity::hasThirdRing` owns), so no fixed geometry
    > can answer it.
    > The city hands that definition to the plots as `CvPlot`'s **`workableBy`** membership, announced per plot as
    > `SEVT_PLOT_WORKABLE_BY_ADDED / _REMOVED`; the fold then reads the plot's own list and is EXACT.
    > ⛔ **There is no radius inverse and no membership test** — a store keyed on the radius folds a DELTA, and a
    > radius GROWING is an ordinary fact rather than something a walk must rediscover.
    > ⚑ **THE ADDRESSING IS WHAT MAKES IT CHEAP, and it is already defined: the city-plot table is
    > RING-ORDERED** — index 0 the city, 1–8 ring 1, 9–20 ring 2, 21–36 ring 3 — so a radius IS a prefix of it and
    > a level change is exactly the index range `[oldCount, newCount)`. Nothing geometric is rebuilt;
    > `CvCity::changeWorkableArea` walks that range and is the whole maintenance surface, reached from three sites
    > (the city starting to exist, ceasing to, and `setCultureLevelInternal`).
    > ⚑ **The same route SEEDS the store**, so there is no separate build pass: a city establishing its work area
    > announces one membership fact per plot, at birth and again at `GAME_LOAD_FINISHED` — where the map streamed
    > before the players, so nothing could have announced to a city that did not yet exist.
    > ⚠ It is DERIVED: zeroed at `CvPlot::reset` and never serialized, since a recycled plot would otherwise name
    > a city from the previous world ([derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-)).
    > ⚠ `m_iCityRadiusCount` / `m_aiPlayerCityRadiusCount` keep their own readers and stay — what this replaces is
    > the vicinity fold's need to re-derive membership, never those counters.
  - the **AREA facts** ← the plot-TYPE fact near the city, the per-area **`SEVT_AREA_TILE_ADDED / _REMOVED`**
    (one area's tile count moved — only the cities IN that area re-read), and the wholesale
    **areas-recalculated** fact below.
    > ⛔ **The per-area route DECLINES while `CvMap::recalculateAreas` is mid-pass** (`isRecalculatingAreas`).
    > That pass clears every plot's area and reassigns every id, firing the per-area fact once per plot, so a
    > per-tile refresh inside it would be O(plots × cities) of work against a map that does not exist yet — and
    > the wholesale fact closes the bracket by refreshing every city once, which is the answer for that window.
    > ⚑ The EMIT is untouched: the fact fires and the CONSUMER declines it
    > ([spine.md](../spine.md) — never suppress an emit to fix a consumer).
  - the **holy-city and HEADQUARTERS counts** ← their own facts, applied `±1`.
    > **⚖ THE DESIGNATION LIVES ON `CvGame`, AND THE CITY HOLDS ONLY HOW MANY NAME IT.** The authoritative
    > assignment is `CvGame`'s, keyed by religion / corporation — exactly one city each, so uniqueness is
    > STRUCTURAL there and a per-city bit could never guarantee it. What the city needs is the bare verdict,
    > and that is a count.
    > ⛔ **The bare verdict is NOT asked of `CvGame` per entry.** `CvCity::isHolyCity()` / `isHeadquarters()`
    > used to walk the WHOLE religion / corporation registry asking `getHolyCity(r) == this` once per entry —
    > on AI paths, and forwarded to by `CityContext::isHeadquartersAny()`, so a context whose premise is O(1)
    > bare fetches was forwarding to a registry scan. That is the forwarded-read-that-COMPUTES defect this
    > document names, and the test settles it: **ask what the read WALKS** — one pointer forwards, a registry
    > scan earns a store.
    > ⛔ **And it is a DELTA store, not a refreshed one.** The holy-city count was previously maintained by a
    > `refreshHolyCity()` the fact CALLED — the legacy read path rescheduled from read-time to event-time, not
    > deleted. The fact now applies `±1` and nothing re-derives. ⚠ Consequently there is **no build pass** for
    > either count at city-founded or load-finish: the facts already carry them (`CvCity::read` announces every
    > designation the city holds), and a rebuild beside a delta store doubles it.
- **`EmpireContext.policies`** ← the **civic / trait / player-init DOMAIN facts**, consumed by the policy store
  itself, which applies each grantor's policy block as a DELTA — never a refill, which would recount every time
  and so hide the multi-grantor case the count exists for. It is the single
  source the one policy read (`ev_playerHasPolicy`) uses — reads never re-walk the grantors. The **player-init** fact
  is load-bearing on its own: a player's INITIAL traits are written straight into the has-array rather than through
  the trait setter, so that fact is the only announcement they ever make.
  ⛔ It is deliberately **not** maintained from `CvPlayer::setCivics` / `setHasTrait`: a direct hook beside an event
  is a second maintenance surface for one fact, and the fact already exists.
  ⚑ **This store is the ONE home of the per-flag policy verdicts** — the boolean getters (`isNoForeignTrade`,
  `isStateReligion`, `isInquisitionConditions` and their kin) read `has`, and the AI civic-value what-ifs read
  `count` with an unconditional vacuum subtraction of the option slot's own civic. The serialized per-flag
  counters `processCivics`/`processTrait` once pushed beside it were the second-surface class and are cut
  ([the uniform legacy-accumulator cut](03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism)); their changers' side-effect riders
  live at the adoption site (`setCivics`), diff-gated on the policy actually moving. Genuine non-cascade state
  on that same class (the revolution index, `changeMaxConscript`, `changeSpecialistValidCount`, hurry counts)
  stays serialized as-is.
- **The HELD-TRAIT set** ← the **same trait / player-init facts**, consumed by its own store beside the policy one
  (one dictionary per area of responsibility — the `TRAIT_` and `POLICY_` key spaces are disjoint registries).
  > **⚖ IT IS THE CASE WHERE ONE AXIS SPLITS ACROSS THE SCAN-vs-HOP TEST, and reading the FORWARD row as settling
  > it is the mistake to avoid.** *Does this player hold `TRAIT_X`* resolves through one pointer, so trait
  > PRESENCE is correctly a forward and earns no store. *Which traits does this player hold* is a different
  > question with the same subject: off `m_pabHasTrait` it walks all 369 trait records to rediscover the handful a
  > leader carries — the O(registry) sweep this document names on the unit plane (§ THE READ PATH, below: *"the
  > sum walks what the unit HOLDS, never the registry"*). ⇒ **Ask what the READ walks, never what the
  > SUBJECT is:** the same axis can forward one question and store another.
  > ⚑ **Its reader is the keyed-deposit walk** (§5): a trait's target-keyed
  > deposits stay SOURCE-side (the §4 per-set carve-out), so the read asks each LIVE SOURCE what it deposits onto
  > that key. That read is cheap *"because it iterates the handful an entity AUTHORED"* — which holds only if
  > discovering the live sources is itself cheap.
  > ⛔ **The sign comes from the fact's IDENTITY, never from a re-read.** `setHasTraitInternal` writes the
  > has-array BEFORE it emits, so a handler asking `hasTrait` would read the NEW value on both ends and never
  > withdraw — the same reason a city can never reach down for a plot's old bits.
- **AREAS are announced WHOLESALE.** `CvMap::recalculateAreas` clears every plot's area, empties the area list and
  recalculates, so it emits **`SEVT_AREAS_RECALCULATED`** (no payload — the fact IS "all of them") and every holder
  of an area id re-reads. Areas are virtually never recalculated (terrain levelled to sea level — the WMD mechanic —
  plus map generation), so the blanket costs nothing at its real frequency, and it is **not** the banned self-heal: a
  wholesale identity reassignment is not addressable per-source, so no finer route exists to derive
  ([self-heal is not a backstop](03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban) bans papering over a MISSED invalidation, not announcing a
  genuine wholesale one).
- **Forwarded** fields need no maintenance — they read the live source.
- **Load** — `EmpireContext.policies` rebuilds from the **in-read civic/trait/player-init emits** as they stream (a
  derived aggregate recomputes from source on load, [derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-),
  never trusted from a save) — through the consumer, not a second build mechanism beside the event stream.
  `CityContext`'s other blocks build once at `GAME_LOAD_FINISHED`, because each reads state that is only complete when
  the whole stream has ended (the areas deserialize after the plots). That single pass IS the load build — the only
  full build there is — after which the facts alone maintain them.
  ⚑ The VICINITY dictionaries, `onSite` included, are not among them: they seed through the ORDINARY membership
  route, because re-establishing each city's work area announces one `SEVT_PLOT_WORKABLE_BY_ADDED` per plot and the
  applier folds that tile's CURRENT bonus and served resource. One route, both jobs — no separate build pass to keep
  in step ([the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed): never a second build mechanism beside the event stream).
  `CityContext.plotAttrs` builds from the in-read DOMAIN events
  ([the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)): each `CvPlot::read` announces its deserialized working-city
  fact (`SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED` — the genuine read site emits), and `CityContext`'s own consumer
  BUFFERS the load bracket's membership facts and folds them through the one applier
  (`CvCity::onCityPlotChanged`) at `GAME_LOAD_FINISHED` — the cities stream AFTER the map, so the fold applies once
  after the stream ends (the [enabler §7.1](../specs/enabler.md) order rule's second option, never the mixed form).
  ⚠ **The buffer is an ORDERING fact, not a staleness mechanism** — there is no city to fold into while the map
  streams. The per-bit facts need no such treatment and are simply dropped inside the bracket: by the drain every
  plot has announced its substrate, so the fold reads FINAL bits and applying them earlier would only count them
  twice. There is never a blanket per-turn recompute.

