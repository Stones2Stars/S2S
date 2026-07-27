# The grants machine — in-DLL build plan ("provisions")

> The cascade's **provisions** consumer: an `IEventConsumer` on the event spine ([event-spine.md](../../specs/event-spine.md))
> that, on a `DOMAIN` state-change, resolves the **source entity's genuine grants** off the mapped `CvJsonInfo`
> (`grantLists`/`grantPulses` in the per-type `InfoRepo`) and — when the apply-loop lands — applies them; the apply-loop must
> MANIFEST in-game ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)). Design authority:
> [json.md](../../specs/json.md) §5 (`grants`). The **classification pass** (2026-07-01, [data-migration-remaining.md](data-migration-remaining.md))
> cleaned the surface: `grants` now holds **only genuine provisions handed out on a trigger** — the mis-homed keys
> moved out, and the unit activated-MISSION keys are a **PERMANENT carve-out (missions pass)** ([mission-outcome-system.md](../../reference/mission-outcome-system.md)).

> ⛔ **The inventory table below is SUPERSEDED as a map of the legacy surface — see
> [grant-apply-sites.md](grant-apply-sites.md).** Checked domain-by-domain against live code, it understated the
> apply surface in EVERY domain, carried drifted line numbers throughout, and cites at least one function that
> does not exist (`CvPlayer::setHolyCity` — the apply is in `foundReligion`). Whole grant-bearing subsystems have
> no row here at all: goody huts, random events, espionage missions, votes, outcome reward payloads, NPC spawners,
> combat loot. Keep this table as the machine's TRIGGER design; take the legacy sites from the map.

## The genuine grant inventory (post-classification) — trigger + legacy apply-site

| grant (holder) | trigger (→ DOMAIN event) | legacy apply-site | shape |
|---|---|---|---|
| unit `promotions` | unit created | `CvUnit::init`→`doSetFreePromotions` | one-shot on-create |
| unit `foundBuildings` | city founded | `CvPlayer::found` (NewCityFree) | one-shot on-settle |
| building `repeatable` (spawn/heal) | per-turn | `CvCity::doPropertyUnitSpawn` / `doUnitFullHeal` | recurring |
| building `freePromotions` | end-turn | `assignPromotionsFromBuildingChecked` | recurring |
| building `freeTechs` / `population` / `goldenAge` | first build | `CvCity` `bFirst` block (:14803/:14724/:14764) | one-shot |
| trait `freePromotions`(dict) / `goldenAgeOnBirthOfGreatPerson` / ~~`eraAdvanceFreeSpecialist`~~ | unit-init / GP-birth / ~~era-advance~~ | `CvPlayer` trait paths | recurring/pulse |

