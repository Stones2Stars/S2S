# HANDOVER 2026-07-04 (late night) — precipice review DONE, fix batch live; next: finish the worklist, then Cut 1

> # ⛔⛔ THE DOCS HAVE TO BE READ — IN FULL — BEFORE YOU TOUCH ANYTHING.
> Same law as ever (the docs-gate hook enforces it; compaction VOIDS the reads — re-read). The read set:
> **[precipice-review-2026-07-04.md](docs/plans/structural-cleanup/precipice-review-2026-07-04.md)** (TODAY's
> master worklist — every finding, ruling, disposition, and fix of the pre-flip adversarial review) ·
> [scope-packages.md](docs/plans/structural-cleanup/scope-packages.md) (now carries the RULED freshness
> contract + the city-creation boundary) · [duplicate-surface.md](docs/plans/structural-cleanup/duplicate-surface.md) ·
> [cutover.md](docs/plans/structural-cleanup/cutover.md) · modifier-substrate.md · ALL of `docs/specs/`
> (http-endpoints.md gained the E-class fold ruling; modifier.md the free-city naming note).

## State (all live-verified today; WORKING TREE UNCOMMITTED — the owner decides commits)

- The scope-package landing stands verified; ON TOP of it today: the owner-ordered **precipice adversarial
  review** (74 agents: 9-channel legacy writer census + 7-dimension diff review + per-finding refuters) —
  33 confirmed source misses + 11 confirmed findings, ALL dispositioned or fixed in the review doc.
- **Fixes LANDED + deployed (Assets DLL 21:04, matches tree; game runs the configured test save, turn 1337):**
  wellbeing pre-init guards · buildRate net de-tautologized (verified 8/8 vs TRUE legacy) · `cityCreated`
  eager ensure at found/acquire (owner ruling: new city's yields stand immediately) · the culture-slider
  happiness seed (`CommerceInfo iInitialHappiness` — verified per-commerce, NOT handicap) · the Royal-Tomb
  keyed-happiness class (curator → `TARGET_KEYED` empire form, 12-wonder regen committed-to-tree + the
  `foldBuildingKeyed` walk; live-verified ARITHMETICALLY EXACT) · **the E-class fold** (owner ruling: clean
  persisted event/vote stores RIDE IN — `buildingFlat` folds `m_aBuildingYieldChange` +
  `m_aBuildingCommerceChangeEvents`; proof case EVENT_FULLERENES_1 +10 research on Oxford's Chemistry Lab).
- **Rulings today (all captured durably):** per-player-slice SNAPSHOT freshness ("mid-turn yield events are
  not retroactive; start of next turn expected" — scope-packages §1, supersedes the state-repositories
  parity line) · city-creation immediate setup · GA trait flats = flat packages, existing mechanism ·
  "free city" ≠ WLTKD (economy.md: WLTKD = trigger no-anger, sole effect zero maintenance; the "doubles
  GPP" doc claim was FALSE, fixed) · gaps settle via JSON where the model allows · E-class clean stores
  fold as raw state ("past looking for parity — ensure we got everything").

## THE QUEUE (work the review doc top-down)

1. **Wellbeing instrument batch**: emit the player `getExtraBuildingHappiness/Health` tables + the cascade
   `extraB` term on `/computed/cities/wellbeing` → close the L3 LEGACY-side attribution (the open mystery:
   legacy's happy verdict provably does NOT pay the building-keyed amounts (post==pre+keyed exact 3/3) yet
   legacy HEALTH `extraBuildingGood` == the authored keyed HAPPY sums city-exactly 5/5 — cross-application
   or drifted player table; candidate engine-wrong class). Add `/state/players` commerce sliders (doc'd but
   only on `/computed/players` today).
2. **L4 spec-tech specialist happiness** — NB `extraTechHappiness` IS emitted on the YIELDS route
   (`/computed/cities/yields` happiness block), not the wellbeing route; attribution possible today.
3. The remaining review-doc dispositions: live-data suspects (L2 process→commerce stub, L6-L12), the
   **defense wiring** (ruled: all additive percents + the min floor — "so cities don't lose defense on
   flip"), NonStateReligionCommerce as the POLICY read, E4-E6 mixed-accumulator extractions.
4. **Then Cut 1 step 1** (tradeRoutes): fold the CvGame vote store + city WB residue + INITIAL_TRADE_ROUTES
   + the PROJECT_THE_INTERNET world walk (curated `tradeRoutes.world.flat:1`, cascade walks buildings only
   — a confirmed miss); flip with `getTradeRoutesLegacy` oracle. The census-corrected shape: the vote class
   lives on CvGame (already a clean persisted store); CvPlayer's accumulator is fully derivable; CvCity
   extra = building feed + WB pokes.
5. The standing gates before ANY demolition: the final adversarial StoneBase sweep + the self-containment
   audit (per-row state in code-cut-map PREREQ + the review doc).

## Operational gotchas (today's additions)

- The PowerShell tool's cwd DRIFTS across calls — `cd C:\code\s2s\s2s\Sources` in the SAME command as
  `_Build.ps1`, every time (bit twice today).
- `agentstart.bat` loads the CONFIGURED `.env` save — the owner's PLAYED saves (with live events) are
  elsewhere; a relaunch loses the played state (the Fullerenes specimen lives only in the owner's save).
- The docs-gate marker is per session id: `.claude/docs-ack/<session-id>` — only after the FULL read.
- Wellbeing attribution fields split across TWO routes: `/computed/cities/wellbeing` (happinessSources/
  healthSources incl. extraBuildingGood) vs `/computed/cities/yields` (extraTechHappiness, playerExtra).
- Session game-control permission (kill/build/deploy/agentstart) is PER SESSION — re-ask.
