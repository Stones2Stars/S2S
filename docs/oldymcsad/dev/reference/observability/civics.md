# Observability — civics, anarchy & revolution

> **Status:** reference (per-system observability map) · **Verified against:** old map (2026-06-18) re-grounded to the reorganized source tree, 2026-06-20
> **Grounding:** `Sources/Engine/CvPlayer.cpp`, `Sources/AI/CvPlayerAI.cpp`, `Sources/Tools/CvHttpServer.cpp`. Citations carried from the old map; the
> `Sources/` prefixes are re-grounded (`Cv*` → `Sources/Engine/`, `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`) but the **line numbers were
> NOT re-verified** against the reorganized tree — treat each as "the named function, around this line" and confirm the function, not the integer.
> One-paragraph orientation: this maps how the empire-wide civic policy set, the revolution/anarchy mechanism, and the AI's civic decision loop are
> observed today — and why the entire civic state (active set, anarchy, cooldowns, ~50 effect scalars) is opaque from outside. Read the scaffold
> [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the three hook shapes) and [`http-server.md`](http-server.md) (the live surface + live-read
> rules) first; this doc does not restate them.

**Tier today: 1 (Telescreen).** The `/players` snapshot carries **zero** civic fields. The only civic observability is a per-civic `/diagnostic/canDoCivics`
gate check and a level-2 AI log line emitted *only* when a revolution is committed. The active civic set, anarchy state, the revolution / AI-eval cooldowns,
civic upkeep, and every per-civic effect scalar are fully opaque from outside the running game.

---

## 1. How it works

### 1a. What a civic is
A civic is an empire-wide persistent policy choice. Players hold one civic per `CivicOptionTypes` slot (the "category": Government, Legal, Labour, Economy,
Religion, …). The active set lives in `CvPlayer::m_paeCivics[]` (one `CivicTypes` per option). NPCs hold civics but `processCivics` is a no-op for them
(`Sources/Engine/CvPlayer.cpp:17995`, verify).

### 1b. Eligibility gate — `canDoCivics`
`CvPlayer::canDoCivics(CivicTypes)` (`Sources/Engine/CvPlayer.cpp:8448`, verify) — the single per-civic eligibility gate:

1. `NO_CIVIC` always passes.
2. If the game has force-set this civic option (`GC.getGame().isForceCivicOption`), only the forced civic passes.
3. For non-NPC players, returns `false` if EITHER:
   - The civic has a `CityLimit > 0`, no `CityOverLimitUnhappy`, and the player already has more cities than the limit (city-count gate), OR
   - The player has not yet unlocked the civic's `CivicOptionType` via `isHasCivicOption` **AND** the team hasn't researched the civic's `TechPrereq`
     (`Sources/Engine/CvPlayer.cpp:8462-8474`, verify).

So "can I even pick this civic" is: **tech prereq fulfilled OR civic option already unlocked (from a prior civic), and city count within limit if applicable.**

### 1c. Revolution — triggering the switch
`CvPlayer::canRevolution(CivicTypes*)` (`Sources/Engine/CvPlayer.cpp:8482`, verify): blocked if (a) already in anarchy, (b) NPC, (c)
`getRevolutionTimer() > 0`, or (d) no eligible civic differs from the current set.

`CvPlayer::revolution(CivicTypes*, bool bForce)` (`Sources/Engine/CvPlayer.cpp:8527`, verify):
1. Saves old civics array.
2. Computes `iAnarchyLength = getCivicAnarchyLength(paeNewCivics)` — may be 0.
3. Calls `changeAnarchyTurns(iAnarchyLength)` if > 0.
4. For each changed slot: `setCivics(option, newCivic)`, records a `civcSwitchInstance` into `m_civicSwitchHistory`.
5. `NoteCivicsSwitched(iCivicChanges)`.
6. Sets `revolutionTimer = max(1, ((100 + anarchyModifier) * MIN_REVOLUTION_TURNS) / 100) + iAnarchyLength` (`Sources/Engine/CvPlayer.cpp:8568`, verify).

**Revolution timer = the cooldown between revolutions.** A player cannot start another revolution while this timer > 0 (checked in `canRevolution`). It ticks
down 1/turn in `CvPlayer::doTurn` (`Sources/Engine/CvPlayer.cpp:3783-3786`, verify).

