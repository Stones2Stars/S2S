# Event spine — spec (#430 cascade; design session 2026-06-17)

**Status: design-converged (owner thinking session 2026-06-17), pre-implementation.** The event spine is the
**front door** that sits in front of the tally (and the modifier's `grants`, and logging). Callers `emit` events
to it; consumers read the kinds they care about. This doc captures the converged model so we build against it.

Companion to `cascade-engine-430.md` (the engine plan), `tally-cascade-spec.md` (the count machine — the spine's
first authoritative consumer), and `modifier-cascade-spec.md` (§7 — the event-hook system that fires `grants` +
maintains the tally; the spine *is* that system, named).

---

## 1. The shape (one front door, explicit kind, selective consumers)

```
  caller ──emit(KIND, type, raw payload)──▶  EVENT SPINE  ──▶ consumers read the kinds they care about
                                                               • TALLY    : SELECTIVE — domain/state kinds → counts (authoritative)
                                                               • LOGGING  : BROAD     — sees all, OUTPUTS per the logging gates
                                                               • GRANTS   : the kinds that fire provisions
```

- **The spine is upstream of the tally.** The tally is a pure *consumer* — it reads the event kinds it cares about
  and never reaches into game state itself; the events come to it (resolves enabler-spec "the have-builder reports
  to the tally" — the report *is* an emitted event).
- **KIND is declared at the call site, never inferred** (the same "explicit, never infer" rule the cascade atoms
  follow). That one declaration does three jobs: routes the event, enforces the OOS firewall (§2), and tells each
  consumer whether it cares.
- **Two appetites, not two spines:** **logging is BROAD** (can see everything, outputs subject to the existing
  logging gates — the comprehensive observer); the **tally is SELECTIVE** (takes only the domain kinds it counts,
  ignores the rest). One front door; the consumers differ in what they take.

## 2. The OOS firewall — KIND splits synced from unsynced

The tally is **authoritative** (it gates what is buildable — `allowed` caps, `requires` count-thresholds), so in
multiplayer a tally computed differently/stale on one machine is a **desync**. The firewall keeps that safe:

| KIND | meaning | synced? | who consumes |
|---|---|---|---|
| **DOMAIN** | game STATE changed (building built, unit created, tech researched) | **yes — deterministic** | TALLY counts it (gate-eligible) + logging + grants |
| **DIAGNOSTIC** | CODE ran (a function entered, a decision re-evaluated N times) | **no — execution trace, differs per machine** | logging only (count for anomaly/threshold) — **never gates, never counted into the authoritative tally** |
| **TRACE** | fine-grained "show me every step" | no | logging only; **the tally ignores it entirely** |

**The bright line:** only `DOMAIN` events feed the authoritative tally / can gate. `DIAGNOSTIC`/`TRACE` are
unsynced and may be counted *for logging policy* but never cross into game state. The KIND is declared, so this is
enforced at the call site, not sniffed.

## 3. The Event — raw payload, never a pre-formatted string

```
Event = { KIND, type, <raw payload fields> }
```

- **`type`** — the data Type index (building/unit/…); the KIND axis stays tiny (the firewall axis), `type` carries
  the specificity.
- **payload = RAW fields** (ints, type-indices, plot ids, a damage value) — **never a pre-formatted string.** The
  costly part of logging is resolving indices to text + composing the line; keeping the payload raw defers all of
  that to the consumer. Both consumers take what they need from the same raw payload (the tally reads the count
  key; logging renders the fields).
- ✅ **RESOLVED 2026-06-18 (owner) — payload = a TYPED-FIELD ARRAY with PER-DOMAIN field resolution (as built).** From
  the Stage-0 field catalog (`../reference/logging-field-catalog.md`: 242 templates, median 5–6 fields, 85% ≤9, ≤16
  operational): `CvCascadeEvent` carries `CvCascadeEventField aFields[16]` (each slot `{int eTag; union{int i; float f;}}`,
  ~128 B) + an `int iDomainTag` + an `int iLevel`. **The slot's `eTag` is a DOMAIN-LOCAL field tag** — there is NO global
  field enum/registry; each domain registers (via `spineRegisterDomain`) its own prefix provider, `.log` file, AND a
  `SpineFieldInfoFn` that resolves its local tag → (name, `SpineFieldType`). The generic logging consumer renders
  `prefix + name=value …`; typeIndex `SpineFieldType` kinds (`SFT_BUILDING`/`UNIT`/`BONUS`/…) resolve the index to a type
  name. Raw (no strings: wide instance names → entity IDs; type names → the index + `SFT_<kind>`). This per-domain
  isolation (vs a global typed-tag enum) was the proper-once choice: zero shared edits per domain ⇒ parallel-safe + no
  3-way-sync debt. **`[PERF]` exception → its own `CvPerfEvent` struct** (later).
- **CLARIFIED 2026-06-19 (owner) — the rule is "the call site never COMPOSES the payload," not "no strings."** "Raw / no
  strings" above is an imprecise shorthand. The actual invariant: **the line is composed in the CONSUMER, one place, one
  gate** — the call site only hands over *ingredients*. So **passing an existing string POINTER** (`const char*`/`const
  wchar_t*` to a literal or a `szReason` already built for other logic) is allowed — it's an ingredient, not call-site
  composition, and it's pointer-cheap + lifetime-safe (synchronous game-thread render). What stays banned is the call site
  **building** the final line (`CvString::format(...)` per site). Consequence: instance-name strings travel as an **entity
  id** (consumer renders `name(id)`, the additive win — e.g. `SFT_PLAYER`, done); genuinely free-text strings can travel
  via a string-pointer field (`addStr`/`SFT_STR`, a build item) — so the CTB/free-text lines do NOT have to stay on legacy.
  Full ruling: §8 (the unrecoverable-lines thread).

## 4. NO verbose `if(loglevel)` gates (owner 2026-06-17) — and why they vanish

Today's scattered `if (gLogLevel >= N) { buildString(); log(); }` exists for ONE reason: to stop expensive
**string-building** from running when the gate is off. In the spine model the call site builds **no string** — it
emits raw fields in **one clean line** (`emit(KIND, type, a, b)`), and all formatting happens downstream **in the
gated logging consumer**. With nothing expensive at the call site, **there is nothing to guard** — the
`if(loglevel)` disappears *structurally*, not by discipline. That is the win: clean one-line call sites, no
boilerplate, and zero wasted formatting (it never runs unless a gate is on).

**Cost when gates are off:** building the cheap raw payload + a cheap "is anyone listening?" interest-check inside
`emit`. Domain events always have a listener (the tally needs them) so they always flow — cheap, and required
anyway. Diagnostic/trace have no tally interest, so they format only when their logging gate is on. The interest-
check is a precomputed per-KIND bool, so a dormant trace point costs ~a function call + a bool test. **Exception:**
an extreme high-volume trace point that must cost *nothing* when off can wrap a thin guard macro — the rare case,
not the rule. (⚑ verify VC7.1 variadic-macro support if we ever need that macro.)

## 5. Logging = superset of today, validated by shadow; trace stays a tier

- The spine-driven logging reproduces **every field the current channels emit** (`[WAI]`/`[CIT]`/`[DAI]`/`[HAI]`/
  `[UNT]`/`[PERF]`) **plus** what only the tally adds (per-event counts, per-turn frequency, threshold/anomaly
  flags). It is a **superset**.
- **TRACE remains its own tier** — a kind the logging consumer emits (gated) and the tally ignores; the deep
  "show me everything" capability is preserved, never routed into counting.
- ⚑ Pre-work: catalog the existing channels' fields so the payloads reproduce them exactly (then the line-diff can
  prove parity).

## 6. C++03 / VC7.1 — no lambdas, no Boost

- **No lambdas** (C++11). Consumers attach via a **virtual interface** `IEventConsumer { virtual void onEvent(const Event&) = 0; }`
  — interface-bounded (the north-star style), stateful (state lives in the consumer), pure C++03.
- **No Boost** (owner 2026-06-17 — avoid `boost::function`/`bind` until we have a handle on it). The spine is plain
  STL: `std::vector<IEventConsumer*>` for the registered consumers, the `CvScopedAccumulator` (`<map>`) for counts,
  a POD-ish `Event`. The substrate primitive (`Sources/Cascade/CvScopedAccumulator.h`) is already Boost-free.
  - **Footgun (do not trip):** the PCH (`CvGameCoreDLL.h`) pulls Boost in transitively and puts some names at
    global scope — a bare `bind` once resolved to `boost::bind` instead of winsock's (`CvHttpServer` lesson). We
    can't stop the PCH; we just never *name* Boost types and avoid generic identifiers like `bind`/`function`.

## 7. Shadow discipline — "do not break all the things" (owner 2026-06-17)

- **Old logging + old counters (`m_pai*Count`) stay live and untouched** as ground truth, gated off in normal play.
- The spine emits a **superset alongside** them; consumers migrate **incrementally**, one channel/call-site at a
  time, each **diffed** against the old (field-diff for logging, count-diff for the tally — the same shadow-verify
  the #195 enabler index used via gated `[PERF]`).
- The old machinery is removed **only at the atomic cutover** (the §4/§14 demolition step), never piecemeal during
  shadow. No big-bang.

## 8. Build order + consumers

The spine is the foundational piece **in front of** the tally, so it comes first among the consumers' shared
machinery (it + `CvScopedAccumulator` are the substrate). Then:

1. **spine** + `CvScopedAccumulator` — **DONE (slice 1, 2026-06-17):** `Sources/Cascade/CvEventSpine.{h,cpp}`
   (`EventKind`/`CvCascadeEvent`/`IEventConsumer`/`CvEventSpine` + `eventSpine()`), pure C++03/STL, allocation-free
   hot path + interest-guard. First consumer = the broad **logging consumer** (Cascade.log + the live `/events`
   tee). Proof emit in `CvGame::doTurn`. Compiles + links (Assert).
2. **tally** — first authoritative consumer; domain events → counts; shadow-diff vs `m_pai*Count`.
   **(STATUS 2026-06-19: PARTIAL — built for buildings+units only, player-leaf; see cascade-engine-430 §Implementation Status.)**
3. **logging** consumer — broad/gated; reproduces channel fields + counts; shadow-diff vs the old lines.
   - **PROGRESS 2026-06-18 — the RAW-FIELD logging contract + the HAI pilot are IN (Assert-clean, shadow):**
     `CvCascadeEvent` gained the raw-field payload (`CvCascadeEventField aFields[16]` + `addI`/`addF`), a per-line
     `iLevel` (the consumer gates on the 0–5 surveillance scale), and a **per-domain prefix-provider registry**
     (`spineRegisterDomain`/`SpineLinePrefixFn`/`SpineFieldInfoFn` — domains self-register their `[TAG]` prefixes,
     `.log` file, and field-tag resolver, so the spine stays domain-agnostic). The logging consumer renders field events
     as `[PREFIX] name=value …` via `cascadeRenderEventLine` + the domain's registered field resolver. **Per-domain FILE
     routing DONE (R-2):** the registered `.log` file routes each domain's field lines (e.g. `[HAI]` → `HunterAI.log`).
   - **`[HAI]` FULLY MIGRATED (Assert-clean, shadow):** all ~24 line templates / 28 sites in `CvHunterAI.cpp` emit through
     the spine ALONGSIDE the legacy `logHunterAI`, level-tagged (1–2), routing to `HunterAI.log`. HAI self-registers its
     `[HAI/...]` prefixes (the spine never names HAI). Free-text lines were recast to clean `[TAG] key=value`
     (e.g. "merge with hunter escort" → `action=mergeEscort`) per the recategorize-freely ruling.
   - **Next:** verify the shadow live (`/events` diff old vs new `[HAI]`), then **CUT** the legacy `logHunterAI` calls
     (and retire `logHunterAI` + its file plumbing); then roll the other domains (CTB last) per
     `../reference/logging-surface-inventory.md`. NB the `[HAI]`/`HunterAI` name-collision rename is a flagged separate dragon.
   - **BULK MIGRATION 2026-06-18 (parallel fan-out, Assert-clean):** the contract was refined to **per-domain field
     resolvers** (each domain registers its own field name/type table → the spine holds NO global field registry →
     fully isolated + parallel-safe; killed the 3-way-sync debt) and HAI+WAR re-pointed. Then 7 agents shadow-wired the
     remaining domains: **`[WAI]` `[CIT]` `[UNT]`+`[COM]`+`[GRP]`+`[FND]` `[DAI]` `[DIP]`+`[ESP]` `[CTB]` `[ENG]` —
     110 sites, ~133 event ids, routing to their own `<Domain>AI.log`.** Multi-file `[CIT]` needed a shared tag-enum
     header (`CvCityLogTags.h`) to dodge the unity-batch enum-redefinition (the one compile fix); all other domain enums
     are single-file. **~52 lines left on legacy** (runtime name/desc strings — the raw-field model has no string slot).
   - **NAME-RECOVERY pass DONE 2026-06-18 (Assert-clean):** the renderer gained the full typeIndex set (`SFT_BONUS`/
     `IMPROVEMENT`/`PROMOTION`/`RELIGION`/`CORPORATION`/`FEATURE`/`TERRAIN`/`CIVIC`/`PROJECT`/`SPECIALIST`), so a former
     `"=%s", GC.getXInfo(eX).getType()` line now travels as the int index + `SFT_<X>` and the consumer prints the name.
     **21 lines recovered** (WAI near-fully covered: 20 event ids / 28 field tags; CTB `CF_unitType`→`SFT_UNIT`).
   - **Of the 31 "unrecoverable" lines, ~25 are now RESOLVED (owner 2026-06-19) — carry the raw ID + resolve the live name,
     render `name(id)` (BOTH).** These are runtime INSTANCE display names (`pCity->getName()`, a player/unit name). Ruling:
     travel the **entity ID** as a raw field (`SFT_PLAYER` and future `SFT_CITY`/unit-instance tags) and render `name(id)` —
     **both the human name AND the raw id**, additive (owner: *"if I want one thing in the logs it doesn't mean exclude the
     other if useful"*). The name gives readability; the **raw id is the stable machine join-key** for the real primary
     consumers — **AI agents** reading during shadow-verify, and **GameTracker** parsing `/events`.
     - **Why live resolution is EXACT and SAFE (verified 2026-06-19):** `CvCascadeLogConsumer::onEvent` renders the line
       SYNCHRONOUSLY on the GAME thread at emit time (`cascadeRenderEventLine` is called inside `onEvent`); only the finished
       STRING is teed to the `/events` server thread. So resolving a live name during render touches no live object
       off-thread (snapshot isolation HARD CONSTRAINT intact) AND captures the name as-of-emit (a later rename does not
       retro-alter past lines — that is correct, not a discrepancy). This **supersedes** an earlier draft's "resolve from a
       snapshot COPY + tolerate staleness" framing — unnecessary, since render is synchronous on the game thread.
     - **IDs are stable across save/load (verified):** `m_iID` is serialized by the name-tagged wrapper for `CvUnit`
       (`CvUnit.cpp` `WRAPPER_READ/WRITE "CvUnit" m_iID`) AND `CvCity` (`CvCity.cpp` ditto) — so keying on the raw id is
       valid even across a reload.
     - **NAME-CHANGE event — REQUIRED for the full Orwell bar (owner 2026-06-19, upgraded from "fallback"). IMPLEMENTED
       2026-06-19 (Assert-clean).** A rename (city/unit/player/civ) IS an observable STATE CHANGE: an out-of-process consumer
       (GameTracker / an agent) that maps the game purely from `/events` + logs must SEE it to keep its id→name table
       accurate. So a **`DOMAIN` event `CASCADE_EVT_NAME_CHANGE`** (`iType=NameChangeKind, iA=owner, iB=entity id`) is emitted
       from the four set-name choke points — `CvPlayer::setName` (`kind=player`), **`CvPlayer::setCivName` (`kind=civ` — the
       empire name)**, `CvCity::setName` (`kind=city`), `CvUnit::setName` (`kind=unit`) — via the `cascadeEmitNameChange()`
       helper. **String-free payload:** the logging consumer resolves the NEW name LIVE on the game thread (exact) and renders
       `[SPINE/DOMAIN] nameChange kind=… player=… id=… name=…` to `Cascade.log` + `/events`; the tally ignores it (switch
       default). It is the event-sourced complement to the inline `name(id)`: inline gives the name at each line; this lets a
       consumer rebuild the mapping and resolve any historical id even after a rename.
       - **Doubles as a bug lever (owner 2026-06-19):** likely helps resolve a year-long bug — *empire names not updating when
         a civic changes* (with that game option on, dynamic civ names should refresh on civic switch and don't). The
         `kind=civ` emit fires from `setCivName`, so a civic switch that SHOULD rename the empire but emits no `kind=civ` line
         pinpoints whether the refresh fires at all — the observability work and the bug hunt share one hook.
     - **Refinement:** a unit's *default* `getDescription()` IS its type name, already carried cleanly via the `SFT_UNIT`
       type-index — so only *custom-named* units and *player/city* names need the id-resolve path.
   - **The real rule is WHERE THE PAYLOAD IS COMPOSED — in the CONSUMER, not the call site (owner 2026-06-19 — the true
     rationale behind §3/R-1; "no strings" was an imprecise shorthand).** The old way built the final line *at every call
     site*, which forced (1) an `if`-gate at each site, (2) an eager full-payload build before knowing if it would even be
     logged, and (3) the format logic DUPLICATED across every individual call — so you had to *pray each site stayed
     consistent* (N copies of the truth, easy to drift). The spine moves composition to ONE place: the call site hands over
     **ingredients** (raw ids, or a pointer to an already-existing string); the **consumer composes the line** — one gate,
     one format, structural consistency. So **passing an EXISTING string is fine** — a `const char*`/`const wchar_t*` POINTER
     (a literal, a `szReason` already built for other logic, a member name) is a pointer store (no concat, no alloc), and the
     call site is still just handing an ingredient to the consumer, NOT composing the payload. Lifetime is safe because the
     consumer renders SYNCHRONOUSLY on the game thread (the pointer is valid at render; `/events` only sees the finished
     string). The ONLY thing still rejected: the call site composing the final line itself — a pre-FORMATTED full log line
     (R-1's Option 2, "like it used to be").
   - **So the ~6 free-text lines NO LONGER need enum-ification (owner 2026-06-19). CAPABILITY IMPLEMENTED + DEMONSTRATED
     2026-06-19 (Assert-clean).** `CvCascadeEvent` gained `addStr`/`addWStr` (the field union carries a `const char*`/`const
     wchar_t*` POINTER — POD, 8B on x86) + `SFT_STR`/`SFT_WSTR` render cases (null-guarded `%s`/`%S`). The call site passes
     the existing `szDecision`/`szReason`/… pointer; the consumer assembles the `[TAG] key=value` line. **First user:
     `CvUnitAI::AI_logAct` → `[UNT/act]`** (was legacy-only precisely because of its free-text `decision`/`reason`; now
     shadow-emits via the spine with `decision`/`reason` as `SFT_STR`). (Use string fields ONLY for genuinely free-text data
     with no id/type to resolve — type/instance data still travels as a raw id so the consumer renders `name(id)`, the
     additive win.) Remaining free-text lines (CTB `szWorkCriteria`/`szCriteriaDescription`, DAI) migrate the same way when
     their domains are swept. Plus 3 CTB lines that just want a `SFT_UNITAI` render-type tag (trivially addable).
   - **Cleanup is MIGRATE-not-blanket-delete (owner 2026-06-19):** during the R-4/R-5 firehose cleanup, genuinely-valuable
     lines are MIGRATED to their correct home (agent judgment — *the agent is the primary log reader during the shadow
     passes*), not deleted. First instance: the `AI_doDiplo` war-ally-purchasing reasoning (ex-`C2C.log`) → `[DIP/warally]`.
   - **Trade/deal AI is broadly buggy (owner 2026-06-19) — EXPAND trade logging LATER, when-needed.** `[DIP/warally]` is a
     first slice; deeper `AI_doDiplo` / `CvDeal` trade-path tracing is a deferred enrichment we add *when actively debugging
     trades*, not pre-emptively. (So the ex-`C2C.log` "traded contact for gold" + "AI unit trade value" lines stay deleted
     for now — actual deals are already covered by `[DIP/trade]`.)
   - **Remaining threads:** the `name(id)` id-resolve path rollout for the ~25 (build item, `SFT_PLAYER` landed first); the
     `szReason`-family enum-ify decision; the 3 `SFT_UNITAI` CTB lines; `[DAI]` civ/flavor wide-strings (nominal);
     per-domain legacy CUT after `/events` verification; and the Autolog BUG-option rework to the 0–5 Surveillance knob.
4. **grants** — fires provisions on its kinds. **(STATUS 2026-06-19: NOT BUILT — no `grants` consumer is registered.)**
5. **modifier**, then **enabler** (read the tally) — per `cascade-engine-430.md`.

## 9a. Observability — `CvHttpServer` is the FORMAL live layer (owner 2026-06-17)

`CvHttpServer` is **not an experimental bolt-on** — it is the cascade's formal **live-observability / query layer**,
and consumers publish to it. (Full endpoint/gating/architecture reference:
[`../reference/http-server.md`](../reference/http-server.md).)

- **Assessed: it needs NO redesign.** It already has the right architecture for this role — publish-and-serve with
  **snapshot isolation** (the server thread never touches game objects), a **bounded** event queue
  (`EVENT_QUEUE_CAP = 65536` since 2026-06-18, bumped from 2048 so level-3 `/events` isn't lossy; ~10-16MB worst-case
  transient on the LAA process, and it **drains even with no client** so it can't bloat),
  the `/events` SSE stream, and the **#419 live-log mechanism** (raw gated log lines teed onto `/events` for
  out-of-process parsing — "the counter-strike way"). The owner's original cost worry (CPU/MAF/bloat) proved
  unfounded; it's already 32-bit-safe. So we **extend + formalize**, not rebuild.
- **The spine streams through the shared `streamLogTee`** (`BetterBTSAI.{h,cpp}`, promoted from file-local `static`
  to a shared public tee 2026-06-17): the BBAI log helpers AND the spine's logging consumer both call it, so a log
  line goes to the live `/events` stream via one canonical path (gated by `gStreamLogLevel`). Post-cutover the spine
  is the central logging path and that tee lives in one place.
- **The tally will expose a `/tally` snapshot endpoint** the same publish-and-serve way (like `/cities`) when it lands —
  **PLANNED, not yet built** (live routes today are only `/`, `/units`, `/players`, `/cities`, `/events`). Tally DOMAIN
  *emits* are already observable via `/events` (the `[SPINE/DOMAIN]` `log` frames); the missing piece is the snapshot GET
  of current aggregated counts. It must publish from the game thread (never read `cascadeTally()` on the server thread —
  the snapshot-isolation HARD CONSTRAINT). See [`../reference/http-server.md`](../reference/http-server.md).

## 9b. SPEED + the 32-bit ceiling (owner 2026-06-17) — non-negotiable

Speed is a given; the harder constraint is **memory on a 32-bit (LAA ~4GB) process** — avoid bloat / allocation
failure. The design holds to it:

- **Allocation-free hot path:** `CvCascadeEvent` is a small POD passed by const-ref (no heap per emit); the
  interest-guard makes a dormant DIAGNOSTIC/TRACE firehose ≈ one bit-test; the logging consumer formats into a stack
  buffer. No per-event `new`.
- **Bounded observability buffers:** the `/events` queue is capped + self-draining (above). No ever-growing
  in-memory log.
- **Compact counts:** the tally weighs sparse (`std::map`) vs dense (`int[]`) per domain against memory
  (per-node overhead × scopes × players adds up on 32-bit) when it lands — pick per domain.

## 9. Open / flagged

- ⚑ Event payload concrete representation (self-describing field set vs tagged union vs per-kind struct) — §3.
- ⚑ KIND taxonomy beyond the firewall axis (do we need domain sub-kinds, or does `type` carry it?) — lean: keep
  KIND tiny, `type` carries specificity.
- ⚑ Whether `emit` dispatches push (spine → consumer `onEvent`) or consumers pull a queue — lean push (direct
  virtual call), simplest; revisit only if a queued/deferred pass is needed (e.g. the future parallel read).
- ⚑ The catalog of existing logging-channel fields (the §5 superset target).
- ⚑ VC7.1 variadic-macro support, only if the §4 extreme-firehose guard macro is ever needed.
