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
> ⛔ **NO PACING IN THIS FILE EITHER — a todo says WHAT, never WHEN (owner).** *"If things are not properly in
> now, for whatever reason, then that is an ERROR, not something to postpone until whatever unicorn arrives
> later."* So a line NEVER carries a `when X is wired` / `once Y lands` / `until Z exists` clause: that is
> [DEC-no-deferred](../../architecture/decisions.md#dec-no-deferred) wearing a schedule, and it is worse than a
> plain deferral because it reads as a PLAN — the next agent sees a precondition rather than a defect and moves
> on. Something not properly in the tree is an ERROR; write it as one.
> ⚠ **A genuine DEPENDENCY is stated as a dependency, not as a date:** *"X cannot be expressed until the verb
> exists, so the verb is the work"* names a thing to DO. *"X, when the verb lands"* names nobody's job and
> nothing to do. ⛔ The ONE exception is an explicit owner-ruled ORDERING, and it is marked as the owner's
> ruling rather than as a property of the item.
> ⚑ **These lines ROT the worst**, which is why the ban is here beside the no-state one: a condition nobody
> owns is never met, so the item ages into scenery — and the tree grows a hole that reads as scheduled.
>
> ⛔ **The tree is the authority, always.** Before acting on any line here, confirm it against the code
> ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)) — and if it is already done, delete the
> line rather than updating it. Sequencing and governing rulings: [roadmap.md](roadmap.md).

## Blocked on an owner ruling

- Rule on how an ENACTED / HELD state pseudo-building (an ordinance, a culture `C_AC_*` set, a folklore
  requirement) expresses the CHOICE behind it — the gate faithfully evaluates data that says every city already
  holds the choice, so each offers unconditionally ([enabler.md §3](../../specs/enabler.md)).
  ⛔ Reading ACTIVE instead of PRESENT at the gate fixes nothing — the mechanism is spelled out in the spec.
  ⛔ Do NOT invent an ordinance-enactment mechanism to close it; the ruling is what the missing condition should BE.
  ⚠ A data-model answer triggers the curator + regen in the same work item
  ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
- Decide the ENABLER's load-gating policy: `CvEnablerConsumer`'s header declares the consumer LOAD-ACTIVE with no
  `spineGameLoadInProgress()` suppression, while `CvBuildingEnabler`/`CvUnitEnabler` suppress on it. One of the
  two is wrong.
  ⛔ Not a delete-the-guards sweep — [enabler.md §7.1](../../specs/enabler.md) sanctions both policies; only the
  MIX is the bug, so removing the guards SWITCHES policy rather than tidying it.
  ⚑ Deliverable: the ruling, plus making the header comment and the guards agree.
- Rule on the operating-set seed's ANNOUNCE suppression in `CvEnablerKernel`, which silences the emit while the
  save streams. Suppressing an EMIT to fix a CONSUMER is banned outright ([event-spine.md](../../specs/event-spine.md)),
  and the reason given is real rather than lazy: the set is idempotent but the DEPOSITS are not, so an in-read
  announce double-applies into the modifier packages — and the enabler's own stored-vs-oracle tripwire is blind
  to it, because the damage lands on the other plane. ⇒ The fix belongs on the CONSUMER side (the modifier
  declining an in-read activation it will fold at the drain), never on the emit; which shape it takes is the
  ruling.
- Make the `savemigration.txt` parser honour its documented `CUT:` / `RENAME:` prefixes. It changes save-load
  behaviour, so it is an owner call ([save.md §3](../../specs/save.md)).
- Decide whether the `…Times100` on AI unit counts and plot strength is swept with the scale conversion — same
  shape, different nature (fractional SizeMatters counts, not a modifier channel).
- Rule on the river-attack term for a CITY defender (`CvUnit::getDefenderCombatValues`). Its two branches
  disagree: attacking across a river hands an ordinary defender `-RIVER_ATTACK_MODIFIER`, while the city branch
  is `min(0, riverDefensePenalty - RIVER_ATTACK_MODIFIER)` — capped at zero, so a city defender can never
  receive that bonus and the building value only bites if authored below the define. Every authoring is above
  it, so the term is inert whatever its scale. ⛔ Not an agent fix: changing it changes combat, which
  [validation.md](../../specs/validation.md) makes a per-case owner authorization, so the choice (drop the
  mechanic, or correct the branch to reduce the crossing penalty) is yours.
- Build RANKED-UP Size-Matters units from the build menu, so a late-game player is not merging hundreds of units
  by hand. The model is settled ([json.md §9](../../specs/json.md)) — this closes by IMPLEMENTING it, never by
  re-deciding it.
  ⚖ Not wanted yet (owner) — waits until wanted; nothing else waits on it and no half is in the tree.

## Data — curator

- Attach the ruling-16 trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
  `combat.nukeInterception`, `diplomacy.techShare`) to its trigger's `chance`
  ([triggers.md](../../specs/triggers.md)).
  ⚠ `techShare` additionally needs its KIND retired: unlike the rest of the set it was minted as
  `DIPLOMACY_TECH_SHARE`, so the re-home is a curator change AND a vocabulary removal.
- Give the §3.9 entry grammar a payload-less form so a carrier can state a cargo RESTRICTION with no capacity of
  its own, then author the flagged carriers ([modifier.md §6](../../specs/modifier.md)).
  ⚠ Settle in-game first whether the ancient transports are civilians-only — unconfirmed, do not author against
  a guess.
  ⛔ Do the grammar + authoring FIRST. Only then: make the load/board gates evaluate the `unit:`-qualified
  `cargo.space` entries per candidate (today only the unqualified capacity sum is read), and strip the
  promotion-side WHAT overrides (domain / special-unit / SM-not-special) — CURATOR + INFO + ENGINE together,
  since a 0-capacity carrier needs the payload-less entry to keep its restriction once they're gone.
- Re-home the remaining `identity` EFFECT keys to the block that already exists for each
  ([json.md §7](../../specs/json.md)): constraints → `requires`/`allowed`; `diploVoteType` → the top-level
  `voteSource` section (and rename the getter off the legacy XML tag); `tradeable` → the `canTrade` block;
  `advancedStart` → resolve the curator's parked flag; `pillageGold` → drop; `base.airCombat` → the `strength`
  family, where every other unit's base value already lives.
  ⚠ `espionagePoints` rides the missions/`CvOutcome` carve-out — its channel is settled, only its authoring home waits.
- Author the leader→trait assignments. The chain is wired and the slots are authorable; the CONTENT is
  community-owned, so this closes by AUTHORING and never by reconstructing the tables the curator dropped.
- Author per-leader `ai.personality.researchSearchDepth` ([enabler.md §8](../../specs/enabler.md)). Same shape as
  the trait assignments: the read is wired and an unauthored leader takes the default, so this closes by
  AUTHORING, never by touching the enablement valuation.
