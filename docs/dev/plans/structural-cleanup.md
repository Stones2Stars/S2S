# Structural cleanup — `Sources/` tree + dead-code/dead-XML pass

> **Status:** plan   ·   **Verified against:** `Sources/` tree + git working set, 2026-06-20
> **Grounding:** `ls Sources/` and per-bucket file counts; `grep` of the logging-tag prefixes and the
> `CvInfos.h` umbrella include sites; the migration scripts under `Sources/Tools/`.
> One-paragraph orientation: this is the status of two cleanups — (A) the `Sources/` source-tree reorg +
> include hygiene, which has **largely landed**, and (B) the dead-code / dead-XML removal pass, which is
> still **candidate-generation only**. Read this to know what is done versus what remains; do **not** read
> it as the original "we will restructure" plan — that move is finished.

**BLUF.** The big `Sources/` reorg is **done**: engine classes now live under `Sources/Engine/`, AI under
`Sources/AI/`, Infos under `Sources/Infos/`, plus `Defines/`, `Infrastructure/`, `Tools/`, `UI/`, `Python/`,
`Repos/`, `Utils/`, `Cascade/` — only 2 `.cpp` + 6 `.h` remain loose at the root (the PCH/DLL-entry glue).
The §1A logging-tag collision-proofing is **also done** (uniform `<DOMAIN>F_` prefixes + shared tag headers).
What **remains**: (1) the `CvInfos.h` umbrella retirement + include-what-you-use sweep (still pending, one
scripted attempt reverted); (2) the dead-code / dead-XML removal pass (candidates generated, no removals).
Per [DEC-proper-once](../architecture/decisions.md#dec-proper-once) the remaining work is done properly once,
not via a session-tail script.

---

## 1. Source-tree reorg (§1A + §1C) — DONE

### What landed

**Folder structure (was: 169 loose `.cpp` + 193 loose `.h` at `Sources/` root).** Files are now bucketed by
responsibility. Current layout (verified `ls Sources/`, 2026-06-20):

| Bucket | Holds | `.cpp` / `.h` |
|---|---|---|
| `Engine/` | EXE-bound `Cv*` domain objects (`CvGame`, `CvMap`, `CvPlot`, `CvCity`, `CvUnit`, `CvPlayer`, `CvTeam`, …) | 32 / 33 |
| `AI/` | DLL-internal AI derived layer (`CvPlayerAI`, `CvCityAI`, `CvUnitAI`, `CvTeamAI`, …) + per-domain logging-tag headers | 12 / 13 |
| `Infos/` | the data `CvXInfo` classes | 113 / 130 |
| `Infrastructure/` | infra/support | 27 / 50 |
| `Python/` | the `Cy*` Python-facing wrappers (`Cy*Interface`) | 41 / 20 |
| `UI/` | UI-facing | 24 / 23 |
| `Tools/` | in-tree tooling (incl. the migration scripts below) | 14 / 21 |
| `Defines/` | defines | 5 / 10 |
| `Cascade/` | the #428/#430 enabler/modifier/tally/spine engine | 8 / 8 |
| `Repos/` · `Utils/` | repositories · utilities | 2 / 2 · 1 / 1 |
| root (loose) | `CvGameCoreDLL.cpp`, `_precompile.cpp` + PCH/DLL-entry `.h` glue | 2 / 6 |

The exact final bucket split diverges from the old plan's *proposed* table (there is no separate `Core/` or
`Combat/` or `Observability/`; the engine base layer lives in `Engine/`, Python wrappers got their own
`Python/`). The proposal was explicitly "a starting point for the owner to refine" — the landed split is the
realized version, and **this table is now the source of truth**, not the old proposal.

**FastBuild + IDE projection.** `fbuild.bff` recursively globs every `.cpp` under `$SOURCE_DIR$`, so the moved
buckets are compiled automatically — no per-dir `.UnityInputPath` edit was needed (the recursive-glob change
predates the move; see root `AGENTS.md` "Adding a new source subdirectory"). The `.vcxproj`/`.filters` are
IDE-display only and were realigned to the new tree; `fbuild.bff` stays the build source of truth.

