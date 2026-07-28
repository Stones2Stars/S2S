# Unit-combat → identity-tag mapping (first pass, EDITABLE worklist)

> Sibling worklist to [`unitcombat-distillation.md`](../../reference/engine.md) §3.A / §3.E. Data source: live
> `Assets/Data/**` (2026-07-19). This is a **first pass** — map the OBVIOUS, FLAG the unsure. Tick / edit freely;
> nothing here is load-bearing until the owner signs off.

## The model

- **A tag is a unit's IDENTITY** — `mounted`, `gunpowder`, `mechanized`, `armored`, `melee`, `archery`, `naval`,
  `air`, `siege`, `recon`, plus the role tags (`worker`/`settler`/`missionary`/`merchant`/`spy`/`outlaw`/`civilian`).
  A tag holds **no stats**.
- **The mapping is ADDITIVE, not a replacement.** "`UNITCOMBAT_X` → tag `Y`" means *every unit carrying
  `UNITCOMBAT_X` (primary `combatClass` ∪ `combatClasses` ∪ promotion-granted) ALSO gets identity tag `Y`.* The
  unit-combat is **kept** — it stays the stat-holding **modifier source** (the "good/bad AGAINST" column). Removing a
  now-statless-identity combat class is a **separate, deferred** question — NOT decided here.
- A UnitCombat that encodes a **vs-tag modifier group** (e.g. anti-mounted) is **not identity** and is out of scope —
  it stays a UnitCombat whose "vs" column references tags. Not mapped here.

## Method + counts

- **Scope:** the **400** unit-combats referenced by ≥1 unit (primary `combatClass` ∪ `combatClasses`) or ≥1
  promotion `identity.unitCombats`. (814 total; 414 referenced by nothing → the merge/purge worklist, separate.)
- **Map the obvious, flag the unsure** — the FIRST-pass method, since superseded for the individual classes:
  **an extra tag costs nothing and certainty is not a gate** (owner, [tags.md](../../specs/tags.md)), so every
  individual class now carries a tag. What stays FLAGGED is only the taxonomy FAMILIES below, which are excluded
  on a different ground — a size/quality/group RANK is not an identity at all, it is `sizeMatters` data.
- **Culture combats are NOT mapped — they were DROPPED** (owner 2026-07-19): the 344 `UNITCOMBAT_CULTURE_*` are
  redundant double-data of the culture BONUS (see [`unitcombat-merge-candidates.md`](unitcombat-merge-candidates.md)),
  removed entirely rather than distilled to a tag; the culture identity already lives on `BONUS_X.enables.units`.
- `*` on a tag = **suggested NEW tag** not yet in the confirmed vocabulary (`tags.md`) — obvious identity, but needs
  owner OK (`animal`, `hero`, `space`).
- Columns: `# units` = distinct units carrying the class (primary or sub); `(primN)` = of those, N carry it as
  PRIMARY `base.combatClass`.

---

## MAPPED (obvious identity)

### Tech / equipment identity

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_GUN | gunpowder | 270 (prim 75) | firearms primary |
| UNITCOMBAT_SIEGE_GUNPOWDER | siege + gunpowder | 25 | gunpowder-era siege |
| UNITCOMBAT_MOUNTED | mounted | 265 (prim 146) | cavalry primary |
| UNITCOMBAT_MOTILITY_RIDING | mounted | 263 | ridden motility |
| UNITCOMBAT_MOUNT_HORSE | mounted | 216 | mount species |
| UNITCOMBAT_MOUNT_ELEPHANT | mounted | 18 | mount species |
| UNITCOMBAT_MOUNT_CAMEL | mounted | 12 | mount species |
| UNITCOMBAT_MOUNT_GIRAFFE | mounted | 8 | mount species |
| UNITCOMBAT_MOUNT_MAMMOTH | mounted | 5 | mount species |
| UNITCOMBAT_MOUNT_ZEBRA | mounted | 5 | mount species |
| UNITCOMBAT_MOUNT_DEER | mounted | 4 | mount species |
| UNITCOMBAT_MOUNT_BEAR | mounted | 2 | mount species |
| UNITCOMBAT_MOUNT_LLAMA | mounted | 2 | mount species |
| UNITCOMBAT_MOUNT_BISON | mounted | 1 | mount species |
| UNITCOMBAT_MOUNT_RHINO | mounted | 1 | mount species |
| UNITCOMBAT_ARMOR_VEHICULAR | mechanized + armored | 83 | armoured vehicle |
| UNITCOMBAT_WHEELED | mechanized | 58 (prim 12) | motor vehicle |
| UNITCOMBAT_MOTILITY_DRIVING | mechanized | 73 | motorised motility |
| UNITCOMBAT_TRACKED | mechanized + armored | 25 (prim 12) | tank / AFV |
| UNITCOMBAT_ASSAULT_MECH | mechanized + armored | 6 (prim 5) | walker mech |