> **⚖ freeSpecialists reclassified OUT of grants (owner ruling 2026-07-02):** a free specialist is alive only as
> long as its source is (building/civic/trait active) — the continuous **modifier** shape
> ([modifier.md](../../specs/modifier.md) §Specialist counts), so the freeSpecialist-shaped entries (incl. the trait
> `eraAdvanceFreeSpecialist` row above) belong to the modifier plane's `freeSpecialists` family, not this machine.
> Pin the exact legacy lifetime semantics of the era-advance path against the modifier family when that channel is built.
| civic `revolution` | civic switch | RevolutionDCM **Python** (no DLL apply) | one-shot pulse |
| civ `civics` / `techs` / `buildings` | game start / first city | `CvPlayer` init / `CvCity::init` | game-start |
| tech `firstFreeUnit` / `firstFreeProphet` / `freeTechs` | first to discover | `CvTeam::setHasTech` (5452+) | one-shot on-discover |
| religion `numFreeUnits` / `freeUnit` | religion founding | `CvPlayer::setHolyCity` block | one-shot on-found |
| era / handicap `starting*` / `freePopulation` / `ai` | game start | `CvPlayer::initFreeUnits`/`initFreeState` | game-start |
| property `buildings` | property present | (#430 pending — auto-built) | continuous |
| feature/improvement property-pulses | per-turn | `CvPropertySolver::doTurn` (spatial, #429) | recurring |

## The machine — `CvTriggerEngine` (`Sources/Triggers/CvTriggerEngine.{h,cpp}`)

An `IEventConsumer` (`wantedKinds` = DOMAIN), registered at the composition root (`spineRegisterConsumers` →
`triggerRegisterConsumer`) — **LAST**, after the contexts / enabler / modifier, because it reads both machines'
output and, unlike them, APPLIES. `onEvent` dispatches by the DOMAIN event, resolves the source entity's payload
off `InfoRepo<CvXInfo>::get().get(id)` (the carved-out missions-pass keys simply not read), applies it, and emits
a `[TRIGGERS/*]` diagnostic via the spine (`SD_TRIGGERS`).

**It APPLIES** — the resolver-only phase is over: the first-build provisions, the tech first-discover set, the
religion founder units, the per-turn spawn/full-heal, the free promotions and the settle-time building seeds all
hand over here, and the legacy apply sites they replace are deleted. What is resolved but NOT yet applied is
listed as a gap in the coverage table below, never left implied.

## Build increments (each compiles before the next)

1. **Slice-1 ✅ DONE** — the consumer + the `[GRANTS]` domain + resolution over the DOMAIN events the spine emits
   **today** (building-built `CASCADE_EVT_BUILDING_COUNT` delta>0, unit-created `CASCADE_EVT_UNIT_COUNT` delta>0):
   building genuine grants (`repeatable`/`freePromotions`/`freeTechs`) + unit genuine grants (`promotions`/
   `foundBuildings`). Emits `[GRANTS/building]` / `[GRANTS/unit]` (gated). Assert green.
2. **The richer grant MAPPING**
   - **2a ✅ DONE** — `rj_walkGrants` now captures the **bool grants** (`goldenAge` → `CvJsonInfo::grantFlags`) and the
     **scoped-pulse dict grants** (`population {city|empire:N}` → `grantScopedPulses`, ×100); object-valued dicts (the
     carved-out missions-pass key `greatPersonAction`) are correctly skipped (no number leaves). The machine's `[GRANTS/building]`
     now surfaces `goldenAge` + `population` (pulses shown ÷100 = human count). No JSON regen — pure readJson enrichment.
   - **2b ✅ DONE** — the **`repeatable` STRUCTURE** is now captured: `rj_parseRepeatable` fills
     `CvJsonInfo::grantRepeatables` (a `CvCascadeGrantRepeatable` per entry) with the payload (unit spawn / unitCombat
     heal / PROPERTY_* pulse), `interval`, the `chance:{per}` scaler id, and the #429 spatial `on`/`relation`/`distance`.
     This fixed a real gap — the old generic id-only capture missed the **149 heal + 49 property** entries entirely (only
     the 26 `{unit}` spawns resolved). `[GRANTS/building]` `repeatable` now counts the full structured set. Pure readJson.
     *(Followed for consistency: `heal:"full"` + property-amount ×100 vs raw; the enabler/#429 consumers
     that ACT on these — the per-turn spawn/heal apply + the spatial property distribution — are increments 3/5.)*
3. **The remaining DOMAIN triggers** — the spine emitted only building/unit-count + name-change; the grant inventory
   needs more. Each is a new synced `DOMAIN` event emitted from a deterministic engine state-change site.
   - **3a ✅ DONE — tech first-discover** (`CASCADE_EVT_TECH_ACQUIRED`): emitted from `CvTeam::setHasTech` inside the
     `bFirst && countKnownTechNumTeams==1` block (the exact site where `firstFreeUnit`/`firstFreeProphet`/`freeTechs`
     fire), carrying the tech + discovering player. The machine resolves them → `[GRANTS/tech]`.
   - **3b ✅ (religion + civic done)** — `CASCADE_EVT_RELIGION_FOUNDED` (`CvPlayer` at `setHolyCity`, the founder-grant
     site → `numFreeUnits`/`freeUnit` → `[GRANTS/religion]`) and `CASCADE_EVT_CIVIC_ADOPTED` (`CvPlayer::setCivics`, a
     newly-adopted non-NPC civic → `revolution` pulse → `[GRANTS/civic]`).
   - **3c ✅ (game-start done)** — `CASCADE_EVT_PLAYER_INIT` (`CvPlayer::initFreeUnits`, where the player's civ/era/
     handicap are set): the machine resolves the whole game-start set at ONE trigger — civilization `civics`/`techs`/
     `buildings` + era/handicap `startingGold` → `[GRANTS/gameStart]` (the legacy apply is spread across init sites;
     the cascade resolves it in one place off `InfoRepo<CvCivilizationInfo>`/`<CvEraInfo>`/`<CvHandicapInfo>`).
   - **All the clean event triggers are now wired** (building/unit/tech/religion/civic/game-start). What remains is NOT
     a resolution trigger: city-founded is minor (`foundBuildings` already resolve on unit-created), and the **per-turn
     recurring** apply (`repeatable` spawn/heal + `freePromotions`) belongs to increment 5 (the apply-loop).
4. **Live manifestation check** — grants are event-driven side-effects; the acceptance is that the resolved grant-set is
   observable via the `[GRANTS]` diagnostic, and — once the apply-loop lands (increment 5) — the granted effect MANIFESTS
   in the running game per channel ([DEC-done-is-observable](../../architecture/decisions.md#dec-done-is-observable)); not a re-run of parity/shadow.
5. **Apply** — the machine APPLIES, replacing the legacy grant application.

   > **⚖ A GRANTED ENTITY IS AN ORDINARY ENTITY (owner ruling).** *"The only difference between a building
   > granted and a building constructed is that we didn't use production if granted."* So the machine does **NOT**
   > get a parallel apply path, a "granted" flag, a distinct lifecycle, or its own ledger: it places the entity
   > through the **SAME creation mechanism** as normal creation, and the **only** divergence is that the
   > production/cost step is skipped. Consequences that are settled by this, not open:
   > - **A grant fires the ordinary DOMAIN events** — `emitBuildingChanged` / unit-created, *"like anything else"*
   >   — so the enabler, the modifier packages and the tally see a granted building exactly as they see a
   >   constructed one. The machine feeds the spine; it never bypasses it
   >   ([event-spine.md](../../specs/event-spine.md): the spine is the SINGLE place a state change is announced).
   > - **A granted building runs its own first-build block** (`bFirst`), because a construct would. The resulting
   >   grant→event→grant chain is intended behaviour, not re-entrancy to guard against.
   > - **Nothing downstream may branch on "was this granted?"** — there is no such state to read.
   >
   > **⚖ THE MACHINE REPLACES THE EXISTING PER-TURN WORK (owner ruling)** — it is not a resolver running beside
   > legacy. The per-turn `repeatable` apply moves onto the machine: `CvCity::doPropertyUnitSpawn` (the spawn,
   > called from `CvCity::doTurn` `:1320`) and the full-heal branch of `CvCity::doHeal` (`:20202`). Their ledgers
   > (`m_aPropertySpawns`, `m_iNumUnitFullHeal`) become DERIVED and are cut by
   > [DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform) via the
   > `savemigration.txt` soft-remove ([save.md §3](../../specs/save.md)) — **not** a `@SAVEBREAK`.
   >
   > ⚠ **`m_paiHealUnitCombatTypeVolume` is NOT this machine's** — despite being authored as a `repeatable` and
   > sharing the same `processBuilding` maintenance, it realizes as a **continuous heal-RATE term** (consumed at
   > `CvUnit.cpp:6232`, a named source in the `/computed/units/heal` decomposition), i.e. alive-with-source ⇒ a
   > MODIFIER, the `freeSpecialists` precedent ([modifier.md](../../specs/modifier.md)). It is equally derived and
   > equally cut, but its replacement is the modifier plane's heal channel. Classification detail + the full site
   > map: [grant-apply-sites.md](grant-apply-sites.md) §5.3.
   >
   > **⚖ THE PER-TURN APPLY ARRIVES VIA THE SPINE — the machine is an `IEventConsumer`, and that is its ONLY way
   > in (owner ruling).** It consumes `SEVT_TURN_STARTED` ([event-spine.md](../../specs/event-spine.md), built for
   > exactly this) and runs that player's per-turn provisions from `onEvent`. ⛔ **NOT a direct call from
   > `CvCity::doTurn`:** a machine that takes events through `onEvent` *and* per-turn work through a bespoke entry
   > point has two front doors, which is the scattered-endpoint disease the machine exists to cure — *"the
   > eventSpine is the ONLY place any 'happening' lives, and everything downstream is a consumer of it"*
   > ([observability.md](../../reference/observability.md), founding decree). The legacy call site
   > (`doPropertyUnitSpawn()` at `CvCity.cpp:1320`) is DELETED, not re-pointed.
   > The player-scoped `SEVT_TURN_STARTED` (`iC` = the player) is the natural grain: legacy ran the per-turn spawn
   > inside `CvCity::doTurn` within `CvPlayer::doTurn`, so consuming the player boundary and walking that player's
   > cities preserves the ordering.
   > **Perf constraint:** legacy walks the tiny cached `m_aPropertySpawns` vector; the machine must NOT re-create
   > an equivalent per-city list (that is the ledger being deleted). It gates on the enabler's already-maintained
   > **operating-building set** ([enabler.md §3.2](../../specs/enabler.md)) — which is required for correctness
   > anyway, since a dormant building must grant nothing.
   >
   > **⛔ The machine does NOT read the legacy collapse members** (`m_iNumUnitFullHeal` /
   > `m_iPropertySpawnUnit`/`Property` / `m_healUnitCombats` on `CvBuildingInfo`) — those are the legacy shape being
   > deleted, and they LOSE `interval`/`enabled`/`chance` at `CvBuildingInfo::mapFrom` (`:236-246`). It reads the
   > composed `getGrants()->repeatables()`, which carries the full structure. Never widen the legacy members to
   > carry the missing fields ([grant-apply-sites.md](grant-apply-sites.md) §0).

## ONE compiled plane — how `grants` and `triggers` meet

Both authoring shapes compile into the **same entry list**, `CvTriggers` on the info; there is no `getGrants()`
section and no `m_grants` member anywhere. A `grants` block becomes ONE entry with `consideredAction = true`, no
condition and no roll — the degenerate trigger json.md §5 describes — and its payload lives in that entry's
`grant` exactly as an explicit `action.grant` payload does. `CvTriggers::consideredGrant()` is the O(1) read
(the entry's index is captured at parse, never searched for), surfaced on the info as `consideredGrants()`.

⚑ The implicit happening is a compiled FLAG, not an on-token: it is never authored (a modder writes a plain
`grants` block and no `trigger` field), so there is no token to collide with, and the dispatching event already
names which considered action it is — a building's construction, a tech's research, a civic's adoption.

## The trigger plane — coverage, measured against the DATA

> **TRIGGER IS THE TOP-LEVEL CONCEPT; a grant is a trigger with a null condition** (owner,
> [json.md §5](../../specs/json.md)) — one machine, one spine domain (`SD_TRIGGERS` / the `[TRIGGERS/*]` tags).
>
> ⛔ **Read this table before "fixing" anything here.** Judged from the CODE the applier looks full of holes — it
> walks only `ob.active` buildings and only `onTurn`. Judged against what is actually AUTHORED, every live entry
> has a home. The two are not the same audit, and the code-shaped one produces work that cannot fire.

| carrier · happening · action | entries | where it is served |
|---|--:|---|
| building · `onTurn` · `grant` (unit spawn) | 26 | the per-turn applier |
| building · `onTurn` · `heal` / `count`+`heal` | 153 | the per-turn applier (full-heal); the `unitCombat` heal-RATE leg is the MODIFIER plane's, deliberately skipped |
| building · `onTurnEnd` · `promote` | 97 | the targeted-propagation free-promotion path (`SEVT_BUILDING_PROCESSED` / `SEVT_UNIT_ENTERED_CITY`) — never a per-turn rescan |
| feature + improvement · `onTurn` · `PROPERTY_*` pulse | 49 | **`CascadePropertyBridge::bridgePulses`** → `CvPropertyManipulators` → the solver's ordered `propagators → interactions → sources` pass |
| trait · `onTurnEnd` · `promote` | 167 | **nothing, correctly** — DEAD CONTENT: the traits and the promotions they depend on are killed, and no leaderhead references any trait |

**Consequences, so they are not re-litigated:**

- ⛔ **The per-turn applier must NOT apply property pulses.** Zero buildings author one; the only carriers bridge
  theirs at load. Applying them again would double the value AND land it outside the solver's order, where spread
  resolves against PRE-source values ([engine.md](../../reference/engine.md)) — and that engine's math is
  owner-LOCKED ([property-audit.md](property-audit.md)).
- ⛔ **`destroy` has ZERO authorings.** It is §5's first worked EXAMPLE, not live data. Parsing it is machinery for
  a hypothetical, which json.md bars; it lands with `removes` if that direction is ever taken.
- ⛔ **No happening beyond `onTurn`/`onTurnEnd` is authored**, so the open `on<Happening>` registry needs no further
  entry point today.
- ⚠ **A promotion that stops being valid is dropped by the PROMOTION SYSTEM itself** (owner). So a granted
  promotion needs no take-away verb, and "the payload plane cannot revoke" is NOT an argument for re-homing
  `freePromotions` — building-list and trait-dict alike stay `triggers` entries.

**FIXED (do not re-report):** a flat `chance: N` now fires — the odds moved onto the trigger
(`tr_triggerChance10000`), where §5 puts them, instead of being reachable only through a property lookup that
returned early on an absent `per`, which made every plain-odds entry silently inert; and an entry granting several
units now places **all** of them, one roll per entry, instead of silently keeping `[0]`.

## ⛔ A DROPPED TRIGGER ANNOUNCES — every skip goes through the ONE census

**If a trigger fails to parse or to land, say so** (owner). The plane is fail-closed by design in several places —
the bridge refuses a source it cannot faithfully translate rather than applying it under a wrong condition, and the
parser refuses malformed input — and being fail-closed is right. Being fail-closed *and silent* is not: authored
data that loads, never applies, and reports nothing is invisible on both axes at once.

Every drop now routes through **`jsonNoteUnconsumed`**, the ONE load-time census
([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)) — the same mechanism the
parser already used for unknown verbs and keys, surfacing on readJson's `unconsumedSections` coverage count and its
per-item events. ⛔ Do NOT add a second reporting path or a bespoke spine domain for this; the census is the path.

| drop | reported as |
|---|---|
| unknown `action` verb | `triggers.action:<verb>` |
| unknown entry key | `triggers:<key>` |
| entry is not an object | `triggers:entryNotAnObject` |
| a `promote` promotion that is not a string / does not resolve | `triggers.action.promote:*` |
| pulse not a per-turn constant source | `triggers.pulse:notPerTurnConstant` |
| pulse is chance-rolled (not a constant source) | `triggers.pulse:chanceRolled` |
| **pulse condition the bridge cannot translate** | `triggers.pulse:conditionUntranslatable` |

⚑ The last row is the one that was genuinely invisible: a conditioned pulse whose condition falls outside the
bridge's known predicate set was dropped with nothing said. What the census now reports is a real question to ASK of
a loaded game, not a hypothetical.

## See also

- [start-packages.md](start-packages.md) — the GAME-START provisions as authored data (design, not yet built):
  start packages authored ONCE and stacked, replacing the hardcoded engine unit selection.
- [json.md](../../specs/json.md) §5 (`grants`) · [event-spine.md](../../specs/event-spine.md) (the `IEventConsumer` front door) ·
  [mission-outcome-system.md](../../reference/mission-outcome-system.md) (the carved-out missions-pass keys, NOT grants) ·
  [data-migration-remaining.md](data-migration-remaining.md) (the classification pass).
