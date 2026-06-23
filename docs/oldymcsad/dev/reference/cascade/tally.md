# The tally — the additive COUNT machine ("how many?")

> **Status:** reference (the count machine — design landed; implementation PARTIAL) · **Verified against:**
> `Sources/Cascade/CvCascadeTally.{h,cpp}`, `Sources/Cascade/CvScopedAccumulator.h`,
> `Sources/Cascade/CvEventSpine.{h,cpp}` (2026-06-20); design reconciled with old
> `docs/dev/plans/tally-cascade-spec.md`. · **Grounding:** every behavioural claim carries a `file:line`
> to live source. Line numbers **drift** — confirm the named function/symbol, not the integer.
>
> The tally is one of the cascade's three machines. This doc is its DEEP reference: the count mechanics, the
> scope-spine roll-up, the count vocabulary, and — its own to own — the **serialize-nothing / rebuild-on-load**
> model ([DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing)). The
> one-paragraph "what it is, how it fits the other machines" lives in the overview
> ([cascade-architecture §4](../../explanation/cascade-architecture.md)); this is the detail behind it.

**BLUF.** The tally answers **"how many of X do I have?"** — presence (`count ≥ 1`) and thresholds
(`count ≥/≤ N`) of any data Type at any scope. It is the **count sibling of the modifier**: the same additive
scope substrate (`CvScopedAccumulator`), summing a different thing. Where the modifier sums effect *magnitudes*
and flows **DOWN** the spine, the tally sums presence *counts* and rolls **UP** it. It is fed by `DOMAIN` events
off the [event spine](event-spine.md) (the only kind it consumes), keyed `(domain, type-index)` per **player**,
and rolled up to team/world on read. It **serializes nothing** — on load it is *rebuilt* from the authoritative
loaded objects, then maintained incrementally by events. Implemented today for **buildings + units only**.

---

## 1. What the tally IS — the additive count substrate, instantiated to count presence

The tally is **engine machinery**, never authored in the JSON (the JSON carries only the clauses that *read* it
— a `requires` count atom and a modifier `per` scaler; [data-model §2](data-model.md)). It and the modifier are
**one primitive, two instantiations** of the shared substrate `CvScopedAccumulator` — a scope-agnostic keyed
additive sum (`Sources/Cascade/CvScopedAccumulator.h:42`): the modifier sums magnitudes, the tally sums counts.
That is why it is a foundational machine, not a bolt-on.

- **The substrate is `CvScopedAccumulator`** — `deposit(key, delta)` folds a value into a key's running sum;
  `get(key)` reads the sum; a sum returning to 0 drops the key so the map stays sparse
  (`CvScopedAccumulator.cpp`, `deposit`/`get`). It is **authoritative + exact**, deliberately NOT a derived-data
  repository tenant (no `TLazy`/`TDependency`/version/dirty) — the enabler gates buildability off it, so it
  cannot be lazy or stale-tolerant (`CvScopedAccumulator.h:27-37`, the "DELIBERATELY FRESH" comment).
- **One accumulator per domain** — keys are Type indices *within one domain* (buildings, units, …). The Type
  prefix routes which accumulator a count lands in, so domains never collide in one map.

## 2. The scope spine vs the stored leaf — counts originate per-city, the tally stores per-PLAYER

The **scope spine** is the shared hierarchy `world → team → empire → … → city → plot{building|unit|…}`. Counts
**originate** per city (a building is built *in* a city) and **roll up** to empire/team/world. But the tally's
**stored leaf is the PLAYER/empire**, not the city — it keeps **no per-city count table**
([DEC-tally-serializes-nothing] context; the model was decided player-leaf 2026-06-19). The store is literally
`CvScopedAccumulator m_counts[NUM_COUNT_DOMAINS][MAX_PLAYERS]` — one accumulator per `(domain, player)`
(`CvCascadeTally.h:74`).

