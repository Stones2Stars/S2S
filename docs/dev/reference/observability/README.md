# Observability Net — Index & Gap Analysis

> **Purpose:** this file is the master index for the 22 per-system observability maps that live in this
> directory. It records where the game sits on the Observability Scale, prioritises the cross-system
> gap work, and flags maps that need a re-check. Update it whenever a map is written or revised.
>
> **Observability Scale:** 0 Oblivious · 1 Telescreen · 2 Informant · 3 Big Brother · 4 Thought Police · 5 Meta
>
> **Reconstruction bar:** be able to reconstruct full game state from HTTP endpoints + `/events` SSE stream +
> gated log files alone — never from the screen.

---

## 1. State of the Net

Twenty-two systems have been mapped. The headline finding is bleak but concrete: **the game is sitting
overwhelmingly at Tier 1 (Telescreen), with two systems at Tier 0 (Oblivious).** No system has yet
reached Tier 2. The HTTP snapshot endpoints (`/players`, `/cities`, `/units`) expose only coarse
aggregate rates — gross food yield, gross commerce yield, composite score, population count, era integer —
with virtually no component breakdowns, no per-turn deltas, no accumulated-state fields (timers, bars,
counters, ledgers), and no real-time events for the majority of state-changing transitions. The `/events`
SSE stream is largely empty of domain signals: it carries build completions (`[CIT/produced]`),
production-overflow-to-gold (`[CIT/waste]`), the `[UNT/act]` push for unit missions, and the
`[WAR/warplan]` transition — and nothing else relevant to the 22 systems mapped here. The biggest
structural blind spots are: (a) the entire *expense side* of the economy (maintenance, upkeep,
inflation, strike, financial-trouble AI gate) is invisible even though the /players goldRate exposes the
net result; (b) every *accumulation bar* that drives city output and AI decisions — food box, GPP bank,
XP/promotion state, culture accumulator, research progress fraction — is absent from every snapshot; (c)
two systems, **Health/Happiness** and **Espionage**, are full Tier 0: not one field of their respective
ledgers reaches any endpoint or log; (d) the AI diplomacy attitude value, which routes every AI
war/trade/contact decision, is computed, cached, and completely unexported; (e) **Victory Progress** has
no endpoint for any condition's countdown state, meaning an agent running AI-only autoplay cannot even
detect when the game has ended. Addressing the `/cities` snapshot gaps and adding a handful of gated log
domains would lift most systems to Tier 2 in a single pass.

---

## 2. System Map

