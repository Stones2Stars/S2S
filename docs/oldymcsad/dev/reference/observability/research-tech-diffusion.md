# Observability — research, beakers & tech diffusion

> **Status:** reference (per-system observability map) · **Verified against:** live source, 2026-06-20
> **Grounding:** `Sources/Engine/CvPlayer.cpp`, `Sources/Engine/CvTeam.cpp`, `Sources/AI/CvPlayerAI.cpp`,
> `Sources/Tools/CvHttpServer.cpp`. Line numbers drift — confirm the named function, not the integer.
> One-paragraph orientation: this maps how per-player beaker accrual, tech-cost modifiers, the per-team
> research ledger, and the special diffusion mechanics are observed today — and the gaps that block
> reconstructing research state from outside the running game. Read the scaffold
> [`README.md`](README.md) (the 0–5 scale, the Orwell bar, the three hook shapes) and
> [`http-server.md`](http-server.md) (the live surface + live-read rules) first; this doc does not restate them.

**Tier today: 1 (Telescreen).** `/players` exposes coarse snapshots (`research`, `techs`, `scienceRate`).
There is **no** per-turn beaker movement, **no** progress fraction, **no** modifier breakdown, **no**
diffusion-input trace, and **no** tech-completion event on the `/events` stream. The "how far along" and
"why faster/slower" questions are both unanswerable from the wire.

---

## 1. How it works

### 1a. Per-turn beaker accrual (`CvPlayer::doResearch`)

Called once per player from `CvPlayer::doTurn` — `Sources/Engine/CvPlayer.cpp:3813`. NPC players are
skipped immediately (`doResearch`, `CvPlayer.cpp:15510`: `if (isNPC()) return`).

1. **Path-length cache invalidated** (`CvPlayer.cpp:15512-15516`): every tech's `m_aiPathLengthCache` /
   `m_aiCostPathLengthCache` reset to `-1` each turn.
2. **AI queue refill if empty** (`CvPlayer.cpp:15520-15534`): if `getCurrentResearch() == NO_TECH`, the
   active human gets `chooseTech()`; once `getElapsedGameTurns() > 4`, `AI_chooseResearch()` is called and
   `bForceResearchChoice` is set.