- Retire `MISSION_RANGE_ATTACK` (`canRangeStrike` / `rangeStrike` / `INTERFACEMODE_RANGE_ATTACK`) — a SECOND
  ranged-attack mechanic sitting beside the removed ranged bombard in every mission switch, which the bombard cut
  did not cover. `canRangeStrike` refuses `DOMAIN_AIR`, so it is the non-airplane class
  [superseded-ideas #24](../../architecture/superseded-ideas.md) already rules out.
  ⛔ Do not assume the bombard cut reached it.
- Rename the `dcmFighterEngage` skill and the `DCM_FIGHTER_ENGAGE` global off the mod-provenance prefix
  ([skills.md](../../specs/skills.md)) — the mechanic stays, only the name changes.
- Move corp-HQ revenue (`HeadquarterCommerces`) with the corporation rework, and with it the two corp shapes no
  corporation currently authors: the HQ free unit and corp-vs-corp exclusion
  ([json.md § `headquarters`](../../specs/json.md)).
  ⛔ Neither is machinery to build ahead of data authoring it.
- Retire `DOMAIN_IMMOBILE` — immobile is not a domain ([json.md §7](../../specs/json.md)), a domain is the medium
  a unit operates in.
  ⚠ Units still author it, so re-author the data FIRST; only then cut the consumers: the enum entry, ~21
  engine/AI sites, `CIV4DomainInfos.xml`, its game text, and a Python read. It is terminal in the enum (before
  `NUM_DOMAIN_TYPES`), so removal shifts no other id.
  ⛔ Several sites treat it as a live case, not a dead one — read each site, never a delete-the-case sweep.
- Decide what an EMPIRE-scope `range` deposit would mean, if one is ever authored — none exists today, every
  authored `range` is unit-scope.
  ⛔ Not a reader to build ahead of data ([DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)); it
  closes through `curate_trait` + a regen ([modifier.md §4](../../specs/modifier.md)), never by hand-editing the
  emitted JSON.

## Legacy still breathing — delete it

- Build the missing PLACERS for the queue-excluded, self-capped entities that have none: achievements, relics,
  traditions, national beliefs, and the `C_AD_*` culture set ([enabler.md §3](../../specs/enabler.md) — a corp
  HQ already has one, `CvGame::setHeadquarters`). They are placed nowhere today, so their effects reach nobody.
  ⚑ The relic half is the `constructs` outcome verb tracked below; achievements and the culture set need their
  own award path — the same question as the ENACTED/HELD choice above.
  ⛔ Not a return to the blanket pass — that's the retired reading ([modifier.md §5](../../specs/modifier.md)).

> The standing rule (purge violently; blast radius is the signal; the worst offenders are the ones OFF the core
> loop) is [roadmap.md § LEGACY STILL BREATHING](roadmap.md). ⚠ KNOWN-INCOMPLETE — legacy found anywhere else is
> killed on the same terms. ⛔ Never record a found legacy surface as acceptable or "kept until X".

- Retire the building-COST-modifier accumulator and move its readers. The writer is already gone — the curator
  re-homed the legacy source-keyed cost map onto the TARGET as a conditioned own-cost entry, so the accumulator
  reads zero. Point readers at the target building's own `costs` entries instead of the player-side map.
  ⚠ Distinct from its `buildRate` sibling, which stays source-keyed and converts to an entry-list read — the two
  look alike and do not resolve the same way.
- Serve the team improvement-yield GRANT through the new Python surface. The wonder events call it from
  `CvEventManager` and the binding they called is gone — by design
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)): it comes back as the library's own
  read, never a restored `CyTeam` binding. Until then the engine stores and serves the value but nothing can
  grant one.
  ⚑ Grants recorded without an event id carry `-1` (unattributed) by design — the events rework threads the
  real id; the store already carries the field for it.
