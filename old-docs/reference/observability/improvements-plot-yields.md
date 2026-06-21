> DRAFT observability map (2026-06-18, agent: claude-sonnet-4-6) — all claims cited from
> live code; verify before relying.

# Observability map: Improvements & Plot Yields

**System scope:** improvement upgrade timers, yield derivation per plot, feature
spread/growth/disappearance.

**Why this matters for #428/#430:** improvements are the primary output of the worker AI
and the subject of `ImprovementYieldChanges` building effects (already shipped). The cascade
must be able to shadow them turn-by-turn without ever looking at the game screen — this map
defines what the verification substrate currently provides and what is missing.

---

## 1. How it actually works

### 1a. Per-turn entry point

`CvMap::doTurn` (CvMap.cpp:491) iterates all plots and calls `CvPlot::doTurn` on each
(CvMap.cpp:499). `CvPlot::doTurn` (CvPlot.cpp:650) is the root of every per-turn plot
mutation. Inside it, in order:

1. Ownership duration increment (CvPlot.cpp:656).
2. Bonus discovery (`doBonusDiscovery`, CvPlot.cpp:662).
3. Bonus depletion (`doBonusDepletion`, CvPlot.cpp:669) — only when a bonus is present and
   was not discovered this turn.
4. **Improvement upgrade** (CvPlot.cpp:672-675) — only when:
   - an improvement is present (`getImprovementType() != NO_IMPROVEMENT`),
   - `isImprovementUpgradable()` returns true (set when `setImprovementType` fires and the
     new improvement has a non-zero `getImprovementUpgrade()` + positive
     `getImprovementUpgradeTime`; CvPlot.cpp:7498-7504),
   - **and** either the plot `isBeingWorked()` (a city citizen is assigned here) OR the
     improvement XML has `isUpgradeRequiresFortify()` (CvPlot.cpp:672).
5. **Feature growth/disappearance** (`doFeature`, CvPlot.cpp:687).
6. Culture diffusion (`doCulture`, CvPlot.cpp:689).

### 1b. Improvement upgrade mechanics (`doImprovementUpgrade`)

`CvPlot::doImprovementUpgrade` (CvPlot.cpp:883-1010):

- **Cache round guard** (CvPlot.cpp:889-892): if the team's
  `getLastRoundOfValidImprovementCacheUpdate()` equals `m_iCurrentRoundofUpgradeCache`, skip
  entirely (the upgrade target set has not changed since last eval).
- **Target selection** (CvPlot.cpp:894-931): walks
  `CvImprovementInfo::getImprovementUpgrade()` (the "main" upgrade) and
  `getNumAlternativeImprovementUpgradeTypes()` / `getAlternativeImprovementUpgradeType(iI)`
  (CvPlot.cpp:908-921), calling `canHaveImprovement` on each. Builds a simple hash of
  eligible targets. If no eligible target exists, returns (optionally caches if definitively
  none; CvPlot.cpp:928).
- **Progress advance** (CvPlot.cpp:935-943): if
  `getImprovementUpgradeProgress() < iTime` and the fortify condition is met (if required),
  calls `changeImprovementUpgradeProgress(GET_PLAYER(getOwner()).getImprovementUpgradeProgressRate(eType))`
  (CvPlot.cpp:943).
  - `getImprovementUpgradeProgressRate` (CvPlayer.cpp:7773): base 100 + civic/trait/tech
    modifiers (`getImprovementUpgradeRateModifier()` + per-type
    `getImprovementUpgradeRateModifierSpecific()`).
  - Total time threshold: `100 * CvGame::getImprovementUpgradeTime(eType)` (CvPlot.cpp:932).
  - `getImprovementUpgradeTime` (CvGame.cpp:3277): XML `getUpgradeTime()` scaled by game
    speed (`getHammerCostPercent()`) and era (`getImprovementPercent()`), minimum 1.
- **Upgrade trigger** (CvPlot.cpp:945-1008): when progress >= threshold:
  - Single upgrade target: `setImprovementType(eUpgrade)` immediately (CvPlot.cpp:957).
  - Multiple targets, human player: `upgradePlotPopup` popup (CvPlot.cpp:961) — human picks
    manually via `CvDLLButtonPopup::launchImprovementUpgradeOptionsPopup`.
  - Multiple targets, AI: calls `CvCity::AI_getImprovementValue` for each candidate and
    picks the best (CvPlot.cpp:977-1006).
