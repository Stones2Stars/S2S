# Triggers & grants — the provisions machine

> The cascade's **provisions** consumer: an `IEventConsumer` on the [event spine](../spine.md) that, on a
> `DOMAIN` state change, resolves the source entity's payload off its info and APPLIES it. The AUTHORING shapes are
> [json.md §5](json.md); this doc is the **machine** that consumes them.
>
> **⚖ TRIGGER IS THE TOP-LEVEL CONCEPT — A GRANT IS A TRIGGER WITH A NULL CONDITION.** One plane, one
> engine, one spine domain. `grants` stays a first-class AUTHORING shape (the overwhelmingly common "acquiring me
> gives this"), but nothing about it needs its own machinery.

## ONE compiled plane — how `grants` and `triggers` meet

Both authoring shapes compile into the **same entry list** (`CvTriggers` on the info); there is no separate grants
section and no `m_grants` member anywhere. A `grants` block becomes ONE entry with `consideredAction = true`, no
condition and no roll — the degenerate trigger — and its payload lives in that entry exactly as an explicit
`action.grant` payload does. The considered-action entry is an O(1) read (its index captured at parse, never
searched for).

⚑ The implicit happening is a compiled FLAG, not an `on*` token: it is never authored, so there is no token to
collide with, and the dispatching event already names which considered action it is — a building's construction, a
tech's research, a civic's adoption.

⛔ **The ODDS are data; the ROLL is not.** A trigger authors a `chance` — a plain number — and the engine compares
its own draw against it. That draw comes off the SYNCHRONIZED stream, which is shared save state, so no JSON
authors a seed, a stream or a draw and neither the cascade nor the curator models one
([the synchronized RNG is shared state](../reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)).

## ⚖ THE SCOPE RULE — what the machine is for

**The machine's job is to UNIFY ALL THE RANDOM PLACES THAT ADD RANDOM THINGS TO THE MAP, OUTSIDE "NORMAL
CREATION"** — construct / research / train / adopt / purchase.

That is the membership test, and it is deliberately broad: the qualifier is *how the thing arrived*, not which
info class declared it. If something appears in the world and the player did not build, research, train, adopt or
buy it, the machine owns it.

- **IN** — vote awards (including a whole city), NPC/barbarian spawners, combat loot / pillage / capture units,
  plot bonus discovery, leader-level-up traits, the trait city-founding provisions (`cityStartCulture`,
  `bonusPopulationinNewCities`), `barbarianInitialDefenders`, the start-era `freePopulation` / `FreeStartEra`
  buildings. **Being authored outside a `grants` block is not an exemption** — the scope test is how the thing
  arrived, not which block holds it; a provision homed elsewhere is a HOMING question, not a scope one.
- **OUT — normal creation.** A built building, a trained unit, a researched tech, an adopted civic, an
  advanced-start *purchase*. (The advanced-start *budget* is granted; what it buys is normal creation.)
- **OUT — not an arrival.** A transfer or transformation of something already owned: `CvUnit::convert`
  (upgrade/merge promotion carry-over), unit merge/split, scrap refunds, production→gold overflow, negotiated
  deal/gift transfers, improvement upgrades.
- **OUT — modifiers.** Anything alive only while its source is never "arrived" independently; it is the source's
  ongoing effect, and it belongs to [the cascade](../cascade.md).

⛔ **OUT OF SCOPE — Python-driven subsystems.** Goody huts, random events and espionage are Python-based and stay
OUT of the machine. They have C++ apply helpers (`receiveGoody`, `applyEvent`, `doEspionageMission`) — do NOT read
those as gaps to close: the C++ is the hand-over mechanism for a Python-driven system, and re-flagging them as
"unmigrated grants" is a rediscovery loop. They move only if the Python boundary moves.

⛔ **OUTCOMES ARE NOT GRANTS.** The `outcomes.kill[]`/`actions[]` reward payloads are their OWN system, already set
up separately: the `CvOutcome`/`CvOutcomeMission`/`CvOutcomeList` classes and their `execute()`/dispatch are
UNCHANGED, fed from JSON via `mapFrom` ([mission-outcome-system.md](../reference/mission-outcome-system.md)).
`CvOutcome::execute` is that system's apply, NOT an unmigrated grant site — do not fold it into this machine.

⛔ **THE MACHINE REPLACES LEGACY — IT NEVER WIDENS IT.** The goal is a FULL replacement and a **vast reduction of
endpoints**: many scattered apply sites collapse into one. So a change that *widens a legacy apply path to
accommodate the machine* is backwards by construction — it grows the endpoint count in service of a machine whose
whole purpose is to shrink it. Widening a condemned legacy member is the transitional shim
[build the proper structure once](../../AGENTS.md#design) bans.

## The classification that decides ownership

Three distinctions, each of which has been got wrong at least once:

- **GRANT** — handed over once, then persists with no living source. The machine's business.
- **MODIFIER** — alive only while its source is; refcounted or toggled with the source's presence. NOT the
  machine's business ([the cascade](../cascade.md) — the `freeSpecialists` precedent).
- **READ** — a consumer of the same info data for pedia/AI/UI. Not an apply, but **must keep working when an apply
  moves**.

And, orthogonally:

- **LEGACY-THAT-STOPPED** — a legacy behaviour whose apply is dead today (usually a stubbed poco getter). Needs a
  ruling: was the drop intended?
- **NEW-DESIGN-NOT-YET-BUILT** — authored data for a mechanic that never had an apply. Not a regression.
  `grants.foundBuildings` (settlers seeding buildings at settle time) is this: a **new mechanic coined for this
  rework**, not a port of the legacy `bNewCityFree`.

⚖ **Reuse the engine's own partition — do not re-derive one.** `CvPlayer::applyEvent` takes
`iEventTriggeredId == -1` to mean "replay the MODIFIER effects only" (`adjustModifiersOnly`), and every one-shot
handout in the event surface is already guarded by `!adjustModifiersOnly`. That flag is an existing, load-bearing,
hand-audited grant-vs-modifier split over the whole event surface. Adopt it.

**MODIFIER, not grant** — building `getFreeTraitTypes` ("conferred while active"); vote
`tradeRoutes`/`isFreeTrade`/`isNoNukes`/`forceCivic` (reversed on repeal); vote-source religion yields;
building/civic/trait `freeSpecialists`.

⚖ **THE FREE BUILDING IS A GRANT — in all scenarios these behave like grants.** A building naming another (or
itself) hands that building over, and the receiving city genuinely HAS it: the authored data gates on holding
these targets in over a thousand `requires` atoms, so a shape that delivered only the EFFECTS would silently break
every one of them. It authors `grants.buildings` on the SOURCE, landing in every city of the empire.
⚠ **This is a behaviour change from legacy, stated rather than hidden** ([validation.md](validation.md)): the
legacy pair was refcounted ±1 with the source's presence and REMOVED the copies when the source went. A grant
persists — losing the wonder keeps the granted buildings.
⚖ **The population SPLITS on empire-uniformity, and only the varying half stays a grant.** A building granting
ITSELF into every city, and a `notConstructible` marker whose only arrival is an empire-wide grant, are
`identity.empireLevel` buildings the PLAYER holds once ([enabler.md §2](enabler.md)) — no fan, no fold, nothing to
transfer on capture. What remains is the wonder granting an ordinary constructible building to every city (a
Granary, Irrigation Canals): real per-city copies whose presence genuinely varies.
⛔ **For that surviving population THE APPLY HAS TWO LEGS, and the second is the one a fan-at-construction
misses: "AFTERWARDS".** A city FOUNDED or ACQUIRED later must receive the copies for every source its owner
already holds, so the grantor fact fanning over the cities that already stand is only half of it — the other half
fires when a CITY STARTS EXISTING and folds what the owner holds. ⚠ A one-shot fan passes every test on the
cities standing at the time and silently misses every future one.

## ⛔ The traps, and the cases that only LOOK like defects

- ⚠ **Do NOT confuse the two `getNumUnitFullHeal`s — they are DIFFERENT RECEIVERS with different fates.** The
  **city-side** `CvCity::getNumUnitFullHeal()` is the applier and STAYS (its accumulator was cut; the mechanic was
  not). The **info-side** read is GONE from the rebuilt info: the effect — the herbalist shape, "set N units to
  100% HP" — is authored as a `triggers` entry ([json.md §5](json.md): a recurring handout is a trigger, never a
  grant), so its payload lives on the compiled entry (`healFull` + `healCount`) and a consumer that SCORES or
  DISPLAYS it reads `CvInfo::hasTriggerFullHeal()`.
  ⚑ **It touches no cache and no cascade** — setting units to full HP moves no deposit and no derived value, so the
  applier needs no mark and the read needs no eval context. ⛔ Do not wire an invalidation for it.
- ⚠ **`m_aPropertySpawns`'s serialization survives as an inert stream drain**, and it cannot simply be dropped.
  Its shape is a count tag named `iNumElts` — **shared by 23 variable-length blocks in `CvCity::read`** — followed
  by N raw untagged records, so naming it in `savemigration.txt` would drain the WRONG block, and dropping the
  write would orphan a tag old saves carry (leaving `iNumElts` holding the previous block's value and reading
  garbage). It retires when those blocks get per-block tags ([save.md](save.md)).
- ⚖ **The religion founder grant — two things that LOOK like defects and are BY DESIGN.** Under
  `GAMEOPTION_RELIGION_DIVINE_PROPHETS`, `foundReligion` early-returns because **founding a religion is an
  OUTCOME** under that option — the outcome system's job, which is a separate system. Nothing is missing. And the
  `eSlotReligion` / `eReligion` split is deliberate: the **SLOT** is what is being claimed (it drives
  `isReligionSlotTaken` / `getTechPrereq`) and therefore sets the free-unit **COUNT**; the **CHOSEN** religion sets
  the free-unit **TYPE**. Slot = reward size, chosen religion = its flavour.
  ⚑ To MOVE this grant the emit must carry what the apply needs: the chosen religion, the slot religion, the award
  flag and the target city — a religion-founded fact carrying only player + religion is not enough.
- ⚠ **A separate ARRIVAL mechanism feeds the same targets and is NOT this machine:** the buildings and heritages
  handed over by animals or entertainers come from the unit's CONSTRUCT MISSION — its own repertoire, not a grant.
- ⚑ **`bFirst` is deliberately NOT a second event.** It does not say how the building arrived, so it names no
  happening; folding it in would mint a fact for a flag.

⚠ **THE THREE LEGACY PER-TURN LEDGERS ARE NOT ONE CLASS — the replacement OWNER differs**, which is the
classification above applied to the very sites that look most alike. All three are Σ over the city's buildings of
a static info field (the stored-accumulator drift class, cut via the `savemigration.txt` soft-remove), but they do
not all land in the same machine:

| ledger | mechanic | replaced by |
|---|---|---|
| `m_aPropertySpawns` | per-turn unit spawn — an arrival outside normal creation | **this machine** |
| `m_iNumUnitFullHeal` | discrete per-turn action (fully heals up to N damaged units) — [json.md §5](json.md) names a heal as a `repeatable` payload | **this machine** |
| `m_paiHealUnitCombatTypeVolume` | **continuous heal-RATE contribution**, not a discrete event | **the MODIFIER heal channel** — alive-with-source ⇒ modifier, the `freeSpecialists` precedent |

## ⚖ THE PALACE: two triggers, not one

The capital building is placed by **two different events**, and covering only one leaves an empire with no capital:

| event | who places it |
|---|---|
| a city is FOUNDED and the empire has no palace | the settler's `grants.foundBuildings`, gated `{BUILDING_PALACE, empire, max:0}` |
| a capital is CAPTURED and the capital relocates | **OWNER CHANGE must handle it** — not the founding gate |

**The gate is "the empire has NO PALACE", never a city count.** A `{CITY, empire, max:0}` proxy is wrong twice: it
can never hold at founding (the grant applies after `initCity` has registered the new city, so the count is
already 1 — verified live: the Palace was silently never seeded and games started with no capital), and it refuses
the case that matters — losing your capital while other cities stand should re-seed one. The building gating on
its OWN absence is correct at both triggers and needs no off-by-one reasoning.

⛔ **The civ-grant palace was NOT redundant.** `BUILDING_PALACE` sat in ~48 civilizations' `grants.buildings` and
was dropped as a duplicate of the settler's `foundBuildings`. It was not: `foundBuildings` covers FOUNDING, while
`CvPlayer::findNewCapital` / `setCapitalCity` place the **civilization building list** into the newly-chosen
capital — that list was the only thing that moved a palace on RELOCATION. Dropping it removed the mechanism.

## ⛔ A GRANTED ENTITY IS AN ORDINARY ENTITY

The ONLY difference between a building granted and one constructed is that a grant spent no production.
So the machine gets **no** parallel apply path, no "granted" flag, no distinct lifecycle, no ledger of
its own: it places the entity through the **SAME creation mechanism** as normal creation, and the ONLY divergence
is that the production/cost step is skipped. Settled by this, not open:

- **A grant fires the ordinary DOMAIN events** — *"like anything else"* — so the enabler, the modifier packages and
  the tally see a granted building exactly as they see a constructed one. The machine FEEDS the spine; it never
  bypasses it ([spine.md](../spine.md): the spine is the SINGLE place a state change is announced).
- **A granted building runs its own first-build block**, because a construct would. The resulting grant→event→grant
  chain is intended behaviour, not re-entrancy to guard against.
- **Nothing downstream may branch on "was this granted?"** — there is no such state to read.

> **⛔ AND IT BINDS UNITS EXACTLY AS IT BINDS BUILDINGS — GRANT AND TRAIN ARE ONE PATH: *"creating a unit
> is creating a unit; how we got to the creation step does not matter."*** The route by which a unit was decided
> upon — a production order, a trigger payload, a first-discoverer award, a founder package — ends at the SAME
> creation step, and everything that step owes a new unit is owed identically. ⛔ So a payload applier does NOT
> hand-assemble its own creation sequence beside the trained one; the divergence is the production debit and
> nothing else.
> ⚑ **The tell that the rule is being broken is a DIVERGENT SIDE-EFFECT SET, not a divergent call**: the same
> `initUnit` reached from several places, each remembering a different subset of what creation owes — free XP
> here, free promotions there, the AI type somewhere else. That is the parallel apply path this section bans,
> merely spelled as duplication rather than as a flag.
> ⚑ **THE ONE STEP IS `CvPlayer::createUnit(eUnit, iX, iY, eUnitAI, bConscript)`** — it brings the unit into
> existence and settles what creation owes it; a CITY standing at that location settles its own share (the free
> experience it gives units born in it), and a unit born in the field has nobody to owe it. Every route ends
> there: the production order, the conscription, each trigger payload, the outcome spawn, the event handout, the
> founding and culture-flip defenders, and the great-person birth, whose CEREMONY sits on top of it.
> ⛔ A payload applier calls it and adds only what is genuinely its own — the excile's jump, a spent movement
> allowance — never a second creation sequence.
> ⚖ **IT IS THE PLAYER'S, NOT THE CITY'S, AND THE TELL IS WORTH KEEPING.** The player OWNS units, and
> not every creation has a city — so a city-side step forces every caller holding only COORDINATES to resolve a
> city first, and the moment two of them did, a second entry point was about to be minted beside it. **Two entry
> points for one concept is the signal that the step is on the wrong object.** On the player it is one total
> signature, and the hostile property spawn needs no owner argument at all: the receiver IS the owner, and the
> city it surfaces in still settles its share.
> ⚠ **MOVEMENT is deliberately NOT settled there**, and that is a distinction to keep: the engine has two
> standing answers (a trained unit is spent, a conscript is ready to act), so the caller states which it means
> rather than inheriting one by accident.
> ⚑ **WHICH UNITS ARE OWED THE CITY'S EXPERIENCE IS DATA, NOT A CARVE-OUT HERE.** `addProductionExperience`
> gates on `canAcquirePromotionAny()`, and a unit the data declares `unpromotable` ([tags.md](tags.md)) answers
> false — so a great person born in a city takes none, without the step carrying an exception for it. ⛔ Do not
> re-add one: if some unit is taking experience it should not, the defect is its TAG, not this step.
> ⚠ **Measured, and the reason the step exists:** three creation shapes had disagreed — the property spawn called
> `addProductionExperience`, the religion founder's free units did not, and the first-discoverer leg went through
> `createGreatPeople`. A unit's starting experience therefore depended on which payload created it, which is
> exactly the "downstream can tell it was granted" state the ruling says must not exist.
> ⛔ **A SCHEDULED REWORK IS NOT A REASON TO STAY OFF THE STEP.** The outcome system's ground-up rework,
> and the events carve-out, bound what may be REDESIGNED and what may be folded into this machine — neither
> licenses a system keeping its own creation shape meanwhile. Re-pointing a creation call is not a redesign.
> **⛔ A SECOND WAY TO CREATE A UNIT IS A ROLLERSKATING SURFACE — AND THE MODDER-FACING ONE MOST OF ALL
>.** The more unified `createUnit` is, the better: a single place that can create a unit by some other route
> is a rollerskating surface, and a modder-facing one most of all. ⇒ **The EDITOR goes through it
> too** — `CyPlayer::createUnit` creates through the step, which is why it carries a FACING DIRECTION parameter;
> WorldBuilder is exactly where an alternate path would teach the wrong lesson, and WorldBuilder is already
> required to travel the engine's own paths — every WB mutation emits like any other ([spine.md](../spine.md)).
> ⚑ **"Unified" means STANDARDIZED PATHS, not merely few of them** — the point is that a reader looking
> for how a unit comes into being finds ONE answer and cannot invent a second.
> ⛔ So a scope boundary is never a reason to keep a creation call off the step: an ARRIVAL created anywhere —
> a combat CAPTURE, an espionage BRIBE, an advanced-start placement, a field spawn — goes through it.
>
> ⚖ **AND THE TRANSFORMATIONS GET THEIR OWN TWIN — `modifyUnit`, in the same vein.** A unit that
> ALREADY EXISTS changing type, owner or count is not a creation: upgrade · gift · trade · merge / split (and
> their `CvMessageData` net twins) · `assimilatePlayer` · the map transfer. Each stands a successor up in place
> of a predecessor, carries its state across (`CvUnit::convert`) and retires the source.
> ⛔ **They must NOT ride `createUnit`, and the reason is concrete rather than taxonomic:** the step settles what
> a city owes a unit BORN there, so routing an upgrade through it would hand out the city's free experience on
> every upgrade, merge and gift — a barracks city turning transformation into an XP faucet.
> ⚑ Until `modifyUnit` lands, those sites are the ONLY legitimate direct callers of `initUnit`, which is what
> keeps the rule checkable: **`initUnit` should be reachable from the step and from a transformation, nothing
> else.** ⚠ Two live non-creations sit outside both by construction and are not gaps — `CvPlayer::getTempUnit`
> (`m_pTempUnit`, the off-map pathing anchor, excluded from `units()` iteration and from every death sweep) and
> a `(UnitTypes)0` probe at the origin in `CvGame`'s slot-takeover path, which passes birthmark `0` and so
> consumes no draw; converting that one would ADD a draw to the synchronized stream
> ([the synchronized RNG is shared state](../reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)).

## ⛔ THE MACHINE REPLACES THE PER-TURN WORK — and the spine is its ONLY way in

It is not a resolver running beside legacy: the per-turn work MOVES onto the machine, and the legacy call sites are
DELETED, not re-pointed. Their ledgers become derived and are cut by
[the uniform legacy-accumulator cut](../cascade/03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism) via the `savemigration.txt`
soft-remove ([save.md §3](save.md)) — never a `@SAVEBREAK`.

⛔ **The per-turn apply arrives as a spine EVENT, never a direct call from `doTurn`.** The machine is an
`IEventConsumer` and that is its only front door; a machine taking events through `onEvent` *and* per-turn work
through a bespoke entry point has two front doors, which is the scattered-endpoint disease it exists to cure. The
player-scoped turn event is the natural grain — legacy ran the per-turn work inside the city loop within the
player's turn, so consuming the player boundary and walking that player's cities preserves the ordering.

**Perf constraint:** it must NOT re-create a per-city list of pending provisions (that list is the ledger being
deleted). It gates on the enabler's already-maintained **operating-building set**
([enabler.md §3.2](enabler.md)) — required for correctness anyway, since a dormant building must grant nothing.

⛔ **It never reads the legacy collapse members**, which LOSE `interval`/`enabled`/`chance` at map time. It reads
the composed entries, which carry the full structure. Never widen a legacy member to carry the missing fields.

## Registration order — the machine registers LAST

After the contexts, the enabler and the modifier: it READS the contexts (every entry condition evaluates through
the fill seams) and the enabler's operating set (a dormant building grants nothing), and unlike those machines it
**APPLIES** — so a stale read hands out a wrong GRANT, not merely a wrong number.

⚠ Every eval context it builds must be filled through the ONE seam AND wired with the enabler's operating set. The
enabler's precomputed sets are the THIRD LEG of the eval state, fed in rather than re-derived; without them the
operating-set legs sit EMPTY and any condition asking an active-building or vicinity-provides question evaluates
against nothing and quietly answers false.

> **⚖ A THRESHOLD ON A HAPPENING IS A TRIGGER, NOT A MAGNITUDE — the `techShare` worked case.** Tech
> sharing reads as a diplomacy number and is not one: *"if 2 civs have the tech, then whoever has the wonder also
> gets the tech."* The `2` is not an amount anything accumulates — it is the **fire condition of an
> `onTechResearched` trigger** whose action grants the tech. So it authors on this plane
> (`trigger` → `chance` → `action`), never as a `diplomacy.<scope>.techShare` deposit, and it carries **no kind**
> — the same standing as the rest of the trigger-plane set (`survivor`, `cityCapture`, `combat.subdueAnimal`,
> `combat.nukeInterception`), which `CvInfoKinds.h` keeps deliberately unkinded pending this re-home.
> ⚑ **The TELL that generalizes** — and it is worth applying to every remaining family member: a number whose
> value is a **COUNT OF OTHER PARTIES / a threshold that decides WHETHER something happens** is a trigger
> condition; a number that is **added, scaled or stacked** is a modifier. `techShare: 2` never combines with
> another `techShare`, which is the giveaway that no channel was ever involved.
> **The condition form, settled:** `{type: TECH_X, scope: world, min: N}` — the world tech count
> ([tally.md](tally.md)), evaluated on the tech-acquired happening, with the action granting that tech.
> ⚖ **The legacy `isHasMet` filter is DROPPED:** `CvTeam::updateTechShare` counted only teams you had
> MET, and the world count does not — tech-sharing happens late enough that all players have normally been met
> by then. An intentional divergence, stated rather than reproduced
> ([validation.md](validation.md): the spec leads).
> ⚑ **And it is RE-ADDABLE, not lost** — the met filter goes back in after the fact if it is wanted. It
> comes back as an ordinary CONDITION on the entry — a met predicate — which *extends* the vocabulary rather than
> reshaping anything ([conditions are predicates, never bespoke members](json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query):
> the predicate registry is extensible by design). ⛔ So do NOT preserve the legacy filter now "to keep the option
> open" — the option is open by construction, and keeping it is the half-migration.
> ⚠ Two residues of that swap, both harmless but worth knowing rather than rediscovering: the world count is over
> **EVER-alive** teams, so a dead civ's tech still counts toward the threshold; and it does not exclude the asking
> team, which is inert here because the trigger only fires for a tech you do NOT hold.
>
> ⛔ Its consumer stays DANGLING until the re-home lands — do not restore it by minting or keeping a kind.

## ⛔ Purely-Python, never-XML effects are out of scope for this data model — but out of scope is not their destination

⚖ **They move to C++ after the rework, and most of them are triggers: *"all these scripts is something we
will port to C++ after rework is done; having scripts like this in Python is root of all evil"* — *"most of it
can even be expressed as triggers."*** The gameplay scripts in `CvEventManager` and the contrib mods (the
per-wonder combat and turn effects, the combat-promotion mod, the respawn and revive handouts) are
**trigger-SHAPED by construction**: a happening, a chance, an action (this doc) — which is the same plane
`CvEventReporter`'s successor already lands on ([patterns.md](../architecture/patterns.md): events move INTO
C++, but that is a separate, later effort).