- Repair the `savemigration.txt` REPLACEMENT-OBLIGATION notes that no longer resolve
  ([save.md §3](../../specs/save.md)). The wellbeing cut notes are the known cluster to start from — they name
  the archived bespoke substrate ([superseded-ideas #14](../../architecture/superseded-ideas.md)) rather than
  what actually serves them now.
- Cut `CvPlayer::processCivics` — the legacy civic ACCUMULATOR PUSH. A civic's deposits reach its cities by
  rolling DOWN the scope chain ([modifier.md §1](../../specs/modifier.md)); pushing them into player-side
  accumulators is the STORED-ACCUMULATOR DRIFT class
  ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).
  ⛔ Not uniform — collapsing the three kinds is the mistake: modifier-channel deposits cut per that DEC;
  per-flag POLICY counters retire onto `EmpireContext.policies` instead
  ([contexts.md](../../architecture/contexts.md)); genuine non-cascade state (the revolution index,
  `changeMaxConscript`, `changeSpecialistValidCount`, hurry counts) stays.
- Sweep the WRITERLESS SERIALIZED ACCUMULATORS — run the detector in
  [state-repositories.md § THE LEGACY-ACCUMULATOR CUT](../../architecture/state-repositories.md) over
  `CvCity`/`CvPlayer`. Known instances to start from: `CvCity::getBonusDefenseChanges` and
  `CvPlayer::getBonusCommerceModifier`.
- Serve the UNIT-QUALIFIED entry sum. `CvModEntry::unitQual` compiles the qualifier and both the gather and the
  valuation deliberately skip it
  ([DEC-unit-modifiers-on-top](../../architecture/decisions.md#dec-unit-modifiers-on-top) — a unit-carried value
  rides live, never cached); what's missing is the read that sums entries matching a unit predicate over the
  live sources, so those deposits reach nobody today.
  ⚑ The live case: `happiness.empire.cities.{unit: IS_MILITARY}` ([modifier.md §6](../../specs/modifier.md)) —
  the consumer wants the epoch-stable per-unit value, folded by the city as `perUnit × liveCount`.
  ⛔ Not `keyedTargetSum` with a wider signature — that answers a NAMED target id, this filters on a PREDICATE
  per candidate unit; a different axis, don't fold them.
- Move every consumer off the hand-named channel-shaped getters on `CvCity`/`CvPlayer`, then delete the old names.
- Cut the hide-and-seek per-type intensity ACCUMULATORS on `CvUnit` (serialized — the cut carries a
  `savemigration.txt` step; confirm the tag spelling against the stream first). Their replacements are built.
  ⚡ The four accessor families have no call sites at all, so this is a plain deletion.
  ⛔ Leave `getInvisibleType` alone — a live `CvUnit` method, unrelated.
- Retire the direct `gDLL->logMsg` / BetterBTSAI log-helper call sites and the log-level globals they gate,
  wholesale as each domain migrates onto the spine — never tidied in place.

## Not built yet

- **Teach the unresolved-FK census the catch-all-token vocabulary** ([json.md §3.1](../../specs/json.md)) so it
  stops reporting engine tokens (`CITY`, `TEAM`, `ERA`, the slider rates, `CULTURE_PERCENTAGE`,
  `DISTANCE_TO_GOVERNMENT_CENTER`, `CORPORATION_LEVEL`, …) as unresolved ids — they bury the genuinely unresolved
  ones. ⚠ Settle the `TAG_*` entries in the same pass — they are classification ids minted at load, same
  resolvability question.

- **De-serialize `CvPlayer::m_bonusExport` / `m_bonusImport`.** Re-derive both maps from the player's held deals
  on load instead ([enabler.md §8](../../specs/enabler.md) — the deal stays serialized, the derived counts do
  not). Uniform soft-remove cut: delete member + read + write, name the tags in `Assets/savemigration.txt`, no
  `WRAPPER_SKIP_ELEMENT` ([save.md §3](../../specs/save.md)).

- **Cut the DEFERRED BONUS-PROCESSING BRACKET together with `processBonus`.**
  `CvCity::startDeferredBonusProcessing` / `endDeferredBonusProcessing` exist only to keep the legacy per-bonus
  apply (`processBonus`) off the intermediates of a merge or load-time rebuild — once nothing calls
  `processBonus`, the bracket has no remaining purpose.
  ⛔ Do not cut it first: cutting it while `processBonus` is still live would run the legacy fold on every
  intermediate of a rebuild.

- **Cut the citizen valuation's WHIP term (`iSlaveryValue`)** — the whip/draft trade it models does not hold at
  S2S's real costs ([citizen-assignment.md § THE WHIP TERM IS NEVER WORTH TAKING](../../reference/citizen-assignment.md)).
  ⚠ It is the tail, not the head: whipping/drafting are what want revisiting, and this term goes with whatever
  that decides — do not tune it in place.

- **Watch the five decisions `AI_countGoodTiles` now feeds** — the ×100 conversion made them live for the first
  time ([citizen-assignment.md](../../reference/citizen-assignment.md), the `AI_getPlotMagicValue` measured
  instance). ⛔ Not a scale defect to repair — it is new behaviour to OBSERVE. Judge it on the `[CIT/assign]`
  census from a real save, never against a remembered figure
  ([DEC-baseline-is-a-smell-test](../../architecture/decisions.md#dec-baseline-is-a-smell-test)).

- **Wire `AI_setAssignWorkDirty` onto the event spine, per the ruled trigger set**
  ([citizen-assignment.md § Dirtying the assignment](../../reference/citizen-assignment.md)) — replace the
  hand-wired call sites and the unconditional per-turn sweep with spine listeners for that set
  ([DEC-flag-is-fossil](../../architecture/decisions.md#dec-flag-is-fossil)).

- **Retire the `CARGO_NAVAL` / `CARGO_MISSILE` kinds onto the tag-predicate shape** — `CvUnit::cargoSpace`'s
  "+N hold when the CARRIER is `DOMAIN_SEA`" / "when its cargo is `SPECIALUNIT_MISSILE`" author as a plural
  `units` target with a tag predicate instead (`cargo.space.empire.units {IS_SEA}`), per the ruled shape
  ([modifier.md §6](../../specs/modifier.md)).
  ⛔ Do not carry the `navalCargo` name forward — it is an empire-wide hold bonus scoped by the carrier's
  domain, not naval cargo or a naval-unit hold.

- **Share `InfoValuation::plotScaledYield`'s two operands with the what-if plot read.** `plotBaseYields` — the
  isolated plot-as-base calc every what-if goes through — does not take them yet, so a what-if answers the
  pre-scaling number while the live plot answers the scaled one
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation): the arithmetic is
  shared, the operands must be too).
- **Re-point `CvPlayer::getExtraYieldThresholds` / `getLessYieldThresholds`, or delete them.** They read the
  channel through `realizedAtEmpire`, which SUMS — but the interval is the smallest positive one held, so the
  summed answer is wrong by construction. Their only callers are the Python bindings; the live feed uses
  `updateExtraYieldThreshold`'s min selection instead.

- **Make the curator emit `tradeRoutes` as the base section the spec declares** ([json.md §2](../../specs/json.md))
  — move the count/cap rows and `TradeYieldModifiers` (currently emitted as `<channel>.<scope>.tradeRoute`) into
  it, with `coastal`/`foreign`/`sharedCivic` re-authored as predicates, not members. Both currently land as
  `unkinded-member` and produce nothing.
  ⛔ Recurate and regenerate in the same change ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision)).
- **Run the LEGACY-ACCUMULATOR CUT detector over `CvCity` and `CvPlayer`** and cut every hit with the uniform
  mechanism ([state-repositories.md § THE LEGACY-ACCUMULATOR CUT](../../architecture/state-repositories.md)).
  Work it together with the unkinded-member census below — a matched pair is the data and carrier sides of one
  missing quantity. ⚠ A genuine one-shot event-state writer (`applyEvent`) correctly stays serialized
  ([save.md §3](../../specs/save.md)); every other hit re-points.
- **Land the UNKINDED MEMBERS — authored deposits `readJson` drops.** ⛔ Read the `[READJSON] unkinded-member
  <family>.<member>` census before anything else on this — it is the authoritative worklist, one grep of the
  load log. Each one resolves to exactly one disposition:
  - **a genuine KIND to mint** — the combat defensive/capture members, `gold.headquarters` /
    `culture.headquarters` (riding the corporation rework);
    ⚠ check the address before minting — `upkeep.upgradePrice` is a COST, not upkeep (→ `costs.empire.upgrade`);
    `happiness.nonStateReligion` is the §3.7 counted-kind filter `{religion: "!IS_STATE_RELIGION"}`
    ([json.md §3.7](../../specs/json.md)); `cityCapture.resistance` is trigger-plane, deliberately unkinded
    ([triggers.md](../../specs/triggers.md)) — minting a row for any of the three carves the rollerskate in;
  - **`tradeRoutes.foreign` / `.coastal`** become CONDITIONS, never kinds — same work item as the `tradeRoutes`
    base-section bullet above ([json.md §2](../../specs/json.md));
  - **the trigger-plane set** (`combat.subdueAnimal`, `combat.nukeInterception`, …) stays deliberately unkinded
    ([triggers.md](../../specs/triggers.md)); minting a kind for one is the banned move.
  ⚑ `<channel>.tradeRoute` needs the route count wired as a count-key first, then lands as the channel's
  ordinary deposit scaled by it — never a member of the channel.

- **Route the remaining `per` COUNT scalers.** Deposits scale on the commerce-slider rates
  (`GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE`), on the wonder counts
  (`WORLD_WONDER`/`NATIONAL_WONDER`/`TEAM_WONDER`) and on per-improvement counts, and none of those counts moves
  the deposits it scales. The facts already exist (`SEVT_EMPIRE_COMMERCE_PERCENT_*`,
  `SEVT_EMPIRE_BUILDING_COUNT_*`, the plot improvement pair); what is missing is the consumer route
  ([DEC-close-event-gaps-now](../../architecture/decisions.md#dec-close-event-gaps-now) — the third gap form).
- **Route `SEVT_PROPERTY_ADDED / _REMOVED` and `SEVT_CITY_PROPERTY_BAND_ADDED / _REMOVED` into the modifier.**
  They fire from the `CvProperties` choke points and the band registry into a consumer set that carries no case
  for either, so a threshold-conditioned deposit on a property never moves
  ([event-spine.md](../../specs/event-spine.md) § THE RECEIVED LINE names this as the worked instance).
- **Delete the atom-fan BANK on the city plane.** The load-bracket bank replays an empire-level crossing's city
  half at `GAME_LOAD_FINISHED`, but plane A already applies the conditioned deposit when its SOURCE arrives — so
  the bank is a second maintenance surface for work another plane already does
  ([roadmap.md](roadmap.md): a wrong wiring is removed on sight). ⚠ Its PLOT half is NOT redundant and must
  survive the cut — it is the only route reaching plot-scope deposits gated by an empire-level atom.

- Serve a city's OFFERED RESOURCES, and give the city screen a VICINITY tab showing them (owner) — no readable
  list exists today for what a city's plot group supplies.
  ⛔ READ only — nothing mirrors onto the city ([enabler.md §8](../../specs/enabler.md) RESIDENCY); a stored
  per-city copy is banned.
  ⛔ VICINITY unions two independently-owned halves per [contexts.md](../../architecture/contexts.md) THE
  VICINITY SPLIT — do not mirror one half onto the other.
  ⛔ Needs a LIST accessor (`getVicinityBonuses`) — `hasVicinityBonus` is per-bonus only, so a tab built on it
  sweeps every bonus id per render ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  ⛔ Serve through the NEW Python surface, never a revived `Cy` binding
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).

- Model the `MAPCATEGORY_` gate — `cascadeEvalCondition` returns TRUE for every `MAPCATEGORY_` atom unevaluated,
  the most-authored plot atom in the data.
  ⚠ Impact is currently latent (owner) — no off-world content is reachable yet, so nothing gates wrong today.
  ⛔ The re-gate route is already wired (a terrain fact seeds the atom, [enabler.md §8](../../specs/enabler.md))
  — give the predicate a body; do not touch the enabler or build a plot-side store.

- Convert the PLOT-YIELD AI CALLERS, the residue of that plane's cut. `calculateNatureYield` still has AI
  callers reading legacy `nature + calculateImprovementYieldChange` math (improvement valuations, feature/hill-
  food heuristics, the `bIgnoreFeature` CHOP what-ifs) that no stored segment can answer.
  ⛔ Do NOT re-point them at the segment — that wires legacy AI calc onto new machinery. They convert with the
  valuation, or they go.
  ⚑ `calculateImprovementYieldChange` collapses to ONE `InfoValuation::plotOwnYield(improvementModifiers,
  family, ctx)` with a plot-anchored ctx (`fillEvalCtxAtPlot`); its `bOptimal`/`bBestRoute` args are the
  AS-IF-HELD what-if the valuation already takes, not new parameters.
  ⛔ Do not build a per-route × per-improvement yield term into the valuation — route valuation is ruled
  movespeed-only ("AI valuation of ROUTES", AGENTS.md), never a yield tradeoff.
  ⚠ Separately, on the DATA side: `RouteYieldChanges` (route entries keyed by improvement,
  [modifier.md §4](../../specs/modifier.md)) currently reach no plot yield — `plotBaseYields`'s route leg is
  `plotOwnYield`, the route's untargeted output only. Serving it needs the improvement's FK passed beside its
  modifiers so `keyedTargetSum` can resolve the keyed entry.

- Serve the SLIDER-SCALED SHARE of a channel — the part of a city's realized happiness contributed by a deposit
  carrying `per:{CULTURE_RATE, each:100}` ([json.md §3.1](../../specs/json.md)). The share resolves at gather;
  no point read or valuation exposes it, so a consumer can neither discount it nor value one slider point.
  ⚑ Two AI reads need it: the civic valuation (discounting slider-removable happiness) and the culture-slider
  chooser (today only the culture-victory branch moves the slider; it cannot otherwise answer "how much RATE
  clears this city's anger").
  ⛔ Not a getter per channel — it is the `per`-scaled share of a group the read already hands out.

- Migrate the `constructs` outcome data onto the unit's grants. `constructs` is the dominant
  `outcomes.actions[]` verb ([json.md §8](../../specs/json.md)) and reaches nothing; the has-building surface it
  needs already exists on the rebuilt unit info and is read by the construct mission.
  ⚡ CURATOR item — units authoring `constructs` vastly outnumber those authoring `grants.buildings`.
  ⛔ Do NOT give `CvOutcome` a building member — that carries a hardcoded ability's payload on the data-driven
  outcome plane, the carve-out the mission-concept rework owns.

- Evaluate the ROUTE's tech-gated movement tail at the movement read. A route's base move cost and its
  tech-gated delta are ONE slot (the bare number plus a conditioned entry, [modifier.md §6](../../specs/modifier.md)),
  but `CvPlot::movementCost` takes the point read (unconditioned entries only), so the gated delta never applies.
  ⛔ Needs the conditioned tail evaluated against the asking team, the same shape every other conditioned read uses.

- Set `ctx.civic` where a civic's value is resolved, so `{CIVIC_CATEGORY: CIVICOPTION_X}` can answer. The
  predicate, ctx slot, and authored deposit all exist (a trait waiving religion-civic upkeep authors
  `upkeep.empire.civic.percent: -100` gated on it) but no walk sets the slot, so it is FALSE everywhere and the
  waiver is inert.
  ⛔ FALSE is the correct unset answer ([contexts.md](../../architecture/contexts.md) § THE SOURCE SLOTS) — the
  gap is the consumer, not the predicate. The civic-upkeep resolve sets the slot on a LOCAL COPY of ctx, exactly
  as the religion/sourceBuilding slots are set.

- Apply the PER-CITY GATES AT THE COMBINE. [modifier.md §1](../../specs/modifier.md) specifies the realized
  value as the scope-package sum with per-city gates (state-religion-in-city, coastal, connected, area
  membership) applied live at the combine; `InfoValuation::rolledLegsAtCity` is a bare package sum, so an
  upper-scope deposit gated on a city predicate never fires.
  ⛔ Not a package-rebuild question — an empire package has no city bound, so the condition can't be evaluated
  when the package is built; it belongs at the COMBINE, where the asking city is known.
  ⚠ Until it lands the affected deposits are unserved; `savemigration.txt` carries the replacement obligation
  for the one whose accumulator is already cut.

- Charge the improvement UPGRADE cost from somewhere that can tell an upgrade from a build. Nothing charges it
  today — the only implementation sat in the improvement-set choke point, which can't tell the two apart (a
  worker building a farm on a seed camp paid twice) and fired on unowned plots with no owner to charge.
  ⛔ Wants the intermediate that only the upgrade path calls, never the shared setter.

- Give GAME OPTIONS and CONFIG VALUES a standardized read surface (owner: *"having standardized getters for
  gameoption, and config values is not a bad idea"*), shaped like the existing `CvGameSpeedScale` /
  `CvTraitSelection` pattern ([engine.md § Consuming-system calcs](../../reference/engine.md)): a
  purely-organizational static-methods class, one getter per group.
  ⛔ Keep the three kinds SEPARATE, never one `getSetting` — GAME OPTION (`GAMEOPTION_*`, fixed at setup,
  [DEC-entity-gate](../../architecture/decisions.md#dec-entity-gate)), CONFIG (authored data on
  eras/gamespeeds/handicaps, read from its sources — [state-repositories.md](../../architecture/state-repositories.md)
  WORLD is CONFIG), and LIVE (user-changeable mid-game, incl. `MODDERGAMEOPTION_*` despite the name —
  [event-spine.md](../../specs/event-spine.md)) — nothing STATIC may depend on a LIVE option.
  ⛔ Add a readJson check refusing a `MODDERGAMEOPTION_` condition, so the split is unsayable to violate.

- Make `hideAndSeek` a CACHED BLOCK on the CITY, as the UNIT side already is
  ([state-repositories.md](../../architecture/state-repositories.md) — a SECTION folds beside the slot table on
  the same mark). The city side marks on its building facts.
  ⚠ `getInvisibleType()` still reads the INFO alone, so a promotion-granted invisibility type does not work at
  all — it wants the same folded read.
- The PLAYER-ALERT consumer, and the alerts owed to it: "power restored" (`TXT_KEY_MISC_POWER_RESTORED`, now
  hangs on `SEVT_CITY_STATUS_REMOVED` carrying `CITYSTATUS_POWER_DISABLED`); the CAN_RETRAIN/NO_RETRAIN pairs
  the promotion KEEP gate used to emit per failing axis (terrain / feature / plot bonus /
  improvement-or-local-building / promotion prereq, plus two more the axis list does not name); and "your
  building was obsoleted" (`SEVT_CITY_BUILDING_OBSOLETED_ADDED / _REMOVED`, emitted for exactly this). All are
  authored and were rendering — a real loss of player-facing information.
  ⛔ They do NOT come back as a per-axis walk beside the gate (rebuilds the legacy battery) — re-attach to the
  fact, never re-inlined at a mutation site ([event-spine.md](../../specs/event-spine.md)).
- Decide WHERE the citizen-assignment re-check is asked for — a CALL-SITE question, never a removal:
  `AI_setAssignWorkDirty` is called from across the engine while `AI_updateAssignWork` re-runs the FULL
  assignment for every marked city, so the flips are a turn-time cost in their own right. The ruled trigger set,
  the canWork gate exceptions, the unit-movement carve-out and the census instrument are all specced in
  [citizen-assignment.md § Dirtying the assignment](../../reference/citizen-assignment.md) — implement that set,
  do not re-derive it.
  ⛔ Do NOT re-add an ungated flip to replace a cut maintainer's gated one.
- The amenity CONSUMER side: re-point consumers onto the CITY read, and retire the per-flag `CvCity` counters
  and their bespoke per-attribute predicates/facts ([contexts.md](../../architecture/contexts.md)). The apply
  sites are the `process*` functions — `changeZoCCount`, `changeProtectedCultureCount`,
  `setWorkableRadiusOverride` — against keys the data already authors (`zoneOfControl`, `protectedCulture`,
  `adds3rdRing`).
  ⚑ POWER is already converted and is the pattern to copy (contexts.md § POWER IS AN AMENITY).
  ⛔ What's left is the GENERALIZATION — one parameterized amenity fact carrying the id, replacing the
  per-attribute ones as each converts; do not grow a second bespoke fact per key.
  ⚠ Check each counter's FEEDERS first — power's conversion was a read-swap (only a dead `CyCity` binding still
  wrote it); the others may not be.
  ⚠ The workable-radius counter is a live BUG, not just an unmigrated shape: it is a plain SET rather than the
  id→COUNT refcount, so a city holding two radius grantors and losing one drops the override to 0 and loses a
  ring a live grantor still justifies.
- Re-fold a conditioned amenity on a BUILDING grantor when its condition moves (the empire half is covered). It
  wants the condition-dependency route the modifier consumer already derives.
- Decide whether `CvTraitInfo` / `CvTechInfo` carry `m_amenities` and its fold leg. ⚑ Answerable NOW: readJson
  reports an entity authoring a block its type cannot hold, so the tree already says whether any does — read it,
  then either wire the leg or record that the block does not belong on those types.
- The endpoint route table, beyond the stored-vs-oracle documents. It routes through the ACCESS SURFACE, which
  does not exist yet; building it is the actual work item here
  ([roadmap.md § THE OPEN ITEM — the ACCESS surface](roadmap.md#-the-open-item--the-access-surface)).
- The `requires` BLOCK COMPOSER — deciding heading, ordering and which clauses compose one block, the text
  manager's own job ([patterns.md](../../architecture/patterns.md) THE DIVISION OF LABOUR).
  ⚠ The CONDITION renderer it calls already exists and takes exactly what `CvRequires` holds — this is a
  composer to write, not a renderer to build.
- Give the ctx-taking KEYED SUM (`keyedTargetSum`) the scope filter its collecting twin (`collectKeyedTarget`)
  already has. `collectKeyedTarget` takes an `iScope` (-1 = any) because the same family+target is authored at
  two scopes with two different consumers; `keyedTargetSum` — the one serving the CONDITIONED tail through the
  ONE evaluator — takes none, so a caller that must pin a leg to one scope falls back to the unconditioned
  collect and silently loses every gated row.
  ⚑ Live case: the free-specialist split authors BOTH a city-scope row and an empire-scope row on the same
  building, so the empire read must pin the scope or the two legs double-count.
  ⛔ Not a second read — one parameter on the existing one, matching the collect's spelling.
- Move the pedia hub's CATEGORY CLASSIFIER onto `identity.pediaCategory`. The home is settled and the read is
  published ([pedia-read-map.md](../../reference/pedia-read-map.md) finding 4); the hub still derives groupings
  from legacy per-field getters — era banding, cost tests, instance-cap tests, promotion-line flags — and for
  three buckets, a SUBSTRING MATCH ON THE LOCALIZED DISPLAY NAME (silently wrong in non-English).
  ⛔ Do NOT publish those legacy getters so the classifier resolves — that preserves the name-matched buckets
  while reading as migrated ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).
  ⚠ The era SUB-category stays derived from the entity's own era, not a second authored field.
- Ranked-target-selection EVALUATION ([parked/ranked-target-selection.md](../parked/ranked-target-selection.md))
  — a ranked entry applies unranked until it lands.
- The Python data-fetching library (below).

## The GETTER cut — game objects + AI

> Sequencing, and the ban on bending the new surface to fit an old call: [roadmap.md](roadmap.md).
> ⛔ The unit of work is the CLASS of read, never the individual getter — many collapse as the rebuilt infos
> wire through ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)).

- Move the IMPROVEMENT tech-yield reads onto the keyed what-if twin. One of them asks a THIRD question the twin
  does not answer: the `bOptimal` branch wants "assume every condition holds", which is neither the live
  evaluation nor a hypothetical over one named id. Decide whether that is a MODE of the same read or an
  entry-list sum before converting it — ⛔ never a second evaluator beside the one the twin already uses
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- Watch civic-choice STABILITY now that the cross-category half-value damper is gone (owner: drop it; if the
  problem shows, add it back properly). ⚠ It existed because civic valuations are linearly combined across
  categories, so a building gated by civics in two options could be counted at full value from both. If choices
  start oscillating, the principled fix is a `civics` id set on `CascadeCondDeps` — which is what the removed
  whole-civic-database sweep was reconstructing by hand — never restoring that sweep.
- ⛔ Neither is a VALUATION question, and filing them as one is what sent a reader looking for a magnitude
  machine that was never the dependency: both ask AVAILABILITY under a hypothetical HAVE, which is the
  enabler's ([DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade)).
- Re-express the specialist EXPERIENCE reads as the ENTRY-LIST read over the specialist's own authored entries.
  ⛔ Not an arity fix — folding a keyed entry scope-wide is the silently-plausible-wrong case
  ([modifier.md §5](../../specs/modifier.md)).
- Give the CityContext its id-keyed RADIUS DICTIONARIES, then move the improvement count onto them. The count
  domain is wired but walks the city's radius per call; it runs at rebuild/per-decision cadence rather than on a
  read path, so it is not the banned read-time scan, but the dictionary is the standing target
  ([contexts.md](../../architecture/contexts.md)) and this read is the one that wants it.
- Fold the city's TYPED-FREE specialists into the specialist count — the count answers the ASSIGNED population
  alone today, and the legacy multiplier counted both. ⚠ Depends on giving the typed-free ledger a home first;
  that is the prerequisite work.
  ⚑ The shape is already ruled ([tally.md](../../specs/tally.md)): give the OBJECT the aggregate and forward it
  through `CityContext`, never a tally side-store. ⚠ The legacy count is assigned + typed-free specialists, so
  the city's assigned-only counter is NOT the number — forwarding it as-is would under-count silently.
- Make the empire GREAT-GENERAL rate a RECEIVER SUM over the player's cities, not an empire-package read.
  ⚑ Great general is NOT great people (owner): great PEOPLE accrue per city, while great general points sum from
  cities plus battlefield experience into the player's own counter — the cross-scope receiver shape
  ([state-repositories.md](../../architecture/state-repositories.md)).
  ⚠ Until it exists the empire read MISSES the city-authored building deposits — a wrong number, not a dangling
  site.
  ⛔ Do NOT re-scope the building data to empire (the city authoring is correct) and do not touch great people's
  own city/empire split — it is right as it stands and out of scope.
- Move the realized-value reads onto the existing group reads (a consumer move, no new surface).
- Delete the per-SOURCE decomposition accumulators: member, `change*`/`get*`, read + write, and the tag named in
  `savemigration.txt` ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).
  ⚠ Audit each `change*` BODY for side-effect riders first ([save.md §6](../../specs/save.md)).
  ⛔ They do NOT each earn a replacement getter — the group read answers the TOTAL, and per-source attribution is
  the ORACLE's job. Their last maintainers (`processBonus`, `processSpecialist`) go with them.
- Design the genuine residue that needs NEW surface: the slider math, the espionage counters, the live combat
  state, and `getHappinessTimer`.
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

- **Verify map generation actually works — start a NEW GAME.** Nothing on the standing save exercises
  `CvMapGeneratorUtil.py` (the DLL's map-gen fallback, [engine.md](../../reference/engine.md)) or the
  game-start grants (free techs/units/gold, `freePopulation`, `FreeStartEra` —
  [legacy-grant-apply-sites.md §5](../../reference/legacy-grant-apply-sites.md)); only a new game observes them
  ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)).
  ⛔ Include `PrivateMaps/` in the sweep — it sits outside `Assets/`, is easy to miss, and reads the same named
  accessors as everything else ([python-read-map.md §7 ruling 1](../../reference/python-read-map.md)).

- **Build the keyed twin of the `expected*` valuation** ([modifier.md §5](../../specs/modifier.md)) — the
  conditioned-tail read a keyed deposit is missing. Closes an improvement's BONUS-conditioned yield, a
  feature's HAS_RIVER-conditioned yield, and the dead handles three PerfectWorld-lineage map scripts
  (`C2C_PerfectMongoose_v310` · `C2C_PerfectWorld2f` · `C2C_Totestra`) keep waiting on it.
  ⛔ Do not close it by summing the conditioned tail directly — that applies every tech/age-gated deposit from
  turn 0, silently ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)).
  ⛔ Those same three scripts also call `isRequiresFlatlands()` on a BONUS — no `CvBonusInfo` has ever carried
  it (it's a FEATURE member). Pre-existing crash, not a migration casualty; leave it standing, don't invent
  which predicate was meant.

> Contract: [patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md). Read maps:
> [pedia-read-map.md](../../reference/pedia-read-map.md) · [python-read-map.md](../../reference/python-read-map.md).
>
> ⛔ **"Stage 4" is a grouping, NOT a phase to wait for — nothing gates the disconnect.** A dead legacy Python
> getter is an OUTLAW, shot on sight; cutting wrong is cheap and is how a real dependency gets named
> ([roadmap.md](roadmap.md)). ADD to the library whenever a read makes sense, and clear Python-side compile debt
> as you meet it — parking it spends the census budget the rest of the worklist needs. ⛔ Never borrow legacy as
> a "temporary solution" for a read the library does not answer yet: add the read.

- **Re-point the surviving `GC.get<X>Info` reads.** Published NOWHERE, so each is an `AttributeError` the
  moment its handler fires. Identity reads (description, text key, civilopedia, strategy, type key, button)
  re-point mechanically onto `CyInfo` for every registry.
  ⛔ The rest is a DELETION list plus a COVERAGE checklist
  ([DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface)), never a port list — each
  remaining name is answered by a group read, an intrinsic slot, an edge family, or a classification test, or
  it's dead. Adding a getter per legacy name is the half-migration reflex.
  ⛔ A cross-link reading a terrain/feature/building's `requires` back at the referencer is `EDGEF_RELATED`,
  never `EDGEF_REQUIRED_BY` — the plot substrate carries no `REQUIRED_BY` edge at all
  ([enabler.md](../../specs/enabler.md), `rp_requiredByRefInfo`'s routing). Not a gap; do not add one.

- **Re-point `CyState`'s city/plot/empire reads onto the contexts.** Python currently asks the `CvCity*` /
  `CvUnit*` object directly, which is exactly what
  [DEC-scope-contexts](../../architecture/decisions.md#dec-scope-contexts) forbids — the HAVE axis is asked of
  the scope's context, never reached ad hoc off the game object. `CityContext`
  ([contexts.md](../../architecture/contexts.md)) already answers most of what Python currently derives the
  long way.
  ⛔ Where a context cannot answer, that's a CONTEXT GAP to close by adding the forward — never a reason to
  reach past it.
  ⚠ UNITS have no context yet (a future role-specific scope): the unit plane reads the unit's own held
  containers meanwhile — O(held), never O(registry).

- **Give WorldBuilder's UNIT editing a home on the new surface.** `CyUnit` carries only the identity set
  ([patterns.md](../../architecture/patterns.md)), no reads — expected, not a defect
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)). Owner-ruled scope: editing an
  individual unit's strength, persisted via the WBS scenario field (engine state already serialized,
  [state-repositories.md](../../architecture/state-repositories.md)).
  ⛔ Do not re-register the legacy `CyUnit` binding to fix this.
  ⛔ Do not reproduce the legacy setter's bug: `setBaseCombatStr` writes the BASE, but `baseCombatStr()` returns
  the fully-composed value (base + promotion/unit-combat delta + SizeMatters) — they are not inverses. The new
  surface needs a base-in/base-out pair, or an explicit set-to-absolute.