- `m_iUpgradeProgress` is the raw per-plot accumulator (serialized;
  CvPlot.cpp:11089/11577). `m_bImprovementUpgradable` is a pre-computed boolean flag
  (serialized; CvPlot.cpp:11410/11833).

### 1c. `setImprovementType` side effects

`CvPlot::setImprovementType` (CvPlot.cpp:7436):

- Updates area improvement count (CvPlot.cpp:7472, 7538).
- Updates player improvement count (CvPlot.cpp:7476, 7542).
- May set/change feature type if the new improvement has `getNumFeatureChangeTypes` entries
  (CvPlot.cpp:7481-7488).
- May set bonus type if the new improvement has `getBonusChange() != NO_BONUS`
  (CvPlot.cpp:7490-7496).
- Resets `m_iUpgradeProgress` to 0 and `m_bImprovementUpgradable` to the appropriate value
  (CvPlot.cpp:7499-7504).
- Calls `updateYield()` (CvPlot.cpp:7570), which recomputes `m_aiYield` and propagates
  changes to the working city's `changePlotYield` (CvPlot.cpp:8142).
- Updates visible improvement for all seeing teams (CvPlot.cpp:7526-7532).
- Updates bonus network connectivity (`updatePlotGroup`, CvPlot.cpp:7566).
- Emits culture pushes if the improvement has `getCulture() > 0` (CvPlot.cpp:7543-7545).

### 1d. Yield derivation

`CvPlot::calculateYield` (CvPlot.cpp:8285) is the full per-plot yield function:

1. **Nature yield** = `calculateNatureYield` (CvPlot.cpp:8158): terrain base + bonus resource
   yield change + feature yield change (river and flat). Floor: 0.
2. **Extra yield** = `m_aExtraYield[eYield]` (CvPlot.cpp:8315) — event-driven modifier,
   serialized (CvPlot.cpp:11516/11898).
3. **City tile bonuses** (CvPlot.cpp:8326-8332): `YieldInfo::getCityChange` + population
   divisor if this plot IS the city centre.
4. **Terrain yield changes** from player (CvPlot.cpp:8339): tech/civic/trait modifiers via
   `GET_PLAYER(ePlayer).getTerrainYieldChange(...)`.
5. **Sea plot yield** (CvPlot.cpp:8343): `GET_PLAYER(ePlayer).getSeaPlotYield(eYield)`.
6. **Working city yield change** (CvPlot.cpp:8348): `pWorkingCity->getYieldChangeAt(this, eYield)`.
7. **Landmark yield** (CvPlot.cpp:8353): if `GAMEOPTION_MAP_PERSONALIZED`.
8. **Extra yield threshold** (CvPlot.cpp:8358-8365): +/- `EXTRA_YIELD` if above/below player
   threshold.
9. **Golden age yield** (CvPlot.cpp:8368-8371).
10. **Improvement yield delta** = `calculateImprovementYieldChange` (CvPlot.cpp:8395), called
    only when NOT a city tile and an improvement is present.

`calculateImprovementYieldChange` (CvPlot.cpp:8208-8282):

- XML base yield change (CvPlot.cpp:8212).
- River side bonus (CvPlot.cpp:8214-8217).
- Irrigation bonus when `isIrrigationAvailable()` (CvPlot.cpp:8219-8221).
- Route bonus — actual current route or best route depending on `bOptimal`/`bBestRoute`
  (CvPlot.cpp:8226-8248).
- Tech yield changes — all techs when `bOptimal`, otherwise only those the player has
  (CvPlot.cpp:8250-8254) — note: this iterates ALL tech infos in the optimal case.
- Civic yield changes — same pattern (CvPlot.cpp:8255-8261).
- Player-level improvement yield change = `GET_PLAYER(ePlayer).getImprovementYieldChange()`
  (CvPlot.cpp:8264) — accumulates trait + civic + building
  `GlobalImprovementYieldChanges` (CvPlayer.cpp:7458-7462) + `ImprovementYieldChanges`
  modifiers from buildings/civics/traits.
