# Event spine — the one dispatch primitive

> A **core spec.** The event spine is **where the consumers get their events** — it is *not* logging; logging is one
> consumer of it (grants, cache-invalidation, and the out-of-process replay are others). One `emit`, fanned out by
> KIND to every registered consumer. *(The in-engine **tally** is NOT a consumer — it reads the object-owned counts
> directly; [tally.md](tally.md).)*

## The primitive
A caller `emit`s an event; every consumer that registered interest in that event's **KIND** receives it. KIND is
declared **at the call site, never inferred**.

## KIND — the OOS firewall
Civ4 multiplayer is deterministic lockstep, so an authoritative count that differs per machine is a desync. KIND
keeps the synced and unsynced streams apart:

| KIND | meaning | synced? | consumed by |
|---|---|---|---|
| **`DOMAIN`** | game **state** changed (building built, unit created, tech researched) | yes — deterministic | logging + grants + cache-invalidation + out-of-process replay (NOT the in-engine tally — it reads the object-owned counts) |
| **`SAVELOAD`** | a fact was **read off the save stream** — a log of LOADING, never what sets state | no | **logging only** — never counted, never gates |
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

> **⛔ `SAVELOAD` IS ITS OWN KIND AND NOT A `DIAGNOSTIC`, AND THE DIFFERENCE IS LOAD-BEARING (owner).**
> `DIAGNOSTIC` means CODE RAN. A save-load fact is a record of what the STREAM CONTAINED — a different
> statement — so filing it under `DIAGNOSTIC` would put *"the save says this plot is `TERRAIN_GRASS`"* in the
> same bucket as *"this function was entered"*, after which only convention separates them.
> ⚑ **Its own kind makes the rule STRUCTURAL rather than remembered:** a state-building consumer registers for
> `DOMAIN`, so *"nothing derives held state from the load log"* is enforced by the interest mask, not by
> reviewer memory — the contract-not-prohibition shape ([patterns.md](../architecture/patterns.md)).
> ⚑ It also gets its own volume story for free: the load record is the highest-volume stream in the engine, and
> as a separate kind it never has to ride the `DIAGNOSTIC` firehose to be watched.
> ⛔ **It needs NO gate knob of its own** — volume rides the event's own `iLevel` through the existing file
> (`gPlayerLogLevel`) and stream (`gStreamLogLevel`) gates. Only `DOMAIN` streams unconditionally; a load
> record must never spend the bounded SSE slots during ordinary play.
> ⚠ **The load's DOMAIN facts are NOT these.** The save read populates base state through the objects' own
> INTERNAL SETTERS, and those setters emit the ordinary `DOMAIN` facts that build the cascade, the enabler and
> the contexts — one mechanism for load and for play. A `SAVELOAD` line is testimony ABOUT the read, beside
> them, and nothing folds on it.

Only `DOMAIN` events carry authoritative synced state-changes (for observability, cache-invalidation, and the
out-of-process replay). The in-engine [tally](tally.md) does **not** consume them — it reads the object-owned counts
directly. The payload is **raw** (typed fields,
never a pre-formatted string) so the costly index→text formatting defers to the gated [logging](logging.md)
consumer — when a gate is off, nothing expensive ran.

## The `IEventConsumer` contract
Consumers attach through **one C++03 interface, `IEventConsumer`** (a pure-virtual base, no data members) — the
`grants` and logging are independent implementations pluggable behind it (the realized exemplar of the
project's [interface-contract pattern](../architecture/patterns.md)); the [tally](tally.md) is **not** a consumer (it
reads objects). **Build order:** spine + the modifier scope accumulator → logging (broad) → grants →
[modifier](modifier.md) → [enabler](enabler.md). *(The tally is a read-only accessor, not a step on the spine.)*

## The C++ shape (`CvEventSpine.{h,cpp}`)
- **`CvSpineEvent`** is a POD carrying **two payloads, not two exclusive modes**: the raw **DOMAIN state ints**
  (`iType`/`iA`/`iB`/`iC` + `iSrcLoc` = WHERE), which `grants` and the cache-invalidation consumer read; **and** the
  **render payload** (`iDomainTag`/`iEventId`/`aFields[]`, `SPINE_MAX_FIELDS = 16`; a field is `{int eTag; union{int
  i; float f; char* s; wchar_t* w;}}`, 8B/POD) that the one logging path formats.
  ⛔ **THE FIELD CAP IS A SILENT CEILING — `addI`/`addStr` DROP a field past 16 rather than failing.** So a
  census line that has grown to exactly 16 cannot be extended at all: the seventeenth term is simply absent from
  the rendered line, and absent reads identically to zero. ⚑ Check a line's field count before adding a term,
  and when it is full the answer is a SECOND event, never a swap of one term for another — which is the right
  shape anyway wherever the new term has an axis of its own (a per-type row is not a term).
  *(`[MODIFIER] rateRead` stands at exactly 16, which is why the `specialists` decomposition is its own line.)* A **`DOMAIN`** event carries BOTH —
  its state ints for the machine consumers **and** a domain tag + fields so it renders through the same registered
  path as everything else; a **`SAVELOAD`/`DIAGNOSTIC`/`TRACE`** event carries only the render payload. There is no
  inline-formatted event: the spine's own DOMAIN events register under `SD_SPINE` exactly like an AI domain.
- **Per-domain isolation:** a domain registers via `spineRegisterDomain` (a line-prefix fn + a field-info fn with
  typed index kinds `SFT_BUILDING`/`UNIT`/`BONUS`/…); `spineRenderEventLine` formats. **Zero global field registry,
  zero shared edits per domain** — adding a domain touches only that domain. **The logging consumer is exactly
  `gate(iLevel) → spineRenderEventLine → write`** — no per-event branch, no inline `sprintf`; a line's identity is
  entirely its registered prefix + fields. Every rendered line carries the game turn as its first field
  (`[TAG] t=NNN …`) — after the tag so prefix-anchored greps keep working — making each line self-placing in
  time (when did this actually fire) instead of inferred from burst position.
- **The `/events` STREAM is its OWN registered consumer** (`CvSpineStreamConsumer`) — never a tee inside the
  logging consumer (that chained stream visibility to the FILE gate). **DOMAIN events stream UNCONDITIONALLY**
  whenever the HTTP server is up (the facts the machine consumers see — the out-of-process replay feed);
  SAVELOAD/DIAGNOSTIC/TRACE lines stream at the stream's own verbosity knob (`gStreamLogLevel` /
  `Autolog__LogLevelStream`), fully decoupled from `gPlayerLogLevel` — streaming everything never requires
  opening the level-4 file firehose. ⚠ SAVELOAD is deliberately NOT on the unconditional side: the load record is
  the highest-volume stream in the engine and would exhaust the bounded SSE slots during ordinary play. The SSE queue is capped; on overflow the first frame that fits again
  reports `[STREAM] dropped=N` — a gap is always visible as a gap, never silent.
- **Interest guard:** an `m_iInterestMask` bit-test gates dispatch, so the verbose call-site `if(logLevel)` gates
  vanish structurally.
- **Allocation-free hot path** (stack-buffer formatting, a bounded `/events` queue) — 32-bit ceiling discipline.
- **Name-change event** (`SEVT_NAME_CHANGE`): the four set-name choke points emit `(NameChangeKind, owner,
  entity_id)` in the DOMAIN ints (an out-of-process consumer keys on those). Because the logging consumer is generic,
  the `emitNameChange` endpoint resolves the NEW name + kind LIVE and passes them as render fields (`SFT_STR` kind +
  `SFT_WSTR` name — the emit render is synchronous on the game thread, so the borrowed pointers outlive it). This is
  the one place a spine endpoint does resolution at emit rather than deferring to the gated render — justified because
  a rename is rare (four low-frequency choke points), not a hot-path firehose.