A KEEP-WORKING repair on one of these — re-pointing a handler onto the id surface — holds a mechanic alive until
its trigger exists; it is never an investment in the Python expression of it. Do the minimum that makes it work,
and do not improve, restructure or extend the script while in there.

⚠ **Do not start the port opportunistically.** Authoring one of these as a trigger needs its happening and its
verb to exist, and minting either speculatively for one mechanic is banned outright (§ What the plane must NOT
do, below). This data model migrates the **XML-dealt-with surface** (XML data + the engine machinery that reads
it). Gameplay living ONLY in Python that reads NO XML field is a separate surface this model never touches — the
hardcoded per-turn wonder spawns/grants in `CvEventManager`, and effects that are structurally INEXPRESSIBLE in
the model (a culture burst when a unit dies has no home in the `grants`/modifier/enabler vocabulary, which
declares provisions on standard triggers, not arbitrary event reactions). ⚑ Their absence from any migration
inventory is the scope boundary working, not a gap — and there is **no exposure by construction**: the grants
machine applies only what is in the XML-derived JSON, so an effect that was never in XML never enters the JSON
and can never double-up or be lost.

## What the plane must NOT do

- ⛔ **The per-turn applier must NOT apply property pulses.** Their carriers bridge theirs at load; applying them
  again would double the value AND land it outside the solver's ordered pass, where spread resolves against
  PRE-source values ([engine.md](../reference/engine.md)) — and that engine's math is owner-LOCKED.
