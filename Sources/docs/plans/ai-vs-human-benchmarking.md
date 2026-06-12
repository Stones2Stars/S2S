# AI-vs-human benchmarking — live playthrough observation

**Goal (owner, 2026-06-11).** Determine empirically whether the AI *runs ahead of* or *lags
behind* a human player over a real game — and use what the gap looks like to drive AI
improvements. The owner plays a normal game; agents observe every civ's trajectory through
the live instrumentation and compare.

**Design principle (owner ruling 2026-06-11): challenge through competence, not handicaps.**
The goal of AI work is an AI that is *better and more challenging to play against* because it
makes better decisions — NOT because its numbers are scaled "to infinity" (production/research
bonuses, free units, cost discounts). Difficulty-style numeric bonuses are a last resort, not
the improvement lever. When a benchmark shows the AI lagging, the response is a competence fix
(valuation, coordination, production choice), not a multiplier.

**Equally valid output: game-balance findings (owner, 2026-06-11).** Not every observed AI
failure is an AI bug. "Animals are simply too strong too early" is a legitimate benchmark
conclusion — nothing requires the AI to magically beat a strength-9 rhino with strength-2
hunters. When the data shows *no* good decision exists (e.g. most wildlife unattackable at
any sane odds gate with era-available units), the fix is content/balance tuning (animal
strength curve, spawn pressure, unit lines), not AI code. Each finding should be explicitly
binned: **competence** (a better decision existed) vs **balance** (no better decision existed).

## Method

Observation runs on shipping FinalRelease infrastructure (no FASSERT there — see `AGENTS.md`):

- **`/units` HTTP census** (`CvHttpServer`, 127.0.0.1:7227) — per-unit owner/type/UNITAI/
  missionAI/activity/damage/level, plus current turn. The live "what is every unit doing" view.
- **Playtest identity:** both endpoint wrappers carry `gameId` = `CvGame::getGameId()` —
  the id stamped at game creation and persisted in every save of that game (pre-existing
  `m_gameId` field; generator reformatted 2026-06-11 to digits-only local yyMMddHHmm, e.g.
  `2606112045`; older saves carry `DD-MM-YYYY HH:MM:SS`). Collector CSVs record it per row,
  so multiple playtests can share analysis tooling and reloads/new games are detectable.
- **`PlotSnapshot_*.csv`** (`Sources/Utils/PlotSnapshot.h`) — per-plot terrain/improvement/
  owner/city/units/animal tokens (incl. animal strength `c` and aggression `a`). `start_t0`
  vs rotating `turn_tN` files give territory/improvement/city/wildlife trends.
- **Collector script** (`Tools/BenchmarkCensusCollector.ps1`) — a detached PowerShell loop
  polling both every ~60s into a CSV time series (one row per observed game turn): civ unit
  totals, hunters (+escorts, +on-HUNT), cities (civ/NPC), ruins, land/water animal counts,
  avg land-animal strength, aggressive share, barb+neanderthal units, per-owner breakdowns.
  Per-game output lands in `Benchmarks/` — see `Benchmarks/README.md` for the folder naming
  and contents conventions (data is gitignored; conventions are tracked).
- The human player's civ is identified per game and **excluded from AI-health conclusions**.
- **Record the difficulty, and normalize for it (owner, 2026-06-11).** Handicap bonuses
  mean an AI lead is partly difficulty-assisted (the 2026-06-11 game runs on Emperor) —
  the competence question is whether the AI *converts* its head start into a durable lead,
  not whether it leads. `/players` carries per-player `handicap` (XML key) and the
  collector logs it per row, so cross-game comparisons can group by difficulty.

Per-decision *why* (production choices, tech picks) needs the gated Autolog channels flipped
on for a stretch of turns — counts say *what* diverged, logs say *why*.

## Open metric gaps

~~The census exposes units only.~~ **`/players` SHIPPED (2026-06-11):**
per alive player score/era/techs/current research/cities/population/units/gold(+rate)/
scienceRate/production via `CvHttpServer` (`?playerNumber=N` filter; picojson-rendered).
This is the progression view that answers "ahead or behind" directly — the collector
logs it per turn alongside the unit census. **`/cities` SHIPPED (same day):** per-city
yields, production head, buildings, culture level, capital, and the crime/education/
disease property values (owner ruling: those three carry real gameplay; flammability and
pollution are dormant) — collector logs `cities_timeseries.csv`. Remaining gaps:
diplomacy state, and per-decision *why* (Autolog channels).

## Early findings (playthrough started 2026-06-11; prehistoric era, 15 civs, owner=England)

