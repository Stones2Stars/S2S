# Culture, religion, research & corporations reference

> Lifted + condensed mechanics (the formulas the validator re-derives). Behaviour as-is; the cascade replaces these
> (verified live in-game).

## Culture (city + plot)

- **Accrual:** `doCulture` adds `getCommerceRateTimes100(CULTURE)` to `m_aiCulture[owner]`; `doPlotCulture` spreads
  to a Chebyshev square of radius `cultureLevel`, linear dropoff via `CITY_CULTURE_DENSITY_FACTOR` (min 1).
  Improvement culture radiates **flat** within `getCultureRange()`.
  > **⛔ CULTURE IS 64-BIT, AND THE REASON IS A BUG THAT WAS LIVE IN SHIPPING SAVES (owner: late-game overflow is
  > real).** City culture accumulates the ×100 rate every turn and **never decays**, so on a long game it wrapped
  > `int`; plot culture only decays *proportionally*, so it settles at an equilibrium that scales with the city
  > feeding it rather than being bounded. Both are `int64_t`, per-player, at both scopes
  > ([fixed-point-and-scales.md §1b](../specs/curators/fixed-point-and-scales.md): an AMOUNT accumulates, so it
  > carries 64 bits).
  > ⚑ **It degraded SILENTLY, which is why it survived:** the getters carried `< 0 ? MAX_INT` guards that detected
  > the wrap and saturated, so a pinned city kept returning a plausible number. Everything downstream then read a
  > total that was not the total — `calculateCulturePercent`, `calculateCulturalOwner`, the level thresholds.
  > ⚠ **CONSEQUENCE ON AN EXISTING SAVE (owner) — recorded so it is not mistaken for a regression:** a city that
  > had pinned now reports its real culture, which shifts **tile ownership and revolt risk** the first time such a
  > save is loaded. That is the fix landing ([validation.md](../specs/validation.md): an intentional divergence is
  > named and shown, never a mystery).
  > ⛔ The per-PLAYER dimension is load-bearing and is not a candidate for "simplifying" to one total: cultural
  > OWNERSHIP is a contest, so every consumer asks how much culture a SPECIFIC player has here. A single number
  > would say a place has culture without saying whose.
- **Decay:** `max(0, culture·(1000−decayPermille)/1000)`, `decayPermille = TILE_CULTURE_DECAY_PERCENT·1000/speedPercent`;
  ×15 when the plot is out of any city's culture range; a value >1 cannot decay below 1.
- **Ownership** (`calculateCulturalOwner`): keep the current owner when `hasFixedBorders() &&
  culture(owner)·FIXED_BORDERS_CULTURE_RATIO_PERCENT/100 ≥ culture(highest)`. `GAMEOPTION_CULTURE_MIN_CITY_BORDER`:
  a plot adjacent to a city goes unconditionally to that city's owner.
- **Revolt:** `baseRevoltRisk100 = highestPop·2 + (era+1)·adjacentAttackerTiles`, × `10000·attackerPct/max(1,defenderPct)`,
  × garrison modifier `10000/(100+garrison)`. **Two rolls:** `rand(100) < REVOLT_TEST_PROB`, then `rand(10000) <
  cityStrength100`. Permanent flip needs `numRevolts(attacker) ≥ NUM_WARNING_REVOLTS`. Fort revolt: an `isActsAsCity`
  improvement owned past `SUPER_FORTS_DURATION_BEFORE_REVOLT`, a different cultural owner, no defenders → immediate flip.

## Religion spread

- Passive spread/decay: **one religion per `doCorporation` call per turn** (break after the first fires); gated by
  `MODDERGAMEOPTION_RELIGION_DECAY` / `_MULTIPLE_RELIGION_SPREAD`.
- **Spread:** `iRandThreshold = max(iSpread)` — the best single source, **NOT a sum**. Foreign distance penalty
  `iSpread /= (count+1)·max(1, RELIGION_SPREAD_DISTANCE_DIVISOR·dist/maxDist − 5)`; local `2·iSpread/(count+1)`. Roll
  `rand(RELIGION_SPREAD_RAND·speedPercent/100)`.
- **Decay:** `iDecay = getSpreadFactor() + (count−2)²`; ×4/3 at war with the holy city, ÷2 for the holy-city owner;
  exempt: state religion, holy city, sole religion. On decay, all `getPrereqReligion` buildings are forcibly removed.
  Holy-city influence = `HOLY_CITY_INFLUENCE` (the spread-source weight). Missionary: `iSpreadProb = getReligionSpreads`,
  ÷2 into a foreign-team city, + an empty-slot bonus; the unit is always killed.

## Research & tech diffusion

- **Beaker deposit:** `calculateResearchRate + getModifiedIntValue(overflow, researchModifier)` → `m_paiResearchProgress`.
  `baseNetResearch = BASE_RESEARCH_RATE + getCommerceRate(RESEARCH)`, × `(nationalTechResearchModifier + researchModifier)`,
  clamped `MAX_RESEARCH_RATE_VALUE`.
