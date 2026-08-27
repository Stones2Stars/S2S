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

### ⛔ A FACT NAMES THE HAPPENING — "something changed" IS NOT A FACT

> *"`BUILDING_CHANGED` is not a valid event — it says that 'something happened', not what actually happened. Any
> event that is not specific relies on actual calculation to happen."*

**THE TEST, and it is about the FACT, never about what any consumer currently does with it: does the event name
WHAT HAPPENED, or only that some state moved?** A fact that names only the movement has handed the consumer a
question instead of an answer, and the only way to answer a question is to CALCULATE — so the calculation the
spine exists to delete reappears inside every consumer at once.

⇒ **It is [a staleness flag is the fossil of a missing emit](../cascade/03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) wearing the emit side's costume.** A
staleness flag says *"something in this bucket moved"*; a `*_CHANGED` event with a direction bit says *"something
about this entity moved"*. Both discard the identity of the happening, and both leave the consumer to reconstruct
it. **A non-specific event is a staleness flag that learned to travel.**

**⛔ SO WHERE SEVERAL DISTINCT HAPPENINGS REACH ONE CHOKE POINT, THEY ARE SEVERAL FACTS — never one fact with a
discriminator field.** A payload int that a consumer must branch on to find out what occurred is the tell: the
branch is the calculation, merely relocated from the consumer's body into its `switch`.

⚑ **The tree already contains the correct shape, so this is a convergence and not a new design.** The UNIT plane
is named happenings throughout — `UNIT_CREATED` / `UNIT_KILLED` / `UNIT_ENTERED_CITY` / `UNIT_LEFT_CITY` /
`UNIT_DEATH_SCHEDULED`. Nobody wrote a `UNIT_CHANGED` carrying `±1`, and the reason is visible in what those
facts buy: a consumer acts on each one directly, and none of them has ever needed a companion event to say what
it meant.

⚑ **And the argument is already recorded in this spec's own history, against exactly this failure.**
`SEVT_CITY_FOUNDED` exists because founding *"produced NO identifiable fact before this, only a constellation of
side-effects (`populationChanged`, `plotOwnerChanged`, `cityNetworkChanged`), which is why the settle-time
provisions had no trigger to hang on."* A constellation of `*_CHANGED` movements could not substitute for the
named happening — that is this rule, discovered once and then not generalized. Generalize it.

⛔ **Splitting one `*_CHANGED` into its happenings is NOT the banned DUPLICATE.** A duplicate is ONE happening
announced twice; this is SEVERAL happenings that were being announced as one. The rule above already settles it:
*distinct facts that happen to fire together are not duplicates.* The choke point stays single — it simply emits
the fact that names what it just did.

