# Observability — Vision & visibility — what's on the wire for fog-of-war & sight

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites re-confirmed in `Sources/Engine/CvPlot.cpp`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvTeam.cpp`, `Sources/Engine/CvGame.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvPlot.cpp` (`isVisible`, `isRevealed`, `changeAdjacentSight`, `updateSight`, `changeVisibilityCount`, `setRevealed`, `[ENG/viscap]`), `Sources/Engine/CvUnit.cpp` (`visibilityRange`, `getExtraVisibilityRange`, `isInvisible`), `Sources/Engine/CvTeam.cpp` (`isStolenVisibility`), `Sources/Engine/CvCity.cpp` (`getEspionageVisibility`), `Sources/Engine/CvGame.cpp` (`doTurn.visibilityRebuild`), `Sources/Tools/CvHttpServer.cpp` (`/units`). Carried over from the old draft survey; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> **Scope:** per-team plot visibility (fog of war), reveal state, unit sight ranges, invisible-unit gating (`isInvisible`), stolen visibility (espionage sight-sharing), city espionage/embassy visibility, and the Hide-and-Seek intensity model (`GAMEOPTION_COMBAT_HIDE_SEEK`).
> Vision is **Tier 1 (Telescreen)** today: `/units` snapshots unit positions and owners, but **every dimension of *what each team can see* is absent** from the endpoint layer. The only visibility log tag is `[ENG/viscap]` (level 2, fires only on erroneous negative counts — an anomaly indicator, not an observability hook). There is no sight-gain/loss event stream, no per-team reveal state in any endpoint, no invisible-unit flag in the unit snapshot, and no stolen-visibility/espionage-sight state anywhere. This map walks the per-team plot arrays → the sight-range formula → the per-plot sight inventory → the full scratch rebuild → invisible-unit gating → stolen visibility, names what's dark, and proposes hooks to climb to Tier 3/4.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. The core per-team plot arrays (`CvPlot.h`)

Each `CvPlot` carries per-team sight state, most of it lazily allocated:

| Array | Type | Allocation | Meaning |
|---|---|---|---|
| `m_aiVisibilityCount` | `short[MAX_TEAMS]` | always present | running count of sight sources covering this plot for each team; `> 0` → currently visible |
| `m_abRevealed` | `bool[MAX_TEAMS]` | lazy (first reveal) | whether this team has ever seen this plot (permanent; shrouded when 0) |
| `m_aiLastSeenTurn` | `short[MAX_TEAMS]` | lazy | the turn the team last had visibility; drives "last-seen" stale display |
| `m_aiStolenVisibilityCount` | `int16_t[MAX_TEAMS]` | lazy | vision piggybacked from a spied-on team's visible plots (espionage) |
| `m_apaiInvisibleVisibilityCount` | `short[MAX_TEAMS][NUM_INVISIBLE]` | lazy | per-(team, invisibleType) spotter count; `>0` → a spotter for that invisible type is in range |
| `m_aPlotTeamVisibilityIntensity` | `std::vector` | lazy, Hide-and-Seek only | per-spotter intensity entries for `GAMEOPTION_COMBAT_HIDE_SEEK`; read by `getHighestPlotTeamVisibilityIntensity` |

`isVisible(eTeam, bDebug)` (CvPlot.cpp:5152): `getVisibilityCount(eTeam) > 0 || getStolenVisibilityCount(eTeam) > 0`.
`isRevealed(eTeam, bDebug)` (CvPlot.cpp:9588): returns `m_abRevealed[eTeam]`.

### 1b. Sight-range formula (`visibilityRange`, CvUnit.cpp:10776)

```cpp
int iRange = 1 + pPlot->getTerrainElevation() + getExtraVisibilityRange();
if (pPlot->getImprovementType() != NO_IMPROVEMENT)
    iRange += GC.getImprovementInfo(pPlot->getImprovementType()).getVisibilityChange();
