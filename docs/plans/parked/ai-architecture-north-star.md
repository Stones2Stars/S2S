# AI architecture — north-star

> **This is S2S, not C2C.** Stones2Stars is its own project, derived from Caveman2Cosmos but
> deliberately forked so we can do exactly this kind of rework **without deference to previous
> authors' conventions**. Inherited C2C code style, class organization, naming, and "the way it was
> always done" are **not constraints** — they are raw material to replace where a cleaner design
> wins. The *only* binding limits are the hard external ones from Firaxis's closed `.exe` (§2.1–2.4);
> everything else is ours to change. Do not cite "but C2C does it this way" as a reason to keep
> anything.
>
> **Explicit non-constraint:** C2C→S2S save compatibility is **already broken** and is a **non-goal**.
> A converter is at most maybe-possible with external tools — never constrain a design to keep it
> achievable. (S2S-internal, version-to-version save compat via `@SAVEBREAK` is a *separate* normal
> concern, and even that is ours to break deliberately when a rework is worth it.)
>
> **What actually breaks a save (mechanism — must-not-rediscover).** Saves store info-type
> references and remap them on load **by `<Type>` string** (`CvTaggedSaveFormatWrapper`,
> `WRAPPER_*` + `REMAPPED_CLASS_TYPE_*`, `getNewClassEnumValue`). So the save-breakers are
> **renaming or removing an XML `<Type>`, removing/reordering game options, or restructuring the
> XML schema/references.** What does **not** break saves: code/behaviour refactors (code isn't
> serialized) and **relocating tagged C++ fields into new structs as long as the tag name is
> preserved** (the format is name-keyed, not position-keyed; `WRAPPER_SKIP_ELEMENT` drops a field
> cleanly). Consequence for this plan: the **decide-side** behaviour rework (per-UNITAI modules,
> moving the small tagged AI state) is **largely save-safe**; the **read-side / data** work carries
> the higher save-break risk — not because derived caches are serialized (they're rebuilt on load),
> but because that is where we'd want to **restructure the source XML** for logical loading/refs.
>
> **Timing — break saves NOW if ever.** The playerbase is very small today, so the cost of breaking
> S2S saves is at its lowest and only rises from here. **Front-load the save-breaking work — chiefly
> the XML data/schema restructuring and dead-game-option removal** — while the window is open, rather
> than contorting a design to avoid a break or deferring it. A clean break now beats a compatibility
> shim later. (The in-memory AI refactors are save-safe, so their timing is flexible.)

**What this doc is.** A single coherence frame for the AI/data rework. Several plans have grown up
independently (`state-repositories`, `turn-time-optimization`, `unit-ai-valuation`,
`sea-ai-rework`, …). This doc is the *index* that makes them one architecture instead of a pile of
loose plans: it states the goal, the hard constraints every sub-plan must respect, the unifying
pattern, and where each existing plan fits. The detailed mechanics stay in the tributary docs — this
doc does not duplicate them, it aligns them.

It distinguishes **firm** (committed direction) from **fluid** (candidate mechanisms still open).
Don't read a fluid item as a decision.

---

## 1. The goal (firm)

**Shrink — ultimately dissolve — the AI inherited god-classes** (`CvCityAI`, `CvPlayerAI`,
`CvUnitAI`, `CvTeamAI`, `CvGameAI`, `CvSelectionGroupAI`). They each bundle several unrelated
responsibilities into one ~10–14k-line file; that bundling is the root of the duplication, the
per-turn blanket cache clears, the file-size pain, and the impossibility of swapping or parallelising
AI. Reducing their size and scope **is the goal in every change we make** — new logic must not be
added to them; existing logic should be peeled out.

**The style (firm): interface-bounded composition.** Replace inheritance + concrete coupling with
small modules behind interfaces, composed onto the base objects. This is the design surface we
optimise for (clean contract + swappable implementation), because it is also what unlocks
parallelism, alternative backends, and eventual portability (§4).

## 2. Hard constraints (firm — every sub-plan must respect these)

These bound the whole design. They are not negotiable and they are *why* the architecture looks the
way it does. **Note the difference:** §2.1–2.4 are hard *external* limits from the closed Firaxis
`.exe` — they bind regardless of what we want. §2.5 is *our* strategic choice. Neither is a "C2C
convention"; inherited author conventions are **not** in this list and carry no weight (see the S2S
banner above).

