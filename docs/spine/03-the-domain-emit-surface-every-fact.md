# 3. The DOMAIN emit surface — every fact names a happening

> Part of the **[spine](../spine.md)** spec.

**The spine is the SINGLE place a state change is announced.** Every game state change emits ONE source-carrying
DOMAIN event through a clean endpoint (`emitBuildingChanged`, `emitTechChanged`, `emitImprovementChanged`,
`emitCityOwnerChanged`, …); the event names WHAT (`iType`), WHO (`iC`, owner/triggering player), and WHERE
(`iSrcLoc` = cityId | plotId | -1). `emit()` dispatches **synchronously** — it is not an async listener bus; it calls
each interested consumer's `onEvent` inline at the mutation site. So nothing else in the engine detects changes: the
hand-wired per-site invalidation is retired in favour of this one surface.

**TURN BOUNDARIES are spine events, not a side-channel.** `SEVT_TURN_STARTED` / `SEVT_TURN_ENDED` (DOMAIN — the
turn counter advancing is a genuine synced state change) carry `iType` = the game turn and `iC` = the player, with
**`-1` marking the GAME-scope boundary**. The game pair straddles the counter advance in `CvGame::doTurn`
(`ended` = the closing turn, `started` = the incremented one); the player pair rides `CvPlayer::setTurnActive`.
They **replaced** the bespoke `CvHttpServer::publishEvent("turnStart"/"turnEnd"/"playerTurnStart"/"playerTurnEnd")`
publishes — a happening lives on the spine ONCE and the file + `/events` consumers carry it for free, rather than
each surface growing its own emitter (the server SERVES, it does not accumulate — §8). ⚠ **Consumer-visible break,
accepted:** the wire form is now the standardized `[SPINE] turnStarted`/`turnEnded` rendered line, not a
named SSE frame carrying `{"turn","gameId"}`. The player pair emits for **every** player, not just humans: a turn
going active/inactive is a state mutation, and the spine's contract is that every mutation emits while CONSUMERS
filter (a consumer wanting humans only tests the player field) — a deliberately partial emit surface is what
defeats the missed-emit tripwire.

**⛔ ADD ALL THE EVENTS, EVER — the ONLY bar is DUPLICATES.** *"As long as it's not duplicate events, go
nuts, add all the events, ever."* The emit surface is meant to be EXHAUSTIVE: every state mutation in the engine
announces itself, and completeness is the goal rather than a budget to spend carefully. This is not enthusiasm —
it is the ordering the whole model rests on — *"the EMIT surface comes first; the cache build is the step AFTER —
caches cannot build from events until the events are completely emitted"* — so an incomplete emit surface is a
foundation defect, not a backlog item.

⛔ **The ONE thing to avoid is a DUPLICATE — the same fact announced twice.** One fact, one emit, at the genuine
mutation choke point. Two emits for one state change double it for every counting consumer and make the stream
lie about what happened; and if two call sites both look like the choke point, the real fix is finding the one
that is (or emitting from the single place they both pass through), never picking one and hoping. Distinct facts
that happen to fire together are NOT duplicates — emit both.

⛔ **THIS PAGE CARRIES RULINGS OF ITS OWN, AND THE PAGES BELOW CARRY THE REST — read both.** It is not a
map you may skip; the parts your work touches are read END TO END on top of it, and the count that applies is
something you FIND, not something you decide ([AGENTS.md](../../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[a fact names the happening](03-the-domain-emit-surface-every-fact/01-a-fact-names-the-happening.md)** | ⛔ A FACT NAMES THE HAPPENING — "something changed" IS NOT A FACT |
| **[the named fact families](03-the-domain-emit-surface-every-fact/02-the-named-fact-families.md)** | The named fact families |
| **[worldbuilder emits like any other](03-the-domain-emit-surface-every-fact/03-worldbuilder-emits-like-any-other.md)** | ⛔ WORLDBUILDER EMITS LIKE ANY OTHER PATH — no WB special case, anywhere |