- Team-level improvement yield change (CvPlot.cpp:8265).
- Bonus resource yield bonus for this improvement on this bonus type (CvPlot.cpp:8274).
- Floor: improvement delta cannot drive the total yield negative (CvPlot.cpp:8279-8281).

The cached `m_aiYield[eYield]` array (CvPlot.cpp:8138) is updated by `updateYield`
(CvPlot.cpp:8119) whenever any input changes (improvement, route, irrigation, terrain,
feature, bonus, etc.). The cache is the live game state value.

### 1e. Feature spread/growth/disappearance (`doFeature`)

`CvPlot::doFeature` (CvPlot.cpp:10749):

- **Disappearance** (CvPlot.cpp:10755-10762): if the existing feature has
  `getDisappearanceProbability() > 0`, rolls `getSorenRandNum(100 * speedPercent)`.
  Fires `setFeatureType(NO_FEATURE)` and returns (does NOT then check for growth).
- **Growth/spread** (CvPlot.cpp:10765-10822): for each feature type in the global list,
  checks:
  - Non-zero `getGrowthProbability()` or `getSpreadProbability()`.
  - Plot has no improvement (unless `isCanGrowAnywhere()` and unworked non-water).
  - Bonus-feature compatibility.
  - `canHaveFeature` returns true.
  - Sums probability from each cardinal neighbour that already has this feature:
    - Neighbour without improvement: `kFeature.getGrowthProbability()`.
    - Neighbour with improvement: `GC.getImprovementInfo(neighbour.getImprovementType()).getFeatureGrowthProbability()` (CvPlot.cpp:10786) — key: improvements can suppress or alter feature spread probability.
    - Plus `getSpreadProbability()` from the feature itself.
  - Scales by `FEATURE_GROWTH_MODIFIER` and `ROUTE_FEATURE_GROWTH_MODIFIER` (if route
    present).
  - Rolls `getSorenRandNum(100 * speedPercent)` (CvPlot.cpp:10802). If probability exceeds
    the roll, `setFeatureType((FeatureTypes)iI)` (CvPlot.cpp:10804) fires.
  - First match fires and breaks; subsequent feature types are not evaluated.
  - An `AddDLLMessage` in-game notification fires only for human players near a city
    (CvPlot.cpp:10808-10819). **This message is NOT emitted to the log or HTTP.**

### 1f. AI improvement-selection decision (multi-upgrade case)

When `doImprovementUpgrade` reaches the AI selection branch (CvPlot.cpp:964-1006), it calls
`CvCity::AI_getYieldMultipliers` and `CvCity::AI_getImprovementValue` for each candidate.
This decision is **silent** — no `[WAI]`, `[CIT]`, or any other log tag is emitted.
The selected upgrade is applied via `setImprovementType(eBestUpgrade)` (CvPlot.cpp:1006)
with no further notification.

---

## 2. Current observability

**Tier: 1 (Telescreen)** — coarse city-aggregate snapshots only; zero per-plot improvement
or yield state is visible from outside.

### What IS exposed today

| Surface | What it provides | Endpoint / file |
|---|---|---|
| `PlotSnapshot_*.csv` (CvPlot.cpp rows) | `improvement` (type string), `feature` (type string), `route`, `bonus`, per-plot `owner`, `improvementCurrentValue` — written every turn + on start/load | log file only (no HTTP path) |
| `/cities` → `food` / `production` / `commerce` | **city-aggregate** yield rates (`getYieldRate(YIELD_*)`) — the sum over all worked plots, NOT decomposed by plot | CvHttpServer.cpp:1550-1552 |
| `[WAI/score]` (gPlayerLogLevel >= 2) | `yield=` on individual improvement candidates, keyed by `(x,y)` — the `calculateImprovementYieldChange` delta for that candidate build | BuildEvaluation.log / `/events` |
| `[WAI/build/cand]` (gPlayerLogLevel >= 3) | `yield=` and `time=` for each qualifying build on a bonus plot | BuildEvaluation.log / `/events` |
| `[WAI/build/hit]` / `[WAI/build/winner]` (level 2) | `yield=` for the winning build on a bonus plot | BuildEvaluation.log / `/events` |

### What is NOT exposed today

