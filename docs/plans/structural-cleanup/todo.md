# #430 — the TODO

> **A LITERAL WORKLIST. What has to be DONE — nothing else** ([DEC-spec-plus-todo](../../architecture/decisions.md#dec-spec-plus-todo)).
>
> ⛔ **NO STATE IN THIS FILE.** No counts, no censuses, no `file:line`, no "measured", no "verified in tree", no
> worked cases, no dispositions, no record of what was achieved. Every one of those DRIFTS, and a drifted claim
> reads as authoritative long after it is fiction — which is how this list started manufacturing work that no
> longer existed. **The DESIGN lives in the specs; the STATE lives in the tree; git history is the record of
> work done.** An item that is finished is DELETED, never ticked, and anything durable it established moves
> into its owning spec first.
>
> ⛔ **The tree is the authority, always.** Before acting on any line here, confirm it against the code
> ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)) — and if it is already done, delete the
> line rather than updating it. Sequencing and governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- Make the `savemigration.txt` parser honour its documented `CUT:` / `RENAME:` prefixes. It changes save-load
  behaviour, so it is an owner call ([save.md §3](../../specs/save.md)).
- Decide whether the `…Times100` on AI unit counts and plot strength is swept with the scale conversion — same
  shape, different nature (fractional SizeMatters counts, not a modifier channel).

## Data — curator

