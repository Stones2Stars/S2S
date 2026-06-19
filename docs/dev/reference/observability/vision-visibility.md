> DRAFT observability map (2026-06-18 by parent agent) — claims cited from code; verify before relying.

# Observability map: Vision & Visibility

**Scope:** per-team plot visibility (fog of war), plot reveal state, unit sight ranges,
invisible-unit gating (`isInvisible`), stolen visibility (espionage sight-sharing), city
espionage/embassy visibility, and the Hide-and-Seek intensity model
(`GAMEOPTION_COMBAT_HIDE_SEEK`).

**Related inventory entry:** `docs/dev/plans/cascade-mapping-inventory.md` §A (Vision &
Visibility: not yet listed; this map is the initial survey).

**Observability tier assigned: 1 — Telescreen.** Unit positions and owners are snapshotted
(`/units`), but every dimension of *what each team can see* is entirely absent from the
endpoint layer. The only existing visibility log tag is `[ENG/viscap]` (level 2, fires only
on erroneous negative counts). There is no sight-gain / sight-loss event stream, no
per-team reveal state in any endpoint, no invisible-unit flag in the unit snapshot, no
stolen-visibility or espionage-sight state anywhere.

---

## 1. How it actually works

### 1-A. The core data structures (CvPlot.h:1026-1029)

Each `CvPlot` carries three lazily-allocated per-team arrays:

| Array | Type | Size | Meaning |
|---|---|---|---|
| `m_aiVisibilityCount` | `short[MAX_TEAMS]` | always present | Running count of sight sources covering this plot for each team. `> 0` → currently visible. |
| `m_abRevealed` | `bool[MAX_TEAMS]` | lazy (first reveal) | Whether this team has ever seen this plot (permanent; shrouded when `0`). |
| `m_aiLastSeenTurn` | `short[MAX_TEAMS]` | lazy | The game turn on which the team last had visibility; used for "last-seen" stale display. |
| `m_aiStolenVisibilityCount` | `int16_t[MAX_TEAMS]` | lazy | Vision piggybacked from a spied-on team's visible plots (espionage stolen-vision). |
| `m_apaiInvisibleVisibilityCount` | `short[MAX_TEAMS][NUM_INVISIBLE]` | lazy | Per-(team, invisibleType) spotter count: >0 means a spotter for that invisible type is in range. |
| `m_aPlotTeamVisibilityIntensity` | `std::vector` | lazy, Hide-and-Seek only | Per-spotter intensity entries for `GAMEOPTION_COMBAT_HIDE_SEEK`; used by `getHighestPlotTeamVisibilityIntensity`. |

`isVisible(eTeam, bDebug)` (CvPlot.cpp:5117-5130):
```
return getVisibilityCount(eTeam) > 0 || getStolenVisibilityCount(eTeam) > 0;
```

`isRevealed(eTeam, bDebug)` (CvPlot.cpp:9547-9556): returns `m_abRevealed[eTeam]`.

### 1-B. Sight range formula (CvUnit.cpp:10775-10788)

```cpp
int CvUnit::visibilityRange(const CvPlot* pPlot) const
{
    int iRange = 1 + pPlot->getTerrainElevation() + getExtraVisibilityRange();
    if (pPlot->getImprovementType() != NO_IMPROVEMENT)
        iRange += GC.getImprovementInfo(pPlot->getImprovementType()).getVisibilityChange();
    return std::min(GC.getMAX_UNIT_VISIBILITY_RANGE(), iRange);
}
```

Where:
- `getTerrainElevation()` (CvPlot.cpp:2557-2568): 0 = flatland, 1 = hills, 2 = peak.
- `getExtraVisibilityRange()` (CvUnit.cpp:15348-15366): unit's base extra sight + promotion bonuses + commander/commodore bonus (the command-point chain).
- Improvement `getVisibilityChange()`: XML-defined; fortresses/watchtowers grant +1 or +2.
- `MAX_UNIT_VISIBILITY_RANGE`: an XML define cap (~6).

