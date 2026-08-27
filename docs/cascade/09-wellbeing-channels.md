# 2b. The WELLBEING channels — health + happiness (signed-split, the §2a sibling)

> Part of the **[cascade](../cascade.md)** spec.

The city's **health** and **happiness** levels are the §2 combine over **FOUR ORDINARY CHANNELS** —
`happiness`, `anger`, `health`, `unhealth` — summed in **opposing pairs** at the verdict: happiness against anger,
health against unhealth. They are four yields like any other, carried on the one uniform package with no special
storage: a source depositing a negative value is routed to the opposing channel **at fill**, so nothing about the
combine or the cache is wellbeing-specific.

**⛔ A CHANNEL *IS* THE LEVEL — there is no separate verdict getter, and the distinction that remains is
DEPOSITS vs REALIZED.** Two reads, and conflating them double-counts:

| read | answers | composes with |
|---|---|---|
| the GROUP read (`getWellbeing`) | the DEPOSITS only — the cascade's roll-up over the scope chain | a CANDIDATE's `expectedWellbeing`, which answers in the same vocabulary ([patterns.md](../architecture/patterns.md) § THE TWO READ ROLES) |
| the REALIZED read (`realizedWellbeing`) | deposits **+** the raw-state inputs below | nothing — it is this city's own level |

The raw-state inputs are folded at the REALIZED read, exactly where the engine folds them, so a consumer never
re-derives one. A consumer wanting one side of a pair indexes the array; there is **no per-side getter**.

The **opposing-pair NETS** (`InfoValuation::netHappiness` / `netHealth`) live once on the calc surface, are fed
the four channels rather than an object — which is what lets the same implementation net a city's realized set
and a candidate's expected delta — and are **signed** (a surplus is as meaningful as a deficit). The realized
end-state values are the clamps over them, and are a final-state CALCULATION, never a channel or a getter
([patterns.md](../architecture/patterns.md) rule 6): `healthRate = min(0, health − unhealth)`;
`angryPopulation = clamp(anger − happiness, 0, pop)`.

⚠ The wellbeing channel has no decomposition census yet ([http-endpoints](../specs/http-endpoints.md)); when the route
table is rebuilt it wants one field per named engine term, so a divergence localises to a single source.

**The TARGET/INPUT split (the tradeYield precedent, [validation](../specs/validation.md) input rules):**

