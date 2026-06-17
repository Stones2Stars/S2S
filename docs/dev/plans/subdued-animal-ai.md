# Subdued / tamed animal AI — plan

**Status:** first implementation shipped (2026-06-11, #381): free-bonus-aware construct
targeting (the resource spread), prompt-disband scrap policy, and the human "auto spread"
automation. Goal: make the AI treat subdued animals as a **managed strategic resource**
(spread / build / butcher by value), not just idle units that pile up.

Legend: **[V]** verified in code; **[O]** owner-reported design knowledge, not yet code-verified.

## Owner rulings (2026-06-11)
- **Spread first**: animals prefer "religion-spreading" their buildings (carry the herd
  building to every city lacking the bonus) over terminal options. Bonus access is
  **city-local** — a city needs the resource in the city itself (or the national wonder) —
  so every horseless city is a real spread target.
- **Tamed animals cannot be slaughtered/converted** — when not needed they must be **promptly
  disbanded**, not hoarded.
- **Never defense or scouting.** A real historical bug had tamed animals used for defence and
  scouting — that behaviour must not return (also a constraint on the #384 auxiliary-garrison
  design). Data-side guard already present: `NotUnitAIs` WORKER/EXPLORE on tamed units.
- Fewer units on the map is itself a perf win (the [PERF/unitai] UNIT_SUBDUED churn).
- Humans get an **auto-spread automation** so tamed animals jog to where they are needed
  instead of being manually ferried.

## The distinction that matters [O]
- **Subdued animal** = the captured mobile *unit* (`UNITCOMBAT_SUBDUED`, `UNITAI_SUBDUED_ANIMAL`;
  cannot merge — `bCannotMergeSplit`). It can be walked into a city and consumed.
- **Tamed animal** = the in-city *building/resource* produced from a subdued animal (e.g. a
  building that grants `BONUS_HORSE` / spreads a resource), vs. **butchering** it for a one-off
  yield. The AI currently doesn't model "unit → which in-city outcome" as a strategic choice.

## What the AI does today [V]
- Keep-value self-limits: in the unit keep/disband valuation, `UNITAI_SUBDUED_ANIMAL` value is
  scaled **down** the more you already have — `iValue *= max(0, cities*2 - numSubdued)/max(1,
  cities)` (`CvPlayerAI.cpp:20916`). So ~2/city is the soft target → the old glut is mostly gone
  (matches owner's observation).
- Use-value is **"primarily on what they can construct"**: the subdued-animal branch of
  `AI_unitValue` (`CvPlayerAI.cpp:~26153`) loops the unit's constructible buildings and scores by
  `AI_getNumBuildingsNeeded(eBuilding, ...) > 0` for the best coastal/capital city. So it only
  values a subdued animal if it can build a *currently-needed* building somewhere.

## Gaps to fix [O]
1. **No butcher evaluation.** The AI never weighs *butchering* an animal in a city — which is
   often worth it when there is **no building left to create** with that animal (one-off
   food/yield). Today an animal with nothing to construct just lingers. → add a butcher-vs-build
   (-vs-keep) value comparison.
2. **No resource spreading.** When a subdued animal can build a resource-granting building (e.g.
   horses), the AI doesn't think to **spread that resource to all cities** — it should distribute
   the bonus across the empire, not stop at one. `AI_getNumBuildingsNeeded` likely undercounts
   this empire-wide intent.
3. **Subdued-vs-tamed not modeled as a decision.** The construct-only valuation sees "buildings I
   can make right now," not the strategic fork: spread the resource, butcher for yield, or keep
   the unit. Each subdued animal should be routed to its best in-city outcome.

## Verified (2026-06-11) [V]
- **Butcher** = `MISSION_SLAY_ANIMAL` → `OUTCOME_SLAY_ANIMAL` (food ~20+rand, production,
  commerce; requires a city plot in friendly territory). **Culture** = `MISSION_RECORD_TALE` →
  per-size-class outcomes granting 1–37 culture (+research at the top end), defined on the
  `UNITCOMBAT_SUBDUED_*` size classes. Both ARE evaluated by `AI_outcomeMission`
  (`CvUnitAI.cpp:~14672`, best `CvOutcomeList::AI_getValueInPlot` across reachable cities),
  and the subdued cascade runs them AFTER construct, BEFORE scrap — the ordering already
  matches the rulings.
- **Resource-spread line**: `BUILDING_HERD_*` grant the bonus via `ExtraFreeBonuses`
  (e.g. `BUILDING_HERD_HORSE` → `BONUS_HORSE`), city-local. Constructed by the matching
  `UNIT_SUBDUED_*` AND `UNIT_TAMED_*` units; both kinds run `UNITAI_SUBDUED_ANIMAL` → the same
  `AI_subduedAnimalMove` cascade. Tamed units have NO slaughter/record-tale actions → their
  terminal option is the scrap step.
- **Scale**: ~194 subdued unit types; the old scrap allowance (2 in-borders / 1 outside, +1
  small-empire, PER TYPE) was the standing-zoo retention.
- `AI_getNumBuildingsNeeded` counts cities lacking the building — fine for spreading; the
  spread blockers were in `getBestConstructValue` (below).

## Shipped (2026-06-11, #381)
- **`getBestConstructValue` skips cities that already have every bonus a building grants**:
  herd buildings fan out to bonus-lacking cities, has-bonus cities stop swallowing herd units
  for ~zero value, and the `assumeSameValueEverywhere` cache gets seeded from a city that
  actually needs the bonus (previously the first-evaluated city — possibly bonus-satisfied —
  set the cached value for the whole empire, erasing the per-city differential that IS the
  spreading decision).
- **`AI_scrapSubdued` prompt-disband policy**: allowance 2/1(+1) → **1/0(+1)** per type (one
  in-borders spare as a hedge for tech-unlocked buildings).
- **`AUTOMATE_SPREAD`** — human automation: the unit runs the same heritage/construct
  targeting each turn (no terminal actions — humans decide slaughter/disband themselves);
  holds in place when nothing currently needs it.

## North star
Cities (or the player AI) declare what they need (a resource they lack, food when starving), and
each subdued animal is valued for its **best** in-city action: build-to-spread-resource, butcher,
or hold. Ties into [[city-driven-worker-valuation]] (cities declaring needs) and the
[[hunter-ai-module-split]] / hunter economy that produces these animals.

## Cross-references
- Related report: `unit-ai-valuation.md` (hunter glut; dog/War-Dog spam still open).
- Memory: [[unit-ai-valuation]], [[city-driven-worker-valuation]], [[building-improvement-yields]].