| System | Tier | Map | One-line gap | Top proposed hook |
|---|---|---|---|---|
| Food, yields & wastage | 1 | [food-yields-wastage.md](food-yields-wastage.md) | foodDifference, food bar, wastage, granary all invisible | `/cities` snapshot: add `foodNet`, `foodStored`, `foodThreshold`, `foodWaste` |
| Health & Happiness | **0** | [health-happiness.md](health-happiness.md) | Not one happiness/health field in any endpoint | `/cities` snapshot: add `happyLevel`, `unhappyLevel`, `goodHealth`, `badHealth`, `healthRate` |
| Culture: accrual, borders, flips | 1 | [culture.md](culture.md) | `cultureTimes100`, revolt count, plot-flip events all absent | `/cities` snapshot: add `cultureTimes100`, `cultureRate`, `cultureThreshold`, `occupationTimer` |
| Religion spread | 1 | [religion-spread.md](religion-spread.md) | City religion set, influence weights, spread rolls entirely dark | `/cities` snapshot: add `religions[]` + `influence{}` arrays |
| Espionage economy & missions | **0** | [espionage.md](espionage.md) | EP balance, spending weights, mission outcomes all invisible | `/players` snapshot: add `espionageRate`, `espionageAgainst[]`, `espionageWeights[]` |
| Corporations | 0* | [corporations.md](corporations.md) | Corp presence, HQ, spread/decay events entirely opaque | `/cities` snapshot: add `corporations[]` array; `/players` add `hqCorps[]` |
| Trade routes & connectivity | 1 | [trade-routes.md](trade-routes.md) | Route topology, per-city `hasBonus` set (cascade dormancy oracle) invisible | `/cities` snapshot: add `connectedBonuses[]`, `tradeRoutes`, `tradeYield` |
| Gold, maintenance & inflation | 1 | [gold-maintenance-inflation.md](gold-maintenance-inflation.md) | Entire expense side (maintenance, upkeep, inflation, strike) invisible | `/players` snapshot: add `totalMaintenance`, `civicUpkeep`, `unitUpkeep`, `inflationMod`, `isStrike` |
| Research, beakers & tech diffusion | 1 | [research-tech-diffusion.md](research-tech-diffusion.md) | Research progress fraction, actual deposit, diffusion modifier all absent | `/players` snapshot: add `researchProgress`, `researchCost`, `overflowResearch` |
| Great people generation | 1 | [great-people.md](great-people.md) | Per-city GPP bank, rate, threshold, spawn event all invisible | `/cities` snapshot: add `gppProgress`, `gppRate`, `gppThreshold` |
| War weariness | 1 | [war-weariness.md](war-weariness.md) | WW matrix, per-player anger scalar, city WW modifier/timer all absent | `/players` snapshot: add `iWarWearinessPercentAnger`, `iWarWearinessModifier` |
| Civics: effects, anarchy, revolution | 1 | [civics.md](civics.md) | Active civic set, anarchy state, revolution timer all absent from `/players` | `/players` snapshot: add `civics{}`, `anarchy`, `revTimer`, `civicUpkeep` |
| Unit promotions & XP | 1 | [promotions-xp.md](promotions-xp.md) | Unit XP, promotion set, every XP-gain event all invisible | `/units` snapshot: add `xp`, `xpNeeded`, `promotionReady`; `[UNT/xp]` log tag |
| Global warming & pollution | 1 | [global-warming-pollution.md](global-warming-pollution.md) | City air/water pollution absent from `/cities`; plot diffusion plume entirely dark | `/cities` snapshot: add `airPollution`, `waterPollution` |
| Improvements & plot yields | 1 | [improvements-plot-yields.md](improvements-plot-yields.md) | Upgrade progress, yield breakdown, feature events absent from all endpoints | New `/plots` HTTP endpoint with per-plot improvement/upgrade/yield state |
| City growth, production, overflow & hurry | 1 | [city-growth-production.md](city-growth-production.md) | Food box, growth threshold, overflow bucket, hurry side effects all absent | `/cities` snapshot: add `foodBox`, `foodThreshold`, `foodNet`, `overflowProduction` |
| Diplomacy & attitude | 1 | [diplomacy-attitude.md](diplomacy-attitude.md) | AI attitude value, all 12 time counters, 44-type memory ledger entirely dark | `[DIP/attitude]` per-turn per-pair log; `/diagnostic/attitude?player=A&vs=B` |
| Victory conditions progress | 1 | [victory-progress.md](victory-progress.md) | Countdown timers, winner/game-state, mercy-rule counter all absent | `/game` endpoint with `gameState`, `winner`, `victoryType`; `[VIC/winner]` SSE event |
| Heritage & score | 1 | [heritage-score.md](heritage-score.md) | Heritage set invisible; score breakdown (pop/land/tech/wonders) absent | `/players` snapshot: add `heritages[]`, `popScore`, `landScore`, `techScore`, `wondersScore` |
| Golden ages & era advance | 1 | [golden-ages-era.md](golden-ages-era.md) | Golden-age turns remaining, unit-GA count, trigger path all absent | `/players` snapshot: add `goldenAgeTurns`, `numUnitGoldenAges`; SSE `goldenAgeStart` event |
| Vision & visibility | 1 | [vision-visibility.md](vision-visibility.md) | Fog-of-war counts, invisible-unit flag, reveal state all absent | `/units` snapshot: add `invisible`, `sightRange`; `/diagnostic/visibilityQuery` |
| Unit upkeep, supply & food-for-units | 1 | [unit-upkeep-supply.md](unit-upkeep-supply.md) | Per-unit upkeep, civilian/military split, AI disband loop all invisible | `/units` snapshot: add `iUpkeep100`, `iMilitary`; `/players` add upkeep breakdown fields |

