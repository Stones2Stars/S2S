# S2S developer docs — start here (the comprehension map)

> **Read this first, in full.** It is the map: what S2S is, the things that will bite you if you assume,
> the architecture, where every subsystem's doc lives and how they relate, and — by task — the order to
> read them in. An agent who reads this and the docs it points to ends up with the **complete, correct**
> model of the codebase.
>
> **THE MISSION (owner, 2026-06-19).** Nobody should ever have to ask an agent *"have you read this?"*
> before a change. Complete understanding should **just happen** from reading the docs — unbidden, by
> default. Policing context is an inefficiency this doc set exists to abolish. So reading and understanding
> must become the *same act*: every doc is grounded (no assumption to trip on — [`_meta/CONVENTIONS.md`](_meta/CONVENTIONS.md)),
> consolidated (one home per concept, not spread thin), and mapped (this file is the reading path). The
> read-gate forces the read; these docs are what make a forced read *deliver* the full model. When that bar
> is met, the question never needs asking.
>
> **Status:** `docs2/` is the rebuilt, grounded doc set (see [`_meta/CONVENTIONS.md`](_meta/CONVENTIONS.md)
> for the standard every doc here is held to). It is **under construction** — the migration of each
> subsystem from the old `docs/dev/` set is tracked in [`_meta/build-plan.md`](_meta/build-plan.md). Where
> a doc is not yet rebuilt, the build plan says so and points at the old source.

---

## 0. How this codebase bites — the NON-NEGOTIABLES

These are not style preferences. Each one has drawn blood. Internalise them before touching anything.
(Authoritative source: the root [`AGENTS.md`](../../AGENTS.md); this is the index, that is the law.)

- **The toolchain is FROZEN and ancient, on purpose.** The DLL compiles with the vendored **MSVC 7.1
  (Visual C++ Toolkit 2003)**, **C++03 only**, **32-bit/x86**, **Python 2.4**, **Boost 1.32 + 1.55**, raw
  Win32 (no `std::thread`, no OpenMP, no C++11+). This is a hard ABI/STL lock to stay compatible with the
  closed Firaxis `.exe` — **not** a convention you may modernise. → grounding: `Sources/fbuild.bff`,
  [`reference/engine/build-and-toolchain.md`] · [`reference/engine/boost-situation.md`].
- **⛔ The `.vcxproj` / `.sln` / `.filters` are DEAD for build facts.** They are stale IDE artifacts and
  lie (their `PlatformToolset` says `v142` — FALSE). **Every build fact comes from FastBuild only**
  (`Sources/fbuild.bff` + `Tools/_Build.ps1`). Never read a `.vcxproj` to learn the compiler, defines,
  includes, or standard.
- **Graphics mutation pre-init crashes.** Any plot-graphics change must be guarded by
  `GC.IsGraphicsInitialized()` — during a NEW game, world-gen runs before the render engine exists. The
  classic "crashes with paging off, fine with it on" signature = a graphics path running pre-init.