### Domain identity — naval

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_MOTILITY_NAVAL | naval | 198 | sea motility |
| UNITCOMBAT_ARMOR_NAVAL | naval | 122 | ship armour |
| UNITCOMBAT_NAVAL_COMBATANT | naval | 113 | naval combatant |
| UNITCOMBAT_WOODEN_SHIPS | naval | 67 (prim 61) | ship era |
| UNITCOMBAT_TRANSPORT | naval | 39 | sea transport |
| UNITCOMBAT_DIESEL_SHIPS | naval | 22 (prim 19) | ship era |
| UNITCOMBAT_STEAM_SHIPS | naval | 20 (prim 16) | ship era |
| UNITCOMBAT_NUCLEAR_SHIPS | naval | 19 (prim 19) | ship era |
| UNITCOMBAT_DESTROYER | naval | 13 | ship type |
| UNITCOMBAT_SUBMARINE | naval | 12 (prim 12) | ship type |
| UNITCOMBAT_BATTLESHIP | naval | 10 | ship type |
| UNITCOMBAT_CRUISER | naval | 8 | ship type |
| UNITCOMBAT_DREADNOUGHT | naval | 5 (prim 5) | ship type |
| UNITCOMBAT_DROID_SHIPS | naval | 0 (promo) | future ship era |
| UNITCOMBAT_GRAVITY_DRIVE_SHIPS | naval | 0 (promo) | future ship era |
| UNITCOMBAT_JET_SHIPS | naval | 0 (promo) | future ship era |
| UNITCOMBAT_LEVITATION_SHIPS | naval | 0 (promo) | future ship era |
| UNITCOMBAT_SHOCKWAVE_SHIPS | naval | 0 (promo) | future ship era |
| UNITCOMBAT_TROID_SHIPS | naval | 0 (promo) | future ship era |

### Domain identity — air

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_ARMOR_AIRCRAFT | air | 43 | aircraft armour |
| UNITCOMBAT_MOTILITY_AERIAL | air | 40 | flight motility |
| UNITCOMBAT_HELICOPTER | air | 15 (prim 11) | rotorcraft |
| UNITCOMBAT_GUNSHIP | air | 8 | attack helicopter |
| UNITCOMBAT_JET_FIGHTERS | air | 8 (prim 8) | jet fighter |
| UNITCOMBAT_BALLOON | air | 7 (prim 7) | early aerial |
| UNITCOMBAT_EARLY_FIGHTERS | air | 5 (prim 5) | fighter |
| UNITCOMBAT_BOMBERS | air | 4 (prim 4) | bomber |
| UNITCOMBAT_EARLY_BOMBERS | air | 2 (prim 2) | bomber |
| UNITCOMBAT_SUPERSONIC_PLANES | air | 2 (prim 2) | jet |
| UNITCOMBAT_ORBITAL_AIRCRAFT | air + space* | 2 (prim 2) | orbital plane |
| UNITCOMBAT_AIR_RECON | air + recon | 3 (prim 3) | recon aircraft |

### Type / combat identity

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_MELEE | melee | 339 (prim 143) | melee primary |
| UNITCOMBAT_ARCHER | archery | 88 (prim 39) | bow primary |
| UNITCOMBAT_SIEGE | siege | 53 (prim 48) | siege primary |
| UNITCOMBAT_SIEGE_FIELD | siege | 28 | field artillery |
| UNITCOMBAT_SIEGE_URBAN | siege | 26 | urban siege |
| UNITCOMBAT_SIEGE_WOODEN | siege | 17 | pre-gunpowder siege |
| UNITCOMBAT_SIEGE_DEFENSIVE | siege | 15 | defensive siege |
| UNITCOMBAT_SIEGE_ROCKETRY | siege | 6 | rocket artillery |
| UNITCOMBAT_SIEGE_ENERGY | siege | 5 | energy siege |
| UNITCOMBAT_SIEGE_GATECRASHER | siege | 4 | breaching siege |
| UNITCOMBAT_RECON | recon | 30 (prim 17) | scout primary |
| UNITCOMBAT_EXPLORER | recon | 67 | explorer |