> \* Corporations is filed at Tier 0 in its map. It is marked 0* in this table because the system
> exists in S2S and fires at runtime — the Tier 0 assessment is accurate and distinct from Global
> Warming whose mechanic is compiled out entirely.

---

## 3. Prioritised Cross-System Gap List

Ordered by how much each item unblocks the *reconstruct-from-API* bar and the §14 hard switch.
Items that are blockers for cascade shadow verification or AI-only autoplay are called out explicitly.

### P0 — Game-ended signal (blocks all autoplay verification)

**Victory Progress: add a `/game` endpoint with `gameState` / `winner` / `victoryType`.**
An agent running AI-only autoplay cannot currently detect when the game has ended. The
`turnEnd`/`turnStart` SSE stream stopping is the only signal — fragile and racey. A single
`/game` snapshot with three scalar fields from `CvGame` removes this blocker entirely. Add
`victoryCountdown` SSE event at the same time (Hook E in victory-progress.md).

### P1 — `/cities` accumulation bars (blocks cascade shadow + §14 H deletion)

**Health/Happiness, Food/Growth, GPP, Culture, Research progress — add missing fields to
`/cities` and `/players` in one pass.**
These are the most-referenced missing fields across the 22 maps, they are all cheap scalar
reads at the existing 5-second publish rate, and they unblock cascade-shadow verification
for the largest set of §14 H property-band maintainer targets. Concrete field list (minimum
viable):
- `/cities`: `happyLevel`, `unhappyLevel`, `goodHealth`, `badHealth`, `healthRate`,
  `foodNet` (= `foodDifference()`), `foodStored`, `foodThreshold`, `foodWaste`,
  `gppProgress`, `gppRate`, `gppThreshold`, `cultureTimes100`, `cultureRate`,
  `cultureThreshold`, `occupationTimer`, `airPollution`, `waterPollution`,
  `overflowProduction`, `productionProgress`, `productionNeed`, `disorder`
- `/players`: `researchProgress`, `researchCost`, `overflowResearch`,
  `goldenAgeTurns`, `numUnitGoldenAges`, `anarchy`, `revTimer`, `civicUpkeep`,
  `civics{}`, `warWearinessPercentAnger`, `heritages[]`, `popScore`, `techScore`

This is a single `CvHttpServer.cpp` pass across `CitySnap`, `PlayerSnap`, and their render
functions. No new infrastructure, no new logging domains, no game-thread mailboxes needed.

### P2 — Expense-side economy (blocks financial-AI verification)

**Gold/Maintenance + Unit Upkeep: surface the component breakdown in `/players`.**
`goldRate` is useless for AI verification when it is an opaque net of 7+ invisible terms.
`AI_isFinancialTrouble()` gates at least 15 call-sites in CityAI/PlayerAI/UnitAI; it must
be in the snapshot for any financial-behaviour shadow. Required fields:
`totalMaintenance`, `civicUpkeep`, `finalUnitUpkeep`, `unitSupply`, `inflationMod`,
`isStrike`, `strikeTurns`, `isFinancialTrouble` (AI-only), `fundingHealth` (AI-only).
Companion: `/units` needs `iUpkeep100` and `iMilitary` to attribute cost per unit.

### P3 — Diplomacy attitude (blocks AI war/trade shadow)