### §1A — logging-tag enum collision-proofing — DONE

The audit found latent anonymous-namespace enum collisions that compiled only because of FastBuild's
unity-batch grouping (two co-batched files each injecting e.g. `CF_owner`/`DF_player` into the same unnamed
namespace would be a redefinition error once batching shifted). The move would have reshuffled batches and
activated them, so this was the forced-first step — and it landed.

**Fix as realized:** every logging domain now uses a **uniform `<DOMAIN>F_` field prefix** (verified present
and unique: `CITF_ COMF_ CTBF_ DIPF_ DAIF_ ESPF_ ENGF_ WAIF_ WARF_ UNTF_ GRPF_ HAIF_ FNDF_`), and the
multi-file domains define their Event+Field enums **once in a shared tag header** rather than mirroring them
per-`.cpp`. `Sources/AI/CvCityLogTags.h` is the prototype: it carries `enum CitEvent` + `enum CitField`
(`CITF_*`) for the `[CIT]` domain, which spans `AI/CvCityAI.cpp` + `Engine/CvCity.cpp` (same unity batch).
No old short prefixes (`CF_owner`, `DF_player`, `UF_*`) leak any more (grep clean).

> **Stale flag (carried):** the old plan proposed putting each domain's enums in a **named namespace**
> (`namespace cit { enum Field {…}; }`). The realized fix instead keeps the enums at **global scope** but
> makes them collision-proof via the unique `<DOMAIN>F_` prefix + define-once shared header (see
> `CvCityLogTags.h:13,47`). The collision hazard is closed either way; the named-namespace form was not the
> shape that shipped.

### Migration tooling (kept)

`Sources/Tools/migrate_structure.py` holds the validated file→bucket move map used for §1C.
`Sources/Tools/retire_cvinfos_umbrella.py` + `Sources/Tools/fix_info_includes.py` encode the
`class→header` map and accessor-aware Info-include detection for §2 below — both still carry the flaw noted
in-script (they need a foundational/EXE-bound-header exclude list + header forward-decl handling before reuse).

---

## 2. `CvInfos.h` umbrella retirement + include hygiene (§1B) — PENDING

The `CvInfos.h` umbrella aggregator is **still present** (`Sources/Infos/CvInfos.h`) and still
`#include`d by **177** sites (verified 2026-06-20). Retiring it — every site including the specific
`CvXInfo.h` it actually uses — plus a full include-what-you-use sweep is the remaining structural work. It is
flagged for retirement in the root `AGENTS.md` Conventions ("Import Info headers DIRECTLY").

