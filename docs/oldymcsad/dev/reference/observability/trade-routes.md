# Observability — Trade routes & connectivity — what's on the wire for routes and the bonus oracle

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites re-confirmed in `Sources/Engine/CvCity.cpp`/`CvPlot.cpp`/`CvPlayer.cpp`; `/diagnostic/cityInput` trade block confirmed in `Sources/Tools/CvHttpServer.cpp:1374`)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (route formation / profit / yield), `Sources/Engine/CvPlot.cpp` + `Sources/Engine/CvPlotGroup.cpp` (trade/bonus network), `Sources/Engine/CvPlayer.cpp` (eligibility / player knobs), `Sources/Tools/CvHttpServer.cpp` (`/cities`, `/diagnostic/cityInput`). Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree, and the "trade yield not exposed" claim **corrected** (it now is, via `cityInput` — see §2.1).
> Trade routes are **Tier 1–2** today. The `/cities` snapshot folds trade yield into the `commerce` total, but the newer `/diagnostic/cityInput` dump now exposes per-city `tradeRoutes`/`maxTradeRoutes`/per-yield `tradeYield` on demand. What remains dark is the **route topology** (who trades with whom), per-route profit, and — most load-bearing for the cascade — the **per-city bonus-connection state** (`hasBonus`, the plot-group bonus oracle) and capital connectivity. This map walks the mechanics, names what is and isn't observable, and proposes the snapshot fields / `[TRD]` log domain / topology endpoint that climb it.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> **Cascade relevance:** the plot-group bonus-connectivity network is the cascade's **resource-dormancy
> oracle** — `requires.operate` calls `hasBonus(eBonus)`, which resolves through the plot group's bonus
> count. Until that is surfaced, the resource-dormancy shadow cannot be verified from the API. See
> [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md).

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works — route formation & connectivity

### 1.1 Per-city slot budget (`CvCity::getTradeRoutes`, CvCity.cpp:15347)

```
slots = GC.getGame().getTradeRoutes()                    // game-level (votes/events)
      + GET_PLAYER(owner).getTradeRoutes()               // player delta (civics/techs/traits)
      + (isCoastal ? player.getCoastalTradeRoutes() : 0) // coastal bonus
      + city.getExtraTradeRoutes()                        // per-building local delta
capped at [0, getMaxTradeRoutes()]                        // MAX_TRADE_ROUTES + player adjustment
```
Contributor knobs: `CvGame::getTradeRoutes` (votes/congress); `CvPlayer::getTradeRoutes`/`changeTradeRoutes`
(civics/tech/traits); `CvPlayer::getCoastalTradeRoutes` (CvPlayer.cpp:11083, building tag);
`CvCity::changeExtraTradeRoutes` (CvCity.cpp:9885, per-building `getTradeRoutes()`).

### 1.2 Route formation (`CvCity::updateTradeRoutes`, CvCity.cpp:15379)

Called **eagerly on every modifier change** (building add/remove, civic, tech, disorder/plague/plunder),
**not** in `doTurn`. It:
1. Clears current routes (`clearTradeRoutes`, CvCity.cpp:15362) — resets `m_paTradeCities` and the
   destination `m_abTradeRoute[ePlayer]` flags.
2. If disordered/plundered/quarantined → route count collapses to zero (§1.6).
3. Otherwise iterates all alive players and their cities for candidates, gating on:
   - `canHaveTradeRoutesWith(iI)` — diplomatic eligibility (§1.3).
   - `pLoopCity->plotGroup(owner) == plotGroup(owner)` — **plot-group connectivity test** (the two
     cities must be in the same `CvPlotGroup` for the owning player); `IGNORE_PLOT_GROUP_FOR_TRADE_ROUTES`
     is a global bypass (off by default).
   - one-route-per-destination-per-player cap (unless same team).
4. Scores each via `calculateTradeProfit(pLoopCity)` (CvCity.cpp:11630) into a best-N descending list:
   - `getBaseTradeProfit(pCity)` (CvCity.cpp:11595) = `min(theirPop × POPULATION_TRADE_PERCENT,
     plotDistance × world_TradeProfitPercent) × TRADE_PROFIT_PERCENT / 100`, floor 100.
   - `totalTradeModifier(pCity)` (CvCity.cpp:11527) — a percentage starting at 100, plus
     `getTradeRouteModifier()` + `getPopulationTradeModifier()` + team `getTradeModifier()` +
     `CAPITAL_TRADE_MODIFIER` (if `isConnectedToCapital`) + `OVERSEAS_TRADE_MODIFIER` (different area) +
     foreign/peace/shared-civic bonuses.
5. After fill, `setTradeYield` (CvCity.cpp:11507) per yield type from the summed profit; the destination
   city's `m_abTradeRoute[ePlayer]` is set so it is not double-selected.