> **⛔ `*_CHANGED` IS NOT A VALID EVENT NAME. FULL STOP.** *"CHANGED is literally not a valid event
> name — it has to say explicitly what happens."* There is no category of fact that is exempt, and the
> exemptions this section used to list were wrong:
>
> | was | is |
> |---|---|
> | `PLOT_TERRAIN_CHANGED` | `PLOT_TERRAIN_ADDED` · `PLOT_TERRAIN_REMOVED` |
> | `PLOT_FEATURE_CHANGED` | `PLOT_FEATURE_ADDED` · `PLOT_FEATURE_REMOVED` |
> | `PLOT_BONUS_CHANGED` (`iB` = ±1) | `PLOT_BONUS_ADDED` · `PLOT_BONUS_REMOVED` |
> | `BONUS_ADDED`/`_REMOVED` (city, unqualified) | `CITY_BONUS_ADDED` · `CITY_BONUS_REMOVED` |
> | …and every other `*_CHANGED` | the pair of happenings it was standing in for |
>
> **⚖ THE EVENT IS THE OPERATOR; THE PAYLOAD IS ONLY EVER A MAGNITUDE.** *"Events can hold a count, but
> it is literally just a count — it's the event that shapes the subtraction/addition, not the count."* So a
> fact may absolutely carry HOW MANY: `CITY_SPECIALIST_ADDED` with a count of 3 adds three times over, and
> `CITY_SPECIALIST_REMOVED` with a count of 3 withdraws three times over. What the payload must never carry is
> WHICH WAY — the count is unsigned in meaning, and the event name supplies the sign.
> ⛔ So a `±1` in `iB`, a presence boolean in `iA`, or an old value beside a new one are all the same defect:
> a DISCRIMINATOR the consumer must branch on, which is the calculation relocated into a `switch`. A consumer
> learns what happened **by which event it received**, and reads the payload only for how much.
>
> ⚑ **SCOPE-QUALIFY THE NAME** — `PLOT_BONUS_ADDED` beside `CITY_BONUS_ADDED`, same reasoning as the
> `PLOT_BONUS`/`CITY_BONUS` split above.
>
> ⚑ **What this buys, concretely: every consumer's direction-decoding disappears.** A consumer that decodes
> three conventions today — an id pairing, a boolean in `iA`, a signed delta in `iB` — decodes none. And a
> WITHDRAWAL becomes announceable at the moment the old state still holds, which is what makes the maintained
> sum exact ([cascade.md](../cascade.md) § THE INVARIANT) rather than
> dependent on a consumer reconstructing what used to be true.
>
> **⚖ A SCALAR IS NO EXEMPTION, AND THE WORKED CASE IS POPULATION:**
> *"When a city grows a pop it is `CITY_POPULATION_ADDED 1`. If a city loses 2 pop, it is
> `CITY_POPULATION_REMOVED 2`."*
> ⚑ **Note what the payload is: the DELTA as a magnitude, not the new total.** `CITY_POPULATION_ADDED 1`, never
> `POPULATION_SET 7` — a consumer maintaining a sum needs how much MOVED, and a new total would force it to
> subtract against a remembered previous value, which is the derivation this whole rule removes. The one
> consumer that wants the total reads the object, which owns it.
> ⛔ So there is no "a scalar carrying its new value is already specific" carve-out: that was this document's own
> wording and it was wrong. Every fact is `<SCOPE>_<THING>_ADDED` / `_REMOVED` with an unsigned magnitude.
>
> ⛔ **Splitting is NOT the banned duplicate** (above): a duplicate is ONE happening announced twice, this is
> SEVERAL announced as one.

**⛔ TOO MANY EVENTS IS BETTER THAN NOT ENOUGH — and if an emit is found not to exist, ADD IT.** When
weighing whether some mutation "deserves" an event, the answer is EMIT. The costs are wildly asymmetric: a
MISSING emit is a silently wrong value that no compiler and no runtime catches, found only by someone noticing a
number is off; a SURPLUS emit costs one consumer branch that declines to act. Never agonize over the judgement —
if it moves state, it emits.

**⛔ AN EVENT GAP IS CLOSED THE MOMENT IT IS FOUND — NEVER RECORDED AND LEFT.** Finding one is not a
discovery to write down; it *is* the work item, and it is done now. This is stronger than the ruling above, and
it binds the same way whichever form the hole takes: a **missing emit** (nothing announces the fact), a
**missing FIELD** on an existing fact (the old-value case above — it fires but cannot be acted on), or a
**missing CONSUMER ROUTE** (the fact is on the wire and the store that needs it ignores it — §6). All three leave a
stored value permanently wrong and none self-heals
([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)) — so a todo entry reading *"`SEVT_X` is the
hook"* is a value that stays wrong for exactly as long as that entry sits there.
⚑ **Why it is a hard rule and not a priority call:** closing one costs almost nothing while the trace is in
front of you, and it never gets cheaper — the next agent must re-derive which fact was missing, which consumer
wanted it, and why. Deferring converts a few minutes of wiring into a re-investigation.
⚠ It does NOT license guessing a structure: if the route needs a design decision the specs do not answer,
surface that — but the gap still closes in the same work item.

