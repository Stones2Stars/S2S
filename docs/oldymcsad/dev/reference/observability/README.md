# Observability — the surveillance surface (the "Orwell" net)

> **Status:** reference (the shared scaffold for the per-system maps) · **Grounding:** the old observability
> net + `cascade-mapping-inventory.md` §D. · **Home of** [DEC-obs-scale](../../architecture/decisions.md#dec-obs-scale)
> + [DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

Observability is **load-bearing, not polish**: you cannot safely delete a legacy maintainer you cannot fully
observe, so every behaviour is shadowed against the live engine until clean, then cut
([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)). This area is the surface
that makes that possible.

## The reconstruction bar (the "Orwell" bar)
Reconstruct full game state from the **HTTP endpoints + `/events` SSE + gated logs alone — never the
screen.** The live surface and the **rules for reading it** (the game holds logs open; use `/events` +
`/diagnostic`) are in [`http-server.md`](http-server.md) — read that before any live read.

## The Observability Scale (0–5)
`0` Oblivious · `1` Telescreen · `2` Informant · `3` Big Brother · `4` Thought Police · `5` Meta. Most game
systems sit at Tier 1 today (coarse snapshot, no *why*); the per-system maps assess each and propose the
hooks to climb.

## The three canonical hook shapes
Every observability hook is one of these — cheap, gated, off by default:
1. **Snapshot field** — a read-only field on the `/players` | `/cities` | `/units` snapshot (game-thread
   copy).
2. **Gated `[TAG]` log line** — by `gPlayerLogLevel` / `gCityLogLevel` / `gTeamLogLevel` / `gUnitLogLevel`,
   teed to `/events` via `streamLogTee`.
3. **Mailbox `/diagnostic/*` endpoint** — on-demand, game-thread-serviced (the `canConstruct` /
   `placementSweep` pattern).

## The measurement vehicle, and the two axes
- **An AI-only AUTOPLAY session is the canonical way to test the reconstruction bar.** With no human and no
  UI in the loop, every piece of state the AI acts on *must* be readable from the endpoints + `/events` +
  gated logs or it is invisible — which is exactly the bar. An AI-player sweep is also a *purer* cascade-vs-
  engine comparison (no BUG/UI display-layer artifacts). So measure observability against a running autoplay,
  not a human game.
- **Rate two axes SEPARATELY — don't conflate them.** (1) the **cascade / buildability** surface (the
  `/diagnostic` sweeps + shadows) and (2) the **whole-game-state** surface (the ~22 per-system maps below)
  are scored on the 0–5 scale *independently*. A high score on one says nothing about the other; a claim like
  "we're at Tier 3" must name which axis.

## Per-system maps
The per-system observability maps (food, health/happiness, culture, religion, espionage, gold/maintenance,
research, great-people, war-weariness, civics, promotions, trade-routes, victory, vision, …) are rebuilt
into this directory from the old set per [`../../_meta/build-plan.md`](../../_meta/build-plan.md) §2c. Each
follows: *how it works → what's on the wire → the gap → hooks to climb (using the three shapes above)*.

## See also
- [`http-server.md`](http-server.md) — the live surface + the live-read rules.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why this exists.