- **Restore the engine→Python callback direction — currently DOWN.** Gated on the INFO plane, not the
  callbacks themselves: the import chain (`CvEventInterface` → `BugEventManager` → `CvEventManager` →
  `CvScreensInterface`, [python-load-sequence.md](../../reference/python-load-sequence.md)) is INFO-dominated
  at module scope, so it cannot come back until `CyInfo` serves the info plane, one module at a time.
  ⛔ Not closed by publishing the legacy god object
  ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)) or a per-module shim
  ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)) — a module comes off it only
  by having its reads served.
- **Serve the INFO-OBJECT accessor plane's per-type tail** (`getEra`, `getGridX/Y`, `getWorldSize`,
  `getPrereqAndTech`, `isVisible`, `getColorType`, `getActionInfoIndex`, …) — `GC.get<X>Info(id).<method>()` is
  the dominant remaining unserved read; `CyInfo` already answers the generic identity reads by prefix.
  ⛔ `getChar` is NOT part of this — TEXT-plane, served off `CyGameTextMgr`
  ([patterns.md](../../architecture/patterns.md)).
  ⚑ Re-derive demand with `python Tools/census-python-boundary.py`.
- Serve the free-function map helpers (plot direction / XY / distance / step distance) — their registrar went
  with the binding purge; the map-generation utilities call them throughout. A failed read here is silent,
  falling to DLL-default generation ([python-load-sequence.md](../../reference/python-load-sequence.md)), so a
  wrong map is the only symptom.