> **⚖ THE COUNTER-CASE, so the rule is not misapplied: a deliberately-drawn SCOPE BOUNDARY is not a gap.** The
> worked instance is the city's **obtained-bonus** pair (`SEVT_CITY_BONUS_ADDED` / `_REMOVED`), which announces the **PRESENCE
> CROSSING ONLY** — 0 ⇄ non-zero — because `processNumBonusChange` reaches `processBonus` solely when the
> has-verdict crosses zero. A count moving 2 → 3 therefore announces nothing, and **that is the ruling, not an
> oversight:** a per-count fact would force the engine to start *"processing all sorts of extra edge
> cases about what city added the bonus"* — attribution the crossing sidesteps entirely.
> ⚠ What is knowingly outside the boundary: a count-THRESHOLD reader (a `min: 3` requires-atom, a `per`
> count-scaler) does not re-evaluate on a move between non-zero counts. Accepted for now.
> ⚑ **The REVISIT TRIGGER is named, and it is VOLUMETRIC** — when a resource stops being present/absent
> and becomes a QUANTITY a city draws against, the crossing stops being sufficient and this reopens. ⚠ But it
> reopens **as part of that work, never ahead of it**: *"then we also have to implement a ton of other things"*.
> Volumetric is a model-wide change (the same direction the amenity id→COUNT dictionary is already shaped for,
> [json.md §8](../specs/json.md)), so a per-count fact added early buys nothing and pays the attribution cost now.
> ⛔ Do not treat the named trigger as a licence to start: it marks WHEN, not a standing invitation.
> ⛔ So do **not** add a per-count bonus fact, and do not read the absence of one as a hole to close — it would
> also be a near-duplicate of the crossing fact on every 0 ⇄ 1 transition. **The test that separates the two: a
> gap is a fact nobody DECIDED to leave out.** Ask whether the omission is recorded as a decision; if it is,
> the rule above does not apply to it.
>
> **⛔ THREE FACTS DESCRIBE ONE RESOURCE REACHING ONE CITY, AND ONLY ONE OF THEM IS A CROSSING — a consumer that
> acts on more than one counts the same holding twice.** They are easy to mistake for one family because they
> share a payload slot and a name stem:
>
> | fact | what it announces | payload `iA` |
> |---|---|---|
> | **`SEVT_CITY_BONUS_ADDED` / `_REMOVED`** | the CITY's has-verdict (above) | — (a crossing) |
> | **`SEVT_CITY_VICINITY_BONUS_ADDED` / `_REMOVED`** | the city's LOCAL supply COUNT moving | how many |
> | **`SEVT_PLOTGROUP_BONUS_ADDED` / `_REMOVED`** | the NETWORK component's holdings moving | how many |
>
> ⇒ **The has-verdict is the only one a value may be applied on.** The other two are the same holding seen from
> the local tile set and from the connectivity component, and each already CAUSES the crossing — the plot group
> fans its member cities so every one fires its own ([enabler.md §8](../specs/enabler.md) RESIDENCY; vicinity answers
> `connection:"onSite"` atoms and nothing else).
> ⚠ **The two count-carrying facts fail WORSE than a plain double, and that is why the split is spelled out
> here:** their payload is a multiplicity, so a consumer using it scales the deposit by the count — three local
> copies apply three times — and a supply that only ever grows never hands any of it back.
> ⚑ A GATE re-check on all three is correct and is not this: re-resolving a deposit CONDITIONED on the resource
> is idempotent (it moves the difference), where applying the resource's OWN deposit is not.

