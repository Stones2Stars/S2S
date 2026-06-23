# docs2 authoring conventions — the grounding standard

> This file governs **how every doc in `docs2/` is written.** It exists because the old doc set
> accreted by restatement and assumption until it actively misled agents — the "hydra that bites your
> head off when you assume." The rebuild's entire point is that an agent who reads `docs2/` ends up with
> a **complete and CORRECT** model of the codebase, unbidden. These rules are what make that true. Hold
> them or you reintroduce the rot.

---

## 1. THE GROUNDING LAW (the one that matters most)

**Every claim about how the code behaves is cited to the code, and verified against the code at the
moment it is written — never reconstructed from memory, from another doc, or from "it obviously must."**

- A behavioural statement carries a `file:line` (or `file::function`) citation to the **live source**
  it was verified against, e.g. `getBuildingCommerce100` adds `BonusCommercePercentChanges` raw beside
  `100 * getBuildingCommerce` — `Sources/Engine/CvCity.cpp:12132`.
- Line numbers **drift**. Every doc says so once, and every citation is treated as "the function named
  here, around this line" — confirm the function, not the integer.
- If you cannot cite it, you do not state it. Write `UNVERIFIED:` and flag it, or leave it out. A gap is
  honest; a confident wrong line is a future head-bite.
- **Trust-but-verify applies to everything** — a prior doc, an AGENTS.md aside, an owner statement, your
  own recollection. All are hypotheses to confirm against ground truth (live code, live data, the running
  game). The owner explicitly flags their own statements as "verify, not confirmation."
- When you verify something non-obvious, **say what you verified it against** so the next reader can
  re-check cheaply.

This is the anti-assumption rule. It is the difference between a doc that builds understanding and a doc
that builds *false* understanding — which is worse than none.

## 2. SINGLE SOURCE OF TRUTH — no restatement

The old set duplicated cross-cutting rulings into N docs; they drifted; agents couldn't tell which was
authoritative and re-added them, *ad infinitum*. Never again.

- **Cross-cutting rulings live ONCE, in the [decisions ledger](../architecture/decisions.md), with a stable `DEC-id`.**
  A doc that needs a ruling **links `[DEC-id]`** — it does not re-articulate it. Before adding any
  ruling, grep the ledger's ID table first.
- **Shared facts live once, in their canonical home, and are linked:** scales → the scale registry
  (`reference/cascade/fixed-point-and-scales.md`); the observability scale + hook shapes →
  `reference/observability/README.md`; save-format rules → `reference/engine/save-load-format.md`. Other
  docs link, never restate.
- If you find yourself explaining a shared concept inline, stop: link its home. If it has no home, give
  it one and link that.

## 3. STRUCTURE — Diátaxis, adapted for an agent reader

Docs are sorted by **what the reader needs**, following the [Diátaxis](https://diataxis.fr) split:

| Folder | Diátaxis mode | Answers | Style |
|---|---|---|---|
| `reference/` | Reference | "How does *this* behave, exactly?" | Dry, complete, code-cited. One subsystem per doc. |
| `explanation/` | Explanation | "Why is it built this way? How do the pieces fit?" | Narrative, design-level, the architecture & rationale. |
| `plans/` | (roadmap) | "What are we changing, and where is it?" | In-flight initiatives, scope, status, removal maps. |
| `decisions.md` | (ledger) | "What was ruled, and where's the authority?" | Index of `DEC-id` rulings → their homes. |
| `_meta/` | (governance) | "How are these docs themselves run?" | This file; the build plan. |

- **Reference describes what IS; plans describe what we INTEND.** Never blur them — a stale "we will…"
  read as "it does…" is a classic head-bite. A plan that lands becomes reference (and the plan is
  archived).
- `reference/observability/` is its own cluster (the surveillance surface) with a shared scaffold README.

## 4. LAYERING — orient before you drill

Every domain has an **overview** that orients the reader (what this subsystem is, its pieces, how they
relate, where to go next), then **detail docs** per piece. An agent reads the overview and knows the
shape; reads a detail doc and knows a piece; never has to reverse-engineer the shape from the details.
The top-level [`README.md`](../README.md) is the overview-of-overviews — the comprehension map.

## 5. DOC TEMPLATE

Every doc opens with a metadata block, then orients, then delivers:

```markdown
# <Title> — <one-line scope>

> **Status:** reference | explanation | plan   ·   **Verified against:** <commit/date>
> **Grounding:** <what live source/data the claims were checked against>
> One-paragraph orientation: what this is, why a reader is here, what they'll know after.

## (body — sections per the doc's job; every behavioural claim code-cited)

## See also
- [<relevant doc>](...) — <why it's relevant> (state the relationship, not just the link)
```

- **Cross-links state the relationship** ("the consumer that emits the log this snapshot supports"), not
  a bare link. "Relevant to each other" means the reader learns *how* they relate.
- **Freshness is explicit:** the `Verified against` date is mandatory. A reader instantly knows how much
  to trust it.

## 6. WRITING STYLE (best practice)

- Lead with the answer (BLUF — bottom line up front). The reader's question is answered in the first
  lines; detail follows.
- Scannable: headers, tables, short paragraphs. An agent skims to the relevant section.
- Precise over breezy. Name the function, the field, the file. Avoid "various", "etc." where a list is
  knowable.
- One concept per doc; one primary subject per section.
- No motivational filler, no restating the obvious, no hedging that isn't a real uncertainty.

## 7. WHAT GETS RETIRED vs KEPT — the criterion

**Retire a doc ONLY if it is one of:**
1. **Reconstructible-from-code AND not currently needed** — it describes how live code behaves and drifts
   fast, so a stale copy misleads. The out-of-scope AI-behaviour class docs are the example: decision
   logic read straight from `Cv*AI.cpp`. **Rebuild from code, grounded, when a task needs it** — never
   carry a stale copy. (Build plan §2a.)
2. **An explicitly KILLED idea** — a design direction the owner has rejected. Move it to history; it no
   longer informs.

**KEEP everything else — in particular, an idea that has NOT been killed is kept (owner ruling
2026-06-19).** Forward design intent — a north-star, a dormant rework, a "we might do X" — is **not
reconstructible from code** and exists nowhere else, so being merely *out of active scope* is **never** a
reason to retire it. Partition it (a clearly-marked out-of-scope area, e.g. `plans/ai/`) so an agent on
the active work isn't forced through it, but preserve it intact until it is either built or explicitly
killed. Losing un-killed intent is the one unrecoverable deletion — code can be re-read; a killed-by-
accident idea cannot.

- Handovers and transient task-lists never live in `docs2/` — they are relays, not durable docs.

## 8. THE NON-NEGOTIABLES STAY LOUD

The codebase's head-biters (frozen VC7.1/C++03 toolchain, the dead `.vcxproj`, the graphics-init guard,
save-format determinism, OOS integer-only math) are surfaced in [`README.md`](../README.md) §
"Non-negotiables" and repeated at the top of every doc whose subsystem touches one. Redundancy of a
*warning that prevents a catastrophe* is good redundancy — it is the exception to §2, and a deliberate one.

