# Pre-hard-switch MAPPING INVENTORY — everything we need to understand + observe before the cutover

**Purpose (owner 2026-06-18): the design-table artifact — a living inventory of every system/behaviour we must
MAP (understand + make observable) before the #428/#430 hard switch**, so the demolition (enabler-spec §14) can't
silently break anything. The completeness bar (owner's metric): *"it should be theoretically possible to completely
render the game purely by reading data from the APIs"* — read-only by design (no commands via API, the OOS-safety
guarantee). If a system can't be reconstructed from the API + logs/events, it's an unmapped gap.

> **⛔ THE TOTAL-OBSERVABILITY ("1984 / Orwell") BAR — owner ruling 2026-06-18, the load-bearing WHY of this whole
> initiative.** The events + logging + diagnostics must make the running game *fully surveilled*: **"map out an
> accurate gamestate purely by using the endpoints and logs without having to actually look in the game."** We will
> still **open** the game (it must run), but **never need to look at the game SCREEN** to know what is going on —
> the agent reads the live state entirely from `/diagnostic` + `/cities`/`/players`/`/units` + `/events` + the gated
> logs. Two reasons this is non-negotiable, not a nice-to-have:
> 1. **It is the ONLY way to reliably REBUILD the game logic on the new setup.** You cannot safely replace a state
>    mechanism you cannot fully observe — total observability is the *verification substrate* for the §14 demolition
>    (map-before-delete: prove the cascade replicates a maintainer, turn over turn, before deleting it).
> 2. **The end goal: the agent SEES what is going on, so we can accurately replace ALL the state logic to fit the
>    cascaders + the tally.** Every state maintainer (§14 H) becomes a `requires`/`enables`/`autoBuild` fact the
>    cascade evaluates and the tally counts — and we only trust that replacement because the shadow proved it matches.
>
> So: a behaviour that is not observable-from-outside is, for this rework, *not done*. Every shadow (the buildability
> sweep, the auto-placement `placementSweep`, the future dormancy/resource shadows) is one more lens toward this bar.
>
> **TEST ACROSS AI PLAYERS, not just the human (owner ruling 2026-06-18).** Shadow testing so far has leaned on the
> human player (`player=0`) because the owner can cross-check against the in-game screen. But that reliance is exactly
> what this bar removes: the endpoints take a `player=N` and work for ANY player, so verification MUST eventually sweep
> the **AI players** too — they are the players whose state the owner *cannot* watch, so they are where endpoint-based
> observability earns its keep (and where AI-only state divergences would otherwise hide). Confirmed live 2026-06-18:
> `placementSweep` returns correctly for AI players (e.g. player 1, 35 cities). Make AI-player sweeps a standard part of
> every shadow's verification, not an afterthought.
>
> **AI players are the CLEANER test, not just an additional one (owner ruling 2026-06-18).** An AI player has **no
> BUG-option / UI display layer** — none of the human-only filters (`HIDE_REPLACED_BUILDINGS`, build-queue hiding,
> per-option toggles) sit between the cascade and the engine's real state; for the AI, *everything has to actually be
> there*. So an AI-player sweep is a **purer comparison of the cascade logic vs the engine truth**, free of the
> UI-layer noise that produced the human-player buildability sweep's "16 UI-acceptable" divergences (`alreadyQueued` /
> `replaced`-via-`HIDE_REPLACED`, known-discrepancies §A). Lean on AI players to tell a *real* logic divergence apart
> from a human-UI artifact.

Companions: enabler-spec **§14 H** (the state-maintainer demolition list), **cascade-known-discrepancies.md** (the
cascade-vs-legacy divergences), the **http-server** diagnostics (the read surface), and the **event spine** (the
live stream). This doc is the *superset* — the full "what must be mapped" list those feed into.

---

## A. OPAQUE GAMEPLAY SYSTEMS — owner flagged as "no clue how they work" (need investigation + observability FIRST)

These are the systems whose mechanics we do NOT currently understand well enough to map/mimic. Each needs: (1) a
read of the live code to document how it actually works → `docs/dev/reference/`, and (2) observability (logs/events/
API) so its per-turn behaviour is visible. (owner 2026-06-18, "for when we map it all".)

| System | Note | Status |
|---|---|---|
| **Food calculation — WASTAGE especially** | how surplus/consumption/wastage actually compute per city | ❓ unmapped |
| **Espionage** | the whole espionage economy/missions/points | ❓ unmapped |
| **Culture** | equilibrium model is KNOWN (owner helped design it); the rest of culture accrual/borders/flips is not | ◐ partial |
| **Religion spread** | how religion propagates between cities | ❓ unmapped |
| **Corporations — ADVANCED corporations especially** | spread, resource consumption, the advanced-corp rules | ❓ unmapped |

