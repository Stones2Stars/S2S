# Inflation — remodel on ACTUAL EXPENDITURE (parked)

> **Parked forward intent, NOT a #430 item** ([the keep-unkilled-ideas policy](README.md#parked--out-of-active-scope-plans-kept-for-intent)).
> The mechanics as the engine computes them today are
> [economy.md § Gold expense](../../reference/economy.md), which is also where the standing rulings live. This
> page exists because the owner named a PLAN that does not exist yet; it records the intent and the constraints,
> and deliberately proposes no design.

## The ruling

> **⛔ INFLATION IS NOT ACTUALLY USED IN THE GAME, AND #430 DOES NOT REMODEL IT — a conscious decision to cut and
> live with the consequences (owner).**
>
> **⚖ WHEN IT RETURNS IT IS A CASCADE CHANNEL DRIVEN BY ACTUAL EXPENDITURE (owner):** *"we need to have a real
> plan for how it is to be modelled based on actual expenditure."*

## Why the present shape cannot be re-wired into the intended one

The engine's inflation keys on **`hurriedCount`** — a count of how often the player RUSHED — scaled by handicap
and a modifier chain. That is a **proxy for spending, not spending**: a civ that never hurries inflates at zero
however much gold it moves, and one that hurried heavily in the ancient era carries that history forever.

⇒ So the gap is not a missing wire. Re-pointing an inflation read onto a cascade kind would connect a live
consumer to a mechanic that is being replaced whole — the half-migration
([build a new getter surface, never widen a legacy one](../../architecture/patterns.md#-the-two-read-roles--one-grammar-two-answers-owner)) — and the model it should be
connected to has not been decided.

## What the remodel owns

- **The DRIVER** — what "actual expenditure" means concretely, and over what window. Nothing in the tree answers
  this today; it is the substance of the plan the owner asked for.
- **The CHANNEL** — inflation becomes an ordinary cascade channel, so the deposit model, its scope and its unit
  fall out of the ordinary vocabulary ([modifier.md](../../cascade.md)) rather than needing invention.
- **The THREE SPELLINGS.** `inflation.*`, `upkeep.*.inflation` and `hurry.*.inflation` are three unrelated
  addresses sharing one word (the table in [economy.md](../../reference/economy.md)). Which survive, and what
  each means, is this remodel's call — ⛔ **not a convergence to perform opportunistically.**
- **The DORMANT AUTHORINGS.** `hurry.empire.inflation` lost its only consumer when the stranded
  `hurryInflationModifier` accumulator was cut; its kind and its two civic authorings stay inert until this
  lands.

## What is NOT owed

⛔ **Not a repair of the current mechanic.** A defect found in it is cut, not fixed — that is the ruling above,
and the consequence is accepted.
⛔ **Not a kind, a predicate or a channel minted ahead of the design** — machinery for a shape nobody has chosen
is the speculative-verb failure ([triggers.md](../../specs/triggers.md)).
⚠ **Not the rest of the inflation surface either.** `getInflationMod10000` / `getInflationCost`,
`SCALAR_INFLATION`, `UPKEEP_INFLATION` and the event-granted modifier are all still live and still read; they
are untouched until this initiative is taken.

## See also
- [economy.md](../../reference/economy.md) — the mechanics as computed today, and the standing rulings.
- [modifier.md](../../cascade.md) — the channel model the replacement is expressed in.
