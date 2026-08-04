# Memory footprint — where the RAM goes (the 32-bit ceiling)

> **Reference — how the running DLL uses memory today.** The `CvGameCoreDLL.dll` is 32-bit and shares the
> process address space with the closed Firaxis EXE, so it lives under a **~3.2 GB ceiling** (a `bad_alloc`
> exit near it is the failure mode — [engine.md](engine.md), [roadmap §Verification](../plans/structural-cleanup/roadmap.md)).
> Measured anchor: a late-game save (era 3, **185 cities / ~9,200 units / ~9,600 plots / 17 live players+teams**)
> sits at **~2.2–2.4 GB working set** and **climbs ~+150–230 MB per turn-end**, and **the climb resets on reload**.
> This doc attributes that number to structures, and answers "are textures loaded once or per-instance?".
> ⛔ The only EXE-side window was the `workingSetMB`/`peakWorkingSetMB`/`pagefileMB` gauge (`GetProcessMemoryInfo`),
> and **its route went with the route-table purge** — so there is currently NO way to read the process working set.
> Every measurement this doc calls for needs that gauge re-emitted first, as a spine event rather than a route
> ([observability.md](observability.md), [../specs/http-endpoints.md](../specs/http-endpoints.md)).

## The big picture — three FLAT static clusters (~126 MB), and a per-turn GROWER

| Cluster | ~MB (late-game) | Grows per turn? | What it is |
|---|--:|:--:|---|
| Resident **info classes** | ~55 | **No** — immutable after load | the boot-loaded `CvJson<X>Info` tables (§1) |
| Per-object **info-sized arrays** | ~31 | **No** (at fixed object/player count) | `CvCity`/`CvTeam`/`CvPlayer` arrays dimensioned by info counts (§2) |
| **Cascade/enabler/property** derived state | ~40 | **No** — recomputed in place | the derived caches (§3) |
| **Art / textures / icons** | ~0 in the DLL | **No** | loaded ONCE, shared; the DLL holds no pixels (§4) |
| **The per-turn climb (+150–230 MB)** | — | **YES**, resets on reload | turn-processing allocation **churn + 32-bit heap fragmentation** (§5) |

> **⚠ SCOPE CAVEAT — this doc sizes the DLL's OWN data structures, NOT the whole process.** `workingSetMB` is
> **process-wide**; the DLL structures below are a *small slice* of it (~126 MB of ~2 GB). "The DLL uses little" and
> "the process is 2 GB" do **not** contradict — the bulk lives in surfaces static code analysis **cannot see**:
> (1) the **EXE-side Gamebryo scene/graphics** (the biggest; grows as the map is revealed early→late — §5),
> (2) the **Python 2.4 heap** (Revolution/events/UI — entirely uncounted here), (3) **save/serialization buffers**,
> (4) **heap fragmentation**. Attributing the ~800 MB→2 GB early→late growth needs a **delta MEASUREMENT**
> (working-set at an early-game state vs the late save, which needs the process gauge re-emitted), NOT more struct estimation —
> that is the honest open question this static audit does not answer.

