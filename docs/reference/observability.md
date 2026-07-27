# Observability reference — the operational logging/endpoint surface (today)

> The OPERATIONAL detail behind the [logging](../specs/logging.md) + [http-endpoints](../specs/http-endpoints.md)
> *specs* — the concrete tag registry, gate knobs, the live HTTP server, the field census, and PlotSnapshot, **as
> they exist today**. (The specs are the design target; this is the current surface.) Lifted from the old
> observability-infra docs (the old `README` was fully superseded by `logging.md`).

## Gate knobs (the log levels)

- The four AI globals `gPlayerLogLevel` / `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel` are **aliases driven by
  the single `Autolog__LogLevelPlayerBBAI` BUG option**. `gPerfLogLevel` is independent; `gStreamLogLevel` (default 1)
  is a further subset gate on top of the file gate. `_DEBUG` forces all four AI globals to 4 — **inert in practice:
  the Debug config is not used.**
- ⛔ **`gPlayerLogLevel` IS the gate for everything (owner) — the other three are FOUR NAMES FOR ONE NUMBER.** All
  four take the same value from the same option, so a per-scope name promises a per-scope knob that does not exist:
  **do not read `gUnitLogLevel` in some AI file as evidence that unit logging has its own tier.** A NEW gate reads
  **`gPlayerLogLevel`**.
  **⚖ The single-value collapse is a CONSCIOUS owner-ruled interim, NOT drift and NOT a deferral to close.** Giving
  each scope a real knob means BUG-UI work, which was deliberately declined against higher-value work — *"we just
  roll with `gPlayerLogLevel`"*. Its END CONDITION is the founding decree below: **ALL log events go through the
  EVENT SPINE (owner)**, so a CALL-SITE gate global is a legacy-of-a-legacy — once a domain emits, gating is the
  CONSUMER's business (the file consumer's level + `gStreamLogLevel`), and these four globals have nothing left to
  gate. They retire wholesale WITH the direct `gDLL->logMsg` / BetterBTSAI-helper call sites they guard, not by
  being tidied first. ⛔ **So do NOT "fix" this by collapsing the three names onto `gPlayerLogLevel`** — that is a
  sweep across a surface scheduled for deletion, and the `DEC-no-deferred` reflex ("slated and never done ⇒ failure
  to fix") MISREADS it. The work is MIGRATING DOMAINS ONTO THE SPINE; the gates then disappear on their own.
- **Level semantics:** 1 = headline (`begin`/`best`/`decision`), 2 = per-decision (`score`/`order`/`act`), 3 =
  per-candidate (`cand`/`skip`), 4 = inner-loop (a genuine fire hazard — CTB emits 10k+ lines/turn at 4). Owner plays at 3.
- `OutputDebugString` is `#define`d to nothing under `FINAL_RELEASE` — it fires only in Release/Assert/Debug (any
  "fires in FinalRelease, CRIT" framing is wrong for the shipped build).

## Domain / tag registry

14 domains, each `[TAG]` prefix → log file → scope global → source: e.g. `[WAI]` → `BuildEvaluation.log` →
`gPlayerLogLevel` → `CvWorkerAI.cpp`; plus `[CIT]`/`[UNT]`/`[COM]`/`[WAR]`/`[CTB]`/`[ENG]`/`[PERF]`. `[PERF/reqmodel]`
passes when `mismatches=0`. `[INIT/*]` was renamed from `[GAME/*]` to avoid clashing with the `[STATE/game]` cascade
feed. Call-site census exists (WAI 43 sites, HAI 54, CTB 66, …). Dead sinks: `CB.log`, `C2C.log` (ruled DELETE).

**The process `memory` gauge** (`workingSetMB`/`peakWorkingSetMB`/`pagefileMB`, the CvPlotPaging
`GetProcessMemoryInfo` mechanism) splits a per-turn RAM climb (leak) from a one-time step (retained structure) —
load-bearing under the 32-bit ~3.2GB address-space ceiling. ⚠ Its `/computed/perf` route went with the route-table
purge; the gauge needs a surface again when the route table is rebuilt.

## The live HTTP server (today)

- Bind **`127.0.0.1:7227`**, GET-only HTTP/1.0 (405 otherwise). BUG option `Autolog__HttpServer` (default **off**).
- **The routes that answer** ([http-endpoints](../specs/http-endpoints.md)) are the derived-state cache documents —
  `/computed/cascade/packages/{stored,oracle}`, `/computed/enabler/operating/{stored,oracle}`,
  `/computed/capabilities/{stored,oracle}` — two per plane, in one shape, diffed by an EXTERNAL consumer (the DLL
  never compares them). Documents live in `Sources/Tools/CvOracleEndpoints.cpp`, never in the server file.
  ⚠ **There is NO other route**, and that is deliberate: an endpoint is a LIVE CONSUMER, so a route reading a
  legacy member keeps that member alive past the compiler census. ⛔ **Restoring a route to read a legacy value is
  the banned move** — the surface returns only with the access surface ([http-endpoints](../specs/http-endpoints.md)).
- `/` (liveness), `/events` (SSE log stream, ≤8 concurrent slots). `/state` and `/computed` bare return their route
  index — `/state`'s is currently EMPTY.
- **HARD CONSTRAINT:** the server thread NEVER touches live game objects. `/state` + `/computed` read live state, so
  they run on the **game thread via a single-slot mailbox** (`evalRequestBlocking` → `serviceEvalMailbox`, drained by
  `publishIfDue`); the server thread only renders the answer + a tiny published `{turn,gameId}` header (refreshed every
  ~5 s) for response metadata + the `/events` hello. A second concurrent data request gets `503` — retry once.
- The single **route table** in `CvHttpServer.cpp::handleRequest` is the one place every endpoint is declared
  (path → mailbox action → doc); the `/state` and `/computed` index pages are generated from it.
- ⛔ **The server SERVES state; it does not ACCUMULATE it.** `CvHttpServer.cpp` had accreted hundreds of per-feature
  counters/accumulators, one per route to answer one question each; the purge took them with the route bodies. Do
  not grow them back: there are exactly **two** observability surfaces, and a new counter is neither. A live question is
  answered by the **`/events` stream** (connect before the burst — the server now comes up at the MAIN MENU via the
  `HTTP_SERVER_FROM_MENU` define, so the whole XML/JSON load is capturable); a post-hoc question is answered by the
  **`.log` files once the game has closed**. If a fact is not on either surface, EMIT it as a spine event — the
  file consumer and the stream then carry it for free — rather than growing a side-counter behind an endpoint.
- Predecessor surfaces (the flat `/units`/`/players`/`/cities` snapshot routes, the `/diagnostic/*` grab-bag, the
  cascade-vs-legacy `/shadow` sweeps) are gone and are not to be revived; the `/shadow` tombstone is
  [superseded-ideas](../architecture/superseded-ideas.md) #12.

## The field census (event-spine migration input)

The exhaustive raw-field census: ~196 gated log templates across 10 domains, each field's name + cType + a sample
call-site. **Distribution:** ~80% int, ~15% string, ~5% typeIndex, ~3% float (PERF only); median 5–6 fields, ~85% fit
≤ 9, only 6 templates > 12. **Migration constraints:** wide `wchar_t*` strings can't travel raw on the spine — carry
entity IDs and let the consumer resolve names; `[STATE/dip]` is variable-width (scales with civ count); the `CTB`
pre-composed `CvString` criteria/joinInfo fields are the hardest to decompose; `[CIT/order] CONSTRUCT` score is an
`int64_t` outlier (needs a dual-slot / extended tag).

## PlotSnapshot — the one CSV surface

- Written at 4 call points (all from `CvGame`): `start` (new game), `load`, `regen`, `turn` (top of every `doTurn`,
  before AI decisions). File: `…/Beyond The Sword/Logs/PlotSnapshot_<tag>_t<turn>.csv`.
- **Rotation:** `turn` keeps only the last 3; `start`/`load`/`regen` wipe **all** other `PlotSnapshot_*.csv` — a turn
  file survives turn rotation but NOT a later start/load/regen (copy it out to keep).
- Uses raw `fopen`/`fclose` (gDLL holds handles open, blocking `remove()`); resolves `%USERPROFILE%\Documents\…` (not
  `SHGetFolderPath` — clashes with the `CATEGORY_INFO` macro), so it **fails silently under Documents redirection
  (OneDrive)**. Schema v2 includes the `animals` field (`<Type>@o<owner>c<combat>a<aggression>e<enemy>`) and the
  `improvementCurrentValue` `0 = uninitialised, not zero` caveat.

## Target consolidation

The migration target is one routing — `emit → CvEventSpine::dispatch → consumers` (the
[logging](../specs/logging.md) §4 event spine): the eventSpine is the ONLY place any "happening" lives, and
everything downstream is a consumer of it (owner, founding decree). Concretely (owner 2026-07-16):
**the BetterBTSAI log helpers (`logAIJson` et al.) are RETIRED — never route new work through them — and every
direct `gDLL->logMsg` inside Engine files is likewise unwanted**; each domain migrates by EMITTING spine events
(the field census below is the prepared input), whereupon it gains the file consumer, the `/events` stream
consumer, and the off-thread writer for free. Every DOMAIN event gets an assigned importance LEVEL as it
migrates (levels today are only meaningful on the DIAGNOSTIC side; DOMAIN defaults to 1). The multiplayer
**OOS special logger is deliberately KEPT** — a synchronization-debugging surface in its own right, and a
natural future consumer of the synced DOMAIN stream. ✅ **DONE — the turn-boundary side-channel is consolidated onto the spine.** The four bespoke
`CvHttpServer::publishEvent("turnStart"/"turnEnd"/"playerTurnStart"/"playerTurnEnd")` publishes are replaced by
`SEVT_TURN_STARTED`/`SEVT_TURN_ENDED` DOMAIN events ([event-spine.md](../specs/event-spine.md)), so the file
consumer and the `/events` stream carry them for free instead of each surface growing its own emitter. This is the
worked example of the rule above: a fact that is not on a surface is EMITTED as a spine event, never published
directly. ⚠ Consumer-visible break, accepted (owner): the wire form is now the standardized `[SPINE] turnStarted`/
`turnEnded` rendered line, not a named SSE frame carrying `{"turn","gameId"}`.
Old anomalies slated for removal: dead
`logCB`/`logToFile` Python exports (an arbitrary-file-write surface), the `C2C.log` firehose, the `rjLogLine`
split gate (a hardcoded level-1 tee), and the `BetterBTSAI.cpp:31` `publishEvent("log")` tee (the retired
BetterBTSAI log-helper family).

**The FILE sink is the off-thread `CvLogWriter`** (`Infrastructure/CvLogWriter.{h,cpp}`): the game thread
renders + enqueues; a dedicated Win32 thread does all disk I/O and flushes per batch — so spine-written log
files are READABLE WHILE THE GAME RUNS (the held-open pain applies only to the not-yet-migrated
`gDLL->logMsg` sinks). Files are truncated fresh per session; lines stamp `[sec.mmm]` at enqueue.

## See also

- [../specs/logging.md](../specs/logging.md) — the observability bar + hook-shape *design*.
  [../specs/http-endpoints.md](../specs/http-endpoints.md) — the transport, its standing invariants, and the
  stored-vs-oracle cache documents.
  [../specs/validation.md](../specs/validation.md) — the live acceptance discipline this surface serves.