⚠ **The ruling above is about EMITS, and it does NOT extend to what a consumer DOES with one.** A surplus emit is
~free; work a consumer performs is paid on the turn path at event volume. So: **emit liberally, apply
precisely** — a consumer acts on exactly the deposits the fact names ([cascade.md](../cascade.md)
§ THE MAINTAINED SUM), never on a widened mask and never on a whole-scope sweep it could not derive
([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). Turn DURATION analytics remain the `[PERF]`
phase logs' job, not these facts'.

### The named fact families

⚖ **BESIDE THE SUBSTRATE FACTS, THE PLOT ANNOUNCES ITS OWN DERIVED VERDICT: `SEVT_PLOT_PREDICATE_ADDED /
_REMOVED`, carrying the `CASC_PRED_*` id.** It is emitted by `PlotContext` — the store that OWNS the verdict —
at the 0 ⇄ 1 crossing and nowhere else, exactly as the amenity fold announces its own crossings: the fold IS
the maintenance path, so an emit anywhere else would be a second one.
⛔ **It is NOT a duplicate of the substrate fact and never replaces one.** A substrate fact says what the TILE
now CARRIES; this says what that MEANS for the one predicate that moved. A consumer routing on a substrate id
is asking about the SOURCE; a consumer routing on this is asking about the VERDICT.
⚑ **It exists because the city cannot derive it.** `CityContext.plotAttrs` is the fold of its member plots'
bits, and by the time any consumer runs the plot already holds the NEW value — so a city-side "unfold the old
bits, refold the new" is impossible, not merely wasteful ([contexts.md](../cascade.md): the plot
sends its bit UP, the city never reaches down). With the fact, a member plot's bit is one `add(bit, ±1)`.
⚠ Its absence would not read as a stale gate but as a **COMPOUNDING MAGNITUDE**: `plotAttrs` is plane B's
COUNT, so a bit never withdrawn leaves every deposit scaled on it permanently inflated, and inflated further on
every later substrate change.

⛔ **THE SUBSTRATE FACTS ARE `ADDED`/`REMOVED` PAIRS, NOT `CHANGED`.** Terrain / feature /
improvement / route each announce a source LEAVING and a source ARRIVING as two facts, because each end is its
own consumer work: the old source's deposits are withdrawn, the new source's applied. ⚑ Carrying the old value
in `iA` on one `CHANGED` fact was the earlier shape and it is what left the gap — a single "the slot moved" fact
makes every consumer DERIVE the removal, and the derivation is impossible once the state has moved. A `REMOVED`
fact is emitted while the old state still holds, so a withdrawal resolves against exactly what it deposited
([cascade.md](../cascade.md) § THE INVARIANT).
⚖ **THE WHOLE FAMILY IS `<SCOPE>_<THING>_ADDED` / `_REMOVED`, SCOPE-QUALIFIED:** `PLOT_BONUS_ADDED` /
`PLOT_BONUS_REMOVED` beside `CITY_BONUS_ADDED` / `CITY_BONUS_REMOVED` — a resource appearing ON A TILE and a
city GAINING that resource are different happenings with different consumers, so the scope belongs in the
name rather than in a reader's head.
⛔ **AND A ±1 IN THE PAYLOAD IS NOT A SUBSTITUTE FOR THE NAME.** A `CHANGED` fact carrying a placed/removed
delta still hands the consumer a discriminator to branch on, which is the calculation relocated into a
`switch` — it is an improvement on an old-id payload and it is not the answer. The direction belongs in the
FACT'S IDENTITY, where a consumer reads it by arriving at all.
⚑ **The payoff is that every consumer's direction-decoding collapses.** A consumer that today decodes three
conventions — an id pairing, a presence boolean in `iA`, a signed delta in `iB` — decodes none: the event it
received IS the direction. The **commerce SLIDERS** are on the surface too
(`SEVT_EMPIRE_COMMERCE_PERCENT_ADDED / _REMOVED`, `CvPlayer::setCommercePercent` — the one choke point
`changeCommercePercent` / `verifyGoldCommercePercent` / `changeCommerceFlexibleCount` all reach the value
through): a slider is synced player state every city's realized per-commerce rate is built on
([modifier.md](../cascade.md) §2a), so DOMAIN. ⚠ **ONE slider move emits SEVERAL facts** — the setter
REBALANCES the other channels in place to hold the total at 100, writing them directly rather than recursing, so
each channel it moves emits its own fact; a consumer reading only the caller's channel sees a 100-total that
does not add up. **PROPERTY VALUES** are on the surface too (`SEVT_PROPERTY_ADDED / _REMOVED`, the three
`CvProperties` mutation choke points — `setValue` plus the two new-property `push_back` branches, which
`changeValue` / `changeValueByProperty` / `setValueByProperty` all funnel through): `PROPERTY_*` is one cascade
channel per property info ([cascade.md](../cascade.md)), read by
`CityContext::propertyValue`, by every `requires.operate` property BAND ([enabler.md](../specs/enabler.md) §3) and
by every threshold-conditioned deposit, and the value is synced save-carried state that folds into the OOS
checksum — so DOMAIN. The fact names the object KIND beside the object id, because a city id and a plot id are
otherwise the same int. It is emitted at the three `CvProperties` mutation choke points, which every owner scope
funnels through. ⚠ The solver's change PROPAGATION fans one change onto OTHER objects, each of which re-enters
the mutation path — distinct objects' facts, so each emits. The object RESET path (`CvProperties::clear`)
deliberately announces nothing (it runs before there is an id or an owner to name — `CvCity::read` / `CvUnit::read`
call `reset()` as their first act).

