# Decisions ledger — the canonical, ID'd home for cross-cutting rulings

> **Why this file exists (owner ruling 2026-06-19).** Cross-cutting rulings kept getting re-stated in
> doc after doc. The causal loop: the capture-immediately mandate is correct → but compaction wipes the
> agent's memory that it ever read the ruling → and there is no discoverable canonical home, so
> "is this already recorded?" is unanswerable → so the agent re-adds it defensively → and each re-add
> makes the *next* agent's existence-check even harder (now it's in 5 places with wording drift, none
> authoritative) → re-add again, *ad infinitum*. The duplication destroys the discoverability whose
> absence caused the duplication.
>
> **This ledger breaks the loop.** It is an **INDEX, not a re-statement**: one stable ID per ruling, a
> one-line summary, and a pointer to the authoritative home. The capture mandate is satisfied by adding
> a line *here* (cheap, one grep to check it isn't already present), not by restating the ruling inside a
> spec. Specs reference `[[DEC-id]]` instead of re-articulating. Three failure drivers die at once:
> the mandate has a cheap home; compaction can't hurt because this index is the always-checked entry
> point; and "does this ruling already exist?" becomes one grep against the IDs below.
>
> **Rules for this file.** (1) Before adding a ruling anywhere, grep this index first. (2) An entry is a
> *pointer*, never a second copy of the ruling — full text lives in the Home doc; only rulings with no
> other natural home carry their text here (marked **Home: this file**). (3) When a doc currently restates
> a ruling that has an ID here, replace the restatement with a `[[DEC-id]]` link in the dedup sweep. (4)
> Convert relative dates to absolute. (5) Workflow/convention rulings stay homed in `AGENTS.md`
> Conventions — this ledger only *indexes* them so they are greppable alongside the design rulings.
>
> **Scope/branch note.** Most entries below index cascade specs that live on `json-data-migration`; this
> ledger is therefore branch-coupled and edited in the working tree only (no commit unless asked), per the
> AGENTS.md docs-commit rule. Workflow entries (DEC-WF-*) point at `AGENTS.md`, which is not branch-coupled.

---

## Index (grep this first)

| ID | One-line | Home |
|---|---|---|
| [DEC-fixedpoint-x100](#dec-fixedpoint-x100) | All cascade value math is integer ×100; the single human→int conversion lives only in readJson | `reference/cascade-fixed-point.md` §0–1 |
| [DEC-per100-closed-set](#dec-per100-closed-set) | The legacy per-100 fields are a CLOSED set of exactly 6 `…100()` accessors; that is the curator's whole de-scale list | `reference/cascade-fixed-point.md` §2 |
| [DEC-curator-owns-descale](#dec-curator-owns-descale) | The curator absorbs ALL legacy per-100-vs-normal ambiguity once; JSON is uniformly human, readJson re-applies ×100 | `reference/cascade-fixed-point.md` §0 |
| [DEC-no-guessing](#dec-no-guessing) | Never hypothesize a divergence cause; EMIT the full decomposition and attribute it to a named source — if the data isn't emitted, emit it first | `AGENTS.md` (Key Subsystem Knowledge) |
| [DEC-map-before-delete](#dec-map-before-delete) | You cannot delete a maintainer you cannot fully observe; every state behaviour gets a shadow diffing cascade vs engine until clean, THEN the legacy is cut | `plans/cascade-mapping-inventory.md` §A; `AGENTS.md` |
| [DEC-parity-not-goal](#dec-parity-not-goal) | Parity is not the goal; ±10% is NOT "parity-adjacent" — the bar is sharper than legacy | `plans/modifier-cascade-known-discrepancies.md` §A.1 |
| [DEC-tally-serializes-nothing](#dec-tally-serializes-nothing) | The tally + scope accumulators serialize NOTHING — rebuilt from authoritative loaded objects on load | `plans/tally-cascade-spec.md` §9 |
| [DEC-save-remove-is-soft](#dec-save-remove-is-soft) | Removing a serialized field/Type is a SOFT change in the name-keyed format; only 4 enumerated cases are HARD | `reference/save-load-format.md` |
| [DEC-derived-never-trusted](#dec-derived-never-trusted) | Derived data is never trusted from a save; reset() marks it dirty and recomputes from live state | `plans/derived-data-repository.md` |
| [DEC-obs-scale](#dec-obs-scale) | The Observability Scale (0 Oblivious … 5 Meta) + the reconstruct-from-API "Orwell" bar | `reference/observability/README.md`; `plans/cascade-mapping-inventory.md` §D |
| [DEC-obs-hook-shapes](#dec-obs-hook-shapes) | The three canonical observability hook shapes (snapshot field / gated `[TAG]` log + streamLogTee / mailbox `/diagnostic`) | `reference/observability/README.md` |
| [DEC-interface-contracts](#dec-interface-contracts) | Clean-Architecture contracts: abstract base + pure virtuals, MI = `implements`, poor-man's-DI `if`/switch at a composition root; graft onto DLL-derived classes, never EXE-bound bases | `AGENTS.md` (Conventions) |
| [DEC-proper-once](#dec-proper-once) | Build the proper structure once — reject transitional shims that only defer the real design | `AGENTS.md` (Conventions) |
| [DEC-WF-pwsh](#dec-wf-pwsh) | `pwsh` good / `powershell.exe` bad; Bash tool equally fine | `AGENTS.md` (Conventions) |
| [DEC-WF-read-gate](#dec-wf-read-gate) | The doc-read before touching a subsystem is mechanically gated, not exhorted; ALL docs every session until the owner declares the codebase under control | `AGENTS.md` (Conventions) |
| [DEC-WF-rulings-to-repo](#dec-wf-rulings-to-repo) | Every owner ruling goes into the repo docs immediately, unprompted, in the same work item | `AGENTS.md` (Conventions) |
| [DEC-WF-no-commit-unmandated](#dec-wf-no-commit-unmandated) | Only branch/commit/PR when tied to an active issue; otherwise edit the working tree only; never switch branches mid-build | `AGENTS.md` (Conventions) |

---

## Entries

### DEC-fixedpoint-x100
All cascade value math is integer fixed-point ×100 (2 decimals); JSON is human-readable; the **single**
human→integer conversion + percent semantics lives **only in readJson**; the cascade calculator never
checks or knows about ×100. A ×100 value appearing in a JSON file is a curator bug.
**Home:** `reference/cascade-fixed-point.md` §0–1 (owner-LOCKED 2026-06-19).
**Currently restated in:** `plans/modifier-cascade-spec.md` §2; `plans/cascade-engine-430.md` §5;
`plans/calc-emulator-spec.md` — replace with `[[DEC-fixedpoint-x100]]` in the sweep.

### DEC-per100-closed-set
The legacy per-100 fields are a CLOSED, verified set of exactly six `…100()` accessors:
`getTechYieldChanges100` + `getTechCommerceChanges100` (Building), `getEraCommerceChanges100` (Heritage),
`getExtraUpkeep100` (Promotion + UnitCombat), and `getTotalModifiedCombatStrength100` (computed — nothing
to de-scale). That set IS the curator's entire de-scale checklist; figure a field's scale from the math,
not the name.
**Home:** `reference/cascade-fixed-point.md` §2.
**Currently restated in:** the curator-descale handover; `plans/building-cascade-conversion.md` §7;
`reference/legacy-value-calc-map.md`.

### DEC-curator-owns-descale
XML→JSON happens once, finally. The curator absorbs ALL legacy per-100-vs-normal mixing in that single run
and emits uniformly human numbers; readJson then has ZERO per-field scale knowledge (a blanket ×100). The
per-field scale map is a curator-only, used-once concern that must not leak into readJson or the cascade.
**Home:** `reference/cascade-fixed-point.md` §0–0.1.

### DEC-no-guessing
We do not guess a divergence's cause and try a fix. We MAP: emit the full legacy decomposition (every
component/source) via the dump, map the cascade's value by the same components, and attribute the
divergence to a NAMED source with numbers. If the data to attribute it isn't being emitted, the FIRST step
is to emit it (extend the dump), not to guess.
**Home:** `AGENTS.md` — "THE NO-GUESSING RULE" (owner ruling 2026-06-19).

### DEC-map-before-delete
You cannot safely delete a maintainer you cannot fully observe. Every state behaviour gets a SHADOW that
diffs the cascade's verdict against the live engine, turn over turn, until clean — *then* the legacy
mechanism is deleted. This is why total observability (the "Orwell" bar) is load-bearing, not polish.
**Home:** `plans/cascade-mapping-inventory.md` §A; `AGENTS.md` (Cascade observability).
**Related:** [[DEC-obs-scale]], [[DEC-no-guessing]].

### DEC-parity-not-goal
Matching legacy is not the goal; the cascade may deliberately diverge where the spec says so. ±10% is NOT
"parity-adjacent" — the acceptance bar is sharper than the legacy number. Residual divergence localizes the
next mis-scaled field or un-wired source (the offline tester is the check).
**Home:** `plans/modifier-cascade-known-discrepancies.md` §A.1 (PART-2).

### DEC-tally-serializes-nothing
The #428/#430 tally and scope accumulators serialize NOTHING — they are rebuilt from the authoritative
loaded objects on load. Anything genuinely NOT recomputable (a true lifetime/historical counter) is owned
and saved on its owning object, never in a derived/aggregation module.
**Home:** `plans/tally-cascade-spec.md` §9 (LOCKED).
**Related:** [[DEC-derived-never-trusted]], [[DEC-save-remove-is-soft]].

### DEC-save-remove-is-soft
The save format is name-keyed and self-describing, so removing a plain serialized member is a SOFT change
(orphan tag drained on object close), and Type/XML churn is effectively free for class enums/arrays. Only
four cases are genuinely HARD: changed-meaning-under-same-name, a still-needed Type deleted without
`_ALLOW_MISSING`, a shrinking legacy raw (non-class) enum-indexed array, and a type-code change under a
reused name.
**Home:** `reference/save-load-format.md`.

### DEC-derived-never-trusted
Derived data is never trusted from a save: each owner's `reset()` marks derived data dirty on load and
recomputes it from live state. So derived state has nothing to migrate, and deleting its serialization is
the intended pattern.
**Home:** `plans/derived-data-repository.md`.
**Related:** [[DEC-tally-serializes-nothing]].

### DEC-obs-scale
The Observability Scale: 0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police ·
5 Meta. The reconstruction ("Orwell") bar: reconstruct full game state from HTTP endpoints + `/events` SSE
+ gated logs alone, never from the screen.
**Home:** `reference/observability/README.md` (top); `plans/cascade-mapping-inventory.md` §D.
**Currently restated in:** every per-system observability map (each "Tier assessment" + intro) — replace
the restatement with `[[DEC-obs-scale]]` in the sweep.

### DEC-obs-hook-shapes
Climbing a tier uses one of three canonical, cheap, gated hook shapes: (1) a read-only field on the
`/players`|`/cities`|`/units` snapshot (game-thread copy); (2) a gated `[TAG]` log line (by
`gPlayerLogLevel`/`gCityLogLevel`/`gTeamLogLevel`/`gUnitLogLevel`) teed to `/events` via `streamLogTee`;
(3) an on-demand mailbox `/diagnostic/*` endpoint (the `canConstruct`/`placementSweep` pattern,
game-thread-serviced).
**Home:** `reference/observability/README.md`.
**Currently restated in:** every per-system observability map §4 — replace with `[[DEC-obs-hook-shapes]]`.

### DEC-interface-contracts
The architectural north star is Clean Architecture: depend on interfaces, not concretions. In C++03 an
interface = an abstract base with only pure-virtuals + a virtual dtor and NO data members; multiple
inheritance is the `implements IA, IB` mechanism (only of stateless pure-virtual bases); there is no DI
container, so wiring is "poor-man's DI" — an `if`/`switch` at a composition root assigns the concrete to a
contract pointer. Graft interfaces onto DLL-internal derived classes (`CvCityAI`/`CvUnitAI`/…), NEVER widen
an EXE-bound base (`CvCity`/`CvUnit`).
**Home:** `AGENTS.md` (Conventions — architectural north-star).

### DEC-proper-once
Build the proper structure once. Reject transitional shims that exist only to defer the real design; do the
prerequisite work and build the real thing. Isolate components behind interface-bounded surfaces so each is
built and reasoned about once.
**Home:** `AGENTS.md` (Conventions).

### DEC-WF-pwsh
`pwsh` (PowerShell 7) is the standard shell; `powershell.exe` (5.1) is never to be invoked or nested
(non-UTF-8 console, missing modern operators). Git Bash / the Bash tool is equally fine.
**Home:** `AGENTS.md` (Conventions). *(Workflow ruling — full text stays in AGENTS.md; indexed here only.)*

### DEC-WF-read-gate
The doc-read before touching a subsystem is mechanically enforced (SessionStart re-injection +
PreToolUse deny across edit/subagent/write-Bash vectors), not exhorted. ALL docs are read every session
until the owner declares the codebase under control; reading means reading in full (not skim/grep/defer).
The only exception is a scoped minion briefed by the orchestrator.
**Home:** `AGENTS.md` (Conventions — read-gates).

### DEC-WF-rulings-to-repo
Every owner ruling goes into the repo docs immediately and unprompted, in the same work item — saving to
assistant memory only is an unfinished task. Workflow/convention rulings → `AGENTS.md`; subsystem/design
rulings → the relevant `docs/dev/` page (and indexed here).
**Home:** `AGENTS.md` (Conventions). *This ledger is the discoverability half of that rule.*

### DEC-WF-no-commit-unmandated
Only auto-branch/commit/PR when the work is tied to an active GitHub issue; for everything else edit the
working tree only. Never switch branches while the user may be mid-build. Verify the current branch
immediately before every commit.
**Home:** `AGENTS.md` (Conventions).
