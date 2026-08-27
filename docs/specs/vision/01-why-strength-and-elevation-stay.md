# Why STRENGTH and ELEVATION stay two channels

> Part of the **[vision](../vision.md)** spec.

Both add budget, so they are interchangeable currencies against obstruction — and that IS the mechanic:
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

> **⛔ MEMBERSHIP IS ASKED BEFORE THE CONTEST, AND HOLDING THE METHOD SKILL *IS* THE MEMBERSHIP QUESTION.** A
> unit is hidden only by a method it actually hides BY, so `hasInvisibilityType(method)` asks whether the unit
> holds `GC.getMethodSkill(method)` and only then applies the negation filters. ⚠ This is the clause that
> carries the whole mechanic: the engine returns INVISIBLE for the first method no seer has registered against,
> **before** the graduated contest is reached — so a membership test that answers yes for every method makes
> every unit invisible to every foreign team, and no amount of authored detection can counter it.
> ⚑ The legacy engine got the same discrimination for free from its per-method `invisibilityIntensityTotal`,
> which the collapse to one method-agnostic `concealment` magnitude removed; the skill is what replaces it.
> ⛔ **The failure direction is FAIL-OPEN TOWARD INVISIBILITY**, which is why this is stated rather than left to
> the code: every way of getting the test wrong hides units rather than revealing them, and a hidden unit
> produces no error, no wrong number and no log line.
>
> ⚠ **DETECTION IS KEYED BY SKILL ID, NEVER BY THE `INVISIBLE_*` INDEX.** `detectionAgainst` takes the method's
> SKILL, so a registration passing the index files a seer's detection under whichever method happens to share
> that number and reads 0 under the one it was authored for — silently, since `setSpotIntensity` stores nothing
> for a zero.
>
> ⚑ **A PROMOTION-GRANTED METHOD REGISTERS THROUGH THE RESOLVED FOLD.** The membership test
> (`hasInvisibilityType`) reads the unit's resolved `hideAndSeek` block — method-skill grants minus revokes
> over info ∪ held promotions ∪ held unit-combat classes, gathered on the promotion/combat facts — never a
> per-read walk of the carriers inside `isInvisible`, which is one of the engine's hottest reads. The
> `noInvisibility` canceller skill rides the same fold.
> ⛔ **The CLASSIC method read stays the INFO's own datum** (§ the classic callout below): only a UNIT authors
> `hideAndSeek.method`, so there is nothing promotion- or combat-class-granted for the classic read to see —
> deriving it from the skill union is the border-patrol bug, never a gap to close.
>
> ⚑ **WHY THIS MATTERS MORE THAN TIDINESS — the mechanic is playable but not UNDERSTANDABLE:**
> *"it's expressed in icons, and nowhere is it really stated what counters what"*, with four kinds of
> invisibility live in the early game. It rested on the assumption that *"the AI should be able to create
> perfect unit combination counters at all times"* — and humans even less so; *"the designer worked under
> the theory that if he understood it, everyone could."* Add animals that instakill from invisibility with
> absurd strength and the whole thing stops being a mechanic and becomes noise.
>
> ⇒ **Comprehensibility is the requirement, not a nice-to-have.** A rule nobody can state is a rule nobody
> can play against. That is why the pairing is written down here, and why the collapse matters: a detection
> entry now RENDERS itself — *"+1 Detection — units matching IS_DISGUISED"* — through the one per-entry
> renderer ([patterns.md](../../architecture/patterns.md) category 5), so what counters what is finally SAYABLE
> in the pedia instead of being inferred from icons.

