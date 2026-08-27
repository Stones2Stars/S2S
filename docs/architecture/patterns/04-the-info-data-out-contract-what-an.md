# The INFO DATA-OUT contract — what an info hands to the cascade

> Part of the **[patterns](../patterns.md)** spec.

> **This section is the home of [parsing/holding info data is info-side, never cascade-side](#the-info-data-out-contract--what-an-info-hands-to-the-cascade)** — parsing and holding
> the info data is INFO-side, and cascade runtime never lives on an info.
>
> The **infos** row of [EACH IS ITS OWN SYSTEM](../north-star.md): readJson puts data into infos, infos SERVE that
> data, the cascade sums, the enabler resolves availability. This section is that row's concrete surface.
> **It is stated as a CONTRACT, not a prohibition** — a prohibition has to be remembered by every future agent,
> the enforcement model this project keeps watching fail; a contract makes the violation unsayable rather than
> forbidden, because there is no member to write to.

**An info is a pure DATA SOURCE with one outbound surface.** It is loaded once, immutable thereafter, and shared
by every player — so it can carry authored data and nothing else. Concretely:

1. **What an info holds** — the availability model (the `enables` family, `requires`/`allowed`, the load-derived
   reverse edge families) and its own authored modifier data, resolved to typed members at `mapFrom`.
2. **What an info hands out** — its data, ASKED FOR BY CHANNEL: *"give me your flats / your percents for these
   channels."* The cascade points at a LIST of infos and sums what comes back. It never reaches inside an info's
   per-type shape, and an info never learns what a cascade, a scope, or an owner is.
3. **What an info CANNOT hold** — per-owner state, a computed total, a staleness flag, a cache. Not by rule: by
   construction. There is nowhere on the object to put it, because the outbound surface is the only surface.

**Why the boundary is load-bearing, not tidiness.** An info is write-once-at-load and shared; cascade runtime is
per-owner mutable derived state. Storing the latter on the former silently makes an immutable, shared object
mutable **per game rather than per load** — and it is the third copy of the same static numbers, after the
authored JSON and the compiled deposit index.

### ⛔ WRITE-ONCE-AT-LOAD — A READ NEVER CREATES, AND AN UNANSWERABLE READ FAILS LOUD

> Binding: [the info plane is write-once-at-load](#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud). The read-side twin of
> [exactly one JSON reader](08-the-one-reader-the-load-pipeline.md#the-one-reader--the-load-pipeline-law) — that rule says there is exactly ONE writer; this one
> says everybody else is a reader, and says what a reader does when it cannot be served.

**Two access paths, and they are not interchangeable.** `InfoRepo::edit`/`editPtr` are **get-or-create**: they
resize the array and `new` a payload for whatever id they are handed. That is correct for the LOAD pipeline, which
is what they were written for, and they belong to exactly three callers — the one reader (`loadJson`), the reverse
pass, and the classification registry. **Every other caller is a READ and uses `atPtr` / `get`.** A read that
creates violates all three of the contract's clauses at once: it writes to the plane outside the one writer, it
does work on a path specified as a bare fetch ([cascade.md](../../cascade.md)), and it answers
quietly where it should fail ([legacy must fail loud, never mask a cascade gap](../../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)).

**What get-or-create actually answers with is a BLANK info, and that is why this is a hard rule rather than a
preference.** A blank is indistinguishable from a real one until something asks it a question — and then
`CvInfoBase::getType()` returns **NULL**, which crosses into the EXE's UI and into boost::python and is
dereferenced *there*. The failure therefore surfaces in someone else's frame, as an access violation at an address
in the EXE's image, with nothing left pointing back at the id that caused it. ⚠ **That address is the bait:** it
reads as an EXE defect, and an agent who stops there is chasing the closed binary for a NULL we supplied.

**⛔ Worse on an ALIASED repo, which is most of them.** The backing IS `GC.m_pa<X>Info`, and `getNum<X>Infos()`
returns that vector's `size()` — so creating on read **moves the registry's own bound**. Every bounded walk over
the registry then runs off into the entries the walk itself created; the observed signature is a walk that never
terminates and dies as an out-of-memory rather than as a bad id. ⚠ And the bounds assert that looks like it guards
this (`FASSERT_BOUNDS`) is **compiled out of `Release`/`FinalRelease`** (`fbuild.bff`), i.e. absent from every
build the game is actually played in — so a fail-loud here is a real function, never an assert.

**⚖ THE RULING: crash at the main menu because things are not loaded, rather than manually incrementing
the registry to limp past it.** An unanswerable read is a LOAD defect — some pass did not fill the slot — and the
only useful thing to do with it is name it, at the bad read, while the registry and the id are still known. It is
reported into `Exceptions.log` beside the handler's own entries and into `Loading.log` beside the `[READJSON]`
census that built the plane, then raised NONCONTINUABLE so the ordinary unhandled path writes the minidump and the
Python callstack.

**⛔ And the corollary that is easiest to get wrong: DO NOT DEFER THE READ TO MAKE THE FAILURE QUIET.** Moving a
read later — screens built on first use instead of at `earlyInit`, a plane consulted lazily "to get to the menu" —
initializes *nothing*. It relocates the failure from a named Python `AttributeError` at the menu to an access
violation deep in the interface, and buys the appearance of a load that got further. **A load that reaches further
by asking fewer questions has not got further.** Screens are therefore constructed eagerly, on the engine's entry
path, precisely so an incompletely stood-up info plane fails where the failure is legible
([python-load-sequence.md](../../reference/python-load-sequence.md)).

**The failure this closes.** Asking each info type for its data through a DIFFERENT accessor is the same defect as
a hand-named scalar per channel ([every derived cache is one shape](../../cascade/04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)): it cannot be
addressed uniformly, so every type needs bespoke read code, and the cascade ends up shaped by the info surface
instead of the other way round.

### An info is STYLED FOR THE JSON, not the legacy field set

The info's MEMBERS mirror the **JSON entity anatomy** ([json.md §2](../../specs/json.md): availability ·
provisions · effects = the modifier families · intrinsic · classification · auxiliary), each held as its proper
typed structure. It is **not** a scalar-per-legacy-XML-field. The turnaround is the whole of "make the infos sane":
the JSON model drives the info's shape; the legacy variable set is gone, not force-fed.

- **The exemplar is the classification block — generalize it.** `m_attributes` is a **JSON-derived bitset** (the
  `ClassificationRegistry` ids minted from the authored `attributes` block,
  [the classification-infos registry](../../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), read by the parameterized
  `CvInfo::hasAttribute(id)` over `clsHasId` — never a legacy `m_bDestroyedOnCapture`, and never a named per-key
  body. Every block gets this shape, one member per block the entity authors (`m_attributes` beside
  `m_amenities` on a building, [json.md §8](../../specs/json.md)).
- **The defect the rebuild removes** is the legacy-named scalar-per-field with a comment mapping it back to a JSON
  address (`m_iDamageToAttacker` ← `defense.city.counterDamage.damage`; `m_aiRiverPlotYieldChange[]` ←
  `<yield>.city.plots` flats). Those are JSON parsed and **scattered into individually-named legacy variables**;
  the sane form holds the JSON structure and reads it, so a new field is DATA, not a new member + getter.
- **An info holds only ITS OWN side — cross-entity own-output lives on the TARGET.** A building does not
  project yield onto an improvement; the **improvement** says *"I produce this much now, because a building is
  present"* — own-output, the building's presence a condition on the improvement's own deposit ([the deliveryguy ownership rule](../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule),
  modifier.md §4). So `CvBuildingInfo` carries no `improvements`/`terrains` yield map. This needs **no curator
  re-home**: the **load-time reverse structure** ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)) builds cross-entity links both ways at
  readJson, so a modder may author *either* side and the relationship is landed on the other programmatically — the
  improvement ends up owning its yield regardless of which side authored it. A target-keyed map survives on the
  source **only** where the source is the genuine deliverer with no target-owner (governing-deliverer, modifier.md §4).

