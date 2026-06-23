# Specialist rebalance — plan

Tracking issue: **#317**. Owner framing (2026-06-11): specialists feel weak and don't really do
anything; identify and pull the levers that make them impactful. Closely tied to the emphasis
campaign (#367 — emphasis is the steering channel that pushes citizens INTO specialists) and the
city-owns-what-it-wants direction (`ai-architecture-north-star.md` §read-side declared-needs).

Distinguish **firm** (committed) from **fluid** (owner floated, to be designed — not locked).

## Why specialists are weak (audited 2026-06-11, all claims source-verified)

1. **Specialist yields bypassed every city yield modifier** (the mechanical core, FIXED — see
   step 1 below). `CvCity::getYieldRate100` multiplied tile yields by the full modifier stack
   (buildings/civics/traits/power, 150–300% late game) but added the extra-yield bucket flat.
   Specialist *commerce* (gold/research/culture/espionage) was always modified
   (`getTotalCommerceRateModifier`); the gap was yields — engineer hammers, slave food, etc.
2. **Raw data loses to tiles before modifiers**: Scientist 3 research / Engineer 2 hammers /
   Merchant 3 gold vs Village 0/3/9 → Town 0/4/11, and tiles additionally get the
   building→improvement yield bonuses.
3. **GPP — the compensation — inflates away**: threshold starts 100 and each GP adds
   `GREAT_PEOPLE_THRESHOLD_INCREASE(50) × (created/5 + 2)` percent (`CvPlayer.cpp:20443`);
   GPP rates are a flat 3 with no era scaling. GP #1 ≈ 50 specialist-turns, GP #5-6 ≈ 80+,
   late game effectively never.

Verified NOT the problem: AI valuation scale (`AI_specialistValue` returns ×100/×175, parity
with `AI_plotValue`'s ×100); slot caps (pop-cap dominated, ~900 SpecialistCount + ~220
FreeSpecialistCount building entries exist); the governor's weak emphasis obedience is its own
track (#367/#370).

## Step 1 — yield-modifier parity (firm, IMPLEMENTED with this doc)

**Owner ruling (2026-06-11, after first playtest):** the original decision to exclude
specialists from the city bonus/modifier stack **was a mistake** — parity is the intended
permanent design, not an experiment. Do not "restore" the flat behaviour; balance concerns are
addressed by tuning data (steps 2–4), never by re-removing the modifier. (Consistent with the
other found-disabled nerfs of the same vintage: the commented-out stronger emphasis weights in
`CvCityAI.cpp` ~10064–10110 and the "AI no longer uses emphasis" comment — these were balance
retreats, not design.) Extended playtests will tune the numbers on top of parity.

Specialist yields now receive the city yield modifier exactly like worked tiles. Mechanics:

- New tagged array `CvCity::m_aiSpecialistYieldTotal` accumulates the specialist share that
  previously sat inside `m_aiExtraYield`. Three writers redirected: per-assignment processing
  (`processSpecialist`), the civic/trait per-specialist extras (`updateExtraSpecialistYield`),
  and the incremental percent-change path (`CvPlayer::changeSpecialistYieldPercentChanges`).
  The flat extra bucket keeps corporations, per-building `BuildingYieldChange`s, flat building
  yields (`m_buildingExtraYield100`) and per-pop yields — multiplying THOSE is a separate,
  deliberate decision nobody has made (it would be an economy-wide change).
- `getYieldRate100` = `(base + specialistTotal) × modifier + flat extras`;
  `getProductionPerTurn` folds the specialist share into its modified term; the city-screen
  yield help moves the specialist lines into the multiplied base section; the three hurry/turns
  helpers in `CvGameTextMgr` include the new term.
- **Old saves:** the array is absent (loads 0) and specialists stay baked flat inside
  `m_aiExtraYield` until a **modifier recalculation** rebuilds both arrays — run one after
  loading a pre-change save. (`recalculateModifiers` zeroes and reprocesses everything.)
- AI: no valuation change needed — `AI_yieldValue` compares specialist and plot yields both
  unmultiplied, and the modifier now applies to both equally at realization.

## Step 2 — GPP rebalance (firm direction, numbers fluid)

Flatten the threshold escalation and/or era-scale the rates so the GPP half of a specialist's
value survives past the early game. Candidate knobs (pick after step 1 settles in play):

- `GREAT_PEOPLE_THRESHOLD_INCREASE` 50 → ~25, or cap the `(created/5 + 2)` factor.
- Era-scale `iGreatPeopleRateChange` in data (3 flat → 3/4/5/6…), or a player-level
  era multiplier in code.

## Step 3 — specialists as CITY AMPLIFIERS (fluid — owner's design, to be designed)

Owner idea (2026-06-11): specialists shouldn't just be tile-substitutes; they amplify the city
around them, so specialists and tiles SYNERGIZE ("one doesn't overtake the other"):

- **(a) Specialist → building percentage buffs.** Assigned specialists buff related buildings —
  either a city-wide buff that activates when the related building is present, or a buff to the
  building's own output (Scientists make the Library itself better).
- **(b) Specialist → plot yield buffs.** Specialists amplify worked-tile yields (an Engineer
  makes mines better, a Farmer-type makes farms better…).

Implementation guidance (from the 2026-06 infrastructure work):

- (b) rides the **shipped building→improvement-yields accumulation path** (per-city accumulation
  - upgrade-chain propagation) with a new source: specialist-grants-ImprovementYieldChanges.
- (a) should be shaped as a **conditional-effects block per #363** (condition: building present;
  scale: per assigned specialist of type X) — NOT a new bespoke 2D `int**` table (#196 lesson).
- Both must be visible to the governor's `AI_specialistValue` (the amplification IS the value)
  and invalidate the affected caches on assign/unassign (building values, yield caches —
  derived-data repository tenancy rules).

## Step 4 — data levers (fluid, cheap, after step 1 settles)

- **Regular buildings have ZERO `SpecialistYieldChanges` entries** (tag works; GreatWonders use
  it ~50×). Library/University/Lab → Scientist, Forge/Factory → Engineer, Market/Bank →
  Merchant, etc.
- Trait hooks exist but are all 0-valued; most civic hooks empty (Technocracy's +150/+200%
  scientist/engineer commerce shows the pattern works).
- Step 3(a) may supersede parts of this — don't double-spend the same flavor twice.

## Governor follow-through (tracked elsewhere)

[#367](https://github.com/Stones2Stars/S2S/issues/367)/[#370](https://github.com/Stones2Stars/S2S/issues/370): emphasis strength + forced-specialist semantics (specialist emphasis is currently a
1.75× nudge with no forcing link — `AI_setEmphasizeSpecialist` never touches
`ForceSpecialistCount`); #368 missing emphasis buttons; #369 building values × emphasis.

## Cleanup noted during the audit

`bSlave` is read from XML but consumed nowhere; specialist insidiousness/investigation only feed
spy-unit crime math; `CvCity.cpp:5145` comment documents the stale-cached-commerce-modifier
quirk (percent modifiers baked at assignment time). Route via the dead-code pass / bug backlog.
