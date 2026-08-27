# 10. See also

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

- [superseded-ideas.md #42](../../architecture/superseded-ideas.md) — why nothing can be shared or reused across
  the boundary: the 26 + 71 virtuals carry no instancing primitive, so residency (paging/viewports) and art
  payload are the only two dials.
- [memory-footprint.md](../memory-footprint.md) — `shouldHaveGraphics() = IsGraphicsInitialized() && isInViewport()`
  with the `isRevealed` clause commented out (`Engine/CvPlot.cpp:612-615`), the deliberate `PAGE_IN_DIST_* = 999`
  for the Low-cost components, and the inert soft cap (§4).
- [special-systems.md](../special-systems.md#the-great-wall-render--compiled-out-on-purpose) — the `THE_GREAT_WALL`
  off-switch on `beforeSwitch`/`afterSwitch` (§2).
- [spine.md](../../spine.md) — the `[GFX]` domain's tiering and the measured volume of the per-pass `centerUnit`
  line (§3).
- [multimap-zone-rework.md](../../plans/parked/multimap-zone-rework.md) — the parked zone plan keeps `CvViewport` +
  `CvPlotPaging` and would replace `UpdatePaging`'s distance marking with zone membership; its eviction premise is
  §7.
