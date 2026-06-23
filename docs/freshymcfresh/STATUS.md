# freshymcfresh — session state & open items (LOCK-DOWN)

> Written to survive a context compaction. After compaction the **immediate next work** is in §3:
> **update the curators per the fold list and REGENERATE `Assets/Data`**, so the JSON matches the specs and
> nothing dangles (specs lead the model; the data/curators must catch up).

---

## 1. The plan (docs restructure)

- **`docs/freshymcfresh/`** = the new clean doc set being built. **`docs/oldymcsad/`** = the old docs, to be
  PURGED once their durable value is lifted (owner moves them; agent does not `rm`). Owner already removed
  `old-docs/`, `docs/crap/`, `docs/players/`.
- **No `docs/modders/`** — modder docs are a *derived* product of the data (future frontend); its authoring
  content folded into `json.md`.
- **Indexes** (DESPAIR/REALISM/COMPLEXITY) → a **root `/indexes/`** folder, NOT part of `docs/` (owner moves +
  updates the git page).
- **spec = the system; glossary = the specific namings** (e.g. `json.md` is the modifier *system*; `naming.md`
  and `skills.md` are *glossaries*).
- **Method (validated):** write a spec → cold-test it with a fresh-context minion that **follows links** (the
  owner-ruled cold-read rule) → fix the flagged defects. A spec passes when it + its linked specs teach the
  concept without guessing.

## 2. What's built — `docs/freshymcfresh/specs/`