### 1d. Anarchy — formula and tick-down
**Civic anarchy length (`getCivicAnarchyLength`, `Sources/Engine/CvPlayer.cpp:8939`, verify):**
- 0 if in a Golden Age.
- 0 if `getMaxAnarchyTurns() < 1` (e.g. Spiritual trait).
- For each changed civic whose `getAnarchyLength() > 0`: `iTotalAnarchyLength += anarchyLength * 100`.
- Quantity discount when N > 1 changes: `total -= total * N * CIVIC_ANARCHY_QTY_DISCOUNT / 100`.
- Scale by game speed: `* speedPercent / 100`.
- Add city count: `+= numCities * worldNumCitiesAnarchyPercent`.
- Apply `getAnarchyModifier()` and `getCivicAnarchyModifier()`: `* (mod + 100) / 100` each.
- Apply era factor: `* eraAnarchyPercent / 100`.
- Rebel discount: `/= 2` if `isRebel()`.
- Final `/= 100` (the centipercent scale).
- Clamp: `max(1, range(result, getMinAnarchyTurns(), getMaxAnarchyTurns()))`.

**Religion anarchy length (`getReligionAnarchyLength`, `Sources/Engine/CvPlayer.cpp:9001`, verify):** similar but based on `BASE_RELIGION_ANARCHY_LENGTH`
global define + city-count + `getAnarchyModifier()` + `getReligiousAnarchyModifier()` from traits.

**Anarchy tick-down:** `CvPlayer::doTurn` (`Sources/Engine/CvPlayer.cpp:3859-3862`, verify): if `getAnarchyTurns() > 0`, increment the `m_iNumAnarchyTurns`
stat counter and call `changeAnarchyTurns(-1)`. Golden age and anarchy are mutually exclusive; starting a golden age clears anarchy
(`changeAnarchyTurns(-getAnarchyTurns())` at `Sources/Engine/CvPlayer.cpp:9409`, verify).

**Max/min anarchy:** `getMaxAnarchyTurns()` (`Sources/Engine/CvPlayer.cpp:9548`, verify) — minimum of the `MAX_ANARCHY_TURNS` global and any trait
`getMaxAnarchy()` override (Spiritual sets 0). `getMinAnarchyTurns()` (`Sources/Engine/CvPlayer.cpp:9580`, verify) — max trait `getMinAnarchy()`, capped by
`getMaxAnarchyTurns()`.

**Anarchy modifier:** `m_iAnarchyModifier` (`Sources/Engine/CvPlayer.cpp:9615`, verify) — accumulates from buildings (`changeAnarchyModifier`) and traits
(`changeCivicAnarchyModifier`). Applied to both civic and religion anarchy lengths; also adjusts revolution/conversion timers on change.

**Policy civics:** `isPolicy()` civics (`Sources/Engine/CvPlayer.cpp:8956`, verify) are excluded from anarchy computation even when switched — zero-cost changes.

### 1e. Civic effects — `setCivics` and `processCivics`
`CvPlayer::setCivics(CivicOptionTypes, CivicTypes)` (`Sources/Engine/CvPlayer.cpp:14288`, verify):
- Updates `m_paeCivics[eIndex]`.
- NPCs return early after the raw array write.
- For non-NPCs: `processCivics(oldCivic, -1)` then `processCivics(newCivic, +1)`.
- Fires `CvEventReporter::civicChanged(player, old, new)` → Python `onCivicChanged` (`Sources/Engine/CvPlayer.cpp:14384`, verify). **Python only — NOT the SSE event spine.**
- Dirties UI caches.

`CvPlayer::processCivics(eCivic, iChange, bLimited)` (`Sources/Engine/CvPlayer.cpp:17990`, verify) applies/removes the full civic effect bundle — **~50+
empire-wide scalars** in full mode (`!bLimited`): Great-People / Great-General rate mods; maintenance mods (distance, num-cities, home-area, other-area); free
XP, worker speed, improvement upgrade rate, military production mod; free unit upkeep + upkeep mods; max conscript, free specialists, trade routes;
state-religion production/unit/building/free-XP mods; per-type commerce rate mods + capital mods + specialist extra commerce + landmark yields; building
happiness/health, building production/commerce mods; per-unit production mods, unit-combat production mods; feature happiness; specialist validity + free
specialist counts + specialist yield/commerce; improvement and terrain yield changes (per × yield); foreign trade-route mod, religion spread rate, corporation
spread mod, distant-unit support mod, extra city defense; freedom fighters, enslavement chance, civic inflation, hurry cost/inflation mods; landmark happiness,
no-landmark-anger, fixed borders; revolution-index mods (local/national/distance/holy-city/nationality/religion); city limit + over-limit-unhappy + foreign
unhappy percent; war-weariness mod; all-religions-active + bans-non-state-religions counts; hurry-type flags; `SpecialBuildingNotRequired` counts (the §14
B-iii group gate); state-religion counts + spread bans + happiness; inquisition counts; vote-source secretary-general clearing; bonus minted percent / bonus
commerce mod; civic/largest-city happiness, no-capital-unhappiness, tax-rate unhappiness, happy-per-military-unit; civic health, no-unhealthy-pop,
building-only-healthy; pop growth rate %, corporation maintenance mod, military food production; no-foreign-trade / no-corporations / no-foreign-corporations.

