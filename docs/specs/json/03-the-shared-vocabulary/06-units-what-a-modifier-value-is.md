# 3.6 Units — what a modifier value *is*

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

A magnitude names the **nature** of the value, not how the engine combines it (§6 owns the combine math):

- **`flat`** — additive amount (`+2` = `2`).
- **`percent`** — additive percent delta (`+50%` = `50`).
- **`multiplier`** — true ×factor, identity `100` (`×2` = `200`).

(Plus `postMultiplier` / `rawPercent` — rare **engine-internal** units, **not for normal authoring**; ignore them
unless porting a specific engine quirk.)

> **Values are human-readable. Always.** `7`, `25`, `1.5` — never ×100. readJson performs the one human→×100
> conversion at load ([the ×100 fixed-point model](../../curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)). **A ×100 value in
> a JSON file is a bug.**