**Headline:** none of the big *DLL* structures is the per-turn climb. The three static clusters sum to **~126 MB
(~5 % of the working set)** and are flat. The useful conclusion is the *inverse* of what it looks like: **DLL-side
trimming (the unitcombat purge, flattening the 2D arrays) buys single-digit MB — it is NOT the lever.** The
~800 MB→2 GB is EXE scene + Python + fragmentation (§5). The one thing this audit *does* settle: the DLL is not
where the memory goes, so the per-turn climb — whatever its exact split — is dominated by legacy turn-processing +
EXE-side churn, which is why the memory hunt is sequenced *after* the legacy cut
([DEC-legacy-decache-poisons-perf](../architecture/decisions.md#dec-legacy-decache-poisons-perf)) and confirmed by a
delta measurement, not static estimation.

---

## 1. Resident info classes — ~55 MB (~2 %)

Boot-loaded once, immutable after load (`CvInfo.h` "write-once-at-load"), so they **cannot** be the per-turn climb.

| Category | Count | ~total MB | Notes |
|---|--:|--:|---|
| **buildings** | 5,202 | **~28** | ~half the info footprint — the largest count AND the fattest struct (~2.8 KB inline) |
| units | 2,073 | ~9 | |
| promotions | 1,229 | ~4 | ~150-field near-mirror of unitcombat |
| techs | 943 | ~3.4 | heap-heavy (rich requires/enables trees) |
| bonuses / traits / unitcombats / … (rest) | ~4,400 | ~10 | unitcombats now 470 (was 814) |
| **TOTAL** | ~12,800 | **~55** (range ~44–74) | estimate; method below |

**Key structural fact (corrects a common assumption):** the `CvJson<X>Info` pocos do **not** hold dense arrays
sized by other-info counts. Keyed data uses **`IDValueMap` = an inline `vector<pair<id,value>>`** (sparse, heap
only for authored entries). The dense `int**`-sized-by-`NUM_<other>_TYPES` arrays are in the **archived legacy**
classes (`SourceArchive/Infos/CvBuildingInfo.h`) — the JSON migration replaced them with sparse maps, so the pocos
are *leaner* than what they replace. The per-instance tax that remains is the **materialized modifier scalar block
(~130 ints) + ~50 empty container heads** carried on every building whether authored or not.

*Method: `A` inline-struct floor (member counts × count, 32-bit widths) + `B` authored-heap (on-disk JSON bytes ×
~0.55). The soft part is `B`'s factor; counts and on-disk bytes are exact. A live `sizeof`/heap-walk probe would tighten it.*

---

## 2. Per-object info-sized arrays — ~45 MB, and the fragmentation problem

Every `CvCity`/`CvTeam`/`CvPlayer`/`CvUnit` allocates arrays dimensioned by info counts (`new T[GC.getNum*Infos()]`)
in its `reset()`/`init()`. At the live counts this is **~26 MB data + ~5 MB tiny-block overhead ≈ 31 MB (~1.5 %)**.

| # | Array | Class | Dimension | Total | Note |
|--:|---|---|---|--:|---|
| 1 | Bonus family (`…ExtraBonusAidModifier` …) | CvCity | Bonus 902 (one ×Property) | 7.7 MB | largest survivor; 902 tiny `int[7]` allocs per city (`CvCity.cpp:712`) |
| 2 | `m_paiUnitProduction` + 2 GP-rate | CvCity | Unit 2073 ×3 | 4.6 MB | |
| 3 | `m_ppiBuildingCommerceModifier` | CvTeam + CvPlayer | Building×Commerce 5202×4 | 3.5 MB | the one remaining Building-outer 2D array |
| 4 | 6× UnitCombat arrays | CvCity | UnitCombat 470 ×6 | 2.1 MB | the unitcombat 814→470 drop reclaimed ~1.55 MB here |

**The real cost is fragmentation, not bytes.** A Building-outer 2D array (`int**` with 5,202 rows) allocates
**~5,200 tiny heap blocks per owner** — so `m_ppiBuildingCommerceModifier` alone is ~177k blocks across 17 teams +
17 players — heavy allocator overhead and address-space fragmentation, disproportionate on a 32-bit heap, over data
that is almost entirely zero. Flattening it to one contiguous block (or sparse storage) would reclaim the overhead
and cut fragmentation. **This cluster also scales with PLAYER/TEAM count:** the 2D arrays attach to every
*initialized* slot, so the figures above roughly triple at the full 51 `MAX_PLAYERS`/`MAX_TEAMS`.

⚑ **The accumulator cut is what shrinks this cluster, and it is the lever that actually works here** — a
Building×Specialist array on `CvTeam` was on its own the single largest entry in this table, and cutting the
accumulator took its ~88k tiny blocks with it ([DEC-accumulator-cut-uniform](../architecture/decisions.md#dec-accumulator-cut-uniform)).
Each further accumulator dimensioned by an info count pays back the same way, which is worth knowing while
weighing a cut — though §5 still holds: this whole cluster is not where the process memory goes.

---

## 3. Cascade / enabler / property derived state — ~40 MB, flat

Maintained in place by the facts, into fixed-width storage; never grows per turn.

| Structure | Per object | ~total | Note |
|---|---|--:|---|
| **CityEnabler** tri-state arrays | ~42.6 KB/city | ~14.6 MB | (5202+2073 ids × 6 B). **~5× the spec's stale "8.5 KB/city" estimate** — the shipped `EnablerDomain` adds two `short` refcount planes + a flags byte per id ([enabler.md §7.1](../specs/enabler.md) budget predates this) |
| **a dense per-building/per-unit keyed ledger** | ~30.4 KB/city + /player | ~10.4 MB | the shape to AVOID re-growing: a full-width `assign(5202,0)+assign(2073,0)` per touch is mostly-zero. Measured here so the space-for-time trade is made deliberately, under the [KEYS ONLY WHERE NEEDED](../architecture/state-repositories.md) ruling, not by default |
| Operating-building sets | ~6–10 KB/city | ~2.8 MB | scales with buildings-present |
| Plot yield cache + plot properties | ~150 B/plot | ~2.2 MB | |
| CvPlayer 8 history hash_maps | ~257 KB/player @T1338 | ~3.9 MB | **GROWING + serialized** (see §5) |

The whole derived surface is ~40 MB and **flat** — not the grower.

---

## 4. Textures & icons — LOADED ONCE (shared), not per-instance

**Verdict: once-shared.** The DLL holds **no per-unit or per-building texture/mesh copy** — only a tag string on
the (shared) Info, and per instance an index into the shared Info array.

- **Art defines (id→file map) load once** into the `ARTFILEMGR` singleton (`CvArtFileMgr::GetInstance`), one
  `vector<CvArtInfo*>` + `map<tag,ptr>` per art type, populated once at load.
- **A `CvArtInfo*` holds only path strings** (`m_szKFM`, `m_szNIF`, button path) — never pixel/mesh bytes. The EXE
  loads the referenced `.nif`/`.kfm`/`.dds`.
- **An Info stores only a tag** (`m_szArtDefineTag`); `getArtInfo()` → `ARTFILEMGR.get<X>ArtInfo(tag)` returns the
  **shared** pointer. `CvUnit::getArtInfo()`/`getButton()` delegate straight to the shared Info.
- **Button icons (DDS) are once** — one shared path per art-define (each button is its own DDS file, loaded once
  by the EXE, not a per-widget copy).
- **The one genuinely per-instance art object** is the DLL-side scene-entity handle `CvUnitEntity`, and it is
  **pooled/dynamic**: `ENABLE_DYNAMIC_UNIT_ENTITIES=1` gives a real entity only to on-screen units (plot center /
  active player), otherwise a shared `g_dummyEntity`; even a real entity references shared assets by path. It is
  counted + probed (`[PERF/entity]`).
- **EXE render side** (Gamebryo): **empirical evidence points to PER-INSTANCE texture memory, NOT a path-shared store**
  (owner observation) — graphics **PAGING** (unloading off-screen scene regions) yields **significant working-set
  reductions**, which a fully path-shared texture cache would *not* give (paging one instance wouldn't free a texture
  others still reference). So a spawned texture — **even a copy of the same art — appears to inhabit its own EXE-side
  memory**. This makes the EXE scene's per-instance texture/model footprint a **real memory lever** (it scales with
  live instance count + revealed tiles), and is the concrete mechanism behind §5's ⭐ prime-grower attribution. (Still
  closed-EXE / no-symbols: the mechanism is inferred from the paging delta, not a symbol read — but the paging delta is
  a real measurement, no longer pure reasoning.) The **DLL art surface is a separate question and stays shared-once**
  (the DLL holds only the tag string, above) — per-instance texture memory lives on the EXE side.
- **FPK:** art is delivered as packed `C2C*.fpk` archives via the external `FpkBuilder` — a packaging container,
  orthogonal to the once-vs-per-instance question ([external-tools-and-workflows.md](external-tools-and-workflows.md)).

**Implication:** the **DLL art surface is flat after load** (shared once — tag strings only), so a per-turn
working-set climb is not DLL art re-instantiation. But on the **EXE side** the per-instance texture evidence (above)
means scene texture/model memory **does** scale with live instance count + revealed tiles — so instance/texture count
IS a memory lever *there*, which is exactly §5's ⭐ grower.

---

## 5. Where the +150–230 MB/turn actually goes

The static clusters (§1–§4) are flat, so the climb is elsewhere. There are TWO distinct questions — the
**early→late baseline growth** (~800 MB→2 GB) and the **per-turn +150–230 MB**. Grounded candidates, biggest first:

- **⭐ EXE-side revealed-map scene + city/unit models — the prime early→late grower (black box, gauge-only).**
  Early game most of the 9,600 tiles are *unexplored* and not in the Gamebryo scene; late game the whole map is
  revealed → the EXE holds terrain/feature/improvement/route/river scene nodes for all 9,600 tiles **plus** the 3D
  models for 185 cities (each with hundreds of buildings) and thousands of unit models. This scales exactly with
  exploration + city/unit count and is almost certainly the bulk of the 800 MB→2 GB. Only observable via the
  `workingSetMB` process gauge (currently un-emitted) — the DLL cannot see inside the EXE scene.
- **Python 2.4 heap (uncounted).** Revolution, random events, the EventManager, and the UI hold Python state that
  grows with game state. Not measured by any DLL probe — a real blind spot.
- **Per-turn allocation churn + heap fragmentation (reload-resetting).** Pathfinding vectors
  (`CvReachablePlotSet`/path vectors alloc+free every pathfind), the property-solver full rebuild each `doTurn`, the
  a full-width keyed-ledger `.assign(getNumBuildingInfos())` realloc per touch. **Churn, not net accumulation** — but on a
  fragmenting 32-bit heap freed blocks aren't reused and the working set climbs; **resets on reload** (defrag),
  matching the owner's observation. The likely driver of the *per-turn* +150–230 MB.
- **Save/serialization buffers** — the whole game state serialized each autosave (large, transient).
- **True never-released DLL accumulators (small, ruled out as the big mover):** `CvPlayer`'s 8 history `hash_map`s
  (~3.9 MB @T1338, survive reload); `CvContractBroker::m_workRequests` (measured-bounded). KB-scale, not the climb.

**⚠ This split is REASONED, not measured.** Confirming it needs the delta measurement (scope caveat, top): capture
working-set at an early-game state vs the late save, and whether it climbs on a *paused* turn (→ fragmentation/Python)
vs tracks revealed-tiles/cities/units (→ EXE scene).

**Conclusion for the ceiling hunt:** the structural levers that exist are (a) **flatten the remaining Building-outer
2D array** (§2 — kills ~177k tiny blocks + their fragmentation), (b) the dense buildRate ledger (§3), (c) the
allocation-churn pooling in turn processing. But the per-turn climb is fundamentally **legacy turn-processing
churn**, so the sequencing ruling holds: finish the legacy cut first, then the climb is measurable and attributable
against the cascade rather than a legacy/cascade mix.

## See also
- [engine.md](engine.md) — the 32-bit/VC7.1 toolchain + the closed-EXE ABI that fixes the ceiling.
- [observability.md](observability.md) — the memory gauge and why its route is gone.
- [../architecture/state-repositories.md](../architecture/state-repositories.md) — the derived-cache model (§3).
- [../plans/structural-cleanup/roadmap.md](../plans/structural-cleanup/roadmap.md) — the parked memory hunt + the legacy cut it waits on.