### The coherent surface — grouped storage, parameterized getters (CLARITY AND PREDICTABILITY IS KING)

The numbers and the booleans are **organized into named groups, each read by ONE getter parameterized over the
group's natural index** — never N individual getters for a groupable set. This is the whole shape of a sane info;
`getYield(YIELD)` is right, `getFoodYield()`/`getProductionYield()`/… is the disease, and so is
`isNukeImmune()`/`isZoneOfControl()`/… for the boolean blocks.

- **The AUTHORED form is the JSON anatomy; the ANATOMY WALK IS LOAD-ONLY.** The [json.md §6](../../specs/json.md)
  deposit model — per family, the [§3.9](../../specs/json.md) entries under their FULL five-axis address
  `<family>.<scope>[.<target>|.<targetType>.{TARGET}][.<member>].<unit>` — is what the reader parses, with
  **every string key interned to a typed id** (family/member → the shared kind-enum vocabulary, scope → the
  scope enum, named-entity targets → FK-resolved ids, conditions → parsed trees;
  [materialize at mapFrom](07-materialize-at-mapfrom-no-runtime.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)), nothing flattened away, the §3.9
  mechanism UNREDUCED (`per`, the `ai` sibling, the `enabled`/`disabled` twin trees in their spec'd order —
  `enabled` first, a holding `disabled` OVERRIDES; a plural-target filter is the entry's own `enabled`
  predicate, [json.md §6.1](../../specs/json.md)).