- ⛔ **Do not build machinery for a hypothetical verb.** A verb with zero authorings is an EXAMPLE in the spec, not
  live data; it lands if and when its authoring direction is taken.
  ⚑ **The worked case — building counter-damage: IF WE WANT IT, IT IS A TRIGGER; UNTIL THEN IT IS
  NOTHING.** A trap building damaging a unit that attacks its city is a trigger by shape — a happening, a roll,
  an effect on the attacker. Modelling it needs an **`onAttacked`** happening and a **`damage`** verb, neither of
  which exists. The ruling takes neither of the two tempting shortcuts: the verbs are NOT minted speculatively for
  one mechanic, and the legacy member is NOT kept alive in the meantime — **the data goes out and the mechanic is
  authored fresh when the rework is taken.** ⚠ So being trigger-SHAPED is not on its own a reason to re-home
  something, and it is equally not a reason to preserve the old shape while waiting: a member parked on the
  `defense` family is a half-migration that reads as done. *(The UNIT-side trap subsystem is separately dead —
  [skills.md](skills.md).)*
- ⚠ **A promotion that stops being valid is dropped by the PROMOTION SYSTEM itself**. So a granted
  promotion needs no take-away verb, and "the payload plane cannot revoke" is NOT an argument for re-homing the
  free-promotion shapes.

