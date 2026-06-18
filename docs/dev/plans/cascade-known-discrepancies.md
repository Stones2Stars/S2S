# Cascade — KNOWN DISCREPANCIES (shadow vs. live game)

**Purpose (owner 2026-06-18): a living list of every place we KNOW the #430 cascade shadow differs from the live
(legacy) game** — so each is a deliberate DECISION, never a surprise at cutover. For each: decide to **CHANGE** (the
cascade's behaviour is the one we want — accept/keep the divergence as a correction) or **ALIGN** (the cascade must
match legacy — fix it, at latest by the hard switch). Nothing here is "discovered late": it's tracked the moment we
see it.

This is the companion to two other artifacts:

- the **`/diagnostic/sweep`** (buildability map: cascade verdict vs `canConstruct`/`canTrain`) — its *open* divergences
  feed §A below;
- the **§14 H state-maintainer demolition list** (enabler-cascade-spec) — the runtime behaviours the sweep can't see
  feed §B.

**Status legend:** ✅ verified-MATCH (no divergence, recorded so we don't re-investigate) · ⚠ KNOWN-GAP (real
divergence, disposition set) · ❓ UNDIAGNOSED (sweep shows it, cause not yet pinned) · 🔭 UNSHADOWED (no tool exercises
it yet).

---

## A. Buildability divergences — FULLY CAUSE-TAGGED by the reason-reporters (2026-06-18: 28 diverge = 27 over + 1 under)

The on-demand `legacyReason`/`cascadeReason` reporters (http-server.md) tag every divergence with its first failing
gate, so this is the **COMPLETE map — zero `other`.** (This superseded ~310→28 of guess-based clustering; the
diagnostic is the single biggest time-saver here — see the process note at the bottom.)

| reason (count) | What | Disposition |
|---|---|---|
| **`alreadyQueued` (12)** | the building is already in the city's build queue (`getFirstBuildingOrder`); the cascade correctly reports it as constructible | ✅ **ACCEPTABLE** — the cascade is *right*; queue-filtering is a UI layer, not a cascade gate. CHANGE-accept |
| **`replaced` (10 → 4)** | a successor (a building whose `replaces.buildings` names this one) is active in the city → legacy blocks (CvCity.cpp:2917) | ✅ **FIXED** (`cascadeIsReplacedInCity` — the verdict's destructive `replaces` subtraction; 6 cleared). **4 remain** (`BUNKER`/`COAL_PLANT`/`ELECTRIC_CHAIR`/`PINBALL_ARCADE`): legacy's 2nd branch (CvCity.cpp:2924) hides a predecessor under `MODDEROPTION_HIDE_REPLACED_BUILDINGS` (a toggleable BUG option, owner has it ON) when the successor is merely CONSTRUCTIBLE (not built). ✅ **ACCEPTABLE — a UI display LAYER, not a frontier gate** (same class as `alreadyQueued`): the cascade frontier correctly includes them (they *are* constructible); the UI applies `HIDE_REPLACED` on top, reading the option. The frontier-vs-display separation is the cleaner model; do NOT bake it into the verdict |
| **`latitude` (2 → 0)** | `CAMP_MOOSE`/`SALT_EVAPORATION_WORKS` — the cascade dropped `{latitude:{min,max}}` (no evaluator) | ✅ **FIXED** — `PRED_LATITUDE` (data-model §2.5 parameterized predicate, NOT an atom — the re-read corrected this) + `CvPredicate` gained `iMin`/`iMax` range bounds (owner-approved; reusable for spawnable-animal latitude + `{natureYield:{…}}`). Absent bound = unbounded. Both cleared (verified). |
| **`prereqOrBuildings` (2)** | `BURIAL_TRADITION_BONE/MUMMY` — legacy's OR-building group requires one **active & non-obsolete** (CvCity.cpp:2883); the cascade's building atom waives obsolete | ⚠ ALIGN — scope the obsolete-waiver out of OR-building groups |
| **`location` (1)** | `FARM_SUPPLY` — `isValidBuildingLocation` (CvCity.cpp:18494). NOT a monolithic gate: it's a bundle of independent location conditions (water/coastal, area-size, river, terrain, freshwater, map-category), each → an EXISTING cascade predicate. For `FARM_SUPPLY` it reduces to the **`MapCategoryTypes=[MAPCATEGORY_EARTH]`** check (its `iMinAreaSize` defaults to 0 → area passes; no water/river/terrain fields). | ⚠ ALIGN — DECOMPOSE `isValidBuildingLocation` into the predicate model (owner 2026-06-18: "a bolton modellable by existing gates"): add a **map-category predicate** (curator emits `MapCategoryTypes`; cascade checks the plot's categories) + an area-size atom. No special location gate. |
| **`requiresBuild` (1, UNDER → fix pending verify)** | `LEECH_CATCHER` — `requires.build = TECH AND (healer-building OR TERRAIN_MARSH)`. `PRED_HAS_TERRAIN` checked only the city CENTER plot; legacy's terrain prereq is **radius-scoped** (`isValidTerrainForBuildings`, CvCity.cpp:20402, owned plots) and the `ConstructCondition` `GOM_TERRAIN` is **worked-plots** (CvGameObject.cpp:1241, `RELATION_WORKING`) — two legacy semantics, both → `{HAS_TERRAIN:X}`. | ✅ **FIXED per enabler-spec §8 VICINITY** — `PRED_HAS_TERRAIN` now scans the city's **current workable radius** (`getCityIndexPlot(0..getNumCityPlots())`, culture-grown), **no ownership/worked filter** → the broadest clean barrier, deliberately MORE PERMISSIVE than both legacy variants (the §8 overlap model; tighten later via `workedBy:SELF` if needed). Expect new cascade-more-permissive over-offers vs legacy = **CHANGE**, not bugs. Verify next sweep. |