**Why player-leaf suffices (and stays reversible):** all six current count-threshold consumers (§5) are
**cross-city, zero per-city** — a player-leaf store answers every one of them. A `CITY`/`PLOT`-scope read does
**not** go through the tally at all: it reads the live `CvCity`/`CvPlot` directly via the condition evaluator
(`cascadeAtomCount`), so `count()` returns 0 for those scopes by design (`CvCascadeTally.h:33-44`). Because the
tally serializes nothing (§4) and is interface-bounded, widening to a city leaf later — if a real per-city
consumer ever appears — is a contained, no-save-break, single-module change. Player-leaf now is reversible, not
a one-way door.

**Scope enum** (`CvCascadeTally.h:37`, `CountScope`):

| scope | `iContext` | path |
|---|---|---|
| `COUNTSCOPE_EMPIRE` | player id | tally — `m_counts[domain][player].get(type)` |
| `COUNTSCOPE_TEAM` | team id | tally — sum over players on the team |
| `COUNTSCOPE_WORLD` | (ignored) | tally — sum over all players |
| `COUNTSCOPE_CITY` | the context city | **direct read** (live object), not the tally → `count()` returns 0 |
| `COUNTSCOPE_PLOT` | the context city's plot | **direct read**, not the tally → 0 |

The five scopes share one enum so an atom carries a single scope field across both the tally path and the
direct-read path.

## 3. The count mechanics — read, roll-up, and the cap exception

### 3.1 The count read-surface

`count(CountDomain eDomain, int iType, CountScope eScope, int iContext)` is the surface the JSON gates call
(`CvCascadeTally.cpp:86`). It picks the `(domain, …)` bucket then resolves the scope:

- **EMPIRE** — one `get` on `m_counts[domain][player]` (`CvCascadeTally.cpp:94`).
- **TEAM** — sum `get` over every player whose `getTeam() == iContext` (`CvCascadeTally.cpp:96-107`). This *is*
  the additive roll-up: there is no stored team accumulator; the team total is computed on read from the player
  leaves.
- **WORLD** — sum `get` over all players (`CvCascadeTally.cpp:108-116`).
- **CITY / PLOT** — fall through to `return 0` (the live-object path handles them, §2).

### 3.2 Incremental maintenance — `onEvent`

The tally is the **SELECTIVE** consumer: `wantedKinds()` returns only `(1 << EVENTKIND_DOMAIN)`
(`CvCascadeTally.h:49`) — `DIAGNOSTIC`/`TRACE` never reach it (the OOS firewall, [event-spine §2](event-spine.md)).
`onEvent` maps a `DOMAIN` count event to its domain by `iEventId` (`CASCADE_EVT_BUILDING_COUNT` →
`COUNTDOMAIN_BUILDING`, `CASCADE_EVT_UNIT_COUNT` → `COUNTDOMAIN_UNIT`), then deposits the **delta** (`iB`) into the
owning player's (`iC`) accumulator: `m_counts[eDomain][iC].deposit(iType, iB)` (`CvCascadeTally.cpp:40-55`). A
`changeBuildingCount → deposit(±1)` ride. A non-counted event (e.g. `CASCADE_EVT_NAME_CHANGE`) hits the `switch`
default and returns — the tally ignores it.

### 3.3 The `allowed`-cap exception — lifetime-created, not currently-alive

`countForCap` (`CvCascadeTally.cpp:121`) exists because the `allowed` cap on a **world-unique UNIT** must count
**lifetime-created**, not currently-alive: a hero "born once, does one thing, then poofs" still consumes its
world slot. So `UNIT @ WORLD` reads the engine's historic `getUnitCreatedCount` (the persisted, increment-only,
never-decremented counter — legacy `isUnitMaxedOut` reads it, `CvGame.cpp:5104`); everything else (buildings,
where alive == created; all other scopes) routes to the normal `count()`. The **storage stays in the engine**;
this is the single located seam where the cascade reads it — the tally owns the *job* without duplicating the
*state* (the §4 ownership rule for genuine historical counters, made concrete). Units have **no team cap** (owner
ruling 2026-06-17, folded to empire); a `UNIT @ EMPIRE` cap reads the live count for now — flagged to revisit as
lifetime-created if a per-player "one ever" unique appears (`CvCascadeTally.h:60-64`).

## 4. Save handling — REBUILT on load, serializes NOTHING ([DEC-tally-serializes-nothing])

