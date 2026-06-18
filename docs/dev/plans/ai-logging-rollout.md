# Plan: roll out the tagged-logging structure across the AI codebase

## Context

We now have three subsystems on a consistent tagged-logging model:
`CvWorkerAI` (`[WAI/*]` → BuildEvaluation.log), `CvHunterAI` (`[HAI/*]` → HunterAI.log),
`CvDecisionAI` (`[DAI/*]` → DecisionAI.log). The legacy free-text BBAI logging
(`logBBAI`/`logAiEvaluations` + `LOG_BBAI_*`/`LOG_EVALAI_*`/`LOG_*_BLOCK`) was deleted,
which removed observability from several AI surfaces (war, unit dispatch, city
production, diplomacy, groups/army, …). This plan extends the new structure to those
surfaces — **one subsystem per branch**, all following one naming/structure convention
so any log reads the same way.

Goal of this document: lock the **convention** and enumerate the **branches**. No code here.

---

## 1. The convention (every branch obeys this)

**Tag prefix** — a short (≈3-char) uppercase mnemonic that is **readily identifiable**;
it need NOT end in `AI`. Prefer a readable abbreviation of the domain (`WAR`, `DIP`,
`ESP`, `COM`…). The already-shipped `[WAI]`/`[HAI]`/`[DAI]` keep their existing form;
new modules use readable codes. Path segments after the slash are lowercase and may
nest: `[XXX/<group>/<detail>]`. (Tag codes are independent of the `<Domain>AI.log`
file name and the optional `Cv<Domain>AI` class name — only the tag changes.)

**Log file** — `<Domain>AI.log` (e.g. `WarAI.log`), written via `gDLL->logMsg`.

**Log function** — `log<Domain>AI(int level, const char* fmt, ...)` added to
`BetterBTSAI.{h,cpp}`, a copy of `logBuildEvaluation` with the file name and the
scope-appropriate gating global swapped in.

**Gating global** — reuse the existing verbosity globals by the decision's natural
scope (no new BUG option, matching the DAI choice):
`gPlayerLogLevel` (player strategy/diplo/espionage/founding), `gTeamLogLevel`
(team/war), `gCityLogLevel` (city production), `gUnitLogLevel`
(unit/group/army/combat). Note: modules sharing a global turn on together — acceptable;
a per-module BUG option is a later refinement if needed.

**Level mapping** (identical to WorkerAI, so levels mean the same everywhere):

- **1** = headline: `begin`, `end`, `baseline`/`flavors`, `best`, final `decision`/`mission`.
- **2** = per-decision: `score`, per-option/per-candidate decisions, `dedup`, `skip` (summary).
- **3** = per-candidate trace: `cand`, per-flavour/per-factor detail, `skip` (detail).

**Shared sub-tag vocabulary** (use these names before inventing domain-specific ones):
`begin`, `end`, `baseline`, `cand`, `score`, `skip`, `best`, `decision`/`mission`, `dedup`.
A reader who knows `[WAI]` should immediately parse `[TAI]`.

**Line shape** — `begin` lines carry the actor id + turn context
(`player=/team=/unit=/city=` + `turn=`). Keep keys `k=v` and space-separated for grep.

**Optional thin class** `Cv<Domain>AI` — add one only when a per-turn **baseline dump**
or turn-scoped state is useful (as `CvDecisionAI` does). Owned by the natural object
(CvPlayer or CvTeam), `onTurnBegin(gameTurn)` wired beside the existing
`m_workerAI/m_hunterAI/m_decisionAI` calls in `CvPlayer::doTurn`/`doMultiMapTurn`
(or the CvTeam turn entry), friend of the classes it reads. Otherwise just add the
log function + instrument in place (no class).

**Header taxonomy doc-comment** — every module documents its full tag table + level
mapping in a doc-comment block (mirror `CvWorkerAI.h:74-103` / `CvDecisionAI.h`). If the
module has no class, put the block atop the owning function or in a short
`docs/dev/<domain>-ai-log.md`.

**Per-branch implementation checklist** (reference implementation = the CvDecisionAI work):

1. Add `log<Domain>AI` to `BetterBTSAI.{h,cpp}`.
2. (Optional) add `Cv<Domain>AI.{h,cpp}`, wire ownership + `onTurnBegin`, register in
   `C2C (VS2019).vcxproj`. Mind unity grouping — add any directly-needed `#include`s
   (see memory `fastbuild-unity-grouping-fragility`); never use range-based-for.
