# Observability — Improvements & plot yields — what's on the wire for per-plot state

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites re-confirmed in `Sources/Engine/CvPlot.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvPlot.cpp` (plot doTurn / upgrade / yield / feature), `Sources/Engine/CvGame.cpp` + `Sources/Engine/CvPlayer.cpp` (upgrade timers/rates), `Sources/Tools/CvHttpServer.cpp` (`/cities`), `Sources/Utils/PlotSnapshot.cpp` (CSV), the `[WAI]` tags in `Sources/AI/`. Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> Per-plot improvement and yield state is **Tier 1 (Telescreen)** today: the HTTP layer exposes only **city-aggregate** yield rates, and the per-plot `PlotSnapshot` CSV (improvement/feature/route type) is **file-only**, not on any endpoint, and carries no yield or upgrade-progress columns. Improvement upgrades and feature growth/disappearance fire silently. This map walks the per-plot mechanics, names what is and isn't observable, and proposes the `/plots` endpoint + transition log tags + yield-decomposition diagnostic that climb it.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> **Why this matters for the cascade:** improvements are the primary output of the worker AI and the
> subject of `ImprovementYieldChanges` building effects. The cascade must shadow them turn-by-turn
> without looking at the screen — this map defines what the verification substrate provides and what is
> missing. See [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md).

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works — per-plot mechanics

### 1.1 Per-turn entry (`CvPlot::doTurn`, CvPlot.cpp:686)

`CvMap::doTurn` iterates all plots and calls `CvPlot::doTurn` on each. Inside, in order:

1. Ownership duration increment.
2. Bonus discovery (`doBonusDiscovery`, CvPlot.cpp:743).
3. Bonus depletion (`doBonusDepletion`, CvPlot.cpp:857) — only when a bonus is present and was not
   discovered this turn.
4. **Improvement upgrade** (`doImprovementUpgrade`) — only when an improvement is present,
   `isImprovementUpgradable()` is true, **and** either the plot `isBeingWorked()` (CvPlot.cpp:5347) OR
   the improvement XML has `isUpgradeRequiresFortify()`.
5. **Feature growth/disappearance** (`doFeature`, CvPlot.cpp:10790).
6. Culture diffusion.

### 1.2 Improvement upgrade (`CvPlot::doImprovementUpgrade`, CvPlot.cpp:918)

- **Cache round guard:** skips entirely when the team's last valid upgrade-cache round equals
  `m_iCurrentRoundofUpgradeCache` (the upgrade target set has not changed).
- **Target selection:** walks `getImprovementUpgrade()` (the main upgrade) plus
  `getNumAlternativeImprovementUpgradeTypes()` / `getAlternativeImprovementUpgradeType(i)`, calling
  `canHaveImprovement` on each. If none is eligible, returns.
- **Progress advance:** if `getImprovementUpgradeProgress() < iTime` (and fortify condition met if
  required), adds `GET_PLAYER(getOwner()).getImprovementUpgradeProgressRate(eType)` (CvPlayer.cpp:7775 —
  base 100 + civic/trait/tech modifiers). Threshold = `100 * CvGame::getImprovementUpgradeTime(eType)`;
  `getImprovementUpgradeTime` (CvGame.cpp:3277) = XML `getUpgradeTime()` scaled by game speed and era,
  min 1.
- **Upgrade trigger** (progress ≥ threshold):
  - Single target → `setImprovementType(eUpgrade)` immediately.
  - Multiple targets, human → `upgradePlotPopup` (player picks manually).
  - Multiple targets, AI → scores each via `CvCity::AI_getImprovementValue`, picks the best, then
    `setImprovementType(eBestUpgrade)`. **This AI decision is silent — no `[WAI]`/`[CIT]`/any tag.**
- `m_iUpgradeProgress` (raw accumulator) and `m_bImprovementUpgradable` (flag) are both serialized.

### 1.3 `setImprovementType` side-effects (CvPlot.cpp:7471)

Updates area + player improvement counts; may set/change feature (`getNumFeatureChangeTypes`) or bonus
(`getBonusChange`); resets `m_iUpgradeProgress`/`m_bImprovementUpgradable`; calls `updateYield`
(CvPlot.cpp:8154) which recomputes `m_aiYield` and propagates to the working city's plot-yield sum;
updates visible improvement for all seeing teams; updates bonus-network connectivity (`updatePlotGroup`);
emits culture pushes if `getCulture() > 0`.

### 1.4 Yield derivation (`CvPlot::calculateYield`, CvPlot.cpp:8320)

