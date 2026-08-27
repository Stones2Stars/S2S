# The components

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

The enabler lives in **`Sources/Enabler/`** — its own tree, carrying no `Cascade` prefix
(the enabler and the modifier cascade are two separate systems):

- **`EnablerDomain`** (`CvEnabler.{h,cpp}`) — the §7.1 shape: the tri-state array + the two membership refcount
  planes + the removal-wins formula. One component, instantiated per scope owner.
- **`EnablerKernel`** (`CvEnablerKernel.{h,cpp}`) — the shared edge-apply (`applyEdges`), the `requires` gate
  (`requiresMet` → `cascadeEvalCondition`), the `allowed` cap (`allowedOk`), and the operating-building fixpoint.
- **The eight per-domain enablers** — `CvTechEnabler` / `CvBuildingEnabler` / `CvUnitEnabler` / `CvCivicEnabler` /
  `CvProjectEnabler` / `CvProcessEnabler` / `CvBuildEnabler` / `CvPromotionEnabler`, each its domain's seed +
  event-delta calculator, all routed through the ONE `applyEdges`.
- **`CvEnablerConsumer`** — the enabler's OWN spine consumer, registered by `enablerRegisterConsumer()`. It is
  **LOAD-ACTIVE**: the reseed's in-read emits BUILD the domains through the same appliers play uses
  ([the load reseed](../../../spine/05-the-load-reseed.md#5-the-load-reseed)) — there is no warm-up seed walk. One
  consumer per system; it never routes modifier work.
- **`OperatingBuildings`** (`CvOperatingBuildings.h`) — the §3.2 set type (`active` + `provided` + `obsolete`).

⛔ **The empire-capability union is NOT one of these** — it is a keyed store the PLAYER holds, fed by the tech /
civic / building facts ([capabilities.md](../../capabilities.md),
[plot/city/player each own one live-state context](../../../cascade/10-contexts.md#the-contexts--the-per-scope-live-state-read-surface)). The enabler is a SOURCE of those facts,
never the home of that answer.

