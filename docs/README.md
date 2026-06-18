# S2S documentation — map

All Stones2Stars documentation lives here, split by **audience** so it's obvious where to look (and where new
docs go). Engine/source docs used to live under `Sources/docs/` — they were consolidated here 2026-06-17.

| folder | audience | what's in it |
|---|---|---|
| **[`dev/`](dev/README.md)** | **engineers / agents** | how the code works (`dev/reference/`), in-flight initiatives + specs (`dev/plans/`), and superseded/historical docs (`dev/archive/`). **Start here for any code work.** |
| **[`modders/`](modders/)** | mod authors | how to author game data — the JSON [`datastructure/`](modders/datastructure/README.md) reference, [`modding/`](modders/modding/) guides, the civics/XML FAQ |
| **[`players/`](players/)** | players | gameplay [`mechanics/`](players/mechanics/), the Caveman2Cosmos manual, keyboard shortcuts, Snofru map-pack readmes |
| **[`indexes/`](indexes/)** | everyone (hosted) | the curated catalogs — DESPAIR / REALISM / COMPLEXITY (`.md` + the hostable `.html`). Owner outlet; served via GitHub Pages |
| **`crap/`** | nobody, yet | half-outdated holding pen (e.g. `WebDocumentation/` stale link bookmarks). Triage out of here over time |

Root files: **[`MOD-README.md`](MOD-README.md)** — what the mod is + how to install it (the repo front door).

## Where new docs go

- **How some code behaves today** → `dev/reference/` (one note per class/system).
- **A change/initiative you intend to make** (plan, spec, rollout) → `dev/plans/`.
- **How to author data / mod** → `modders/`.
- **Player-facing** (manual, mechanics, FAQ) → `players/`.
- **Rules & conventions for agents/contributors** → the top-level **`AGENTS.md`** (the single rule home), *not* here.
- **Superseded** dev docs → `dev/archive/` (don't delete history; move it out of the live set).

*Looking for the engineering docs? → [`dev/README.md`](dev/README.md).*