### Role identity (folds onto existing role tags)

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_CIVILIAN | civilian | 532 | non-combatant |
| UNITCOMBAT_MISSIONARY | missionary | 31 (prim 30) | religion spreader |
| UNITCOMBAT_WORKER | worker | 29 (prim 28) | land worker |
| UNITCOMBAT_TRADE | merchant | 37 (prim 36) | trade-mission unit |
| UNITCOMBAT_CRIMINAL | outlaw | 22 (prim 19) | criminal gate (matches tags.md outlaw rule) |
| UNITCOMBAT_SETTLER | settler | 19 (prim 19) | founds cities |
| UNITCOMBAT_SEA_WORKER | worker | 10 (prim 10) | work boat |
| UNITCOMBAT_SPY | spy | 5 (prim 5) | espionage |
| UNITCOMBAT_COMBAT_WORKER | worker | 0 (promo) | combat engineer |

### Suggested NEW tags (obvious identity, not yet in vocabulary — needs owner OK)

| UNITCOMBAT_X | tag(s) | # units | note |
|---|---|---:|---|
| UNITCOMBAT_HERO | hero* | 406 (prim 406) | hero-unit identity; not in vocab |
| UNITCOMBAT_ANIMAL | animal* | 240 (prim 234) | animal identity; not in vocab |
| UNITCOMBAT_SEA_ANIMAL | animal* | 24 (prim 24) | aquatic animal |
| UNITCOMBAT_SEA_ANIMAL_TALE | animal* | 23 (prim 23) | aquatic animal |
| UNITCOMBAT_SPACE_WORKER | worker + space* | 67 (prim 67) | space worker |
| UNITCOMBAT_EARLY_SPACESHIP | space* | 48 | spacecraft |
| UNITCOMBAT_WORMHOLE_SPACESHIP | space* | 13 | spacecraft |
| UNITCOMBAT_SOLAR_SAIL_SPACESHIP | space* | 8 | spacecraft |
| UNITCOMBAT_ANTIMATTER_SPACESHIP | space* | 4 | spacecraft |
| UNITCOMBAT_NUCLEAR_SPACESHIP | space* | 4 | spacecraft |

---

## FLAGGED (unsure / not-an-identity / needs owner)

> **2ND PASS RESOLVED — the individual ambiguous classes are ruled; `curate_common.py`'s `TAG_BY_UNITCOMBAT`
> carries them, emitted onto the UNITCOMBAT by `curate_unitcombat.py`.** NEW vocabulary added: `police` (LAW_ENFORCEMENT) · `medic` (HEALTH_CARE) ·
> `missile` (MISSILE/BALLISTIC) · `synthetic` (ROBOT/HITECH/CLONES/NANITE/NANOMORPHIC) · `diplomat` · `entertainer`.
> Folded onto EXISTING tags: HUNTER/STRIKE_TEAM→`recon`, EXECUTIVE→`merchant`, PACIFIST→`civilian`,
> COMMODORE/CAPTAIN→`naval`, ROCKET_LAUNCHER→`siege`. Ruled NOT tags (left excluded): COMBATANT, SPEED_FAST/SLOW,
> ATTACK_FORM_*, STEALTH, SWARM (movement/attack-form/generic taxonomy); the animal sub-states/species WILD, TAMED,
> CAPTIVE, RECKLESS_ANIMAL, CANINE, FELINE (base `animal` covers them).
>
> **THIRD PASS — the remainder is tagged, under the extra-tags-are-free ruling.** Folded onto existing vocabulary:
> HOVERCRAFT→`mechanized`, ATTACHE→`diplomat`, RUFFIAN/EXILE/PIRATE→`outlaw`, ADMINISTRATOR→`bureaucrat`. Minted:
> `throwing` (a thrown-weapon skirmisher is ranged but not archery), `commander`, `prodigy`, `nomad`, `idea`,
> `doom`. The taxonomy FAMILIES below stay data (sizeMatters) — excluded because a RANK is not an identity, which
> is a different reason from being unsure, so the ruling does not touch them.

### Taxonomy families — NOT identity tags (each family = one reason)

These are the size/species/motility/weapon taxonomy the distillation plan (§3.A.1) keeps as `sizeMatters` /
`identity` data or as modifier groups — **not** identity tags. Flagged wholesale; do not force.