- **Give WorldBuilder's two `CyMapGenerator` calls a home.** Its registrar is gone (no `class_<>`, no `.def`),
  so `CyMapGenerator()` raises `NameError` at the click; the class itself still compiles as a dead TU nobody
  reaches.
  ⛔ Do not re-register the wrapper ([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)) —
  serve both calls on the new surface.
  ⚠ The two callers want different things: `CvWBDesc.py`'s `addBonuses()` is a real method on this wrapper;
  `WBGameDataScreen.py`'s `eraseGoodies()` is NOT — it's `CvMapGenerator::eraseGoodies`, reachable through
  `CyMap`, not this class. Re-registering the wrapper would not have fixed the second one.
- Bind the XML-named callbacks that resolve to no `def` in the module the DLL names, and the DLL-named
  game-utils callbacks with no Python definition at all
  ([python-load-sequence.md](../../reference/python-load-sequence.md) — a name resolves only in the module the
  DLL names).
- Add the missing `CvEventManager` handlers for the DOMAIN events the reporter emits and nothing handles. They
  drop silently through the dispatch-map miss path.
- Shoot the dead `Cy*` bindings on sight as the compiler names them. ⚠ Distinguish DEAD from UNREGISTERED
  first: a wrapper the engine still hands across is not dead, it is missing its registration (above).