- Curate `culture.unit.garrison` and `costs.empire.perInstance` (flagged in-code, awaiting their batch).
- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`) to its trigger's `chance` ([triggers.md](../../specs/triggers.md)).
  ⛔ Do not unblock the dangling `getSubdueAnimalBonusAI` consumer by minting a kind for it.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only; the owner's recollection is explicitly
  unconfirmed and must not be authored against.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `commerceDoubleTime` → a second deposit gated on `existedFor`; `advancedStart` → resolve the curator's parked
  flag; `pillageGold` → drop.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Emit property pulses through the shared property-source cleaner as trigger entries carrying
  `on`/`relation`/`distance`, instead of parking them verbatim.
- Author the leader→trait assignments. The chain is wired and the slots are authorable; the CONTENT is
  community-owned, so this closes by AUTHORING and never by reconstructing the tables the curator dropped.
- Retire the legacy `largestCity` member once ranked-target-selection EVALUATION lands.
- Re-home `stronglyRestricted` to a `requires.build` civ-membership gate, when NPC civilizations are wired.
- Move corp-HQ revenue (`HeadquarterCommerces`) with the corporation rework.
- Move `paralyze` to the `state` block, once the greenfield `state` model is BUILT ([state.md](../../specs/state.md)).
- Map the flagged unitcombat remainder — map the obvious, flag the unsure, never blunt-purge
  ([unitcombat-tag-mapping.md](unitcombat-tag-mapping.md)).

## Legacy still breathing — delete it

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Move every consumer off the hand-named channel-shaped getters on `CvCity`/`CvPlayer`, then delete the old names.
- Cut the hide-and-seek per-type intensity ACCUMULATORS on `CvUnit` (serialized — the cut carries a
  `savemigration.txt` step; confirm the tag spelling against the stream first). Their replacements are built.
  ⛔ The AI sites SUM inside a loop over every `INVISIBLE_*`, so this is a rewrite, not a rename — a mechanical
  swap would count concealment once per type.
  ⛔ Do NOT sweep the neighbouring `getInvisibleType` / `getSeeInvisibleType` calls: those are live `CvUnit`
  methods sitting in the same blocks.
- Replace the per-type hide-and-seek help text with the detection entry's own render.
- Cut `CvCity`'s hand-rolled dirty caches when the channel that replaces each lands — demolition fodder, never
  conversion targets.
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.
- Delete the legacy `ConstructRequirement` / construct-condition surface once the `requires` RENDERER exists
  (its last consumers are the prereq-block composers).
- Delete `CvPlayer::getBuildingPrereqBuilding` with the last of its text consumers. ⛔ Do not revive the prereq
  table to keep a text line rendering.

## Not built yet

- The PLAYER-ALERT consumer, and the alerts owed to it — they re-attach to the OPERATE CROSSING fact, never
  re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)). Expect the owed list to GROW as
  each legacy mutator is cut; add them together on the facts.
- The amenity CONSUMER side: re-point consumers onto the CITY read and retire the per-flag `CvCity` counters and
  their bespoke per-attribute predicates/facts ([contexts.md](../../architecture/contexts.md)).
- Re-fold a conditioned amenity on a BUILDING grantor when its condition moves (the empire half is covered).
  It wants the condition-dependency route the modifier consumer already derives.
- Add `m_amenities` (and its fold leg) to `CvTraitInfo` / `CvTechInfo` WHEN data authors one — not before.
  readJson already reports an entity authoring a block its type cannot hold.
- The endpoint route table, beyond the stored-vs-oracle documents — it stays empty until the access surface can
  be read THROUGH ([http-endpoints.md](../../specs/http-endpoints.md)).
- The `requires` RENDERER — `CvEntryText` renders modifier entries and conditions only.
- A home for pedia category / sort metadata ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4).
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- A DOMAIN event on a game-option flip, if/when WorldBuilder option toggling is in scope.
- The Python data-fetching library (below).

## The GETTER cut — game objects + AI

> Sequencing, and the ban on bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Move the what-if valuation consumers onto `expected*` — the AI candidate weighting and the build-list hover
  tooltip are ONE call ([patterns.md](../../architecture/patterns.md) THE VALUATION PROTOCOL).
- Decide where the YIELD what-if's supersession-netting lives — the enabler owns `replacedBy`, or the call site
  composes two valuations. ⛔ Not by widening `expected*` with a replaced-buildings argument.
- Build the AS-IF-ADOPTED valuation the CIVIC what-if needs: a caller-held overlay on the context the valuation
  evaluates against, mirroring the enabler's hypothetical-HAVE overlay ([enabler.md §8](../../specs/enabler.md)).
  Until it exists, `AI_civicValue`'s keyed terms are a visible hole.
- Re-express the specialist EXPERIENCE reads as the ENTRY-LIST read over the specialist's own authored entries.
  ⛔ Not an arity fix — folding a keyed entry scope-wide is the silently-plausible-wrong case
  ([modifier.md §5](../../specs/modifier.md)).
- Make `CvCity::getNumBonuses` a BARE FETCH of a maintained per-city count ([enabler.md §8](../../specs/enabler.md)
  open item 2). ⛔ Every per-read re-plumbing of this is the wrong axis and has been backed out before.
  ⚠ `CityContext::tradedBonusCount` re-derives on refresh and is on the wrong side of it too.
- Move the realized-value reads onto the existing group reads (a consumer move, no new surface).
- Delete the per-SOURCE decomposition accumulators: member, `change*`/`get*`, read + write, and the tag named in
  `savemigration.txt` ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).
  ⚠ Audit each `change*` BODY for side-effect riders first ([save.md §6](../../specs/save.md)).
  ⛔ They do NOT each earn a replacement getter — the group read answers the TOTAL, and per-source attribution is
  the ORACLE's job. Their last maintainers (`processBonus`, `processSpecialist`) go with them.
- Design the genuine residue that needs NEW surface: the slider math, the espionage counters, the live combat
  state, `getHappinessTimer`, and the `CvPlayer` unit-upkeep family.
- Hoist the per-commerce valuation in `getBuildingCommerceValue` — it runs once per (candidate × channel) where
  the caller already threads other per-candidate arrays for exactly this reason.

### How to work the compiler census

> Method, not a count to drive to zero. Regenerate it, never trust a stale number:
> `Assert build` from `Sources/`, then `grep -o "error C2039: '[^']*' : is not a member of '[^']*'" <log> | sort -u`.

- ⛔ **The census UNDER-REPORTS by an order of magnitude** — MSVC stops at 100 errors per TU (`C1003`), several
  TUs truncate, and a symbol can be broken with NO diagnostic at all. `grep -rn` over `Sources/` is the authority
  for whether a symbol is gone; never conclude a file is clean from its absence.
- ⛔ **A DROP in the count is not progress either** — the unity batches share the error budget, so deleting code
  anywhere changes WHICH files get to report. Count distinct `(member, class)` pairs, and treat a pair as cleared
  only when `grep` says the symbol is gone.
- **The disposition test, in order:** (1) grep the owning info's header for a RENAMED successor — the rebuilt
  infos are named for the JSON, so a "missing member" is most often a re-point; (2) if none, check whether the
  DATA is still authored — if it is, the INFO is missing a member and that is the defect, not the call site;
  (3) only then is the term dead — DELETE it, never comment it out.
- ⛔ **A blanket rename across `Sources/` is the trap** — info getter names are also live methods on game objects
  (some `DllExport`, i.e. EXE-bound), and the same name can be dead on one info and live on another.
  Distinguishing them is SEMANTIC (what is the receiver?), never textual: leave those and let the compiler name
  the sites individually.
- ⛔ A dangling site whose replacement MACHINE is unbuilt stays dangling — that is the census working. Name the
  missing machine, never the folder.

## Stage 4 — the Python surface

> Contract: [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Read maps:
> [pedia-read-map.md](../../reference/pedia-read-map.md) · [python-read-map.md](../../reference/python-read-map.md).
>
> ⛔ **"Stage 4" is a grouping, NOT a phase to wait for — nothing gates the disconnect.** A dead legacy Python
> getter is an OUTLAW, shot on sight; cutting wrong is cheap and is how a real dependency gets named
> ([roadmap.md](roadmap.md)). ADD to the library whenever a read makes sense, and clear Python-side compile debt
> as you meet it — parking it spends the census budget the rest of the worklist needs. ⛔ Never borrow legacy as
> a "temporary solution" for a read the library does not answer yet: add the read.

- Build the ONE data-fetching library toward COMPLETE (its end state, not a gate on cutting). Build it for the
  pedia (a SHAPE oracle, NOT a coverage oracle — the appendix is enumerable).
- Shoot the dead `Cy*` bindings on sight as the compiler names them.
- Serve enum resolution AND EXTENSION as a first-class operation — BUG reaches three engine enums only that way
  and MINTS new members at runtime, so a library without it forces the banned reach-around.
- Move the `CvGameTextMgr` composers onto rendered entry lines (`appendEntryLines`).
  ⚠ `parsePromotionHelpInternal` ACCRUES across a promotion line, so it needs summed values — give the accrual a
  real home first; do not force it through `appendEntryLines`.
  ⛔ The four WELLBEING composers are NOT `appendEntryLines` targets — a realized per-scope aggregate has no
  entry list to render from, and is a BLOCK ([patterns.md](../../architecture/patterns.md) THE DIVISION OF LABOUR).
- Replace `setBonusTradeHelp`'s whole-database reverse scan with the bonus's own `EDGEF_RELATED`
  ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)) — the entries live on the
  BUILDING, so this is not a renderer swap.
- Re-point the unit consumer getters onto `resolvedValue()`.
- Re-point the unit power-value plane's readers.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.

## Scale conversion

> Method: [fixed-point-and-scales.md § CONVERT BY ARITHMETIC CLUSTER](../../specs/curators/fixed-point-and-scales.md).
> Acceptance per cluster: ZERO new fudge factors at the mixing sites.

- Convert the remaining human-twin getters CLUSTER BY CLUSTER, never getter by getter: yield/food/wellbeing (the
  keystone), commerce, gold/maintenance/upkeep, trade profit, war weariness. Unit experience is self-contained
  and is the one safely parallelizable cluster.
- Decide which single denominator MOVEMENT speaks in — a curator question, and the prerequisite for finishing
  that family's scale ([fixed-point-and-scales.md](../../specs/curators/fixed-point-and-scales.md)).
- Reconcile `getFinalExpense`'s ×10000 inflation modifier when the gold cluster converts.

## Enabler

- Give the UNIT frontier an incremental path — events that do not affect it currently blanket-dirty it, forcing a
  full re-walk. The operating-building fixpoint rides the same triggers.
- Point the AI production decision at the maintained LISTED set, and collapse the `AI_chooseProduction`
  focus-ladder into ONE unified scoring pass ([enabler.md §6/§8](../../specs/enabler.md)). The collapse is an
  AI-architecture change, not a per-loop rewrite.
- Build the bonus VALUATION `AI_baseBonusVal` needs; its per-bonus building loop waits on that machine, not on a
  driver swap. ⛔ Converting the driver ahead of the valuation wires the loop to a surface that cannot answer it.
- Give the OBJECT a player-level held-building aggregate for the HELD-building sweeps
  ([tally.md](../../specs/tally.md): let an object care about itself) — never a side-store.
- Restore the members `AI_techValue`'s remaining sweeps read, then drive them from the tech's own edges.
- Retire the active-set work-list ripple: the fact that justified it is now emitted, so the parallel propagation
  machine goes ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ `emit()` dispatches SYNCHRONOUSLY, so an event chain recurses on the call stack where the work-list iterated
  — design for that; it is not a reason to keep the machine.
  ⛔ Its runaway cap claims to "self-heal at the slice boundary"; that rebuild was REMOVED and nothing heals it.
- Converge the operate reverse index's two PER-ID buckets onto `EDGEF_REQUIRED_BY`.
  ⛔ The axis-flag lists and the PROPERTY band index are NOT convergence targets — a coarse list matches a coarse
  event, and the reverse pass deliberately excludes engine tokens and the plot substrate.
- Close the remaining [enabler.md §8](../../specs/enabler.md) items: residency/counting, plot-group membership
  not trusted from a save, the load-end dormancy fixpoint, the dynamic operate axes.

## Tree / include hygiene

- Retire the `CvInfos.h` umbrella — a hand-careful pass; the lessons and hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- Run the dead-code / dead-XML pass — tooling generates CANDIDATES only; every removal verified against
  source/data and test-loaded, one subsystem at a time.
- Route the `[CTB/work/intransit]` block onto the same gate as every other CTB line so it reaches `/events`.

## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · `CvOutcomeMission::mapFrom` · the property-manipulator helpers ·
  `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