### 1-C. Sight radius update (`changeAdjacentSight`, CvPlot.cpp:2571-2631)

Called when a unit moves or is created/destroyed. For each plot within `[−iRange, iRange]` in a square that passes the line-of-sight check (`canSeePlot`/`canSeeDisplacementPlot`), calls `changeVisibilityCount(eTeam, ±1, eInvisible, ...)`.

`canSeeDisplacementPlot` (CvPlot.cpp:2647-2815): recursive LOS test using terrain elevation blocking — peaks block sight, hills partially block. Aerial (`DOMAIN_AIR`) units see all plots regardless of LOS.

The invisible-type dimension: if `GAMEOPTION_COMBAT_HIDE_SEEK` is **not** active, only a unit's declared `getSeeInvisibleType(i)` types are tracked; otherwise ALL invisible types are tracked (the Hide-and-Seek blanket mode). `NO_INVISIBLE` is always tracked (standard sight). (CvPlot.cpp:2578-2598.)

### 1-D. `updateSight` — the per-plot sight source inventory (CvPlot.cpp:2818-2895)

Called when a plot's content changes (unit enters/leaves, city founded/razed, ownership changes). Sources added:

1. **City sight** — for each alive PC team that is a vassal of the plot's team, OR has espionage visibility (`pCity->getEspionageVisibility(eTeamX)` = passive EP mission active, CvCity.cpp:13128), OR the city is a capital and `GET_TEAM(team).isHasEmbassy(eTeamX)`: `changeAdjacentSight(eTeamX, 1, ...)`. (CvPlot.cpp:2830-2842.)
2. **Owned plot** — if the plot has an owner: `changeAdjacentSight(eTeam, 1, ...)`. (CvPlot.cpp:2846-2848.)
3. **Units** — each unit on the plot adds `visibilityRange()` for its team. (CvPlot.cpp:2851-2874.)
4. **Recon** — units in recon posture (`getReconPlot() == this`) add `RECON_VISIBILITY_RANGE` sight. (CvPlot.cpp:2876-2894.)

### 1-E. Full scratch rebuild every turn (`doTurn.visibilityRebuild`, CvGame.cpp:5992-6004)

Every turn, before AI processing, the game:
1. Calls `clearVisibilityCounts()` on every plot in the map (zeroes all `m_aiVisibilityCount`, destroys stolen-visibility and invisible-count arrays).
2. Calls `GC.getMap().updateSight(true, false)` — a full replay of every sight source.

This is a known "stickytape" (CvGame.cpp:5992 comment: *"can't find where it's skewing visibility counts"*) that runs even with correct incremental updates. The cost is measured by `[PERF/phase]` `doTurn.visibilityRebuild`. The zeroing-then-rebuild pattern means the incremental counts computed during AI processing are *discarded at turn start* — the only authoritative counts are those computed from the full rebuild.

### 1-F. Invisible-unit gating (`isInvisible`, CvUnit.cpp:12865-12924)

A unit is invisible to `eTeam` if:

1. Never if `getTeam() == eTeam` (own team always sees own units).
2. Always if `alwaysInvisible()` = `m_pUnitInfo->isInvisible() || getAlwaysInvisibleCount() > 0`.
3. Always if `isCargo()` (loaded into a transport and `bCheckCargo=true`).
4. Never if `isNeverInvisible()` = no invisible type at all (handles standard visible units fast).
5. Never if `isRevealed()` (unit was revealed by some other means).

Without `GAMEOPTION_COMBAT_HIDE_SEEK`:
- Invisible if `getInvisibleType() != NO_INVISIBLE && !plot()->isSpotterInSight(eTeam, eInvisible)` (CvUnit.cpp:12900). `isSpotterInSight` = `getInvisibleVisibilityCount(eTeam, eInvisible) > 0` (CvPlot.cpp:10461-10463).

