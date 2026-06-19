> DRAFT observability map (2026-06-18, parent agent) — claims cited from code; verify before relying.

# Observability map: Trade routes & connectivity

**System:** Trade routes (per-city commerce-yield generation) + the plot-group / bonus-connectivity
network (the cascade's BONUS-connection oracle).
**Anchor files:** `Sources/CvCity.cpp`, `Sources/CvPlot.cpp`, `Sources/CvPlotGroup.cpp`, `Sources/CvPlayer.cpp`
**Context:** #428/#430 "render from API without looking at the screen" bar
(cascade-mapping-inventory.md §A, §D).

---

## 1. How it actually works

### 1a. Per-city trade-route slot count

A city's slot budget is: `CvCity::getTradeRoutes()` (`CvCity.cpp:15336`):

```
slots = GC.getGame().getTradeRoutes()         // game-level global (votes/events)
      + GET_PLAYER(owner).getTradeRoutes()     // player-level delta (civics/techs/traits)
      + (isCoastal ? player.getCoastalTradeRoutes() : 0)   // coastal bonus
      + city.getExtraTradeRoutes()             // building-contributed local delta
capped at [0, getMaxTradeRoutes()]             // hard ceiling = GC.getMAX_TRADE_ROUTES()
                                               //   + player.getMaxTradeRoutesAdjustment()
```

The three contributor knobs are changed by:
- `CvGame::getTradeRoutes` (`CvGame.cpp:3754`) — votes, world congress events.
- `CvPlayer::getTradeRoutes` / `changeTradeRoutes` (`CvPlayer.cpp:11099`) — civics
  (`CvCivicInfo::getTradeRoutes`), tech research (`CvPlayer.cpp:30883`), traits
  (`CvPlayer.cpp:28516`).
- `CvPlayer::getCoastalTradeRoutes` / `changeCoastalTradeRoutes` (`CvPlayer.cpp:11083`) —
  building effect tag `getCoastalTradeRoutes()`.
- `CvCity::changeExtraTradeRoutes` (`CvCity.cpp:9875`) — per-building `kBuilding.getTradeRoutes()`.

### 1b. Route formation — `CvCity::updateTradeRoutes` (`CvCity.cpp:15368`)

Called eagerly on every modifier change (building added/removed, civic change, tech, disorder/plague
change, plunder state change), **not** in doTurn directly. It:

1. Clears all current routes (`clearTradeRoutes`, `CvCity.cpp:15351`) — sets `m_paTradeCities` back
   to empty `IDInfo` slots and clears the boolean `m_abTradeRoute[ePlayer]` flags on previously-routed
   cities.
2. If the city is disordered, plundered, or quarantined — route count collapses to zero
   (`CvCity.cpp:15377`).
3. Otherwise iterates **all alive players** and **all their cities** to find candidates
   (`CvCity.cpp:15383`):
   - `canHaveTradeRoutesWith(iI)` — diplomatic eligibility (see §1c).
   - `pLoopCity->plotGroup(owner) == plotGroup(owner)` — **plot-group connectivity test**
     (`CvCity.cpp:15393`); `GC.getIGNORE_PLOT_GROUP_FOR_TRADE_ROUTES()` is a global bypass
     (off by default). Two cities must be in the **same `CvPlotGroup` for the owning player** to
     form a route.
   - A city may not already have a route from this owner (one-route-per-city-per-player cap),
     unless they are on the same team.
4. Scores each candidate via `calculateTradeProfit(pLoopCity)` (`CvCity.cpp:11629`) and inserts into
   a best-N sorted list — greedy descending selection. The score formula:
   - `getBaseTradeProfit(pCity)` (`CvCity.cpp:11585`) = `min(theirPop × THEIR_POPULATION_TRADE_PERCENT,
     plotDistance × world_TradeProfitPercent) × TRADE_PROFIT_PERCENT / 100`, floor 100.
   - `totalTradeModifier(pCity)` (`CvCity.cpp:11517`) — a percentage modifier starting at 100:
     - `+getTradeRouteModifier()` (building tags)
     - `+getPopulationTradeModifier()` (own pop)
     - `+GET_TEAM(team).getTradeModifier()` (team-level)
     - `+GC.getCAPITAL_TRADE_MODIFIER()` if `isConnectedToCapital()` (`CvCity.cpp:11526`)
     - `+GC.getOVERSEAS_TRADE_MODIFIER()` if different area
     - `+getForeignTradeRouteModifier()` + player/team foreign bonuses + shared-civic bonus
       + `getPeaceTradeModifier` if foreign team
5. After slot fill, calls `setTradeYield` for each yield type from the summed profit
   (`CvCity.cpp:15447`).

The **`m_abTradeRoute[ePlayer]` boolean array** on the *destination* city is set true when it is
selected (`CvCity.cpp:15435`). This is how `updateTradeRoutes` avoids selecting the same destination
city twice for different source cities of the same non-team player.

**`CvPlayer::updateTradeRoutes` (`CvPlayer.cpp:4272`)** iterates a player's cities
highest-modifier-first and calls each city's `updateTradeRoutes()` in that order, ensuring cities
with more trade power get first pick of destinations.

### 1c. Diplomatic eligibility — `canHaveTradeRoutesWith` (`CvPlayer.cpp:24210`)

Returns true if:
- Same team (always), OR
- Free trade / limited borders OR `forceAllTradeRoutes > 0` is active **AND** at least one of:
  vassal relationship, OR both players pass `!isNoForeignTrade()`.

`forceAllTradeRoutes` is incremented by buildings with `isForceAllTradeRoutes()` tag
(`CvCity.cpp:12333`).

### 1d. Plot-group / connectivity network — `CvPlotGroup`

Each plot owned by a player belongs to a `CvPlotGroup` for that player
(`CvPlot::getPlotGroup(ePlayer)`). A group is a **connected component** of plots that are:
- On the trade network (`CvPlot::isTradeNetwork(eTeam)`, `CvPlot.cpp:5618`): not at war with the
  owning team, not blockaded, not trade-impassable, owned or revealed, and on the **bonus network**
  (`isBonusNetwork`, `CvPlot.cpp:5610`): is a route OR river network OR network terrain.
- Adjacent connectivity is computed by `isTradeNetworkConnected` (`CvPlot.cpp:5646`): route↔route,
  city↔network-terrain, network-terrain↔network-terrain, and river-network adjacency rules.

The `CvPlotGroup::m_paiNumBonuses` array tracks how many of each `BonusType` the group "sees"
(`CvPlotGroup::getNumBonuses`). A city's `hasBonus(eBonus)` (`CvCity.cpp`) is true when the city's
plot group for the owning player contains at least one unit of that bonus — this is the cascade's
**BONUS-connection oracle**.

**Plot-group update triggers** (`CvPlot::updatePlotGroup`, `CvPlot.cpp:8846`): fired whenever a
plot's trade-network membership may change — route built/destroyed, improvement placed/removed,
bonus visibility change, city founded/razed, blockade placed/lifted. The update walks the connected
component graph and rebuilds groups via `colorRegion`. Because it is O(network size) it is deferred
during bulk operations (`startBulkRecalculate`/`endBulkRecalculate`).

`updatePlotGroupBonus` (`CvPlot.cpp:1717`) is the bonus-accounting side: called when a plot joins
or leaves a group — adds/removes the plot's contributed bonuses (raw extracted bonuses + city free
bonuses, capital import/export adjustments).

