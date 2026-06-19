# Sources/ structural cleanup — campaign plan

> **STATUS: proposed 2026-06-19 — a BIG-HOLE note for the OWNER to execute (next session).** Per the session model
> (align/organize/find-holes: small holes plugged inline, big holes noted in the right doc + added to the end-of-session
> "fix now" list). Surfaced by the 2026-06-19 uncommitted-work audit (5-minion adversarial pass) + owner directives the
> same day. Nothing here is built yet; this is the scoped plan + the load-bearing ordering.

## 0. Why / scope (owner directives 2026-06-19)

- *"Tech debt and shortcuts are the root of all evil, particularly in this codebase. To be efficiently understood by
  agents and humans alike, we put the effort in now to do it right."*
- Remove the **`CvInfos.h` umbrella** ("we tripped over it a couple of times yesterday"). Already flagged for retirement
  in `AGENTS.md` (Conventions: import Info headers directly).
- *"Clear up all includes in general, so they are valid, and needed."*
- *"Align the `.vcxproj` file and folders, and actually set up a sensible folder structure."*

Goal: a `Sources/` tree whose layout, includes, and IDE projection all reflect reality and are self-explanatory.

## 1. The three big holes

### 1A. Logging-tag homes / enum collision-proofing — **do FIRST** (de-risks the rest)

The audit found **latent anonymous-namespace enum collisions that compile today ONLY because of FastBuild's current
unity-batch grouping** — i.e. shortcuts that work by luck:

- `Sources/CvCityLogTags.h` declares `enum CitField` (+`CitEvent`) at **GLOBAL** scope, leaking ~50 short tags
  (`CF_owner`, `CF_val`, `CF_pop`, …); `CvUnitAI.cpp`'s `ComField` **already** declares `CF_owner` too. No collision
  today only because the city batch and the unit batch are separate.
- `CvDecisionAI.cpp` (`DecisionField`) and `CvPlayerAI.cpp` (`DipField`) **both** inject `DF_player`/`DF_value` into the
  unnamed namespace. Safe today only by batch separation. (`CvDeal.cpp` avoided it via `_id`-suffixed aliases.)

In a unity build, two concatenated files that each declare `namespace { enum X {…}; }` inject into the SAME unnamed
namespace of that TU → **redefinition compile error**. `CvCityLogTags.h` exists precisely because `[CIT]` is emitted
from two co-batched files — it is the prototype of the fix, but it solved it by going *global* (trading an in-batch
redefinition for a global-scope-pollution hazard).

**Proper fix (do-it-right, not a throwaway rename):** every logging domain that has Event/Field tag enums gets a
**per-domain header home** with its enums in a **named namespace** (e.g. `namespace cit { enum Field {…}; }`), included
by every `.cpp` that emits that domain. Result: zero global/unnamed-namespace pollution, collision-proof under ANY
unity batching, greppable, and isolated per domain (the "isolate components" principle). This is also where the eventual
per-domain headers belong in the new folder structure (§1C).

### 1B. `CvInfos.h` umbrella removal + include hygiene (valid + needed)

- Retire the `CvInfos.h` aggregator; every site includes the specific `CvXInfo.h` / `Infos/CvXInfo.h` it actually uses.
- Sweep **all** includes to **valid + needed** (include-what-you-use): drop unused includes, add directly-needed ones,
  end transitive reliance.
- **⚠ HAZARD — the unity build HIDES missing includes:** a symbol resolves because *another* file in the same batch
  already included its header. So iwyu here is fragile: a "needed" include can look unnecessary, and removing the wrong
  one only breaks when batching changes. Verify by clean rebuild **and** by perturbing unity grouping (see §3).

### 1C. `.vcxproj`/`.filters` alignment + sensible folder structure

- **The `.vcxproj`/`.sln`/`.filters` are DEAD for the BUILD** (`fbuild.bff` is the source of truth — AGENTS.md hard
  rule). The owner's ask is to **ALIGN the IDE projection with reality** — a *display-accuracy* fix (the project files
  currently misrepresent the file set), NOT a build change. Keep `fbuild.bff` authoritative.