| State | Why it is opaque |
|---|---|
| **Per-plot upgrade progress** (`m_iUpgradeProgress`) | Not in PlotSnapshot, not in any HTTP endpoint; invisible externally |
| **Upgrade completion event** | No log line when `setImprovementType` fires from `doImprovementUpgrade` — no `[WAI]`, `[ENG]`, or any tagged line |
| **Feature growth event** | No log line when `setFeatureType` fires from `doFeature` (growth path); the `AddDLLMessage` at CvPlot.cpp:10813 is UI-only, not logged |
| **Feature disappearance event** | Same: `setFeatureType(NO_FEATURE)` at CvPlot.cpp:10760 has zero logging |
| **Per-plot yield breakdown** | `calculateYield` / `calculateImprovementYieldChange` results are not emitted per-turn; only city aggregate rates are on HTTP |
| **AI improvement-choice decision** (multi-upgrade branch) | Completely silent — which of competing upgrades the AI chose and why is not observable |
| **`isBeingWorked()` state per plot** | Not in PlotSnapshot or HTTP; needed to know whether upgrade timer is advancing |
| **`ImprovementYieldChanges` building contribution** | Not broken out anywhere — only the aggregate city `food/production/commerce` rates surface the effect |
| **`isImprovementUpgradable()` flag** | Not in PlotSnapshot or HTTP; no way to tell which plots are "in upgrade progress" without reading the serialized save |
| **Route on plot** | PlotSnapshot has `route` column, but it is file-only (no HTTP). Cannot be correlated turn-by-turn against HTTP data |

---

## 3. The gap

Scale + reconstruction bar: see [DEC-obs-scale](../../decisions.md#dec-obs-scale).

**Critical gaps (cannot reconstruct without screen):**

1. **No per-plot improvement state on HTTP.** The HTTP layer exposes only city-aggregate
   yield rates. A per-plot map of `{x, y, improvement, upgradeProgress, upgradeTimeLeft,
   feature, route, yield[food, prod, comm]}` does not exist on any endpoint. An agent
   watching the wire cannot know: which tiles have forests vs cleared, which farms are
   partway through upgrading to villages, which forts are mid-timer.

2. **Upgrade completions are silent.** When a forest clears (improvement upgrade fires
   `setImprovementType`), or when a seed camp auto-promotes to a farm, there is no event
   on `/events`, no `[ENG]` line, no `[WAI]` line. An agent watching the wire only
   discovers the change at the NEXT PlotSnapshot — a file that lags up to the start of
   the next turn and is not on HTTP at all.

3. **Feature dynamics are silent.** Forest spread, jungle encroachment, feature
   disappearance — none of these fire any observable event. The DLL message
   (`AddDLLMessage`, CvPlot.cpp:10813) goes only to the human player's message queue (not
   to a log or the event stream).

4. **Yield derivation is opaque.** The current aggregate city rates tell you nothing about
   which plot is contributing what, which improvement bonus is active, whether an
   improvement is suppressed by missing irrigation/road, or whether a civic change just
   buffed all farms. The full `calculateImprovementYieldChange` stack (7+ additive terms)
   is internally consistent but externally invisible.

5. **AI improvement-selection is silent.** When a plot has multiple upgrade candidates and
   the AI chooses between them (CvPlot.cpp:964-1006), there is no log line recording which
   candidate won or what scores were computed. This is a black hole for the #428/#430
   "replace with cascade + tally" goal.

**Secondary gaps:**

6. PlotSnapshot is a file-only artifact — it is not on HTTP. It bridges some of the gap
   (current improvement/feature type) but: (a) it does not include yield values, upgrade
   progress, or `isBeingWorked`; (b) it requires correlating a file with an endpoint across
   a turn boundary; (c) it is scoped to worker AI support, not a first-class observability
   surface.

7. The `[WAI]` yield logs are scoped to **bonus-plot improvement candidates** — they only
   fire for plots considered by the worker AI in that turn, not for the whole map, and only
   when `gPlayerLogLevel >= 2`. Non-bonus improvements (farms, workshops, villages) on
   plots not currently being evaluated by a worker are never logged.

---

## 4. Proposed hooks (concrete additions to climb a tier)

Target: climb from **Tier 1 → Tier 3** (Big Brother) for this system — add an HTTP endpoint
for per-plot state + event-stream entries for state transitions.

