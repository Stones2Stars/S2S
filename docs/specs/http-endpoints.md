# HTTP endpoints — the observability surface

The local server (`127.0.0.1:7227`) publishes game state for reading. It is a **GET-only** dev server, gated by the
BUG option `Autolog__HttpServer` (off by default), bound to loopback only.

**This doc is nearly empty on purpose.** There is no route catalogue, and the emptiness is the design — read the
next section before you change anything here.

---

## ⛔ WHY THE SURFACE IS EMPTY — an endpoint is a LIVE CONSUMER

The route bodies were purged **wholesale**, and the reason is not that they were untidy:

> *"I expected the entire http endpoint doc to be mostly empty, because keeping any endpoints would ensure legacy
> has a potential to survive when it should not."* (owner)

A legacy data member whose only remaining reader is a route is **not actually still used** — but the **compiler
census cannot tell the difference**. The member compiles, so the delete-driven cut walks past it; it survives by
being kept alive *self-referentially*: the member exists because the route reads it, and the route exists to read
the member. A route is therefore the ideal hiding place for exactly the legacy this rebuild exists to remove, and
it hides it from the one census we trust ([DEC-playability-not-a-gate](../architecture/decisions.md#dec-playability-not-a-gate)
— removal is delete-driven and the compiler is the census;
[DEC-no-legacy-masking](../architecture/decisions.md#dec-no-legacy-masking) — legacy must fail LOUD, never be
preserved by a reader).

⛔ **So: restoring a route in order to read a legacy value is the BANNED move** — not a shortcut, not a stopgap,
not "just for observability while we finish". It is the precise mechanism that would resurrect legacy, and it
looks like helpfulness every single time. The surface is not restored until the new access/getter surface exists,
because only then can an endpoint read what every other consumer reads
([DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface)) instead of reaching around it.

**When the surface returns it is re-specced here, against that access surface** — ⛔ the roadmap's open item
([THE OPEN ITEM — the ACCESS surface](../plans/structural-cleanup/roadmap.md#-the-open-item--the-access-surface)).

⚖ **WHAT IT SHOULD CARRY *IS* DECIDED, THOUGH: DECOMPOSITION CENSUSES (owner).** *"Censuses like this are the
exact censuses we want to have in the http endpoints, because they give us real breakdowns, that are
observable."* A route that serves ONE number answers nothing when that number is wrong; a route that serves a
value **term by term** — the growth threshold beside its base, its gamespeed percent and its era percent; the
consumption beside its per-pop rate — attributes a divergence to a NAMED source without a code read. That is
the [DEC-obs-scale](../architecture/decisions.md#dec-obs-scale) Orwell bar as a route shape, and it is what the
no-guessing rule needs in order to be followable at all: at a gap the moves are VERIFY or ASK, and a bare total
supports neither.
⛔ It does NOT reopen the route ban above — the test is unchanged: a census reads the cascade's OWN computed
terms, never a legacy accumulator, so nothing is kept alive by its existence. ⚑ Until the surface returns the
same breakdown is emitted as a DIAGNOSTIC spine fact (`[MODIFIER] growthRead`, `[MODIFIER] rateRead`,
`[MODIFIER] plotsFan`), which is where a value not on a surface belongs meanwhile — and those emits are what the
routes serve when they land.
⚑ **`rateRead` is the worked example of what a census buys.** A city's §2a yield RATE is SIX independent
quantities collapsed into one int (`plotBase` · `trade` · `goldenAgeYield` · `upperFlat` · `specialists` ·
`cityFlatExtra`, plus `percentSum` and the `workedPlots` the plot Σ walked), so "this city produces too little"
is unanswerable against the total and immediately answerable against the terms.
⚑ **And a term that is itself a Σ decomposes again — `plotBase` carries its THREE SEGMENTS beside it**
(`plotNature` · `plotImprovement` · `plotRest`, the plot package's own storage split). One level of
decomposition only moves the question: a short `plotBase` says the plots are short and not WHICH leg is short,
and a dead improvement leg is the same number in the total as a dead nature leg. ⚠ The segments are reported
RAW — pre-floor — so they need not add to `plotBase` exactly (`readFlat` floors nature at 0, improvement at
−nature, the total at 0); **a gap between Σsegments and `plotBase` is itself the signal that a floor is biting**,
which a floored report would have hidden. They come out of the SAME walk the total does
([DEC-single-implementation]).
⚑ **`specialists` decomposes too, and on its OWN line (`[MODIFIER] specialistRead`, one row per held type)** —
because that Σ has an axis the term does not: WHICH specialist type. A per-type row is not a term, so it could
never have ridden `rateRead` inline, and `rateRead` is at the field cap besides
([event-spine.md](event-spine.md)). ⚠ Each row carries the ASSIGNED and the FREE-TYPED count **separately**: the
term multiplies by assigned alone while [modifier.md §2a](modifier.md) and the engine both say the count is the
sum, so the two columns SIZE that gap without moving a value. A type held only as free-typed reports a row with
contribution 0 rather than no row at all — an absent row would read as "no such specialist here". ⛔ Its terms come OUT of the real
combine rather than being re-derived beside it ([DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)):
a census that recomputed its own decomposition could disagree with the number it claims to explain, which is the
one thing it must never do.

---

## The transport (what exists)

- Bind **`127.0.0.1:7227`**, loopback only — never reachable off-machine. **GET-only**: anything else gets
  `405 Allow: GET`, an unmatched path gets `404`. Every response carries the **`X-S2S-Turn`** header.
- Gated by the BUG option **`Autolog__HttpServer`** (default off). The server can come up at the **MAIN MENU** via
  the `HTTP_SERVER_FROM_MENU` global define, so the whole XML/JSON load is capturable.
- **`/`** — liveness, `hello world` (the 11-byte smoke check).
- **`/events`** — the gated `[TAG]` SSE stream, served on the server thread, never ending (`: keepalive` ~15 s).
  There are **≤ 8 concurrent stream slots**; beyond that it answers `503 {"error":"too many event streams"}` — a
  capture that exhausts the slots records NOTHING while reading exactly like "the feature did not fire". Per-turn
  lines burst at the top of `doTurn`, so connect *before* the turn ticks. See [logging.md](logging.md).
- **The single-slot game-thread mailbox.** A data request is serviced on the game thread and waits up to
  **18 seconds**; a second concurrent data request — or one whose answer does not arrive in time — gets
  **`503 … retry`**.
- The route table in `CvHttpServer.cpp::handleRequest` is the one place any endpoint is declared; the `/state` and
  `/computed` index pages are generated from it (`/state`'s list is empty today).

## The two standing invariants

- ⛔ **The server thread NEVER touches live game objects.** That is why data routes go through the mailbox at all:
  the server thread only renders the answer the game thread produced, plus a small published `{turn, gameId}`
  header for response metadata and the `/events` hello. This is architecture, not convenience.
- ⛔ **The server SERVES state; it does not ACCUMULATE it.** Never grow a per-feature counter or accumulator behind
  a route to answer one question — that is how the previous surface accreted, hundreds of them, one per route.
  There are exactly two observability surfaces and a side-counter is neither: a live question is answered by the
  **`/events` stream**, a post-hoc one by the **`.log` files**. If a fact is on neither, EMIT it as a spine event —
  the file consumer and the stream then carry it for free.

---

## ⛔ THE STORED-vs-ORACLE ROUTES ARE DEAD — DO NOT RUN THEM, DO NOT REBUILD THEM

The six routes (cascade packages / enabler operating set / team capabilities, served `stored` and `oracle`) were
the missed-emit tripwire: the same values twice, event-built and recomputed-from-source, diffed outside the DLL.

**⛔ The oracle side CANNOT WORK the way things are set up (owner).** Reproducing event-built state means
replaying the FULL EVENT CHAIN, and an endpoint cannot build that chain — so the oracle does not answer a second
derivation of the same quantity. It answers a number that was never comparable.
⚑ **The tell, before the numbers fool you:** an oracle fetch is supposed to be a full recompute and orders of
magnitude slower than its stored twin. A whole-empire fetch returns in **half a second**. It is not slow because
it is not recomputing — and the diff then reports ~1500 divergent city slots with the oracle 17-29x high, which
reads as a catastrophic cascade failure and is entirely the instrument
([superseded-ideas #33](../architecture/superseded-ideas.md)).
⛔ **This is the single most-revived dead idea in the project** — *"agent after agent refuse to let it go"* — so
the ban is on RUNNING it as evidence, not merely on rebuilding it. A number from a broken instrument is worse
than no number: it is specific, plausible and wrong.

**⚖ WHAT TO DO INSTEAD (owner) — the THREE legs, and two of them is not a check:** read the **LOGS** (what
actually landed: source, channel, scope, unit, driving fact, apply COUNT), check them against the **JSON INFO**
(what that source is authored to deposit), and against **WHAT STATE EXPECTS** (who holds the source, which gates
hold, what the counts are). A deposit is conditioned and scaled, so the authored number alone predicts nothing —
correctness is all three agreeing, attributed to a named source with numbers.

## See also
- [logging.md](logging.md) — the SSE `[TAG]` stream and the read rules.
- [observability.md](../reference/observability.md) — the operational surface as it stands today.
- [validation.md](validation.md) — the live-verification discipline this surface feeds.
- [state-repositories.md](../architecture/state-repositories.md) — the stored-vs-oracle tripwire's home.
