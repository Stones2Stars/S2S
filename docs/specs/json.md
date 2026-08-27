# JSON data spec — the authoritative entity shape & vocabulary

The single source of truth for what a Stones2Stars data file may **contain**. Every game entity — a building,
unit, tech, civic, religion, terrain, … — is **one JSON object in its own file** under `Assets/Data/<type>/`.
The three **cascade** machines — the engine systems that read this data: [enabler](enabler.md) ("can I build
it?"), [modifier](../cascade.md) ("how much?"), [tally](tally.md) ("how many?") — plus `readJson` consume exactly
this shape; the future modder reference is *derived* from this spec, never the other way round.

**The one promise — the data reads cold.** A well-authored file is understandable with zero engine knowledge.
Keys say what they mean; values say what they are. If a shape only makes sense once you know the C++, it is
wrong — the engine is built to fit the data.

> **Validate while you author.** `readjson.exe Assets/Data --render BUILDING_FORGE` parses a file, flags anything
> unrecognized, and renders it to plain English (*"Forge: +25% production; +1 happiness while powered; unlocks
> Crossbowman"*) so you can check "is this what I meant?".

---

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.**
Read the parts your work touches END TO END; the count that applies is something you FIND, not something you
decide ([AGENTS.md](../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[the big picture](json/01-the-big-picture.md)** | 1. The big picture |
| **[anatomy of an entity](json/02-anatomy-of-an-entity.md)** | 2. Anatomy of an entity |
| **[the shared vocabulary](json/03-the-shared-vocabulary.md)** | 3. The shared vocabulary |
| **[availability](json/04-availability.md)** | 4. Availability |
| **[grants pure payload on the considered action t](json/05-grants-pure-payload-on-the-considered-action-t.md)** | 5. `grants` — pure payload on the considered action · `triggers` — when/why → odds → effect |
| **[provides continuous in vicinity supply](json/06-provides-continuous-in-vicinity-supply.md)** | 5a. `provides` — continuous in-vicinity supply |
| **[effects modifier families](json/07-effects-modifier-families.md)** | 6. Effects — modifier families |
| **[intrinsic](json/08-intrinsic.md)** | 7. Intrinsic |
| **[classification unit skillstagsstate building a](json/09-classification-unit-skillstagsstate-building-a.md)** | 8. Classification — unit `skills`/`tags`/`state`, building `attributes` & empire `capabilities` |
| **[auxiliary bespoke sections](json/10-auxiliary-bespoke-sections.md)** | 9. Auxiliary & bespoke sections |
| **[worked examples](json/11-worked-examples.md)** | 10. Worked examples |
| **[quick reference](json/12-quick-reference.md)** | 11. Quick reference |

