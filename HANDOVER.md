# Handover 2026-07-04 — every city channel open; wellbeing FLIPPED; the unit-modifier on-top law

> **Transient one-time relay** (AGENTS.md handover rules): a task list, nothing more. Every ruling, design and
> result below lives in the durable docs cited inline — this file is deletable without loss. Branch:
> `json-data-migration`, through `97c93b8cf` (~110 unpushed commits; pushing is the owner's call). The game
> runs the latest build from the working tree; the frozen test save reloads the same turn every time.

## State of the world (verify against the cited docs, don't trust this summary)

| Channel | State |
|---|---|
| Yields + commerce | FLIPPED (prior session) — `[SLOT]`/`[GETTER]` nets standing |
| Wellbeing (happy/unhappy/goodHealth/badHealth) | **FLIPPED this session** — `Cascade/CvCascadeWellbeing.{h,cpp}` + `ACCD_WB` slots + live-military-on-top; the four `CvCity` getters return the cascade, `*Legacy` siblings are the net oracles |
| Properties | **7/7 EXACT** per-city source rates (`properties` block on `/computed/cities/wellbeing`) — calculator stage (`Cascade/CvCascadeProperty.{h,cpp}`); slot + solver wiring pending |
| GP-rate | base + modifier **EXACT** on all probes (`Cascade/CvCascadeScalarChannels.{h,cpp}`) |
| Defense (building stack) | **EXACT vs engine recompute**; stored accumulator +60 phantom at P10 (drift class) |
| Maintenance modifier | **EXACT vs parts**; the stored AREA accumulator is PURE phantom (zero XML sources) |
| tradeRoutes | cityExtra EXACT; the player remainder (−5, equal across players) = the VOTE class (`CvGame.cpp:7971`, un-derivable game state) |
| buildRate | **4/5 EXACT first contact**; P10-C16394 reads exactly ×2 (85 vs 170) → task 1 |
| Unit plane (strength/XP/…) | UNBUILT — needs the `unitInput` endpoint (legacy-value-calc-map §12) |

**Load-bearing rulings this session (ledgered/specced — never re-derive):**
- **[DEC-unit-modifiers-on-top] ("full stop")** — traveling unit modifiers NEVER enter a cached calc and NEVER
  take percent modification; live FLAT on top; unit movement invalidates NOTHING. Three legacy per-move storms
  were DELETED on it (garrison-governor `AI_setAssignWorkDirty`, the `noteUnitMoved` property-memo clear →
  end-turn refresh in `CvCity::doTurn`, the siege-blockade invalidation). This fixed the measured
  unit-automation collapse — owner-verified in play ("a lot faster now").
- **End-turn cadence for unit-driven wellbeing** (modifier.md §2b) — the turn-roll IS the cadence;
  pop/specialist churn deliberately does NOT dirty `ACCD_WB`; the small within-turn slot lag is RULED behavior.
- **The STORED-ACCUMULATOR DRIFT class** (modifier.md §2b) — stored-vs-recomputed disagreement is history
  pollution, engine-wrong/cascade-right, repaired at cutover. Convicted on FIVE families now
  (improvement-yields, tech-wellbeing, bonus-happiness, building-defense, area-maintenance).
- **Masked `CvDerivedCacheSet::ensure(iWantMask)`** — a read path only pays its OWN components' recompute
  (the unmasked form made yield reads pay wellbeing walks).
- Two REAL data bugs fixed via parity: the wellbeing STATE_RELIGION gate + the property multi-source merge
  overwrite (`curate_building.py`; both regenerated + committed). StoneBase is RETIRED to spot-verification
  (owner: source-completeness proven — build channels in C++, not C#).

## The task list

1. **buildRate P10 ×2 probe.** P10-C16394's head-order modifier: cascade 85, legacy 170 — exactly double.
   Candidates: the produced unit lists its main combat type in `SubCombatTypes` (check its XML), or a dual-fed
   legacy accumulator. First identify WHAT it produces (the `/state` city payload doesn't carry the order under
   obvious names — check the yields dump or add the order to an emit), then diff the keyed parts. Net:
   `scalars.buildRateCasc/Leg` on `/computed/cities/wellbeing`.
2. **Slot + flip the attributed channels** (GP-rate, defense, maintenance, tradeRoutes, buildRate, properties)
   on the wellbeing increment-D pattern: `ACCD_*` components on `CascadeRateSlots` → refresh dispatch in
   `CascadeAccumulator::refreshComponents` → dirty hooks (building/religion/corp sites carry masks; epoch +
   turn-roll cover the rest) → shadow net clean → getter flip with `*Legacy` oracles. When slotting, cache the
   player-wide walks per player (`WbPlayerRollup` in `CvCascadeWellbeing.cpp` is the pattern;
   `sc_playerBuildings` recomputes per call today — fine for calculators only).
3. **Property engine-side wiring** — feed the solver's source pass from `CascadeProperty::citySourceFlat` +
   `cityUnitFlat` (ON TOP, never cached) + `cityDecayPercent`; then cut the legacy per-building manipulator
   walks + `m_unitSourcedPropertyCache`. #429 spatial (RELATION_NEAR pulses, propagators) stays deferred.
4. **The `unitInput` endpoint** (calc-map §12, an unbuilt spec) — the unit-plane dump that unblocks the whole
   unit modifier machine. Aggregate-fidelity first; per-source attribution later.
5. **Wellbeing residual polish (low).** happy ~3 / unhappy ≤3 vs legacy = the accepted classes riding live.
   The oracle's `*Recomputed` twins + the `extraHappiness` decomposition are all emitted if per-term
   re-attribution is ever wanted.
6. **Standing smalls:** unresolvedFks=2 data typos (FORCE_TEAM_ELIGIBLE, PROMOTION_COMPLEX_AGGRESSIVE);
   `regen_project.py` parked; the data-derived event→cache routing off the compiled deposit index
   (state-repositories end-state, rides the turn-end unified rebuild).

## Session traps (they bit THIS session; they will bite yours)

- **cwd drifts between Bash calls** — `cd /c/code/s2s/s2s` (or `.../Sources` for builds) explicitly in EVERY
  compound command; a git "pathspec did not match" means you are in `Sources/`.
- **Python:** the Install Manager was uninstalled but Store stubs may still shadow `python`/`py`. The real
  interpreter: `%LocalAppData%\Python\bin\python.exe` (documented in `Tools/Migration/README.md`). Curators
  run from `Tools/Migration/`.
- **pwsh cannot read `/tmp/...`** (Git Bash paths) — give pwsh `C:\`-style paths, or grep in bash.
- **The verify loop:** build from `Sources/` (`_Build.ps1 Release build deploy`) → `agentstart.bat`
  (per-session owner permission — re-confirm each session) → poll `http://127.0.0.1:7227/` for `hello world`
  → the mailbox 503s for ~1–2 min after hello (retry loop); the per-city wellbeing action takes up to ~90s.
- **Shadow watching:** connect `curl -sN /events` to a file BEFORE a turn ends; grep
  `[MODIFIER/wellbeing] checked= diverging= casc100=` (casc100 = slot-vs-calc; ~0–7 is the ruled cadence lag)
  and `[MODIFIER/perf]` (carries `wbN`/`wbMsX10`). Every game relaunch kills the stream — reconnect.
- **Owner-in-the-loop deploys:** killing the game mid-play is safe (frozen save) but SAY so and confirm
  "GAME UP" before they resume end-turns.
- **Line endings:** multi-line pwsh `.Replace` on CRLF files silently no-ops — use the Edit tool for
  multi-line edits; verify every text replacement landed (grep after).
