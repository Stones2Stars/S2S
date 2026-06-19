# Observability map — Health & Happiness (S2S cities)

> DRAFT observability map (2026-06-18 by parent cascade-mapping-inventory sweep) — every claim cited
> from live code; verify before relying. File: `docs/dev/reference/observability/health-happiness.md`.
> Scale reference: `docs/dev/plans/cascade-mapping-inventory.md` §D (0 Oblivious … 5 Thought Police).

---

## 1. How it actually works

### 1a. Net happiness — `happyLevel()` minus `unhappyLevel()`

`CvCity::happyLevel()` and `CvCity::unhappyLevel()` are the two poles of the happiness ledger
(CvCity.cpp:5679 and CvCity.cpp:5596). The net is `happy - unhappy`; a negative net produces
`angryPopulation()` (CvCity.cpp:5720), which has downstream effects on production, growth, and
`isWeLoveTheKingDay`.

**`unhappyLevel(iExtra)` sources** (CvCity.cpp:5596–5676):

| Source | Accessor | Type |
|---|---|---|
| Overcrowding (always ≥0 for pop>0) | `getOvercrowdingPercentAnger(iExtra)` (CvCity.cpp:5341) | percent-anger |
| No military units in city | `getNoMilitaryPercentAnger()` (CvCity.cpp:5351) | percent-anger |
| Foreign culture on plot | `getCulturePercentAnger()` (CvCity.cpp:5361) — enemy culture × `CULTURE_PERCENT_ANGER` | percent-anger |
| Enemy-state-religion cities at war | `getReligionPercentAnger()` (CvCity.cpp:5402) | percent-anger |
| Hurry (rush-buy) anger | `getHurryPercentAnger(iExtra)` (CvCity.cpp:5438) — driven by `getHurryAngerTimer()` | percent-anger |
| Conscript anger | `getConscriptPercentAnger(iExtra)` (CvCity.cpp:5448) — driven by `getConscriptAngerTimer()` | percent-anger |
| Defy-resolution anger | `getDefyResolutionPercentAnger(iExtra)` (CvCity.cpp:5457) — `getDefyResolutionAngerTimer()` | percent-anger |
| War weariness (player-level accumulated + city-local timer) | `getWarWearinessPercentAnger()` (CvCity.cpp:5467) — `GET_PLAYER().getWarWearinessPercentAnger()` × `getWarWearinessModifier()` × `(getWarWearinessTimer()+100)/100` | percent-anger |
| Revolution request anger | `getRevRequestPercentAnger(iExtra)` (CvCity.cpp:5481) — `getRevRequestAngerTimer()` | percent-anger |
| Revolution index anger | `getRevIndexPercentAnger()` (CvCity.cpp:5499) — fires when `getRevolutionIndex() > 325`; scaled by `getLocalRevIndex()` | percent-anger |
| Civic percent-anger | `GET_PLAYER().getCivicPercentAnger(eCivic)` loop (CvCity.cpp:5618–5621) | percent-anger |
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
| Espionage happiness counter | `getEspionageHappinessCounter()` (positive = unhappy side; CvCity.cpp:5639) | flat |
| Civic happiness (negative) | `getCivicHappiness()` | flat |
| Specialist unhappiness | `getSpecialistUnhappiness()/100` | flat |
| World happiness (negative) | `GET_PLAYER().getWorldHappiness()` | flat |
| Project happiness (negative) | `GET_PLAYER().getProjectHappiness()` | flat |
| Tax rate unhappiness | `GET_PLAYER().calculateTaxRateUnhappiness()` | flat |
| Corporation happiness (negative) | `calculateCorporationHappiness()` | flat |
| Event anger | `getEventAnger()` (decays by 1 every `10 × speedPercent/100` turns; CvCity.cpp:21753) | flat |
| Extra tech happiness total (negative) | `getExtraTechHappinessTotal()` | flat |
| Foreign unhappy percent (culture-penalized) | `getForeignUnhappyPercent()` (CvCity.cpp:5649–5654) | flat |
| Landmark anger (MAP_PERSONALIZED only) | `getLandmarkAnger()` unless `isNoLandmarkAnger()` | flat |
| City-over-limit (civic soft cap) | `getCityOverLimitUnhappy() × overLimitCities` | flat |