| family (member classes) | reason |
|---|---|
| `UNITCOMBAT_WEAPON_*` (106 classes, e.g. `_DIST_RIFLE`, `_H2H_LONG_BLADE`, `_A2G_BOMBS`, `_METHOD_*`, `_DETONATE_*`) | weapon taxonomy — the identity (gunpowder/archery/…) is already carried by the unit's PRIMARY combat class; a weapon sub does not by itself imply a coarse identity |
| `UNITCOMBAT_ANIMAL_COMBAT_*` (36, e.g. `_CHARGING`, `_STOMP`, `_POWER_KICK`) | animal-combat **modifier/ability groups** (vs / ability), not identity |
| `UNITCOMBAT_HEALS_*` / `UNITCOMBAT_HEALS_AS_*` (17, e.g. `_AS_PEOPLE`, `_AS_NAVAL`, `_AS_MECHANICAL`) | healing-category taxonomy — mirrors type but is a heal-mechanic bucket, not identity |
| `UNITCOMBAT_QUALITY_*` (11, e.g. `_STANDARD`, `_SUPERIOR`, `_EPIC`) | quality-rank taxonomy (sizeMatters) |
| `UNITCOMBAT_GROUP_*` (10, e.g. `_SOLO`, `_PARTY`, `_BATTALION`) | group-size taxonomy (sizeMatters) |
| `UNITCOMBAT_SIZE_*` (10, e.g. `_MEDIUM`, `_LARGE`, `_HUGE`) | physical-size taxonomy (sizeMatters) |
| `UNITCOMBAT_MAMMAL_*` / `_BIRD_*` / `_REPTILE_*` / `_FISH_*` / `_INVERTEBRATE_*` / `_AMPHIBIAN_*` / `_ANIMAL_<class>` (54) | animal-species taxonomy — if `animal*` is adopted these are sub-species, likely sizeMatters/data not tags |
| `UNITCOMBAT_SPECIES_*` (4: `_HUMAN`, `_AI`, `_NEANDERTHAL`, `_ALIEN`) | species taxonomy |
| `UNITCOMBAT_MOTILITY_*` residual (5: `_FOOT`, `_ANIMAL_DRAWN`, `_PILOTING`, `_UNMANNED`, `_HOVERING`) | motility taxonomy — the clear ones (`_RIDING`/`_NAVAL`/`_AERIAL`/`_DRIVING`) are MAPPED above; these are ambiguous |
| `UNITCOMBAT_ARMOR_*` tiers (4: `_LIGHT`, `_MEDIUM`, `_HEAVY`, `_MODERN`) | personal-armour tier taxonomy (`_VEHICULAR`/`_NAVAL`/`_AIRCRAFT` are MAPPED as they name a domain) |
| `UNITCOMBAT_SHIELD_*` (5, e.g. `_LARGE`, `_TOWER`, `_ENERGY`) | shield-equipment taxonomy |
| `UNITCOMBAT_SUBDUED*` (7, e.g. `_SUBDUED`, `_SUBDUED_LARGE`) | subdued-animal taxonomy (animal-adjacent; captured wildlife) |
| `UNITCOMBAT_ASSISTED_*` (7, e.g. `_MULE`, `_DOG`, `_ELEPHANT`) | pack/assist-animal taxonomy |

### Individual classes — ambiguous / not-a-clean-identity (needs owner)

