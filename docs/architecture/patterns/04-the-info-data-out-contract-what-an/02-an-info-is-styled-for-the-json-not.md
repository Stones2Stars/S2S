# An info is STYLED FOR THE JSON, not the legacy field set

> Part of the **[04-the-info-data-out-contract-what-an](../04-the-info-data-out-contract-what-an.md)** spec.

The info's MEMBERS mirror the **JSON entity anatomy** ([json.md §2](../../../specs/json.md): availability ·
provisions · effects = the modifier families · intrinsic · classification · auxiliary), each held as its proper
typed structure. It is **not** a scalar-per-legacy-XML-field. The turnaround is the whole of "make the infos sane":
the JSON model drives the info's shape; the legacy variable set is gone, not force-fed.

- **The exemplar is the classification block — generalize it.** `m_attributes` is a **JSON-derived bitset** (the
  `ClassificationRegistry` ids minted from the authored `attributes` block,
  [the classification-infos registry](../../../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), read by the parameterized
  `CvInfo::hasAttribute(id)` over `clsHasId` — never a legacy `m_bDestroyedOnCapture`, and never a named per-key
  body. Every block gets this shape, one member per block the entity authors (`m_attributes` beside
  `m_amenities` on a building, [json.md §8](../../../specs/json.md)).
- **The defect the rebuild removes** is the legacy-named scalar-per-field with a comment mapping it back to a JSON
  address (`m_iDamageToAttacker` ← `defense.city.counterDamage.damage`; `m_aiRiverPlotYieldChange[]` ←
  `<yield>.city.plots` flats). Those are JSON parsed and **scattered into individually-named legacy variables**;
  the sane form holds the JSON structure and reads it, so a new field is DATA, not a new member + getter.
- **An info holds only ITS OWN side — cross-entity own-output lives on the TARGET.** A building does not
  project yield onto an improvement; the **improvement** says *"I produce this much now, because a building is
  present"* — own-output, the building's presence a condition on the improvement's own deposit ([the deliveryguy ownership rule](../../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule),
  modifier.md §4). So `CvBuildingInfo` carries no `improvements`/`terrains` yield map. This needs **no curator
  re-home**: the **load-time reverse structure** ([reverse lookups are populated once, at load](../../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)) builds cross-entity links both ways at
  readJson, so a modder may author *either* side and the relationship is landed on the other programmatically — the
  improvement ends up owning its yield regardless of which side authored it. A target-keyed map survives on the
  source **only** where the source is the genuine deliverer with no target-owner (governing-deliverer, modifier.md §4).

