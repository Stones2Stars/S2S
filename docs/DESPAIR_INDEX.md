# The S2S Despair Index™

> A rigorously unscientific ranking of the standout bugs from the last couple of weeks,
> ordered by the quantity of despair each induces in a developer at the moment of
> comprehension. Despair is measured in **centiphants (cp)** — the SI unit of despair,
> calibrated against a war elephant that was assigned to defend a city and was later found,
> by the project owner personally, standing seven tiles away in someone else's country.
>
> This document is not holy writ. It is an outlet. No process shall ever cite it.

---

## 🥇 1. The Tactical Elephant Vacation Package — 100 cp *(definitional)*

An Elephant Gunner garrisoned to defend its city was observed sightseeing in the human
player's territory, 7 tiles from home. This required **three independent bugs stacked like
a turducken**:

1. Parking in a city silently retyped any unit to `UNITAI_CITY_DEFENSE` — a unit that
   *cannot use defensive bonuses* became the city's official primary defender.
2. The garrison was then offered a "nearby" attack whose search radius was
   `(range+1) × (moves+1)` = **9 tiles**, with no territory restriction.
3. And "guard a good spot in the city's *vicinity*" scanned a **43×43 tile box**, because
   `NUM_CITY_PLOTS_2` (a plot *count*, 21) was passed to `rect()` (which takes a *radius*).
   The vicinity was a quarter of the continent.

Every individual fix reveals the next bug, like a matryoshka doll where every layer is
also on fire. The live specimen (unit 1843333) was traced in real time via the new HTTP
endpoint: fortified on a lovely defensive hill, 7 tiles out, proudly counted as a garrison
member of a city it contributed exactly nothing to.

*Status: fixed across PRs #383 and #391. The elephant stays home now. Probably.*

---

## 🥈 2. The Library of Alexandria, Burned Nightly — 95 cp

Building values — the numbers behind every "what should this city build?" decision — were
guarded by a cache that `CvCity::doTurn` **deleted every turn, for every city,
unconditionally**. No event-driven invalidation, no staleness check; just `SAFE_DELETE` at
dawn. And every "recalculation" re-derived **static XML facts by brute force** — which
buildings enable which (O(buildings²) `canConstruct` sweeps), which units each building
unlocks (O(buildings × units) expression evaluations) — answers that cannot change without
restarting the game, re-learned ~1,350 times per turn at ~100 ms each. Peak measurement:
**134.7 seconds of a 170.8-second turn. 87% of the entire late game**, spent re-deriving
the tech tree from first principles.

The despair is a trilogy:

- **Act II:** the static relationships were made *actually static* — a load-time reverse
  index, ~390× on the hotspot (11.7 s/turn → 0.05 s). Victory declared. Then the
  **identical O(buildings²) brute-force sweep was found alive in a second location** — the
  building-scoring loop's "enables other buildings" bonus — quietly burning ~4 s/turn and
  wearing a `// TODO OPT` comment like a name tag.
- **Act III:** the first cache-*retention* fix returned 40% instead of the expected 4×,
  because the "retain values" flush still stamped every cache **incomplete** every turn,
  and the first read then forced a full recompute anyway. The library kept its books and
  re-read all of them just to be sure.

*Status: fixed in three installments (the #314 static index, retention + staggered
refresh, the scoring-loop migration). The residual ~2.6 s/turn is event-driven and has an
appointment with the repository.*

---

## 🥉 3. The (Un)Fortified Position — 92 cp

AI garrisons parked with one-turn `MISSION_SKIP`, so they never actually fortified —
meaning **AI cities have never received fortification defense bonuses**, plausibly since
the BTS era. Cities under-read their own defense, concluded they needed more defenders,
hoarded them, and every one of those defenders re-planned its entire existence from
scratch **every single turn, forever** (~15 s/turn for CITY_DEFENSE alone).

The perf bug and the gameplay bug were the same bug, the fix was *"push FORTIFY instead of
SKIP"*, and the wrongness was old enough to vote. Generations of defenders stood in cities,
awake, slightly nervous, accruing nothing.

*Status: fixed in the #342 campaign (persistent parks, staggered re-plans).*

---

## 4. Schrödinger's Job Application — 85 cp

A unit's first `processContracts` call of the turn *advertised* for work, found none, and
returned `true` — "I did something!" The slice driver, taking it at its word, re-ran the
unit's **entire decision cascade a second time**. For ~6,000 advertising units. Every turn.

The unit's day consisted of asking "does anyone need me?", hearing silence, and billing it
as a full shift — and the *system agreed*.

*Status: fixed. Asking around is no longer a career.*

---

## 5. The RESERVE ↔ PROPERTY_CONTROL Hokey Pokey — 78 cp

The role-conversion gate asked *"could this unit theoretically help with crime?"* while
the assignment handler asked *"does this unit actually have the equipment?"* Different
answers → units flipped roles **~750 times per turn**, and the phantom "we're handling
it!" signal suppressed real crime-fighting production, which drove the cheap-military
production spam.

You put your whole UnitAI in, you take your whole UnitAI out, you do the hokey pokey and
you corrupt the demand accounting. That's what it's all about.

*Status: fixed — both sides now ask the same question.*

---

## 6. The Unkillable Peasant — 71 cp

