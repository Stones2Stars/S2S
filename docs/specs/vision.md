# Vision — "how far can I see?"

How far an observer sees, and what stops them. **Vision works the way movement works** (owner): a budget spent
walking outward, where open ground costs 1 and difficult ground costs more. A sight of N sees N plots of open
ground, and fewer through anything costlier.

**Vision is ONE family** — `vision` — with three kinds and the scope axis
([DEC-scope-is-an-axis](../architecture/decisions.md#dec-scope-is-an-axis)) saying whose is whose. It had no spec
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

- **A unit's vision STRENGTH is exclusively its own base stat plus its promotions** (owner) — no other source
  raises it. Its **elevation** then comes from the ground it stands on, which is what a hill or a watchtower is
  for. Strength travels with the unit; elevation belongs to the place.
- **A city's elevation is what its BUILDINGS raise** — a tree platform puts the lookout a storey up. The deposit
  is **city-scoped**, and that is not a detail: ⛔ *a building by its very definition cannot add elevation to a
  unit that moves* (owner). It elevates the fixed observer it belongs to and transfers to nobody passing through,
  which is precisely what distinguishes it from an improvement on the same plot.

> **⚖ `elevation`, never "vantage" (owner).** The plain-English word wins: not every reader knows "vantage", and a
> name nobody has to look up beats a precise one that some do.

---

## 1a. THE SCALE — one plot of open ground costs 100

**A baseline of 1 is too low** (owner), and the shipped data shows exactly what it cost: **all 78
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

⚑ **A modder writes the sensible number and nothing else** (owner). `100` is the authored value; readJson's
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

visible(T)  ⟺  Σ cost(p)  over p on the straight line O → T, excluding O  ≤  sight
```

⚑ **Elevation is POSITIONAL, never carried** (owner): a peak has 2 elevation, so a unit standing on the peak has
2 — *and only while on that plot*. Step off and it is gone. That is what makes elevation the ground's property
rather than the unit's, and why it is authored on the GROUND rather than on whoever stands there: the peak is 2
high whether or not anyone is looking at it.

All values are ×100 fixed point internally and human in JSON like every other channel
([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100)) — an author writes `1`, `1.5`, `2`, so
fractional obstruction needs no new scale.

**What the formula gives, by construction:**

- Open ground everywhere ⇒ every `cost` is 1 ⇒ the sum out to distance *d* is *d* ⇒ **sight 1 sees 1 plot,
  sight 2 sees 2**, and so on.
- A jungle costing 2 eats two plots of budget, so an observer sees INTO it and not past it — you always see the
  obstruction itself, never through it.
- An observer's own plot is free: you are not charged to see where you stand.

⛔ **The walk is the STRAIGHT LINE, not the cheapest path — the one place the movement mirror deliberately
breaks.** Movement may route around a mountain; vision must not, because routing around is exactly what would let
you see behind it. Everything else about the two machines is the same shape.

### Why STRENGTH and ELEVATION stay two channels

Both add budget, so they are interchangeable currencies against obstruction — and that IS the mechanic (owner):
**a jungle demands extra strength, and you may pay it either by seeing better (a hunter's promotion) or by
standing above it (elevation).** Two routes to the same view is the design, not a redundancy to collapse.

They stay two channels because they answer different questions — **strength is how well you see, elevation is how
high you stand** — and the difference is where the room is. A spyglass is strength; a tower is elevation. Keeping
them apart now means a later rule that treats them *unlike* (elevation weighed against an obstruction's own
height, so height sees OVER what strength must see THROUGH) needs no re-authoring. ⚑ The 1:1 sum is the SIMPLE
rule, not the final word.

---

## 3. Worked authoring

```jsonc
// a jungle: one extra plot's worth to see through, so it costs 2 plots in all
"vision": { "plot": { "obstruction": { "flat": 100 } } }

// a peak: three plots of elevation to stand on
"vision": { "plot": { "elevation": { "flat": 300 } } }

// a watchtower improvement: raises whoever stands here by a plot
"vision": { "plot": { "elevation": { "flat": 100 } } }

// a unit's own sight, and a promotion sharpening it
"vision": { "unit": { "flat": 200 } }

// tree platforms: the city's lookout goes up a storey
"vision": { "city": { "elevation": { "flat": 100 } } }
```

Ground that authors no `vision.plot.obstruction` costs the open-ground default — **absent means ordinary**, never a special
case to encode.

---

## 4. HIDE AND SEEK — the intent, written down

> **The DATA is collapsed onto this shape; the CONTEST is not yet evaluated by the engine.** The pairing below
> is what the data now says and what the runtime must come to enforce.
>
> ⚑ **WHY THIS MATTERS MORE THAN TIDINESS — the mechanic is playable but not UNDERSTANDABLE (owner):**
> *"it's expressed in icons, and nowhere is it really stated what counters what"*, with four kinds of
> invisibility live in the early game. It rested on the assumption that *"the AI should be able to create
> perfect unit combination counters at all times"* — and humans even less so; *"the designer worked under
> the theory that if he understood it, everyone could."* Add animals that instakill from invisibility with
> absurd strength and the whole thing stops being a mechanic and becomes noise.
>
> ⇒ **Comprehensibility is the requirement, not a nice-to-have.** A rule nobody can state is a rule nobody
> can play against. That is why the pairing is written down here, and why the collapse matters: a detection
> entry now RENDERS itself — *"+1 Detection — units matching IS_DISGUISED"* — through the one per-entry
> renderer ([patterns.md](../architecture/patterns.md) category 5), so what counters what is finally SAYABLE
> in the pedia instead of being inferred from icons.

### The rule the code never states

**ONE detection type counters ONE concealment type** (owner). It is a PAIRING, not a matrix and not a single
contest: a seeker's strength against submarines is weighed against a hider's submarine concealment, and against
nothing else.

The shipped data says so plainly once you know to look — the same key appears on both sides:

| side | carries | means |
|---|---|---|
| the hider | `invisible: INVISIBLE_SUBMARINE` | the METHOD it hides by |
| | `invisibilityIntensity: { INVISIBLE_SUBMARINE: n }` | how well it hides by that method |
| the seeker | `visibilityIntensity: { INVISIBLE_SUBMARINE: n }` | how well it answers that method |
| | `visibilityIntensityRange` | ⚠ a SECOND reach, parallel to vision's |
| | `visibilityIntensitySameTile` | a bonus at zero distance |

**The type IS the pairing.** Nothing in the engine says so, which is why it reads as an arbitrary pile of tables.

### What the data actually uses (measured, not assumed)

14 invisible types across 13 table keys, 477 authorings.

- **Magnitudes are genuinely graduated** — 1 … 26, plus ~100 NEGATIVE entries (counter-detection, something
  actively reducing what a seeker perceives). This texture is real and any redesign keeps it.
- **The per-type CROSS-PRODUCT is largely fiction** — **270 of 355 authoring entities name exactly ONE type**;
  only 10 name four or more, and `CAMOUFLAGE` / `SIZE` / `DISGUISED` are three quarters of everything. The
  14×13 surface serves a quarter of its own data.

### Where it lands — THE `hideAndSeek` BLOCK, never inside `vision`

⛔ **HIDE AND SEEK IS ITS OWN BLOCK AND ITS OWN EVALUATION (owner).** `vision` answers ONE question — *how far
do you see* — and stops there. Whether a unit standing inside that reach is PERCEIVED is a graduated CONTEST
between how well it hides and how well the seeker detects, which is a different mechanic with a different
evaluation. ⚑ **The separation is the deliverable, not tidiness: the legacy engine's hide-and-seek evaluation
bled into its classic-visibility evaluation for years** (owner), so the two must not be expressed in one family
where the same bleed can re-form. The contest data therefore lives in **`hideAndSeek`**, the option-gated block
([json.md §9](json.md): a dedicated system's data lives in its own block, and the module is ON iff that block
exists and is non-empty), and `vision` keeps only the budget — strength, `elevation`, `obstruction`.

⚖ **THE METHOD IS A SKILL, NOT A TAG (owner).** The operative test is *can a promotion grant it?*
([json.md §8](json.md)) — and it plainly can: **optical camouflage** is exactly a late-game promotion INTO a
hiding method. So the method is a [skill](skills.md), which fits on both counts: promotion-grantable, and a pure
boolean enabler carrying no value — correct, because the LEVEL is the `concealment` magnitude beside it.
⛔ It is NOT a [tag](tags.md), and the reason generalizes: a tag says what a unit **IS**, while `camouflage` /
`size` / `political` say how it **HIDES**. **`submarine` is the case that proves the split** — it is a genuine
identity tag AND carries the method skill, because a surfaced submarine is not hidden: *"submarine does not need
to be hidden/invisible, it just mostly is"* (owner).
⚑ **The tag reading also DESTROYED authored data, which is what settles it.** Tags are not promotion-grantable,
so a method named by a PROMOTION had nowhere to land and was dropped on the floor — and **73 promotions author
one** (`CAMOUFLAGE` 40 · `DISGUISED` 21 · `NAVAL_DISGUISE` 16 · `POLITICAL` 15 · `INVISIBLE` 10 · `SIZE` 9 ·
`CLOAKED` 8 · `SUBMARINE` 3), the cloaking line among them. A carrier that cannot hold what the data authors is
the wrong carrier.

```jsonc
// the hider: the METHOD is a skill (promotion-grantable), the LEVEL is a magnitude
"skills":      [ "camouflage" ],
"hideAndSeek": { "concealment": { "flat": 300 } }

// the seeker: sonar answers submarines well and camouflage poorly
"hideAndSeek": { "detection": [ { "value": 500, "unit": "HAS_SUBMARINE" },
                                { "value": 200, "unit": "HAS_CAMOUFLAGE" } ] }
```

A skill is something a unit **HAS**, so the qualifier reads `HAS_<SKILL>` ([json.md §3.5](json.md): `IS_*` is
what the target IS, `HAS_*` is what it has) — the same `{unit: …}` qualifier cargo uses, pointed at the skill
plane rather than the tag plane.

`perceived ⟺ reachable ∧ detection(against that method) ≥ concealment`

⛔ **Detection gets NO reach of its own (owner).** Reach is the §2 budget, already computed; the contest only
ever runs on a plot that budget already granted. That is what retires `visibilityIntensityRange` — a second
range system running beside vision's, with nothing keeping the two in step. Negatives need no mechanism either:
the block's entries sum, so counter-detection is a negative deposit.

### What the collapse did, and what it cost

⚖ **Owner ruling: collapse and rescale, marginal data loss accepted** — *"it's a TB mod after all, it's on
drugs."* The 13 per-type tables (477 authorings across 14 types) are gone; what stands in their place:

| was | is |
|---|---|
| `invisible: INVISIBLE_SUBMARINE` | the **`camouflage`-family SKILL** — the method, promotion-grantable |
| `invisibilityIntensity{X: n}` | `hideAndSeek.concealment` + the method skill |
| `visibilityIntensity{X: n}` · `seeInvisible` · `negates` | `hideAndSeek.detection`, each entry qualified `{unit: HAS_<SKILL>}` |
| `visibilityIntensityRange` + its 3 substrate variants | **gone** — the contest rides §2's reach |
| `visibilityIntensitySameTile`, the per-substrate conditional tables | **gone** — the marginal loss taken deliberately |

**What survived is what the data used:** the 1:1 pairing, graduated strengths, and negatives as
counter-detection (the entries sum, so a negative deposit just subtracts). A promotion carries **both** — the
method skill it grants, and the magnitudes it adds — which is precisely what the tag reading could not express.

## 5. What this model retires

The legacy engine expressed one idea with two unrelated number systems: a **radius**
(`visibilityRange = 1 + terrainElevation + extraVisibility + improvement.visibilityChange`, clamped) and an
**elevation tier** compared per step (`seeFromLevel` against `seeThroughLevel`). Both collapse into the single
budget above, and the `seeFrom` / `seeThrough` / `visibilityRange` members go with them — a feature's
see-through value IS its obstruction, an improvement's see-from IS its elevation.

`MAX_UNIT_VISIBILITY_RANGE` survives as a plain clamp on `sight`. Nothing else of the old shape does.

---

## See also
- [json.md](json.md) — the modifier grammar this family is authored in (§6 the address, §3.9 the entry).
- [modifier.md](modifier.md) — the machine, and the `movement` family this one mirrors (§6: a bespoke resolver
  still reads an ordinary family).
- [naming.md](naming.md) — the `TERRAIN_`/`FEATURE_`/`ROUTE_` ids that carry the ground side.