`CvPlayer::updateTradeRoutes` (CvPlayer.cpp:4274) iterates a player's cities highest-modifier-first so
stronger trade cities get first pick of destinations.

### 1.3 Diplomatic eligibility (`CvPlayer::canHaveTradeRoutesWith`, CvPlayer.cpp:24210)

True if: same team; OR free-trade/limited-borders/`forceAllTradeRoutes > 0` active **and** (vassal
relationship OR both players `!isNoForeignTrade()`). `forceAllTradeRoutes` is incremented by buildings
with `isForceAllTradeRoutes()`.

### 1.4 The plot-group / connectivity network (`CvPlotGroup`)

Each owned plot belongs to a `CvPlotGroup` for that player — a **connected component** of plots that are
on the trade network (`CvPlot::isTradeNetwork`, CvPlot.cpp:5653: not at war, not blockaded, not
trade-impassable, owned/revealed, and on the bonus network `isBonusNetwork` CvPlot.cpp:5645: a route OR
river OR network terrain). Adjacency is `isTradeNetworkConnected` (CvPlot.cpp:5681).

`CvPlotGroup::m_paiNumBonuses` tracks how many of each `BonusType` the group "sees". A city's
`hasBonus(eBonus)` is true when its plot group for the owning player holds ≥1 of that bonus — the
cascade's **bonus-connection oracle**. Updated by `CvPlot::updatePlotGroup(PlayerTypes)` (CvPlot.cpp:8925,
fired on route/improvement/bonus/city/blockade changes; deferred during bulk ops) and the bonus-accounting
side `updatePlotGroupBonus` (CvPlot.cpp:1752).

`isConnectedToCapital` (CvCity.cpp:6681) = same plot group as the capital; feeds the
`CAPITAL_TRADE_MODIFIER` and maintenance. A city not connected to the capital loses the modifier but keeps
routes sharing a plot group (groups can exist without the capital).

### 1.5 Yield conversion (`setTradeYield` / profit → yield)

`tradeProfit × player.getTradeYieldModifier(eIndex) / 100` → the per-yield trade contribution stored as
`m_aiTradeYield[eIndex]` and read by `getTradeYield(eIndex)` (CvCity.cpp:11520). Typically `YIELD_COMMERCE`,
but food/production if those modifiers are non-zero.

### 1.6 Suppression triggers

`updateTradeRoutes` zeros routes when `isDisorder()`, `isPlundered()`, or `isQuarantined()` — each of
which calls `updateTradeRoutes` on change.

---

## 2. What's on the wire today — **Tier 1–2**

### 2.1 What IS observable

| What | Where | Notes |
|---|---|---|
| City `commerce`/`food`/`production` rates | `GET /cities` | Trade yield is **folded into** these totals — not broken out here |
| City `population`, `capital` flag | `GET /cities` | Inputs to `getBaseTradeProfit` / `isConnectedToCapital` |
| Player `gold`/`goldRate`/`scienceRate` | `GET /players` | Downstream commerce aggregates |
| **Per-city `tradeRoutes` (active count)** | `GET /diagnostic/cityInput?player=N&city=M` → `tradeRoutes.tradeRoutes` (CvHttpServer.cpp:1377) | **Now exposed** — corrects the old "not published" claim |
| **Per-city `maxTradeRoutes`** | `/diagnostic/cityInput` → `tradeRoutes.maxTradeRoutes` (CvHttpServer.cpp:1378) | **Now exposed** |
| **Per-city trade yield (food/prod/commerce)** | `/diagnostic/cityInput` → `tradeRoutes.tradeYield*` (CvHttpServer.cpp:1379-1381) | **Now exposed** — the realized `getTradeYield(YIELD_*)` breakout. Per-partner profit is **not** reproduced offline (noted in the dump) |
| Per-city `hasBonus` (narrow) | `/diagnostic/canConstruct` → `legacyReason:"bonus"` / `cityInput` resources list (CvHttpServer.cpp:563, 1062) | Only the diagnostic path for one building/loadout at a time — not a general per-city bonus list |

### 2.2 What is NOT observable

| State | Function | Significance |
|---|---|---|
| Route topology (who → who) | `m_paTradeCities` | Which cities are connected by a route is published nowhere |
| Per-route profit | `calculateTradeProfit(pOther)` | Per active route; not published |
| Per-city `totalTradeModifier` | `totalTradeModifier()` (CvCity.cpp:11527) | The priority/profit modifier; not published |
| `isConnectedToCapital` per city | `isConnectedToCapital()` (CvCity.cpp:6681) | Cascade `requires` atom (`CAPITAL_TRADE_MODIFIER` + maintenance); not a snapshot field |
| Plot-group membership | `plotGroup(owner)` | Whether two cities share a group is invisible |
| **Per-city connected-bonus set** | `hasBonus(eBonus)` over all bonuses | The cascade resource-dormancy oracle; only the narrow diagnostic path answers it today |
| Diplomatic eligibility | `canHaveTradeRoutesWith(P)` per pair | Cannot reconstruct possible trade partners |
| Suppression state | `isDisorder()`/`isPlundered()`/`isQuarantined()` | Cannot tell from outside why a city has 0 trade yield |
| Player trade knobs | `getTradeRoutes`/`getCoastalTradeRoutes`/`isNoForeignTrade`/`getForceAllTradeRoutes` | Absent from `/players` |
| `[TRD]` log domain | — | No trade-route log tag exists; zero per-turn observability from the log stream |