With `GAMEOPTION_COMBAT_HIDE_SEEK` (intensity model):
- For each invisible type the unit has, invisible if no spotter at all, OR if intensity of best spotter (`getHighestPlotTeamVisibilityIntensity(eInvisible, eTeam)`) is less than the unit's `invisibilityIntensityTotal(eInvisible)`. (CvUnit.cpp:12903-12923.) Intensity decays with distance from the spotter unit (CvPlot.cpp:2614-2625).

### 1-G. Stolen visibility (espionage sight-sharing, CvTeam.cpp:3476-3507)

`CvTeam::isStolenVisibility(eIndex)` is true when `getStolenVisibilityTimer(eIndex) > 0`. When it transitions on/off, every plot currently visible to `eIndex` gets `changeStolenVisibilityCount(thisTeam, ±1)` (CvTeam.cpp:3495-3505). So spying team A "borrows" all plots that spied-on team B currently sees — a bulk copy of their entire visible set. Timer ticks down per turn (see espionage.md for the EP mission that sets it).

### 1-H. NPC units in the `/units` snapshot

The `/units` endpoint iterates `kPlayer.units()` for all alive players with no filter on NPC, invisibility, or cargo status (CvHttpServer.cpp:1467-1496). NPC units (animals, barbarians, etc.) appear in the snapshot with `npc=true` on the `/players` row. Invisible units appear in the snapshot with no invisible flag — the snapshot does not call `isInvisible`.

### 1-I. `[ENG/viscap]` — the only existing visibility log tag

`logEngine(2, "[ENG/viscap] team=%d plot=(%d,%d) count=%d change=%d", ...)` fires in `changeVisibilityCount` (CvPlot.cpp:9142-9144) when the count would go negative and is clamped to 0. It is an error/anomaly indicator, not a normal observability hook. As the logging reference notes (`ai-logging-reference.md` §3): known to fire en masse during `recalculateModifiers` due to the remove/re-add sight ordering; floods outside recalc indicate a real accounting bug.

---

## 2. Current observability

### 2-A. What is exposed today

| Source | Field | Granularity |
|---|---|---|
| `GET /units` → `x`, `y`, `owner`, `type`, `ai` | Unit position, type, unitAI | Per-unit, snapshotted every 5s |
| `GET /units` → `iNPC` on `/players` | Whether a player is NPC | Per-player |
| `[ENG/viscap]` (level 2, `gTeamLogLevel`) | Anomaly: negative count clamped | Per incident, gated |
| `[PERF/phase]` `doTurn.visibilityRebuild` | Wall-clock cost of the full sight rebuild | Per turn, gated `gPerfLogLevel>=1` |
| PlotSnapshot per-turn CSV | Plot terrain, ownership, numUnits — **not** per-team visibility | Per-plot, per-turn |

### 2-B. What is NOT exposed (the gap)

The following are entirely invisible from outside:

