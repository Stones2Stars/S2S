# The mission / outcome system (`CvOutcome`) — reference + `missions`-migration map

> How the engine's unit **action → outcome** subsystem behaves today, mapped in full (2026-07-01). It is
> **un-wrappered** (`CvUnitInfo.cpp:2569` — *"outcome system (no wrapper)"*).
>
> **⛔ Owner ruling (2026-07-01) — the mission concept is slated for a GROUND-UP REWORK; NOT ported in #430.** The real
> reason missions/CvOutcome are kept out of the migration is that **the entire mission concept is being redesigned from
> scratch** (a new design, NOT a port) — so porting the old CvOutcome model would be **throwaway work**. The outcomes/
> payloads therefore **stay in the old XML** and **legacy keeps applying them** meanwhile; the subsystem is isolated
> enough to defer cleanly, same posture as the random-events system (also a ground-up rework, also kept out). This doc
> is the behaviour reference for **the old system the rework replaces**. When the rework lands, the cascade `missions`
> block is (at minimum) a **LIST of which
> missions a unit can use** (the outcome-mission references + the hardcoded mission-abilities) — **no payload
> migration**, no probabilistic/expression/`grants`-as-outcome model. The earlier full-port sketch (the "7 design
> questions" below) is **SCRAPPED** — kept only as a record of what NOT to do. This doc stays the behaviour reference
> for that future clean pass. **Scope decided for that pass (owner 2026-07-01):** the `missions` array lists **BOTH**
> the data-driven outcome-missions (`<Actions>`) **and** the hardcoded mission-abilities; the `greatPersonAction`
> `base`/`multiplier` **magnitudes are LEFT as-is** (whether they end up used is TBD). Until the pass runs, **the four
> grants keys** (`buildings`/`greatPersonAction`/`goldenAge`/`greatPeople`, magnitudes and all) — a PERMANENT carve-out
> (ground-up rework, out of #430) — stay in `grants` untouched (the grants machine already ignores them).

## The three objects (the payload does NOT live where you'd expect)

- **`CvOutcomeInfo`** (`Infos/CvOutcomeInfo`) — a registered infotype (**100** types, `CIV4OutcomeInfos.xml`). It is
  **ONLY a gate + identity + replace-tier tag — NO effect payload.** Fields: `prereqTech`/`obsoleteTech`/`prereqCivic`,
  territory bools (coastal/friendly/neutral/hostile/barbarian), `city`/`notCity`, `bCapture`, prereq-buildings, a
  per-promotion extra-chance odds table, a `ReplaceOutcomes` self-FK list (tier supersession), a message string.
- **`CvOutcome`** (`Engine/CvOutcome`, read from each carrier's `<Outcome>` block) — **where the effect lives**:
  `unitType`, `promotionType`, `bonusType`, `yields[]`, `commerce[]`, `GPP`, `Properties`, `happinessTimer`,
  `populationBoost`, `reduceAnarchyLength`, `eventTrigger`, `bKill`, `iChance`. **`execute()` is DATA-DRIVEN** — it
  applies whatever fields the instance declares, unconditionally; it **never switches on the `OUTCOME_*` Type name**
  (those are semantic labels + the replace-tier handle only).
- **`CvOutcomeMission`** (`Engine/CvOutcomeMission`, read from `<Action>`) — binds a `MISSION_*` to an outcome list:
  `mission`, `outcomeList`, `propertyCost`, `payerType` (defaults to the unit), `bKill` (**defaults TRUE**),
  `iCost` (`IntExpr` gold), `plotCondition`/`unitCondition` (`BoolExpr`).
- **`CvOutcomeList`** (`UI/CvOutcomeList`) — a `vector<CvOutcome*>` that **rolls exactly ONE** weighted outcome
  (chances summed, floored to 100 so a sub-100 sum reads as absolute %), after **recursively removing** any outcome
  listed in a surviving one's `ReplaceOutcomes` (higher tier prunes lower). `CvOutcomeListMerged` merges a unit's list
  with its combat-class lists, dedup'd by `OutcomeType`.

## Two carriers × two surfaces

Both `CvUnitInfo` **and** `CvUnitCombatInfo` expose both surfaces (runtime merges unit + all its combat-class lists):

- **`KillOutcomes`** (`m_KillOutcomeList`, `<KillOutcomes>`) — fired on a **combat kill** (the subdue/hunt case).
  Runtime: `CvUnit::updateCombat` (3100/3623). ⚠ **Subject asymmetry:** the **victor** receives the grant, gated by
  the **defeated** unit's kill-outcome definition.
- **`Actions`** (`m_aOutcomeMissions`, `<Actions>/<Action>`) — **player/AI-triggered** outcome missions. Dispatch is
  the **`default:` case** of `CvSelectionGroup::canStartMission`/`startMission` → `CvUnit::doOutcomeMission` (9279) →
  `getOutcomeMissionByMission` (unit info, then each combat-class) → `isPossible` → `execute` → `isKill()` ? `kill`.
  AI: `CvUnitAI::AI_outcomeMission` (15168) scores every action list over reachable city plots.

## Effect KINDS an outcome can produce (`CvOutcome::execute`, 991-1334)

Promotion · spawn-unit (at plot, or `bUnitToCity`→nearest/coastal city; subdued animals auto-join a hunter group) ·
city yields (production/food) · culture · GPP · **`CvProperties` deltas** (crime/disease/…) · temp-happiness timer ·
population boost · reduce anarchy/occupation timer · gold/research/espionage · **place a bonus on the plot** · fire an
**EventTrigger** · **embedded/compiled Python** callbacks · kill the acting unit. **All magnitudes are `IntExpr`
trees** (Constant/Random/Plus/Mult/AdaptUnitYield/Property/Python), conditions are `BoolExpr` trees — **not scalars**.

## The PARALLEL hardcoded mission-abilities (a separate, non-data-driven system)

18+ hand-coded `can*`/`do*` pairs in `CvUnit.cpp`, gated by dedicated `CvUnitInfo` flags, dispatched by explicit
`case MISSION_*` in `CvSelectionGroup` (the outcome-mission `default:` is the extensible alternative to adding more):

| key | `MISSION_*` | gate (`CvUnitInfo`) | produces | kills? |
|---|---|---|---|---|
| found | FOUND | `isFound` | new city | yes |
| construct | CONSTRUCT | per-building `getHasBuilding` | building in city | yes |
| heritage | HERITAGE | `getHasHeritage` | player heritage | yes |
| join | JOIN | `getGreatPeoples` | +free specialist in city | yes |
| discover | DISCOVER | `getBaseDiscover`/`…Multiplier` | tech beakers | yes |
| hurry | HURRY | `getBaseHurry`/`…Multiplier` | +production | yes |
| trade | TRADE | `getBaseTrade`/`…Multiplier` | +gold | conditional |
| great work | GREAT_WORK | `getGreatWorkCulture` | +culture, clear occupation | yes |
| golden age | GOLDEN_AGE | `isGoldenAge` | golden age | yes (may consume several) |
| lead | LEAD | `getLeaderPromotion`/`…Experience` | promote a target unit | yes |
| infiltrate | INFILTRATE | `getEspionagePoints` | +esp points | conditional |
| espionage | ESPIONAGE | `isSpy`+`getEspionagePoints` | espionage mission | conditional |
| spread religion | SPREAD | `getReligionSpreads` | religion in city | yes |
| spread corp | SPREAD_CORPORATION | `getCorporationSpreads` | corp in city (gold cost) | yes |
| inquisition | INQUISITION | `isInquisitor` | purge non-state religion | yes |
| hurry food | HURRY_FOOD | `getBaseFoodChange` | +food | yes |
| claim territory | CLAIM_TERRITORY | (player fixed-borders) | set plot claim | no |
| great commander | GREAT_COMMANDER | `isGreatGeneral` | become Commander | no |
| great commodore | GREAT_COMMODORE | (option-gated) | become Commodore | no |

The three **carved-out grants keys** (PERMANENT carve-out — ground-up rework, out of #430) are members of this family: unit `buildings`→construct, `greatPersonAction`→
discover/hurry/trade/greatWork/hurryFood, `goldenAge`→golden age (and `greatPeople`→join).

## Data census

- **100** `OutcomeInfo` types (one file), effect-less gates. Semantic families: subdue-animal, hunting-kill, record-tale
  knowledge tiers, animal-combat arena tiers, unit-upgrade/conversion, join-city, slave system, cannibalism, human
  sacrifice, go-to-map/space-travel, colonize-map.
- **194** units with `KillOutcomes` (all wild animals). **237** `Actions` occurrences (subdued/tamed animals + 28 land
  units + combat-class-level). `<Action>` `iCost`/`bKill`/`PropertyCost` are unused in current data.
- **Curator: NONE.** `curate_unit.py` lists `KillOutcomes`/`Actions` in its pass-2 **PERMANENT carve-out (ground-up rework, out of #430)** set and passes them
  through `engine.generic()` verbatim under an `outcomes` key — faithfully copied, not migrated.

## ~~Design questions the `missions` block must answer~~ (SCRAPPED — outcomes are not ported; see the ruling up top)

> These questions only mattered for a *full CvOutcome port* (probabilistic outcome-lists, expression fields, payload
> migration). Per the owner ruling above, that port is NOT happening — the outcomes stay in XML, and the future
> `missions` block just LISTS which missions a unit can use. Retained below only as a record of the complexity that
> justified NOT porting it.

1. **Probabilistic, mutually-exclusive lists with tier-replacement** — roll one of N weighted, minus superseded.
   `grants` today is deterministic apply-all; missions need a "roll one, weighted, minus `ReplaceOutcomes`" semantic.
2. **Two homes for effect data** — the 100 shared `OutcomeInfo` gate/identity/replace-tier tags vs the per-carrier
   `<Outcome>` payload magnitudes. Keep the shared replace-tier registry separate, or flatten `OutcomeInfo` into a
   named gate/tier enum the mission entries reference.
3. **Expression-valued fields** — `iChance`, yields, commerce, cost, conditions are `IntExpr`/`BoolExpr` trees + a
   `CvProperties` delta; the schema must carry expressions, not ints.
4. **Non-grant-shaped payloads** — event-trigger firing, place-bonus-on-plot, reduce-anarchy, property deltas,
   spawn-to-nearest-city (coastal/teleport variants), embedded Python — these don't fit a pure resource-grant shape.
5. **Kill flag in two places** (per-`CvOutcome` and per-`CvOutcomeMission`, default TRUE) + the KillOutcomes **subject
   asymmetry** (victor gets the grant, gated by the defeated's def) must be modeled explicitly.
6. **Carriers include `CvUnitCombatInfo`**, and the runtime merges unit + all combat-class lists — the block can't
   assume a single per-unit list.
7. **Unify the hardcoded mission-abilities?** The 18+ hand-coded `can*`/`do*` missions are NOT data-driven today; the
   outcome-mission `default:` dispatch is the hook that could absorb them, but that's a larger reunification.

## See also

- [json.md](../specs/json.md) §5 (`grants`) / §8 (the `missions` block — a PERMANENT carve-out, ground-up rework, out of #430). ·
  [data-migration-remaining.md](../plans/structural-cleanup/data-migration-remaining.md) (the deferred-pass entry).