*(This list is a SEED, not exhaustive — add systems as they surface. A system the owner can't explain off-hand is
prima-facie unmapped and a priority for the "render-from-API" bar.)*

> **EXPANDED MAP 2026-06-18 — the multi-agent state-mapping sweep (8 Sonnet explorers, 219 behaviours) → durable in
> [`state-mapping-2026-06-18.md`](state-mapping-2026-06-18.md).** It confirms the owner's suspicion that the prior §14 H/§A
> mapping was **too shallow**: only **~15-20% of the per-turn/per-event state surface is observable today**, and it names
> opaque clusters the seed list above MISSED — **(a) city anger/happiness/TIMER fields** (≥8 timer decrements/turn, WLTK,
> war-weariness, occupation — these gate dormancy preconditions), **(b) building DORMANCY state** (resource-disabling +
> religious-disabling + replacement-suppression — `hasBuilding=true` but `isActiveBuilding=false`, invisible to `/cities`),
> **(c) the full culture pipeline** (plot culture arrays, city balance, revolt probabilities — only the discrete level tier
> shows), **(d) per-player FINANCE breakdown** (maintenance / unit-upkeep / civic-upkeep / hurry-inflation), **(e) AI
> DIPLOMATIC memory** (attitude counters, memory, contact timers, war-plan), **(f) CvTeam diplomacy** (deal verification,
> vote timers, circumnavigation, WW decay), plus the §A five (food wastage, espionage, religion spread, corporations). And
> **(g) NO `/tally` snapshot endpoint** → empire/team/world counts aren't reconstructible at a point in time.
>
> **RECOMMENDED NEXT SHADOW (from the sweep): B-ii DORMANCY** — the single highest-risk gap blocking the §14 switch. Build a
> `/diagnostic/dormancySweep` (+ `[DORMANCY]` per-turn line) analogous to `placementSweep`, emitting the per-city active/
> dormant split for all present buildings across the three mechanisms, comparing cascade `requires.operate` vs `isActiveBuilding`.
> *(Until it exists, B-i `placementSweep` is incomplete: a "cascade would not place" can actually be a present-but-dormant
> building — correct B-ii behaviour misread as a B-i divergence.)* Same pass: add per-city `religions[]` + player `activeCivics`
> to the endpoints (unblocks the `STATE_RELIGION`/`CIVIC_` operate-atom verification) and surface the anger/timer cluster.

## B. STATE MAINTAINERS (enabler-spec §14 H) — investigation 2026-06-18

The per-turn/per-event "decide a building's state" quirks. The buildability sweep does NOT exercise these (they act on
already-built/auto-placed things) → each needs its OWN behaviour shadow before deletion. Grouped by what they DO, with
current `file:line`, the cascade replacement, and the shadow the cutover needs. (Over-reach bias + map-before-delete: §14 H.)

