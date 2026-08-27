# What survived the collapse

> Part of the **[vision](../vision.md)** spec.

The legacy per-invisible-type table pair is retired — [superseded-ideas.md #35](../../architecture/superseded-ideas.md).
**What survived is what the data used:** the 1:1 pairing, graduated strengths, and negatives as
counter-detection (the entries sum, so a negative deposit just subtracts). A promotion carries **both** — the
method skill it grants, and the magnitudes it adds — which is precisely what the tag reading could not express.

⛔ **The CLASSIC system keeps its own datum — `hideAndSeek.method` — and the contest never reads it.** Legacy
carried TWO invisibility planes: the single `<Invisible>` tag (what the classic branch reads with the option
OFF) and the intensity tables (the contest's ancestor). The method-skill SET is the contest's membership and
deliberately wider than the classic plane — a robber contests by disguise and politics yet authors no classic
tag, i.e. it was **never classically invisible at all**. Deriving the classic method from the skill union
therefore made the whole contest-only population classically invisible for the first time ever (border patrols
stopped killing criminals — the live find that forced this datum). The curator emits `method` from the single
tag alone; absent means classically never-invisible ([json.md §9](../json.md)).

## 5. What this model retires

The legacy engine expressed one idea with two unrelated number systems: a **radius**
(`visibilityRange = 1 + terrainElevation + extraVisibility + improvement.visibilityChange`, clamped) and an
**elevation tier** compared per step (`seeFromLevel` against `seeThroughLevel`). Both collapse into the single
budget above, and the `seeFrom` / `seeThrough` / `visibilityRange` members go with them — a feature's
see-through value IS its obstruction, an improvement's see-from IS its elevation.

`MAX_UNIT_VISIBILITY_RANGE` survives as a plain clamp on `sight`. Nothing else of the old shape does.

---

## See also
- [json.md](../json.md) — the modifier grammar this family is authored in (§6 the address, §3.9 the entry).
- [modifier.md](../../cascade.md) — the machine, and the `movement` family this one mirrors (§6: a bespoke resolver
  still reads an ordinary family).
- [naming.md](../naming.md) — the `TERRAIN_`/`FEATURE_`/`ROUTE_` ids that carry the ground side.