3. Add the taxonomy doc-comment.
4. Instrument the decision functions with `[XAI/...]` at the right levels.
5. Build (`Tools/_Build.ps1 Assert build` from `Sources/`), then deploy + play to confirm
   `<Domain>AI.log` populates; confirm other logs unaffected.
6. Add a `<domain>-ai-log-taxonomy` memory (sibling of `decision-ai-log-taxonomy`).

**Branch naming** — `ai-logging/<domain>` (e.g. `ai-logging/war`, `ai-logging/unit`).
Foundation doc branch: `ai-logging/conventions`.

**Shared-file conflict note** — the only file every branch edits is `BetterBTSAI.{h,cpp}`
(one new `log<Domain>AI` each). These are additive one-liners → trivial merges; just
expect to re-add the function if a branch rebases after another merges.

---

## 2. The tag registry (reserve up front to avoid collisions)

| Domain | Tag | Owner / where instrumented | Log file | Gating | Class? |
|---|---|---|---|---|---|
| Worker | `[WAI]` | CvWorkerAI | BuildEvaluation.log | gPlayerLogLevel | yes — done |
| Hunter | `[HAI]` | CvHunterAI | HunterAI.log | gUnitLogLevel | yes — done |
| Decision | `[DAI]` | CvDecisionAI | DecisionAI.log | gPlayerLogLevel | yes — done |
| Team & War | `[WAR]` | CvTeamAI (in place) | WarAI.log | gTeamLogLevel | optional |
| Unit dispatch | `[UNT]` | CvUnitAI (in place) | UnitAI.log | gUnitLogLevel | no |
| City production | `[CIT]` | CvCityAI (in place) | CityAI.log | gCityLogLevel | optional |
| Group & Army | `[GRP]` | CvSelectionGroupAI, CvArmy | GroupAI.log | gUnitLogLevel | no |
| Diplomacy/Deals | `[DIP]` | CvPlayerAI AI_dealVal/AI_considerOffer | DiploAI.log | gPlayerLogLevel | no — **DONE (current work)** |
| Espionage | `[ESP]` | CvPlayerAI (espionage) | EspionageAI.log | gPlayerLogLevel | no |
| Founding/Settler site | `[FND]` | CvPlayerAI found-value | FoundAI.log | gPlayerLogLevel | optional |
| Combat | `[COM]` | CvUnitAI attack/withdraw, CvCombatModel | CombatAI.log | gUnitLogLevel | no |

(Tags are readable mnemonics, not the old single-letter+AI form. `WAI`/`HAI`/`DAI` are the
only `*AI` holdovers — already shipped, left unchanged. Lock this registry before any branch begins.)

---

## 3. Branches, prioritized

### Tier P1 — heaviest former BBAI coverage / highest analytic value

**`ai-logging/conventions`** (do first, tiny)

- Add `docs/dev/ai-logging-conventions.md` capturing Section 1 (the registry + level
  vocabulary). Single source of truth every other branch links to. No code.

**`ai-logging/war`** — `[TAI]` → WarAI.log (gTeamLogLevel)

- Instrument `CvTeamAI::AI_doWar` (warplan open/abandon, total/limited/dogpile decisions,
  funding-gap gates — the ~20 logBBAI sites removed here), `AI_calculateAreaAIType`
  (area strategy per area), `AI_startWarVal`/`AI_endWarVal`/`AI_declareWarTradeVal`.
- Tags: `[WAR/area]` (per-area AI type, lvl 1), `[WAR/warplan]` (transition, lvl 1),
  `[WAR/cand]` (per-target war value, lvl 2/3), `[WAR/decision]` (declare/peace, lvl 1).
- Optional baseline: `[WAR/begin]` once/turn per team with enemy-power summary.

**`ai-logging/unit`** — `[UAI]` → UnitAI.log (gUnitLogLevel)

- The biggest former surface (~100+ logBBAI in CvUnitAI). Instrument the
  `AI_*Move` dispatcher and the high-traffic routines (`AI_attackMove`, `AI_defenseMove`,
  `AI_settleMove`, `AI_workerMove` handoff to CvWorkerAI, etc.).
- Tags: `[UNT/begin]` (unit + UNITAI + group state, lvl 1), `[UNT/route]` (which AI
  routine chosen, lvl 2), `[UNT/cand]`/`[UNT/skip]` (per-option, lvl 3),
  `[UNT/mission]` (pushed mission, lvl 1).
- Big file → scope to the dispatch + top N routines first; expand later.

**`ai-logging/city`** — `[CAI]` → CityAI.log (gCityLogLevel)