- **Serve the UPGRADE-AWARE building count** the random-event surface reads — its binding is already
  unpublished. ⛔ It does not come back as the legacy count: "including upgrades" resolves through a building's
  dormant triggers ([enabler.md §2](../../specs/enabler.md)), which merges successors with unrelated dormancy
  sources (e.g. a pollution band dorming an observatory). Summing the bucket counts the band as an upgrade —
  plausibly, silently, wrong. Decide what the read MEANS before serving it.
- **REBUILD the emptied `CvGameTextMgr` composers** on `appendEntryLines` + the requires block composer — their
  bodies were CUT, not migrated, so this is written fresh, never restored to what they used to say. Acceptance
  test: reads NO legacy getter, never "reads nicely"
  ([patterns.md § THE DIVISION OF LABOUR](../../architecture/patterns.md)) — and per the ruling there, every
  family the entity carries must render, not a converted subset.
  ⛔ The disabled-citizen tooltip's "which building opens this slot" list is a REVERSE cross-link — read the
  specialist's own `EDGEF_RELATED`, never a whole-building scan.
  ⛔ The four WELLBEING composers are BLOCKS, not `appendEntryLines` targets (a realized per-scope aggregate has
  no entry list). A bonus's "what needs me" block reads its own `EDGEF_RELATED`; a promotion's per-unit lines
  read `CvPromotionAccrual::sum`, never a re-rolled rung loop.
  ⚠ They currently mislabel the whole wellbeing deposit total "from buildings" — every source (civics, traits,
  bonuses, corporations, specialists, features, techs, projects) deposits into it. ⛔ Do not fix this by
  pointing the city source-walker at it (that walker is buildings/civics/traits/culture-level only, partial for
  wellbeing) — either widen the walker to wellbeing's full source set, or relabel the lump for what it is.
