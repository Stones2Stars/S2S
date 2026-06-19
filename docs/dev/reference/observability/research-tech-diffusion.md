> DRAFT observability map (2026-06-18, parent agent cascade-mapping sweep) — all claims cited from
> live code; verify line numbers before relying (they drift). Written as input to the #428/#430
> hard-switch observability bar: every gap = something the cascade replacement cannot yet be
> verified against from outside.

# Observability Map — Research, Beakers & Tech Diffusion

**System scope:** per-player beaker accrual, tech cost modifiers (handicap/era/gamespeed/
diffusion/win-for-losing/national), the per-team research progress ledger, tech completion
and overflow, the AI research-choice decision, and the special diffusion mechanics
(`GAMEOPTION_TECH_DIFFUSION` + `GAMEOPTION_TECH_WIN_FOR_LOSING`).

**Current tier: 1 (Telescreen).** Coarse snapshots are available (`/players` exposes
`research`, `techs`, `scienceRate`). No per-turn beaker movement, no progress fraction,
no modifier breakdown, no diffusion input trace, no tech-completion event on the SSE stream.

---

## 1. How It Actually Works

### 1a. Per-turn beaker accrual (`CvPlayer::doResearch`)

Called once per player in `CvPlayer::doTurn` (CvPlayer.cpp:3811). NPC players are skipped
immediately (CvPlayer.cpp:15510: `if (isNPC()) return`).

**Step 1 — path-length cache invalidated** (CvPlayer.cpp:15512-15516): every tech's
`m_aiPathLengthCache` and `m_aiCostPathLengthCache` reset to -1 each turn.

**Step 2 — AI queue refill if empty** (CvPlayer.cpp:15520-15535): if `getCurrentResearch()`
returns `NO_TECH` and not an NPC, `AI_chooseResearch` is called (for human players: only
when `getElapsedGameTurns() > 4`).