This is a **dedicated, hand-careful pass**, NOT a session-tail script run ([DEC-proper-once](../architecture/decisions.md#dec-proper-once)).
A scripted attempt on 2026-06-19 was reverted to green (it built down to a fragile tail). Hard-won lessons to
carry into the redo:

- **Scope:** of the umbrella's includers, ~**118 don't use any Info type** → pure dead-include removal (the
  easy win, near-zero risk). The PCH's own copy of the include is **commented out**, so the umbrella is not
  globally provided — retirement is real, not cosmetic.
- **Detect usage by ACCESSOR, not just type name.** Most files touch an Info type via `GC.getXInfo()` and
  never write `CvXInfo`. Scanning only for `\bCv\w+Info\b` under-adds (the reverted attempt hit 853
  undefined-type errors). Map **both** `\bCv\w+Info\b` AND `get<X>Info(` → `Cv<X>Info.h`.
- **⛔ Never inject Info includes into FOUNDATIONAL / EXE-bound headers.** Adding `#include "CvXInfo.h"` to
  `CvInfoBase.h`, `CvEnums.h`, or the EXE-bound core headers (`CvCity/CvUnit/CvPlayer/CvGame/CvTeam/CvPlot/
  CvGlobals.h`) creates include cycles ("base class undefined" cascades). Headers must **forward-declare**
  Info types used by pointer/ref; only by-value use needs the include → header retirement is hand-careful,
  not scripted.
- **The ART-INFO chain is special.** `CvArtInfo*` form an inheritance chain instantiated via
  `boost::is_polymorphic` inside `CvArtFileMgr.cpp`'s `ART_INFO_DEFN` macros — needs the art headers in
  dependency order. `CvArtFileMgr.cpp` is a file where **keeping the umbrella is the pragmatic choice**.
- **`CvInfos.h` is INCOMPLETE** — it omits `CvImprovementInfo.h` + `CvBonusInfo.h`, so even "include the
  umbrella" can miss types (this bit `PlotSnapshot` during the move).
- **Unity build HIDES missing includes** — a symbol resolves because a batch-mate already included its
  header. iwyu here is fragile; verify by clean rebuild **and** by perturbing unity grouping (tweak
  `fbuild.bff` `UnityNumFiles` and rebuild) to flush latent missing-includes the current batching masks.

**Recommended redo order:** (1) drop the umbrella from the 118 non-users (zero risk); (2) `.cpp` users →
specifics (accessor-aware); (3) headers → forward-decls (hand-careful, foundational headers excluded);
(4) special-case `CvArtFileMgr` + art chain; (5) clean Assert rebuild + `UnityNumFiles` perturbation each step.

---

## 3. Dead-code / dead-XML removal pass — CANDIDATES ONLY, no removals

A companion cleanup: strip the C2C-inherited zombie code and orphan data before any larger rework — you
don't want to port garbage. Tooling **generates candidate lists**; **nothing is auto-deleted**, and every
removal is personally verified against source/data and test-loaded against a save first. One subsystem per PR;
C++ Assert-build before each PR; `Tools/XmlValidator.exe -a` + `Tools/XMLTools/verify-python-callbacks.py`
after XML/Python changes.

> The detection tooling here (enumerate every tag the DLL reads, every `<Type>`, every `TXT_KEY_*`, every
> define) doubles as the data-model extractor the XML→JSON migration needs — build it reusably. **Note:** the
> JSON migration the old plan framed as "deferred north-star" is now the **active** work (see
> [cascade-migration.md](../json-migration/cascade-migration.md) / [cascade-architecture.md](../explanation/cascade-architecture.md)),
> so the dead-data pass feeds the live migration rather than a hypothetical future one.

### Tier 1 — high-confidence (verified 2026-06-11, no removals yet)

- **29** `//#if 0` disabled blocks inventoried (clustered: `CvGameCoreDLL.cpp` ×8 profiler instrumentation,
  `CvGameCoreUtils.cpp` ×8 pathfinding/profiling; rest scattered), plus **14** commented-out `logBBAI()`
  calls in `CvUnitAI.cpp` left from the deleted BBAI logging.
- **CONFIRMED DEAD** (no callers, no Python `.def()`): `CvCity::calculateTotalTradeYield` +
  `CvPlayer::calculateTotalTradeYield`; the billw-disabled stubs (`AI_doCentralizedProduction` call,
  the early-returned wonder-city block).
- **Dead info-class members** (batch, issue **#358**): `CvBonusInfo::m_piImprovementChange`,
  `CvHandicapInfo::m_szHandicapName`, `CvCivilizationInfo::m_iNumLeaders`,
  `CvEventInfo::m_iUnitPromotion`, and others. **Dead class** `CvDiplomacyTextInfo` (issue **#359**).
- ~~`Assets/Python/MapScriptToolsOld.py`~~ — **NOT DEAD, do not delete**: live `import` in two `PrivateMaps/`
  scripts.

### Tier 2 — audit required

- **ACO BUG-options surface — already REMOVED (2026-06-09)** (the only Tier-2 item actioned). The dead ACO
  options screen, its config XML, ~21 `TXT_KEY_BUG_OPT_ACO__*` keys, the `ACO_*` GlobalDefine block, and the
  Civilopedia concept were deleted once the preview became a pure `computeCombatPreview` renderer. **KEEP**
  (live, retained naming only): the `Art/ACO/*_bar_*.dds` bars and the `TXT_ACO_*` renderer text keys.
- **Dead-branch zombies** routed to the bug backlog, not bulk-deleted: `#105` (`m_bGameStart` never set true),
  `#139` (always-false `isNPC() and not isNPC()` guard), `#64` (inverted-while). Removed-game-option code paths.
- **Loader/merge bugs found by the #196 migration** (route via backlog): issues #352–#357.

### Tier 3 — data orphans (needs tooling; emits candidate CSV, auto-deletes nothing)

1. **Dead schema tags** — every tag the DLL reads (`GetChildXmlValByName`/`SetVariableListTagPair` literals)
   vs tags in the XML. Caveat: generic loaders + `BoolExpr`/`IntExpr` reads aren't literal-greppable → keep an allowlist.
2. **Orphan `<Type>` entries**. Caveats: savegame compat (removed Type can crash old saves — test-load);
   modular `z*`-prefixed XML; Type strings built by Python concatenation.
3. **Dead `TXT_KEY_*`** — defined vs referenced. Caveat: dynamic `"TXT_KEY_" + type` construction.
4. **Dead GlobalDefines** — **Tier-3.4 sweep done (2026-06-11):** 515 defines scanned, all call sites use
   literal strings (static analysis complete modulo the EXE). DLL-side-dead candidates fall in CAMERA / RENDER /
   AIR / FLAG / FOW engine-knob families that **may be read by the closed Firaxis EXE** — assume EXE-read and
   test in-game before deleting. (UI/BUG-option candidates are safer.)

### Hard caveats a detection script MUST account for

Boost.Python `.def()` bindings (a no-C++-caller function may be live via Python); generic tag loaders +
expression reads; modular `z*` XML appended to base lists; savegame compatibility (test-load after any data
removal); dynamic Type/`TXT_KEY` strings in Python.

**Suggested order:** Tier 1 (fast, low-risk) → Tier 3 tooling (candidate lists, reusable for the JSON
extraction) → Tier 2 audits. Small per-category PRs.

---

## 4. Medium holes (fold into the campaign or the fix-now list)

- **Dormancy shadow uncached per-turn JSON IO** — `cascadeDormancyShadow` calls the uncached
  `cascadeReadJsonAvailability` (ifstream + `FindFirstFile` dir scan) per distinct built building per turn when
  `gPlayerLogLevel>=1`; on a mature save that's hundreds of file reads/turn. Add a cross-turn parse cache.
  (Cost is nil when logging is off.)
- **CTB mixed-gate `/events` blind spot** — the `[CTB/work/intransit]` block is gated on `gUnitLogLevel>2`
  while every other CTB gate is `gPlayerLogLevel`, and its `%S` line stays legacy-only → a permanent `/events`
  blind spot. Align the gate + shadow the line (needs a runtime-string story; ties into the unrecoverable-line
  decision in the event-spine spec).

---

## See also
- [decisions.md](../architecture/decisions.md) — the ID'd ruling home; [DEC-proper-once](../architecture/decisions.md#dec-proper-once) governs the umbrella-retirement-as-careful-pass and the no-throwaway-shim posture for both cleanups.
- [cascade-architecture.md](../explanation/cascade-architecture.md) — the #428/#430 design the dead-data pass feeds (the XML→JSON migration is now active, not deferred).
- [README.md](../README.md) — the comprehension map / overview-of-overviews.
- root `AGENTS.md` Conventions — "Import Info headers DIRECTLY" (the umbrella-retirement directive) and "Adding a new source subdirectory" (the recursive-glob / `.vcxproj`-is-IDE-only build facts).
