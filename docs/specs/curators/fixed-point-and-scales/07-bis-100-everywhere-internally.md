# 4c-bis. ⛔ ×100 EVERYWHERE INTERNALLY — TRUNCATE ONCE, AT THE EDGE

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

> *"This, right here, is why we truncate once, at edge — and use ×100 everywhere internally."*

The ×100 exists so an amount can carry two decimals **to the edge**. A `÷100` anywhere before that edge throws
those decimals away, and if it happens inside an AGGREGATION it throws them away once **per term**.

> **`Σ trunc(xᵢ) ≠ trunc(Σ xᵢ)`** — and the gap grows with the number of terms.

⇒ **A value stays ×100 through every intermediate step — every sum, every per-item calculation, every hand-off
between systems — and reduces exactly once, at the surface that shows it.** An intermediate truncation is a
DEFECT even when each individual truncation is "only" a rounding, because the error is systematic (always
downward) and multiplies by the term count.

⚑ **The worked case, and it is what forced this to be written down: the trade-route list against the food /
production tooltip.** Both were internally consistent and neither had a missing modifier. The LIST was already
right — it sums the per-route yields on the ×100 plane and renders hundredths. The **STORE** was the defect:
`CvCity::updateTradeRoutes` reduced the city's whole trade contribution to a WHOLE UNIT before handing it over,
and the combine then lifted it back ×100 to fold it into TIER-1 BASE. The fraction was not deferred to the edge,
it was destroyed — and the percent stack then multiplied the loss.
⚖ **So the reduce belonged at neither end of that round trip.** The repair is the general one: the stored value
is ×100 like every other amount, the fold is a plain add, and each reader divides at its own point of use.
⚠ **Note which side "looks" wrong and is not.** The per-item surface is the one a reader suspects first, because
it visibly divides once per row; but a per-row render is the EDGE doing its job. Ask which value goes on to be
CONSUMED, and check its scale there.

⚠ **The tell to recognise:** two surfaces reporting the same quantity, each defensible on its own arithmetic,
disagreeing by an amount that scales with how many things were added up. That is not a missing deposit; it is a
reduction in the wrong place.

> **⛔ AN ALWAYS-DEFINED COMPILE GUARD IS WHERE THIS CLASS HIDES, AND IT HID FOR FIFTEEN YEARS.** The reduce sat
> inside `#ifdef _MOD_FRACTRADE`, a guard defined unconditionally in `fbuild.bff` since the mod inherited it — so
> the `#else` half had never once compiled, and the live half read as *one arm of a fractional-vs-whole switch*
> rather than as a plain `÷100` inside an aggregation. A reduce that looks like a deliberate mode is a reduce
> nobody audits.
> ⚑ **The general form, and it is the INVERSE of the attic test** ([AGENTS.md](../../../../AGENTS.md) Conventions
> §Design, which asks whether a guard is defined NOWHERE): a guard defined ALWAYS is equally dead, and it is
> worse, because the surviving branch keeps a companion that justifies its shape. ⇒ When a scale question lands
> inside a `#ifdef`, resolve the GUARD first — if it cannot vary, delete it and re-read the code as the plain
> arithmetic it actually is.

