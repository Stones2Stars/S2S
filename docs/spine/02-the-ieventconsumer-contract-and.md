# 2. The `IEventConsumer` contract and the C++ shape

> Part of the **[spine](../spine.md)** spec.

Consumers attach through **one C++03 interface, `IEventConsumer`** (a pure-virtual base, no data members) — the
`grants` and logging are independent implementations pluggable behind it (the realized exemplar of the project's
[interface-contract pattern](../architecture/patterns.md)); the [tally](../specs/tally.md) is **not** a consumer (it reads
objects). **Build order:** spine + the modifier scope accumulator → logging (broad) → grants → [modifier](../cascade.md)
→ [enabler](../specs/enabler.md). *(The tally is a read-only accessor, not a step on the spine.)*

`CvEventSpine.{h,cpp}` (`Sources/Spine/`) is the concrete shape:

- **`CvSpineEvent`** is a POD carrying **two payloads, not two exclusive modes**: the raw **DOMAIN state ints**
  (`iType`/`iA`/`iB`/`iC` + `iSrcLoc` = WHERE), which `grants` and the cache-invalidation consumer read; **and** the
  **render payload** (`iDomainTag`/`iEventId`/`aFields[]`, `SPINE_MAX_FIELDS = 16`; a field is `{int eTag; union{int
  i; float f; char* s; wchar_t* w;}}`, 8B/POD) that the one logging path formats.
  ⛔ **THE FIELD CAP IS A SILENT CEILING — `addI`/`addStr` DROP a field past 16 rather than failing.** So a
  census line that has grown to exactly 16 cannot be extended at all: the seventeenth term is simply absent from
  the rendered line, and absent reads identically to zero. ⚑ Check a line's field count before adding a term,
  and when it is full the answer is a SECOND event, never a swap of one term for another — which is the right
  shape anyway wherever the new term has an axis of its own (a per-type row is not a term). *(`[MODIFIER] rateRead`
  stands at exactly 16, which is why the `specialists` decomposition is its own line.)* A **`DOMAIN`** event carries
  BOTH — its state ints for the machine consumers **and** a domain tag + fields so it renders through the same
  registered path as everything else; a **`SAVELOAD`/`DIAGNOSTIC`/`TRACE`** event carries only the render payload.
  There is no inline-formatted event: the spine's own DOMAIN events register under `SD_SPINE` exactly like an AI
  domain.
- **Per-domain isolation:** a domain registers via `spineRegisterDomain` (a line-prefix fn + a field-info fn with
  typed index kinds `SFT_BUILDING`/`UNIT`/`BONUS`/…); `spineRenderEventLine` formats. **Zero global field registry,
  zero shared edits per domain** — adding a domain touches only that domain. **The logging consumer is exactly
  `gate(iLevel) → spineRenderEventLine → write`** — no per-event branch, no inline `sprintf`; a line's identity is
  entirely its registered prefix + fields. Every rendered line carries the game turn as its first field
  (`[TAG] t=NNN …`) — after the tag so prefix-anchored greps keep working — making each line self-placing in
  time (when did this actually fire) instead of inferred from burst position. Passing `NULL` for the file routes a
  domain into `Cascade.log` — a per-registration choice, not a constraint (§8).
- **The `/events` STREAM is its OWN registered consumer** (`CvSpineStreamConsumer`) — never a tee inside the
  logging consumer (that chained stream visibility to the FILE gate, so a quiet `gPlayerLogLevel` silently starved
  the stream). **DOMAIN events stream UNCONDITIONALLY** whenever the HTTP server is up (the facts the machine
  consumers see — the out-of-process replay feed); SAVELOAD/DIAGNOSTIC/TRACE lines stream at the stream's own
  verbosity knob (`gStreamLogLevel` / `Autolog__LogLevelStream`), **fully decoupled from `gPlayerLogLevel` /
  the file gate** — streaming everything never requires opening the level-4 file firehose, and turning the file
  gate up or down changes nothing about what streams. ⚠ SAVELOAD is deliberately NOT on the unconditional side:
  the load record is the highest-volume stream in the engine and would exhaust the bounded SSE slots during
  ordinary play. The SSE queue is capped; on overflow the first frame that fits again reports `[STREAM] dropped=N`
  — a gap is always visible as a gap, never silent. (The transport's own bound — **≤ 8 concurrent stream slots**,
  `503 {"error":"too many event streams"}` beyond that — is [http-endpoints.md](../specs/http-endpoints.md).)
- **Interest guard:** an `m_iInterestMask` bit-test gates dispatch, so the verbose call-site `if(logLevel)` gates
  vanish structurally.
- **Allocation-free hot path** (stack-buffer formatting, a bounded `/events` queue) — 32-bit ceiling discipline.
- **Name-change event** (`SEVT_NAME_CHANGE`): the four set-name choke points emit `(NameChangeKind, owner,
  entity_id)` in the DOMAIN ints (an out-of-process consumer keys on those). Because the logging consumer is generic,
  the `emitNameChange` endpoint resolves the NEW name + kind LIVE and passes them as render fields (`SFT_STR` kind +
  `SFT_WSTR` name — the emit render is synchronous on the game thread, so the borrowed pointers outlive it). This is
  the one place a spine endpoint does resolution at emit rather than deferring to the gated render — justified
  because a rename is rare (four low-frequency choke points), not a hot-path firehose.

**The spine primitive, KIND firewall and `IEventConsumer` live in `Spine/CvEventSpine.{h,cpp}`.** The **DOMAIN
emit surface** sits at the genuine mutation choke points across `CvPlayer`, `CvCity`, `CvPlot`, `CvUnit`, `CvGame`,
`CvProperties`, `CvTeam`, `CvArea`, `CvMap`, `CvPlotGroup`. The PLOT substrate is complete: terrain / feature /
improvement / route / bonus / owner / **type / river / irrigation / landmark / worked**, so the per-scope contexts
are maintained purely by facts, with no choke point driving a derivation directly
([contexts.md](../cascade.md)).