### Hook A — `/plots` or `/plots?player=N` snapshot endpoint (HIGH PRIORITY)

**What:** a new HTTP snapshot endpoint that returns per-plot state for all (or a player's
owned + worked) plots. Published from the game thread via `publishIfDue`, immutable snapshot
contract same as `/cities`.

**Minimum viable schema per plot:**
```json
{
  "x": 12, "y": 34,
  "owner": 0,
  "improvement": "IMPROVEMENT_FARM",
  "upgradeTarget": "IMPROVEMENT_VILLAGE",
  "upgradeProgress": 47,
  "upgradeTotal": 100,
  "upgradeTurnsLeft": 3,
  "improvementUpgradable": true,
  "feature": "NONE",
  "route": "ROUTE_ROAD",
  "bonus": "BONUS_WHEAT",
  "isBeingWorked": true,
  "workingCityId": 7,
  "yield": [3, 1, 0]
}
```

`upgradeTotal` = `100 * CvGame::getImprovementUpgradeTime(eType)`.
`upgradeTurnsLeft` = `getUpgradeTimeLeft(eImprovement, eOwner)` (CvPlot.cpp:6016).
`yield[0/1/2]` = `m_aiYield[YIELD_FOOD / YIELD_PRODUCTION / YIELD_COMMERCE]` (already
cached in the plot; free to read).

The snapshot is large (up to 9600+ plots on a standard map). To keep it manageable:
- Default: only plots with `getOwner() != NO_PLAYER` and (`getImprovementType() != NO_IMPROVEMENT` or `getFeatureType() != NO_FEATURE`).
- `?all=1`: full map.
- `?player=N`: only plots owned by player N.

**Emit site:** `CvHttpServer::publishIfDue` publishes the snapshot the same way as cities.

**Cost:** reading `m_aiYield` and the upgrade fields is pure memory access — near-zero game
thread cost. The snapshot publication (`publishIfDue`) runs every 5s, so the yield array
read is amortized.

### Hook B — `[PLT/upgrade]` log tag + `/events` stream (HIGH PRIORITY)

**What:** when `setImprovementType` fires as a result of `doImprovementUpgrade` completing,
emit a `[PLT/upgrade]` log line to `PlotLog` (new log file) AND publish a
`"plotChange"` event via `CvHttpServer::publishEvent`.

**Emit site:** inside `CvPlot::doImprovementUpgrade`, at each `setImprovementType(eUpgrade)` /
`setImprovementType(eBestUpgrade)` call site (CvPlot.cpp:957, 1006). The
`upgradePlotPopup` path (human player, multi-upgrade) fires upgrade via
`CvMessageControl::sendImprovementUpgrade` → ultimately also calls `setImprovementType`;
instrument the same tag there.

**Proposed log line format:**
```
[PLT/upgrade] turn=<N> owner=<P> x=<X> y=<Y> from=IMPROVEMENT_SEED_CAMP to=IMPROVEMENT_FARM progress=<progress> time=<total>
```

For the AI-selection branch (CvPlot.cpp:964-1006), add:
```
[PLT/upgrade/aiChoice] turn=<N> owner=<P> x=<X> y=<Y> from=<X> chosen=<BEST> score=<bestScore> candidates=<N>
```
(level 2, emitting candidate scores at level 3).

**Gate:** `gPlayerLogLevel >= 1` for the completion event, `>= 2` for the AI-choice trace.
Call `streamLogTee` to also push to `/events`.

**SSE event:** `"plotChange"` with payload `{turn, x, y, change:"improvementUpgrade", from:"IMPROVEMENT_X", to:"IMPROVEMENT_Y", owner:N}`.

### Hook C — `[PLT/feature]` log tag for feature transitions (HIGH PRIORITY)

**What:** when `setFeatureType` fires from `doFeature` (both growth and disappearance
paths), emit a `[PLT/feature]` tagged line.

**Emit site:** `CvPlot::doFeature` at CvPlot.cpp:10760 (disappearance) and CvPlot.cpp:10804
(growth). The `setFeatureType` call itself at CvPlot.cpp:7094 is called from many other
sites (nukes, improvement builds, events) — add a discriminator parameter or emit at the
call site level in `doFeature` directly to avoid noisy cross-site fires.

