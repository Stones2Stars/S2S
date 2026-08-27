# 7. Intrinsic

> Part of the **[json](../json.md)** spec.

Empire-agnostic self-description. Read directly — never summed or cascaded.

- **`identity`** — "what am I": all **TEXT** (`description`, `help`, `civilopedia`, `message`, `quote`, `strategy`,
  `adjective`, `shortDescription`), display/pedia placement, and metadata ABOUT the entity that produces nothing on
  its own (`conquestProbability`, `mapCategories`, AI worth).
  ⛔ **`identity` is STRICTLY self-description — NEVER a catch-all**: a datum that isn't "what am I"
  (e.g. per-religion spread strength) does NOT go here; it gets its own block (`spread`, §9). Reaching for `identity`
  because a value has no obvious home is the anti-pattern.
  > **⚖ `identity.pediaCategory` IS THE PEDIA-PLACEMENT MEMBER** — the concrete form of the
  > "display/pedia placement" clause above: which pedia bucket an entity is listed in. It passes identity's own
  > test outright, producing nothing on its own.
  > **⚑ AND UNIVERSALITY IS WHY IT IS ON IDENTITY AT ALL: that is why it lives on identity: it applies to all of them.** That is the discriminator to reuse, not a fact about this one field: a datum
  > every entity KIND carries belongs on the shared identity plane, while one only some kinds carry belongs to
  > their own block (a unit's `skills`, a building's `attributes`, a plot substrate's `characteristics` — §8).
  > ⇒ Pedia placement is asked of buildings, units, techs, promotions and the rest identically, so putting it on
  > any per-type block would mint the same member N times and leave each type free to drift on its meaning.
  > ⛔ **THE POINT IS THAT THE TAXONOMY BECOMES DATA — a consumer must never RE-DERIVE a category.** The pedia
  > classified buildings in Python from seven legacy per-field getters plus, for three buckets, a **substring
  > match on the localized DISPLAY NAME** (`"Folklore -"`, `"Enclosure -"`, `"Remains -"`) — a taxonomy built out
  > of prose, silently wrong in every non-English localization. Publishing those getters so the classifier
  > resolves would preserve it exactly ([build a new getter surface, never widen a legacy one](../../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers):
  > a walk that compiles against the new surface while doing what it always did is the half-migration).
  > ⚑ **The CURATOR derives the value once — CRAZY → curator, offline** (the [modifier.md §4](../../cascade.md) trait
  > precedent), and most of it falls out of data that already exists rather than being authored: world/national
  > wonder from **which self-cap the entity authors** ([enabler.md §4](../enabler.md) — the category IS the cap's
  > scope, "never from an `isWorldWonder` mirror"), the system-placed buckets from `notConstructible`/`autoBuild`
  > (§7), the off-world bucket from `mapCategories`. What genuinely needs authoring is only what no existing
  > datum expresses — the name-matched group above.
  > ⚠ **Absent means the ORDINARY bucket**, never a special case to encode; the pedia's sub-category (era
  > chronology) stays derived from the entity's own era and is not a second field.
  ⚖ **The worked case that PASSES — a unit's `domain`.** Where a unit operates is empire-agnostic
  self-description that produces nothing on its own, so it is a genuine identity member rather than a value
  parked there for want of a home. ⛔ It is deliberately NOT a [tag](../tags.md): a tag says what a unit IS, a
  domain says where it OPERATES, and answering the second from the tag set means filtering every tag for what
  one field holds. It is exclusive (no unit has two), and crossing a domain is a SKILL — a helicopter is a land
  unit with `canMoveAllTerrain` ([skills.md](../skills.md)), never an air one.
  ⚖ **A DOMAIN IS THE MEDIUM, NOT THE PLACE.** LAND is any solid surface — Earth, Mars, the Moon — so a
  planetary surface is categorised as land wherever it orbits, and infantry standing on it are LAND units.
  SPACE is the actual VOID, so only spaceships operate there (and they move over land AND space, the ordinary
  cross-domain SKILL shape above). ⛔ So "it is in space" is never a reason to give a surface unit a different
  domain, and a space domain is a question about SPACECRAFT, never about where a foot soldier is deployed.
  ⛔ **AND IT CARRIES NO EFFECTS AT ALL.** Not "few", not "only intrinsic ones" — **none**. A value that
  DOES something has a home already and `identity` is never it:
  - a **held boolean ability** is a classification block — unit `skills`, building `attributes`, empire
    `capabilities` (§8). *`nukeImmune` is the worked case: a BUILDING authors it in `amenities` (it makes the CITY
    immune — the building is not the thing protected) and a plot substrate in `characteristics` (the FEATURE
    itself survives the blast). One word, two mechanics, different carriers — which is exactly why the blocks
    are distinct, and why re-homing must never merge them.*
  - a **magnitude** is a modifier family (§6) — a radius, a movement cost, a sight range, a cargo amount.
  - a **constraint on what may exist or be built** is `requires` / `allowed` (§4).
  - a **capability to trade / work / travel on something** is its own root block (`canTrade`, `canTradeOn`,
    `canWorkOn` — [capabilities.md](../capabilities.md)).

  ⚠ An effect authored into `identity` is a data error.
  Two buildability flags: `notConstructible` (excluded from the player production queue; placed by another system)
  and `autoBuild` (the placing system is the band placer: placed once in every city at founding, its
  `requires.operate` toggling active/dormant forever — [enabler.md §3](../enabler.md); a world/team-capped member is
  excluded and needs its own award path); `autoBuild ⊂ notConstructible`.
  ⛔ **A `notConstructible` entity carries NO `requires.build` — placement is UNCONDITIONAL and DORMANCY decides
  everything**. It is placed in every city and its `requires.operate` then makes it active or dormant, the
  uniform band model ([enabler.md §3](../enabler.md)) applied to the whole queue-excluded class. `build` only ever
  greys a QUEUE candidate and is checked once; a queue-excluded entity is never a queue candidate, so a clause left
  in `build` would never be evaluated again. Authoring one is a data error — the curator folds it into `operate`.
  A third holding-scope flag: **`empireLevel`** — the building is held by the PLAYER, once, empire-wide, and is
  never present in any city; an atom naming one implies EMPIRE scope (§3.4 — the tag IS the type's domain).
  Curator-DERIVED from empire-uniformity, never hand-authored; the machine and the membership rule are
  [enabler.md §2](../enabler.md) ([empire-level buildings](../enabler/02-pass-1-generate-the-frontier-the.md#2-pass-1--generate-the-frontier-the-enables-family)).
  **Civilization selectability** lives here too: `playable` / `aiPlayable` (can a human / the AI pick this civ) —
  **load-only metadata, no gameplay relevance** (animals/barbarians/neanderthals are technically civilizations), so
  it is intrinsic self-description, not a `policy`.
- **`cost`** — what it costs to make (`production`, and cost sub-fields).
- **`ui`** — interface art/sound (icons, buttons, movies) · **`world`** — **on-map 3D art**: the `world.art` block
  carries the on-map art **tag ids** — the `ART_DEF_*` **art-define tag** plus the model / texture references it
  spans (art is more than the icon — models and textures too). **Only the tag ids live in JSON**; the art
  *definitions* stay in the ART XML (`CIV4ArtDefines_*`), resolved by `ARTFILEMGR` from the id (a `BUILDING_`/`UNIT_`
  entity keeps `getArtInfo()` = `ARTFILEMGR.get<X>ArtInfo(<the id>)`). `ART_`/`EFFECT_` ids are XML-only Types
  ([naming.md](../naming.md)), *referenced* from here. · **`sound`** — audio assets.
  > **⛔ THE TAG KEY IS `define`, AND AN INFO THAT READS A DIFFERENT ONE FAILS AS A CRASH, NOT AS A MISSING
  > PICTURE.** `getArtInfo()` is `DllExport` and the EXE does **not** null-check it, so an unresolved tag makes
  > `ARTFILEMGR` answer NULL and the EXE dereferences it while reading the art's own path strings — an access
  > violation in the EXE's frame with nothing naming the entity ([the info plane is write-once-at-load](../../architecture/patterns/04-the-info-data-out-contract-what-an/01-write-once-at-load-a-read-never.md#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud):
  > the address is the bait). ⚠ The DLL-side reads around it (`getLeaderHead`, `getButton`) DO null-check, so
  > they degrade quietly to "no art" and hide the fault until the EXE asks.
  > ⚑ **The failure is a silent key mismatch, which no census catches:** the reader accounts every authored key
  > to *some* consumer, so a key one info ignores while its siblings consume it is not an unknown key and not an
  > unconsumed section. Nothing reports it. ⇒ When adding or reviewing a `world.art` read, check the key against
  > what the data actually authors — not against what the surrounding comment says it reads.
  >
  > **⚖ `world.art.notShownInCity` — THE ON-MAP PRESENCE VERDICT IS DATA, DERIVED ONCE BY THE CURATOR.**
  > **~90% of buildings have no model to place** (4,683 of 5,180): they are real game entities carrying a real
  > `<Button>` for the pedia and the build list, whose ART define says *"nothing to draw"* by scaling the model to
  > zero or pointing at the empty model. The flag is emitted only when TRUE, so an absent key means "placeable".
  > ⛔ **The verdict is NOT sniffed out of the art plane at runtime.** Legacy re-derived it per read from
  > `fScale == 0 || NIF == "Art/empty.nif" || no tag`; the curator now answers it once, offline, from those same
  > three conditions, and the info reads a member. ⚑ Two reasons it belongs in the data rather than in a getter:
  > the intent becomes VISIBLE (a zero scale is a marker nobody would recognise as "deliberately invisible"), and
  > the art defines load through `SetGlobalArtDefines`, which is `DllExport` — so their order against the JSON load
  > is the EXE's and is not readable from this tree ([python-load-sequence.md](../../reference/python-load-sequence.md)),
  > which is exactly what a `mapFrom` derivation would have had to assume.
  > ⚠ **The three markers OVERLAP rather than partition** — 1,908 defines carry both scale 0 and the empty model,
  > 2,766 only the scale — so all three are tested and none stands proxy for the others. An UNKNOWN tag counts as
  > not-placeable: `ARTFILEMGR` answers NULL for one, and a NULL define is precisely what must not reach the layout.
  > ⚑ **What it costs when it is missing is measurable, not theoretical:** `CvCity::getVisibleBuildings` offers the
  > render engine every building the city holds, so without the flag a late-game save logs **26,273**
  > `is not associated with a CvCityLSystem node` warnings over 260 buildings plus **4,168** `Art/Empty.nif` shadow
  > complaints ([spine.md](../../spine.md) — `LSystem.log`), once per layout rebuild, per city.

  > **⛔ THE ART CARVE-OUT — art is OUT OF SCOPE, leave it alone.** The art defines
  > (`CIV4ArtDefines_*`), their `ART_`/`EFFECT_` tag ids, and the asset files are UNTOUCHED by this data model:
  > JSON carries only the tag id, the definitions stay in the ART XML, and `ARTFILEMGR` keeps resolving them
  > ([naming.md](../naming.md)). This includes **not** cleaning up art that becomes orphaned when a consumer is
  > removed — an unreferenced define is inert, and pruning it is neither a cascade job nor a tidiness licence.
  > Same standing as TXT: an unmigrated system boundary, not a gap
  > ([patterns.md § THE PYTHON READ BOUNDARY](../../architecture/patterns.md)).
  >
  > **⚖ ART IS ART, AND IT STAYS TOGETHER.** A unit's MESH GROUPS were authored as one block with the
  > art tags that name their models (`<UnitMeshGroups>`: `iGroupSize` · `fMaxSpeed` · `fPadTime` ·
  > `iMeleeWaveSize` · `iRangedWaveSize`, then per `<UnitMeshGroup>` an `iRequired` and its **per-era** art
  > tags). They belong in `world.art` **as that one block**, never split between an art half and an
  > "animation numbers" half.
  > ⛔ **THE PER-(ERA, STYLE) GRID IS CARRIED, NOT COLLAPSED (I don't think the exe survives without
  > it).** `getArtInfo(iIndex, eEra, eStyle)` resolves a civilization's UNIT ART STYLE override first
  > (`CvUnitArtStyleTypeInfo`, keyed by unit) and falls back to the unit's own per-era tag. ⚠ Reducing the era
  > or style dimensions is a SEPARATE consolidation pass and unrelated to carrying them — do not slim the grid
  > while restoring it.
  > ⚑ **Why it is load-bearing rather than cosmetic: six of these reads are `DllExport` and the EXE lays out
  > and animates the unit through them** (`getGroupSize` · `getGroupDefinitions` · `getUnitGroupRequired` ·
  > `isRenderAlways` · `getAnimationMaxSpeed` · `getAnimationPadTime`). Answering them with absent values does
  > not degrade to "no art": a max animation speed of **0** is a unit that plays its walk cycle and never
  > translates, and **0** group definitions is a formation with no per-member offsets, so the models stack on
  > one another.
  > ⛔ **AND THE SIX ANSWER THE AUTHORED MESH-GROUP BLOCK *ALONE* — a value from another system is the same
  > defect wearing a plausible name.** The EXE reads a figure COUNT and the per-member OFFSETS across these
  > calls and lays one formation out of them, so the layout is coherent only while all of them come from the one
  > block. ⚑ The trap is a name collision: Size Matters' `groupRank()` also says "group", and
  > `CvUnit::getGroupSize` returned it whenever `GAMEOPTION_COMBAT_SIZE_MATTERS` was on — a merge/size rank
  > summed from the combat classes, whose consumer is `getUnitCountSM`, handed to the renderer as a figure
  > count the art cannot supply. That is the pivot rule in the `sizeMatters` block below failing on the ART
  > plane: **one `DllExport` read meaning two different things depending on a player toggle.**
  > ⚠ It stayed invisible because the whole family answered `0/-1` while the mesh-group data was uncarried —
  > the formation loop never ran, so the override had no consumer. **Carrying the data is what made it
  > reachable**, which is the shape to expect from any stub this family is restored from: the bug is not in the
  > restoration, it is the pre-existing wrong answer the restoration finally delivers to someone.
  > ⛔ It is NOT covered by the ART CARVE-OUT above, which carves out the art DEFINES and asset files — these are
  > the unit's own numbers, authored in the unit XML, that merely reference a define.
  > **⛔ ART IS NOT A DECIMAL — the animation numbers carry NO fixed-point scale.**
  > [the ×100 fixed-point model](../curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries) governs an AMOUNT, i.e. a magnitude
  > the cascade carries and combines; `maxSpeed` and `padTime` are neither — they are handed to the EXE in its
  > own animation units and never enter a calculation, a package or a synced decision. So they are read as
  > authored (`1.75` is `1.75`), and a ×100 on them would be a scale invented for a plane that has none.
  > ⚠ The ×100 reflex is what makes this worth stating: a value with a decimal point LOOKS like the two-decimal
  > case the scaling exists for, and the tell that it is not is that nothing ever sums or modifies it.
  > ⚑ **The grid is what `isRanged()` reads, so this is a COMBAT surface too, not only a visual one.**
  > `CvUnit::isRanged` walks `getGroupDefinitions()` asking each group's art `getActAsRanged()`, so a collapsed
  > grid returned **0 groups and the loop answered TRUE for every unit in the game** — first-strike ordering in
  > `updateCombat` reads it at ~10 sites. Carried, it is true for 606 units and false for 1,467.
- **`ai`** — AI-only metadata (flavours, weights, personality); never affects rules, only AI behaviour.

---