- **Diffusion:** `knownExp` += 0.5 per met team holding the tech (1.5 if open-borders/vassal); speed tiers (`/100` if
  `teamsHaveTech·3 < teams`, `×3` if `·3 > 2·teams`), scaled by metTeams/teams. `iTechDiffusion = max(0,
  KNOWN_TEAM_MODIFIER·(1 − 0.85^knownExp))`, **capped at 100** (can't more than double base). Welfare branch when
  `bestKnownTechScorePercent < TECH_DIFFUSION_WELFARE_THRESHOLD`.
  > ⚑ `calculateResearchModifier` emits **no logging** (an old map wrongly claimed it logged to `C2C.log`).
- **Tech cost** (per team) = XML base × `TECH_COST_MODIFIER` × speed × era-research% × team-member modifier ×
  AI-handicap reduction. Barbarian free-tech is a separate `CvTeam::doTurn` path (bypasses `doResearch`).

## Heritage & score

- **Heritage** is a permanent player flag (`m_myHeritage`, no removal path). Prereqs: `needLanguage()` (a tech with
  `isLanguage`), `getPrereqTech()`, `getPrereqOrHeritage()` (OR-list of held heritages). **Commerce:**
  `getEraCommerceChanges100` (an era→commerce map) stacks for all eras ≤ current, applied as a flat empire-wide
  `extraCommerce100`; on era advance the delta is applied. Acquired by `MISSION_HERITAGE` (the unit is consumed).
  Cascade atom `ATOMDOMAIN_HERITAGE` → `hasHeritage`.
- **Score** (Python `CvGameUtils.py`): `Σ FACTOR·(score+free)/(free+max)` over pop/land/tech/wonders; tech weight
  `1 + era`, wonder weight 6 (limited) / 1; computed in the frame loop (`CvGame::update`), not `doTurn`.

## Corporations

- Two modes: **Classic** (manual unit spread only; `doCorporation` returns immediately) vs **Advanced**
  (`GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS`; autonomous per-turn spread/decay).
- **Spread:** `iRandThreshold = max over connected cities of corpInfluence·getSpread/100 / distance`, × player
  modifier × owner influence ÷ `(1 + count/2)`; roll `rand(CORPORATION_SPREAD_RAND·speedPercent/100)`. **Influence**
  = `100 + prereqBonusInstances + CORPORATION_RESOURCE_BASE_INFLUENCE/bonusesConsumed` per prereq, ÷10 per competing
  corp, × pop/avgPop.
- **Dormancy gate** (city): `isHasCorporation && isActiveCorporation && !tech-obsolete && ≥1 prereq bonus present`.
  Going inactive: yields/commerce → 0; `getPrereqCorporation` buildings disabled. **Maintenance** = `Σ
  headquartersCommerce + perBonusRate·worldSize/100`, × `(17+pop)/18`, × handicap (²/8000 Advanced);
  rebels pay 50%.
  ⚑ **The per-bonus rate and the owned-bonus COUNT are ONE authored deposit, never two reads.** The rate carries
  a `per:{anyOf: consumed bonuses}` scaler ([json.md §3.7](../specs/json.md)), so the valuation resolves the rate
  and the city's count of those bonuses together — summing the count by hand re-implements the scaler the entry
  already states. The corp's active gate rides the same entry's `{HAS_CORPORATION: SELF}` condition. Its set half
  (which bonuses, for the spread/dormancy gate) is `getConsumedBonuses()`, the union of those `per.anyOf` ids.
  > **⚖ Cascade boundary (owner ruling): corp active/dormant is ENGINE-DRIVEN SPREAD STATE — an engine-owned INPUT the
  > modifier cascade READS, never a cascade-computed dormancy verdict.** Corporations spawn and spread themselves per
  > turn like religion (autonomously under `GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS`), so "is this corp active in
  > this city?" is asked of the engine (`isActiveCorporation`) — the same sanctioned class as religion presence and the
  > connected-bonus network, NOT the [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in) trap
  > (which bans reading a *cascade-owned* computed verdict — above all a BUILDING's active/dormant, which the cascade
  > DOES own via `cascadeIsBuildingActive`). Hence `{HAS_CORPORATION:X}` = ACTIVE ([enabler §3](../specs/enabler.md),
  > [json §3.5](../specs/json.md)), and the corp-commerce deposit gate reading `isActiveCorporation` is correct, not
  > interim debt.
- **⚖ A CORPORATION CAN BE OBSOLETED, and the capability is KEPT (owner) — it is HEADROOM, not dead surface.**
  The chain is wired end to end and needs no code to activate: a tech authoring `obsoletes.corporations` lands on
  its `EDGEF_OBSOLETES`/`EDGEB_CORPORATIONS` edge, the readJson reverse pass stamps the corporation's obsoleting
  tech from exactly that edge, and every consumer already reads it — the city dormancy gate above, the
  spread/found gates, the team's obsolete-tech sweep that strips the corp from its cities, the pedia line, and
  the AI's tech valuation.
  ⚠ **ZERO corporations author one today — in the curated JSON *and* in the legacy XML, which carries no
  `ObsoleteTech` tag for the type at all** — so the gate is inert and the AI term contributes nothing. ⛔ That is
  NOT a data gap to fill and NOT a reason to purge the member as unused: the ability is wanted, so the check
  stays and lights up the moment data authors one. (Same shape as the capability lapse-with-source semantic in
  [capabilities.md](../specs/capabilities.md) — headroom the model carries for free.)

## See also

- [economy.md](economy.md) · [engine.md](engine.md) (the property solver — corporations ride it) ·
  [../specs/enabler.md](../specs/enabler.md) (the resource/tech dormancy gates) · [../specs/modifier.md](../specs/modifier.md).
