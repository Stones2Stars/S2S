# Upgrade chains as a first-class concept — parked

> **Status:** parked forward intent (owner) · **Policy:**
> [the keep-unkilled-ideas policy](README.md).
> The RULING that governs the model today lives in the spec —
> [enabler.md §2, NO BUILDING EVER OBSOLETES A BUILDING](../../specs/enabler.md#2-pass-1--generate-the-frontier-the-enables-family).
> This file carries only the direction that is NOT built.

## The intent

> *"I don't think having a building obsoleting another building is very good design at all; they should be
> considered upgrade chains more than anything else."* — *"which is actually something we can lean into more
> later."* (owner)

A building superseding another building is not obsolescence and not removal. It is a **chain**: a tier that
supersedes the tier below it, where the predecessor parks while the successor stands, and becomes the successor
once it goes obsolete. Both halves are now expressed — but the CHAIN itself, as a thing with an identity and an
order, still is not.

## What is TRUE today (do not re-solve it)

- `obsoletedBy` on a building carries **techs only**; there is no building→building obsolescence edge.
- **The PRESENCE half** is the predecessor's `requires.operate.dormant: [successor, …]`, mirrored from the legacy
  `ReplacementBuildings` (the engine's `setDisabledBuilding` — reversible dormancy). 1,685 buildings carry one.
- **The OBSOLESCENCE half** is `whenObsolete.becomes`, which names the single next tier and is applied by walking
  the chain to the first placeable link ([json.md §4.2](../../specs/json.md#42-obsoletes--replaces--disables--removal-permanent-source-side)).
- The two are different triggers in different directions and a building carries both.

## What is NOT built

The chain is still **implicit as a whole** — each building names its own neighbours, but nothing names the LINE.
Consequences worth having, none of them urgent:

- **No chain identity.** Nothing can say "this is the Bridge line, tier 3 of 12". A player reading a tooltip, the
  pedia rendering a ladder, and an AI weighing whether to invest in a tier all reconstruct it separately, or do
  not have it at all. (Measured: the bridge line runs 14 links; 437 edges have a target that itself upgrades.)
- **No ordering beyond the next hop.** `becomes` gives the NEXT tier and dormancy gives the set of things that
  outrank me, but neither yields the line's position or length — so "how far up this ladder am I" is
  unanswerable without walking.
- **No CHOSEN upgrade, and there will not be one.** The automatic upgrade IS specified — a building becomes its
  successor once it is obsolete, authored on `whenObsolete`
  ([json.md §4.2](../../specs/json.md#42-obsoletes--replaces--disables--removal-permanent-source-side)). What
  does not exist — and is ruled out permanently — is a *player-chosen, priced* upgrade in the unit sense
  ("upgrade this Forge for N gold"): it is trivially exploitable via the cheapest rung, and the interface for it
  would be a nightmare ([superseded-ideas #41](../../architecture/superseded-ideas.md)).
- **AI valuation is chain-blind.** A tier that is one step from obsolete and a terminal tier score alike.

## The UNIT-UPGRADE parallel — how far it actually carries (owner question)

> *"In reality, we can use pretty much the same logic for building upgrades as unit upgrades, can't we?"* (owner)

**The STRUCTURE transfers; the SEMANTICS of the middle part do not.** The unit side is two separate mechanisms
and only one of them has a building counterpart today:

| | unit | building today |
|---|---|---|
| **availability succession** | `replacedBy.units` — the predecessor leaves the buildable set once the successor is buildable (a genuine `replaces` edge, [enabler.md §2](../../specs/enabler.md)) | `requires.operate.dormant` — reversible parking, and the build gate is `build ∧ operate`, so it also stops being offered |
| **instance transformation** | `CvUnit::upgrade` — gated by `upgradeAvailable` + the enabler's LISTED verdict, priced by `upgradePrice`, stands the successor up and retires the source (`CvUnit::convert`) | **nothing exists** |

⚑ **What DOES carry, and it is the valuable half:** the three-part shape — a succession edge, an availability
consequence, and a **transformation verb that is deliberately NOT the creation path**. The reason the verb must be
separate is the SAME defect in both planes: creation settles what is owed a newly-created thing, so routing an
upgrade through it hands that out again — free experience on every unit upgrade
([triggers.md](../../specs/triggers.md) § `modifyUnit`), and a building's first-build block plus its `grants` on
every building upgrade. A building transformation would be `modifyUnit`'s sibling, never `createBuilding`.

⛔ **Three differences, all now SETTLED — do not re-open them as design questions:**
- **REVERSIBILITY — settled by the two halves being different triggers.** Unit succession is one-way; building
  supersession has BOTH: the successor being BUILT parks the predecessor reversibly (presence), and the
  predecessor becoming OBSOLETE turns it into the successor one-way (`whenObsolete.becomes`). They do not
  contradict — a building carries both, and the Forge does.
- **AGENCY — settled: never chosen.** A unit upgrade is CHOSEN and paid for. A building's is a consequence,
  applied automatically. There is no gold-paid building upgrade and there will not be one
  ([superseded-ideas #41](../../architecture/superseded-ideas.md)).
- **CARRIED STATE — settled: nothing is carried.** `convert` exists to carry experience, promotions, damage and
  name across. A building holds essentially no per-instance state, so the building side is a PLACEMENT, not a
  conversion, and needs no state-transfer step.

## When it becomes active

Bring it into the active roadmap and re-ground it then; it is not a gap to close opportunistically. ⛔ In
particular, do **not** mint a `chain` section, a tier index, or an upgrade verb ahead of that work — the standing
ban on minting a vocabulary speculatively for one mechanic applies here exactly as it does on the trigger plane
([triggers.md](../../specs/triggers.md) § What the plane must NOT do).

## See also
- [enabler.md §2](../../specs/enabler.md) — the ruling, the dormancy mirror, and why the two fates cannot coexist.
- [json.md §4.2](../../specs/json.md) — `obsoletes` / `replaces` / `whenObsolete` authoring shapes.
