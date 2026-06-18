# Derived-data repository — architecture plan

> **⛔ SUPERSEDED (owner 2026-06-17) — do NOT build on this; do NOT treat as the live plan.** The #430 cascade
> (the scope-accumulator substrate + tally + modifier + enabler — see `cascade-engine-430.md`, `event-spine-spec.md`,
> `tally-cascade-spec.md`) subsumes this repository's intended purposes. The cascade accumulator is deliberately NOT a
> repository tenant (authoritative additive aggregation, no `TLazy`/version/dirty machinery). The skeleton
> (`CvDerivedData.{h,cpp}` + the `dataRepository()` wiring) **stays physically in place during shadow** (its
> `init()`/`reset()` is live), and its removal is a deferred cutover/demolition step. The one genuinely-useful leftover
> idea — a build-list cache for UI responsiveness on selection-change — IS the enabler's generated frontier, cacheable
> cleanly later if measured. Kept below as the historical record of the design we iterated through.
>
> **Part of a larger frame.** This is the **read-side** of the AI architecture north-star
> ([`ai-architecture-north-star.md`](ai-architecture-north-star.md)) — the change-driven derived-data
> modules. Read the north-star for the goal, the hard constraints (VC2003/C++03, EXE base-class ABI,
> MP lockstep determinism), and how this fits the unit-AI and backend work.

**Four drivers, served by one pattern** (they reinforce each other, not compete):

1. **Only (re)calculate what changed** — stop wiping whole caches every turn and rebuilding from
   scratch. (The worst case, the building-value `PreLoop`, is already fixed by memoization + the
   static enabler index — see §1. The next worst, the building-*value* recompute itself, is the
   flagship tenant.)
2. **One API surface (reusability)** — developers ask a getter (`getBuildingValue`,
   `getConstructibleSet`, …) instead of re-deriving; new code reuses tested logic instead of
   reinventing it (and its bugs).
3. **De-duplicate** — the same "scan all building/unit types to re-derive a relationship" idiom is
   hand-rolled across `CvCityAI`/`CvCity`/`CvPlayer`. A repository getter computed once replaces
   them all; the duplicated loops get deleted.
4. **Shrink the monster files** — moving these derivations out of the ~14k-line `CvCityAI.cpp` (and
   `CvUnitAI.cpp`/`CvPlayerAI.cpp`) into focused repository code is a direct file-size win.

> **Sequencing caveat (file-size vs risk).** Pure mechanical file-splitting (zero behaviour change)
> is safe and independent of this plan; repository extraction *touches AI behaviour* and is
> correctness-sensitive. The stale-value hang (§5) is the proof. Land behaviour-sensitive steps
> deliberately, with the gated-logging debug-verify (§5.6).

---

## 0. Status (2026-06-10)

- **Skeleton v2 IMPLEMENTED** (`Sources/CvDerivedData.h` + `.cpp`). Repositories live on the
  **base game objects** — `CvGame`/`CvTeam`/`CvPlayer`/`CvCity`, accessor `dataRepository()` — per
  the north-star placement ruling (v1 had them on the AI subclasses; corrected before any tenants
  existed, since the AI classes are being dissolved and the flagship tenants are UI-shared).
  Mechanics (see the header for the full doc comment and getter idioms):
  - `TLazyBase` — per-datum dirty flag, **change version counter** (bumped only when a recompute
    actually changed the value), computed-turn stamp, optional **bounded-staleness max age**.
  - `TLazy<T>` — typed value holder; `value()` is invariant-guarded; `set()` detects no-change
    recomputes so downstream version checks skip work (the common case — e.g. the constructible
    set is flat turn-over-turn).
  - `TDependency` — records the upstream `version()` a datum computed against: cross-**level**
    staleness is **pulled** via version compare, needing no invalidation fan-out wiring.
  - **Automatic registration** — every datum registers with its repository on construction, so
    `invalidateAll()`/`reset()` can never drift out of sync with the member list.
  - `reset()` wired into each owner's `reset()` (game init **and** load): derived data is never
    trusted from a save.
  - **Read-only phase** (`TLazyBase::setReadOnlyPhase`) — recompute/invalidate assert inside it;
    the precondition for the future parallel read pass (north-star §4).
  No data members yet — tenants land per §6.