⚖ **`isPowered()` announces ONCE, and what it announces is the VERDICT — never a leg.** `CvCity::isPowered` is
the ONE definition (a live grantor supplies power AND no blackout gates delivery), and its crossing is announced
by the AMENITY FOLD as `SEVT_CITY_POWER_ADDED / _REMOVED` — the fact the modifier's plane-C route and the
enabler's gate both ride. Its inputs reach that fold and stop there: the grantor crossing itself, and the
blackout status (`SEVT_CITY_STATUS_ADDED / _REMOVED` carrying `CITYSTATUS_POWER_DISABLED`), which is MIDDLEWARE
gating delivery and is never a cascade input ([state.md](../specs/state.md) § A STATUS IS MIDDLEWARE).
⛔ **Announcing a LEG instead would be wrong twice**: no single leg is the verdict (a second plant built during a
blackout moves the store and delivers nothing; a blackout lifting delivers power with the store unmoved), and
routing several legs into one plane-C application would double-apply. ⚠ A status TICKS DOWN every turn, so it
emits at the derived 0-CROSSING only, never per decrement — a counter that moves on a schedule is not a state
change until its verdict flips, and this is the general rule for every timer-backed fact.

> **⚖ THE THRESHOLD CROSSING IS ITS OWN FACT, AND THE HOLDER OF THE VALUE ANNOUNCES IT.** *"There
> should be events for when a threshold actually changes; that is done on the holder … if power goes from 0 to
> 1 an event is emitted, but another event is not emitted from 1 to 2 — and if 1 to 0, then power removed is
> emitted."* So a value's own fact says the VALUE moved, and a SECOND fact beside it says a VERDICT built on
> that value crossed. The two are different happenings with different consumers, and the second is the one a
> gate routes on.
> ⚑ **Power is the shape; it generalizes to every threshold.** The second instance is the PROPERTY BAND:
> `SEVT_PROPERTY_ADDED / _REMOVED` announces the value, which the solver moves for nearly every property of
> every city every turn, while **`SEVT_CITY_PROPERTY_BAND_ADDED / _REMOVED`** announces the far rarer crossing
> of a boundary some `requires.operate` clause actually declares ([enabler.md §3](../specs/enabler.md)). The third is
> the **CORPORATION-ACTIVE verdict** (`SEVT_CITY_CORPORATION_ACTIVE_ADDED / _REMOVED`): the `{HAS_CORPORATION}`
> verdict is a four-leg engine composition (`CvCity::isActiveCorporation` — presence, the player-level state,
> the obsoleting tech, a consumed bonus held), so no leg's fact is it — CityContext's verdict store re-reads
> the one engine implementation on each leg's fact and announces only a genuine crossing
> ([contexts.md](../cascade.md)). The corporation PRESENCE pair is one leg and must never route
> the `{HAS_CORPORATION}`-gated deposits: a present-but-dormant corporation is the case that separates them.
> ⛔ **The detection belongs to the HOLDER, never to each consumer.** A consumer that gates on the raw value
> fact re-derives the same sweep once per consumer AND pays it per event — and the boundaries are one registry
> (`EnablerKernel::propertyBandThresholds`), so testing them anywhere else is a second implementation
> ([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
> ⚑ **And it is what makes plane C's WITHDRAWAL exact.** If the fact IS the crossing, a consumer applies or
> withdraws on the fact's IDENTITY and never re-tests the atom — so it never depends on reading state the
> mutation has already moved past, which is the one thing
> [cascade.md](../cascade.md) § THE INVARIANT cannot enforce for itself.
> ⚠ A band fact is deliberately DIRECTION-LESS in effect: the consumer re-reads the live value against each
> band, so which way the boundary was crossed is redundant once the fact says one was.

Beside them: **`SEVT_CITY_HEADQUARTERS_ADDED / _REMOVED`** (`CvGame::setHeadquarters`, per affected city — the
`setHolyCity` shape, and **not** a duplicate of the building/corporation PRESENCE facts the same setter drives),
**`SEVT_PLOT_CITY_ADDED / _REMOVED`** (`CvPlot::setPlotCity` — the ONE emit covering its `changeCityRadiusCount` /
`changePlayerCityRadiusCount` pass-throughs), **`SEVT_CITY_AMENITY_ADDED / _REMOVED`** (the city's AMENITY FOLD
crossing 0 ⇄ non-zero on ONE key, carrying the `AMENITY_*` id in `iType` — an OPEN-registry member id, the
`SEVT_CITY_STATUS` shape, so a newly authored amenity needs no engine change. It is emitted by the FOLD, the
store that owns the verdict, and by nothing else. ⚠ Government centre and fresh water ride THIS fact and carry
no pair of their own: nothing gates their delivery, so the refcount crossing IS their verdict and a bespoke
fact for either would be one happening announced twice. POWER is the exception and keeps its own pair — it
announces the GATED verdict (`isPowered`), which genuinely differs from the store crossing),
**`SEVT_EMPIRE_ANARCHY_ADDED / _REMOVED`** (`CvPlayer::changeAnarchyTurns`), **`SEVT_TEAM_MEMBER_ADDED / _REMOVED`**
and **`SEVT_AREA_TILE_ADDED / _REMOVED`** (the two bare counters `EmpireContext::teamMemberCount` / `CityContext`'s
AREA_SIZE + max-adjacent-water read), and **`SEVT_WORLD_UNIT_CREATED_COUNT_ADDED`** (the world-instance cap's
cumulative counter — distinct from `SEVT_EMPIRE_UNIT_COUNT_ADDED / _REMOVED`, the player's LIVE per-type tally, and
from `SEVT_UNIT_CREATED`, the instance; all three fire at one birth and none duplicates another).

**THE UNIT PLANE has its mark triggers** — [cascade.md](../cascade.md) specifies a
unit's resolved values move on a promotion or combat-class change plus one seeding gather at birth:
`SEVT_UNIT_PROMOTION_ADDED / _REMOVED` (`CvUnit::processPromotion`, the ONE funnel both `setHasPromotion`
overloads reach), `SEVT_UNIT_COMBAT_ADDED / _REMOVED` (`CvUnit::processUnitCombat`, reached once past
`setHasUnitCombat`'s change guard AND its game-option/spy validity gate), and `SEVT_UNIT_CREATED` itself — the
seed that serves the unit's OWN info's share (the non-delta slots, vision above all, carry the unit's base),
without which a unit holding no promotion and no extra combat class never gathered and read 0 sight. ⚠ At LOAD
the seed is the unit marking ITSELF at the end of its own `read()` — the consumer's mark cannot serve a
save-carried unit, because its getUnit lookup runs while the player's unit list is still mid-stream and silently
resolves nothing; the created/promotion facts remain the play-time triggers. **`SEVT_UNIT_KILLED`** is the DEATH
TWIN `SEVT_UNIT_CREATED` lacked — without it grants and the out-of-process replay see units born and never die.
⛔ Its correctness is **STRUCTURAL, not positional**: it is emitted on the FIRST line of **`CvUnit::die`**, the
one function that ends a unit's life, which carries no early return and no conditional deletion and always ends
in `deleteUnit` ([unit-lifecycle.md](../reference/unit-lifecycle.md)). The outcomes that leave a unit ALIVE
(evacuate-to-capital, last-stand survival) are decided BEFORE `die()` is entered and never reach it, so a new
outcome cannot silently slip in ahead of the fact — the shape a placement "past every early return" could not
guarantee. An OFF-MAP death is a real outcome of that function, not a skipped one: `iSrcLoc` is -1 and the unit
is deleted exactly as an on-map one is. Beside it, **`SEVT_UNIT_DEATH_SCHEDULE_ADDED / _REMOVED`** carries
`m_bDeathDelay`, the save-carried state a DELAYED kill leaves behind so the object outlives combat resolution,
read across the engine through `isDelayedDeath()`/`isDead()`. It is **not** a duplicate of KILLED: a scheduled
death is an INTENTION whose outcome can still flip to survival, so a consumer treating it as a death would bury
units that walk away. ⚠ Both TRANSITIONS announce (scheduled, and cleared by either survival outcome) — a
one-way fact would leave a survivor permanently marked dying — and `CvUnit::read` carries the in-read half for a
save taken mid-schedule. **`SEVT_UNIT_LEFT_CITY`** is the leave twin of `SEVT_UNIT_ENTERED_CITY`; ⚠ it is
announced for EVERY city plot a unit vacates while the ENTRY's conquest branch resolves into an acquisition
instead of an entry, so the two do NOT net to occupancy — a consumer needing occupancy reads the unit's live
plot.

**GAME OPTIONS and DIFFICULTY announce** — the two facts every maintained verdict is built on but nothing used to
announce. **`SEVT_GAME_OPTION_ADDED / _REMOVED`** (`CvGame::setOption` / `setModderGameOption`, both unguarded so
the emit supplies the flip guard): an option is the ONE axis an entity-level gate reads
([the whole-entity applicability gate](../specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)), and options are read BELOW that level too (civics
carry option-gated production / happiness / commerce deposits), so a flip moves gate verdicts AND deposits at
once. ⚠ It carries TWO id spaces, so `iB` = `GameOptionSpace` disambiguates them (the `SEVT_PROPERTY_ADDED /
_REMOVED` shape — a game-option id and a modder-option id are otherwise the same int). **`SEVT_EMPIRE_HANDICAP_ADDED
/ _REMOVED`** (`CvPlayer::setHandicap`) is a genuine cascade input rather than observability: the gather folds the
handicap's own modifier families into that player's packages, so **FLEXIBLE DIFFICULTY moving it silently froze
every handicap-derived deposit at the old difficulty** with nothing to re-derive it. **`SEVT_GAME_HANDICAP_ADDED /
_REMOVED`** (`CvGame::setHandicapType`) is its DISTINCT twin, not a duplicate — the derived average over alive
humans that every `getAI*` advantage reads ([engine.md](../reference/engine.md): AI advantages scale with the
HUMAN's difficulty), derived and never saved, so it needs no in-read half.
**`SEVT_GAME_GLOBAL_DEFINE_ADDED / _REMOVED`** completes that surface from the other side — the three
`cvInternalGlobals::setDefine*` setters, i.e. the **LIVE-OPTION bridge**: a BUG option fires a Python callback →
`GC.setDefineINT` → `cacheGlobals()`, so a user can flip an engine tunable at any time mid-game. It was the one
mutation of that class with no fact at all, which made a live option unreactable by construction.
⚠ It announces ONLY on the genuine LOCAL set: the `bUpdate` path sends a net message and
`CvGlobalDefineUpdate::Execute` calls straight back in with `bUpdate=false`, so announcing on both paths would
double-emit one change on the initiating machine. And a define is STRING-KEYED with no id space, so the NAME
rides as a render field (the `SEVT_NAME_CHANGE` precedent) and a machine consumer keys on that, not the ints.
⛔ Its existence does NOT make a live option something authored data may gate on — that ruling
([python-read-map.md](../reference/python-read-map.md)) is about a value moving under static data and is unchanged;
the fact closes reactability only.
⚑ **Only the GAME space routes anywhere, and the two spaces differ in KIND, not just in id range.** A
`GAMEOPTION_` is fixed at setup, which is what lets an entity gate depend on it; a `MODDERGAMEOPTION_` is set
from the BUG menu at any time (`setModderGameOption` + a net message for MP sync), so it is a LIVE option
wearing a confusingly similar name. Authored data honours that split — **no** authored gate or condition names a
`MODDERGAMEOPTION_` — so a modder flip (a slider such as the leader-promotion culture threshold
`MODDERGAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT`) moves no verdict and no deposit. It still EMITS, being a
genuine synced state change; it simply marks nothing. That is "emit liberally, mark precisely" as a routing rule
rather than a slogan. ⛔ Separating the two by grep needs a negative lookbehind — `MODDERGAMEOPTION_` contains
`GAMEOPTION_`, so a naive scan conflates them.
⚑ Both option and difficulty route to **WHOLESALE** consumer work (the enabler re-gates every city; the modifier
marks the affected player's packages whole) — the `SEVT_AREAS_RECALCULATED` shape, sanctioned for the same
reason: the fact names no source to route from, so no finer derivation exists, and it is not the banned
self-heal, which papers over a MISSED invalidation rather than announcing a genuine wholesale one.

### ⛔ WORLDBUILDER EMITS LIKE ANY OTHER PATH — no WB special case, anywhere

**⚑ WHY it is categorically different: *"WorldBuilder can add or remove anything, at will."*** Every
other surface reaches state through a genuine acquisition — a building is CONSTRUCTED, a unit is TRAINED, a tech
is RESEARCHED — and this whole spine is built on that: one fact, emitted at the genuine mutation choke point. WB
instead mutates arbitrary state directly, so it can violate every invariant the model rests on — an entity
appearing with no acquisition, vanishing with no death, changing owner with no conquest. **A WB edit that changes
state silently leaves every cache, context and enabler set wrong, exactly as a missing emit does**; WB is simply
the surface that can produce that condition deliberately, on any field, in one click.

⛔ **So WB adding or removing anything EMITS, exactly as the normal path does.** Do NOT build a "WorldBuilder
mode" that suppresses or reroutes facts: a second, quieter mutation path is precisely the hole this model closes.
- **ADDING is "grants on demand"** — the grants machine hands an entity over on a genuine acquisition;
  WB hands the same entity over on a click. From the model's side they are the SAME event: same DOMAIN fact,
  every consumer reacting identically ([triggers.md](../specs/triggers.md)).
- ⚠ **REMOVING is the mirror, and *"we do not have any"*** — a WB removal is an inverse grant, and the
  machine has no such notion, so the remove side cannot lean on precedent the way adding can. ⛔ The answer is
  NOT grant-removal machinery for WB's sake: the removal FACT must exist and be emitted, the same fact a genuine
  in-play removal would announce. **Those facts are THINNEST exactly where WB is most arbitrary**, because normal
  gameplay rarely removes — a tech is monotonic in play, but WB can un-research one. Expect to FIND MISSING
  removal facts rather than merely route existing ones; per *"add all the events, ever"*, the answer to a missing
  one is to add it.

⚖ **WB does not CONSTRAIN a cut, and that is not licence to leave a break.** It *"will need a real review
and pass, post rework"* and may temporarily lag — so a WB path is never a reason to preserve a shape or keep a
legacy call alive. ⛔ But *"we cannot accept actually breaking worldbuilder stuff, we fix things we see"*: a WB
path that shows up broken, in a log or on screen, is wired onto the new surface like any other consumer, never
patched by restoring a legacy binding. The misreading that has already cost a pass is reading "not a constraint"
as "WB errors are accepted breakage" and skipping them in a sweep.