3. **Beaker calculation** (`CvPlayer.cpp:15537-15547`):
   - `eCurrentTech == NO_TECH` (human hasn't chosen): overflow accumulates at `calculateResearchRate()`
     with a negative modifier — `changeOverflowResearch(getModifiedIntValue(calculateResearchRate(), -calculateResearchModifier(NO_TECH)))`.
   - Otherwise: previous-turn overflow is drained and applied (with modifier) in the same call —
     `GET_TEAM(getTeam()).changeResearchProgress(eCurrentTech, calculateResearchRate(eCurrentTech) + getModifiedIntValue(iOverflow, calculateResearchModifier(eCurrentTech)), getID())`.

### 1b. Beaker rate components

- `calculateResearchRate(TechTypes eTech)` (`CvPlayer.cpp:8234`): if `isCommerceFlexible(COMMERCE_RESEARCH)`
  returns `calculateBaseNetResearch(eTech)`; otherwise (locked slider) `max(1, calculateBaseNetResearch(eTech) + calculateBaseNetGold())`.
- `calculateBaseNetResearch(TechTypes eTech)` (`CvPlayer.cpp:8203`): `BASE_RESEARCH_RATE +
  getCommerceRate(COMMERCE_RESEARCH)`, modified by `(getNationalTechResearchModifier(eTech) +
  calculateResearchModifier(eTech))`, clamped to `MAX_RESEARCH_RATE_VALUE`. `getCommerceRate(COMMERCE_RESEARCH)`
  is the player's raw beaker sum (city yields, buildings, civics already folded in).
- `getNationalTechResearchModifier(TechTypes)` (`CvPlayer.cpp:29960`, verify): a per-tech per-player
  percentage stored in `m_paiNationalTechResearchModifier[eTech]`, set by events — a hook, little used in
  vanilla content.

`calculateResearchModifier(TechTypes eTech)` (`CvPlayer.cpp:8086`) is the two "difficulty diffusion"
modifiers, both game-option gated, both skipping human players when `GAMEOPTION_TECH_NO_HANDICAPS_FOR_HUMANS`
is on (`CvPlayer.cpp:8095-8102`):

- **Win-for-Losing** (`GAMEOPTION_TECH_WIN_FOR_LOSING`, `CvPlayer.cpp:8095-8099`):
  `iModifier += GET_TEAM(getTeam()).getWinForLosingResearchModifier()` — a percentage bonus for
  smaller/weaker civs, derived from city count and population (verify `CvGame.cpp`). Both inputs are on `/players`.
- **Tech Diffusion** (`GAMEOPTION_TECH_DIFFUSION`, `CvPlayer.cpp:8101-8160`): a floating accumulator
  `knownExp`. For every alive **met** team that has `eTech`: `+0.5` base; `+1.5` if open borders OR that
  team is a vassal; else `+0.5` if at war OR own team is a vassal. Speed tier on adoption fraction: if
  `iTeamsHaveTech * 3 < iTeams` then `knownExp /= 100` (slow); if `iTeamsHaveTech * 3 > 2 * iTeams` then
  `knownExp *= 3` (fast). Scaled `*= iMetTeams / iTeams`. Final
  `iTechDiffusion = max(0, KNOWN_TEAM_MODIFIER - KNOWN_TEAM_MODIFIER * pow(0.85, knownExp))`
  (`GC.getTECH_DIFFUSION_KNOWN_TEAM_MODIFIER()`).
- **Welfare sub-modifier** (`CvPlayer.cpp:8162-8194`): if `GET_TEAM(getTeam()).getBestKnownTechScorePercent() <
  TECH_DIFFUSION_WELFARE_THRESHOLD`, adds `iWelfareTechDiffusion = max(0, WELFARE_MOD - WELFARE_MOD *
  pow(0.98, threshold - percent))`, then scaled by `bestPlayerScore / ownScore` (only when behind).
- **Cap** (`CvPlayer.cpp:8199`): `iModifier = min(iModifier, 100)` — diffusion can never more than double
  base research.

> ⚠ **Stale-flag correction from the old map.** The old map claimed the two diffusion values are logged to
> `C2C.log` via `logging::logMsg` at `CvPlayer.cpp:8158/8188`. **Those `logMsg` calls no longer exist** —
> `calculateResearchModifier` emits **no logging at all** today (verified: zero `logMsg`/`C2C.log` matches in
> `CvPlayer.cpp`). The diffusion modifier is therefore fully unlogged; the gap is larger than the old map
> stated, and Hook 4 below is a *new* log line, not a re-tag of an existing one.

### 1c. Tech cost (per-team, `CvTeam::getResearchCost`)

`CvTeam::getResearchCost(TechTypes eTech)` (`Sources/Engine/CvTeam.cpp:2584`, verify) multiplies the XML
base cost through (integer arithmetic, ×100/100 intermediates):

1. `TECH_COST_MODIFIER` (global %).
2. `GameSpeedInfo::getSpeedPercent()`.
3. `EraInfo::getResearchPercent()` for the tech's era.
4. `TECH_COST_EXTRA_TEAM_MEMBER_MODIFIER * getNumMembers()` (team-size penalty).
5. **AI handicap reduction** `AIResearchPercent - 100 + AIPerEraModifier * currentEra` (non-human non-NPC
   teams) — this is what makes AI research cheaper at higher difficulty. It is **per-team** and invisible
   outside (no field, no log).
6. `GAMEOPTION_TECH_CUTTING_EDGE_CUTS` modifier (fewer than 3 peers in-era and tech is from a past era).
7. `GAMEOPTION_TECH_UPSCALED_COSTS`: adds `UPSCALED_RESEARCH_COST_MODIFIER` %.

Result clamped `max(1, …)`.

### 1d. Research progress ledger (per-team)

Stored in `CvTeam::m_paiResearchProgress[TechTypes]`. `changeResearchProgress` → `setResearchProgress`
(`CvTeam.cpp`, verify). `setResearchProgress` fires completion when `iNewValue >= getResearchCost(eIndex)`:
`setHasTech(eIndex, true, ePlayer, true, true)`, then `doMultipleResearch` to consume overflow into the
next queued tech. Completion triggers `CvEventReporter::techAcquired(...)` — a **Python-only** callback,
**not** published on `/events`.

### 1e. Overflow and multi-tech completion (`CvPlayer::doMultipleResearch`)

`doMultipleResearch(int iOverflow)` (`CvPlayer.cpp`, verify) loops while overflow (adjusted by
`-calculateResearchModifier`) covers the remaining cost of the queued tech (`CvPlayer.cpp:26821`). Each
iteration calls `setHasTech` directly (not `changeResearchProgress`) and the AI refills via
`AI_chooseResearch`. Repeat/future-tech breaks the loop. Returns unused overflow into `m_iOverflowResearch`.

### 1f. Barbarian / hominid free-tech accrual

`CvTeam::doTurn` (`CvTeam.cpp`, verify): for `isHominid()` teams, each adjacent researchable tech receives
free beakers proportional to how many PC teams already have it —
`max(1, cost * BARBARIAN_FREE_TECH_PERCENT * count / (100 * possible))`. This bypasses per-player
`doResearch` entirely; no player logging applies.

### 1g. AI research choice (`CvPlayerAI::AI_chooseResearch`)

Called from `doResearch` when the queue is empty (`CvPlayer.cpp:15531`) and from `doMultipleResearch` on
completion. Logic (`Sources/AI/CvPlayerAI.cpp`, verify): clear queue; piggyback on a teammate already
researching something this player can; else `AI_bestTech(depth)` (depth 1 for human + Culture3-victory;
depth 3 normally); `pushResearch`. `AI_bestTech` emits `[DAI/tech/best]` at level 1 and `[DAI/tech/cand]`
(flavor breakdown) at level 3 via `logDecisionAI` — so they reach `DecisionAI.log` and `/events` at the
matching level, **only at decision time, not every turn**.

### 1h. Team-merge progress inheritance

During permanent alliance (`CvTeam::copyFrom`, `CvTeam.cpp`, verify): the receiving team's progress on a
tech is bumped up to the joining team's if higher. No logging.

---

## 2. What's on the wire today

**Tier 1 (Telescreen).** Observable:

| What | Source | Exposed via | Notes |
|---|---|---|---|
| Current research tech (key) | `getCurrentResearch()` | `/players` → `research` | XML key, e.g. `TECH_METAL_CASTING` |
| Tech count | `m_paiTechCount` snapshot | `/players` → `techs` | team-shared count |
| Raw beaker rate | `getCommerceRate(COMMERCE_RESEARCH)` | `/players` → `scienceRate` | **pre-modifier** — NOT the actual per-turn deposit |
| Era index | `getCurrentEra()` | `/players` → `era` | needed to reconstruct AI handicap scaling |
| City count | `getNumCities()` | `/players` → `cities` | WinForLosing input |
| Population | `getTotalPopulation()` | `/players` → `population` | WinForLosing input |
| AI tech-choice decision | `[DAI/tech/best]` (lvl 1) | `DecisionAI.log` + `/events` | decision time only |
| AI tech flavor breakdown | `[DAI/tech/cand]` (lvl 3) | `DecisionAI.log` + `/events` | very verbose |
| Handicap | snapshot | `/players` → `handicap` | input to AI cost reduction |

---

## 3. The gap

The system cannot be reconstructed from outside above the coarsest level:

- **"How far along" is unanswerable.** No endpoint gives `researchProgress` or `researchLeft`
  (`CvTeam::m_paiResearchProgress[eTech]`, `getResearchLeft(eTech)`). You see *what* a player researches,
  never how far through. For AI players it is fully opaque.
- **"Why faster/slower" is unanswerable.** The actual deposit `calculateResearchRate(eTech)` is not
  published — `scienceRate` is the raw pre-modifier commerce rate. The diffusion + WFL + national modifier
  values are computed nowhere visible: `calculateResearchModifier(eTech)` emits **no log** (see §1b
  correction), `getWinForLosingResearchModifier()` and `m_paiNationalTechResearchModifier[eTech]` are
  unexposed.
- **Tech cost is hidden.** The full modifier chain (`getResearchCost`, incl. the per-team AI handicap
  reduction) is invisible — turns-to-tech cannot be computed from the XML base.
- **Completions are invisible on the wire.** No SSE event for "tech acquired"; the Python `techAcquired`
  callback has no bridge to `/events`. An observer must poll `/players` → `techs`/`research` to infer it.
- **Overflow, queue, multi-completion are invisible.** `m_iOverflowResearch`, the queued path
  (`headResearchQueueNode()`), and second-tech-in-one-turn completions (which fire via `setHasTech` directly,
  bypassing `changeResearchProgress`) cannot be reconstructed.
- **Diffusion inputs scattered, modifier absent.** `isOpenBorders`/`isHasTech` per team and alive-team
  counts exist but aren't directly exposed; the computed modifier is nowhere; `getBestKnownTechScorePercent`
  (welfare branch input) is unexposed.
- **Barbarian/hominid free-tech** is a separate `CvTeam::doTurn` path with no player-level logging.

---

## 4. Proposed hooks

All hooks are one of the three canonical shapes — see
[DEC-obs-hook-shapes](../../architecture/decisions.md#dec-obs-hook-shapes).

### Hook 1 — `/players` snapshot additions (3 fields)
Add to `PlayerSnap` / `renderPlayers` (`Sources/Tools/CvHttpServer.cpp`):
- **`researchProgress`** (int): `GET_TEAM(kPlayer.getTeam()).getResearchProgress(kPlayer.getCurrentResearch())`; 0 when no tech.
- **`researchCost`** (int): `GET_TEAM(kPlayer.getTeam()).getResearchCost(kPlayer.getCurrentResearch())` — modifier-applied cost; 0 when no tech.
- **`overflowResearch`** (int): `kPlayer.getOverflowResearch()`.

Together these give `%complete`, `turnsLeft` (with `scienceRate`), and multi-completion prediction. Cheap
inline lookups; immediate climb to mid-Tier-2.

### Hook 2 — `[RES/turn]` per-player per-turn line (new `[RES]` domain, `gPlayerLogLevel`, `ResearchAI.log`)
One line per player per turn in `doResearch`:
```
[RES/turn] turn=N player=P tech=TECH_X deposit=D overflow=OVF modifier=M pct=PP% progress=PROG cost=COST left=LEFT turnsLeft=T
```
`deposit = calculateResearchRate(eTech)`; `modifier = calculateResearchModifier(eTech)`. Level 1 = `deposit/modifier/pct/turnsLeft`; level 2 adds `progress/cost/left`. The single most valuable hook — full per-turn beaker movement for all players.

### Hook 3 — `[RES/tech]` tech-completion event (same `[RES]` domain, level 1)
In `CvTeam::setHasTech` at the completion point (`iNewValue >= getResearchCost(eIndex)`), before the
`setHasTech` proper:
```
[RES/tech] turn=N team=T player=P tech=TECH_X overflow=OVF cost=C progress=P
```
Must also fire on the `doMultipleResearch` `setHasTech` path so the multi-tech chain is reconstructible.
This is the critical gap for shadow testing (no polling to detect completions).

### Hook 4 — `[RES/diffusion]` diffusion-modifier trace (level 2)
In `calculateResearchModifier`, after computing `iTechDiffusion` and `iWelfareTechDiffusion`, emit (this is
**new** logging — there is no existing `logMsg` to re-tag, see §1b):
```
[RES/diffusion] turn=N player=P tech=TECH_X knownExp=E teamsWithTech=K metTeams=M allTeams=T diffusion=D welfare=W totalMod=M
```
Brings the per-player per-tech diffusion magnitude onto the wire.

### Hook 5 — `[RES/queue]` research-queue line (level 2)
In `AI_chooseResearch` after `pushResearch`, iterate `headResearchQueueNode()`:
```
[RES/queue] turn=N player=P queue=[TECH_A, TECH_B, TECH_C]
```
Reveals the AI's full research path. Lowest priority (QoL).

### Hook 6 — `GET /diagnostic/researchState?player=N` (mailbox endpoint)
On-demand snapshot read (no game-thread calc), same mailbox pattern as existing `/diagnostic/*`:
```json
{ "player": N, "research": "TECH_X", "progress": 1234, "cost": 2500, "left": 1266,
  "pctComplete": 49, "researchRate": 87, "scienceRate": 70, "modifier": 24,
  "overflowResearch": 0, "turnsLeft": 15, "winForLosingMod": 12, "techDiffusionMod": 12 }
```
The single-shot "what is this player's research doing right now" query — the tech advisor, from outside, for all players.

**Priority:** Hook 1 (snapshot, zero infra) → Hook 3 (completion event, the shadow-critical gap) → Hook 2
(per-turn ledger) → Hook 6 (on-demand spot-check) → Hook 4 (diffusion trace, for Tier-5 diffusion coverage)
→ Hook 5. Hooks 1+3+2 reach **Tier 3 (Big Brother)** for research; 4+6 push toward Tier 4/5.

---

## See also
- [`README.md`](README.md) — the observability scale, the Orwell bar, and the three hook shapes this map applies.
- [`http-server.md`](http-server.md) — the live surface (`/players`, `/diagnostic/*`, `/events`) these hooks extend, and the live-read rules.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-obs-scale], [DEC-obs-hook-shapes], [DEC-map-before-delete].
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing for the #428/#430 hard-switch.
- [`../../README.md`](../../README.md) — the comprehension map (overview-of-overviews).
