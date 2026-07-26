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
| **`DIAGNOSTIC`** | **code** ran (a function entered, a decision re-evaluated) | no — execution trace | **logging only** — never counted, never gates |
| **`TRACE`** | fine-grained "every step" | no | logging only |

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
  i; float f; char* s; wchar_t* w;}}`, 8B/POD) that the one logging path formats. A **`DOMAIN`** event carries BOTH —
  its state ints for the machine consumers **and** a domain tag + fields so it renders through the same registered
  path as everything else; a **`DIAGNOSTIC`/`TRACE`** event carries only the render payload. There is no
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
  DIAGNOSTIC/TRACE lines stream at the stream's own verbosity knob (`gStreamLogLevel` /
  `Autolog__LogLevelStream`), fully decoupled from `gPlayerLogLevel` — streaming everything never requires
  opening the level-4 file firehose. The SSE queue is capped; on overflow the first frame that fits again
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
  The **DOMAIN emit surface = BUILT**: **95 call sites** across `CvPlayer` (31) · `CvCity` (23) · `CvPlot` (20) ·
  `CvGame` (7) · `CvTeam` (4) · `CvUnit` (3) · `CvPlotGroup` (1) + the cascade's own diagnostics (6), restored
  at the genuine mutation choke points after the clean-slate revert had stripped them. The PLOT substrate is
  complete: terrain / feature / improvement / route / bonus / owner / **type / river / irrigation / landmark /
  worked**, so the per-scope contexts are maintained purely by facts, with no choke point driving a derivation
  directly ([contexts.md](../architecture/contexts.md)). The **load-lifecycle
  bracket** `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED` is emitted (`CvGame::read` / end of
  `CvGame::onFinalInitialized`), so `spineGameLoadInProgress()` reports correctly and result-producers suppress
  inside it. The **load reseed** = BUILT: the in-read emits fire from inside the save read (`CvPlayer::read` per
  held tech / project / civic / trait / heritage + era / golden-age / state-religion / nukes, `CvCity::read` per
  building / religion + holy-city / corp / specialist / population / power / culture-level, `CvPlot::read`
  substrate + owner + working-city), wrapped by the bracket. ⚠ A plot fact whose field deserializes with no emit of
  its own (type / river / irrigation / landmark / worked) needs **none**: `CvPlot::read`'s terrain emit is
  UNCONDITIONAL, and a substrate fact re-derives the plot's WHOLE verdict block — so every plot is already covered
  exactly once, and adding a second in-read emit would only re-derive the same block.
  ⚠ **One endpoint is deliberately unwired: `emitLoadPipeline`** — every one of its arguments is produced by the
  archived load-time warm-up/rebuild pass, which the CAPSTONE rule removed
  ([state-repositories.md](../architecture/state-repositories.md)); the event reseed replaced that pass, so the
  endpoint has no honest caller. Open follow-ups (the tile-driven vicinity backstop; the per-city enabler
  priming that preceded the reseed emits): [info-rebuild.md](../plans/structural-cleanup/info-rebuild.md) ledger.
  **Registered consumers today:** the broad FILE logging consumer, the `/events` STREAM consumer, the **grants
  engine** (`Grants/CvGrantsEngine` -- resolver AND appliers built: `gr_applyTechFirstDiscover` /
  `gr_applyBuildingFirstBuild` / `gr_applyPerTurn` / `gr_applyCityPerTurn` / `gr_applySpawn` / `gr_applyFullHeal`,
  dispatched from `SEVT_TECH_ACQUIRED` / `RELIGION_FOUNDED` / `PLAYER_INIT` / `CITY_FOUNDED` / `CIVIC_ADOPTED` /
  `TURN_STARTED` / `BUILDING_PROCESSED` / `UNIT_CREATED` / `CAPITAL_CHANGED`; the remaining increments are in
  [grants-machine.md](../plans/structural-cleanup/grants-machine.md)), the **enabler's own** consumer
  (`Enabler/CvEnablerConsumer`, load-active), and the **modifier's own** consumer
  (`Cascade/CvModifierConsumer`, load-active for cache building): DOMAIN events in, index-derived dirty marks
  out (`DepositIndex::routeFor` + the condition-dependency routes --
  [state-repositories.md](../architecture/state-repositories.md)).
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
Turn DURATION analytics remain the `[PERF]` phase logs' job, not these facts'.

**Events are FACTS, not causal steps.** "This building is here", "this tech is held" — order-independent,
prerequisite-free. Prerequisites are evaluated ONLY by the enabler (`canConstruct`/`canTrain`/`canResearch` — the
"*can* I?" question), never by a has-been-done fact; so the emit stream carries no ordering and no prereq logic.
Corollary — **yield is a computed RESULT, never an event**: emit the CAUSES (improvement/terrain/feature/route
changed), and a consumer computes the yield downstream.

**The load RESEED — event-source the save READ (BUILT, live).** A loaded save deserializes state directly into the
`CvCity`/`CvPlot` objects — the incremental setters never fire, so the **cascade** (its value packages AND its enabler
side) would have nothing to build from. The reseed fixes this **from inside the save read itself**: reading a fact off
the stream is what fires its DOMAIN event (`CvGame::read` → `CvPlayer::read` → `CvCity::read` / `CvPlot::read`; tech
is team-held but emitted per-self from each member's `CvPlayer::read`, one emit per alive member; projects the same).
The north-star is that the event itself SETS the state — read → emit → populate, one mechanism for the
game object AND the cascade; object-populated-by-events is out of the current scope, and the events come from
the genuine read.

⛔ What the reseed is **NOT**: a separate pass that walks already-deserialized objects and **fabricates** events from
their populated state (a "for each building present, emit built"). That pseudo-emit feeds the cascade reconstructed
lies and trains the next agent to reconstruct more — it is banned
([superseded-ideas](../architecture/superseded-ideas.md)). There is no clean middle between it and the real
event-sourced read, so the read-driven reseed is built as its own step, never shimmed.

**The load lifecycle is bracketed by two spine events — `GAME_LOAD_STARTED` / `GAME_LOAD_FINISHED`.** Result-producers
(grants, and any future on-event side-effect machinery) rely **purely on the spine**: they see `LOAD_STARTED` →
suppress, `LOAD_FINISHED` → resume, so nothing is granted during reconstruction (a grant is a RESULT of a genuine
in-play acquisition, and a load is not an acquisition). The **cache-build consumer** is the load-active one — it
consumes the in-read events to build the cascade. New game builds the same way: its real init fires the same events,
with grants active because those are genuine acquisitions. Ledgered as
[DEC-spine-reseed](../architecture/decisions.md#dec-spine-reseed).

## See also
- [logging.md](logging.md) — the broad consumer (what to log). [tally.md](tally.md) — the read-only count accessor
  (reads object-owned counts; NOT a spine consumer). [../architecture/patterns.md](../architecture/patterns.md) — the `IEventConsumer` interface pattern.
