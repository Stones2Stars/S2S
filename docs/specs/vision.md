# Vision — "how far can I see?"

How far an observer sees, and what stops them. **Vision works the way movement works**: a budget spent
walking outward, where open ground costs 1 and difficult ground costs more. A sight of N sees N plots of open
ground, and fewer through anything costlier.

**Vision is ONE family** — `vision` — with three kinds and the scope axis
([scope is a separate axis, never folded into the kind](../architecture/patterns/04-the-info-data-out-contract-what-an/03-the-coherent-surface-grouped.md#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king)) saying whose is whose. It had no spec
until now, which is exactly why it had no family: each curator invented a shape, and `seeFrom` / `seeThrough` /
`visibilityRange` ended up as three names sliding between the same two ideas.

---

## 1. One family, three kinds

| kind | means | authored on |
|---|---|---|
| **strength** (memberless) | how well an OBSERVER sees | unit · promotion |
| **`elevation`** | how **high** something is, which grants sight to whoever looks from it | terrain · improvement (plot) · building (its city) |
| **`obstruction`** | what the GROUND costs to see **through** | terrain · feature · route |

⛔ **`elevation` and `obstruction` are KINDS inside `vision`, never families of their own.** They are what vision
is *made of* — the same relationship `defense` has to `bombardDefense`, or `movement` to `moveDiscount` — and
splitting them out would scatter one mechanic across three families to no end.

**Where each observer's sight comes from:**

- **A unit's vision STRENGTH is exclusively its own base stat plus its promotions** — no other source
  raises it. Its **elevation** then comes from the ground it stands on, which is what a hill or a watchtower is
  for. Strength travels with the unit; elevation belongs to the place.
- **A city's elevation is what its BUILDINGS raise** — a tree platform puts the lookout a storey up. The deposit
  is **city-scoped**, and that is not a detail: ⛔ *a building by its very definition cannot add elevation to a
  unit that moves*. It elevates the fixed observer it belongs to and transfers to nobody passing through,
  which is precisely what distinguishes it from an improvement on the same plot.
- **A city's base STRENGTH is engine config, not authored data: the `CITY_VISIBILITY_RANGE` global define**,
  written in PLOTS and lifted to the scale at the read (the `MAX_UNIT_VISIBILITY_RANGE` shape). No data authors a
  memberless `vision.city` strength — every city sees the same base, and buildings differentiate via elevation.
- **⛔ A CITY'S RING IS BOUGHT BY THE BUDGET, NEVER GUARANTEED PAST IT: "a guaranteed innermost vision
  feels wrong — this should be modelled by a default elevation increase and sight strength."** A settlement
  stands tall by construction, so beside its base strength (`CITY_VISIBILITY_RANGE`) every city carries a
  DEFAULT ELEVATION (`CITY_BASE_ELEVATION`, plots, lifted at the read like its sibling); buildings raise the
  elevation further through the channel. The combined budget is what keeps the earliest city's innermost ring
  visible through ordinary obstruction — through the walk, with no bypass, so an extreme obstruction may still
  legitimately blind it.
- **⛔ A city REGISTERS ITS OWN TEAM'S SIGHT like any other observer.** The inherited city block registered only
  vassal / espionage / embassy viewers — legacy leaned on the owned-plot leg's RANGE semantics to light the
  owner's ring, which the budget walk deliberately does not reproduce — so `CvPlot::updateSight`'s city leg
  carries the own-team condition first.
- **⛔ A FOREIGN VIEWER SEES THE CITY, NEVER *FROM* THE CITY.** Espionage, embassy and vassal city
  visibility register the city PLOT alone — a ZERO budget, which collapses the walk's box to the origin — never
  the city's own observer budget: a watcher must not inherit the watched city's eyes and see into the lands
  around it.

> **⚖ `elevation`, never "vantage".** The plain-English word wins: not every reader knows "vantage", and a
> name nobody has to look up beats a precise one that some do.

---

## 1a. THE SCALE — one plot of open ground costs 100

**A baseline of 1 is too low**, and the shipped data shows exactly what it cost: **all 78
obstruction-authoring features carried the identical `1`**, because there was no value between "free" and
"twice as expensive" — forest simply could not be made cheaper than jungle. One plot now costs **100**, the same
"one step = 100" denominator movement already uses, so both families read alike and there is room to say what
you mean.

| quantity | value | reads as |
|---|--:|---|
| open ground — the baseline | **100** | one plot |
| jungle / forest obstruction | +100 | costs 2 plots to see through |
| hills elevation | +100 | one plot |
| peak elevation | +300 | three plots |
| a unit's base sight | 200 | sees 2 plots |
| a sharpening promotion | +25 … +100 | a quarter plot to a full one |

⛔ **ONE SCALE, ALWAYS.** Movement fractured into two — terrain in whole moves, routes in denominator units —
because its baseline could not express a part-step. Vision inherits no such split and must never grow one.

⚑ **A modder writes the sensible number and nothing else**. `100` is the authored value; readJson's
×100 conversion at the boundary is none of their business, and that the engine then works in 10,000 has no real
consequence. Anything finer than a hundredth of a plot authors `0.5` and the fixed point carries it — so there
is never a reason to invent a second unit.

---

## 2. The formula

```
sight       = Σ vision.<observerScope>.flat  +  elevationAt(O)
cost(p)     = Σ vision.plot.obstruction.flat  on p           (open ground = 1)

  elevationAt(a unit) = Σ vision.plot.elevation.flat  on the plot it is STANDING ON
  elevationAt(a city) = Σ vision.city.elevation.flat  on that city

visible(T)  ⟺  Σ cost(p)  over p on the straight line O → T, excluding O AND T  <  sight
```

**⚖ THE SPEND MIRRORS MOVEMENT EXACTLY: a positive REMAINDER reaches the next plot** — as a unit with
a fraction of a move left still enters an expensive tile. A plot is seen on the budget left BEFORE its own cost
is charged; the charge then gates seeing PAST it. Two things fall out for free: any ADJACENT plot is visible to
any observer with a positive budget (no intervening plot to charge — a city's innermost ring needs no
guarantee), and "into the jungle, not past it" is the charge doing its one job.

⚑ **Elevation is POSITIONAL, never carried**: a peak has 2 elevation, so a unit standing on the peak has
2 — *and only while on that plot*. Step off and it is gone. That is what makes elevation the ground's property
rather than the unit's, and why it is authored on the GROUND rather than on whoever stands there: the peak is 2
high whether or not anyone is looking at it.

All values are ×100 fixed point internally and human in JSON like every other channel
([the ×100 fixed-point model](curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) — an author writes `1`, `1.5`, `2`, so
fractional obstruction needs no new scale.

**What the formula gives, by construction:**

- Open ground everywhere ⇒ every `cost` is 1 ⇒ the sum out to distance *d* is *d* ⇒ **sight 1 sees 1 plot,
  sight 2 sees 2**, and so on.
- A jungle costing 2 eats two plots of budget, so an observer sees INTO it and not past it — you always see the
  obstruction itself, never through it.
- An observer's own plot is free: you are not charged to see where you stand.

⛔ **The walk is the STRAIGHT LINE, not the cheapest path — the one place the movement mirror deliberately
breaks.** Movement may route around a mountain; vision must not, because routing around is exactly what would let
you see behind it. The SPEND itself mirrors movement fully (the remainder rule above); only the routing differs.

⛔ **THIS PAGE CARRIES RULINGS OF ITS OWN, AND THE PAGES BELOW CARRY THE REST — read both.** It is not a
map you may skip; the parts your work touches are read END TO END on top of it, and the count that applies is
something you FIND, not something you decide ([AGENTS.md](../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[why strength and elevation stay](vision/01-why-strength-and-elevation-stay.md)** | Why STRENGTH and ELEVATION stay two channels |
| **[the rule the code never states](vision/02-the-rule-the-code-never-states.md)** | The rule the code never states |
| **[what the data actually uses](vision/03-what-the-data-actually-uses.md)** | What the data actually uses (measured, not assumed) |
| **[where it lands the hideandseek](vision/04-where-it-lands-the-hideandseek.md)** | Where it lands — THE `hideAndSeek` BLOCK, never inside `vision` |
| **[what survived the collapse](vision/05-what-survived-the-collapse.md)** | What survived the collapse |