## ⛔ A DROPPED TRIGGER ANNOUNCES — every skip goes through the ONE census

**If a trigger fails to parse or to land, say so**. The plane is fail-closed in several places — the bridge
refuses a source it cannot faithfully translate rather than applying it under a wrong condition, and the parser
refuses malformed input — and being fail-closed is right. Being fail-closed *and silent* is not: authored data that
loads, never applies, and reports nothing is invisible on both axes at once.

Every drop routes through the ONE load-time census
([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)) — the same mechanism the
parser already uses for unknown verbs and keys, surfacing on readJson's coverage counts. ⛔ Do NOT add a second
reporting path or a bespoke spine domain for this.

⚑ The case that was genuinely invisible: a conditioned pulse whose condition falls outside the bridge's known
predicate set, dropped with nothing said. What the census reports is a real question to ASK of a loaded game.

---

## Game-start provisions — START PACKAGES

The game-start sequence (what a player begins the game holding) is a set of JSON-authored **start packages** the
machine applies, replacing the hardcoded engine selection. Owner intent: *"this gives modders a chance to set up
how they want."*

**The problem it solves.** Today the start is split between data and hardcoded engine logic, and the engine half is
the problem: the COUNTS are curated, but the **unit identity is not authored anywhere** — it is picked at runtime by
scanning the whole unit database, filtering on trainability and scoring with an AI valuation — and the **settler is
not in data at all**. A modder therefore cannot say what a start looks like; they can only nudge counts and hope the
scorer picks the unit they meant.

