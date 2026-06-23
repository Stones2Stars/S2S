# `Tools/RawStateExtractor/` — the raw game-state extractor client

> **Status:** tool (offline-testable Python client) · **Grounding:** `extract.py`; the DLL endpoint
> `Sources/Tools/CvHttpServer.cpp` (`/extractor/gamestate`); the document spec
> [`../ModifierCalc/README.md`](../ModifierCalc/README.md).

Pulls the **full raw game-state JSON** from the live game and validates/saves it, so you can read it, feed it
to the offline calc, or build further features on it.

## The split — who does what

- **The DLL does the EXTRACTION.** `GET /extractor/gamestate[?player=N]` (in `CvHttpServer.cpp`) walks the
  cascade scope spine `world → teams → empires → areas → cities → plots` and emits **raw facts only** — no
  calculated value ([DEC-calc-zero-ride-in](../../docs/dev/architecture/decisions.md#dec-calc-zero-ride-in));
  the lone map-number is `distanceFromCapital`. Game-side every fact is readable, so this is one clean endpoint,
  not a scrape-and-strip of the calculated dumps. **Schema:** [`../ModifierCalc/README.md`](../ModifierCalc/README.md).
- **This client just pulls + validates + saves.** It adds NO data. "Valid" means the document loads via
  `Tools/ModifierCalc/dry_calc.py` `load_raw_state` — the consumer contract — so a fetched document is
  guaranteed calc-ready.

## Run

```
python extract.py                  # fetch live (127.0.0.1:7227) -> game_state.json + summary
python extract.py --player 0       # restrict to one player (smaller dump)
python extract.py --out path.json  # choose the output file
python extract.py --from FILE      # validate/summarize an existing dump (offline; no game needed)
python extract.py --selftest       # offline self-test against the fabricated example (no game needed)
```

Live mode needs the game running with the HTTP server on (BUG option `Autolog__HttpServer`, Logging tab).
On a failed live fetch the client fails **honestly** (it never fabricates a clean document) — smoke-check with
`curl http://127.0.0.1:7227/`.

## Where it fits

This is piece 3 of the parity architecture (the extractor); piece 1 is the calc (`dry_calc.py`), piece 2 is the
game-state JSON spec ([`../ModifierCalc/README.md`](../ModifierCalc/README.md)). Once a document is extracted,
run `dry_calc` on it and diff against the game's realized values — attributable per channel because the input is
provably identical on both sides. See [`shadow.md` §7](../../docs/dev/reference/cascade/shadow.md).

## See also
- [`../ModifierCalc/README.md`](../ModifierCalc/README.md) — the game-state document spec + the calculators.
- [`../../docs/dev/reference/observability/http-server.md`](../../docs/dev/reference/observability/http-server.md) — the HTTP server `/extractor` lives on, and the live-read rules.
