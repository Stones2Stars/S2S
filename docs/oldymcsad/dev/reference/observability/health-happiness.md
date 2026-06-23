# Observability — Health & Happiness — what's on the wire for a city's Civ4 ledgers

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites confirmed in `Sources/Engine/CvCity.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (happy/unhappy/good/bad-health ledgers, anger timers, WLTK), `Sources/AI/CvCityAI.cpp` (downstream scoring), `Sources/Tools/CvHttpServer.cpp` (`/cities`, `/players`). Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> The Civ4 `happyLevel`/`unhappyLevel`/`goodHealth`/`badHealth` ledger is **Tier 0** today — completely dark. The C2C property system (crime/disease/education) has Tier 1 coverage via `[CIT/proplevel]` + `/cities`, but net happiness, net health, WeLoveTheKingDay, every anger timer, the revolution index and player-level war weariness are computed live and emitted nowhere. This map walks the two ledgers and their downstream consequences, names what's dark, and proposes the snapshot fields / log tags to climb to Tier 3.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer. (The old draft's numbers were already ~10 lines stale when re-confirmed; the functions are stable.)

---

## 1. How it actually works

### 1a. Net happiness — `happyLevel()` minus `unhappyLevel()`

`CvCity::happyLevel()` and `CvCity::unhappyLevel()` are the two poles of the happiness ledger
(CvCity.cpp:5689 and CvCity.cpp:5606). The net is `happy - unhappy`; a negative net produces
`angryPopulation()` (CvCity.cpp:5730), which has downstream effects on production, growth, and
`isWeLoveTheKingDay`.

**`unhappyLevel(iExtra)` sources** (CvCity.cpp:5606–):

| Source | Accessor | Type |
|---|---|---|
| Overcrowding (always ≥0 for pop>0) | `getOvercrowdingPercentAnger(iExtra)` | percent-anger |
| No military units in city | `getNoMilitaryPercentAnger()` | percent-anger |
| Foreign culture on plot | `getCulturePercentAnger()` — enemy culture × `CULTURE_PERCENT_ANGER` | percent-anger |
| Enemy-state-religion cities at war | `getReligionPercentAnger()` | percent-anger |
| Hurry (rush-buy) anger | `getHurryPercentAnger(iExtra)` — driven by `getHurryAngerTimer()` | percent-anger |
| Conscript anger | `getConscriptPercentAnger(iExtra)` — driven by `getConscriptAngerTimer()` | percent-anger |
| Defy-resolution anger | `getDefyResolutionPercentAnger(iExtra)` — `getDefyResolutionAngerTimer()` | percent-anger |
| War weariness (player-level + city timer) | `getWarWearinessPercentAnger()` — `GET_PLAYER().getWarWearinessPercentAnger()` × `getWarWearinessModifier()` × `(getWarWearinessTimer()+100)/100` | percent-anger |
| Revolution request anger | `getRevRequestPercentAnger(iExtra)` — `getRevRequestAngerTimer()` | percent-anger |
| Revolution index anger | `getRevIndexPercentAnger()` — fires when `getRevolutionIndex() > 325`; scaled by `getLocalRevIndex()` | percent-anger |
| Civic percent-anger | `GET_PLAYER().getCivicPercentAnger(eCivic)` loop | percent-anger |
| *(sum → iAngerPercent × (pop+iExtra) / PERCENT_ANGER_DIVISOR = iUnhappiness base)* | | |
| LargestCity bonus (negative → unhappy) | `getLargestCityHappiness()` if negative | flat |
| Military happiness (negative) | `getMilitaryHappiness()` if negative | flat |
| State religion happiness (negative) | `getCurrentStateReligionHappiness()` | flat |
| Building bad happiness | `getBuildingBadHappiness()` | flat |
| Extra building bad happiness | `getExtraBuildingBadHappiness()` | flat |
| Feature bad happiness | `getFeatureBadHappiness()` | flat |
| Bonus bad happiness | `getBonusBadHappiness()` | flat |
| Religion bad happiness | `getReligionBadHappiness()` | flat |
| Commerce happiness (negative) | `getCommerceHappiness()` | flat |
| Area building happiness (negative) | `area()->getBuildingHappiness(owner)` | flat |
| Player building happiness (negative) | `GET_PLAYER().getBuildingHappiness()` | flat |
| Extra happiness (negative) | `getExtraHappiness() + GET_PLAYER().getExtraHappiness()` | flat |
| Handicap happy bonus (negative) | `GC.getHandicapInfo().getHappyBonus()` | flat |
| Vassal unhappiness | `getVassalUnhappiness()` | flat |
| Espionage happiness counter | `getEspionageHappinessCounter()` (positive = unhappy side) | flat |
| Civic happiness (negative) | `getCivicHappiness()` | flat |
| Specialist unhappiness | `getSpecialistUnhappiness()/100` | flat |
| World happiness (negative) | `GET_PLAYER().getWorldHappiness()` | flat |
| Project happiness (negative) | `GET_PLAYER().getProjectHappiness()` | flat |
| Tax rate unhappiness | `GET_PLAYER().calculateTaxRateUnhappiness()` | flat |
| Corporation happiness (negative) | `calculateCorporationHappiness()` | flat |
| Event anger | `getEventAnger()` (decays by 1 every `10 × speedPercent/100` turns) | flat |
| Extra tech happiness total (negative) | `getExtraTechHappinessTotal()` | flat |
| Foreign unhappy percent (culture-penalized) | `getForeignUnhappyPercent()` | flat |
| Landmark anger (MAP_PERSONALIZED only) | `getLandmarkAnger()` unless `isNoLandmarkAnger()` | flat |
| City-over-limit (civic soft cap) | `getCityOverLimitUnhappy() × overLimitCities` | flat |

Short-circuit bypass: `isNoUnhappiness()` → entire function returns 0. Also `isCapital() && GET_PLAYER().isNoCapitalUnhappiness()`.

**`happyLevel()` sources** (CvCity.cpp:5689–):

| Source | Accessor | Type |
|---|---|---|
| Revolution-success happiness | `getRevSuccessHappiness()` | flat |
| LargestCity bonus (positive) | `getLargestCityHappiness()` — fires if `findPopulationRank() <= worldTargetNumCities` | flat |
| Military happiness (positive) | `getMilitaryHappiness()` | flat |
| State religion happiness (positive) | `getCurrentStateReligionHappiness()` | flat |
| Building good happiness | `getBuildingGoodHappiness()` | flat |
| Extra building good happiness | `getExtraBuildingGoodHappiness()` | flat |
| Feature good happiness | `getFeatureGoodHappiness()` | flat |
| Bonus good happiness | `getBonusGoodHappiness()` | flat |
| Religion good happiness | `getReligionGoodHappiness()` | flat |
| Commerce happiness (positive) | `getCommerceHappiness()` | flat |
| Area building happiness (positive) | `area()->getBuildingHappiness(owner)` | flat |
| Player building happiness (positive) | `GET_PLAYER().getBuildingHappiness()` | flat |
| Extra happiness (positive) | `getExtraHappiness() + GET_PLAYER().getExtraHappiness()` | flat |
| Handicap happy bonus (positive) | `GC.getHandicapInfo().getHappyBonus()` | flat |
| Vassal happiness | `getVassalHappiness()` | flat |
| Civic happiness (positive) | `getCivicHappiness()` | flat |
| Specialist happiness | `getSpecialistHappiness()/100` | flat |
| World happiness (positive) | `GET_PLAYER().getWorldHappiness()` | flat |
| Project happiness (positive) | `GET_PLAYER().getProjectHappiness()` | flat |
| Corporation happiness (positive) | `calculateCorporationHappiness()` | flat |
| Celebrity happiness | `getCelebrityHappiness()` (celebrity units on the city plot) | flat |
| Extra tech happiness total (positive) | `getExtraTechHappinessTotal()` | flat |
| Landmark happiness (MAP_PERSONALIZED) | `GET_PLAYER().getLandmarkHappiness()` | flat |
| Temp happy (happiness timer) | `getHappinessTimer() > 0 → GC.getTEMP_HAPPY()` | temporary |

### 1b. Anger timers — per-turn countdown, set by game events

Seven per-city integer counters that decay automatically each turn in `CvCity::doTurn`
(CvCity.cpp:1374–1417). All are `-1` per turn; none "recharge" — set at event time and burn down:

| Timer | Set by | Anger contribution |
|---|---|---|
| `getHurryAngerTimer()` | Hurry (rush-buy) a unit/building | `getHurryPercentAnger()` |
| `getConscriptAngerTimer()` | Drafting/conscripting a unit | `getConscriptPercentAnger()` |
| `getDefyResolutionAngerTimer()` | Defying a UN resolution | `getDefyResolutionPercentAnger()` |
| `getHappinessTimer()` | Celebrations / certain buildings / events | `GC.getTEMP_HAPPY()` bonus to happy side |
| `getRevRequestAngerTimer()` | Revolution request events | `getRevRequestPercentAnger()` |
| `getRevSuccessTimer()` | Successful revolution | `getRevSuccessHappiness()` |
| `getLandmarkAngerTimer()` | Landmark events (MAP_PERSONALIZED) | `getLandmarkAnger()` |

**War weariness city timer** (`getWarWearinessTimer()`, CvCity.cpp:21723): decays by 20/turn; set from
combat results via `CvPlayer.cpp:16482`. Multiplies the player-level `getWarWearinessPercentAnger()`.

**Event anger** (`getEventAnger()`): decays by 1 every `10 × speedPercent/100` turns (checked at
`doWarWeariness`).

**Espionage counters** (`getEspionageHappinessCounter()`, `getEspionageHealthCounter()`): decay by 1 per
turn (CvCity.cpp:1414–1417); set by espionage missions.

### 1c. Revolution index — `getRevolutionIndex()` / `getLocalRevIndex()`

`getRevolutionIndex()` (CvCity.cpp:951) — a per-city accumulator; contributes to
`getRevIndexPercentAnger()` only when `> 325`. `getLocalRevIndex()` (CvCity.cpp:969) scales the
contribution. Both are updated by events and the doTurn revolution system; full details deferred to a
future revolution-system observability map.

### 1d. Health — `goodHealth()` minus `badHealth()`

**`goodHealth()` sources** (CvCity.cpp:5831–):

| Source | Accessor |
|---|---|
| Fresh water | `getFreshWaterGoodHealth()` |
| Feature good health | `getFeatureGoodHealth()` |
| Bonus good health | `getBonusGoodHealth()` |
| Building good health (city + area + player + extra) | `totalGoodBuildingHealth()` |
| Extra health (positive) | `getExtraHealth()` |
| Handicap health bonus (positive) | `GC.getHandicapInfo().getHealthBonus()` |
| Improvement good health | `getImprovementGoodHealth()/100` |
| Specialist good health | `getSpecialistGoodHealth()/100` |
| Corporation health (positive) | `calculateCorporationHealth()` |
| Extra tech health total (positive) | `getExtraTechHealthTotal()` |
| Player extra health (positive) | `owner.getExtraHealth()` (incl. civic health, civ health) |
| Player civic health (positive) | `owner.getCivicHealth()` |
| Player civilization health (positive) | `owner.getCivilizationHealth()` |
| World health (positive) | `owner.getWorldHealth()` |
| Project health (positive) | `owner.getProjectHealth()` |

**`badHealth()` sources** (CvCity.cpp:5858–):

| Source | Accessor |
|---|---|
| Espionage health counter (malus) | `-getEspionageHealthCounter()` |
| Feature bad health | `getFeatureBadHealth()` |
| Bonus bad health | `getBonusBadHealth()` |
| Building bad health (city + area + player + extra) | `totalBadBuildingHealth()` |
| Extra health (negative) | `getExtraHealth()` if negative |
| Handicap health bonus (negative) | `GC.getHandicapInfo().getHealthBonus()` if negative |
| Extra building bad health | `getExtraBuildingBadHealth()` |
| Improvement bad health | `getImprovementBadHealth()/100` |
| Specialist bad health | `getSpecialistBadHealth()/100` |
| Corporation health (negative) | `calculateCorporationHealth()` if negative |
| Extra tech health total (negative) | `getExtraTechHealthTotal()` if negative |
| Player extra health (negative) | `owner.getExtraHealth()` if negative |
| Player civic/civ/world/project health (negative) | same accessors as goodHealth |
| **Population itself** | `unhealthyPopulation(bNoAngry, iExtra)` — `max(0, pop - angryPop)` unless `isNoUnhealthyPopulation()` |

Short-circuit bypass: `isBuildingOnlyHealthy()` → `totalBadBuildingHealth()` returns 0.
`isNoUnhealthyPopulation()` → unhealthy population term = 0.

`healthRate()` = `min(0, goodHealth() - badHealth())` (CvCity.cpp:5886) — always ≤ 0; a sick city
returns a negative value.

### 1e. Downstream consequences

**Food starvation via healthRate** (CvCity.cpp:5922–):
`foodConsumption() = getFoodConsumedByPopulation() - angryPopulation() - healthRate()` — negative
`healthRate` is subtracted, so sick cities consume *more* food per turn, reducing `foodDifference()` →
faster starvation or negative food-stores → population loss via the `changeFood()` loop. The food side of
this is mapped in [`food-yields-wastage.md`](food-yields-wastage.md).

**Growth suppression**: when `angryPopulation(1) > 0`, `AI_avoidGrowth()` triggers (CvCityAI.cpp), pinning
food at the threshold.

**WeLoveTheKingDay** (CvCity.cpp:1419–1430): set `false` if `isOccupation() || angryPopulation()>0 ||
healthRate()<0`; set `true` stochastically (pop-weighted rand < `WE_LOVE_THE_KING_RAND`) otherwise. WLTK
waives distance+numCities maintenance and doubles GPP generation. So both health and happiness feed this
crucial economic gate.

**Disorder** (`isDisorder()` = `isOccupation() || GET_PLAYER().isAnarchy()`): `foodDifference()` returns 0,
production idles, corporations and doProduction stall.

**Production choice** (CvCityAI): `happyLevel() - unhappyLevel()` and `goodHealth() - badHealth()` both
feed directly into building value scoring (CvCityAI.cpp:654, 698, 884, 885, 5077, 5079, etc.) and the
hurry threshold (CvCityAI.cpp:10695).

---

## 2. What's on the wire today — **Tier 0 (health/happiness ledger)**

### What is exposed

| Surface | Fields | Notes |
|---|---|---|
| `GET /cities` | `population`, `food` (YIELD_FOOD rate), `production`, `commerce`, `crime`, `education`, `disease` | CvHttpServer.cpp (city walk) |
| `GET /players` | `score`, `era`, `techs`, `gold`, `goldRate`, `scienceRate`, `population`, `units`, `cities` | CvHttpServer.cpp (player walk) |
| `[CIT/proplevel]` (CityAI.log, level 1) | per-turn per-city snapshot: `prop=` `val=` `change=` for every active property | CvCity.cpp:1244 |
| `[CIT/begin]` (CityAI.log, level 1) | `pop=` `danger=` `finTrouble=` `critGold=` `foodProd=` | CvCityAI.cpp:966 |

### What is NOT exposed

Everything else:

- **Net happiness** (`happyLevel()`, `unhappyLevel()`, `angryPopulation()`) — zero endpoints, zero log lines.
- **Net health** (`goodHealth()`, `badHealth()`, `healthRate()`) — zero endpoints, zero log lines.
- **WeLoveTheKingDay** (`isWeLoveTheKingDay()`) — not in any snapshot or log.
- **All anger timers** (`hurryAngerTimer`, `conscriptAngerTimer`, `defyResolutionAngerTimer`,
  `happinessTimer`, `warWearinessTimer`, `revRequestAngerTimer`, `revSuccessTimer`, `eventAnger`,
  `espionageHappinessCounter`, `espionageHealthCounter`) — none visible.
- **Revolution index** (`getRevolutionIndex()`, `getLocalRevIndex()`) — not exposed.
- **War weariness at player level** (`getWarWearinessPercentAnger()`) — not in `/players`.
- **Per-source breakdown** of any happiness or health contributor — nowhere.
- **Disorder flag** (`isDisorder()`, `isOccupation()`, `isAnarchy()`) — not in any snapshot.
- **Bypass flags** (`isNoUnhappiness()`, `isNoUnhealthyPopulation()`) — not exposed.
- **Food starvation signal**: `/cities.food` = `YIELD_FOOD` rate (gross), NOT net `foodDifference()`.

### Tier assessment

| Tier | Name | Status |
|---|---|---|
| 0 | Oblivious | baseline — no health/happiness on wire at all |
| **1** | **Telescreen** | `/cities` gives pop + food RATE (not net); `[CIT/proplevel]` gives property values (disease, crime) — but those are the C2C property system, not the Civ4 health/happiness ledger |

**Current rating: Tier 0 for health & happiness specifically.** The property system (disease/crime/education)
has Tier 1 coverage via `[CIT/proplevel]` + the `/cities` `crime`/`education`/`disease` fields, but the
Civ4 `goodHealth/badHealth/happyLevel/unhappyLevel` ledger is completely dark.

---

## 3. The gap

An agent watching the HTTP endpoints + logs TODAY cannot:

1. Know whether a city is in anger (angry population > 0) or sick (healthRate < 0).
2. Know whether WeLoveTheKingDay is active (waives maintenance, doubles GPP).
3. Know why a city is angry — which source dominates (overcrowding? hurry? war weariness?).
4. Know the magnitude of any anger timer — can't predict when anger will abate.
5. Know whether food starvation is occurring (needs `foodDifference()` < 0, not just the yield rate).
6. Know player-level war weariness (`getWarWearinessPercentAnger()`), which multiplies into every city's
   war-weariness anger.
7. Know when disorder is active (production and food zeroed, corporations stalled).
8. Know which happiness/health sources are positive vs negative — can't attribute a happy/sick city.
9. Know whether bypass flags are active (`isNoUnhappiness`, `isNoUnhealthyPopulation`, `isBuildingOnlyHealthy`).

**Cascade-verification consequence:** the `requires.operate` targets for happiness- and health-gated
buildings (property-band buildings from `checkPropertyBuildings`, religion-dormancy triggers) cannot be
verified against ground truth without this layer. The `autoBuild` placement shadow likewise cannot explain
WHY a property-band building was added/removed without seeing `goodHealth`/`badHealth`.

---

## 4. Proposed hooks — climbing from Tier 0 to Tier 3

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4a. `/cities` snapshot additions (cheapest — same publish path)

Add to `CitySnap` struct and the `publishIfDue` city-walk:

```
happyLevel        int   happyLevel()
unhappyLevel      int   unhappyLevel(0)
angryPop          int   angryPopulation()
goodHealth        int   goodHealth()
badHealth         int   badHealth(false, 0)
healthRate        int   healthRate()
foodDifference    int   foodDifference()           — net food change this turn (negative = starving)
weLoveKingDay     bool  isWeLoveTheKingDay()
isDisorder        bool  isDisorder()
```

All cheap single-call reads (most already computed once per turn for AI scoring). They make the most
critical derived state readable at Tier 1 for ALL players (AI included).

### 4b. `/players` snapshot additions

Add to `PlayerSnap` struct and its publish walk:

```
warWearinessAnger   int   getWarWearinessPercentAnger()   — the player-level % anger fed to every city
noCapitalUnhappy    bool  isNoCapitalUnhappiness()
noUnhealthyPop      bool  isNoUnhealthyPopulation()
extraHappiness      int   getExtraHappiness()
extraHealth         int   getExtraHealth()
buildingHappiness   int   getBuildingHappiness()
buildingGoodHealth  int   getBuildingGoodHealth()
buildingBadHealth   int   getBuildingBadHealth()
worldHappiness      int   getWorldHappiness()
worldHealth         int   getWorldHealth()
```

The player-level aggregates; combined with the per-city fields above, an agent can attribute city
happiness/health to player-level vs city-level sources.

### 4c. `[CIT/happy]` log tag — per-turn per-city headline (Tier 3)

A new level-1 tag in `CvCity::doTurn` (alongside `[CIT/proplevel]`), emitting the net figures and any
**changed** anger timers:

```
[CIT/happy] turn=N city=X owner=P happy=H unhappy=U angry=A health=G bad=B rate=R wltk=0|1 disorder=0|1
```

- Gated at `gCityLogLevel >= 1`.
- Emitted ONCE per city per turn (before production choice, so the state is current).
- Streamed to `/events` as a `log` frame (same `streamLogTee` path as `[CIT/proplevel]`).

Optional level-2 expansion: `[CIT/happy/timers]` emitting the non-zero anger timers (hurry, conscript,
warWeariness, etc.). Only emit when any timer > 0 (keeps the log quiet).

### 4d. `[CIT/angry]` events for timer-set moments (Tier 3 delta)

At the event sites that SET anger timers (hurry, conscript, defy-resolution, war-weariness), emit a
level-1 `[CIT/angry]` line: `city= owner= cause=hurry|conscript|warWeariness timer=N`. This gives the
triggering event as it happens (live in `/events`), not just the current value.

Key sites:
- Hurry timer set: `CvCity::changeHurryAngerTimer`.
- Conscript timer set: `CvCity::changeConscriptAngerTimer`.
- War weariness timer injection: `CvPlayer.cpp:16482`.

### 4e. `[CIT/wltk]` on WeLoveTheKingDay change

At `CvCity::setWeLoveTheKingDay`, emit a level-1 line when the value **changes**:
`[CIT/wltk] city= owner= wltk=0|1`. WLTK fires/clears every turn — the toggle itself is the signal (WLTK
clear = happiness or health problem started; WLTK set = resolved).

### 4f. No new diagnostic gate-eval needed yet

Unlike buildability (where cascade vs legacy divergence demands a diagnostic endpoint), health and
happiness are read-only aggregates without a cascade equivalent yet. The snapshot fields (§4a/§4b) and log
tags (§4c–4e) are sufficient for Tier 3. A `/diagnostic/cityHappiness?id=N` gate-eval can be added later
when the cascade starts representing happiness sources as `requires`/`enables` atoms — at that point a
per-source breakdown endpoint becomes the Tier-4/5 verification tool.

---

## 5. Priority order

| Hook | Tier gain | Effort | Rationale |
|---|---|---|---|
| `/cities` snapshot fields (§4a) | 0→1 for health/happiness | ~20 lines in CvHttpServer.cpp | Unblocks AI-player observability immediately; no log volume |
| `/players` snapshot fields (§4b) | 1→2 (attributable) | ~15 lines | War weariness + player-level sources visible |
| `[CIT/happy]` per-turn log tag (§4c) | 2→3 (wiretap) | ~10 lines in CvCity.cpp | Live stream of net happy/health + WLTK each turn |
| `[CIT/wltk]` change event (§4e) | 3 | ~5 lines | Pin-points when disorder starts/ends |
| `[CIT/angry]` timer-set events (§4d) | 3 | ~10 lines at 3 sites | Attributable causation for anger spikes |

All five are `gCityLogLevel`-gated and off by default — zero cost at `gCityLogLevel=0`.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine classes → `Sources/Engine/`,
> `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `unhappyLevel` | `Sources/Engine/CvCity.cpp:5606` |
| `happyLevel` | `Sources/Engine/CvCity.cpp:5689` |
| `angryPopulation` | `Sources/Engine/CvCity.cpp:5730` |
| `goodHealth` | `Sources/Engine/CvCity.cpp:5831` |
| `badHealth` | `Sources/Engine/CvCity.cpp:5858` |
| `healthRate` | `Sources/Engine/CvCity.cpp:5886` |
| `foodConsumption` (health feeds food via healthRate) | `Sources/Engine/CvCity.cpp:5922` |
| `changeFood` (starvation pop-loss loop) | `Sources/Engine/CvCity.cpp:9723` |
| anger timer countdown + WLTK gate | `Sources/Engine/CvCity.cpp:1374–1430` |
| `doWarWeariness` (warWearinessTimer/eventAnger decay) | `Sources/Engine/CvCity.cpp:21747` |
| war-weariness timer injection | `Sources/Engine/CvPlayer.cpp:16482` |
| `CitySnap` struct / city publish walk (add fields here) | `Sources/Tools/CvHttpServer.cpp` |
| production-choice scoring (happy/health feed) | `Sources/AI/CvCityAI.cpp:654, 698, 884, 5077, 10695` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/players`, `/events`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`food-yields-wastage.md`](food-yields-wastage.md) — the food balance that `healthRate()` feeds:
  sick cities (negative `healthRate`) consume more food and starve faster.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing for map-before-delete (the `requires.operate` verification this gap blocks).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