- **Intra-AI variance dwarfs the AI-vs-human gap.** Same era, same wildlife pressure: one AI
  (Aachen) built a 22-unit military and 3–4 hunters by t170 while two others (Paris, Qosqo)
  repeatedly bled to 1 unit; one AI civ died before t84. Whatever separates the healthy AI
  from the dying ones is the highest-value competence lever this benchmark has surfaced.
- **Hunter attrition ≈ production.** Global hunter count treads water (10–17) against an
  `AI_neededHunters` target of ~11/civ; the limiter is the production ladder + field deaths,
  not the target. ~56% of land animals are at/above hunter base strength (avg 3.1, 74%
  aggressive), so the 70%-odds engage gate (`CvHunterAI.cpp`) correctly keeps hunters passive
  — count alone won't dent the animal cap until hunter *strength* improves.
- **Balance finding: barbarian criminals predate their counter (owner, t~222).** Barb
  `UNIT_THIEF` (str 3, `INVISIBLE_CAMOUFLAGE`, enabled by `TECH_THEFT`, tech-grid x=10)
  swarm civ cities (census t222: 7 barb thieves at distance 0–11 from civ cities, 18–58
  from any barb city) and make early workers ("gatherers") unusable. The earliest practical
  see-camouflage counter is the dog line (`UNIT_TRAINEDDOG`, `TECH_CANINE_DOMESTICATION`,
  x=16) — recon units (Scout, x=2) technically see camouflage but don't protect workers.
  Six tech columns of no-counterplay (expansion tech Tribalism sits at x=13, inside the
  window). Owner direction: **bar camouflaged units from barbarians until the counter is
  broadly available** — no data-layer flag exists (`bTreatAsBarbarian` is unrelated), so
  the fix is a small DLL rule, generic: NPC players may not field a unit with an
  `InvisibleType` until every alive PC team has tech-unlocked a trainable unit that can see
  that invisibility class. **IMPLEMENTED** (2026-06-11, Assert-build green): seer index
  `cvInternalGlobals::buildInvisibleSeerIndex` + `getUnitsSeeingInvisible`,
  `CvTeam::isInvisibleSeerUnlocked`, `CvGame::canNPCFieldUnit`, gate in the
  `CvPlayer::canTrain` NPC early-out. **Refined (v2):** only units offering
  `UNITAI_SEE_INVISIBLE` count as seers (dog line + law-enforcement line) — v1 counted any
  see-camouflage unit, and roaming recon (Scout, `TECH_TRAILS`, x=2) would open the gate
  near game start. Note the t~270 "thief spawned through the gate" observation was NOT a
  confirmed leak: by then every team (incl. the tech-laggard human) had the dog tech, so
  the gate was legitimately open under either version. **Rules-context:** matching keys on
  the binary `SeeInvisibleTypes` lists, which is exactly the active rule with
  `GAMEOPTION_COMBAT_HIDE_SEEK` OFF (the owner keeps it off — its intensity rules are
  near-incomprehensible; #403 tracks documenting + reworking H&S); under H&S ON the gate
  would be approximate (intensity-blind) — acceptable, revisit with #403. Gate covers barb map spawns, barb
  city training, property spawns (`CIV4SpawnInfos.xml` spawning bypasses `canTrain` — no
  camouflaged unit has a SpawnInfo today). Owner rulings folded in: civ criminals stay
  exempt by design (they must *travel* from owned territory — an investment and an
  exposure — while NPCs spawn virtually anywhere); the broader "invisible units arrive
  too early for everyone" timing question is deferred to a future tech rework.
- **Balance finding: early wildlife may be overtuned** (owner observation). With
  era-available units capping at str ~4 and a quarter of land animals at str 5–7 (74%
  aggressive), turtling is the *only* correct AI play; civs bleed anyway (one dead by t84,
  another at zero units by t188 with its city standing open). If the intended early game is
  "dangerous but huntable", the levers are content-side: animal strength/spawn curve vs era,
  aggression share, spawn cap, hunter-line anti-animal modifiers.
- Wildlife population sits at its spawn cap (~700 land / ~450 water) regardless of hunting.

## How findings feed fixes

Counts → divergence → root-cause (Autolog) → competence fix in the relevant initiative:
`unit-ai-valuation.md` (valuation correctness), `ai-architecture-north-star.md` §6
(decide-side modules, city declared-needs), `subdued-animal-ai.md`, worker/improvement plans.

Memory cross-refs: `ai-vs-human-benchmarking`, `live-census-workflow`, `unit-ai-valuation`.