return std::min(GC.getMAX_UNIT_VISIBILITY_RANGE(), iRange);
```

- `getTerrainElevation()`: 0 flatland, 1 hills, 2 peak.
- `getExtraVisibilityRange()` (CvUnit.cpp): unit base extra sight + **promotion bonuses** + commander/commodore
  bonus (the command-point chain). (This is the promotion link — see [`promotions-xp.md`](promotions-xp.md).)
- improvement `getVisibilityChange()`: XML-defined; fortresses/watchtowers grant +1/+2.
- `MAX_UNIT_VISIBILITY_RANGE`: an XML define cap (~6).

### 1c. Sight-radius update (`changeAdjacentSight`, CvPlot.cpp:2606)

Called when a unit moves or is created/destroyed. For each plot in the `[−iRange, iRange]` square that
passes the line-of-sight test (`canSeePlot`/`canSeeDisplacementPlot`), calls
`changeVisibilityCount(eTeam, ±1, eInvisible, …)`. `canSeeDisplacementPlot` is a recursive LOS test using
terrain-elevation blocking — peaks block sight, hills partially block; `DOMAIN_AIR` units see all plots
regardless of LOS.

The invisible-type dimension: if `GAMEOPTION_COMBAT_HIDE_SEEK` is **not** active, only a unit's declared
`getSeeInvisibleType(i)` types are tracked; otherwise ALL invisible types are tracked (the Hide-and-Seek
blanket mode). `NO_INVISIBLE` (standard sight) is always tracked.

### 1d. Per-plot sight-source inventory (`updateSight`, CvPlot.cpp:2853)

Called when a plot's content changes (unit enters/leaves, city founded/razed, ownership changes). Sources
added:

1. **City sight** — for each alive PC team that is a vassal of the plot's team, OR has espionage visibility
   (`pCity->getEspionageVisibility(eTeamX)` = passive EP mission active), OR the city is a capital and
   `GET_TEAM(team).isHasEmbassy(eTeamX)`: `changeAdjacentSight(eTeamX, 1, …)`.
2. **Owned plot** — if the plot has an owner: `changeAdjacentSight(eTeam, 1, …)`.
3. **Units** — each unit on the plot adds `visibilityRange()` for its team.
4. **Recon** — units in recon posture (`getReconPlot() == this`) add `RECON_VISIBILITY_RANGE` sight.

### 1e. Full scratch rebuild every turn (`doTurn.visibilityRebuild`, CvGame.cpp:6007)

Every turn, before AI processing, the game (1) calls `clearVisibilityCounts()` on every plot (zeroing all
`m_aiVisibilityCount`, destroying stolen-visibility + invisible-count arrays), then (2) calls
`GC.getMap().updateSight(true, false)` — a full replay of every sight source.

This is a known **"stickytape"** — the verbatim CvGame.cpp:6002 comment is *"a stickytape - can't find
where it's skewing visibility counts"* — that runs even with correct incremental updates. Cost is measured
by `[PERF/phase] doTurn.visibilityRebuild`. The zero-then-rebuild means the incremental counts computed
during AI processing are **discarded at turn start**; the only authoritative counts are those from the full
rebuild. (Consequence: a "sight as of turn start" vs "sight mid-AI-turn" distinction is not externally
observable — an endpoint can only ever observe the post-rebuild state.)

### 1f. Invisible-unit gating (`isInvisible`, CvUnit.cpp:12866)

A unit is invisible to `eTeam` per these checks (in order):

1. Never if `getTeam() == eTeam` (own team always sees own units).
2. Always if `alwaysInvisible()` = `m_pUnitInfo->isInvisible() || getAlwaysInvisibleCount() > 0`.
3. Always if `isCargo()` (loaded into a transport, when `bCheckCargo`).
4. Never if `isNeverInvisible()` = no invisible type at all (the fast path for standard visible units).
5. Never if `isRevealed()` (revealed by some other means).

Without `GAMEOPTION_COMBAT_HIDE_SEEK`: invisible if
`getInvisibleType() != NO_INVISIBLE && !plot()->isSpotterInSight(eTeam, eInvisible)`, where `isSpotterInSight`
= `getInvisibleVisibilityCount(eTeam, eInvisible) > 0`.

With Hide-and-Seek (intensity model): for each invisible type the unit has, invisible if no spotter at all,
OR if the best spotter's intensity (`getHighestPlotTeamVisibilityIntensity(eInvisible, eTeam)`) is less than
the unit's `invisibilityIntensityTotal(eInvisible)`. Intensity decays with distance from the spotter.

### 1g. Stolen visibility (espionage sight-sharing, `CvTeam::isStolenVisibility`, CvTeam.cpp:3449)

`isStolenVisibility(eIndex)` is true when `getStolenVisibilityTimer(eIndex) > 0`. On an on/off transition,
every plot currently visible to `eIndex` gets `changeStolenVisibilityCount(thisTeam, ±1)` — so spying team A
"borrows" the entire visible set of spied-on team B in bulk. The timer ticks down per turn (the EP mission
that sets it is in [`espionage.md`](espionage.md)).

### 1h. NPC / invisible units in the `/units` snapshot

The `/units` walk iterates `kPlayer.units()` for all alive players with **no** filter on NPC, invisibility,
or cargo status (`CvHttpServer.cpp`). NPC units (animals, barbarians) appear with `npc=true` on the
`/players` row. Invisible units appear with **no** invisible flag — the snapshot never calls `isInvisible`,
so a census-based comparison against the human's screen overcounts units the human cannot see.

### 1i. `[ENG/viscap]` — the only existing visibility log tag

`logEngine(2, "[ENG/viscap] team=%d plot=(%d,%d) count=%d change=%d …", …)` fires in `changeVisibilityCount`
(CvPlot.cpp:9177) when the count would go negative and is clamped to 0. It is an **error/anomaly** indicator,
not a normal observability hook — known to fire en masse during `recalculateModifiers` due to the
remove/re-add sight ordering; a flood *outside* recalc indicates a real accounting bug.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### What exists

| Source | Field | Granularity |
|---|---|---|
| `/units` → `x`, `y`, `owner`, `type`, `ai` | unit position, type, unitAI | per-unit, snapshotted ~every 5s |
| `/players` → `npc` | whether a player is NPC | per-player |
| `[ENG/viscap]` (level 2, `gTeamLogLevel`) | anomaly: negative count clamped to 0 | per incident, gated |
| `[PERF/phase] doTurn.visibilityRebuild` | wall-clock cost of the full sight rebuild | per turn, gated `gPerfLogLevel>=1` |
| PlotSnapshot per-turn CSV | plot terrain, ownership, numUnits — **not** per-team visibility | per-plot, per-turn |

### What is NOT exposed (the gap)

| State | Why it matters | Missing surface |
|---|---|---|
| `m_aiVisibilityCount[eTeam]` per plot | whether a team currently sees a plot — core fog-of-war | no endpoint; only derivable by running `updateSight` externally (impossible without game objects) |
| `m_abRevealed[eTeam]` per plot | shroud map: which plots a team has ever explored | no endpoint; needed to understand AI exploration frontiers |
| `m_aiLastSeenTurn[eTeam]` per plot | when a team last had active sight (staleness) | no endpoint |
| `m_apaiInvisibleVisibilityCount[eTeam][eInvisible]` per plot | the gate for `isSpotterInSight` | no endpoint |
| `isInvisible(eTeam)` per unit | whether a unit is invisible to each team | no `/units` field; all units appear visible even when invisible to enemies |
| `getExtraVisibilityRange()` per unit | total promotion+equipment sight bonus | no `/units` field |
| `visibilityRange()` per unit | actual tile radius covered | not snapshotted (needs plot terrain, on PlotSnapshot but unlinked) |
| stolen visibility (`isStolenVisibility`, `getStolenVisibilityTimer`) | which teams share sight via espionage, for how long | no endpoint |
| espionage city visibility (`getEspionageVisibility` per city) | which rivals have passive EP city-sight | no `/cities` field |
| embassy capital visibility (`isHasEmbassy` per team) | whether a capital is visible via embassy | no `/players` field |
| `isRevealedGoody` / `getRevealedOwner` / `getRevealedImprovementType` / `getRevealedRouteType` | what a team *thinks* it knows behind fog (stale data) | no endpoint |
| `GAMEOPTION_COMBAT_HIDE_SEEK` active | switches the entire invisibility model (intensity vs binary) | not surfaced anywhere |
| intensity per spotter (`m_aPlotTeamVisibilityIntensity`) | under Hide-and-Seek, the partial-detection ledger | no endpoint |
| the `doTurn.visibilityRebuild` scratch-rebuild semantics | midturn incremental counts are discarded; only post-rebuild state is meaningful | documented in CvGame.cpp:6002 comment only |

---

## 3. The gap

At Tier 1 we can answer only "where is each unit, who owns it, what type is it?" (`/units`), "is this player
NPC?" (`/players`), and "how long did the sight rebuild take?" (`[PERF/phase]`). We **cannot** answer,
without the screen: what can team X see right now; has team X ever explored tile (a,b); is unit U invisible
to team Y (the snapshot exposes ALL units with no per-viewer filter); which enemy cities are watched by
espionage sight; which teams share stolen vision and for how many turns; what sight range unit U has; or —
under Hide-and-Seek — whether a specific invisible unit is detectable by a specific spotter.

The gap is severe because vision is the **primary filter** on what each player "knows": every AI
offensive/defensive decision (`isVisibleEnemyDefender`, `isVisiblePotentialEnemyDefender`) is shaped by what
the AI can see, and none of that filtering is observable. The cascade's `requires` atoms will eventually gate
on visibility (e.g. "the target city must be revealed"), and today that condition cannot be shadowed from
outside ([DEC-map-before-delete]).

---

## 4. Proposed hooks — climbing from Tier 1 toward Tier 3/4

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. Hook A — `/units` snapshot fields (Tier 1 → 2)

Add to `UnitSnap` + the per-unit JSON (`CvHttpServer.cpp`):

| JSON key | Source call | Notes |
|---|---|---|
| `"invisible"` | `pLoopUnit->isNeverInvisible() ? 0 : 1` | coarse: 1 = unit HAS an invisible type. The full per-team query is too expensive; this flags units worth querying further |
| `"sightRange"` | `pLoopUnit->visibilityRange(pLoopUnit->plot())` | actual tile radius from current position; one cheap call |
| `"alwaysInvisible"` | `pLoopUnit->alwaysInvisible() ? 1 : 0` | distinguishes always-invisible (e.g. cargo) from conditionally invisible |

These close the "which units are potential invisibles" gap and let consumers compute approximate sight
coverage from position data.

### 4b. Hook B — `[VIS]` log domain, team-scope (`gTeamLogLevel`) (Tier 2 → 3)

New helper `logVisionAI(int level, …)` → `VisionAI.log`, tag prefix `[VIS]`, teed to `/events`. Emit at
state *transitions* — never per-plot (millions of lines/turn):

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[VIS/gain]` | 2 | `changeVisibilityCount` when `bOldVisible=false → isVisible=true` | `turn= team= plot=(x,y) source=unit\|city\|owned\|recon` |
| `[VIS/lose]` | 2 | same, `true → false` | `turn= team= plot=(x,y)` |
| `[VIS/reveal]` | 1 | `setRevealed` (CvPlot.cpp:9601) when `bNewValue=true` (first sighting) | `turn= team= plot=(x,y) fromTeam=` |
| `[VIS/stolen/gain]` | 1 | `setStolenVisibilityTimer` when `isStolenVisibility` → true | `turn= team= spiedTeam= timer=` |
| `[VIS/stolen/lose]` | 1 | same, → false | `turn= team= spiedTeam=` |
| `[VIS/espionage/city]` | 2 | `setEspionageVisibility` when value changes | `turn= city= owner= visTeam= visible=0\|1` |
| `[VIS/invisible]` | 2 | `isInvisible` (with a per-(unit,team) `m_bLastInvisibleCache`) when the result differs from last | `turn= unit= team= result=visible\|invisible reason=always\|cargo\|spotter\|intensity` |