| State | Why it matters | Missing surface |
|---|---|---|
| **`m_aiVisibilityCount[eTeam]`** per plot | Whether a team currently sees a plot — the core fog-of-war state | No endpoint. Only derivable by running `updateSight` externally (impossible without game objects). |
| **`m_abRevealed[eTeam]`** per plot | Shroud map: which plots a team has ever explored | No endpoint. Needed to understand AI exploration frontiers. |
| **`m_aiLastSeenTurn[eTeam]`** per plot | When a team last had active sight on a plot (staleness) | No endpoint. |
| **`m_apaiInvisibleVisibilityCount[eTeam][eInvisible]`** per plot | Whether a spotter for each invisible type is in range — the gate for `isSpotterInSight` | No endpoint. |
| **`isInvisible(eTeam)` per unit** | Whether a given unit is invisible to each team | No field on the `/units` snapshot. All units appear as visible in the data even if invisible to their enemies. |
| **`getExtraVisibilityRange()`** per unit | A unit's total promotion+equipment sight bonus | No field on `/units`. |
| **`visibilityRange()`** per unit | The actual tile radius this unit covers | Not snapshotted. Requires knowing the unit's plot terrain, which IS on PlotSnapshot but not linked. |
| **Stolen visibility** (`isStolenVisibility(eA, eB)`, `getStolenVisibilityTimer`) | Which teams are sharing sight via espionage and for how long | No endpoint. The espionage.md map lists the EP state as opaque; this is an additional invisible dimension. |
| **Espionage city visibility** (`getEspionageVisibility(eTeam)` per city) | Which rival teams have passive EP missions granting city-area sight | No field on `/cities`. |
| **Embassy capital visibility** (`isHasEmbassy` per team) | Whether a team's capital is visible to another due to embassy | No field on `/players`. |
| **`isRevealedGoody`**, **`getRevealedOwner`**, **`getRevealedImprovementType`**, **`getRevealedRouteType`** | What a team *thinks* they know about unrevealed plots (stale data behind fog) | No endpoint. |
| **`GAMEOPTION_COMBAT_HIDE_SEEK`** active | Changes the entire invisibility model (intensity-based vs binary) | Not surfaced in any endpoint. |
| **Intensity per spotter** (`m_aPlotTeamVisibilityIntensity`) | Under Hide-and-Seek: the spotter-intensity ledger determining partial detection | No endpoint. |
| **`doTurn.visibilityRebuild` known bug** | Count is rebuilt from scratch every turn; midturn incremental counts from AI processing are discarded — any "sight at turn midpoint" query is not meaningful | Documented in CvGame.cpp:5992 comment only; not surfaced to outside observers. |

---

## 3. The gap

At **Tier 1 (Telescreen)**, we can answer only:
- "Where is each unit, who owns it, what type is it?" (from `/units`).
- "Does this player own NPC?" (from `npc` on `/players`).
- Coarse timing: how long did the sight rebuild take? (from `[PERF/phase] doTurn.visibilityRebuild`).

We **cannot** answer without looking at the screen:
- What can team X see right now? (The fog-of-war state is entirely opaque.)
- Has team X ever explored tile (a, b)? (No reveal-state endpoint.)
- Is unit U invisible to team Y? (The `/units` snapshot exposes ALL units unconditionally; there is no per-viewer filter.)
- Which enemy cities are currently watched by espionage sight?
- Which teams share stolen vision via espionage, and for how many turns?
- What sight range does unit U have from its current plot?
- Under Hide-and-Seek: is a specific invisible unit detectable by a specific spotter?

For AI players especially: we cannot verify that any AI unit's position is "known" to another AI player, which is the root condition for AI attack/defence decisions. The cascade's `requires` atoms will eventually gate on visibility (e.g., "the target city must be revealed"), but today we cannot shadow that condition from outside.

The gap is severe. The vision system is the **primary filter** on what each player "knows" — every AI offensive/defensive decision is shaped by what the AI can see, but none of that filtering is observable externally.

---

## 4. Proposed hooks (concrete additions to climb toward Tier 3/4)

All additions are gated (zero cost when off) and read-only (no mutation). Patterns follow the
existing `[ENG]`/`[UNT]`/`[WAI]` conventions.

### Hook A — `/units` snapshot fields: `invisible`, `sightRange`

Add to `UnitSnap` and the per-unit JSON in `CvHttpServer.cpp`:

| JSON key | Source call | Notes |
|---|---|---|
| `"invisible"` | `pLoopUnit->isNeverInvisible() ? 0 : 1` | Coarse: 1 = unit HAS an invisible type. Full per-team query is too expensive; this flags units that CAN be invisible so consumers know to query further. |
| `"sightRange"` | `pLoopUnit->visibilityRange(pLoopUnit->plot())` | The actual tile radius from current position. Cheap (one call). |
| `"alwaysInvisible"` | `pLoopUnit->alwaysInvisible() ? 1 : 0` | Distinguishes always-invisible (e.g. cargo) from conditionally invisible. |

These three fields close the "which units are potential invisibles" gap and let consumers
compute approximate sight coverage from position data.

### Hook B — `[VIS]` log domain, `VisionAI.log`, team-scope (`gTeamLogLevel`)

