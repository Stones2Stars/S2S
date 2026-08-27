# 4. Availability

> Part of the **[json](../json.md)** spec.

The availability sections decide **what is offered** — what unlocks what, what an entity needs, and how many may
exist. (The two-pass machine that consumes them is the [enabler](../enabler.md).)

### 4.1 `enables` — what this unlocks (permanent, source-side)

```jsonc
"enables": { "units": ["UNIT_CROSSBOWMAN"], "buildings": ["BUILDING_BANK"] }
```

Buckets: `buildings · units · builds · techs · civics · religions · corporations · projects · processes ·
promotions · promotionLines · heritages · specialBuildings · specialBuildingsWaived · improvements · bonuses ·
routes · votes · hurries · traits · specialists`. **Tech unlocks live here** (a tech `enables` what it researches).

### 4.2 `obsoletes` / `replaces` / `disables` — removal (permanent, source-side)

Same per-kind bucket shape as `enables`.

- **`obsoletes`** — supersession: new builds barred; **existing instances persist** (an obsolete unit stays on the map).
  ⛔ **For a BUILDING the target-side `obsoletedBy` carries `techs` ONLY — no building ever obsoletes a building**
  ([enabler.md §2](../enabler/02-pass-1-generate-the-frontier-the.md#2-pass-1--generate-the-frontier-the-enables-family)): a building→building relation is
  an UPGRADE CHAIN, expressed as the predecessor's reversible `requires.operate.dormant` (§4.3). The two fates
  cannot coexist on one pair — obsolescence is checked before the operate verdict, so it wins and destroys what the
  chain meant to park.
- **`whenObsolete`** — the built instance's **fate once obsolete** (its `obsoletedBy` tech is held), authored
  **target-side** as a **full modifier tree in the §6 grammar** (channels · scopes · units · `enabled`/`disabled`
  predicates — a *separate* tree, not a gate on the normal families). When the building is obsolete its **normal
  modifier families stop and this tree applies instead**; the surviving output is authored directly here and may
  differ from the working values.

  > **⚖ OBSOLESCENCE HAS THREE FATES, AND `whenObsolete` DECIDES WHICH.** This is the whole rule; there
  > is no fourth case and no flag beside it:
  >
  > | `whenObsolete` | the built instance |
  > |---|---|
  > | **absent / empty** | **HARD REMOVED** — the building is gone from the city |
  > | **carries any modifier** | **STAYS**, and that tree **TAKES OVER** from its normal families |
  > | **carries the UPGRADE** | **BECOMES its successor** — the predecessor goes, the successor is placed |
  >
  > **⛔ `whenObsolete` LIVES IN ISOLATION AND DOES NOT CARE *WHAT* OBSOLETES.** It declares what becomes
  > of the INSTANCE once the building is obsolete, full stop. It never names its cause, never branches on one, and
  > nothing in it may be read as "…when a tech does it". ⚑ That a **tech is the only obsoleter today** is a fact
  > about the CAUSE side (`obsoletedBy`, [enabler.md §2](../enabler.md)) and is stated there — so a second obsoleter
  > kind arriving later changes that side alone and leaves every authored fate untouched. ⛔ Do not couple the two:
  > a fate that knows its trigger is a fate that has to be re-authored the first time the trigger set grows.
  >
  > **⚖ THE UPGRADE LIVES HERE — `whenObsolete` IS WHERE THE UPGRADE GOES.** Becoming obsolete and what
  > becomes of the instance are ONE happening, so the fate and the successor are authored in one place. ⛔ Do
  > **not** mint a separate top-level upgrade section for it. The key is **`becomes`** (becomes works
  > well, it's unambiguous), a reserved key beside the modifier families in the tree:
  >
  > ```jsonc
  > "whenObsolete": { "becomes": "BUILDING_FOUNDRY" }
  > ```
  >
  > **⛔ THERE IS NO GOLD-PAID BUILDING UPGRADE, AND THERE WILL NOT BE ONE.** The upgrade is a
  > CONSEQUENCE of becoming obsolete, applied automatically — never a player action, never priced, never
  > prompted. ⚑ The unit parallel is where the temptation comes from and it stops at the structure: a unit
  > upgrade is CHOSEN and PRICED (`CvUnit::upgrade` / `upgradePrice`); a building's is neither. **Do not port
  > `upgradePrice`-shaped machinery to buildings, and do not add a cost, a prompt or a player action to this
  > fate.** ([superseded-ideas #41](../../architecture/superseded-ideas.md).)
  > ⚑ **It is the OTHER half of the upgrade chain, and the two must not be confused**
  > ([enabler.md §2](../enabler.md)): the successor being BUILT parks the predecessor (reversible dormancy, keyed on
  > PRESENCE); the TECH landing turns the predecessor INTO the successor (one-way, keyed on the tech). Different
  > triggers, different directions, no contradiction — a building may carry both, and the Forge does.
  >
  > ⛔ **THE PLACEMENT WALKS THE CHAIN.** Chains are real and deep — 437 upgrade edges whose target ITSELF
  > upgrades, and the bridge ladder runs 14 links — so the successor named may itself already be obsolete by the
  > time the tech lands. The walk follows the chain to the **first successor that is neither obsolete for this
  > team nor refused by its own `requires.build` in this city**, which is exactly what the legacy culture-shell
  > swap did. ⚠ If the walk finds nothing placeable the fate falls back to **HARD REMOVED** — the predecessor is
  > obsolete either way; there is simply no tier to hand it to.
  > ⚖ **THE FATE FIRES ROUTINELY; THE PLACEMENT LEG IS *VERY* RARE — and the two must not be conflated.**
  > The obsoleting tech sits typically **2–3 eras past** the successor's own availability, and it is very rare that the building has not already been built — so the walk almost always finds the successor already held,
  > stops on its first test, and the fate resolves to a plain REMOVAL.
  > ⚑ **It still fires, and visibly, BECAUSE OF DORMANCY.** A superseded predecessor is parked, not
  > removed, so it is still PRESENT in the city for those 2–3 eras; the obsoleting tech is what finally clears it.
  > ⇒ The routine, observable behaviour of this fate is *the long-dormant predecessor disappearing when its tech
  > lands* — not a building turning into another one.
  > ⛔ **Do not optimise the walk, cache its result, or narrow it for cost.** Its expensive leg is the cold one,
  > and its bound exists to stop a spin rather than to be tight.
  > ⚠ **The real hazard of a rare leg is the opposite of cost** — it is exercised late and seldom, so a defect in
  > it survives far longer than in a hot path. Keep it obviously correct rather than clever.
  >
  > ⚑ **Placement is IDEMPOTENT and needs no extra rule** — the ONE placement choke point already refuses a
  > building the city holds, refuses an obsolete one, and evaluates the successor's `requires.build`
  > ([triggers.md](../triggers.md)). ⚠ **Convergence is therefore SAFE but LOSSY, and that is the intended
  > behaviour, not a defect:** many predecessors upgrade into one receiver (83 buildings converge on
  > `BUILDING_HIGH_TECH_CULTURAL_ENRICHMENT`, 47 on `BUILDING_FOOD_MANUFACTURING_DISTRICT`), so a city holding
  > several of them ends with ONE. The count is not preserved and was never preserved in legacy either.
  >
  > ⛔ **THE OLD "NO SUCCESSOR IS PLACED" BAN NARROWS TO THE RELIC SHELL, and only there.** Its rationale was
  > DOUBLE-APPLICATION: for a **non-constructible** target (the wonder relics) the curator emits that relic's OWN
  > modifier tree as this building's `whenObsolete`, so also placing the relic would deliver the same effect
  > twice. That reasoning does not reach a **constructible** successor, for which no tree is emitted at all — so
  > placing it delivers the effect exactly once. Relic ⇒ tree, never placed; real tier ⇒ placed.
  > ⚑ The enabler's `obsolete` set ([enabler.md §3.2](../enabler.md)) is therefore the **tree-carrying** population —
  > present, non-active, depositing `whenObsolete` — never the removed ones, and never the upgraded ones, which
  > are not in the city to hold.

  The canonical use is a wonder keeping culture/tourism while it loses its working bonus:
  `"whenObsolete": { "culture": { "city": { "flat": 5 } } }`.
- **`replaces`** — succession removal: a successor takes the predecessor's slot, removing it from the buildable set
  once the successor is itself buildable. Authored **target-side** as **`replacedBy.{kind}`** (the entities that
  hard-replace this one, e.g. `replacedBy.units`), mirroring `obsoletedBy`. The §9 `replacedBy` (whole-entity Info-swap
  under a culture-level / game-option) is the **same hard-replace mechanic** — one entity supersedes another — just a
  different trigger. *(Distinct from dormancy, where the predecessor stays inactive-but-kept: `requires.operate.dormant`, §4.3.)*
- **`disables`** — a **law/ban** that **destroys** the target (a policy forbidding a building; repeal ⇒ rebuilt
  from scratch). It is **not** the dormancy mechanism: a target that should go **dormant** while a condition holds
  (e.g. an observatory under blackened skies — it parks and auto-resumes, never nuked-from-orbit) carries
  `requires.operate.dormant` (§4.3), not a `disables`. The choice of *mechanism* IS the fate (enabler §2/§3).

### 4.3 `requires` — what this NEEDS (reversible, target-side)

The means a target needs. Two timings:

```jsonc
"requires": {
  "build":   { "all": [ {"type":"BONUS_STONE","scope":"city","connection":"trade"} ] },
  "operate": { "all": [ {"type":"CIVIC_GUILDS","scope":"empire"} ] }
}
```

- **`build`** — needed to construct it; **greyed** if missing. Checked once, at build.
- **`operate`** — needed to construct **and** keep running; if lost later the built thing goes **dormant**
  (inactive, not destroyed) and wakes when it returns.
- **`spread`** *(CORPORATIONS only)* — needed to **spread** into a city, evaluated by the spread system at
  spread time against the target city's owner — never the enabler. Same condition vocabulary; the grounded
  legacy case is the per-building empire-count need (`{type: BUILDING_X, scope: empire, min: N}`, from the
  corp `PrereqBuildings` table — authored by no shipped corp, served so future data lands live).
- **`build` and `operate` share the SAME conditional vocabulary**, including the **`dormant`** sub-clause. **Units
  carry `build` only** (a trained unit never goes dormant on resource loss; on-map behaviour is out of the cascade's
  `canTrain` scope). A unit's two upgrade relationships are **distinct gates** (the machine: [enabler](../enabler.md)):
  - **`requires.build.dormant.all`** = the unit's *direct* upgrades (minus any that also `replace` it): dormant out of
    the buildable set only when **every** one resolves to a reachable-trainable unit. Fail-safe — default *not*-dormant.
  - **`replacedBy.units`** (the §4.2 `replaces` edge) = the superseders: a genuine removal, dropping the unit from
    buildable the moment any superseder is buildable.

  *(`identity.spawnOnly` (§7) is a separate never-buildable flag, not dormancy. A game-option prereq is a declarative
  `GAMEOPTION_X` condition in `requires.build`; a unit's resource/corp prereq `{HAS_CORPORATION:X}` requires the corp
  ACTIVE, vs a building's bare `CORPORATION_` = present.)*

Each is an `all`/`any`/`noneOf` tree (§3.4). A single bare predicate may be given as a `disabled`/`enabled` clause:

```jsonc
"requires": { "build": { "disabled": "IS_CAPITAL" } }   // can't be built in a city that is already a capital
```

`requires` holds genuine **needs** (resources, civics, religion, count thresholds of *other* types). It does not
hold "how many of myself" — that's `allowed`.

### 4.4 `allowed` — caps

How many of this entity may **exist**. Author the **real number** — the engine permits a build while the current
count is below it. Absent ⇒ uncapped. Two shapes, told apart by the key:

```jsonc
"allowed": { "world": 1 }     // self-cap: at most ONE of me anywhere (a world wonder; a globally-unique tech)
"allowed": { "empire": 1 }    //           at most one per player (a national wonder; a unique unit)
"allowed": { "team": 1 }      //           at most one per team (a team wonder)
"allowed": { "worldWonders": 3, "teamWonders": 2, "nationalWonders": 8 }   // category cap (on CultureLevel)
```

- **self-cap** — a **scope** key (`world`/`team`/`empire`) = "at most N of *me* at that scope." For a building the
  cap scope also makes it a world / team / national wonder.
- **category count-cap** — a **wonder-category** key (`worldWonders`/`teamWonders`/`nationalWonders`; `totalWonders`
  reserved), on `CultureLevel`, caps how many of a category a *city* may hold.

- **SpecialBuilding group cap** — each member authors `identity.specialBuildingType: SPECIALBUILDING_X`; the group
  entity holds the cap (`allowed:{empire:N}`). Member→group is authored, group→members derived.
- **Units have no `team` cap** (units belong to players) — unit caps are `world`/`empire` only; for units `world`
  reads the lifetime-created count and `empire` the live count (buildings keep all three scopes).

The engine owns ignoring caps under the relevant game options, era-scaling, and per-entity exceptions — you just
declare the number. Enforcement reads the [tally](../tally.md) count.

---