**Limited mode (`bLimited = true`):** only building happiness/health, feature happiness, and specialist validity — the subset the AI civic valuation needs for
a temporary test-swap without a full recalculation.

### 1f. AI civic decision — `AI_doCivics`
`CvPlayerAI::AI_doCivics()` (`Sources/AI/CvPlayerAI.cpp:16848`, verify) runs on AI (non-human, non-NPC) players from `AI_doTurnPre`
(`Sources/AI/CvPlayerAI.cpp:497`, verify):
1. Decrements `m_turnsSinceLastRevolution`, decays `m_iCivicSwitchMinDeltaThreshold`.
2. Checks `AI_getCivicTimer()` — if > 0, ticks down and returns (the cooldown between evaluations).
3. Checks `canRevolution(NULL)` — if false, returns.
4. For each civic option: finds the best available civic via `AI_bestCivic` (using a temporary `processCivics` bLimited test-swap to evaluate interactions).
5. Iterates to convergence (max `getNumCivicOptionInfos()` passes).
6. If the best set differs from current AND the value delta exceeds `m_iCivicSwitchMinDeltaThreshold`: checks near-future civics (techs within
   `20 * anarchyLength` turns) — may defer; costs the anarchy against a benefit estimate (`perTurnDelta * min(50, turnsSinceRevolution) * speedPercent`);
   drops the least-efficient civic change if anarchy cost > benefit.
7. Calls `revolution(paeBestCivic)` if `bDoRevolution` and `canRevolution` still holds.
8. Sets `AI_getCivicTimer()` to `CIVIC_CHANGE_DELAY` (25 turns) or `MIN_REVOLUTION_TURNS` (no-anarchy / golden-age cases).

Logging emitted today:
- `[DAI/civic/best]` (level 2) — the REVOLUTION commit decision: player, `curValue`, `bestValue`, and each changed option (`Sources/AI/CvPlayerAI.cpp:17290-17297`, verify).
- `[DAI/civic/cand]` (level 3) — per-candidate flavor contribution during `AI_civicValue` (`Sources/AI/CvPlayerAI.cpp:13799`, verify).

### 1g. Cooldowns and re-evaluation triggers
- **Revolution timer** (`m_iRevolutionTimer`, `Sources/Engine/CvPlayer.cpp:11115`, verify): "can't start another revolution for N turns" — ticks 1/turn, blocks `canRevolution`.
- **Conversion timer** (`m_iConversionTimer`): same mechanic for religion changes.
- **AI civic timer** (`m_iAICivicTimer`): 25-turn re-evaluation throttle. Set to `CIVIC_CHANGE_DELAY=25` after any evaluation, or `MIN_REVOLUTION_TURNS` for
  no-anarchy cases. Serialized in `CvPlayerAI` (`Sources/AI/CvPlayerAI.cpp:19939`, verify, reads `m_turnsSinceLastRevolution`).
- **`verifyCivics`** (`Sources/Engine/CvPlayer.cpp:4080-4103`, verify): called each doTurn. If a current civic became ineligible (tech revoked via conquest)
  and the player is not in anarchy, **silently** switches to the first eligible civic in the same slot.

### 1h. Anarchy effects on gameplay
While `isAnarchy()` is true: civic upkeep is zero (`getSingleCivicUpkeep` returns 0, `Sources/Engine/CvPlayer.cpp:14232-14234`, verify); commerce is
recalculated on anarchy start/end (`setCommerceDirty`, `Sources/Engine/CvPlayer.cpp:9514`, verify); trade routes and corporation state update; work-assignment
is dirtied; religion conversion is blocked (`canConvert` checks `isAnarchy()`); further revolutions are blocked; the UI anarchy message is shown.

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** Observable:

