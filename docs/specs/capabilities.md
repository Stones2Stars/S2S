# Empire capabilities — glossary

The catalogue of **empire/team-wide, tech-unlocked abilities** — the **empire counterpart to unit
[skills](skills.md)**. This is the **glossary** (the namings); the **system** is the [json spec](json.md) §8.
Sibling of skills.md.

> **Started; tech-curated (owner 2026-06-29).** The empire capabilities are the **tech `enabler` channels** (a tech
> *unlocks* the ability); `curate_tech.py` now folds them into the `capabilities` block (`enabler_block="capabilities"`
> in `curate_common.apply_channel` — `{cap: true}`, scope implied), so a tech reads `"capabilities": {techTrading: true,
> …}` instead of a top-level `techTrading: {team:{enabler:true}}` family. The **civic** `enabler` channels are the
> sibling case — **policies enacted by a civic** → the `policies` block (already emitted by `curate_civic`). Entity-level
> boolean gates that are neither (a building's `damageAllAttackers`, a wonder's `buildingOnlyHealthy`) stay as-is. The
> full clean-name list still needs grounding against the engine team-flags.

## What a capability is (recap)
- **Team / empire scope** — applies to the whole civilization, not one unit (the section name carries the scope).
- **Tech-unlocked** — granted by a tech (or civic); monotonic (once unlocked, kept).
- The empire analogue of a unit `skill` (a `skill` is the *unit* ability; a `capability` is the *empire* one).

## Capabilities (first list — names tentative, to be grounded)
| capability | meaning |
|---|---|
| `foundOnPeaks` | can found cities on peak tiles |
| `passPeaks` | units may pass through peaks |
| `moveOnWater` | (early) water movement |
| `techTrading` | can trade techs with other players |
| `irrigation` | irrigation spreads / chains |
| `bridgeBuilding` | bridges — cross rivers |
| `riverTrade` | river trade routes |
| … | *(ground + complete from the engine team flags + tech/civic grants)* |

## Open
- **Ground the full list** against the engine team-capability flags and the tech/civic grants that unlock them.
- The **naming** (legacy flag → clean key).

## See also
- [json.md](json.md) §8 — the system. · [skills.md](skills.md) — the unit counterpart. · [tags.md](tags.md) ·
  [state.md](state.md).