**Proposed log line format:**
```
[PLT/feature] turn=<N> x=<X> y=<Y> change=grew|vanished feature=FEATURE_FOREST owner=<P>
```

**Gate:** `gPlayerLogLevel >= 1`. SSE `plotChange` event with `change:"featureGrew"` or
`change:"featureVanished"`.

### Hook D — extend PlotSnapshot CSV schema (MEDIUM PRIORITY)

Add to `PlotSnapshot` schema:
- `upgradeProgress` — `getImprovementUpgradeProgress()`, or 0 when `!isImprovementUpgradable()`.
- `upgradeTurnsLeft` — `getUpgradeTimeLeft(eImprovement, getOwner())`, or 0 when not upgradable.
- `isBeingWorked` — `isBeingWorked()` (1/0).
- `yieldFood`, `yieldProd`, `yieldComm` — `m_aiYield[0/1/2]` (the cached values; zero cost).

These extend schema=2 → schema=3 (bump the `schema=` header field). Non-breaking for
consumers that check the version header.

**Emit site:** `writePlotSnapshot` in `Sources/Utils/PlotSnapshot.cpp` (PlotSnapshot.cpp:294
format string).

### Hook E — `/diagnostic/plotYield?x=N&y=M&player=P` gate-eval endpoint (MEDIUM PRIORITY)

**What:** a diagnostic endpoint that returns the full `calculateImprovementYieldChange`
breakdown for a specific plot and player, attributing each term (base, river, irrigation,
route, tech sum, civic sum, player modifier, team modifier, bonus modifier).

**Emit site:** `CvHttpServer::routeRequest`, evaluated on the game thread via the mailbox
(same as existing diagnostic endpoints). Calls `calculateImprovementYieldChange` with
per-term instrumentation (or a new `calculateImprovementYieldChangeDetailed` wrapper that
returns the decomposed terms).

**Response shape:**
```json
{
  "x": 12, "y": 34, "player": 0, "improvement": "IMPROVEMENT_FARM",
  "yield": {
    "food": {"base": 1, "river": 0, "irrigation": 1, "route": 0, "techSum": 0,
             "civicSum": 0, "playerMod": 1, "teamMod": 0, "bonusMod": 0, "total": 3},
    "production": {...},
    "commerce": {...}
  }
}
```

### Hook F — upgrade-progress summary in `/diagnostic` (LOW PRIORITY)

**What:** `GET /diagnostic/upgradeProgress?player=N` — lists all plots owned by player N
that have `isImprovementUpgradable()` true, with `{x, y, improvement, upgradeTarget,
upgradeProgress, upgradeTotal, upgradeTurnsLeft}`. Useful for verifying the cascade's
understanding of which improvements are mid-upgrade.

---

## 5. Cross-references

- `CvPlot.cpp:650` (`doTurn`) — per-turn entry point.
- `CvPlot.cpp:883` (`doImprovementUpgrade`) — upgrade timer mechanics.
- `CvPlot.cpp:7436` (`setImprovementType`) — the state mutation + yield/bonus/feature side
  effects.
- `CvPlot.cpp:8208` (`calculateImprovementYieldChange`) — per-plot yield delta derivation.
- `CvPlot.cpp:8285` (`calculateYield`) — full per-plot yield stack.
- `CvPlot.cpp:8119` (`updateYield`) — recache `m_aiYield` after any change.
- `CvPlot.cpp:10749` (`doFeature`) — feature spread/growth/disappearance.
- `CvGame.cpp:3277` (`getImprovementUpgradeTime`) — speed/era-scaled timer.
- `CvPlayer.cpp:7773` (`getImprovementUpgradeProgressRate`) — per-player rate modifier.
- `CvHttpServer.cpp:1542-1578` — `/cities` snapshot (has city aggregate yields; NO per-plot data).
- `Sources/Utils/PlotSnapshot.cpp:260` — per-turn CSV (has improvement/feature type; NO yield/progress).
- `docs/dev/reference/PlotSnapshot.md` — PlotSnapshot schema reference.
- `docs/dev/plans/cascade-mapping-inventory.md` §D — the Observability Scale (Tier 0-5).
