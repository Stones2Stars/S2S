# validation/ — JSON-validation tooling (.NET 10)

A .NET solution of **libraries behind clean interfaces**; hosts (a console today, an API later) are just
consumers. Modern .NET 10 — **entirely separate** from the frozen VC2003 / C++ DLL toolchain in `Sources/`
(that "don't modernize the build chain" rule is about the DLL, not this).

> Each tool's logic is a library behind an interface, picked at a composition root (poor-man's DI). The
> console is one output, not the thing. **No mediator / CQRS layer here by design** — that slots in later
> via the owner's Clean-Architecture API template, especially once a Pedia API host is real.

## Projects

| Project | Kind | Role |
|---|---|---|
| `S2S.Model` | library | Typed Info model over `Assets/Data/**` — the **definitions** plane. The keystone shared by validation, cascade-parity, and a future hosted pedia. |
| `S2S.Extractor` | library | Pulls a live game-state snapshot from the DLL's HTTP surface (`IStateExtractor`) — the **live-state** plane. Returns raw JSON for any purpose. |
| `S2S.Extractor.Cli` | console host | One host of `IStateExtractor`: writes the snapshot to `output/`. |

## Two data planes

- **Definitions** — `S2S.Model` over `Assets/Data`: what entities *are*. Feeds the pedia.
- **Live state** — `S2S.Extractor` over the endpoints: what the running game *is doing*. Feeds parity.

## Output

`output/` (gitignored) holds extractor snapshots, e.g. `gamestate_<timestamp>.json`.

## Run the extractor

With the game running and the HTTP server enabled:

```
dotnet run --project S2S.Extractor.Cli
# optional: dotnet run --project S2S.Extractor.Cli -- http://127.0.0.1:7227 ./output
```