- **The PreLoop hotspot is FIXED** (it was ~30% of the whole late-game turn): within-turn
  memoization (3.6× CABV) + the **static enabler reverse-index** (#195 Phase 1, PR #314 — PreLoop
  ~390×, ~11.7 s/turn → ~0.05 s, shadow-verified set-identical). The index lives on
  `cvInternalGlobals` (`getBuildingsEnabledBy`/`getUnitsEnabledBy`, built once in
  `doPostLoadCaching`) — that **is** this plan's Game-static tier, delivered (§3).
- **The flagship perf tenant is now BUILDING VALUES**, not the constructible set. The remaining
  doProduction chain re-computes per-building values for every city every turn (the flush at
  `CvCity.cpp:1261`). Cross-turn retention was tried and **REVERTED**: stale values hung the turn
  via `CvCity::doProduction`'s completion loop. That consumer is now **hardened** (bounded, gated
  `[CIT/spin]` diagnostics — §6 step 1, done 2026-06-10), unblocking the pilot.

---

## 1. Why — the evidence (updated post-#314)

- Pre-#314 measurement (`[PERF]`, late game, ~37 s turns): the turn was ~80% one chain —
  `CvPlayer::doTurn` → `doTurn.cities` → `city.doProduction` → `AI_chooseProduction` →
  `CalculateAllBuildingValues`. The PreLoop half of that is now fixed; the **rest of the chain
  remains**: per-building value math + the other CABV dimensions (~5.7 s/turn) and the non-CABV
  decision cost (~5 s/turn), recomputed per city per turn. (All numbers predate #314 —
  **re-measure before optimizing**, see [`turn-time-optimization.md`](turn-time-optimization.md).)
- **The pattern is everywhere, done the brute-force way:** `CvPlayerAI::AI_doTurnPre` wipes
  `m_cachedTechValues`, `m_aiCivicValueCache`, `m_numBuildingsNeeded`, `m_missionTargetCache`,
  `plotDangerCache`, and re-runs `AI_updateBonusValue()` — every turn, unconditionally. That is a
  degenerate repository with the dumbest possible invalidation ("invalidate all, always").
- There are already **three** `canConstruct` caches to **absorb** (not add a fourth):
  `CvPlayer::m_bCanConstruct[]`, `CvCity::m_bCanConstruct`,
  `CvCityAI::BuildingValueCache::m_buildingsToCalculate`.
- So this isn't a new system — it formalizes the existing caches into one pattern and replaces
  "clear everything every turn" with "recompute only what changed."

---

## 2. Core abstraction (implemented — see `CvDerivedData.h`)

A derived datum = **value + dirty flag + version + a recompute getter + the events that dirty it.**
The header's doc comment carries the authoritative getter idioms; in short:

- **Same level:** `if (m_foo.dirty()) m_foo.set(computeFoo()); return m_foo.value();`
- **Cross level:** freshen the upstream *through its getter*, then compare `version()` via a
  `TDependency`; recompute if it moved. Versions bump only on real value change, so an upstream
  rebuild that produced the same answer costs downstream nothing.
- **Bounded staleness:** a datum whose inputs move too often for event-exact invalidation declares
  a max age in turns; `dirty()` then also expires it by age. This is the systematic form of the
  "recompute every N turns" backstop.

No central registry/event bus — the per-member idiom plus automatic registration covers it;
revisit only if the event wiring becomes repetitive (north-star "fluid" decision).

---

## 3. Scopes & ownership — four levels + the GC static tier

Each datum lives at the **highest level where its inputs are invariant**; lower levels read upward
(City → Player → Team → Game) via the cross-level idiom.

| Tier | Owner | Holds | Invalidated by |
|---|---|---|---|
| **Static** | `cvInternalGlobals` (NOT the repository) | XML-derived indices: prereq/enabler reverse-indices (`getBuildingsEnabledBy`/`getUnitsEnabledBy`), unit/building classifications | never — built once in `doPostLoadCaching` |
| **Game** | `CvGame::dataRepository()` | game-**state**-derived, game-scoped data | per datum |
| **Team** | `CvTeam::dataRepository()` | known/obsolete tech sets, tech-derived availability, war state | tech acquired/obsoleted, war/peace |
| **Player** | `CvPlayer::dataRepository()` | tech/civic/promotion/bonus values, building counts, area unit-AI counts, needed-building counts | civic/trait/strategy change, unit/building count change |
| **City** | `CvCity::dataRepository()` | constructible set, building values, best-build, declared needs, workers-needed | building built/lost, tech, bonus, pop/plot change (+ bounded age) |

