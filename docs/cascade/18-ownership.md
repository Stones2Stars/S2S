# 4. Ownership — the deliveryguy rule

> Part of the **[cascade](../cascade.md)** spec.

> **This doc is the home of the deliveryguy ruling.**

A cross-entity modifier (X-keyed-by-Y) — does it live on X or fold onto Y? The test is **semantic sense: who
BRINGS this modifier to the table?** That deliverer **owns** it; the other entity is referenced as a
**condition** (`enabled` / `requires`), never the home. Two shapes, chosen per case by what reads cleanly:

- **own-output** — an entity's *own* produced output (a specialist's yield, an improvement's tile yield, a
  unit's strength) lives on **that entity**, with tech/civic/building as an `enabled` condition. *A civic
  boosting a Merchant's commerce → on the **specialist**, `enabled:{civic}` — NOT on the civic.*
- **governing-deliverer** — an entity that *delivers/governs* an effect on others lives on **the actor**, keyed
  by the target. *A route upgrading improvements → on the **route**, keyed by improvement.*

Plot-substrate entities (terrain / feature / improvement / route) each own their own `plot`-scope output. The
rule has **no special cases** — every cross-entity modifier lands by it.

**Conditioner axis:** a **tech** conditions on the **enabling** axis (`enabled:{tech}`, monotonic — once you
have it, you keep it); a **religion / resource** conditions on the **requiring** axis (`requires.operate`,
reversible — it can be lost).

**Data ≠ runtime.** The JSON is organised for a human (one home per relationship); `readJson` builds the links
both ways at parse so the machine reads top-down. Any "land it on the target" is a **parse transform**, never an
authored shape.

