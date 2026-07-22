# Observability reference — the operational logging/endpoint surface (today)

> The OPERATIONAL detail behind the [logging](../specs/logging.md) + [http-endpoints](../specs/http-endpoints.md)
> *specs* — the concrete tag registry, gate knobs, the live HTTP server, the field census, and PlotSnapshot, **as
> they exist today**. (The specs are the design target; this is the current surface.) Lifted from the old
> observability-infra docs (the old `README` was fully superseded by `logging.md`).

## Gate knobs (the log levels)

- The four AI globals `gPlayerLogLevel` / `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel` are **aliases driven by
  the single `Autolog__LogLevelPlayerBBAI` BUG option**. `gPerfLogLevel` is independent; `gStreamLogLevel` (default 1)
  is a further subset gate on top of the file gate. `_DEBUG` forces all four AI globals to 4.
- **Level semantics:** 1 = headline (`begin`/`best`/`decision`), 2 = per-decision (`score`/`order`/`act`), 3 =
  per-candidate (`cand`/`skip`), 4 = inner-loop (a genuine fire hazard — CTB emits 10k+ lines/turn at 4). Owner plays at 3.
- `OutputDebugString` is `#define`d to nothing under `FINAL_RELEASE` — it fires only in Release/Assert/Debug (any
  "fires in FinalRelease, CRIT" framing is wrong for the shipped build).

## Domain / tag registry

14 domains, each `[TAG]` prefix → log file → scope global → source: e.g. `[WAI]` → `BuildEvaluation.log` →
`gPlayerLogLevel` → `CvWorkerAI.cpp`; plus `[CIT]`/`[UNT]`/`[COM]`/`[WAR]`/`[CTB]`/`[ENG]`/`[PERF]`. `[PERF/reqmodel]`
passes when `mismatches=0`. `[INIT/*]` was renamed from `[GAME/*]` to avoid clashing with the `[STATE/game]` cascade
feed. Call-site census exists (WAI 43 sites, HAI 54, CTB 66, …). Dead sinks: `CB.log`, `C2C.log` (ruled DELETE).

**The process `memory` gauge rides `/computed/perf`** (`workingSetMB`/`peakWorkingSetMB`/`pagefileMB`, the
CvPlotPaging `GetProcessMemoryInfo` mechanism) — poll across turns to split a per-turn RAM climb (leak) from a
one-time step (retained structure); load-bearing in the 32-bit ~3.2GB address-space ceiling.

## The live HTTP server (today)

- Bind **`127.0.0.1:7227`**, GET-only HTTP/1.0 (405 otherwise). BUG option `Autolog__HttpServer` (default **off**).
- **Two data buckets, split on verification** (full map + the route table: [http-endpoints](../specs/http-endpoints.md)):
  - **`/state/*`** — RAW inputs only, **no computed value** (no yields, no buildability verdicts). `/state/all`,
    `/state/techs`, `/state/players`, `/state/cities`, `/state/units`. This is the calculator's input side.
  - **`/computed/*`** — the engine's OWN answers (the verification ground-truth): `/computed/cities/yields`,
    `/computed/players`, `/computed/can*`, `/computed/available*`, `/computed/tally`, `/computed/whyNot`, `/computed/game`.
  - `/` (liveness), `/events` (SSE log stream). `/state` and `/computed` bare return their route index.
- **HARD CONSTRAINT:** the server thread NEVER touches live game objects. `/state` + `/computed` read live state, so
  they run on the **game thread via a single-slot mailbox** (`evalRequestBlocking` → `serviceEvalMailbox`, drained by
  `publishIfDue`); the server thread only renders the answer + a tiny published `{turn,gameId}` header (refreshed every
  ~5 s) for response metadata + the `/events` hello. A second concurrent data request gets `503` — retry once.
- The single **route table** in `CvHttpServer.cpp::handleRequest` is the one place every endpoint is declared
  (path → mailbox action → doc); the `/state` and `/computed` index pages are generated from it.
- ⛔ **The server SERVES state; it does not ACCUMULATE it.** `CvHttpServer.cpp` has long accreted per-feature
  counters/accumulators added to answer one question each (known debt, owner — a cleanup in its own right). Do not
  add more: there are exactly **two** observability surfaces, and a new counter is neither. A live question is
  answered by the **`/events` stream** (connect before the burst — the server now comes up at the MAIN MENU via the
  `HTTP_SERVER_FROM_MENU` define, so the whole XML/JSON load is capturable); a post-hoc question is answered by the
  **`.log` files once the game has closed**. If a fact is not on either surface, EMIT it as a spine event — the
  file consumer and the stream then carry it for free — rather than growing a side-counter behind an endpoint.
- The dropped predecessors (`/units`/`/players`/`/cities`, the `/diagnostic/*` grab-bag, the cascade-vs-legacy
  `/shadow` sweeps) and the rationale are in [http-endpoints](../specs/http-endpoints.md) "What was dropped".

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

## The `(scope,channel)` calc-count gate

Every calculation logs its `(scope, channel)` — scope ∈ world / team / empire / area / city / plot / building / unit /
specialist; channel = every modifiable number (base yields, commerce, properties, free XP, free specialists, …). The
per-turn count is a standing acceptance gate AND regression tripwire: over ~50k calculations for anything in a single
turn is near-certainly a failure (a blanket recompute has crept back); a quiet turn approaches zero; steady-state cost
tracks EVENT volume (thousands), never entity count (millions). The counter is exposed live via the StoneBase
performance dashboard (Razor + SignalR), the histogram naming the culprit scope/channel on a breach
([DEC-calc-count-gate](../architecture/decisions.md#dec-calc-count-gate)).

## See also

- [../specs/logging.md](../specs/logging.md) — the observability bar + hook-shape *design*.
  [../specs/http-endpoints.md](../specs/http-endpoints.md) — the endpoint redesign this server moves toward.
  [../specs/validation.md](../specs/validation.md) — the extractor's role as the verification oracle.