The static-vs-Game split is deliberate: truly static XML-derived data has a working home (`GC`,
never invalidated) and should **not** migrate into the repository; the Game repository is reserved
for game-state-derived data. Don't create a second home.

---

## 4. Staleness model — push dirty, pull versions, bounded age

Three pieces, used together:

1. **Push (dirty flag):** a mutation endpoint (`setHasBuilding`, tech-acquired, civic change,
   bonus change, …) calls `invalidate()` on the data it touches **at the level that owns the
   mutated input** only. No cross-level fan-out.
2. **Pull (version compare):** data derived from another level's datum self-detects staleness by
   comparing the upstream `version()` it last computed against (`TDependency`). This removes the
   "missed invalidation hook → silently stale forever" failure class that killed the value-cache
   experiment.
3. **Bounded age:** for data whose inputs move continuously (population, culture, properties —
   e.g. the constructible set), a max-age makes event-exactness unnecessary; events make it
   *prompt*, age makes it *correct*.

Incremental delta updates (apply the change instead of recomputing) remain an opt-in bonus for
cheap aggregates; default is dirty + lazy full recompute.

---

## 5. Safety & correctness (the hard-won rules)

1. **Advisory data only.** The repository holds derived AI heuristics, never synced game state. A
   stale advisory number → a slightly worse decision, never a desync.
2. **Consumers must be stale-tolerant.** The building-value retention experiment hung the game:
   stale values looped `AI_chooseProduction`. The datum was blamed, but the consumer is the
   landmine — *any* future missed hook reproduces the hang. Harden the consumer (validate the
   choice against live `canConstruct` before pushing; bound re-decisions) **before** retaining the
   data it reads. A repository value must never feed control flow that can spin.
3. **Determinism / MP-OOS.** Derived-only, rebuilt identically on every machine; never an input to
   synced state.
4. **Save/load.** Never serialized as truth — every owner `reset()` marks all data dirty; rebuild
   lazily.
5. **Bounded-staleness backstop.** `invalidateAll()` is the coarse safety net; per-datum max-age is
   the systematic one. Because data is advisory (rule 1), bounded staleness is acceptable.
6. **Debug-verify via gated logging, NOT asserts.** During a tenant's migration, recompute-and-
   compare emits through the gated `[PERF]` channel (ships in FinalRelease; `FAssert` does not, and
   asserts are reserved for invariant violations, not diagnostics). Mirrors the #195 Phase 1
   shadow-verify that proved the enabler index byte-identical before making it authoritative.

---

## 6. Migration plan (sequence agreed 2026-06-10; each step measurable + revertible)

0. **Re-measure the post-#314 `[PERF]` tree** (owner plays a late-game session, `gPerfLogLevel=1`,
   Autolog 0) — every whole-turn number predates the index; re-rank before optimizing.
1. **Harden `AI_chooseProduction` against stale inputs — DONE (2026-06-10).** Root cause of the
   retention hang: `CvCity::doProduction`'s completion loop (`while (productionLeft() <= 0)`).
   Stale value/`canConstruct` data lets the AI re-choose a building it just completed; the turn's
   overflow re-completes it instantly and `popOrder(bChoose)` re-picks it — the loop cycles
   forever within one city-turn. ("Chose nothing" can NOT hang it: an empty queue makes
   `getProductionNeeded()` return `MAX_INT`, which exits the loop.) Fix: cap completions per
   city-turn (50) + a defensive break/re-decide flag when no production gets established, both
   logging gated `[CIT/spin]` lines (FinalRelease-visible). A stale advisory value now degrades
   to one logged idle city-turn instead of a hang. Note: `pushOrder` already live-validates via
   `canConstruct`/`canTrain` — but those read the same caches, so correct invalidation (step 2)
   is the real fix and the cap is the backstop.