| UNITCOMBAT_X | # units | reason |
|---|---:|---|
| UNITCOMBAT_COMBATANT | 1021 | generic "is a combatant" base; near-`military` but that tag is already `bMilitarySupport`-derived — not a specific identity |
| UNITCOMBAT_PACIFIST | 378 | non-combatant marker; likely → `civilian` but overlaps the derived `civilian` tag — confirm |
| UNITCOMBAT_WILD | 198 | animal lifecycle state (wild vs tamed); animal-adjacent, not a clean identity |
| UNITCOMBAT_SPEED_FAST | 466 | movement-speed taxonomy, not identity |
| UNITCOMBAT_SPEED_SLOW | 93 | movement-speed taxonomy, not identity |
| UNITCOMBAT_RECKLESS_ANIMAL | 90 | animal behaviour marker; animal-adjacent |
| UNITCOMBAT_THROWING | 57 (prim 35) | thrown-weapon skirmisher (javelin/sling/bola); ranged but distinct from `archery`; no vocab tag |
| UNITCOMBAT_TAMED | 53 | animal lifecycle state; animal-adjacent |
| UNITCOMBAT_LAW_ENFORCEMENT | 45 (prim 32) | police/enforcer role; no vocab tag (suggest a `law`/`police` tag?) |
| UNITCOMBAT_HUNTER | 37 (prim 32) | hunter/scout line (subdues animals); recon-adjacent — suggest `recon`? confirm |
| UNITCOMBAT_HEALTH_CARE | 30 (prim 25) | medic/healer role; no vocab tag (suggest `medic`?) |
| UNITCOMBAT_ROBOT | 26 (prim 20) | hi-tech synthetic troop; leans `mechanized` but also a species-ish identity — confirm |
| UNITCOMBAT_DIPLOMAT | 23 | diplomat role; civilian-ish great-person, not core combat identity |
| UNITCOMBAT_EXECUTIVE | 23 (prim 23) | corporate executive; runs trade missions — suggest `merchant`? confirm |
| UNITCOMBAT_IDEA | 23 | abstract "idea" unit; unclear identity |
| UNITCOMBAT_STRIKE_TEAM | 21 (prim 19) | special-forces primary (sniper/assassin per skills.md); recon/spec-ops, also `outlaw` via CRIMINAL subcombat — confirm |
| UNITCOMBAT_CANINE | 20 | war-animal species (dogs); animal-species, not top-level identity |
| UNITCOMBAT_FELINE | 17 (prim 5) | war-animal species (big cats); animal-species |
| UNITCOMBAT_ATTACK_FORM_COLLISION_PIERCING | 16 | attack-form modifier group, not identity |
| UNITCOMBAT_HITECH | 16 (prim 12) | hi-tech marker; leans `mechanized`/synthetic — confirm |
| UNITCOMBAT_ATTACHE | 15 | military attaché; great-person-ish role |
| UNITCOMBAT_BALLISTIC | 15 | ballistic missile/warhead; no `missile` vocab tag — suggest? |
| UNITCOMBAT_CAPTAIN | 15 (prim 12) | naval/leader role; great-person-ish — maybe `naval`? confirm |
| UNITCOMBAT_ENTERTAINER | 14 (prim 13) | entertainer role; civilian-ish great person |
| UNITCOMBAT_RUFFIAN | 14 (prim 10) | criminal-adjacent, but tags.md fixes the `outlaw` gate as UNITCOMBAT_CRIMINAL (subcombat), not RUFFIAN |
| UNITCOMBAT_PRODIGY | 13 (prim 13) | prodigy/great-person role; not combat identity |
| UNITCOMBAT_ROCKET_LAUNCHER | 12 (prim 10) | rocket artillery; suggest `siege`? confirm (vs a `missile` tag) |
| UNITCOMBAT_DOOM | 11 (prim 11) | unique/doomsday unit; unclear identity |
| UNITCOMBAT_PIRATE | 11 | criminal/naval-adjacent; `outlaw` gate is CRIMINAL, and naval identity comes from ship class — confirm |
| UNITCOMBAT_SWARM | 9 | swarm/insect group; size/species-ish, not identity |
| UNITCOMBAT_CLONES | 8 (prim 5) | cloned troops; synthetic identity — confirm (mechanized? own tag?) |
| UNITCOMBAT_STEALTH | 7 (prim 6) | stealth units; "stealth" reads as an ability/modifier, not identity |
| UNITCOMBAT_NANITE | 5 (prim 4) | nanite swarm; synthetic — confirm |
| UNITCOMBAT_HOVERCRAFT | 4 (prim 3) | hovercraft; land/sea hybrid — `mechanized`? `naval`? confirm |
| UNITCOMBAT_MISSILE | 4 (prim 4) | guided missile; no `missile` vocab tag — suggest? |
| UNITCOMBAT_ADMINISTRATOR | 3 (prim 2) | administrator role; civilian-ish |
| UNITCOMBAT_CAPTIVE | 3 (prim 3) | captured-unit state; not identity |
| UNITCOMBAT_NANOMORPHIC | 2 | nanomorph; synthetic — confirm |
| UNITCOMBAT_COMMANDER | 1 (promo 6) | commander/leader role; great-person-ish |
| UNITCOMBAT_COMMODORE | 1 (promo 1) | naval leader role; maybe `naval`? confirm |
| UNITCOMBAT_EXILE | 1 | criminal-adjacent; `outlaw` gate is CRIMINAL — confirm |
| UNITCOMBAT_NOMAD | 0 (promo 4) | promo-only; unclear identity |