Short-circuit bypass: `isNoUnhappiness()` (CvCity.cpp:5600) → entire function returns 0. Also `isCapital() && GET_PLAYER().isNoCapitalUnhappiness()`.

**`happyLevel()` sources** (CvCity.cpp:5679–5717):

| Source | Accessor | Type |
|---|---|---|
| Revolution-success happiness | `getRevSuccessHappiness()` | flat |
| LargestCity bonus (positive) | `getLargestCityHappiness()` (CvCity.cpp:5541) — fires if `findPopulationRank() <= worldTargetNumCities` | flat |
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
| Celebrity happiness | `getCelebrityHappiness()` (celebrity units on the city plot; CvCity.cpp:5589) | flat |
| Extra tech happiness total (positive) | `getExtraTechHappinessTotal()` | flat |
| Landmark happiness (MAP_PERSONALIZED) | `GET_PLAYER().getLandmarkHappiness()` | flat |
| Temp happy (happiness timer) | `getHappinessTimer() > 0 → GC.getTEMP_HAPPY()` (CvCity.cpp:5712) | temporary |

### 1b. Anger timers — per-turn countdown, set by game events

Seven per-city integer counters that decay automatically each turn in `CvCity::doTurn`
(CvCity.cpp:1374–1417). All are `-1` per turn; none "recharge" — they are set at event time
and burn down:

| Timer | Set by | Anger contribution |
|---|---|---|
| `getHurryAngerTimer()` | Hurry (rush-buy) a unit/building | `getHurryPercentAnger()` |
| `getConscriptAngerTimer()` | Drafting/conscripting a unit | `getConscriptPercentAnger()` |
| `getDefyResolutionAngerTimer()` | Defying a UN resolution | `getDefyResolutionPercentAnger()` |
| `getHappinessTimer()` | Celebrations / certain buildings / events | `GC.getTEMP_HAPPY()` bonus to happy side |
| `getRevRequestAngerTimer()` | Revolution request events | `getRevRequestPercentAnger()` |
| `getRevSuccessTimer()` | Successful revolution | `getRevSuccessHappiness()` |
| `getLandmarkAngerTimer()` | Landmark events (MAP_PERSONALIZED) | `getLandmarkAnger()` |

**War weariness city timer** (`getWarWearinessTimer()`, CvCity.cpp:21723): decays by 20/turn
(CvCity.cpp:21751); set from combat results via `CvPlayer.cpp:16482`. Multiplies the
player-level `getWarWearinessPercentAnger()` in `getWarWearinessPercentAnger()` (CvCity.cpp:5474).

**Event anger** (`getEventAnger()`, CvCity.cpp:21765): decays by 1 every `10 × speedPercent/100`
turns (checked at `doWarWeariness`, CvCity.cpp:21753).

**Espionage counters** (`getEspionageHappinessCounter()`, `getEspionageHealthCounter()`): decay
by 1 per turn (CvCity.cpp:1414–1417); set by espionage missions.

### 1c. Revolution index — `getRevolutionIndex()` / `getLocalRevIndex()`

`getRevolutionIndex()` (CvCity.cpp:951) — a per-city accumulator; contributes to
`getRevIndexPercentAnger()` only when `> 325` (CvCity.cpp:5503). `getLocalRevIndex()` (CvCity.cpp:969)
scales the contribution. Both are updated by events and the doTurn revolution system; details
deferred to a future revolution-system observability map.

### 1d. Health — `goodHealth()` minus `badHealth()`

**`goodHealth()` sources** (CvCity.cpp:5821–5844):