- **The ONE load COMPILE pass walks those entries ONCE and produces the runtime forms — after load, nothing
  ever walks the anatomy.** A **null-condition entry's value folds STRAIGHT into its group's compiled member
  array** — the enum-keyed `[kind × family-scope-set]` unconditioned ×100 sums, the grouped member pattern,
  scope-free kind names (Σflat vs Σpercent separate slots — the unit is part of the slot key,
  [modifier.md §2](../../cascade.md)). A **conditioned entry** lands in the group's compiled conditioned
  list, its condition tree prebuilt, evaluated ONLY at event-driven package rebuild and the per-decision
  `expected*` read — never re-parsed, never re-derived. Classification compiles to JSON-derived bitsets
  (`m_attributes` / `m_capabilities` / `m_skills` / `m_policies`); edges to the load-populated forward/reverse
  families ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)); intrinsic lone values to plain typed
  members. No string, no parse node, and no anatomy tree survives into a runtime read path.
- **⛔ THE SCOPE AXIS — a kind-enum names its CONCEPT ONLY; scope is a separate dimension**
  ([scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king)). Scope is its own axis of the deposit address + a
  spelled-out getter parameter, NEVER a fragment of an enum, member, or getter name — a scope word (`GLOBAL`,
  `ALL_CITY`, `WORLD`, `AREA`, …) inside a kind name collapses two of the address's axes into bespoke per-pair
  entries. `getDefense(DEFENSE_AMOUNT, SCOPE_CITY)` — kind and scope are separate arguments, exactly as the
  JSON's own `<family>.<scope>.<member>` separates them.
- **⚖ THE DECISION PROTOCOL IS TWO STAGES, IN ORDER: ask the ENABLER what is possible, then ask the CASCADE
  what happens if you do it.** Every AI decision is that pair — the ENABLER narrows to the
  candidates ([enabler.md §6](../../specs/enabler.md): the frontier is the shared choice set, iterated instead of
  the entity database), and the CASCADE values each survivor through the what-if. ⛔ Neither half substitutes
  for the other: scoring what cannot be done wastes the expensive half on candidates the cheap half would have
  dropped, and gating without valuing picks an option the AI cannot weigh. ⚑ **This is what makes the what-if
  affordable at all** — it runs over a small maintained set, never over everything.