- **Set up a sensible folder structure.** Today: **169 loose `.cpp` + 193 loose `.h` at `Sources/` root**, with only
  `Cascade/` (16), `Infos/` (214), `Repos/` (4), `Utils/` (2) grouped. Proposed grouping — a **STARTING POINT for the
  owner to refine**, by responsibility (Clean-Architecture-ish layering):
  | Folder | Holds | Notes |
  |---|---|---|
  | `Core/` | engine domain objects: `CvGame`, `CvMap`, `CvArea`, `CvPlot`, `CvCity`, `CvUnit`, `CvPlayer`, `CvTeam`, `CvSelectionGroup`, `CvDeal`, `CvGlobals`, `CvGameCoreDLL`, … | the EXE-bound `Cv*` base layer |
  | `AI/` | `CvPlayerAI`, `CvCityAI`, `CvUnitAI`, `CvTeamAI`, `CvSelectionGroupAI`, `CvWorkerAI`, `CvHunterAI`, `CvDecisionAI`, `CvContractBroker`, `BetterBTSAI` + the **per-domain logging-tag headers** (§1A) | the DLL-internal AI derived layer |
  | `Cascade/` | (exists) the #428/#430 enabler/modifier/tally/spine engine | — |
  | `Infos/` | (exists, 214) the data `CvXInfo` classes | retire `CvInfos.h` umbrella here (§1B) |
  | `Repos/` / `Utils/` | (exist) | — |
  | `Python/` (or `Cy/`) | the `Cy*` Python wrappers + `Cy*Interface` | the Python-facing boundary |
  | `Combat/` | `CvCombatModel.{h,cpp}` | the combat module (memory: combat-model) |
  | `Observability/` | `CvHttpServer` | (spine logging consumer stays in `Cascade/`) — owner's call whether to split |
- **ABI-safe:** moving files changes *paths only*; the closed EXE binds symbols/vtables, not file locations. But moves
  change **relative `#include` paths** (→ dovetails with §1B) and **require `fbuild.bff` `.UnityInputPath` + the
  `.vcxproj`(+`.filters`) entries** for every new dir (AGENTS.md "adding a new source subdirectory").

## 2. Dependency ordering (LOAD-BEARING — get this wrong and the campaign breaks mid-flight)

Both **1B (include changes)** and **1C (file moves)** **reshuffle the unity batches** — which is exactly what would
**activate the 1A latent collisions**. Therefore the order is forced:

1. **1A first** — collision-proof the logging tags (per-domain named-namespace header homes). De-risks everything below.
2. **1C next** — folder structure + `fbuild.bff`/`.vcxproj`/`.filters` alignment; update the include paths the moves force.
3. **1B last** — `CvInfos.h` removal + the iwyu sweep, done **within** the new structure (the most batch-disruptive step;
   do it once, properly, verified by a clean rebuild).

Doing 1B/1C before 1A risks a build break from the latent collisions surfacing. (This is the single most important
finding of the audit for sequencing the owner's work.)

## 3. Verification (per step)

- **Clean Assert rebuild** after each step — the unity build hides missing includes, so an *incremental* build can lie;
  only a clean rebuild proves it.
- **After 1B:** perturb unity grouping (tweak `fbuild.bff` `UnityNumFiles`) and rebuild, to flush latent missing-includes
  the current batching masks. (Memory: "FastBuild unity grouping fragility" — adding a `.cpp` reshuffles batches and
  exposes latent missing `#include`s.)
- Pure structure → no behaviour change expected; a `FinalRelease` + in-game smoke after the campaign is prudent.

## 4. Medium holes (fold into the campaign or the fix-now list)

- **Dormancy shadow uncached per-turn JSON IO** — `cascadeDormancyShadow` calls the uncached `cascadeReadJsonAvailability`
  (ifstream + `FindFirstFile` dir scan) per distinct built building per turn when `gPlayerLogLevel>=1`; on a mature save
  that's hundreds of file reads/turn. Add a cross-turn parse cache. (Gated-off cost is nil; only bites when logging is on.)
- **CTB mixed-gate `/events` blind spot** (`logging-surface-inventory.md` A-16) — the `[CTB/work/intransit]` block is
  gated on `gUnitLogLevel>2` (every other CTB gate is `gPlayerLogLevel`) and its `%S` line stays legacy-only → a permanent
  `/events` blind spot. Align the gate + shadow the line (needs a runtime-string story; ties into the ~31 unrecoverable
  lines decision in `event-spine-spec.md` §8).
