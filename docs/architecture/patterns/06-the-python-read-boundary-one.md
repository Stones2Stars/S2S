# ⛔ THE PYTHON READ BOUNDARY — ONE COMPLETE DATA-FETCHING LIBRARY

> Part of the **[patterns](../patterns.md)** spec.

**⚖ THIS IS THE TARGET, NOT TODAY'S ACCEPTANCE BAR.** The rules below cannot be followed to the letter yet; the
Python layer is a mess and its reorganization is a separate independent pass. An agent measuring current code
against these rules will read ordinary sanctioned work as violation.

⇒ **What binds NOW is deliberately a low bar: serve the read the call site needs, NAME it, and put it somewhere a
later pass can move cheaply.** Homing, the per-type accessor split and the import conversion are that separate
pass, taken when the demand map is known — never as a rider on a repair.

⛔ **It is NOT licence to skip a fix.** The mutation surface is published and live
(`set*`/`change*`/`do*`/`create*`/`push*` across `CvPythonPlayerLoader`/`CvPythonPlotLoader`/`CyGame`/`CyTeam`/
`CyMap`/`CyArea`/`CyAct`) — the cut was directional and took READS only. "Not organized yet" never justifies
leaving a broken handler broken. ⛔ Equally it is not licence to call the current shape correct or to ADD to the
disorder: the point of groundwork is that the later pass stays MECHANICAL, and every unnamed read added meanwhile
is what stops it being mechanical.
⚠ This is a SEQUENCING ruling with a named end state, so ["deferred" is banned](../../../AGENTS.md#design) does
not reach it — same standing as the golden-age / anarchy carve-out ([state.md](../../specs/state.md)).

**This is a REBUILD, not an invention.** Python has always fetched through a binding layer and that MECHANISM
(boost::python) stays. What is wrong is the SHAPE — scattered per-type interfaces, one getter per legacy field, no
coherent payload anywhere. ⛔ "It kind of exists already" is never licence to widen a `Cy*` binding.

## ⚖ The kill is a FORCING FUNCTION, not tidiness

A live `Cy` surface is an ESCAPE HATCH: while it answers, the cheap move is to bend the new design so Python keeps
working, and the result is consistent nowhere. The hard kill removes the option, and consistency is what that buys.

⛔ **So a good-sounding reason to spare one binding is the failure, every time.** "Python still calls it", "cutting
it breaks a screen", "wait until the replacement lands" — each is the shortcut wearing caution.
⚠ Nothing requires the kill to be atomic; piecemeal cutting is fine. What is banned is bending anything to keep the
old surface functional.

⚑ **AND CUTTING WRONG IS CHEAP.** A binding that turns out to be needed comes back as the NEW surface serving that
read, never as the old `.def` restored — so a cut converts an assumed dependency into a named one.
⚑ The cheapness is structural: Python takes the surface with `from CvPythonExtensions import *` — **169 files
star-import it against 3 with an explicit list** — so nothing declares a dependency on any single binding. A removed
`.def` causes no import-time failure; it surfaces at the one call site that used it. ⛔ So do not slow a cut to
protect a binding, and do not build a resolver to prove one safe first.

## The four words

- **ONE SURFACE.** A single library IS the Python-facing read boundary — never two live surfaces for one read.

  ⛔ **"ONE SURFACE" MEANS ONE LIBRARY, NOT ONE CLASS.** The word bans a SECOND live answer for one read; it says
  nothing about how many accessors the library is composed of. A flat class accumulating every type's reads behind
  an `(owner, id)` address satisfies the word and violates the design — which is how the mishomed `CyState` got
  built. ⚠ Read it as ONE LIBRARY, COHERENTLY HOMED: a game object's data lives on that object's accessor, and the
  library is one because there is no second place to ask.

  ⛔ **THE FLAT STATE CLASS IS BEING DISSOLVED, NOT TRIMMED.** An address-keyed flat class makes every call site say
  WHICH object it means and never WHAT KIND of thing it asks, so it reproduces both failures this boundary exists to
  end — getter spaghetti, and unreadable provenance — while looking organized because the endpoints are named.
  ⇒ Each type's re-home is one pass of that dissolution, and what is left behind is the NEXT pass, never a
  sanctioned residue. ⛔ Do not add a read to the flat class because a neighbour is still there.

- **COMPLETE.** The END STATE is that every read Python performs has an answer here, so no gap forces a reach-around
  into legacy — that reach-around IS the second live surface.
  ⛔ **But completeness is the DESTINATION, never a GATE ON CUTTING.** A dead legacy binding is an OUTLAW, shot on
  sight; reading this as "cut only once the library is complete" inverts the ruling into a shield for the surface
  being removed. The cut is the forcing function that DRIVES completeness, so it never waits on it — and when the
  library cannot answer a read, ADD the read, never borrow legacy meanwhile.

- **DATA FETCHING, not gameplay.** It serves reads/payloads; Python-authoritative gameplay (Revolution, events)
  stays Python and becomes a CONSUMER.

  ⚖ **THE `Cy*` LAYER IS THE CONTROLLER AND STAYS THIN — BUT THE INTERNAL→EXTERNAL CONVERSION IS ITS JOB.** Engine
  is the model, `Cy*` the controller, Python the view. Thin means **no logic**: no computation, no policy, no
  aggregation the model does not already answer — a controller that starts deciding is a second engine, and it will
  disagree with the first.
  ⛔ **What is NOT a violation of thin is REPRESENTATION.** Where an internal form becomes an external one, this is
  the only place it should happen: the ×100 fixed point reduces here
  ([the ×100 fixed-point model](../../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)),
  an enum-indexed group becomes a list, a handle becomes an address. ⚑ Pushing that conversion OUTWARD forces every
  consumer to know the engine's internal scale, then to disagree about it.
  ⚖ Because nothing downstream does deterministic math, an external getter may hand out a FLOAT rather than
  truncating: the two decimals survive the boundary instead of being thrown away at it.

- **⛔ ENUM OPERATIONS ARE FIRST CLASS** — name→type resolution is supported, covering **resolution AND EXTENSION**:
  BUG resolves `WidgetTypes`/`InputTypes`/`InterfaceDirtyBits` by name from config strings *and* MINTS new
  `WidgetTypes` members at runtime. It generalizes `getInfoTypeForString` and mirrors the load-minted classification
  registries ([the classification-infos registry](../../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)).
  ⚑ **THE ENUM VOCABULARY IS A PREREQUISITE OF THE READ SURFACE.** Group reads are specified as
  `getYields()[YieldTypes.YIELD_FOOD]`, so until the enum TYPES are published the replacement cannot be consumed at
  all.
  ⛔ **Publishing them is NOT a survival of the banned surface** — the ban is on the `.def` GETTER contract, and a
  publication carrying zero `.def` and zero `class_` is CONSTANTS, not reads. ⚠ Worth stating because the enum
  publication was once swept up in the binding purge, which takes the whole Python layer down: every module names an
  engine constant.
  ⚑ **EXTENSION needs no API of its own.** A published boost enum is a real Python type, so BUG's existing
  construct-from-int + `setattr` adds members the moment the type exists.
  ⚠ **Only TWO of the three are ours** — `WidgetTypes` and `InterfaceDirtyBits` are in `CvEnums.h`; `InputTypes` is
  the EXE's. A name absent from `CvEnums.h` is the EXE's to serve, never a hole in the library.

## ⛔⛔ `import *` is the real enemy — it outranks every other concern here

Named endpoints are never a problem; endpoint HOMING is real but secondary. **Imports must be named so a reader can
see what is being fetched.**

⛔ **EXPLICIT IMPORTS, ALWAYS.** ⚑ What was wrong with the old surface was the **GC COUPLING and the OPACITY, not
per-info accessors**: `GC.getBuildingInfo(i).getFoo()` declares nothing — a reader cannot tell from the module which
registries it touches, and the god object hands out everything.
⇒ **A PER-INFO accessor, explicitly bound at module scope, is the WANTED shape** — the bindings list IS the module's
dependency list. ⛔ What stays banned is a different axis: the ~300 hand-named getters mirroring the legacy per-FIELD
contract ([build a new getter surface, never widen a legacy one](05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)).
⚠ **An opaque SLOT enum re-creates the fault it was meant to cure.**
`INFO.getIntrinsic("WORLD_", id, PYINT_CORP_MAINT_PERCENT)` is decoupled from `GC` and still fails — the call site
names a slot rather than the thing. ⇒ Reserve the generic prefix-addressed plane for what is genuinely UNIFORM
across every registry (identity text, edge families); a value belonging to ONE type is named on that type's accessor.

**⚖ ENDPOINT COUNT IS EXPLICITLY NOT THE TARGET — PROPERLY ORGANIZED IS.** What binds is HOMING: an endpoint belongs
on the accessor for the type it addresses. A flat class holding UNIT, BUILDING and HANDICAP reads side by side is
spaghetti wearing named endpoints. N understandable endpoints beat one parameterized endpoint hiding N meanings.

⇒ So a consumer asks **`INFO.isHiddenNationality(unitId)`**, never a parameterized test carrying an id.
⛔ **This settles how Python names a classification id by REMOVING it: Python never names one.** Both former
precedents are retired — the generated-enum form (`hasSkill(prefix, id, CLS_SKILL_HIDDEN_NATIONALITY)`) and the
authored-key-string form (`hasSkill(prefix, id, "hiddenNationality")`) are the opaque-slot shape in different
costumes. ⚠ The C++ side is the exact OPPOSITE and correctly so: there the id IS a compile-time constant, so
`hasSkill(CLS_SKILL_BLITZ)` names the thing at the call site. Python has no such constant, which is why the two
planes diverge rather than one being wrong.
⛔ The parameterized `CyInfo::hasSkill`/`hasTag` are not the consumer surface; a named endpoint is added **on demand,
for the call site that wants it**, never pre-emptively across a registry.

**⛔ THE FAILURE ORGANIZATION PREVENTS IS DUPLICATION** — three similarly-named endpoints doing the same thing
because the previous modder did not know where to look. The test on a new endpoint is never *how many are there*, it
is **could someone find the one that already answers this?** An unfindable endpoint is re-minted under a
near-synonym, and the surface carries three spellings of one question that drift apart.
⇒ **LOOK BEFORE YOU ADD.** Read the accessor for the type you are about to serve; a near-synonym you did not find is
the defect you are about to file.
⚑ **It is [the DRY single-implementation law](03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)
on the boundary** — a helper the next consumer cannot see is one they will reimplement, and the mechanism is
identical whether the thing reimplemented is a calculator or a read. ⚠ The worked case is C++ and more convincing
for it: `modSegmentCached` existed as THREE file-static copies, each written because its author could not see the
others. Nobody decided to duplicate it; the shape did.
⚠ A genuine near-pair must SAY why it is two — `isShrine` (29 buildings) beside `isReligiousBuilding` (213) is two
questions, and the comment carries the discriminator so the next reader does not "consolidate" them or mint a third.

**⚖ THE ORGANIZING PASS IS SCHEDULED, AND THE CURRENT PILE IS ACKNOWLEDGED DEBT.** ⇒ Keep ADDING named reads
wherever a call site demands one; homing is corrected wholesale later, never negotiated per endpoint. ⛔ Not licence
to call the flat pile correct, nor to withhold a read a call site needs meanwhile.

**⚖ THE REASON IS TRACING, and it is the point the other rules serve.** The question a reader must be able to answer
is **where does this come from**, and today they cannot. ⛔ **The root is GLOBAL imports and IMPLICIT imports** — two
failures wearing one symptom, and only the first is the star import:
- a **GLOBAL/star import** leaves the name but erases its ORIGIN — `CyInfo` could come from anywhere;
- an **IMPLICIT import** was never written in Python at all — the config/XML-bound dispatch (BUG
  `lookupModule`/`lookupFunction`, the `<PythonCallback>` family;
  [python-read-map.md](../../reference/python-read-map.md) §5.3/§5.4), which no grep of the Python can see.
⚑ That second one is why *"just read the code"* fails here: a read found is a read to SERVE, and a read not found is
never evidence of absence.
⚑ **The standard is the ordinary one** — `from CvPythonExtensions import CyInfo, CyState, CyVictoryInfo` — so the
import block IS the dependency list.

**⚖ BUT THE SEQUENCING IS DISCOVERY-FIRST.** The expensive work is finding every read and homing it on the right
surface; re-pointing the layer afterwards is mechanical. ⛔ So converting imports AHEAD of the demand map is the
failure — it is then done twice, and the second pass is the expensive one. ⚠ Equally not licence to call the star
import acceptable: it is a real defect with a scheduled fix, not a sanctioned shape.

⚑ **The corroboration — the espionage advisor crash.** `INFO.getIntrinsic("ESPIONAGEMISSION_", i, PYINT_COST)` names
a SLOT, and `PYINT_COST` was wired for `BUILDING_` only. The unwired prefix fell through to the shared `-1`,
indistinguishable from a real answer, so every mission failed its guard and a `-1` mission id reached the engine: an
ACCESS_VIOLATION inside a boost::python call, in a different screen from the read that was wrong. **A named accessor
cannot fail that way — an unwired read does not compile.**

## ⚑ Build it for the pedia — but know what that proves

The pedia displays every entity exhaustively, so it is not a sample of the info surface — it **is** the info surface
rendered.

- **SHAPE — complete by construction.** Nothing in Python needs a payload shape the pedia does not already force, so
  serving the pedia SETTLES the library's structure; no later consumer introduces a new kind of read.
- **⚠ COVERAGE — NOT proven, and the gap is enumerable.** The pedia is ~99.7% a static reader: a fraction of STATE,
  almost no COMPUTED, no MUTATION. The residue is an appendix — whole info types with no pedia page (map-gen,
  game-config, diplomacy/victory/vote, command/UI-action) plus per-field reads. **Serving the pedia completes the
  INFO plane and the shapes; it does not complete the boundary.** Treating it as a coverage oracle is the mistake.

**⛔ WHAT IS ACTUALLY WRONG WITH THE OLD SURFACE IS THE LOOPING, NOT THE READS.**
- **A TEXT or NAMING read is CHEAP and is simply SERVED.** An entity's authored identity text — description, help,
  civilopedia, strategy, adjective, short description — is content, not a legacy getter contract. ⛔ Do not ration it
  and do not file one as "per-type tail" merely because it is absent from `CvInfoBase`: a civilization's NAME, SHORT
  name and ADJECTIVE are three different texts and the dynamic naming composes from them, so collapsing them
  destroys content. Their `uiForm` argument is carried through — it selects the grammatical variant localization
  needs.
- **A WHOLE-REGISTRY LOOP is the actual defect.** Sweeping every id to ask a per-id predicate re-derives what the
  entity already carries ([reverse lookups are populated once, at load](../../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1)),
  and the per-id reads it walks are the ones being deleted. It converts to the maintained set, the entity's own
  compiled entries, or its reverse edge families — never to a faster per-id getter, which leaves the loop doing
  exactly what it did before while reading as migrated.

**⚖ THE PEDIA IS THE ONE PLACE A FULL SCAN IS UNAVOIDABLE, AND IT IS NOT A DEFECT** — enumerating a registry to
display every entity IS its job, so those loops STAY. ⚑ What changes is the COST, not the shape: an enumeration
crossing the boundary once per entity becomes ONE crossing via the per-type index read, since a `boost::python` call
costs far more than the lookup inside it. ⛔ The carve-out is for ENUMERATION only — a pedia page walking a DIFFERENT
registry to find "what needs me" is a cross-link, answered by the load-time reverse families
([pedia-read-map.md](../../reference/pedia-read-map.md) finding 2).

## ⛔ The cut is directional — only the READ surface dies

- **Python → engine READS** (the `Cy*` info/state bindings) — replaced by the library; the binding surface is GONE.
- **Engine → Python CALLBACKS** — **NEEDED, and kept.** `CvEventReporter`, the map-script hooks and `CvOutcome`'s
  Python outcomes are what make Python-authoritative gameplay possible at all, so this is REQUIRED FUNCTIONALITY,
  not a deferral. ⚠ The list is open: treat a callback you find as kept unless ruled otherwise.
  ⚖ **Kept, but the successor is named:** `CvEventReporter` is replaced by the TRIGGERS machine and events move INTO
  C++ ([triggers.md](../../specs/triggers.md)) when that work item is taken. So this is a scope boundary with a
  known destination — do not read the event surface as "Python owns this forever".
- ⚑ Consequence: the `Cy*` WRAPPER classes (`CyCity`/`CyUnit`/`CyPlayer`/…) STAY while the legacy per-field binding
  contract does not — 33 engine files hold them for that direction. Reading that as a half-cut to complete would
  delete working gameplay.

### ⛔ The identity set is the FLOOR, not the ceiling

The `Cy*` bindings are the literal API surface for Python, so a type publishes the GET/PUT/POST it is required to.
The identity set is what a handle must ALWAYS carry so a consumer can name its object; it was never a cap on what
the accessor answers.
⚑ **The tree settles it** — `CyPlayer` publishes 332 endpoints, `CyCity` 157 (the coherent group reads `getYields`,
`getCommerces`, `getWellbeing`, `getScalars`, `getDefenseKinds`, … plus mutators), `CyTeam` 116, `CyPlot` 106. That
IS the per-type accessor prescribed below, already built.
⇒ So an UNDER-PUBLISHED wrapper is an UN-RE-HOMED TYPE, never a finished one: `CyUnit` at 8 endpoints against 58
legacy declarations its Python still calls is work outstanding. The burndown is countable —
`python Tools/verify-python-bindings.py`.
⚠ "Under-published" is about COVERAGE, not depth — a controller stays THIN (no logic) however many endpoints it
carries. The two are independent.

**⚖ EVERY HANDLE PUBLISHES OWNER + ID + POSITION.** It is the ADDRESS: what a consumer needs to say WHICH object it
holds. The ban on the legacy info/state GETTER contract is untouched — what a handle must never become is the old
per-field surface restored wholesale.

**⛔ BUT A GAME OBJECT'S OWN DATA IS READ FROM ITS OWN ACCESSOR — `CyCity`, NEVER A STATE CLASS KEYED BY ADDRESS.** A
city's population, name, maintenance and food are the CITY's data, so they are asked of the city.
⛔ **THE TEST IS THE METHOD NAME, and it is mechanical: the moment you have `getAnotherObjectSomething`, we have
failed.** `CyState::getCityPopulation(owner, id)` is `get<ANOTHER OBJECT><Something>`; `CyCity::getPopulation()`
names only what the receiver already is. ⚑ The prefix is the tell precisely because it exists to disambiguate a
receiver that should never have held the read: an accessor that owns its subject needs no noun in its verbs.
⇒ So the per-type accessor ruling is the SAME rule stated from the naming side, and the two are checkable against
each other: if the name needs the noun, the endpoint is on the wrong class.

**⚖ THE HANDLE CHAIN IS THE POINT, NOT A COST — IT SHOWS THE HIERARCHY.** A caller resolves the object and then asks
it (`PLAYER.getCity(id).getPopulation()`), and that chain STATES where the value comes from: a city read is reached
THROUGH the player that owns it, so containment is visible at the call site instead of being flattened into an
`(owner, id)` pair. ⛔ The resolve step is not a two-hop to optimize away — it is the provenance the flat
address-keyed class destroyed.
⚑ **Consequence: publishing the ADDRESS→HANDLE path is part of homing the reads, never a separate favour.** An
accessor nobody can obtain serves nothing, and an event payload hands over the identity PAIR rather than a handle.

**⛔ A LEGACY DECLARATION IS KILL-ON-SIGHT — THE `.def` IS NOT THE ONLY OUTLAW.** An unpublished legacy method on a
wrapper is the per-field contract still written down, so the next agent reads it as the surface, a re-homed read
COLLIDES with it, and "just publish what is already declared" looks like the cheap fix at exactly the moment the new
surface arrives. ⇒ The declaration AND its body go the moment they are seen
([leave no evidence of the abandoned path](../../../AGENTS.md#design)).
⚑ **So a re-home does not have a collision PROBLEM — the collision IS the work.** The legacy name dies as the
coherent read takes its place.

**⚖ AND THE COUNTERWEIGHT, WITHOUT WHICH KILL-ON-SIGHT UNDER-SERVES: WE ARE NOT STINGY.** We do not follow legacy
declarations because they are "already used somewhere", which most no longer are. The surface is designed from
DEMAND and freely given where a consumer genuinely needs a read; what it is never derived from is the legacy list.
⇒ **The two rules are one move, and each fails alone:** killing without serving pushes the next consumer back onto
legacy ([legacy must fail loud, never mask a cascade gap](../../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)),
and serving by preserving the legacy set re-creates the per-field contract.
⛔ So "something might still call it" is NOT a reason to keep a declaration — verify the demand, and where a read is
genuinely wanted, ADD it as the coherent read.
⚠ **What is NOT killed with them:** the `class_<>` REGISTRATION, the identity set (the kept engine→Python direction
depends on both), and anything the ENGINE itself calls on the wrapper — the compiler names those.
⚠ **What this does NOT license** is reviving the legacy `.def` field-by-field contract on the handle: what a
game-object accessor carries is the coherent GROUP reads and named concepts of the new surface, homed on their own
object — never the ~300 per-legacy-field getters re-registered because a screen once called them.
⚑ **Each publish lives in the file named for its type** (`CyCity::pythonPublish`, the `CyInfo` pattern), never piled
into the composition root — the numbered-bucket shape (`CyGameCoreInterface1/2/3`) is the disorganization this
avoids.

### ⛔ "No binding" means no `.def` — it does NOT mean no `class_<>`

A `class_<CyX>("CyX")` carrying **zero `.def`s** is not a read surface — it publishes no getter and answers no
question. It is the type IDENTITY that lets an object cross the boundary at all: the marshaller
(`Cy::PyWrap` → `makePythonObject`) is `python::object(pObj)`, which **throws at runtime unless the type has a
registered converter**. So the engine→Python direction depends on the registration exactly as much as on the
callback.
⚑ **The measure is mechanical, not a judgement:** a `Cy*` type is registration-REQUIRED iff any engine call site
passes it — `DECLARE_PY_WRAPPER(CyX, CvX*)` with at least one live `args << pCvX`, or a
`CvGameObject::createPythonWrapper` branch. `CyCity`, `CyUnit` and `CyPlot` each carry dozens of such sites;
`CySelectionGroup` declares the wrapper and has **zero**, so it genuinely needs none.
⚠ **The same defect class reaches every published method whose RETURN type is a `Cv*`/`Cy*` object.** Publishing the
accessor without registering what it returns yields a def that resolves and then raises at conversion: a `TypeError`
where a reader expects an `AttributeError`, which is why it reads as a mystery rather than a missing binding.
⇒ **When cutting a read surface, cut the `.def`s and KEEP the `class_<>` for any type the engine hands across or
hands back.** Deleting a whole registrar file takes both halves, and the second half is not yours to take.

**⚖ THE PLAIN VALUE STRUCTS ARE THE SAME RULE ONE LEVEL DOWN, AND THEIR FIELDS ARE NOT A READ SURFACE.**
`NiPoint3`/`NiPoint2`/`NiColorA`/`POINT`/`IDInfo`/`OrderData`/`MissionData` are the MARSHALLING VOCABULARY, not
handles: a coordinate pair or an RGBA quadruple answers no question about game state, so `def_readwrite` on it is
the VALUE ITSELF and the GETTER-contract ban does not reach it. ⛔ A struct registered without its fields is useless
— Python can neither read the point it was handed nor build the colour it must pass.
⚑ **They fail in BOTH directions, which is why the absence is easy to misread.** Python CONSTRUCTS some
(`NiColorA(0,0,0,0)` for the dot-map overlay) — those raise `NameError` at IMPORT. The engine RETURNS others
(`Win32::getCursorPos` → `POINT`) — those resolve and then throw at CONVERSION, far from the cut. Restore on DEMAND,
named by the call site that wanted it.

**⚖ WHERE A MAP SCRIPT DRAWS THROUGH THE HANDLE, THE OPERATION STAYS ON THE HANDLE** — a named endpoint beside it is
the near-synonym duplication, not the fix. `CvRandom` is the worked case: registered so the handle can cross
(`getMapRand` → the EXE's `shuffleList`), and Python also DRAWS from it — the map scripts alone at dozens of sites
(`CvMapGeneratorUtil`'s `mapRand`). Those are an OPEN EXTENSION POINT whose contract is the named callbacks, so a
third-party script cannot be re-pointed and `get` has to exist on the type regardless.
⛔ **So publishing a tidier `getASyncRandNum` on the config context and re-pointing the in-tree callers is wrong
twice over:** every map script stays broken, and it creates a second spelling of one job. ⚑ The test: **can every
caller be re-pointed?** If a map script or any other open extension point is among them, the answer is no and the
operation belongs on the type.
⚠ It does reach the SYNCHRONIZED stream (`getSorenRand` hands that one across), but `getSorenRandNum` is already
published, so restoring the draw adds a SPELLING and not a POWER. What binds is where a given draw belongs: a
cosmetic pick — which greeting variant a leader uses — is `getASyncRand`, because the synced stream's draw COUNT is
shared save state ([the synchronized RNG is shared state](../../reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)).

## ⛔ Three things the library does not own

- **TEXT/localization.** `getText`-style key→string resolution is not info data, and decisively: **TXT and ART keys
  are NOT MIGRATED** — both remain XML-side systems the JSON only REFERENCES
  ([json.md §7](../../specs/json.md); [naming.md](../../specs/naming.md)). The library serves already-RENDERED lines
  and the raw key reference; resolution stays with the existing managers. This is an unmigrated system BOUNDARY, not
  a hole in the library.

  ⛔ **A FONT GLYPH IS TEXT-PLANE, NOT INFO DATA — the case most likely to send a reader after a deleted info
  accessor.** `CvYieldInfo::getChar()` and its kin LOOK like authored data and are not: the glyph is a runtime
  GameFont slot the `CvGameTextMgr` symbol pass assigns via `setChar`, for seven registries (yield · commerce ·
  religion · corporation · property · invisible · bonus) that straddle the JSON/XML line — so it is not info data on
  EITHER side.
  ⛔ **THREE ROUTES SERVE IT, split by whether the registry is FIXED-COUNT**, and a reader who knows only one
  concludes the glyph is unserved. A token is a literal STRING, so a runtime-count registry can only be addressed by
  ID:
  - **`CyGame.getSymbolID(FontSymbols.X)`** — the fixed engine symbols (happy, bullet, strength, …).
  - **`CyTranslator().getText("[ICON_X]", ())`** — the `[ICON_*]` token map (`CvDllTranslator::initializeTags`): the
    fixed symbols, the 3 YIELDS and 4 COMMERCES by name, plus a token built PER ENTITY at load for **property** and
    **invisible** (`[ICON_<TYPE>]`). ⚠ Those two are the only registries whose per-entity token exists.
  - **`CyGameTextMgr().getSymbolChar(prefix, id)`** — the symbol pass's own read, covering the five registries it
    assigns by id: `YIELD_` · `COMMERCE_` · `RELIGION_` · `CORPORATION_` · `BONUS_`. ⛔ **RELIGION, CORPORATION and
    BONUS have NO `[ICON_*]` token of any kind** — they are variable-count, so this is their ONLY route, and
    `[ICON_RELIGION]` is the generic religion symbol rather than a per-religion glyph. It returns an INT, so it
    substitutes for a `getChar()` under an existing `%c` with no format surgery.
  ⚑ **Two registries carry a SECOND, distinct glyph**: a religion's HOLY-CITY marker (`getHolyCitySymbolChar`) and a
  corporation's HEADQUARTERS marker (`getHeadquarterSymbolChar`).

- **REVOLUTION's distance mechanic.** ⚠ `revolution.distanceMod` is **NOT dead** — Revolutions is
  Python-authoritative and consumes it through the player/city aggregates, which makes the read INVISIBLE to any
  engine-side grep. It is the standing exhibit for why an engine-read census cannot prove Python coverage. **Both
  distance kinds STAY AS-IS, untouched by any stage:** Revolutions is due its own rework, and that rework owns every
  revolution-data question.

- **MAP SCRIPTS.** They read map-gen types nothing else reads, run BEFORE most game state exists, are
  WRITE-dominated (they build the map; this is a read surface), and `eval` script-supplied expressions as an open
  extension point. Their contract stays the named Python CALLBACKS ([engine.md](../../reference/engine.md)), so
  third-party scripts are unaffected by the `Cy*` cut and their types leave this library's coverage appendix.