| Surface | What it shows | Where |
|---|---|---|
| `/players` | `era`, `score`, `gold`, `goldRate`, `scienceRate`, `techs`, `production` — broad empire stats, **no civic fields** | `Sources/Tools/CvHttpServer.cpp:286-303` (verify) |
| `/diagnostic/canDoCivics?type=CIVIC_X&player=N` | Legacy gate result (`canDoCivics`) + partial cascade verdict (wired, no `legacyReason`) | `Sources/Tools/CvHttpServer.cpp:844-853` (verify) |
| `[DAI/civic/best]` (level 2) | AI revolution commit: `curValue`, `bestValue`, options switched | `Sources/AI/CvPlayerAI.cpp:17290-17297` (verify) |
| `[DAI/civic/cand]` (level 3) | Per-candidate flavor contribution during civic valuation | `Sources/AI/CvPlayerAI.cpp:13799` (verify) |
| Python `onCivicChanged` | Fires via `CvEventReporter::civicChanged` → Python only, NOT the SSE spine | `Sources/Engine/CvPlayer.cpp:14384` (verify) |

The `[DAI/civic/best]` line fires **only** when an AI actually commits a revolution — silent on every turn it evaluates and stays, and the 25-turn AI-eval
throttle makes even the commit event coarse-grained. There is no line for the current civic set, anarchy state, or the revolution timer.

---

## 3. The gap

State that cannot be reconstructed from the HTTP layer + logs today:

