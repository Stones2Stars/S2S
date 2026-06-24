#!/usr/bin/env python3
"""extract.py -- the RAW game-state extractor client.

The DLL does the EXTRACTION: the `/extractor/gamestate` endpoint (Sources/Tools/CvHttpServer.cpp) emits the
full raw game-state as ONE document along the cascade scope spine
    world -> teams -> empires -> areas -> cities -> plots
RAW FACTS ONLY -- no calculated value (DEC-calc-zero-ride-in); the lone map-number is distanceFromCapital.

This client just PULLS that document, VALIDATES it (it must load via Tools/ModifierCalc/dry_calc.py -- the
consumer contract), and SAVES it -- so you can read the gamestate JSON directly, feed it to dry_calc, or build
further features on it. It adds NO data of its own; zero-ride-in is enforced upstream by the endpoint.

Spec of the document: Tools/ModifierCalc/README.md.

Run:
  python extract.py                  # fetch live (127.0.0.1:7227) -> game_state.json + summary
  python extract.py --player 0       # restrict to one player (smaller dump)
  python extract.py --out path.json  # choose the output file
  python extract.py --from FILE      # validate/summarize an existing dump (offline; no game needed)
  python extract.py --selftest       # offline self-test against the fabricated example (no game needed)
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODIFIERCALC = os.path.normpath(os.path.join(HERE, "..", "ModifierCalc"))
sys.path.insert(0, MODIFIERCALC)
import dry_calc  # the consumer: load_raw_state validates the document + builds per-city ScopeContexts

HOST, PORT = "127.0.0.1", 7227


def fetch_live(player=None, timeout=20):
    """GET /state/all from the running game (the whole raw state in one document). Fails HONESTLY (never
    fabricates a clean document). NB the endpoint moved /extractor/gamestate -> /state/all in the HTTP-server
    rework (docs/specs/http-endpoints.md); the document shape (schema gamestate/1, world/teams/...) is unchanged."""
    import urllib.request
    url = "http://%s:%d/state/all" % (HOST, PORT)
    if player is not None:
        url += "?player=%d" % player
    try:
        resp = urllib.request.urlopen(url, timeout=timeout)
        body = resp.read().decode("utf-8")
    except Exception as e:
        raise SystemExit("extract: live fetch FAILED (%s: %s). Is the game running with the HTTP server on "
                         "(BUG option Autolog__HttpServer)? Smoke-check: curl http://%s:%d/" %
                         (type(e).__name__, e, HOST, PORT))
    doc = json.loads(body)
    if "error" in doc:
        raise SystemExit("extract: endpoint returned an error: %s" % doc["error"])
    return doc


def validate(doc):
    """The document is valid iff dry_calc can consume it (the contract). Returns (config, [city records])."""
    return dry_calc.load_raw_state(doc)


def summarize(doc, recs):
    teams = doc.get("world", {}).get("teams", [])
    nplayers = sum(len(t.get("empires", [])) for t in teams)
    nplots = sum(len(r["ctx"].plots) for r in recs)
    print("gamestate schema=%s turn=%s | teams=%d players=%d cities=%d plots=%d" %
          (doc.get("schema"), doc.get("turn"), len(teams), nplayers, len(recs), nplots))
    for r in recs[:25]:
        c = r["ctx"]
        print("  t%d/p%d/a%d city=%s '%s' pop=%d cap=%s | bldg=%d spec=%d bonus=%d rel=%d plots=%d dist=%d" %
              (r["team"], r["empire"], r["area"], r["city"], r["name"], c.population, c.isCapital,
               len(c.buildings), sum(c.specialists.values()), len(c.bonuses), len(c.religions),
               len(c.plots), r["distanceFromCapital"]))
    if len(recs) > 25:
        print("  ... (%d more cities)" % (len(recs) - 25))


def run_selftest():
    """Offline: validate the fabricated example end-to-end + confirm spine-scope inheritance. No game."""
    ex = os.path.join(MODIFIERCALC, "raw_state.example.json")
    with open(ex, encoding="utf-8") as fh:
        doc = json.load(fh)
    config, recs = validate(doc)
    assert len(recs) == 2, "expected 2 cities, got %d" % len(recs)
    by = {r["name"]: r["ctx"] for r in recs}
    assert set(by) == {"Alpha", "Beta"}, by.keys()
    a = by["Alpha"]
    assert "TECH_POTTERY" in a.techs, "team-scope tech not inherited"
    assert "CIVIC_REPUBLIC" in a.civics, "empire-scope civic not inherited"
    assert a.stateReligion == "RELIGION_X", "empire-scope stateReligion not inherited"
    assert "BONUS_IRON" in a.bonuses, "city-scope bonus not read"
    assert "RELIGION_X" in a.religions, "city-scope religion not read"
    assert a.religionLevels.get("RELIGION_X") == 2, "world-scope religionLevels not applied"
    assert a.counts.get("PROPERTY_CRIME") == 50, "city property value not read"
    print("extract selftest: OK -- example validated; scope inheritance correct (world/team/empire/city).")
    summarize(doc, recs)


def main():
    ap = argparse.ArgumentParser(description="RAW game-state extractor client (/extractor/gamestate -> JSON).")
    ap.add_argument("--player", type=int, default=None, help="restrict to one player id")
    ap.add_argument("--out", default=None, help="output file (default game_state.json)")
    ap.add_argument("--from", dest="src", default=None, help="validate/summarize an existing dump file (offline)")
    ap.add_argument("--selftest", action="store_true", help="offline self-test against the fabricated example")
    args = ap.parse_args()

    if args.selftest:
        run_selftest()
        return
    if args.src:
        with open(args.src, encoding="utf-8") as fh:
            doc = json.load(fh)
    else:
        doc = fetch_live(args.player)
    config, recs = validate(doc)
    summarize(doc, recs)
    if not args.src:
        out = args.out or os.path.join(HERE, "game_state.json")
        with open(out, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, indent=1)
        print("saved -> %s" % out)


if __name__ == "__main__":
    main()
