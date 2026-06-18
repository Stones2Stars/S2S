# HTTP server — the live observability / query layer

`CvHttpServer` (`Sources/CvHttpServer.{h,cpp}`) is S2S's **live game-state HTTP endpoint** and the
cascade's **formal live-observability / query layer** (event-spine-spec §9a — *not* an experimental
bolt-on). It serves read-only JSON snapshots of game state plus a Server-Sent-Events stream that
includes a live tee of the gated AI/cascade logs, so external tooling can watch a running game
out-of-process ("the counter-strike way"). Source of truth: the header comment block in
`CvHttpServer.h` and the line references below (they drift — confirm against the named function).

> **HARD CONSTRAINT — the server thread NEVER touches live game objects.** It runs on its own Win32
> thread and only reads the immutable snapshot the *game* thread publishes (the publish-and-serve
> contract; the same one the derived-data repository uses — [`../plans/derived-data-repository.md`](../plans/derived-data-repository.md) §8b).
> This is what makes it safe to query mid-turn without OOS/threading risk.

## At a glance
| | |
|---|---|
| **Bind** | `127.0.0.1:7227` only (`HTTP_PORT`) — never reachable off-machine |
| **Protocol** | GET-only HTTP/1.0. Anything but `GET` → `405 Method Not Allowed` (`Allow: GET`) |
| **On / off** | BUG option `Autolog__HttpServer` (BOOL, **default off**), Logging tab |
| **Log-tee gate** | BUG option `Autolog__LogLevelStream` → `gStreamLogLevel` (INT, **default 1**) |
| **Thread** | own Win32 thread, `select()` loop, 2s per-socket timeouts, graceful stop on option-off |
| **Snapshot refresh** | game thread republishes every `PUBLISH_INTERVAL_MS` = **5s** (`publishIfDue`) |
| **Response headers** | `X-S2S-Turn` (the turn the snapshot was taken on); `gameId` inside `/units`+`/players` JSON |

## Enabling it
1. In-game **BUG options → Logging tab → enable `Autolog__HttpServer`**, then close the BUG screen.
   Toggling is wired into `cvInternalGlobals::refreshOptionsBUG` (`CvGlobals.cpp:3105` →
   `CvHttpServer::setEnabled`), so it takes effect when the options screen closes — no restart.
2. For the `/events` **log** tee, leave/raise `Autolog__LogLevelStream` (`CvGlobals.cpp:3100` →
   `gStreamLogLevel`, default 1). A line streams only if it *also* passed its own file-log gate — the
   stream is a **subset** of what is file-logged (see "The live-log tee" below).

Smoke test once enabled: `curl http://127.0.0.1:7227/` → `hello world`.

## Snapshot / query endpoints
All return `application/json` "as of the last publish" (≤5s stale); unknown query params are ignored.
Routing is in `CvHttpServer::routeRequest` (`CvHttpServer.cpp:455+`).

| Endpoint | Returns |
|---|---|
| `GET /` | `hello world` (smoke test) |
| `GET /units` | array of every unit: id, owner, x/y, group, missionAI, activity, damage, level, `type` (e.g. `UNIT_WARDOG`), `unitAI` |
| `GET /units?id=N` | filtered to one unit id |
| `GET /units?playerNumber=N` | filtered to one player's units (combinable with `id`) |
| `GET /players` | array of every alive player: score, era, tech count, current research, cities, population, units, gold (+rate), science rate, production, civ, name, handicap |
| `GET /players?playerNumber=N` | filtered to one player |
| `GET /cities` | array of every city: position, name, population, food/production/commerce rates, production head (+turns left), building count, culture level, capital flag, and the live property values **crime / education / disease** |
| `GET /cities?id=N` | filtered to one city id (combinable with `playerNumber`) |
| `GET /cities?playerNumber=N` | filtered to one player's cities |
| `GET /events` | Server-Sent-Events stream (see below) |
| `GET /diagnostic` | lists the diagnostic gate-eval endpoints below |
| `GET /diagnostic/<gate>?type=PREFIX_NAME&player=N` | evaluate an engine gate (+ the cascade verdict where wired) on the current game state — see "Diagnostic gate-eval" below |

