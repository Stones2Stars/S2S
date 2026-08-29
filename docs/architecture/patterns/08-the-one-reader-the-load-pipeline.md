# The ONE reader — the load pipeline law

> Part of the **[patterns](../patterns.md)** spec.

> Binding: [exactly one JSON reader](#the-one-reader--the-load-pipeline-law). Owner rulings: exactly one JsonReader exists;
> JSON is read at GAME LOAD only; no string matching on any read path.

- **Exactly ONE JSON reader exists** — the load pipeline in `Sources/Data`, entry point `loadJson()`. The
  reader is **readJson**, the first of the four systems ([north-star.md](../north-star.md)) — it is NOT the
  cascade, and no reader name carries a `cascade` prefix ([the enabler and the modifier cascade are two separate systems](../../specs/enabler.md)'s
  naming guard, applied one system over). It enumerates `Assets/Data` once,
  parses each file ONCE into memory, registers every type→id before any `mapFrom` (the two-pass rule), maps every
  entity, runs the full-registry FK/reverse pass over the RETAINED in-memory parse (never a disk re-read), and
  compiles the routing index. **Every JSON-shaped object is freed before load ends.** A second parse call site
  anywhere in the tree is a defect, whatever it is named.
- **Fail-loud key coverage.** The reader accounts EVERY top-level key of every entity to exactly one consumer (a
  reserved-section parser or the modifier-family walk); an unconsumed key is a loud load-time report. "The info
  matches the JSON structure" is thereby a mechanical check, never an agent's self-assertion.
  ⛔ **AND COVERAGE RUNS BOTH WAYS — accounting for every key PRESENT does not account for a key ABSENT.** A
  required key nobody authored passes every check above: there is no key to place, no section left unconsumed,
  no id that fails to resolve. The reader substitutes a default and the game runs on a value no author chose,
  with the substitution indistinguishable from authorship. ⇒ **A fallback over a SEMANTIC default is therefore
  itself a reportable miss** (`jsonNoteMissingKey`), and it is the count to read FIRST, because it is the only
  one whose evidence does not exist in the data. ⚖ Not every default qualifies: where absence and the default
  carry the same meaning — a numeric zero, an empty list, a value the curator deliberately elides — there is
  nothing unknown to report, and reporting it buries the misses that matter.
- **The `Json` name-fragment is reserved for the load-time parse surface** (the reader + the parse walkers). A
  runtime-resident type carries no `Json` in its name — so a `Json*`-named type living past load is, by its own
  name, misnamed or misplaced.
- **After load, nothing string-shaped remains readable** — the reader's half of
  [materialize at mapFrom](07-materialize-at-mapfrom-no-runtime.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling): every served value is typed, id-resolved,
  and ×100 before the first turn runs.