Level-1 lines (reveal, stolen-vision transitions) are the essentials — they put first-reveal events and
espionage sight windows on `/events`. Level 2 adds individual sight-gain/lose + espionage-city lines.

**Volume warning:** `[VIS/gain]`/`[VIS/lose]` fire during the `doTurn.visibilityRebuild` scratch rebuild —
potentially millions of transitions/turn at level 2. Guard with `if (gTeamLogLevel >= 2 && !bInRebuildPass)`
to suppress during the known-noisy rebuild. `[VIS/reveal]` is naturally low-volume (first-ever revelation
only).

### 4c. Hook C — `/diagnostic/visibilityQuery?x=N&y=M&team=T` (Tier 4)

A mailbox-pattern endpoint (the `canConstruct`/`placementSweep` pattern; game-thread-serviced). The single
most powerful visibility hook — the full per-(plot, team) sight state on demand; all fields are direct array
reads on `CvPlot`:

```json
{
  "plot": {"x": N, "y": M}, "team": T,
  "visibilityCount": 3, "stolenVisibilityCount": 0,
  "isVisible": true, "isRevealed": true, "lastSeenTurn": 142,
  "invisibleCounts": {"INVISIBLE_SUBMARINE": 1, "INVISIBLE_SEA": 0},
  "revealedOwner": 2, "revealedImprovement": "IMPROVEMENT_FORT"
}
```

