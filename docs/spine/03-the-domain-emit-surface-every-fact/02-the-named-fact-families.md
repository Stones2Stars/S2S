# The named fact families

> Part of the **[03-the-domain-emit-surface-every-fact](../03-the-domain-emit-surface-every-fact.md)** spec.

⚖ **BESIDE THE SUBSTRATE FACTS, THE PLOT ANNOUNCES ITS OWN DERIVED VERDICT: `SEVT_PLOT_PREDICATE_ADDED /
_REMOVED`, carrying the `CASC_PRED_*` id.** It is emitted by `PlotContext` — the store that OWNS the verdict —
at the 0 ⇄ 1 crossing and nowhere else, exactly as the amenity fold announces its own crossings: the fold IS
the maintenance path, so an emit anywhere else would be a second one.
⛔ **It is NOT a duplicate of the substrate fact and never replaces one.** A substrate fact says what the TILE
now CARRIES; this says what that MEANS for the one predicate that moved. A consumer routing on a substrate id
is asking about the SOURCE; a consumer routing on this is asking about the VERDICT.
⚑ **It exists because the city cannot derive it.** `CityContext.plotAttrs` is the fold of its member plots'
bits, and by the time any consumer runs the plot already holds the NEW value — so a city-side "unfold the old
bits, refold the new" is impossible, not merely wasteful ([contexts.md](../../cascade.md): the plot
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
([cascade.md](../../cascade.md) § THE INVARIANT).
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
([modifier.md](../../cascade.md) §2a), so DOMAIN. ⚠ **ONE slider move emits SEVERAL facts** — the setter
REBALANCES the other channels in place to hold the total at 100, writing them directly rather than recursing, so
each channel it moves emits its own fact; a consumer reading only the caller's channel sees a 100-total that
does not add up. **PROPERTY VALUES** are on the surface too (`SEVT_PROPERTY_ADDED / _REMOVED`, the three
`CvProperties` mutation choke points — `setValue` plus the two new-property `push_back` branches, which
`changeValue` / `changeValueByProperty` / `setValueByProperty` all funnel through): `PROPERTY_*` is one cascade
channel per property info ([cascade.md](../../cascade.md)), read by
`CityContext::propertyValue`, by every `requires.operate` property BAND ([enabler.md](../../specs/enabler.md) §3) and
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
gating delivery and is never a cascade input ([state.md](../../specs/state.md) § A STATUS IS MIDDLEWARE).
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
> of a boundary some `requires.operate` clause actually declares ([enabler.md §3](../../specs/enabler.md)). The third is
> the **CORPORATION-ACTIVE verdict** (`SEVT_CITY_CORPORATION_ACTIVE_ADDED / _REMOVED`): the `{HAS_CORPORATION}`
> verdict is a four-leg engine composition (`CvCity::isActiveCorporation` — presence, the player-level state,
> the obsoleting tech, a consumed bonus held), so no leg's fact is it — CityContext's verdict store re-reads
> the one engine implementation on each leg's fact and announces only a genuine crossing
> ([contexts.md](../../cascade.md)). The corporation PRESENCE pair is one leg and must never route
> the `{HAS_CORPORATION}`-gated deposits: a present-but-dormant corporation is the case that separates them.
> ⛔ **The detection belongs to the HOLDER, never to each consumer.** A consumer that gates on the raw value
> fact re-derives the same sweep once per consumer AND pays it per event — and the boundaries are one registry
> (`EnablerKernel::propertyBandThresholds`), so testing them anywhere else is a second implementation
> ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).
> ⚑ **And it is what makes plane C's WITHDRAWAL exact.** If the fact IS the crossing, a consumer applies or
> withdraws on the fact's IDENTITY and never re-tests the atom — so it never depends on reading state the
> mutation has already moved past, which is the one thing
> [cascade.md](../../cascade.md) § THE INVARIANT cannot enforce for itself.
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

**THE UNIT PLANE has its mark triggers** — [cascade.md](../../cascade.md) specifies a
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
in `deleteUnit` ([unit-lifecycle.md](../../reference/unit-lifecycle.md)). The outcomes that leave a unit ALIVE
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
([the whole-entity applicability gate](../../specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)), and options are read BELOW that level too (civics
carry option-gated production / happiness / commerce deposits), so a flip moves gate verdicts AND deposits at
once. ⚠ It carries TWO id spaces, so `iB` = `GameOptionSpace` disambiguates them (the `SEVT_PROPERTY_ADDED /
_REMOVED` shape — a game-option id and a modder-option id are otherwise the same int). **`SEVT_EMPIRE_HANDICAP_ADDED
/ _REMOVED`** (`CvPlayer::setHandicap`) is a genuine cascade input rather than observability: the gather folds the
handicap's own modifier families into that player's packages, so **FLEXIBLE DIFFICULTY moving it silently froze
every handicap-derived deposit at the old difficulty** with nothing to re-derive it. **`SEVT_GAME_HANDICAP_ADDED /
_REMOVED`** (`CvGame::setHandicapType`) is its DISTINCT twin, not a duplicate — the derived average over alive
humans that every `getAI*` advantage reads ([engine.md](../../reference/engine.md): AI advantages scale with the
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
([python-read-map.md](../../reference/python-read-map.md)) is about a value moving under static data and is unchanged;
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