1. **Toolchain: MSVC 7.1 / VC2003, C++03, 32-bit, Win32-only threading.** The only build is FastBuild
   (`Sources/fbuild.bff`); the `.vcxproj` is dead (see `AGENTS.md`). No `std::thread`, no OpenMP, no
   C++11+. In-process parallelism = raw Win32 threads. The lock exists because the DLL must stay
   ABI/STL-compatible with the **closed VC7.1 game `.exe`** — STL objects cross that boundary. Modern
   C++ in-process is therefore only reachable behind a **C/POD ABI boundary** (no STL crossing the
   seam), as a separately-compiled module (§4).
2. **EXE binds only the BASE classes.** The AI subclasses are 100% DLL-internal (instantiated inside
   the DLL — `new FFreeListTrashArray<CvCityAI>`, `new CvPlayerAI[]`, `new CvUnitAI`; named in no
   `*Interface*.h`). The EXE contract is the **base-class vtable + exported entry points**. So
   dissolving the AI subclasses is *legal*; the invariant to preserve is the base-class virtual
   interface (don't reorder/insert base virtuals; keep every object a base-derived instance). Adding
   composition members to a base class is safe.
3. **MP is deterministic lockstep.** Every client simulates everything (including all AI) identically
   from a shared RNG seed; only commands are networked; OOS = checksum mismatch. That sync layer is
   in the closed EXE — it **cannot** be turned into host-authoritative state-replication from the
   DLL. The lever against OOS is **determinism, not authority** (even Pitboss runs the deterministic
   DLL). Anything feeding a synced decision must be byte-identical across machines.
4. **Advisory-only derived data; never loop-driving.** Derived/cached data is AI heuristics, never
   synced game state, and must never feed control flow that can loop (the building-*value* cache hung
   the game by looping `AI_chooseProduction`; an input *set* is safe). Rebuild dirty on load; keep a
   bounded-staleness backstop; debug-verify (recompute == cache) during migration.
5. **The inherited corpus is the asset — no rewrite.** S2S's value is the rules+content+AI corpus
   (~100Ks of lines) inherited from C2C, not the engine. A from-scratch rewrite transfers none of it. In-place
   interface-bounded work gets the modernisation benefits *and* keeps a future port option open at no
   extra cost; it dominates both "stay messy" and "rewrite now." (Rewrite is only right if the goal
   changes to "a new game inspired by C2C" — a different project.)

## 3. The unifying frame (firm shape; fluid details)

**Base game objects stay the stable, EXE-facing core. Everything the AI inheritance does becomes
composed modules hanging off them, each behind an interface.** The god-classes drain into these
modules until little is left.

Two module *kinds*, mirroring two directions of data flow:

| Kind | Answers | Examples | Interface = |
|---|---|---|---|
| **Read-side** (derived data) | "what does the world look like?" | constructible set, building values, tech/civic/bonus values, city declared-needs | data getters |
| **Decide-side** (behaviour) | "what do I do about it?" | per-UNITAI behaviour, city production choice, player strategy | decision methods |

**Two data-flow rules (firm):**

- **Reads pull *up* the ownership tree, lazily.** A City getter needing player/team data calls the
  parent getter, which recomputes itself if dirty. Cross-level read dependencies self-resolve — no
  ordering logic.
- **Invalidation pushes *down* the tree, eagerly.** A mutation fires **one** call at the owning
  level, which dirties its own members and cascades to the children that care.

**Placement (firm): on the base objects** (`CvGame`/`CvTeam`/`CvPlayer`/`CvCity`/`CvUnit`), not the
AI subclasses — the data must not be trapped in the class we are dissolving, and the flagship
read-side tenant (constructibility) is shared with the UI, whose caches already live on base objects.

**The coherence filter.** Every new idea must answer **one** of: *is it a read-side module, a
decide-side module, a placement choice, or a backend choice?* If it can't, it doesn't belong in the
architecture yet. This is the test that stops loose-plan sprawl.

## 4. Backends — why the interface seam matters (firm rationale; fluid choice)

Because each module sits behind an interface, *where its implementation runs* is a swappable backend
decision, made per-module, later — not a rewrite. Cheapest → wildest:

1. **In-process C++03** (the default; what composition produces).
2. **In-process modern-C++ module behind a C/POD ABI** — separately compiled with a current
   toolchain, linked into the same process, no STL across the seam. Gets modern C++ + better
   threading **without** inter-process marshalling and **without** touching the EXE-locked core
   toolchain. This is the realistic route to modern code / richer parallelism.
3. **Out-of-process / another language / ML** — only when you genuinely need a different runtime.
   Pays marshalling; for MP it must be deterministic or run host-authoritative-bot-client (command
   injection like a human) — realistically **SP/research only**.

**Parallelism is a real perf lever** for the value-calc workload (it is embarrassingly parallel), but
its prerequisite is the read-side cleanup: today the `mutable` lazy caches mutate *during* const
reads (`CvCity::m_bCanConstruct`, the building-value cache) and would data-race. A change-driven
repository that is **computed in a defined phase then read-only** is what makes a parallel pass safe.
*Same cleanup, three payoffs: interfaces, parallelism, optional modern backend.* (For MP, the
parallel reduction must be deterministic — see constraint §2.3.)

## 5. Firm vs fluid — quick reference

**Firm:** the goal (§1); interface-bounded composition; placement on base objects; push-down /
pull-up; the constraints (§2); advisory-only safety; no rewrite.

**Fluid (candidate, decide per-module):**

- Invalidation wiring: **typed cascade** (one `onXxx()` per event per level, cascading down —
  generalises the existing `clearCanConstructCache` cascade; recommended for ~6–8 events) **vs** a
  central event/registry bus.
- Unit-AI decision authority: **on the unit** (decomposed per-UNITAI module) **vs** lifted to a
  **`CvPlayer` orchestrator** (they can layer: orchestrator assigns, module executes).
- Constructible-set retention: **bounded-staleness** (rebuild every N turns + reliable events) vs
  attempting event-exact invalidation (harder — inputs include pop/culture/properties, which move
  often; see [`state-repositories.md`](../../architecture/state-repositories.md)).
- Per-module backend rung (§4).
- Module granularity / how many interfaces.

## 6. Module catalog (where responsibilities go)

Read-side (repository, base objects, change-driven):

- **Static:** the prereq → units/buildings reverse index this bullet originally named is now the
  enabler's own `EDGEF_REQUIRED_BY` reverse index ([`enabler.md`](../../specs/enabler.md) §2/§7.1),
  not a standalone `cvInternalGlobals` table — the enabler cascade absorbed this responsibility.
  The Game *repository* is for game-state-derived data only.
- **Team:** known/obsolete tech sets, tech-derived availability.
- **Player:** tech / civic / promotion / bonus values, building & area-unit counts, needed-building
  counts. (Absorb the `AI_doTurnPre` blanket `.clear()`s.)
- **City:** constructible set, building values, best-build, **declared needs** (the substrate for
  city-driven worker AI), workers-needed.
  - **Declared-needs channel = the emphasis system (owner ruling 2026-06-11, #367).** Emphasis is
    the city's "what I want" signal and must genuinely steer the governor before it can carry
    AI-declared needs: the same channel is read by humans-steering-the-governor today and will be
    *written* by AI cities tomorrow (one signal for plot/specialist allocation, building preference,
    worker/unit valuation). Current state from the #367 audit: process-choice weighting fixed
    (#366/#68); three commerce emphasis types have consumers but no buttons (#368); building values
    ignore emphasis entirely — gates only (#369); governor effects are weak 1.3–1.75× nudges with no
    forced-specialist link (#370). `AI_getYieldMultipliers`'s "AI no longer uses emphasis" is the
    gap the declared-needs work fills deliberately.

Decide-side (behaviour, interfaces):

- **`IUnitAI`** — one concrete per UNITAI type (WorkerAI, HunterAI, …). Split **flyweight behaviour**
  (one shared instance per type, zero per-unit memory) from **per-unit state** (only the type's
  slice). Precedent already in tree: `CvHunterAI`/`CvWorkerAI` (currently per-player).
- City production strategy, player strategy — later carve-outs from `CvCityAI`/`CvPlayerAI`.

## 7. Tributary map (how existing plans fit)

| Plan / initiative | Role in the frame | Status |
|---|---|---|
| [`state-repositories.md`](../../architecture/state-repositories.md) | the read-side modules + invalidation mechanism | this is the doc that now owns the pattern — the *maintained-sum, event-updated* model, not the `dataRepository()`/versioning skeleton this row originally described (superseded) |
| `turn-time-optimization.md` | the *payoff* of change-driven read-side + a parallel pass | see that doc for current status |
| `unit-ai-valuation.md` | decide-side correctness (what `IUnitAI` modules must get right) | living report; several bugs found |
| `sea-ai-rework.md` | decide-side (naval) — early example of carving behaviour out of `CvUnitAI` | PRs merged; one spin root-cause open |
| `improvement-category-yields.md`, `subdued-animal-ai.md`, `worker-stranded-tiles-reachability.md` | decide-side + read-side (city declared-needs) for worker/improvement AI | planned |
| `dead-code-xml-pass.md` | shrinks the surface before/while carving | planned |

Memory cross-refs: `shrink-ai-inherited-classes`, `derived-data-repository`,
`ai-unit-movement-to-player-level`, `city-driven-worker-valuation`,
`design-style-interfaces-and-loose-ideas`.

## 8. Roadmap (risk-ordered; each step measurable + reversible)

The ordering is deliberate: prove the pattern on safe, high-value, *verifiable* data first; defer
behaviour-sensitive and exotic-backend work.

**Save-break sequencing (orthogonal to risk-ordering):** the steps below are mostly save-safe — the
only save-breaking part is **XML data/schema restructuring** (cleaning source data, `<Type>`
identity, references) and **dead-game-option removal**, which naturally rides along with the
read-side / Game-static-index work (steps 1–3) and `dead-code-xml-pass`. Per the S2S timing note
(§ banner), **do that XML restructuring now**, while the playerbase is small, rather than later. The
in-memory carve-outs (steps 4–6) don't break saves, so their timing is driven purely by risk/value.

1. **Pilot — read-side repository absorbing the existing `canConstruct` caches + building values.**
   (The old "~30% turn-time" PreLoop payoff is already banked by the #195 static index, PR #314 —
   the remaining prize is the building-VALUE recompute chain.) Make the three current caches
   (`CvPlayer::m_bCanConstruct[]`, `CvCity::m_bCanConstruct`,
   `CvCityAI::BuildingValueCache::m_buildingsToCalculate`) and the building values the *first
   tenants* of the base-object repository — **replacing** `clearCanConstructCache`/
   `FlushCanConstructCache`/`AI_FlushBuildingValueCache`, not adding a 4th layer. Bounded
   staleness + events; debug-verify via gated logging. **Precondition:** harden
   `AI_chooseProduction` against stale inputs first (the retention experiment hang —
   `CvCity.cpp:1256`).
2. **Absorb the `AI_doTurnPre` blanket clears** (tech value, civic value, bonus value, needed counts)
   one at a time → event-driven invalidation behind the same pattern.
3. **Game-level static indices.** The prereq/enabler reverse-index this step named now lives inside
   the enabler cascade itself (`EDGEF_REQUIRED_BY`, [`enabler.md`](../../specs/enabler.md) §2/§7.1)
   rather than as a standalone `cvInternalGlobals` table.
4. **Parallelise a clean read-side pass** (value calcs), Win32 threads in-process, deterministic
   reduction. Prereq: the data it reads is computed-then-read-only (steps 1–3).
5. **First decide-side carve-out — `IUnitAI`** for one UNITAI type, behind the interface, flyweight +
   per-type state. Decide the authority fork (§5) before cutting.
6. **Modern-C++ module behind a C/POD ABI** (§4.2) only if/when a hot module wants threading/idioms
   C++03 can't give — proven by measurement, isolated from the EXE-locked core.

Nothing is migrated until profiling shows it matters and is recompute-bound (not work better
*removed* than cached). Each step ships on a working game.

---

## The AI VALUATION plane — one memo shape, fact-invalidated (⛔ OUT OF SCOPE for #430)

> **⚖ THE DIRECTION (owner): *"without extending AI valuation to some sort of cascade-alike structure,
> we won't be able to avoid walks — that is the nature of the beast."* And its bound, same breath:
> *"is that something that can be done? certainly to some degree, but it's not in scope."*** So this is
> forward intent ([DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas)),
> recorded so it is not re-derived — ⛔ never a licence to start it inside #430.

**⛔ FIRST, WHAT IS *NOT* THE PROBLEM — SIMPLE AI VALUATION CACHING IS ALLOWED (owner): *"we can cache AI
valuation, in the simple way it used to be done — that is not banned."*** The per-site AI memos (the
`AI_yieldValue` LRU, plot danger, the attitude cache, the strategy-hash turn stamp, the
`AI_isFinancialTrouble` `(turn, gold)` memo) are the SANCTIONED heuristic residual
([superseded-ideas #1](../../architecture/superseded-ideas.md)), not debt.
⛔ **Do NOT cite [DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape) at them** —
that ruling governs the CASCADE plane, where a slot is a Σ addressed by a compiled deposit address. An AI score
is not, so "one uniform slot table" has nothing to bite on and the hand-named-scalar argument does not carry
across. ⚠ This mislabelling was made once, in the session that wrote this note; it is recorded so the next
reader does not repeat it and "clean up" caching the AI is entitled to.

**⚖ WHAT THE CASCADE IS ACTUALLY FOR, HERE (owner): *"we use cascade to ensure AI does not loop ALL THE THINGS
ALL THE TIME, which will make life better."*** That is the relationship, and it runs one way: the cascade does
not become the AI's cache — it removes the AI's REASON to walk. The enabler hands it a small maintained
candidate set instead of the entity database ([enabler.md §6](../../specs/enabler.md)), and the packages hand it
O(1) inputs instead of re-derivations. The AI then caches its own scores however simply it likes, on top.
⇒ **So the lever is making the cascade's reads genuinely cheap, never making the AI plane cascade-shaped.**


**⚑ AND IT REFRAMES WHICH WALKS ARE ACTUALLY IRREDUCIBLE — three kinds, and only one is the beast:**

1. **Walks over INPUTS** (`getYields`, `AI_isFinancialTrouble`, `getCommerces`). ⛔ NOT irreducible and not
   the AI's problem: these are reads specified as O(1) that were computing. Fixing them is the CASCADE's job
   ([state-repositories.md](../../architecture/state-repositories.md)) — three were found and fixed in one
   session, all by attaching a debugger to a spinning process.
2. **The FRONTIER walk** (candidates × cities) — intended and bounded by design
   ([enabler.md §6](../../specs/enabler.md): the AI iterates a small maintained set, never the database).
3. **The CONDITIONED tail** — genuinely irreducible, because the answer depends on the asking city.

⇒ **Category 1 is what produced every hang so far, wearing category 3's clothes.** So the highest-value
work is not this structure — it is the AUDIT of which "reads" still compute
([DEC-legacy-decache-poisons-perf](../../architecture/decisions.md#dec-legacy-decache-poisons-perf)). The
structure is what stops category 1 recurring as fresh ad-hoc memos afterwards.