## 9. One-time project folders — when done, they're gone

Some work is a **massive ONE-TIME project**: a migration, a from-scratch rebuild, a big mechanical sweep. Its
working material — roadmaps, registries, per-step decisions, conversion maps — is **load-bearing while the
project is in flight and OBSOLETE the moment it lands.** That material gets its own **dedicated, explicitly
named, front-and-center folder** (the worked example: [`../json-migration/`](../json-migration/README.md) for
the #428/#430 XML→JSON migration), with these properties (owner ruling 2026-06-20):

- **Named for the project, not generic** — `json-migration/`, not `migration/` / `misc/` — so its scope and
  one-time nature are obvious at a glance.
- **Front-and-center while active** — a top-level folder under `docs/dev/`, not buried, so the live project is
  impossible to miss.
- **NOT held to the durable grounding bar** (§1–§6). These are transient working docs; perfect `file:line`
  citations and single-source dedup matter far less than for `reference/`. Don't over-invest polishing docs
  that will evaporate (some internal cross-links may even point at retired specs — acceptable here).
- **Archived out of active scope when done — NOT deleted.** At completion the folder is **moved out of the
  main docs scope in one move** (the same treatment as `old-docs/`), so it stays **referenceable when needed**
  — just out of the active set. It is not migrated piecemeal into `reference/`. Its `README.md` states this
  lifecycle up front.
- **Repoint durable references BEFORE archiving.** Anything in the durable set (`reference/`, `explanation/`,
  the ledger) that must stay reachable is moved/repointed there *first*, so the active docs don't dangle into
  the archived folder. Nothing is lost either way (it's archived, not deleted) — but the active set must stay
  self-contained.
- **Keep a `TO-BE-MADE-DURABLE.md` index in the folder, appended AS-YOU-GO.** The moment durable knowledge is
  written inside the folder (a decision, a ruling, a lasting reference fact), add a **pointer row** to that
  index — *what it is · where it sits now · its target durable home* (a pointer, never a copy — §2). Archiving
  is then mechanical and safe: **work the index to empty** (each item lifted to its durable home + source
  repointed), *then* archive — a non-empty index BLOCKS archiving. This converts "remember to extract before
  archiving" from a memory-dependent step into a checklist with a clear done-condition (the same discoverability
  principle as the decisions ledger / [DEC-WF-rulings-to-repo](../architecture/decisions.md#dec-wf-rulings-to-repo)).
  Worked example: [`../json-migration/TO-BE-MADE-DURABLE.md`](../json-migration/TO-BE-MADE-DURABLE.md).

Distinct from the other transient-ish zones: **`plans/parked/`** = un-killed forward intent, KEPT
indefinitely ([DEC-keep-unkilled-ideas](../architecture/decisions.md#dec-keep-unkilled-ideas)); **`reference/`**
= durable, grounded, permanent. A one-time-project folder is neither — it is *active now, archived out when
done*. Ruling:
[DEC-ephemeral-project-folder](../architecture/decisions.md#dec-ephemeral-project-folder).
