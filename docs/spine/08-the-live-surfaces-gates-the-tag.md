# 8. The live surfaces — gates, the tag registry, the server, and the files

> Part of the **[spine](../spine.md)** spec.

### Gate knobs (the log levels)

- The four AI globals `gPlayerLogLevel` / `gTeamLogLevel` / `gCityLogLevel` / `gUnitLogLevel` are **aliases driven by
  the single `Autolog__LogLevelPlayerBBAI` BUG option**. `gPerfLogLevel` is independent. **`gStreamLogLevel`
  (default 1, `Autolog__LogLevelStream`) is its OWN independent gate, fully decoupled from `gPlayerLogLevel` / the
  file gate** — it is not a subset sitting on top of the file gate; a domain can stream at full volume while the
  file gate stays quiet, and vice versa (§2). `_DEBUG` forces all four AI globals to 4 — **inert in practice: the
  Debug config is not used** (it does not touch `gStreamLogLevel`).
- ⛔ **`gPlayerLogLevel` IS the gate for everything — the other three are FOUR NAMES FOR ONE NUMBER.** All
  four take the same value from the same option, so a per-scope name promises a per-scope knob that does not exist:
  **do not read `gUnitLogLevel` in some AI file as evidence that unit logging has its own tier.** A NEW gate reads
  **`gPlayerLogLevel`**.
  **⚖ The single-value collapse is a CONSCIOUS owner-ruled interim, NOT drift and NOT a deferral to close.** Giving
  each scope a real knob means BUG-UI work, which was deliberately declined against higher-value work — *"we just
  roll with `gPlayerLogLevel`"*. Its END CONDITION is the founding decree below: **ALL log events go through the
  EVENT SPINE**, so a CALL-SITE gate global is a legacy-of-a-legacy — once a domain emits, gating is the
  CONSUMER's business (the file consumer's level + `gStreamLogLevel`), and these four globals have nothing left to
  gate. They retire wholesale WITH the direct `gDLL->logMsg` / BetterBTSAI-helper call sites they guard, not by
  being tidied first. ⛔ **So do NOT "fix" this by collapsing the three names onto `gPlayerLogLevel`** — that is a
  sweep across a surface scheduled for deletion, and the ["deferred" is banned](../../AGENTS.md#design) reflex ("slated and never done ⇒ failure
  to fix") MISREADS it. The work is MIGRATING DOMAINS ONTO THE SPINE; the gates then disappear on their own.
- **Level semantics:** 1 = headline (`begin`/`best`/`decision`), 2 = per-decision (`score`/`order`/`act`), 3 =
  per-candidate (`cand`/`skip`), 4 = inner-loop (a genuine fire hazard — CTB emits 10k+ lines/turn at 4). Owner plays
  at 3.
- **⛔ A DIAGNOSTIC BUILT FOR A DEBUGGING SESSION MOVES TO LEVEL 4 WHEN THAT SESSION ENDS: diagnostics like this are not needed; 4 is where trace logs belong once debugging is finished.** The tiers above
  describe what a line COSTS; this says what it is FOR. **Because the owner plays at 3, anything at 1–3 is on
  during ordinary play** — so a trace kept at its investigation-time tier does not merely sit there, it runs
  forever, in every session, for a question nobody is asking any more.
  ⚑ **The instrument is KEPT, not deleted** — that is the point of moving it rather than removing it. It cost
  real effort to build, it is the thing that makes the same class of defect findable next time, and at 4 it is
  free until someone raises the gate. ⇒ Closing an investigation has a step: **re-tier its diagnostics to 4**.
  ⚠ The cost of skipping it is measurable, not theoretical: the `[GFX]` domain left at its investigation tiers
  (2 and 3) wrote a **133 MB** `Graphics.log` in one ordinary session — 77,245 `centerUnit` lines per 8 MB,
  including a full-map sweep of plots that draw no units at all — which is exactly the legibility loss the
  own-file rule below exists to prevent, arriving through the level gate instead.
- `OutputDebugString` is `#define`d to nothing under `FINAL_RELEASE` — it fires only in Release/Assert/Debug (any
  "fires in FinalRelease, CRIT" framing is wrong for the shipped build).

### ⛔ A domain gets its own log file — `Cascade.log` is not the default

> *"We really should stop having all these emits in the same file; `Cascade.log` is getting ridiculous as is."*

`spineRegisterDomain` already takes the file as a parameter, and passing **`NULL` routes the domain into
`Cascade.log`** — which is why they all ended up there. That is a per-registration choice, not a constraint:
name the file.

⚑ **The cost of not doing it is that the file stops being readable at the level you need.** A domain emitting at
level 3 buries a level-1 line from another domain in the same file, so turning the volume up on ONE question
costs legibility on every other — and the whole point of a spine-written file is that it is readable while the
game runs.
⚠ **This is not a sweep of the existing domains**, and it is not a backlog item: the ones sharing `Cascade.log`
keep working. It binds NEW domains, and an existing one moves when someone is already in it — the same
opportunistic disposition the contradicting-comment rule takes ([AGENTS.md](../../AGENTS.md) Conventions §Docs).

### Domain / tag registry

14 domains, each `[TAG]` prefix → log file → scope global → source: e.g. `[WAI]` → `BuildEvaluation.log` →
`gPlayerLogLevel` → `CvWorkerAI.cpp`; plus `[CIT]`/`[UNT]`/`[COM]`/`[WAR]`/`[CTB]`/`[ENG]`/`[PERF]`. `[PERF/reqmodel]`
passes when `mismatches=0`. `[INIT/*]` was renamed from `[GAME/*]` to avoid clashing with the `[STATE/game]` cascade
feed. Call-site census exists (WAI 43 sites, HAI 54, CTB 66, …). Dead sinks: `CB.log`, `C2C.log` (ruled DELETE).

### ⛔ The internal profiler is DEAD — the CENSUS is the perf surface

**Never use the internal profiler; never reinstate the `PROFILE_*` macro family.** The one attempt to ship it in
**Release** (behind a runtime gate) caused an allocation-failure crash on end-turn and was reverted the same day.

⚑ **The mechanism, because it is what makes the family kraken bait:** `PROFILE_FUNC`/`PROFILE` are live in the
Profile configs but no-ops in Release/Assert — *the same line behaves differently per config* — and the
`PROFILE_BEGIN` sites (including a per-FRAME one) call the sampler **directly, bypassing any scope gate**. So
compiling the profiler into Release ran those ungated, per frame, with a critical section per call. There is no
fix-and-reinstate plan: removal is the direction. ⛔ Do not add, un-gate, or Release-compile any `PROFILE_*` /
internal-profiler path.

**What we use instead** is the gated per-turn CENSUS teed to `/events`: call counts (operating-building recomputes
vs cache hits, rate/percent-stack/commerce computes, condition-evaluator leaf evals), ms accumulators, the
condition-eval CALLER split (which is what makes an outlier attributable rather than merely visible), the
enabler-frontier fill counts + ms, and the flush-to-flush whole-turn wall clock — the headline number
([turn time is king](../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)). Non-invasive, always shippable, and
sufficient.

**The process `memory` gauge** (`workingSetMB`/`peakWorkingSetMB`/`pagefileMB`, the CvPlotPaging
`GetProcessMemoryInfo` mechanism) splits a per-turn RAM climb (leak) from a one-time step (retained structure) —
load-bearing under the 32-bit ~3.2GB address-space ceiling. ⚠ Its `/computed/perf` route went with the route-table
purge; the gauge needs a surface again when the route table is rebuilt.

### The live HTTP server (today)

- Bind **`127.0.0.1:7227`**, GET-only, gated by BUG option `Autolog__HttpServer` (default **off**). Full transport
  shape, the mailbox/route-table invariants, and why there is no `oracle` side: [http-endpoints.md](../specs/http-endpoints.md).
- **The routes that answer** are four STORED-side documents, each serving what the events built, decomposed term
  by term: `/computed/cascade/packages` (per-scope flat/percent slots + receiver sums, by channel name),
  `/computed/enabler/operating` (the per-city operating set targeted propagation maintains), `/computed/city/yield`
  (the yield tooltip's own census — every term of the §2a combine, plus the REFUSED deposits with the atom that
  refused each one), `/computed/capabilities` (the empire ability union). Documents live in
  `Sources/Tools/CvStateEndpoints.cpp`, never in the server file.
- **The mailbox, concretely:** a data request runs on the **game thread** via a single-slot mailbox
  (`evalRequestBlocking` → `serviceEvalMailbox`, drained by `publishIfDue`); the server thread only renders the
  answer plus a tiny published `{turn,gameId}` header (refreshed every ~5 s). A second concurrent data request gets
  `503` — retry once.
- Predecessor surfaces (the flat `/units`/`/players`/`/cities` snapshot routes, the `/diagnostic/*` grab-bag, the
  cascade-vs-legacy `/shadow` sweeps) are gone and are not to be revived; the `/shadow` tombstone is
  [superseded-ideas](../architecture/superseded-ideas.md) #12.

### ⚑ `LSystem.log` — the EXE's own city-render log, and the one surface that shows ART FAILING

A THIRD kind of log sits beside the spine-written domains and the legacy `gDLL->logMsg` sinks: **the closed EXE
writes `LSystem.log` itself**, recording how it lays a city out from `XML/Buildings/CIV4CityLSystem.xml`. Nothing
in this repo emits it and no gate controls it, so it is readable like a spine log and answers a question no DLL
surface can: **what the render engine did with what we handed it.**

⛔ **THE RUNNING GAME HOLDS IT OPEN, so it reads like a spine log and is NOT one.** It is written by the
EXE, which puts it in the same class as the not-yet-migrated `gDLL->logMsg` sinks: a mid-session read is
PARTIAL, and the file keeps growing. ⇒ **Re-read it every time, and take any absolute count from a CLOSED game.**
⚠ **The trap is COMPARING two reads**, because the sizes are not stated anywhere in the numbers: a completed
session diffed against one still being written looks like a real before/after and is not. Compare RATIOS that a
truncation cannot explain (26,273 → 5), never totals — and re-read after the game closes before recording one.

⚑ **It is how an art gap becomes VISIBLE rather than merely suspected.** The lines that matter are
`Warning: building <id> is not associated with a CvCityLSystem node; it will not be visible!`, the
`does not contain a node called SHADOW` complaints (the engine loading `Art/Empty.nif` and trying to shadow it),
and `Failed to place goal building <ART_DEF>` / `Layout failed to complete while adding generic buildings!` —
i.e. the engine hunting for art that is not there, per layout rebuild, per city.
⚠ **The id is the building's RUNTIME INDEX**, so it reads as meaningless until resolved through the category's
`_order.json` manifest ([engine.md](../reference/engine.md) § Info loading) — resolve it before concluding anything
about which building is at fault.
⛔ A warning naming a building that HAS real art and real scale is an **art-XML** gap (the entity is missing from
`CIV4CityLSystem.xml`), which is the ART carve-out ([json.md](../specs/json.md)) — not a DLL defect. One that names an art-LESS building is ours: the city offered the engine
something it was never meant to place.

### The field census (event-spine migration input)

The exhaustive raw-field census: ~196 gated log templates across 10 domains, each field's name + cType + a sample
call-site. **Distribution:** ~80% int, ~15% string, ~5% typeIndex, ~3% float (PERF only); median 5–6 fields, ~85% fit
≤ 9, only 6 templates > 12. **Migration constraints:** wide `wchar_t*` strings can't travel raw on the spine — carry
entity IDs and let the consumer resolve names; `[STATE/dip]` is variable-width (scales with civ count); the `CTB`
pre-composed `CvString` criteria/joinInfo fields are the hardest to decompose; `[CIT/order] CONSTRUCT` score is an
`int64_t` outlier (needs a dual-slot / extended tag).

### PlotSnapshot — the one CSV surface

- Written at 4 call points (all from `CvGame`): `start` (new game), `load`, `regen`, `turn` (top of every `doTurn`,
  before AI decisions). File: `…/Beyond The Sword/Logs/PlotSnapshot_<tag>_t<turn>.csv`.
- **Rotation:** `turn` keeps only the last 3; `start`/`load`/`regen` wipe **all** other `PlotSnapshot_*.csv` — a turn
  file survives turn rotation but NOT a later start/load/regen (copy it out to keep).
- Uses raw `fopen`/`fclose` (gDLL holds handles open, blocking `remove()`); resolves `%USERPROFILE%\Documents\…` (not
  `SHGetFolderPath` — clashes with the `CATEGORY_INFO` macro), so it **fails silently under Documents redirection
  (OneDrive)**. Schema v2 includes the `animals` field (`<Type>@o<owner>c<combat>a<aggression>e<enemy>`) and the
  `improvementCurrentValue` `0 = uninitialised, not zero` caveat.

### Target consolidation

The migration target is one routing — `emit → CvEventSpine::dispatch → consumers` (§2): the eventSpine is the ONLY
place any "happening" lives, and everything downstream is a consumer of it — a founding decree. Concretely: **the BetterBTSAI log helpers (`logAIJson` et al.) are RETIRED — never route new work through them — and
every direct `gDLL->logMsg` inside Engine files is likewise unwanted**; each domain migrates by EMITTING spine
events (the field census above is the prepared input), whereupon it gains the file consumer, the `/events` stream
consumer, and the off-thread writer for free.

> **⚖ THE OLD LOGGING IS NOT A CLEANUP BACKLOG — IT IS A SURFACE YOU MUST NOT RELY ON TO FIND THINGS:**
> Removing the old logging is not a priority; it should simply not be relied on to find things, because
> ⇒ **The migration is DEMAND-DRIVEN, and the trigger is an INVESTIGATION, not a sweep.** The moment answering a
> question requires reading a legacy `log<Domain>AI` sink, that requirement IS the finding: the fact belongs on
> the spine and is not there. **Emit it** ([an event gap is closed the moment it is found](03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact))
> — the domain then gets the file consumer, the `/events` stream and the off-thread writer for free, and the legacy
> line beside it stops mattering whether or not anyone deletes it.
> ⛔ So do NOT plan, size, or schedule a wholesale conversion of the remaining call sites; a count of them is not
> a worklist. And do not read a surviving legacy line as debt to pay down — it is inert until someone LEANS on
> it, and leaning on it is the only thing that is actually banned.
> ⚑ **The test while debugging: "which surface answered my question?"** A spine-written domain (its
> `spineRegisterDomain` file — `Cascade.log`, `CityAI.log`, …) is the instrument working. A legacy sink is a gap
> report with your name on it.

Every DOMAIN event gets an assigned importance LEVEL as it migrates (levels today are only meaningful on the
DIAGNOSTIC side; DOMAIN defaults to 1). The multiplayer **OOS special logger is deliberately KEPT** — a
synchronization-debugging surface in its own right, and a natural future consumer of the synced DOMAIN stream.
Old anomalies slated for removal: dead `logCB`/`logToFile` Python exports (an arbitrary-file-write surface), the
`C2C.log` firehose, the `rjLogLine` split gate (a hardcoded level-1 tee), and the `BetterBTSAI.cpp:31`
`publishEvent("log")` tee (the retired BetterBTSAI log-helper family).

**The FILE sink is the off-thread `CvLogWriter`** (`Infrastructure/CvLogWriter.{h,cpp}`): the game thread renders
+ enqueues; a dedicated Win32 thread does all disk I/O and flushes per batch — **so spine-written log files are
READABLE WHILE THE GAME RUNS.** The held-open pain applies only to the not-yet-migrated `gDLL->logMsg` sinks (and
to the EXE-owned `LSystem.log` above) — never infer "logging is off" from a quiet legacy log. Files are truncated
fresh per session; lines stamp `[sec.mmm]` at enqueue.