A start package is an ordinary entity — one JSON object per file — essentially a named `grants` block plus the
condition that decides when it applies:

```jsonc
{
  "type": "STARTPACKAGE_ANCIENT_DEFAULT",
  "enabled": { "type": "ERA", "max": 1 },          // the ordinary entity-level gate
  "grants": {
    "units": [ { "unit": "UNIT_SETTLER", "count": 1 },
               { "unit": "UNIT_BRUTE",   "count": 2 } ],
    "startingGold": 40
  }
}
```

- **Reuses `grants` wholesale** — no parallel vocabulary ([json.md §5](json.md)).
- **The settler stops being hardcoded** — it is just a `units` entry.
- The per-role starting counts are superseded by explicit unit entries.

> **⚖ THE POINT: author the shared start ONCE, not per civilization.** `grants` can already express a start,
> but putting it on the CIVILIZATION means repeating the same block with the same conditionals across ~50 civ files
> that all start identically — *"kinda dumb, when it's the same package for all of them."* A package inverts that:
> **the condition lives ON THE PACKAGE, evaluated once**, and every civ it applies to gets it without authoring
> anything. A civilization authors something only when it DEVIATES, and that deviation is its own package stacking
> on top of the shared default.

**Packages STACK.** Applicable packages sum, exactly like any other grants deposit — they are not mutually
exclusive alternatives. Stacking SUBSUMES single-selection, so the modder chooses the granularity: one coherent
package, or era + handicap + civilization composing. Single-selection could not express the second, and the engine
already adds era + handicap counts today, so stacking is the behaviour-preserving choice as well as the flexible one.