**This doc is the home of the serialize-nothing ruling's explanation**
([DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing); decided
2026-06-17, LOCKED). The tally serializes **nothing**. On game load (and new-game init) it is **rebuilt** from
the authoritative loaded objects, then maintained incrementally by `DOMAIN` events during play.

**Why rebuilt, not saved:**

- **Derived, not source.** The tally is a pure additive roll-up of counts that already live in the saved
  objects (`m_paiBuildingCount`, unit counts, …). It holds no source-of-truth state; persisting it would
  duplicate what the save already has.
- **OOS / determinism.** The tally is the authoritative synced gate (`requires` / `allowed` / `per` read it),
  so a stale or divergent tally **is** a desync. A deterministic rebuild from the loaded objects makes every
  client's tally match actual state; a *saved* tally could drift (version skew, out-of-band mutation) — exactly
  what the DOMAIN/DIAGNOSTIC firewall ([event-spine §2](event-spine.md)) exists to prevent.
- **Save-format hygiene.** No serialization = no save surface, no versioning, no `@SAVEBREAK`, no migration as
  the tally shape evolves through #428/#430.
- **Authoritative ≠ persisted.** Authoritative at *runtime* (the gates read it), but fully recomputable from the
  base objects — authoritative like a *computed index*, not source state.

This is the same stance as the derived-data repository and the project's save-load principle that
**derived/repository state serializes nothing** ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)).

**The HOW — one deterministic scan, used two ways.** During play the tally rides event **deltas**. On load there
are no deltas (objects deserialize fully-formed), so the seed is the **bulk** form: **clear, then scan the loaded
objects and deposit their current counts.**

- `rebuild()` clears + seeds every wired domain (`CvCascadeTally.cpp:79`).
- `rebuildDomain` walks every alive player × every type index, reads the authoritative `seededTruth` (=
  `getBuildingCount` / `getUnitCount`, `CvCascadeTally.cpp:29-38`), and `deposit`s any non-zero count into the
  freshly-`clear`ed accumulator (`CvCascadeTally.cpp:57-77`).
- **One "what counts" routine, two callers.** That same per-domain enumeration is what `shadowVerify` /
  `shadowDomain` use to diff the event-maintained tally against ground truth (`CvCascadeTally.cpp:134-178`). One
  definition of "what counts" ⇒ the **load-seed and the live event path cannot diverge**.

**Mechanics (all confirmed in code):**

- **Idempotent + deterministic.** `rebuild()` is CLEAR-then-repopulate, so a second load in the same EXE session
  can't double-count; identical on every client (OOS-safe).
- **Hook — `CvGame::onFinalInitialized`, on every load/new-game** (NOT at consumer registration). The seed runs
  there so a stale tally can't gate before the first end-of-turn, and a 2nd in-session load reseeds
  (`CvEventSpine.cpp:302-305`, the registration comment). Events are **never** replayed on load — the loaded
  objects ARE the state.
- **Per-domain, co-located.** Each tracked domain owns its incremental hook AND its seed-scan contribution side
  by side, so they can't silently drift apart.

**Genuine historical counters are NOT owned by the tally.** A counter that is *not* recomputable from current
state (e.g. "units of type X ever created") lives on its **owning object** and is saved *there*; the tally only
reads/rolls it up. `getUnitCreatedCount` (§3.3) is exactly this — the engine persists it; the tally reads it for
the world-cap. So nothing unrecoverable ever lives only in the tally.

## 5. GROUNDED — the count-threshold inventory the tally must serve