The full per-plot stack: nature yield (terrain + bonus + feature, floored 0); plot extra yield
(`m_aExtraYield`, event-driven); city-tile bonuses + pop divisor (if city centre); player terrain yield
change (tech/civic/trait); sea-plot yield; working-city `getYieldChangeAt`; landmark yield (if
`GAMEOPTION_MAP_PERSONALIZED`); extra-yield threshold ±; golden-age yield; then the **improvement yield
delta** `calculateImprovementYieldChange` (CvPlot.cpp:8243) when not a city tile and an improvement is
present.

`calculateImprovementYieldChange` is a 7+-term additive stack: XML base change; river-side bonus;
irrigation bonus (when `isIrrigationAvailable()`); route bonus (actual or best route); tech yield
changes (all techs when `bOptimal`, else only the player's); civic yield changes (same pattern);
player-level `getImprovementYieldChange()` (accumulates trait + civic + building
`GlobalImprovementYieldChanges`/`ImprovementYieldChanges`); team-level change; bonus-resource yield bonus
for this improvement on this bonus. Floored so the delta cannot drive total yield negative.

The cached `m_aiYield[eYield]` array is updated by `updateYield` (CvPlot.cpp:8154) whenever any input
changes — it is the live game-state value (free to read).

### 1.5 Feature growth/disappearance (`CvPlot::doFeature`, CvPlot.cpp:10790)

- **Disappearance:** if the existing feature has `getDisappearanceProbability() > 0`, rolls
  `getSorenRandNum(100 * speedPercent)`; on hit fires `setFeatureType(NO_FEATURE)` and returns.
- **Growth/spread:** for each feature type with non-zero growth/spread probability, on an
  improvement-free (or `isCanGrowAnywhere` + unworked non-water) plot, sums probability from each
  cardinal neighbour that already has that feature (neighbour-without-improvement uses the feature's
  `getGrowthProbability()`; neighbour-with-improvement uses the improvement's
  `getFeatureGrowthProbability()` — improvements can suppress or alter spread), plus the feature's own
  `getSpreadProbability()`; scales by `FEATURE_GROWTH_MODIFIER` (and `ROUTE_FEATURE_GROWTH_MODIFIER` if
  routed); rolls and fires `setFeatureType(...)` on the first match. The in-game `AddDLLMessage`
  notification is **UI-only — not logged, not on HTTP**.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### 2.1 What IS observable

| Surface | What it provides | Where |
|---|---|---|
| `PlotSnapshot_*.csv` | per-plot `improvement` / `feature` / `route` / `bonus` / `owner` type strings — written every turn + on start/load | **log file only, no HTTP path** |
| `GET /cities` → `food` / `production` / `commerce` | **city-aggregate** yield rates — the sum over worked plots, NOT decomposed by plot | `Sources/Tools/CvHttpServer.cpp` |
| `[WAI/score]` (gPlayerLogLevel ≥ 2) | `yield=` on individual improvement candidates, keyed by `(x,y)` — the `calculateImprovementYieldChange` delta for that candidate | `Sources/AI/` / `/events` |
| `[WAI/build/cand]` (level 3) | `yield=` + `time=` for each qualifying build on a bonus plot | `Sources/AI/` / `/events` |
| `[WAI/build/hit]` / `[WAI/build/winner]` (level 2) | `yield=` for the winning build on a bonus plot | `Sources/AI/` / `/events` |

### 2.2 What is NOT observable

| State | Why it is opaque |
|---|---|
| Per-plot upgrade progress (`m_iUpgradeProgress`) | Not in PlotSnapshot, not on any endpoint |
| Upgrade completion event | No log line when `setImprovementType` fires from `doImprovementUpgrade` |
| Feature growth / disappearance events | No log; the `AddDLLMessage` (growth path) is UI-only |
| Per-plot yield breakdown | `calculateYield`/`calculateImprovementYieldChange` results never emitted; only city aggregates on HTTP |
| AI improvement-choice (multi-upgrade) | Completely silent — which candidate won and why is unobservable |
| `isBeingWorked()` per plot | Not in PlotSnapshot or HTTP; needed to know whether the upgrade timer is advancing |
| `ImprovementYieldChanges` building contribution | Not broken out anywhere; only the aggregate city rates surface the effect |
| `isImprovementUpgradable()` flag | Not in PlotSnapshot or HTTP; no way to tell which plots are mid-upgrade short of the save |
| Route on plot (correlatable to HTTP) | PlotSnapshot has `route`, but file-only — cannot correlate turn-by-turn against HTTP |

---

## 3. The gap

Given only the current `/cities` aggregates + `/events` + the file-only PlotSnapshot, an observer
**cannot**:

1. **Read any per-plot improvement state from HTTP.** No endpoint returns
   `{x, y, improvement, upgradeProgress, upgradeTimeLeft, feature, route, yield[]}`. Which farms are
   partway to villages, which forts are mid-timer, which tiles are forested vs cleared — all invisible
   on the wire.
2. **See upgrade completions.** A forest clearing or a seed-camp promoting to a farm fires
   `setImprovementType` with no `/events`, no `[ENG]`, no `[WAI]` line — discoverable only at the next
   (file-only, turn-lagged) PlotSnapshot.
3. **See feature dynamics.** Forest spread, jungle encroachment, feature disappearance fire no
   observable event (the DLL message is UI-only).
4. **Decompose yield.** City aggregates say nothing about which plot contributes what, which improvement
   bonus is active, whether an improvement is suppressed by missing irrigation/road, or whether a civic
   change just buffed all farms. The 7+-term `calculateImprovementYieldChange` stack is internally
   consistent but externally invisible.
5. **See AI improvement-selection.** The multi-upgrade choice is a black hole — no log of which
   candidate won or its score. Directly load-bearing for the cascade's "replace with cascade + tally"
   goal.

**Secondary:** PlotSnapshot bridges *some* of the gap (current improvement/feature type) but is
file-only, carries no yield/progress/`isBeingWorked`, and must be correlated with an endpoint across a
turn boundary. The `[WAI]` yield logs are scoped to **bonus-plot candidates only** (not the whole map,
only at `gPlayerLogLevel ≥ 2`); farms/workshops/villages on plots not currently evaluated by a worker
are never logged.

---

## 4. Proposed hooks (concrete additions)

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes]. Target: climb
**Tier 1 → Tier 3** for this system (per-plot HTTP snapshot + transition events).

