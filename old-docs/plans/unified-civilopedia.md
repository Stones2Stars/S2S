# Unified Civilopedia — Game-side: clean, single-source content & loading

Status: **design captured (2026-06-13).** This is the **game repo's** side of the unified
Civilopedia vision: getting the game's *content* — its XML game-data, its GameText, its
mechanics prose — **clean, single-sourced, and well-loaded into the game.** It is worth
doing for the game's own health (a uniform, drift-free data model) independent of anything
downstream.

**The website is NOT here.** The web Civilopedia — the XML⇄JSON converter, the JSON content
store, the React frontend, the backend, accounts/forum/community — is a **separate project**
planned in [`s2swebsite/unified-civilopedia-plan.md`](../../../../s2swebsite/unified-civilopedia-plan.md)
(sibling of this repo, like GameTracker/FpkBuilder). That project is purely *downstream*; its
only dependency on this repo is the **clean XML** the work below produces. This document owns
the upstream half: the data model and its loading. Nothing about the website lives here.

> The two halves meet at exactly one seam: **clean, uniform, declarative XML (+ GameText).**
> This repo produces it; `s2swebsite` consumes it. Keeping that the *only* contract is the
> anti-piecemeal discipline both sides follow.

---

## 1. The single-source principle (why this matters for the game)

> **One source of truth per *kind* of content; every other surface is GENERATED or RENDERED
> from it. No hand-maintained duplicate that can drift.**

This C2C-derived codebase is "in this mess" precisely because the same fact lives in several
hand-maintained places that fall out of sync — the premise of
[`dead-code-xml-pass.md`](dead-code-xml-pass.md) (zombie data, orphan `<Type>`s, dead
`TXT_KEY_*`), and the very pattern the declarative-loading work *deleted* inside the DLL: the
four hand-written methods (`read`/`copyNonDefaults`/`getCheckSum`/ctor) that "all had to agree
field-for-field" ([`declarative-info-loading.md`](../reference/declarative-info-loading.md)).
Every game-content duplicate is a future drift bug. So the game-side goal is a data model where
**a fact is authored once and loaded uniformly** — and the cleanup + loading work below is how
we get there.

---

## 2. Content taxonomy & single sources (game-side)

Three kinds of game content, each with exactly one authoritative home. (Developer reference —
`docs/dev/` — is the fourth kind and is already single-sourced under
[`docs/dev/README.md`](../README.md); not repeated here.)

| Kind | Single source of truth | Surfaces it should feed (generated/rendered, never re-typed) |
|---|---|---|
| **Game-data entities** — units, buildings, techs, civics, traits, bonuses/resources, improvements, promotions, projects, eras, terrain, features, religions, specialists, … | the loaded `Cv*Info` tables, defined by `Assets/XML/**/CIV4*Infos.xml` and read via `CvInfoUtil`/`getDataMembers`. XML is the on-disk form; the loaded table is the in-memory truth. | the in-game Python pedia (queries the loaded tables); downstream, the website (via the converter, in `s2swebsite`). |
| **Display / help text** — names, pedia paragraphs, strategy, help | the GameText `TXT_KEY_*` catalog (`Assets/XML/GameText/*.xml`, multilingual). Entities hold only the *key*; resolution is `CyTranslator.getText`. | the in-game pedia (resolved at runtime); downstream, the website. |
| **Game-mechanics prose** — "how X works" narrative not tied to one entity (active defense, conscription, power, combat odds, BUG options…) | the **`NewConceptInfo` Civilopedia text** (`TXT_KEY_CONCEPT_*_PEDIA`), declared in `Assets/XML/BasicInfos/CIV4NewConceptInfos.xml`. This is the ONE home. | the in-game pedia "Concepts/Strategy/Shortcuts" sections (already render concept text); the player docs under `docs/players/mechanics/` **link/transclude** it, they do not re-author it; downstream, the website. |

**The cross-cutting join.** Entities and concepts carry only `TXT_KEY_*` references; the strings
live separately. The audit lever: a key referenced by an entity with no GameText entry is a
content bug (Tier-3 `TXT_KEY` audit, [`dead-code-xml-pass.md`](dead-code-xml-pass.md) §3.3).

**Not a content kind here:** live game state / telemetry (`CvHttpServer`, GameTracker,
`Benchmarks/`) — internal dev tooling for monitoring AI behaviour, nothing to do with content.

---

## 3. The cleanup work — a uniform, drift-free XML data model

This is the heart of the game-side effort, and it is the prerequisite that makes the content
*worth* anything (to the in-game pedia today, and to `s2swebsite` later). **Don't port garbage:**
a clean, minimal, uniform model first; never bless the current haphazard XML as-is.