| spec | what it is | cold-test |
|---|---|---|
| `json.md` | the JSON data spec — entity shape, the shared vocabulary (scopes singular / targets plural / atoms / predicates / units / `per` / `perUnit` / entry shape), reserved sections, modifier families, the ×100 human-readable scale | ✓ passes |
| `naming.md` | the `INFOTYPE_NAME` glossary — 38 infotype prefixes + a ported/where-to-look map (`Assets/Data/<folder>/` vs XML-only) | ✓ |
| `enabler.md` | "can I?" — GENERATE-then-GATE; the `enables`/`disables`/`replaces`/`obsoletes` chain is the **authority on the tree**, `requires` only gates **attainability** (can't remove from the tree); pass 1 completes before pass 2; bidirectional `require` callback UP | ✓ passes |
| `modifier.md` | "how much?" — deposit DOWN / accumulate / combine; **a modifier = a `requires`-shaped gate + an output** (same vocabulary, SEPARATE fields, output can buff **or** nerf); deliveryguy ownership; `perUnit:<predicate>`; unit-plane self-accumulator; cargo split | ✓ passes |
| `tally.md` | "how many?" — count substrate (sibling of modifier, rolls UP), player-leaf store, serializes-nothing/rebuild-on-load | ✓ passes |
| `orwellian-logging.md` | observability — the Orwell reconstruction bar, the 0–5 scale, the 3 hook shapes, the event spine + `IEventConsumer` + the `DOMAIN`/`DIAGNOSTIC`/`TRACE` KIND firewall, read rules, the shadow | ◐ partial (only because `http-endpoints` is unwritten) |
| `skills.md` | unit-skill glossary — §1 Validated (~56), §2 ⚠ Needs validation (~23), §3 Not-skills-relocate (3) | glossary (not cold-tested) |

## 3. The unit-classification model + the revised curator plan (this session's main outcome)

The "skills" work reshaped into a **four-block unit-classification model** — now in [skills.md](specs/skills.md)
intro + [json.md](specs/json.md) §8. **A unit carries three blocks; the empire counterpart to `skills` is
`capabilities`.** The **operative test for sorting any flag: *can a promotion grant it?***

- **`skills`** — **mutable** abilities, promotion-grantable (`blitz`, amphib, …).
- **`tags`** — **immutable, type-derived** membership; set at creation, re-set on upgrade; **accounting-only**
  (overlapping `military`/`civilian`/`worker`/`spy`/`gunpowder`/`mechanized`/…), read by `IS_<TAG>` predicates;
  **not** promotion-grantable.
- **`state`** — **transient** (fired → countdown → over: `paralyze`); **greenfield** (faked via pseudo-promotions
  + Python events; no clean field to migrate).
- **`capabilities`** — empire/team counterpart to skills (tech-unlocked).

> **✅ Committed this session (`json-data-migration`):** `capabilities`→`skills` (`ed58abce9`); derived `tags`
> first pass — `military` from `bMilitarySupport` (suppressed when a `DefaultUnitAI` role applies), civilian roles
> (worker/settler/missionary/merchant) + `spy` from `DefaultUnitAI` (`d13fc12a3`, `f894247c2`); cargo →
> `cargo.space.{unit: IS_<domain>, flat}` carries-what (`46c6db296`). Items 1, 7, 8 below are **done**; the rest
> are **post-migration** (reclassification + the modifier amounts).

### Curator folds (revised — fix curator, `--write`, commit):

1. **Split the unit `capabilities` block** (data: 82 keys) into **`skills`** + **`tags`**, sorting each flag by
   the promotion-grantable test. `curate_promotion.py` emits **skills only** (promotions can't set tags);
   `curate_unit.py` emits both (`tags` ← unit-type intrinsic). Block-assembly sites: `curate_promotion.py:493`
   (`out["capabilities"]=caps`), `curate_unit.py:637`, `curate_unitcombat.py` — plus `STRUCT`/order lists.
2. **`military*` flags → the `military` tag + `IS_MILITARY`.** Drop `militaryHappiness`/`militaryProduction`/
   `militarySupport` as flags. Engine-verified: support = the military **upkeep pool** (`getFinalUnitUpkeepChange`
   routes to `m_iUnitUpkeepMilitary100`); happiness = `getMilitaryHappiness` count source; production = the city
   military-build bonus. Legacy sets differed (1007/1325/1276) → one `IS_MILITARY` is a deliberate behaviour
   change. `mechanized`, `gunpowder` are also **tags**.
3. **Modifier *amounts*** keyed on `IS_MILITARY` live on civics/traits/buildings: `perMilitaryUnit` →
   `happiness.empire.cities.{unit: IS_MILITARY, flat: N}` (`curate_civic.py:89` + `curate_trait.py` — a
   **structural** emission change; **spec done** (json.md §3.7, `unit:` qualifier), curator post-migration); city
   military-production → `buildRate.<scope>.military`.
4. **DEAD — drop** (owner): traps are removed — `triggerBeforeAttack`, `trapImmunity`, `trapTarget`, `trapSetWith`.
   `oneUp` dead? (verify entertainer/revolt). `paralyze` → `state` (not a skill).
5. **~~Curator gap~~ — FALSE ALARM (verified).** `bOnslaught`/`bGatherHerd`/`bTriggerBeforeAttack` are NOT
   authored on any unit (only promotions, which are handled); COVERAGE is clean. No fix needed.
6. **`militaryTrade` + `workerTrade` → one `tradable` skill** (consolidate; `curate_unit.py:107,111`).
7. **✅ Cargo (done).** `iCargo` → `cargo.unit.space.{unit: IS_<domain>, flat}` (carries-what; carrier = IS_AIR,
   landing craft = IS_LAND); Special/SMNotSpecial stay `identity.cargo`. Still **post-migration**: `cargoPrereq` →
   `is_cargo_vessel` skill (carry ability); `cargo.size` footprint (default 1) = the SizeMatters rework.
8. **Open before curating `tags`:** the per-unit-type tag list (which units are `military`/`worker`/`spy`/
   `gunpowder`/…) and its source signal (legacy `militarySupport` → `military`; others TBD). **`state` is
   greenfield** — identify the pseudo-promotion/Python "states" and formalize.
9. **Verify** the `<scope>.<targets>` + `IS_*`/`HAS_*` predicate folds regenerated (per `migration-renames`).

> **Skill-pass result:** all 23 previously-`⚠` skills traced to engine consumption (skills.md §2); owner domain
> pass then ruled several dead/re-homed (traps, `oneUp`?, `paralyze`→state) — a code path ≠ a live mechanic.
> **Sibling glossaries still to write:** `tags`, `state`, `capabilities`.

## 4. Parked / later (don't drop)

- **Grant-only transform** of skills & capabilities — drop the grant/revoke `false` pairs (`skills.md` §4).
- **`cargoSpace` stat rework** — volumetric under SizeMatters vs per-unit normal; the current "volume = static
  multiplier" hack. Separate issue, part of the SizeMatters work.
- **Sibling glossaries to write** — `tags`, `state`, `capabilities` (the other three classification blocks, all
  siblings of `skills.md`). The `~23 ⚠` skill meanings are now resolved/ruled (skills.md §2).
- **Predicate system** — make `IS_*` predicates **definable as JSON objects** + support **predicate groups**
  (compose them). Predicates are independent queries that *may* be defined to encompass tags (json.md §3.7).
  **Hardcoded for migration** (`IS_MILITARY`/`IS_LAND`/`IS_AIR`).
- **Criminal-type tag** — the 13 `UNITAI_INFILTRATOR` units (`thief`/`robber`/`thug`/`mobster`/`exile`/…) need
  their tag(s) mapped — owner: "map the lunacy" as a separate pass.
- **`bSpy` skill → `spy` tag** — the spy notion is mis-filed as both a skill (`bSpy`) and a tag; drop the skill,
  keep the tag.
- **`http-endpoints.md`** — to write (clean REDESIGN). Survey done — current routes: `/`, `/players`, `/cities`,
  `/units`, `/events`, `/diagnostic/*` { `sweep`, `placementSweep`, `dormancySweep`, `modifierSweep`,
  `movementSweep`, `modifier`, `cityInput`, and the gate queries `canConstruct`/`canTrain`/`canResearch`/
  `canDoCivics`/`canBuild`/`canHurry`/`canAcquireExperience` }, `/extractor/*`. **Proposed clean grouping
  (awaiting owner's structure call):** `/shadow/{buildable,placement,dormancy,modifier,movement}`,
  `/can/{construct,train,…}`, `/decompose/{city,modifier}`, the snapshots, `/extractor/*`.
- **Launch:** tighten `docs/Civ4_BTS_LaunchParameters.md` to the verified S2S procedure + add an AGENTS.md note
  that agents may launch via `agentstart.bat` (reverses the old "agents don't start the game" rule).
- **all-`docs/dev` cold-read purge-triage** (owner-chosen scope) — not started.
- Minor: the modder-README / data-model footer cites stale `docs/dev/plans/` paths.

## 5. Launch capability (BUILT + VERIFIED — working tree only, not committed)

- **`agentstart.bat`** (repo root) — reads `.env`, `taskkill`s the game, waits, `cd`s to repo root, launches
  `mod=" mods\Stones2Stars"` + `/FXSLOAD` the test save. **Default SKIPS the dev boot-check**; `agentstart.bat
  bootcheck` runs it.
- **`.env`** (gitignored): `S2S_BTS_DIR` = `C:\Games\Civilization IV Complete\Civ4\Beyond the Sword`,
  `S2S_MOD_NAME=Stones2Stars`, `S2S_SAVE_GAME=testsave.CivBeyondSwordSave`. `.env.example` tracked; `.gitignore`
  has `.env`.
- **`Sources/CvGameCoreDLL.cpp` `DllMain`** (~line 81): the dev rebuild-on-load (FPKLive + `_BootDLLCheck.bat`,
  fired when `git_directory.txt` is present) is now gated `!IsDebuggerPresent() &&
  GetEnvironmentVariableA("S2S_SKIP_BOOTCHECK",NULL,0)==0`. **Built + deployed (Release).**
- **Verified:** `agentstart.bat skip` → testsave loads (`/players` = count 17, turn 1336), no dialog, no hang.
- The game holds `.log` files open — read live state via `/diagnostic/*` or `/events` (connect before the turn
  ticks), not the logs. `data-reader` minion for bulk reads.

## 6. Key decisions this session (so they survive)

- **modifier = `requires` + output:** the deposit's condition uses the SAME vocabulary as `requires`, but is a
  SEPARATE field (require iron, buff from power); output can buff **or nerf**.
- **enabler authority vs follow-up:** the `enables` family alone owns tree membership; `requires` only makes a
  member attainable/unattainable, never removes it; GENERATE completes fully before the GATE pass.
- **`perUnit:<predicate>`** is a real model addition (count units matching a predicate); resolves `byOccupant`.
- **`byOccupant`/`byCargo` were old-doc inventions** — dissolved: `byOccupant` → a `perUnit` modifier; carry =
  the `is_cargo_vessel` skill, capacity = the `cargoSpace` modifier.
- **spec vs glossary** distinction (owner). **`capabilities`→`skills`** rename (unit, vs empire capabilities).
- **Cold-read follows links** (owner ruling); a single-file-isolation test is too strict.