- Re-point the unit power-value plane's readers.

## Triggers / grants

- Build START PACKAGES: the entity type, its folder + prefix + repo row + manifest, and the shipped defaults
  ([triggers.md](../../specs/triggers.md)). Two content decisions ride it — which units the defaults name, and
  NPC/barbarian starts.
- Retire the engine start selection (the whole-database scan + AI scoring, and the per-role starting counts)
  once packages carry the identities.
- Walk the player's HELD TRAITS in the per-unit promote pass. `tr_promoteOneUnit` walks the city's OPERATING
  BUILDINGS only, so no trait entry is ever consulted and trait free promotions reach nobody — the data is
  authored, so this is the whole of what is missing. The era-advance resolver is the same PRESENCE read, never
  the banned own-data inversion.
  ⚑ The per-class filter needs no new mechanism: it is the entry's own `enabled: "IS_<TAG>"` predicate, which
  `tr_promoteFromEntries` already evaluates for the building leg.
  ⛔ Do not answer this by restoring a trait-side promotion×unitcombat MAP — that is the legacy mechanism. The
  legacy removal half needs no take-away verb ([triggers.md](../../specs/triggers.md)).
  ⚠ `CvUnit::setFreePromotion`'s trait legs dangle naming exactly this.

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

- Point the AI production decision at the maintained LISTED set, and collapse the `AI_chooseProduction`
  focus-ladder into ONE unified scoring pass ([enabler.md §6/§8](../../specs/enabler.md)). The collapse is an
  AI-architecture change, not a per-loop rewrite.
