# The HTTP server & the live-read rules — the observability surface

> **Status:** reference (static layout + fixed operational rules) · **Grounding:** `Sources/Tools/CvHttpServer.{h,cpp}` (under `Sources/Tools/` after the source-tree reorg)
> (header comment + routing) verified against the old `http-server.md` 2026-06-19; the logs-held-open rule
> is authoritative in [`../../../AGENTS.md`](../../../../AGENTS.md). Line numbers drift — confirm the named
> function.
>
> This is the **fixed, factual** layout of the live surface and the rules for reading it. These facts do
> not change and keep getting mis-stated — they live here so they are **never re-litigated**.

`CvHttpServer` is S2S's live game-state HTTP endpoint and the cascade's formal live-observability layer —
read-only JSON snapshots + an SSE stream, so external tooling watches a running game out-of-process.

> **HARD CONSTRAINT — the server thread NEVER touches live game objects.** It runs on its own Win32 thread
> and reads only the immutable snapshot the *game* thread publishes. That is what makes querying mid-turn
> safe (no OOS / threading risk).

## At a glance
| | |
|---|---|
| **Bind** | `127.0.0.1:7227` only (`HTTP_PORT`) — never off-machine |
| **Protocol** | GET-only HTTP/1.0 (anything else → `405`) |
| **On / off** | BUG option `Autolog__HttpServer` (default **off**), Logging tab |
| **Snapshot refresh** | game thread republishes every **5 s** (`publishIfDue`); responses are ≤5 s stale |
| **Smoke test** | `curl http://127.0.0.1:7227/` → `hello world` (an 11-byte up-check) |

## Endpoints
| Endpoint | Returns |
|---|---|
| `GET /` | `hello world` (smoke test) |
| `GET /units` `[?id=N\|?playerNumber=N]` | every unit: id, owner, x/y, group, `missionAI`, activity, damage, level, `type`, `unitAI` |
| `GET /players` `[?playerNumber=N]` | every alive player: score, era, tech count, research, cities, population, units, gold (+rate), science rate, production, civ, name, handicap |
| `GET /cities` `[?id=N\|?playerNumber=N]` | every city: position, name, population, food/production/commerce rates, production head (+turns left), building count, culture level, capital flag, and the live properties **crime / education / disease** |
| `GET /events` | Server-Sent-Events stream (never ends; ≤8 concurrent) — see below |
| `GET /diagnostic` `/diagnostic/<gate>?type=PREFIX_NAME&player=N` | evaluate an engine gate (+ the cascade verdict where wired) on the current state, computed on the game thread via the **mailbox** |

`gameId` (on `/units` + `/players`) is the persistent playtest identity, so tooling detects a reload / new
game mid-session.

## ⛔ THE LIVE-READ RULES — the facts that keep getting corrected

1. **The running game holds its `.log` files OPEN — you CANNOT stream/live-read them.** Tailing or reading
   `Documents/My Games/Beyond The Sword/Logs/*.log` (`Cascade.log`, `BuildEvaluation.log`, …) mid-session
   gives stale / empty / partial junk. **Do not do it, and do not infer "logging is off" from a quiet log
   file.** (Authoritative: [`../../../AGENTS.md`](../../../../AGENTS.md).)
2. **The two reliable live reads:**
   - **`/events` SSE** — but the per-turn shadow lines (`[MODSHADOW]`/`[PLACEMENT]`/`[STATE/*]`/`[READJSON]`)
     **burst at the TOP of `doTurn`**, so you must be **CONNECTED BEFORE the turn ticks** (connect-then-
     end-turn).
   - **`/diagnostic/*`** — computes an on-demand snapshot via the mailbox; **does NOT depend on
     `gPlayerLogLevel` or any log file.** The most reliable read, no timing games. When in doubt about a
     magnitude/state, hit the endpoint, not the log.
3. **Two separate gates:** `gPlayerLogLevel ≥ 1` makes the per-turn shadows *generate* their lines (to
   `Cascade.log`); the `/events` tee (`gStreamLogLevel`) is a **further** gate — a line can be in
   `Cascade.log` yet absent from `/events`.
4. **Read the live surface via the cheap `data-reader` minion** — never pull raw endpoint/log dumps into an
   expensive (Opus/Sonnet) context (a sweep dump is tens of KB). The minion curls / greps / aggregates and
   returns a compact summary; confirm "surface DOWN" with one smoke-curl before acting on a junk read.
   (Authoritative: [`../../../AGENTS.md`](../../../../AGENTS.md).)

## See also
- [`README.md`](README.md) — the observability scale + the three canonical hook shapes.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) §7 — why total
  observability is load-bearing ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).