**Capital connectivity** (`isConnectedToCapital`, `CvCity.cpp:6671`) = `plot()->isConnectedToCapital`
= same plot group as the capital. Used in `totalTradeModifier` for the
`+CAPITAL_TRADE_MODIFIER` bonus and in maintenance calculation
(`CvCity.cpp:7445`). A city that is not connected to the capital loses the modifier but keeps any
routes that share a plot group (plot groups can exist that don't contain the capital).

### 1e. Yield conversion — `calculateTradeYield` (`CvCity.cpp:11642`)

`tradeProfit × player.getTradeYieldModifier(eIndex) / 100` — converts the raw profit value to a
yield of type `eIndex` (typically `YIELD_COMMERCE`). If the yield modifier is 0, that yield type
gets nothing.

### 1f. Suppression triggers

`updateTradeRoutes` immediately zeros routes when any of these become true
(`CvCity.cpp:15377`): `isDisorder()`, `isPlundered()`, `isQuarantined()`. All three conditions
call `updateTradeRoutes` on change (disorder via `CvCity::setOccupationTimer` `CvCity.cpp:10229`;
plunder via `setPlundered` `CvCity.cpp:10341`; quarantine wires similarly).

---

## 2. Current observability

**Tier: 1 — Telescreen** (coarse snapshot; barely above Tier 0 for this subsystem).

### What is already exposed

| What | Where | Notes |
|---|---|---|
| City `commerce` rate | `GET /cities` → `commerce` field (`CvHttpServer.cpp:348`) | `YIELD_COMMERCE` total including trade yield. Trade yield is **folded in** — not broken out. |
| City `food` + `production` rates | `GET /cities` | Likewise, totals only. |
| City `population` | `GET /cities` | Input to `getBaseTradeProfit` / `getPopulationTradeModifier`. |
| City `capital` flag | `GET /cities` | Boolean; needed to determine `isConnectedToCapital`. |
| Player `gold` + `goldRate` + `scienceRate` | `GET /players` | These are downstream aggregates of commerce; trade yield contributes indirectly. |
| Building bonus prereqs (diagnostic only) | `GET /diagnostic/canConstruct` `unitBonusPrereqs` field (`CvHttpServer.cpp:901`) | Only in the canTrain diagnostic; whether the **city** currently has the bonus is answered as `and:yes/NO` — but this only covers the diagnostic path for one building/unit at a time. |
| Building `hasBonus` gate | `GET /diagnostic/canConstruct` → `legacyReason: "bonus"` (`CvHttpServer.cpp:559`) | Diagnostic path only; not in the `/cities` snapshot. |

### What is NOT exposed (the gap)

None of the following is in any snapshot endpoint or log stream:

1. **Which cities are connected by a route** — the `m_paTradeCities` list (destinations, by city
   owner/id) is not published anywhere. You cannot tell from the API who trades with whom.
2. **Per-city trade yield** — `CvCity::getTradeYield(YIELD_COMMERCE)` and
   `getTradeYield(YIELD_FOOD)` / `YIELD_PRODUCTION` (trade can yield food/production too if the
   modifiers are nonzero) are not in the `/cities` snapshot.
3. **Per-city route count** — `getTradeRoutes()` (number of active slots used) is not published.
4. **Per-route profit** — `calculateTradeProfit(pOtherCity)` per active route; not published.
5. **Per-city `totalTradeModifier`** — the percentage modifier that determines route priority and
   profit; not published.
6. **`isConnectedToCapital` per city** — not a field in `/cities`. Critical for the cascade
   `requires` atom (`CAPITAL_TRADE_MODIFIER` and maintenance modifier gating).
7. **Plot-group membership / bonus-connection state** — `plotGroup(owner)` per city is not
   published. Whether two cities are in the same plot group is invisible.
8. **Per-city `hasBonus` for all bonus types** — `CvCity::hasBonus(eBonus)` (= bonus in plot
   group) is answered only in the narrow diagnostic path (`canConstruct`/`canTrain`), not as a
   general per-city observable.
9. **Diplomatic eligibility** — `canHaveTradeRoutesWith(ePlayer)` per player pair is not
   published; cannot reconstruct which trade partners are possible.
10. **Suppression state** — `isDisorder()`, `isPlundered()`, `isQuarantined()` are not fields in
    `/cities`; cannot tell from outside why a city has 0 trade yield.
11. **Player-level trade knobs** — `getTradeRoutes()`, `getCoastalTradeRoutes()`,
    `getMaxTradeRoutesAdjustment()`, `isNoForeignTrade()`, `getForceAllTradeRoutes()` are absent
    from `/players`.
12. **No trade-route log tags exist** — there is no `[TRD]` or equivalent domain in the AI
    logging registry (`BetterBTSAI.cpp`, `ai-logging-reference.md §2`). Zero per-turn observability
    from the log stream.
13. **`/tally` endpoint not yet built** — the planned bonus-count tally (`/tally` per
    `http-server.md §Planned`) does not exist; no count-of-bonus-X-in-empire is queryable.

---

## 3. The gap — what cannot be reconstructed from outside today

Given only the current `/cities` + `/players` + `/events` + log stream:

- **Cannot identify active trade routes at all.** The `commerce` field in `/cities` is the sum of
  all commerce sources; there is no way to decompose how much is from trade vs base yields vs
  specialist slots vs buildings.
- **Cannot assess connectivity.** Whether a city is connected to the capital (plot-group
  membership) cannot be determined — this matters both for trade route value and for building
  maintenance modifiers, which the cascade must reproduce correctly.
- **Cannot assess bonus-connection state.** The cascade's `requires.operate` uses `hasBonus` as
  its resource-dormancy oracle. For every building that requires a bonus, the cascade needs to know
  whether each city is in a plot group containing that bonus. This is the most load-bearing gap
  relative to the #430 cascade rework — if bonus-connection state is invisible, resource-dormancy
  shadows (B-ii in cascade-mapping-inventory.md) cannot be verified from the API.
- **Cannot explain why a city's commerce changed.** No event fires when trade routes are
  recalculated; `updateTradeRoutes` is called eagerly but silently.
- **Cannot reconstruct the profit formula inputs per route.** `baseTradeProfit` is driven by
  `theirPop` and `plotDistance`; `totalTradeModifier` is driven by 8+ factors. None of these are
  individually surfaced.
- **Cannot determine which player pairs have diplomatic eligibility for trade.** Cascade needs
  this if it ever models route formation rather than just dormancy.

---

## 4. Proposed hooks — concrete additions to climb from Tier 1 to Tier 3

The additions below are cheap, gated, and match the existing patterns in `CvHttpServer.cpp` and
the `[TAG]` log system. They are ordered by cascade-criticality: the bonus-connection oracle
and capital-connectivity are the highest-value items for the #430 rework.

### Priority 1 — `/cities` snapshot fields (server-thread safe; add to `CitySnap` + publish)

These are read-only scalar/boolean fields captured from the game thread during `publishIfDue`
(`CvHttpServer.cpp:1542` loop), same pattern as `iCommerce`, `iCapital`:

| Field name | Engine call | Purpose |
|---|---|---|
| `connectedToCapital` | `pLoopCity->isConnectedToCapital()` | Connectivity oracle for cascade dormancy + maintenance modifier |
| `tradeRoutes` | `pLoopCity->getTradeRoutes()` | Slot count in use (not the max — the active count) |
| `tradeYield` | `pLoopCity->getTradeYield(YIELD_COMMERCE)` | Commerce contributed by trade (breakout from `commerce` total) |
| `disordered` | `pLoopCity->isDisorder()` | Suppression reason 1 |
| `plundered` | `pLoopCity->isPlundered()` | Suppression reason 2 |
| `quarantined` | `pLoopCity->isQuarantined()` | Suppression reason 3 |
| `tradeRouteModifier` | `pLoopCity->totalTradeModifier()` | Priority-ordering observable |

### Priority 2 — `/cities` extended: per-city bonus-connection list

The most load-bearing missing piece for the cascade. Add to `CitySnap` as an optional array
(only when `?bonuses=1` query param is set, to avoid snapshot bloat by default):

```json
"connectedBonuses": ["BONUS_IRON", "BONUS_WHEAT"]
```

Implementation: iterate `GC.getNumBonusInfos()`, emit bonus type strings where
`pLoopCity->hasBonus((BonusTypes)iI)` is true. Gated on a query param so the default
snapshot stays small. This directly enables the resource-dormancy shadow (B-ii,
cascade-mapping-inventory.md).

### Priority 3 — `/players` snapshot fields

Add to `PlayerSnap` (same publish loop, `CvHttpServer.cpp:~1520`):

| Field name | Engine call | Purpose |
|---|---|---|
| `tradeRoutes` | `kPlayer.getTradeRoutes()` | Player-level delta toward city slot budget |
| `coastalTradeRoutes` | `kPlayer.getCoastalTradeRoutes()` | Coastal bonus pool |
| `noForeignTrade` | `kPlayer.isNoForeignTrade()` | Diplomatic trade block flag |
| `forceAllTradeRoutes` | `kPlayer.getForceAllTradeRoutes()` | Building-driven foreign override |

### Priority 4 — `[TRD]` log domain (new gated log tag)

Add a `logTradeAI` helper to `BetterBTSAI.{h,cpp}` (copy pattern from any existing
`log<Domain>AI`; use `gPlayerLogLevel` scope; new file `TradeRoutes.log`):

- **`[TRD/update]`** (level 1) — emitted at the start of `CvCity::updateTradeRoutes` when the
  route set actually changes (compare old `m_paTradeCities` vs new): `city=<id> owner=<n>
  routes=<count> reason=<modifier|building|civic|plotGroup|suppressed>`. Only on change, so it
  self-throttles.
- **`[TRD/route]`** (level 2) — one line per committed route slot: `src=<cityId> dst=<cityId>
  dstOwner=<n> profit=<n> modifier=<n>`. Provides full route topology per-turn.
- **`[TRD/plotGroup]`** (level 1) — emitted in `CvPlot::updatePlotGroup(Player)` when a city's
  group membership changes: `city=<id> player=<n> newGroup=<id> oldGroup=<id>`. This makes
  connectivity changes visible in `/events`.
- **`[TRD/bonus]`** (level 2) — emitted in `CvPlot::updatePlotGroupBonus` when a city's group
  acquires or loses a bonus: `city=<id> player=<n> bonus=<NAME> change=<+1/-1>`.

### Priority 5 — `/diagnostic/tradeRoutes?player=N` endpoint

A diagnostic-family endpoint (similar to `/diagnostic/sweep`) that on-demand returns, for a given
player, the full route topology as the engine sees it:

```json
{
  "player": N, "turn": T,
  "cities": [
    {
      "id": C, "name": "...", "routes": 3,
      "connectedToCapital": true, "disordered": false,
      "tradeYield": 14,
      "slots": [
        {"dst": D, "dstOwner": P, "profit": 47, "modifier": 130, "foreign": false},
        ...
      ]
    }, ...
  ]
}
```

Evaluated on the game thread via the mailbox mechanism (same as `placementSweep`). This provides
the "full topology in one call" view the Orwell bar needs — with it you can reconstruct every
active route, its profit, and whether it is domestic or foreign, without looking at the screen.

---

## 5. Summary table

| Piece | Currently observable? | Proposed hook |
|---|---|---|
| City commerce total | Yes (`/cities commerce`) | — |
| Trade-route yield contribution | No | `/cities tradeYield` (P1) |
| Active route count | No | `/cities tradeRoutes` (P1) |
| Capital connectivity | No | `/cities connectedToCapital` (P1) |
| Suppression state (disorder/plunder/quarantine) | No | `/cities disordered/plundered/quarantined` (P1) |
| Trade modifier % | No | `/cities tradeRouteModifier` (P1) |
| Per-city connected-bonus set | No | `/cities?bonuses=1 connectedBonuses[]` (P2) |
| Player trade-route knobs | No | `/players tradeRoutes/coastalTradeRoutes/noForeignTrade/forceAllTradeRoutes` (P3) |
| Route topology (who→who, profit) | No | `[TRD/route]` log tag + `/diagnostic/tradeRoutes` (P4/P5) |
| Connectivity changes in event stream | No | `[TRD/plotGroup]` + `[TRD/bonus]` (P4) |

---

## 6. Cascade-specific notes

- **BONUS oracle gap is the most urgent** for #430. The cascade's `requires.operate` resource-dormancy
  check calls `hasBonus(eBonus)` on the city (`CvCity.cpp:~14364` via `isActiveBuilding`), which
  resolves through the plot-group's `m_paiNumBonuses`. Until this is surfaced (Priority 2 above),
  the B-ii dormancy shadow (cascade-mapping-inventory.md §B-ii) cannot be verified from the API.
- **`isConnectedToCapital`** is needed for the cascade to match `totalTradeModifier`, which affects
  route selection and yields. If the cascade is to reproduce route-formation order or yields, this
  must be observable (Priority 1).
- **No per-turn "routes changed" event** means the cascade cannot detect when a plot-group split
  silently dropped a city's routes (e.g., road destroyed by pillage). The `[TRD/update]` + `[TRD/plotGroup]`
  tags plug this hole.
- **Plot groups are per-player, not global.** Two cities connected for player A are not necessarily
  connected for player B (different road networks, visibility, war state). The cascade must query
  per owning-player, not per team or globally.