**`[DIP/attitude]` per-turn per-pair gated log + `/diagnostic/attitude?player=A&vs=B`.**
`AI_getAttitudeVal(A,B)` drives every AI war, trade, contact, and gift decision. It is
never exported. Adding a per-pair gated log line (level 1, one line per known player pair
per turn) and an on-demand diagnostic endpoint unlocks the entire diplomacy shadow in one
step. The 44-type memory ledger and 12 time counters can follow at level 2/3.

### P4 — Trade routes / bonus connectivity (blocks cascade resource-dormancy verification)

**`/cities` connected-bonuses set + `connectedToCapital`.**
`CvCity::hasBonus()` (the cascade's B-ii dormancy oracle — does this city have resource X
connected via its plot-group?) is used in every cascade `requires.operate` resource gate.
Without it the cascade shadow cannot verify dormancy transitions. Adding `connectedBonuses[]`
as a query-param-gated array on `/cities` (to avoid snapshot bloat) is the minimum viable
fix. `isConnectedToCapital()` should accompany it as it gates the trade-route modifier and
a connected-city maintenance discount.

### P5 — Espionage economy (blocks espionage + cascade counter-espionage shadow)

**`/players` espionage fields + `[ESP/mission]` log tag.**
Both Tier 0 gaps (Espionage, and the espionage contributions to Health/Happiness timers) are
closed by adding `espionageRate`, `espionageAgainst[]` (per-team EP balance), and
`espionageWeights[]` to `/players`, and emitting a `[ESP/mission]` line from
`doEspionageMission()` with the XML mission key, cost, outcome, and pre/post EP balance.
Without at least the mission log, any espionage effect on city health/happiness/power is
causally invisible.

### P6 — Religion spread visibility (blocks cascade religion-prereq shadow)

**`/cities` religion set + `/players` stateReligion.**
The `/diagnostic/canConstruct` endpoint already returns `prereqReligion` / `stateReligionInCity`
as a legacy-reason string when a building is blocked, but the underlying city religion set is
absent. The cascade's `requires.build` religion conditions cannot be shadow-verified without
knowing which religions each city holds. Adding `religions[]` to `/cities` and `stateReligion`
to `/players` is a snapshot-only change.

### P7 — `[CIT/food]` and `[CIT/happy]` gated log domains (stream signal for doTurn)

**Add `[CIT/food]` and `[CIT/happy]` level-1 log tags in `CvCity::doTurn`.**
Once the snapshot fields (P1) exist, these two per-city per-turn log lines complete the
picture by streaming deltas to `/events` in the turn they happen rather than waiting for the
next 5-second snapshot. Both are single-line additions gated by `gCityLogLevel>=1`.

### P8 — Unit XP/promotion visibility (blocks cascade `requires.operate` promotion shadow)

**`/units` snapshot: add `xp`, `xpNeeded`, `promotionReady`; `[UNT/xp]` and `[UNT/promo]`
log tags.** The cascade's `requires.operate` checks may gate on promotion state. Currently
neither XP nor any promotion is in `/units`. The snapshot fields are trivial additions;
the log tags from `setExperience100` and `setHasPromotion` make every XP gain and every
AI auto-promotion (currently completely silent) visible on `/events`.

### P9 — Victory countdown SSE events (closes autoplay-completion race)

**SSE `victoryCountdown` and `winner` events from `testVictory()` and `setWinner()`.**
Once the `/game` endpoint (P0) exists, adding `publishEvent()` calls at `setWinner()` and
at countdown start/tick makes the game-ended signal immediate rather than poll-dependent.
This also enables wall-clock timing of condition countdowns in post-session analysis.

### P10 — `/diagnostic/attitude`, `/diagnostic/financeBreakdown`, `/diagnostic/revoltRisk` mailboxes