`getCombatOdds` floored per-round damage to 0 against ~zero-strength defenders, making a
dying militiaman **mathematically immortal**. The AI under-reported its own win odds
against trivial opposition — in the engine that feeds *every AI attack decision and the
player-facing combat preview*.

Somewhere in the math, a tank regarded a wounded peasant with genuine caution, and every
layer above it nodded along.

*Status: fixed (damage clamped to ≥1 per round). The peasant is at peace.*

---

## 7. The Bear Patrol — 62 cp

The world's city defenders kept abandoning their posts to duel wildlife. Garrison sorties
fire at a **55% odds bar** against any "enemy" within reach — and in the prehistoric era,
the enemy is eleven hundred bears. A defender leaving the walls to fight one at coin-flip
odds risks everything to remove a threat that **cannot attack cities**; multiplied across
every garrison, every turn, the world's starting defense corps (84 units, including the
non-renewable founding guardians) bled to **25 by turn 71** — protecting cities from
creatures that were never coming. Bonus reveal: the second sortie path turned out to be
the unexplained dispatch from the elephant cold case — `chokeDefend`'s "radius 1" attack
actually searched 4–6 tiles, through the same range-inflation formula, the whole time.

Is the Bear Patrol working? Citizens, look around: not a single city has ever fallen to a
bear.

*Status: fixed (#400) — garrisons only hunt bears they are nearly certain to beat (the
meat is genuinely worth it, ruled the owner), and chokeDefend uses the leashed sortie.
The bears are now mostly unbothered, and exclusively by professionals.*

---

## 8. The Eternal Anesthesiologist — 60 cp

`AI_heal` returned `true` for a heal no-op when the unit *couldn't heal*, so units
re-decided "heal in city" **49–196 times per turn** — and in rare alignments, the turn
simply *never ended*.

The software equivalent of pressing an elevator button that is lit, knowing it is lit, and
pressing it 195 more times. Except occasionally the building never lets you leave.

*Status: fixed (`canHeal()` guard). The elevator arrives.*

---

## 9. International Civil Asset Forfeiture — 55 cp

Foreign police cars were observed parked on the human player's **resource tiles**, mission
hover proudly reading *"Maintain property control."* The mechanism: when a property-control
unit isn't at its target city, the AI rolls a d10 every re-plan — and on 0–5 it simply
**doesn't move**, skipping in place wherever it stands, formally maintaining whatever tile
that happens to be. The code comment says *"RNG 50% only will move."* `iValue <= 5` out of
ten outcomes is 60%. The comment cannot count, the police cannot drive, and the unit
random-walks toward its destination at ~0.4 steps per turn, getting stranded mid-route in
other nations' territory — where it sets up a checkpoint on someone else's uranium.

Bonus: if pathing failed, it parked on *"waiting for escort"* — an escort that is never
dispatched to property units. The police car waits for backup. Forever. Abroad.

In fairness to the code, the mission is called *property control*, and the police cars
controlled the plot like it was their property. The bug was in our expectations.

*Status: fixed (#396) — the dice are gone, journeys commit, and units pool at home,
fortified. The uranium has been returned.*

---

## 10. The .vcxproj of Lies — 47 cp

The Visual Studio project file confidently states `PlatformToolset: v142`. The actual
compiler is the **Microsoft Visual C++ Toolkit 2003** (MSVC 7.1). The project file drives
nothing, is synced with nothing, and has already caused one wrong toolchain conclusion
that had to be corrected with a HARD RULE in block capitals.

The single most authoritative-*looking* file in the repository is a museum exhibit. Trust
it and you'll write `auto`, and the year 2003 will personally reach through the linker to
stop you.

*Status: mitigated by documentation and fear.*

---

## Honorable Mentions

- **The Zen Logger** — `AI_setAsGarrison` contained, for years, a gated logging block of
  perfectly empty braces: at log level 3 it logged *nothing*, flawlessly. One hand
  garrisoning. Replaced this week by `[UNT/garrison]`, which logs *something*.
- **The Integer Answering a Yes/No Question** — a stale argument passed `2` into
  `bool bAllowAnyDefenders`, left over from a signature that no longer exists. Truncated
  to `true`, it spent years confidently answering a question it was never asked.
- **Waiting for Escortdot** — human-automated workers stall forever on
  `WAIT_FOR_ESCORT`, because escorts are only ever dispatched to AI workers. Two workers,
  by a road, waiting. *"Shall we improve the tile?" "We can't." "Why not?" "We're waiting
  for the escort."* The escort has never once come.

---

*Scale calibration note: 100 cp = 1 full Elephant. Readings above 100 are theoretically
possible but would legally require the elephant to file an expense report for the trip.*

*Contribution policy (owner-sanctioned): when the ongoing rework unearths something worthy
— a bug whose comprehension produces an audible exhale — it may be added here, with a
score, a name, and the respect it does not deserve. These entries double as an honest
summary of the state of the inherited codebase, which is part of why the rework exists.*

*Fresh despair may be reported, contested, or savored on the
[S2S Discord](https://discord.gg/R8Uejx6uaK).*

*Sibling publication: the [Realism Index](REALISM_INDEX.md) — for mechanics that work
exactly as designed, which is somehow worse.*
