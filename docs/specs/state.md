# Unit states — glossary

The catalogue of a unit's **transient states** — fired → counted down → over. This is the **glossary** (the
namings); the **system** is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Greenfield — open by design.** `state` was **never a first-class concept**: it's been faked via
> **pseudo-promotions** and **Python event handlers**, and this glossary formalizes it. Like its sibling
> registries ([json.md §8](json.md)) the member set grows as states are identified from the data — an ongoing
> activity, not a gap to close.

> ⛔ **THIS DOC DOES NOT SPEC HOW A STATUS IS AUTHORED — THAT SHAPE DOES NOT EXIST YET.** Neither this file nor
> [json.md §8](json.md) defines a JSON authoring shape for a status (timer / trigger / effect / expiry). Nothing
> today is authored as data at all: every current status is applied only via a pseudo-promotion or a Python event
> handler (see "Historically NOT a data block" below) and read through the runtime `hasStatus`/`ContextDict`
> model this file DOES define. **Do not read this file as a complete data-model spec, and do not invent the
> authoring shape to fill the gap** — that is exactly the guessing [the no-guessing rule](../../AGENTS.md#conduct)
> bans. The shape is undesigned; see [Open](#open).

> **⚖ STATUS IS A SCOPE CONCEPT, NOT A UNIT ONE.** A unit is PARALYZED, a PLAYER is in a GOLDEN AGE, a
> CITY is CELEBRATING — all three are the same mechanic: applied, ticking down every turn, over at zero. So each
> scope that carries statuses gets its own enum and the identical store / accessor / tick shape on its owner
> (`Engine/CvStatus.h`), instead of every timer being a hand-named member with its own getter, setter,
> decrement and save field. ⚑ That is the whole value: the legacy engine wrote this mechanic out longhand once
> per timer.
> ⚠ **A status change is not always a bare decrement.** Where crossing zero has CONSEQUENCES — a golden age
> starting cancels anarchy, announces its fact and re-yields — the crossing keeps its side-effect surface; the
> store replaces the hand-named counter, never the crossing logic ([save.md §6](save.md): audit a changer's
> whole body before cutting it).

> ⚖ **A DURATION-1 STATUS IS THE NATURAL SHAPE FOR "WHILE X HOLDS".** We Love the King/Emperor Day is a
> ONE-TURN status re-applied every turn by a trigger while its conditions match — so it lapses by simply not
> being re-applied, and needs no separate clear. The trigger owns the TEST; the counter owns the ENDING.
> ⛔ **Its legacy trigger wiring STAYS:** re-homing that per-turn condition test is funky, so the status
> owns the storage and the read while the existing code owns deciding whether the conditions match. That is a
> ruled carve-out, not a half-migration to finish opportunistically.

## What a state is (recap)
- **A SPECIFIC COUNTER, DECREMENTED EVERY TURN** — applied to the unit, ticking down, over at zero.
  Unlike a *mutable* skill (persists until changed) or an *immutable* tag (set at creation).
- ⚑ **The block's name is `status`** ([json.md §8](json.md)); this file is its glossary.
- ⛔ **A status is NOT a skill**, and mis-filing one as a skill is a recurring error the owner has rejected more
  than once: a skill is an ability the unit HAS, a status is a condition something PUT ON it for N turns. The
  curator therefore maps no status tag into `skills` — an unmapped tag reports loudly instead.
- **The read is `count > 0`** — a status HOLDS while its value is above zero, the ordinary
  `ContextDict` semantic ([contexts.md](../cascade.md)). Expiry IS the counter reaching 0; there
  is no separate present/absent plane beside it.
- ⚠ **It is id→COUNT like a city's `amenities`, but the COUNT MEANS SOMETHING ELSE** — an amenity's count is a
  refcount of live grantors (moved when events add or repeal one), a status's is TURNS REMAINING and moves on
  its own. Same shape, different model; do not merge the mechanisms.
- Historically NOT a data block — a pseudo-promotion or a Python event stands in for it.

> **⚖ OPEN BY DESIGN — when we find more, we add more.** The `UnitStatus` enum is a HAND-MAINTAINED
> list and identifying new statuses is an ongoing activity for the life of the mod, exactly as it is for
> [tags](tags.md): a new member is a one-line addition, and **more arriving is the normal state, never a gap to
> close**. ⛔ So this glossary is never "incomplete" against a finish line, and the short list is not a backlog.
> ⚑ That nothing but an EVENT applies one today, and that no data authors one, is likewise BY DESIGN — the
> standardization is the deliverable; the empty authoring surface is the model working.

## States

**UNIT** (`UnitStatus`)

| state | meaning | legacy mechanism |
|---|---|---|
| `paralyze` | immobilises the unit for the turn (`setImmobileTimer(1)`) | a promotion granted by an event |
| … | *(to identify)* | pseudo-promotions / Python event handlers |

**CITY** (`CityStatus`)

| state | meaning | legacy mechanism |
|---|---|---|
| `weLoveTheKingDay` | the celebration; a DURATION-1 status re-applied every turn while its conditions hold | its own bool + timer |
| `powerDisabled` | a blackout — the city's power is out for N turns and comes back on its own | `m_iDisabledPowerTimer`, longhand |

**PLAYER** (`PlayerStatus`)

| state | meaning | legacy mechanism |
|---|---|---|
| `goldenAge` | the empire-wide boost period. Starting one **instantly ENDS anarchy** — not merely mutually exclusive with it ([golden-age.md](../reference/golden-age.md)) | `m_iGoldenAgeTurns`, longhand |
| `anarchy` | the empire is in anarchy — **city disorder, empire-wide** (the callout below) | `m_iAnarchyTurns`, longhand |
| `revolutionCooldown` | how long until CIVICS may be changed again | `m_iRevolutionTimer`, longhand |
| `conversionCooldown` | how long until the STATE RELIGION may be changed again | `m_iConversionTimer`, longhand |

⛔ **None is wired, deliberately** — see the carve-out below; `CvPlayer` carries no status store at all.

> **⚖ ANARCHY IS CITY DISORDER, EMPIRE-WIDE — AND IT IS AN EMPIRE-SIDE STATUS IN ITS OWN RIGHT.** The
> city half is already the model working: `CvCity::isDisorder()` is `isOccupation() || owner.isAnarchy()`, so
> an anarchic empire's cities are in disorder and the [economy.md](../reference/economy.md) participation gate
> already declines their packages at the Σ. ⛔ That does NOT make anarchy a purely city-landed effect: it
> ALSO acts at the empire — the research line is replaced by an anarchy COUNTDOWN — so the player genuinely
> HOLDS a status rather than merely sourcing one the cities hold.
> ⚑ **Which is why the PLAYER scope carries statuses at all, and why the list above is not just the two.**
> The anarchy COOLDOWNS are the same shape and the same scope: each bars its action until its counter reaches
> zero, gated `> 0` and ticked once per turn, with the UI reading *"while in anarchy"* and then *"wait N more
> turns"* off them. ⛔ So they are not timers to leave longhand on the grounds that anarchy is the interesting
> one — they are members of this enum.
> ⚖ **AND THE COOLDOWN IS PER-ACTION: TWO members, never one.** A civic change and a religion change
> bar only THEMSELVES, from counters set at different moments, so folding them into a single empire cooldown
> would make converting your religion bar a civic swap. ⛔ That is a BEHAVIOUR change wearing a consolidation,
> which is exactly what the one-member reading looks like from the outside — the two counters are the mechanic,
> not duplication to tidy.
> ⚠ **The city fan is therefore anarchy's CITY HALF, never its whole expression** — reading the
> land-on-each-city design below as the complete model is what leaves the empire-side effects homeless.

> **⛔ GOLDEN AGE AND ANARCHY ARE THE TWO DELIBERATE EXCEPTIONS, AND THEY ARE NOT WIRED.** They remain the
> hand-named `m_iGoldenAgeTurns` / `m_iAnarchyTurns` on `CvPlayer`, and **the existing engine handles their
> empire-wide effect today**. `PlayerStatus` is forward intent; nothing implements it, and `CvPlayer` carries no
> status store. **A held decision, not an unfinished conversion** — do not read the enum entry as wired.
>
> **⚖ THE DESIGN IS SETTLED — this is SEQUENCING, not an open question.** Their CITY-reaching half
> resolves by **landing a status ON EACH CITY**, driven by the empire-scope happening:
> `SEVT_EMPIRE_GOLDEN_AGE_ADDED` / `_REMOVED` and `SEVT_EMPIRE_ANARCHY_ADDED` / `_REMOVED`, both of which
> **already exist**. The player holds the status; each city holds the per-city EFFECT as an ordinary city
> status. What is missing is only the consumer that does the landing.
> ⚑ **So the object-local rule is not broken by them — it is RESTORED by the fan.** Once the per-city effect is
> city-held, `hasStatus()` at the point of use is the whole wiring again, exactly as everywhere else.
> ⚠ **But the fan is not the whole model** (the callout above): an empire-side effect — the anarchy countdown
> displacing the research line, the two post-anarchy countdowns barring their own action — is held and read at
> the PLAYER, and has no city to land on.
>
> **⛔ AND IT IS BUILT AT THE END, WHEN THE STRUCTURE IS SET — NOT AS PART OF INITIAL SETUP:** *"that is
> how rollerskating happens."* ⛔ Do not wire the consumer now, and do not re-home the two members onto a player
> store to "prepare" for it — both look like progress while pre-committing a structure that is not settled yet.
> ⚠ This is an owner-ruled ORDERING, so ["deferred" is banned](../../AGENTS.md#design) does not
> reach it: the work is named, its design is decided, and its position in the sequence is the ruling.

> **⚖ THE STORE IS SERIALIZED; WHAT IS NOT CARRIED IS THE CONVERSION.** Turns-remaining is genuine
> NON-DERIVABLE state — nothing reconstructs *"three turns of blackout left"* from anything else — so it is
> exactly the class [save.md §5](save.md) keeps a serialized store for, and
> [derived data is never trusted from a save](save.md#5-derived-data-serializes-nothing-) does not reach it: that
> rule bans serializing DERIVED data.
> ⛔ **What is deliberately dropped is the MIGRATION of a legacy timer into the store.** Re-homing one deletes
> its old save field, so an existing save's in-flight value is lost — *"we just don't convert the old statuses
> to the new object for virtually no real gain"*. **The blackout is the worked case:** a save taken mid-blackout
> loads with the power already back on. The old tag is named in `Assets/savemigration.txt` and drains
> ([save.md §3](save.md)).
> ⚑ **The recipe generalizes to every status that follows:** re-home onto the store, name the old tag, take the
> one-time loss. There is no per-status migration to design, and none is worth designing.
> ⚠ Its PLAYER-ALERT ("power restored") died with the per-turn maintainer, as those alerts do, and comes back
> as a CONSUMER of the fact ([spine.md](../spine.md)).

> **⚖ A STATUS ACTS ON ITS OWN OBJECT — ITS CONSUMERS ARE NOTIFICATIONS AND LOGGING.** *"Not a lot of
> things outside of notifications and logging actually care about statuses; they mostly have an effect on the
> actual ongoings on its own object."* A paralyzed unit cannot move, a blackout city is not powered, a
> celebrating city pays no maintenance — the effect lands **where the status is held**, so almost nothing
> downstream needs to hear about it at all.
> ⛔ **So a status gets NO context store, NO dictionary and NO mirror anywhere.** It is object-owned and O(1), so
> it is FORWARDED under the STORES-vs-FORWARDS split ([contexts.md](../cascade.md)) — storing it a
> second time would be the duplication that rule exists to prevent. ⚠ This is the sentence to re-read before
> "wiring statuses into" anything: the wiring is a `hasStatus` call at the point of use.
> ⚑ **That is also why the generic fact is enough.** With no machine folding on a status, per-status facts would
> buy a routing precision nobody consumes — while costing an engine change per authored status.
> ⚠ **Two things qualify this, and they are DIFFERENT KINDS of exception — do not collapse them.**
> - **A cross-machine READER, effect still object-local.** `CITYSTATUS_POWER_DISABLED` gates
>   `CvCity::isPowered()`, which `HAS_POWER` reads. The rule survives intact: the consumer reads the verdict off
>   the CITY and the fact only tells it to re-gate — the fact carries the trigger, never the value. Something
>   outside listens; the effect still lands where the status is held.
> - **An EFFECT that lands on ANOTHER scope's objects** — golden age and anarchy, held by the player and acting
>   on every city (the carve-out above). These resolve by FANNING the empire happening into a per-city status,
>   after which they are ordinary again; they are unwired on ORDER, not on doubt.
> ⇒ **The test for any new status: where does its EFFECT land?** On its holder ⇒ ordinary, wire it. On a
> different scope's objects ⇒ it is the golden-age class: the holder announces, and a consumer lands a status on
> each object the effect reaches. ⛔ The storage never moves up to the announcing scope.

> **⛔ A STATUS IS MIDDLEWARE BETWEEN A SOURCE AND ITS TARGETS — IT GATES WHAT IS DELIVERED, NEVER WHAT IS STORED
>.** *"What status should do is live as kind of middleware, inputs and outputs — so if blackout, even
> though power amenity is operational, it doesn't get to the targets."* The source keeps its own truth and the
> status decides whether that truth reaches anyone.
> ⚑ **The worked case is POWER, and the two planes stay entirely separate** (§ the id→COUNT note above): the
> `AMENITY_PROVIDES_POWER` refcount is untouched by a blackout — two power plants are two live grantors
> throughout an outage — while the city delivers nothing. ⛔ So a status is NEVER folded into the store it
> gates, never given a grantor's `±1`, and never becomes a cascade input.
> ⚑ **This GENERALIZES an instance the specs already carried:** a city under WLTKD or disorder emits **0 instead
> of its maintenance package** ([economy.md](../reference/economy.md) — *"the package is sent out to the rest of
> the cascade only if no status negates it"*, suppressing the CONSUMPTION of the value and never its contents).
> That was written as a maintenance quirk; it is this rule, and the rule is what binds.
> ⚖ **AND THE GATED VALUE THEREFORE EARNS AN EXPLICIT GETTER** — *"power having an explicit getter that
> blackout as a status can tap into makes sense here"* — the one qualification to
> [patterns.md](../architecture/patterns.md)'s one-getter-per-group grammar: **a gate needs a named point to tap
> into**, which a channel-indexed group read does not offer. ⛔ That is not licence to grow the per-channel
> getter surface back (what I don't want is to have the getter spaghetti we used to have) — the test
> is whether a getter carries a CONCEPT something else attaches to, not whether a caller wants one value.
> ⚑ The shape it takes: an **UNGATED** read (the source's own answer, `CvCity::hasPowerSource`) and the **GATED**
> read every consumer uses (`CvCity::isPowered`), the second composing the first with the status.
> ⛔ **The gated read is then the ONE definition, and the CROSSING that is announced is ITS crossing, never the
> store's.** The two genuinely differ — a grantor arriving mid-blackout moves the store and delivers nothing; the
> blackout lifting delivers power while the store stands still — so announcing the store would put the fact and
> every consumer's read on two different values, leaving [the maintained sum](../cascade/05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed)'s
> plane C holding deposits nothing withdraws. The fold owns that announcement
> ([contexts.md](../cascade.md)), and the status crossing reaches the fold for that reason alone.

> **⚖ THE CROSSING IS ANNOUNCED, AND THE FACT IS GENERIC OVER THE STATUS.** `CvCity::setStatus` is the ONE write
> path — the per-turn tick and the LOAD both come through it — and it emits `SEVT_CITY_STATUS_ADDED` /
> `_REMOVED` at the 0-crossing only, carrying WHICH status in `iType`.
> ⚠ **The load therefore LANDS through it, never straight into the array.** The store deserializes wholesale, so
> a status written directly into the slot announces nothing and every consumer gating on it reads a holder that
> is not held — the same hole the plot substrate had. ⛔ That id is not the discriminator
> [a fact names the happening](../spine/03-the-domain-emit-surface-every-fact.md#-a-fact-names-the-happening--something-changed-is-not-a-fact) bans: it names which
> member of an OPEN registry moved, exactly as a religion or property id does, and the direction is in the event
> name. A fact per status would mean an engine change per authored status — the very thing the open registry and
> the no-named-accessor rule exist to avoid.

## Open
- **Identify the faked states** — catalogue everything currently implemented as a pseudo-promotion or a Python
  event handler that is really a transient state, and list each here.
- The **`state` data shape** — the formal model (timer / trigger / effect / expiry).

## See also
- [json.md](json.md) §8 — the system. · [skills.md](skills.md) · [tags.md](tags.md) · [capabilities.md](capabilities.md).
