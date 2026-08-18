# Unified Civilopedia — Game-side: clean, single-source content & loading

**The entity single-source problem this doc originally scoped is solved — by the JSON/cascade migration, not the
`#196` declarative-registry route this plan first proposed.** `CvBuildingInfo`, `CvUnitInfo`, `CvPromotionInfo`,
`CvTraitInfo`, `CvImprovementInfo`, `CvCivicInfo` are rebuilt JSON pocos fed from `Assets/Data/**`, one JSON-fed
declaration per class driving read/validation/inspection uniformly — see [`json.md`](../../specs/json.md) for the
current model. That is this doc's entity-content goal, delivered. What's left is the non-entity content taxonomy
below and its guardrails.

**The website is NOT here.** The web Civilopedia — the XML⇄JSON converter, the JSON content store, the React
frontend, the backend, accounts/forum/community — is a **separate project** planned in
[`s2swebsite/unified-civilopedia-plan.md`](../../../../s2swebsite/unified-civilopedia-plan.md) (sibling of this
repo). It is purely *downstream*; its only dependency on this repo is the clean XML this repo produces. The two
halves meet at exactly one seam: **clean, uniform, declarative XML + GameText.**

## Content taxonomy & single sources (game-side)

Three kinds of game content, each with exactly one authoritative home. (Developer reference — `docs/` — is a
fourth kind, already single-sourced under [`docs/README.md`](../README.md); not repeated here.)

| Kind | Single source of truth | Surfaces it feeds (generated/rendered, never re-typed) |
|---|---|---|
| **Game-data entities** — units, buildings, techs, civics, traits, bonuses/resources, improvements, promotions, projects, eras, terrain, features, religions, specialists, … | the loaded `Cv*Info` tables, defined by `Assets/Data/**` JSON and read via the `CvJson<X>Info` poco model ([`json.md`](../../specs/json.md)) | the in-game Python pedia (queries the loaded tables); downstream, the website (via the converter, in `s2swebsite`) |
| **Display / help text** — names, pedia paragraphs, strategy, help | the GameText `TXT_KEY_*` catalog (`Assets/XML/GameText/*.xml`, multilingual); entities hold only the *key* | the in-game pedia (resolved via `CyTranslator.getText`); downstream, the website |
| **Game-mechanics prose** — "how X works" narrative not tied to one entity (active defense, conscription, power, combat odds, BUG options…) | the `NewConceptInfo` Civilopedia text (`TXT_KEY_CONCEPT_*_PEDIA`, `Assets/XML/BasicInfos/CIV4NewConceptInfos.xml`) | the in-game pedia's Concepts/Strategy/Shortcuts sections; player docs under `docs/players/mechanics/` link/transclude it, they do not re-author it; downstream, the website |

The audit lever: a `TXT_KEY` referenced by an entity/concept with no GameText entry is a content bug, covered by
the dead-code/dead-XML pass (see [`json.md`](../../specs/json.md)) and [`codebase-bug-hunt.md`](codebase-bug-hunt.md)
— not re-tracked here.

## Still open

- **De-duplicate authored prose against the data.** Where a player doc restates a value that lives
  authoritatively in XML/`CvCity.cpp`/concept text (e.g. a mechanics page re-typing a cost or requirement
  number), retrofit it to link/transclude the governing concept instead of re-editing a copy whenever the
  source moves.
- **CI validation for the GameText join.** `Tools/XmlValidator.exe -a` and `verify-python-callbacks.py` already
  gate XML/Python; extend with a `TXT_KEY` resolve check (every entity/concept key has a GameText entry) — not
  yet built.

## Guardrails

1. **Single-source rule, per content kind (above).** Reject any change that *re-states* a value already owned
   by XML/GameText/concept text in a second hand-maintained place. Mechanics docs link/transclude; they never
   re-type numbers.
2. **Generation over duplication.** New surfaces over the content are renderers over the loaded model, never
   new copies.
3. **Don't port garbage.** A category is not "done" (or handed downstream) until it is clean and declarative —
   never bless data known to be dead or malformed.