---

## 3. The gap

Given the current surface, an observer **cannot**:

1. **Identify active routes at all.** `commerce` is the sum of all sources; `/diagnostic/cityInput`
   gives the per-city trade *yield total* and route *count*, but not the topology — no way to see who
   trades with whom or the per-route profit.
2. **Assess connectivity.** `isConnectedToCapital` (plot-group membership) is not surfaced — it matters
   for both route value and maintenance modifiers the cascade must reproduce.
3. **Assess bonus-connection state.** The most load-bearing gap for the cascade: `requires.operate` uses
   `hasBonus` as its resource-dormancy oracle, but the per-city connected-bonus set is only answerable
   through the narrow `canConstruct`/`cityInput`-loadout diagnostic, not as a general per-city observable.
   Until it is, the resource-dormancy shadow cannot be verified from the API.
4. **Explain why commerce changed.** `updateTradeRoutes` is called eagerly but **silently** — no event
   fires when routes recalculate (e.g. a road pillaged splitting a plot group and dropping routes).
5. **Reconstruct the profit inputs.** `baseTradeProfit` (theirPop, plotDistance) and the 8+-factor
   `totalTradeModifier` are surfaced nowhere individually.
6. **Determine diplomatic eligibility** for any player pair.

---

## 4. Proposed hooks (concrete additions)

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes]. Ordered by
cascade-criticality: the bonus-connection oracle and capital connectivity are highest-value for the rework.

### 4.1 `/cities` snapshot fields (HIGH) — snapshot-field shape

Read-only scalars captured during `publishIfDue`, same pattern as `commerce`/`capital`:

| Field | Engine call | Purpose |
|---|---|---|
| `connectedToCapital` | `isConnectedToCapital()` | Connectivity oracle for cascade dormancy + maintenance modifier |
| `tradeRoutes` | `getTradeRoutes()` | Active slot count (promote from `cityInput` to the polling snapshot) |
| `tradeYield` | `getTradeYield(YIELD_COMMERCE)` | Commerce-from-trade breakout from the `commerce` total |
| `disordered` / `plundered` / `quarantined` | `isDisorder()` / `isPlundered()` / `isQuarantined()` | The three suppression reasons |
| `tradeRouteModifier` | `totalTradeModifier()` | Priority-ordering observable |

### 4.2 Per-city connected-bonus list (HIGH) — snapshot-field shape

The most load-bearing piece for the cascade. Add to the `/cities` snapshot as an optional array, gated on
`?bonuses=1` to avoid default bloat:
```json
"connectedBonuses": ["BONUS_IRON", "BONUS_WHEAT"]
```
Iterate `GC.getNumBonusInfos()`, emit type strings where `hasBonus((BonusTypes)i)` is true. This directly
enables the resource-dormancy shadow — promoting `hasBonus` from the narrow diagnostic path to a general
per-city observable.

### 4.3 `/players` snapshot fields (MEDIUM) — snapshot-field shape

`tradeRoutes` (`getTradeRoutes()`), `coastalTradeRoutes` (`getCoastalTradeRoutes()`), `noForeignTrade`
(`isNoForeignTrade()`), `forceAllTradeRoutes` (`getForceAllTradeRoutes()`) — the player-level slot/diplomacy
knobs.

### 4.4 `[TRD]` log domain (MEDIUM) — gated-log shape

Add a `logTradeAI` helper (copy any existing `log<Domain>AI`, scope `gPlayerLogLevel`, new file
`TradeRoutes.log`), teed to `/events`:
- **`[TRD/update]`** (level 1) — at the start of `updateTradeRoutes` when the route set actually changes:
  `city=ID owner=N routes=N reason=modifier|building|civic|plotGroup|suppressed`. Self-throttles (on change only).
- **`[TRD/route]`** (level 2) — one line per committed slot: `src=ID dst=ID dstOwner=N profit=N modifier=N`.
  Full per-turn route topology.
- **`[TRD/plotGroup]`** (level 1) — in `updatePlotGroup(Player)` when a city's group membership changes:
  `city=ID player=N newGroup=ID oldGroup=ID`. Makes connectivity changes visible in `/events`.