- **THE WHAT-IF DRIVER — the AI's planning asks are STRAIGHT RESPONSES, 0 calculation.** The two
  most-asked questions in the engine both answer from compiled structures: *"what can I do next after getting
  this?"* is the FUNDAMENTAL enabler-tree read — the info's load-compiled `enables`/reverse edge families + the
  enabler's maintained domain vectors, a pure list fetch ([enabler.md §7](../../specs/enabler.md): every read is an
  O(1) lookup that never calls a calculator; the tree is conditional-free by design). **The ONE calculation in
  that whole flow is the `requires` gate** — very few things have a single prerequisite, so a newly-proposed
  candidate is confirmed against its remaining prerequisites — **and it runs at HAVE-CHANGE time**, over only the
  affected candidates via the `EDGEF_REQUIRED_BY` re-gate ([enabler.md §7.1](../../specs/enabler.md)), never at ask
  time: when the AI asks, the verdict already sits in the tri-state vector. *"What do I gain from building
  this?"* fetches the compiled unconditioned sums straight — one load per slot — and only the compiled
  CONDITIONED tail is ever evaluated (through the ONE evaluator against the contexts,
  [contexts.md](../../cascade.md)), at per-decision cadence in the `expected*` read. The entity-level active/dormant
  verdict stays the ENABLER's, fed in via the precomputed operating set — a what-if read never re-evaluates
  `requires`.