- **DEPOSIT-COMPUTED (the cascade's targets)** — everything a live source's `health`/`happiness` family deposits
  produce: **buildings** (city `flat`/`perPopulation` + the empire-scope rollups + conditioned entries incl.
  `HAS_STATE_RELIGION`-gated and the reverse-landed source-keyed boosts — a wonder/civic/tech `buildings.{B}`
  wellbeing deposit is authored deliverer-side (§4) but the readJson reverse pass lands it on the TARGET building
  as a CITY-scope conditioned entry gated on the source's presence at the authored scope, so it reads
  building-side under this term), **civics** (empire flats — incl. the tax-anger deposit, a `happiness.empire`
  entry per-scaled on `GOLD_RATE`, re-booked by the slider-rate count route — + the keyed/heterogeneous members
  read civic-side: `features.{F}`, `nonStateReligion`, the `cities.{unit: IS_MILITARY}` per-unit scaler, the
  ranked `cities` scaler — the civic's `buildings.{B}` member lands building-side per the above), **traits** (same member
  vocabulary), **features** (`health.plot.percent` — summed over radius plots, ÷100 — the fallout class),
  **bonuses** (`empire.cities` flats, presence-gated — ⛔ NEVER a bare `empire` flat: that lands in the empire
  package and rolls DOWN to every city, while the engine applies it on the per-city presence fact, so one
  connected luxury is counted once per holding city and the product handed back to every city. The `cities`
  target lands it in the HOLDING city's package, which is what a luxury means — the cities that HAVE it are
  happier — the same precedent the specialist `cities`-target deposit sets one entity
  over), **specialists** (city flats; the fractional values are the
  curator's ÷100 de-scale of the legacy latent-×100 — the engine `…/100` at use), **corporations**
  (`HAS_CORPORATION`-conditioned city flats), **techs**/**projects** (empire — projects also the lone `world`
  scope)/**handicaps** (empire flats), and **military units** (`happiness.empire.cities.{unit: IS_MILITARY}`
  §3.7). **Religion happiness has NO religion-side data** (verified: legacy religion info carries none) — the
  state/non-state religion terms derive from CIVIC/TRAIT/BUILDING configs × religion presence.
  ⚖ **Improvement health is a BALANCE-CUT (curator ruling, `curate_improvement.py`):** legacy `iHealthPercent`
  is deliberately dropped from the data, so the engine's `improvementGood/Bad` term is an **intentional
  divergence** — attributed by the engine's own `improvementGood100/Bad100` terms, shown, never chased
  ([validation](../specs/validation.md) intentional-model-change class); the term dies at the channel's legacy cut.
  ⚖ **Improvement HAPPINESS, by contrast, IS represented** (owner ruling — no gaps): the intrinsic per-radius
  improvement happiness (`happiness.plot.flat` on the improvement) and the civic per-improvement happiness
  (`happiness.empire.improvements.{I}.flat`) are **folded into the feature happiness terms** (`featSubstrate` +
  `featMember`) — because the legacy `getFeatureGoodHappiness` bundles feature + improvement happiness into ONE
  number. Structurally live end-to-end; **zero data carries it today** (schema-only civic field, no improvement
  authors `iHappiness`), so the verdict is unchanged — the path is future-proof for any modder data.
  **Celebrity happiness** is an INPUT; the `skills.celebrity` unit-scan port (the CvCity scan) finishes it.
- **RAW-STATE INPUTS (folded, never derived)** — the runtime timers/counters no deposit produces: the **anger
  percents** (overcrowding = f(pop), noMilitary, foreign-culture, enemy-religion, hurry/conscript/defy/
  revRequest timers, war-weariness, revIndex, civic anger%), the **happiness timer** (`getHappinessTimer` —
  the same countdown shape as the anger timers above, folded on the happiness side: `GC.getTEMP_HAPPY()` while
  the timer runs), the **espionage counters**, **event anger**
  (one-shot event state), **foreign-culture anger**, **landmark anger** (option-gated —
  ⚖ KEEP through the migration: the existing engine implementation stays, *"straight up state derived from the
  plot in question"*; the landmark data pass is a sanctioned separate data pass (#448); the engine impl KEEPS),
  **city-over-limit**, and **vassal** terms. These are saved/derived-from-saved state — legitimate inputs, since
  no deposit produces them and nothing about them is a cascade output ([validation](../specs/validation.md) pollution
  guardrail) — and the calc folds them at the level combine exactly where the engine does.
  ⚖ **The `extraHappiness`/`extraHealth` accumulators are EVENT-GRANTED persisted state, a
  SANCTIONED read, not a ride-in:** the CITY `getExtraHappiness`/`getExtraHealth` are written ONLY by `applyEvent`
  (an event granting extra happiness/health) — genuine one-shot non-derivable state (the event-store class,
  § THE MAINTAINED SUM, above); the PLAYER accumulator additionally bundles the
  DERIVABLE trait+tech, which the calc NETS OUT (− engine trait/tech + the cascade nets), keeping only the
  event/unattributed residual. Wiring these as proper cascade event grants is **event-rework scope** (#425 events
  stay Python / the F3 grants apply-loop), NOT a modifier-cut ride-in to fix here.
- **GATE FLAGS** — the `abolished<Channel>` amenity family ([json.md §8](../specs/json.md)) zeroes its side wholesale.
  They are **HARD OFF-SWITCHES, never modifiers**: while a live grantor confers one *"unhappiness does
  not exist in the city"* — the side ceases to exist rather than being reduced, so the combine drops the whole
  channel instead of subtracting from it.
  ⛔ **The gate asks the CITY, never a grantor** — `CvCity::isNoUnhappiness` /
  `isNoUnhealthyPopulation` / `isBuildingOnlyHealthy` are folds over the city's `amenities`
  (§ THE CONTEXTS, below), so a WHERE rides the grant's own `enabled` condition and is
  evaluated per receiver at fold time. There is no hand-named counter and no per-key grantor read to reach for.
  ⚑ **No BUILDING authors one, and that is DELIBERATE — the mechanic is "wildly overpowered"** — so
  finding the building side unauthored is never licence to author one, and equally never a reason to purge the
  key as unused. ⚠ The CHANNEL is nonetheless LIVE: a civic confers `abolishedAnger` gated `IS_CAPITAL`, which
  is what retired the legacy key that baked the capital into its name
  ([conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary.md#35-predicates--a-systems-runtime-state-query)).
- **`unhealthyPopulation`** (= `max(0, pop − angryPop)` unless flagged) enters the BAD side as the engine's
  population term — a state-derived input (it reads the happiness verdict; the calc computes it from its own
  happiness result, never reads the engine's).

⚠ Two engine quirks the calc reproduces verbatim — named here so the reproduction is DELIBERATE and visible rather
than accidental. Whether they survive is a SPEC decision (the spec leads), never a silent "fix" at a call site:
`badHealth` adds `min(0, extraBuildingBadHealth)` **twice** (once inside `totalBadBuildingHealth`, once
directly); and the anger percents scale by `pop/PERCENT_ANGER_DIVISOR` with truncating integer division.

**⛔ TRAVELING UNIT MODIFIERS RIDE ON TOP (GENERAL — all channels).** A modifier that
TRAVELS with a unit (unit-sourced happiness, anger, property emission, and any future unit-carried channel
value) is **never part of a cached cascade computation**: it is computed LIVE at read and **added on top as a
FLAT term, after and outside every percentage modification**. Two structural consequences: (1) unit movement
never dirties any cache — the cached sums are unit-free by construction; (2) the traveling value is a plain
flat addition to the realized number, never an input to a percent stack. The implementation shape: the cache
stores the unit-free number (+ any epoch-stable per-unit multiplier, e.g. a civic's per-military-unit VALUE);
the read folds `perUnit × liveCount` / the live unit walk on top (an O(1)-ish live engine read).
**The AUTHORING BAN that keeps this coherent: no unit gives — or can ever be
ALLOWED to give — PERCENTAGES to yields of any kind.** A unit-carried value is always a raw flat number on
top; a unit-authored percent would force units back inside the cached percent stacks and break the whole
on-top model. Enforceable at the curator/validation layer: a `units/**` JSON authoring a yield/commerce
`percent` deposit is a data error.
Ledgered as [unit-carried modifiers apply on top, live, never cached](#2b-the-wellbeing-channels--health--happiness-signed-split-the-2a-sibling).

> **⚖ THE COMMANDER RIDES ON TOP OF A UNIT EXACTLY AS A UNIT RIDES ON TOP OF A CITY.** *"Whatever a
> commander does is on top, it is not part of the unit itself — it is literally the combat calc's job to check
> if the commander has points left to add to the attack."* So this is the SAME rule one scope down, not a new
> one: the commander→unit relationship is the unit→city relationship, and everything above applies unchanged.
> - The unit's RESOLVED values (§ THE READ PATH, below — UNIT plane) are
>   **COMMANDER-FREE by construction**: they gather the unit's own info ∪ its promotions ∪ its unit-combat
>   classes, and nothing else. A commander attaching, detaching or moving is neither a promotion nor a
>   combat-class change, so it must never be a cache input — there is no fact that would move it, and baking it
>   in yields a plausible, permanently stale number the moment the commander moves.
> - The commander's contribution is added **LIVE, ON TOP, at the COMBAT CALC**, which is also the only place
>   that can ask the question the mechanic actually turns on: **has this commander got control points left to
>   spend on this attack?** A stat read cannot answer that, which is why the fold does not belong in one.
> ⛔ So a per-unit stat getter that reaches through `getCommander()` and adds the commander's own accumulator is
> the wrong shape twice over: it puts a traveling modifier inside the unit's own number, and it spends the
> commander's points without ever checking whether any remain.

**UNIT-driven wellbeing is END-TURN cadence.** The military/unit-count happiness
term recomputes **once per turn** (the substrate's turn-roll), NEVER per unit move — a per-move mark hook made
every post-move rate read pay the wellbeing walk (a measured unit-automation collapse) and is banned. The
within-turn lag this leaves on the wellbeing slots (a handful of cities whose garrison changed mid-turn) is the
RULED cadence, not a freshness hole; the getter flip proceeds with it.

**The STORED-ACCUMULATOR DRIFT class.** The legacy wellbeing terms are
INCREMENTAL SERIALIZED accumulators (`m_iBonusGood/BadHappiness`, `m_iBuildingGood/BadHappiness`,
`m_paiStateReligionHappiness`, `m_iExtraBuilding*FromTech`, …) — event-sourced numbers that carry decades of
save history. **The old cache model folded event-type grants DIRECTLY into these caches** (there is no separate
event-yield data — the per-building `m_aBuildingHappy/HealthChange` ledgers carry nothing on real saves), so a
stored value that disagrees with its current-state recompute is **DRIFT (history pollution), never
event state to preserve**. ⛔ The `*Recomputed` twin that once stood beside each incremental accumulator is
GONE, and so is the comparison surface it fed ([superseded-ideas #17](../architecture/superseded-ideas.md): zero
such symbols remain). A stored-vs-recompute divergence of this class is **engine-wrong / cascade-right** and is
repaired by the slots recomputing from data — never by re-adding a twin to measure it with.

---

