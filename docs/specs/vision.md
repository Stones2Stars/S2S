# Vision — "how far can I see?"

How far an observer sees, and what stops them. **Vision works the way movement works** (owner): a budget spent
walking outward, where open ground costs 1 and difficult ground costs more. A sight of N sees N plots of open
ground, and fewer through anything costlier.

Three plain nouns, and only three — **vision is vision, and there are obstructions** (owner), with **elevation**
the height that grants sight. Nothing else is part of the vocabulary, because the scope axis
([DEC-scope-is-an-axis](../architecture/decisions.md#dec-scope-is-an-axis)) already says whose is whose.

---

## 1. The three nouns

| family | means | authored on |
|---|---|---|
| **`vision`** | how well an OBSERVER sees — its **strength** | unit · promotion |
| **`elevation`** | how **high** something is, which grants sight to whoever looks from it | terrain · improvement (plot) · building (its city) |
| **`obstruction`** | what the GROUND costs to see **through** | terrain · feature · route |

⛔ **There is no `vision.plot`** — a plot does not see. What the ground contributes is `elevation` (it grants) and
`obstruction` (it costs). Calling either of them "vision" is exactly what produced the legacy tangle of `seeFrom`
/ `seeThrough` / `visibilityRange`, three names sliding between the same two ideas.

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

## 2. The formula

```
sight       = Σ vision.<observerScope>.flat  +  elevationAt(O)
cost(p)     = Σ obstruction.plot.flat  on p                  (open ground = 1)

  elevationAt(a unit) = Σ elevation.plot.flat  on the plot it is STANDING ON
  elevationAt(a city) = Σ elevation.city.flat  on that city

visible(T)  ⟺  Σ cost(p)  over p on the straight line O → T, excluding O  ≤  sight
```

⚑ **Elevation is POSITIONAL, never carried** (owner): a peak has 2 elevation, so a unit standing on the peak has
2 — *and only while on that plot*. Step off and it is gone. That is what makes elevation the ground's property
rather than the unit's, and it is why it is its own family instead of a `vision` member: the peak is 2 high
whether or not anyone is looking at it.

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
// a jungle: expensive to see through, flat ground
"obstruction": { "plot": { "flat": 2 } }

// a peak: high ground, and costly to see past
"elevation":   { "plot": { "flat": 2 } },
"obstruction": { "plot": { "flat": 3 } }

// a watchtower improvement: raises whoever stands here
"elevation": { "plot": { "flat": 1 } }

// a unit's own sight, and a promotion sharpening it
"vision": { "unit": { "flat": 1 } }

// tree platforms: the city's lookout goes up a storey
"elevation": { "city": { "flat": 1 } }
```

Ground that authors no `obstruction` costs the open-ground default — **absent means ordinary**, never a special
case to encode.

---

## 4. The opening this leaves — HIDE AND SEEK

A **strength** axis on the seeing side is what a sane hide-and-seek mechanic needs to contest against.
Concealment is then the mirror of obstruction — a hidden unit raises what a seeker must overcome — so the whole
system becomes the same contest described here instead of the bespoke per-`INVISIBLE_*` intensity tables it is
today. *"Now we have the basis for a real vision system without a bolt-on per question"* (owner).

⛔ **It is NOT folded in here, and the reserved word is `detection`** ([json.md §6](json.md)): map-level spotting
of hidden units is its own system with its own block. What this model contributes is the missing half — a
seeing-strength number to weigh against — and the guarantee that the weighing needs no new vocabulary when it
lands. Do not build it ahead of that, and do not re-purpose `obstruction` as concealment.

---

## 5. What this model retires

The legacy engine expressed one idea with two unrelated number systems: a **radius**
(`visibilityRange = 1 + terrainElevation + extraVisibility + improvement.visibilityChange`, clamped) and an
**elevation tier** compared per step (`seeFromLevel` against `seeThroughLevel`). Both collapse into the single
budget above, and the `seeFrom` / `seeThrough` / `visibilityRange` members go with them — a feature's
see-through value IS its obstruction.

`MAX_UNIT_VISIBILITY_RANGE` survives as a plain clamp on `sight`. Nothing else of the old shape does.

---

## See also
- [json.md](json.md) — the modifier grammar both families are authored in (§6 the address, §3.9 the entry).
- [modifier.md](modifier.md) — the machine, and the `movement` family this one mirrors (§6: a bespoke resolver
  still reads an ordinary family).
- [naming.md](naming.md) — the `TERRAIN_`/`FEATURE_`/`ROUTE_` ids that carry the obstruction side.