**Already FIXED + verified this session** (the bulk of the original 310): `notConstructible` (cost==-1, ~234),
religion gates (`STATE_RELIGION_IN_CITY` + specific state religion, 14), building-group inheritance (TechPrereq +
ObsoleteTech), `bWater→IS_COASTAL` (~9 coastal under-offers), and the **operate-OR-civics** `any`-hoist (13 `civics`).
*(The earlier guess-based A1–A5 framing is superseded — the reporters disproved the obsolete-runtime and bonus-connection
guesses; the real over-offer causes are `replaced` + a few small predicate gaps.)*

## B. Runtime-maintainer behaviours (the sweep CANNOT see — it tests buildability, not already-built things)

These are the §14 H state maintainers. The buildability sweep excludes built things (`!hasBuilding`), so a maintainer
matching/​diverging is **unverified until it has its own behaviour shadow.**

| # | What | Status | Disposition |
|---|---|---|---|
| B1 | **Religiously-limited dormancy** — a built state-religion wonder under no matching state religion. Legacy `setReligiouslyLimitedBuilding` (`processBuilding ∓1`, reversible) ↔ cascade `requires.operate` dormancy | ✅ MATCH (verified 2026-06-18) | keep; recorded so we don't re-litigate |
| B2 | **`hasAllReligionsActive()` exemption** — under an all-religions-active civic/trait, legacy keeps a built religious wonder ACTIVE (CvCity.cpp:14975); the cascade operate gate would dormant it | ⚠ KNOWN-GAP | currently MOOT (no current civic — Secularism/Free Church/Egalitarian all `AllReligionsActive=None` — grants it). ALIGN at switch via a `hasAllReligionsActive` waiver clause IF any civic/trait ever sets it |
| B3 | **Property-band placement** (`checkPropertyBuildings`, crime/disease/pollution) — per-turn add/remove on a value band ↔ cascade end-state = `requires.operate` property-in-band dormancy | 🔭 UNSHADOWED | property gate **replaced at the hard switch** (PropertyEffect, enabler-spec §3). Build a maintainer shadow before deletion |
| B4 | **autoBuild placement** (per-turn autobuild loop) ↔ cascade end-state = `enables` + `autoBuild` placement marker | 🔭 UNSHADOWED | wrangle at switch (data-model §4.2b). Needs a placement shadow |
| B5 | **Resource dormancy** (`PrereqBonuses` via `isActiveBuilding`) — losing a continuous resource ↔ cascade `requires.operate` | 🔭 UNSHADOWED | many bonus prereqs currently sit in `requires.build` (grey-only) but should `operate` (Phase-F build-vs-operate). Needs a shadow |

## C. Intentional / interim divergences (cascade differs BY DESIGN — recorded so they're not mistaken for bugs)

| # | What | Status | Disposition |
|---|---|---|---|
| C1 | **`notConstructible`** (cost==-1) — the cascade hides these from the player build list; the property/autobuild/outcome systems still PLACE them via `canConstruct(bIgnoreCost=true)` | ✅ MATCH (interim) | correct interim; effect buildings formalize OUT to `PropertyEffect` at the switch (data-model §4.2b) |
| C2 | **State-religion gate in `requires.operate`** (adds dormancy) — matches legacy's religiously-limited (B1), not a build-only gate | ✅ MATCH | keep |

---

*Process: when the sweep surfaces a divergence, diagnose it; if it's a real behaviour difference, it lands here with a
disposition (CHANGE / ALIGN) rather than being silently "fixed to match." A maintainer (§B) is not "done" until it has a
behaviour shadow AND its row is resolved. The hard-switch demolition (enabler-spec §14) is complete only when every row
here is CHANGE-accepted or ALIGN-done.*
