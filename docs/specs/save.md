# Save / load — the name-keyed format & the soft-remove discipline

> **A core spec.** How S2S serializes game state and stays load-compatible. The cascade's entire "derived data
> serializes nothing / rebuild on load" discipline rests on this. **The soft-remove mechanism (§3) is SETTLED — read
> it HERE and trust it; do NOT re-derive it from `CvTaggedSaveFormatWrapper.cpp` each session** (the recurring
> rollerskate). Home of [the soft-remove save discipline](#3-removing-a-serialized-field--the-soft-remove-via-assetssavemigrationtxt-) +
> [derived data is never trusted from a save](#5-derived-data-serializes-nothing-).

## 1. The format — name-keyed, not positional

Saves are `(id, type-code, value)` tuples keyed by a normalized **`"ClassName::memberName"`** tag: **field-level**
compatibility resolves dynamically by name (`CvTaggedSaveFormatWrapper`). A read asks for a tag and `Expect()` matches
it against the stream. Because position doesn't matter, adding / removing / reordering fields is handled by name,
never by a version gate.

⚠ **There IS a whole-save version stamp, and it is an all-or-nothing GATE, not a migration router.**
`SAVE_FORMAT_VERSION` (`Sources/Defines/CvDefines.h`) is written by `CvInitCore::write` and read back by
`CvInitCore::read`, which throws `std::invalid_argument` behind an "Unreadable Save Game!" message box on any
mismatch (`Sources/Engine/CvInitCore.cpp` ~:1701-1709). Bumping it makes **every** existing save unreadable in one
step — it is the deliberate hard-break switch, and it is unrelated to the per-field name matching above. ⛔ Do not
reach for it to solve a field-level change: that is what §3's soft-remove exists for.

## 2. Adding a field is SOFT (nothing to declare)

Old save, new code: the new read's tag isn't in the old stream, so `Expect()` returns false, leaves the stream
untouched, and the member keeps its default (`CvTaggedSaveFormatWrapper.cpp` ~:3830). No action needed.

## 3. Removing a serialized field — the SOFT-REMOVE via `Assets/savemigration.txt` ⭐

> [the soft-remove save discipline](#3-removing-a-serialized-field--the-soft-remove-via-assetssavemigrationtxt-) — this is its authoritative home.

**FULL-DELETE the member + its read + its write, and NAME the orphan tag in `Assets/savemigration.txt`.** That is the
whole procedure. The reader (`Expect` → `sm_isCut`, `CvTaggedSaveFormatWrapper.cpp` ~:3944) parses that file **once**
at load and drains any listed orphan tag **transparently, wherever it sits in the stream** — the drain runs inside the
header loop and falls through to read the next tag, so **consecutive orphan tags of the same named field all drain**.
The field is then FULLY GONE from the object: no member, no read, no write.

- **⛔ NO `WRAPPER_SKIP_ELEMENT` for a removed field.** A lingering skip still names the dead member in the read path —
  a rollerskate target ([leave no evidence of the abandoned path](../../AGENTS.md#design)) — and
  the central drain makes it redundant. The old two-stage `SKIP_ELEMENT`-now / flush-at-the-next-break model is
  **RETIRED**; there is no "flush."
- **Save-breaking is OBSOLETE for field removal.** A listed tag loads clean from any old save, forever — no version
  bump, no `@SAVEBREAK`.
- **RENAME rides the same file:** `Class::m_old -> Class::m_new` remaps the old tag onto the new member.
- **⛔ THE ONE HARD RULE — the entry is MANDATORY.** An **UNLISTED** orphan (a deleted read with no
  `savemigration.txt` line) makes `Expect()` treat the mismatch as "code ahead of stream," never consume the element,
  and **desync every subsequent read in that object** into silent defaults — the load guts wholesale (proven live:
  empty tech lists, buildingless cities). List the tag and it is soft; forget it and the object is corrupt.
- **⛔ A LISTED TAG WHOSE MEMBER IS STILL SERIALIZED DRAINS LIVE STATE — and it is MECHANICALLY CHECKABLE.** The
  drain runs in the header loop and does not care that a live `WRAPPER_READ` was waiting for that tag, so the
  read finds nothing, the member keeps its default, and the value is silently lost on EVERY load — the inverse
  of the unlisted-orphan desync, and quieter. The check: intersect the file's bare `Class::m_field` entries with
  the members still carrying a `WRAPPER_READ`/`WRITE` in their owning class; the intersection must be EMPTY.
  ⚠ Resolve a hit by asking which side is right, never by reflex — a member whose only writer is `applyEvent` is
  genuine one-shot event state that CORRECTLY stays serialized (§5), so the ENTRY is the defect; a member the
  cascade genuinely replaced keeps its entry and loses its read + write.
  ⚑ The same scan catches the other false-entry class for free: a **bracketed** decorated tag (§3 above) can
  never match, so an entry for one is inert-but-false and its drain loop (§4) is what is actually doing the work.
  ⚠ And nothing marks prose as prose except the leading `|`, so a note line that BEGINS with a `Class::` token
  registers as an entry — keep member names off the start of a wrapped line.
- **⚑ ALL THREE FAILURES ABOVE ARE CHECKED BY `python Tools/verify-savemigration.py`** — the drain-live-state
  intersection, the bracketed entry, and the prose line that registers as an entry. It mirrors the reader's own
  `sm_ensureLoaded`/`sm_token` parse rather than a stricter one, because a stricter checker under-reports
  exactly the prose case. Run it after editing this file or cutting a serialized member. ⛔ A hit is resolved by
  deciding which SIDE is right, never by reflex — and never by loosening the check.
- **⚑ THE FILE IS ALSO THE REPLACEMENT-OBLIGATION LEDGER — read it that way, never as history.** An entry's note
  records WHY the field could be cut: the value is now served by a NAMED replacement (a cascade gather, a computed
  accessor). If that replacement is later archived, renamed, or never built, the field is already gone from every
  save and the value has **no source at all** — a silent hole no compiler catches. So: when you retire or replace a
  gatherer, grep `savemigration.txt` for it and re-home every obligation it carries; and never "clean up" an entry —
  the drain is live forever, and the note is the only record of what owes a value.

The tag name is the **normalized** `"ClassName::memberName"` (what the stream dictionary stored, via `NormalizeName`)
and `sm_isCut` is an **exact string match**. ⛔ **A BRACKETED decorated per-element tag (`m_ppaai…[iI]` /
`…[newIndex]`) does NOT reliably match** — the normalized dictionary name differs from the C++ source literal, so a
`savemigration.txt` entry for it silently fails to drain and desyncs the load (verified live 2026-07-21: it guts
`CvPlayer::read`). **Do NOT soft-remove a decorated per-element array via `savemigration.txt`** — keep its
enum-remapping drain loop (§4). savemigration is for whole named scalar/array fields; a **bracket-free** decorated
sub-tag (e.g. a `…Size` / `…Type` / `…Value` variable-length tag) is fine.

## 4. The `WRAPPER_SKIP_ELEMENT` that STAYS — live enum-remapping (NOT a removed field) ⛔ the recurring trap

A `WRAPPER_SKIP_ELEMENT` in the tree is **two different things** — do not conflate them:

- **(a) A dead-field drain** — a skip of a fully-removed member. If it's a **plain named** scalar/array tag, **retire
  it per §3** (delete the skip, list the tag). ⛔ **EXCEPTION — a DECORATED per-element array drained in a loop**
  (`m_ppaai…[iI]`): the member is dead, but its bracketed tag can't be soft-removed (§3, the normalized name differs),
  so the **drain loop STAYS** — do not convert it. Only the plain-named ones are the convert-me set.
- **(b) The `else`-branch of a LIVE enum-REMAPPING loop** — the read loops the *saved* Types, maps each to its current
  id, reads the surviving ones into a **still-live** array member, and skips a **removed Type's** slot:

  ```cpp
  for (int i = 0; i < wrapper.getNumClassEnumValues(REMAPPED_CLASS_TYPE_X); ++i) {
      int iI = wrapper.getNewClassEnumValue(REMAPPED_CLASS_TYPE_X, i, true);
      if (iI != -1) WRAPPER_READ_ARRAY(..., m_ppX[iI]);          // surviving Type -> the LIVE member
      else          WRAPPER_SKIP_ELEMENT(..., m_ppX[iI], ...);   // removed  Type -> drain its orphan slot
  }
  ```

  This skip is **PERMANENT and correct** — the member is alive; only a per-save removed *index* is drained. It is
  **NOT** a rollerskate target and **NOT** convertible to `savemigration.txt` (the drain is runtime per-save
  remapping keyed on which Types that save had, not a static field cut). **Leave it.**

**The test:** does the `if (iI != -1)` branch read a live member? ⇒ case (b), leave it. Do *both* branches merely
drain / read-to-throwaway? ⇒ the member is dead ⇒ case (a), convert per §3.

- **⛔ A DRAIN THAT READS INTO A LOCAL MUST NAME THE ORIGINAL TAG — the UNDECORATED macro takes the tag from the
  LOCAL'S OWN NAME.** `WRAPPER_READ_ARRAY(w, cls, n, name)` expands to `w.Read(cls "::" #name, …)`, so draining a
  dead member through a scratch local (`int aiDrainCommerce[…]`) asks the stream for `Class::aiDrainCommerce` — a
  tag NO save has ever contained. Nothing matches, nothing is consumed, and the dead member's elements stay in the
  stream as UNLISTED ORPHANS: the §3 hard failure, with the load gutted from that point on. **Use the
  `_DECORATED` form and pass the member's real bracketed literal** (`"m_ppiBonusCommerceModifier[iI]"`), which
  `NormalizeName` reduces to the same `[]` form the stream stored.
  ⚠ **It fails LOUDLY in `SaveRead.log` and silently everywhere else**: the giveaway is a run of
  `[SAVEREAD] mismatch expected=<later tag> got=<the dead member>[]` — the reader walking forward while one tag
  sits unconsumed. ⚑ The neighbouring drains in the same function already carry the decorated form; a new one
  written from the undecorated macro's shorter signature is how the two diverge.

## 5. Derived data serializes NOTHING ⭐

A recompute-only cache (yields, commerce, the cascade packages, network bonus counts, power, dormancy verdicts, …) is
**never trusted from a save**: don't read it, don't write it, drain any old-save orphan via §3. `reset()` /
marked-on-construct means the first read after load recomputes from current state — never stale-from-save. This is
**universal, not per-field-optional**: no cache is ever serialized. With nothing derived read from a
save there is nothing to purge, so no blanket recompute exists to purge it ([self-heal is not a backstop](../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).
A serialized store survives ONLY for genuine **non-derivable** state (event/vote grants,
e.g. `CvCity::m_paiFreeBonusEvents`). Cache mechanics: [cascade.md](../cascade.md).

## 6. Deleting a changer? Audit its whole BODY for side effects

An apply-site audit alone misses non-obvious riders. Legacy changers carry them: `changeTerrainTradeCount` /
`changeRiverTradeCount` call `updatePlotGroups()` (the trade-network recompute) per team player;
`changeBridgeBuildingCount` marks bridges dirty; others fire UI dirty bits. Post-cut, the surviving trigger site
(`setHasTech` / `processTech` / …) must still fire those effects, or derived engine state goes progressively stale.

## 7. Removing a TYPE is SOFT — every class read allows missing ⭐

> **Owner ruling: `_ALLOW_MISSING` on ALL class reads.** Every `WRAPPER_READ_CLASS_{ENUM,ARRAY,ENUM_ARRAY}`
> (including the `_DECORATED` forms) uses its `_ALLOW_MISSING` variant, so **deleting an XML Type never breaks a
> save**: an ARRAY read drops the removed Type's slot on remap; an ENUM read resolves it to `-1`/`NO_<X>`.
> **Why it is a blanket rule, not a per-case choice:** a hard-throw on a missing Type is what makes content
> undeletable — and *"it has happened numerous times that things have been named savebreak, by modders, without
> being savebreak"*, so features sit parked behind a `@SAVEBREAK` label forever for a break that was never real.
>
> ⚠ **The obligation this creates.** Allow-missing turns a LOUD failure into a silent `-1`. That is correct
> wherever `NO_<X>` is a valid state (features, bonuses, improvements, routes, civics — the engine's normal
> sentinels). It is NOT sufficient where the enum is a **record's IDENTITY** (e.g.
> `EventTriggeredData::m_eTrigger`): there the read loop must DROP the orphaned record, never keep one pointing
> at nothing. Apply that same test to every class read you add.
>
> ### The THREE classes of Type removal — the decision procedure
>
> Do not memorize a list; run the two questions. **(1) Is `NO_<X>` a valid state for the owner? (2) If not, is
> there a sane substitute?**
>
> | class | answer | handling | examples |
> |---|---|---|---|
> | **SOFT** | `NO_<X>` is normal | allow-missing, nothing more (drop the record if the enum is its IDENTITY) | features · bonuses · improvements · routes |
> | **FALLBACK** | not valid, but any valid entry serves | allow-missing **+ substitute a default at load** | **civics** (worked example below) |
> | **FAIL-LOUD** | not valid, and NO substitute exists | read stays hard; the Type is UNDELETABLE | terrain · gamespeed · mapcategory |
>
> **The FALLBACK class, worked (civics).** A player must hold *a* civic per option slot, but any valid one will
> do — *"you can just replace with whatever is first in the list"*. The substitute only has to be
> VALID, never optimal.
>
> ⚑ **Why first-in-list is ALWAYS a legal substitute — it is structural, not luck.** Category id order
> comes from the curator's `_order.json` manifest, which reproduces the legacy XML document order
> ([engine.md § Info loading](../reference/engine.md)), so the FIRST civic in an option slot is that slot's BASE
> civic — and the base civics are exactly what `TECH_GAME_START`'s `enables` unlocks, the synthetic root node
> **every player always holds** ([enabler.md §2](enabler.md)). So the head of the list is available by
> construction, in every game, at every point. Do not "improve" this into a cleverer selection.
>
> Both layers already implement it: `CvPlayer::read` (~:18694) re-checks every slot after the allow-missing read
> and replaces a `NO_CIVIC` **or wrong-category** civic with the civ's initial civic;
> `cvInternalGlobals::checkInitialCivics` repairs that initial table in turn, taking the first civic in the
> option **with no tech prereq** — which selects precisely the start-enabled civic described above. When you add
> a FALLBACK-class Type, copy that shape: repair at load, immediately after the read, in the same loop that
> knows the slot's meaning.
>
> ⛔ **The FAIL-LOUD class — the DO-NOT-DELETE list.** Here allow-missing is the WORST outcome: it loads
> `-1` and yields a silent black hole / a broken game, where a throw would have named the problem at once.
>
> | Type | Why it breaks the game | Where it is protected |
> |---|---|---|
> | **`TERRAIN_`** | every plot must have one — *"if you remove plot terrain, you get a black hole there"* | `CvPlot::m_eTerrainType` read stays `WRAPPER_READ_CLASS_ENUM` (hard) |
> | **`GAMESPEED_`** | the save's every scaled cost/threshold/turn was accumulated against it — *"changing gamespeed literally breaks the game"* | `CvInitCore::m_eGameSpeed` read stays hard |
> | **`MAPCATEGORY_`** | gates building placement / feature spread / bonus placement | ⚠ **NOT a save-path concern — a DATA-integrity one** (below) |
>
> ⛔ **THE TABLE IS KNOWN-INCOMPLETE: *"there may be more that will completely break the game, but those
> are the ones I could think of."*** Treat it as the members identified so far, NEVER as a cleared list — the
> absence of a Type from it is not evidence that deleting that Type is safe. **The mechanical tell of a missing
> member is an UNGUARDED dereference of a possibly-absent id** — `GC.get<X>Info(getX())` with no `NO_<X>` check
> (exactly the shape of `CvPlot::getMapCategories()`'s `GC.getTerrainInfo(getTerrainType())`), or a consumer for
> which `NO_<X>` is not a modelled state. Before deleting ANY Type, run that test on its consumers rather than
> trusting this table; when you find a new member, add it here with its failure mode.
>
> ⚠ **`MAPCATEGORY_` has no save read to harden.** A plot does not store its categories: `CvPlot::getMapCategories()`
> delegates to `GC.getTerrainInfo(getTerrainType()).getMapCategories()`, so they are DERIVED from terrain info and
> never serialized. Deleting one therefore breaks the game through the DATA path — and **silently permissively**,
> because `CvPlot::isMapCategoryType` returns true when the plot's list is EMPTY (`plotMapCategories.empty() ||
> …`), i.e. a lost list reads as "everything is allowed" rather than as an error. It is also the same dependency
> chain as terrain (categories hang off terrain), so removing a terrain takes both out at once.
>
> ⛔ **The three identity reads that are NOT yet allow-missing** — `VoteSelectionData::eVoteSource`,
> `VoteTriggeredData::eVoteSource`, `VoteTriggeredData::kVoteOption.eVote`
> (all in `Sources/Defines/CvStructs.cpp`). Each is the identity of an object owned by an
> `FFreeListTrashArray`, whose `ReadStreamableFFreeListTrashArray` does `new T; pData->read(); flist.load(pData)`
> — **the per-object `read()` has no way to drop its own record**, so flipping them alone would register a live
> object pointing at nothing. Removing a `VOTE_SOURCE` or `VOTE` Type is therefore still HARD until that drop
> exists (a two-phase read that can reject, or a post-load sweep). Every other class read in the tree is
> allow-missing.

⛔ **A `@SAVEBREAK` comment is a CLAIM TO VERIFY, not a fact.** Check it against the list below before
believing it — several in-tree labels do not survive that check — and never park work behind an unverified one.

Enum/Type drift is name-remapped on load (`getInfoTypeForString`); XML reorder/insert is free. The real
save-breaks that remain:

1. a same-tag field whose **meaning** changed (silent wrong load);
2. a legacy raw enum-indexed int array that **shrinks**;
3. a **NARROWING** type-code change under a reused name (widening is soft — §8);
4. a deleted field with **no** `savemigration.txt` entry (the §3 stale-tag desync);
5. removing an **`EVENT_TRIGGER` / `VOTE_SOURCE` / `VOTE`** Type — the four `FFreeListTrashArray` identity reads
   above, hard until the drop mechanism exists. **This one is a REAL `@SAVEBREAK`** (so the `EVENTTRIGGER_*`
   entries in the `@SAVEBREAK - Delete` block are correctly labelled — verify, don't assume, in both directions).

Everything else — field add, field remove, rename, reorder, and **every other** Type removal — is soft.

## 8. WIDENING a serialized field is SOFT — the READER absorbs the narrower form ⭐

A save tuple carries a **type code** (§1), and `Expect` used to demand an exact match — which is why changing a
member's type has historically broken saves, and why it broke them *silently* (a mismatched tag is skipped, the
member keeps its default, and a plausible wrong number loads).

**That is fixed in the ONE place it belongs: the reader.** `Expect` takes an optional NARROWER type it will also
accept, and the 64-bit reads pass their 32-bit twin (`SAVE_VALUE_TYPE_INT` / `SAVE_VALUE_TYPE_INT_ARRAY`). On a
match the read inspects `m_iNextElementType` to see which form the stream actually holds and converts. So:

- **Widening `int` → `int64_t` needs NOTHING.** Keep the member, keep its name, keep its tag. An old save's
  32-bit value is read and widened in place; a new save writes the wide form.
- **No new tag, no scratch array, no seeding pass, and no `savemigration.txt` entry** — nothing is orphaned,
  because nothing was removed.

⚑ **Why the reader and not the field:** the alternative is a read-old/write-new dance in every widened member —
a per-field transitional shape, repeated forever, that each future reader has to recognise. One rule in the
reader retires the whole class ([build the proper structure once](../../AGENTS.md#design)). It is the same
move `savemigration.txt` already makes for removal and rename: the format absorbs the change centrally rather
than every field carrying its own migration.

⚠ **This covers WIDENING ONLY — a wider stored value into a narrower member is still a break**, and correctly so:
there is no safe conversion, only truncation. The direction is one-way by design.

⚠ Widening changes what the FIELD can hold, never what it MEANS. A same-tag field whose *meaning* changed is
still real save-break #1 (the silent-wrong-load class) and this does nothing for it.
## See also

- [cascade.md](../cascade.md) — the derived-cache model that rests on §5.
- [leave no evidence of the abandoned path](../../AGENTS.md#design) — the same discipline applied to a lingering
  `WRAPPER_SKIP_ELEMENT`.
- [engine.md](../reference/engine.md) — the closed-`.exe` VC7.1 toolchain the save format is frozen by.