| Piece of state | Why it matters for the Orwell bar |
|---|---|
| **Active civic set** — `CivicTypes` per `CivicOptionTypes` | Foundation: every downstream civic effect keys to this. No effect is reconstructible without it. |
| **Anarchy state** — `isAnarchy()`, `getAnarchyTurns()` | Changes buildability, upkeep, and the `canRevolution` gate. AI players in anarchy are invisible. |
| **Revolution timer** — `getRevolutionTimer()` | The cooldown that blocks the next revolution; needed to know when an AI COULD switch. |
| **AI civic eval timer** — `AI_getCivicTimer()` | When the AI next re-evaluates; a 25-turn blind window. |
| **Civic upkeep** — `getCivicUpkeep()` | Major gold drain; `/players` carries `goldRate` but its breakdown is opaque. |
| **Anarchy modifier / max-min anarchy** — `getAnarchyModifier()`, `getMaxAnarchyTurns()`/`getMinAnarchyTurns()` | Scale anarchy length; `max=0` (Spiritual) means free switches. All invisible. |
| **Switch decision inputs** — `iCurCivicsValue`, `iBestCivicsValue`, the near-future horizon, the per-option breakdown | `[DAI/civic/best]` logs the commit, never the "evaluated and stayed" turns or the cost-benefit when it chose not to switch. |
| **`verifyCivics` silent switches** | No log or event; the switch just happens. Could corrupt a cascade baseline undetected. |
| **The ~50+ `processCivics` scalars** | None exposed. You see aggregate `goldRate`/`scienceRate` but not the civic fraction. |
| **`SpecialBuildingNotRequired` counts** | Set/cleared by civics; drives the §14 B-iii group gate — needed for cascade build-prereq observability. |
| **Religion anarchy** — `getConversionTimer()`, `getReligionAnarchyLength()` inputs | Conversion is the sibling mechanism; same blind spot. |

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see [DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook A — `/players` snapshot: civic fields
In the player-snap loop (`Sources/Tools/CvHttpServer.cpp:1524`, verify), add to `PlayerSnap` and render in `renderPlayers`:
```
"anarchy":     getAnarchyTurns()      // 0 = not in anarchy; >0 = turns remaining
"maxAnarchy":  getMaxAnarchyTurns()   // 0 = Spiritual / no-anarchy trait
"revTimer":    getRevolutionTimer()   // turns until next revolution allowed
"civicUpkeep": getCivicUpkeep()       // total gold/turn drain from active civics
"civics": {                           // per-option map of active civic type strings
  "CIVICOPTION_GOVERNMENT": "CIVIC_REPUBLIC", ...   // GC.getCivicInfo(getCivics(opt)).getType()
}
```
This alone climbs to **Tier 3** on the civic axis — full civic state every 5 s.

### Hook B — `[CIV/state]` per-player per-turn heartbeat (level 1)
A new `[CIV]` domain tag, once per player turn from `AI_doTurnPre`, gated `gPlayerLogLevel >= 1`:
```
[CIV/state] player=N anarchy=K revTimer=T aiCivicTimer=U
```
A per-turn heartbeat on every AI player's civic state — no more 25-turn blind window.

### Hook C — `[CIV/switch]` on every civic change (incl. `verifyCivics`)
In `CvPlayer::setCivics` (`Sources/Engine/CvPlayer.cpp:14288`, verify), after the swap, at level 1:
```
[CIV/switch] player=N option=CIVICOPTION_X old=CIVIC_OLD new=CIVIC_NEW anarchy=K bForced=0|1
```
`bForced=1` for `verifyCivics` switches (pass a flag through). Covers ALL civic changes including the currently-silent auto-replacement.

### Hook D — `[CIV/anarchy]` on anarchy start/end (level 1)
In `CvPlayer::changeAnarchyTurns` (`Sources/Engine/CvPlayer.cpp:9503`, verify), when `bOldAnarchy != isAnarchy()`:
```
[CIV/anarchy] player=N start=1|0 turns=K revTimer=T
```
Makes anarchy transitions instantly visible on the stream for all players.

### Hook E — `[DAI/civic/eval]` when AI evaluates and does NOT switch (level 2)
In the no-revolution `else` branch of `AI_doCivics` (`Sources/AI/CvPlayerAI.cpp:17303-17309`, verify):
```
[DAI/civic/eval] player=N NOSTAY curValue=C bestValue=B threshold=T aiCivicTimer=U
```
Without this, every 25-turn "not worth it" evaluation is invisible.

### Hook F — `[DAI/civic/defer]` when AI defers for a near-future civic (level 2)
In the near-future horizon block of `AI_doCivics` (`Sources/AI/CvPlayerAI.cpp:~17136-17244`, verify), when the near-future score exceeds the current best:
```
[DAI/civic/defer] player=N waiting=CIVIC_X turns=T nearFutureValue=NF bestValue=B
```

### Hook G — `/diagnostic/civics?player=N` full snapshot endpoint
The civic parallel of `/diagnostic/placementSweep` (mailbox pattern):
```json
{ "player": N, "anarchy": K, "maxAnarchy": M, "revTimer": T, "conversionTimer": U,
  "aiCivicTimer": V, "civicUpkeep": W,
  "civics": [ { "option": "CIVICOPTION_GOVERNMENT", "civic": "CIVIC_REPUBLIC", "canDo": true, "anarchyLength": 2 }, ... ] }
```
Enables reconstruct-without-screen of every AI player's civic state on demand.

**Priority:** **Tier 3** = A + C + D (snapshot the set + emit switches + emit anarchy transitions — covers human and AI within the 5 s window, all
state-changes on the stream). **Tier 4** = additionally B + E + F + G (per-turn heartbeat + AI-no-switch log + near-future-defer log + diagnostic endpoint —
the full decision loop narrated). **Tier 5** (the full `processCivics` effect breakdown — ~50 scalars per civic) would need per-civic modifier snapshots or a
dedicated `/civic-effects` endpoint; expensive to enumerate and likely not needed for the #428/#430 shadow.

---

## 5. Cascade relevance (#428/#430)

Civics are a `requires`-family gate and a heavy modifier source for the cascade:
- **Build/train prereqs:** `canDoCivics` gates which civics are pickable; civics in turn set `SpecialBuildingNotRequired` counts (the §14 B-iii group gate)
  and `isHasCivicOption` flags — both feed `canConstruct`/`canTrain`. A civic shadow needs the active set (Hook A) to attribute a `canConstruct` divergence.
- **Modifier-cascade targets:** the ~50 `processCivics` scalars (commerce/yield/happiness/health/maintenance mods) are exactly the `modifier-cascade` channel;
  the cascade's tally must reproduce them, and today none are attributable on the wire.
- **State the tally tracks:** anarchy suppresses upkeep and blocks revolution/conversion; `verifyCivics` silently mutates the active set. Both are player-owned
  state-machine transitions the cascade must observe before any legacy maintainer is cut ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).
- Hooks A + C are the minimum substrate for a **civic shadow** comparing the cascade's expected build/modifier set against the live engine.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/diagnostic/*`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing, and the modifier/`requires` channels civics feed.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