- **Build status.** The spine primitive + KIND firewall + `IEventConsumer` = BUILT (`Spine/CvEventSpine.{h,cpp}`).
  The **DOMAIN emit surface = BUILT**: **127 call sites** at the genuine mutation choke points — `CvPlayer` (36) ·
  `CvCity` (30) · `CvPlot` (20) · `CvUnit` (14) · `CvGame` (10) · `CvProperties` (7) · `CvTeam` (6) · `CvArea` (2) ·
  `CvMap` (1) · `CvPlotGroup` (1). The PLOT substrate is
  complete: terrain / feature / improvement / route / bonus / owner / **type / river / irrigation / landmark /
  worked**, so the per-scope contexts are maintained purely by facts, with no choke point driving a derivation
  directly ([contexts.md](../architecture/contexts.md)).
  ⚖ **BESIDE THE SUBSTRATE FACTS, THE PLOT ANNOUNCES ITS OWN DERIVED VERDICT: `SEVT_PLOT_PREDICATE_ADDED /
  _REMOVED`, carrying the `CASC_PRED_*` id.** It is emitted by `PlotContext` — the store that OWNS the verdict —
  at the 0 ⇄ 1 crossing and nowhere else, exactly as the amenity fold announces its own crossings: the fold IS
  the maintenance path, so an emit anywhere else would be a second one.
  ⛔ **It is NOT a duplicate of the substrate fact and never replaces one.** A substrate fact says what the TILE
  now CARRIES; this says what that MEANS for the one predicate that moved. A consumer routing on a substrate id
  is asking about the SOURCE; a consumer routing on this is asking about the VERDICT.
  ⚑ **It exists because the city cannot derive it.** `CityContext.plotAttrs` is the fold of its member plots'
  bits, and by the time any consumer runs the plot already holds the NEW value — so a city-side "unfold the old
  bits, refold the new" is impossible, not merely wasteful ([contexts.md](../architecture/contexts.md): the plot
  sends its bit UP, the city never reaches down). With the fact, a member plot's bit is one `add(bit, ±1)`.
  ⚠ Its absence would not read as a stale gate but as a **COMPOUNDING MAGNITUDE**: `plotAttrs` is plane B's
  COUNT, so a bit never withdrawn leaves every deposit scaled on it permanently inflated, and inflated further on
  every later substrate change.
  ⛔ **THE SUBSTRATE FACTS ARE `ADDED`/`REMOVED` PAIRS, NOT `CHANGED` (owner ruling — see § A FACT NAMES THE
  HAPPENING).** Terrain / feature / improvement / route each announce a source LEAVING and a source ARRIVING as
  two facts, because each end is its own consumer work: the old source's deposits are withdrawn, the new
  source's applied. ⚑ Carrying the old value in `iA` on one `CHANGED` fact was the earlier shape and it is what
  left the gap — a single "the slot moved" fact makes every consumer DERIVE the removal, and the derivation is
  impossible once the state has moved. A `REMOVED` fact is emitted while the old state still holds, so a
  withdrawal resolves against exactly what it deposited ([state-repositories.md](../architecture/state-repositories.md)
  § THE INVARIANT).
  ⚖ **THE WHOLE FAMILY IS `<SCOPE>_<THING>_ADDED` / `_REMOVED`, SCOPE-QUALIFIED (owner):** `PLOT_BONUS_ADDED` /
  `PLOT_BONUS_REMOVED` beside `CITY_BONUS_ADDED` / `CITY_BONUS_REMOVED` — a resource appearing ON A TILE and a
  city GAINING that resource are different happenings with different consumers, so the scope belongs in the
  name rather than in a reader's head.
  ⛔ **AND A ±1 IN THE PAYLOAD IS NOT A SUBSTITUTE FOR THE NAME.** A `CHANGED` fact carrying a placed/removed
  delta still hands the consumer a discriminator to branch on, which is the calculation relocated into a
  `switch` (§ A FACT NAMES THE HAPPENING) — it is an improvement on an old-id payload and it is not the answer.
  The direction belongs in the FACT'S IDENTITY, where a consumer reads it by arriving at all.
  ⚑ **The payoff is that every consumer's direction-decoding collapses.** A consumer that today decodes three
  conventions — an id pairing, a presence boolean in `iA`, a signed delta in `iB` — decodes none: the event it
  received IS the direction. The **commerce SLIDERS** are on the surface too
  (`SEVT_EMPIRE_COMMERCE_PERCENT_ADDED / _REMOVED`, `CvPlayer::setCommercePercent` — the one choke point `changeCommercePercent` /
  `verifyGoldCommercePercent` / `changeCommerceFlexibleCount` all reach the value through): a slider is synced
  player state every city's realized per-commerce rate is built on ([modifier.md](modifier.md) §2a), so DOMAIN.
  ⚠ **ONE slider move emits SEVERAL facts** — the setter REBALANCES the other channels in place to hold the total
  at 100, writing them directly rather than recursing, so each channel it moves emits its own fact; a consumer
  reading only the caller's channel sees a 100-total that does not add up. **PROPERTY VALUES** are on the surface
  too (`SEVT_PROPERTY_ADDED / _REMOVED`, the three `CvProperties` mutation choke points — `setValue` plus the two
  new-property `push_back` branches, which `changeValue` / `changeValueByProperty` / `setValueByProperty` all
  funnel through): `PROPERTY_*` is one cascade channel per property info
  ([state-repositories.md](../architecture/state-repositories.md)), read by `CityContext::propertyValue`, by every
  `requires.operate` property BAND ([enabler.md](enabler.md) §3) and by every threshold-conditioned deposit, and
  the value is synced save-carried state that folds into the OOS checksum — so DOMAIN. The fact names the object
  KIND beside the object id, because a city id and a plot id are otherwise the same int.
  ⛔ It is emitted at the `CvProperties` sites and **never** in `CvGameObject::eventPropertyChanged`:
  `CvGameObjectUnit` OVERRIDES that hook without chaining to the base, so an emit placed there is silently
  skipped for every unit. ⚠ The solver's change PROPAGATION fans one change onto OTHER objects, each of which
  re-enters the mutation path — distinct objects' facts, so each emits. The object RESET path
  (`CvProperties::clear`) deliberately announces nothing (it runs before there is an id or an owner to name —
  `CvCity::read` / `CvUnit::read` call `reset()` as their first act). The
  **load-lifecycle
  bracket** `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED` is emitted (`CvGame::read` / end of
  `CvGame::onFinalInitialized`), so `spineGameLoadInProgress()` reports correctly and result-producers suppress
  inside it. The **load reseed** = BUILT: the in-read emits fire from inside the save read (`CvPlayer::read` per
  held tech / project / civic / trait / heritage + era / golden-age / state-religion / nukes / commerce sliders,
  `CvCity::read` per building / religion + holy-city / corp / specialist / population / power / culture-level,
  `CvPlot::read`
  the whole substrate + owner + working-city — through the INTERNAL SETTERS, so the read announces nothing
  itself, `CvProperties::readWrapper` per stored property value on every owner scope),
  wrapped by the bracket. ⚠ The property block emits from its own read because a property value is DERIVED FROM
  NOTHING; nothing else could announce it. A stored 0 is skipped, for the same reason the setter suppresses a no-op (the owner's `reset()`
  emptied the bag, so 0 → 0 is not a change); the per-turn CHANGE ledger beside it is deliberately silent, being
  an accumulation of deltas the value facts already carry. ⛔ **ONE SERIALIZED SLOT, ONE FACT — there is no slot a neighbour's fact covers for.** This spec used to claim
  that a plot field deserializing with no emit of its own (type / river / irrigation / landmark / worked) needed
  **none**, because *"a substrate fact re-derives the plot's WHOLE verdict block"* so every plot was already
  covered. **That premise WAS event-as-command** — a consumer reaching back through the object to re-derive a
  block from a fact that did not name it — and it went with the model. A fact now sets ONLY the bit it names
  ([contexts.md](../architecture/contexts.md)), so a slot nothing announces is a slot nothing knows about.
  ⚑ **The fix is structural, not five remembered emits:** the emit lives in the slot's INTERNAL SETTER, so a
  newly serialized field cannot be added without going through the one body that announces it. That is what
  closed these five, and it is why the answer was never a longer hand-maintained list in the read.
  ⚠ **The WORKED set is the CITY's slot, so it reseeds from `CvCity::read`, not from the plot's.** The plot
  carries the `IS_WORKED` verdict but only the city can attribute it, so the array's in-read landing is where
  the fact comes from — and the cities stream AFTER the map, which is exactly what makes the plot available to
  carry the bit and puts the fact before the `GAME_LOAD_FINISHED` fold that reads each plot's FINAL block.
  **THE TIER-1 HOLES ARE CLOSED** — the facts a named consumer read but nothing announced.
  ⚖ **`isPowered()` announces ONCE, and what it announces is the VERDICT — never a leg.** `CvCity::isPowered` is the
  ONE definition (a live grantor supplies power AND no blackout gates delivery), and its crossing is announced by
  the AMENITY FOLD as `SEVT_CITY_POWER_ADDED / _REMOVED` — the fact the modifier's plane-C route and the enabler's
  gate both ride. Its inputs reach that fold and stop there: the grantor crossing itself, and the blackout status
  (`SEVT_CITY_STATUS_ADDED / _REMOVED` carrying `CITYSTATUS_POWER_DISABLED`), which is MIDDLEWARE gating delivery
  and is never a cascade input ([state.md](state.md) § A STATUS IS MIDDLEWARE).
  ⛔ **Announcing a LEG instead would be wrong twice**: no single leg is the verdict (a second plant built during a
  blackout moves the store and delivers nothing; a blackout lifting delivers power with the store unmoved), and
  routing several legs into one plane-C application would double-apply. ⚠ A status TICKS DOWN every turn, so it
  emits at the derived 0-CROSSING only, never per decrement — a counter that moves on a schedule is not a state
  change until its verdict flips, and this is the general rule for every timer-backed fact.

  > **⚖ THE THRESHOLD CROSSING IS ITS OWN FACT, AND THE HOLDER OF THE VALUE ANNOUNCES IT (owner).** *"There
  > should be events for when a threshold actually changes; that is done on the holder … if power goes from 0 to
  > 1 an event is emitted, but another event is not emitted from 1 to 2 — and if 1 to 0, then power removed is
  > emitted."* So a value's own fact says the VALUE moved, and a SECOND fact beside it says a VERDICT built on
  > that value crossed. The two are different happenings with different consumers, and the second is the one a
  > gate routes on.
  > ⚑ **Power is the shape; it generalizes to every threshold.** The second instance is the PROPERTY BAND:
  > `SEVT_PROPERTY_ADDED / _REMOVED` announces the value, which the solver moves for nearly every property of
  > every city every turn, while **`SEVT_CITY_PROPERTY_BAND_ADDED / _REMOVED`** announces the far rarer crossing
  > of a boundary some `requires.operate` clause actually declares ([enabler.md §3](enabler.md)).
  > ⛔ **The detection belongs to the HOLDER, never to each consumer.** A consumer that gates on the raw value
  > fact re-derives the same sweep once per consumer AND pays it per event — and the boundaries are one registry
  > (`EnablerKernel::propertyBandThresholds`), so testing them anywhere else is a second implementation
  > ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)).
  > ⚑ **And it is what makes plane C's WITHDRAWAL exact.** If the fact IS the crossing, a consumer applies or
  > withdraws on the fact's IDENTITY and never re-tests the atom — so it never depends on reading state the
  > mutation has already moved past, which is the one thing
  > [state-repositories.md](../architecture/state-repositories.md) § THE INVARIANT cannot enforce for itself.
  > ⚠ A band fact is deliberately DIRECTION-LESS in effect: the consumer re-reads the live value against each
  > band, so which way the boundary was crossed is redundant once the fact says one was.

  Beside them:
  **`SEVT_CITY_HEADQUARTERS_ADDED / _REMOVED`** (`CvGame::setHeadquarters`, per affected city — the `setHolyCity` shape, and
  **not** a duplicate of the building/corporation PRESENCE facts the same setter drives),
  **`SEVT_PLOT_CITY_ADDED / _REMOVED`** (`CvPlot::setPlotCity` — the ONE emit covering its `changeCityRadiusCount` /
  `changePlayerCityRadiusCount` pass-throughs), **`SEVT_CITY_GOVERNMENT_CENTER_ADDED / _REMOVED`** and
  **`SEVT_CITY_FRESH_WATER_ADDED / _REMOVED`** (the two `CvCity` counters, at their existing verdict crossings; the fresh
  water one is the provider-BUILDING-fed access counter, distinct from the plot-adjacency verdict the substrate
  maintains), **`SEVT_EMPIRE_ANARCHY_ADDED / _REMOVED`** (`CvPlayer::changeAnarchyTurns`), **`SEVT_TEAM_MEMBER_ADDED / _REMOVED`** and
  **`SEVT_AREA_TILE_ADDED / _REMOVED`** (the two bare counters `EmpireContext::teamMemberCount` / `CityContext`'s
  AREA_SIZE + max-adjacent-water read), and **`SEVT_WORLD_UNIT_CREATED_COUNT_ADDED`** (the world-instance cap's
  cumulative counter — distinct from `SEVT_EMPIRE_UNIT_COUNT_ADDED / _REMOVED`, the player's LIVE per-type tally, and from
  `SEVT_UNIT_CREATED`, the instance; all three fire at one birth and none duplicates another).
  **THE UNIT PLANE has its mark triggers** — [state-repositories.md](../architecture/state-repositories.md)
  specifies a unit's resolved values move *"ONLY when a promotion or combat class changes"*, and neither
  existed: `SEVT_UNIT_PROMOTION_ADDED / _REMOVED` (`CvUnit::processPromotion`, the ONE funnel both `setHasPromotion`
  overloads reach) and `SEVT_UNIT_COMBAT_ADDED / _REMOVED` (`CvUnit::processUnitCombat`, reached once past
  `setHasUnitCombat`'s change guard AND its game-option/spy validity gate). **`SEVT_UNIT_KILLED`** is the DEATH
  TWIN `SEVT_UNIT_CREATED` lacked — without it grants and the out-of-process replay see units born and never die.
  ⛔ Its correctness is **STRUCTURAL, not positional**: it is emitted on the FIRST line of **`CvUnit::die`**, the
  one function that ends a unit's life, which carries no early return and no conditional deletion and always ends
  in `deleteUnit` ([unit-lifecycle.md](../reference/unit-lifecycle.md)). The outcomes that leave a unit ALIVE
  (evacuate-to-capital, last-stand survival) are decided BEFORE `die()` is entered and never reach it, so a new
  outcome cannot silently slip in ahead of the fact — the shape a placement "past every early return" could not
  guarantee. An OFF-MAP death is a real outcome of that function, not a skipped one: `iSrcLoc` is -1 and the unit
  is deleted exactly as an on-map one is. Beside it, **`SEVT_UNIT_DEATH_SCHEDULE_ADDED / _REMOVED`** carries `m_bDeathDelay`, the
  save-carried state a DELAYED kill leaves behind so the object outlives combat resolution, read across the engine
  through `isDelayedDeath()`/`isDead()`. It is **not** a duplicate of KILLED: a scheduled death is an INTENTION
  whose outcome can still flip to survival, so a consumer treating it as a death would bury units that walk away.
  ⚠ Both TRANSITIONS announce (scheduled, and cleared by either survival outcome) — a one-way fact would leave a
  survivor permanently marked dying — and `CvUnit::read` carries the in-read half for a save taken mid-schedule.
  **`SEVT_UNIT_LEFT_CITY`** is the leave twin of `SEVT_UNIT_ENTERED_CITY`; ⚠ it is announced for EVERY city plot
  a unit vacates while the ENTRY's conquest branch resolves into an acquisition instead of an entry, so the two
  do NOT net to occupancy — a consumer needing occupancy reads the unit's live plot.
  **The reseed grew the matching in-read halves** wherever the setter cannot run on a load: `CvCity::read`
  (government-centre count, fresh-water counter, disabled-power timer — a save can be taken mid-blackout — and
  the headquarters designation, tested off the loaded IDInfo via `CvGame::isHeadquartersByOwnerId`, the
  `isHolyCityByOwnerId` precedent, because `CvGame`'s array deserializes before the cities), `CvPlayer::read`
  (anarchy turns — a save can load mid-revolution — the golden-age twin), `CvPlot::read` (the plot's city, whose
  fact no other slot covers — no slot ever covers for another), `CvTeam::read` (the member count, landed through `changeNumMembers` after `m_eID`
  deserializes rather than beside the count, since the id the fact hangs on is only valid from there — and that
  is the team's WHOLE reseed, because `EmpireContext` forwards exactly three team facts and the other two, techs
  and projects, are announced per-self from `CvPlayer::read`), and
  **`CvUnit::read`, which previously emitted NOTHING** — the instance, its promotion set and its combat-class
  set, each at its own genuine per-element read. ⛔ Two in-read halves are deliberately ABSENT and are not
  oversights: **the world unit-created counter** (nothing stores a derivative of it — the cap reads it live, so
  there is nothing to seed) and **the area tile count** (`SEVT_AREAS_RECALCULATED` is a WHOLESALE fact by
  construction — it names no source, so every area-id holder re-reads on it and a per-area announcement would
  say nothing the wholesale one has not).
  ⛔ Neither is the retired *"another slot's fact covers it"* argument, which is why they are stated in their own
  terms: one has no consumer to seed, the other already has a fact. **No slot is ever covered by a neighbour's.**
  ⚠ **One endpoint is deliberately unwired: `emitLoadPipeline`** — every one of its arguments is produced by the
  archived load-time warm-up/rebuild pass, which the CAPSTONE rule removed
  ([state-repositories.md](../architecture/state-repositories.md)); the event reseed replaced that pass, so the
  endpoint has no honest caller. Open follow-ups (the tile-driven vicinity backstop; the per-city enabler
  priming that preceded the reseed emits): [todo.md](../plans/structural-cleanup/todo.md).
    **GAME OPTIONS and DIFFICULTY announce** — the two facts every maintained verdict is built on but nothing used
  to announce. **`SEVT_GAME_OPTION_ADDED / _REMOVED`** (`CvGame::setOption` / `setModderGameOption`, both unguarded so the
  emit supplies the flip guard): an option is the ONE axis an entity-level gate reads
  ([DEC-entity-gate](../architecture/decisions.md#dec-entity-gate)), and options are read BELOW that level too
  (civics carry option-gated production / happiness / commerce deposits), so a flip moves gate verdicts AND
  deposits at once. ⚠ It carries TWO id spaces, so `iB` = `GameOptionSpace` disambiguates them (the
  `SEVT_PROPERTY_ADDED / _REMOVED` shape — a game-option id and a modder-option id are otherwise the same int).
  **`SEVT_EMPIRE_HANDICAP_ADDED / _REMOVED`** (`CvPlayer::setHandicap`) is a genuine cascade input rather than
  observability: the gather folds the handicap's own modifier families into that player's packages, so
  **FLEXIBLE DIFFICULTY moving it silently froze every handicap-derived deposit at the old difficulty** with
  nothing to re-derive it. **`SEVT_GAME_HANDICAP_ADDED / _REMOVED`** (`CvGame::setHandicapType`) is its DISTINCT twin, not
  a duplicate — the derived average over alive humans that every `getAI*` advantage reads
  ([engine.md](../reference/engine.md): AI advantages scale with the HUMAN's difficulty), derived and never saved,
  so it needs no in-read half.
  **`SEVT_GAME_GLOBAL_DEFINE_ADDED / _REMOVED`** completes that surface from the other side — the three
  `cvInternalGlobals::setDefine*` setters, i.e. the **LIVE-OPTION bridge**: a BUG option fires a Python callback →
  `GC.setDefineINT` → `cacheGlobals()`, so a user can flip an engine tunable at any time mid-game. It was the one
  mutation of that class with no fact at all, which made a live option unreactable by construction.
  ⚠ It announces ONLY on the genuine LOCAL set: the `bUpdate` path sends a net message and
  `CvGlobalDefineUpdate::Execute` calls straight back in with `bUpdate=false`, so announcing on both paths would
  double-emit one change on the initiating machine. And a define is STRING-KEYED with no id space, so the NAME
  rides as a render field (the `SEVT_NAME_CHANGE` precedent) and a machine consumer keys on that, not the ints.
  ⛔ Its existence does NOT make a live option something authored data may gate on — that ruling
  ([python-read-map.md](../reference/python-read-map.md)) is about a value moving under static data and is
  unchanged; the fact closes reactability only.
  ⚑ **Only the GAME space routes anywhere, and the two spaces differ in KIND, not just in id range.** A
  `GAMEOPTION_` is fixed at setup, which is what lets an entity gate depend on it; a `MODDERGAMEOPTION_` is set
  from the BUG menu at any time (`setModderGameOption` + a net message for MP sync), so it is a LIVE option
  wearing a confusingly similar name. Authored data honours that split — **no** authored gate or condition names a
  `MODDERGAMEOPTION_` — so a modder flip (a slider such as the leader-promotion culture threshold
  `MODDERGAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT`) moves no verdict and no deposit. It still EMITS, being a
  genuine synced state change; it simply marks nothing. That is "emit liberally, mark precisely" as a routing rule
  rather than a slogan. ⛔ Separating the two by grep needs a negative lookbehind — `MODDERGAMEOPTION_` contains
  `GAMEOPTION_`, so a naive scan conflates them.
  ⚑ Both option and difficulty route to **WHOLESALE** consumer work (the enabler re-gates every city; the modifier
  marks the affected player's packages whole) — the `SEVT_AREAS_RECALCULATED` shape, sanctioned for the same
  reason: the fact names no source to route from, so no finer derivation exists, and it is not the banned
  self-heal, which papers over a MISSED invalidation rather than announcing a genuine wholesale one.

**⚖ PLAYER ALERTS ARE A SPINE CONSUMER, RE-ADDED ON THE FACT (owner) — never re-inlined at a mutation site.**
The legacy notifications ("a building shut down", "a building was restored") were emitted from inside the
setter that changed the state, which is why they die with every legacy mutator that gets cut — and why they
cannot simply be kept: the setter they lived in is the duplicate being removed. They come back as a CONSUMER of
the DOMAIN fact that already announces the change (the operate crossing, the owner change, …), exactly as
logging and the `/events` stream are consumers.
⚑ **This is a growing list, not a one-off:** each legacy mutator cut takes its alerts with it, and they are
re-added together on the facts rather than one at a time inside whatever replaced the setter. What is currently
owed is tracked in [todo.md](../plans/structural-cleanup/todo.md).
⛔ An alert re-inlined into a machine's apply path is the same mistake in a new place: it makes a UI concern a
condition of the state change, and it is invisible to anything else that wanted to know.

**Registered consumers today:** the broad FILE logging consumer, the `/events` STREAM consumer, the **trigger
  engine** (`Triggers/CvTriggerEngine` -- the ONE payload machine, grants folded in as the null-condition case:
  resolver AND appliers built (`tr_applyTechFirstDiscover` / `tr_applyBuildingFirstBuild` / `tr_applyPerTurn` /
  `tr_applyCityPerTurn` / `tr_applySpawn` / `tr_applyFullHeal` / `tr_promoteFromEntries`), dispatched from
  `SEVT_TECH_ACQUIRED` / `RELIGION_FOUNDED` / `PLAYER_INIT` / `CITY_FOUNDED` / `CIVIC_ADOPTED` / `TURN_STARTED` /
  `BUILDING_ADDED` / `BUILDING_ACTIVATED` / `UNIT_CREATED` / `UNIT_ENTERED_CITY` / `CAPITAL_CHANGED`; the
  remaining increments are in [grants-machine.md](triggers.md)).
  ⛔ It registers **LAST**, after the contexts / enabler / modifier: it READS the contexts (every entry condition
  evaluates through `fillEvalCtx`) and the enabler's operating set (a dormant building grants nothing), and unlike
  those machines it APPLIES -- so a stale read hands out a wrong GRANT, not merely a wrong number.
  Beside it: the **enabler's own** consumer
  (`Enabler/CvEnablerConsumer`, load-active), and the **modifier's own** consumer
  (`Cascade/CvModifierConsumer`, load-active for cache building): DOMAIN events in, the moved source's compiled
  deposits APPLIED into the slots they feed -- the maintained sum's one write path
  ([state-repositories.md](../architecture/state-repositories.md)).
  The **tally** is NOT a consumer -- it reads the object-owned counts (`Tally/CvTally.{h,cpp}`).
  ⛔ **One consumer per system** -- the shared consumer that routed BOTH machines is dead
  ([superseded-ideas](../architecture/superseded-ideas.md) #16); never re-merge them.

## The DOMAIN emit surface + the load RESEED

**The spine is the SINGLE place a state change is announced.** Every game state change emits ONE source-carrying
DOMAIN event through a clean endpoint (`emitBuildingChanged`, `emitTechChanged`, `emitImprovementChanged`,
`emitCityOwnerChanged`, …); the event names WHAT (`iType`), WHO (`iC`, owner/triggering player), and WHERE
(`iSrcLoc` = cityId | plotId | -1). `emit()` dispatches **synchronously** — it is not an async listener bus; it calls
each interested consumer's `onEvent` inline at the mutation site. So nothing else in the engine detects changes: the
hand-wired per-site invalidation is retired in favour of this one surface.

**TURN BOUNDARIES are spine events, not a side-channel.** `SEVT_TURN_STARTED` / `SEVT_TURN_ENDED` (DOMAIN — the
turn counter advancing is a genuine synced state change) carry `iType` = the game turn and `iC` = the player, with
**`-1` marking the GAME-scope boundary**. The game pair straddles the counter advance in `CvGame::doTurn`
(`ended` = the closing turn, `started` = the incremented one); the player pair rides `CvPlayer::setTurnActive`.
They **replaced** the bespoke `CvHttpServer::publishEvent("turnStart"/"turnEnd"/"playerTurnStart"/"playerTurnEnd")`
publishes — a happening lives on the spine ONCE and the file + `/events` consumers carry it for free, rather than
each surface growing its own emitter ([observability.md](../reference/observability.md): the server SERVES, it does
not accumulate). The player pair emits for **every** player, not just humans: a turn going active/inactive is a
state mutation, and the spine's contract is that every mutation emits while CONSUMERS filter (a consumer wanting
humans only tests the player field) — a deliberately partial emit surface is what defeats the missed-emit tripwire.

**⛔ ADD ALL THE EVENTS, EVER — the ONLY bar is DUPLICATES (owner).** *"As long as it's not duplicate events, go
nuts, add all the events, ever."* The emit surface is meant to be EXHAUSTIVE: every state mutation in the engine
announces itself, and completeness is the goal rather than a budget to spend carefully. This is not enthusiasm —
it is the roadmap's stated ordering (*"the EMIT surface comes first; the cache build is the step AFTER — caches
cannot build from events until the events are completely emitted"*), so an incomplete emit surface is a
foundation defect, not a backlog item.

⛔ **The ONE thing to avoid is a DUPLICATE — the same fact announced twice.** One fact, one emit, at the genuine
mutation choke point. Two emits for one state change double it for every counting consumer and make the stream
lie about what happened; and if two call sites both look like the choke point, the real fix is finding the one
that is (or emitting from the single place they both pass through), never picking one and hoping. Distinct facts
that happen to fire together are NOT duplicates — emit both.

## ⛔ A FACT NAMES THE HAPPENING — "something changed" IS NOT A FACT (owner)

> *"`BUILDING_CHANGED` is not a valid event — it says that 'something happened', not what actually happened. Any
> event that is not specific relies on actual calculation to happen."*

**THE TEST, and it is about the FACT, never about what any consumer currently does with it: does the event name
WHAT HAPPENED, or only that some state moved?** A fact that names only the movement has handed the consumer a
question instead of an answer, and the only way to answer a question is to CALCULATE — so the calculation the
spine exists to delete reappears inside every consumer at once.

⇒ **It is [DEC-flag-is-fossil](../architecture/decisions.md#dec-flag-is-fossil) wearing the emit
side's costume.** A staleness flag says *"something in this bucket moved"*; a `*_CHANGED` event with a direction bit
says *"something about this entity moved"*. Both discard the identity of the happening, and both leave the
consumer to reconstruct it. **A non-specific event is a staleness flag that learned to travel.**

**⛔ SO WHERE SEVERAL DISTINCT HAPPENINGS REACH ONE CHOKE POINT, THEY ARE SEVERAL FACTS — never one fact with a
discriminator field.** A payload int that a consumer must branch on to find out what occurred is the tell: the
branch is the calculation, merely relocated from the consumer's body into its `switch`.

⚑ **The tree already contains the correct shape, so this is a convergence and not a new design.** The UNIT plane
is named happenings throughout — `UNIT_CREATED` / `UNIT_KILLED` / `UNIT_ENTERED_CITY` / `UNIT_LEFT_CITY` /
`UNIT_DEATH_SCHEDULED`. Nobody wrote a `UNIT_CHANGED` carrying `±1`, and the reason is visible in what those
facts buy: a consumer acts on each one directly, and none of them has ever needed a companion event to say what
it meant.

⚑ **And the argument is already recorded in this spec's own history, against exactly this failure.**
`SEVT_CITY_FOUNDED` exists because founding *"produced NO identifiable fact before this, only a constellation of
side-effects (`populationChanged`, `plotOwnerChanged`, `cityNetworkChanged`), which is why the settle-time
provisions had no trigger to hang on."* A constellation of `*_CHANGED` movements could not substitute for the
named happening — that is this rule, discovered once and then not generalized. Generalize it.

⛔ **Splitting one `*_CHANGED` into its happenings is NOT the banned DUPLICATE.** A duplicate is ONE happening
announced twice; this is SEVERAL happenings that were being announced as one. The rule above already settles it:
*distinct facts that happen to fire together are not duplicates.* The choke point stays single — it simply emits
the fact that names what it just did.

> **⛔ `*_CHANGED` IS NOT A VALID EVENT NAME. FULL STOP (owner).** *"CHANGED is literally not a valid event
> name — it has to say explicitly what happens."* There is no category of fact that is exempt, and the
> exemptions this section used to list were wrong:
>
> | was | is |
> |---|---|
> | `PLOT_TERRAIN_CHANGED` | `PLOT_TERRAIN_ADDED` · `PLOT_TERRAIN_REMOVED` |
> | `PLOT_FEATURE_CHANGED` | `PLOT_FEATURE_ADDED` · `PLOT_FEATURE_REMOVED` |
> | `PLOT_BONUS_CHANGED` (`iB` = ±1) | `PLOT_BONUS_ADDED` · `PLOT_BONUS_REMOVED` |
> | `BONUS_ADDED`/`_REMOVED` (city, unqualified) | `CITY_BONUS_ADDED` · `CITY_BONUS_REMOVED` |
> | …and every other `*_CHANGED` | the pair of happenings it was standing in for |
>
> **⚖ THE EVENT IS THE OPERATOR; THE PAYLOAD IS ONLY EVER A MAGNITUDE (owner).** *"Events can hold a count, but
> it is literally just a count — it's the event that shapes the subtraction/addition, not the count."* So a
> fact may absolutely carry HOW MANY: `CITY_SPECIALIST_ADDED` with a count of 3 adds three times over, and
> `CITY_SPECIALIST_REMOVED` with a count of 3 withdraws three times over. What the payload must never carry is
> WHICH WAY — the count is unsigned in meaning, and the event name supplies the sign.
> ⛔ So a `±1` in `iB`, a presence boolean in `iA`, or an old value beside a new one are all the same defect:
> a DISCRIMINATOR the consumer must branch on, which is the calculation relocated into a `switch`. A consumer
> learns what happened **by which event it received**, and reads the payload only for how much.
>
> ⚑ **SCOPE-QUALIFY THE NAME** — `PLOT_BONUS_ADDED` beside `CITY_BONUS_ADDED`. A resource appearing on a TILE
> and a city GAINING that resource are different happenings with different consumers, so the scope belongs in
> the name rather than in a reader's head.
>
> ⚑ **What this buys, concretely: every consumer's direction-decoding disappears.** A consumer that decodes
> three conventions today — an id pairing, a boolean in `iA`, a signed delta in `iB` — decodes none. And a
> WITHDRAWAL becomes announceable at the moment the old state still holds, which is what makes the maintained
> sum exact ([state-repositories.md](../architecture/state-repositories.md) § THE INVARIANT) rather than
> dependent on a consumer reconstructing what used to be true.
>
> **⚖ A SCALAR IS NO EXEMPTION, AND THE WORKED CASE IS POPULATION (owner):**
> *"When a city grows a pop it is `CITY_POPULATION_ADDED 1`. If a city loses 2 pop, it is
> `CITY_POPULATION_REMOVED 2`."*
> ⚑ **Note what the payload is: the DELTA as a magnitude, not the new total.** `CITY_POPULATION_ADDED 1`, never
> `POPULATION_SET 7` — a consumer maintaining a sum needs how much MOVED, and a new total would force it to
> subtract against a remembered previous value, which is the derivation this whole rule removes. The one
> consumer that wants the total reads the object, which owns it.
> ⛔ So there is no "a scalar carrying its new value is already specific" carve-out: that was this document's own
> wording and it was wrong. Every fact is `<SCOPE>_<THING>_ADDED` / `_REMOVED` with an unsigned magnitude.
>
> ⛔ **Splitting is NOT the banned duplicate** (above): a duplicate is ONE happening announced twice, this is
> SEVERAL announced as one.

**⛔ TOO MANY EVENTS IS BETTER THAN NOT ENOUGH (owner) — and if an emit is found not to exist, ADD IT.** When
weighing whether some mutation "deserves" an event, the answer is EMIT. The costs are wildly asymmetric: a
MISSING emit is a silently wrong value that no compiler and no runtime catches, found only by someone noticing a
number is off; a SURPLUS emit costs one consumer branch that declines to act. Never agonize over the judgement —
if it moves state, it emits.

**⛔ AN EVENT GAP IS CLOSED THE MOMENT IT IS FOUND — NEVER RECORDED AND LEFT (owner).** Finding one is not a
discovery to write down; it *is* the work item, and it is done now. This is stronger than the ruling above, and
it binds the same way whichever form the hole takes: a **missing emit** (nothing announces the fact), a
**missing FIELD** on an existing fact (the old-value case above — it fires but cannot be acted on), or a
**missing CONSUMER ROUTE** (the fact is on the wire and the store that needs it ignores it). All three leave a
stored value permanently wrong and none self-heals
([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)) — so a todo entry reading *"`SEVT_X` is the
hook"* is a value that stays wrong for exactly as long as that entry sits there.
⚑ **Why it is a hard rule and not a priority call:** closing one costs almost nothing while the trace is in
front of you, and it never gets cheaper — the next agent must re-derive which fact was missing, which consumer
wanted it, and why. Deferring converts a few minutes of wiring into a re-investigation.
⚠ It does NOT license guessing a structure: if the route needs a design decision the specs do not answer,
surface that — but the gap still closes in the same work item.

> **⚖ THE COUNTER-CASE, so the rule is not misapplied: a deliberately-drawn SCOPE BOUNDARY is not a gap.** The
> worked instance is the city's **obtained-bonus** pair (`SEVT_CITY_BONUS_ADDED` / `_REMOVED`), which announces the **PRESENCE
> CROSSING ONLY** — 0 ⇄ non-zero — because `processNumBonusChange` reaches `processBonus` solely when the
> has-verdict crosses zero. A count moving 2 → 3 therefore announces nothing, and **that is the ruling, not an
> oversight (owner):** a per-count fact would force the engine to start *"processing all sorts of extra edge
> cases about what city added the bonus"* — attribution the crossing sidesteps entirely.
> ⚠ What is knowingly outside the boundary: a count-THRESHOLD reader (a `min: 3` requires-atom, a `per`
> count-scaler) does not re-evaluate on a move between non-zero counts. Accepted for now.
> ⚑ **The REVISIT TRIGGER is named, and it is VOLUMETRIC (owner)** — when a resource stops being present/absent
> and becomes a QUANTITY a city draws against, the crossing stops being sufficient and this reopens. ⚠ But it
> reopens **as part of that work, never ahead of it**: *"then we also have to implement a ton of other things"*.
> Volumetric is a model-wide change (the same direction the amenity id→COUNT dictionary is already shaped for,
> [json.md §8](json.md)), so a per-count fact added early buys nothing and pays the attribution cost now.
> ⛔ Do not treat the named trigger as a licence to start: it marks WHEN, not a standing invitation.
> ⛔ So do **not** add a per-count bonus fact, and do not read the absence of one as a hole to close — it would
> also be a near-duplicate of the crossing fact on every 0 ⇄ 1 transition. **The test that separates the two: a
> gap is a fact nobody DECIDED to leave out.** Ask whether the omission is recorded as a decision; if it is,
> the rule above does not apply to it.

⚠ **The ruling is about EMITS, and it does NOT extend to what a consumer DOES with one.** A surplus emit is
~free; work a consumer performs is paid on the turn path at event volume. So: **emit liberally, apply
precisely** — a consumer acts on exactly the deposits the fact names ([state-repositories.md](../architecture/state-repositories.md)
§ THE MAINTAINED SUM), never on a widened mask and never on a whole-scope sweep it could not derive
([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)).
Turn DURATION analytics remain the `[PERF]` phase logs' job, not these facts'.

## ⚖ `CvEventReporter` EMITS SPINE FACTS BESIDE ITS PYTHON CALLS (owner)

> *"I don't want to convert the current `CvEventReporter` yet, I simply want it to emit spineevents — so that
> things can be actually migrated down the line."*

**A happening that reaches only Python is invisible to the spine, to `/events`, to the file consumers and to
every C++ consumer.** `CvEventReporter` (`Sources/UI/CvEventReporter.{h,cpp}`, 85 `void` report methods) is the engine→Python
callback hub, and for a large part of its surface it is the ONLY announcement a happening makes. So each method
gains a spine emit ALONGSIDE its existing Python call.

⛔ **This is an ADDITION, never a conversion.** The reporter keeps calling Python exactly as it does; nothing is
rerouted, removed or re-bodied. Converting `CvEventReporter` onto the triggers machine is the LATER work item
([patterns.md](../architecture/patterns.md): its successor is the triggers machine and *"events move INTO C++,
but that is not 430"*), and starting it here — one handler at a time — is the event rework beginning by accident.

⚑ **WHY THE REPORTER IS THE RIGHT EMIT SITE**, even though it is a reporting hub rather than a mutation choke
point: every method is CALLED at the happening with the parties already in hand
(`combatResult(pWinner, pLoser)`, `unitCaptured(eFromPlayer, eUnitType, pNewUnit)`). So the spine fact carries
**exactly what Python receives**, which is the property that makes the later migration a SWAP rather than a
re-investigation. An emit placed anywhere else would have to rediscover those arguments.

**⛔ THE FACT IS RAW, NOT FORMALIZED — and this is the part to get right (owner): *"we have no info, or way to
define a lot of these events in C++ yet; that will happen when we move the actual eventsystem over, when we can
clearly formalize all these python things in a structured json."*** The emit announces the happening with the
reporter's OWN arguments and invents nothing: no designed payload, no modelled semantics, no
`on<Happening>` token, no action verb. ⚑ That is also why this does not breach
[triggers.md](triggers.md)'s ban on minting a happening or a verb speculatively — the AUTHORING vocabulary stays
unminted; only the engine's announcement lands. Anything designed now is undone by the formalization later.

**⛔ THE ONE BAR IS THE STANDING ONE — DUPLICATES.** A reporter method whose happening ALREADY has a spine fact
gets no second emit: `unitPromoted` (`SEVT_UNIT_PROMOTION_ADDED`), `techAcquired`, `buildingBuilt`
(`SEVT_CITY_BUILDING_ADDED`), `cityBuilt` (`SEVT_CITY_FOUNDED`), `religionFounded`. ⚑ **The worklist is therefore
a SUBTRACTION, not a judgement call: the reporter's method list minus the facts that already exist** — and what
falls out is the combat and arrival cluster that has never been on the spine at all (`combatResult`,
`combatRetreat`, `combatWithdrawal`, `unitCaptured`, `unitPillage`, `goodyReceived`, `cityRazed`,
`nukeExplosion`, `greatPersonBorn`).
⚠ **`unitKilled` is the case that must be DECIDED rather than skipped:** the reporter fires it from
`scheduleDeath` while `SEVT_UNIT_KILLED` fires from `die()`, and those are different moments — a scheduled death
can still resolve into survival ([unit-lifecycle.md](../reference/unit-lifecycle.md)). Near-duplicate, not
duplicate; resolve it on the facts rather than assuming either way.

**⚖ THE KIND IS PER-METHOD, by the test this spec already states** (§ THE RECEIVED LINE): *does the fact say what
the STATE is, or what some CODE did?* `combatResult` / `unitCaptured` are state changes ⇒ **DOMAIN**;
`combatLogCollateral` / `combatLogFlanking` are log entries ⇒ **DIAGNOSTIC**. ⛔ Erring toward DIAGNOSTIC defeats
the purpose: no consumer may build state from one, so a DIAGNOSTIC fact cannot serve as the migration seam and
would need converting in a second pass.

⚑ **AND IT ANSWERS A TRACING GAP THAT IS NOT HYPOTHETICAL (owner): *"it's hard to trace where things come from,
captives being the best example."*** A captured unit today announces `SEVT_UNIT_CREATED` and nothing else, so it
is indistinguishable from a trained, granted or WorldBuilder-placed one — and no provenance tag on
`UNIT_CREATED` could express a capture's SECOND PARTY. `unitCaptured` carrying captor and victim is what makes
the origin readable.

> **⚖ A SOURCE on `SEVT_UNIT_CREATED` IS A RENDER FIELD, AND ONLY A RENDER FIELD (owner).** Where the creating
> happening is worth reading off the existence fact, it rides the RENDER payload (the `SEVT_NAME_CHANGE`
> precedent — a field the machine consumers do not read), so the log answers *where did this come from* at a
> glance while `UNIT_CREATED` stays the one fact every "does this unit exist" consumer rides.
> ⛔ **The moment anything ROUTES on that field it is no longer diagnostic and must become its own fact**
> ([DEC-facts-name-happenings](../architecture/decisions.md#dec-facts-name-happenings): a payload a consumer
> branches on is the calculation relocated into a `switch`).
> ⚑ Two facts at one birth is not a duplicate and the tree already does it deliberately —
> `SEVT_WORLD_UNIT_CREATED_COUNT_ADDED`, `SEVT_EMPIRE_UNIT_COUNT_ADDED` and `SEVT_UNIT_CREATED` all fire at one
> birth and none duplicates another.

## ⚖ THE RECEIVED LINE — auditing the whole event flow live (owner)

> *"We can literally audit the entire eventflow with the `/events` endpoint live — all we have to do is have a
> 'received' event on the other side that is purely for logging."*

**A consumer announces that it ACTED on a fact.** Emitted lines and received lines then stream side by side on
`/events`, and the audit is a diff: **a DOMAIN fact with no matching received line names a MISSING CONSUMER
ROUTE** — the third gap form in [DEC-close-event-gaps-now](../architecture/decisions.md#dec-close-event-gaps-now),
and the only one with no other observable signature. The first two forms are visible today (a missing emit leaves
the stream silent, a missing field leaves the payload short); this one is not, because the fact goes out
perfectly and is simply dropped on the floor. The worked case is `SEVT_PROPERTY_ADDED / _REMOVED`, which fires from the
`CvProperties` choke points into a consumer set that carries no case for it — a defect that took a code audit to
find and that a missing received row would have named at a glance.

⚑ **It is load-bearing under the maintained-sum model specifically:** the consumer route IS the maintenance
([state-repositories.md](../architecture/state-repositories.md)), so this audits the correctness mechanism
itself rather than the number that falls out of it.

> **⚖ A "JOB DONE" ANNOUNCEMENT IS A RECEIVED LINE, AND IT IS ALWAYS `DIAGNOSTIC` (owner).** *"`SEVT_CITY_BUILDING_PROCESSED`
> is a 'I have completed my job' event, if anything, and should purely be logging."*
> ⛔ **THE TEST: does the fact say WHAT THE STATE IS, or WHAT SOME CODE DID?** A completion notice is the second,
> so it is `DIAGNOSTIC` and **NO CONSUMER MAY BUILD STATE FROM IT** — deriving held state from an announcement
> that an apply ran is the failure this kind exists to make unsayable.
> ⚑ **The STATE a completion notice sits next to is a separate DOMAIN fact and gets its own id.** A building's
> operate crossing (`ACTIVATED` / `DORMANTED`) is what the deposit, amenity and free-promotion consumers read;
> the processed notice announces only that the apply ran, and nothing folds on it. ⚠ Letting one id carry both
> means neither consumer can tell which arrived — a completion notice and a state change are not two readings of
> one event, they are two events.
> ⛔ The repair for such a conflation is always ADDITIVE, never a deletion
> ([DEC-close-event-gaps-now](../architecture/decisions.md#dec-close-event-gaps-now)): mint the state fact, leave
> the notice a notice, re-point the folds.
> ⛔ **Do NOT suppress an EMIT to fix a CONSUMER.** Conflating "this fact fired" with "this consumer should act"
> is what produced both the plot-mark fan and this shared id. **Emit every distinct fact, always; decide handling
> per consumer, separately.**

⛔ **THE KIND IS `DIAGNOSTIC`, NEVER `DOMAIN` — and this is what keeps it from becoming the killed verifier.**
The firewall above already defines `DIAGNOSTIC` as *"code ran (a function entered, a decision re-evaluated) …
logging only — never counted, never gates"*, which is exactly what a received line is, so it needs no new
machinery.
⚠ The near-miss to recognise is [superseded-ideas #19](../architecture/superseded-ideas.md), the gated in-DLL
cache verifier, which was killed for putting a **divergence** on the spine: an event is an invitation to a
consumer, and the next agent's consumer "handles" a value known to be wrong by CORRECTING it — so the shape
itself licensed self-heal. **A received line announces THAT CODE RAN, never a VERDICT ABOUT A VALUE**, so it
contains nothing to correct. Emitting it as DOMAIN would make it a synced authoritative fact the machine
consumers may read, which is the shape that grows the self-healer, and would double the unconditional stream.
⚑ As DIAGNOSTIC it rides `gStreamLogLevel` — decoupled from the file gate — so it costs nothing until it is
turned on, and stays off the bounded SSE slot budget during ordinary play
([http-endpoints.md](http-endpoints.md)).
⛔ It reports NO judgement and accumulates NO counter behind a route: it is a line, on the one surface that
already exists ([http-endpoints.md](http-endpoints.md): the server SERVES, it does not ACCUMULATE — a fact that
is on neither surface is EMITTED, never given a side-counter).

**Events are FACTS, not causal steps.** "This building is here", "this tech is held" — order-independent,
prerequisite-free. Prerequisites are evaluated ONLY by the enabler (`canConstruct`/`canTrain`/`canResearch` — the
"*can* I?" question), never by a has-been-done fact; so the emit stream carries no ordering and no prereq logic.
Corollary — **yield is a computed RESULT, never an event**: emit the CAUSES (improvement/terrain/feature/route
changed), and a consumer computes the yield downstream.

**The load RESEED — the save read goes through the INTERNAL SETTERS.** A loaded save used to deserialize straight
into the `CvCity`/`CvPlot` members, so the setters never fired and the **cascade** (its value packages AND its
enabler side) had nothing to build from. The reseed is fixed **at the read itself**: each slot deserializes into a
LOCAL and is handed to that slot's **internal setter** — the ONE body that commits the member, maintains whatever
derived state the object owns, and announces the fact.

> **⛔ THE CRUD IS NOT THE EVENT; WHAT HAPPENED IS (owner).** The event does NOT set the state, and the earlier
> north-star that said so — *read → emit → populate* — was **backward**. It made an EFFECT the thing that mutates
> base state, which violates the principle the whole model rests on, and it is precisely what the old `*_CHANGED`
> payload existed to serve (an old value beside a new one, so a consumer could drive the mutation). **The stream
> is authoritative for base state; the fact is TESTIMONY about a completed act, in the past tense.**
>
> ⇒ **An INTERNAL setter is commit + maintain + announce, and NOTHING else.** The public setter is its guard,
> then the internal one, then its EFFECTS (plot groups, areas, sight, graphics, cascading type changes). The read
> calls the internal one directly — which is exactly what lets it: no effect gets to decide any part of the state
> the save is authoritative for.
> ⚑ **And the drift it closes was real, not theoretical.** While the read wrote members raw, the emit and the
> derived-state maintenance lived in a SECOND place that had to be kept in step BY HAND — and was not: five
> `CvPlot` slots deserialized with no fact at all, and the movement hash needed a whole rebuild pass at the end of
> the read to paper over the maintenance the setters would have done. The pass is gone with the bypass that
> caused it. ⛔ Such a rebuild can NEVER stand beside the setters: both would apply, and for an XOR-maintained
> value that cancels it to zero — silently, on every object.
> ⚠ **The save TAG does not move when the destination does.** `NormalizeName` strips the address-of and any cast
> (*"m_thingy on save should match `(int*)&m_thingy` on load"*), so a `WRAPPER_READ_DECORATED` naming the tag
> explicitly reads the byte-identical tag the plain form produced. Keep each local the MEMBER'S OWN TYPE — the
> wrapper picks its read overload from the destination, and a wider local asks the stream for a type code the
> writer never wrote ([save.md](save.md)).

Tech is team-held but emitted per-self from each member's `CvPlayer::read`, one emit per alive member; projects
the same. Beside the DOMAIN facts the setters emit, the read may announce what the STREAM CONTAINED as
`SAVELOAD` lines — a log of loading, consumed by logging alone.

⛔ What the reseed is **NOT**: a separate pass that walks already-deserialized objects and **fabricates** events from
their populated state (a "for each building present, emit built"). That pseudo-emit feeds the cascade reconstructed
lies and trains the next agent to reconstruct more — it is banned
([superseded-ideas](../architecture/superseded-ideas.md)). There is no clean middle between it and the real
event-sourced read, so the read-driven reseed is built as its own step, never shimmed.

> **⚖ AI RE-EVALUATION IS A RESULT-PRODUCER TOO — IT RUNS ONCE THE GAME HAS LOADED, NEVER DURING THE SAVE READ
> (owner).** *"The AI needs to be allowed to do work; the important part is to not have the AI do work during
> saveload."* A citizen assignment, a production choice, a re-scored plan are DECISIONS taken over base state —
> and while the stream is still arriving that state is incomplete, so the decision is paid for in full and then
> invalidated by the next fact. The save already carries the answer the AI would re-derive, so a load owes no
> re-decision at all.
> ⚑ **The flag STAYS SET; only the WORK is suppressed.** Whoever marked the city still wants the work, so it is
> DEFERRED to the first sweep past `GAME_LOAD_FINISHED` — the identical suppress-then-resume shape grants take
> off this bracket, applied to the AI plane.
> ⛔ **Guard the WORK, not the mark.** A guard on each marking site is a rule every future caller must remember,
> and the marks arrive from whole-scope fans as well as direct calls; a guard at the one place the work is
> performed covers every path by construction. ⚠ It is NOT a licence to suppress the FACTS — the reseed's
> DOMAIN events are what build the state, and they are exactly what must keep flowing.
> ⚑ **The measured shape this closes:** the load fanned a full-empire citizen-reassignment mark once per
> empire-scope yield-modifier deposit, concentrated in the load's closing seconds — thousands of marks over
> state that no citizen had yet been placed against.

**The load lifecycle is bracketed by two spine events — `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED`.** Result-producers
(grants, and any future on-event side-effect machinery) rely **purely on the spine**: they see `LOAD_STARTED` →
suppress, `LOAD_FINISHED` → resume, so nothing is granted during reconstruction (a grant is a RESULT of a genuine
in-play acquisition, and a load is not an acquisition). The **cache-build consumer** is the load-active one — it
consumes the in-read events to build the cascade. New game builds the same way: its real init fires the same events,
with grants active because those are genuine acquisitions. Ledgered as
[DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed).

> **⛔ `spineGameLoadInProgress()` IS RESULT-PRODUCER SUPPRESSION, AND AGENTS KEEP MISCONSTRUING IT (owner —
> repeatedly, across sessions).** It answers ONE question: *would acting on this fact HAND SOMETHING OUT for a
> load, which is not an acquisition?* That is why the trigger/grant machinery consults it. ⛔ It is **NOT** a
> licence for a LOAD-ACTIVE consumer to skip work the reseed exists to perform — reaching for it there asserts
> that the load cannot be trusted to build the state, which is precisely the claim
> [DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed) exists to falsify.
> ⚑ **THE MECHANISM OF THE MISTAKE IS COPYING, not reasoning** — the guard is read off an adjacent case in the
> same `switch` and carried into a new one, so it spreads without anyone deciding it. ⇒ **Never inherit one from
> a neighbouring case. Ask what YOUR handler does**, and answer the test below.
>
> **THE TEST — what does acting on this fact PRODUCE?**
>
> | the handler | verdict |
> |---|---|
> | hands an entity / payload over, or takes an AI DECISION | **guard it** — a load is not an acquisition, and the AI re-decides against state the save already carries |
> | BUILDS derived state (a context store, an enabler domain, a package) | ⛔ **no guard** — this is the reseed's whole job |
> | needs an object the stream has not delivered yet | ⛔ **not a guard — a BUFFER with a load-end DRAIN.** The two are not the same shape: a guard DROPS the fact, a buffer KEEPS it. Dropping a fact you needed is a permanent hole ([DEC-no-self-heal](../architecture/decisions.md#dec-no-self-heal)) |
>
> ⚠ **The over-correction is equally wrong, so three legitimate non-result-producer uses are named here rather
> than rediscovered:** the ORDERING BUFFER above (the city membership fold, the modifier's `plots` fan); the
> TWO-LEG FOLD whose play-time fan would otherwise double-count against the load build
> ([contexts.md](../architecture/contexts.md)); and reading the bracket as the **new-game-vs-load discriminator**,
> which suppresses nothing at all. ⛔ Do not sweep those out in the name of this rule.
> ⛔ **And a guard must never suppress an EMIT** — that is the separate, absolute ban (§ THE RECEIVED LINE: emit
> every distinct fact, decide handling per consumer). A consumer that would double-apply on an in-read fact is a
> CONSUMER defect; silencing the fact hides it from every other consumer too.
> ⚑ **When a handler genuinely needs neither, say so where the next reader will look.** A comment stating why a
> guard is ABSENT is what stops the copy: the enabler's consumer already carries one, and the guards that grew
> underneath it are what that line was meant to prevent.

## See also
- [logging.md](logging.md) — the broad consumer (what to log). [tally.md](tally.md) — the read-only count accessor
  (reads object-owned counts; NOT a spine consumer). [../architecture/patterns.md](../architecture/patterns.md) — the `IEventConsumer` interface pattern.