> **⛔ THE TWO TRAIT SETS ARE COMPLETELY SEPARATE — SEPARATED BY ID, NOT ONLY BY FOLDER.**
> A leader's traits resolve to ONE `CvTraitInfo` table from *either* its simple set (`traits/simple/`, the
> `DefaultTraits`) *or* its complex/Thunderbrd set (`traits/complex/`, the `DefaultComplexTraits`), chosen at runtime
> by **`GAMEOPTION_LEADER_COMPLEX_TRAITS`**. The curator emits both as **two cleanly-separated, self-complete folders**
> (`traits/simple/` + `traits/complex/`); a consumer **loads the one active folder** by the live game option — this is
> NOT an entity-level option gate and NOT a mid-game swap (any WorldBuilder mid-game trait swap is a post-migration
> concern).
> **A complex trait KEEPS ITS OWN `TRAIT_COMPLEX_` IDENTITY** ([naming.md](../specs/naming.md): `TRAIT_` is a simple trait,
> `TRAIT_COMPLEX_` a complex one). ⛔ It is NEVER re-keyed onto the base trait's id: that re-key is what
> manufactured the colliding-id problem — two genuinely different entities answering to one name — which then
> forced every reader to disambiguate by game option and made a wrong read silently return wrong magnitudes.
> Distinct ids remove the ambiguity by construction rather than by discipline.
> ⚖ **A COMPLEX-ONLY RUNG OF A SPLIT LINE TAKES THE PREFIX TOO.** A developing line's upper rungs exist
> only in the complex set (the simple ladder tops out early), so they are not `CvInfoReplacements` variants — and
> keeping their authored id left a chain reading `TRAIT_COMPLEX_SEAFARING` → `TRAIT_COMPLEX_SEAFARING1` →
> `TRAIT_SEAFARING2`. **The LINE is the complex variant, so every rung of it is**, whether or not that particular
> rung has a simple twin. The test is the rung's LINE, never the rung's own id.
> **⚖ IT IS A TYPE RENAME, AND THE SAVELOAD MECHANISM TRANSLATES IT.** A renamed Type is NOT a removed
> one: the record still exists under a new id, so resolving the old name to `-1` and letting the allow-missing
> class read drop the slot would throw away a rung the player still holds. The old id is mapped to the new one in
> `Assets/savemigration.txt` (a `TYPE::INFOTYPE_NAME` key — the `TYPE::` namespace satisfies the parser's `::`
> guard and cannot collide with a `Class::field` rename) and
> applied at the ONE stored-Type resolution point the class reads share.
> ⚠ The distinction generalizes beyond traits, and [save.md §7](../specs/save.md)'s three removal classes do not cover it:
> that decision procedure asks what to do when a Type is GONE. Ask first whether it is gone or merely RENAMED —
> only the first is a removal.
> ⛔ **The re-key has ONE definition, on the STORE (`Store::trait_rekey`), applied where the inverted edges are
> handed out** — because a trait id is named from several curators, above all the TECH edge that GATES a rung
> ([enabler.md](../specs/enabler.md): without it every upper rung is permanently unreachable, and silently so). A
> per-curator copy would drift and emit an id no record defines.
> ⛔ **A COMPLEX ID IS DERIVED FROM THE SIMPLE ONE, NEVER READ FROM THE AUTHORED `<ReplacementID>` (use
> the simple names as base, because that is the base of the names).** `complex_variant_id` (a module-level
> function in `Tools/Migration/store.py`, not a `Store` method) is that one derivation — the base's own stem
> under the `TRAIT_COMPLEX_` prefix — and both callers go through it: the
> replacement variant keyed at load, and the re-key of a complex-ONLY record.
> ⚑ **The authored `<ReplacementID>` is not even unique** — `TRAIT_EXCESSIVE` and `TRAIT_EXCESSIVE1` name the SAME
> replacement, so keying on it folded a whole rung into the base with nothing reporting the loss. That was
> invisible while the engine hot-swapped these in memory (the id was only ever FOLLOWED, never read); it costs a
> record the moment the sets are separated BY ID.
>
> ⛔ **A LINE MEMBER'S `PromotionLine` / `bNegativeTrait` IS SOURCE DATA THAT CAN BE WRONG, AND BOTH FAIL
> SILENTLY.** A rung tagged onto a NEIGHBOURING line leaves its own ladder with a hole; the fix is to RESTORE THE
> TAG, never to delete the rung or teach the classifier around it (the `TRAIT_TIMID1` precedent, below). ⚑ Both
> are found by comparing a record against its LINE SIBLINGS — a member whose line disagrees with its stem's
> majority, or whose negativity disagrees with its line's BASE, never the local arm (a negative line whose deeper
> rungs lost the flag can leave the untagged rungs outnumbering the tagged ones).
> **⛔ EVERY RECORD IN THE COMPLEX SET CARRIES `TRAIT_COMPLEX_`, WITH NO EXCEPTIONS.** *"If it was built
> as complex, it's complex, no matter what."* The prefix STATES THE SET — it is not a marker for "is a variant of
> a simple trait" — so a complex-ONLY line with no simple counterpart is `TRAIT_COMPLEX_` like every other record
> in the folder.
> ⛔ **THE TWO SETS ARE COMPLETELY SELF-SUFFICIENT, IN EVERY WAY — they share NO id.** A simple trait
> with no complex variant is still copied into `complex/`, but under its OWN `TRAIT_COMPLEX_` id: the copy is
> identical in content and distinct in identity. `TRAIT_BARBARIAN` was the last shared id and is one no longer.
> ⚑ **The reason is empirical, not aesthetic: *"it is impossible for agents to actually not conflate the 2."***
> A shared id is the one thread that keeps the sets tied together, and every reader who meets it has to
> reconstruct which set is meant. Distinct ids make the conflation UNSAYABLE rather than forbidden.
> ⚑ **AND THIS IS WHY THE SPLIT WORKS AT ALL: a trait is purely a collection of BUFFS — it unlocks no
> promotion, building or unit.** Nothing's availability hangs off a particular trait id, so duplicating the id
> space across two sets breaks no edge. ⚠ The dependency runs the other way and is real: a TECH names trait ids
> to gate a developing rung, so those edges must name the ACTIVE set's ids — which is why a re-key regenerates
> techs, not just traits.
> **⚖ A SAVE IS RESOLVED INTO THE ACTIVE SET AT LOAD: *"for savegames, if you see it is a complex trait
> game, you make sure the trait is the complex version."*** A stored plain `TRAIT_X` in a game running
> `GAMEOPTION_LEADER_COMPLEX_TRAITS` resolves into the active set. This is distinct from the
> `savemigration.txt` rename plane (which id, not which SET), so it lives at the ONE stored-Type resolution
> point (`sm_resolveStoredType`), beside the rename lookup rather than inside it — otherwise a loaded save could
> hold simple rank-1 rungs beside complex rank-2/3 ones.
> ⛔ **The sets are SEPARATE and complex carries no rung 0** ([the separate-trait-sets rule](#4-ownership--the-deliveryguy-rule)),
> so this resolution may NOT assume a prefixed id always exists — the retired superset claim is exactly what
> made it look free. **Which rung a stored un-digited `TRAIT_X` resolves to in a complex game is UNDECIDED and
> is the owner's call**; do not infer one.
> ⚠ **Leaderheads DO author traits** — 118 of 120 carry both a `traits` and a `complexTraits` list, so a NEW
> GAME takes its held ids from the leaderhead and the save is not the only source. *(The retired claim that
> leaderheads author none was used to argue the save-side resolution was sufficient on its own.)*
>
> ⚠ A record that does not obey this is a CURATOR defect, and fixing it rides the curator + regen in the same
> work item ([recurate on every decision](../../AGENTS.md#git--delivery)); the id change is
> a TYPE RENAME the save layer translates via `Assets/savemigration.txt` (the rename rule below), never a removal.
> (The enabler is unaffected either way: it reads trait *presence*; only the modifier cascade reads trait
> *family values*.)
>
> **⛔ Inverted-onto-a-SHARED-entity boosts stay on the TRAIT, per set — the own-output carve-out.**
> The [deliveryguy rule](#4-ownership--the-deliveryguy-rule) normally puts a trait's boost of *another* entity's output
> ON that entity as **own-output** (a trait boosting a Merchant's commerce → on the **specialist**, `enabled:{trait}`).
> But a **specialist is ONE shared file**, while a split trait's `SpecialistYield/CommerceChange` has **different values
> in the simple vs complex set** — so inverting it onto the specialist would force a single value across both systems and
> break the clean separation. Therefore, for a TRAIT keyed to a specialist (or any shared sub-city target with a per-set
> value), the deposit takes the **governing-deliverer** shape instead: it lives **on the trait, keyed by the target** —
> `yield.empire.specialists.{SPECIALIST_X}.flat` (and `commerce.…`) — authored in **each set's folder** (simple = the
> base value; complex = the **replacement's** value — a **whole-Info swap, NO base-fill**, per the legacy
> replacement semantics: a field the replacement
> omits is **inherited from the base**, §4-bis). The cascade reads it from the **active** trait
> set and applies it × the city's count of that specialist. *(Building/civic specialist boosts have no
> simple/complex split, so they keep the ordinary own-output inversion onto the specialist.)*
>
> **⛔ Trait option resolution — the curator translates the CRAZY → sensible; the cascade applies only CLEAN gates
> (this is the volcano every agent rollerskates into — read it before touching trait values).**
> Several `GAMEOPTION_LEADER_*` options can be live at once (complex, developing, pure, no-negative, …) and each
> mutates a trait's *effective* values. The TB implementation was a runtime hack — **deleted from this tree**, and
> described here only so it is never rebuilt: a base trait carried an inline replacement id + a `BoolExpr` condition,
> and a global re-run swapped the WHOLE `CvTraitInfo` in place for the first replacement whose condition held. **We do
> NOT emulate that hack anywhere in the cascade.** The split of responsibility is absolute:
>
> - **CRAZY → curator (`curate_trait`), offline, once.** The replacement/promotion-line machinery is dissolved into
>   sensible JSON:
>   - **Simple/complex split** by `COMPLEX_TRAITS` — the two `DefaultTraits`/`DefaultComplexTraits` sets become
>     `traits/simple/` + `traits/complex/`.
>
>     > **⛔⛔ THE TWO SETS ARE COMPLETELY SEPARATE AND EACH IS SELF-COMPLETE ON ITS OWN TERMS — THERE IS NO
>     > OVERLAY, NO BASE-FILL, AND NO SUPERSET RELATIONSHIP.** A complex record is NOT a simple record
>     > with tags laid over it, and a simple record is never copied into `complex/` to make the sets line up.
>     > Each set is authored, emitted and read as its own table; the only thing they share is the game option
>     > that selects which one is live.
>     > ⛔ **The overlay/`<ReplacementID>` machinery was TB's workaround for not knowing how to do this properly.
>     > It is NOT the model and is NOT reproduced** — do not rebuild it, and do not reason from it.
>     > **⛔ A COMPLEX GAME HAS NEVER USED RUNG 0 OF ANY TRAIT — A LINE IS `1 → 2 → 3`.** The un-digited
>     > record is the SIMPLE set's base; in `complex/` it is not a lower rung, it is the simple trait leaking in,
>     > and nothing in a complex game ever holds it. So a leaderhead's `complexTraits` names rung 1 and above,
>     > never a base beside it.
>     > ⚠ **This has been corrected repeatedly, and every recurrence traced back to THIS PARAGRAPH still carrying
>     > the retired model** ([rulings go to the repo immediately](../../AGENTS.md#documentation--knowledge--keep-it-in-the-repo)): the
>     > ruling was given in conversation and the spec was left standing, so each new agent read the overlay model
>     > here and rebuilt it in good faith. A ruling that is not written down is a ruling that gets re-litigated.
>     > ⚑ **The measurable tell that the leak is present:** `complex/` carrying an un-digited record for a line
>     > that has numbered rungs. Only a line with NO rungs may legitimately have a bare record.
>
>     **Folder classification** keys on the `OnGameOptions: COMPLEX` gate /
>     replacement-variant; a developing-line (`PromotionLine`) member that UNIQUELY lacks the gate its siblings carry is
>     a SOURCE-data bug to fix (restore the tag), not a classifier change (the `TRAIT_TIMID1` case). The active set is
>     chosen by the live option (callout above).
>     **⛔ TRAITS ARE NOT CONTENT-LOCKED — THE CURATOR IS THE AUTHORITY AND THE FOLDERS ARE REGENERATED.**
>     A hand-maintained lock let an edge in one set point at an entity only the other has, with nothing regenerable
>     to correct it; `curate_trait` reads the legacy XML like every other curator and `--write` rewrites both
>     folders. ⚑ **Its input is the ARCHIVED XML** (`SourceArchive/Assets/**`, searched by `store.py` alongside the
>     live roots) — curator INPUT only, never a game load path
>     ([reading a replaced info's XML into the game is banned](../../AGENTS.md#build-and-test)), and unrelated to the red-ratchet
>     ban on reviving a `CvXInfo` from `SourceArchive/Infos/`. ⚠ Community-owned trait CONTENT still lands through
>     `_additions/` like any other post-curation authoring ([curators/README.md](../specs/curators/README.md)) — a
>     regenerable base with an overlay, not a frozen folder.
>   - **⛔ THE LADDER EDGE IS RESOLVED FROM LINE MEMBERSHIP, NEVER FROM THE ID SPELLING.** A rung `enables` the rung
>     above it ([json.md §9](../specs/json.md): a ladder is an `enables` edge, not a section), and which rung that IS comes
>     from the line's members ordered by `iLinePriority` — restricted to the FOLDER being emitted, so a chain simply
>     ends where that set ends (`simple/` tops out at rung 1) and never reaches into the other set. The base rung is
>     `iLinePriority` 0/absent, and the two arms (`+1,+2,+3` and `-1,-2,-3`) each chain outward from it.
>     ⚠ Deriving the successor by string arithmetic on the id (`TRAIT_X1` → `TRAIT_X2`) fails silently on a
>     mid-chain RENAME (`TRAIT_NOMAD1` → `TRAIT_NOMADIC2`, a fabricated edge to an id no record defines), a rank
>     SKIP (the link is lost entirely), or a top rung (an edge to a rung that does not exist).
>   - **Developing line — do NOT auto-develop (engine-verified).** A `PromotionLine` is a chain of trait *levels*
>     (`TRAIT_NOMAD1`→`TRAIT_NOMADIC2`→`…`, ordered by `iLinePriority`, each with a `PrereqTech`+`TraitPrereq`), but
>     **researching a level's `PrereqTech` does NOT advance the held trait**. The **held trait the engine reports IS
>     the authoritative level**; the cascade uses its payload as-is. ⚠️ A tech-gated "collapse" that folds higher
>     levels into the entry is the WRONG model (it re-levels traits the engine leaves alone). Levels advance by some
>     other gameplay progression, not by tech alone; until that's mapped, trust the engine's own reading.
>   - **Complete, not pre-filtered.** The JSON carries ALL values — positive AND negative — plus the `negativeTrait`
>     flag, so the runtime gates below have the full data to act on. The curator never bakes in a pure/no-negative pass.
> - **CLEAN gates → cascade, at eval (its ordinary condition-eval, NOT hack emulation).**
>   - **`PURE_TRAITS` gate (implemented)** — when `GAMEOPTION_LEADER_PURE_TRAITS` is live, drop each trait value whose
>     alignment opposes the trait's: a `negativeTrait`'s **upside** values drop and a positive trait's **downside**
>     values drop.
>     > **⛔ ALIGNMENT IS FAMILY METADATA, NOT THE SIGN — `infoKindAlignmentInverted(family, kind)` is the one
>     > table.** On an **INVERTED** (family, kind) a POSITIVE value is the DOWNSIDE, because the number counts a
>     > cost, a penalty, a timer or a worse-when-higher threshold. Grounded row-for-row in the legacy per-getter
>     > filters (`CvTraitInfo`): 48 members guard `isNegativeTrait() && x > 0` and **19** guard
>     > `isNegativeTrait() && x < 0`. Inverted today: `maintenance` · `costs` · `hurry` · `lessYieldThreshold` ·
>     > `growth` · `anarchy` (whole families); `upkeep` **except** its free-amount kinds (§2: a positive
>     > `freeMilitary`/`freeCivilian` GRANTS, so it stays normal); `experience.levelModifier` only;
>     > `durations`' two ANARCHY timers; `diplomacy.warWeariness` only — its `enemyWarWeariness` twin is a GAIN
>     > and `attitude` an ordinary upside; and `revolution`'s unrest kinds, whose `*Good` twins stay normal.
>     > ⛔ **Three families carry BOTH polarities** (`revolution`, `diplomacy`, `durations`), which is why this is
>     > declared per (family, KIND) and can never be collapsed to a per-family flag.
>     > ⚠ **`growth` is the trap worth naming:** it is the growth THRESHOLD percentage — higher means more food
>     > per citizen, i.e. slower — so it reads as an upside and is not. It surfaced on the FOOD tooltip, where a
>     > positive trait was losing its upside and keeping its downside on the one number displayed.
>     > ⛔ **Derive the list from the legacy getters, never from the authored SIGNS:** traits author both
>     > directions in every one of these families, so the data cannot tell you the polarity — only the 48/19
>     > getter split can.
>     > ⚑ **A sign-only gate is wrong in BOTH directions and silently so:** it keeps `lessYieldThreshold: +5` on a
>     > positive trait as though a gain, and drops `maintenance.distance: −10%` — a genuine upside — as though a
>     > penalty. Neither shows as an anomaly; the totals stay plausible.
>     Concretely for thresholds: an
>     `extraYieldThreshold` is an UPSIDE → dropped from a negative trait; a `lessYieldThreshold` is a DOWNSIDE →
>     dropped from a positive trait (engine `getLessYieldThreshold` 2132-2147 sets it to −1). The cascade reads the
>     `negativeTrait` flag (`NegativeTraits*` in the repo) + the live option. This is how "parity comes to us": a
>     legacy behaviour we judge correct is reproduced by a clean gate, never re-implemented as the hack.

**`production` vs `buildRate`.** `production` = `InfoValuation::cityRate`'s PRODUCTION channel (total city output — scales every
build every turn; a flat ADD or city-wide percent). `buildRate` = `getProductionModifier(eItem)` (shrinks the
COST of a SPECIFIC item, never a per-turn yield), sub-shapes `buildRate.self` /
`.<scope>.{units|buildings|domains|unitCombats}.{TARGET}` (keyed) / `.<scope>.{units|buildings}` predicate-filtered
(the §5 plural target) / `.<scope>.{worldWonder|teamWonder|nationalWonder}` (category). (The "Versailles bug" =
filing an item discount under `production.city`.)

> **⛔ AN EVENT AUTHORS YIELDS, NEVER A PRODUCTION MODIFIER — WE DO NOT DO `buildRate` ON EVENTS.** An
> event's payload is the ordinary yield vocabulary; it does not reach this family at all, at any scope.
> ⚑ **The reason is the SHAPE, not the size of the effect.** A `buildRate` deposit is alive-while-its-source-is
> — the continuous-deposit model this whole doc describes — while an event is a one-shot happening with no
> surviving source to withdraw against, so a slot fed by one can be maintained by neither mechanism
> (§ WHY DELTA-DERIVING FAILED BEFORE, above: a baked-in
> one-shot grant is precisely what makes an accumulator unrecoverable).
> ⛔ **And if it is ever wanted, it is built PROPERLY — not in a roundabout fashion.** The legacy shape
> reached it by pushing into a hand-named per-player accumulator behind an `applyEvent` write, which is the
> STORED-ACCUMULATOR DRIFT class ([the uniform legacy-accumulator cut](03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism));
> reviving that is the banned move, and a genuine event-driven build discount would author on the trigger plane
> ([triggers.md](../specs/triggers.md)) like every other happening-fired effect.

> **⛔ `military` AND `space` ARE NOT CATEGORIES — `units` IS THE BASE TARGET AND THEY ARE PREDICATES ON IT
>: *"military is not a base category, units is."*** Both legacy tags answer WHICH UNITS build faster, so
> both author the ordinary plural target with a filter — `buildRate.<scope>.units.percent`, entry
> `{value, enabled: IS_MILITARY | IS_SPACE}` — which is [§6.1](../specs/json.md)'s own `units {IS_WATER}` exemplar and
> needs no vocabulary of its own.
> ⛔ **A category member per legacy tag NAME is the curator minting a kind off a spelling** — the
> condition-as-member shape [conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query)
> retires, and it is what the §6 member-triage test already rejects: a member is a KIND only if it answers WHICH
> COMPONENT, never WHICH TARGET or WHEN.
> ⚠ **The buildRate MECHANIC is legit and is not under review: *"buildRate is a legit mechanic, we do not
> kill them all."*** This narrows how two members are ADDRESSED; it removes no effect.
> ⚑ **Spacecraft are not a class outside the military one**, which is why space is a sibling predicate rather
> than a tier of its own. ⛔ And the legacy consumer's gate — `CvProjectInfo::isSpaceship`, the vanilla
> space-VICTORY spaceship parts — **does not apply**: that is vanilla Civ's victory machinery, not the space
> units the boost describes.

---

