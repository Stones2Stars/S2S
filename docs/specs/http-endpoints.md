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
(`plotNature` · `plotImprovement` · `plotRest`, the plot package's own storage split), with **`plotRoute` as a
BREAKDOWN of `plotRest`** rather than a fourth term: the package stores route apart (for the golden-age
operand, [modifier.md §2](modifier.md)) while `plotRest` keeps reporting route + the owner's plot flats, so no
stored segment goes unreported. ⚠ `plotRoute` reaches the
`/computed/city/yield` document and the tooltip, NOT the `[MODIFIER] rateRead` line — that line stands at the
16-field cap, where a seventeenth term is silently DROPPED ([event-spine.md](event-spine.md)). One level of
decomposition only moves the question: a short `plotBase` says the plots are short and not WHICH leg is short,
and a dead improvement leg is the same number in the total as a dead nature leg. They come out of the SAME walk
the total does ([DEC-single-implementation]).

> **⛔ Σsegments DOES NOT EQUAL `plotBase`, AND THE DIFFERENCE IS THE WHOLE §2a PLOT RESOLVE — reading a gap as
> drift is the misreading this callout exists to stop.** The segments are the package's STORAGE; `plotBase` is
> the Σ of each worked plot's RESOLVED value, and `resolvePlotFlat` puts a whole composition between the two:
> the three floors (nature at 0, improvement at −nature, the total at 0), **the city-centre block**, **both
> scaling legs**, **the golden-age addend**, and the **MinCity** floor ([modifier.md §2](modifier.md)). None of
> those is a segment, because none of them is a stored deposit — each is read LIVE off the plot at resolve.
> ⚑ **So a gap is EXPECTED, and its ordinary size is one thing: the CENTRE plot's city block.** On a city with
> no scaling, no golden age and no biting floor, `plotBase − Σsegments` is exactly `CityChange + population /
> PopulationChangeDivisor` for that one tile — 41 on a production channel at population 81, 0 on food wherever
> the −1 constant and the ÷5 cancel. A reader who has not accounted for it measures that on every city and
> reads a systematic double-count.
> ⚠ **The earlier wording here named only the FLOORS**, which is what made the difference look like a defect
> class rather than the resolve. Attribute a gap against the full list above before suspecting the Σ.

⚑ **When the Σ genuinely IS wrong, `[MODIFIER] plotBaseFold` names the leg.** The
city's worked-plot Σ is maintained by several legs — the segment resolve, the city-plot re-resolve, the
owner-operand pass, the worked-membership fold — and a total that is merely wrong cannot say which of them put
the extra there. The line carries the LEG, the plot, the channel, the delta and the running total, and it is
emitted from the Σ's ONE write point (`applyPlotBaseFlat`), so a leg added later cannot escape it.
⛔ **The census gap above is NOT the trigger for reaching for it** — that gap is the resolve, not a drift. What
this answers is a `plotBase` that disagrees with what the FACTS should have folded.
⚠ Level 4, and bounded by WORKED plots rather than by the map — a fold only happens where a plot is in a city's
base — so it is a readable few tens of thousands of lines on a large save, not a per-plot firehose.

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
  `/computed` index pages are generated from it (`/state`'s list is empty today). ⚑ The table carries each route's
  own one-line doc, so it — not this page — is the census; what this page states is the SHAPE a route must have.
- **Query parameters are `?player=N`, `?city=M`, `?unit=K` and `?type=<INFOTYPE_NAME>`**, parsed once and threaded
  to the mailbox. ⚠ A route wanting a named entity uses `type=`; it does not mint a parameter of its own.

### ⚖ THE ENABLER'S TWO HALVES ARE TWO ROUTES — what a city HAS RUNNING vs what it is OFFERED

`/computed/enabler/operating` answers the first: the active / obsolete / provided set the targeted propagation
maintains. **That is only half the machine**, and for a long time the other half had no route at all — so the
GREYED tier ("go get copper") could be seen on a screen and nowhere else, which is precisely what the
[observability bar](../reference/observability.md) forbids.

- **`/computed/enabler/buildings`** — the VISIBLE tri-state per city: `listed[]` (may be started now) and
  `greyed[]`, **every greyed row carrying the REASON that refused it**, plus a `greyedByReason` histogram. It reads
  `EnablerDomain::inTreeIds` (LISTED + GREYED), which is what makes the greyed tier enumerable at all.
- **`/computed/enabler/verdict?type=BUILDING_X`** — ONE building's verdict in every city: `state`
  (HIDDEN / GREYED / LISTED), the gate `reason`, and whether the city already `has` it. This is the
  *"why can I not build this, and where"* read.
- **`/computed/enabler/units?type=UNIT_X`** — the UNIT twin, and deliberately RICHER, because the unit gate has
  legs a single reason cannot carry: `spawnOnly` · `obsoleteTech` · `capped` · `entityGateFail` ·
  `requiresFail` · `upgradeDormant` · `superseded` (+ `supersededBy`), each served beside the maintained
  `state`/`inTree`/`listed`. ⛔ It DECOMPOSES the maintained verdict and never recomputes it — the legs say why
  the tri-state reads as it does, they are not a second opinion about it.

⛔ **A verdict is served with its REASON, never as a bare state** ([enabler.md §6](enabler.md): the gate yields the
reason precisely so a greyed entry hands the asker an answer instead of a question). ⚠ The names are the enum's own
spellings from `EnablerDomain::stateName`/`reasonName` — diagnostic, not player text, so the reader sees exactly the
byte the enabler stored with no translation in between.
⚑ **The histogram is the useful read, and it is a correctness instrument rather than a listing:** a reason that
should HIDE appearing in the greyed set is a defect in the reason SELECTION, which is how hide-vs-grey is checked
without opening the game.

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
