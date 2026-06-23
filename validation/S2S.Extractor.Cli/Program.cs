using S2S.Extractor;

// Composition root for the CLI host. The console is ONE output of the extraction library: it picks the
// concrete IStateExtractor, runs it, and writes the JSON to validation/output/. Swap the concrete (or the
// host) without touching the extraction logic.
//
// Args: [0] base URL (default http://127.0.0.1:7227)   [1] output dir (default validation/output)

string baseUrl = args.Length > 0 ? args[0] : HttpStateExtractor.DefaultBaseUrl;
string outDir = args.Length > 1
    ? Path.GetFullPath(args[1])
    : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "output"));
Directory.CreateDirectory(outDir);

using var extractor = new HttpStateExtractor(baseUrl);
IStateExtractor state = extractor;

if (!await state.IsAliveAsync())
{
    Console.Error.WriteLine(
        $"[extractor] live surface DOWN at {baseUrl} (root != 'hello world'). " +
        "Is the game running with the HTTP server enabled?");
    return 2;
}

string json = await state.FetchGameStateAsync();
string stamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
string path = Path.Combine(outDir, $"gamestate_{stamp}.json");
await File.WriteAllTextAsync(path, json);

Console.WriteLine($"[extractor] wrote {json.Length:N0} chars -> {path}");
return 0;