### 4d. Hook D — `/players` snapshot fields (Tier 2 → 3)

Add to `PlayerSnap`:

| JSON key | Source call | Notes |
|---|---|---|
| `"stolenVisionFrom"` | `GET_TEAM(eTeam).isStolenVisibility(eOtherTeam)` per other team | array of team IDs currently sharing sight via espionage |
| `"embassyTeams"` | `GET_TEAM(eTeam).isHasEmbassy(eOtherTeam)` per other team | array of team IDs this team has an embassy with (capital visibility) |

Small arrays (≤ MAX_TEAMS) exposing the otherwise-invisible "why can I see their capital" state.

### 4e. Hook E — `/units` field `visibleToTeams` (deferred; expensive)

A per-unit bitmask of which teams currently see this unit (`!isInvisible(eTeam, false)` per team) — the most
complete invisibility surface, but it iterates all teams per unit in the snapshot publish. Deferred:
implement Hook A first and gate this behind `Autolog__HttpServer && gUnitLogLevel >= 3` to keep it opt-in.

### 4f. Hook F — PlotSnapshot visibility columns (Tier 4)

Extend `PlotSnapshot` (per-turn CSV, `Utils/PlotSnapshot.cpp`) with per-team visibility columns, opt-in
behind a `?visibility=1` gate (default off — iterating all teams × all plots is expensive at map scale):

