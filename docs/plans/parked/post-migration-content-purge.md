# Post-migration content purge backlog

> **Status:** parked partition · **Policy:**
> [the keep-unkilled-ideas policy](README.md#parked--out-of-active-scope-plans-kept-for-intent).

These are content reclassifications/collapses deferred to AFTER the #428/#430 cascade migration — the
migration preserves each faithfully for now; clean up later. Per the migration's content-vs-structure rule
([json.md](../../specs/json.md) / [enabler.md](../../specs/enabler.md)), #428 migrates each entity FAITHFULLY in
its CURRENT shape (structure-only migration — it sheds dead *structure*, never live content, and makes no
"should this exist / which bucket" calls mid-move). Purging or reclassifying actual content is a SEPARATE,
DELIBERATE pass that happens once the migration is complete.

## Culture-intermediary bonuses (building → bonus → building)

~410 of 907 bonuses (~45%) are "culture" intermediary bonuses (`BONUS_ABBASID`, `BONUS_ATOMPUNK`, … one per
Culture). Verified mechanism: a Culture national wonder (`SPECIALBUILDING_C2C_CULTURE`) grants its `BONUS_*` via
`ExtraFreeBonuses`, **purely so** the buildings that require it (PrereqAndBonus `<Bonus>`) become buildable —
i.e. `building → bonus → building`. The bonus tier is collapsible to a direct **building → building** enable,
which also kills the "AI tries to trade a culture as a resource" oddity (owner-recalled, rare/stale).

Now **ISOLATED in `Assets/Data/bonuses/cultures/`** — the whole class is one contained folder, so removal is
self-contained. (The other bonuses split `map/` = spawns on the map vs `manufactured/` = produced.) Migrate the
`building → bonus → building` chain faithfully meanwhile; collapse it post-migration.

## Terrain / feature / bonus reclassification

A few plot-substrate entities sit ambiguously across the TERRAIN / FEATURE / BONUS boundary, and there is a
legit case to reclassify some into `Bonus` (or `Feature`) where that fits better. Named example: **`tar`** —
owner: *"I think tar is terrain, it may also be feature… point is… yes"* (its exact bucket is itself unsettled,
which is the point). #428 migrates each FAITHFULLY in its current entity; the reclassification is a deliberate
post-migration content pass. Capture the specific candidates (tar, …) during the Terrain/Feature curation and
revisit here.

## Misc deferred flags

- **advanced-start** → currently `identity.advancedStart` everywhere; a separate pre-game **points currency** is
  pending the advanced-start review.
- **The "great farmer" mechanic** — a special-case the owner called *"a bit of an abomination"*; pending review
  (distinct from the `mapGeneration` bonus-placement grouping, which is done).
- **Map-generation field names** — the `mapGeneration` group's de-Hungarianized-only names
  (`placementOrder`/`constAppearance`/`rands.iRandApp*`, …) want clearer names — TBD.

## See also
- [`README.md`](README.md) — the parked partition index.
- [`../../specs/json.md`](../../specs/json.md) / [`../../specs/enabler.md`](../../specs/enabler.md) — the data-model
  + enabler specs the migration follows (content-vs-structure rule).