- ⛔ **FIND AND FIX EVERY AI EVALUATION LOOP THAT SCANS A WHOLE REGISTRY (owner).** A scan asks the entity
  database a question the maintained state already answers; it no longer works and is not needed.
  ⚑ Two populations, different fix, do not treat alike: a loop over an **enabler domain**
  (units/buildings/techs/civics/projects/processes/promotions/builds) re-points onto the maintained FRONTIER
  ([enabler.md §6](../../specs/enabler.md)); a loop over any OTHER registry
  (unitcombats/specialists/terrains/features/bonuses/religions/properties/invisibles) is the OWN-DATA INVERSION
  and re-points onto the entity's own compiled entries via `InfoValuation::collectKeyedTarget`/`collectKeyedCombat`
  ([modifier.md §5](../../specs/modifier.md)).
  ⛔ Exception: a PLOT loop is not this class and must not be swept ([enabler.md §7.1](../../specs/enabler.md) —
  the worker-builds carve-out).
  ⛔ The compiler will never name one of these — this closes only by being searched for, never by the
  error-driven census.
  ⚠ A loop calling a gate with WHAT-IF args cannot simply swap to the frontier — it wants the as-if-held
  overlay or the gate twin instead ([enabler.md](../../specs/enabler.md)).
- Move `AI_baseBonusVal`'s per-kind loops off the whole-database driver onto the frontier, and off the dead
  prereq getters onto the bonus's own `EDGEF_REQUIRED_BY` ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view)).
  ⚠ Its "would this bonus UNLOCK this" half needs the AS-IF-HELD overlay (above) built first; the rest does not
  depend on it.
  ⛔ The two halves are separable — treating the whole loop as blocked parks convertible work behind a
  dependency only part of it has.
- Restore the members `AI_techValue`'s remaining sweeps read, then drive them from the tech's own edges.
- Retire the active-set work-list ripple: the fact that justified it is now emitted, so the parallel propagation
  machine goes ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ `emit()` dispatches SYNCHRONOUSLY, so an event chain recurses on the call stack where the work-list iterated
  — design for that; it is not a reason to keep the machine.
  ⛔ Its runaway cap claims to "self-heal at the slice boundary"; that rebuild was REMOVED and nothing heals it.
- Converge the enabler's bespoke per-id reverse indices onto `EDGEF_REQUIRED_BY`
  ([DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) — a side index is banned
  "especially not inside an enabler"). There are **five**, in two files, all rebuilt by re-scanning every info at
  load: `s_operateBonusConsumers`, `s_operateBuildingDependents`, `s_operateDormantTriggeredBy`
  (`CvEnablerKernel.cpp`) and `s_udUnitDeps`, `s_udUpgradePred` (`CvUnitEnabler.cpp`).
  ⚑ The canonical reverse pass already inverts both `requires.build`/`requires.operate` and `dormantTriggers()`,
  so the answer these five recompute is already on the info — the only delta is they are operate-only while the
  canonical edge merges build+operate, which is safe per over-inclusion ([enabler.md §5](../../specs/enabler.md)).
  ⛔ Read [enabler.md §8](../../specs/enabler.md) "The reverse index, and what is deliberately NOT one" first —
  the axis-flag lists, `s_operatePropertyBandConsumers`, and `s_specialBuildingMembers` are explicitly NOT
  convergence targets. Sweeping those together with the five is the documented mistake.
- Delete the hand-rolled condition walks in `CvImprovementInfo` and route them through `CvConditionQuery`
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)). `ii_collectPredicateIds`
  duplicates the canonical leaf test **verbatim** while the canonical `collectPredicateIds` sits at ZERO callers —
  and `namesId` / `bucketForType` are equally unused. ⚠ The private walk covers only `all`+`anyOf`; the canonical
  also walks `noneOf`/`enabled`/`disabled`, so the duplicate is not merely redundant, it is NARROWER.
  ⚑ Same family: the three-way copy of the bonus/building presence-id collector in `CvImprovementInfo`,
  `CvBuildInfo` (`collectPrereqBonuses`) and `CvCorporationInfo` (`corpCollectSpreadBuildings`) — each re-testing
  an inline `compare(0,N,"PREFIX_")` that the zero-caller `bucketForType` exists to answer. The corp variant also
  needs the clause's `min`, which the shared surface does not yet return: LIFT that onto the shared surface, never
  keep the private copy for it. ⛔ `CvTechInfo`'s walk is NOT in this family — it reconstructs AND-vs-OR structure,
  which `CvConditionQuery` deliberately refuses to expose.
## Tree / include hygiene

- Retire the `CvInfos.h` umbrella — a hand-careful pass; the lessons and hard bans are in
  [AGENTS.md](../../../AGENTS.md) Conventions §Design.
- Run the dead-code / dead-XML pass — tooling generates CANDIDATES only; every removal verified against
  source/data and test-loaded, one subsystem at a time.
- Delete the `#ifdef` ATTICS — a guarded block whose symbol is defined nowhere AND has no commented-out
  `#define` either ([AGENTS.md](../../../AGENTS.md) Conventions §Design — the test, the off-switch exception,
  and the predefine exclusions are stated there in full).
  ⚠ `GLOBAL_WARMING` carries a commented-out `#define` and is nonetheless owner-ruled DEAD
  ([economy.md](../../reference/economy.md)), so it goes WITH the attics — the switch marks a candidate, never a
  verdict. Its nuke counter (`getNukesExploded` and its changer) is live outside the feature and STAYS; only the
  warming machinery and the orphaned `GLOBAL_WARMING_*` defines go.
## Green-up (after the structure, never ahead of it)

- Engine-repair debt: the bare Engine includes · the property-manipulator helpers · `CvCity.h`'s functor row.
- The vocabulary TXT keys (one per family/kind/predicate/token) — polish on a working machine; the renderer's
  spell-back fallback is the accepted output until then.

## Spec gaps to close

- Give the mod-data design invariants a spec home — a requirement may not unlock after the thing requiring it;
  replacements are explicit, never implicit; a replacing entity must be better. The checks are gone; the
  invariants belong in [json.md](../../specs/json.md).