**On-demand mailbox endpoints for the three highest-complexity subsystems.**
The game-thread mailbox pattern (same as the existing `/diagnostic/canConstruct` and
`/diagnostic/placementSweep`) lets an external agent request a full decomposition of
attitude, finance, or revolt risk without adding snapshot overhead. These are the forensic
workbenches for shadow-discrepancy diagnosis once the stream-level coverage (P1-P8) exists.

---

## 4. Maps Needing Re-check / Draft-Quality Flags

The following maps have lower confidence and should be verified against live source before
implementing their hooks. Reasons are noted.

**Corporations (`corporations.md`) — medium confidence.**
The map notes `doCorporation()` at `CvCity.cpp:21548/21577` and `doFoundCorporation()` at
`CvGame.cpp:11258`. The exact line numbers and call-site shapes should be confirmed against
the live source before adding the `[CORP/spread]` and `[CORP/found]` log hooks. The C2C
corporation system has accumulated substantial extensions beyond BTS; the map was written
from first-principles inference without tracing every extension path.

**Global Warming & Pollution (`global-warming-pollution.md`) — high confidence on the
compiled-out fact; medium confidence on pollution band thresholds.**
The `#define GLOBAL_WARMING` commented-out status is verified. The 24 effect-building band
thresholds (CIV4PropertyInfos.xml:607-822) were identified by reference and should be
counted precisely before writing the `requires.operate` band atoms (Hook E). The claim that
all 24 report `reason=noMarker` in `placementSweep` has not been run against a live game.

**Improvements & Plot Yields (`improvements-plot-yields.md`) — medium confidence.**
The `/plots` endpoint hook (Hook A) is the highest-complexity proposed addition in the entire
net — a per-plot HTTP endpoint is significantly more work than a snapshot field, and the map
does not address the cost model for the full-map case. The `?all=1` flag should be treated as
a later phase; the `?player=N` scoped version should be scoped to improved/featured plots only
in the first pass. Verify `CvPlot::doImprovementUpgrade` call structure before the `[PLT/upgrade]`
hook — the multi-upgrade candidate branch (CvPlot.cpp:964-1006) is described without line-number
verification.

**Diplomacy & Attitude (`diplomacy-attitude.md`) — high gap confidence, medium hook confidence.**
The map correctly identifies `AI_getAttitudeVal` as the master variable. The proposed
`[DIP/attitude]` hook placement "in AI_doDiploCounters after counter updates" assumes that
function is called every turn for every player pair — confirm this is true for all meeting
states (met/vassal/etc.) before wiring the hook, as attitude is not always freshly computed
every turn for every pair.

**Vision & Visibility (`vision-visibility.md`) — medium confidence on the intensity ledger.**
The `m_aPlotTeamVisibilityIntensity` ledger under `GAMEOPTION_COMBAT_HIDE_SEEK` is described
at a level of abstraction that suggests it was not directly traced in the source. Whether this
ledger actually drives detection probabilities (vs. being an input to a separate gate) should
be confirmed before the `/diagnostic/visibilityQuery` hook references it.

**Unit Upkeep, Supply & Food-for-Units (`unit-upkeep-supply.md`) — medium confidence on
free-allowance field names.**
The field names `getFreeUnitUpkeepCivilian` / `getFreeUnitUpkeepMilitary` should be verified
against the live `CvPlayer.h` — C2C has accumulated several upkeep extensions and the exact
getter names may differ. The `calculateUnitSupply()` call-site location in the snapshot
builder should be confirmed before adding it to `/players`.

**Heritage & Score (`heritage-score.md`) — medium confidence on score component getters.**
`CvPlayer::getPopScore()`, `getLandScore()`, `getTechScore()`, `getWondersScore()` are assumed
to exist as public getters on `CvPlayer`. Confirm against `CvPlayer.h` — these may be
computed inside `CvGame::updateScore` without being separately accessible on `CvPlayer`.

---

*Index last updated: 2026-06-18. Maps written by automated observability survey pass; all
hook proposals are preliminary — verify call-site line numbers against live source before
implementing.*