| Column | Source | Notes |
|---|---|---|
| `visibleTeams` | pipe-separated team IDs where `isVisible(eTeam, false)` | the most complete fog-of-war snapshot |
| `revealedTeams` | pipe-separated team IDs where `isRevealed(eTeam, false)` | which teams have ever seen this plot |

---

## 5. Cascade / tally implications

- **`isRevealed` as a `requires` precondition.** Some `requires.build` atoms already implicitly assume a
  plot/city is revealed (e.g. trading). The cascade will eventually need a `DOMAIN_REVEAL` tally counting
  revealed plots per team — not yet planned, but a Tier-4 prerequisite.
- **Invisible-unit gating for AI decisions.** The AI's attack/defence (`isVisibleEnemyDefender`,
  `isVisiblePotentialEnemyDefender`) filters on `isInvisible`. The cascade's `requires.operate` conditions
  will reference unit-visibility state; without Hook A or C, those conditions can't be shadowed externally.
- **The scratch rebuild.** Because `doTurn.visibilityRebuild` runs before AI turns, a current-turn sight
  snapshot is internally consistent — but "sight at turn start" vs "mid-AI-turn" is invisible; an endpoint
  only ever sees the post-rebuild state.
- **Stolen-vision coupling.** `isStolenVisibility` and per-city `getEspionageVisibility` are non-obvious
  team-sight sources with no cascade representation — the vision system is entangled with espionage
  (see [`espionage.md`](espionage.md)) and that coupling is entirely unobservable.
- **The NPC invisible-unit benchmark gate.** `/units` does not filter invisible NPC units, so a
  census-vs-screen comparison overcounts NPC units the human cannot see. Hook A (`alwaysInvisible`) is the
  minimum needed to replicate the gate in post-processing.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `Cv*AI` →
> `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `isVisible` / `isRevealed` | `Sources/Engine/CvPlot.cpp:5152 / 9588` |
| `changeAdjacentSight` (sight-radius update) | `Sources/Engine/CvPlot.cpp:2606` |
| `updateSight` (per-plot sight inventory) | `Sources/Engine/CvPlot.cpp:2853` |
| `changeVisibilityCount` / `[ENG/viscap]` | `Sources/Engine/CvPlot.cpp:9162 / 9177` |
| `setRevealed` (first-sighting hook site) | `Sources/Engine/CvPlot.cpp:9601` |
| `visibilityRange` / `isInvisible` | `Sources/Engine/CvUnit.cpp:10776 / 12866` |
| `isStolenVisibility` (espionage sight-share) | `Sources/Engine/CvTeam.cpp:3449` |
| `doTurn.visibilityRebuild` (full scratch rebuild + "stickytape" comment) | `Sources/Engine/CvGame.cpp:6002–6014` |
| `/units` snapshot walk (add Hook A/E fields) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/units`, `/players`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`promotions-xp.md`](promotions-xp.md) — the sibling per-unit map; `getExtraVisibilityRange` (a unit's
  sight bonus) is promotion-driven, so promotion state feeds the sight-range formula here.
- [`espionage.md`](espionage.md) — the EP economy; stolen visibility and per-city espionage visibility are
  espionage-driven sight sources, an §A opaque-system coupling into this map.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: a visibility-gated cascade atom cannot be shadowed/cut until the fog-of-war
  state is on the wire ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