Six count-threshold types, **all cross-city, zero per-city** — empirically validating the player-leaf store (a
city's set-HAS answers none of them) (sweep 2026-06-14, enabler-spec §13.2):
`PrereqNumOfBuildings` · `getNumCitiesPrereq` · `getUnitLevelPrereq` · `getNumTeamsPrereq` · `ProjectInfo
PrereqProjects/iNeeded` · the CIVIC city-limit (`CvPlayer.cpp` 6707/6754/6763/6685/6872/8466), plus the
SpecialBuilding group-cap waive (`specialBuildingsWaived`). The `per`-scaler demand (Corporation per-bonus
output, population-scaled deposits) adds the modifier-side readers.

The readers, all of which resolve through one tally module:

1. **Enabler `requires` count-thresholds** — `min(TYPE,N)`/`max(TYPE,N)` at empire/team/world read the tally;
   the higher-scope HAS sets *are* the tally. City-scope `requires` reads the local city count directly.
2. **Enabler `allowed` cap enforcement** — a build is permitted while `count(X, scope) < allowed` (via
   `countForCap`, §3.3); the engine, not the parser, does the `count < cap` check.
3. **Modifier `per` count-scaler** — `per:{type,each,scope}` at cross-city scopes resolves via the tally
   (`count / each`); `city`/`plot` = the local count.
4. **Demographics / UI / AI / score** — current counts (and lifetime facts via the owning object, §4). Wanted
   independent of the cascade — part of why the tally is its own module, built once and read several ways.

## 6. The count vocabulary — presence is the N=1 degenerate case

The JSON count vocabulary maps onto `count()` as (the atom evaluator applies the comparison;
`CvCascadeTally.h:18-21`):

| JSON form | meaning | resolves to |
|---|---|---|
| presence (`∈ HAS`) | "I have ≥ 1" | `count ≥ 1` |
| `min:N` | threshold | `count ≥ N` |
| `max:N` | cap | `count ≤ N` |
| exact-N | `min ∧ max` | (no separate primitive) |
| `allowed` cap | build permitted | `count < cap` (via `countForCap`) |
| `per:{each}` | count-scaler | `count / each` |
| `SELF` | own type at scope | `count(ownDomain, ownType, scope)` |

- **Presence ⇒ `min(X,1)`** (VOLUMETRIC-READY): authoring presence as the count form with N=1 means going
  volumetric later (resources gain amounts) is a value change, zero model rework.
- **Routing by Type PREFIX** — the id prefix (`BUILDING_`/`UNIT_`/…) selects the count domain; no separate
  `kind` field, the namespace self-routes (`CvCascadeTally.h:13-14`, the prefix → `COUNTDOMAIN_*` mapping).

## 7. Implementation status — design landed, coverage PARTIAL

| aspect | state |
|---|---|
| Substrate (`CvScopedAccumulator`), spine, tally as a consumer | **built** (slice 1+2) |
| Domain coverage | **buildings + units only** — `CountDomain` has `COUNTDOMAIN_BUILDING`, `COUNTDOMAIN_UNIT` (`CvCascadeTally.h:26-31`) |
| tech / civic / religion / bonus / project / specialist domains | **not yet wired** — each adds a `CountDomain` value + emit site + `seededTruth`/`domainNumTypes`/`onEvent` mapping + a shadow id |
| Stored leaf | **player-leaf** (per-player accumulator); city/plot scopes read live |
| Save handling | **serializes nothing**, rebuilt on load — built (`rebuild`/`rebuildDomain`) |
| Shadow verify (`shadowVerify` vs `getXCount`) | **built** — emits `DIAGNOSTIC` results per domain |
| `/tally` snapshot HTTP endpoint | **planned, not built** — DOMAIN emits are already observable via `/events` (see [event-spine §9a](event-spine.md)) |

Adding a domain is "add a `CountDomain` value, then wire its five touchpoints" (`CvCascadeTally.h:24-25`).

## See also
- [cascade-architecture §4](../../explanation/cascade-architecture.md) — the one-paragraph overview of the tally
  and how it sits among the three machines; this doc is the detail behind it.
- [event-spine.md](event-spine.md) — the front door that feeds the tally; the `DOMAIN`/`DIAGNOSTIC`/`TRACE`
  firewall (§2 there) is *why* the tally consumes only `DOMAIN`, and `IEventConsumer` is the contract it
  implements.
- [data-model.md](data-model.md) — the JSON `requires` count atom and `per` scaler that *read* the tally; the
  tally is engine machinery, never authored there.
- [DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing) — the ruling this
  doc explains (§4 is its home); related [DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted).
- [`../../architecture/patterns/composability.md`](../../architecture/patterns/composability.md) — the tally is
  an `IEventConsumer` implementation, the realized exemplar of the small-contract pattern.