### B-i. AUTO-PLACEMENT — per-turn `changeHasBuilding(true/false)` that MUTATES the building set (the riskiest)
Both run in `CvCity::doTurn`'s building-maintenance block (~CvCity.cpp:1455-1488) and both call the to-be-replaced `canConstruct`.
- **Autobuild loop** (CvCity.cpp:1459-1487) — over `BuildingsRepo::get().autoBuildings()`: if absent + `canConstruct(…,bIgnoreCost)`
  → ADD (+"auto-build" message); if present + non-wonder + a `PrereqNumOfBuildings` modifier dropped below threshold → REMOVE
  (the adopted-cultures rule; wonders `getMaxGlobalInstances()==-1` exempt from removal). → cascade **`autoBuild` placement marker
  + `requires`** (place when `requires` hold, drop when they go false; the owner's "autoBuild ≡ enables/requires slots" ruling).
- **`checkPropertyBuildings`** (CvCity.cpp:1490-1518, non-NPC only, called 1457) — for each PropertyType × each `PropertyBuilding`
  `{eBuilding,[iMinValue,iMaxValue]}`: ADD when the city's property value is IN-band + `canConstruct`, REMOVE when out-of-band or
  not constructible. These are the `BUILDING_EFFECT_*` "really effects, not buildings". → cascade **`requires.operate`
  property-in-band dormancy (the §3 PropertyEffect reverse-enabler; data-model §4.2b)** + autoBuild placement; formalizes OUT to
  `PropertyEffect`/`BaseEffect` (#429-adjacent).
- **SHADOW — ✅ BUILT 2026-06-18 (the auto-placement shadow, B-i).** Two surfaces, both gated `Autolog__HttpServer` /
  `gPlayerLogLevel`, comparing the cascade's *would-place* decision against the legacy maintainers' *realized presence*
  (`hasBuilding`, which after `doAutobuild` ⟺ what the maintainers placed) — a **presence diff**:
  - **Snapshot:** `GET /diagnostic/placementSweep?player=N` — per (auto-placed building × the player's cities):
    `{cascade, legacy, reason, kind}` (`kind` bitmask: 1 = bAutoBuild loop, 2 = property-band). `?type=full` adds the
    complete per-cell `all[]` array → the **total-observability** view (reconstruct the entire auto-placement state from
    the API alone, §A bar). Default = divergence triage list (cap 250) + summary.
  - **Per-turn stream:** `[PLACEMENT]` lines (Cascade.log + `/events`) every turn — headline counts at `gPlayerLogLevel≥1`,
    per-divergence at `≥2`. The runtime twin of `[READJSON]`, hooked in `CvGame::doTurn` (`cascadePlacementShadow`).
  - **Mechanism added:** the cascade now parses `identity.autoBuild` (the placement marker) + a `PROPERTY_X` band atom
    (`{type:PROPERTY_X, scope:city, min, max}` → the city's `getValueByProperty`) so a property-effect building's band
    can live in `requires.operate`. **Expected divergence today:** property-band buildings show `reason=noMarker` (their
    JSON isn't yet `autoBuild`-flagged / banded) — the shadow correctly flags the un-migrated set, *driving* the curation.
  - Code: `cascadeAutoPlacedRoster` / `cascadePlacementReason` / `cascadePlacementShadow` in `Sources/Cascade/CvCascadeReadJson.{h,cpp}`;
    endpoint in `CvHttpServer.cpp` (`placementSweep`). The §14 H deletion of these maintainers stays blocked until the shadow runs CLEAN.

### B-ii. DORMANCY — a BUILT building goes inactive-but-PRESENT when a condition fails
- **Religiously-limited** — `isReligiouslyLimitedBuilding` / `m_pabReligiouslyDisabledBuilding` / `setReligiouslyLimitedBuilding`
  (CvCity.cpp:21279-21319; the 14940-ish trigger), with the `hasAllReligionsActive` exemption; reversible `processBuilding ∓1`.
  → `requires.operate` (`STATE_RELIGION`/`STATE_RELIGION_IN_CITY` + a `hasAllReligionsActive` waiver clause). **B1 in
  known-discrepancies: MATCH verified 2026-06-18** (currently moot — no civic sets AllReligionsActive).
- **Resource dormancy** — `isActiveBuilding` (CvCity.cpp:14364) folding `PrereqBonuses` / `isDisabledBuilding` (`setDisabledBuilding`
  21239-21269) → `requires.operate` resource dormancy. The `setDisabledBuilding` replace/re-enable chain (§14 F) is the imperative
  twin to derive away.
- **SHADOW — ✅ BUILT 2026-06-18 (the dormancy shadow, B-ii; the sweep's #1-recommended next shadow).** `GET
  /diagnostic/dormancySweep?player=N` (`?type=full` = per-cell) + per-turn `[DORMANCY]` lines: per BUILT building per
  city, diff cascade `requires.operate` (`cascadeOperational`) vs legacy `hasFullyActiveBuilding`. One comparison covers
  all three legacy mechanisms — `legacyReason` ∈ `disabled` (`isDisabledBuilding`: resource + replacement-suppression),
  `religiousLimit` (`isReligiouslyLimitedBuilding`), `active`. B1 (religious) was already MATCH-verified; B5 (resource)
  now shadowed — expect cascade-active / legacy-`disabled` where bonus prereqs still sit in `requires.build` (the B5
  build→operate curation driver). Code: `cascadeDormancyReason`/`cascadeDormancyLegacyReason`/`cascadeDormancyShadow`
  (CvCascadeReadJson.{h,cpp}); endpoint in CvHttpServer.cpp.

### B-iii. GROUP GATE — `isSpecialBuildingNotRequired` (CvPlayer.cpp:13927; civic-driven count 18239)
The SpecialBuilding group cap/tech/obsolete/waiver. → uniform group-gate inheritance (data-model §7, the building-group deliverable;
the cap half + TechPrereq/ObsoleteTech inheritance already shipped this session). **SHADOW:** group membership active/waived parity.

### B-iv. WAIVER — `hasAllReligionsActive` (CvPlayer.cpp:30299; civic `isAllReligionsActive`)
The religion-exemption → a DECLARED `requires` waiver clause, not a buried `if`. Currently moot (no civic sets it) but must be a
defined fact pre-switch. Folds into B-ii's religion dormancy.

**⚠ The shadows above are the honest scope of "map every current behaviour" — a runtime twin of the buildability sweep, NOT yet built.**

## C. CASCADE DIVERGENCES (from cascade-known-discrepancies.md)

Every place the cascade shadow differs from the live game, cause-tagged via the `/diagnostic` reason-reporters. See
that doc; the diagnostics make each one a one-query diagnosis.

## D. Measuring the bar — AUTOPLAY sessions + THE OBSERVABILITY SCALE (owner 2026-06-18)

**The measurement vehicle = a complete AUTOPLAY session** (AI-only, no human in the loop). It is the truest test of the
total-observability bar precisely because there is **no human and no UI display layer**: every piece of state the AI acts
on must be readable from the endpoints/logs/events, or it is *invisible* to us — there's no screen to fall back on. Run
one or more full autoplay games, poll the endpoints + tee `/events` throughout (the benchmarking harness already does this
— `Tools/BenchmarkCensusCollector.ps1`, GameTracker), and grade how completely the run could be reconstructed from outside.

**THE OBSERVABILITY SCALE — oblivious → thought-police → Meta (a deliberately fun ranking; accurate under the humor, like
the DESPAIR/REALISM indexes).** ONE named ranking, used two ways: the **runtime LOG LEVEL** (the knob) and — here — the
**maturity RATING** (how much we have instrumented; what tier is reconstructable from the wire). **The scale is defined
canonically in [`../reference/ai-logging-reference.md`](../reference/ai-logging-reference.md) §0** — `0 Oblivious · 1
Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police · 5 Meta(reserved)`. This doc does NOT redefine it; it only
*rates us against it.* (Per tier, the reconstructable surface = the artifacts in §B/§C: snapshots → buildability shadows →
maintainer shadows + `[STATE/*]` feed → opaque-system internals.)

The hard switch (enabler-spec §14) wants the building/placement+dormancy surface fully covered (the top of **Big Brother**)
and a clear path into **Thought Police** on the opaque systems. An autoplay session we can fully narrate from the wire = bar met.

**SELF-ASSESS AFTER EVERY SESSION (owner 2026-06-18): rank our current observability against this scale, dated.**

- **2026-06-18 → TIER 3 (Big Brother), with a widening Tier-4 foothold (2 of ~4 maintainer clusters shadowed).**
  Live-verified this session against a running game, **human (p0) AND AI players (p1: 35 cities, p2)** — confirming the
  endpoints work screen-free for players we can't watch. We have: snapshots (T1), buildability shadows + reason-reporters
  (T2), the `/events` stream + the per-turn maintainer shadows `[READJSON]`, `[PLACEMENT]`+`placementSweep` (B-i), and now
  `[DORMANCY]`+`dormancySweep` (B-ii) (T3). **Toward Tier 4 (Thought Police):** of the §14 H maintainer clusters, **B-i
  auto-placement AND B-ii dormancy are now shadowed**; **B-iii group-gate** is not, the property-band cascade rep is parsed
  but un-curated (`reason=noMarker`), and B5 resource prereqs need the `build→operate` move (the dormancy shadow now
  surfaces them). The §A **opaque systems** (anger/timers, culture pipeline, religion spread, corporations, espionage,
  finance, AI diplo memory) — the rest of Tier 4 — have only a partial live surface so far (this session's `[STATE/*]` feed
  added game/finance/attitude/city-accumulation); see `state-mapping-2026-06-18.md` (~15-20% observable baseline) + the
  per-system `docs/dev/reference/observability/` maps for each remaining gap + its hooks.
- **To climb to Tier 4:** shadow B-iii group-gate; curate the property bands (kind=2 `noMarker` → real) + the B5
  `build→operate` resource prereqs (dormancy `disabled` divergences → clean), for all players. **To climb to Tier 5:**
  give each §A opaque system a live read surface + add the `/tally` snapshot endpoint (counts aren't point-in-time readable today).

- **TWO AXES — don't conflate them (clarified 2026-06-18).** The Tier-3 rating above is the **cascade/buildability
  surface** (sweeps + placement + dormancy). The **whole-game-state surface** is rated SEPARATELY by the 22-system net
  (`docs/dev/reference/observability/README.md`): it sat at **Tier 1 (Telescreen), with Health/Happiness + Espionage at
  Tier 0**, no system at Tier 2. This session's **LIVE STATE FEED + new endpoints** attack its top-5 gaps directly:
  `[GAME]` + `/diagnostic/game` (gap #1 end-detection), `[CITY]` accumulation layer (gap #2), `[FIN]` (gap #3 economy
  expense), `[DIP]` + `AI_getAttitudeVal` (gap #5 attitude) — lifting several systems off Tier 0-1 toward **Tier 2**.
  Still open on this axis: connected-bonus list per city (gap #4, resource-dormancy oracle), the corporation/religion-spread/
  culture-pipeline internals, and a real `/tally` snapshot. The per-system maps under `observability/` detail each hook.

---

*Process: each entry gets a `docs/dev/reference/` page documenting how it ACTUALLY works (read the code, don't guess
— the trust-but-verify rule) plus an observability hook (gated log + event + API field) so it meets the
render-from-API bar. The hard switch is "ready" on a system only when it is both understood and observable.*