### 4.1 `/plots` snapshot endpoint (HIGH) — snapshot-field shape

A new HTTP snapshot returning per-plot state, published from the game thread via `publishIfDue`, same
immutable-snapshot contract as `/cities`. Minimum schema per plot:
```json
{ "x": 12, "y": 34, "owner": 0,
  "improvement": "IMPROVEMENT_FARM", "upgradeTarget": "IMPROVEMENT_VILLAGE",
  "upgradeProgress": 47, "upgradeTotal": 100, "upgradeTurnsLeft": 3, "improvementUpgradable": true,
  "feature": "NONE", "route": "ROUTE_ROAD", "bonus": "BONUS_WHEAT",
  "isBeingWorked": true, "workingCityId": 7, "yield": [3, 1, 0] }
```
`upgradeTotal` = `100 * CvGame::getImprovementUpgradeTime(eType)`; `upgradeTurnsLeft` =
`getUpgradeTimeLeft(eImprovement, eOwner)` (CvPlot.cpp:6051); `yield[]` = the cached `m_aiYield`
(free to read). To bound the snapshot (up to 9600+ plots): default to owned plots with an improvement
or feature; `?all=1` for the full map; `?player=N` to scope. Cost is pure memory access, amortized at
the 5s publish rate.

### 4.2 `[PLT/upgrade]` log tag + `plotChange` event (HIGH) — gated-log shape

When `setImprovementType` fires from `doImprovementUpgrade` completing, emit (and tee to `/events`):
```
[PLT/upgrade] turn=N owner=P x=X y=Y from=IMPROVEMENT_SEED_CAMP to=IMPROVEMENT_FARM progress=N time=N
```
For the AI-selection branch add `[PLT/upgrade/aiChoice] … chosen=BEST score=N candidates=N` (level 2,
candidate scores at level 3). Gate: `gPlayerLogLevel ≥ 1` for completion, `≥ 2` for the AI-choice trace.
SSE `plotChange` payload `{turn, x, y, change:"improvementUpgrade", from, to, owner}`.

### 4.3 `[PLT/feature]` log tag for feature transitions (HIGH) — gated-log shape

When `setFeatureType` fires from `doFeature` (growth and disappearance paths):
```
[PLT/feature] turn=N x=X y=Y change=grew|vanished feature=FEATURE_FOREST owner=P
```
Emit at the `doFeature` call sites directly (not in `setFeatureType`, which has many other callers — nukes,
builds, events). Gate `gPlayerLogLevel ≥ 1`; SSE `plotChange` with `change:"featureGrew"|"featureVanished"`.

### 4.4 Extend the PlotSnapshot CSV schema (MEDIUM)

Add `upgradeProgress` (`getImprovementUpgradeProgress()`, 0 when not upgradable), `upgradeTurnsLeft`
(`getUpgradeTimeLeft(...)`), `isBeingWorked` (1/0), `yieldFood`/`yieldProd`/`yieldComm` (`m_aiYield[0/1/2]`,
zero cost). Bump `schema=2 → 3` (non-breaking for version-checking consumers). Emit site:
`writePlotSnapshot` in `Sources/Utils/PlotSnapshot.cpp`.

