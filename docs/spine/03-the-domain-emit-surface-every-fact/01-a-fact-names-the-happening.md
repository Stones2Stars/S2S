# ⛔ A FACT NAMES THE HAPPENING — "something changed" IS NOT A FACT

> Part of the **[03-the-domain-emit-surface-every-fact](../03-the-domain-emit-surface-every-fact.md)** spec.

> *"`BUILDING_CHANGED` is not a valid event — it says that 'something happened', not what actually happened. Any
> event that is not specific relies on actual calculation to happen."*

**THE TEST, and it is about the FACT, never about what any consumer currently does with it: does the event name
WHAT HAPPENED, or only that some state moved?** A fact that names only the movement has handed the consumer a
question instead of an answer, and the only way to answer a question is to CALCULATE — so the calculation the
spine exists to delete reappears inside every consumer at once.

⇒ **It is [a staleness flag is the fossil of a missing emit](../../cascade/03-no-staleness-no-selfheal.md#-a-staleness-flag-is-the-fossil-of-an-incomplete-emit-surface--the-same-rule-one-level-up) wearing the emit side's costume.** A
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
> sum exact ([cascade.md](../../cascade.md) § THE INVARIANT) rather than
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
([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)) — so a todo entry reading *"`SEVT_X` is the
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
> [json.md §8](../../specs/json.md)), so a per-count fact added early buys nothing and pays the attribution cost now.
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
> fans its member cities so every one fires its own ([enabler.md §8](../../specs/enabler.md) RESIDENCY; vicinity answers
> `connection:"onSite"` atoms and nothing else).
> ⚠ **The two count-carrying facts fail WORSE than a plain double, and that is why the split is spelled out
> here:** their payload is a multiplicity, so a consumer using it scales the deposit by the count — three local
> copies apply three times — and a supply that only ever grows never hands any of it back.
> ⚑ A GATE re-check on all three is correct and is not this: re-resolving a deposit CONDITIONED on the resource
> is idempotent (it moves the difference), where applying the resource's OWN deposit is not.

⚠ **The ruling above is about EMITS, and it does NOT extend to what a consumer DOES with one.** A surplus emit is
~free; work a consumer performs is paid on the turn path at event volume. So: **emit liberally, apply
precisely** — a consumer acts on exactly the deposits the fact names ([cascade.md](../../cascade.md)
§ THE MAINTAINED SUM), never on a widened mask and never on a whole-scope sweep it could not derive
([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)). Turn DURATION analytics remain the `[PERF]`
phase logs' job, not these facts'.

