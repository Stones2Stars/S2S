# 4. Consumers, registration order, and `CvEventReporter`

> Part of the **[spine](../spine.md)** spec.

**⚖ PLAYER ALERTS ARE A SPINE CONSUMER, RE-ADDED ON THE FACT — never re-inlined at a mutation site.**
The legacy notifications ("a building shut down", "a building was restored") were emitted from inside the
setter that changed the state, which is why they die with every legacy mutator that gets cut — and why they
cannot simply be kept: the setter they lived in is the duplicate being removed. They come back as a CONSUMER of
the DOMAIN fact that already announces the change (the operate crossing, the owner change, …), exactly as
logging and the `/events` stream are consumers.
⚑ **This is a growing list, not a one-off:** each legacy mutator cut takes its alerts with it, and they are
re-added together on the facts rather than one at a time inside whatever replaced the setter.
⛔ An alert re-inlined into a machine's apply path is the same mistake in a new place: it makes a UI concern a
condition of the state change, and it is invisible to anything else that wanted to know.

**Registered consumers today:** the broad FILE logging consumer, the `/events` STREAM consumer, the **trigger
engine** (`Triggers/CvTriggerEngine` — the ONE payload machine, grants folded in as the null-condition case:
resolver AND appliers built (`tr_applyTechFirstDiscover` / `tr_applyBuildingFirstBuild` / `tr_applyPerTurn` /
`tr_applyCityPerTurn` / `tr_applySpawn` / `tr_applyFullHeal` / `tr_promoteFromEntries`), dispatched from
`SEVT_TECH_ACQUIRED` / `RELIGION_FOUNDED` / `PLAYER_INIT` / `CITY_FOUNDED` / `CIVIC_ADOPTED` / `TURN_STARTED` /
`BUILDING_ADDED` / `BUILDING_ACTIVATED` / `UNIT_CREATED` / `UNIT_ENTERED_CITY` / `CAPITAL_CHANGED`; the
remaining increments are in [triggers.md](../specs/triggers.md)).
⛔ It registers **LAST**, after the contexts / enabler / modifier — the ordering rule and why is
[triggers.md](../specs/triggers.md) § Registration order.
Beside it: the **enabler's own** consumer (`Enabler/CvEnablerConsumer`, load-active), and the **modifier's own**
consumer (`Cascade/CvModifierConsumer`, load-active for cache building): DOMAIN events in, the moved source's
compiled deposits APPLIED into the slots they feed — the maintained sum's one write path
([cascade.md](../cascade.md)).
The **tally** is NOT a consumer — it reads the object-owned counts (`Tally/CvTally.{h,cpp}`).
⛔ **One consumer per system** — the shared consumer that routed BOTH machines is dead
([superseded-ideas](../architecture/superseded-ideas.md) #16); never re-merge them.

### ⚖ `CvEventReporter` emits spine facts beside its Python calls

> CvEventReporter is not converted yet; it simply emits spine events, so that

**A happening that reaches only Python is invisible to the spine, to `/events`, to the file consumers and to
every C++ consumer.** `CvEventReporter` (`Sources/UI/CvEventReporter.{h,cpp}`, 85 `void` report methods) is the
engine→Python callback hub, and for a large part of its surface it is the ONLY announcement a happening makes.
So each method gains a spine emit ALONGSIDE its existing Python call.

⛔ **This is an ADDITION, never a conversion.** The reporter keeps calling Python exactly as it does; nothing is
rerouted, removed or re-bodied. Converting `CvEventReporter` onto the triggers machine is the LATER work item
([patterns.md](../architecture/patterns.md): its successor is the triggers machine and *"events move INTO C++, but
that is not 430"*), and starting it here — one handler at a time — is the event rework beginning by accident.

⚑ **WHY THE REPORTER IS THE RIGHT EMIT SITE**, even though it is a reporting hub rather than a mutation choke
point: every method is CALLED at the happening with the parties already in hand
(`combatResult(pWinner, pLoser)`, `unitCaptured(eFromPlayer, eUnitType, pNewUnit)`). So the spine fact carries
**exactly what Python receives**, which is the property that makes the later migration a SWAP rather than a
re-investigation. An emit placed anywhere else would have to rediscover those arguments.

**⛔ THE FACT IS RAW, NOT FORMALIZED — and this is the part to get right: there is no info, and no way to define many of these events in C++ yet; that happens when the ac** The emit announces the happening with the
reporter's OWN arguments and invents nothing: no designed payload, no modelled semantics, no `on<Happening>`
token, no action verb. ⚑ That is also why this does not breach [triggers.md](../specs/triggers.md)'s ban on minting
a happening or a verb speculatively — the AUTHORING vocabulary stays unminted; only the engine's announcement
lands. Anything designed now is undone by the formalization later.

**⛔ THE ONE BAR IS THE STANDING ONE — DUPLICATES.** A reporter method whose happening ALREADY has a spine fact
gets no second emit: `unitPromoted` (`SEVT_UNIT_PROMOTION_ADDED`), `techAcquired`, `buildingBuilt`
(`SEVT_CITY_BUILDING_ADDED`), `cityBuilt` (`SEVT_CITY_FOUNDED`), `religionFounded`. ⚑ **The worklist is therefore
a SUBTRACTION, not a judgement call: the reporter's method list minus the facts that already exist** — and what
falls out is the combat and arrival cluster that has never been on the spine at all (`combatResult`,
`combatRetreat`, `combatWithdrawal`, `unitCaptured`, `unitPillage`, `goodyReceived`, `cityRazed`, `nukeExplosion`,
`greatPersonBorn`).
⚠ **`unitKilled` is the case that must be DECIDED rather than skipped:** the reporter fires it from
`scheduleDeath` while `SEVT_UNIT_KILLED` fires from `die()`, and those are different moments — a scheduled death
can still resolve into survival ([unit-lifecycle.md](../reference/unit-lifecycle.md)). Near-duplicate, not
duplicate; resolve it on the facts rather than assuming either way.

**⚖ THE KIND IS PER-METHOD, by the test §6 already states:** *does the fact say what the STATE is, or what some
CODE did?* `combatResult` / `unitCaptured` are state changes ⇒ **DOMAIN**; `combatLogCollateral` /
`combatLogFlanking` are log entries ⇒ **DIAGNOSTIC**. ⛔ Erring toward DIAGNOSTIC defeats the purpose: no
consumer may build state from one, so a DIAGNOSTIC fact cannot serve as the migration seam and would need
converting in a second pass.

⚑ **AND IT ANSWERS A TRACING GAP THAT IS NOT HYPOTHETICAL: *"it's hard to trace where things come from,
captives being the best example."*** A captured unit today announces `SEVT_UNIT_CREATED` and nothing else, so it
is indistinguishable from a trained, granted or WorldBuilder-placed one — and no provenance tag on
`UNIT_CREATED` could express a capture's SECOND PARTY. `unitCaptured` carrying captor and victim is what makes
the origin readable.

> **⚖ A SOURCE on `SEVT_UNIT_CREATED` IS A RENDER FIELD, AND ONLY A RENDER FIELD.** Where the creating
> happening is worth reading off the existence fact, it rides the RENDER payload (the `SEVT_NAME_CHANGE`
> precedent — a field the machine consumers do not read), so the log answers *where did this come from* at a
> glance while `UNIT_CREATED` stays the one fact every "does this unit exist" consumer rides.
> ⛔ **The moment anything ROUTES on that field it is no longer diagnostic and must become its own fact**
> ([a fact names the happening](03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md#-a-fact-names-the-happening--something-changed-is-not-a-fact): a payload a consumer
> branches on is the calculation relocated into a `switch`).
> ⚑ Two facts at one birth is not a duplicate and the tree already does it deliberately —
> `SEVT_WORLD_UNIT_CREATED_COUNT_ADDED`, `SEVT_EMPIRE_UNIT_COUNT_ADDED` and `SEVT_UNIT_CREATED` all fire at one
> birth and none duplicates another.

