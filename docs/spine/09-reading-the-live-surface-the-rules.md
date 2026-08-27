# 9. Reading the live surface — the rules

> Part of the **[spine](../spine.md)** spec.

> **The running game holds its `.log` files OPEN — never live-read them** *(the not-yet-migrated `gDLL->logMsg`
> sinks and `LSystem.log`, §8; a spine-written domain file is the exception — it is readable while the game
> runs).* Tailing a legacy log mid-session gives stale/empty/partial results; do not infer "logging is off" from
> a quiet log file.

The two reliable live reads:

- **The `/computed` cache documents** — an on-demand snapshot via the game-thread mailbox; depends on no log file
  and no gate, and is the most reliable read for a POINT-IN-TIME value. ⚠ They are the ONLY data routes that
  answer; there is no `/state` surface today ([http-endpoints.md](../specs/http-endpoints.md)).
- **`/events` SSE stream** — the gated `[TAG]` lines, live. DOMAIN facts stream unconditionally; DIAGNOSTIC/TRACE
  ride `gStreamLogLevel` (§2, §8). The per-turn lines burst at the **top of `doTurn`**, so you must **connect
  *before* the turn ticks** (connect-then-end-turn).
  - **⚠ Capture with an AUTO-RECONNECT loop, not a fixed-window curl.** `CvGame::doTurn` fires at the
    END of the inter-turn processing, which on a logged late-game turn can run **many minutes** — a fixed
    `curl -m 600` dies before the burst and the reconnect gap loses it.
    Capture with `while true; do curl -sN -m 3600 …/events >> capture.log; sleep 1; done` and grep the growing file.
  - **⚠ There are ≤ 8 concurrent stream slots** ([http-endpoints.md](../specs/http-endpoints.md)) — a capture loop
    left running, or one that respawns `curl` in a `while` loop, holds them; once exhausted the endpoint returns
    `503 {"error":"too many event streams"}` and your capture silently records NOTHING. Verify the first frames
    are `event: hello` and not that error, and kill every loop when done — an empty capture reads exactly like
    "the feature did not fire."
  - **A force-killed game may lose the tail** — `taskkill /F` can drop OS-buffered log/burst lines written moments
    earlier. Post-mortem `Cascade.log` reads (legitimate once the process is dead) are only trustworthy for data
    older than the kill by a few seconds.

> **Delegate bulk reads to the cheap `data-reader` sub-agent.** A sweep dump is tens of KB; pulling it raw into
> an expensive (orchestrator) context burns budget for nothing. The reader curls/greps, aggregates, and returns a
> compact distilled summary (histograms, cause-tags, anomalies). It must fail **honestly** (distinguish
> "surface down" from "reader error", never fabricate a clean summary); when it reports DOWN or returns junk,
> confirm with ONE cheap smoke-curl (`curl -s http://127.0.0.1:7227/` → `hello world`) before acting.

