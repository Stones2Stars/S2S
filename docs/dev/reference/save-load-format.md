# Save / load format — the tagged wrapper, and the real cost of removing something

How S2S serializes game state, and — the practically important part — **what actually happens when you
remove a serialized field, member, or Type.** Source of truth: `Sources/CvTaggedSaveFormatWrapper.{h,cpp}`
(line numbers below are from there unless noted). Bottom line up front: **the format is name-keyed, so
removing a plain serialized member is a SOFT change, not a save-break.**

> **Recovered 2026-06-17.** This authoritative reference was authored in the #428 feat commit and was
> accidentally dropped when `Sources/docs/` was consolidated into `docs/dev/` (no delete commit — it simply
> wasn't carried over in the working-tree shuffle). Restored here. The **format mechanics below are
> stable/authoritative.** The only part still being finalized is the **cascade-specific** save handling — what
> the #428/#430 tally and accumulators serialize (answer: nothing; rebuilt on load), locked in
> [`../plans/tally-cascade-spec.md`](../plans/tally-cascade-spec.md) §9. Currently branch-resident on
> `json-data-migration` pending an owner call on promoting it back to `main`.

## The tagged format in one paragraph
The save is **not positional** — it is a self-describing, **name-keyed** dictionary stream. Every
`WRAPPER_WRITE(wrapper, "CvPlayer", m_iGold)` writes a `(id, type-code, value)` tuple where `id` keys into a
name dictionary built lazily as fields are first seen (`getId`, `2418-2478`): the first write of a tag like
`"CvPlayer::m_iGold"` emits a dictionary entry `(SAVE_ELEMENT_ID_DICTIONARY_ENTRY, newId, type, nameLen,
name[])`. Objects are bracketed by start/end delimiters (`WriteStartObject`/`ReadEndObject`), and the first
object dumps **class mapping tables** for every `REMAPPED_CLASS_TYPE` (`WriteClassMappingTables`, `1406-1410`;
tables `616-1209`). Because the on-disk key is the field's **name**, not its ordinal position, the reader
resynchronizes by name — which is the whole reason add/remove of a field is tolerable rather than
catastrophic. **There is no save-format version number;** compatibility is decided dynamically by whether
`Expect()` keeps matching names and types.

## The read path
Reading flows through `Expect(name, type)` (`3830-3922`); dictionary entries / class-map tables encountered
inline are consumed silently (`3848-3858`). Then:
- **Added field (new DLL reads a tag the old save never wrote):** the next stream element has a different
  name, the compare fails (`3900`), `Expect` returns `false` (`3912`), and the scalar `Read` **does not touch
  the stream** (`Read(int*)` only reads *inside* `if (Expect(...))`, `2925-2928`). The member keeps its
  **default-initialized value**. → **added fields default cleanly; no migration code needed.**
- **Removed field (old save still contains a tag the new DLL never reads):** the orphan bytes sit until the
  object closes; `ReadEndObject` (`3799-3819`) runs `while (!Expect(NULL, OBJECT_DELIMITER)) SkipElement();`,
  draining every unconsumed element at this nesting level, and `SkipElement` (`3957-4151`) consumes exactly
  the right byte count from the per-element type metadata. → **removed fields are harmlessly skipped.**

So the everyday symmetric case — add a member, or remove a member — is **SOFT in both directions**, purely
because keys are names and objects are delimited.

## Enum / Type drift — name-based, remapped on load
Class-typed references (buildings, units, promotions, civics, … — the `REMAPPED_CLASS_TYPE` list, `h:17-72`)
are **stored by NAME, not raw enum int.** Save writes each class's ordered `getType()` strings as they were in
that DLL's XML load (`616-1209`). On load, `ReadClassMap` (`4182-4202`) rebuilds `m_enumMaps` keyed by the
**old** int; the first dereference remaps lazily via `getNewClassEnumValue` (`1366-1398`):
`info.m_id = GC.getInfoTypeForString(info.m_szType, true)` (`1384`). `GC.getInfoTypeForString`
(`CvGlobals.cpp:2682-2701`) is the **current** XML load's name→int registry — so the **old int is discarded
and a fresh current int is looked up by name.** Reordering / inserting Types in XML is therefore **free**.
A **removed Type** → `getInfoTypeForString` returns `-1`:
- `WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING` (`h:341`) → `-1`, load continues. **SOFT.**
- plain `WRAPPER_READ_CLASS_ENUM` → `HandleIncompatibleSave` (message box + throw). **HARD.**

(Nuance: the *"only add to the END"* note `h:16` governs the `REMAPPED_CLASS_TYPE` **registry enum** — the
fixed list of *which kinds* get remapped — not the XML order of buildings/units within a kind, which is fully
name-driven and free to churn.)

## The real ramification of removing something completely
| What you remove | Old save → new DLL | Deciding code |
|---|---|---|
| **A scalar/array member** (`WRAPPER_READ`/`_WRITE` deleted) | **SOFT** — orphan tag drained, `SkipElement` consumes the right bytes, no desync | `3814-3818`, `3957-4151`, `2925-2928` |
| **A class Type from XML** (delete `BUILDING_X`) | **CONDITIONAL** — SOFT if read site is `…_ALLOW_MISSING` (→ `-1`); **HARD** otherwise | `1382-1393` |
| **A Type inside a class-array** (`m_ab*[BUILDING_X]`) | **SOFT (best-effort)** — `ReadClassArray` (`3425-3470`) drops vanished slots; only a *non-default* value at a gone slot raises a **recoverable** warning, not a crash; old/new counts need not match | `3438-3470` |
| **Shrinking a legacy raw (non-class) enum-indexed int array** | **HARD** — `if (num > count) HandleIncompatibleSave` (`3478-3482`), only in the `SAVE_VALUE_TYPE_INT_ARRAY` fallback; proper class arrays are immune | `3478-3482` |

**Is "remove a serialized accumulator" a save-break? No — it's a soft change.** Deleting the
`WRAPPER_WRITE`/`WRAPPER_READ` pair for a plain accumulator costs **nothing** on load: old saves carry the
orphan tag, the new read never asks for it, the skip loop eats it. The only "loss" is historical bytes nobody
reads. ("Structural mid-serialization desync" is **not** a real failure mode of the symmetric same-era format
— the name-keyed `Expect` + delimiter-drain design prevents it; it would only bite on a *type-code change
under a reused name*.)

**The genuinely HARD cases (all that a save-migration / "rebuild save" tool must really handle):**
1. **Changed-meaning fields** — same tag name, but the value's semantics/units changed; the format can't
   detect it and silently loads a now-wrong number. (Needs explicit, versioned migration.)
2. **A still-needed Type deleted from XML, read without `_ALLOW_MISSING`** (`HandleIncompatibleSave`).
3. **Legacy raw (non-class) enum-indexed arrays that shrink** (`3478-3482`).
4. **Type-code change under a reused name** — `Expect`'s type check (`3896`) mismatches, the field silently
   defaults.

## Repository structure shrinks the problem
The derived-data repository ([`../plans/derived-data-repository.md`](../plans/derived-data-repository.md))
**serializes nothing** — every owner's `reset()` marks derived data dirty on load and recomputes it from live
state ("derived data is never trusted from a save"). So **derived state has nothing to migrate.** If an
accumulator's only job is to feed a derived value, deleting its serialization is the *intended* pattern — the
value recomputes on load. The #423 recompute-on-load cascade generalizes exactly this: **the more
authoritative state becomes derived, the smaller the serialized surface, and the less there is to break or
migrate.**

This is exactly the stance the **#428/#430 cascade** takes: the **tally and scope accumulators serialize
nothing** — they are **rebuilt from the authoritative loaded objects on load** (LOCKED:
[`../plans/tally-cascade-spec.md`](../plans/tally-cascade-spec.md) §9). Anything genuinely NOT recomputable
from current state (a true lifetime/historical counter) is owned + saved on its **owning object**, never in a
derived/aggregation module — so a derived module is always safely rebuildable and nothing unrecoverable lives
only in it.

## Consequences for save-breaks
- **The dormant → prune model is cheap.** You can stop writing a field today (dormant in new saves; old
  saves' copies skipped) and **delete the member later with zero added load risk**. Pruning an already-unread
  member is a non-event for the loader. (For a *plain* member you don't even need the dormant step — direct
  deletion is soft; dormant is just tidiness.)
- **Type/XML churn is effectively free** for class enums/arrays: reorder, insert, and (with `_ALLOW_MISSING`)
  remove Types with no migration step. Cheap hardening before deleting a Type: flip its read site to
  `WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING` first.
- **What is actually a save-break** is only the four HARD cases above — that is the real target for any
  versioned-migration / "rebuild save" effort (#426). Everything else is soft by construction.