- **THE GETTER SETUP — one exemplar shape for every info (the aim). Four read categories, nothing else:**
  1. **Sections** — whole typed objects the enabler + grants/provides machinery read: `getRequires()` /
     `getEdges()` / `getAllowed()` / `getGrants()` / `getProvides()` / `getWhenObsolete()`.
  2. **Classification** — O(1) bitset tests, the **name encoding hold-vs-provide** (json.md §8): what the
     entity HAS is `hasAttribute(id)`/`hasAttributes()` (building) and `hasSkill(id)`/`hasTag(id)` (unit); what it
     PROVIDES to something else is `providesCapability(id)`/`providesCapabilities()` (to the empire) and
     `providesSkill(id)` (a grantor handing a skill on).
     > **⛔ THE PARAMETERIZED READ IS THE CONSUMER SURFACE — a consumer NEVER asks for a key by name.** A
     > consumer asks `kUnitInfo.hasSkill(CLS_SKILL_BLITZ)`; the id is a compile-time constant from the
     > **generated** `Infos/CvClassificationIds.h`. Seven reads on `CvInfo` cover every domain
     > (`hasSkill` / `hasTag` / `hasAttribute` / `providesAmenity` / `hasCharacteristic` / `providesCapability` /
     > `providesPolicy`, plus `revokesSkill` for the §4 revoke plane), each a NULL-safe `clsHasId` over the
     > block — so a block-less info answers FALSE and the read is total.
     > ⛔ **A new authored key is a REGENERATED TABLE ENTRY, never a new function.** Do not add a named getter,
     > and do not re-introduce a per-key read class: that shape is what this replaced.
     > ⚑ **How the open registry still hands out a compile-time id:** the blocker was the id ORDER being
     > DISCOVERED at load, not the openness. `Tools/Migration/curate_classification_ids.py` pins the order (as
     > `curate_order.py` pins `_order.json`); `ClassificationRegistry::buildAndResolve` SEEDS from that table
     > before minting, so the constant IS the runtime id. The category stays OPEN
     > ([the classification-infos registry](../../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)): an unlisted key still mints at load,
     > appended after the seeded block — regenerating just promotes it to a constant, save-neutral since nothing
     > serializes.
     > ⚠ **The constants carry a reserved `CLS_` namespace** (`CLS_TAG_MILITARY`) distinct from the runtime
     > INFOTYPE STRING (`TAG_MILITARY`): the property engine owns global `TagTypes`/`AttributeTypes` in
     > `CvEnums.h`, and an OPEN registry could otherwise collide with any engine enumerator.
     > ⚑ A **spec-named key that no data authors yet** (skills.md §1/§2 headroom — `noSelfHeal`, `freeDrop`, …) is
     > seeded from the generator's `HEADROOM` list so a legitimate consumer still compiles and reads false, until
     > data authors it.
     >
     > ⛔ **THE INFO-SIDE NAMED `CLS_HAS` BODIES ARE GONE TOO — the whole per-key shape is retired, both halves.**
     > ⚠ The names permanently COLLIDE with live game-object methods: `isCapital` is a building AMENITY *and*
     > `CvCity::isCapital`; `isNukeImmune` is a building amenity, a plot-substrate CHARACTERISTIC *and*
     > `CvUnit::isNukeImmune`; `isGovernmentCenter` is an amenity *and* a `CvCity` method. The RECEIVER decides
     > which one a call site means, never the name — a textual sweep destroys the game-object half. Convert
     > receiver-by-receiver, with an allowlist of info-typed receivers.
     >
     > **⚖ AND FOR AN AMENITY THE RECEIVER IS THE QUESTION ITSELF (json.md §8, [contexts.md](../../cascade.md)).** An
     > amenity is CITY-HELD, grantor-PROVIDED: *"does THIS CITY have it"* is answered by the city's FOLD
     > (`CityContext::hasAmenity`), never by asking a grantor — re-pointing such a gate at
     > `kBuilding.providesAmenity(...)` would leave it unchanged while reading as migrated. ⚑ The converse holds
     > for most sites: an AI VALUATION and a FOLD APPLY (`processBuilding`) legitimately ask the GRANTOR, since
     > the candidate's own block IS the answer there. Classify by the QUESTION: gate → the city; valuation/apply/
     > display → the grantor.
  3. **Modifier groups — three reads per group, all over the LOAD-COMPILED forms:**
     - the **straight point read** over the compiled unconditioned sum — `getDefense(DefenseKind eKind,
       ScopeKind eScope)` → one array load, **0 calculation** (kind and scope separate arguments,
       [scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king));
     - the **compiled conditioned list** (`defenseConditioned()` / `yieldConditioned()` / … — the typed entries
       with prebuilt condition trees; what the package rebuild, the pedia, and the valuation walk);
     - the **what-if valuation** — the [contexts.md](../../cascade.md) per-GROUP endpoints
       (`expectedFlatYields(cityContext, empireContext, plotGroup, flatYields)` and siblings): the compiled
       sums fetched straight PLUS the group's conditioned tail through the ONE evaluator, `plots`-targets scaled
       by `cityContext.plotAttrs`, scopes folded into the experienced-here answer, the active/dormant verdict fed
       from the enabler. This IS the AI's *"what do I gain from building this?"* read.
  4. **Intrinsic** — bare typed reads (`getAirlift`, the shrine/corpHQ FKs, flavours), plus `getScalar(SCALAR_X)`
     for the 1–2-entry stragglers (genuinely lone unconditioned values).
     > **⛔ A TEXT read NAMES WHICH SIDE OF THE BOUNDARY IT IS ON — `*Key()` returns a TXT_KEY, the bare form
     > returns RESOLVED TEXT.** A text naming must specify that a KEY is being fetched rather than the actual
     > text, so it is unambiguous which of the two a call site holds. TXT is an
     > unmigrated system the JSON only REFERENCES ([json.md §7](../../specs/json.md)), so an INFO holds keys and
     > resolution belongs to the text manager; a name that hides which one you are holding is how a raw key ends
     > up rendered to a player, or a resolved string ends up fed back into `getText`.
     > ⚑ The convention is already the tree's: `getCivilopediaKey`/`getHelpKey`/`getStrategyKey`/
     > `getShortDescriptionKey`/`getAdjectiveKey` return keys beside the `DllExport` bare forms that return text.
     > ⚠ The four bare EXE-bound reads (`getTextKeyWide`, `getDescription`, `getText`, `getHelp` on
     > `CvInfoBase`) are FIXED BY ABI and are not renameable — check `DllExport` before proposing any text
     > rename ([engine.md § Is a symbol really EXE-bound?](../../reference/engine.md)).
  5. **The per-entry TEXT render (so that tooltips work properly)** — every compiled entry renders
     itself as ONE localized detail line (`+25% Production — while Coal connected`), the `detailLines` pattern
     of the combat calculator (`CvCombatModel::computeCombatPreview`'s itemised per-modifier breakdown),
     through ONE shared renderer ([the DRY single-implementation law](03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)) — the
     tooltip/pedia composers consume rendered entry lines, never hand-assemble from getters. Cold path:
     spell-back segments + TXT keys are the honest cost there. **Structural consequence: the compiled entry
     list is COMPLETE — unconditioned entries are RETAINED as entries** (the folded sums are the derived fast
     plane beside them, never a replacement) — per-entry text and per-entry attribution both require the list.

     > **⚖ THE DIVISION OF LABOUR — `CvGameTextMgr` KEEPS THE BLOCKS AND LOSES THE SUB-BLOCKS.** The
     > renderer removes *"the vast majority of bespoke work GameTextMgr used to do"*: the text manager
     > *"should only care about TXT_KEY replacements, and be the `Cy` target for actual string content — it
     > should not need to manually convert entries for each individual tooltip, or text box, when that text
     > conversion can be built programmatically."*
     > ⛔ **But the BLOCKS STAY: *"the blocks are different sources put together"*.** A block is a COMPOSITION —
     > one heading over contributions from several distinct sources (building, civic, trait all feeding one
     > happiness block) — so which sources compose it, in what order, under which TXT_KEY heading, is genuinely
     > the text manager's job. ⛔ What must never be hand-built is every SUB-BLOCK — one `appendEntryLines` call
     > per (source, family), and a block simply issues several.
     > ⚑ The test: `getText` around a MAGNITUDE is a hand-built sub-block and wrong; `getText` for a HEADING or
     > choosing which sources belong together is the block, and right.
     >
     > **⛔ A BREAKDOWN ITEMISES WHAT THE OBJECT HAS, AND NOTHING ELSE.** It lists sources DELIVERING a
     > realized value — never a candidate that WOULD deliver one; the two read identically once rendered, so a
     > panel carrying both is unusable, not richer. ⚠ A separator does not rescue it, nor an option defaulting on.
     > ⚑ The discriminator is the ENABLER STATE the line was selected by: a source the object HOLDS belongs in
     > the breakdown; anything off the frontier (`STATE_LISTED`) is a WHAT-IF, for the valuation surface
     > answering *"what do I gain from this?"* — never the account of what a city already has.
     > ⛔ A whole-entity "render every family at once" dump is NOT the shape — it flattens the composition the
     > blocks exist to express, which is why the surface is per-family.
     >
     > **⛔ THE ACCEPTANCE TEST ON A COMPOSER IS *DOES IT STILL READ A LEGACY GETTER*, NEVER *DOES IT READ
     > NICELY*: "we just want to make sure that we don't rely on legacy, and have legacy purged, when
     > creating tooltips."** A composer rendering identically but still reaching a legacy accessor is NOT done;
     > one whose wording changed but whose legacy reads are gone IS
     > ([legacy must fail loud, never mask a cascade gap](../../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)). ⚑ A conversion DELETES rather than ports:
     > each converted band removes its legacy accessor call with it. ⛔ Altered visible text is never a reason
     > to hesitate — say what changed and move on.
     >
     > **⚖ TOOLTIPS ARE NOW THE INSTRUMENT — FEATURE-COMPLETE, AND THEY MUST LOOK RIGHT.** The work is far
     > enough along that tooltips are highly relevant: they must look right and carry ALL the data, because that
     > is how the final missing pieces get found. A tooltip carrying every term of a value is a DECOMPOSITION CENSUS in
     > UI form, reached through the screen instead of the wire — an incomplete tooltip does not merely look
     > sparse, it **hides the gap it exists to reveal**.
     > ⛔ The acceptance test is EVERY FAMILY THE ENTITY CARRIES RENDERS, not the subset a composer happens to
     > have been converted onto. ⚑ MOVEMENT is the named exemplar (especially with things like
     > movement, it's easy to spot when it's wrong) — a player checks it against the unit in front of them, so
     > it is the canary for the rest.
     > ⚠ The composer that renders NOTHING is the one to hunt, and it is silent — an empty tooltip logs no line,
     > fails no build, and reads like an entity with nothing to say. Enumerate composers mechanically rather
     > than trusting a screen looks populated.
     >
     > ⛔ **The retired ruling, recorded because its ABSENCE will look like drift:** tooltips were once
     > explicitly end-stage — *"how tooltips are rendered is fairly irrelevant right now, these are bugs we
     > catch at the end"*. That was an ANTI-RATHOLE GUARD, not a judgement of value: written when *"everything
     > was fucked"* and agents *"consistently and constantly tried to 'keep the game running' despite it not
     > even compiling"*. A red tree renders nothing, so attention spent there was unrecoverable. ⇒ The guard was
     > scoped to a CONDITION that is over — the tree builds, so the work it deferred is now the work.
     > ⚠ What does NOT come back: legacy PARITY — the tooltip SET stays demand-driven, decided from community
     > requests and playtests, so a legacy line a cut removed is
     > not a regression. Completeness is measured against what the ENTITY CARRIES, never what the legacy
     > composer used to print.
     >
     > **⚖ THE DEMAND ORDER, MEASURED BY USE — worker · combat · plot · building · unit.** That is the
     > order they are hovered in, so it is the order they are worked in; COMBAT is the standardized exemplar, so
     > live work is worker → plot → building → unit. ⚑ WORKER is two composers, not one: what a worker CAN do
     > here (`CvDLLWidgetData::parseActionHelp`) and what it IS doing (`CvGameTextMgr::setUnitHelp`'s instance
     > form).
     >
     > **⚖ A TOOLTIP IS AN ORDERED SET OF BLOCKS, AND THE BLOCKS ARE THE DESIGN.** Tooltips are designed around
     > the concept of blocks. A composer's deliverable is a BLOCK LIST; per block there are exactly
     > three decisions — which sources compose it, what heading it sits under, and WHEN IT SHOWS.
     > ⛔ The content is NOT the concern (most of the info should be able to be programmatically
     > generated — it is the final structuring of the tooltips themselves, and when they show, that is the
     > trick) — `appendEntryLines` needs no design; a pass spent generating lines worked the half that was
     > never the problem.
     > ⚠ A run-on comma-separated line is NOT a block structure, however complete its content. ⛔ A SHOW
     > CONDITION is a design call, asked never inferred.
     >
     > **⚖ THE DLL DOES NOT CONVERT FOR DISPLAY — THE CONSUMER CONVERTS ITSELF (let python convert
     > themselves).** A composer doing `(float)value / 100 / denominator` to print `%.2f` is the DLL doing the
     > presentation layer's arithmetic, in FLOAT, for a value the engine holds as an integer. ⚠ Not an OOS risk
     > while display-only — exactly why it survives unnoticed — but it is the wrong side of the boundary: remove
     > it as each composer moves, never copy it into a new one.

  ```cpp
  // SECTIONS — whole typed objects
  const CvRequires*  getRequires() const;
  const CvProvides*  getProvides() const;
  // CLASSIFICATION — O(1) bitset, hold-vs-provide in the name
  bool hasAttribute(int attributeId) const;
  bool providesCapability(int capabilityId) const;
  // MODIFIER GROUPS — straight compiled reads, the conditioned list, the what-if valuation
  int getDefense(DefenseKind eKind, ScopeKind eScope) const;      // one load, 0 calculation
  const CvModEntries& defenseConditioned() const;                 // prebuilt trees; walked at bounded cadences
  void expectedFlatYields(const CityContext& cityContext, const EmpireContext& empireContext,
                          const CvPlotGroup* plotGroup, int (&flatYields)[NUM_YIELD_TYPES]) const;
  // INTRINSIC — bare typed reads (×100 where a magnitude)
  int getAirlift() const;
  ```

  **A point getter reads the LOAD-COMPILED sum and nothing else** — compiled once from the anatomy by the one
  compile pass, never runtime-summed, never a second hand-maintained store beside it. And no read anywhere on
  the surface does a per-call string address, map walk, or channel resolution
  ([materialize at mapFrom](07-materialize-at-mapfrom-no-runtime.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)).
- **THE SINGLE-THREAD BUDGET — why this shape is efficient on the one game thread.** The layering is the
  efficiency: (1) repeated hot reads (a BUILT thing's realized value) hit the package caches on the game objects
  ([cascade.md](../../cascade.md)) — O(1) bare fetches, never an info read; (2) **the anatomy
  walk is LOAD-ONLY** — every runtime ask is a straight fetch of a compiled structure: the point reads over the
  compiled sums, the edge-family lists, the enabler's maintained frontier vectors — **0 calculation on the
  straight asks**; (3) the ONLY thing ever evaluated is the compiled CONDITIONED tail — condition evaluation is
  irreducible (the answer depends on the asking city) and runs at exactly two bounded cadences: event-driven
  package rebuild (EVENT volume), and the per-decision `expected*` read, bounded by **frontier × cities**
  ([enabler.md §6](../../specs/enabler.md)), never database × cities; (4) every evaluator predicate is an O(1)
  CONTEXT fetch (`plotAttrs` counts, the `policies` union, the operating set) — a predicate that walks
  plots/units per call is the efficiency defect to reject in review. **Consumer call discipline:** `expected*` is
  a per-DECISION read — once per (city, candidate) per pass; an AI needing repeated score access caches its OWN
  scores (the sanctioned AI-heuristic residual, [superseded-ideas #1](../superseded-ideas.md)) — it never re-asks the
  what-if in an inner loop. A regression in any of this surfaces where every performance regression surfaces — the
  per-turn wall clock ([turn time is king](../../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).
- **Every AMOUNT getter is ×100 native; a PERCENT is never scaled, and there is no `getX`/`getX100` pair**
  ([the ×100 fixed-point model](../../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries) —
  [fixed-point-and-scales.md](../../specs/curators/fixed-point-and-scales.md) has the full boundary/unit rules and
  the silent-failure tell of a `÷100` landing on a percent slot). The name says the VALUE, never the scale; the
  flat-vs-modifier split lives in the member name (`getFlatYield` vs `getYieldModifier`), never a scale suffix.
- **⚑ A LEGACY `Global*` / `Area*` / `National*` PREFIX IS A SCOPE FRAGMENT — its successor is the SAME kind read
  with a scope ARGUMENT.** This is the single most common disposition in the compiler census, and reading it as a
  missing member instead sends an agent looking for a getter that was never meant to come back:
  `getGlobalYieldModifier` → `getYieldModifier(eYield, CASC_SCOPE_EMPIRE)`, `getCommerceChange` →
  `getFlatCommerce(eCommerce, CASC_SCOPE_CITY)`, `getGlobalFreeSpecialist` / `getAreaFreeSpecialist` →
  `getFreeSpecialistsAny(CASC_SCOPE_EMPIRE)` (area authors at EMPIRE — a landmass is not a scope,
  [cascade.md](../../cascade.md)). The name lost the fragment because scope became an axis
  ([scope is a separate axis, never folded into the kind](#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king)); nothing was removed.
  ⚠ Confirm the KIND enum at the call site rather than pattern-matching the name — the prefix tells you the
  SCOPE, never which kind the value is, and a wrong kind compiles clean and reports a plausible wrong number.
- **Extensible by DATA, not by new members/getters.** A new scalar family is a new `m_scalars` enum entry; a new
  property is a new id in `m_properties`; a new attribute is a new bitset key. The getter surface does not grow.
- Intrinsic self-description (`getAirlift`, `getMaxStartEra`, the shrine/corpHQ FKs, flavours) stays a bare typed
  read — genuine lone values, not a groupable set. The ~300 hand-named getters mirroring the legacy `CvXInfo`
  contract collapse into this surface, and consumers rewire onto it
  ([build a new getter surface, never widen a legacy one](05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)) — the info half of the access surface.

