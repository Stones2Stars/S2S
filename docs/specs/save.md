# Save / load — the name-keyed format & the soft-remove discipline

> **A core spec.** How S2S serializes game state and stays load-compatible. The cascade's entire "derived data
> serializes nothing / rebuild on load" discipline rests on this. **The soft-remove mechanism (§3) is SETTLED — read
> it HERE and trust it; do NOT re-derive it from `CvTaggedSaveFormatWrapper.cpp` each session** (the recurring
> rollerskate). Home of [DEC-save-remove-is-soft](../architecture/decisions.md#dec-save-remove-is-soft) +
> [DEC-derived-never-trusted](../architecture/decisions.md#dec-derived-never-trusted).

## 1. The format — name-keyed, not positional

Saves are `(id, type-code, value)` tuples keyed by a normalized **`"ClassName::memberName"`** tag; there is **no
save-version number** — compatibility resolves dynamically by name (`CvTaggedSaveFormatWrapper`). A read asks for a
tag and `Expect()` matches it against the stream. Because position doesn't matter, adding / removing / reordering
fields is handled by name, never by a version gate.

## 2. Adding a field is SOFT (nothing to declare)

Old save, new code: the new read's tag isn't in the old stream, so `Expect()` returns false, leaves the stream
untouched, and the member keeps its default (`CvTaggedSaveFormatWrapper.cpp` ~:3830). No action needed.

## 3. Removing a serialized field — the SOFT-REMOVE via `Assets/savemigration.txt` ⭐

> [DEC-save-remove-is-soft](../architecture/decisions.md#dec-save-remove-is-soft) — this is its authoritative home.

**FULL-DELETE the member + its read + its write, and NAME the orphan tag in `Assets/savemigration.txt`.** That is the
whole procedure. The reader (`Expect` → `sm_isCut`, `CvTaggedSaveFormatWrapper.cpp` ~:3944) parses that file **once**
at load and drains any listed orphan tag **transparently, wherever it sits in the stream** — the drain runs inside the
header loop and falls through to read the next tag, so **consecutive orphan tags of the same named field all drain**.
The field is then FULLY GONE from the object: no member, no read, no write.

- **⛔ NO `WRAPPER_SKIP_ELEMENT` for a removed field.** A lingering skip still names the dead member in the read path —
  a rollerskate target ([DEC-no-rollerskate-evidence](../architecture/decisions.md#dec-no-rollerskate-evidence)) — and
  the central drain makes it redundant. The old two-stage `SKIP_ELEMENT`-now / flush-at-the-next-break model is
  **RETIRED**; there is no "flush."
- **Save-breaking is OBSOLETE for field removal.** A listed tag loads clean from any old save, forever — no version
  bump, no `@SAVEBREAK`.
- **RENAME rides the same file:** `Class::m_old -> Class::m_new` remaps the old tag onto the new member.
- **⛔ THE ONE HARD RULE — the entry is MANDATORY.** An **UNLISTED** orphan (a deleted read with no
  `savemigration.txt` line) makes `Expect()` treat the mismatch as "code ahead of stream," never consume the element,
  and **desync every subsequent read in that object** into silent defaults — the load guts wholesale (proven live:
  empty tech lists, buildingless cities). List the tag and it is soft; forget it and the object is corrupt.

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

## 5. Derived data serializes NOTHING ⭐

> [DEC-derived-never-trusted](../architecture/decisions.md#dec-derived-never-trusted) — this is its authoritative home.

A recompute-only cache (yields, commerce, the cascade packages, network bonus counts, power, dormancy verdicts, …) is
**never trusted from a save**: don't read it, don't write it, drain any old-save orphan via §3. `reset()` /
dirty-on-construct means the first read after load recomputes from current state — never stale-from-save. This is
**universal, not per-field-optional** (owner ruling): no cache is ever serialized. (`CvGame::recalculateModifiers` —
the old purge of drifted serialized derived data — is RETIRED as a concept: with nothing derived read from a save,
there is nothing to purge.) A serialized store survives ONLY for genuine **non-derivable** state (event/vote grants,
e.g. `CvCity::m_paiFreeBonusEvents`). Cache mechanics: [state-repositories.md](../architecture/state-repositories.md).

## 6. Deleting a changer? Audit its whole BODY for side effects

An apply-site audit alone misses non-obvious riders. Legacy changers carry them: `changeTerrainTradeCount` /
`changeRiverTradeCount` call `updatePlotGroups()` (the trade-network recompute) per team player;
`changeBridgeBuildingCount` marks bridges dirty; others fire UI dirty bits. Post-cut, the surviving trigger site
(`setHasTech` / `processTech` / …) must still fire those effects, or derived engine state goes progressively stale.

## 7. The genuinely HARD cases (the only things left that break a save)

Enum/Type drift is name-remapped on load (`getInfoTypeForString`); XML reorder/insert is free; a removed **Type** is
soft only if its read uses `WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING` (else HARD: message box + throw). The five real
save-breaks:

1. a same-tag field whose **meaning** changed (silent wrong load);
2. a deleted **Type** read without `_ALLOW_MISSING`;
3. a legacy raw enum-indexed int array that **shrinks**;
4. a **type-code** change under a reused name;
5. a deleted field with **no** `savemigration.txt` entry (the §3 stale-tag desync).

*Hardening path:* flip a Type's read to `_ALLOW_MISSING`, then delete. Everything else — field add, field remove,
rename, reorder — is soft.

## See also

- [state-repositories.md](../architecture/state-repositories.md) — the derived-cache model that rests on §5.
- [decisions.md](../architecture/decisions.md) — [DEC-save-remove-is-soft], [DEC-derived-never-trusted],
  [DEC-no-rollerskate-evidence].
- [engine.md](../reference/engine.md) — the closed-`.exe` VC7.1 toolchain the save format is frozen by.