| Source | Accessor |
|---|---|
| Fresh water | `getFreshWaterGoodHealth()` |
| Feature good health | `getFeatureGoodHealth()` |
| Bonus good health | `getBonusGoodHealth()` |
| Building good health (city + area + player + extra) | `totalGoodBuildingHealth()` (CvCity.cpp:5797) |
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

**`badHealth()` sources** (CvCity.cpp:5848–5873):

| Source | Accessor |
|---|---|
| Espionage health counter (malus) | `-getEspionageHealthCounter()` |
| Feature bad health | `getFeatureBadHealth()` |
| Bonus bad health | `getBonusBadHealth()` |
| Building bad health (city + area + player + extra) | `totalBadBuildingHealth()` (CvCity.cpp:5807) |
| Extra health (negative) | `getExtraHealth()` if negative |
| Handicap health bonus (negative) | `GC.getHandicapInfo().getHealthBonus()` if negative |
| Extra building bad health | `getExtraBuildingBadHealth()` |
| Improvement bad health | `getImprovementBadHealth()/100` |
| Specialist bad health | `getSpecialistBadHealth()/100` |
| Corporation health (negative) | `calculateCorporationHealth()` if negative |
| Extra tech health total (negative) | `getExtraTechHealthTotal()` if negative |
| Player extra health (negative) | `owner.getExtraHealth()` if negative |
| Player civic/civ/world/project health (negative) | same accessors as goodHealth |
| **Population itself** | `unhealthyPopulation(bNoAngry, iExtra)` (CvCity.cpp:5787) — `max(0, pop - angryPop)` unless `isNoUnhealthyPopulation()` |

Short-circuit bypass: `isBuildingOnlyHealthy()` (CvCity.cpp:9671) → `totalBadBuildingHealth()` returns 0.
`isNoUnhealthyPopulation()` (CvCity.cpp:9642) → unhealthy population term = 0.

`healthRate()` = `min(0, goodHealth() - badHealth())` (CvCity.cpp:5876) — always ≤ 0; a sick city
returns a negative value.

### 1e. Downstream consequences

**Food starvation via healthRate** (CvCity.cpp:5912–5918):
`foodConsumption() = getFoodConsumedByPopulation() - angryPopulation() - healthRate()`
(negative `healthRate` is subtracted, so sick cities consume *more* food per turn, which then
reduces `foodDifference()` → faster starvation or negative food-stores → population loss via
`changeFood()` loop at CvCity.cpp:9730–9740).

**Growth suppression**: when `angryPopulation(1) > 0`, `AI_avoidGrowth()` triggers (CvCityAI.cpp:9747),
pinning food at the threshold.

**WeLoveTheKingDay** (CvCity.cpp:1419–1430): set `false` if `isOccupation() || angryPopulation()>0 ||
healthRate()<0`; set `true` stochastically (pop-weighted rand < `WE_LOVE_THE_KING_RAND`) otherwise.
WLTK day waives distance+numCities maintenance (CvCity.cpp:7433/7471/7493/7516/7540) and doubles
GPP generation (CvCity.cpp:7604). So both health and happiness feed this crucial economic gate.

**Disorder** (`isDisorder()` = `isOccupation() || GET_PLAYER().isAnarchy()`, CvCity.cpp:5282):
`foodDifference()` returns 0, production idles, corporations and doProduction stall.

**Production choice** (CvCityAI): `happyLevel() - unhappyLevel()` and `goodHealth() - badHealth()` both
feed directly into building value scoring (CvCityAI.cpp:654, 698, 884, 885, 5077, 5079, etc.)
and the hurry threshold (CvCityAI.cpp:10695).

---

## 2. Current observability — **TIER 0–1**

### What is exposed today

| Surface | Fields | Notes |
|---|---|---|
| `GET /cities` | `population`, `food` (YIELD_FOOD rate), `production`, `commerce`, `crime`, `education`, `disease` | CvHttpServer.cpp:1544–1562 |
| `GET /players` | `score`, `era`, `techs`, `gold`, `goldRate`, `scienceRate`, `population`, `units`, `cities` | CvHttpServer.cpp:1465+ |
| `[CIT/proplevel]` (CityAI.log, level 1) | per-turn per-city snapshot: `prop=` `val=` `change=` for every active property | CvCity.cpp:1244 |
| `[CIT/begin]` (CityAI.log, level 1) | `pop=` `danger=` `finTrouble=` `critGold=` `foodProd=` | CvCityAI.cpp:966 |