2. **Pilot tenants: building values + the three `canConstruct` caches** (~4.4 s/turn measured —
   the CABV half of the choose cost). Sequenced deliberately in three sub-steps, one measured
   cycle each:
   - **(a) Retention policy — SHIPPED + VERIFIED 2026-06-10:** the per-turn
     `AI_FlushBuildingValueCache()` in `CvCity::doTurn` is a staggered periodic refresh
     (`BUILDING_VALUE_REFRESH_PERIOD = 4`, offset by city id); the cache stays **complete**
     between refreshes. `setHasBuilding` still flushes immediately. Measured over 6 turns:
     **0 `[CIT/spin]`**, doTurn ~20.6 → ~18.6 s/turn, CABV −40% (4.4 → 2.7 s/turn). The
     residual CABV is event-driven (~190 completions/turn each retain-flush their city), so
     the refinement for phase (c) is invalidation granularity: completions rebuild the
     candidate SET only (cheap via the enabler index) while VALUES age out on the period.
   - **(b) was re-aimed by measurement:** the scoring half turned out to be a **second
     un-migrated legacy O(buildings²) enabler sweep** in `AI_scoreBuildingsFromListThreshold`
     (same pattern the PreLoop shed in PR #314). Migrated to `getBuildingsEnabledBy` with the
     legacy trigger-triple kept verbatim → scored values unchanged. The score-once memo as a
     repository datum is deferred until this is measured — removing work beats caching it.
   - **(c) Absorb the legacy caches** (`CvPlayer::m_bCanConstruct[]`, `CvCity::m_bCanConstruct`,
     `BuildingValueCache`) into the repositories, replacing the flush cascade — after (a)+(b)
     are proven in play; includes the granularity refinement from (a).
3. **Score-once memoization** (~4.6 s/turn measured — same size as step 2!): the
   `[PERF/choose]` data shows `AI_scoreBuildingsFromListThreshold` runs ~8.5× per choose (once
   per focus-flag cascade rule), re-gathering candidates and re-running focus-independent
   per-building production math every time. Memoize the candidate list + production-turns per
   value-cache lifetime as a City-repository datum; per-focus scoring then reads cached values.
   The enabler index (`getBuildingsEnabledBy`) gives targeted invalidation on completion.
   (The previously planned "gate process-running cities" lever is DEAD: measured 5/980 chooses —
   S2S cities always have buildings available; 77% of chooses are legitimate empty-queue
   decisions after a completion.)
4. **Absorb the `AI_doTurnPre` blanket clears** (tech value, civic value, bonus value, needed
   counts) one at a time → event-driven behind the same pattern.
5. **`recalculateAllResourceConsumption` → event-driven** (consumption moves only on building/
   bonus/city changes).
6. **Parallel read pass** (north-star §4): precompute, enter the read-only phase, fan the value
   calc across Win32 threads with a deterministic reduction.

---

## 7. Data-source inventory (candidates to absorb)

Per-city: constructible set, building values, best-build, workers-needed, yield/commerce base
rates. Per-player: tech value (`m_cachedTechValues`), civic value (`m_aiCivicValueCache`), needed
buildings (`m_numBuildingsNeeded`), bonus value (`AI_updateBonusValue`), promotion value, area
unit-AI counts, mission targets, plot danger. Per-team: tech/obsolete sets. Static (already in
GC): prereq reverse-indices, enabler graph.

---

## 8. What this unlocks beyond perf — city-declared needs (worker AI)

The City-level repository is the natural home for **a city's declared needs** — the plots/yields
it wants (food when starving, production, a specific bonus). Cities publish needs into the
repository; the per-player worker AI (`CvWorkerAI`) reads across all cities' published needs to
decide what to improve and where (memory/plan: `city-driven-worker-valuation`). One mechanism, two
payoffs: speed + better AI coordination. Aligns with lifting unit decisions to per-player
orchestration (`ai-unit-movement-to-player-level`).

### 8b. Queryability — the live-state endpoint (owner ruling 2026-06-11, #387)

**"Have the ability to do a http request, and get data from the repositories."** The repository
is the layer a live game-state query (dev/verification tooling — see #387) should serve:
tenants are versioned snapshots by construction, so a serving thread reading a *published*
repository version never races the game thread. Design requirement this places on the tenant
interface NOW (cheap to honor, expensive to retrofit): tenants must be **enumerable and
serializable** — a uniform "dump tenant to JSON" hook plus the version/timestamp header — so
the eventual endpoint is a thin reader over the registry rather than per-feature plumbing.

**Endpoint shipped (2026-06-11):** `Sources/CvHttpServer.{h,cpp}` — a GET-only HTTP/1.0
server on `127.0.0.1:7227`, on its own Win32 thread, gated by the BUG option
`Autolog__HttpServer` (Logging tab, off by default; toggling takes effect on closing the
options screen via `refreshOptionsBUG`). Live routes: `/` (hello-world smoke test),
**`/units`** with optional `?id=N` / `?playerNumber=N` filters — JSON per unit:
id/owner/x/y/type/ai (XML keys)/group/missionAI/activity/damage/level — and **`/players`**
(2026-06-11, for the AI-vs-human benchmarking initiative) with optional `?playerNumber=N`:
per alive player id/team/civ/name/human/npc/score/era/techs/research/cities/population/
units/gold/goldRate/scienceRate/production/handicap, rendered through picojson per the
rule below (it carries a free-text name field) — and **`/cities`** (same day) with
optional `?id=N` / `?playerNumber=N`: per city position/name/population/yield rates/
production head (+turns left)/buildings/culture level/capital plus the live crime,
education and disease property values (picojson; free-text name). Wrappers carry the
snapshot turn (also the `X-S2S-Turn` response header) and `gameId`. **`/events`**
(2026-06-12, #407) is a Server-Sent Events stream: `hello` (current turn + gameId) on
connect, then `turnEnd`/`turnStart` bracketing every game-turn increment
(`CvGame::doTurn` → `publishEvent`, a lock-guarded pre-rendered frame queue the server
thread broadcasts — SSE chosen over WebSocket: same persistent-connection rework, none
of the handshake/framing; consumable from PowerShell/curl/`EventSource` natively). At
most 8 concurrent streams; ~15s comment keepalives reap dead clients.

**Publish-and-serve contract (the pattern phase 2 generalizes):** the server thread NEVER
touches game objects. `CvHttpServer::publishIfDue()` runs on the game thread once per frame
(end of `CvGame::update`), is a single bool check while the server is off, and every 5s
while it is on walks the units of all alive players into a POD snapshot vector swapped in
under a critical section. The server thread renders JSON from that snapshot only. Data is
therefore "as of the last publish" (≤5s stale, labelled with its turn). Phase 2 = replace
the hand-rolled units walk with enumeration of repository tenants once the uniform
serialize hook exists; each tenant becomes a route the same way.

JSON rendering rule: `/units` hand-builds its JSON (flat ints + XML keys only — no escaping
risk, and faster than a DOM) and that is the boundary: **any field that can carry free text
(unit/city names, etc.) and the phase-2 tenant dump hook must use the vendored `picojson`**
(`include/picojson.h`, already in the PCH; FAssert's AssertsJson.log is the precedent) so
string escaping is never hand-rolled. Beware `CvString::format`'s ~82KB cap — it silently
returns an empty string beyond it; assemble large documents by concatenation.

Build pitfalls already solved in its comments — read them before touching: must include
`winsock.h` (NOT `winsock2.h` — unity batches pull a full-fat `windows.h` whose `winsock.h`
makes `winsock2.h` uncompilable), and winsock's `bind` must be taken through a typed
function pointer because `boost::bind` is visible at global scope via the PCH and VC7.1
resolves even `::bind` calls to it. The DLL is pinned by the server thread
(`GetModuleHandleEx` + `FreeLibraryAndExitThread`) so an EXE-side `FreeLibrary` can never
unmap code under the running thread.

## 9. Decisions — resolved & open

Resolved:

- **Placement:** base objects (`dataRepository()`), not the AI subclasses. Done.
- **Mechanics:** per-member `TLazy` idiom + automatic registration; no central registry for now.
- **Cross-level staleness:** version pull (`TDependency`), not invalidation fan-out.
- **Constructible-set style:** bounded staleness (max age) + prompt events, not event-exact.
- **Verification channel:** gated `[PERF]` logging, not asserts.

Open:

- Per-datum event endpoints vs coarse scope invalidation where a dependency is broad — decide per
  tenant.
- When (if ever) the per-member idiom gets repetitive enough to justify a central event registry.

## Cross-references

- [`turn-time-optimization.md`](turn-time-optimization.md) — the measurements; current lever ranking.
- [`unified-prerequisites-and-constructibility.md`](unified-prerequisites-and-constructibility.md) —
  the static enabler index (#195) that killed the PreLoop.
- Memory: `turn-time-optimization`, `city-driven-worker-valuation`, `ai-unit-movement-to-player-level`.
