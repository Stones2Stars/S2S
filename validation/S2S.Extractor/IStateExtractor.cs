namespace S2S.Extractor;

/// <summary>
/// Pulls a live game-state snapshot from the running DLL's HTTP observability surface.
/// Extraction is logic, not a console concern: any host (CLI, an API, a test harness) consumes this
/// interface and decides what to do with the JSON. The console is just one possible output.
/// </summary>
public interface IStateExtractor
{
    /// <summary>Cheap liveness probe — the root endpoint returns "hello world".</summary>
    Task<bool> IsAliveAsync(CancellationToken ct = default);

    /// <summary>The full game-state snapshot as raw JSON (<c>/extractor/gamestate</c>).</summary>
    Task<string> FetchGameStateAsync(CancellationToken ct = default);

    /// <summary>Any endpoint path (e.g. <c>/diagnostic/sweep</c>) as raw JSON.</summary>
    Task<string> FetchAsync(string path, CancellationToken ct = default);
}