**Step 3 — beaker calculation** (CvPlayer.cpp:15537-15547):
- If `eCurrentTech == NO_TECH` (human hasn't chosen yet): overflow accumulates at
  `calculateResearchRate()` with a negative modifier — `changeOverflowResearch(getModifiedIntValue(calculateResearchRate(), -calculateResearchModifier(NO_TECH)))`.
- Otherwise: `GET_TEAM(getTeam()).changeResearchProgress(eCurrentTech, calculateResearchRate(eCurrentTech) + getModifiedIntValue(iOverflow, calculateResearchModifier(eCurrentTech)), getID())` — overflow from the previous turn is drained and applied (with modifier) in the same call.

### 1b. Beaker rate components

`calculateResearchRate(TechTypes eTech)` (CvPlayer.cpp:8234):
- If `isCommerceFlexible(COMMERCE_RESEARCH)`: returns `calculateBaseNetResearch(eTech)` directly.
- Otherwise (fixed slider): returns `max(1, calculateBaseNetResearch(eTech) + calculateBaseNetGold())` — the gold-research split when the slider is locked.

`calculateBaseNetResearch(TechTypes eTech)` (CvPlayer.cpp:8203):
```
BASE_RESEARCH_RATE + getCommerceRate(COMMERCE_RESEARCH)
  modified by (getNationalTechResearchModifier(eTech) + calculateResearchModifier(eTech))
  clamped to MAX_RESEARCH_RATE_VALUE
```
where `BASE_RESEARCH_RATE` is a GlobalDefines int and `getCommerceRate(COMMERCE_RESEARCH)`
is the player's raw beaker commerce sum (already includes city yields, buildings, civics).

`getNationalTechResearchModifier(TechTypes)` (CvPlayer.cpp:29960): a per-tech per-player
percentage modifier stored in `m_paiNationalTechResearchModifier[eTech]` — set by events and
other sources; not widely used in vanilla content but exists as a hook.

`calculateResearchModifier(TechTypes eTech)` (CvPlayer.cpp:8084) — the two "difficulty
diffusion" modifiers, both GAME-OPTION GATED and both skip human players when
`GAMEOPTION_TECH_NO_HANDICAPS_FOR_HUMANS` is on:

**Win-for-Losing** (`GAMEOPTION_TECH_WIN_FOR_LOSING`, CvPlayer.cpp:8093-8097):
```
modifier += GET_TEAM(getTeam()).getWinForLosingResearchModifier()
  = 100 - (100*ownCities/topCities + 100*ownPop/topPop) / 2
```
Gives a percentage bonus to research for smaller/weaker civs, derived entirely from city
count and population (CvGame.cpp:11902-11905). Both inputs are available from `/players`.

**Tech Diffusion** (`GAMEOPTION_TECH_DIFFUSION`, CvPlayer.cpp:8099-8194):
Computed via `knownExp` (a floating-point accumulator):
- For every alive met team that has `eTech`:
  - +0.5 base
  - +1.5 if open borders OR vassal relationship
  - +0.5 if at war or own team is vassal
- Speed tier based on adoption fraction:
  - If fewer than 1/3 of teams have the tech: `knownExp /= 100` (slow)
  - If more than 2/3 have it: `knownExp *= 3` (fast)
- Scaled by `(iMetTeams / iTeams)`
- Final: `iTechDiffusion = max(0, TECH_DIFFUSION_KNOWN_TEAM_MODIFIER - TECH_DIFFUSION_KNOWN_TEAM_MODIFIER * pow(0.85, knownExp))`

**Welfare sub-modifier** (CvPlayer.cpp:8162-8194): if `getBestKnownTechScorePercent() < TECH_DIFFUSION_WELFARE_THRESHOLD`:
```
iWelfareTechDiffusion = max(0, WELFARE_MOD - WELFARE_MOD * pow(0.98, threshold - percent))
  then multiplied by (bestPlayerScore / ownScore)
  then added to iModifier
```

Both diffusion values are logged to `C2C.log` via `logging::logMsg` (CvPlayer.cpp:8158, 8188)
— **NOT through the tagged logger (`logDecisionAI`/`logPlayerAI`), so NOT streamed via `/events`**.

**Cap:** `iModifier = min(iModifier, 100)` (CvPlayer.cpp:8199) — diffusion bonus can never
exceed 100% (i.e. cannot more than double your base research rate from diffusion alone).

### 1c. Tech cost (per-team, CvTeam::getResearchCost)

`CvTeam::getResearchCost(TechTypes eTech)` (CvTeam.cpp:2584). Multiplies the XML base cost
through the following chain (all in integer arithmetic with intermediate × 100 / 100 steps):

1. `TECH_COST_MODIFIER` (global define %)
2. `GameSpeedInfo::getSpeedPercent()` (game speed)
3. `EraInfo::getResearchPercent()` for the tech's era
4. `TECH_COST_EXTRA_TEAM_MEMBER_MODIFIER * getNumMembers()` (team-size penalty)
5. AI handicap reduction: `AIResearchPercent - 100 + AIPerEraModifier * currentEra` (for non-human non-NPC teams; CvTeam.cpp:2651-2658)
6. `GAMEOPTION_TECH_CUTTING_EDGE_CUTS` modifier (if player has fewer than 3 peers in the same era and the tech is from a past era; CvTeam.cpp:2606-2635)
7. `GAMEOPTION_TECH_UPSCALED_COSTS`: adds `UPSCALED_RESEARCH_COST_MODIFIER` %

Result clamped to `max(1, ...)`. The AI handicap reduction (step 5) is what makes AI research
cheaper/faster at higher difficulty settings — **it applies to the team, not per-player**, and
is entirely invisible outside (no endpoint field, no log).

### 1d. Research progress ledger (per-team)

Stored in `CvTeam::m_paiResearchProgress[TechTypes]` (CvTeam.cpp:4723). Changed by
`changeResearchProgress` (CvTeam.cpp:4770) → `setResearchProgress` (CvTeam.cpp:4733).

`setResearchProgress` fires tech-completion when `iNewValue >= getResearchCost(eIndex)`:
- Calls `setHasTech(eIndex, true, ePlayer, true, true)` (CvTeam.cpp:4751)
- Then `doMultipleResearch` to consume overflow into the next queued tech (CvTeam.cpp:4756-4763)

Tech completion triggers `CvEventReporter::getInstance().techAcquired(...)` (CvTeam.cpp:5323)
— **Python-only callback; NOT published on the SSE `/events` stream.**

### 1e. Overflow and multi-tech completion (`CvPlayer::doMultipleResearch`)

`doMultipleResearch(int iOverflow)` (CvPlayer.cpp:26800): loops while the overflow (adjusted
by `-calculateResearchModifier`) covers the remaining cost of the current queued tech. Each
iteration calls `setHasTech` directly (not via `changeResearchProgress`) and the AI refills its
queue via `AI_chooseResearch`. Repeat techs (future tech) break the loop immediately
(CvPlayer.cpp:26824). Returns unused overflow, which is stored back into `m_iOverflowResearch`.

### 1f. Barbarian / hominid free-tech accrual

`CvTeam::doTurn` (CvTeam.cpp:1015-1050): for `isHominid()` teams, each adjacent researchable
tech receives free beakers proportional to how many PC teams already have it:
`max(1, cost * BARBARIAN_FREE_TECH_PERCENT * count / (100 * possible))` (CvTeam.cpp:1046).
This bypasses the per-player `doResearch` path entirely — no player logging applies.

### 1g. AI research choice (`CvPlayerAI::AI_chooseResearch`)

Called from `doResearch` when the queue is empty (CvPlayer.cpp:15531) and from
`doMultipleResearch` when a tech completes (CvPlayer.cpp:26811, 26838).

Logic (CvPlayerAI.cpp:6259-6290):
1. Clears queue.
2. If teammates are already researching something this player can research, piggybacks on them.
3. Otherwise calls `AI_bestTech(depth)` (depth=1 for human+Culture3-victory; depth=3 normally).
4. Pushes the result with `pushResearch`.

`AI_bestTech` emits `[DAI/tech/best]` at level 1 and `[DAI/tech/cand]` (flavor breakdown)
at level 3 — both via `logDecisionAI` (CvPlayerAI.cpp:4130, 5335), so they appear in
`DecisionAI.log` and on `/events` at the corresponding log level.

### 1h. Team-merge research progress inheritance

During permanent alliance (`CvTeam::copyFrom`, CvTeam.cpp:973-989): if the joining team's
progress on a tech exceeds the receiving team's, the receiving team's progress is bumped up.
No logging.

---

## 2. Current Observability

**Tier: 1 (Telescreen)**

| What | Source | Exposed via | Notes |
|---|---|---|---|
| Current research tech (name/key) | `CvPlayer::getCurrentResearch()` | `/players` → `research` field | XML key string, e.g. `TECH_METAL_CASTING` |
| Tech count (how many techs the team has) | snapshot from `m_paiTechCount` | `/players` → `techs` field | **team-shared count** on the player snapshot |
| Raw beaker rate (`COMMERCE_RESEARCH`) | `getCommerceRate(COMMERCE_RESEARCH)` | `/players` → `scienceRate` | Pre-modifier, pre-tech-specific-modifier; NOT the actual per-turn deposit |
| Era index | `getCurrentEra()` | `/players` → `era` | Needed to reconstruct AI handicap scaling |
| City count | `getNumCities()` | `/players` → `cities` | Input to WinForLosing calculation |
| Population | `getTotalPopulation()` | `/players` → `population` | Input to WinForLosing calculation |
| AI tech-choice decision (target + path start) | `[DAI/tech/best]` (level 1) | `DecisionAI.log` + `/events` stream | Only at decision time, not every turn |
| AI tech flavor breakdown | `[DAI/tech/cand]` (level 3) | `DecisionAI.log` + `/events` stream | Very verbose; level 3 only |
| Handicap (per-player difficulty) | snapshot | `/players` → `handicap` | AI cost reduction is a function of this + era |

**Not exposed — complete gaps:**

| What is missing | Why it matters | Where it lives |
|---|---|---|
| Research progress fraction (beakers accumulated / cost) | Cannot say how far through a tech any player is | `CvTeam::m_paiResearchProgress[eTech]`, `CvTeam::getResearchLeft(eTech)` |
| Actual per-turn beaker deposit (post-modifier rate) | `scienceRate` ≠ `calculateResearchRate(eTech)` — the actual deposit includes diffusion+WFL bonus AND the tech-specific national modifier AND the overflow carryover | `CvPlayer::calculateResearchRate(eTech)` |
| Overflow research (carried from previous tech) | Invisible; affects turn-count math; can cascade a second tech in the same turn | `CvPlayer::m_iOverflowResearch` |
| Tech diffusion modifier value | How large the diffusion bonus is this turn for each player × each tech | `CvPlayer::calculateResearchModifier(eTech)` result |
| Win-for-Losing modifier value | How large the WFL catch-up bonus is for a trailing civ | `CvTeam::getWinForLosingResearchModifier()` |
| National tech research modifier | Per-player per-tech modifier from events/buildings | `CvPlayer::m_paiNationalTechResearchModifier[eTech]` |
| Tech cost (computed, not XML base) | Full modifier chain (handicap/era/gamespeed/cutting-edge/upscaled) — cannot reconstruct turns-to-tech | `CvTeam::getResearchCost(eTech)` |
| Tech completion event | Cannot detect WHEN a tech completed, nor WHICH player's overflow triggered it | `CvTeam::setHasTech` → Python `techAcquired` only |
| AI handicap cost reduction in effect | Invisible as a separate value; baked into `getResearchCost` | `CvHandicapInfo::getAIResearchPercent()` + per-era modifier |
| Diffusion inputs (open borders, adoption fraction, met-teams ratio) | Cannot reconstruct the diffusion modifier without knowing team relationships | `CvTeam::isOpenBorders`, `CvTeam::isHasTech`, team-alive counts |
| Diffusion log to C2C.log | Not in tagged-logger family; doesn't stream via `/events` | `logging::logMsg("C2C.log", ...)` at CvPlayer.cpp:8158,8188 |
| Research queue (ordered list of techs after the current one) | Cannot see the AI's queued tech path | `CvPlayer::headResearchQueueNode()` |
| Multi-tech completion (two techs completed in one turn via overflow) | Invisible — the first completion fires via `setResearchProgress`; subsequent via `doMultipleResearch` directly via `setHasTech` (bypasses `changeResearchProgress`) | CvPlayer.cpp:26820-26841 |
| Barbarian/hominid free-tech accrual | Separate path through `CvTeam::doTurn`; no player-level logging | CvTeam.cpp:1026-1050 |
| `getBestKnownTechScorePercent` (welfare threshold input) | Needed to reconstruct welfare branch of diffusion — requires per-player `getTechScore`, not just raw tech count | `CvTeam::getBestKnownTechScorePercent()` |

---

## 3. The Gap

The system cannot be reconstructed from outside on any but the coarsest level:

- **The "how far along" question is unanswerable:** no endpoint gives `researchProgress` or
  `researchLeft`. You can see *what* a player is researching but not how far through it they
  are. For AI players this is completely opaque.

- **The "why is it going faster/slower" question is unanswerable:** the actual deposit
  per turn (`calculateResearchRate`) is not published. `scienceRate` only gives the raw
  commerce rate before tech-specific modifiers. The diffusion and WFL modifier values are
  unlogged (they go to `C2C.log` via `logging::logMsg`, not the tagged logger, so they
  don't reach `/events`).

- **Tech completions are invisible on the wire:** there is no SSE event for "tech acquired".
  The Python `techAcquired` callback fires but has no bridge to the SSE stream. An observer
  must poll `/players` → `techs` count or `research` field to infer a tech completed.

- **Overflow and queue are invisible:** cannot reconstruct whether a tech completed mid-turn
  from overflow, nor what the AI's queued path is, nor how much overflow is being carried.

- **The diffusion model is partially reconstructible but tediously so:** all the inputs
  (`isOpenBorders`, `isHasTech` per team, team counts) exist in principle but are scattered
  across `/players`-team relationships that are not directly exposed. The actual computed
  modifier is nowhere.

---

## 4. Proposed Hooks

All hooks are additive (no changes to existing code), gated (off-state cost = one int compare),
and follow the existing `logDecisionAI` / `logPlayerAI` + `streamLogTee` pattern so they reach
both the file log and `/events` automatically.

### Hook 1 — `/players` snapshot additions (3 fields)

Add to `PlayerSnap` and the `renderPlayers` serializer in `CvHttpServer.cpp`:

- **`researchProgress`** (int): `GET_TEAM(kPlayer.getTeam()).getResearchProgress(kPlayer.getCurrentResearch())` — beakers accumulated on the current tech. 0 when `research` is `"NONE"`.
- **`researchCost`** (int): `GET_TEAM(kPlayer.getTeam()).getResearchCost(kPlayer.getCurrentResearch())` — the actual (modifier-applied) cost. 0 when no tech.
- **`overflowResearch`** (int): `kPlayer.getOverflowResearch()` — overflow carried from the previous completed tech.

These three together let an observer compute `%complete`, `turnsLeft` (with `scienceRate`),
and predict multi-tech completions. They are cheap (two inline lookups into the team's
`m_paiResearchProgress` array).

### Hook 2 — `[RES/turn]` per-player per-turn research log (new `[RES]` domain)

New log domain `[RES]` in `BetterBTSAI.{h,cpp}`, scope global `gPlayerLogLevel`,
file `ResearchAI.log`. One line per player per turn in `doResearch`:

```
[RES/turn] turn=N player=P tech=TECH_X deposit=D overflow=OVF modifier=M pct=PP%
           progress=PROG cost=COST left=LEFT turnsLeft=T
```

- `deposit` = `calculateResearchRate(eTech)` — what actually was banked.
- `overflow` = the overflow that was applied this turn (pre-consumed).
- `modifier` = `calculateResearchModifier(eTech)` — the total diffusion+WFL+national mod %.
- Fields match `key=value` convention for grep recipes.
- Level 1: just `deposit`, `modifier`, `pct` (completion percent), `turnsLeft`.
- Level 2: full line including `progress`, `cost`, `left`.

This is the most valuable single hook: it makes the per-turn beaker movement fully visible
for all players simultaneously.

### Hook 3 — `[RES/tech]` tech completion event (same `[RES]` domain)

In `CvTeam::setHasTech` at the point `iNewValue >= getResearchCost(eIndex)`, before calling
`setHasTech` proper (CvTeam.cpp:4749), emit:

```
[RES/tech] turn=N team=T player=P tech=TECH_X overflow=OVF cost=C progress=P
```

- `overflow` = `iNewValue - getResearchCost(eIndex)` (raw; before modifier un-application).
- Level 1 always (tech completions are always headline events).
- This also fires for multi-tech completions in `doMultipleResearch` since they call
  `setHasTech` (CvTeam.cpp:26830) — that path should log identically so the multi-tech
  chain is reconstructible from the log.

### Hook 4 — `[RES/diffusion]` diffusion modifier trace (level 2)

In `calculateResearchModifier`, after computing `iTechDiffusion` and `iWelfareTechDiffusion`,
replace the two `logging::logMsg("C2C.log", ...)` lines with (or add alongside them):

```
[RES/diffusion] turn=N player=P tech=TECH_X knownExp=E teamsWithTech=K metTeams=M
                allTeams=T diffusion=D welfare=W totalMod=M
```

- Gate at level 2 (one line per player per tech being researched per turn — moderate volume).
- This bridges the un-tagged `C2C.log` path into `/events`, making diffusion per-player
  per-tech visible on the wire.

### Hook 5 — `[RES/queue]` research queue log at level 2

In `AI_chooseResearch` after `pushResearch`, emit:

```
[RES/queue] turn=N player=P queue=[TECH_A, TECH_B, TECH_C]
```

(Iterate `headResearchQueueNode()` to build the list.) Lets an observer know the AI's
full research path, not just the current tech.

### Hook 6 — `GET /diagnostic/researchState?player=N` endpoint

New diagnostic endpoint (no game-thread calculation needed — pure snapshot read):

```json
{
  "player": N,
  "research": "TECH_X",
  "progress": 1234,
  "cost": 2500,
  "left": 1266,
  "pctComplete": 49,
  "researchRate": 87,
  "scienceRate": 70,
  "modifier": 24,
  "overflowResearch": 0,
  "turnsLeft": 15,
  "winForLosingMod": 12,
  "techDiffusionMod": 12
}
```

Fields: `calculateResearchRate(eTech)`, `calculateResearchModifier(eTech)`,
`getWinForLosingResearchModifier()`, etc. — all on-demand, game-thread-serviced (same
mailbox pattern as existing `/diagnostic/*` endpoints), no snapshot required.

This is the single-shot "what is going on with this player's research right now" query,
equivalent to what the in-game tech advisor shows but accessible from outside for all players.

---

## 5. Priority Order

1. **Hook 1** (snapshot fields) — zero new logging infrastructure, pure snapshot additions.
   Immediate climb to mid-Tier-2 for research observability.
2. **Hook 3** (`[RES/tech]` tech-complete event) — the most critical gap for shadow testing:
   without it, an observer must poll to detect tech completions.
3. **Hook 2** (`[RES/turn]` per-turn line) — makes the beaker ledger fully reconstructible
   turn-by-turn; needed to shadow the cascade's research-rate evaluation.
4. **Hook 6** (diagnostic endpoint) — convenient on-demand spot-check; depends on nothing else.
5. **Hook 4** (`[RES/diffusion]` trace) — closes the untagged-C2C.log gap; needed for Tier 5
   (total observability of the diffusion mechanics).
6. **Hook 5** (queue log) — lower priority; AI research queue is QoL for narrating AI decisions.

Hooks 1+3+2 together bring the system to **Tier 3 (Big Brother)** for research: snapshot (T1),
per-turn movement visible (T2 → T3 boundary), and tech-completion events on the wire (T3).
Hooks 4+6 push toward **Tier 4/5** (diffusion internals reconstructible, full state queryable).