New log helper `logVisionAI(int level, const char* fmt, ...)` → `VisionAI.log`, gated by
`gTeamLogLevel`. Tag prefix `[VIS]`.

Emit at key state transitions — NOT per-plot (that would be millions of lines per turn):

| Tag | Level | Where | Payload |
|---|---|---|---|
| `[VIS/gain]` | 2 | `CvPlot::changeVisibilityCount` when `bOldVisible=false → isVisible=true` | `turn= team= plot=(x,y) source=unit|city|owned|recon` |
| `[VIS/lose]` | 2 | Same, when `bOldVisible=true → isVisible=false` | `turn= team= plot=(x,y)` |
| `[VIS/reveal]` | 1 | `CvPlot::setRevealed` when `bNewValue=true` (first time a team sees a plot) | `turn= team= plot=(x,y) fromTeam=` |
| `[VIS/stolen/gain]` | 1 | `CvTeam::setStolenVisibilityTimer` when `isStolenVisibility` transitions true | `turn= team= spiedTeam= timer=` |
| `[VIS/stolen/lose]` | 1 | Same, transitions false | `turn= team= spiedTeam=` |
| `[VIS/espionage/city]` | 2 | `CvCity::setEspionageVisibility` when value changes | `turn= city= owner= visTeam= visible=0|1` |
| `[VIS/invisible]` | 2 | `CvUnit::isInvisible` when called with `bCheckCargo=false` AND result differs from last cached result (add a per-unit `m_bLastInvisibleCache` per team) | `turn= unit= team= result=visible|invisible reason=always|cargo|spotter|intensity` |

Level-1 lines (reveal, stolen-vision transitions) are the essentials — they make first-reveal events and espionage sight windows visible in the `/events` stream. Level 2 adds individual sight-gain/lose events and espionage city lines.

**Volume warning**: `[VIS/gain]` and `[VIS/lose]` fire during the `doTurn.visibilityRebuild` scratch rebuild — potentially millions of transitions per turn if logged at level 2. Guard with `if (gTeamLogLevel >= 2 && !bInRebuildPass)` to suppress during the known-noisy rebuild. The `[VIS/reveal]` tag fires only on first-ever revelation, so it is naturally low-volume.

### Hook C — `/diagnostic/visibilityQuery` endpoint

Add a mailbox-pattern diagnostic endpoint:

```
GET /diagnostic/visibilityQuery?x=N&y=M&team=T
```

Returns:
```json
{
  "plot": {"x": N, "y": M},
  "team": T,
  "visibilityCount": 3,
  "stolenVisibilityCount": 0,
  "isVisible": true,
  "isRevealed": true,
  "lastSeenTurn": 142,
  "invisibleCounts": {"INVISIBLE_SUBMARINE": 1, "INVISIBLE_SEA": 0},
  "revealedOwner": 2,
  "revealedImprovement": "IMPROVEMENT_FORT"
}
```

This is the single most powerful visibility hook — it exposes the full per-(plot, team) sight state on demand. Evaluated on the game thread (same mailbox pattern as `canConstruct`, CvHttpServer.cpp:151-164). Cheap: all fields are direct array reads on `CvPlot`.

### Hook D — `/players` snapshot fields: `stolenVision`, `embassies`, `hasEmbassyWith`

Add to `PlayerSnap` (rendered through the `/players` endpoint):

| JSON key | Source call | Notes |
|---|---|---|
| `"stolenVisionFrom"` | `GET_TEAM(eTeam).isStolenVisibility(eOtherTeam)` for each other team | Array of team IDs currently sharing sight with this team via espionage. |
| `"embassyTeams"` | `GET_TEAM(eTeam).isHasEmbassy(eOtherTeam)` | Array of team IDs with which this team has an embassy (capital visibility). |

These are small arrays (at most MAX_TEAMS entries) and expose the "why can I see their capital" state that is otherwise invisible.

### Hook E — `/units` snapshot field: `visibleToTeams` (deferred; expensive)