- **Save format is name-keyed → removing a serialized member is SOFT, not a break.** Only four cases are
  genuinely HARD. Derived/accumulator state serializes nothing (recomputed on load). Intentional breaks →
  `@SAVEBREAK`. → [DEC-save-remove-is-soft](architecture/decisions.md#dec-save-remove-is-soft).
- **OOS / lockstep determinism: integer math only, synced Soren RNG, no float ever.** Civ4 MP is
  deterministic lockstep; CPU-dependent float math desyncs. The cascade's fixed-point (×100) exists for
  this. → [DEC-fixedpoint-x100](architecture/decisions.md#dec-fixedpoint-x100).
- **Nothing is "just a one-liner."** Tightly-coupled BTS/C2C wiring with non-obvious cross-cutting
  ripples. Read the subsystem's doc, trace every caller/consumer, before you touch it. The read is
  mechanically gated for exactly this reason → [DEC-WF-read-gate](architecture/decisions.md#dec-wf-read-gate).
- **Shell:** `pwsh` good, `powershell.exe` (5.1) bad; Git Bash fine. **Git:** edit the working tree only
  unless the work is tied to an active issue; never switch branches mid-build (the owner builds from the
  working tree) → [DEC-WF-no-commit-unmandated](architecture/decisions.md#dec-wf-no-commit-unmandated).

## 1. What S2S is (architecture at a glance)

**Stones2Stars (S2S)** is a Civ4 / Beyond-the-Sword / Caveman2Cosmos (C2C) mod, forked from C2C to rework
freely. The compiled artifact is **`CvGameCoreDLL.dll`** (the C++ engine + AI, `Cv*` engine classes /
`Cy*` Python wrappers), paired with **`Assets/XML`** data and **`Assets/Python`** callbacks. C2C→S2S save
compat is an explicit non-goal; only the closed Firaxis EXE binds.

**The dominant in-flight work is the #428/#430 cascade rework** — the reason most of these docs exist:

- **The problem it solves:** decades of ad-hoc BTS/C2C "maintainer" machinery (~7–8k lines) that pushes
  building/unit effects around imperatively, opaquely, and bugs included.
- **The replacement:** three **top-down machines** behind interface contracts —
  **enabler** ("can I?" — generate-then-gate), **modifier** ("how much?" — magnitudes deposit DOWN), and
  **tally** ("how many?" — counts roll UP) — fed by an **event spine**.
- **The data move:** game data migrates **XML → JSON** (`Assets/Data/**`) via **offline Python curators**
  (`Tools/Migration/`). Values are human-readable in JSON; the ×100 fixed-point conversion lives in
  **one** place (`readJson`). → [DEC-fixedpoint-x100](architecture/decisions.md#dec-fixedpoint-x100),
  [DEC-curator-owns-descale](architecture/decisions.md#dec-curator-owns-descale).
- **How it's proven safe:** you cannot delete a maintainer you cannot fully observe, so every behaviour
  gets a **shadow** that diffs the cascade against the live engine until clean, *then* the legacy is cut →
  [DEC-map-before-delete](architecture/decisions.md#dec-map-before-delete). The verification surface is the
  **observability / "Orwell" bar**: reconstruct game state from HTTP endpoints + `/events` + gated logs,
  **never the screen** → [DEC-obs-scale](architecture/decisions.md#dec-obs-scale).

**Architectural north star (the owner is a Clean-Architecture .NET dev at core):** depend on interfaces,
not concretions; isolate layers; compose behaviour via small contracts, not the inherited Civ4
god-classes. C++03 has no DI container, so wiring is **"poor-man's DI"** — an `if`/`switch` at a
composition root assigns a concrete to a contract pointer. Graft interfaces onto the DLL-internal derived
classes (`CvCityAI`/`CvUnitAI`/…), **never** widen an EXE-bound base (`CvCity`/`CvUnit`) →
[DEC-interface-contracts](architecture/decisions.md#dec-interface-contracts).

## 2. The domain map — what's documented, and how it relates

| Domain | Lives in | What it covers | Start at |
|---|---|---|---|
| **Architecture — the guiding core** *(read before any structural change)* | `architecture/` | the **north-star** (the two-sided engine; the landed core data-structures — cascaders, tally, Orwellian logging) + the landed **design patterns** (faking-DI, composability) + the **decisions** ledger | [`architecture/README.md`](architecture/README.md) |
| **Cascade & info-handling** *(the current work)* | `reference/cascade/`, `explanation/` | the 3 machines + spine + tally; readJson; the fixed-point **scale registry**; constructibility/prereqs | `explanation/cascade-architecture.md` |
| **Data model & migration** | `reference/cascade/`, `json-migration/` | the JSON shape; the curators; XML→JSON entity migration; rename registry | `reference/cascade/data-model.md` |
| **Observability** *(the verification surface)* | `reference/observability/` | the surveillance maps per game-system; PlotSnapshot; the HTTP server; the logging surface | [`reference/observability/README.md`](reference/observability/README.md) |
| **Engine core state** | `reference/engine/` | the info/state classes the rework touches (`CvCity`/`CvPlayer`/`CvPlot`/…); save format; properties | `reference/engine/README.md` |
| **Cross-cutting decisions** | [`decisions.md`](architecture/decisions.md) | every binding ruling, ID'd, with its authoritative home | [`decisions.md`](architecture/decisions.md) |
| **Plans / roadmap** | `plans/` | in-flight scope, status, removal maps, dead-code passes | `plans/README.md` |

**The AI domain (out of *active* scope — kept, partitioned).** It is orthogonal to the info-handling
rework, so an agent on the cascade work is not routed through it — but the two halves are treated
differently (owner ruling 2026-06-19, *"ideas that have not been killed should be kept"*):
- **AI-behaviour REFERENCE docs** (how `CvCityAI`/`CvUnitAI`/worker/contract-broker *decide today*) are
  **retired** — they're read straight from `Cv*AI.cpp`, drift fast, and referenced already-fixed code.
  Rebuilt from code, grounded, when a task needs them; not carried as stale copies.
- **AI-DESIGN plans** (the AI north-star, the future reworks) are **kept**, in `plans/ai/` — forward intent
  is not reconstructible from code, so being merely out-of-scope is never a reason to drop it.

See `_meta/build-plan.md` §2a / §2a-bis for the exact split.

## 3. Reading orders — by what you're about to do

> Each list is the *minimum* to hold the relevant design before touching it. The non-negotiables (§0) are
> assumed read every time.

- **Make any structural change (new component, refactor, dissolve a god-class):**
  [`architecture/north-star.md`](architecture/north-star.md) — the grain of the wood; build with it, not
  against it.
- **Understand the cascade rework (general):** `explanation/cascade-architecture.md` →
  `reference/cascade/data-model.md` → the three machine specs → skim [`decisions.md`](architecture/decisions.md).
- **Touch the modifier machine / a value calc:** `reference/cascade/fixed-point-and-scales.md` (the
  **scale registry** — read it before any value work) → the modifier spec → `legacy-value-calc-map` →
  the relevant `reference/observability/` map for the system you're changing.
- **Migrate an entity XML→JSON (a curator pass):** `reference/cascade/data-model.md` →
  `reference/cascade/fixed-point-and-scales.md` → [`json-migration/`](json-migration/README.md) (the entity-ranking
  + the [rename registry](json-migration/migration-renames.md)) → the `Tools/Migration/` README.
- **Add observability for a system:** [`reference/observability/README.md`](reference/observability/README.md)
  (the scale + the three canonical hook shapes) → that system's map → the HTTP-server + logging-surface
  reference.
- **Change save/serialization:** `reference/engine/save-load-format.md` →
  [DEC-save-remove-is-soft](architecture/decisions.md#dec-save-remove-is-soft) →
  [DEC-tally-serializes-nothing](architecture/decisions.md#dec-tally-serializes-nothing).

## 4. The promise this doc set makes

Every behavioural claim here is **cited to live code and verified when written** — never reconstructed
from memory or another doc. Citations carry `file:line`; line numbers drift, so confirm the *function*,
not the integer. Where something is unverified, it says so. Cross-cutting rulings live once in the ledger
and are linked, never restated. This is enforced by [`_meta/CONVENTIONS.md`](_meta/CONVENTIONS.md) — the
standard that keeps the rebuild from rotting back into the thing it replaced. If a doc bites you, the
doc — not your assumption — is the bug; fix it in the same change.
