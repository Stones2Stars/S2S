# The settled model rulings

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

- **HAVE model:** the enabler owns NO HAVE store — it ties into the object-owned has-lists that already exist
  (city buildings/religions/corps, player civics/traits/heritages, team techs). Presence stays on the objects; the
  [tally](../../tally.md) stays the count accessor.
- **Evaluator depth:** `cascadeEvalCondition` reads raw object-owned state (legitimate live reads). What is
  event-driven is the MAINTENANCE — which dependents re-gate, when — never the read source.
- **Component model:** one unified component, instantiated per §7.1 owner; delta-apply, never
  mark-then-recompute — no such path exists at all (§7).
- **The root rule:** no implicit "no-edge ⇒ available" engine rule. Start-available entities are authored onto
  `TECH_GAME_START`'s `enables` (§2, curator-derived), the tree is fully connected, a missing edge fails closed.
  The load backfill of `TECH_GAME_START` itself is the ONLY engine special case the model needs.
- **The BONUS axis is GATE-ONLY**: a plot-group-carried bonus NEVER drives tree membership. The
  curator keeps authoring bonus `enables` edges (the reverse-mapped view of the target's retained `requires`
  atom), but the runtime consumes bonus events as pure stateless gate re-checks over the bonus's
  `EDGEF_REQUIRED_BY` dependents. Membership rides tech/building/civic edges + the root; an entity whose only
  inbound edges are bonuses ROOTS, sitting visible-GREYED on its bonus requirement. The one carve-out — a bonus ON
  a plot enabling an improvement's placement (`enables.builds`) — is a live per-plot gate, no domain involvement.