⛔ **"Conditionally loaded" means the ENTITY GATE, never a load-time prune.** The applicability condition is the
entity-level `enabled`/`disabled` pair evaluated LIVE ([the whole-entity applicability gate](json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)).
Do NOT build a "load these files, skip those" prune — that is the killed `loadPrune`
([superseded-ideas](../architecture/superseded-ideas.md) #3). Every package loads; the gate decides which APPLY.

**What a new entity type requires** (skipping any of these is how an entity ends up half-wired): a folder under
`Assets/Data/`, one object per file · the `STARTPACKAGE_` infotype prefix registered in [naming.md](naming.md) · a
row in the ONE per-type repo dispatch (which earns it the full-registry re-map, the DepositIndex push, and an
info-repo home) · an `_order.json` manifest · and authoring through `_additions`, since entity curation is
complete and there is no legacy XML to convert — the unit identities never existed as data.

## Free promotions — TWO SOURCES, one relation

A city's OPERATING BUILDINGS and the unit owner's HELD TRAITS both arm units, and they author the identical
shape, so the plane carries no trait-specific payload — only a different owner and a different leg (2).

|  | the source is | leg (1) unit arrives | leg (2) source arrives |
|---|---|---|---|
| **building** | the CITY's, active per the operating set | walk the city's active buildings | `SEVT_CITY_BUILDING_ACTIVATED` — that city's units |
| **trait** | the UNIT OWNER's, held per the empire | walk the owner's held traits | `SEVT_EMPIRE_TRAIT_ADDED` — that empire's units, over all its cities |

⚠ **The owner axis differs and the difference is load-bearing:** a same-team ally's unit standing in your city
is armed by your BUILDING (the city arms whoever it shelters) and by its OWN empire's traits — never by yours.
So the building leg filters on TEAM and the trait leg on the unit's OWNER; using one filter for both silently
hands your traits to a teammate.

⛔ **Do NOT answer a dangling trait promotion by restoring a trait-side promotion × unitcombat map** — that is
the legacy mechanism whose data moved, and it swept the whole trait registry per promotion to do it. The trait
legs of `CvUnit::setFreePromotion` are gone deliberately.

⚖ **THE LEGACY FUNCTION SPLIT THREE WAYS, AND ONLY TWO LEGS ARE THIS MACHINE'S.** `CvUnit::setFreePromotion`
folded three sources with different LIFETIMES, so it must never be moved wholesale:

| leg | lifetime | owner |
|---|---|---|
| the unit info's own free promotions — fed from **`grants.promotions`** | set at creation, never removed | **GRANT — this machine** |
| the player free-promotion registry, keyed by unit type AND unitcombat | written **ONLY** by `CvPlayer::applyEvent` | **OUT OF SCOPE — random events** (genuine one-shot event-store state) |
| trait `isFreePromotionUnitCombats` | removed when the trait is lost | **TRIGGER/GRANT — this machine** |

Moving the whole function would import an out-of-scope event store into the machine — the exact classification
mistake above. The unit-info and TRAIT legs migrate; the event-registry leg does not.

⚖ **FREE PROMOTIONS LIVE ON THIS PLANE — EVERY LEG, INCLUDING THE TRAIT'S.** The alive-with-source lifetime does
NOT re-home this one to the modifier plane: a free promotion is a PAYLOAD handed to units, not a magnitude
deposited into a channel, and there is no `freePromotions` modifier family for it to land in.
⛔ So the removing-the-source-removes-it test ([json.md §5](json.md)) does not decide the PLANE here; it decides
`grants`-vs-`freeSpecialists` for a SPECIALIST, and reading it as a general plane test is what put this row on the
modifier plane.

⚑ **A trait is EMPIRE-scoped while this happening is a CITY relation, and that is the authored model, not an
oversight:** a trait's promotions reach a unit that is in one of the empire's cities. In practice that is every
unit at birth (creation runs the same applier) and every unit that ever passes through one; a promotion once
handed over is permanent, so nothing has to hold the unit there. The re-fire is free — the applier skips a
promotion the unit already holds.

## See also
- [json.md §5](json.md) — the authoring shapes (`grants`, `triggers`).
- [spine.md](../spine.md) — the `IEventConsumer` front door and the DOMAIN facts this dispatches on.
- [enabler.md §3.2](enabler.md) — the operating-building set the per-turn apply gates on.
- [mission-outcome-system.md](../reference/mission-outcome-system.md) — the outcome system, which is NOT this
  machine and must not be folded into it.
