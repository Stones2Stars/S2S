# HTTP endpoints — the observability surface

The local server (`127.0.0.1:7227`) publishes game state for reading. This is the **clean redesign** that
replaces the flat `/diagnostic/*` grab-bag (which jammed snapshots, gate-queries, decompositions and shadows into
one namespace). Endpoints are organized by **purpose**, and the load-bearing axis is **verification**: **`/state`**
serves the **raw inputs** (human-readable and **pre-formatted**; the input to our calculator), while
**`/extractor`** serves the **yield-loaded actual state** (the game's computed outputs — the ground-truth the
calculator is checked against).

> **Why two surfaces — the verification flow.** Our calculator runs on the **raw inputs** (`/state`) → *expected*
> outputs; the verifier checks those against the **actual yields** in the **yield-loaded state** (`/extractor` —
> *"honestly the new shadow"*). Inputs and outputs on **separate** surfaces is what prevents validation pollution
> (the calculator can never read the game's own answer). `/state` doubles as the human surface — one readable
> question, one readable answer.

> **⏳ The spec is the target, not the current server.** Mapping the live HTTP server to this structure — real
> route structure, DRY, the buckets below — is its **own dedicated session**; today's `/diagnostic/*` server does
> not yet match this spec.

---

## The buckets

| bucket | answers | audience | lifetime |
|---|---|---|---|
| `/` | alive? | both | — |
| `/events` | what just changed (SSE stream) | both | — |
| `/state/*` | **what is the state** (raw inputs, pre-formatted) | **frontend + validator** | — |
| `/extractor/*` | everything, comprehensively (the "new shadow") | machine / AI verify | — |
| `/can/*` | can I do X? (per-candidate) | both | — |
| `/shadow/*` | cascade-vs-engine quick sweeps | verify | **temporary — dies with migration** |
| `/decompose/*` | a modifier's named-source breakdown | human / verify | — |

---

## `/` · `/events`

- **`/`** — liveness ("hello world"); an 11-byte smoke check.
- **`/events`** — the gated `[TAG]` SSE stream, live. The per-turn shadow lines burst at the **top of `doTurn`**,
  so **connect before the turn ticks** (connect-then-end-turn). [logging](logging.md) §5.

## `/state/*` — the human surface

> **The principle: serve only RAW INPUTS — computed outputs (yields) are CONSCIOUSLY EXCLUDED from every `/state`
> endpoint.** A verifier pulls the inputs and **calculates the output itself**, then compares to the game's output.
> ⛔ If `/state` served the game's *computed* yield it would **pollute the validation** — you'd be comparing the
> game against itself instead of against an independent re-derivation. So `/state/city/<id>/plots` returns the
> worked plots *with their improvement, terrain, features, river, coast* (the yield-*determining* facts) and
> **never** the yield. **No `/state` endpoint emits a computed output** — that is a deliberate, load-bearing rule,
> not an omission.

> **And PRE-FORMATTED — one legible question, one legible answer.** `/state/player/<id>/techs` answers "which
> techs has this empire researched" as a **clean list**, *not* a JSON ocean to parse. This is the surface for
> **hand-building the validator** (and for eyeballing state by a human): targeted and legible, where `/extractor`
> is the machine firehose.

> **`/state` is a real API — two consumers.** Hand-building the validator doubles as building the **frontend's**
> game-state API: `/state` serves both the **validator** (the calculator's input) *and* the **frontend** (a clean,
> comprehensible state API for the UI). So it is held to **API standards** — stable, legible, comprehensible — not
> throwaway diagnostic output. (`/extractor`, `/shadow`, `/decompose` are the throwaway/verification surfaces.)

Shape: **`/state/<entity>/<id>/<slice>`**.

- **list views** — `/state/players` · `/state/cities` · `/state/units`
- **one instance** — `/state/player/<id>` · `/state/city/<id>` · `/state/unit/<id>`
- **raw slices** (extend as the human use-cases demand):
  - `/state/player/<id>/techs` — techs researched · `/civics` · `/religions` · `/gold` · …
  - `/state/city/<id>/buildings` — buildings built
  - `/state/city/<id>/plots` — the worked plots, each with **improvement · terrain · features · river · coast** —
    so the city's output can be re-calculated
  - `/state/city/<id>/units` — the garrison
  - …

## `/extractor/*` — the yield-loaded state (verification ground-truth)

The extractor is the comprehensive, **yield-LOADED** state — the game's actual computed outputs (the real yields +
loaded state), machine-parseable and cast-iron. It is the **verification ground-truth**: our calculator runs on
the raw `/state` inputs to produce *expected* outputs, and the verifier checks those against the extractor's
*actual* yields. *"Honestly the new shadow"* (owner) — the durable verification surface. It is **not** the same
data as `/state`: `/state` is the **input** side (no computed outputs), `/extractor` is the **output** side.

## `/can/*` — enabler gate queries

"Can I do X?", per-candidate — the [enabler](enabler.md) "can I?" surface:
`/can/construct` · `/can/train` · `/can/research` · `/can/civics` · `/can/build` · `/can/hurry` ·
`/can/acquireExperience`.

## `/shadow/*` — quick targeted shadows (⏳ TEMPORARY)

The legacy `/diagnostic` sweeps, kept as quick cascade-vs-engine checks while channels are being proven:
`/shadow/buildable` (was `sweep`) · `/shadow/placement` · `/shadow/dormancy` · `/shadow/modifier` ·
`/shadow/movement`. **These die with the migration** (owner) — once a channel is shadow-clean and its legacy
maintainer is cut, its sweep goes. `/extractor` is the durable shadow that remains.

## `/decompose/*` — modifier source breakdown

`/decompose/modifier` — a modifier value's **named-source decomposition** (every component attributed to a named
source with numbers — the map-everything / no-guessing surface, [logging](logging.md) §6).
NB the old `/diagnostic/cityInput` (pre-computed city yields) is **superseded by raw `/state/city/<id>/plots`**,
per the raw-inputs principle above.

---

## See also
- [logging.md](logging.md) — the observability bar, the three hook shapes, and the read rules
  this surface serves (the `/diagnostic`→`/shadow` mailbox pattern, `/events` timing, the `data-reader` minion).
- [enabler.md](enabler.md) — the "can I?" machine behind `/can/*`.
- [modifier.md](modifier.md) — the magnitudes that `/decompose/modifier` attributes.