### What is NOT exposed

Everything else. Specifically:

- **Net happiness** (`happyLevel()`, `unhappyLevel()`, `angryPopulation()`) — zero endpoints, zero log lines.
- **Net health** (`goodHealth()`, `badHealth()`, `healthRate()`) — zero endpoints, zero log lines.
- **WeLoveTheKingDay** (`isWeLoveTheKingDay()`) — not in any snapshot or log.
- **All anger timers** (`hurryAngerTimer`, `conscriptAngerTimer`, `defyResolutionAngerTimer`,
  `happinessTimer`, `warWearinessTimer`, `revRequestAngerTimer`, `revSuccessTimer`,
  `eventAnger`, `espionageHappinessCounter`, `espionageHealthCounter`) — none visible.
- **Revolution index** (`getRevolutionIndex()`, `getLocalRevIndex()`) — not exposed.
- **War weariness at player level** (`getWarWearinessPercentAnger()`) — not in `/players`.
- **Per-source breakdown** of any happiness or health contributor — nowhere.
- **Disorder flag** (`isDisorder()`, `isOccupation()`, `isAnarchy()`) — not in any snapshot.
- **`isNoUnhappiness()`** / **`isNoUnhealthyPopulation()`** bypass flags — not exposed.
- **Food starvation signal**: `foodDifference()` could be negative (starvation); current `/cities`
  `food` field = `YIELD_FOOD` rate (gross), NOT net food change. There is no `foodDifference` field.

### Tier assessment

| Tier | Name | Status |
|---|---|---|
| 0 | Oblivious | baseline — no health/happiness on wire at all |
| **1** | **Telescreen** | `/cities` gives pop + food RATE (not net); `[CIT/proplevel]` gives property values (disease, crime) but those are the C2C property system, not the Civ4 health/happiness ledger |

**Current rating: Tier 0 for health & happiness specifically.** The property system (disease/crime/education)
has Tier 1 coverage via `[CIT/proplevel]` + the `/cities` `crime`/`education`/`disease` fields, but
the Civ4 `goodHealth/badHealth/happyLevel/unhappyLevel` ledger is completely dark.

---

## 3. The gap

An agent watching the HTTP endpoints + logs TODAY cannot:

1. Know whether a city is in anger (angry population > 0) or sick (healthRate < 0).
2. Know whether WeLoveTheKingDay is active (waives maintenance, doubles GPP).
3. Know why a city is angry — which source dominates (overcrowding? hurry? war weariness?).
4. Know the magnitude of any anger timer — can't predict when anger will abate.
5. Know whether food starvation is occurring (need `foodDifference()` < 0, not just the yield rate).
6. Know player-level war weariness (`getWarWearinessPercentAnger()`), which multiplies into every
   city's war-weariness anger.
7. Know when disorder is active (production and food zeroed, corporations stalled).
8. Know which happiness/health sources are positive vs negative — can't attribute a happy/sick city.
9. Know whether bypass flags are active (`isNoUnhappiness`, `isNoUnhealthyPopulation`,
   `isBuildingOnlyHealthy`).

Consequence for cascade verification: the `requires.operate` targets for happiness- and health-gated
buildings (property-band buildings from `checkPropertyBuildings`, religion-dormancy triggers) cannot be
verified against ground truth without adding this observability layer. Similarly, the `autoBuild` placement
shadow cannot explain WHY a property-band building was added/removed without seeing `goodHealth`/`badHealth`.

---

## 4. Proposed hooks — climbing from Tier 0 to Tier 3

### 4a. `/cities` snapshot additions (cheapest — same publish path)