`gameId` (on `/units` + `/players`) is `CvGame::getGameId()` — the persistent playtest identity
(digits-only `yyMMddHHmm` local time for new games; older saves carry `DD-MM-YYYY HH:MM:SS`) — so
tooling can tell playtests apart and detect a reload / new game mid-session.

## The `/events` SSE stream
`text/event-stream`; the response **never ends** (the socket joins the broadcast list). At most
**8 concurrent streams** (`SSE_CLIENT_CAP`; a 9th gets `503`). Frames are pre-rendered on the game
thread (`publishEvent`, `CvHttpServer.cpp:795`) and drained/broadcast by the server thread, so it too
never touches game objects. The pending-frame queue is bounded (`EVENT_QUEUE_CAP` = **2048**; excess
frames are dropped — a backstop, sized for the log stream's headline bursts) and **self-draining even
with no client connected**, so it can't bloat.

| `event:` | when | `data:` | emit site |
|---|---|---|---|
| `hello` | on connect | `{turn, gameId}` | `beginEventStream` |
| `turnEnd` | game-turn increment (old turn) | `{turn, gameId}` | `CvGame.cpp:6015` |
| `turnStart` | game-turn increment (new turn) | `{turn, gameId}` | `CvGame.cpp:6022` |
| `playerTurnStart` / `playerTurnEnd` | **HUMAN players only** — the live "human thinking vs AI processing" phase signal | (player frame) | `CvPlayer.cpp:11974` |
| `log` | every gated log line at `gStreamLogLevel` and below (#419) | the **raw** log line | `streamLogTee` → `publishEvent("log", …)` |
| `: keepalive` | comment line every ~15s | — | server loop |

Turn *duration* analytics belong to the `[PERF]` file logs, not these phase events.

Quick watch (collect a window, then exit):
```bash
curl -s --max-time 5 http://127.0.0.1:7227/events
```

## The live-log tee (#419) — how AI/cascade logs reach `/events`
`streamLogTee(level, szLine)` (`BetterBTSAI.cpp:27`) is the one shared "log line → live stream" path:

```c++
void streamLogTee(int level, const char* szLine)
{
    if (level <= gStreamLogLevel && CvHttpServer::isEnabled())
        CvHttpServer::publishEvent("log", szLine);
}
```

- **Both** the BBAI/AI log helpers (`log<Domain>AI`, `[WAI]`/`[CIT]`/`[DAI]`/`[HAI]`/`[UNT]`/`[PERF]`)
  **and** the cascade event spine's logging consumer call it — so the spine's `[SPINE/DOMAIN]` /
  `[SPINE/DIAGNOSTIC]` lines (building/unit count changes, tally shadow results) appear in `/events`
  as `log` frames. Parse them out-of-process against the tag taxonomy in
  [`ai-logging-reference.md`](ai-logging-reference.md) (the wire spec).
- It has its **own** gate (`Autolog__LogLevelStream`) distinct from the per-domain file gates, so the
  *files* can keep deep forensics (level 3+) while the *pipe* carries headlines (level 1). A line
  streams only if it also passed its file gate → the stream is a subset of the files.
- Lines must be single-line (no embedded `\n`); off-state cost is one int compare.
- **Call-site rule:** `publishEvent`/`streamLogTee` no-op when the server is off, but C++ still
  evaluates the *arguments* — so guard expensive payload **formatting** with `CvHttpServer::isEnabled()`
  (or the spine's interest-guard / a log-level check) at the call site, or the off-state cost becomes a
  wasted format instead of one bool test.

## Diagnostic gate-eval endpoints (#430 cascade testing)
A read-only family for verifying the #428/#430 cascade against the engine **on the current loaded game state**,
without playing to force a state. Each evaluates an engine gate function and (where the cascade is wired) the
cascade equivalent, returning both so divergences surface as triage items.

| Endpoint | Engine gate | cascade verdict |
|---|---|---|
| `GET /diagnostic/canConstruct?type=BUILDING_X&player=N` | `CvPlayer::canConstruct` | **yes** — `cascadeBuildable` over the parsed JSON `requires`/`allowed`, + a cap shadow |
| `GET /diagnostic/canTrain?type=UNIT_X&player=N` | `CvPlayer::canTrain` | pending (option A) |
| `GET /diagnostic/canResearch?type=TECH_X&player=N` | `CvPlayer::canResearch` | pending |
| `GET /diagnostic/canDoCivics?type=CIVIC_X&player=N` | `CvPlayer::canDoCivics` | pending |
| `GET /diagnostic/canCreate?type=PROJECT_X&player=N` | `CvPlayer::canCreate` | pending |
| `GET /diagnostic/canMaintain?type=PROCESS_X&player=N` | `CvPlayer::canMaintain` | pending |

- `player` is optional → defaults to the **active player**. `type` is required (a `PREFIX_NAME` id).
- Example response (`/diagnostic/canConstruct?type=BUILDING_ABU_SIMBEL`):
  `{"action":"canConstruct","type":"BUILDING_ABU_SIMBEL","player":0,"legacy":false,"cascade":true,`
  `"notes":"0 atoms wired, 2 pending [build:scope-pending(plot); build:disabled-predicate-pending]",`
  `"cap":{"scope":"world","json":1,"legacy":1,"tally":0}}` — the cascade `cap` shadow proves the parsed
  `allowed.world` equals the engine's `getMaxGlobalInstances` and the live tally count; `notes` lists what the
  tally can't evaluate yet (option A: plot/city scope, predicates).
- **Evaluated on the game thread, never the server thread** (the hard constraint): the request is parked in a
  single-slot mailbox that `publishIfDue` services on its next frame tick (so the answer is a consistent
  game-state read; "5s-stale is sufficient"). A second concurrent request gets `503`; a non-ticking (paused with
  no `update`) game thread eventually self-heals. GET-only — no mutation, so no construction is performed.
- The doTurn harness `cascadeReadJsonSlice` (gated by `gPlayerLogLevel`) logs the same shadow for two sample
  buildings every turn as `[READJSON]` lines (Cascade.log + `/events`); the endpoints are the on-demand spot-check.

## Architecture & safety
- **Snapshot isolation.** The game thread snapshots queryable state into a refcounted immutable
  `GameSnapshot` (`bst::shared_ptr<const GameSnapshot>`) under a critical section that only guards the
  pointer swap (nanoseconds). Readers render from their own reference, so a slow full-dump render can
  never stall the game thread's publish.
- **32-bit memory ceiling.** The process is 32-bit (LAA ~4GB); the bounded + self-draining event queue
  and refcounted snapshot keep the transient footprint to a few hundred KB worst case (event-spine-spec
  §9b). No per-event heap churn on the publish side.
- **winsock 1.1 deliberately** (`#include <winsock.h>`, not winsock2) — a unity-batch reshuffle can pull
  a full `windows.h`/`winsock.h` first, and winsock2 can't follow it. Everything used is 1.1 API.
- **Lifecycle.** `setEnabled(bEnable)` is idempotent and game-thread-only; `publishIfDue()` is the
  per-frame publish hook (`CvGame::update`, `CvGame.cpp:2407`), throttled to one snapshot / 5s.

## Planned — `/tally` snapshot endpoint (NOT yet built)
The #428/#430 **tally** (`Sources/Cascade/CvCascadeTally`) will expose a **`/tally` snapshot endpoint
the same publish-and-serve way as `/cities`** — a GET returning the current per-(domain, type, scope)
counts as JSON — *when it lands* (event-spine-spec §9a). It does **not** exist today; the live routes
are only `/`, `/units`, `/players`, `/cities`, `/events`.

Note the distinction: **tally DOMAIN *emits* are already observable** today via `/events` (they tee
through `streamLogTee` as `[SPINE/DOMAIN]` `log` frames — `buildingCount`/`unitCount` deltas). What is
missing is the **`/tally` snapshot GET** (the current aggregated counts at a point in time, the count
sibling of `/cities`). When it lands it should publish from the game thread into the snapshot the same
way the others do — never reading `cascadeTally()` from the server thread (the HARD CONSTRAINT above).

## Cross-references
- [`../plans/event-spine-spec.md`](../plans/event-spine-spec.md) §9a/§9b — the spine's relationship to this layer; the planned `/tally`.
- [`../plans/derived-data-repository.md`](../plans/derived-data-repository.md) §8b — the shared publish-and-serve contract.
- [`ai-logging-reference.md`](ai-logging-reference.md) — the tag taxonomy the `/events` `log` frames carry.
- [`../plans/ai-vs-human-benchmarking.md`](../plans/ai-vs-human-benchmarking.md) — the benchmarking tooling that consumes `/players` + `/cities`.