### 4.5 `/diagnostic/plotYield?x=N&y=M&player=P` (MEDIUM) — mailbox-endpoint shape

Returns the full `calculateImprovementYieldChange` breakdown for a plot/player, attributing each term
(base, river, irrigation, route, techSum, civicSum, playerMod, teamMod, bonusMod, total) per yield.
Evaluated on the game thread via the mailbox (same as existing diagnostics); needs a
`calculateImprovementYieldChangeDetailed` wrapper returning the decomposed terms.

### 4.6 `/diagnostic/upgradeProgress?player=N` (LOW) — mailbox-endpoint shape

Lists all plots owned by N with `isImprovementUpgradable()`, each `{x, y, improvement, upgradeTarget,
upgradeProgress, upgradeTotal, upgradeTurnsLeft}` — for verifying the cascade's view of mid-upgrade plots.

---

## 5. Tier assessment

| Tier | Name | Improvements/yields status |
|---|---|---|
| 0 | Oblivious | (below current) |
| **1** | **Telescreen** | **Current state.** City-aggregate yields on HTTP; per-plot type strings in the file-only PlotSnapshot; bonus-plot `[WAI]` yields. No per-plot HTTP state, no transition events, no yield decomposition. |
| 2 | Informant | + `/diagnostic/plotYield` (§4.5) + `/diagnostic/upgradeProgress` (§4.6): per-plot detail on demand. |
| 3 | Big Brother | + `/plots` snapshot (§4.1) + `[PLT/upgrade]` / `[PLT/feature]` transition events (§4.2–4.3): per-plot state + transitions on the wire. |
| 4 | Thought Police | + AI improvement-choice trace (§4.2 aiChoice) for all players — every upgrade decision attributable. |
| 5 | Meta | + full per-plot yield-term decomposition streamed for all worked plots. |

**To climb 1 → 3 (the Orwell bar):** implement §4.1 (`/plots`) + §4.2–4.3 (transition log tags). All
cheap, gated, off by default — `m_aiYield` and the upgrade fields are pure reads.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine classes → `Sources/Engine/`,
> `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`, `PlotSnapshot` → `Sources/Utils/`).
> Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| Per-turn plot entry | `CvPlot::doTurn` — `Sources/Engine/CvPlot.cpp:686` |
| Bonus discovery / depletion | `CvPlot::doBonusDiscovery` `:743` / `doBonusDepletion` `:857` |
| Improvement upgrade timer | `CvPlot::doImprovementUpgrade` — `Sources/Engine/CvPlot.cpp:918` |
| Upgrade time-left | `CvPlot::getUpgradeTimeLeft` — `Sources/Engine/CvPlot.cpp:6051` |
| `isBeingWorked` | `CvPlot::isBeingWorked` — `Sources/Engine/CvPlot.cpp:5347` |
| `setImprovementType` side-effects | `CvPlot::setImprovementType` — `Sources/Engine/CvPlot.cpp:7471` |
| Per-plot yield delta | `CvPlot::calculateImprovementYieldChange` — `Sources/Engine/CvPlot.cpp:8243` |
| Full per-plot yield stack | `CvPlot::calculateYield` — `Sources/Engine/CvPlot.cpp:8320` |
| Yield recache | `CvPlot::updateYield` — `Sources/Engine/CvPlot.cpp:8154` |
| Feature growth/disappearance | `CvPlot::doFeature` — `Sources/Engine/CvPlot.cpp:10790` |
| Speed/era-scaled upgrade time | `CvGame::getImprovementUpgradeTime` — `Sources/Engine/CvGame.cpp:3277` |
| Per-player upgrade rate | `CvPlayer::getImprovementUpgradeProgressRate` — `Sources/Engine/CvPlayer.cpp:7775` |
| `/cities` snapshot (city aggregates; NO per-plot) | `Sources/Tools/CvHttpServer.cpp` (`publishIfDue` / `renderCities`) |
| Per-turn CSV (type strings; NO yield/progress) | `Sources/Utils/PlotSnapshot.cpp` (`writePlotSnapshot`) |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/events`, `/diagnostic`) the `/plots`
  endpoint extends, and the live-read rules.
- [`food-yields-wastage.md`](food-yields-wastage.md) — the city-aggregate food yield these per-plot
  contributions sum into (`m_aiBaseYieldRate`).
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why the
  cascade must shadow improvement/yield state turn-by-turn (`ImprovementYieldChanges`).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
