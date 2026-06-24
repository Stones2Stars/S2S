# `Tools/ApiSpec/` — the HTTP observability surface, as a machine spec

Two generated descriptions of the live DLL server (`Sources/Tools/CvHttpServer.cpp`, `127.0.0.1:7227`).
The authoritative prose + design rationale is [`docs/specs/http-endpoints.md`](../../docs/specs/http-endpoints.md);
these are the machine-consumable mirrors of that same route table.

- **`openapi.yaml`** — an OpenAPI 3.0 description of every endpoint (`/`, `/events`, `/state/*`, `/computed/*`)
  with their query params (`player`, `city`, `type`). Use it to generate a typed **.NET `HttpClient`** on the
  validator side, and as documentation.
- **`bruno/`** — a [Bruno](https://docs.usebruno.com) collection (`.bru` files) for clicking through the
  endpoints by hand. Open the `bruno/` folder in Bruno and select the **Local** environment (`baseUrl =
  http://127.0.0.1:7227`). Each request ships with a ready-to-fire example (`player=0`, sample `type=`).

> Keep both in sync with the route table in `CvHttpServer.cpp::handleRequest` when endpoints change (the route
> table is the source of truth; these are derived). The spec's `/state` vs `/computed` split is load-bearing:
> `/state` carries only raw inputs (never a drycalc target), `/computed` carries the engine's own answers.

## Generate the .NET client from the OpenAPI spec

With [NSwag](https://github.com/RicoSuter/NSwag) (`dotnet tool install -g NSwag.ConsoleCore`):

```bash
nswag openapi2csclient /input:openapi.yaml /classname:S2SObservabilityClient /namespace:S2S.Observability /output:S2SObservabilityClient.cs
```

…or with [swagger-codegen](https://github.com/swagger-api/swagger-codegen) / OpenAPI Generator
(`openapi-generator-cli generate -i openapi.yaml -g csharp -o ./client`).

Response bodies are described as generic JSON objects (the documents are large and vary by route); deserialize
to whatever DTOs the consumer needs — the field shapes are documented in `docs/specs/http-endpoints.md`.

## Bruno can also import the OpenAPI spec

Instead of the checked-in `bruno/` collection you can regenerate one from `openapi.yaml` via Bruno's
**Import → OpenAPI** (or the `openapi-to-bruno` converter). The checked-in collection is the curated version
with example values pre-filled.