- **Finish the `#196` declarative migration.** Hybrid/un-migrated classes (`CvBuildingInfo`,
  `CvUnitInfo`, `CvPromotionInfo`, `CvTraitInfo`, `CvImprovementInfo`, `CvCivicInfo`) still carry
  hand-written remnants alongside `getDataMembers`
  ([`declarative-info-loading.md`](../reference/declarative-info-loading.md) status §). Every
  field pulled into the declarative registry is one fewer hand-maintained agreement that can
  drift — and one field that becomes uniformly loadable/inspectable. The "not yet supported"
  wrappers (2D arrays, pair-vectors, delayed-resolution vectors, non-info enums) are the
  remaining infra gap to close.
- **Run the dead-XML pass.** Orphan `<Type>` entries, dead schema tags, dead `TXT_KEY_*`
  ([`dead-code-xml-pass.md`](dead-code-xml-pass.md) Tier 3). Dead data is drift waiting to
  happen and noise in every surface.
- **De-duplicate authored prose against the data.** Where a player doc restates numbers that
  live authoritatively in XML/`CvCity.cpp`/concept text (e.g.
  [`docs/players/mechanics/conscription.md`](../../../docs/players/mechanics/conscription.md) re-typing
  cost/requirement values), retrofit the doc to **link/transclude the governing concept**, not
  re-edit a copy whenever the source moves.

**Exit:** the targeted categories are fully declarative, their `TXT_KEY` references all resolve,
and no authored doc restates a value that lives in XML/concept text.

---

## 4. Loading the XML into the game — `CvInfoUtil` / `getDataMembers`

How the cleaned XML actually loads, and why the declarative registry is the linchpin (this is
the mechanism the cleanup above feeds, and the reason the data becomes uniformly inspectable):

- `CvInfoUtil` builds, per info object, a `std::vector<WrappedVar*>` where each `WrappedVar`
  holds the field's pointer + its XML `m_tag` (`Sources/CvInfoUtil.h` ~100–128). One declaration
  in `getDataMembers` drives *all* of: XML read (`readXml`), checksum (`checkSum`), default-merge
  (`copyNonDefaults`), and init — replacing the four hand-written methods that used to have to
  agree field-for-field. That convergence is the single-source win at the loading layer.
- **The registry is the runtime data-model catalog.** Because the wrapper list enumerates every
  field's tag and type, the loaded model is introspectable — the basis for validation, the
  `#196` parity checks, and (downstream, in `s2swebsite`) a schema descriptor the converter can
  consume. *Note:* the wrapper has no serialization-to-JSON virtual today; emitting JSON is the
  converter's job and lives in `s2swebsite`, not in the game — see that plan. The game-side
  goal is only **full declarative coverage**, so the model is uniformly loaded and inspectable.
- **In-game pedia stays table-backed.** The Python pedia already reads the loaded tables and
  resolves `TXT_KEY` at runtime (`Assets/Python/Screens/Pedia/PediaBuilding.py:96` etc.) — it is
  *already* a generated surface over the single source. It needs no change here; it is the
  canonical **content-schema reference** (what a Tech/Building/Unit entry shows) for any other
  renderer.

---

## 5. Guardrails (so the data model stays single-sourced)

1. **Single-source rule, per content kind (§2).** Reject any change that *re-states* a value
   already owned by XML/GameText/concept text in a second hand-maintained place. Mechanics docs
   link/transclude; they never re-type numbers.
2. **Generation over duplication.** New surfaces over the content are renderers over the loaded
   model, never new copies.
3. **CI validation, reusing what exists.** `Tools/XmlValidator.exe -a` and
   `verify-python-callbacks.py` already gate XML/Python; extend with a `TXT_KEY` resolve check
   (every entity/concept key has GameText).
4. **Don't port garbage.** A category is not declared "done" (or handed downstream) until it is
   clean and declarative. We never bless data we know is dead or malformed.
5. **Docs governance is already the model.** [`docs/dev/README.md`](../README.md) + the
   `AGENTS.md` "every owner ruling goes into the repo immediately" rule keep knowledge
   single-sourced; this plan extends the same discipline to game *content*.

---

## Downstream

The website / web Civilopedia (converter, JSON contract, React frontend, community) consumes the
clean XML this plan produces and is planned entirely in
[`s2swebsite/unified-civilopedia-plan.md`](../../../../s2swebsite/unified-civilopedia-plan.md).
This repo's responsibility ends at "clean, uniform, declaratively-loaded content."