- Instrument `CvCityAI::AI_chooseProduction` end-to-end (focus flags, emergency
  branches, the unit/building/project/process pick) and `AI_bestUnit`/`AI_bestBuilding`
  selection. Complements the flavour-only `[DAI/city/*]` already in DecisionAI.
- Tags: `[CIT/begin]` (city + status flags, lvl 1), `[CIT/focus]` (econ/mil/culture
  emphasis, lvl 2), `[CIT/cand]` (per-building/unit value, lvl 3), `[CIT/decision]`
  (chosen order, lvl 1).

### Tier P2

**`ai-logging/group`** — `[GAI]` → GroupAI.log (gUnitLogLevel)

- `CvSelectionGroupAI` (stack AI update, stuck-loop detection, stack-compare) and
  `CvArmy` (mission execution, leader assignment) — the removed LOG_UNIT_BLOCK sites.
- Tags: `[GRP/begin]`, `[GRP/decision]` (split/merge/move), `[GRP/skip]`, `[GRP/army]`.

**`ai-logging/diplomacy`** — `[DIP]` → DiploAI.log (gPlayerLogLevel) — **DONE in the current working tree** (folded into current work per request, not a separate branch). Implemented in `CvPlayerAI::AI_dealVal` (`[DIP/cand]` per item + `[DIP/dealval]` list total) and `AI_considerOffer` (`[DIP/begin]`, `[DIP/score]` our-vs-their value, `[DIP/decision]` accept/reject incl. denial/grant/renewal). Optional later extension: `AI_doDiplo` (proactive proposals/demands) + `AI_counterPropose`.

- **Known gap this restores:** Phase 2 deleted CvDeal's 14 per-trade-item logs (they were
  `gTeamLogLevel >= 2`), so "why the AI valued/accepted/refused a given trade item" is
  currently invisible. This branch is where that observability comes back, in the new
  tagged form (re-gated under the module's `gPlayerLogLevel`, or keep `gTeamLogLevel`
  if you prefer — decide once for the whole module).
- Instrument:
  - `CvDeal` trade-item valuation across all item kinds: TRADE_TECHNOLOGIES, RESOURCES,
    CITIES, GOLD, GOLD_PER_TURN, MAPS, SURRENDER/VASSAL, PEACE, EMBARGO, CIVIC, RELIGION,
    DEFENSIVE_PACT, PEACE_TREATY (one `[DIP/item]` line per item with its computed value).
  - `CvPlayerAI::AI_doDiplo` (what the AI proactively proposes/demands this turn) and the
    deal-value/`AI_dealVal`-style aggregate + the accept/reject threshold compare.
- Tags: `[DIP/begin]` (player + counterparty + attitude, lvl 1), `[DIP/item]` (per trade
  item value, lvl 3), `[DIP/score]` (summed give-vs-get value vs threshold, lvl 2),
  `[DIP/decision]` (propose / accept / reject + reason, lvl 1).
- This is a strong candidate to promote to **P1** given it's a regression from the BBAI
  removal, not just a net-new surface.

### Tier P3 (smaller / specialized)

**`ai-logging/espionage`** — `[EAI]` → EspionageAI.log (gPlayerLogLevel): `AI_doEspionage`,
mission target/weight selection.

**`ai-logging/founding`** — `[FAI]` → FoundAI.log (gPlayerLogLevel): `AI_foundValue` /
`AI_updateFoundValues` city-site scoring (per-plot site value breakdown — natural `cand`/
`score`/`best` shape; consider the thin-class baseline for the per-turn site map).

**`ai-logging/combat`** — `[BAI]` → CombatAI.log (gUnitLogLevel): attack/withdraw decisions
in CvUnitAI consuming `CvCombatModel` odds (ties into the combat-model work; log the odds
inputs + the go/no-go threshold compare).

---

## 4. Sequencing & verification

- Cut every branch from `main` after the current DecisionAI work merges.
- Recommended order: `conventions` → P1 (`war`, `unit`, `city`) → P2 → P3. They're largely
  independent (different functions); the only shared edit is the one-line `log<Domain>AI`
  in `BetterBTSAI.{h,cpp}`.
- Per branch DoD: Assert build green; deploy + a few AI turns shows `<Domain>AI.log`
  populated at level 1 and richer at level 3; other logs unaffected; taxonomy memory added.
- Keep each branch logging-only and behaviour-preserving (the one allowed exception pattern:
  fixing a latent log bug while keeping the computation identical, as the `[DAI/civic]` fix did).
