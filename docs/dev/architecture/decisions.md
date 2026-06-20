# Decisions ledger — the canonical, ID'd home for cross-cutting rulings

> **What this is.** An **index, not a re-statement**: one stable `DEC-id` per cross-cutting ruling, a
> one-line summary, and a pointer to its authoritative home. It exists to break the duplication loop —
> rulings kept getting re-stated doc-after-doc because there was no discoverable canonical home, so agents
> re-added them defensively, *ad infinitum*. **Operational rule: before adding any cross-cutting ruling
> anywhere, grep this ID table first.** A doc that needs a ruling links `[DEC-id]`; it does not
> re-articulate it. Full rationale: [`AGENTS.md`](../../../AGENTS.md) Conventions ("the discoverability half").
>
> **Transition note (2026-06-19):** the rulings are being carried from the old `docs/dev/decisions.md`
> into this docs2 set. Until each subsystem's home is rebuilt (tracked in
> [`_meta/build-plan.md`](../_meta/build-plan.md)), an entry's **full text** may still live in the old set;
> the one-line + home below is authoritative for *what the ruling is and where to read it*.

---

## Index (grep this first)

| ID | One-line | Home |
|---|---|---|
| [DEC-fixedpoint-x100](#dec-fixedpoint-x100) | All cascade value math is integer ×100; the one human→int conversion lives only in readJson | `reference/cascade/fixed-point-and-scales.md` |
| [DEC-per100-closed-set](#dec-per100-closed-set) | The legacy per-100 fields are a CLOSED set of exactly 6 `…100()` accessors | `reference/cascade/fixed-point-and-scales.md` §4b |
| [DEC-curator-owns-descale](#dec-curator-owns-descale) | The curator absorbs all per-100 ambiguity once; JSON is uniformly human; readJson re-applies ×100 | `reference/cascade/fixed-point-and-scales.md` §1 |
| [DEC-deliveryguy](#dec-deliveryguy) | A cross-entity modifier lives on whoever DELIVERS it (the deliveryguy), keyed by the target — by semantic sense, not inversion | `reference/cascade/modifier.md` §6 |
| [DEC-cascade-bidirectional](#dec-cascade-bidirectional) | The cascade is bidirectional (enabler `require` callback UP the chain); down-only was tried and fails AND-modeling + dumps maintenance on modders | `architecture/north-star.md` §2 |
| [DEC-no-guessing](#dec-no-guessing) | Never hypothesize a divergence cause; map it to a named source — emit the data first if missing | `AGENTS.md` |
| [DEC-map-before-delete](#dec-map-before-delete) | You cannot delete a maintainer you cannot fully observe; shadow it until clean, then cut | `AGENTS.md`; old `cascade-mapping-inventory.md` §A |
| [DEC-parity-not-goal](#dec-parity-not-goal) | Parity is not the goal; ±10% is NOT "parity-adjacent" — sharper than legacy | [`reference/cascade/shadow.md` §3](../reference/cascade/shadow.md) |
| [DEC-tally-serializes-nothing](#dec-tally-serializes-nothing) | Tally + scope accumulators serialize NOTHING — rebuilt from loaded objects on load | [`reference/cascade/tally.md` §4](../reference/cascade/tally.md) |
| [DEC-save-remove-is-soft](#dec-save-remove-is-soft) | Removing a serialized field/Type is SOFT in the name-keyed format; only 4 cases are HARD | [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) |
| [DEC-derived-never-trusted](#dec-derived-never-trusted) | Derived data is never trusted from a save; reset() marks dirty and recomputes | [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) |
| [DEC-obs-scale](#dec-obs-scale) | The Observability Scale (0 Oblivious … 5 Meta) + the reconstruct-from-API "Orwell" bar | [`reference/observability/README.md`](../reference/observability/README.md) |
| [DEC-obs-hook-shapes](#dec-obs-hook-shapes) | The 3 canonical observability hook shapes (snapshot field / gated `[TAG]` log / mailbox `/diagnostic`) | [`reference/observability/README.md`](../reference/observability/README.md) |
| [DEC-interface-contracts](#dec-interface-contracts) | C++03 Clean-Architecture contracts: pure-virtual bases, MI = implements, poor-man's-DI at a composition root | `AGENTS.md` |
| [DEC-proper-once](#dec-proper-once) | Build the proper structure once — reject transitional shims | `AGENTS.md` |
| [DEC-keep-unkilled-ideas](#dec-keep-unkilled-ideas) | Retire only code-reconstructible-stale or explicitly-killed docs; an un-killed idea is kept (out-of-scope ≠ retire) | `_meta/CONVENTIONS.md` §7 |
| [DEC-WF-read-gate](#dec-wf-read-gate) | The doc-read before touching a subsystem is mechanically gated, not exhorted | `AGENTS.md` |
| [DEC-WF-rulings-to-repo](#dec-wf-rulings-to-repo) | Every owner ruling → repo docs immediately, unprompted, same work item | `AGENTS.md` |
| [DEC-WF-no-commit-unmandated](#dec-wf-no-commit-unmandated) | Edit working tree only unless tied to an issue; never switch branches mid-build | `AGENTS.md` |
| [DEC-WF-surface-sprawl](#dec-wf-surface-sprawl) | Surface "getting out of hand"/undefined-structure to the owner instead of overcompensating with serial partial fixes; don't make the owner restate; optimise for efficiency | `AGENTS.md` |
| [DEC-ephemeral-project-folder](#dec-ephemeral-project-folder) | A massive one-time project gets a dedicated, explicitly-named, front-and-center folder, not held to the durable bar, deleted wholesale when done (durable knowledge extracted first) | `_meta/CONVENTIONS.md` §9 |

---

## Entries

### DEC-fixedpoint-x100
All cascade value math is integer fixed-point ×100; JSON is human-readable; the single human→int conversion
+ percent semantics lives only in readJson; the cascade never knows about ×100. **Home:**
[`reference/cascade/fixed-point-and-scales.md`](../reference/cascade/fixed-point-and-scales.md).

### DEC-per100-closed-set
The legacy per-100 fields are a CLOSED, verified set of exactly six `…100()` accessors — that set is the
curator's entire de-scale list; figure scale from the math, not the name. **Home:**
[`reference/cascade/fixed-point-and-scales.md` §4b](../reference/cascade/fixed-point-and-scales.md#4b-the-closed-per-100-set--100-to-humanize).

### DEC-curator-owns-descale
XML→JSON happens once; the curator absorbs all per-100-vs-normal mixing and emits uniform human numbers, so
readJson has zero per-field scale knowledge. **Home:**
[`reference/cascade/fixed-point-and-scales.md` §1](../reference/cascade/fixed-point-and-scales.md).

### DEC-deliveryguy
Ownership of an **entity-keyed (cross-entity) modifier** is decided by **semantic sense — "who BRINGS this
modifier to the table?"** That deliverer (the *deliveryguy*) OWNS it; the other entity is the **condition**
("what ENABLES it?"). Two equally first-class expression modes, chosen per-case by what reads sensibly:
- **keep-on-source** — the source owns it and references the other entity as an `enabled`/`per` condition
  (e.g. a civic's +happiness buff conditioned on `BONUS_X` stays on the civic).
- **fold-onto-the-deliveryguy** — the modifier lives on the delivering entity, keyed by the target (e.g. a
  route making an improvement better → the boost lives on the **route** keyed by improvement; a building
  making a terrain's tiles yield more → stays on the **building** keyed by terrain, **NOT** inverted onto
  the terrain).

Plot-substrate entities (terrain/feature/improvement/route) each own their *own* intrinsic output at plot
scope. This REFINES keep-on-source and **superseded the earlier "inversion" approach** — the discriminator
is *who delivers*, not conditioner-vs-target. **Home:** [`reference/cascade/modifier.md` §6](../reference/cascade/modifier.md#6-ownership--the-deliveryguy-rule) (owner ruling 2026-06-16).

### DEC-cascade-bidirectional
The cascade is **bidirectional**, not down-only: the enabler resolves its `requires` by a `require` callback
**UP** the scope chain. `requires` is precisely the **AND** mechanism — it is **mapped on the subset of data
enabled through the enabler chain**, resolved up-chain. Down-only was the *original* design and was abandoned
during iteration because
**(1)** it models **OR** (via enablers) but **cannot reliably model AND**, and **(2)** it forces a modder to
maintain every requirement at the **top of the chain** — a maintenance nightmare. The upward `require`
callback is **load-bearing, not optional**; do not "simplify" back to down-only. **Home:**
[`../architecture/north-star.md` §2](north-star.md) (owner ruling 2026-06).

### DEC-no-guessing
Do not hypothesize a divergence's cause and try a fix; emit the full legacy decomposition via the dump and
attribute the divergence to a named source with numbers. If the data isn't emitted, emit it first.
**Home:** [`AGENTS.md`](../../../AGENTS.md) — "THE NO-GUESSING RULE".

### DEC-map-before-delete
You cannot safely delete a maintainer you cannot fully observe; every state behaviour gets a shadow diffing
cascade vs engine until clean, then the legacy is deleted. **Home:** [`AGENTS.md`](../../../AGENTS.md); old
`cascade-mapping-inventory.md` §A. Related: [[DEC-obs-scale]], [[DEC-no-guessing]].

### DEC-parity-not-goal
Matching legacy is not the goal; the cascade may deliberately diverge; parity-ZERO is only the plumbing proof and the
end-state target is parity-ADJACENT; ±10% is NOT "parity-adjacent" — the bar is sharper. **Home:**
[`reference/cascade/shadow.md` §3](../reference/cascade/shadow.md). Related: [[DEC-map-before-delete]], [[DEC-no-guessing]].

### DEC-tally-serializes-nothing
The tally + scope accumulators serialize nothing — rebuilt from authoritative loaded objects on load; true
historical counters live on their owning object. **Home:**
[`reference/cascade/tally.md` §4](../reference/cascade/tally.md). Related: [[DEC-derived-never-trusted]],
[[DEC-save-remove-is-soft]].

### DEC-save-remove-is-soft
The name-keyed save format makes removing a plain member SOFT; Type/XML churn is free for class
enums/arrays; only 4 cases are HARD. **Home:** [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md).

### DEC-derived-never-trusted
Derived data is never trusted from a save: `reset()` marks it dirty on load and recomputes from live state.
**Home:** [`reference/engine/save-load-format.md`](../reference/engine/save-load-format.md) (the
recompute-on-load model; the repository it came from is superseded — see
[`superseded-ideas.md`](superseded-ideas.md)). Related: [[DEC-tally-serializes-nothing]].

### DEC-obs-scale
Observability Scale: 0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police · 5 Meta.
Reconstruction bar: rebuild game state from HTTP + `/events` + gated logs, never the screen. **Home:**
[`reference/observability/README.md`](../reference/observability/README.md).

### DEC-obs-hook-shapes
Three canonical hook shapes: (1) a snapshot field on `/players`|`/cities`|`/units`; (2) a gated `[TAG]` log
teed to `/events` via `streamLogTee`; (3) a mailbox `/diagnostic/*` endpoint. **Home:**
[`reference/observability/README.md`](../reference/observability/README.md).

### DEC-interface-contracts
Depend on interfaces, not concretions. C++03 interface = abstract base, pure-virtuals, no data members; MI
is the `implements` axis; wiring is poor-man's-DI (`if`/switch at a composition root); graft onto
DLL-derived classes, never EXE-bound bases. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-proper-once
Build the proper structure once; reject transitional shims that only defer the real design; isolate
components behind interface-bounded surfaces. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-keep-unkilled-ideas
Retire a doc ONLY if it is reconstructible-from-code-and-unneeded, or an explicitly KILLED idea. Forward
design intent that has not been killed is **kept** (partitioned if out of active scope) — it is not
reconstructible from code and exists nowhere else; out-of-scope is never a reason to retire. Losing
un-killed intent is the one unrecoverable deletion. **Home:** [`_meta/CONVENTIONS.md` §7](../_meta/CONVENTIONS.md) (owner ruling 2026-06-19).

### DEC-WF-read-gate
The doc-read before touching a subsystem is mechanically enforced (SessionStart re-inject + PreToolUse deny
across edit/subagent/write-Bash), not exhorted; reading means reading in full. **Home:**
[`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-WF-rulings-to-repo
Every owner ruling → the right repo home immediately and unprompted, same work item; memory-only is
unfinished. This ledger is the discoverability half. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions.

### DEC-WF-no-commit-unmandated
Only branch/commit/PR when tied to an active issue; otherwise edit the working tree only; never switch
branches mid-build; verify the branch immediately before any commit. **Home:** [`AGENTS.md`](../../../AGENTS.md).

### DEC-WF-surface-sprawl
When a change starts to sprawl (serial partial fixes piling up) or you are patching pieces of something whose
target STRUCTURE is undefined, STOP and tell the owner there's a risk of it getting out of hand — don't
agent-overcompensate with more partial fixes. The owner (aware of agentic limits, not omnipotent, expects
trust-but-verify) must be *told* so they can define the structure and authorize ONE proper cleanup; they get
frustrated by inefficiency and by having to restate the same thing — so capture rulings durably the first time
and don't churn or re-litigate. Conduct-level twin of [[DEC-proper-once]]; the docs-rebuild mess is the
motivating case. **Home:** [`AGENTS.md`](../../../AGENTS.md) Conventions (owner rulings 2026-06-20).

### DEC-ephemeral-project-folder
A massive ONE-TIME project (a migration, a from-scratch rebuild, a big sweep) gets its own **dedicated,
explicitly-named, front-and-center folder** under `docs/dev/` (worked example: `json-migration/` for #428/#430).
It is **not held to the durable grounding bar** (transient working docs), and is **archived OUT of the active
docs scope when the project completes** — moved out in one step like `old-docs/`, **not deleted** (it stays
referenceable later) — after any durable references are repointed into the durable set
(`reference/`/`explanation/`/this ledger) so the active set doesn't dangle into it. The folder keeps a running
**`TO-BE-MADE-DURABLE.md`** index (append-as-you-go pointers to durable knowledge accumulating inside it);
archiving is gated on working that index to empty. Distinct from
`plans/parked/` (un-killed intent, kept) and `reference/` (durable, permanent): a one-time-project folder is
*active now, archived out when done*. **Home:**
[`_meta/CONVENTIONS.md` §9](../_meta/CONVENTIONS.md) (owner ruling 2026-06-20). Related: [[DEC-keep-unkilled-ideas]].
