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

## The live HTTP server (today)
- Bind **`127.0.0.1:7227`**, GET-only HTTP/1.0 (405 otherwise). BUG option `Autolog__HttpServer` (default **off**).
- Snapshot refresh: the game thread republishes every **5 s** (`publishIfDue`) — responses are ≤ 5 s stale.
- **HARD CONSTRAINT:** the server thread NEVER touches live game objects — it reads only the immutable snapshot the
  game thread publishes (why the server is read-only and the event-spine names no Boost type).
- `gameId` on `/units` + `/players` = the persistent playtest identity (detects reload / new-game mid-session).
- Live `/units` movement fields: `baseMoves`/`maxMoves`(×100 budget)/`movesLeft`/`moveDiscount`/`range`(air)/`domain`;
  `/cities` carries live crime/education/disease + `corporations`/`presentCorporations`.
- `/extractor` = **raw game state only, no calculated values** (the lone map number is `distanceFromCapital`); schema
  at `Tools/ModifierCalc/README.md`. This is the [validation](../specs/validation.md) "calc-zero-ride-in" guardrail.

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
The migration target is one routing — `emit → CvEventSpine::dispatch → CvCascadeLogConsumer / Tally / grants` (the
[logging](../specs/logging.md) §4 event spine). Old anomalies slated for removal: dead `logCB`/`logToFile` Python
exports (an arbitrary-file-write surface), the `C2C.log` firehose, the `rjLogLine` split gate (a hardcoded level-1 tee).

## See also
- [../specs/logging.md](../specs/logging.md) — the observability bar + hook-shape *design*.
  [../specs/http-endpoints.md](../specs/http-endpoints.md) — the endpoint redesign this server moves toward.
  [../specs/validation.md](../specs/validation.md) — the extractor's role as the verification oracle.