A per-unit bitmask of which teams currently see this unit (`!isInvisible(eTeam, false)` for each team). This is the most complete invisibility surface but requires iterating all teams per unit in the snapshot publish. Deferred — implement Hook A first and Gate E behind `Autolog__HttpServer && gUnitLogLevel >= 3` to keep it opt-in.

### Hook F — PlotSnapshot visibility columns

Extend `PlotSnapshot` (per-turn CSV, `Utils/PlotSnapshot.cpp`) with per-team visibility columns:

| Column | Source | Notes |
|---|---|---|
| `visibleTeams` | Pipe-separated team IDs where `isVisible(eTeam, false)` | "Which teams see this plot?" — the most complete fog-of-war snapshot. |
| `revealedTeams` | Pipe-separated team IDs where `isRevealed(eTeam, false)` | "Which teams have ever seen this plot?" |

These columns are opt-in (add a `?visibility=1` gate flag, defaulting off, since iterating all teams × all plots is expensive at map scale).

---

## 5. Cascade/tally implications

The vision & visibility system matters to the cascade in these ways:

- **`isRevealed` as a `requires` precondition.** Some `requires.build` atoms already implicitly assume a plot or city is revealed (e.g., trading). The cascade will need a `DOMAIN_REVEAL` tally to count revealed plots for a team — currently not planned but necessary for Tier 4.
- **Invisible-unit gating for AI decisions.** The AI's attack/defence decisions (`isVisibleEnemyDefender`, `isVisiblePotentialEnemyDefender`) filter on `isInvisible`. The cascade's `requires.operate` conditions will eventually reference unit-visibility state; without Hook A or C, we cannot verify those conditions from outside.
- **The `doTurn.visibilityRebuild` full-scratch rebuild** means any "current-turn sight" snapshot taken mid-AI-processing is consistent (the rebuild ran before AI turns). But a "sight as of turn start" vs "sight mid-AI-turn" distinction is invisible: the endpoint can only observe the post-rebuild state.
- **Stolen-vision cascades.** `isStolenVisibility` (espionage) and `getEspionageVisibility` (per-city) add non-obvious sources of team sight that have no cascade representation yet. They are §A opaque-system entanglements — the vision system is coupled to the espionage system and that coupling is entirely unobservable from outside.
- **The NPC invisible-unit benchmark gate** (referenced in `memory/ai-vs-human-benchmarking.md`): the `/units` endpoint does not filter invisible NPC units. This means a census-based comparison against the screen will overcount NPC units that the human player cannot see. Hook A (`alwaysInvisible` field) is the minimum needed to replicate the gate in post-processing.

---

## 6. Summary

| Dimension | Current state |
|---|---|
| **Tier** | 1 — Telescreen |
| **Exposed** | Unit positions/types/owners (all players, no visibility filter); `[ENG/viscap]` error indicator; `[PERF/phase] doTurn.visibilityRebuild` timing; PlotSnapshot terrain/owner/numUnits (no per-team visibility columns) |
| **Opaque** | Per-team plot visibility count, reveal state, last-seen turn, invisible-spotter counts; per-unit `isInvisible(eTeam)` flag; sight range; stolen visibility timers; espionage city visibility; embassy capital sight; Hide-and-Seek intensity ledger; game option `GAMEOPTION_COMBAT_HIDE_SEEK` |
| **Minimum hooks for Tier 2** | Hook A (3 `/units` fields: `invisible`, `sightRange`, `alwaysInvisible`) + Hook C (`/diagnostic/visibilityQuery`) |
| **Hooks for Tier 3** | Hook B level 1 (`[VIS/reveal]`, `[VIS/stolen/*]`) + Hook D (`stolenVisionFrom`, `embassyTeams` on `/players`) |
| **Hooks for full Tier 4** | All of A+B+C+D above + Hook F (PlotSnapshot visibility columns, gated) |
| **Cascade-blocking gap** | `isInvisible(eTeam)` per unit is not observable; stolen-vision and espionage-city visibility are not observable; the NPC invisible-unit benchmark gate cannot be replicated in post-processing without Hook A |