- **`[TRD/bonus]`** (level 2) — in `updatePlotGroupBonus` when a group gains/loses a bonus:
  `city=ID player=N bonus=NAME change=+1|-1`.

### 4.5 `/diagnostic/tradeRoutes?player=N` (MEDIUM) — mailbox-endpoint shape

The "full topology in one call" view the Orwell bar needs, evaluated on the game thread via the mailbox
(same as `placementSweep`):
```json
{ "player": N, "turn": T, "cities": [
  { "id": C, "name": "...", "routes": 3, "connectedToCapital": true, "disordered": false,
    "tradeYield": 14,
    "slots": [ {"dst": D, "dstOwner": P, "profit": 47, "modifier": 130, "foreign": false} ] } ] }
```
Reconstructs every active route, its profit, and domestic/foreign status without the screen.

---

## 5. Tier assessment

| Tier | Name | Trade-routes status |
|---|---|---|
| 0 | Oblivious | (below) |
| **1** | **Telescreen** | `/cities commerce` folds trade in; no breakout in the polling snapshot. |
| **2** | **Informant** | **Partly here today** via `/diagnostic/cityInput` (per-city `tradeRoutes`/`maxTradeRoutes`/`tradeYield` on demand). |
| 3 | Big Brother | + `[TRD/*]` log domain (§4.4) + `/diagnostic/tradeRoutes` topology (§4.5) + connected-bonus list (§4.2): topology + connectivity changes on the wire. |
| 4 | Thought Police | + `/cities` extended with `connectedToCapital`/`tradeYield`/suppression/`connectedBonuses` (§4.1–4.2) for all players from the polling snapshot. |
| 5 | Meta | + per-route profit-formula input decomposition + per-pair diplomatic-eligibility matrix. |

**To climb toward the Orwell bar:** §4.2 (connected-bonus list) is the cascade-critical one;
§4.1 (`connectedToCapital` + suppression) + §4.5 (topology endpoint) complete the route picture. All
cheap, gated, off by default.

---

## 6. Cascade-specific notes

- **The bonus oracle gap is the most urgent.** `requires.operate` resource-dormancy calls
  `hasBonus(eBonus)` (resolves through `m_paiNumBonuses`). Until §4.2 surfaces it as a general per-city
  observable, the resource-dormancy shadow cannot be verified from the API. See
  [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md).
- **`isConnectedToCapital`** is needed for the cascade to match `totalTradeModifier` (route selection +
  yields) and the maintenance modifier — §4.1.
- **No per-turn "routes changed" event** means a plot-group split silently dropping routes (e.g. a
  pillaged road) is invisible; `[TRD/update]` + `[TRD/plotGroup]` (§4.4) plug it.
- **Plot groups are per-player, not global** — two cities connected for player A are not necessarily
  connected for player B. The cascade must query per owning-player, never per team or globally.

---

## 7. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine classes → `Sources/Engine/`,
> `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| Per-city slot budget | `CvCity::getTradeRoutes` — `Sources/Engine/CvCity.cpp:15347` |
| Route formation | `CvCity::updateTradeRoutes` — `Sources/Engine/CvCity.cpp:15379`; clear `:15362` |
| Base trade profit / total modifier | `CvCity::getBaseTradeProfit` `:11595` / `totalTradeModifier` `:11527` / `calculateTradeProfit` `:11630` |
| Trade yield store/read | `CvCity::setTradeYield` `:11507` / `getTradeYield` `:11520` |
| Capital connectivity | `CvCity::isConnectedToCapital` — `Sources/Engine/CvCity.cpp:6681` |
| Player route order | `CvPlayer::updateTradeRoutes` — `Sources/Engine/CvPlayer.cpp:4274` |
| Diplomatic eligibility | `CvPlayer::canHaveTradeRoutesWith` — `Sources/Engine/CvPlayer.cpp:24210` |
| Player coastal routes | `CvPlayer::getCoastalTradeRoutes` — `Sources/Engine/CvPlayer.cpp:11083` |
| Trade/bonus network | `CvPlot::isTradeNetwork` `:5653` / `isBonusNetwork` `:5645` / `isTradeNetworkConnected` `:5681` |
| Plot-group update / bonus accounting | `CvPlot::updatePlotGroup(Player)` `:8925` / `updatePlotGroupBonus` `:1752` |
| `/cities` snapshot | `Sources/Tools/CvHttpServer.cpp` (`publishIfDue` / `renderCities`) |
| `/diagnostic/cityInput` trade block (already exposes count + yields) | `Sources/Tools/CvHttpServer.cpp:1374` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/diagnostic/cityInput`, `/events`)
  these hooks extend, and the live-read rules.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — the cascade
  side: the bonus-connectivity network is its resource-dormancy oracle (`hasBonus` → `requires.operate`).
- [`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) — `isConnectedToCapital` also gates the
  city maintenance modifier mapped there.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
