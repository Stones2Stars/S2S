# The availability READ surface

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

**⚖ THE NEW SURFACE IS BUILT WITHOUT WAITING FOR THE LEGACY DISCONNECT** — assume the legacy surface is
already disconnected and add the new one. The disconnect is its own sweep; gating the replacement on it is what leaves the
machine unreachable indefinitely. Build the new surface as if the legacy one were already gone.

**⚖ BUILDING CONSTRUCTION AND UNIT TRAINING ARE THE SAME PLANE** — one design, two domains, never two
designs. Both are **CITY** concerns for the same concrete reason: the gate needs *"what resources are in VICINITY,
and in the PLOT GROUP"* — city-local supply that no other scope can answer. ⛔ **There is therefore no
player-level construct/train verdict**, and a player-scope `canTrain`/`canConstruct` is not merely redundant, it
is asking at a scope that cannot know. A caller with a city in hand asks that city; a caller genuinely meaning
"anywhere" fans over the player's cities. ⛔ Do NOT mint a maintained player-level union to make the old shape
work: it is duplicated state that must never drift — the same argument that keeps projects/processes player-held
rather than copied per city (§7.1), running the other way.

**ONE READ PAIR PER DOMAIN** — the domain IS the group, and the existing engine enum
(`BuildingTypes`/`TechTypes`/…) is the consumer's vocabulary. The domain set is fixed and small, so the surface
grows by DOMAIN, never by candidate; there is no per-candidate getter and no what-if argument.

| owner | verdict (tri-state) | frontier (caller-owned fill) |
|---|---|---|
| `CvCity` | `getBuildingAvailability` · `getUnitAvailability` | `getAvailableBuildings` · `getAvailableUnits` |
| `CvPlayer` | `getTechAvailability` · `getCivicAvailability` · `getProjectAvailability` · `getProcessAvailability` | `getAvailableTechs` · `getAvailableCivics` · `getAvailableProjects` · `getAvailableProcesses` |
| `CvPlayer` (carve-outs) | `getBuildUnlocked` · `getPromotionUnlocked` | `getUnlockedBuilds` · `getUnlockedPromotions` |

⛔ **Every read is a BARE O(1) FETCH of the maintained tri-state** — no gate runs, no calculator is called, and
`requires` is never evaluated (§7). A missed propagation therefore leaves a visibly wrong verdict instead of
being silently recomputed away ([self-heal is not a backstop](../../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).

**The tri-state is returned WHOLE, answering TREE + GATE only.** HIDDEN vs GREYED is the "why not" the build list
needs (§6), so reducing it to a bool would force a second read to recover it. ⛔ The **QUEUED overlay is
deliberately not folded in**: the domain keeps `FLAG_QUEUED` separate from `FLAG_GATE_FAILED` precisely so
"already queued" stays distinguishable from "requires unmet", and collapsing a queued candidate onto GREYED would
destroy that and misreport why it is not offered. The overlay rides only the two reads that care — the FRONTIER
(fresh offer, queued excluded) and `CvCity::isBuildingContinuable` (reads past it, so the production-check sweep
does not cancel every in-progress build).