Add to `CitySnap` struct (CvHttpServer.cpp:83) and the `publishIfDue` city-walk
(CvHttpServer.cpp:1542+):

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

These are all cheap single-call reads (most are already computed once per turn for AI scoring).
They make the most critical derived state readable at Tier 1 for ALL players (AI included).

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

These are the player-level aggregates; combined with the per-city fields above, an agent can
attribute city happiness/health to player-level vs city-level sources.

### 4c. `[CIT/happy]` log tag — per-turn per-city headline (Tier 3)

A new level-1 tag in `CvCity::doTurn` (alongside `[CIT/proplevel]`), emitting the net figures
and any **changed** anger timers. Proposed tag: `[CIT/happy]`.

```
[CIT/happy] turn=N city=X owner=P happy=H unhappy=U angry=A health=G bad=B rate=R wltk=0|1 disorder=0|1
```

- Gated at `gCityLogLevel >= 1`.
- Emitted ONCE per city per turn (before production choice, so the state is current).
- Streamed to `/events` as a `log` frame (same `streamLogTee` path as `[CIT/proplevel]`).

Optional level-2 expansion: `[CIT/happy/timers]` emitting the non-zero anger timers (hurry, conscript,
warWeariness, etc.) for forensic correlation. Only emit when any timer > 0 (keeps log quiet).

### 4d. `[CIT/angry]` events for timer-set moments (Tier 3 delta)

At the event sites that SET anger timers (hurry, conscript, defy-resolution, war-weariness), emit a
level-1 `[CIT/angry]` line: `city= owner= cause=hurry|conscript|warWeariness timer=N`. This gives the
agent the triggering event as it happens (live in `/events`), not just the current value.

Key sites:
- Hurry timer set: `CvCity::changeHurryAngerTimer` (CvCity.cpp:~9509 area).
- Conscript timer set: `CvCity::changeConscriptAngerTimer` (CvCity.cpp:~9560 area).
- War weariness timer injection: `CvPlayer.cpp:16482`.

### 4e. `[CIT/wltk]` on WeLoveTheKingDay change

At `CvCity::setWeLoveTheKingDay` (CvCity.cpp:10352), emit a level-1 line when the value **changes**:
`[CIT/wltk] city= owner= wltk=0|1`. Currently WLTK fires/clears every turn — the toggle itself is
the signal (WLTK clear = happiness or health problem started; WLTK set = resolved).

### 4f. No new diagnostic gate-eval needed for anger/health

Unlike buildability (where cascade vs legacy divergence demands a diagnostic endpoint), health and
happiness are read-only aggregates without a cascade equivalent yet. The snapshot fields (§4a/§4b) and
log tags (§4c/§4d/§4e) are sufficient for Tier 3. A `/diagnostic/cityHappiness?id=N` gate-eval can be
added later when the cascade starts representing happiness sources as `requires`/`enables` atoms — at
that point a per-source breakdown endpoint becomes the Tier-4/5 verification tool.

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

## 6. Cross-references

- `CvCity.cpp:5596` — `unhappyLevel`
- `CvCity.cpp:5679` — `happyLevel`
- `CvCity.cpp:5821` — `goodHealth`
- `CvCity.cpp:5848` — `badHealth`
- `CvCity.cpp:5876` — `healthRate`
- `CvCity.cpp:5912` — `foodConsumption` (health feeds food via healthRate)
- `CvCity.cpp:9713` — `changeFood` (starvation pop-loss loop)
- `CvCity.cpp:1374–1430` — anger timer countdown + WLTK gate
- `CvCity.cpp:21747` — `doWarWeariness` (warWearinessTimer decay, eventAnger decay)
- `CvHttpServer.cpp:83` — `CitySnap` struct (add fields here)
- `CvHttpServer.cpp:1542` — city publish walk (add reads here)
- `docs/dev/plans/cascade-mapping-inventory.md` §D — the observability scale
- `docs/dev/reference/http-server.md` — live surface reference
- `docs/dev/reference/ai-logging-reference.md` — `[CIT/*]` tag registry
